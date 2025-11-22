# Symbol Conflict Detection System

## Overview

The Symbol Conflict Detection System is designed to catch **One Definition Rule (ODR) violations** and symbol conflicts **before linking fails**. This prevents wasted time debugging linker errors and improves development velocity.

**The Problem:**
- Developers accidentally define the same symbol in multiple translation units
- Errors only appear at link time (after 10-15+ minutes of compilation on some platforms)
- The error message is often cryptic: `symbol already defined in object`
- No early warning during development

**The Solution:**
- Extract symbols from compiled object files immediately after compilation
- Build a symbol database with conflict detection
- Pre-commit hook warns about conflicts before committing
- CI/CD job fails early if conflicts detected
- Fast analysis: <5 seconds for typical builds

## Quick Start

### Generate Symbol Database

```bash
# Extract all symbols and create database
./scripts/extract-symbols.sh

# Output: build/symbol_database.json
```

### Check for Conflicts

```bash
# Analyze database for conflicts
./scripts/check-duplicate-symbols.sh

# Output: List of conflicting symbols with file locations
```

### Combined Usage

```bash
# Extract and check in one command
./scripts/extract-symbols.sh && ./scripts/check-duplicate-symbols.sh
```

## Components

### 1. Symbol Extraction Tool (`scripts/extract-symbols.sh`)

Scans all compiled object files and extracts symbol definitions.

**Features:**
- Cross-platform support (macOS/Linux/Windows)
- Uses `nm` on Unix/macOS, `dumpbin` on Windows
- Generates JSON database with symbol metadata
- Skips undefined symbols (references only)
- Tracks symbol type (text, data, read-only)

**Usage:**
```bash
# Default: scan ./build directory, output to build/symbol_database.json
./scripts/extract-symbols.sh

# Custom build directory
./scripts/extract-symbols.sh /path/to/custom/build

# Custom output file
./scripts/extract-symbols.sh build symbols.json
```

**Output Format:**
```json
{
  "metadata": {
    "platform": "Darwin",
    "build_dir": "build",
    "timestamp": "2025-11-20T10:30:45.123456Z",
    "object_files_scanned": 145,
    "total_symbols": 8923,
    "total_conflicts": 2
  },
  "conflicts": [
    {
      "symbol": "FLAGS_rom",
      "count": 2,
      "definitions": [
        {
          "object_file": "flags.cc.o",
          "type": "D"
        },
        {
          "object_file": "emu_test.cc.o",
          "type": "D"
        }
      ]
    }
  ],
  "symbols": {
    "FLAGS_rom": [...]
  }
}
```

**Symbol Types:**
- `T` = Text/Code (function in `.text` section)
- `D` = Data (initialized global variable in `.data` section)
- `R` = Read-only (constant in `.rodata` section)
- `B` = BSS (uninitialized global in `.bss` section)
- `U` = Undefined (external reference, not a definition)

### 2. Duplicate Symbol Checker (`scripts/check-duplicate-symbols.sh`)

Analyzes symbol database and reports conflicts in a developer-friendly format.

**Usage:**
```bash
# Check default database (build/symbol_database.json)
./scripts/check-duplicate-symbols.sh

# Specify custom database
./scripts/check-duplicate-symbols.sh /path/to/symbol_database.json

# Verbose output (show all symbols)
./scripts/check-duplicate-symbols.sh --verbose

# Include fix suggestions
./scripts/check-duplicate-symbols.sh --fix-suggestions
```

**Output Example:**
```
=== Duplicate Symbol Checker ===
Database: build/symbol_database.json
Platform: Darwin
Build directory: build
Timestamp: 2025-11-20T10:30:45.123456Z
Object files scanned: 145
Total symbols: 8923
Total conflicts: 2

CONFLICTS FOUND:

[1/2] FLAGS_rom (x2)
      1. flags.cc.o (type: D)
      2. emu_test.cc.o (type: D)

[2/2] g_global_counter (x2)
      1. utils.cc.o (type: D)
      2. utils_test.cc.o (type: D)

=== Summary ===
Total conflicts: 2
Fix these before linking!
```

