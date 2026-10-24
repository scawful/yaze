# Dungeon tileset coverage

A dungeon art family needs complete wall, corner, floor, door and layer behavior,
not only a seamless atlas. This guide connects the
[Sky family profile](../../scripts/pixel_art/profiles/oracle-sky-dungeon-family.json)
to [routine-family bindings](../../scripts/pixel_art/contracts/dungeon_object_families.json)
and the [source inventory tool](../../scripts/pixel_art/INVENTORY.md).

For the larger reference-to-placement method, see the
[object authoring workflow](zelda-object-authoring-workflow.md), which covers
overworld terrain rules and dungeon room/route proposals.

The profile is advisory data. It is not an executable pixel recipe, a ROM patch,
an allocated graphics sheet or a claim that the art is complete. Use the
[exact pixel recipe tool](../../scripts/pixel_art/README.md) for original indexed
pixels and explicit palette mappings. Production import and allocation are
separate steps.

## First use

From the repository root, validate the source inventory and family joins:

```sh
python3 -B scripts/pixel_art/dungeon_inventory.py \
  --family-bindings scripts/pixel_art/contracts/dungeon_object_families.json \
  --check
```

The extractor reads repository sources, not a ROM. Its family check joins all
registered routines and both default/custom-enabled object routes to this
contract. A successful source join does not measure characters used by a room.
The default check also validates the profile's 40 repository source references,
family IDs, module backlinks and unallocated palette bindings. Two optional
disassembly references remain explicitly listed as unbound. Supplying
`--usdasm-root` validates those two references as well.
See the [inventory CLI guide](../../scripts/pixel_art/INVENTORY.md) for saving
a generated inventory and optional independently pinned disassembly references.

Then inspect the profile's `families`, `draft_modules` and
`palette_contract.per_family_bindings`. Drawing coverage, engine binding and
palette allocation are separate statuses.

## Three different drawing units

| Unit | Meaning | Contract |
|---|---|---|
| 8×8 character | Hardware-addressed raster | The tilemap word holds its 10-bit character ID. |
| 16×16 drawing module | Convenient composition of four characters | It may be only part of an engine object. |
| Object payload | Ordered tilemap words consumed by a draw routine | Dimensions, ordering, repeat counts, context and layers come from the routine. |

