# Handoff: Yaze Readiness + Dungeon/Graphics Validation (2026-02-05)

**Owner:** Codex  
**Reviewer:** docs-janitor (2026-02-05)  
**Scope:** Yaze stability for ALTTP + Oracle of Secrets (OOS) editing  
**Status:** In-progress; docs and CLI validation improvements added, no tests in this pass.

## What was requested
- Evaluate readiness of yaze for overworld/dungeon/graphics/messages.
- Start ZScream parity capture.
- Start dungeon object draw validation framework.
- Improve safety/accuracy for OOS (configurable addresses, ROM overrides).

## Summary of work completed
### 1) Readiness evaluation docs
- Expanded `docs/internal/agents/yaze-readiness-plan.md` with detailed per‑area
  evaluation (overworld/dungeon/graphics/messages) including risks + validation
  checklists.

### 2) ZScream parity capture (code‑level)
- Updated `docs/internal/agents/zscream-capture-log.md` with code‑confirmed
  selection/marquee/context menu behaviors and file references.
- Updated parity status in
  `docs/internal/agents/dungeon-object-ux-parity-matrix.md` to reflect
  “code‑confirmed / runtime‑pending”.

### 3) Dungeon object validation (CLI)
- Added `--room <id>` support to `dungeon-object-validate` so it can validate
  actual room objects (not just synthetic sizes).
- Reports now include room context fields and trace offsets for selection
  bounds verification.
- Trace dumps can include room metadata.
- Files:
  - `src/cli/handlers/tools/dungeon_object_validate_commands.cc`
  - `src/cli/handlers/tools/dungeon_object_validate_commands.h`

### 4) Graphics pointer overrides (OOS safety)
- Added `rom_addresses` keys for overworld graphics pointer tables:
  `overworld_gfx_ptr1/2/3`.
- Applied overrides to both graphics load and save paths.
- Added commented keys to the project template.
- Files:
  - `src/core/rom_settings.h`
  - `src/zelda3/game_data.cc`
  - `src/app/editor/graphics/graphics_editor.cc`
  - `src/app/editor/code/project_file_editor.cc`

### 5) Validation spec updates
- `docs/internal/agents/dungeon-object-validation-spec.md` updated to reflect
  new room-mode validation support and clarified TileTrace explanation.

## Current repo state
`git status -sb` shows multiple modified files. This workspace is **ahead 13**
with prior commits (not created in this handoff). There are still uncommitted
changes from this session.

Modified files in this session (check `git status -sb` for full list):
- `CMakePresets.json`
- `cmake/dependencies/grpc.cmake`
- `cmake/options.cmake`
- `docs/internal/agents/dungeon-object-ux-parity-matrix.md`
- `docs/internal/agents/dungeon-object-validation-spec.md`
- `docs/internal/agents/yaze-readiness-plan.md`
- `docs/internal/agents/zscream-capture-log.md`
- `src/CMakeLists.txt`
- `src/app/app.cmake`
- `src/app/editor/code/project_file_editor.cc`
- `src/app/editor/dungeon/panels/minecart_track_editor_panel.cc`
- `src/app/editor/graphics/graphics_editor.cc`
- `src/app/service/grpc_support.cmake`
- `src/cli/handlers/tools/dungeon_object_validate_commands.cc`
- `src/cli/handlers/tools/dungeon_object_validate_commands.h`
- `src/core/rom_settings.h`
- `src/rom/rom.cc`
- `src/rom/rom.h`
- `src/zelda3/game_data.cc`
- `test/unit/editor/message/message_data_test.cc`
- Untracked: `docs/internal/agents/handoff-2026-02-05-codex.md`

## Review notes (docs-janitor, 2026-02-05)
- Build presets now default to a minimal dev build (no gRPC/AI/tests) with new
  presets for AI/tests and gRPC, and build jobs set to 1. File:
  `CMakePresets.json`.
- gRPC/proto plumbing updates: regenerated protobuf outputs clear the gens
  directory, gRPC support now gates on `YAZE_ENABLE_GRPC`/`YAZE_WITH_GRPC`, and
  proto objects are packaged into `yaze_proto_lib` with explicit dependencies.
  Files: `cmake/dependencies/grpc.cmake`, `cmake/options.cmake`,
  `src/CMakeLists.txt`, `src/app/app.cmake`, `src/app/service/grpc_support.cmake`.