**Exit Codes:**
- `0` = No conflicts found
- `1` = Conflicts detected

### 3. Pre-Commit Hook (`.githooks/pre-commit`)

Runs automatically before committing code (can be bypassed with `--no-verify`).

**Features:**
- Only checks changed `.cc` and `.h` files
- Fast analysis: ~2-3 seconds
- Warns about conflicts in affected object files
- Suggests common fixes
- Non-blocking (just a warning, doesn't fail the commit)

**Usage:**
```bash
# Automatically runs on git commit
git commit -m "Your message"

# Skip hook if needed
git commit --no-verify -m "Your message"
```

**Setup (first time):**
```bash
# Configure Git to use .githooks directory
git config core.hooksPath .githooks

# Make hook executable
chmod +x .githooks/pre-commit
```

**Hook Output:**
```
[Pre-Commit] Checking for symbol conflicts...
Changed files:
  src/cli/flags.cc
  test/emu_test.cc

Affected object files:
  build/CMakeFiles/z3ed.dir/src/cli/flags.cc.o
  build/CMakeFiles/z3ed_test.dir/test/emu_test.cc.o

Analyzing symbols...

WARNING: Symbol conflicts detected!

Duplicate symbols in affected files:
  FLAGS_rom
    - flags.cc.o
    - emu_test.cc.o

You can:
  1. Fix the conflicts before committing
  2. Skip this check: git commit --no-verify
  3. Run full analysis: ./scripts/extract-symbols.sh && ./scripts/check-duplicate-symbols.sh

Common fixes:
  - Add 'static' keyword to make it internal linkage
  - Use anonymous namespace in .cc files
  - Use 'inline' keyword for function/variable definitions
```

## Common Fixes for ODR Violations

### Problem: Global Variable Defined in Multiple Files

**Bad:**
```cpp
// flags.cc
ABSL_FLAG(std::string, rom, "", "Path to ROM");

// test.cc
ABSL_FLAG(std::string, rom, "", "Path to ROM");  // ERROR: Duplicate definition
```

**Fix 1: Use `static` (internal linkage)**
```cpp
// test.cc
static ABSL_FLAG(std::string, rom, "", "Path to ROM");  // Now local to this file
```

**Fix 2: Use Anonymous Namespace**
```cpp
// test.cc
namespace {
  ABSL_FLAG(std::string, rom, "", "Path to ROM");
}  // Now has internal linkage
```

**Fix 3: Declare in Header, Define in One .cc**
```cpp
// flags.h
extern ABSL_FLAG(std::string, rom);

// flags.cc
ABSL_FLAG(std::string, rom, "", "Path to ROM");

// test.cc
// Use via flags.h declaration, don't redefine
```

### Problem: Duplicate Function Definitions

**Bad:**
```cpp
// util.cc
void ProcessData() { /* ... */ }

// util_test.cc
void ProcessData() { /* ... */ }  // ERROR: Already defined
```

**Fix 1: Make `inline`**
```cpp
// util.h
inline void ProcessData() { /* ... */ }

// util.cc and util_test.cc can include and use it
```

**Fix 2: Use `static`**
```cpp
// util.cc
static void ProcessData() { /* ... */ }  // Internal linkage
```

**Fix 3: Use Anonymous Namespace**
```cpp
// util.cc
namespace {
  void ProcessData() { /* ... */ }
}  // Internal linkage
```

### Problem: Class Static Member Initialization

**Bad:**
```cpp
// widget.h
class Widget {
  static int instance_count;  // Declaration only
};

// widget.cc
int Widget::instance_count = 0;

// widget_test.cc (accidentally includes impl)
int Widget::instance_count = 0;  // ERROR: Multiple definitions
```

**Fix: Define in Only One .cc**
```cpp
// widget.h
class Widget {
  static int instance_count;
};

// widget.cc (ONLY definition)
int Widget::instance_count = 0;

// widget_test.cc (only uses, doesn't redefine)
```

## Integration with CI/CD

### GitHub Actions Example

Add to `.github/workflows/ci.yml`:

```yaml
- name: Extract symbols
  if: success()
  run: |
    ./scripts/extract-symbols.sh build
    ./scripts/check-duplicate-symbols.sh

- name: Upload symbol report
  if: always()
  uses: actions/upload-artifact@v3
  with:
    name: symbol-database
    path: build/symbol_database.json
```

### Workflow:
1. **Build completes** (generates .o/.obj files)
2. **Extract symbols** runs immediately
3. **Check for conflicts** analyzes database
4. **Fail job** if duplicates found
5. **Upload report** for inspection

## Performance Notes

### Typical Build Timings

| Operation | Time | Notes |
|-----------|------|-------|
| Extract symbols (145 obj files) | ~2-3s | macOS/Linux with `nm` |
| Extract symbols (145 obj files) | ~5-7s | Windows with `dumpbin` |
| Check duplicates | <100ms | JSON parsing and analysis |
| Pre-commit hook (5 changed files) | ~1-2s | Only checks affected objects |

### Optimization Tips

1. **Run only affected files in pre-commit hook** - Don't scan entire build
2. **Cache symbol database** - Reuse between checks if no new objects
3. **Parallel extraction** - Future enhancement for large builds
4. **Filter by symbol type** - Focus on data/text symbols, skip weak symbols

## Troubleshooting

### "Symbol database not found"

**Issue:** Script says database doesn't exist
```
Error: Symbol database not found: build/symbol_database.json
```

**Solution:** Generate it first
```bash
./scripts/extract-symbols.sh
```

### "No object files found"

**Issue:** Extraction found 0 object files
```
Warning: No object files found in build
```

**Solution:** Rebuild the project first
```bash
cmake --build build  # or appropriate build command
./scripts/extract-symbols.sh
```

### "No compiled objects found for changed files"

**Issue:** Pre-commit hook can't find object files for changes
```
[Pre-Commit] No compiled objects found for changed files (might not be built yet)
```

**Solution:** This is normal if you haven't built yet. Just commit normally:
```bash
git commit -m "Your message"
```

### Symbol not appearing in conflicts

**Issue:** Manual review found duplicate, but tool doesn't report it

**Cause:** Symbol might be weak, or in template/header-only code

**Solution:** Check with `nm` directly:
```bash
nm build/CMakeFiles/*/*.o | grep symbol_name
```

## Future Enhancements

1. **Incremental checking** - Only re-scan changed object files
2. **HTML reports** - Generate visual conflict reports with source references
3. **Automatic fixes** - Suggest patches for common ODR patterns
4. **Integration with IDE** - Clangd/LSP warnings for duplicate definitions
5. **Symbol lifecycle tracking** - Track which symbols were added/removed per build
6. **Statistics dashboard** - Monitor symbol health over time

## References

- [C++ One Definition Rule (cppreference)](https://en.cppreference.com/w/cpp/language/definition)
- [Linker Errors (isocpp.org)](https://isocpp.org/wiki/faq/linker-errors)
- [GNU nm Manual](https://sourceware.org/binutils/docs/binutils/nm.html)
- [Windows dumpbin Documentation](https://learn.microsoft.com/en-us/cpp/build/reference/dumpbin-reference)

## Support

For issues or suggestions:
1. Check `.githooks/pre-commit` is executable: `chmod +x .githooks/pre-commit`
2. Verify git hooks path is configured: `git config core.hooksPath`
3. Run full analysis for detailed debugging: `./scripts/check-duplicate-symbols.sh --verbose`
4. Open an issue with the `symbol-detection` label
