#!/usr/bin/env python3
"""Fill/copy Lyra Tile16 rectangles in an existing sidecar; never opens a ROM.

Operations are a JSON array, applied in order (all numbers are JSON integers):
  {"op":"fill", "map":160, "rect":[x,y,width,height], "tile":707}
  {"op":"copy", "from_map":160, "rect":[x,y,width,height],
   "to_map":161, "at":[destination_x,destination_y]}
Without --output, only a dry-run summary is printed. --output must be new.
Profile, authoring and unknown fields are retained as JSON values.
"""

import argparse
import copy
import json
from pathlib import Path
import sys


def require(condition, message):
    if not condition:
        raise ValueError(message)


def integer(value, low, high, label):
    require(type(value) is int and low <= value <= high,
            f"{label} must be an integer in {low}..{high}")
    return value


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, f"Duplicate JSON key: {key}")
        result[key] = value
    return result


def invalid_constant(value):
    raise ValueError(f"Invalid JSON constant: {value}")


def read_json(path):
    require(path.stat().st_size <= 4 * 1024 * 1024,
            "JSON input exceeds 4 MiB")
    return json.loads(path.read_text(encoding="utf-8"),
                      object_pairs_hook=unique_object,
                      parse_constant=invalid_constant)


def validate_document(document):
    require(type(document) is dict
            and document.get("schema") == "yaze.lyra-terrain",
            "Expected a yaze.lyra-terrain document")
    integer(document.get("schema_version"), 1, 3, "schema_version")
    for name, value in (("columns", 8), ("rows", 4), ("map_width", 32),
                        ("map_height", 32), ("tile_pixels", 16)):
        require(type(document.get(name)) is int and document[name] == value,
                f"{name} must be {value}")
    profile = document.get("profile")
    require(type(profile) is dict, "Expected a profile object")
    count = integer(profile.get("tile_count"), 1, 65536, "profile.tile_count")
    maps = document.get("maps")
    require(type(maps) is list and len(maps) == 32, "Expected all 32 maps")
    indexed = {}
    for item in maps:
        require(type(item) is dict, "Expected map objects")
        map_id = integer(item.get("map_id"), 160, 191, "map_id")
        require(map_id not in indexed, f"Duplicate map_id: {map_id}")
        grid = item.get("tiles16")
        require(type(grid) is list and len(grid) == 32,
                f"Map {map_id} requires 32 rows")
        for row in grid:
            require(type(row) is list and len(row) == 32,
                    f"Map {map_id} requires 32 columns")
            for tile in row:
                integer(tile, 0, count - 1, f"Map {map_id} Tile16")
        indexed[map_id] = grid
    return indexed, count


def rectangle(value):
    require(type(value) is list and len(value) == 4,
            "rect must be [x,y,width,height]")
    x, y, width, height = value
    integer(x, 0, 31, "rect.x")
    integer(y, 0, 31, "rect.y")
    integer(width, 1, 32 - x, "rect.width")
    integer(height, 1, 32 - y, "rect.height")
    return x, y, width, height


def edit(document, operations):
    """Return a separate edited document; a failed operation changes no input."""
    result = copy.deepcopy(document)
    maps, tile_count = validate_document(result)
    require(type(operations) is list, "Operations must be a JSON array")
    for number, operation in enumerate(operations, 1):
        try:
            require(type(operation) is dict, "Expected an operation object")
            name = operation.get("op")
            fields = {"fill": {"op", "map", "rect", "tile"},
                      "copy": {"op", "from_map", "rect", "to_map", "at"}}
            require(type(name) is str and name in fields,
                    "op must be fill or copy")
            require(set(operation) == fields[name],
                    f"{name} requires exactly {sorted(fields[name])}")
            x, y, width, height = rectangle(operation["rect"])
            if name == "fill":
                target = maps[integer(operation["map"], 160, 191, "map")]
                tile = integer(operation["tile"], 0, tile_count - 1, "tile")
                patch = [[tile] * width for _ in range(height)]
            else:
                source = maps[integer(operation["from_map"], 160, 191,
                                      "from_map")]
                target = maps[integer(operation["to_map"], 160, 191, "to_map")]
                patch = [row[x:x + width] for row in source[y:y + height]]
                at = operation["at"]
                require(type(at) is list and len(at) == 2,
                        "at must be [destination_x,destination_y]")
                x = integer(at[0], 0, 32 - width, "destination_x")
                y = integer(at[1], 0, 32 - height, "destination_y")
            for offset, row in enumerate(patch):
                target[y + offset][x:x + width] = row
        except ValueError as error:
            raise ValueError(f"Operation {number}: {error}") from error
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--input", type=Path, required=True,
                        help="Existing Lyra terrain sidecar JSON")
    parser.add_argument("--operations", type=Path, required=True,
                        help="JSON array of fill/copy operations")
    parser.add_argument("--output", type=Path,
                        help="New sidecar path; omit for a dry run")
    args = parser.parse_args(argv)
    try:
        if args.output is not None:
            require(args.output.resolve() not in
                    (args.input.resolve(), args.operations.resolve()),
                    "Output aliases an input")
            require(not args.output.exists() and not args.output.is_symlink(),
                    "Output already exists")
        original = read_json(args.input)
        operations = read_json(args.operations)
        result = edit(original, operations)
        before, _ = validate_document(original)
        after, _ = validate_document(result)
        changed = {f"{map_id:02X}": sum(a != b
                   for row_a, row_b in zip(before[map_id], after[map_id])
                   for a, b in zip(row_a, row_b)) for map_id in before}
        summary = {"dry_run": args.output is None, "operations": len(operations),
                   "changed_cells": sum(changed.values()),
                   "changed_maps": {key: value for key, value in changed.items()
                                    if value},
                   "output": str(args.output) if args.output else None}
        encoded = json.dumps(result, indent=2, ensure_ascii=False,
                             allow_nan=False) + "\n"
        if args.output is not None:
            # Exclusive creation also refuses destinations appearing after preflight.
            with args.output.open("x", encoding="utf-8") as stream:
                stream.write(encoded)
        print(json.dumps(summary, sort_keys=True))
        return 0
    except (OSError, ValueError, RecursionError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
