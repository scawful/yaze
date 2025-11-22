# Symbol Conflict Detection - Quick Reference

## One-Minute Setup

```bash
# 1. Enable git hooks (one-time)
git config core.hooksPath .githooks

# 2. Make hook executable
chmod +x .githooks/pre-commit

# Done! Hook now runs automatically on git commit
```

## Common Commands

### Extract Symbols
```bash
./scripts/extract-symbols.sh          # Extract from ./build
./scripts/extract-symbols.sh /path    # Extract from custom path
```

### Check for Conflicts
```bash
./scripts/check-duplicate-symbols.sh          # Standard report
./scripts/check-duplicate-symbols.sh --verbose # Show all symbols
./scripts/check-duplicate-symbols.sh --fix-suggestions # With hints
```

### Test System
```bash
./scripts/test-symbol-detection.sh  # Full system validation
```

### Combined Check
```bash
./scripts/extract-symbols.sh && ./scripts/check-duplicate-symbols.sh
```

## Pre-Commit Hook

```bash
# Automatic (runs before commit)
git commit -m "message"

# Skip if intentional
git commit --no-verify -m "message"

# See what changed
git diff --cached --name-only
```

## Conflict Resolution

### Global Variable Duplicate

**Issue:**
```
SYMBOL CONFLICT DETECTED:
  Symbol: FLAGS_rom
  Defined in:
    - flags.cc.o
    - emu_test.cc.o
```

**Fixes:**

Option 1 - Use `static`:
```cpp
static ABSL_FLAG(std::string, rom, "", "Path to ROM");
```

Option 2 - Use anonymous namespace:
```cpp
namespace {
  ABSL_FLAG(std::string, rom, "", "Path to ROM");
}
```

Option 3 - Declare elsewhere:
```cpp
// header.h
extern ABSL_FLAG(std::string, rom);

// source.cc (only here!)
ABSL_FLAG(std::string, rom, "", "Path to ROM");
```

### Function Duplicate

**Fixes:**

Option 1 - Use `inline`:
```cpp
inline void Process() { /* ... */ }
```

Option 2 - Use `static`:
```cpp
static void Process() { /* ... */ }
```

Option 3 - Use anonymous namespace:
```cpp
namespace {
  void Process() { /* ... */ }
}
```

### Class Member Duplicate

**Fixes:**

```cpp
// header.h
class Widget {
  static int count;  // Declaration only
};

// source.cc (ONLY here!)
int Widget::count = 0;

// test.cc
// Just use Widget::count, don't redefine!
```

## Symbol Types

| Type | Meaning | Location |
|------|---------|----------|
| T | Code/Function | .text |
| D | Data (init) | .data |
| R | Read-only | .rodata |
| B | BSS (uninit) | .bss |
| C | Common | (weak) |
| U | Undefined | (reference) |

## Workflow

### During Development
```bash
[edit files] → [build] → [pre-commit hook warns] → [fix] → [commit]
```

### Before Pushing
```bash
./scripts/extract-symbols.sh && ./scripts/check-duplicate-symbols.sh
```

### In CI/CD
Automatic via `.github/workflows/symbol-detection.yml`

## Files Reference

| File | Purpose |
|------|---------|
| `scripts/extract-symbols.sh` | Extract symbol definitions |
| `scripts/check-duplicate-symbols.sh` | Report conflicts |
| `scripts/test-symbol-detection.sh` | Test system |
| `.githooks/pre-commit` | Pre-commit hook |
| `.github/workflows/symbol-detection.yml` | CI workflow |
| `build/symbol_database.json` | Generated database |

## Debugging

### Check what symbols nm sees
```bash
nm build/CMakeFiles/*/*.o | grep symbol_name
```

### Manually find object files
```bash
find build -name "*.o" -o -name "*.obj" | head -10
```

### Test extraction on one file
```bash
nm build/CMakeFiles/z3ed.dir/src/cli/flags.cc.o | head -20
```

### View symbol database
```bash
python3 -m json.tool build/symbol_database.json | head -50
```

## Exit Codes

```bash
./scripts/check-duplicate-symbols.sh
echo $?  # Output: 0 (no conflicts) or 1 (conflicts found)
```

## Performance

| Operation | Time |
|-----------|------|
| Full extraction | 2-3 seconds |
| Conflict check | <100ms |
| Pre-commit check | 1-2 seconds |

## Notes

- Pre-commit hook only checks **changed files** (fast)
- Full extraction checks **all objects** (comprehensive)
- Hook can be skipped with `--no-verify` if intentional
- Symbol database is kept in `build/` (ignored by git)
- Cross-platform: Works on macOS, Linux, Windows

## Issues?

```bash
# Reset hooks
git config core.hooksPath .githooks
chmod +x .githooks/pre-commit

# Full diagnostic
./scripts/test-symbol-detection.sh

# Clean and retry
rm build/symbol_database.json
./scripts/extract-symbols.sh build
./scripts/check-duplicate-symbols.sh
```

## Learn More

- **Full docs:** `docs/internal/testing/symbol-conflict-detection.md`
- **Implementation:** `docs/internal/testing/IMPLEMENTATION_GUIDE.md`
- **Sample DB:** `docs/internal/testing/sample-symbol-database.json`
