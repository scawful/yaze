# Z3ED Command Reference

Complete command reference for the z3ed CLI tool, including all automation and AI-powered features.

## Table of Contents

1. [ROM Operations](#rom-operations)
2. [Editor Automation](#editor-automation)
3. [Testing Commands](#testing-commands)
4. [Build & CI/CD](#build--cicd)
5. [Query & Discovery](#query--discovery)
6. [Interactive REPL](#interactive-repl)
7. [Network & Collaboration](#network--collaboration)
8. [AI Integration](#ai-integration)

## ROM Operations

### `z3ed rom read`
Read bytes from ROM at specified address.

**Syntax:**
```bash
z3ed rom read --address <hex> [--length <bytes>] [--format hex|ascii|binary]
```

**Examples:**
```bash
# Read 16 bytes from address 0x1000
z3ed rom read --address 0x1000 --length 16

# Read 256 bytes and display as ASCII
z3ed rom read --address 0x20000 --length 256 --format ascii

# Read single byte
z3ed rom read --address 0xFFFF
```

**Output:**
```json
{
  "address": "0x001000",
  "data": "A9 00 85 2C A9 01 85 2D A9 02 85 2E A9 03 85 2F",
  "ascii": ".........,......",
  "length": 16
}
```

### `z3ed rom write`
Write bytes to ROM at specified address.

**Syntax:**
```bash
z3ed rom write --address <hex> --data <hex_string> [--verify]
```

**Examples:**
```bash
# Write 4 bytes
z3ed rom write --address 0x1000 --data "A9 00 85 2C"

# Write with verification
z3ed rom write --address 0x2000 --data "FF FE FD FC" --verify

# Write ASCII string (converted to hex)
z3ed rom write --address 0x3000 --data "YAZE" --format ascii
```

### `z3ed rom validate`
Validate ROM integrity and structure.

**Syntax:**
```bash
z3ed rom validate [--checksums] [--headers] [--regions] [--fix]
```

**Examples:**
```bash
# Full validation
z3ed rom validate

# Validate checksums only
z3ed rom validate --checksums

# Validate and attempt fixes
z3ed rom validate --fix
```

**Output:**
```json
{
  "valid": true,
  "checksums": {
    "header": "OK",
    "complement": "OK"
  },
  "headers": {
    "title": "THE LEGEND OF ZELDA",
    "version": "1.0",
    "region": "USA"
  },
  "issues": [],
  "warnings": ["Expanded ROM detected"]
}
```

### `z3ed rom snapshot`
Create a named snapshot of current ROM state.

**Syntax:**
```bash
z3ed rom snapshot --name <name> [--compress] [--metadata <json>]
```

**Examples:**
```bash
# Create snapshot
z3ed rom snapshot --name "before_dungeon_edit"

# Compressed snapshot with metadata
z3ed rom snapshot --name "v1.0_release" --compress --metadata '{"version": "1.0", "author": "user"}'

# List snapshots
z3ed rom list-snapshots
```

### `z3ed rom restore`
Restore ROM from a previous snapshot.

**Syntax:**
```bash
z3ed rom restore --snapshot <name> [--verify]
```

**Examples:**
```bash
# Restore snapshot
z3ed rom restore --snapshot "before_dungeon_edit"

# Restore with verification
z3ed rom restore --snapshot "last_stable" --verify
```

## Editor Automation

### `z3ed editor dungeon`
Automate dungeon editor operations.

**Subcommands:**

#### `place-object`
Place an object in a dungeon room.

```bash
z3ed editor dungeon place-object --room <id> --type <object_id> --x <x> --y <y> [--layer <0|1|2>]

# Example: Place a chest at position (10, 15) in room 0
z3ed editor dungeon place-object --room 0 --type 0x22 --x 10 --y 15
```

#### `set-property`
Modify room properties.

```bash
z3ed editor dungeon set-property --room <id> --property <name> --value <value>

# Example: Set room darkness
z3ed editor dungeon set-property --room 5 --property "dark" --value true
```

#### `list-objects`
List all objects in a room.

```bash
z3ed editor dungeon list-objects --room <id> [--filter-type <type>]

# Example: List all chests in room 10
z3ed editor dungeon list-objects --room 10 --filter-type 0x22
```

#### `validate-room`
Check room for issues.

```bash
z3ed editor dungeon validate-room --room <id> [--fix-issues]

# Example: Validate and fix room 0
z3ed editor dungeon validate-room --room 0 --fix-issues
```

### `z3ed editor overworld`
Automate overworld editor operations.

**Subcommands:**

#### `set-tile`
Modify a tile on the overworld map.

```bash
z3ed editor overworld set-tile --map <id> --x <x> --y <y> --tile <tile_id>

# Example: Set grass tile at position (100, 50) on Light World
z3ed editor overworld set-tile --map 0x00 --x 100 --y 50 --tile 0x002
```

#### `place-entrance`
Add an entrance to the overworld.

```bash
z3ed editor overworld place-entrance --map <id> --x <x> --y <y> --target <room_id> [--type <type>]

# Example: Place dungeon entrance
z3ed editor overworld place-entrance --map 0x00 --x 200 --y 150 --target 0x10 --type "dungeon"
```

#### `modify-sprite`
Modify sprite properties.

```bash
z3ed editor overworld modify-sprite --map <id> --sprite-index <idx> --property <prop> --value <val>

# Example: Change enemy sprite type
z3ed editor overworld modify-sprite --map 0x00 --sprite-index 0 --property "type" --value 0x08
```

### `z3ed editor batch`
Execute multiple editor operations from a script file.

**Syntax:**
```bash
z3ed editor batch --script <file> [--dry-run] [--parallel] [--continue-on-error]
```

**Script Format (JSON):**
```json
{
  "operations": [
    {
      "editor": "dungeon",
      "action": "place-object",
      "params": {
        "room": 1,
        "type": 34,
        "x": 10,
        "y": 15
      }
    },
    {
      "editor": "overworld",
      "action": "set-tile",
      "params": {
        "map": 0,
        "x": 20,
        "y": 30,
        "tile": 322
      }
    }
  ],
  "options": {
    "stop_on_error": false,
    "validate_after": true
  }
}
```

## Testing Commands

### `z3ed test run`
Execute test suites.

**Syntax:**
```bash
z3ed test run [--category <unit|integration|e2e>] [--filter <pattern>] [--parallel]
```

**Examples:**
```bash
# Run all tests
z3ed test run

# Run unit tests only
z3ed test run --category unit

# Run specific test pattern
z3ed test run --filter "*Overworld*"

# Parallel execution
z3ed test run --parallel
```

### `z3ed test generate`
Generate test code for a class or component.

**Syntax:**
```bash
z3ed test generate --target <class|file> --output <file> [--framework <gtest|catch2>] [--include-mocks]
```

**Examples:**
```bash
# Generate tests for a class
z3ed test generate --target OverworldEditor --output overworld_test.cc

# Generate with mocks
z3ed test generate --target DungeonRoom --output dungeon_test.cc --include-mocks

# Generate integration tests
z3ed test generate --target src/app/rom.cc --output rom_integration_test.cc --framework catch2
```

### `z3ed test record`
Record interactions for test generation.

**Syntax:**
```bash
z3ed test record --name <test_name> --start
z3ed test record --stop [--save-as <file>]
z3ed test record --pause
z3ed test record --resume
```

**Examples:**
```bash
# Start recording
z3ed test record --name "dungeon_edit_test" --start

# Perform actions...
z3ed editor dungeon place-object --room 0 --type 0x22 --x 10 --y 10

# Stop and save
z3ed test record --stop --save-as dungeon_test.json

# Generate test from recording
z3ed test generate --from-recording dungeon_test.json --output dungeon_test.cc
```

### `z3ed test baseline`
Manage test baselines for regression testing.

**Syntax:**
```bash
z3ed test baseline --create --name <baseline>
z3ed test baseline --compare --name <baseline> [--threshold <percent>]
z3ed test baseline --update --name <baseline>
```

**Examples:**
```bash
# Create baseline
z3ed test baseline --create --name "v1.0_stable"

# Compare against baseline
z3ed test baseline --compare --name "v1.0_stable" --threshold 95

# Update baseline
z3ed test baseline --update --name "v1.0_stable"
```

## Build & CI/CD

### `z3ed build`
Build the project with specified configuration.

**Syntax:**
```bash
z3ed build --preset <preset> [--verbose] [--parallel <jobs>]
```

**Examples:**
```bash
# Debug build
z3ed build --preset lin-dbg

# Release build with 8 parallel jobs
z3ed build --preset lin-rel --parallel 8

# Verbose output
z3ed build --preset mac-dbg --verbose
```

### `z3ed ci status`
Check CI/CD pipeline status.

**Syntax:**
```bash
z3ed ci status [--workflow <name>] [--branch <branch>]
```

**Examples:**
```bash
# Check all workflows
z3ed ci status

# Check specific workflow
z3ed ci status --workflow "Build and Test"

# Check branch status
z3ed ci status --branch develop
```

## Query & Discovery

### `z3ed query rom-info`
Get comprehensive ROM information.

**Syntax:**
```bash
z3ed query rom-info [--detailed] [--format <json|yaml|text>]
```

**Output:**
```json
{
  "title": "THE LEGEND OF ZELDA",
  "size": 2097152,
  "expanded": true,
  "version": "1.0",
  "region": "USA",
  "checksum": "0xABCD",
  "header": {
    "mapper": "LoROM",
    "rom_speed": "FastROM",
    "rom_type": "ROM+SRAM+Battery"
  }
}
```

### `z3ed query available-commands`
Discover available commands and their usage.

**Syntax:**
```bash
z3ed query available-commands [--category <category>] [--format tree|list|json]
```

**Examples:**
```bash
# List all commands as tree
z3ed query available-commands --format tree

# List editor commands
z3ed query available-commands --category editor

# JSON output for programmatic use
z3ed query available-commands --format json
```

### `z3ed query find-tiles`
Search for tile patterns in ROM.

**Syntax:**
```bash
z3ed query find-tiles --pattern <hex> [--context <bytes>] [--limit <count>]
```

**Examples:**
```bash
# Find specific tile pattern
z3ed query find-tiles --pattern "FF 00 FF 00"

# Find with context
z3ed query find-tiles --pattern "A9 00" --context 8

# Limit results
z3ed query find-tiles --pattern "00 00" --limit 10
```

## Interactive REPL

### Starting REPL Mode

```bash
# Start REPL with ROM
z3ed repl --rom zelda3.sfc

# Start with history file
z3ed repl --rom zelda3.sfc --history ~/.z3ed_history
```

### REPL Features

#### Variable Assignment
```
z3ed> $info = rom-info
z3ed> echo $info.title
THE LEGEND OF ZELDA
```

#### Command Pipelines
```
z3ed> rom read --address 0x1000 --length 100 | find --pattern "A9"
z3ed> query find-tiles --pattern "FF" | head -10
```

#### Session Management
```
z3ed> session save my_session
z3ed> session load my_session
z3ed> history show
z3ed> history replay 5-10
```

## Network & Collaboration

### `z3ed network connect`
Connect to collaborative editing session.

**Syntax:**
```bash
z3ed network connect --host <host> --port <port> [--username <name>]
```

**Examples:**
```bash
# Connect to session
z3ed network connect --host localhost --port 8080 --username "agent1"

# Join specific room
z3ed network join --room "dungeon_editing" --password "secret"
```

### `z3ed network sync`
Synchronize changes with other collaborators.

**Syntax:**
```bash
z3ed network sync [--push] [--pull] [--merge-strategy <strategy>]
```

**Examples:**
```bash
# Push local changes
z3ed network sync --push

# Pull remote changes
z3ed network sync --pull

# Bidirectional sync with merge
z3ed network sync --merge-strategy last-write-wins
```

## AI Integration

### `z3ed ai chat`
Interactive AI assistant for ROM hacking.

**Syntax:**
```bash
z3ed ai chat [--model <model>] [--context <file>]
```

**Examples:**
```bash
# Start chat with default model
z3ed ai chat

# Use specific model
z3ed ai chat --model gemini-pro

# Provide context file
z3ed ai chat --context room_layout.json
```

### `z3ed ai suggest`
Get AI suggestions for ROM modifications.

**Syntax:**
```bash
z3ed ai suggest --task <task> [--model <model>] [--constraints <json>]
```

**Examples:**
```bash
# Suggest dungeon layout improvements
z3ed ai suggest --task "improve dungeon flow" --constraints '{"rooms": [1,2,3]}'

# Suggest sprite placements
z3ed ai suggest --task "balance enemy placement" --model ollama:llama2
```

### `z3ed ai analyze`
Analyze ROM for patterns and issues.

**Syntax:**
```bash
z3ed ai analyze --type <pattern|bug|optimization> [--report <file>]
```

**Examples:**
```bash
# Analyze for bugs
z3ed ai analyze --type bug --report bugs.json

# Find optimization opportunities
z3ed ai analyze --type optimization

# Pattern analysis
z3ed ai analyze --type pattern --report patterns.md
```

## Global Options

These options work with all commands:

- `--rom <file>` - Specify ROM file to use
- `--verbose` - Enable verbose output
- `--quiet` - Suppress non-error output
- `--format <json|yaml|text>` - Output format
- `--output <file>` - Write output to file
- `--no-color` - Disable colored output
- `--config <file>` - Use configuration file
- `--log-level <level>` - Set logging level
- `--help` - Show help for command

## Exit Codes

- `0` - Success
- `1` - General error
- `2` - Invalid arguments
- `3` - ROM not loaded
- `4` - Operation failed
- `5` - Network error
- `6` - Build error
- `7` - Test failure
- `127` - Command not found

## Configuration

Z3ed can be configured via `~/.z3edrc` or `z3ed.config.json`:

```json
{
  "default_rom": "~/roms/zelda3.sfc",
  "ai_model": "gemini-pro",
  "output_format": "json",
  "log_level": "info",
  "network": {
    "default_host": "localhost",
    "default_port": 8080
  },
  "build": {
    "default_preset": "lin-dbg",
    "parallel_jobs": 8
  },
  "test": {
    "framework": "gtest",
    "coverage": true
  }
}
```

## Environment Variables

- `Z3ED_ROM_PATH` - Default ROM file path
- `Z3ED_CONFIG` - Configuration file location
- `Z3ED_AI_MODEL` - Default AI model
- `Z3ED_LOG_LEVEL` - Logging verbosity
- `Z3ED_HISTORY_FILE` - REPL history file location
- `Z3ED_CACHE_DIR` - Cache directory for snapshots
- `Z3ED_API_KEY` - API key for AI services
