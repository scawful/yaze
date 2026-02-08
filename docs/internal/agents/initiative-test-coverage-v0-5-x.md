# Initiative: v0.5.x Test Coverage Expansion

Status: IN_PROGRESS  
Owner: backend-infra-engineer  
Created: 2026-01-11  
Last Reviewed: 2026-01-11  
Next Review: 2026-01-25  
Coordination Board: docs/internal/agents/coordination-board.md

## Summary
- Lead agent/persona: backend-infra-engineer
- Supporting agents: test-infrastructure-expert, imgui-frontend-engineer, snes-emulator-expert
- Problem statement: Coverage gaps in GUI workflows (Graphics/Sprite/Message/Music), emulator save-state UI, and WASM debug API checks are not enforced in CI.
- Success metrics:
  - GUI smoke tests cover Graphics/Sprite/Message/Music editors.
  - Emulator Save States panel validated in GUI tests.
  - WASM debug API tests run in CI and report pass/fail.
  - Coverage report updated to reflect new tests and remaining gaps.

## Scope
- In scope:
  - GUI E2E/smoke tests for Graphics, Sprite, Message, Music editors.
  - Emulator Save States panel UI validation.
  - WASM debug API test automation in CI (Playwright + debug API script).
  - Documentation updates for coverage report/testing guide.
- Out of scope:
  - Emulator core accuracy fixes.
  - New editor features or ROM schema changes.
  - Broad refactors of test framework beyond needed hooks.

## Work Breakdown (Tasks/Issues)
| Task | Owner | Milestone | Status |
| --- | --- | --- | --- |
| Add GUI smoke tests for Graphics/Sprite/Message (panel visibility + key widgets) | backend-infra-engineer | M1 | DONE |
| Add GUI smoke test for Music editor panels | backend-infra-engineer | M1 | DONE |
| Add emulator Save States UI test (panel + placeholder text) | backend-infra-engineer | M1 | DONE |
| Promote WASM debug API tests into CI (Playwright + injected script) | backend-infra-engineer | M1 | DONE |
| Expand CLI coverage for doctor/editor automation | test-infrastructure-expert | M2 | TODO |
| Add ROM-dependent tests for version-gated overworld + dungeon persistence | zelda3-hacking-expert | M2 | TODO |
| Add settings/layout serialization tests | imgui-frontend-engineer | M2 | TODO |
| Review AI runtime GUI tests for stability (gating + skips) | test-infrastructure-expert | M3 | TODO |

## Risks & Mitigations
- GUI tests flaky in headless mode → keep smoke tests minimal and use panel visibility + non-destructive checks.
- WASM CI time increases → run Playwright only on web build workflow and limit to 1 browser.
- ROM availability in CI → tests must skip or degrade gracefully when ROM env vars are missing.

## Testing & Validation
- Required test targets: `yaze_test_gui`, `yaze_test_unit`, `yaze_test_integration`, Playwright smoke for web.
- ROM/test data requirements: `YAZE_TEST_ROM_VANILLA` for GUI editor tests with ROM-dependent widgets.
- Manual validation (if needed): run `./build/bin/yaze_test --ui --show-gui --gtest_filter="*Editor*Smoke*"` locally.

## Documentation Impact
- Public docs to update: `docs/public/reference/feature-coverage-report.md`, `docs/public/developer/testing-guide.md` (if needed).
- Internal docs/templates to update: none.
- Coordination board entry link: docs/internal/agents/coordination-board.md (entry created 2026-01-11).
- Helper scripts to log: `scripts/agents/run-tests.sh`, `scripts/agents/run-gh-workflow.sh`, `scripts/agents/smoke-build.sh`.

## Timeline / Checkpoints
- M1 (2026-01-18): GUI smoke tests for Graphics/Sprite/Message/Music + Save States panel + WASM debug API CI.
- M2 (2026-02-01): CLI doctor/editor automation coverage + ROM-dependent version-gated saves.
- M3 (2026-02-15): Settings/layout serialization tests + AI GUI test gating review.
