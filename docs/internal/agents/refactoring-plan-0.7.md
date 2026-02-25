# Yaze Refactoring Plan: 0.6.1 → 0.7.0

Status: DRAFT
Owner: ai-infra-architect + codex (joint review)
Created: 2026-02-24

## Phasing Strategy

Four phases, each shippable independently. Breaking changes OK between releases.
Full test coverage required per phase before merging.

---

## Phase 0: Hygiene Pass (1-2 days, no architectural changes)

**Goal:** Remove dead code, fix obvious tech debt, establish clean baseline.

### 0.1 Delete dead files
- [ ] Remove 7 `.bak` files: `src/core/asar_wrapper_orig.cc.bak`, `src/app/editor/overworld/overworld_editor.cc.bak`, `src/app/editor/code/assembly_editor.cc.bak`, `src/app/editor/message/message_editor.cc.bak`, `src/app/editor/sprite/sprite_editor.cc.bak`, `src/app/editor/graphics/screen_editor.cc.bak`, `src/app/editor/graphics/graphics_editor.cc.bak`
- [ ] Audit `src/lib/` — contains vendored macOS dylibs from 2020. If nothing links them, delete entire directory.

### 0.2 Fix or remove DISABLED tests (13 total)
- `PaletteManagerTest::DISABLED_*` (2) — fix initialization order or delete
- `EmulatorObjectPreviewTest::DISABLED_CpuCanExecuteInstructions` — fix or mark ROM-dependent
- `OverworldIntegrationTest::DISABLED_*` (4) — likely need ROM fixtures; convert to GTEST_SKIP
- `AITilePlacementTest::DISABLED_*` (2) — AI infrastructure tests; either wire up or remove
- `DungeonEditorV2IntegrationTest::DISABLED_*` (4) — headless init tests; likely fixable

### 0.3 Add ext/miniz to dependency tracking
- [ ] Add `ext/miniz/` entry to `dependencies.lock` or equivalent

### 0.4 Document the ImGui Test Engine integration
- [ ] Add `docs/internal/testing/imgui-test-engine-guide.md` covering:
  - How to enable (`-DYAZE_ENABLE_IMGUI_TEST_ENGINE=ON`)
  - PostSwap wiring (SDL2/SDL3 backends)
  - How to register new tests (`dungeon_ui_tests.cc` as template)
  - How to run from the GUI test dashboard

**Validation:** `cmake --build build_ai --target yaze_test_unit z3ed --parallel 8`, all existing tests still pass, no DISABLED tests remain without justification.

---

## Phase 1: EditorManager Decomposition (1-2 weeks)

**Goal:** Split EditorManager from 3,939 LOC god class into focused components.

### 1.1 Extract SessionLifecycleManager
- Owns: ROM loading, project opening, session create/switch/close, duplicate detection
- Methods to extract: `OpenRomOrProject`, `LoadRom`, `CloseSession`, `SwitchToSession`, `HasDuplicateSession`
- ~800 LOC moves out of EditorManager

### 1.2 Extract EditorAssetManager
- Owns: Lazy asset loading, editor initialization, game data propagation
- Methods to extract: `LoadAssets`, `LoadAssetsLazy`, `InitializeEditor`, `PropagateGameData`
- ~600 LOC moves out

### 1.3 Extract ApplyDefaultBackupPolicy → RomBackupPolicy
- Already partially done (this session). Expand to own all backup config including project-level overrides.

### 1.4 EditorManager becomes thin facade
- Keeps: Update loop orchestration, keyboard shortcut dispatch (short), delegation to extracted components
- Target: <1000 LOC