A tilemap word stores character bits 0–9, palette bank bits 10–12, priority bit 13,
H flip bit 14 and V flip bit 15. See
[TileInfo](../../src/app/gfx/types/snes_tile.h#L45).
A palette bank, graphics-sheet number, character ID, object ID, routine ID and
room tilemap coordinate are distinct identifiers.

The eight sample modules are a partial drawing baseline. Their 64×32 atlas
contains 32 character positions and 24 unique characters. Those are drawing
metrics, not available ROM/VRAM capacity or a production tileset budget.
The profile distributes sample metadata without raster assets.

A full/half-height wall requirement is an art requirement. Do not equate it
automatically with the source's top/bottom wall variants, a half-room quadrant,
a lower-floor plane or a Left/Right palette half. Short partitions, endcaps and
height changes need explicit object bindings.

## Layers and palettes

Yaze's semantic BG numbers differ from the PPU background numbers:

| Semantic map | Yaze buffer | WRAM | Hardware |
|---|---|---|---|
| Upper | BG1 | TILEMAPA, `$7E2000` | BG2 |
| Lower | BG2 | TILEMAPB, `$7E4000` | BG1 |

The `BG3` object enum represents list 2, the upper overlay stream. It does not
add a third dungeon art layer. TM/TS register bits refer to hardware BG numbers.
See [room layer registers](../../src/zelda3/dungeon/room_layer_registers.h#L11).

Both maps share the dungeon main palette. The other three palette-set slots
select sprite auxiliaries. The room palette-set byte also requires main-palette
indirection. It is not a direct palette index.
See [palette resolution](../../src/zelda3/dungeon/room.cc#L921),
[shared application](../../src/zelda3/dungeon/room.cc#L1429) and
[palette-set fields](../../src/app/editor/graphics/paletteset_editor_panel.cc#L119).

A complete pack can use multiple palette banks and several explicit 3bpp color
sets. Main dungeon CGRAM rows 2–7 each contain 15 opaque entries plus transparency.
A 3bpp source has indices 0–7. Runtime expansion uses either:

| Loaded slot mode | Expanded pixel indices |
|---|---|
| Left | 0–7 |
| Right | 0 remains 0; nonzero 1–7 become 9–15 |

The graphics group and loaded slot choose the half; BG number does not.
See [palette-half selection](../../src/zelda3/dungeon/room.cc#L1085) and
[expansion](../../src/zelda3/dungeon/room.cc#L1157).
The profile's eight-color sample is not a pack-wide palette allocation.

The current recipe CLI accepts one palette per recipe: at most eight entries for
3bpp, including transparent index 0. Author and export separate bound families
when they need different color sets. This advisory profile can describe multiple
sets, but it does not extend the recipe grammar or render a complete multi-bank
CGRAM scene. Actual bank/half assignment and tilemap-word remapping remain later
integration work.

Each production family needs a palette-set/main-palette identity, tilemap bank,
loaded slot, Left/Right half and explicit index-to-CGRAM mapping. The profile
leaves all 24 family bindings unallocated. Audit animation, reserved entries and
shared sheet consumers before assigning them.

## Whole-room composition

Keep source pass order separate from tile priority and final PPU composition.

1. Header floors paint lower and upper base maps using separate selectors.
2. Layout objects draw after floors.
3. Primary object list 0 targets the upper map by default.
4. Overlay list 1 targets the lower map.
5. Overlay list 2 targets the upper map again.

Doors, chest-hole overlays, global blocks/torches and dynamic effects add paths
outside the regular object registry. The event index continues across object
lists. See [room rendering](../../src/zelda3/dungeon/room.cc#L1686),
[list mapping](../../src/zelda3/dungeon/room_object.h#L294) and
[special tables](../../src/zelda3/dungeon/room.cc#L1789).

BothBG is engine routing. The drawer fans registered routines out to both maps.
Other routines have fixed or mixed routes. Custom-object data can preempt built-in
routing. Use
[effective layer semantics](../../src/zelda3/dungeon/object_layer_semantics.h#L51)
and [dispatch](../../src/zelda3/dungeon/object_drawer.cc#L465), not only a routine
name suffix. One historically named BothBG routine is explicitly single-layer:
[spaced rightwards columns](../../src/zelda3/dungeon/draw_routines/rightwards_routines.cc#L178).

Room BGACT, darkness, effects and water flags can change visibility or color math.
“Upper map” alone does not determine whether a pixel covers Link or another sprite.
See [register derivation](../../src/zelda3/dungeon/room_layer_registers.cc#L15).

## Required art families

All 24 families remain in scope. An isolated sample is not a completed family.

| ID | Family | Required coverage | Baseline sample coverage |
|---|---|---|---|
| F01 | Base floors and patterned fills | both header-selected floor fields; plain paving; alternate/inlaid/special floor patterns; 32 x 32 floor-chunk composition; ice/conveyor/invisible/spiked floor art and directional variants | Partial: floor, blue-inlay |
| F02 | Cardinal top walls | north; south; west; east; straight repeat; start/end join; full wall-face depth | Partial: wall-face, horizontal-cap |
| F03 | Cardinal bottom walls | north; south; west; east; BothBG structural variants; full wall-face depth; top/bottom level transitions | Missing |
| F04 | Wall columns and deep walls | four cardinal column faces; four cardinal deep walls; pilaster endcaps; column-to-flat-wall joins | Partial: door-jamb-left |
| F05 | Short partitions and wall termination | full-height-to-short-height join; short partition sides; end caps; inside/outside junction; height-change shadow | Partial: horizontal-cap |
| F06 | Top wall inside and outside corners | four concave orientations; four convex orientations; 32 x 32 canonical composite; joins to both cardinal directions | Partial: platform-outer-sw-corner, platform-inner-nw-corner |
| F07 | Bottom wall inside and outside corners | four concave orientations; four convex orientations; 32 x 32 canonical composite; BothBG coverage | Missing |
| F08 | Kinked and deep corners | north/south kink variants; west/east kink variants; deep concave variants; deep convex variants; 24 x 32 and 32 x 24 footprints | Missing |
| F09 | Diagonal wall faces | A/B/C top variants; A/B/C bottom variants; four named orientations; acute/grave slopes; flat-to-diagonal joins | Missing |
| F10 | Ceilings and diagonal ceiling masks | horizontal/vertical ceiling repeats; large ceiling fill; all four diagonal ceiling algorithms; ceiling-to-wall/corner joins; mask boundary | Missing |
| F11 | Door openings, lintels, jambs and state art | north/south/west/east; upper/lower; open/closed; normal/key/big-key/shutter; bomb/crack/curtain/exit joins; special large exits; priority-only markers | Partial: door-jamb-left |
| F12 | Raised platforms and lower-floor transitions | four visible edges; four exterior corners; four interior corners; wall-to-platform join; rail/bridge transition; chest-platform open/closed joins | Partial: platform-south-edge, platform-outer-sw-corner, platform-inner-nw-corner |
| F13 | Stairs and ladders | platform stairs; fat interroom stairs; spiral up/down upper/lower; auto merged/multilayer; straight north/south upper/lower; swim/water-hop; north/south ladder; flank/front priority joins | Missing |
| F14 | Pits, voids and pit-edge trims | north/south/west/east; edge cap/middle/end; four inner/outer corners; diagonal masks; large holes; full lower-map reveal; floor-to-void shadow | Partial: platform-south-edge, platform-outer-sw-corner, platform-inner-nw-corner |
| F15 | Water, banks and flood states | water-floor patterns; four bank directions; waterfalls/faces; drained/filled state; floodgate joins; water-hop stairs; state-dependent overlay geometry | Missing |
| F16 | Rails, bridges, bars and supports | horizontal/vertical rails; thick/long rails; wall rails; caps/tees/corners; prison/bar pieces; support columns; bridge-to-floor edge | Missing |
| F17 | Overlay and foreground occlusion | upper-only; lower-only; BothBG duplicate; mixed/fixed routes; transparent overwrite; pixel reveal; rectangular reveal; low/high priority; main/subscreen blend; occlusion over Link/sprites | Missing |
| F18 | Moving walls and replacement state | west/east motion families; initial/shifted/replaced floor; exposed wall/rail/platform joins | Missing |
| F19 | Interactive room fixtures | pot; pushable block; unlit/lit torch; small/big chest open/closed; key lock; pressure/fake plate; pegs; warp/telepathy tile; bombable/fake floor; spike blocks and hazard fixture art | Missing |
| F20 | Somaria and other track-like paths | horizontal/vertical segments; endpoints; all corner orientations; tee/cross/crossover; state/contrast against floor | Missing |
| F21 | Furniture and room utilities | bed; table/bowl; shelves/bookcases; drapes/carpet/trim; toolrack/furnace/anvil; tubs/range/bottles; rocks/statues/columns | Missing |
| F22 | Large facades and special-purpose objects | sanctuary wall; altar; fortune-teller interior; prison; large wall/floor ornaments; boss shells/goo; pipes and junctions; game targets; directional cannon holes; small wall holes/shooters; arrow and special marker tiles | Missing |
| F23 | Light, animated and effect overlays | window/floor sunlight; lamp cone; floor light; animated graphics frames; animated palette/state; dark-room readability | Missing |
| F24 | Object, stream and tilemap coverage ledger | all mapped object IDs; project custom overrides; all size/state branches in use; floor/layout/door/global passes; both physical maps; overwritten and priority-only writes | Missing |

The profile records per-family source references and constraints. Its families
are drawing categories; the source inventory owns exhaustive object IDs.

### Footprints and joins

- **Cardinal walls:** horizontal strips can consume 2×4 characters, vertical
  strips 4×2. Preserve repeat rules, zero-size cases, depth and directional
  lighting. See [horizontal](../../src/zelda3/dungeon/draw_routines/rightwards_routines.cc#L77)
  and [vertical](../../src/zelda3/dungeon/draw_routines/downwards_routines.cc#L77) routines.
- **Corners:** canonical 4×4 corners span 32×32 pixels and consume 16 words in
  column-major order. Weird corners use 3×4 or 4×3 characters. Short-payload
  fallbacks do not establish canonical coverage. See
  [corner payloads](../../src/zelda3/dungeon/draw_routines/corner_routines.cc#L114).
- **Diagonals:** wall routines use five-character columns across `size+6`
  steps. Ceiling triangles have side `size+4`; the four algorithms do not
  share one mirrored anchor. See
  [diagonal walls](../../src/zelda3/dungeon/draw_routines/diagonal_routines.cc#L12)
  and [ceiling algorithms](../../src/zelda3/dungeon/draw_routines/corner_routines.cc#L67).
- **Doors/stairs:** door art and priority changes can extend beyond the visible
  opening. A south fancy exit can span 10×8 characters. Spiral stair rasters
  use 4×3, auto/straight stairs 4×4; mixed lower stairs expose an upper-map edge.
  Include surrounding priority-only mutations. See
  [door spans](../../src/zelda3/dungeon/object_drawer.cc#L1928),
  [large exits](../../src/zelda3/dungeon/object_drawer.cc#L2115) and
  [stairs](../../src/zelda3/dungeon/draw_routines/special_routines.cc#L1238).
- **Pits/overlays:** a transparent tilemap write still replaces the old entry.
  Pixel reveal and rectangular reveal masks differ. Retain transparent cutouts.
  See [masks](../../src/zelda3/dungeon/object_drawer.cc#L581) and
  [overwrite semantics](../../src/zelda3/dungeon/object_drawer.cc#L3056).
- **Water/effects:** a static stamp does not establish HDMA, drained/filled
  geometry, palette animation, color math or dynamic occlusion.
  See [water overlay limits](../../src/zelda3/dungeon/draw_routines/special_routines.cc#L1116).
- **Conditional joins:** rail/pit caps inspect existing character IDs.
  Reassigning those IDs requires contextual verification as well as a new atlas.
  See [conditional edges](../../src/zelda3/dungeon/draw_routines/rightwards_routines.cc#L294).

Review all orientations together at native scale. H/V flips can reverse
lighting, trim or material cues; storage reuse is conditional on correct actual
tilemap transforms.

## Machine-readable join

The profile and bindings use `schema_version:1`. They are advisory contracts,
not input schemas for `pixel_art.py build`.

- The profile's `families[].id` identifies 24 drawing categories.
  `draft_modules[]` distinguishes drawn samples from unverified engine bindings.
- The binding's `bindings[].routine_id` joins inventory `routines[].id`.
  Object `routine_id_default` and `routine_id_custom_enabled` join the same key.
  Object metadata stays in generated inventory rather than being duplicated.
- Each binding has a primary family, secondary families or an explicit
  unclassified/no-draw/custom-data status. `source_basis` retains definition
  and registration references. Source-file fingerprints identify source
  compatibility; a Git HEAD change from documentation alone does not make the
  renderer contract stale.
- `dispatch_exception_keys` covers the union of default and custom-enabled
  routes. A base no-draw routine can still have an object-specific mask override.
  A stateful chest or lock can bypass generic drawing.
- The profile marks taxonomy accounting separately from
  `actual_character_usage:OPEN_UNMEASURED` and
  `production_engine_bindings:OPEN_UNALLOCATED`.

The current source baseline accounts for 135 routines,448 metadata IDs and 440
serializable objects. It has 104 semantic family assignments,13 named-use
assignments,16 unclassified multipurpose primitives,1 base no-draw routine with
possible overrides and 1 unclassified external custom routine. Canonical top
corner IDs 0x100–0x107 use generic routine 116. Registered corner helper 19 has no
default-mapped objects. Counts are source inventory, not art completeness.

The checker must reject a missing/duplicate routine row, incompatible source
fingerprints, unknown family/status or an unresolved object-to-routine join.
Source changes require an explicit review of the affected pins and contract;
the checker does not accept new fingerprints automatically.
Generic primitives keep their classification gaps until per-object payload and
context review resolves them. No keyword-derived family guess establishes art.

## Completion evidence

1. Give every applicable object/payload a redraw, reviewed reuse or explicit scope
   disposition. Resolve multipurpose/custom data per object, not only per routine.
2. Include floor/layout/door/global/state/animation paths outside registry rows.
3. Bind actual loaded sheets, shared consumers, characters, palette banks and
   Left/Right halves before production storage selection.
4. Replay ordered room rendering into disposable logical buffers. Preserve both
   maps, source word bits, writes, masks, priority-only changes and final results.
5. Review joined structural families and actual-room behavior before import.
   Dynamic effects, sprites and gameplay need corresponding runtime evidence.

Raw traces preserve layer, palette, priority and flips, but
[ObjectTileLayout::FromTraces](../../src/zelda3/dungeon/object_tile_editor.cc#L158)
coalesces by x/y without layer. A
[trace-only draw](../../src/zelda3/dungeon/object_drawer.cc#L2911) skips logical
tilemap updates used by conditional routines. Neither alone is complete room
usage evidence. The inventory extractor reports source metadata only.

All eight sample modules retain missing engine mappings and unverified joined
compositions. The family pack needs more art, complete per-object dispositions,
measured room usage and allocation. Exact planar export is useful evidence for
pixel encoding; it does not replace those steps.
