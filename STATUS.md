# yaze Status

## Current Release
- **Version**: v0.7.2 (release-candidate prep; latest tagged release is
  v0.7.1 from April 19, 2026)
- **Focus**: Post-0.7.1 dungeon RC stabilization plus 0.8.0 room-render
  regression fixtures, pit-damage table encoder, and dungeon compositing tests.
- **Next milestone**: `0.8.0` dungeon completion — see
  `docs/internal/plans/release-ladder-0x-2026.md` and
  `docs/internal/plans/dungeon-0.8.0-issue-test-backlog-2026-06-28.md`.
- **Beta-feedback backlog**:
  `docs/internal/plans/yaze-beta-feedback-backlog-2026-07-01.md`.

## 0.7.0 Completion Tracking
- See `docs/internal/plans/0.7.0-feature-completion.md` for full task breakdown
- P0 (must-ship): ALL COMPLETE
  - Tile16 palette/render pipeline (DONE)
  - Sprite undo/redo (DONE — 9 tests)
  - Screen undo/redo (DONE — 5 tests)
  - Message find/replace (DONE — 8 tests)
  - Desktop BPS export (DONE — 13 tests)
- P1 (should-ship): ALL COMPLETE
  - Usage statistics card (DONE)
  - Tracker stubs (DONE)
  - Overworld item deletion + iteration continuity flow (DONE)
  - CLI palette commands (DONE)
  - Dungeon usage tracker visualization (DONE)
- P2 (deferred to 0.8.0): scratch persistence, eyedropper tool, project-file flow, SPC/MML import, graphics clipboard

## 0.7.2 Validation Snapshot (June 11, 2026)
- `ctest --preset mac-ai-unit` → **2326/2326 passed**
- `ctest --preset mac-ai-integration` → **254/254 passed**
- Flake watch: `DungeonIssueReportStorageTest` (2 tests) failed once under
  heavy system load (parallel builds running); passed on all re-runs,
  serial and `-j4`.

## 0.7.0 Validation Snapshot (historical)
- `ctest --preset mac-ai-unit --output-on-failure` → **1622/1622 passed**
- `ctest --preset mac-ai-integration --output-on-failure` → **237/237 passed**
- `ctest --preset mac-ai-unit --output-on-failure --tests-regex "(ObjectDrawerRegistryReplayTest|ObjectTileEditorTest|CustomObjectManagerTest|ObjectParserTest|Tile16EditorActionStateTest|OverworldItemOperationsTest)"` → **68/68 passed**

## Recent Commits
- `42137074 dungeon: harden object labels and door bounds`
- `535c6675 overworld: expose tile16 sampling from canvas menu`
- `4f2ea6b4 docs: refresh dungeon persistence and preview status`

## Legacy 0.7.0 Stretch Follow-ups
- `task_20260303T212605Z_32450` — Tile16 quadrant strip/hotkeys parity tests
- `task_20260303T212605Z_29503` — Overworld batch item undo coverage
- `task_20260303T212605Z_17791` — Dungeon registry-first dispatcher policy + ownership docs

## CI/CD
- GitHub Actions CI covers Linux/macOS/Windows with stable test labels.
- Release workflow packages DMG/NSIS/DEB+TGZ artifacts.

## Tracking
- Coordination board snapshot: `docs/internal/agents/coordination-board.generated.md`
- Canonical roadmap: `docs/internal/roadmap.md`
- Current release plan: `docs/internal/plans/release-ladder-0x-2026.md`
- 0.7.0 completion plan: `docs/internal/plans/0.7.0-feature-completion.md`
