#!/usr/bin/env python3
"""Inspect references and compile explicit indexed pixel-art recipes to artifacts.

Requires Pillow. No model, network, ROM access, quantization or editor integration.
Run with --help, or see README.md beside this file.
"""
from __future__ import annotations

import argparse
from collections import Counter
from dataclasses import dataclass
import hashlib
import io
import json
from pathlib import Path
import re
import sys

from PIL import Image, __version__ as PILLOW_VERSION

VERSION = "1.0"
MAX_PIXELS = 1024 * 1024
MAX_SOURCE_PIXELS = 16 * 1024 * 1024
HEX_COLOR = re.compile(r"#[0-9a-fA-F]{6}(?:[0-9a-fA-F]{2})?\Z")
IDENTIFIER = re.compile(r"[a-zA-Z0-9_-]{1,64}\Z")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def integer(value, low, high, name):
    require(type(value) is int and low <= value <= high,
            f"{name} must be an integer in {low}..{high}")
    return value


def fields(value, allowed, required=(), name="object"):
    require(isinstance(value, dict), f"{name} must be an object")
    require(not set(value) - set(allowed), f"{name}: unknown fields {sorted(set(value) - set(allowed))}")
    require(set(required) <= set(value), f"{name}: missing fields {sorted(set(required) - set(value))}")


def pair(value, name, low=0, high=4096):
    require(isinstance(value, list) and len(value) == 2, f"{name} must contain two integers")
    return tuple(integer(v, low, high, name) for v in value)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def json_bytes(value):
    return (json.dumps(value, indent=2, sort_keys=True) + "\n").encode("utf-8")


def unique_keys(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, f"Duplicate JSON key: {key}")
        result[key] = value
    return result


def color(value):
    require(isinstance(value, str) and HEX_COLOR.fullmatch(value), f"Invalid color: {value!r}")
    raw = bytes.fromhex(value[1:])
    return tuple(raw if len(raw) == 4 else raw + b"\xff")


def color_name(rgba):
    return "#" + bytes(rgba).hex().upper()


def palette_colors(values):
    require(isinstance(values, list) and 1 <= len(values) <= 16, "Palette must have 1..16 entries")
    colors = tuple(color(value) for value in values)
    require(colors[0][3] == 0, "Palette index 0 must be transparent for SNES pixel semantics")
    for index, rgba in enumerate(colors):
        require(rgba[3] == 255 or (index == 0 and rgba[3] == 0),
                "Only palette index 0 may be transparent; partial alpha is unsupported")
    # Duplicate colors intentionally retain distinct palette indices.
    return colors


def dimensions(width, height):
    integer(width, 8, 1024, "width")
    integer(height, 8, 1024, "height")
    require(width % 8 == height % 8 == 0, "Dimensions must align to 8 pixels")
    require(width * height <= MAX_PIXELS, "Output exceeds pixel budget")


@dataclass(frozen=True)
class Raster:
    width: int
    height: int
    pixels: tuple[int, ...]

    def tiles(self):
        for y in range(0, self.height, 8):
            for x in range(0, self.width, 8):
                yield bytes(self.pixels[(y + row) * self.width + x + col]
                            for row in range(8) for col in range(8))


def decode_png(data):
    image = Image.open(io.BytesIO(data))
    require(image.format == "PNG", "Reference must be PNG")
    require(image.width * image.height <= MAX_SOURCE_PIXELS, "Reference exceeds 16-megapixel limit")
    require(image.mode in ("P", "RGB", "RGBA", "L", "LA", "1"),
            f"Unsupported PNG mode {image.mode}; no implicit depth conversion")
    image.load()
    return image


def cropped(image, box, aligned=False):
    require(isinstance(box, list) and len(box) == 4, "crop must be [x,y,width,height]")
    x, y, width, height = (integer(v, 0, MAX_SOURCE_PIXELS, "crop") for v in box)
    require(width > 0 and height > 0 and x + width <= image.width and y + height <= image.height,
            "Crop exceeds reference bounds or is empty")
    if aligned:
        dimensions(width, height)
    return image.crop((x, y, x + width, y + height))


