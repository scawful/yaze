# Custom dungeon objects and minecart authoring

Status: ACTIVE AUDIT
Owner: `zelda3-hacking-expert` with `imgui-frontend-engineer`
Last reviewed: 2026-09-14
Next review: 2026-09-28
Universe task: `task_20260913T233703Z_21853`

Intent: make project-specific dungeon visuals and gameplay objects safe to
author in Yaze while preserving their source-patch and in-game behavior.

The release priorities live in [the roadmap](../roadmap.md). This file is the
current behavior and design contract for custom objects; the original 2025
handoff was superseded after rendering, previews, and the workshop were added.

## Current implementation

The safety changes below are implemented on the custom-object safety branch.
They remain pending integration and hands-on runtime acceptance.

| Concern | Current state | Main code |
| --- | --- | --- |
| Project configuration | `custom_objects_folder`, the feature flag, and per-ID subtype filename lists persist in the project descriptor. | `src/core/project.{h,cc}` |
| Runtime slots | Oracle currently exposes exactly 16 runtime slots for object `0x31` and three for object `0x32`. Project mappings may replace filenames within those slots; the Workshop cannot add runtime subtypes. | `custom_object.{h,cc}`, `dungeon_object_selector.cc` |
| Loading and session identity | `CustomObjectManager` keeps one entry point but stores the project path, mappings, decoded cache, and asset generation in session-keyed runtime contexts. Session switches activate the matching context, and teardown removes it. | `custom_object.{h,cc}`, `editor_manager.cc`, `session_types.{h,cc}` |
| Rendering | Active project overrides route before built-in draw routines. Corner aliases `0x100-0x103` use track-corner assets only when the project explicitly maps the asset and the current room contains a real minecart-track subtype. Placement ghosts use the same room gate. | `object_layer_semantics.h`, `object_drawer.cc`, `tile_object_handler.cc` |
| Geometry and previews | Custom layout bounds include the active asset generation. Asset reloads and session switches invalidate stale geometry, thumbnails, and queued custom placements. | `object_geometry.{h,cc}`, `dungeon_object_selector.cc` |
| Tile authoring and publication | The Workshop exposes **Edit Graphics** and **Use in Room** for existing fixed slots. The tile editor retains the exact source snapshot, supports a terminator-only empty asset through **Add First Tile**, and publishes desktop changes through strict encoding, stale-write comparison, rollback-protected atomic replacement, and decoded readback. Browser builds disable editing and fail closed in the publication API. | `object_tile_editor.{h,cc}`, `object_tile_editor_panel.{h,cc}`, `custom_object.{h,cc}` |
| Minecart source | The Minecart panel parses and preserves the configured ASM start-room/X/Y source and can publish guarded changes. | `minecart_track_source.{h,cc}`, `minecart_track_editor_panel.{h,cc}` |
| Minecart audit | The panel finds track subtype usage, start-table gaps, and collision coverage. It can still write generated collision for one or many rooms directly. | `minecart_track_editor_panel.cc`, `track_collision_generator.{h,cc}` |
| Oracle overlays | Project lists identify track tiles, stops, switches, object IDs, and minecart sprites. | `Project::dungeon_overlay`, Dungeon overlays |

Focused unit coverage now includes strict custom-object decoding and encoding,
sparse layouts, 32-tile segments, zero-word no-ops, terminator-only assets,
path confinement, stale-write rejection, fixed runtime capacities, session and
geometry isolation, feature and asset-generation transitions, corner-alias room
gating, tile-editor publication, and minecart components. This is component
evidence; it is not yet a complete edit, publish, rebuild, and Mesen workflow.

## Important semantics

### Fixed runtime slots

When Custom Objects is enabled, the low size bits select an existing Oracle
runtime slot. They do not create a new object definition.

#### Object `0x31`

Object `0x31` has 16 fixed slots:

- slots `0-12` and `14` are minecart track graphics;
- slot `13` is `wall_sword_house`;
- slot `15` is `small_statue`;
- mappings longer than 16 entries do not extend the runtime dispatch table.

