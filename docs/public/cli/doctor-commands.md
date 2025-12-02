# z3ed Doctor Commands

The doctor command suite provides diagnostic and repair tools for ROM data integrity. All commands support structured JSON output for automation.

## Available Doctor Commands

| Command | Description |
|---------|-------------|
| `overworld-doctor` | Diagnose/repair overworld data (tile16, pointers, ZSCustom features) |
| `overworld-validate` | Validate map32 pointers and decompression |
| `dungeon-doctor` | Diagnose dungeon room data (objects, sprites, chests) |
| `rom-doctor` | Validate ROM file integrity (header, checksums, expansions) |
| `rom-compare` | Compare two ROMs for differences |

## Common Flags

All doctor commands support:
- `--rom <path>` - Path to ROM file (required)
- `--format json|text` - Output format (default: text)
- `--verbose` - Show detailed output

## overworld-doctor

Diagnose and repair overworld data corruption.

```bash
# Basic diagnosis
z3ed overworld-doctor --rom zelda3.sfc

# Compare against vanilla baseline
z3ed overworld-doctor --rom zelda3.sfc --baseline vanilla.sfc

# Apply fixes with dry-run preview
z3ed overworld-doctor --rom zelda3.sfc --fix --output fixed.sfc --dry-run

# JSON output for agents
z3ed overworld-doctor --rom zelda3.sfc --format json
```

### Detects
- ZSCustomOverworld version (Vanilla, v2, v3)
- Expanded tile16/tile32 regions
- Expanded pointer tables (tail map support)
- Tile16 corruption at known problem addresses
- Map pointer validity for all 160+ maps

## dungeon-doctor

Diagnose dungeon room data integrity.

```bash
# Sample key rooms (fast)
z3ed dungeon-doctor --rom zelda3.sfc

# Analyze all 296 rooms
z3ed dungeon-doctor --rom zelda3.sfc --all

# Analyze specific room
z3ed dungeon-doctor --rom zelda3.sfc --room 0x10

# JSON output
z3ed dungeon-doctor --rom zelda3.sfc --format json --verbose
```

### Validates
- Room header pointers
- Object counts (max 400 before lag)
- Sprite counts (max 64 per room)
- Chest counts (max 6 per room for item flags)
- Object bounds (0-63 for x/y coordinates)

### Sample Output (Text)
```
╔═══════════════════════════════════════════════════════════════╗
║                    DUNGEON DOCTOR                             ║
╠═══════════════════════════════════════════════════════════════╣
║  Rooms Analyzed: 19                                           ║
║  Valid Rooms: 19                                              ║
║  Rooms with Warnings: 0                                       ║
║  Rooms with Errors: 0                                         ║
╠═══════════════════════════════════════════════════════════════╣
║  Total Objects: 890                                           ║
║  Total Sprites: 98                                            ║
╚═══════════════════════════════════════════════════════════════╝
```

## rom-doctor

Validate ROM file integrity and expansion status.

```bash
# Basic validation
z3ed rom-doctor --rom zelda3.sfc

# Verbose with all findings
z3ed rom-doctor --rom zelda3.sfc --verbose

# JSON output for CI/automation
z3ed rom-doctor --rom zelda3.sfc --format json
```

### Validates
- SNES header (title, map mode, country)
- Checksum verification (complement XOR checksum = 0xFFFF)
- ROM size (vanilla 1MB vs expanded 2MB)
- ZSCustomOverworld version detection
- Expansion flags (tile16, tile32, pointer tables)
- Free space analysis in expansion region

### Sample Output (Text)
```
╔═══════════════════════════════════════════════════════════════╗
║                      ROM DOCTOR                               ║
╠═══════════════════════════════════════════════════════════════╣
║  ROM Title: THE LEGEND OF ZELDA                               ║
║  Size: 0x200000 bytes (2048 KB)                               ║
║  Map Mode: LoROM                                              ║
║  Country: USA                                                 ║
╠═══════════════════════════════════════════════════════════════╣
║  Checksum: 0xAF0D (complement: 0x50F2) - VALID                ║
║  ZSCustomOverworld: Vanilla                                   ║
║  Expanded Tile16: NO                                          ║
║  Expanded Tile32: NO                                          ║
║  Expanded Ptr Tables: NO                                      ║
╚═══════════════════════════════════════════════════════════════╝
```

## rom-compare

Compare two ROMs to identify differences.

```bash
# Basic comparison
z3ed rom-compare --rom my_rom.sfc --baseline vanilla.sfc

# Show detailed byte differences
z3ed rom-compare --rom my_rom.sfc --baseline vanilla.sfc --show-diff

# JSON output
z3ed rom-compare --rom my_rom.sfc --baseline vanilla.sfc --format json
```

## Diagnostic Schema

All doctor commands produce findings with consistent structure:

```json
{
  "findings": [
    {
      "id": "tile16_corruption",
      "severity": "error",
      "message": "Corrupted tile16 at 0x1E878B",
      "location": "0x1E878B",
      "suggested_action": "Run with --fix to zero corrupted entries",
      "fixable": true
    }
  ],
  "summary": {
    "total_findings": 1,
    "critical": 0,
    "errors": 1,
    "warnings": 0,
    "info": 0,
    "fixable": 1
  }
}
```

### Severity Levels
- `info` - Informational, no action needed
- `warning` - Potential issue, may need attention  
- `error` - Problem detected, should be fixed
- `critical` - Severe issue, requires immediate attention

## Agent Usage

For AI agents consuming doctor output:

```bash
# Get structured JSON for parsing
z3ed rom-doctor --rom zelda3.sfc --format json

# Chain with jq for specific fields
z3ed rom-doctor --rom zelda3.sfc --format json | jq '.checksum_valid'

# Check exit code for pass/fail
z3ed rom-doctor --rom zelda3.sfc --format json && echo "ROM OK"
```