- ROM read helpers are now `const`. Files: `src/rom/rom.cc`, `src/rom/rom.h`.
- Message data tests updated to use expanded constants. File:
  `test/unit/editor/message/message_data_test.cc`.
- Minecart track editor hex parsing now uses `absl::string_view` before
  converting to `std::string`. File:
  `src/app/editor/dungeon/panels/minecart_track_editor_panel.cc`.

## How to continue (top priorities)
### A) Runtime ZScream capture
Code‑level parity is documented but runtime confirmation is still pending.
Perform quick runtime captures and update:
- `docs/internal/agents/zscream-capture-log.md`
- `docs/internal/agents/dungeon-object-ux-parity-matrix.md`

Capture checklist:
1. Overlapping object selection order across BG1/BG2/BG3.
2. Marquee inclusion on edge/corner intersections.
3. Modifier behavior (Shift/Ctrl/Alt during selection/drag).
4. Room context menu actions + ordering.
5. Object context menu actions + ordering (single vs group).

### B) Run dungeon object validation on real rooms
Use the new `--room` mode to find mismatches quickly.
On macOS, the CLI binary is inside the app bundle at
`./build/bin/yaze.app/Contents/MacOS/yaze` (replace with `./build/bin/yaze` on
non-macOS builds).
Example:
```
./build/bin/yaze.app/Contents/MacOS/yaze dungeon-object-validate --rom /path/to/zelda3.sfc \
  --room 0x88 --report /tmp/room_0x88 --trace-out /tmp/room_0x88_trace.json \
  --format json
```
Use results to target draw routine fixes (large decor etc).

### C) Graphics pointer overrides validation
Now supports `rom_addresses` for `overworld_gfx_ptr1/2/3`. Validate on OOS
ROMs that relocate graphics pointer tables. If missing, update project config:
```
[rom_addresses]
overworld_gfx_ptr1=0x......
overworld_gfx_ptr2=0x......
overworld_gfx_ptr3=0x......
```

## Notes / risks to keep in mind
- `dungeon-object-validate` room mode compares trace bounds to
  `ObjectDimensionTable` selection bounds. If object selection rules differ
  from ZScream (collisionPoint vs bounds), mismatches may be expected.
- Graphics save does not check compressed size vs original block size yet.
  Avoid saving large edits on OOS unless you’re certain the block fits.
- OOS ROM address drift still needs systematic validation beyond overlays.

## Testing (not run this pass)
- No tests executed during this handoff.
- If you want to sanity‑check:
  - `ctest -R MessageDataTest --output-on-failure`
  - For dungeon validation CLI, no unit tests exist yet.

## Helpful context files
- Readiness plan: `docs/internal/agents/yaze-readiness-plan.md`
- ZScream capture log: `docs/internal/agents/zscream-capture-log.md`
- UX parity matrix: `docs/internal/agents/dungeon-object-ux-parity-matrix.md`
- Validation spec: `docs/internal/agents/dungeon-object-validation-spec.md`

## AFS scratchpad
Progress logs saved in `.context/scratchpad/yaze-oos-readiness.md`.

## Next actions I would take
1) Run ZScream runtime capture and update parity docs.  
2) Run `dungeon-object-validate --room` on a few problematic rooms (large decor) and file issues.  
3) Add validation warnings in UI for graphics pointer overrides (optional).  
4) Add unit tests for `dungeon-object-validate --room` report shape (optional).

## Continuation (2026-02-06, zelda3-hacking-expert)
- Added room-mode clipping unit tests (repeat spacing + base_h=0) in
  `test/unit/cli/dungeon_object_validate_test.cc` and ran
  `ctest -R DungeonObjectValidateTest --output-on-failure` (pass).
- Expanded room-mode samples beyond Goron Mines (Mushroom Grotto, Tail Palace,
  Zora Temple, Dragon Ship) and documented the run results in
  `docs/internal/agents/dungeon-object-validation-spec.md` (mismatch_count=0).

## Updated next actions
1) Run ZScream runtime capture and update parity docs.  
2) Validate graphics pointer overrides on OOS ROMs that relocate graphics
   pointer tables.  
