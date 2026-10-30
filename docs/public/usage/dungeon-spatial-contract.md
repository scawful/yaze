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
Graphics loading leaves the ROM size unchanged. Floor operators `$C4/$DB` use
the room's floor-pattern selectors; standalone measurements read them from the
persisted object stream, while room analysis uses the supplied Room's selectors.

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

## Check the connection to existing walls

A copied assembly can have correct dimensions while erasing the corner that
terminates an existing wall. Inspect retaining-wall connections before calling
the room proposal ready:

```sh
z3ed --rom=candidate.sfc dungeon-describe-room --room=0x26 \
  --project-context=project.yaze --spatial --spatial-strict-joins --format=json
```

`spatial.retaining_wall_joins` reports known native architectural endpoint loss,
including the original face/corner indices, buffer planes, affected cells and
later wall objects. Strict mode returns a failing status for a detected loss or
an unassessed known retaining-wall object; the structured diagnosis is retained.
Ordinary `--spatial` reports the same findings without changing its inspection
exit behavior.

The first supported relation is a native horizontal `$003` face ending at an
east `$10C` corner, including the compatible north-side `$063` approach that
shares only the corner's top row. Mirrored, shifted or deeper wall replacement
does not qualify as that approach. The rejected Zora bank keeps `$003(21,44,size7)` and
`$10C(37,44)` in its stream, but new `$064(37,43,size1)` and `$10E(37,47)`
replace all 16 corner cells on each buffer. Keeping the old corner record is
insufficient: its architectural connection no longer survives.

This check evaluates the architectural wall pass. It excludes later water,
floor and decoration repaint, which valid vanilla assemblies use intentionally.
It does not require every final wall pixel to survive. Custom replacements,
unmeasured bounds and other wall grammars remain unqualified. The result always
sets `complete_room_qualification=false`; `no_known_endpoint_loss` is not a
general visual, collision or gameplay approval.

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
The strict reader requires these fields and accepts the optional
`retaining_wall_loops` contract described below. It rejects other unknown keys,
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
   Known native retaining-wall endpoint loss also blocks. Allocation, graphics
   compatibility, actor/reward dependencies, room states,
   event registration and movement acceptance remain separately unverified.
2. `plan.existing_objects` maps every existing index to `layout` or `room` and
   its original `source_index`. Use this map for `--allow-existing=0,3` or
   `--allow-pairs=2:4,5:7`; do not substitute array positions from another report.
3. Internal template overlap is normally an ordered-repaint observation. New
   architectural endpoint loss is a separate blocking `wall_join_loss` conflict.
   Existing-object
   overlap blocks unless explicitly allowed. Permissions apply to identified
   overlaps and never bypass wall-join loss, reserved regions or unknown
   footprint checks. An unrelated preexisting wall issue is not attributed to
   the new placement.
4. A `--clearance=x,y,width,height,upper|lower|both` box reserves space against
   this new group's drawn cells. It does not assert that existing terrain is
   empty or walkable. Existing doors additionally reserve their typed editor
   draw bounds on both buffers; these are not player approach envelopes.
5. Blocked plans return a failing command status and structured `plan.conflicts`.
   Counts cover the complete intersection; a diagnostic keeps its first 64 cells.
   `--write` is rejected. No writer, partial insertion, generic rotation or
   procedural expansion is hidden behind this command.

## Declare the complete retaining-wall assembly

For a complete dark retaining contour, add an independent membership contract:

```json
"retaining_wall_loops": [
  {"name": "Complete stepped perimeter", "members": [0, 1, 2, 3, 4, 5, 6, 7]}
]
```

Member numbers refer to `objects` in this same template, before painter-order
sorting. They are not room-object indices. Removing, moving or flipping a
member must preserve the declared connections; deleting a corner cannot remove
its missing-connection witness. Changing the membership is a change to the
authored contract and template hash. No declaration means `not_declared`, not
an assembly pass.

The native profile covers `$003/$004/$063/$064` and `$108..$10F`. The dark
wall's narrow rim is part of this family; gray `$001/$002/$061/$062` room walls
are a different architectural material. Each member has two oriented ports
with a required straight-wall family. Port coordinates lie on Tile8 boundaries,
so a 4x4 corner's east port is `x+4`, not its last cell at `x+3`.

The planner requires one closed connected cycle, matching families, opposite
directions and supported boundary alignment. The source-qualified north
`$063` approach to `$10C` may share one corner row. Other overlaps do not gain
that exception. Direct corner-to-corner connections are unsupported by this
profile: a rectangle touching another rectangle is insufficient evidence for
the required straight shoulder. Unsupported connections block this contract;
they are not a claim that every native corner-to-corner arrangement is invalid.