Corner aliases `0x100-0x103` resolve to the mapped `0x31` corner slots only
when the requested asset exists and the current room contains a track object
using subtype `0-12` or `14`. Decorative subtypes `13` and `15` never enable
track-corner aliases. This keeps ordinary wall corners on their vanilla draw
routines in non-minecart rooms.

#### Object `0x32`

Object `0x32` has three fixed slots:

- slot `0` is `furnace`;
- slot `1` is `firewood`;
- slot `2` is `ice_chair`.

A fourth filename in the project mapping does not create a fourth runtime slot.
Adding any new subtype requires a corresponding ASM dispatch-table change.

### Visual object versus gameplay behavior

An object's static tile stamp does not prove its runtime behavior:

- icy/slippery floors require the correct visual tiles plus collision or room
  behavior used by the game;
- moving floors and moving water have static editor tiles but runtime BG motion;
- water-fill objects may control HDMA or masks rather than stamp the final pixels;
- minecart pieces require visuals, connected collision, a valid start slot, and
  the patched runtime that consumes those tables.

The editor must show these as related properties, not pretend they are one
ordinary bitmap.

## Safety status

### Resolved in the current implementation

1. **Custom `.bin` layout preservation.** A strict decoder and encoder validate
   segment alignment, bounds, overlap, termination, and trailing data. Sparse
   positions, leading and long gaps, zero-word runtime no-ops, terminator-only
   empty assets, and 32-tile segments retain their runtime meaning.
2. **Fixed runtime capacity.** The editor exposes only the 16 valid `0x31`
   slots and three valid `0x32` slots. The removed create-and-append path can no
   longer invent a subtype that the runtime cannot dispatch.
3. **Contained publication.** Filenames must be portable, project-relative
   `.bin` paths. Canonical path and symlink checks prevent publication outside
   the configured custom-object folder.
4. **Conflict-safe publication.** The editor retains the exact opened path and
   source bytes. Apply uses a publication lock, exact-source compare-and-swap,
   rollback-protected atomic replacement, exact byte readback, and decoded
   layout verification. A stale editor keeps its draft and does not replace the
   newer file.
5. **Session isolation.** Project path, mapping, decoded assets, and generation
   state are keyed by ROM session. Switching or closing sessions cannot reuse
   another project's custom-object cache.
6. **Fail-closed browser behavior.** WASM may load and preview configured
   assets, but **Edit Graphics** is disabled and the publication API returns a
   failed precondition because durable atomic project-file replacement is not
   guaranteed.
7. **Room-scoped corner aliases.** Track-corner overrides require an explicit
   mapped asset and a real track subtype in the current room. Room rendering,
   geometry, selector refresh, and placement ghosts share this rule.

### Remaining safety and product gaps

1. **Minecart collision batch generation is not transactional.** **Generate
   All** writes rooms directly. If a later room fails, earlier ROM-buffer writes
   remain, and the operation has no single preview, review, commit, or undo step.
2. **Minecart actions still share one dense panel.** ASM source publication,
   project overlay settings, starts, audits, and direct collision writes remain
   easy to confuse, although graphics editing now links to the separate
   Object Tile Editor.
3. **The project object catalog is not implemented.** Wall overrides, ice,
   moving floors, moving water, and HDMA/control objects do not yet expose
   explicit visual, collision, runtime-behavior, source, and validation fields.
4. **End-to-end proof is incomplete.** The branch still needs an
   application-path edit/publish/reopen test, a patched-ROM rebuild, hands-on
   desktop acceptance, and representative wall, ice, water, and minecart
   witnesses in Mesen.

Until these remaining gaps are closed, fixed-slot custom graphics publication
remains an advanced desktop Oracle workflow. Viewing and placement can enter
the tester lane sooner, but the UI must label graphics-only evidence separately
from collision and runtime behavior.

## Target model: project object catalog

