#!/usr/bin/env python3
"""Import suggested Tile16 observations with exact replay and input bindings.

This offline tool reads retained JSON and optionally hashes the original ROM and
exporter. It never executes an exporter or writes a ROM/editor document. Repeat
anchors are re-enumerated against the corpus; source records remain attached.
Outputs are create-only. Suggested discoveries confer no placement, collision,
overlap, semantic-label or destination-profile approval.
"""
import argparse
import copy
import gzip
import hashlib
import json
from pathlib import Path

from PIL import Image


ADAPTER = "alttp.overworld.tile16.v1"
TRANSPARENCY = "decoded_source_zero_before_palette_remapping"


def require(condition, message):
    if not condition:
        raise ValueError(message)


def integer(value, low, high, name):
    require(type(value) is int and low <= value <= high, f"Invalid {name}")
    return value


def canonical(value):
    """nlohmann::json's sorted, compact UTF-8 encoding for these integer inputs."""
    return json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode()


def fingerprint(data):
    value = 14695981039346656037
    for byte in data:
        value = ((value ^ byte) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return f"fnv1a64:{value:016x}"


def snapshot(path):
    """Read once so parsing/fingerprinting and manifest SHA use identical bytes."""
    path = Path(path)
    data = path.read_bytes()
    return data, {"name": path.name, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def identity(path):
    return snapshot(path)[1]


def read_json_snapshot(path):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, f"Duplicate JSON key {key}")
            result[key] = value
        return result
    data, receipt = snapshot(path)
    return json.loads(data, object_pairs_hook=unique), receipt


def read_json(path):
    return read_json_snapshot(path)[0]


def parse_pattern(record):
    rows = record["rows"]
    w = integer(record["width"], 1, 256, "candidate width")
    h = integer(record["height"], 1, 256, "candidate height")
    require(isinstance(rows, list) and len(rows) == h, "Candidate height differs")
    out = []
    for row in rows:
        require(isinstance(row, list) and len(row) == w, "Candidate width differs")
        parsed = []
        for tile in row:
            require(tile is None or isinstance(tile, str), "Expected hex Tile16 or null skip")
            parsed.append(None if tile is None else integer(int(tile, 16), 0, 65535, "Tile16"))
        out.append(tuple(parsed))
    require(any(t is not None for row in out for t in row), "Empty candidate footprint")
    return tuple(out)


def matches(grid, pattern, x, y):
    """A null is a skip; grid -1 is excluded data and cannot match a Tile16."""
    h, w = len(pattern), len(pattern[0])
    if type(x) is not int or type(y) is not int or x < 0 or y < 0:
        return False
    if x + w > grid["width"] or y + h > grid["height"]:
        return False
    return all(tile is None or grid["rows"][y + dy][x + dx] == tile
               for dy, row in enumerate(pattern) for dx, tile in enumerate(row))


def replay(grids, record):
    pattern = parse_pattern(record)
    bad = []
    for anchor in record["at"]:
        require(isinstance(anchor, list) and len(anchor) == 3, "Invalid source anchor")
        area, x, y = anchor
        if area not in grids or not matches(grids[area], pattern, x, y):
            bad.append(anchor)
    return bad


def enumerate_matches(grids, pattern):
    """Enumerate physical canonical-unit matches, including overlapping matches."""
    h, w = len(pattern), len(pattern[0])
    return [[area, x, y] for area, grid in sorted(grids.items())
            for y in range(grid["height"] - h + 1)
            for x in range(grid["width"] - w + 1) if matches(grid, pattern, x, y)]


def validate_grid(grid):
    w = integer(grid["width"], 1, 256, "grid width")
    h = integer(grid["height"], 1, 256, "grid height")
    require(len(grid["rows"]) == h and all(len(row) == w for row in grid["rows"]),
            "Grid dimensions differ")
    for row in grid["rows"]:
        for tile in row:
            integer(tile, -1, 65535, "grid Tile16")


def validate_export(data):
    require(type(data["schema_version"]) is int and data["schema_version"] == 1,
            "Unsupported export schema")
    require(data["adapter"] == ADAPTER, "Unsupported export adapter")
    require(data["transparency_basis"] == TRANSPARENCY, "Unsupported transparency basis")
    require(data["profile"].get("adapter") == ADAPTER, "Profile adapter differs")
    require(fingerprint(canonical(data["profile"])) == data["profile_fingerprint"],
            "Profile fingerprint differs from exported profile")
    w = integer(data["width_tiles"], 1, 256, "export width")
    h = integer(data["height_tiles"], 1, 256, "export height")
    rows = [[None] * w for _ in range(h)]
    definitions = {}
    palette = data["profile"]["palette"]
    require(1 <= len(palette) <= 256, "Invalid exported palette size")
    for word in palette + [data["profile"]["background_color"]]:
        integer(word, 0, 32767, "packed BGR555 color")
    for record in data["records"]:
        tid = integer(record["tile_id"], 0, 65535, "export Tile16")
        require(tid not in definitions, "Duplicate exported Tile16")
        require(len(record["quadrants"]) == 4, "Expected four Tile8 quadrants")
        words = [integer(q["raw_word"], 0, 65535, "Tile8 word") for q in record["quadrants"]]
        definition = b"".join(word.to_bytes(2, "little") for word in words)
        require(fingerprint(definition) == record["definition_fingerprint"],
                "Definition fingerprint differs from exported words")
        definitions[tid] = record["definition_fingerprint"]
        require(len(record["pixels"]) == len(record["transparent_mask"]) == 256,
                "Expected 256 native pixels and source-zero mask values")
        for index in record["pixels"]:
            integer(index, 0, len(palette) - 1, "palette index")
        require(all(type(bit) in (bool, int) and bit in (0, 1)
                    for bit in record["transparent_mask"]), "Invalid source-zero mask")
        for x, y in record["occurrences"]:
            integer(x, 0, w - 1, "export x")
            integer(y, 0, h - 1, "export y")
            require(rows[y][x] is None, "Duplicate exported source cell")
            rows[y][x] = tid
    require(all(t is not None for row in rows for t in row), "Missing exported source cell")
    return rows, definitions


def load_observations(grids, export_dir):
    paths = sorted(Path(export_dir).glob("req_*.json"))
    require(bool(paths), "No req_*.json exports")
    parents, exports, roms, global_definitions, appearances = {}, [], set(), {}, {}
    requested = {}
    for path in paths:
        data, export_identity = read_json_snapshot(path)
        rows, definitions = validate_export(data)
        roms.add(data["rom_fingerprint"])
        require(len(roms) == 1, "ROM fingerprint differs across exports")
        area = f'{integer(data["parent_area"], 0, 255, "parent area"):02X}'
        require(area in grids, f"Export parent {area} absent from grids")
        grid = grids[area]
        require((data["width_tiles"], data["height_tiles"]) == (grid["width"], grid["height"]),
                f"Grid/export dimensions differ for {area}")
        require(data["profile_fingerprint"] == grid["profile_fingerprint"],
                f"Grid/export profile differs for {area}")
        masked = set(grid.get("masked_screens", []))
        for y, row in enumerate(rows):
            for x, tile in enumerate(row):
                physical = f'{data["parent_area"] + (y // 32) * 8 + x // 32:02X}'
                expected = -1 if physical in masked else tile
                require(grid["rows"][y][x] == expected,
                        f"Grid/export cell differs at {area}:{x},{y}")
        for tid, fp in definitions.items():
            require(tid not in global_definitions or global_definitions[tid] == fp,
                    f"Definition differs globally for {tid:04X}")
            global_definitions[tid] = fp
        for record in data["records"]:
            key = (data["profile_fingerprint"], record["tile_id"],
                   record["definition_fingerprint"])
            # Coordinates and labels are observations; pixels, source-zero
            # masks, quadrants and any future payload fields must agree for
            # the same rendering profile and Tile16 definition.
            appearance = {k: v for k, v in record.items() if k not in ("occurrences", "labels")}
            require(key not in appearances or appearances[key] == appearance,
                    f'Appearance differs globally for {record["tile_id"]:04X}')
            appearances[key] = appearance
        # Requested map is transport metadata; every parent payload must agree.
        parent = {k: v for k, v in data.items() if k != "requested_map"}
        require(area not in parents or parents[area] == parent,
                f"Duplicate export payload differs for parent {area}")
        parents[area] = parent
        request = f'{integer(data["requested_map"], 0, 255, "requested map"):02X}'
        require(request not in requested, f"Duplicate requested map {request}")
        requested[request] = area
        exports.append(dict(export_identity, requested_map=request, parent_area=area,
                            profile_fingerprint=data["profile_fingerprint"]))
    require(set(parents) == set(grids), "Grid/export parent coverage differs")
    for area, grid in grids.items():
        require(set(grid.get("requested_maps", [])) == {m for m, a in requested.items() if a == area},
                f"Grid/export requested maps differ for {area}")
    return parents, exports, next(iter(roms))


def bind_candidate(record, parents):
    tiles = {int(t, 16) for row in record["rows"] for t in row if t is not None}
    bindings = []
    for area in record["areas"]:
        parent = parents[area]
        definitions = {r["tile_id"]: r["definition_fingerprint"] for r in parent["records"]}
        require(tiles <= definitions.keys(), f"Candidate definitions absent in parent {area}")
        bindings.append({"area": area, "adapter": ADAPTER,
                         "profile_fingerprint": parent["profile_fingerprint"],
                         "definition_fingerprints": {f"{t:04X}": definitions[t] for t in sorted(tiles)}})
    record["bindings"] = bindings


def correct_candidates(grids, candidates, parents):
    corrected, mismatches, seen = [], {}, set()
    for source in candidates:
        require(source["id"] not in seen, "Duplicate candidate id")
        seen.add(source["id"])
        require(source["review_state"] == "suggested", "Discovery must remain suggested")
        original_bad = replay(grids, source)
        if original_bad:
            mismatches[source["id"]] = len(original_bad)
        record = copy.deepcopy(source)
        if source["kind"] in ("texture", "strip"):
            record["source_observation"] = copy.deepcopy(source)
            record["correction_method"] = "exact_canonical_unit_reenumeration"
            record["source_extent_note"] = (
                "Legacy width/height describe the unit; supplied extent metadata "
                "is preserved in source_observation and is never inferred")
            record["at"] = enumerate_matches(grids, parse_pattern(source))
        else:
            require(not original_bad, f'Non-repeat source replay failed for {source["id"]}')
        require(bool(record["at"]), f'No exact occurrences for {source["id"]}')
        require(not replay(grids, record), f'Corrected replay failed for {source["id"]}')
        require(len({tuple(a) for a in record["at"]}) == len(record["at"]), "Duplicate anchor")
        record["occurrences"] = len(record["at"])
        record["tile32_aligned"] = sum(x % 2 == 0 and y % 2 == 0 for _, x, y in record["at"])
        record["areas"] = sorted({a for a, _, _ in record["at"]})
        record["area_count"] = len(record["areas"])
        record["cells"] = sum(t is not None for row in record["rows"] for t in row)
        record["score"] = record["cells"] * record["occurrences"]
        record["review_state"] = "suggested"
        record["overlap_policy"] = "unapproved"
        record["placement_enabled"] = False
        bind_candidate(record, parents)
        corrected.append(record)
    return corrected, mismatches


def bgr555(word):
    integer(word, 0, 32767, "packed BGR555 color")
    def expand(value):
        return (value << 3) | (value >> 2)
    return tuple(expand((word >> shift) & 31) for shift in (0, 5, 10))


def native_indexed_tile(record, profile):
    """Return native P pixels and a separate L source-zero mask (255 means zero).

    A mask is per pixel, rather than a palette alpha entry: exported display
    indices cannot recover source-zero transparency after palette remapping.
    """
    require(len(record["pixels"]) == len(record["transparent_mask"]) == 256,
            "Expected native 16x16 observation")
    palette = profile["palette"]
    require(1 <= len(palette) <= 256, "Invalid exported palette size")
    for index in record["pixels"]:
        integer(index, 0, len(palette) - 1, "palette index")
    require(all(type(bit) in (bool, int) and bit in (0, 1)
                for bit in record["transparent_mask"]), "Invalid source-zero mask")
    image = Image.frombytes("P", (16, 16), bytes(record["pixels"]))
    image.putpalette(bytes(c for word in palette for c in bgr555(word)))
    mask = Image.frombytes("L", (16, 16), bytes(255 if b else 0 for b in record["transparent_mask"]))
    return image, mask


def integer_nearest(image, scale):
    integer(scale, 1, 16, "integer preview scale")
    return image.resize((image.width * scale, image.height * scale), Image.Resampling.NEAREST)


def import_corpus(grids_path, candidates_path, export_dir, *, rom_path=None,
                  exporter_path=None, exporter_source_identity=None, source_paths=()):
    corrector_identity = identity(__file__)
    source_identities = [identity(p) for p in source_paths]
    exporter_identity = identity(exporter_path) if exporter_path else None
    grids, grids_identity = read_json_snapshot(grids_path)
    candidates, candidates_identity = read_json_snapshot(candidates_path)
    require(isinstance(grids, dict) and bool(grids), "No parent grids")
    for area, grid in grids.items():
        require(area == f"{int(area, 16):02X}", "Noncanonical parent key")
        validate_grid(grid)
    parents, exports, rom_fp = load_observations(grids, export_dir)
    rom = None
    if rom_path is not None:
        rom_bytes, rom = snapshot(rom_path)
        require(fingerprint(rom_bytes) == rom_fp, "ROM bytes differ from exports")
    records, bad = correct_candidates(grids, candidates, parents)
    document = {"schema_version": 1, "status": "suggested_discovery_only",
                "agent_writes_enabled": False, "adapter": ADAPTER,
                "rom_fingerprint": rom_fp, "coordinate_units": "parent_area_tile16_cells",
                "skip_value": None, "excluded_source_value": -1,
                "candidates": records}
    observations = {"schema_version": 1, "rom_fingerprint": rom_fp, "parents": parents}
    manifest = {"schema_version": 1, "inputs": {"grids": grids_identity,
                "candidates": candidates_identity, "exports": exports},
                "tools": {"corrector": corrector_identity,
                          "source_files": source_identities,
                          "exporter": exporter_identity,
                          "exporter_source_identity_claim": exporter_source_identity,
                          "exporter_source_to_binary_verification": "not_performed"},
                "rom": rom, "rom_fingerprint": rom_fp,
                "original_replay": {"bad_candidates": len(bad),
                                    "bad_occurrences": sum(bad.values()), "by_id": bad},
                "corrected_replay": {"candidates": len(records),
                                     "occurrences": sum(r["occurrences"] for r in records),
                                     "bad_occurrences": 0},
                "review_state": "suggested", "overlap_policy": "unapproved",
                "placement_enabled": False,
                "observations_sha256": hashlib.sha256(canonical(observations)).hexdigest(),
                "render_convention": {"channels": "(v<<3)|(v>>2)",
                                      "pixels": "native indexed; separate source-zero mask",
                                      "preview": "integer nearest-neighbor only"}}
    require(identity(__file__) == corrector_identity, "Corrector source changed during import")
    return document, manifest, observations


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--grids", required=True, type=Path)
    parser.add_argument("--candidates", required=True, type=Path)
    parser.add_argument("--exports", required=True, type=Path)
    parser.add_argument("--rom", type=Path, help="optional read-only byte/fingerprint binding")
    parser.add_argument("--exporter", type=Path, help="optional binary hash; never executed")
    parser.add_argument("--exporter-source-identity", help="recorded claim, not binary/source proof")
    parser.add_argument("--source-tool", action="append", default=[], type=Path)
    parser.add_argument("--out", type=Path, help="new directory; absent means validate without output")
    args = parser.parse_args()
    document, manifest, observations = import_corpus(
        args.grids, args.candidates, args.exports, rom_path=args.rom,
        exporter_path=args.exporter, exporter_source_identity=args.exporter_source_identity,
        source_paths=args.source_tool)
    if args.out is not None:
        args.out.mkdir(exist_ok=False)
        payloads = {"candidates.json": canonical(document) + b"\n",
                    "observations.json.gz": gzip.compress(canonical(observations), mtime=0)}
        manifest["outputs"] = []
        for name, payload in payloads.items():
            with (args.out / name).open("xb") as stream:
                stream.write(payload)
            manifest["outputs"].append({"name": name, "bytes": len(payload),
                                        "sha256": hashlib.sha256(payload).hexdigest()})
        with (args.out / "manifest.json").open("xb") as stream:
            stream.write(json.dumps(manifest, indent=2).encode() + b"\n")
    print(json.dumps({"original_replay": manifest["original_replay"],
                      "corrected_replay": manifest["corrected_replay"],
                      "out": str(args.out) if args.out else None}, indent=2))


if __name__ == "__main__":
    main()