def inspect_reference(path, box=None, tile_limit=64):
    """Describe decoded pixels; incomplete edge tiles are explicitly excluded."""
    integer(tile_limit, 0, 512, "tile_limit")
    data = Path(path).read_bytes()
    source = decode_png(data)
    box = box if box is not None else [0, 0, source.width, source.height]
    image = cropped(source, box)
    rgba = image.convert("RGBA")
    histogram = sorted(rgba.getcolors(image.width * image.height), key=lambda entry: entry[1])
    index_counts = Counter(image.tobytes()) if image.mode == "P" else None
    patterns, rgba_patterns, metrics = set(), set(), []
    for y in range(0, image.height - 7, 8):
        for x in range(0, image.width - 7, 8):
            tile = image.crop((x, y, x + 8, y + 8))
            raw = tile.tobytes()
            patterns.add(raw)
            rgba_patterns.add(tile.convert("RGBA").tobytes())
            if len(metrics) < tile_limit:
                metrics.append({"x": x, "y": y, "colors": len(tile.convert("RGBA").getcolors(64)),
                                "pixel_sha256": sha(raw)})
    total = (image.width // 8) * (image.height // 8)
    return {"source": {"path": str(Path(path).resolve()), "sha256": sha(data),
                       "size": list(source.size), "mode": source.mode},
            "crop": box, "size": list(image.size), "rgba_colors": len(histogram),
            "rgba_color_count_fits_16": len(histogram) <= 16,
            "used_index_count_fits_16": None if index_counts is None else len(index_counts) <= 16,
            "source_index_range_fits_4bpp": None if index_counts is None else max(index_counts) < 16,
            "source_index_range_fits_3bpp": None if index_counts is None else max(index_counts) < 8,
            "colors": [{"rgba": color_name(value), "pixels": count} for count, value in histogram],
            "source_indices": None if index_counts is None else dict(sorted(index_counts.items())),
            "grid8": {"complete_tiles": total, "unique_pixel_tiles": len(patterns),
                      "unique_rgba_tiles": len(rgba_patterns),
                      "partial_edge_pixels": [image.width % 8, image.height % 8],
                      "tiles": metrics, "omitted_tile_metrics": total - len(metrics)}}


def transform(raster, spec, palette_size):
    remap = spec.get("remap", {})
    require(isinstance(remap, dict), "remap must be an index-to-index object")
    table = list(range(palette_size))
    for key, value in remap.items():
        require(isinstance(key, str) and key.isdecimal() and str(int(key)) == key,
                "remap keys must be canonical decimal indices")
        table[integer(int(key), 0, palette_size - 1, "remap source")] = integer(
            value, 0, palette_size - 1, "remap target")
    for flag in ("flip_x", "flip_y"):
        require(type(spec.get(flag, False)) is bool, f"{flag} must be boolean")
    xs = range(raster.width - 1, -1, -1) if spec.get("flip_x") else range(raster.width)
    ys = range(raster.height - 1, -1, -1) if spec.get("flip_y") else range(raster.height)
    return Raster(raster.width, raster.height,
                  tuple(table[raster.pixels[y * raster.width + x]] for y in ys for x in xs))


class Compiler:
    def __init__(self, recipe, root):
        self.recipe, self.root = recipe, Path(root)
        fields(recipe, ("schema_version", "name", "palette", "canvas", "budget", "sources", "assets", "stamps", "exports"),
               ("schema_version", "name", "palette", "canvas", "budget", "assets", "stamps"), "recipe")
        require(type(recipe["schema_version"]) is int and recipe["schema_version"] == 1,
                "Only recipe schema_version 1 is supported")
        require(isinstance(recipe["name"], str) and IDENTIFIER.fullmatch(recipe["name"]), "Invalid recipe name")
        self.palette = palette_colors(recipe["palette"])
        exports = recipe.get("exports", {})
        fields(exports, ("preview_scale", "snes_palette", "bpp"), name="exports")
        self.bpp = integer(exports.get("bpp", 4), 3, 4, "bpp")
        require(len(self.palette) <= 1 << self.bpp,
                f"{self.bpp}bpp palette exceeds {1 << self.bpp} entries; explicitly remap the source")
        require(isinstance(recipe["assets"], dict) and 1 <= len(recipe["assets"]) <= 256,
                "assets must contain 1..256 definitions")
        self.assets, self.active, self.provenance, self.sources = {}, set(), [], {}
        source_specs = recipe.get("sources", {})
        require(isinstance(source_specs, dict) and len(source_specs) <= 16, "At most 16 sources are supported")
        for name, spec in source_specs.items():
            fields(spec, ("path", "sha256"), ("path", "sha256"), f"source {name}")
            require(isinstance(spec["path"], str), "Source path must be text")
            require(isinstance(spec["sha256"], str) and re.fullmatch(r"[0-9a-f]{64}", spec["sha256"]),
                    "Source requires a lowercase SHA256 pin")
            path = (self.root / spec["path"]).resolve()
            data = path.read_bytes()
            require(sha(data) == spec["sha256"], f"Source SHA256 differs: {name}")
            self.sources[name] = decode_png(data)
            self.provenance.append({"id": name, "path": str(path), "sha256": sha(data),
                                    "size": list(self.sources[name].size), "mode": self.sources[name].mode})

    def assemble(self, spec):
        fields(spec, ("width", "height", "background", "stamps"), ("width", "height", "background", "stamps"), "assembly")
        width, height = spec["width"], spec["height"]
        dimensions(width, height)
        pixels = [integer(spec["background"], 0, len(self.palette) - 1, "background")] * (width * height)
        require(isinstance(spec["stamps"], list) and len(spec["stamps"]) <= 4096, "Too many stamps")
        for stamp in spec["stamps"]:
            fields(stamp, ("asset", "at", "repeat", "step", "flip_x", "flip_y", "remap"), ("asset", "at"), "stamp")
            tile = transform(self.asset(stamp["asset"]), stamp, len(self.palette))
            x, y = pair(stamp["at"], "stamp at")
            nx, ny = pair(stamp.get("repeat", [1, 1]), "repeat", 1, 128)
            dx, dy = pair(stamp.get("step", [tile.width, tile.height]), "step", 8, 1024)
            require(all(v % 8 == 0 for v in (x, y, dx, dy)), "Stamp coordinates/steps must align to 8 pixels")
            require(x + (nx - 1) * dx + tile.width <= width and y + (ny - 1) * dy + tile.height <= height,
                    "Stamp exceeds canvas bounds")
            for iy in range(ny):
                for ix in range(nx):
                    for row in range(tile.height):
                        at = (y + iy * dy + row) * width + x + ix * dx
                        pixels[at:at + tile.width] = tile.pixels[row * tile.width:(row + 1) * tile.width]
        return Raster(width, height, tuple(pixels))

    def asset(self, name):
        require(isinstance(name, str) and name in self.recipe["assets"], f"Unknown asset: {name!r}")
        if name in self.assets:
            return self.assets[name]
        require(name not in self.active, f"Asset dependency cycle at {name}")
        self.active.add(name)
        spec = self.recipe["assets"][name]
        fields(spec, ("pixels", "sample", "assemble", "from", "flip_x", "flip_y", "remap"), name=f"asset {name}")
        require(sum(key in spec for key in ("pixels", "sample", "assemble", "from")) == 1,
                f"Asset {name} needs exactly one of pixels/sample/assemble/from")
        if "pixels" in spec:
            rows = spec["pixels"]
            require(isinstance(rows, list) and rows and isinstance(rows[0], list), "pixels must contain rows")
            width, height = len(rows[0]), len(rows)
            dimensions(width, height)
            require(all(isinstance(row, list) and len(row) == width for row in rows), "Pixel rows differ in width")
            raster = Raster(width, height, tuple(integer(v, 0, len(self.palette) - 1, "pixel index")
                                                 for row in rows for v in row))
        elif "sample" in spec:
            sample = spec["sample"]
            fields(sample, ("source", "crop", "colors", "indices"), ("source", "crop"), "sample")
            require(sample["source"] in self.sources, "Unknown sample source")
            require(("colors" in sample) != ("indices" in sample), "Sample needs exactly one explicit colors/indices mapping")
            image = cropped(self.sources[sample["source"]], sample["crop"], aligned=True)
            mapping = sample.get("indices", sample.get("colors"))
            require(isinstance(mapping, dict) and mapping, "Sample mapping must be nonempty")
            for value in mapping.values():
                integer(value, 0, len(self.palette) - 1, "sample target index")
            if "indices" in sample:
                require(image.mode == "P", "Source-index mapping requires an indexed PNG")
                for key in mapping:
                    require(key.isdecimal() and str(int(key)) == key and 0 <= int(key) <= 255,
                            "Source-index keys must be canonical decimal indices 0..255")
                source_pixels = [str(v) for v in image.tobytes()]
            else:
                parsed = {color_name(color(key)): value for key, value in mapping.items()}
                require(len(parsed) == len(mapping), "Duplicate normalized source colors")
                mapping = parsed
                rgba = image.convert("RGBA").tobytes()
                source_pixels = [color_name(rgba[i:i + 4]) for i in range(0, len(rgba), 4)]
            missing = set(source_pixels) - set(mapping)
            require(not missing, f"Unmapped source colors/indices: {sorted(missing)[:16]}")
            raster = Raster(image.width, image.height, tuple(mapping[v] for v in source_pixels))
        elif "assemble" in spec:
            raster = self.assemble(spec["assemble"])
        else:
            raster = self.asset(spec["from"])
        raster = transform(raster, spec, len(self.palette))
        self.active.remove(name)
        self.assets[name] = raster
        return raster

    def compile(self):
        # Validate unused definitions too; a typo must not remain hidden.
        for name in self.recipe["assets"]:
            self.asset(name)
        canvas = self.recipe["canvas"]
        fields(canvas, ("width", "height", "background"), ("width", "height", "background"), "canvas")
        result = self.assemble({**canvas, "stamps": self.recipe["stamps"]})
        budget = self.recipe["budget"]
        fields(budget, ("max_tiles", "max_unique_tiles"), ("max_tiles", "max_unique_tiles"), "budget")
        count, unique = result.width * result.height // 64, len(set(result.tiles()))
        require(count <= integer(budget["max_tiles"], 1, 16384, "max_tiles"), "Tile budget exceeded")
        require(unique <= integer(budget["max_unique_tiles"], 1, 16384, "max_unique_tiles"), "Unique tile budget exceeded")
        return result


def encode_bpp(raster, bpp=4):
    integer(bpp, 3, 4, "bpp")
    dimensions(raster.width, raster.height)
    require(len(raster.pixels) == raster.width * raster.height, "Raster pixel count differs")
    for value in raster.pixels:
        integer(value, 0, (1 << bpp) - 1, f"{bpp}bpp pixel index")
    result = bytearray()
    for tile in raster.tiles():
        packed = bytearray(bpp * 8)
        for y in range(8):
            for x in range(8):
                value = tile[y * 8 + x]
                for plane in range(bpp):
                    offset = 16 + y if bpp == 3 and plane == 2 else (plane // 2) * 16 + y * 2 + plane % 2
                    packed[offset] |= ((value >> plane) & 1) << (7 - x)
        result.extend(packed)
    return bytes(result)


def encode_palette(palette, bpp=4):
    integer(bpp, 3, 4, "bpp")
    require(1 <= len(palette) <= 1 << bpp, "Palette exceeds bit depth")
    expanded = {(v << 3) | (v >> 2): v for v in range(32)}
    result = bytearray()
    for rgba in palette:
        require(all(c in expanded for c in rgba[:3]),
                "SNES palette requires exact bit-replicated 5-bit RGB channels; choose an explicit target palette")
        r, g, b = (expanded[c] for c in rgba[:3])
        result.extend((r | g << 5 | b << 10).to_bytes(2, "little"))
    return bytes(result) + bytes(((1 << bpp) - len(palette)) * 2)


def png_bytes(raster, palette, scale=1):
    image = Image.frombytes("P", (raster.width, raster.height), bytes(raster.pixels))
    image.putpalette(bytes(c for rgba in palette for c in rgba[:3]) + bytes((16 - len(palette)) * 3))
    if palette[0][3] == 0:
        image.info["transparency"] = bytes([0] + [255] * 15)
    if scale != 1:
        image = image.resize((image.width * scale, image.height * scale), Image.Resampling.NEAREST)
    output = io.BytesIO()
    image.save(output, "PNG", bits=4, optimize=False, compress_level=9)
    return output.getvalue()


def build_artifacts(recipe_path):
    path = Path(recipe_path).resolve()
    raw = path.read_bytes()
    recipe = json.loads(raw, object_pairs_hook=unique_keys)
    compiler = Compiler(recipe, path.parent)
    raster = compiler.compile()
    exports = recipe.get("exports", {})
    fields(exports, ("preview_scale", "snes_palette", "bpp"), name="exports")
    scale = integer(exports.get("preview_scale", 8), 1, 16, "preview_scale")
    require(raster.width * raster.height * scale * scale <= MAX_SOURCE_PIXELS,
            "Preview exceeds 16-megapixel limit; reduce integer scale")
    require(type(exports.get("snes_palette", False)) is bool, "snes_palette must be boolean")
    artifacts = {"atlas.png": png_bytes(raster, compiler.palette),
                 "preview.png": png_bytes(raster, compiler.palette, scale),
                 f"tiles.{compiler.bpp}bpp": encode_bpp(raster, compiler.bpp),
                 "palette.json": json_bytes([color_name(c) for c in compiler.palette])}
    if exports.get("snes_palette"):
        artifacts["palette.bgr555"] = encode_palette(compiler.palette, compiler.bpp)
    tiles = list(raster.tiles())
    columns = raster.width // 8
    manifest = {"schema_version": 1, "generator": "yaze-explicit-pixel-recipe", "generator_version": VERSION,
                "generator_sha256": sha(Path(__file__).read_bytes()), "pillow_version": PILLOW_VERSION,
                "recipe_path": str(path), "recipe_sha256": sha(raw), "recipe": recipe,
                "sources": compiler.provenance, "size": [raster.width, raster.height],
                "palette": [color_name(c) for c in compiler.palette],
                "tile_count": len(tiles), "unique_tile_count": len(set(tiles)),
                "tile_order": "8x8 characters, left-to-right then top-to-bottom; no deduplication",
                "bpp": compiler.bpp, "sheet_columns8": columns,
                "planar_format": ("SNES 4bpp, 32 bytes/character, planes01 then23, MSB-left" if compiler.bpp == 4
                                  else "SNES 3bpp, 24 bytes/character, planes01 then contiguous plane2, MSB-left"),
                "palette_format": "little-endian BGR555; exact bit-replicated RGB; index0 transparency not encoded",
                "pixel_indices_sha256": sha(bytes(raster.pixels)),
                "tile_sha256": [sha(tile) for tile in tiles],
                "families16": [{"x": x, "y": y, "characters": [y // 8 * columns + x // 8 + dy * columns + dx
                                                                         for dy, dx in ((0, 0), (0, 1), (1, 0), (1, 1))]}
                               for y in range(0, raster.height - 15, 16) for x in range(0, raster.width - 15, 16)],
                "artifacts": {name: {"bytes": len(data), "sha256": sha(data)} for name, data in artifacts.items()}}
    artifacts["manifest.json"] = json_bytes(manifest)
    return artifacts, manifest


def publish(artifacts, destination):
    """Reserve a new directory exclusively. Never overwrite files or directories."""
    allowed = {"atlas.png", "preview.png", "tiles.3bpp", "tiles.4bpp", "palette.json", "palette.bgr555", "manifest.json"}
    require(set(artifacts) <= allowed and "manifest.json" in artifacts,
            "Only fixed artifact filenames may be published")
    destination = Path(destination)
    require(not destination.exists() and not destination.is_symlink(), "Output already exists; choose a new directory")
    require(destination.parent.is_dir(), "Output parent must already exist")
    destination.mkdir(exist_ok=False)
    # All compilation/validation precedes publication. Preserve partial output
    # on I/O failure for diagnosis rather than deleting an unexpected file.
    for name, data in artifacts.items():
        with (destination / name).open("xb") as stream:
            stream.write(data)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    inspect = sub.add_parser("inspect", help="Inspect exact PNG colors and 8x8 metrics")
    inspect.add_argument("png", type=Path)
    inspect.add_argument("--crop", help="x,y,width,height; exact pixels")
    inspect.add_argument("--tile-limit", type=int, default=64)
    build = sub.add_parser("build", help="Validate/build a version1 recipe; dry-run by default")
    build.add_argument("recipe", type=Path)
    destination = build.add_mutually_exclusive_group()
    destination.add_argument("--out", type=Path, help="Create this new artifact directory")
    destination.add_argument("--dry-run", action="store_true", help="Validate and report without writing")
    args = parser.parse_args(argv)
    try:
        if args.command == "inspect":
            box = [int(v) for v in args.crop.split(",")] if args.crop else None
            result = inspect_reference(args.png, box, args.tile_limit)
        else:
            artifacts, manifest = build_artifacts(args.recipe)
            if args.out is not None:
                publish(artifacts, args.out)
            result = {"dry_run": args.out is None, "output": None if args.out is None else str(args.out.resolve()),
                      "size": manifest["size"], "tile_count": manifest["tile_count"],
                      "unique_tile_count": manifest["unique_tile_count"],
                      "artifacts": {name: {"bytes": len(data), "sha256": sha(data)} for name, data in artifacts.items()}}
        print(json.dumps(result, indent=2, sort_keys=True))
        return 0
    except (ValueError, OSError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
