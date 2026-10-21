#!/usr/bin/env python3
"""Export a saved Lyra map or bound assembly proposal without writing a project.

The native lyra-verify-project observation is required. It binds the saved
terrain/profile to actual decoded assets; this script does not guess graphics
or silently rebind a footprint. Distinct map graphics or sprite profiles bind
the map's complete properties and three verified resolved asset hashes.
"""

from __future__ import annotations

import argparse
import configparser
import hashlib
import json
from pathlib import Path
import zlib

import footprint_proposal as planner


PROFILE_FIELDS = {
    "graphics_sheets", "main_palette", "palette_set", "animated_sheet",
    "tile_count", "tile16_definition_fingerprint", "graphics_fingerprint",
    "palette_fingerprint", "animation_fingerprint",
}
VISUAL_FIELDS = ("graphics_sheets", "main_palette", "palette_set", "animated_sheet")
FOOTPRINT_FIELDS = {
    "schema_version", "units", "binding", "semantic_id", "review_state",
    "content_id", "rows", "anchor", "overlap_policy",
}
FOOTPRINT_OPTIONAL = {"clearance", "ports", "provenance", "unresolved_behavior"}
# Oracle's expanded Tile16 table, also independently checked by the native
# verifier. This adapter deliberately refuses other layouts instead of probing.
DEFINITIONS_PC = 0x1E8000
MAP_ASSETS_ADAPTER = "lyra-sidecar-v3-map-assets-v1"


def sha(data):
    return hashlib.sha256(data).hexdigest()


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def read_json(data):
    return json.loads(data, object_pairs_hook=unique_object)


def integer(value, label, minimum, maximum):
    if type(value) is not int or not minimum <= value <= maximum:
        raise ValueError(f"invalid {label}")
    return value


def legacy_crc32(data):
    """Match persisted Yaze hashes; new hashes use standard zlib CRC32.

    These fifteen historical table defects are preserved in util/rom_hash.cc.
    Generate the standard IEEE table and replace only the defective entries.
    """
    table = []
    for value in range(256):
        for _ in range(8):
            value = (value >> 1) ^ (0xEDB88320 if value & 1 else 0)
        table.append(value)
    table[237:252] = [
        0xDEDE86C5, 0x47D7857F, 0x30D095E9, 0xBDDC8A1C, 0xCADBB48A,
        0x53D39330, 0x24D4A3A6, 0xBA906995, 0xCDCF094B, 0x54C65941,
        0x23C3B9D7, 0xB364A7AE, 0xC4632738, 0x5D6A1682, 0x2A6D0614,
    ]
    crc = 0xFFFFFFFF
    for value in data:
        crc = table[(crc ^ value) & 255] ^ (crc >> 8)
    return crc ^ 0xFFFFFFFF


def verify_expected_hash(expected, data):
    normalized = expected.strip().lower()
    if normalized.startswith("0x"):
        normalized = normalized[2:]
    if not normalized:
        return
    if any(c not in "0123456789abcdef" for c in normalized):
        raise ValueError("invalid project expected_hash")
    if len(normalized) == 40 and normalized == hashlib.sha1(data).hexdigest():
        return
    if len(normalized) == 8:
        if normalized == f"{zlib.crc32(data):08x}":
            return
        if normalized == f"{legacy_crc32(data):08x}":
            return
    raise ValueError("project expected_hash differs from the saved ROM (CRC32/SHA1)")


def native_json(value, depth=0):
    """Refuse metadata the native canonical-JSON parser cannot admit."""
    if depth > 16:
        raise ValueError("JSON nesting exceeds the native depth limit")
    if isinstance(value, dict):
        for key, child in value.items():
            if not isinstance(key, str):
                raise ValueError("JSON keys must be strings")
            key.encode("utf-8", errors="strict")
            native_json(child, depth + 1)
    elif isinstance(value, list):
        for child in value:
            native_json(child, depth + 1)
    elif type(value) is int:
        if not -(1 << 63) <= value < (1 << 63):
            raise ValueError("JSON integer exceeds signed 64-bit range")
    elif type(value) is str:
        value.encode("utf-8", errors="strict")
    elif value is not None and type(value) is not bool:
        raise ValueError("floating-point metadata is unsupported by native admission")


