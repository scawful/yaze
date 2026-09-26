# D6 minecart report (2026-09-26)

Read-only. Nothing was written to the Oracle repo. All commands ran on copies.

## Inputs

- `Roms/oos168.sfc` and `Data/dungeons/custom_collision.json`, copied together.
  The base ROM SHA-256 is `027619bc98c9…`, the same as the original.
- `z3ed` built from `claude/editor-integration` at `81baa372e`.
- The Oracle project sets no dungeon overlay, so the defaults apply: minecart
  sprite `0xA3`, stop tiles `B7–BA`.
- Rooms: the 15 D6 track-start rooms from `goron_mines_minecart_design.md`
  (`0x98,0x88,0x87,0x77,0xA8,0xB8,0xB9,0x78,0x89,0xDA,0xD9,0xD7,0x79,0x97,0xD8`).

## Matched pair

The ROM's custom collision (`dungeon-export-custom-collision-json`) equals
`custom_collision.json`: 17 rooms each, 0 differing. `0x78` and `0x79` are in
neither.

## Per room (`dungeon-minecart-audit`, `dungeon-minecart-map`)

Tile coordinates are 8-pixel tiles, and a cart's tile is its top-left tile.

| Room | Custom collision | Track tiles | Stop tiles | Carts (tile x,y; on a stop?) | Issue |
|---|---|---|---|---|---|
| 0x98 | yes | 0 | 1 | (50,48) yes; (50,48) yes | none |
| 0x88 | yes | 0 | 1 | (44,26) yes | none |
| 0x87 | yes | 173 | 12 | (46,48), (14,26), (14,34), (44,6): all yes | none |
| 0x77 | yes | 15 | 12 | none | none |
| **0xA8** | yes | 314 | 8 (B7 at x 13–16, y 44–45) | **(14,48) no** | cart not on a stop |
| **0xB8** | yes | 1464 | **2** (B7 at 14,2; B8 at 14,32) | none | see note |
| 0xB9 | yes | 26 | 4 | none | none |
| **0x78** | **no** | 0 | 0 | **(24,46) no** | no custom collision |
| 0x89 | yes | 62 | 16 | (14,34) yes; (14,16) yes | none |
| **0xDA** | yes | 19 | 4 (B7 at x 51–54, y 10) | **(52,12) no** | cart not on a stop |
| 0xD9 | yes | 11 | 4 | none | none |
| 0xD7 | yes | 21 | 22 | none | none |
| **0x79** | **no** | 0 | 0 | **(14,28) no** | no custom collision |
| 0x97 | yes | 1 | 2 | (8,44), (8,44), (16,12): all yes | none |
| **0xD8** | yes | 32 | 16 (B9 at x 14, y 13–16; BA at x 52–53, y 13–16 and x 56, y 53–56) | **(44,54) no; (16,14) no** | carts not on a stop |

The audit reports issues in 5 of 15 rooms.

### Nearest stop for each misplaced cart

| Room | Cart tile (sprite x,y) | Nearest stop | Distance | Suggested fix |
|---|---|---|---|---|
| 0xA8 | (14,48) sprite (7,24) | (14,45) B7 | 3 tiles | Move the cart up to tile y 44 (sprite y 22) |
| 0xDA | (52,12) sprite (26,6) | (52,10) B7 | 2 tiles | Move the cart up to tile y 10 (sprite y 5) |
| 0xD8 | (16,14) sprite (8,7) | (14,14) B9 | 2 tiles | Move the cart left to tile x 14 (sprite x 7) |
| 0xD8 | (44,54) sprite (22,27) | (56,54) BA | 12 tiles | Too far to call a nudge: add a stop at (44,54) or move the cart; design choice |

## Against the February audit

| Claim | Now |
|---|---|
| 0x78 and 0x79 have no custom collision | **Still true** |
| 0xB8 has no stop tiles | **No longer true**: it now has 2 (B7 at 14,2; B8 at 14,32) |
| 0xA8, 0x89, 0xDA, 0xD8 carts are not on a stop tile | 0xA8, 0xDA and 0xD8 **still true**. 0x89 is **fixed** (both carts on B7/B8), as the doc's 2026-09-15 note says |

Note on 0xB8: its collision covers x 14–63 and y 0–63 with 1248 track and 216
junction tiles. That is far more than any other room, and the dry-run generator
would produce only 202 tiles from the room's rail objects. It may be deliberate,
since 0xB8 is a large rail room, or it may be leftover fill. It is worth one look
in the editor's collision overlay.

## What `dungeon-generate-track-collision` would change (dry run, `--preserve-stops`)

The ROM copy was byte-identical after the run.

| Room | Tiles generated from rail objects | Existing collision tiles |
|---|---|---|
| 0x78 | 564 (12 corners) | none |
| 0x79 | 100 (4 corners) | none |
| 0x89 | 228 (10 corners) | 78 |
| 0xA8 | 40 (4 corners) | 322 |
| 0xB8 | 202 (5 corners) | 1466 |
| 0xD8 | 288 (12 corners) | 48 |
| 0xDA | 196 (9 corners) | 23 |

- The generator creates **no stop tiles**. It keeps existing ones only.
- For 0x78 and 0x79 it is a plausible starting point, but the result would still
  have no stops, so carts there would stay invalid.
- For rooms with existing collision, the output differs a lot from what is there.
  Do not apply it wholesale; it would replace hand-authored collision.

## Not done

- Mesen2 cross-check. No savestate exists for these rooms. The only D6 states are
  `0x87` and `0x88`, saved on a February `oos168x` build (SHA-1 `65db5efc…`), so
  they would not load cleanly on a current build. Next step: make a fresh state
  in one of the rooms on a current build, then read the cart position and the
  collision tile under it.

## Reproduce

```sh
Z=~/src/hobby/yaze-worktrees/validate-placement/build/presets/mac-ai/bin/Debug/z3ed
$Z dungeon-minecart-audit --rom=<copy> --rooms 0x98,0x88,0x87,0x77,0xA8,0xB8,0xB9,0x78,0x89,0xDA,0xD9,0xD7,0x79,0x97,0xD8 --format json
$Z dungeon-minecart-map --rom=<copy> --room 0xA8 --format json
$Z dungeon-generate-track-collision --rom=<copy> --rooms 0x78,0x79,0x89,0xA8,0xB8,0xD8,0xDA --preserve-stops --format json   # no --write: dry run
$Z dungeon-export-custom-collision-json --rom=<copy> --out <file> --all
```
