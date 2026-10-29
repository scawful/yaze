# Dungeon spatial explanation and pattern composition

Use this workflow before placing a related group of dungeon objects. It gives
human authors and agents the same coordinate frame, object measurements and
placement decisions. The first commands explain an existing room and plan a
translated object template. They do not write a ROM.

## Explain the room before choosing objects

```sh
z3ed --rom=game.sfc dungeon-describe-room --room=0x26 \
  --project-context=project.yaze --spatial --format=json
```

Add `--spatial-cells` for individual tile writes. Add `--spatial-collision` to
render the room and compute the existing vanilla room-load collision model.
Together, those flags include the two collision grids. Without project context,
the command cannot know source-owned custom artwork or pattern overrides.

The `spatial` result separates these concepts:

| Concept | Meaning | Consequence for placement |
|---|---|---|
| Coordinates | Room-local Tile8 cells: 64×64 cells, 8 pixels each; bounds are half-open | Object anchors use this frame. Sprites use a separate 16-pixel grid. A quadrant is 32×32 Tile8 cells. |
| Layout | A shared object template selected by object-stream byte 1, bits 2–4 | Shared walls are part of the baseline, even when absent from the room's individual object list. This is separate from the authored dungeon route graph. |
| Draw list | Primary list 0, lower overlay list 1, final upper overlay list 2 | Preserve order within each list. The engine fixes the list-pass order. A list is not a gameplay floor. |
| Rendered buffers | Yaze BG1 means upper/TILEMAPA/hardware BG2; Yaze BG2 means lower/TILEMAPB/hardware BG1 | A routine can use stored placement, fixed routing, mixed routing or both buffers. Register-derived priority, transparency and color math affect visibility. |
| Collision and state | Upper/lower collision maps, room collision key, room flags, special object/door passes and project hooks | Visible art alone cannot establish walking, swimming, layer changes or water-level gates. |

`floor1_graphics` and `floor2_graphics` select floor tile patterns. Neither is
a dungeon floor number or a collision-plane selector. `size` is also an encoded
routine-specific field; it can mean repetition, two packed axes, or a custom
subtype. Use measured bounds rather than `size+1` or a guessed rectangle.

Each footprint includes its original `object_index`, anchor, draw list,
measured bounds, effective Yaze buffer routing, per-buffer write counts and
diagnostics. Layout and room objects occupy separate namespaces. Sorting the
report for draw order does not change their original indices.

The current footprint model combines isolated `ObjectDrawer` traces with
standalone `ObjectGeometry` bounds. It excludes prior-room tilemap context,
priority-only changes, pixel reveal masks and complete event-registration
history. An observed trace may be clipped. Strict planning additionally requires
`full_bounds_measured`; mixed routing, unqualified custom overrides and explicit
state contexts cannot silently use an estimated rectangle.

## Compose an ordered pattern

Patterns use the existing `ObjectTemplate` payload:

```json
{
  "name": "Pattern name",
  "description": "Purpose and external dependencies",
  "category": "Measured reference",
  "objects": [
    {"id": 4, "rel_x": 4, "rel_y": 0, "size": 3, "layer": 0}
  ]
}
```

IDs and other JSON integers are decimal. Relative offsets are Tile8 coordinates.
The strict reader requires these exact fields and rejects unknown rule keys,
duplicate fields, partial objects, noninteger numbers and unsupported ranges.
It accepts 1–512 objects and at most 1 MiB of input. A separate rule description
must not be passed as if the template parser had validated it.

Plan the complete group at one explicit origin:

```sh
z3ed --rom=game.sfc dungeon-template-plan --room=0x96 \
  --template=assets/templates/dungeon/swamp-island-wall-shell.json \
  --x=8 --y=8 --clearance=12,12,8,16,both --format=json
```

Use a room selected by its actual owner. The room number above is an inspection
example, not a reservation or permission to replace that room.

The planner checks canonical object encoding, translated anchors, full bounds,
draw-list order, baseline layout/object writes, typed door editor bounds and
explicit reserved regions. It emits all ordered placements or rejects unsafe
construction; it never returns an apparently successful partial group.

Important output contracts:

1. `valid=true` establishes the reported default-state geometry checks only.
   Allocation, graphics compatibility, actor/reward dependencies, room states,
   event registration and movement acceptance remain separately unverified.
2. `plan.existing_objects` maps every existing index to `layout` or `room` and
   its original `source_index`. Use this map for `--allow-existing=0,3` or
   `--allow-pairs=2:4,5:7`; do not substitute array positions from another report.
