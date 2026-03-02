# yaze Status

## Current Release
- **Version**: v0.7.0 (March 2026)
- **Focus**: Editor feature completion — closing undo/redo gaps, BPS export, ZScream parity items

## 0.7.0 Completion Tracking
- See `docs/internal/plans/0.7.0-feature-completion.md` for full task breakdown
- P0 (must-ship): Tile16 palette/render pipeline (DONE), Message replace (DONE), Sprite undo, Screen undo, BPS export
- P1 (should-ship): Usage statistics card (DONE), Tracker stubs, OW item deletion, CLI palette commands

## Recent Commits (March 2, 2026)
- `feat(message): implement find/replace workflow with tests`
- `feat(overworld): wire usage statistics card to real map data`
- `refactor(tile16): extract renderer and usage index with coverage`

## CI/CD
- GitHub Actions CI covers Linux/macOS/Windows with stable test labels.
- Release workflow packages DMG/NSIS/DEB+TGZ artifacts.

## Tracking
- Coordination board: `docs/internal/agents/coordination-board.md`
- Canonical roadmap: `docs/internal/roadmap.md`
- Feature plan: `docs/internal/plans/0.7.0-feature-completion.md`
