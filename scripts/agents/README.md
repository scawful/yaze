# Agent Helper Scripts

| Script | Description |
|--------|-------------|
| `run-gh-workflow.sh` | Wrapper for `gh workflow run`, prints the run URL for easy tracking. |
| `get-gh-workflow-status.sh` | Checks the status of a GitHub Actions workflow run using `gh run view`. |
| `smoke-build.sh` | Runs `cmake --preset` configure/build in place and reports timing. |
| `run-tests.sh` | Configures the preset (if needed), builds `yaze_test`, and runs `ctest` with optional args. |
| `test-http-api.sh` | Polls the HTTP API `/api/v1/health` endpoint using curl (defaults to localhost:8080). |
| `windows-smoke-build.ps1` | PowerShell variant of the smoke build helper for Visual Studio/Ninja presets on Windows. |

Usage examples:
```bash
# Trigger CI workflow with artifacts and HTTP API tests enabled
scripts/agents/run-gh-workflow.sh ci.yml --ref develop upload_artifacts=true enable_http_api_tests=true

# Get the status of a workflow run (using either a URL or just the ID)
scripts/agents/get-gh-workflow-status.sh https://github.com/scawful/yaze/actions/runs/19529930066
scripts/agents/get-gh-workflow-status.sh 19529930066

# Smoke build mac-ai preset
scripts/agents/smoke-build.sh mac-ai

# Build & run tests for mac-dbg preset with verbose ctest output
scripts/agents/run-tests.sh mac-dbg --output-on-failure

# Check HTTP API health (defaults to localhost:8080)
scripts/agents/test-http-api.sh

# Windows smoke build using PowerShell
pwsh -File scripts/agents/windows-smoke-build.ps1 -Preset win-ai -Target z3ed
```

When invoking these scripts, log the results on the coordination board so other agents know which
workflows/builds were triggered and where to find artifacts/logs.

## Reducing Build Times

Local builds can take 10-15+ minutes from scratch. Follow these practices to minimize rebuild time:

### Use a Consistent Build Directory
Defaults now use `build/` for native builds. If you need isolation, set `YAZE_BUILD_DIR` or add a `CMakeUserPresets.json` locally:
```bash
cmake --preset mac-dbg
cmake --build build -j8 --target yaze
```

### Incremental Builds
Once configured, only rebuild—don't reconfigure unless CMakeLists.txt changed:
```bash
# GOOD: Just rebuild (fast, only recompiles changed files)
cmake --build build -j8 --target yaze

# AVOID: Reconfiguring when unnecessary (triggers full dependency resolution)
cmake --preset mac-dbg && cmake --build build
```

### Build Specific Targets
Don't build everything when you only need to verify a specific component:
```bash
# Build only the main editor (skips CLI, tests, etc.)
cmake --build build -j8 --target yaze

# Build only the CLI tool
cmake --build build -j8 --target z3ed

# Build only tests
cmake --build build -j8 --target yaze_test
```

### Parallel Compilation
Always use `-j8` or higher based on CPU cores:
```bash
cmake --build build -j$(sysctl -n hw.ncpu)  # macOS
cmake --build build -j$(nproc)              # Linux
```

### Quick Syntax Check
For rapid iteration on compile errors, build just the affected library:
```bash
# If fixing errors in src/app/editor/dungeon/, build just the editor lib
cmake --build build -j8 --target yaze_editor
```

### Verifying Changes Before CI
Before pushing to trigger CI builds (which take 15-20 minutes each):
1. Run an incremental local build to catch obvious errors
2. If you modified a specific component, build just that target
3. Only push when local build succeeds

### ccache/sccache (Advanced)
If available, these tools cache compilation results across rebuilds:
```bash
# Check if ccache is installed
which ccache

# View cache statistics
ccache -s
```

The project's CMake configuration automatically uses ccache when available.
