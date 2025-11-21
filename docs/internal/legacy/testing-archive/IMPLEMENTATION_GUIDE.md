# Symbol Conflict Detection - Implementation Guide

This guide explains the implementation details of the Symbol Conflict Detection System and how to integrate it into your development workflow.

## Architecture Overview

### System Components

```
┌─────────────────────────────────────────────────────────┐
│     Compiled Object Files (.o / .obj)                   │
│     (Created during cmake --build)                       │
└──────────────────┬──────────────────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────────────────┐
│  extract-symbols.sh                                     │
│  ├─ Scan object files in build/                         │
│  ├─ Use nm (Unix/macOS) or dumpbin (Windows)            │
│  ├─ Extract symbol definitions (skip undefined refs)    │
│  └─ Generate JSON database                              │
└──────────────────┬──────────────────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────────────────┐
│  symbol_database.json                                   │
│  ├─ Metadata (platform, timestamp, stats)               │
│  ├─ Conflicts array (symbols defined multiple times)    │
│  └─ Symbols dict (full mapping)                         │
└──────────────────┬──────────────────────────────────────┘
                   │
      ┌────────────┼────────────┐
      │            │            │
      ▼            ▼            ▼
   ┌──────────────────────────────────┐
   │ check-duplicate-symbols.sh       │
   │ └─ Parse JSON & report conflicts │
   └──────────────────────────────────┘
      │            │            │
      │            │            │
   [CLI]    [Pre-Commit]      [CI/CD]
```

## Script Implementation Details

### 1. extract-symbols.sh

**Purpose:** Extract all symbol definitions from object files

**Key Functions:**

#### Symbol Extraction (Unix/macOS)
```bash
nm -P <obj_file>  # Parse format: SYMBOL TYPE [VALUE] [SIZE]
```

Format:
- Column 1: Symbol name
- Column 2: Symbol type (T=text, D=data, R=read-only, etc.)
- Column 3: Address (if defined)
- Column 4: Size

Filtering logic:
1. Skip symbols with name starting with space
2. Skip symbols with "U" in the type column (undefined)
3. Keep symbols with types: T, D, R, B, C, etc.

#### Symbol Extraction (Windows)
```bash
dumpbin /symbols <obj_file>  # Parse binary format output
```

Note: Windows extraction is less precise than Unix. Symbol types are approximated.

#### JSON Generation
Uses Python3 for portability:
1. Read all extracted symbols from temp file
2. Group by symbol name
3. Identify conflicts (count > 1)
4. Generate structured JSON
5. Sort conflicts by count (most duplicated first)

**Performance Considerations:**
- Process all 4000+ object files sequentially
- `nm` is fast (~1ms per file on macOS)
- Python JSON generation is <100ms
- Total: ~2-3 seconds for typical builds

### 2. check-duplicate-symbols.sh

**Purpose:** Analyze symbol database and report conflicts

**Algorithm:**
1. Parse JSON database
2. Extract metadata and conflicts array
3. For each conflict:
   - Print symbol name
   - List all definitions with object files and types
4. Exit with code based on conflict count

**Output Formatting:**
- Colors for readability (RED for errors, GREEN for success)
- Structured output with proper indentation
- Fix suggestions (if --fix-suggestions flag)

### 3. Pre-commit Hook (`.githooks/pre-commit`)

**Purpose:** Fast symbol check on changed files (not full extraction)

**Algorithm:**
1. Get staged changes: `git diff --cached`
2. Filter to .cc/.h files
3. Find matching object files in build directory
4. Use `nm` to extract symbols from affected objects only
5. Check for duplicates using `sort | uniq -d`

**Key Optimizations:**
- Only processes changed files, not entire build
- Quick `sort | uniq -d` instead of full JSON parsing
- Can be bypassed with `--no-verify`
- Runs in <2 seconds

**Matching Logic:**
```
source file: src/cli/flags.cc
object file: build/CMakeFiles/*/src/cli/flags.cc.o
```

### 4. test-symbol-detection.sh

**Purpose:** Validate the entire system

**Test Sequence:**
1. Check scripts are executable (chmod +x)
2. Verify build directory exists
3. Count object files (need > 0)
4. Run extract-symbols.sh (timeout: 2 minutes)
5. Validate JSON structure (required fields)
6. Run check-duplicate-symbols.sh
7. Verify pre-commit hook configuration
8. Display sample output

**Exit Codes:**
- `0` = All tests passed
- `1` = Test failed (specific test prints which one)

## Integration Workflows

### Development Workflow

```
1. Make code changes
        │
        ▼
2. Build project: cmake --build build
        │
        ▼
3. Pre-commit hook runs automatically
        │
        ├─ Fast check on changed files
        ├─ Warns if conflicts detected
        └─ Allow commit with --no-verify if intentional
        │
        ▼
4. Run full check before pushing (optional):
   ./scripts/extract-symbols.sh && ./scripts/check-duplicate-symbols.sh
        │
        ▼
5. Push to GitHub
```