Use one project-owned entry for each `(object_id, subtype)` with explicit
fields for:

- name and category;
- visual source: vanilla ROM, external `.bin`, or Yaze-authored project asset;
- source filename and build/export target;
- bounds and anchor;
- default object stream/layer behavior;
- semantic role: decoration, wall override, track, ice, moving floor, water,
  HDMA/control, or another project-defined role;
- collision/runtime profile and required patch capability;
- usage locations and validation witnesses.

Yaze-authored overrides should publish the source asset and project mapping used
by the ROM build. Do not add preview-only overrides that cannot reach the game.
Existing Oracle `.bin`, ASM, manifest, and project data must import without
losing information.

## Target UI

### Object Library

- Search vanilla and project objects together.
- Show badges for custom visual, runtime behavior, collision, source status,
  and validation state.
- From a selected room object, expose **Edit Visual**, **Override Visual**,
  **Find Uses**, and **Validate** in one stable inspector.
- Keep creation/editing in a resizable side panel or drawer; avoid nested modal
  popups that resize the canvas or context menu.

### Minecart mode

Present a task sequence instead of one large table:

1. Place and connect visual track pieces.
2. Preview the inferred route and collision without writing.
3. Resolve endpoints, switches, and disconnected pieces.
4. Assign or pick the cart start for each used subtype/route.
5. Review a source/ROM diff.
6. Apply transactionally, then build and validate in Mesen.

Advanced overlay IDs and source paths belong in a collapsed project settings
section, not the primary authoring flow.

### Ice, moving floor, and water

Show a behavior badge and the evidence required for each family. A static
preview may be marked correct while runtime motion, collision, or HDMA remains
unverified. Reuse the dungeon issue reporter to capture room, object, layer,
effect, collision, and Mesen evidence without moving the canvas.

## Implementation order

### P0: finish write containment

Completed for fixed-slot custom `.bin` assets:

- strict decoding, encoding, and runtime-layout verification;
- sparse, no-op, empty, and 32-tile format handling;
- project-root path confinement and portable filename validation;
- stale-source rejection and rollback-protected atomic publication;
- fixed `0x31` and `0x32` runtime capacities;
- session-scoped manager state and generation-aware cache invalidation;
- fail-closed WASM publication;
- room-gated track-corner aliases in final rendering and placement ghosts.

Remaining P0 work:

1. Make minecart collision generation preview-only until the user confirms one
   complete ROM and editor transaction.
2. Add one review surface that shows every affected room before commit.

### P1: consolidate identity and UI

1. Introduce a session-owned project object catalog on top of the scoped
   manager without creating a second source of truth.
2. Move the modal Workshop into the stable Object Library inspector or drawer.
3. Add task-based Minecart mode with route, collision, endpoint, and start
   validation.
4. Model wall overrides, ice, moving floors, water, and HDMA/control behavior
   explicitly.
5. Let Yaze create or replace a source asset only through a valid runtime slot
   and a build-compatible project mapping.

### P2: migrate and prove runtime

1. Import existing Oracle mappings and preserve compatible `.bin`, ASM,
   manifest, and project outputs.
2. Add application-path tests for edit, publish, reopen, conflict rejection,
   and session switching.
3. Verify view-only WASM behavior separately from desktop publication.
4. Build the patched ROM and validate representative wall, ice, water, and
   minecart witnesses in Mesen.

## Exit criteria

- No custom source or collision write can partially apply or escape the project.
- Existing custom layouts preserve their decoded runtime positions, zero-word
  no-op behavior, and empty-object meaning through an edit, or fail before
  replacement. Closing an unmodified asset leaves its source bytes unchanged.
- A user can see whether an object changes visuals, collision, runtime behavior,
  or more than one of them.
- Wall aliases never activate from decorative `0x31` subtypes.
- Minecart authoring reports disconnected routes, missing starts, and collision
  differences before commit.
- The published project rebuild and Mesen runtime agree with the Yaze preview
  for documented witnesses.
