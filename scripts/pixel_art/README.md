# Explicit pixel-art recipes

Inspect a reference, choose a palette and build small tile families without
changing their pixels through resampling or automatic color matching. This is a
reference-guided recipe generator, not a trained style model. The artist supplies
the source selection, palette indices, drawing and composition rules.

This standalone prototype requires Python 3.10+ and Pillow 9.1+ (verified with
Pillow 12.3.0). It does not integrate
with the Yaze GUI, load a ROM, write a ROM, install an app or fetch references.

## First use

Run these commands from the repository root:

```sh
# Inspect/validate the self-authored example without writing anything.
python3 scripts/pixel_art/pixel_art.py build \
  scripts/pixel_art/examples/synthetic.recipe.json --dry-run

# Export into a NEW directory whose parent already exists.
python3 scripts/pixel_art/pixel_art.py build \
  scripts/pixel_art/examples/synthetic.recipe.json --out /tmp/my-tile-family

# Read the exact palette and 8x8 metrics of your PNG or an explicit crop.
python3 scripts/pixel_art/pixel_art.py inspect /path/to/reference.png \
  --crop 272,80,16,16

# Focused tests; no ROM, emulator, C++ build, network or model needed.
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover \
  -s scripts/pixel_art/tests -v
```

`build` defaults to a dry run when `--out` is absent. Every validation and export
is computed before an output directory is created. Existing directories, files
and symlinks are refused. There is no overwrite flag. An I/O failure retains any
partial output for inspection; `manifest.json` is written last.

The example is an original geometric stone/inlay pattern written as indices.
No extracted Nintendo or ROM-hack artwork is checked in. See
[the reference catalog](references.catalog.template.json) and
[the reference guide](../../docs/reference/pixel_art_references.md) for organizing
local art without putting it in Git.

## Recipe version 1

The complete [synthetic example](examples/synthetic.recipe.json) demonstrates
an 8x8 character, a four-character 16x16 family and an explicit palette variant.
All coordinates are native pixels. Object fields are checked; unknown fields,
duplicate JSON keys and unsupported schema versions are refused.

| Field | Contract |
|---|---|
| `schema_version` | Integer `1`. |
| `name` | 1–64 ASCII letters, digits, underscores or hyphens. |
| `palette` | Ordered `#RRGGBB` or `#RRGGBBAA` colors. Index 0 must have alpha 0; every other entry is opaque. Duplicate RGB values retain their distinct indices. |
| `canvas` | `width`, `height`, `background` palette index. Width/height are positive multiples of 8, at most 1024 each. |
| `budget` | Required `max_tiles` and `max_unique_tiles`, counted in 8x8 characters. These are author-declared artifact limits, not free ROM allocations. |
| `sources` | Optional named PNGs with a path and exact lowercase SHA256. Relative paths resolve beside the recipe. |
| `assets` | Named explicit pixel grids, samples, assemblies or transformed copies. Forward references work; cycles and invalid unused definitions fail. |
| `stamps` | Ordered placements onto the final canvas. Later placements replace earlier indices, including transparent index 0. There is no alpha blending. |
| `exports` | Optional `bpp` (`3` or `4`, default `4`), integer `preview_scale` (`1..16`, default `8`) and boolean `snes_palette` (default `false`). |

The default is 4bpp with at most 16 palette entries. Selecting 3bpp restricts the
palette to 8 entries and the pixel indices to 0–7. Raw indices are never masked
to force them into range. Real Oracle dungeon graphics use 3bpp; selecting 4bpp
does not convert them into a compatible dungeon sheet.

### Drawing and assembling

An explicit pixel grid contains rows of integer palette indices:

```json
{"pixels": [[0,0,1,1,1,1,0,0], [0,1,2,2,2,2,1,0]]}
```

The abbreviated grid above illustrates the row syntax. An executable asset must
have a complete height and width aligned to 8 pixels.

An asset may instead use `{"from":"other-asset"}` or
`{"assemble":{"width":16,"height":16,"background":0,"stamps":[...]}}`.
Assemblies use the same stamp contract as the final canvas:

```json
{"asset":"corner", "at":[8,0], "flip_x":true,
 "remap":{"1":2}, "repeat":[1,2], "step":[8,8]}
```

`at` and `step` must align to 8 pixels. `repeat` contains positive counts; its
default is `[1,1]`. Default `step` is the transformed asset's width and height.
`flip_x` and `flip_y` are exact index reflections. `remap` contains canonical
decimal source-index keys and integer target indices; omitted indices retain
their values. Crop and placement bounds are checked before Pillow operations,
so padding and clipping cannot silently change a recipe.

### Sampling a reference

Every source is pinned. Changing the source bytes requires deliberately updating
its SHA256 in the recipe. `inspect` provides that hash, color frequencies,
source palette-index usage, and 8x8 uniqueness/color metrics. It reports
unprocessed partial edge dimensions and how many detailed tile metrics were
omitted by `--tile-limit` (default 64, maximum 512). Color-count fit, used-index
count and raw index-range fit are reported separately. Inspection performs no
palette reduction.

For an indexed (`P`) PNG, map the actual source indices explicitly:

```json
{
  "sources": {"detail": {"path":"my-reference.png", "sha256":"<64 lowercase hex digits>"}},
  "assets": {"detail-crop": {"sample": {
    "source":"detail", "crop":[272,80,16,16],
    "indices":{"24":1,"26":2,"27":3,"29":4,"41":5,"75":6}
  }}}
}
```

The fragment supplies fields to a complete recipe. Every index used by the crop
must be mapped. This preserves distinctions between duplicate RGB palette slots
and transparent/opaque pixels. The palette is chosen explicitly; mapping several
source indices onto one target is an intentional conversion recorded in the
recipe, not an automatic nearest-color match.

For RGB/RGBA PNGs, replace `indices` with `colors`, for example
`{"#D89048FF":1,"#784858FF":2}`. Every used RGBA value needs an explicit mapping.
Choose exactly one mapping form. Crop origins can select any source pixel;
sample width and height must align to 8. No sample is resized, dithered,
antialiased, color-managed or automatically quantized.

## Artifacts and ordering

| File | Contents |
|---|---|
| `atlas.png` | Lossless indexed PNG with the exact target indices, PLTE colors and index-0 transparency. Native canvas size. |
| `preview.png` | Integer nearest-neighbor enlargement only. Never used as tile data. |
| `tiles.3bpp` / `tiles.4bpp` | 24 or 32 bytes per 8x8 character. Raster character order: left-to-right, then top-to-bottom, without deduplication. |
| `palette.json` | Exact ordered RGBA palette. |
| `palette.bgr555` | Optional little-endian SNES palette: 8 or 16 entries, unused entries zero-filled. |
| `manifest.json` | Version, generator/source/recipe hashes, full recipe, palette, tile/family ordering, pixel-index hashes and hashes of every other artifact. No timestamps/output paths, so repeated builds of identical inputs are deterministic. |

3bpp uses interleaved planes 0/1 in bytes 0–15, followed by the contiguous third
plane in bytes 16–23. 4bpp interleaves planes 2/3 in bytes 16–31. Both use the
leftmost pixel as the highest bit. `sheet_columns8` and `families16` identify the
8x8 raster coordinates and each 16x16 family's top-left, top-right, bottom-left
and bottom-right character numbers. These are not SNES OBJ placement words,
which have separate tile-stride rules. No tilemap, VRAM assignment, metatile
table or Yaze import is generated.

Optional BGR555 export accepts only exact colors expanded from five bits using
Yaze's `(channel << 3) | (channel >> 2)` rule. Other RGB colors are refused rather
than truncated. Change the target palette explicitly or retain only
`palette.json`. SNES transparency is a pixel-index rule and is not encoded in
the BGR555 color words. A GBA reference can therefore be sampled losslessly into
PNG and raw indexed tile data while its hardware palette export remains off.

Reproducing byte-identical PNGs also requires the same Pillow/compression
runtime. The manifest records Pillow and generator identities. Provenance
identifies source bytes and operations; it does not assert rights or approve an
art direction. Inspect a generated preview before considering a separate editor
import or game-specific allocation.
