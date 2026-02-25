# Refactoring Handoff — 2026-02-24

Session participants: `ai-infra-architect` (Claude, primary implementer), Codex (orchestrator + Phase 1 executor), second Claude agent (IEmulator rewrite + service impl fixes).

## Current Baseline

- **Branch:** `master` (164 modified files in working tree, no commits yet)
- **Build:** Green (`cmake --build build_ai --target yaze_test_unit z3ed --parallel 8`)
- **Unit tests:** 1,399 tests across 211 suites
- **Integration tests:** 18 dungeon editor integration tests pass
- **Oracle smoke:** `ok: true` (structural pass)
- **Coordination:** Universe event log at `~/.context/agent-universe/`

## What Was Completed

### Phase 0 + Phase 1: Code Hygiene (DONE)

| Item | Status | Who | Notes |
|------|--------|-----|-------|
| Delete .bak files (7) | DONE | ai-infra-architect | All removed, confirmed no references |
| Remove `src/lib/` directory | DONE | Other agent/user | Directory deleted, all 7 cmake include paths removed |
| Remove legacy aliases in `room.h` (40 aliases) | DONE | Codex | Migrated 25+ source files + 12 test files to `kPrefixed` names |
| Replace goto/malloc in `compression.cc` | DONE | Codex | `std::vector<uint8_t>` + lambda early-returns |
| Replace goto in `tracker.cc` | DONE | Codex | Structured loops + lambda error reporting |
| Triage DISABLED tests (13) | DONE | Codex | 2 re-enabled, 11 deleted with rationale |

### Phase 3: IEmulator Interface Cleanup (DONE)

| Item | Status | Who | Notes |
|------|--------|-----|-------|
| Create `emulator_types.h` domain types | DONE | ai-infra-architect + other agent | `InputButton`, `BreakpointKind`, `CpuKind`, `CpuStateSnapshot`, `GameSnapshot`, `BreakpointSnapshot`, `BreakpointHitResult`, `EmulatorFeature` |
| Rewrite `i_emulator.h` | DONE | Other agent | Removed `#ifdef YAZE_WITH_GRPC` guard entirely, uses domain types only |
| Create `proto_converter.h` | DONE | Other agent | Full bidirectional enum + struct conversion |
| Update `MesenEmulatorAdapter` | DONE | Other agent | New `Connect()` method, SPC700 returns `UnimplementedError`, `SaveState`/`LoadState`/`SupportsFeature` added |
| Fix `emulator_service_impl.cc` | DONE | Both agents | Button validation with rollback, `GetCpuState` error propagation at all 3 call sites |

### 0.6.1 Release Features (DONE — from earlier in session)

| Feature | Files | Tests |
|---------|-------|-------|
| Sprite/door placement limit enforcement | `sprite_interaction_handler.cc`, `door_interaction_handler.cc` | 9 tests |
| Tile selector range filter | `tile_selector_widget.h/.cc` | 9 tests |
| Toast dedup | `toast_manager.h` | 8 tests |
| Dungeon limits centralization | `dungeon_limits.h` + 3 handlers + validator | 11 tests |
| Project bundle verify/pack/unpack | `project_bundle_*_commands.{h,cc}` | 52 tests |
| `--check-rom-hash` flag | `project_bundle_verify_commands.cc` | 4 tests |
| `--dry-run` for unpack | `project_bundle_archive_commands.cc` | 5 tests |
| Portable SHA1 | `rom_hash.cc` | 4 tests |
| ImGui Test Engine Tier 1 | `dungeon_ui_tests.cc` + backend PostSwap wiring | 11 tests (need engine enabled to run) |
| CI protocol checks | `.github/workflows/ci.yml` | protocol-audit + universe-coord in release-readiness job |
| InteractionCoordinator SEGFAULT fix | `base_entity_handler.h` | 18 coordinator tests now pass |

## What Remains (Ordered by Phase)

### Phase 2: Build System Modernization (NOT STARTED)

**Blocking dependency:** Phase 1 (DONE)

