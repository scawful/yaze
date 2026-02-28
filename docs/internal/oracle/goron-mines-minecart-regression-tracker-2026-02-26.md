# Goron Mines Minecart Regression Tracker (2026-02-26)

## Purpose
Track and fix current Goron Mines minecart regressions systematically, with concrete evidence and validation gates.

## Scope
- yaze/z3ed minecart collision generation
- CLI vs editor behavior parity
- Mesen2-OOS savestate reliability for Goron Mines smoke tests
- ROM workflow safety for Oracle minecart edits

## Context Snapshot
- Current branch/head: `master` @ `30367ef7`
- Key recent commits:
  - `7a7cb0ba` `cli: add dungeon-minecart-map output for track tile introspection`
  - `8754fa19` `add savestate freshness verify/regen commands`
  - `a3e91185` `add mesen state capture hook and scenario registry`
  - `30367ef7` `fix minecart track collision footprint generation`
- Relevant session history:
  - Claude project session `1563d406-df29-4951-842e-5146400432e1` and subagent summaries for Goron Mines smoke test + minecart edits.

## Issue List

### GM-001: Track collision generation under-counts rail footprints in CLI
- Severity: Critical
- Status: Fixed in yaze (pending runtime validation)
- Symptom:
  - `dungeon-generate-track-collision` dry-run currently reports very small footprints for known minecart rooms.
  - Current local output (oos168):
    - room `0xA8`: `tiles_generated=40`
    - room `0xB8`: `tiles_generated=202`
    - room `0xD8`: `tiles_generated=288`
    - room `0xDA`: `tiles_generated=196`
  - Prior session evidence reported much larger values (example: `0xA8=320`, `0xB8=1466`) in earlier working runs.
- Code evidence:
  - Generator now uses `DimensionService`:
    - `src/zelda3/dungeon/track_collision_generator.cc`
  - `DimensionService` prioritizes `ObjectGeometry`:
    - `src/zelda3/dungeon/dimension_service.cc`
  - For object IDs `0x31/0x32`, routine maps to `kNothing` when custom objects disabled:
    - `src/zelda3/dungeon/draw_routines/draw_routine_registry.cc`
  - `DrawNothing` writes nothing:
    - `src/zelda3/dungeon/draw_routines/special_routines.cc`
- Root-cause hypothesis:
  - In CLI context (custom objects typically off), geometry measurement for `0x31` can collapse to empty/non-representative bounds, producing a degenerate `1x1` footprint.
  - Historical larger values were also invalid in many runs because legacy `RoomObject.width_/height_` were stale (`16x16` default), producing runaway oversized footprints.
- Fix plan:
  1. Add robust fallback in track generator for `track_object_id` (default `0x31`) when geometry collapses to a degenerate `1x1`.
  2. Use stable fallback rules for rail objects in CLI path (`2x2` footprint).
  3. Add hard regression assertions against known OOS rooms.
- Implemented:
  - `src/zelda3/dungeon/track_collision_generator.cc`
    - Added `ResolveTrackObjectDimensions(...)` and a `2x2` fallback for degenerate track bounds.
  - `test/unit/zelda3/dungeon/track_collision_generator_test.cc`
    - Added `DegenerateTrackDimensionsFallbackToTwoByTwo`.
- Validation commands:
  - `./scripts/z3ed dungeon-generate-track-collision --room 0xA8 --rom roms/oos168.sfc --format json`
  - `./scripts/z3ed dungeon-generate-track-collision --room 0xB8 --rom roms/oos168.sfc --format json`
  - `./scripts/z3ed dungeon-generate-track-collision --room 0xD8 --rom roms/oos168.sfc --format json`
  - `./scripts/z3ed dungeon-generate-track-collision --room 0xDA --rom roms/oos168.sfc --format json`

### GM-002: CLI/editor custom-object feature parity gap
- Severity: High
- Status: Implemented (pending runtime parity validation)
- Symptom:
  - Editor and CLI can produce different outcomes for custom-object-dependent logic.
- Code evidence:
  - Editor applies project feature flags, refreshes draw mappings, initializes custom object manager:
    - `src/app/editor/editor_manager.cc`
  - Generic CLI command path does not apply project feature flags/custom object setup:
    - `src/cli/service/resources/command_handler.cc`
    - `src/cli/service/resources/command_context.cc`
- Fix plan:
  1. Add explicit CLI mode to load/apply project feature flags for commands that need custom objects.
  2. Add command-level opt-in for deterministic custom-object behavior.
  3. Document required invocation pattern in z3ed docs.
- Implemented:
  - `src/cli/service/resources/command_handler.cc`
    - Added `--project-context <path.yaze|path.yazeproj>` runtime opt-in.
  - `src/cli/service/resources/command_context.{h,cc}`
    - Loads project context, applies project feature flags, refreshes draw routine mappings, and initializes custom-object manager with project config.
    - Adds scoped restore of feature flags/custom-object manager singleton state after command execution.
  - `src/zelda3/dungeon/custom_object.{h,cc}`
    - Added snapshot/restore helpers so CLI project context apply is reversible.
  - `src/cli/handlers/game/dungeon_commands.h`
    - Updated `dungeon-generate-track-collision` usage to expose `--project-context`.
