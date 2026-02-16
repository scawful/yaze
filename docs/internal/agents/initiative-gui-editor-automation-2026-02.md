# GUI Editor Simplification + Automation Reliability (2026-02)

Status: IN_PROGRESS  
Owner: backend-infra-engineer  
Created: 2026-02-14  
Last Reviewed: 2026-02-14  
Next Review: 2026-02-21  
Coordination Board: `docs/internal/agents/coordination-board.md`

## Summary
- Lead agent/persona: `backend-infra-engineer`
- Supporting agents: `imgui-frontend-engineer`, `ai-infra-architect`, `test-infrastructure-expert`
- Problem statement: GUI/editor architecture remains high-cognitive-load (large orchestration classes, split panel systems, weak automation selector stability), slowing feature velocity and increasing UI/test flake.
- Success metrics:
  - `EditorManager` responsibilities reduced to orchestration shell; lifecycle/state logic moved to focused services.
  - Panel visibility/pinning/window identity governed by one runtime policy path.
  - Automation actions (`click/type/wait/assert`) support stable selector keys and deterministic idle synchronization.
  - GUI/E2E flakes reduced by moving tests off string-label matching to stable selector helpers.

## Scope
- In scope:
  - `src/app/editor/editor_manager.*` responsibility decomposition.
  - Panel runtime unification across `PanelManager` and `RightPanelManager`.
  - Automation harness hardening in `src/app/service/imgui_test_harness_service.*` and proto/schema updates.
  - Widget registry adoption and stable selector contract for automation and ImGui tests.
  - Tile16/Overworld editor structure cleanup (state/commands/view split) to reduce method-level complexity.
  - Test framework upgrades for deterministic GUI integration runs.
- Out of scope:
  - Large visual redesign/theming initiatives.
  - New end-user feature sets unrelated to architecture/testability.
  - Plugin/runtime extension platform beyond what is needed for internal decomposition.
- Dependencies / upstream projects:
  - Existing 0.6.0 stabilization track and CI fixes.
  - iOS/SwiftUI integration work already in progress (must remain compatible with bridge contracts).

## Workstreams

### WS1: Editor Core Decomposition
- Extract lifecycle/state responsibilities from `EditorManager`:
  - Startup surface + visibility flow.
  - Session lifecycle + project context switching.
  - ROM save policy and safety gating.
- Keep `EditorManager` as coordinator/facade with bounded surface.

### WS2: Panel Runtime Unification
- Define one canonical panel state path:
  - visibility
  - pinning
  - window naming/identity
  - width/size persistence
- Reduce duplicate behavior between `PanelManager` and `RightPanelManager`.

### WS3: Automation Contract Hardening
- Add stable widget selector key support in harness interfaces.
- Add deterministic execution controls (`wait-for-idle` / deferred flush semantics).
- Consolidate repeated harness action plumbing (`click/type/wait/assert`) into shared executor path.

### WS4: ImGui Test Strategy Upgrade
- Add stable selector helpers for test authors.
- Add shared GUI fixture utilities for session/editor/panel setup.
- Standardize failure artifacts (screenshot + selector snapshot + harness logs).

### WS5: Overworld/Tile16 Complexity Reduction
- Split Tile16 editor internals into:
  - mutable edit state model
  - command/undo operations
  - rendering/panel presentation
- Keep behavior parity while making logic independently testable.

## Execution Checkpoints (Fast Batch Model)

### Checkpoint A: Baseline + Contracts
- Deliverables:
  - Baseline complexity inventory (target files, public API surface, hot methods).
  - Stable selector contract draft and harness deterministic-operation contract.
  - Test-helper API sketch for GUI selector-based interactions.
- Exit criteria:
  - Team agrees on selector and idle-sync contracts; no active blocking conflicts on board.

### Checkpoint B: Core Refactors
- Deliverables:
  - `EditorManager` lifecycle/state extraction lands in incremental slices.
  - Panel runtime policy consolidation in place with no behavior regression.
  - Harness shared execution pipeline implemented for click/type/wait/assert.
- Exit criteria:
  - Target builds pass for app + tests, and automation CLI compatibility preserved.

### Checkpoint C: Test/Automation Convergence
- Deliverables:
  - Selector-based GUI tests in key editor paths.
  - Flake-focused waits/idle helpers adopted in integration/E2E suites.
  - Tile16/Overworld refactor complete with parity validation.
- Exit criteria:
  - Stable green on core CI jobs for this track; reduced flaky retries/manual reruns.

## Risks & Mitigations
- Risk 1: Active parallel agent edits conflict with architecture refactor.
  - Mitigation: keep slices narrow, maintain canonical spec + board updates, avoid cross-file broad rewrites in one batch.
- Risk 2: Automation API changes break existing tooling/scripts.
  - Mitigation: additive selector-v2 path with backward compatibility and staged migration.
- Risk 3: Test changes increase runtime.
  - Mitigation: preserve quick/smoke subsets and only expand selector-based checks in critical paths.
