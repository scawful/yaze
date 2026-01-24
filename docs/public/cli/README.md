# z3ed CLI Reference

The `z3ed` command-line tool provides ROM inspection, validation, AI-assisted editing, and automation capabilities.

---

## Command Categories

### Doctor Suite (Diagnostics)

Validate and repair ROM data integrity.

- [Doctor Commands](doctor-commands.md) - `rom-doctor`, `dungeon-doctor`, `overworld-doctor`, `rom-compare`

### Test Infrastructure

Machine-readable test discovery and execution.

- [Test Commands](test-commands.md) - `test-list`, `test-run`, `test-status`

### Inspection Tools

| Command | Description |
|---------|-------------|
| `rom-read` | Read raw bytes from ROM |
| `rom-write` | Write raw bytes to ROM |
| `hex-read` | Read raw bytes from ROM |
| `hex-search` | Search for byte patterns |
| `palette-get-colors` | Extract palette data |
| `sprite-list` | List sprites |
| `music-list` | List music tracks |
| `dialogue-list` | List dialogue entries |

### Mesen2 Live Debugging

Requires Mesen2-OoS running with the socket API available under `/tmp/mesen2-*.sock`.
Prefer `z3ed debug ...` as the front door (Mesen2 is the default backend).

| Command | Description |
|---------|-------------|
| `mesen-gamestate` | Read live ALTTP game state from Mesen2 |
| `mesen-sprites` | List active sprites |
| `mesen-cpu` | Read CPU register state |
| `mesen-memory-read` | Read emulator memory |
| `mesen-memory-write` | Write emulator memory |
| `mesen-disasm` | Disassemble code at an address |
| `mesen-trace` | Fetch execution trace |
| `mesen-breakpoint` | Manage breakpoints |
| `mesen-control` | Pause/resume/step/frame/reset |
| `debug state` | Alias for `mesen-gamestate` |
| `debug mem read` | Alias for `mesen-memory-read` |
| `debug mem write` | Alias for `mesen-memory-write` |

### Overworld Tools

| Command | Description |
|---------|-------------|
| `overworld-find-tile` | Find tile usage across maps |
| `overworld-describe-map` | Describe map properties |
| `overworld-list-warps` | List warp points |
| `overworld-list-sprites` | List overworld sprites |

### Dungeon Tools

| Command | Description |
|---------|-------------|
| `dungeon-list-sprites` | List dungeon sprites |
| `dungeon-describe-room` | Describe room properties |
| `dungeon-list-objects` | List room objects |

---

## Common Flags

| Flag | Description |
|------|-------------|
| `--rom <path>` | Path to ROM file |
| `--sandbox` | Run ROM commands against a sandbox copy |
| `--format json\|text` | Output format |
| `--data-format hex\|ascii\|both` | Byte view for read commands |
| `--verbose` | Detailed output |
| `--ai_provider <name>` | AI provider for agent commands (ollama, gemini, openai, anthropic, mock) |
| `--ai_model <name>` | Provider-specific model override |
| `--openai_base_url <url>` | OpenAI-compatible base URL (LM Studio: `http://localhost:1234`) |
| `--mesen-socket <path>` | Override Mesen2 socket path |
| `--help` | Show command help |

Environment variables for AI providers:
`GEMINI_API_KEY`, `ANTHROPIC_API_KEY`, `OPENAI_API_KEY`,
`OPENAI_BASE_URL`/`OPENAI_API_BASE`, `OLLAMA_HOST`, `OLLAMA_MODEL`.

---

## Examples

```bash
# List all commands
z3ed help

# Get help for a command
z3ed help rom-doctor

# JSON output for scripting
z3ed rom-doctor --rom zelda3.sfc --format json

# Parse with jq
z3ed rom-doctor --rom zelda3.sfc --format json | jq '.checksum_valid'
```

---

## Related Documentation

- [z3ed CLI Guide](../usage/z3ed-cli.md) - Usage tutorials and workflows
- [API Reference](../reference/api.md) - HTTP + gRPC automation endpoints
- [Getting Started](../overview/getting-started.md) - Quick start guide