- Validation commands:
  - Compare same room generation via CLI with and without `--project-context` loading.
  - Confirm parity with editor output for track object occupancy.
- 2026-02-27 validation run:
  - ROM: `../oracle-of-secrets/Roms/oos168x.sfc`
  - Project context: `../oracle-of-secrets/Oracle-of-Secrets.yaze`
  - Rooms tested: `0xA8`, `0xB8`, `0xD8`, `0xDA`
  - Result: no delta between with/without `--project-context` for `tiles_generated`, `stop_count`, `corner_count`, `switch_count`, or ASCII visualization hash.

### GM-003: Unit tests do not catch footprint regression mode
- Severity: High
- Status: Partially fixed
- Symptom:
  - `TrackCollisionGeneratorTest.*` passes while real room outputs are implausibly small.
- Code evidence:
  - Test expected values are computed from `DimensionService`, same path under test:
    - `test/unit/zelda3/dungeon/track_collision_generator_test.cc`
- Fix plan:
  1. Add fixture-based tests using known room/object data with externally asserted expected footprint ranges.
  2. Add golden JSON checks for selected Goron Mines rooms.
  3. Add integration test to fail when tile counts drop below threshold.
- Implemented:
  - `test/integration/zelda3/oracle_workflow_integration_test.cc`
    - Added `D6TrackCollisionGenerationStaysWithinPlausibleBounds` with per-room min/max guardrails for `0xA8/0xB8/0xD8/0xDA`.
- Validation commands:
  - `./build_ai/bin/Debug/yaze_test_unit --gtest_filter='TrackCollisionGeneratorTest.*'`
  - `./build_ai/bin/Debug/yaze_test_integration --gtest_filter='OracleWorkflowTest.D6TrackCollisionGenerationStaysWithinPlausibleBounds'`

### GM-004: Savestate reliability for live Goron Mines testing
- Severity: High
- Status: Implemented (pending live runtime validation)
- Symptom:
  - Prior live smoke runs loaded unexpected room/state context (`room 0x30`, `mode 0x14`) instead of intended dungeon room.
- Evidence:
  - Prior session summaries from Claude subagent logs in project session `1563d406-df29-4951-842e-5146400432e1`.
  - New tooling now exists: `mesen-state-verify`, `mesen-state-regen`, `mesen-state-capture`, `mesen-state-hook`.
- Fix plan:
  1. Make state freshness verification mandatory before live room-navigation scripts.
  2. Ensure scenario IDs from `scripts/dev/mesen-state-scenarios.tsv` are used consistently.
  3. Regenerate stale states and metadata where mismatches exist.
- Implemented:
  - `scripts/dev/mesen-agent-preflight.sh`
    - Enforces strict scenario resolution by default.
    - Fails on `--scenario` vs map mismatches.
    - Requires explicit scenario (or map-derived scenario) unless `--no-strict-scenario` is set.
  - `scripts/dev/refresh-oos-states.sh`
    - Requires mapped scenarios for `minecart_room_*.state` fixtures by default (override: `--no-strict-minecart-scenarios`).
- Validation commands:
  - `./scripts/z3ed mesen-state-verify --state <state> --rom-file <rom> --meta <meta>`
  - `./scripts/z3ed mesen-state-regen --state <state> --rom-file <rom> --meta <meta> --scenario <id>`
  - `./scripts/z3ed mesen-state-capture --state <state> --rom-file <rom> --meta <meta> --scenario <id>`
  - `scripts/dev/mesen-agent-preflight.sh --state <state> --rom <rom> [--scenario-map scripts/dev/mesen-state-scenarios.tsv]`
- 2026-02-27 validation run:
  - Real fixture set used: `../oracle-of-secrets/Roms/SaveStates/library/*D6*.mss`
  - Created run map: `.context/runtime/d6-live-scenarios.tsv`
  - Regenerated metadata with scenario pinning:
    - `scripts/dev/refresh-oos-states.sh --rom ../oracle-of-secrets/Roms/oos168x.sfc --scenario-map .context/runtime/d6-live-scenarios.tsv <states...>`
  - Strict hook preflight after regen:
    - `scripts/dev/mesen-agent-preflight.sh --state <mss> --rom ../oracle-of-secrets/Roms/oos168x.sfc --scenario-map .context/runtime/d6-live-scenarios.tsv`
  - Result: 7/7 D6 fixture states passed strict preflight (`status=pass`, `ok=true`, `fresh=true`).

### GM-005: Build sanity check — ensure base and patched ROMs stay in sync
- Severity: Medium
- Status: Implemented (2026-02-28)
- Clarified workflow:
  - `oos168.sfc` is the **unpatched base ROM** and the **canonical edit target**.  All dungeon room edits and custom collision data are authored directly into `oos168.sfc`.
  - `oos168x.sfc` is the **build output only** — produced by `build_rom.sh` copying `oos168.sfc` and then applying Asar patches.  It is never edited directly.
  - The `Oracle-of-Secrets.yaze` `rom_filename=Roms/oos168x.sfc` entry is used for loading the fully patched ROM in the editor; saves always target `oos168.sfc`.