- Risk 4: iOS/Desktop bridge regressions during editor decomposition.
  - Mitigation: lock bridge contract tests and run focused iOS build validation after integration-touching changes.

## Testing & Validation
- Required test/build targets:
  - `cmake --build --preset dev --target yaze yaze_test_unit --parallel 8`
  - `cmake --build build --config RelWithDebInfo --target yaze_test --parallel 8`
  - Focused filters for editor/panel/automation:
    - `yaze_test_unit --gtest_filter="EditorManagerTest.*:PanelManagerPolicyTest.*:UserSettingsLayoutDefaultsTest.*"`
    - `yaze_test_unit --gtest_filter="*Tile16*:*Overworld*"`
- Automation validation:
  - `scripts/agents/test-grpc-api.sh`
  - targeted `z3ed` GUI automation commands (widget discovery, replay, status)
- ROM/test data requirements:
  - vanilla and Oracle ROM fixtures as currently used in integration tests.
- Manual validation:
  - dungeon/overworld editor panel open/close + dock persistence
  - selector-driven automation click/type/assert on at least one panel per editor family

## Documentation Impact
- Public docs to update:
  - `docs/public/reference/api.md` (selector contract + deterministic ops)
  - `docs/public/reference/changelog.md` / release notes for behavior and API deltas
- Internal docs/templates to update:
  - `docs/internal/architecture/editor_manager.md`
  - `docs/internal/architecture/editor_card_layout_system.md`
  - this initiative spec (canonical)
- Helper scripts to use/log:
  - `scripts/agents/smoke-build.sh`
  - `scripts/agents/run-tests.sh`
  - `scripts/agents/run-gh-workflow.sh`
  - `scripts/agents/test-grpc-api.sh`

## Checkpoint A Outputs (2026-02-14)

### A1. Baseline Complexity Inventory
- Measurement note: method span is approximate (`current_definition_line -> next_definition_line`).
- Repro command: `scripts/agents/gui-complexity-baseline.sh`

| File | LOC | Includes | Impl Methods | Public API Decls | Notes |
| --- | ---: | ---: | ---: | ---: | --- |
| `src/app/editor/editor_manager.cc` | 3828 | 97 | 96 | 74 (`editor_manager.h`) | Primary orchestration hotspot. |
| `src/app/editor/ui/ui_coordinator.cc` | 1735 | 33 | 43 | 41 (`ui_coordinator.h`) | Menu/status/startup surface complexity. |
| `src/app/editor/system/panel_manager.cc` | 2010 | 21 | 88 | 116 (`panel_manager.h`) | Large policy/state surface for panels. |
| `src/app/editor/menu/right_panel_manager.cc` | 1660 | 26 | 41 | 19 (`right_panel_manager.h`) | Separate panel runtime path. |
| `src/app/editor/overworld/tile16_editor.cc` | 3579 | 22 | 64 | n/a | Monolithic editor (state + commands + rendering). |
| `src/app/editor/overworld/overworld_editor.cc` | 3445 | 65 | 61 | n/a | Broad map/render/input/save responsibilities. |
| `src/app/service/imgui_test_harness_service.cc` | 1845 | 30 | 13 | n/a | RPC action handlers are large and repeated. |
| `src/app/editor/dungeon/dungeon_canvas_viewer.cc` | 2015 | 37 | 14 | n/a | Dense rendering/interaction path. |
| `src/app/editor/dungeon/dungeon_object_selector.cc` | 1459 | 30 | 25 | n/a | Mixed browser/editor/preview responsibilities. |
| `src/app/gui/automation/widget_id_registry.cc` | 450 | 15 | n/a | n/a | Registry exists; adoption is partial. |
| `src/app/service/widget_discovery_service.cc` | 259 | 11 | n/a | n/a | Discovery payload builder is concise. |

#### Hottest Methods (approx span)
- `EditorManager`:
  - `LoadProjectWithRom` (~218 lines)
  - `InitializeSubsystems` (~194 lines)
  - `SaveRom` (~182 lines)
  - `DrawInterface` (~160 lines)
  - `LoadAssets` (~158 lines)
- `Tile16Editor`:
  - `UpdateTile16Edit` (~811 lines)
  - `Update` (~156 lines)
  - `DrawPaletteSettings` (~154 lines)
  - `DrawToCurrentTile16` (~150 lines)
- `OverworldEditor`:
  - `CheckForCurrentMap` (~248 lines)
  - `CheckForOverworldEdits` (~238 lines)
  - `LoadGraphics` (~170 lines)
  - `DrawOverworldCanvas` (~166 lines)
- `ImGuiTestHarnessServiceImpl`:
  - `ReplayTest` (~341 lines)
  - `Assert` (~210 lines)
  - `Wait` (~183 lines)
  - `Type` (~172 lines)
  - `Click` (~169 lines)

