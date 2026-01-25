# Agent Helper Scripts

| Script | Description |
|--------|-------------|
| `run-gh-workflow.sh` | Wrapper for `gh workflow run`, prints the run URL for easy tracking. |
| `get-gh-workflow-status.sh` | Checks the status of a GitHub Actions workflow run using `gh run view`. |
| `smoke-build.sh` | Runs `cmake --preset` configure/build in place and reports timing. |
| `run-tests.sh` | Configures the preset (if needed), builds `yaze_test`, and runs `ctest` with optional args. |
| `test-http-api.sh` | Smoke-checks HTTP API endpoints (health/models/symbols + core POSTs) via curl; defaults to localhost:8080. |
| `test-grpc-api.sh` | Smoke-checks gRPC automation API via grpcurl; defaults to localhost:50052 and the ImGui test harness Ping. |
| `ralph-loop-codex.sh` | Runs a Ralph Wiggum loop with Codex CLI using a mission prompt file. |
| `ralph-loop-report.sh` | Builds a Markdown report from Ralph loop logs for structured review. |
| `ralph-loop-lock.sh` | Acquire/release simple lock files to prevent multi-agent overlap. |
| `ralph-loop-status.sh` | Print key fields from the Ralph loop state file (delegates to the OOS helper). |
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

# Smoke-check HTTP API endpoints (defaults to localhost:8080)
scripts/agents/test-http-api.sh

# Smoke-check gRPC automation API (defaults to localhost:50052)
scripts/agents/test-grpc-api.sh

# Run the Codex Ralph loop with the yaze mission prompt
scripts/agents/ralph-loop-codex.sh --mission docs/internal/plans/ralph-wiggum-codex-mission.md \
  --max-iterations 15 --auto-extend --extend-by 20 --report-every 3 --report-limit 20 \
  --heartbeat-seconds 30 --watchdog-seconds 300 --watchdog-retries 1 \
  --completion-promise "YAZE_RALPH_DONE" -- --full-auto

# Disable colors/prefixing if desired (NO_COLOR also works)
scripts/agents/ralph-loop-codex.sh --mission docs/internal/plans/ralph-wiggum-codex-mission.md \
  --no-color --no-prefix-output -- --profile mac-ai --full-auto

# Generate a Markdown report from loop logs
scripts/agents/ralph-loop-report.sh --limit 25

# Print loop state summary (uses RALPH_OOS_ROOT for the consolidated helper)
scripts/agents/ralph-loop-status.sh

# Lock an area while working on it
scripts/agents/ralph-loop-lock.sh acquire --area ai-integration --owner codex
scripts/agents/ralph-loop-lock.sh status
scripts/agents/ralph-loop-lock.sh release --area ai-integration

# Windows smoke build using PowerShell
pwsh -File scripts/agents/windows-smoke-build.ps1 -Preset win-ai -Target z3ed
```

When invoking these scripts, log the results on the coordination board so other agents know which
workflows/builds were triggered and where to find artifacts/logs.

Ralph loop logs:
- `.claude/ralph-loop.codex/iteration-*.log` (full console stream per iteration)
- `.claude/ralph-loop.codex/iteration-*.last.txt` (last message per iteration)
- `.claude/ralph-loop.codex/index.md` (embedded last messages for search)
- `.claude/ralph-loop.codex/combined.log` (concatenated console stream)
- `.claude/ralph-loop.codex/iteration-*.repo.txt` (git status snapshot per iteration)
- `.claude/ralph-loop.codex/iteration-*.heartbeat` (heartbeat timestamps)
- `.claude/ralph-loop.codex/iteration-*.watchdog` (watchdog notes if triggered)

Ralph loop state:
- `.claude/ralph-loop.codex.md` (YAML front matter with active flag, iteration counters, timestamps)
- Includes per-iteration metadata like `last_preflight_status`, `last_sanity_status`, `last_exit_status`,
  `last_outcome_status`, `watchdog_triggered`, and resolved Mesen2 socket fields (`mesen_socket_resolved`,
  `mesen_socket_source`, `mesen_socket_resolved_at`).
- `RALPH_OOS_ROOT` overrides the Oracle-of-Secrets root used for default sanity/preflight/registry scripts.

Note: AFS CLI writes to `~/.context/projects/yaze` are blocked by the sandbox. Use repo `.context` paths for scratchpad updates.

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
