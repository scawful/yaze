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
| `hex-read` | Read raw bytes from ROM |
| `hex-search` | Search for byte patterns |
| `palette-get-colors` | Extract palette data |
| `sprite-list` | List sprites |
| `music-list` | List music tracks |
| `dialogue-list` | List dialogue entries |

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
| `--format json\|text` | Output format |
| `--verbose` | Detailed output |
| `--ai_provider <name>` | AI provider for agent commands (ollama, gemini, openai, anthropic, mock) |
| `--ai_model <name>` | Provider-specific model override |
| `--help` | Show command help |

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
- [Getting Started](../overview/getting-started.md) - Quick start guide