#### Test/AUTOMATION Fragility Signals
- E2E string-coupled interaction volume:
  - `ItemClick(...)`: 141 uses in `test/e2e`
  - `ItemExists(...)`: 52 uses in `test/e2e`
  - `WindowInfo(...)`: 46 uses in `test/e2e`
- Widget registry lifecycle is wired (`BeginFrame`/`EndFrame` in `controller.cc`), but auto-register wrappers are minimally adopted across editor UI.

### A2. Stable Selector Contract Draft (v2 additive)
Goal: replace label/path fragility with stable keys while preserving backward compatibility.

#### Selector Model
- Canonical selector key: `widget_key` (stable, registry-backed, not user-facing text).
- Debug selector: `imgui_id` (non-stable; diagnostic only).
- Legacy selector: current `target` / `condition` string fields.

#### Proposed gRPC Additions
- Add `widget_key` fields to action requests (`Click`, `Type`, `Wait`, `Assert`) while keeping legacy fields.
- Add `resolved_widget_key` + `resolved_path` in responses for migration visibility.
- Expand `DiscoverWidgetsResponse` entries with:
  - `widget_key` (stable key)
  - `legacy_path` (current path label fallback)
  - `alias_of` (optional renamed-key mapping)

#### Stability Rules
- `widget_key` must remain stable across:
  - display-name changes
  - icon/theme changes
  - docking/layout movement
- `widget_key` changes only with explicit alias mapping in a central migration table.
- Session-aware widgets append deterministic session scope (no random suffixes).

### A3. Deterministic Harness Operation Contract Draft
Goal: remove race/flakiness from deferred UI state and async editor operations.

#### Proposed RPCs
- `FlushUiActions`: process deferred editor/layout queues for current frame.
- `WaitForIdle(timeout_ms, stable_frames, flush_first)`:
  - waits until UI reaches deterministic idle criteria.
- `GetUiSyncState`:
  - returns frame and pending-work counters for diagnostics.

#### Idle Criteria (initial)
- `EditorManager` deferred action queue empty.
- `LayoutCoordinator` deferred action queue empty.
- No queued harness actions pending completion.
- UI stable for `N` consecutive frames (default `N=2`).

#### Diagnostics Contract
- `WaitForIdleResponse` should include:
  - `last_frame_id`
  - pending counters by subsystem
  - timeout reason (if failed)

### A4. GUI Selector Test-Helper API Sketch
Target file family: `test/gui_selector_test_utils.*` (new).

```cpp
namespace yaze::test::gui {
struct WidgetRef {
  std::string widget_key;
  std::string resolved_path;
  uint32_t imgui_id = 0;
};

absl::StatusOr<WidgetRef> ResolveWidget(ImGuiTestContext* ctx,
                                        absl::string_view widget_key);
absl::Status ClickWidget(ImGuiTestContext* ctx, absl::string_view widget_key);
absl::Status TypeWidget(ImGuiTestContext* ctx, absl::string_view widget_key,
                        absl::string_view text, bool clear_first);
absl::Status WaitForWidget(ImGuiTestContext* ctx, absl::string_view widget_key,
                           int timeout_ms = 5000);
absl::Status WaitForUiIdle(ImGuiTestContext* ctx, int timeout_ms = 5000);
}  // namespace yaze::test::gui
```

#### Migration Intent
- Keep existing `ItemClick("label")` helpers for short-term compatibility.
- New tests should default to `widget_key` helpers.
- Critical existing E2E suites migrate in place (Dungeon + Overworld + Tile16 first).

### A5. Checkpoint A Exit Status
- Baseline complexity inventory: complete.
- Selector/idle-sync contracts: draft complete (additive, backward-compatible).
- Test-helper API sketch: complete.
- Ready to begin Checkpoint B implementation slices.

## Checkpoint B Progress

### B1. Selector-v2 Additive Wiring (2026-02-14)
- Status: complete (slice 1).
- Scope delivered:
  - Proto additions in `src/protos/imgui_test_harness.proto`:
    - request `widget_key` for `Click`/`Type`/`Wait`/`Assert`
    - response `resolved_widget_key` + `resolved_path` for those actions
    - discovery payload additions (`widget_key`, `legacy_path`, `alias_of`)
  - Harness/service wiring in `src/app/service/imgui_test_harness_service.cc`:
    - selector resolution path (`widget_key` -> `WidgetIdRegistry`, fallback legacy `target/condition`)
    - wait/assert default selector behavior when `widget_key` is provided without a target payload
    - replay support passes optional `widget_key` through per-step action requests
  - Script pipeline wiring:
    - `TestScriptStep.widget_key` added in parser/recorder path
    - recorder emits `widget_key` and parser round-trips it
  - CLI client and command updates:
    - `GuiAutomationClient` sends/receives selector-v2 fields
    - `gui-click` accepts `--widget-key` as an alternative to `--target`
- Backward compatibility:
  - Legacy `target`/`condition` usage preserved (no behavior break expected for existing scripts).
- Validation:
  - `cmake --build --preset dev --target z3ed yaze_grpc_support --parallel 8` passed.