### 1.5 Eliminate ContentRegistry::Context global state
- Replace `ContentRegistry::Context::rom()` calls (80 occurrences across 40 files) with injected `EditorContext*`
- Each editor's `Initialize()` already receives `EditorDependencies` — extend that to include ROM/GameData directly
- Remove the global accessors one file at a time, starting with panels (they're leaf nodes)

**Test requirements:**
- Unit tests for SessionLifecycleManager (mock ROM, project open/close)
- Unit tests for EditorAssetManager (lazy loading, propagation)
- Integration test: EditorManager facade delegates correctly
- All existing editor tests pass unchanged

**Validation:** 0 regressions in `ctest -L stable`, EditorManager.cc < 1200 LOC.

---

## Phase 2: Build System Cleanup (3-5 days)

**Goal:** Remove dead vendored code, standardize dependency management, fix cmake duplication.

### 2.1 Remove src/lib/
- Verify nothing links the vendored dylibs (check all CMakeLists.txt and .cmake files)
- Delete `src/lib/` entirely
- Update any include paths that referenced it

### 2.2 Centralize Homebrew detection
- Create `cmake/homebrew.cmake` with `find_homebrew_prefix()` function
- Replace 5+ inline Homebrew detection blocks across cmake files

### 2.3 Standardize on CPM.cmake
- Document which deps use FetchContent, CPM, or add_subdirectory
- Migrate FetchContent deps to CPM where practical
- Keep ext/ vendored deps (imgui, imgui_test_engine, nfd, SDL, asar, miniz) as-is — they're build-from-source

### 2.4 Add dependencies.lock
- Catalog all third-party deps with version, source, license
- Include ext/miniz 3.0.2 (added this session)

**Test requirements:**
- Build completes on macOS (primary), Linux CI, Windows CI
- All presets still configure and build
- `ctest -L stable` passes

---

## Phase 3: Emulator Interface + State Cleanup (1 week)

**Goal:** Decouple IEmulator from gRPC protos, reduce OverworldEditor/DungeonEditorV2 state explosion.

### 3.1 Create emulator domain models
- `emu::domain::CpuState`, `emu::domain::GameState`, `emu::domain::BreakpointInfo`
- IEmulator returns domain models, not proto types
- gRPC service handles proto ↔ domain conversion in its own layer

### 3.2 Extract OverworldEditor state structs
- `OverworldCanvasState` — pan, zoom, selection, active map
- `OverworldInteractionState` — boolean flags for editing modes
- `OverworldHistoryState` — undo/redo stacks
- Target: OverworldEditor member count drops from 200+ to <50 (struct members don't count)

### 3.3 Complete DungeonEditorV2 component delegation
- Already partially done (DungeonCanvasViewer, interaction handlers). Continue:
- Extract DungeonUndoManager from inline PendingUndo logic
- Extract DungeonPanelCoordinator from SyncPanelsToRoom/ShowPanel logic

**Test requirements:**
- Emulator domain model round-trip tests (proto ↔ domain)
- State extraction preserves existing behavior (diff-test before/after)
- All dungeon/overworld tests pass

---

## Phase 4: Memory + Naming Modernization (ongoing, incremental)

**Goal:** Consistent ownership, modern C++23 patterns, naming convention completion.

### 4.1 Ownership audit
- Establish rule: `unique_ptr` for owning, raw `T*` for non-owning observer
- Fix the 15+ places where raw `new` is used without clear ownership
- Replace `malloc`/`free` in `compression.cc` with `std::vector<uint8_t>`

### 4.2 Naming convention migration
- Complete room.h TODO (line 77): migrate 40+ legacy aliases to `kPrefixedNames`
- Standardize `is_*` for boolean state, `show_*` for visibility flags
- File-by-file migration, one PR at a time

### 4.3 Add [[nodiscard]] to absl::Status returns
- Grep for `absl::Status` return types without `[[nodiscard]]`
- Add systematically, starting with public APIs

**Test requirements:**
- Compile with `-Wunused-result` and verify no new warnings
- All existing tests pass after each file migration

---

## What NOT to refactor now

These patterns are well-designed and should be preserved:
- **Write Fence pattern** (`ScopedWriteFence`) — ROM safety
- **Arena deferred textures** — GPU resource management
- **EventBus** — session lifecycle pub/sub
- **CLI CommandHandler base** — consistent CLI pattern
- **EditorRegistry factory** — editor creation
- **Feature flags** (`FeatureFlags::Flags`) — granular save control
- **DungeonLimits centralization** — done this session, working well

---

## Execution Rules

1. Each phase has a clean build + test gate before the next phase starts.
2. Breaking changes are OK but must be documented in CHANGELOG.md.
3. Each sub-task gets a universe coordination entry (no manual board writes).
4. Run `scripts/dev/validate-next-pass.sh` before marking any task complete.
5. Oracle smoke check must pass at every phase gate.