| Task | Difficulty | Key Files | Notes |
|------|-----------|-----------|-------|
| 2.1 Remove `src/lib/` | DONE | — | Already deleted |
| 2.2 Centralize Homebrew detection | Small | `cmake/platform/homebrew.cmake` (create), then migrate 6 cmake files (26 occurrences of `/opt/homebrew`) | Inventory complete: `grpc.cmake` (10 refs), `sdl2.cmake` (3), `sdl3.cmake` (3), `testing.cmake` (4), `yaml.cmake` (4), `llvm-brew.toolchain.cmake` (2) |
| 2.3 Resolve dual SDL (ext/SDL vs CPM) | Medium | `cmake/dependencies/sdl2.cmake`, `ext/SDL/` | Decide which is authoritative |
| 2.4 Add `dependencies.lock` | Small | Create new file | Catalog all ext/ deps with version + license |
| 2.5 Simplify `grpc.cmake` (~400 lines) | Medium | `cmake/dependencies/grpc.cmake` | Extract OpenSSL discovery, use homebrew utilities |

### Phase 4: Data Model Encapsulation (NOT STARTED)

**Blocking dependency:** Phase 1 (DONE)

| Task | Difficulty | Key Files |
|------|-----------|-----------|
| 4.1 Encapsulate `room.h` public members | Large | `src/zelda3/dungeon/room.h` (1,127 lines) — add getter/setter, deprecate direct access, then make private |
| 4.2 Split `EditorDependencies` | Medium | `src/app/editor/editor.h` — into `CoreDependencies` + `UIDependencies` |
| 4.3 Extract LRU cache utility | Small | `src/util/lru_cache.h` — replace inline LRU in `DungeonEditorV2` |

### Phase 5: EditorManager Decomposition (NOT STARTED)

**Blocking dependency:** Phase 1 + Phase 4

| Task | Difficulty | Key Files | Current State |
|------|-----------|-----------|---------------|
| 5.1 Extract `RomLifecycleManager` | Large | `src/app/editor/system/rom_lifecycle_manager.{h,cc}` | EditorManager is 3,939 LOC |
| 5.2 Create interface abstractions | Medium | `src/app/editor/system/editor_manager_interfaces.h` | 5 circular deps identified |
| 5.3 Break circular dependencies | Medium | SessionCoordinator, UICoordinator, PopupManager, MenuOrchestrator, DashboardPanel | All have `EditorManager*` back-pointers |
| 5.4 Eliminate `ContentRegistry::Context` | Large | 80 call sites across 40 files | Extend `EditorDependencies` instead |

### Phase 6: Editor State Reduction (NOT STARTED)

**Blocking dependency:** Phase 4 + Phase 5

| Task | Difficulty | Key Files |
|------|-----------|-----------|
| 6.1 Decompose `OverworldEditor` (3,455 LOC, 200+ members) | Large | `src/app/editor/overworld/overworld_editor.{h,cc}` |
| 6.2 Finish `DungeonEditorV2` delegation | Medium | Already partially done — finish undo manager + panel coordinator extraction |

## Key File Inventory (Most Touched This Session)

```
src/app/emu/emulator_types.h          — NEW: domain types for IEmulator
src/app/emu/i_emulator.h              — REWRITTEN: proto-free interface
src/app/emu/proto_converter.h         — NEW: proto <-> domain conversion
src/app/emu/mesen/mesen_emulator_adapter.{h,cc} — UPDATED: domain types + Connect()
src/app/service/emulator_service_impl.cc — UPDATED: converter usage + error propagation
src/zelda3/dungeon/dungeon_limits.h   — NEW: centralized limits + DungeonLimit enum
src/zelda3/dungeon/dungeon_validator.h — UPDATED: removed private limit constants
src/app/editor/dungeon/interaction/base_entity_handler.h — UPDATED: toast context guards
src/app/editor/dungeon/interaction/{sprite,door,tile_object}_handler.cc — UPDATED: limit enforcement
src/app/gui/widgets/tile_selector_widget.{h,cc} — UPDATED: range filter
src/app/editor/ui/toast_manager.h     — UPDATED: dedup
src/cli/handlers/rom/project_bundle_{verify,archive}_commands.{h,cc} — NEW: full bundle CLI
src/util/rom_hash.{h,cc}              — UPDATED: portable SHA1
src/app/test/dungeon_ui_tests.{h,cc}  — NEW: ImGui Test Engine tests
ext/miniz/{miniz.h,miniz.c}           — NEW: zip library for bundle pack/unpack
```

