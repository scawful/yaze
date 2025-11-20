# Agent Helper Scripts

| Script | Description |
|--------|-------------|
| `run-gh-workflow.sh` | Wrapper for `gh workflow run`, prints the run URL for easy tracking. |
| `smoke-build.sh` | Runs `cmake --preset` configure/build in place and reports timing. |
| `run-tests.sh` | Configures the preset (if needed), builds `yaze_test`, and runs `ctest` with optional args. |
| `test-http-api.sh` | Polls the HTTP API `/api/v1/health` endpoint using curl (defaults to localhost:8080). |
| `windows-smoke-build.ps1` | PowerShell variant of the smoke build helper for Visual Studio/Ninja presets on Windows. |

Usage examples:
```bash
# Trigger CI workflow with artifacts and HTTP API tests enabled
scripts/agents/run-gh-workflow.sh ci.yml --ref develop upload_artifacts=true enable_http_api_tests=true

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
