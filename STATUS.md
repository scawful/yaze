# yaze Status

## Current Development
- **Version**: v0.8.0 development (latest tagged release: v0.7.2, July 17,
  2026)
- **Focus**: Dungeon Editor completion, persistence stability, and Oracle
  daily-driver readiness.
- **Milestone plan**: `0.8.0` dungeon completion — see
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

## 0.7.2 Validation Snapshot

**Last full local suites: July 11, 2026**

- `ctest --preset mac-ai-unit --output-on-failure -j4` → **2353/2353
  non-failing** (26 fixture-dependent skips).
- `YAZE_TEST_ROM_VANILLA=roms/alttp_vanilla.sfc ctest --preset
  mac-ai-integration --output-on-failure -j4` → **274/274 non-failing** (16
  expanded/Oracle-ROM-only skips).
- PR #70 added fail-closed, whole-ROM transactional saves and reported 2,407
  unit tests, 619 quick-editor tests, and 279 vanilla/Oracle integration tests
  passing before merge on July 12, 2026.
- Post-merge `master` CI completed successfully for PR #70 merge commit
  `06575b15` ([run 29211231166](https://github.com/scawful/yaze/actions/runs/29211231166)).
- The non-publishing Release workflow dry run completed successfully across
  Linux, macOS, and Windows, including the automated Windows zip layout and
  `z3ed.exe --help` smoke ([run 29211528653](https://github.com/scawful/yaze/actions/runs/29211528653)).

### Release Follow-up
- v0.7.2 is tagged at merge commit `8fed46dc`; tag-triggered workflow run
  [29608567567](https://github.com/scawful/yaze/actions/runs/29608567567)
  owns its artifact builds, checksum verification, and publication.
- The deferred interactive packaged-Windows open, Save As, and reopen smoke is
  tracked in GitHub issue #107; a confirmed startup or save regression remains
  hotfix criteria.

## 0.7.0 Validation Snapshot (historical)
- `ctest --preset mac-ai-unit --output-on-failure` → **1622/1622 passed**
- `ctest --preset mac-ai-integration --output-on-failure` → **237/237 passed**
- `ctest --preset mac-ai-unit --output-on-failure --tests-regex "(ObjectDrawerRegistryReplayTest|ObjectTileEditorTest|CustomObjectManagerTest|ObjectParserTest|Tile16EditorActionStateTest|OverworldItemOperationsTest)"` → **68/68 passed**

## Recent Dungeon Follow-through
- Editable pit-damage room membership controls and integration classification
  (PRs #64–65).
- Pushable-block loader operand/capacity guards with boundary coverage (PRs
  #66–67).
- A hardcoded room `0x001` object-overlap compositing golden (PR #68).
- Whole-ROM save rollback, dirty-state restoration, and fail-closed dungeon
  stream boundaries (PR #70).

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
