# GEMINI.md - YAZE Build Instructions

_Extends: ~/AGENTS.md, ~/GEMINI.md, file:///Users/scawful/src/config/zelda-dev/rules.md_

Build and test instructions for YAZE project. Follow commands exactly.

## Critical Rules

1. **Use `build_ai/` directory** - Never use `build/` (reserved for user)
2. **Use `*-ai` presets** - Never use `*-dbg` presets
3. **Load persona** - Check `.claude/agents/<agent-id>.md` for system prompt
4. **Use helper script:**
   ```bash
   # Recommended: Use mesen-agent for unified workflow
   mesen-agent build

   # Legacy: Use helper script
   ./scripts/agent_build.sh [target]
   ```
   *Example:* `./scripts/agent_build.sh yaze` or `./scripts/agent_build.sh yaze_test`

---

## Quick Reference: Build Times

**First Build (Cold Start)**:
- **Fast Mode (Recommended)**: 2-4 minutes (uses system gRPC/sccache)
- Standard Mode: 10-20 minutes (compiles gRPC from source)

**Incremental Builds (After Changes)**:
- Typically 10-60 seconds depending on what changed
- **sccache/ccache**: Automatically detected and used if installed (highly recommended)

## Platform-Specific Build Commands

### macOS

```bash
# Step 1: Configure (First time only, or when CMakeLists.txt changes)
cmake --preset mac-ai

# Step 2: Build the entire project
cmake --build build_ai --parallel 8

# Step 3: Build specific targets (faster for incremental work)
cmake --build build_ai --target yaze                  # GUI application only
cmake --build build_ai --target yaze_test            # Test suite only
cmake --build build_ai --target ylib                 # Core library only
```

**Available macOS Presets**:
- `mac-ai` - **Preferred for Agents**. Configured to use system gRPC/protobuf if available (brew installed) and defaults to `build_ai`.

### Linux

```bash
# Step 1: Configure
cmake --preset lin-ai

# Step 2: Build
cmake --build build_ai --parallel 8
```

**Available Linux Presets**:
- `lin-ai` - **Preferred for Agents**. Uses `build_ai` and system libraries.

### Windows

```bash
# Step 1: Configure (PowerShell or CMD)
cmake --preset win-ai

# Step 2: Build
cmake --build build_ai --parallel 8
```

**Available Windows Presets**:
- `win-ai` - **Preferred for Agents**. Uses `build_ai`.

## Testing

### Running All Tests

```bash
# Recommended: Use mesen-agent for unified testing
mesen-agent test

# Legacy: Build tests first
cmake --build build_ai --target yaze_test
```

### Running Specific Test Categories

```bash
# Unit tests only (fast, ~5-10 seconds)
./scripts/yaze_test --unit

# Integration tests (requires ROM file)
./scripts/yaze_test --integration --rom-path /path/to/zelda3.sfc

# End-to-end GUI tests
./scripts/yaze_test --e2e --show-gui

# Run specific test by name pattern
./scripts/yaze_test "*Asar*"          # All tests with "Asar" in name
./scripts/yaze_test "*Dungeon*"       # All dungeon-related tests
```

### Test Output Modes

```bash
# Minimal output (default)
./scripts/yaze_test

# Verbose output (shows all test names)
./scripts/yaze_test -v

# Very verbose (shows detailed test execution)
./scripts/yaze_test -vv

# List all available tests without running
./scripts/yaze_test --list-tests
```

## Common Build Issues and Solutions

### Issue 1: "No preset found"

**Error**: `CMake Error: No such preset in CMakePresets.json`

**Solution**: Check the exact preset name. Use tab-completion or check `CMakePresets.json`.

```bash
# List available presets
cmake --list-presets

# Common mistake: Using wrong platform prefix
cmake --preset dbg          # ❌ WRONG
cmake --preset mac-ai       # ✅ CORRECT (macOS)
cmake --preset lin-ai       # ✅ CORRECT (Linux)
cmake --preset win-ai       # ✅ CORRECT (Windows)
```

### Issue 2: "Build directory exists but is outdated"

**Error**: CMake complains about existing build directory

**Solution**: Clean and reconfigure

```bash
# Remove old build directory
rm -rf build_ai

# Reconfigure from scratch
cmake --preset mac-ai  # or lin-ai / win-ai
```

### Issue 3: "Tests fail with 'ROM not found'"

**Error**: Integration tests fail with ROM-related errors

**Solution**: Some tests require a Zelda3 ROM file

```bash
# Skip ROM-dependent tests
./scripts/yaze_test --unit

# Or provide ROM path
./scripts/yaze_test --integration --rom-path zelda3.sfc
```

### Issue 4: Long build times on first run

**Not an Error**: This is normal!

**Explanation**:
- CPM.cmake downloads all dependencies (~3-5 minutes)
- gRPC compilation (Windows only, ~15-20 minutes)
- ImGui compilation (~2-3 minutes)
- SDL2, Abseil, PNG libraries (~3-5 minutes)

**Solution**: Be patient on first build. Subsequent builds use ccache/sccache and are MUCH faster (10-60 seconds).

```bash
# Monitor build progress with verbose output
cmake --build build_ai -v | tee build.log

# Check build log for specific step taking long
grep "Linking" build.log
```

### Issue 5: Incremental builds seem slow

**Solution**: Only rebuild what changed

```bash
# Instead of rebuilding everything:
cmake --build build_ai                               # ❌ Rebuilds all targets

# Build only what you need:
cmake --build build_ai --target yaze                 # ✅ Just the GUI app
cmake --build build_ai --target ylib                 # ✅ Just the core library
cmake --build build_ai --target object_editor_card   # ✅ Just one component
```

