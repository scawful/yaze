# z3ed Doctor Commands

The doctor command suite provides diagnostic and repair tools for ROM data integrity. All commands support structured JSON output for automation.

## Available Doctor Commands

| Command | Description |
|---------|-------------|
| `overworld-doctor` | Diagnose/repair overworld data (tile16, pointers, ZSCustom features) |
| `overworld-validate` | Validate map32 pointers and decompression |
| `overworld-allocation` | Map area IDs $00-$BF and check TailMapExpansion.asm (read-only) |
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
- Tile16 layout (vanilla or expanded). The doctor does not judge tile16 entries: every 16-bit value is a legal tile word, so damage cannot be told apart from edits. Compare against a backup with `rom-compare` instead.
- Map pointer validity for all 160+ maps

## overworld-allocation

Read-only map of overworld area IDs `$00-$BF`, with an optional check of
what `assets/patches/Overworld/TailMapExpansion.asm` would do to the ROM.

```bash
z3ed overworld-allocation --rom zelda3.sfc
z3ed overworld-allocation --rom oos168x.sfc --tail-check \
  --project Oracle-of-Secrets.yaze --symbols oos168x.sym
z3ed overworld-allocation --rom oos168x.sfc --tail-check --format json
```

Per area ID it reports:
- world (`$00` light, `$40` dark, `$80` special) and used or free. An ID is
  used when its map has more than one unique tile32, it belongs to a
  large/wide/tall area, entrances, holes, exits or sprites reference it, or
  it is a subscreen overlay source (table entry or ID named in code).
- parent and area size as the game reads them.
- map pointer source and validity: both streams end in `$FF` and decode to
  256 bytes. `=$XX` marks an ID that shares another ID's pointers.
- the per-area tables that do not cover the ID. The table list names what a
  read past each table's last entry returns (a missed table reads its
  neighbour).

The summary counts used and free IDs for light, dark, special (`$80-$9F`)
and special_tail (`$A0-$BF`), and splits free IDs by whether a valid map
pointer exists.

`--tail-check` adds the patch marker, the target regions (`$28:A3FF`,
`$28:A400`, `$28:A640`, `$30:8000`, `$30:9000`) with non-zero byte counts,
the blank map streams decoded as the game would read them, the 8 map-loader
operands the patch rewrites, the free bytes between ZS code and `$28:A3FF`,
and a verdict: `safe`, `unsafe` or `already_applied`. Each blocker is tagged
`rom`, `patch` or `yaze`; `rom_compatible` is true when no `rom` blocker
exists. `--project` adds area names and hack-manifest ownership (an
`asm_owned`, `asm_expansion` or hook address blocks the patch). `--symbols`
names the code at each target.

`overworld-doctor --apply-tail-expansion` and Settings > ASM Patches >
Apply refuse the patch when the verdict is `unsafe`.

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
      "id": "invalid_map_pointer",
      "severity": "error",
      "message": "Map 0x12 has invalid pointer",
      "location": "0x1794D",
      "suggested_action": "Restore from baseline ROM",
      "fixable": false
    }
  ],
  "summary": {
    "total_findings": 1,
    "critical": 0,
    "errors": 1,
    "warnings": 0,
    "info": 0,
    "fixable": 0
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

