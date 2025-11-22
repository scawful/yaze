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
- Build target: `cmake --build --preset <preset> --target yaze_test`
- Run all tests: `./build/bin/yaze_test`
- Filtered runs:
  - `./build/bin/yaze_test --unit`
  - `./build/bin/yaze_test --integration`
  - `./build/bin/yaze_test --e2e --show-gui`
  - `./build/bin/yaze_test --rom-dependent --rom-path path/to/zelda3.sfc`
- Preset-based ctest: `ctest --preset dev`

Environment variables:
- `YAZE_TEST_ROM_PATH` – default ROM for ROM-dependent tests.
- `YAZE_SKIP_ROM_TESTS`, `YAZE_ENABLE_UI_TESTS` – gate expensive suites.

## 6. Troubleshooting & References
- Detailed troubleshooting: `docs/public/build/troubleshooting.md`
- Platform compatibility: `docs/public/build/platform-compatibility.md`
- Internal agents must follow coordination protocol in
  `docs/internal/agents/coordination-board.md` before running builds/tests.
