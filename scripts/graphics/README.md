# Graphics scripts

| Script | Purpose |
|--------|---------|
| `package_art_catalog.py` | Package verified indexed art kits for the Graphics Catalog reader |
| `snes_fx.js` | Render an effect module to SNES OBJ frames and an indexed PNG for `z3ed gfx-import` |

## SNES effect frames

`snes_fx.js` (Node 18+, no packages) renders an effect module such as
`fx/poof.js` to 16x16 or 32x32 OBJ frames, checks them against SNES limits,
and writes into a new directory:

- `<name>.png`: one row of 16x16 blocks, one per OBJ cell, for `gfx-import`
- `contact.png`: the frames side by side, scaled up for review
- `<name>.json`: checks, palette as BGR555 words, and each cell's sheet block
  and 8x8 tiles
- `<name>_oam.asm` (with `--slot`): `%DrawSprite()` tables for the frames

```sh
node scripts/graphics/snes_fx.js --fx scripts/graphics/fx/poof.js --out /tmp/poof-fx --block 3
z3ed gfx-import --rom game.sfc --sheet <id> --png /tmp/poof-fx/poof.png --block 3 --dry-run
z3ed gfx-import --rom game.sfc --sheet <id> --png /tmp/poof-fx/poof.png --block 3 --write --out edited.sfc
```

### ROM palette and draw tables

`--rom game.sfc --palette GROUP:INDEX[:FIRST]` reads a palette with
`z3ed palette-get-colors` (set `--z3ed` or `Z3ED` when z3ed is not on `PATH`)
and moves each effect color to the nearest of its first 7 colors, starting at
`FIRST` (default 0; `global_sprites` has 60). The frames then use colors the
game already loads. It warns when a color moves far and when two effect colors
merge into one ROM color, which flattens shading.

`--slot 0..3 --oam-palette 0..7 [--priority 3]` also writes `<name>_oam.asm`:
`.start_index`, `.nbr_of_tiles`, `.x_offsets`, `.y_offsets`, `.chr`,
`.properties`, `.sizes` and `.holds` for Oracle's `%DrawSprite()`
(`Core/sprite_macros.asm`). `incsrc` it right after `%DrawSprite()`; frame k
is `SprGfx+SprFrame` = k. `.chr` is slot * 64 + the cell's first tile, and the
effect origin lands on the center of the 16x16 body.

```sh
node scripts/graphics/snes_fx.js --fx scripts/graphics/fx/poof.js --out /tmp/poof-oam --block 3 \
  --rom game.sfc --palette sprites_aux3:0 --slot 1 --oam-palette 5
```

An effect exports `name`, `size`, `origin`, `holds` (60 Hz game frames per
frame), a `palette` of up to 7 `#rrggbb` colors for OBJ indices 1..7 (sheets
are 3bpp), a `seed`, and `draw(p, k, t, rng)`. The painter writes palette
indices only: `put`, `disc`, `ring`, `outline`; alpha below 1 is an
ordered-dither fade. `rng` is reseeded for every frame, so draw the same random
numbers in the same order each frame.

The run exits 1 on pixels outside the OBJ, an index past the palette, unseeded
randomness, more cells than the sheet has blocks after `--block`, or (with
`--slot`) a frame with no tiles. It warns
when a palette color changes on BGR555 snapping and when a frame touches the
OBJ edge. The JSON also reports colors used and OAM objects per frame, and
8x8 tiles that stay unique after H/V flips.

Ideas kept from pixelfx (MIT, github.com/patch-gg/pixelfx): seeded
closed-form particles, dither fades, dark-to-bright ramps, a check pass.
Dropped as not SNES-safe: translucency, additive light, arbitrary rotation,
120 Hz simulation, free-size canvases.
