#!/usr/bin/env python3
"""Validate bound Tile16 footprints and report changes without editing a map.

This artifact planner deliberately has no ROM, editor, allocation or apply path.
Tile32 tuples are ordered top-left, top-right, bottom-left, bottom-right.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


BINDING_FIELDS = {"game_id", "adapter_id", "rom_sha256",
                  "definition_fingerprint", "profile_fingerprint", "tile_count"}


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":"),
                                    ensure_ascii=True).encode()).hexdigest()


def integer(value, label, minimum=0):
    if type(value) is not int or value < minimum:
        raise ValueError(f"{label} must be an integer >= {minimum}")
    return value


def binding(value):
    if not isinstance(value, dict) or set(value) != BINDING_FIELDS:
        raise ValueError("binding must contain exactly the declared identity fields")
    integer(value["tile_count"], "tile_count", 1)
    for key in BINDING_FIELDS - {"tile_count"}:
        if not isinstance(value[key], str) or not value[key]:
            raise ValueError(f"binding.{key} must be nonempty")
    for key in ("rom_sha256", "definition_fingerprint", "profile_fingerprint"):
        if len(value[key]) != 64 or any(c not in "0123456789abcdef" for c in value[key]):
            raise ValueError(f"binding.{key} must be a lowercase SHA-256")
    return value


def grid(rows, tile_count, allow_skip=False):
    if not isinstance(rows, list) or not rows or not isinstance(rows[0], list) or not rows[0]:
        raise ValueError("rows must be a nonempty rectangle")
    width = len(rows[0])
    for row in rows:
        if not isinstance(row, list) or len(row) != width:
            raise ValueError("rows must be rectangular")
        for tile in row:
            if tile is None and allow_skip:
                continue
            integer(tile, "Tile16")
            if tile >= tile_count:
                raise ValueError("Tile16 exceeds bound definition table")
    return width, len(rows)


def pair(value, label):
    if not isinstance(value, list) or len(value) != 2:
        raise ValueError(f"{label} must be [x,y]")
    return tuple(integer(v, label) for v in value)


def content_id(prefab):
    return digest({k: v for k, v in prefab.items() if k != "content_id"})


def plan(target, prefab, placements):
    """Return a deterministic proposal; never mutate any supplied object."""
    if not isinstance(target, dict) or not isinstance(prefab, dict):
        raise ValueError("target and footprint must be JSON objects")
    if target.get("schema_version") != 1 or prefab.get("schema_version") != 1:
        raise ValueError("unsupported schema_version")
    if target.get("units") != "tile16" or prefab.get("units") != "tile16":
        raise ValueError("units must be tile16")
    bound = binding(target.get("binding"))
    if binding(prefab.get("binding")) != bound:
        raise ValueError("prefab and destination bindings differ")
    if prefab.get("review_state") != "reviewed":
        raise ValueError("footprint must be explicitly reviewed")
    if not isinstance(prefab.get("semantic_id"), str) or not prefab["semantic_id"]:
        raise ValueError("semantic_id is required")
    if prefab.get("content_id") != content_id(prefab):
        raise ValueError("footprint content_id mismatch")
    rows = target["rows"]
    width, height = grid(rows, bound["tile_count"])
    if width % 2 or height % 2:
        raise ValueError("destination requires complete 2x2 Tile32 blocks")
    fw, fh = grid(prefab["rows"], bound["tile_count"], allow_skip=True)
    ax, ay = pair(prefab["anchor"], "anchor")
    if ax >= fw or ay >= fh:
        raise ValueError("anchor outside footprint")
    if not any(v is not None for row in prefab["rows"] for v in row):
        raise ValueError("footprint has no occupied cells")
    overlap = prefab.get("overlap_policy")
    if overlap not in ("reject", "same_only"):
        raise ValueError("overlap_policy must be reject or same_only")
    protected = set()
    for item in target.get("protected", []):
        x, y = pair(item, "protected cell")
        if x >= width or y >= height:
            raise ValueError("protected cell outside destination")
        protected.add((x, y))
    if not isinstance(placements, list) or not placements:
        raise ValueError("at least one placement is required")
    writes = {}
    guards = []
    occupied = []
    for index, at in enumerate(placements):
        px, py = pair(at, "placement")
        ox, oy = px - ax, py - ay
        for y, row in enumerate(prefab["rows"]):
            for x, tile in enumerate(row):
                if tile is None:
                    continue
                dx, dy = ox + x, oy + y
                if not (0 <= dx < width and 0 <= dy < height):
                    raise ValueError("occupied cell outside destination")
                pos = (dx, dy)
                if pos in protected and rows[dy][dx] != tile:
                    raise ValueError("placement changes a protected cell")
                if pos in writes and (overlap == "reject" or writes[pos] != tile):
                    raise ValueError("conflicting or forbidden footprint overlap")
                writes[pos] = tile
                occupied.append({"placement": index, "x": dx, "y": dy, "tile16": tile})
        for kind in ("clearance", "ports"):
            constraints = prefab.get(kind, [])
            if not isinstance(constraints, list) or any(not isinstance(g, dict) for g in constraints):
                raise ValueError(f"{kind} must be a list of constraint objects")
            for guard in constraints:
                gx, gy = pair(guard["at"], kind)
                dx, dy = ox + gx, oy + gy
                if not (0 <= dx < width and 0 <= dy < height):
                    raise ValueError(f"{kind} cell outside destination")
                allowed = guard["allowed_tile16"]
                if not isinstance(allowed, list) or not allowed:
                    raise ValueError(f"{kind} needs explicit allowed_tile16")
                grid([allowed], bound["tile_count"])
                guards.append((kind, dx, dy, allowed))
    # Check final proposed data, including writes from every placement.
    for kind, x, y, allowed in guards:
        if writes.get((x, y), rows[y][x]) not in allowed:
            raise ValueError(f"{kind} constraint fails")
    changes = [{"x": x, "y": y, "before": rows[y][x], "after": tile}
               for (x, y), tile in sorted(writes.items(), key=lambda p: (p[0][1], p[0][0]))
               if rows[y][x] != tile]
    touched = sorted({(c["x"] // 2, c["y"] // 2) for c in changes}, key=lambda p: (p[1], p[0]))
    inventory = target.get("tile32_inventory", {"complete": False, "entries": []})
    if not isinstance(inventory, dict):
        raise ValueError("tile32_inventory must be a JSON object")
    if type(inventory.get("complete")) is not bool:
        raise ValueError("tile32_inventory.complete must be boolean")
    if not isinstance(inventory.get("entries", []), list):
        raise ValueError("Tile32 entries must be a list")
    known = {}
    ids = set()
    for entry in inventory.get("entries", []):
        if not isinstance(entry, dict):
            raise ValueError("Tile32 entry must be an object")
        tile_id = integer(entry["id"], "Tile32 ID")
        if tile_id in ids:
            raise ValueError("duplicate Tile32 inventory ID")
        ids.add(tile_id)
        values = entry["tiles16"]
        if not isinstance(values, list) or len(values) != 4:
            raise ValueError("Tile32 entry needs four Tile16 values")
        grid([values], bound["tile_count"])
        known.setdefault(tuple(values), []).append(tile_id)
    blocks = []
    missing = set()
    for bx, by in touched:
        cells = [(bx * 2 + dx, by * 2 + dy) for dy in (0, 1) for dx in (0, 1)]
        before = [rows[y][x] for x, y in cells]
        after = tuple(writes.get((x, y), rows[y][x]) for x, y in cells)
        reuse = sorted(known.get(after, []))
        if not reuse:
            missing.add(after)
        blocks.append({"x": bx, "y": by, "before": before,
                       "after": list(after), "existing_ids": reuse})
    capacity = {"status": "unknown", "unseen_tuples": len(missing),
                "inventory_complete": inventory["complete"]}
    if inventory["complete"]:
        maximum = integer(inventory.get("capacity"), "Tile32 capacity", 1)
        if any(i >= maximum for i in ids):
            raise ValueError("Tile32 inventory ID exceeds capacity")
        # Require every source tuple in this target, so an empty inventory cannot claim completeness.
        for y in range(0, height, 2):
            for x in range(0, width, 2):
                source = tuple(rows[y + dy][x + dx] for dy in (0, 1) for dx in (0, 1))
                if source not in known:
                    raise ValueError("complete inventory omits a destination source tuple")
        available = maximum - len(ids)
        capacity.update(status="fits" if len(missing) <= available else "exceeded",
                        free_slots=available, new_tuples=len(missing))
        if capacity["status"] == "exceeded":
            raise ValueError("Tile32 capacity exceeded")
    return {"schema_version": 1, "kind": "read_only_tile16_proposal", "writes_enabled": False,
            "units": "tile16", "binding": dict(bound), "semantic_id": prefab["semantic_id"],
            "content_id": prefab["content_id"], "target_digest": digest(target),
            "request_digest": digest({"prefab": prefab, "placements": placements}),
            "placements": [list(p) for p in placements], "occupied": occupied,
            "changes": changes, "touched_tile32": blocks, "capacity": capacity,
            "unresolved_behavior": prefab.get("unresolved_behavior", []),
            "acceptance": "artifact proposal only; editor transaction and runtime unqualified"}


def lyra_bundle(target, prefab, placements, map_id):
    """Package complete read-only inputs for explicit editor admission."""
    integer(map_id, "Lyra map ID")
    if not 0xA0 <= map_id <= 0xBF:
        raise ValueError("Lyra map ID must be 0xA0..0xBF")
    if not isinstance(placements, list) or not 1 <= len(placements) <= 64:
        raise ValueError("Lyra bundles require 1..64 placements")
    result = plan(target, prefab, placements)
    if (len(target["rows"]) != 32 or len(target["rows"][0]) != 32 or
            target["binding"]["game_id"] != "oracle-of-secrets" or
            target["binding"]["adapter_id"] != "lyra-sidecar-v3"):
        raise ValueError("Lyra bundles require a bound 32x32 Oracle sidecar map")
    if len(prefab["rows"]) > 32 or len(prefab["rows"][0]) > 32:
        raise ValueError("Lyra bundle footprints must fit a 32x32 rectangle")
    if result["capacity"]["inventory_complete"]:
        raise ValueError("Lyra bundle admission does not support complete Tile32 inventories")
    return {"schema_version": 1, "kind": "lyra_tile16_proposal_bundle",
            "map_id": map_id, "target": target, "footprint": prefab,
            "report": result}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("target", type=Path)
    parser.add_argument("prefab", type=Path)
    parser.add_argument("--at", action="append", required=True, help="anchor x,y in Tile16 units")
    parser.add_argument("--out", type=Path, help="new output file, never overwritten")
    parser.add_argument("--bundle-map", type=lambda v: int(v, 0),
                        help="package full inputs for explicit Lyra admission; e.g. 0xA0")
    args = parser.parse_args()
    try:
        positions = [[int(v) for v in s.split(",")] for s in args.at]
        target = json.loads(args.target.read_text())
        prefab = json.loads(args.prefab.read_text())
        result = (plan(target, prefab, positions) if args.bundle_map is None else
                  lyra_bundle(target, prefab, positions, args.bundle_map))
        data = json.dumps(result, indent=2, sort_keys=True) + "\n"
        if args.bundle_map is not None and len(data.encode("utf-8")) > 1024 * 1024:
            raise ValueError("Lyra proposal bundle exceeds 1 MiB")
        if args.out:
            with args.out.open("x") as stream:
                stream.write(data)
        else:
            print(data, end="")
    except (ValueError, KeyError, TypeError, OSError) as exc:
        parser.exit(2, f"footprint proposal refused: {exc}\n")


if __name__ == "__main__":
    main()
