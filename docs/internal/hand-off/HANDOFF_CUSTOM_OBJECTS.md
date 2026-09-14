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

| Concern | Current state | Main code |
| --- | --- | --- |
| Project configuration | `custom_objects_folder`, feature flag, and per-ID subtype filename lists persist in the project descriptor. | `src/core/project.{h,cc}` |
| Loading | `CustomObjectManager` loads and caches external `.bin` layouts for IDs such as `0x31` and `0x32`. | `src/zelda3/dungeon/custom_object.{h,cc}` |
| Rendering | Active project overrides route before the built-in draw routine. Ordinary corner objects keep vanilla behavior unless an explicit track-corner alias is configured. | `object_layer_semantics.h`, `object_drawer.cc` |
| Geometry and previews | Custom layout bounds feed selection and object-browser previews. Preview keys include subtype and room graphics context. | `object_dimensions.cc`, `dungeon_object_selector.cc` |
| Tile authoring | The Custom Object Workshop can create an object and the Object Tile Editor can update tile words in a `.bin` file. | `dungeon_object_selector.cc`, `object_tile_editor.{h,cc}` |
| Minecart source | The Minecart panel parses and preserves the configured ASM start-room/X/Y source and can publish guarded changes. | `minecart_track_source.{h,cc}`, `minecart_track_editor_panel.{h,cc}` |
| Minecart audit | The panel finds track subtype usage, start-table gaps, collision coverage, and can generate collision for one or many rooms. | `minecart_track_editor_panel.cc`, `track_collision_generator.{h,cc}` |
| Oracle overlays | Project lists identify track tiles, stops, switches, object IDs, and minecart sprites. | `Project::dungeon_overlay`, Dungeon overlays |

Focused unit coverage exists for custom-object parsing/bounds/rendering, corner
alias rules, tile-editor state/writeback, minecart source identity and project
binding, overlay configuration, and collision generation. This is component
evidence; it is not yet one complete authoring/publish/build/runtime workflow.

## Important semantics

### Object `0x31`

The size nibble is a project subtype, not a vanilla width/height value.

- Subtypes `0-12` and `14` are minecart track pieces.
- Subtype `13` is `wall_sword_house`.
- Subtype `15` is `small_statue`.
- Optional `0x100-0x103` wall-corner aliases may use track-corner files only
  when the current project explicitly configures those files.

Track detection must never classify subtypes `13` or `15` as rails. Doing so
can replace ordinary wall corners and can generate false collision.

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

## Confirmed safety gaps

1. **Custom `.bin` writeback is not lossless for every parsed layout.** The
   current writer groups cells by row, emits each row densely, and always jumps
   one row. Sparse columns or skipped rows can move tiles on rewrite.
2. **The creation width allows 32 tiles, but the binary row count is five
   bits.** A count of 32 masks to zero and can become a terminator. Creation
   must stop at 31 or the format must split a row into valid segments.
3. **Custom file publishing is a direct overwrite.** The writer joins a
   user-controlled filename to the base path and uses `std::ofstream` directly.
   It needs canonical project-root containment, a temporary file, verification,
   and atomic replacement.
4. **Project persistence can fail silently after first creation.** The callback
   updates the mapping and discards the status from `project->Save()`.
5. **The manager is process-global.** Multiple ROM/project sessions can share a
   singleton unless every session switch rebinds it correctly. The project
   object catalog should be session-owned or explicitly scoped.
6. **Minecart collision batch generation is not transactional.** Generate All
   writes each room directly. If a later room fails, earlier writes remain in
   the ROM buffer, and the operation has no single undo/review step.
7. **The UI mixes destinations.** One panel contains ASM source publishing,
   project overlay settings, track starts, room audits, and direct ROM collision
   writes. The different Save/Publish/Generate actions are easy to confuse.

Until these are fixed, custom-object editing and collision generation are an
advanced Oracle workflow, not part of the general tester lane.

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

### P0: contain writes

1. Add strict custom-binary validation and parse -> encode -> parse equality
   tests, including sparse layouts and maximum row counts.
2. Reject unsupported layouts before touching a file.
3. Enforce project-root containment and atomic verified file replacement.
4. Propagate project-save failures and roll back the in-memory mapping.
5. Make minecart batch generation preview first and commit all rooms in one ROM
   and editor transaction.

### P1: consolidate identity and UI

1. Introduce the session-owned project object catalog and adapt the existing
   manager/mappings to it without a second source of truth.
2. Move the workshop into the Object Library inspector/drawer.
3. Add task-based Minecart mode with route/collision/start validation.
4. Model wall overrides, ice, moving floors, and water behavior explicitly.

### P2: migrate and prove runtime

1. Import existing Oracle mappings and publish compatible `.bin`/ASM outputs.
2. Add application-path tests for create/edit/publish/reopen.
3. Build the patched ROM and validate representative wall, ice, water, and
   minecart witnesses in Mesen.

## Exit criteria

- No custom source or collision write can partially apply or escape the project.
- Existing custom layouts roundtrip byte-equivalently or fail before writing.
- A user can see whether an object changes visuals, collision, runtime behavior,
  or more than one of them.
- Wall aliases never activate from decorative `0x31` subtypes.
- Minecart authoring reports disconnected routes, missing starts, and collision
  differences before commit.
- The published project rebuild and Mesen runtime agree with the Yaze preview
  for documented witnesses.