- Symptom (agent-reported, previously mischaracterised):
  - Prior agent sessions incorrectly assumed edits were being made to `oos168x.sfc` and would be lost on rebuild.  This was a documentation error; the actual workflow was correct all along.
- Evidence:
  - **2026-02-28 audit:** Custom collision region (`$128090–$12DFFF`) is IDENTICAL in both ROMs — no data was lost.
  - The only diff between the two ROMs is Asar patch bytes in the low banks and the generated water fill table at `$25E000` — both expected outputs of the build.
- Implemented:
  - `scripts/build_rom.sh`
    - Added a consistency guard before the `cp -f` step.
    - Uses an inline Python check comparing PC `0x128090–0x12E000` between base and patched ROMs.
    - If they unexpectedly diverge (e.g. a `z3ed --write` command was mistakenly pointed at `oos168x.sfc`), the build aborts with an explanatory error.
    - Override: `OOS_ALLOW_EDIT_OVERWRITE=1` (use with caution).
- Validation commands:
  - Run `scripts/build_rom.sh 168` with in-sync ROMs → should proceed normally (guard exit=0).
  - If guard fires, investigate what wrote to `oos168x.sfc` outside the build pipeline.

### GM-006: `dungeon-generate-track-collision --write` overwrites stop-tile authoring
- Severity: Medium
- Status: Implemented (2026-02-28)
- Symptom:
  - Stop tiles (0xB7–0xBA) can be replaced by generated track tiles, requiring manual re-application.
- Evidence:
  - `WriteTrackCollision` writes only what `GenerateTrackCollision` produces; stop tiles are not part of rail-object geometry.
  - **2026-02-28 audit:** Stop tiles verified intact in both ROMs:
    - Room 0xA8: 8 stop tiles (StopNorth), Room 0xB8: 2 (N/S), Room 0xD8: 16 (E/W), Room 0xDA: 4 (StopNorth).
- Implemented:
  - `src/cli/handlers/game/dungeon_commands.cc`
    - Added `MergeStopTiles()` helper: copies existing stop tiles (0xB7–0xBA) into the generated map wherever generated output is zero.
    - Added `do_preserve_stops` flag from `parser.HasFlag("preserve-stops")`.
    - Applied to both single-room and batch paths: loads existing collision map before write, merges stop tiles, then writes merged result.
    - Reports `"stops_preserved": true` in JSON output when flag is active.
  - `src/cli/handlers/game/dungeon_commands.h`
    - Updated usage string for `dungeon-generate-track-collision` to include `[--preserve-stops]`.
- Validation commands:
  - Dry-run: `./scripts/z3ed dungeon-generate-track-collision --room 0xA8 --rom Roms/oos168x.sfc --preserve-stops --format json`
    - Confirmed: `stops_preserved=True` in output.
  - Write: `./scripts/z3ed dungeon-generate-track-collision --rooms 0xA8,0xB8,0xD8,0xDA --write --preserve-stops --rom Roms/oos168.sfc`
    - Use this to re-apply track data to base ROM after layout edits, with stops preserved.

### GM-007: Tooling gap for quick object-level room inspection in CLI
- Severity: Low
- Status: Open
- Symptom:
  - `dungeon-describe-room` output currently does not provide enough tile-object detail for quick track-object debugging.
- Fix plan:
  1. Add optional object listing payload (`--include-objects` or similar).
  2. Include object IDs/subtypes/coords in JSON.
- Validation commands:
  - `./scripts/z3ed dungeon-describe-room --room 0xB8 --rom roms/oos168.sfc --format json --include-objects`

## Execution Order (Recommended)
1. GM-001 (generator correctness)
2. GM-003 (tests/gates)
3. GM-002 (CLI/editor parity)
4. GM-006 (stop-tile preservation)
5. GM-004 (savestate reliability)
6. GM-005 (ROM workflow guardrails)
7. GM-007 (diagnostic ergonomics)

## Coordination Task Mapping
- GM-001 -> `task_20260226T043912Z_5419`
- GM-002 -> `task_20260226T043912Z_22848`
- GM-003 -> `task_20260226T043912Z_1757`
- GM-004 -> `task_20260226T043912Z_9206`
- GM-005 -> `task_20260226T043912Z_21048`
- GM-006 -> `task_20260226T043912Z_17567`
- GM-007 -> `task_20260226T043912Z_25422`

## Definition of Done
- Known Goron Mines rooms (`0xA8`, `0xB8`, `0xD8`, `0xDA`) produce stable, plausible track collision generation outputs.
- Unit/integration tests fail on future tile-count/footprint regressions.
- CLI and editor behavior match for custom-object-dependent generation paths.
- Mesen live smoke tests begin from validated, scenario-matched savestates.
- ROM edit/build workflow no longer silently drops minecart data edits.