### CI/CD Workflow

```
GitHub Push/PR
        │
        ▼
.github/workflows/symbol-detection.yml
        │
        ├─ Checkout code
        ├─ Setup environment
        ├─ Build project
        ├─ Extract symbols
        ├─ Check for conflicts
        ├─ Upload artifact (symbol_database.json)
        └─ Fail job if conflicts found
```

### First-Time Setup

```bash
# 1. Configure git hooks (one-time)
git config core.hooksPath .githooks

# 2. Make hook executable
chmod +x .githooks/pre-commit

# 3. Test the system
./scripts/test-symbol-detection.sh

# 4. Create initial symbol database
./scripts/extract-symbols.sh && ./scripts/check-duplicate-symbols.sh
```

## JSON Database Schema

```json
{
  "metadata": {
    "platform": "Darwin|Linux|Windows",
    "build_dir": "/path/to/build",
    "timestamp": "ISO8601Z format",
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
    "FLAGS_rom": [
      { "object_file": "flags.cc.o", "type": "D" },
      { "object_file": "emu_test.cc.o", "type": "D" }
    ]
  }
}
```

### Schema Notes:
- `symbols` dict only includes conflicted symbols (keeps file size small)
- `conflicts` array is sorted by count (most duplicated first)
- `type` field indicates symbol kind (T/D/R/B/U/etc.)
- Timestamps are UTC ISO8601 for cross-platform compatibility

## Symbol Types Reference

| Type | Name | Meaning | Common in |
|------|------|---------|-----------|
| T | Text | Function/code | .cc/.o |
| D | Data | Initialized variable | .cc/.o |
| R | Read-only | Const data | .cc/.o |
| B | BSS | Uninitialized data | .cc/.o |
| C | Common | Tentative definition | .cc/.o |
| U | Undefined | External reference | (skipped) |
| A | Absolute | Absolute symbol | (rare) |
| W | Weak | Weak symbol | (rare) |

## Troubleshooting Guide

### Extraction Fails with "No object files found"

**Cause:** Build directory not populated with .o files

**Solution:**
```bash
cmake --build build  # First build
./scripts/extract-symbols.sh
```

### Extraction is Very Slow

**Cause:** 4000+ object files, or nm is slow on filesystem

**Solution:**
1. Ensure build is on fast SSD
2. Check system load: `top` or `Activity Monitor`
3. Run in foreground to see progress
4. Optional: Parallelize in future version

### Symbol Not Appearing as Conflict

**Cause:** Symbol is weak (W type) or hidden/internal

**Solution:**
Check directly with nm:
```bash
nm build/CMakeFiles/*/*.o | grep symbol_name
```

### Pre-commit Hook Not Running

**Cause:** Git hooks path not configured

**Solution:**
```bash
git config core.hooksPath .githooks
chmod +x .githooks/pre-commit
```

### Windows dumpbin Not Found

**Cause:** Visual Studio not properly installed

**Solution:**
```powershell
# Run from Visual Studio Developer Command Prompt
# or install Visual Studio with "Desktop development with C++"
```

## Performance Optimization Ideas

### Phase 1 (Current)
- Sequential symbol extraction
- Full JSON parsing
- Complete database generation

### Phase 2 (Future)
- Parallel object file processing (~4x speedup)
- Incremental extraction (only new/changed objects)
- Symbol caching (reuse between builds)

### Phase 3 (Future)
- HTML report generation with source links
- Integration with IDE (clangd warnings)
- Automatic fix suggestions with patch generation

## Maintenance

### When to Run Extract

| Scenario | Command |
|----------|---------|
| After major rebuild | `./scripts/extract-symbols.sh` |
| Before pushing | `./scripts/extract-symbols.sh && ./scripts/check-duplicate-symbols.sh` |
| In CI/CD | Automatic (symbol-detection.yml) |
| Quick check on changes | Pre-commit hook (automatic) |

### Cleanup

```bash
# Remove symbol database
rm build/symbol_database.json

# Clean temp files (if stuck)
rm -f build/.temp_symbols.txt build/.object_files.tmp
```

### Updating for New Platforms

To add support for a new platform:

1. Detect platform in `extract-symbols.sh`:
```bash
case "${UNAME_S}" in
    NewOS*) IS_NEWOS=true ;;
esac
```

2. Add extraction function:
```bash
extract_symbols_newos() {
    local obj_file="$1"
    # Use platform-specific tool (e.g., readelf for new Unix variant)
}
```

3. Call appropriate function in main loop

## References

- **nm manual:** `man nm` or online docs
- **dumpbin:** Visual Studio documentation
- **Symbol types:** ELF specification (gabi10000.pdf)
- **ODR violations:** C++ standard section 3.2
