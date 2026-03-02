# yaze Status

## Current Release
- **Version**: v0.7.0 (March 2026)
- **Focus**: Editor feature completion — P0 complete, P1 items remaining

## 0.7.0 Completion Tracking
- See `docs/internal/plans/0.7.0-feature-completion.md` for full task breakdown
- P0 (must-ship): ALL COMPLETE
  - Tile16 palette/render pipeline (DONE)
  - Sprite undo/redo (DONE — 9 tests)
  - Screen undo/redo (DONE — 5 tests)
  - Message find/replace (DONE — 8 tests)
  - Desktop BPS export (DONE — 13 tests)
- P1 (should-ship): Usage statistics card (DONE), Tracker stubs, OW item deletion, CLI palette commands, Dungeon usage tracker viz
- 1538 unit tests passing

## Recent Commits
- `4b62b14e feat: implement P0 features for 0.7.0 release`
- `ae07276b docs(roadmap): update 0.7.0 completion status and tile16 backlog`
- `97160983 docs(0.7.0): mark screen undo complete across trackers`

## CI/CD
- GitHub Actions CI covers Linux/macOS/Windows with stable test labels.
- Release workflow packages DMG/NSIS/DEB+TGZ artifacts.

## Tracking
- Coordination board: `docs/internal/agents/coordination-board.md`
- Canonical roadmap: `docs/internal/roadmap.md`
- Feature plan: `docs/internal/plans/0.7.0-feature-completion.md`
