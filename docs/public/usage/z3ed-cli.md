# z3ed CLI Guide

_Last reviewed: November 2025. `z3ed` ships alongside the main editor in every `*-ai` preset and
runs on Windows, macOS, and Linux._

`z3ed` exposes the same ROM-editing capabilities as the GUI but in a scriptable form. Use it to
apply patches, inspect resources, run batch conversions, or drive the AI-assisted workflows that
feed the in-editor proposals.

## 1. Building & Configuration

```bash
# Enable the agent/CLI toolchain
cmake --preset mac-ai
cmake --build --preset mac-ai --target z3ed

# Run the text UI (FTXUI)
./build/bin/z3ed --tui
```

The AI features require at least one provider:
- **Ollama (local)** – install via `brew install ollama`, run `ollama serve`, then set
  `OLLAMA_MODEL=qwen2.5-coder:0.5b` (the lightweight default used in CI) or any other supported
  model. Pass `--ai_model "$OLLAMA_MODEL"` on the CLI to override per-run.
- **Gemini (cloud)** – export `GEMINI_API_KEY` before launching `z3ed`.

If no provider is configured the CLI still works, but agent subcommands will fall back to manual
plans.

## 2. Everyday Commands

| Task | Example |
| --- | --- |
| Apply an Asar patch | `z3ed asar patch.asm --rom zelda3.sfc` |
| Export all sprites from a dungeon | `z3ed dungeon list-sprites --dungeon 2 --rom zelda3.sfc --format json` |
| Inspect an overworld map | `z3ed overworld describe-map --map 80 --rom zelda3.sfc` |
| Dump palette data | `z3ed palette export --rom zelda3.sfc --output palettes.json` |
| Validate ROM headers | `z3ed rom info --rom zelda3.sfc` |

Pass `--help` after any command to see its flags. Most resource commands follow the
`<noun> <verb>` convention (`overworld set-tile`, `dungeon import-room`, etc.).

## 3. Agent & Proposal Workflow

### 3.1 Interactive Chat
```bash
z3ed agent chat --rom zelda3.sfc --theme overworld
```
- Maintains conversation history on disk so you can pause/resume.
- Supports tool-calling: the agent invokes subcommands (e.g., `overworld describe-map`) and
  returns structured diffs.

### 3.2 Plans & Batches
```bash
# Generate a proposal but do not apply it
z3ed agent plan --prompt "Move the eastern palace entrance 3 tiles east" --rom zelda3.sfc

# List pending plans
z3ed agent list

# Apply a plan after review
z3ed agent accept --proposal-id <id> --rom zelda3.sfc
```
Plans store the command transcript, diffs, and metadata inside
`$XDG_DATA_HOME/yaze/proposals/` (or `%APPDATA%\yaze\proposals\`). Review them before applying to
non-sandbox ROMs.

### 3.3 Non-interactive Scripts
```bash
# Run prompts from a file
z3ed agent simple-chat --file scripts/queries.txt --rom zelda3.sfc --stdout

# Feed stdin (useful in CI)
cat <<'PROMPTS' | z3ed agent simple-chat --rom zelda3.sfc --stdout
Describe tile 0x3A in map 0x80.
Suggest palette swaps for dungeon 2.
PROMPTS
```

## 4. Automation Tips

1. **Sandbox first** – point the agent at a copy of your ROM (`--sandbox` flag) so you can review
   patches safely.
2. **Log everything** – `--log-file agent.log` captures the provider transcript for auditing.
3. **Structure output** – most list/describe commands support `--format json` or `--format yaml`
   for downstream tooling.
4. **Combine with `yaze_test`** – run `./build_ai/bin/yaze_test --unit` after batch patches to
   confirm nothing regressed.
5. **Use TUI filters** – in `--tui`, press `:` to open the command palette, type part of a command,
   hit Enter, and the tool auto-fills the available flags.

## 5. Troubleshooting

| Symptom | Fix |
| --- | --- |
| `agent chat` hangs after a prompt | Ensure `ollama serve` or the Gemini API key is configured. |
| `libgrpc` or `absl` missing | Re-run the `*-ai` preset; plain debug presets do not pull the agent stack. |
| CLI cannot find the ROM | Use absolute paths or set `YAZE_DEFAULT_ROM=/path/to/zelda3.sfc`. |
| Tool reports "command not found" | Run `z3ed --help` to refresh the command index; stale binaries from older builds lack new verbs. |
| Proposal diffs are empty | Provide `--rom` plus either `--sandbox` or `--workspace` so the agent knows where to stage files. |

## 6. Related Documentation
- `docs/public/developer/testing-without-roms.md` – ROM-less fixtures for CI.
- `docs/public/developer/debugging-guide.md` – logging and instrumentation tips shared between the
  GUI and CLI.
- `docs/internal/agents/` – deep dives into the agent architecture and refactor plans (internal
  audience only).
