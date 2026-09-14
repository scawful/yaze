# Custom dungeon objects and minecart authoring

Status: PENDING INTEGRATION — implementation is in draft PR #217
Owner: [`zelda3-hacking-expert` with `imgui-frontend-engineer`](../agents/personas.md)
Last reviewed: 2026-09-14
Next review: 2026-09-28
Universe task: `task_20260913T233703Z_21853` ([coordination system](../agents/universe-coordination-spec.md))

Intent: make project-specific dungeon visuals and gameplay objects safe to
author in Yaze while preserving their source-patch and in-game behavior.

The release priorities live in [the roadmap](../roadmap.md). This file is the
current behavior and design contract for custom objects; the original 2025
handoff was superseded after rendering, previews, and the workshop were added.

## Pending implementation in PR #217

The safety changes below are implemented in
[PR #217](https://github.com/scawful/yaze/pull/217) on the
`codex/custom-object-publish-safety` branch. They are not part of this
documentation PR or `master` until PR #217 merges, and they remain pending
hands-on runtime acceptance.

| Concern | Current state | Main code |
| --- | --- | --- |
| Project configuration | `custom_objects_folder`, the feature flag, and per-ID subtype filename lists persist in the project descriptor. | `src/core/project.{h,cc}` |
| Runtime slots | Oracle currently exposes 21 fixed assets: 16 slots for object `0x31`, three for object `0x32`, and two sprite-body slots for object `0x54`. Project mappings may replace filenames within those slots; the Workshop cannot add runtime subtypes. | `custom_object.{h,cc}`, `dungeon_object_selector.cc` |
| Loading and session identity | `CustomObjectManager` keeps one entry point but stores the project path, mappings, decoded cache, and asset generation in session-keyed runtime contexts. Session switches activate the matching context, and teardown removes it. | `custom_object.{h,cc}`, `editor_manager.cc`, `session_types.{h,cc}` |
| Rendering | Active project overrides route before built-in draw routines. Object `0x54` preserves raw source words but applies Oracle's nonzero `OR #$0300` tile-page rule while drawing. Corner aliases `0x100-0x103` use track-corner assets only when the project explicitly maps the asset and the current room contains a real minecart-track subtype. Placement ghosts use the same room gate. | `minecart_object_semantics.h` (introduced by PR #217), `object_layer_semantics.h`, `object_drawer.cc`, `tile_object_handler.cc` |
| Geometry and previews | Custom layout bounds include the active asset generation. Asset reloads and session switches invalidate stale geometry, thumbnails, and queued custom placements. The `0x54` tilemap preview applies the runtime page mask, but Yaze does not yet load the separate boss pixel graphics that Oracle DMA-copies into VRAM. | `object_geometry.{h,cc}`, `object_tile_editor.cc`, `dungeon_object_selector.cc` |
| Tile authoring and publication | The Workshop exposes **Edit Graphics** and **Use in Room** for existing fixed slots. The tile editor retains the exact source snapshot, supports a terminator-only empty asset through **Add First Tile**, and publishes desktop changes through strict encoding, stale-write comparison, rollback-protected atomic replacement, and decoded readback. Browser builds disable editing and fail closed in the publication API. | `object_tile_editor.{h,cc}`, `object_tile_editor_panel.{h,cc}`, `custom_object.{h,cc}` |
| Minecart source | The **Routes** tab parses and preserves a configured ASM start-room/X/Y source and publishes it with source-identity and stale-write checks. Route slots come from minecart sprite subtypes, not visual track-piece subtypes. The current Oracle manifest does not yet declare `minecart_tracks.source`, so route publication correctly fails closed until that project metadata is added. | `minecart_track_source.{h,cc}`, `minecart_track_editor_panel.{h,cc}` |
| Minecart collision | The **Collision** tab audits loaded rooms without blocking routine edits. **Preview All Rooms** performs an explicit 296-room scan, excludes rooms that already contain custom collision, shows every proposed room, and applies the confirmed maps to the editor model as one undoable batch. Preview and Apply do not write ROM bytes; **Save ROM** remains the serialization boundary. | `minecart_track_editor_panel.cc`, `dungeon_editor_v2_undo.cc`, `track_collision_generator.{h,cc}` |
| Oracle overlays | Project lists identify track tiles, stops, switches, object IDs, and minecart sprites. | `Project::dungeon_overlay`, Dungeon overlays |

Focused unit coverage now includes strict custom-object decoding and encoding,
sparse layouts, 32-tile segments, zero-word no-ops, terminator-only assets,
path confinement, stale-write rejection, all 21 fixed runtime slots, raw versus
runtime `0x54` tile words, session and geometry isolation, feature and
asset-generation transitions, corner-alias room gating, tile-editor
publication, and minecart components. This is component evidence; it is not yet
a complete edit, publish, rebuild, and Mesen workflow.

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

Object `0x31` subtype selects a visual graphics slot. A minecart sprite subtype
selects a route/start-table slot. These identities are independent: a visual
track subtype never selects or proves a route slot.

#### Object `0x32`

Object `0x32` has three fixed slots:

- slot `0` is `furnace`;
- slot `1` is `firewood`;
- slot `2` is `ice_chair`.

A fourth filename in the project mapping does not create a fourth runtime slot.
Adding any new subtype requires a corresponding ASM dispatch-table change.

#### Object `0x54`

Object `0x54` has two fixed sprite-body tilemap slots:

- slot `0` is `kydreeok_body`;
- slot `1` is `manhandla_body_1a`.

Oracle checks each source word for zero and then applies `OR #$0300` before
writing a nonzero tile to the room tilemap. Yaze keeps the raw source word in
the editor and `.bin` publication path, and applies that page mask only while
drawing or rendering a tilemap preview. A zero source word remains a no-op.

These `.bin` files contain tilemaps, not the boss pixel graphics. Oracle loads
separate Kydreeok and Manhandla graphics into VRAM at runtime; Yaze does not yet
load those external graphics into the room preview. The editor can therefore
prove `0x54` geometry and source round-trip behavior, but not pixel parity. The
Kydreeok preview will eventually also need an explicit phase-one/phase-two
graphics choice.

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
   slots, three valid `0x32` slots, and two valid `0x54` slots. The removed
   create-and-append path can no longer invent a subtype that the runtime cannot
   dispatch.
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
8. **Transactional minecart collision editing.** Collision generation remains a
   preview until the user reviews and confirms the complete room list. Apply
   revalidates every room, refuses to replace existing custom collision, updates
   only room models, and records the complete batch as one undoable action.
   Neither Preview nor Apply mutates ROM bytes; **Save ROM** is the persistence
   boundary.

### Remaining safety and product gaps

1. **Oracle route-source metadata is not wired.** The Minecart **Routes** tab
   requires an explicit `minecart_tracks.source`; the current Oracle manifest
   does not declare one. This intentionally blocks route publication rather
   than guessing at or rewriting an undeclared ASM file.
2. **The project object catalog is not implemented.** Wall overrides, ice,
   moving floors, moving water, and HDMA/control objects do not yet expose
   explicit visual, collision, runtime-behavior, source, and validation fields.
3. **The project-wide scan is synchronous.** **Preview All Rooms** is explicit
   so normal object edits stay responsive, but the full scan still needs visible
   progress, cancellation, or an asynchronous job before it is polished UX.
4. **End-to-end proof is incomplete.** The branch still needs an
   application-path edit/publish/reopen test, a patched-ROM rebuild, hands-on
   desktop acceptance, and representative wall, ice, water, and minecart
   witnesses in Mesen.

Fixed-slot custom graphics publication and transactional collision editing now
need hands-on desktop acceptance. Route publication remains unavailable for
Oracle until `minecart_tracks.source` is declared. Full tester readiness still
requires patched-ROM and Mesen witnesses that distinguish graphics, collision,
and runtime behavior.

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

The current branch provides the first separation:

- **Routes** owns sprite-subtype route/start usage and the manifest-owned ASM
  source.
- **Collision** owns loaded-room audits, explicit full-project preview,
  confirmation, and one undoable room-model transaction.
- **Object Workshop** owns visual track graphics. Canonical object `0x31`
  subtypes `13` and `15` remain decorations.

The remaining target sequence is:

1. Place and connect visual track pieces.
2. Preview generated collision without writing.
3. Resolve endpoints, switches, and disconnected pieces.
4. Assign or pick the route start selected by each minecart sprite subtype.
5. Review model and source changes separately.
6. Apply to room models, support Undo/Redo, **Save ROM**, then build and validate
   in Mesen.

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
- fixed `0x31`, `0x32`, and `0x54` runtime capacities;
- session-scoped manager state and generation-aware cache invalidation;
- fail-closed WASM publication;
- room-gated track-corner aliases in final rendering and placement ghosts;
- preview, complete-room review, confirmation, stale-preview revalidation, and
  one model-level undo transaction for minecart collision generation.

Remaining P0 work:

1. Load and select the external Kydreeok/Manhandla boss pixel graphics used by
   object `0x54`, then capture Mesen parity evidence for both body types and
   both Kydreeok phases.
2. Add the authoritative `minecart_tracks.source` to Oracle project metadata,
   then validate guarded route publication against that exact ASM file.
3. Complete hands-on desktop acceptance for preview, confirmation, Apply,
   Undo/Redo, **Save ROM**, reopen, and patched-ROM behavior.

### P1: consolidate identity and UI

1. Introduce a session-owned project object catalog on top of the scoped
   manager without creating a second source of truth.
2. Move the modal Workshop into the stable Object Library inspector or drawer.
3. Continue the task-based Minecart mode: the **Routes** and **Collision** tabs
   now separate source-table work from generated collision; endpoint and
   connectivity guidance plus asynchronous project scanning remain.
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