3. Internal template overlap is an ordered-repaint observation. Existing-object
   overlap blocks unless explicitly allowed. Permissions apply to identified
   overlaps and never bypass reserved regions or unknown footprint checks.
4. A `--clearance=x,y,width,height,upper|lower|both` box reserves space against
   this new group's drawn cells. It does not assert that existing terrain is
   empty or walkable. Existing doors additionally reserve their typed editor
   draw bounds on both buffers; these are not player approach envelopes.
5. Blocked plans return a failing command status and structured `plan.conflicts`.
   Counts cover the complete intersection; a diagnostic keeps its first 64 cells.
   `--write` is rejected. No writer, partial insertion, generic rotation or
   procedural expansion is hidden behind this command.

## Measured reference patterns

The bundled reference templates are factual translations from the clean USA
ALTTP room `$36`, not a prescribed style for every Oracle dungeon:

| Template | Members | Proven relationship |
|---|---|---|
| `swamp-island-wall-shell.json` | Original ordered objects 58–65 | A 16×24 Tile8 wall/corner shell with an 8×16 interior at relative `(4,4)`. The shared wall length is quantized by its draw routine. |
| `swamp-island-bank-assembly.json` | Shell, water-facing strips, chest and stairs | Preserves the larger ordered assembly for inspection. Floor2 graphics `$0E`, target art, chest bookkeeping, stair registration and gameplay require their own contracts. |

Reference ROM SHA256:
`66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`.
The shell's source origin is `(23,21)`. Translation preserves relative offsets,
IDs, sizes and order. The larger template is useful for exposing unsupported
dependencies; it must not be called a completed water puzzle because its
rectangles fit. Do not copy its blockset, palette or floor selectors into Oracle
without target-room evidence.

Other useful pattern families can share this contract: a door approach with
supports outside its route, a hazard island with a deliberate access port,
a minecart stop with a dismount region, or a reward alcove with a chest-front
envelope. Their spacing belongs to a named room/pattern profile. There is no
universal spacing constant that makes all four playable.

## Relational rules for authored rooms

Describe a room using its role, ports, states and preserved regions before
expanding patterns. A useful agent brief states concrete postconditions:

- **Geometry:** identified shell, opening and interior regions; supported
  size quantization; full bounds; allowed ordered overlap; preserved landmarks.
- **Access:** typed door/stair port, arrival plane and approach region; obstacle
  state; declared player/body model for static path checks.
- **State:** explicit LOW/HIGH or other state variants, expected reachable
  regions, collision-plane changes and which controller causes them.
- **Actors:** item/reward records, sprite channels and staging space for Link,
  followers and NPC interactions. Visual anchors do not substitute for these.
- **Acceptance:** geometry report, target render/collision, native interaction
  and human layout feedback, with each result kept at its actual scope.

For a Zora receiving basin, for example, the authored contract can require all
of the room to drain in LOW, a coherent HIGH water connection to the source
room, an isolated raised bank in LOW and an actual water-stair approach in HIGH.
Those are room-state requirements. Clearing a few water rectangles does not
prove them. A water-hop operator can retain visible tiles after a later merged
stair resets its registration. A dam operator can update global state as well
as draw artwork. Such operations need complete ordered assemblies and native
state evidence.

For Goron Mines, apply the same process to cart routes, boarding/dismount
regions, switches and doorway approaches. For Sky Tower, include pits, landing
regions and layer transitions. For Fortress, validate typed links to the retained
room set rather than treating every registry entry as active or every header
destination as a usable edge. Existing `dungeon-graph`/room-link APIs distinguish
actual links from unused header references.

## Generation scaffold

Both deterministic and procedural generation can feed the same pipeline:

```text
authored room role + states + typed ports + preserved regions
  -> select versioned patterns and explicit parameters
  -> expand existing ObjectTemplate groups
  -> deterministic template placement and conflict reports
  -> composed render/collision and typed route checks
  -> reviewed transaction through existing Room/save/COW writer
  -> native and human acceptance
```

The first implementation supplies room explanation and the geometry-plan step.
It does not implement graph synthesis, automatic spacing choices or an apply
command. A later seeded expander should record recipe version, seed, ROM hash,
project/custom-asset hashes and engine version, then emit the same concrete
ordered objects. Equal declared inputs must produce equal plans. Unmodeled
state predicates remain unresolved, rather than becoming random choices or
being accepted from appearance.
