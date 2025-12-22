# Gemini Pro 3 Build Setup Guide

Quick iteration setup for maximizing Gemini's effectiveness on YAZE.

## One-Time Setup

```bash
# Use the dedicated Gemini build script (recommended)
./scripts/gemini_build.sh

# Or manually configure with the Gemini preset
cmake --preset mac-gemini

# Verify setup succeeded
ls build_gemini/compile_commands.json
```

## Fast Rebuild Cycle (~30s-2min)

```bash
# Recommended: Use the build script
./scripts/gemini_build.sh              # Build yaze (default)
./scripts/gemini_build.sh yaze_test    # Build tests
./scripts/gemini_build.sh --fresh      # Clean reconfigure

# Or use cmake directly
cmake --build build_gemini -j8 --target yaze

# Rebuild editor library (for src/app/editor/ changes)
cmake --build build_gemini -j8 --target yaze_editor

# Rebuild zelda3 library (for src/zelda3/ changes)
cmake --build build_gemini -j8 --target yaze_lib
```

## Quick Validation (~2-3min)

```bash
# Run stable tests (unit + integration, no ROM required)
ctest --test-dir build_gemini -L stable -j4 --output-on-failure

# Run GUI smoke tests only (~1min)
ctest --test-dir build_gemini -L gui --output-on-failure

# Run specific test by name
ctest --test-dir build_gemini -R "OverworldRegression" --output-on-failure
```

## Full Validation (when confident)

```bash
# All tests
ctest --test-dir build_gemini --output-on-failure -j4
```

## Format Check (before committing)

```bash
# Check format without modifying
cmake --build build_gemini --target format-check

# Auto-format all code
cmake --build build_gemini --target format
```

## Key Directories

| Path | Purpose |
|------|---------|
| `src/app/editor/overworld/` | Overworld editor modules (8 files) |
| `src/zelda3/overworld/` | Overworld data models |
| `src/zelda3/dungeon/` | Dungeon data models |
| `src/app/editor/dungeon/` | Dungeon editor components |
| `test/unit/zelda3/` | Unit tests for zelda3 logic |
| `test/e2e/` | GUI E2E tests |

## Iteration Strategy

1. **Make changes** to target files
2. **Rebuild** with `cmake --build build_gemini -j8 --target yaze`
3. **Test** with `ctest --test-dir build_gemini -L stable -j4`
4. **Repeat** until all tests pass

## Common Issues

### Build fails with "target not found"
```bash
# Reconfigure (CMakeLists.txt changed)
./scripts/gemini_build.sh --fresh
# Or: cmake --preset mac-gemini --fresh
```

### Tests timeout
```bash
# Run with longer timeout
ctest --test-dir build_gemini -L stable --timeout 300
```

### Need to see test output
```bash
ctest --test-dir build_gemini -L stable -V  # Verbose
```
