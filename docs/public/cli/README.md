# z3ed CLI Reference

The `z3ed` command-line interface provides tools for ROM inspection, validation, and automation.

## Command Categories

### Doctor Suite
Diagnostic and repair tools for ROM data integrity.

- **[Doctor Commands](doctor-commands.md)** - `overworld-doctor`, `dungeon-doctor`, `rom-doctor`, `rom-compare`

### Test Infrastructure
Machine-readable test discovery and execution.

- **[Test Commands](test-commands.md)** - `test-list`, `test-run`, `test-status`

### Inspection Tools
ROM data inspection and extraction (agent-focused).

| Command | Description |
|---------|-------------|
| `hex-read` | Read raw bytes from ROM |
| `hex-search` | Search for byte patterns |
| `palette-get-colors` | Extract palette data |
| `sprite-list` | List sprites |
| `music-list` | List music tracks |
| `dialogue-list` | List dialogue entries |

### Overworld Tools
Overworld-specific inspection and validation.

| Command | Description |
|---------|-------------|
| `overworld-find-tile` | Find tile usage |
| `overworld-describe-map` | Describe map properties |
| `overworld-list-warps` | List warp points |
| `overworld-list-sprites` | List overworld sprites |

### Dungeon Tools
Dungeon-specific inspection.

| Command | Description |
|---------|-------------|
| `dungeon-list-sprites` | List dungeon sprites |
| `dungeon-describe-room` | Describe room properties |
| `dungeon-list-objects` | List room objects |

## Common Flags

All commands support:

| Flag | Description |
|------|-------------|
| `--rom <path>` | Path to ROM file |
| `--format json\|text` | Output format (default varies by command) |
| `--verbose` | Show detailed output |
| `--help` | Show command help |

## Getting Help

```bash
# List all commands
z3ed help

# Get help for specific command
z3ed help <command>

# Example
z3ed help rom-doctor
```

## For AI Agents

Commands emit structured JSON when using `--format json`:

```bash
# Machine-readable output
z3ed rom-doctor --rom zelda3.sfc --format json

# Parse with jq
z3ed rom-doctor --rom zelda3.sfc --format json | jq '.checksum_valid'
```

Commands that don't require ROM (e.g., `test-list`, `test-status`) can be called without `--rom`.

