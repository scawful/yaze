# Z3ED Command Reference

Complete command reference for the z3ed CLI tool, including all automation and AI-powered features.

## Table of Contents

1. [ROM Operations](#rom-operations)
2. [Editing Commands](#editing-commands)
3. [Testing Commands](#testing-commands)
4. [Resource & Search Commands](#resource--search-commands)
5. [Interactive TUI](#interactive-tui)
6. [Agent CLI](#agent-cli)
7. [Mesen2 Live Debugging](#mesen2-live-debugging)

## ROM Operations

### `z3ed rom read`
Read bytes from ROM at specified address.

**Syntax:**
```bash
z3ed rom read --address <hex> [--length <bytes>] [--data-format <hex|ascii|both>] [--format json|text]
```

**Notes:**
- `--data-format` controls the hex/ascii view of bytes.
- `--format` controls the output structure (json/text).

**Examples:**
```bash
# Read 16 bytes from address 0x1000
z3ed rom read --address 0x1000 --length 16

# Read 256 bytes and display as ASCII
z3ed rom read --address 0x20000 --length 256 --data-format ascii

# Read single byte
z3ed rom read --address 0xFFFF
```

**Output:**
```json
{
  "address": "0x001000",
  "data": "A9 00 85 2C A9 01 85 2D A9 02 85 2E A9 03 85 2F",
  "ascii": ".........,......",
  "length": 16,
  "format": "both"
}
```

### `z3ed rom write`
Write bytes to ROM at specified address.

**Syntax:**
```bash
z3ed rom write --address <hex> --data <hex_string> [--sandbox]
```

**Notes:**
- `--sandbox` writes to a sandbox copy instead of the original ROM.

**Examples:**
```bash
# Write 4 bytes
z3ed rom write --address 0x1000 --data "A9 00 85 2C"

# Write to a sandboxed ROM copy (safe default for edits)
z3ed rom write --address 0x2000 --data "FF FE FD FC" --sandbox
```

### `z3ed rom info`
Show basic ROM metadata.

**Syntax:**
```bash
z3ed rom info --rom <file>
```

**Examples:**
```bash
z3ed rom info --rom zelda3.sfc
```

### `z3ed rom diff`
Compare two ROM files.

**Syntax:**
```bash
z3ed rom diff --rom_a <file> --rom_b <file>
```

**Examples:**
```bash
z3ed rom diff --rom_a base.sfc --rom_b mod.sfc
```

### `z3ed rom compare`
Compare a ROM against a baseline to identify differences.

**Syntax:**
```bash
z3ed rom compare --rom <file> --baseline <file>
```

**Examples:**
```bash
z3ed rom compare --rom mod.sfc --baseline vanilla.sfc
```

### `z3ed rom validate`
Validate ROM integrity and structure.

**Syntax:**
```bash
z3ed rom validate
```

**Examples:**
```bash
# Full validation
z3ed rom validate
```

**Output:**
```json
{
  "validation_passed": true,
  "results": "checksum: PASSED; header: PASSED"
}
```

### `z3ed rom generate-golden`
Generate a golden ROM file for testing.

**Syntax:**
```bash
z3ed rom-generate-golden --rom_file <file> --golden_file <file>
```

**Examples:**
```bash
z3ed rom-generate-golden --rom_file zelda3.sfc --golden_file golden.sfc
```

## Editing Commands

These commands operate directly on ROM data (no GUI required).

### Dungeon Commands
- `dungeon-describe-room --room <hex>`
- `dungeon-list-sprites --room <hex>`
- `dungeon-list-objects --room <hex>`
- `dungeon-list-chests --room <hex>`
- `dungeon-get-entrance --entrance <hex> [--spawn]`
- `dungeon-export-room --room <hex> --output <file>`
- `dungeon-get-room-tiles --room <hex>` *(stubbed)*
- `dungeon-set-room-property --room <hex> --property <name> --value <value>` *(stubbed)*

Example:
```bash
z3ed dungeon-describe-room --room=0x05 --rom=zelda3.sfc
```

### Overworld Commands
- `overworld-describe-map --map <hex>`
- `overworld-find-tile --tile <hex>`
- `overworld-list-warps --map <hex>`
- `overworld-list-sprites --map <hex>`
- `overworld-list-items --map <hex>`
- `overworld-get-entrance --entrance <hex>`
- `overworld-tile-stats --map <hex>`

Example:
```bash
z3ed overworld-describe-map --map=0x40 --rom=zelda3.sfc
```

### GUI Automation (requires GUI gRPC server)
- `gui-place-tile --tile <hex> --x <x> --y <y>`
- `gui-click --target <path> [--click-type <left|right|double>]`
- `gui-discover-tool [--window <name>] [--type <type>]`
- `gui-summarize-widgets`
- `gui-screenshot [--region <region>]`

## Testing Commands

### `z3ed test-list`
List available tests.

### `z3ed test-run`
Execute tests.

**Syntax:**
```bash
z3ed test-run [--label <label>] [--format json|text]
```

### `z3ed test-status`
Check test execution status.

## Resource & Search Commands

- `resource-list --type <type>`
- `resource-search --query <text> [--type <type>]`
- `message-list`, `message-read`, `message-search --query <text>`
- `message-encode --text <text>`, `message-decode --hex <hex_bytes>`
- `message-export-org --output <path>`, `message-import-org --file <path>`
- `message-export-bundle --output <path> [--range <all|vanilla|expanded>]`
- `message-import-bundle --file <path> [--apply] [--strict] [--range <all|vanilla|expanded>]`
- `message-write --id <id> --text <text>`
- `message-export-bin --output <path> [--range expanded]`
- `message-export-asm --output <path> [--range expanded]`
- `dialogue-list`, `dialogue-read`, `dialogue-search --query <text>`
- `hex-search --pattern <hex>`

## Interactive TUI

Use the built-in TUI:
```bash
z3ed --tui
```
The TUI includes a command palette and live status panels for ROM work.

## Agent CLI

Agent commands are routed through the `agent` subcommand:
```bash
z3ed agent simple-chat --rom=zelda3.sfc
z3ed agent plan --rom=zelda3.sfc
z3ed agent run --rom=zelda3.sfc
z3ed agent diff --rom=zelda3.sfc
```

Use `z3ed agent` with no args for a full list of agent subcommands.

## Mesen2 Live Debugging

These commands connect to a running **Mesen2-OoS** instance via the local socket
(`/tmp/mesen2-<pid>.sock`). They do **not** require a ROM argument. The
`debug` alias is the recommended front door (Mesen2 is the default backend).

### `z3ed mesen-gamestate`
Read ALTTP game state from Mesen2.

**Syntax:**
```bash
z3ed mesen-gamestate [--format <json|text>]
```

**Examples:**
```bash
z3ed mesen-gamestate
z3ed debug state
```

### `z3ed mesen-sprites`
List active sprites from Mesen2.

**Syntax:**
```bash
z3ed mesen-sprites [--all] [--format <json|text>]
```

**Examples:**
```bash
z3ed mesen-sprites
z3ed mesen-sprites --all
```

### `z3ed mesen-cpu`
Read CPU register state.

**Syntax:**
```bash
z3ed mesen-cpu [--format <json|text>]
```

**Examples:**
```bash
z3ed mesen-cpu
```

### `z3ed mesen-memory-read`
Read emulator memory.

**Syntax:**
```bash
z3ed mesen-memory-read --address <hex> [--length <n>] [--format <json|text>]
```

**Examples:**
```bash
z3ed mesen-memory-read --address 0x7E0020 --length 16
```

### `z3ed mesen-memory-write`
Write emulator memory (hex bytes).

**Syntax:**
```bash
z3ed mesen-memory-write --address <hex> --data <hexbytes> [--format <json|text>]
```

**Examples:**
```bash
z3ed mesen-memory-write --address 0x7E0010 --data 01
```

### `z3ed mesen-disasm`
Disassemble code at an address.

**Syntax:**
```bash
z3ed mesen-disasm --address <hex> [--count <n>] [--format <json|text>]
```

**Examples:**
```bash
z3ed mesen-disasm --address 0x008000 --count 20
```

### `z3ed mesen-trace`
Fetch execution trace.

**Syntax:**
```bash
z3ed mesen-trace [--count <n>] [--format <json|text>]
```

**Examples:**
```bash
z3ed mesen-trace --count 50
```

### `z3ed mesen-breakpoint`
Manage breakpoints.

**Syntax:**
```bash
z3ed mesen-breakpoint --action <add|remove|clear|list> [--address <hex>] [--id <n>] [--type <exec|read|write|rw>]
```

**Examples:**
```bash
z3ed mesen-breakpoint --action add --address 0x008000
z3ed mesen-breakpoint --action clear
```

### `z3ed mesen-control`
Control emulation state.

**Syntax:**
```bash
z3ed mesen-control --action <pause|resume|step|frame|reset>
```

**Examples:**
```bash
z3ed mesen-control --action pause
```

## AI Integration

AI workflows are available via the `z3ed agent` commands (see **Agent CLI**).

## Global Options

These options work with all commands:

- `--rom <file>` - Specify ROM file to use
- `--sandbox` - Run ROM commands against a sandbox copy
- `--format <json|text>` - Output format
- `--data-format <hex|ascii|both>` - Byte view for read commands
- `--tui` - Launch the interactive TUI
- `--list-commands` - List all available commands
- `--export-schemas` - Export command schemas as JSON
- `--version` - Show version information
- `--self-test` - Run CLI self-test
- `--verbose` - Enable verbose output
- `--quiet` - Suppress non-error output
- `--ai_provider <name>` - AI provider (auto, ollama, gemini, openai, anthropic, mock)
- `--ai_model <name>` - Provider-specific model override
- `--gemini_api_key <key>` - Gemini API key
- `--anthropic_api_key <key>` - Anthropic API key
- `--ollama_host <url>` - Ollama host URL
- `--openai_base_url <url>` - OpenAI-compatible base URL (LM Studio: `http://localhost:1234`)
- `--gui_server_address <host:port>` - GUI automation server
- `--prompt_version <name>` - Prompt version override
- `--use_function_calling <true|false>` - Enable Gemini function calling
- `--mesen-socket <path>` - Override Mesen2 socket path
- `--help` - Show help for command
- `--http-port <port>` - HTTP API port (if enabled in build)
- `--http-host <host>` - HTTP API display host (printed URLs only)

Environment variable shortcuts for AI providers:
`GEMINI_API_KEY`, `ANTHROPIC_API_KEY`, `OPENAI_API_KEY`,
`OPENAI_BASE_URL`/`OPENAI_API_BASE`, `OLLAMA_HOST`, `OLLAMA_MODEL`.

## Exit Codes

- `0` - Success
- `1` - Error (invalid args, missing ROM, failed command)
