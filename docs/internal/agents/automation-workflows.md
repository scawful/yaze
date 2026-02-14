# Headless & Server Automation Workflows

This document describes how AI agents can utilize the `headless`, `server`, and `symbol-export` features of `yaze` for autonomous workflows, such as CI/CD, automated testing, and remote debugging.

## 1. Headless Execution

Running `yaze` in headless mode is essential for environments without a display (CI/CD, remote servers, or background agents).

### **Workflow: Automated GUI Testing**
Agents can run GUI tests headlessly to verify UI logic without visual feedback.

```bash
# Start yaze headlessly with the gRPC test harness enabled
./build_ai/bin/yaze --headless --enable_test_harness --rom_file zelda3.sfc &

# Use the yaze_test suite to connect to the running instance
./build_ai/bin/yaze_test --gui-remote-port 50052 --suite integration_ui
```

## 2. Server Mode

Server mode is the preferred way to run `yaze` for long-running autonomous tasks. It implies `--headless`, `--enable_api` (HTTP), and `--enable_test_harness` (gRPC).

### **Workflow: Remote Agent Management**
An agent can start `yaze` as a background worker and interact with it via APIs.

```bash
# Start as a server
./build_ai/bin/yaze --server --rom_file zelda3.sfc &

# Interact via HTTP API (e.g., list models, health check)
curl http://localhost:8080/api/v1/health

# Interact via gRPC (e.g., step the emulator, read memory)
# (Use your internal gRPC tools to connect to port 50052)
```

## 3. Symbol Synchronization (Mesen 2)

For advanced disassembly and debugging workflows, agents can sync `yaze` symbols with external tools like Mesen 2.

### **Workflow: External Debugging**
1. **Export Symbols:**
   ```bash
   ./build_ai/bin/yaze --headless --rom_file zelda3.sfc --export_symbols project.mlb --symbol_format mesen
   ```
2. **Launch Mesen 2:**
   Place `project.mlb` next to your ROM and open Mesen 2. The labels will be synchronized.

## 4. Expert Chain (External Analysis)

For Oracle-of-Secrets debugging/triage, you can chain local expert models via LM Studio:

```bash
~/src/tools/expert-chain --list-workflows
~/src/tools/expert-chain water-collision --dry-run
```

Notes:
- Requires LM Studio running on `localhost:1234` with a model loaded.
- Results saved under `~/.context/projects/oracle-of-secrets/scratchpad/`.

## 5. Resource Efficiency

In headless/server mode, the application loop is optimized:
- **No Rendering:** `DoRender()` is skipped.
- **CPU Yielding:** The main loop sleeps for 16ms per tick to maintain ~60Hz logic without spinning at 100% CPU.
- **Background Safety:** Screenshots can still be requested via gRPC and will be captured on the "headless" backbuffer.

## 6. Summary of Flags

| Flag | Purpose | Implicitly Enables |
| :--- | :--- | :--- |
| `--headless` | Disables GUI window and rendering. | - |
| `--server` | Configures app for background service. | `--headless`, `--enable_api`, `--enable_test_harness` |
| `--asset_mode` | Asset loading: `auto`, `full`, or `lazy` (auto → lazy for headless/server). | - |
| `--export_symbols` | One-shot export of symbol table to file. | - |
| `--symbol_format` | Format for export (`mesen`, `asar`, `wla`, `bsnes`). | - |

## 7. Ninja Build Metadata Recovery

If incremental builds start printing `ninja: warning: premature end of file; recovering`, use the helper below to rotate Ninja state files and re-stabilize the build graph:

```bash
scripts/agents/ninja-heal.sh --preset dev --build-dir build --parallel 8
```

Behavior:
- Backs up `build/.ninja_deps` and `build/.ninja_log` with timestamps.
- Runs a recovery build and an optional verification build.
- Fails with a non-zero exit code if the warning still appears during verification.

## 8. Gemini YOLO Loops (Oracle Workstream)

For Gemini-driven prompt automation (non-interactive with auto-approval), use the agent scripts:

```bash
# Single task section from prompt markdown
scripts/agents/gemini-yolo-loop.sh \
  --prompt-doc ~/.context/projects/oracle-of-secrets/scratchpad/gemini_prompts_2026-02-12.md \
  --section "Task 2: Object Dimension Validation" \
  --task-name dimensions --max-iterations 2 --timeout-seconds 120 --model gemini-2.5-pro \
  -- --extensions code-review --allowed-mcp-server-names nanobanana

# Full 4-task Oracle fanout (dialogue + dimensions + wrap + annotation)
scripts/agents/gemini-oracle-workstream.sh \
  --model gemini-2.5-pro --max-iterations 2 --timeout-seconds 120 \
  -- --extensions code-review --allowed-mcp-server-names nanobanana
```

Behavior:
- Runs `gemini` with `--approval-mode yolo` and `--yolo` by default.
- Retries up to `--max-iterations` until the configured completion marker appears.
- Applies per-iteration timeout via `--timeout-seconds` so stuck generations fail fast (exit 124).
- Writes per-iteration prompts/logs and a run summary for auditability.