## Development Workflow

### Typical Development Session

```bash
# 1. Configure once (first time only)
cmake --preset mac-ai

# 2. Make code changes to src/app/editor/dungeon/object_editor_card.cc

# 3. Rebuild only the affected target (fast!)
cmake --build build_ai --target yaze

# 4. Run the application to test manually
./scripts/yaze --rom_file zelda3.sfc --editor Dungeon

# 5. Run automated tests to verify
./scripts/yaze_test --unit

# 6. If tests pass, commit
git add src/app/editor/dungeon/object_editor_card.cc
git commit -m "feat(dungeon): add feature X"
```

### Testing Dungeon Editor Changes

```bash
# 1. Build just the GUI app (includes dungeon editor)
cmake --build build_ai --target yaze

# 2. Launch directly to dungeon editor with ROM
./scripts/yaze --rom_file zelda3.sfc --editor Dungeon

# 3. To test keyboard shortcuts specifically:
#    - Open Object Editor card
#    - Try Ctrl+A (select all)
#    - Try Delete key (delete selected)
#    - Try Ctrl+D (duplicate)
#    - Try Arrow keys (nudge objects)
#    - Try Tab (cycle selection)
```

### Before Committing Changes

```bash
# 1. Run unit tests (fast check)
./scripts/yaze_test --unit

# 2. Run format check (ensure code style)
cmake --build build_ai --target format-check

# 3. If format check fails, auto-format
cmake --build build_ai --target format

# 4. Build in release mode to catch optimization warnings
cmake --build build_ai --config Release --target yaze z3ed yaze_test

# 5. If all passes, you're ready to commit!
```

## Presets

This file is intentionally agent-centric; it assumes `*-ai` presets and the
`build_ai/` directory. For the full preset list (dbg/rel/dev/wasm/ios), see:
`docs/public/build/quick-reference.md`.

## CI/CD Build Times (For Reference)

GitHub Actions runners typically see these build times:

- **Ubuntu 22.04**: 6-8 minutes (with caching)
- **macOS 14**: 8-10 minutes (with caching)
- **Windows 2022**: 12-18 minutes (gRPC adds time)

Your local builds may be faster or slower depending on:
- CPU cores (more = faster parallel builds)
- SSD speed (affects linking time)
- Available RAM (swap = slower builds)
- ccache/sccache hit rate (warm cache = much faster)

## Target Dependencies Reference

Understanding what rebuilds when you change files:

```
yaze (GUI app)
├── ylib (core library)
│   ├── zelda3_dungeon (dungeon module)
│   │   └── object_editor_card.cc ← Your changes here
│   ├── zelda3_overworld
│   ├── gfx (graphics system)
│   └── core (compression, ROM I/O)
├── imgui (UI framework)
└── SDL2 (windowing/graphics)

yaze_test (test suite)
├── ylib (same as above)
├── gtest (Google Test framework)
└── test/*.cc files
```

**When you change**:
- `object_editor_card.cc` → Rebuilds: ylib, yaze (30-60 seconds)
- `object_editor_card.h` → Rebuilds: ylib, yaze, any test including header (1-2 minutes)
- `rom.cc` → Rebuilds: Most of ylib, yaze, yaze_test (3-5 minutes)
- `CMakeLists.txt` → Reconfigure + full rebuild (5-10 minutes)

## Quick Command Cheat Sheet

```bash
# === Configuration ===
cmake --list-presets                    # Show available presets
cmake --preset mac-ai                   # Configure (macOS)

# === Building ===
cmake --build build_ai --target yaze       # Build GUI app
cmake --build build_ai --target yaze_test  # Build test suite
cmake --build build_ai --target format     # Format all code
cmake --build build_ai -v                  # Verbose build output

# === Testing ===
./scripts/test_fast.sh --quick            # Fast, high-signal suites (ctest -L quick)
ctest --preset mac-ai-quick-unit          # Quick unit suite (macOS)
ctest --preset mac-ai-quick-integration   # Quick integration suite (macOS)
./scripts/yaze_test                   # Run all tests
./scripts/yaze_test --unit            # Unit tests only
./scripts/yaze_test "*Asar*"          # Specific test pattern
./scripts/yaze_test --list-tests      # List available tests

# === Running ===
./scripts/yaze                                    # Launch GUI
./scripts/yaze --rom_file zelda3.sfc              # Load ROM
./scripts/yaze --editor Dungeon                   # Open editor
./scripts/yaze --rom_file zelda3.sfc --editor Dungeon  # Combined

# === Automation ===
./scripts/yaze --headless                         # Run without GUI window
./scripts/yaze --server --rom_file zelda3.sfc      # Headless + API + gRPC
./scripts/yaze --export_symbols out.mlb            # Export Mesen labels

# For advanced Mesen2 automation (Headless/CI), see the Golden Path:
# [Mesen2 Architecture Ref](../oracle-of-secrets/Docs/Tooling/Mesen2_Architecture.md)

# === Cleaning ===
cmake --build build_ai --target clean      # Clean build artifacts
rm -rf build_ai                            # Full clean (reconfigure needed)
```

## Key Reminders

- Use full preset names: `mac-ai` not just `ai`
- First builds: 10-20 min (normal), incremental: 10-60 sec
- Build specific targets: `--target yaze` faster than full build
- Some tests require ROM file to pass
- **Automation**: Use `--server` for background agents and API access.

**DEPRECATION NOTICE**: The `yaze-mcp` tool is deprecated. Use the direct CLI flags (`--server`) and the gRPC/Socket API for automation.