def footprint_schema(footprint):
    if (not isinstance(footprint, dict) or not FOOTPRINT_FIELDS <= set(footprint) or
            set(footprint) - FOOTPRINT_FIELDS - FOOTPRINT_OPTIONAL):
        raise ValueError("footprint has missing or unsupported editor fields")
    integer(footprint["schema_version"], "footprint schema_version", 1, 1)
    for kind in ("clearance", "ports"):
        constraints = footprint.get(kind, [])
        if not isinstance(constraints, list) or len(constraints) > 1024:
            raise ValueError(f"{kind} exceeds the native list limit")
        for guard in constraints:
            if (not isinstance(guard, dict) or not {"at", "allowed_tile16"} <= set(guard) or
                    set(guard) - {"at", "allowed_tile16", "name", "provenance"} or
                    ("name" in guard and not isinstance(guard["name"], str))):
                raise ValueError(f"unsupported {kind} constraint schema")
            allowed = guard["allowed_tile16"]
            if not isinstance(allowed, list) or not 1 <= len(allowed) <= 1024:
                raise ValueError(f"{kind} allowed Tile16 list exceeds the native limit")
    notes = footprint.get("unresolved_behavior", [])
    if (not isinstance(notes, list) or len(notes) > 1024 or
            any(not isinstance(note, str) for note in notes)):
        raise ValueError("unresolved_behavior must be a bounded string list")


def snapshot(project_path, observation_path):
    """Read and cross-check saved inputs; never open an editor or alter files."""
    inputs = {}

    def read(path):
        path = path.resolve(strict=True)
        data = path.read_bytes()
        if path in inputs and inputs[path] != data:
            raise ValueError(f"input changed during read: {path}")
        inputs[path] = data
        return data

    project_path = project_path.resolve(strict=True)
    config = configparser.ConfigParser(interpolation=None, strict=True)
    config.read_string(read(project_path).decode("utf-8"))
    root = project_path.parent
    rom_path = root / config.get("files", "rom_filename")
    terrain_path = root / config.get("files", "lyra_terrain_file")
    rom_bytes = read(rom_path)
    remainder = len(rom_bytes) % 0x8000
    if remainder not in (0, 512):
        raise ValueError("ROM must have whole LoROM windows and an optional 512-byte header")
    rom_body = rom_bytes[remainder:]
    if len(rom_body) < DEFINITIONS_PC + 4096 * 8:
        raise ValueError("ROM does not contain Oracle's expanded Tile16 table")
    verify_expected_hash(config.get("rom", "expected_hash", fallback=""), rom_body)
    terrain_bytes = read(terrain_path)
    document = read_json(terrain_bytes)
    observation = read_json(read(observation_path))
    if document.get("schema_version") != 3:
        raise ValueError("only schema-3 Lyra sidecars are supported")
    profile = document.get("profile")
    if not isinstance(profile, dict) or set(profile) != PROFILE_FIELDS:
        raise ValueError("unexpected shared Lyra profile schema")
    count = integer(profile["tile_count"], "tile_count", 1, 4096)
    definitions = rom_body[DEFINITIONS_PC:DEFINITIONS_PC + count * 8]
    if sha(definitions) != profile["tile16_definition_fingerprint"]:
        raise ValueError("saved ROM Tile16 definitions differ from the sidecar")
    if observation.get("source_rom_sha256") != sha(rom_body):
        raise ValueError("native observation belongs to another ROM")
    if observation.get("terrain_sha256") != sha(terrain_bytes):
        raise ValueError("native observation belongs to another terrain sidecar")
    if observation.get("profile") != profile or observation.get("map_count") != 32:
        raise ValueError("native observation does not verify the shared profile and 32 maps")
    maps = document.get("maps")
    if not isinstance(maps, list) or len(maps) != 32:
        raise ValueError("sidecar must contain all 32 Lyra maps")
    verified = observation.get("verified_asset_bindings")
    if not isinstance(verified, list) or len(verified) != 32:
        raise ValueError("native observation lacks 96 asset bindings")
    by_id = {}
    for index, area in enumerate(maps):
        map_id = integer(area.get("map_id"), "map ID", 0xA0, 0xBF)
        if map_id != 0xA0 + index:
            raise ValueError("maps must be in native sidecar order")
        if planner.grid(area.get("tiles16"), count) != (32, 32):
            raise ValueError("each map must contain 32x32 Tile16 cells")
        bindings = area["authoring"]["asset_bindings"]
        if (not isinstance(bindings, list) or len(bindings) != 3 or
                verified[index] != bindings):
            raise ValueError(f"native asset bindings differ for map ${map_id:02X}")
        by_id[map_id] = area
    binding = {
        "game_id": "oracle-of-secrets", "adapter_id": "lyra-sidecar-v3",
        # Editor admission hashes the copier header too; the native verifier's
        # source hash above is intentionally the headerless ROM body.
        "rom_sha256": sha(rom_bytes),
        "definition_fingerprint": profile["tile16_definition_fingerprint"],
        "profile_fingerprint": planner.digest(profile), "tile_count": count,
    }
    planner.binding(binding)
    return document, by_id, binding, inputs