The architectural check also detects later recognized gray-wall writes that
replace declared dark-wall cells with different words. Painter order uses the
serialized draw lists, not template array order alone. Floor, water and
decoration repaint are excluded from this wall-stage check.

`plan.retaining_wall_loops` reports coverage, every required port, matched
connections and diagnostics. A successful result is
`closed_ports_not_final_pixels`; `complete_room_qualification` remains false.
`assembly_connection` conflicts block even with explicit overlap permissions.
Their `expected_region` identifies the missing or incompatible connection on
the room canvas. Final ceiling/water composition, target graphics, collision,
standing space and state behavior remain separate requirements.

`swamp-stepped-retaining-contour.json` supplies the complete 48-member dark
stepped perimeter from clean USA room `$36`, original objects 10 through 57,
at source origin `(5,7)`. It contains all eight corner orientations. It is a
complete architectural reference rather than an isolated cap or a target-room
replacement. The normal template manager preserves the membership on save and
reload. The legacy direct insertion path refuses declared assemblies before
creating Undo; use the planner and the reviewed room writer rather than losing
the contract during insertion.

## Open the planner diagnosis in Proposal Preview

Save the normal JSON report and load that same file in the Dungeon Workbench's
floating **Proposal Preview**:

```sh
z3ed --rom=game.sfc dungeon-template-plan --room=0x36 \
  --template=assets/templates/dungeon/swamp-stepped-retaining-contour.json \
  --x=5 --y=7 --format=json > retaining-plan.json
```

Preserve the command's exit status. A blocked report is still useful for visual
review; it is not permission to apply the objects. Existing-room overlap may
block this reference invocation and must be reviewed by identified indices.

`plan.proposal_overlay` uses the existing overlay v1 schema. Preview accepts a
raw plan, the normal `{ "plan": ... }` output, or its supported
`{ "Result": { "plan": ... } }` formatter wrapper. Ambiguous reports,
duplicate keys, unsupported wrappers, malformed provenance and room mismatches
are rejected. ROM and template hashes are displayed as saved producer labels;
they are not automatically matched to the current editor session.

Blue rectangles show proposed measured bounds. Purple rectangles show reserved
regions. Red annotations show blocked connections or placement conflicts; the
first 64 blocking conflicts are displayed and the report retains every finding.
The underlying room remains the loaded room. These annotations do not render or
apply candidate objects, establish walkable floor, or change the ROM.

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

## Build a pattern vocabulary from examples

Keep reference studies small and entrance-anchored. Pin the source-file hash,
copier-header offset, reader version and project/custom-art profile. A separate
payload-view hash can reveal header variants; it does not replace the original
file identity. Filenames, modification dates and checksum-complement validity
alone cannot establish the latest playable release. Preserve ambiguous versions.

Reuse `CollectRoomLinks` and its maintained graph command for typed connections.
Reciprocal doors, instantiated stair slots, pits and unused header destinations
have different evidence. A structurally strong edge does not prove that Link
has its key, can reach its approach, or arrives in the intended region/plane.
Progression requirements and effects belong above those existing facts: inventory,
fungible key counts, one-time pickups/unlocks, reversible state and return routes.
An internal locked door can divide one room into several gameplay regions.

An example becomes a pattern by naming the relationship that serves its role:
an aligned approach with varied rhythm, a reward visible before its route opens,
a water-fed island with a return port, or a prison encounter with staging space.
Retain the exact measured assembly as evidence, then distinguish that evidence
from authored choices about silhouette, pacing, art and deliberate exceptions.
Object count and graph degree do not establish room quality or justify removal.

Use the existing proposal preview to compare two or three concrete arrangements
before expanding the chosen one into ordered templates. Keep a companion brief
for intent, preserved landmarks, ports/arrival/approach, states, actor dependencies,
spacing profile, exact overlap permissions and unknowns; these are not additional
keys accepted by the strict template parser. Human feedback selects the design.
When converting half-open measured bounds to an inclusive preview rectangle,
use `right = x + width - 1` and `bottom = y + height - 1`.

The bundled `assets/proposals/dungeon/zora-reservoir-review.json` demonstrates
one complete two-room review: current bank and puzzle, proposed supply join,
Princess/Baby staging, intended return, and unresolved seams. Load it from the
Dungeon Workbench's **Proposal Preview**. The floating panel keeps its own pan,
zoom, Fit and optional 8/16-pixel grid; **Room** and **Room + proposal** share
that view without changing the editing canvas. A step filters annotations; it
does not operate the dam or change water state. Use the correct separately
bound LOW/HIGH ROMs. The loaded room image is not an immutable before snapshot.

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
