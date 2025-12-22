# Testing Documentation Archive (November 2025)

This directory contains testing-related documentation that was archived during a comprehensive cleanup of `/docs/internal/testing/` to reduce duplication and improve maintainability.

## Archive Rationale

The testing directory contained 25 markdown files with significant duplication of content from:
- `test/README.md` - The canonical test suite documentation
- `docs/public/build/quick-reference.md` - The canonical build reference
- `docs/internal/ci-and-testing.md` - CI/CD pipeline documentation

## Archived Files (6 total)

### Bloated/Redundant Documentation

1. **testing-strategy.md** (843 lines)
   - Duplicates the tiered testing strategy from `test/README.md`
   - Reason: Content moved to canonical test/README.md
   - Reference: See test/README.md for current strategy

2. **TEST_INFRASTRUCTURE_IMPROVEMENT_PLAN.md** (2257 lines)
   - Massive improvement proposal document
   - Duplicates much of test/README.md and docs/internal/ci-and-testing.md
   - Reason: Content integrated into existing canonical docs
   - Reference: Implementation recommendations are in docs/internal/ci-and-testing.md

3. **ci-improvements-proposal.md** (690 lines)
   - Detailed CI/CD improvement proposals
   - Overlaps significantly with docs/internal/ci-and-testing.md
   - Reason: Improvements documented in canonical CI/testing doc
   - Reference: See docs/internal/ci-and-testing.md

4. **cmake-validation.md** (672 lines)
   - CMake validation guide
   - Duplicates content from docs/public/build/quick-reference.md
   - Reason: Build validation covered in quick-reference.md
   - Reference: See docs/public/build/quick-reference.md

5. **integration-plan.md** (505 lines)
   - Testing infrastructure integration planning document
   - Much of content duplicated in test/README.md
   - Reason: Integration approach implemented and documented elsewhere
   - Reference: See test/README.md for current integration approach

6. **matrix-testing-strategy.md** (499 lines)
   - Platform/configuration matrix testing strategy
   - Some unique content but much is duplicated in other docs
   - Reason: Matrix testing implementation is in scripts/
   - Reference: Check scripts/test-config-matrix.sh and related scripts

## Deleted Files (14 total - Already in git staging)

These files were completely duplicative and offered no unique value:

1. **QUICKSTART.md** - Exact duplicate of QUICK_START_GUIDE.md
2. **QUICK_START_GUIDE.md** - Duplicates test/README.md Quick Start section
3. **QUICK_REFERENCE.md** - Redundant quick reference for symbol detection
4. **README_TESTING.md** - Duplicate hub documentation
5. **TESTING_INDEX.md** - Navigation index (redundant)
6. **ARCHITECTURE_HANDOFF.md** - AI-generated project status document
7. **INITIATIVE.md** - AI-generated project initiative document
8. **EXECUTIVE_SUMMARY.md** - AI-generated executive summary
9. **IMPLEMENTATION_GUIDE.md** - Symbol detection implementation guide (superseded)
10. **MATRIX_TESTING_README.md** - Matrix testing system documentation
11. **MATRIX_TESTING_IMPLEMENTATION.md** - Matrix testing implementation guide
12. **MATRIX_TESTING_CHECKLIST.md** - Matrix testing checklist
13. **SYMBOL_DETECTION_README.md** - Duplicate of symbol-conflict-detection.md
14. **TEST_INFRASTRUCTURE_IMPROVEMENT_PLAN.md** - (see archived files above)

## Files Retained (5 total in docs/internal/testing/)

1. **dungeon-gui-test-design.md** (1007 lines)
   - Unique architectural test design for dungeon editor
   - Specific to DungeonEditorV2 testing with ImGuiTestEngine
   - Rationale: Contains unique architectural and testing patterns not found elsewhere

2. **pre-push-checklist.md** (335 lines)
   - Practical developer checklist for pre-commit validation
   - Links to scripts and CI verification
   - Rationale: Useful operational checklist referenced by developers

3. **README.md** (414 lines)
   - Hub documentation for testing infrastructure
   - Links to canonical testing documents and resources
   - Rationale: Serves as navigation hub to various testing documents

4. **symbol-conflict-detection.md** (440 lines)
   - Complete documentation for symbol conflict detection system
   - Details on symbol extraction, detection, and pre-commit hooks
   - Rationale: Complete reference for symbol conflict system

5. **sample-symbol-database.json** (1133 bytes)
   - Example JSON database for symbol conflict detection
   - Supporting documentation for symbol system
   - Rationale: Example data for understanding symbol database format

## Canonical Documentation References

When working with testing, refer to these canonical sources:

- **Test Suite Overview**: `test/README.md` (407 lines)
  - Tiered testing strategy, test structure, running tests
  - How to write new tests, CI configuration

- **Build & Test Quick Reference**: `docs/public/build/quick-reference.md`
  - CMake presets, common build commands
  - Test execution quick reference

- **CI/CD Pipeline**: `docs/internal/ci-and-testing.md`
  - CI workflow configuration, test infrastructure
  - GitHub Actions integration

- **CLAUDE.md**: Project root CLAUDE.md
  - References canonical test documentation
  - Links to quick-reference.md and test/README.md

## How to Restore

If you need to reference archived content:

```bash
# View specific archived document
cat docs/internal/agents/archive/testing-docs-2025/testing-strategy.md

# Restore if needed
mv docs/internal/agents/archive/testing-docs-2025/<filename>.md docs/internal/testing/
```

## Cleanup Results

- **Before**: 25 markdown files (12,170 total lines)
- **After**: 5 markdown files (2,943 total lines)
- **Reduction**: 75.8% fewer files, 75.8% fewer lines
- **Result**: Cleaner documentation structure, easier to maintain, reduced duplication

## Related Cleanup

This cleanup was performed as part of documentation janitor work to:
- Remove AI-generated spam and duplicate documentation
- Enforce single source of truth for each documentation topic
- Keep root documentation directory clean
- Maintain clear, authoritative documentation structure
