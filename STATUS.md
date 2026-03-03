# yaze Status

## Current Release
- **Version**: v0.7.0 (March 2026)
- **Focus**: 0.7.0 validation gate complete, parity hardening in progress

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

## Validation Snapshot (March 3, 2026)
- `ctest --preset mac-ai-unit --output-on-failure` → **1622/1622 passed**
- `ctest --preset mac-ai-integration --output-on-failure` → **237/237 passed**
- `ctest --preset mac-ai-unit --output-on-failure --tests-regex "(ObjectDrawerRegistryReplayTest|ObjectTileEditorTest|CustomObjectManagerTest|ObjectParserTest|Tile16EditorActionStateTest|OverworldItemOperationsTest)"` → **68/68 passed**

## Recent Commits
- `4b62b14e feat: implement P0 features for 0.7.0 release`
- `ae07276b docs(roadmap): update 0.7.0 completion status and tile16 backlog`
- `97160983 docs(0.7.0): mark screen undo complete across trackers`

## Active 0.7.0 Stretch Tasks
- `task_20260303T212605Z_32450` — Tile16 quadrant strip/hotkeys parity tests
- `task_20260303T212605Z_29503` — Overworld batch item undo coverage
- `task_20260303T212605Z_17791` — Dungeon registry-first dispatcher policy + ownership docs

## CI/CD
- GitHub Actions CI covers Linux/macOS/Windows with stable test labels.
- Release workflow packages DMG/NSIS/DEB+TGZ artifacts.

## Tracking
- Coordination board snapshot: `docs/internal/agents/coordination-board.generated.md`
- Canonical roadmap: `docs/internal/roadmap.md`
- Feature plan: `docs/internal/plans/0.7.0-feature-completion.md`