def map_asset_binding(document, area, shared_binding):
    """Bind an exact map context after snapshot's 96-binding observation check."""
    assets = area["authoring"]["asset_bindings"]
    if (not isinstance(assets, list) or len(assets) != 3 or
            any(not isinstance(value, str) or len(value) != 64 or
                any(c not in "0123456789abcdef" for c in value) for value in assets)):
        raise ValueError("per-map asset bindings require three nonempty lowercase SHA-256 hashes")
    context = {
        "schema_version": 1, "kind": "lyra_map_asset_context",
        "map_id": area["map_id"], "profile": document["profile"],
        "properties": area["authoring"]["properties"], "asset_bindings": assets,
    }
    native_json(context)
    result = dict(shared_binding, adapter_id=MAP_ASSETS_ADAPTER,
                  profile_fingerprint=planner.digest(context))
    planner.binding(result)
    return result


def export(project_path, observation_path, map_id, *, footprint_path=None,
           placements=None, protected=None, per_map_assets=False):
    integer(map_id, "Lyra map ID", 0xA0, 0xBF)
    document, areas, binding, inputs = snapshot(project_path, observation_path)
    area = areas[map_id]
    properties = area["authoring"]["properties"]
    if (per_map_assets or
            any(properties.get(key) != document["profile"][key] for key in VISUAL_FIELDS) or
            any(properties.get(key) != [-1, -1, -1]
                for key in ("sprite_graphics", "sprite_palettes"))):
        binding = map_asset_binding(document, area, binding)
    protected = protected or []
    if len(protected) > 1024:
        raise ValueError("protected exceeds the native list limit")
    for item in protected:
        x, y = planner.pair(item, "protected cell")
        if x >= 32 or y >= 32:
            raise ValueError("protected cell outside destination")
    target = {
        "schema_version": 1, "units": "tile16", "binding": binding,
        "rows": area["tiles16"], "protected": protected,
        "provenance": {
            "project_sha256": sha(inputs[project_path.resolve()]),
            "terrain_sha256": sha(inputs[(project_path.parent / config_path(
                inputs[project_path.resolve()])).resolve()]),
            "native_observation_sha256": sha(inputs[observation_path.resolve()]),
            "map_id": map_id,
        },
    }
    result = target
    if footprint_path is not None:
        footprint_path = footprint_path.resolve(strict=True)
        footprint_bytes = footprint_path.read_bytes()
        inputs[footprint_path] = footprint_bytes
        footprint = read_json(footprint_bytes)
        footprint_schema(footprint)
        result = planner.lyra_bundle(target, footprint, placements, map_id)
    elif placements:
        raise ValueError("placements require a footprint")
    for path, before in inputs.items():
        if path.read_bytes() != before:
            raise ValueError(f"input changed during export: {path}")
    native_json(result)
    data = json.dumps(result, indent=2, sort_keys=True, ensure_ascii=True) + "\n"
    if len(data.encode()) > 1024 * 1024:
        raise ValueError("proposal exceeds the editor's 1 MiB limit")
    return data


def config_path(project_bytes):
    config = configparser.ConfigParser(interpolation=None, strict=True)
    config.read_string(project_bytes.decode("utf-8"))
    return config.get("files", "lyra_terrain_file")


def pair(text):
    try:
        result = [int(v, 0) for v in text.split(",")]
        planner.pair(result, "coordinate")
        return result
    except ValueError as exc:
        raise argparse.ArgumentTypeError("use nonnegative x,y Tile16 coordinates") from exc


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("project", type=Path)
    parser.add_argument("--native-receipt", type=Path, required=True,
                        help="exact saved lyra-verify-project JSON observation")
    parser.add_argument("--map", dest="map_id", type=lambda v: int(v, 0), required=True)
    parser.add_argument("--footprint", type=Path)
    parser.add_argument("--at", type=pair, action="append")
    parser.add_argument("--protect", type=pair, action="append")
    parser.add_argument("--per-map-assets", action="store_true",
                        help="bind this map's complete properties and verified state assets")
    parser.add_argument("--out", type=Path, required=True, help="new output file; never replaced")
    args = parser.parse_args()
    try:
        data = export(args.project, args.native_receipt, args.map_id,
                      footprint_path=args.footprint, placements=args.at, protected=args.protect,
                      per_map_assets=args.per_map_assets)
        with args.out.open("x", encoding="utf-8") as stream:
            stream.write(data)
        result = read_json(data)
        changes = len(result["report"]["changes"]) if "report" in result else 0
        print(json.dumps({"output": str(args.out), "map_id": args.map_id,
                          "kind": result.get("kind", "bound_tile16_target"), "changes": changes}))
    except (ValueError, KeyError, TypeError, configparser.Error, OSError) as exc:
        parser.exit(2, f"project proposal refused: {exc}\n")


if __name__ == "__main__":
    main()
