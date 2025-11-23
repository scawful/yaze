# Build & Test Quick Reference

Use this document as the single source of truth for configuring, building, and testing YAZE across
platforms. Other guides (README, CLAUDE.md, GEMINI.md, etc.) should link here instead of duplicating
steps.

## 1. Environment Prep
- Clone with submodules: `git clone --recursive https://github.com/scawful/yaze.git`
- Run the verifier once per machine:
  - macOS/Linux: `./scripts/verify-build-environment.sh --fix`
  - Windows PowerShell: `.\scripts\verify-build-environment.ps1 -FixIssues`

## 2. Build Presets
Use `cmake --preset <name>` followed by `cmake --build --preset <name> [--target …]`.

| Preset      | Platform(s) | Notes |
|-------------|-------------|-------|
| `mac-dbg`, `lin-dbg`, `win-dbg` | macOS/Linux/Windows | Standard debug builds, tests on by default. |
| `mac-ai`, `lin-ai`, `win-ai` | macOS/Linux/Windows | Enables gRPC, agent UI, `z3ed`, and AI runtime. |
| `mac-rel`, `lin-rel`, `win-rel` | macOS/Linux/Windows | Optimized release builds. |
| `mac-dev`, `lin-dev`, `win-dev` | macOS/Linux/Windows | Development builds with ROM-dependent tests enabled. |
| `mac-uni` | macOS | Universal binary (ARM64 + x86_64) for distribution. |
| `ci-*` presets | Platform-specific | Mirrors CI matrix; see `CMakePresets.json`. |

**Verbose builds**: add `-v` suffix (e.g., `mac-dbg-v`, `lin-dbg-v`, `win-dbg-v`) to turn off compiler warning suppression.

## 3. AI/Assistant Build Policy
- Human developers typically use `build` or `build_test` directories.
- AI assistants **must use dedicated directories** (`build_ai`, `build_agent`, etc.) to avoid
  clobbering user builds.
- When enabling AI features, prefer the `*-ai` presets and target only the binaries you need
  (`yaze`, `z3ed`, `yaze_test`, …).
- Windows helpers: use `scripts/agents/windows-smoke-build.ps1` for quick builds and `scripts/agents/run-tests.sh` (or its PowerShell equivalent) for test runs so preset + generator settings stay consistent.

## 4. Common Commands
```bash
# Debug GUI build (macOS)
cmake --preset mac-dbg
cmake --build --preset mac-dbg --target yaze

# Debug GUI build (Linux)
cmake --preset lin-dbg
cmake --build --preset lin-dbg --target yaze

# Debug GUI build (Windows)
cmake --preset win-dbg
cmake --build --preset win-dbg --target yaze

# AI-enabled build with gRPC (macOS)
cmake --preset mac-ai
cmake --build --preset mac-ai --target yaze z3ed

# AI-enabled build with gRPC (Linux)
cmake --preset lin-ai
cmake --build --preset lin-ai --target yaze z3ed

# AI-enabled build with gRPC (Windows)
cmake --preset win-ai
cmake --build --preset win-ai --target yaze z3ed
```

## 5. Testing

### Default Tests (Always Available)

Default test suites run automatically with debug/dev presets. Include stable unit/integration tests and GUI smoke tests:

```bash
# Build stable test suite (always included in debug presets)
cmake --build --preset mac-dbg --target yaze_test_stable

# Run with ctest (recommended approach)
ctest --preset mac-dbg -L stable          # Stable tests only
ctest --preset mac-dbg -L gui             # GUI smoke tests
ctest --test-dir build -L "stable|gui"    # Both stable + GUI
```

### Optional: ROM-Dependent Tests

For tests requiring Zelda3 ROM file (ASAR ROM tests, complete edit workflows, ZSCustomOverworld upgrades):

```bash
# Configure with ROM path
cmake --preset mac-dbg -DYAZE_ENABLE_ROM_TESTS=ON -DYAZE_TEST_ROM_PATH=~/zelda3.sfc

# Build ROM test suite
cmake --build --preset mac-dbg --target yaze_test_rom_dependent

# Run ROM tests
ctest --test-dir build -L rom_dependent
```

### Optional: Experimental AI Tests

For AI-powered feature tests (requires `YAZE_ENABLE_AI_RUNTIME=ON`):

```bash
# Use AI-enabled preset
cmake --preset mac-ai

# Build experimental test suite
cmake --build --preset mac-ai --target yaze_test_experimental

# Run AI tests
ctest --test-dir build -L experimental
```

### Test Commands Reference

```bash
# Stable tests only (recommended for quick iteration)
ctest --test-dir build -L stable -j4

# All enabled tests (respects preset configuration)
ctest --test-dir build --output-on-failure

# GUI smoke tests
ctest --test-dir build -L gui

# Headless GUI tests (CI mode)
ctest --test-dir build -L headless_gui

# Tests matching pattern
ctest --test-dir build -R "Dungeon"

# Verbose output
ctest --test-dir build --verbose
```

### Test Organization by Preset

| Preset | Stable | GUI | ROM-Dep | Experimental |
|--------|--------|-----|---------|--------------|
| `mac-dbg`, `lin-dbg`, `win-dbg` | Yes | Yes | No | No |
| `mac-ai`, `lin-ai`, `win-ai` | Yes | Yes | No | Yes |
| `mac-dev`, `lin-dev`, `win-dev` | Yes | Yes | Yes | No |
| `mac-rel`, `lin-rel`, `win-rel` | No | No | No | No |

### Environment Variables

- `YAZE_TEST_ROM_PATH` - Set ROM path for ROM-dependent tests (or use `-DYAZE_TEST_ROM_PATH=...` in CMake)
- `YAZE_SKIP_ROM_TESTS` - Skip ROM tests if set (useful for CI without ROM)
- `YAZE_ENABLE_UI_TESTS` - Enable GUI tests (default if display available)

## 6. Troubleshooting & References
- Detailed troubleshooting: `docs/public/build/troubleshooting.md`
- Platform compatibility: `docs/public/build/platform-compatibility.md`
- Internal agents must follow coordination protocol in
  `docs/internal/agents/coordination-board.md` before running builds/tests.