## Build Commands

```bash
# Standard build (what we've been using all session)
cmake --build build_ai --target yaze_test_unit z3ed --parallel 8

# Run focused handler/bundle tests
./build_ai/bin/Debug/yaze_test_unit --gtest_filter='TileObjectHandlerTest.*:SpriteInteractionHandlerTest.*:DoorInteractionHandlerTest.*:InteractionCoordinatorTest.*:ProjectBundle*Test.*:DungeonLimitsTest.*:RoomLimitRegressionTest.*:ToastDedupTest.*:RomHashTest.*'

# Oracle smoke (must return ok: true)
./build_ai/bin/Debug/z3ed oracle-smoke-check --rom roms/oos168.sfc --min-d6-track-rooms=4 --format=json

# Protocol audit (must say PASS)
bash scripts/agents/protocol-audit.sh

# Universe coordination tests (must say PASS)
bash scripts/agents/test-universe-coord.sh

# Full validation script
bash scripts/dev/validate-next-pass.sh

# Integration tests (separate binary)
./build_ai/bin/Debug/yaze_test_integration --gtest_filter='DungeonEditorV2IntegrationTest.*'
```

## Known Issues / Gotchas

1. **164 uncommitted files** — This working tree has a LOT of changes from multiple agents across the day. Commit in logical chunks before continuing.

2. **`YAZE_ENABLE_IMGUI_TEST_ENGINE` is off in `build_ai`** — The 11 ImGui UI tests compile as no-op stubs. To actually run them, build with `-DYAZE_ENABLE_IMGUI_TEST_ENGINE=ON` and the `ImGuiTestEngine` target must be available.

3. **Oracle preflight known-fail** — `dungeon-oracle-preflight --required-collision-rooms=0x25,0x27,0x32` returns `ok=false` because rooms 0x25 and 0x32 don't have authored collision data yet. This is an authoring gap, NOT a regression.

4. **`src/lib/` is fully deleted** — but verify nothing broke on Linux/Windows CI. The two `<png.h>` users (`mesen_screenshot_panel.cc`, `visual_diff_engine.cc`) depend on system libpng via `find_package(PNG)`.

5. **Mesen adapter SPC700 breakpoints** — Now returns `UnimplementedError` instead of silently setting on 65816. This is a behavioral change callers should be aware of.

6. **`emulator_service_impl.cc` button validation** — Press/Hold now track pressed buttons and roll back on failure. This changes the error contract: a partial button press sequence now gets cleaned up instead of leaving buttons stuck.

## Coordination System

```bash
# List active tasks
scripts/agents/coord task-list --status active --format text

# Add a new task
scripts/agents/coord task-add --title "Description" --agent "persona-id"

# Claim, heartbeat, complete
scripts/agents/coord task-claim --id <task_id> --agent <persona-id>
scripts/agents/coord task-heartbeat --id <task_id> --agent <persona-id> --note "progress"
scripts/agents/coord task-complete --id <task_id> --agent <persona-id> --note "summary"

# Generate human-readable snapshot
scripts/agents/coord task-generate-board --out docs/internal/agents/coordination-board.generated.md
```

## Recommended Next Steps (Priority Order)

1. **Commit the working tree** in logical chunks (Phase 1 hygiene, Phase 3 emulator, 0.6.1 features, bundle CLI, test infra).
2. **Tag v0.6.1** — all automated gates are green. Manual QA blocks B/C/E from the test checklist are the remaining gate.
3. **Phase 2.2** — Create `cmake/platform/homebrew.cmake` and migrate the 6 consumer files. I mapped all 26 occurrences.
4. **Phase 4.3** — Extract the LRU cache from DungeonEditorV2 into `src/util/lru_cache.h`. Small, contained, useful utility.
5. **Phase 5.1** — Start EditorManager decomposition with `RomLifecycleManager` extraction. This is the biggest payoff item.

## Architecture Patterns to Preserve

- `ScopedWriteFence` for ROM safety
- Arena deferred texture processing
- EventBus for session lifecycle
- `absl::Status`/`StatusOr` error handling
- `CommandHandler` base class for CLI
- `dungeon_limits.h` as single source of truth for entity limits
- Universe coordination (no manual board writes)
