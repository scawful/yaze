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
cmake --build build_ai --preset mac-ai

# Step 3: Build specific targets (faster for incremental work)
cmake --build build_ai --target yaze                  # GUI application only
cmake --build build_ai --target yaze_test            # Test suite only
cmake --build build_ai --target ylib                 # Core library only
```

**Available macOS Presets**:
- `mac-ai` - **Preferred for Agents**. Configured to use system gRPC/protobuf if available (brew installed) and defaults to `build_ai`.
- `mac-dbg` - User's debug build (DO NOT USE).

### Linux

```bash
# Step 1: Configure
cmake --preset lin-ai

# Step 2: Build
cmake --build build_ai --preset lin-ai
```

**Available Linux Presets**:
- `lin-ai` - **Preferred for Agents**. Uses `build_ai` and system libraries.

### Windows

```bash
# Step 1: Configure (PowerShell or CMD)
cmake --preset win-ai

# Step 2: Build
cmake --build build_ai --preset win-ai
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
./build/bin/yaze_test --unit

# Integration tests (requires ROM file)
./build/bin/yaze_test --integration --rom-path /path/to/zelda3.sfc

# End-to-end GUI tests
./build/bin/yaze_test --e2e --show-gui

# Run specific test by name pattern
./build/bin/yaze_test "*Asar*"          # All tests with "Asar" in name
./build/bin/yaze_test "*Dungeon*"       # All dungeon-related tests
```

### Test Output Modes

```bash
# Minimal output (default)
./build/bin/yaze_test

# Verbose output (shows all test names)
./build/bin/yaze_test -v

# Very verbose (shows detailed test execution)
./build/bin/yaze_test -vv

# List all available tests without running
./build/bin/yaze_test --list-tests
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
cmake --preset mac-dbg      # ✅ CORRECT (macOS)
cmake --preset lin-dbg      # ✅ CORRECT (Linux)
cmake --preset win-dbg      # ✅ CORRECT (Windows)
```

### Issue 2: "Build directory exists but is outdated"

**Error**: CMake complains about existing build directory

**Solution**: Clean and reconfigure

```bash
# Remove old build directory
rm -rf build

# Reconfigure from scratch
cmake --preset mac-dbg  # or lin-dbg / win-dbg
```

### Issue 3: "Tests fail with 'ROM not found'"

**Error**: Integration tests fail with ROM-related errors

**Solution**: Some tests require a Zelda3 ROM file

```bash
# Skip ROM-dependent tests
./build/bin/yaze_test --unit

# Or provide ROM path
./build/bin/yaze_test --integration --rom-path zelda3.sfc
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
cmake --build build --preset mac-dbg -v | tee build.log

# Check build log for specific step taking long
grep "Linking" build.log
```

### Issue 5: Incremental builds seem slow

**Solution**: Only rebuild what changed

```bash
# Instead of rebuilding everything:
cmake --build build --preset mac-dbg              # ❌ Rebuilds all targets

# Build only what you need:
cmake --build build --target yaze                 # ✅ Just the GUI app
cmake --build build --target ylib                 # ✅ Just the core library
cmake --build build --target object_editor_card   # ✅ Just one component
```

## Development Workflow

### Typical Development Session

```bash
# 1. Configure once (first time only)
cmake --preset mac-dbg

# 2. Make code changes to src/app/editor/dungeon/object_editor_card.cc

# 3. Rebuild only the affected target (fast!)
cmake --build build --target yaze

# 4. Run the application to test manually
./build/bin/yaze --rom_file zelda3.sfc --editor Dungeon

# 5. Run automated tests to verify
./build/bin/yaze_test --unit

# 6. If tests pass, commit
git add src/app/editor/dungeon/object_editor_card.cc
git commit -m "feat(dungeon): add feature X"
```

### Testing Dungeon Editor Changes

```bash
# 1. Build just the GUI app (includes dungeon editor)
cmake --build build --target yaze

# 2. Launch directly to dungeon editor with ROM
./build/bin/yaze --rom_file zelda3.sfc --editor Dungeon

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
./build/bin/yaze_test --unit

# 2. Run format check (ensure code style)
cmake --build build --target format-check

# 3. If format check fails, auto-format
cmake --build build --target format

# 4. Build in release mode to catch optimization warnings
cmake --preset mac-rel
cmake --build build --preset mac-rel

# 5. If all passes, you're ready to commit!
```

## Preset Comparison Matrix

| Preset     | Platform | Build Type | AI Features | gRPC | Agent UI | Use Case |
|------------|----------|------------|-------------|------|----------|----------|
| mac-dbg    | macOS    | Debug      | No          | No   | No       | Daily development |
| mac-rel    | macOS    | Release    | No          | No   | No       | Performance testing |
| mac-ai     | macOS    | Debug      | Yes         | Yes  | Yes      | z3ed development |
| lin-dbg    | Linux    | Debug      | No          | No   | No       | Daily development |
| lin-rel    | Linux    | Release    | No          | No   | No       | Performance testing |
| lin-ai     | Linux    | Debug      | Yes         | Yes  | Yes      | z3ed development |
| win-dbg    | Windows  | Debug      | No          | No   | No       | Daily development |
| win-rel    | Windows  | Release    | No          | No   | No       | Performance testing |
| win-ai     | Windows  | Debug      | Yes         | Yes  | Yes      | z3ed development |

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
cmake --preset mac-dbg                  # Configure for macOS debug

# === Building ===
cmake --build build --target yaze       # Build GUI app
cmake --build build --target yaze_test  # Build test suite
cmake --build build --target format     # Format all code
cmake --build build -v                  # Verbose build output

# === Testing ===
./build/bin/yaze_test                   # Run all tests
./build/bin/yaze_test --unit            # Unit tests only
./build/bin/yaze_test "*Asar*"          # Specific test pattern
./build/bin/yaze_test --list-tests      # List available tests

# === Running ===
./build/bin/yaze                                    # Launch GUI
./build/bin/yaze --rom_file zelda3.sfc              # Load ROM
./build/bin/yaze --editor Dungeon                   # Open editor
./build/bin/yaze --rom_file zelda3.sfc --editor Dungeon  # Combined

# === Automation ===
./build/bin/yaze --headless                         # Run without GUI window
./build/bin/yaze --server --rom_file zelda3.sfc      # Headless + API + gRPC
./build/bin/yaze --export_symbols out.mlb            # Export Mesen labels

# For advanced Mesen2 automation (Headless/CI), see the Golden Path:
# [Mesen2 Architecture Ref](../oracle-of-secrets/Docs/Tooling/Mesen2_Architecture.md)

# === Cleaning ===
cmake --build build --target clean      # Clean build artifacts
rm -rf build                            # Full clean (reconfigure needed)
```

## Key Reminders

- Use full preset names: `mac-ai` not just `ai`
- First builds: 10-20 min (normal), incremental: 10-60 sec
- Build specific targets: `--target yaze` faster than full build
- Some tests require ROM file to pass
- **Automation**: Use `--server` for background agents and API access.

**DEPRECATION NOTICE**: The `yaze-mcp` tool is deprecated. Use the direct CLI flags (`--server`) and the gRPC/Socket API for automation.
