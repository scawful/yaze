# Testing Docs Index

Status: ACTIVE
Owner: test-infrastructure-expert
Last Reviewed: 2026-09-16
Next Review: 2026-09-30

This file is an index only. It deliberately holds no commands or strategy text so
there is exactly one place to update when the test setup changes.

## Source of truth

| Question | Read |
|----------|------|
| How is the test suite structured, and how do I run it? | [`test/README.md`](../../../test/README.md) |
| What runs in CI, on which trigger, with which labels? | [overview.md](overview.md) |
| Which CMake options gate ROM / AI / benchmark suites? | [configuration.md](configuration.md) |
| What do I check before pushing? | [pre-push-checklist.md](pre-push-checklist.md) |
| Build presets and test commands | [docs/public/build/quick-reference.md](../../public/build/quick-reference.md) |
| Contributor-facing testing walkthrough | [docs/public/developer/testing-guide.md](../../public/developer/testing-guide.md) |
| Running without ROM files | [docs/public/developer/testing-without-roms.md](../../public/developer/testing-without-roms.md) |

## Specialised references

| Topic | Doc |
|-------|-----|
| ODR / duplicate symbol detection | [symbol-conflict-detection.md](symbol-conflict-detection.md) (+ [sample-symbol-database.json](sample-symbol-database.json)) |
| DungeonEditorV2 E2E test design | [dungeon-gui-test-design.md](dungeon-gui-test-design.md) |
| Script classification (test runners, validators) | [scripts/README.md](../../../scripts/README.md) |

## Conventions

- ROM policy: CI runs without ROMs and skips ROM-dependent suites. Enable them locally
  with `YAZE_ENABLE_ROM_TESTS=ON` plus `YAZE_TEST_ROM_*` paths, or force-skip with
  `YAZE_SKIP_ROM_TESTS=1`. Details in [configuration.md](configuration.md).
- Test work is tracked in the universe coordination log
  (`scripts/agents/coord task-list --status active`), not in markdown status pages.
- Archived testing docs live in
  [`agents/archive/testing-docs-2025/`](../agents/archive/testing-docs-2025/archive-index.md)
  and are history only.
