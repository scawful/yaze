# YAZE Testing Infrastructure

This directory contains comprehensive documentation for YAZE's testing infrastructure, designed to prevent build failures and ensure code quality across platforms.

## Quick Start

**Before pushing code**:
```bash
# Unix/macOS
./scripts/pre-push-test.sh

# Windows
.\scripts\pre-push-test.ps1
```

**Time**: ~2 minutes
**Prevents**: ~90% of CI failures

## Documents in This Directory

### 1. [Gap Analysis](gap-analysis.md)
**Purpose**: Documents what testing gaps led to recent CI failures

**Key Sections**:
- Issues we didn't catch (Windows Abseil, Linux FLAGS conflicts)
- Current testing coverage analysis
- CI/CD coverage gaps
- Root cause analysis by issue type

**Read this if**: You want to understand why we built this infrastructure

### 2. [Testing Strategy](testing-strategy.md)
**Purpose**: Complete guide to YAZE's 5-level testing pyramid

**Key Sections**:
- Level 0-6: From static analysis to E2E tests
- When to run each test level
- Test organization and naming conventions
- Platform-specific testing considerations
- Debugging test failures

**Read this if**: You need to write tests or understand the testing framework

### 3. [Pre-Push Checklist](pre-push-checklist.md)
**Purpose**: Step-by-step checklist before pushing code

**Key Sections**:
- Quick start commands
- Detailed checklist for each test level
- Platform-specific checks
- Troubleshooting common issues
- CI-matching presets

**Read this if**: You're about to push code and want to make sure it'll pass CI

### 4. [CI Improvements Proposal](ci-improvements-proposal.md)
**Purpose**: Technical proposal for enhancing CI/CD pipeline

**Key Sections**:
- Proposed new CI jobs (config validation, compile-check, symbol-check)
- Job dependency graph
- Time and cost analysis
- Implementation plan
- Success metrics

**Read this if**: You're working on CI/CD infrastructure or want to understand planned improvements

## Testing Levels Overview

```
Level 0: Static Analysis     → < 1 second   → Format, lint
Level 1: Config Validation   → ~10 seconds  → CMake, includes
Level 2: Smoke Compilation   → ~90 seconds  → Headers, preprocessor
Level 3: Symbol Validation   → ~30 seconds  → ODR, conflicts
Level 4: Unit Tests          → ~30 seconds  → Logic, algorithms
Level 5: Integration Tests   → 2-5 minutes  → Multi-component
Level 6: E2E Tests           → 5-10 minutes → Full workflows
```

## Scripts

### Pre-Push Test Scripts
- **Unix/macOS**: `scripts/pre-push-test.sh`
- **Windows**: `scripts/pre-push-test.ps1`

**Usage**:
```bash
# Run all checks
./scripts/pre-push-test.sh

# Only validate configuration
./scripts/pre-push-test.sh --config-only

# Skip symbol checking
./scripts/pre-push-test.sh --skip-symbols

# Skip tests (faster)
./scripts/pre-push-test.sh --skip-tests

# Verbose output
./scripts/pre-push-test.sh --verbose
```

### Symbol Verification Script
- **Unix/macOS**: `scripts/verify-symbols.sh`
- **Windows**: `scripts/verify-symbols.ps1` (TODO)

**Usage**:
```bash
# Check for symbol conflicts
./scripts/verify-symbols.sh

# Show detailed output
./scripts/verify-symbols.sh --verbose

# Show all symbols (including safe duplicates)
./scripts/verify-symbols.sh --show-all

# Use custom build directory
./scripts/verify-symbols.sh --build-dir build_test
```

## Success Metrics

### Target Goals
- **Time to first failure**: <5 minutes (down from ~15 min)
- **PR iteration time**: 30-60 minutes (down from 2-4 hours)
- **CI failure rate**: <10% (down from ~30%)
- **Symbol conflicts caught**: 100% (up from manual detection)

### Current Status
- ✅ Pre-push infrastructure created
- ✅ Symbol checker implemented
- ✅ Gap analysis documented
- 🔄 CI improvements planned (see proposal)

## Related Documentation

### Project-Wide
- `CLAUDE.md` - Project overview and build guidelines
- `docs/public/build/quick-reference.md` - Build commands
- `docs/public/build/troubleshooting.md` - Platform-specific fixes

### Developer Guides
- `docs/public/developer/testing-guide.md` - Testing best practices
- `docs/public/developer/testing-without-roms.md` - ROM-independent testing
