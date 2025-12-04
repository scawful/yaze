# Overworld v3 Padding: Special-World Tail (0xA0–0xBF)

Status: **IMPLEMENTED** (TailMapExpansion.asm patch available)
Owner: zelda3-hacking-expert
Purpose: make the unused special-world map slots (0xA0–0xBF) safe/editable without corrupting existing expanded data. We place blank map data in verified free space and point the tail of the pointer tables to them.

## Quick Start

```bash
# Apply tail expansion to a ZSCustomOverworld v3 ROM
z3ed overworld-doctor --rom=zelda3.sfc --apply-tail-expansion --output=expanded.sfc

# Or apply manually with Asar (after ZSCustomOverworld v3)
asar assets/patches/Overworld/TailMapExpansion.asm zelda3.sfc
```

## ROM Layout (LoROM, no header)

- Size: 0x200000 (2 MB)

### Expanded Data Regions (DO NOT OVERWRITE)

These regions contain critical expanded tile/map data:

| Region | PC Start | PC End | SNES | Contents |
|--------|----------|--------|------|----------|
| Tile16 Expanded | 0x1E8000 | 0x1EFFFF | $3D:0000-$3D:FFFF | 4096 tile16 entries (8 bytes each) |
| Map32 BL Expanded | 0x1F0000 | 0x1F7FFF | $3E:0000-$3E:7FFF | Map32 bottom-left tiles |
| Map32 BR Expanded | 0x1F8000 | 0x1FFFFF | $3E:8000-$3F:FFFF | Map32 bottom-right tiles |
| ZSCustom Tables | 0x140000 | 0x1421FF | $28:8000+ | Custom overworld arrays |
| Overlay Space | 0x120000 | 0x12FFFF | $24:8000+ | Expanded overlay data |

### Safe Free Space

Verified free regions for new data (banks $28–$3B):

| PC Start | PC End | Size | SNES Bank | Notes |
|----------|--------|------|-----------|-------|
| 0x1422B2 | 0x17FFFF | ~245 KB | $28-$2F | Primary free space, before expanded data |
| 0x180000 | 0x1DFFFF | 384 KB | $30-$3B | Additional free space |

**WARNING**: Do NOT use addresses 0x1E0000+ for new data - this region contains expanded tile16/map32!

### Previous Errors (FIXED)

The original padding attempted to use these addresses as "existing custom blocks":
- ~~0x1E878B, 0x1E95A3, 0x1ED6F3, 0x1EF540~~

**These were actually inside the expanded tile16 region (0x1E8000-0x1F0000) and caused tile corruption!**

The `overworld-doctor` CLI tool can detect and repair this corruption.

## Pointer Table Expansion (IMPLEMENTED)

### Original Problem

**The vanilla pointer tables only have 160 entries (maps 0x00-0x9F).**

```
Original High Table: PC 0x1794D (SNES $02:F94D), 160 entries × 3 bytes
Original Low Table:  PC 0x17B2D (SNES $02:FB2D), 160 entries × 3 bytes
```

Writing to entries for maps 0xA0+ would overwrite existing data, corrupting Light World maps.

### Solution: TailMapExpansion.asm

The `TailMapExpansion.asm` patch relocates the pointer tables to safe free space with room for 192 entries:

| Component | PC Address | SNES Address | Size |
|-----------|------------|--------------|------|
| Detection Marker | 0x1423FF | $28:A3FF | 1 byte (value: 0xEA) |
| New High Table | 0x142400 | $28:A400 | 576 bytes (192 × 3) |
| New Low Table | 0x142640 | $28:A640 | 576 bytes (192 × 3) |
| Blank Map High | 0x180000 | $30:8000 | 188 bytes |
| Blank Map Low | 0x181000 | $30:9000 | 4 bytes |

### Game Code Patches

The patch updates 8 LDA instructions in bank $02 to use the new table addresses:

**Function 1: `Overworld_DecompressAndDrawOneQuadrant`**

| PC Address | Original | Patched |
|------------|----------|---------|
| 0x1F59D | `LDA.l $02F94D,X` | `LDA.l $28A400,X` |
| 0x1F5A3 | `LDA.l $02F94E,X` | `LDA.l $28A401,X` |
| 0x1F5C8 | `LDA.l $02FB2D,X` | `LDA.l $28A640,X` |
| 0x1F5CE | `LDA.l $02FB2E,X` | `LDA.l $28A641,X` |

**Function 2: Secondary quadrant loader**

| PC Address | Original | Patched |
|------------|----------|---------|
| 0x1F7E3 | `LDA.l $02F94D,X` | `LDA.l $28A400,X` |
| 0x1F7E9 | `LDA.l $02F94E,X` | `LDA.l $28A401,X` |
| 0x1F80E | `LDA.l $02FB2D,X` | `LDA.l $28A640,X` |
| 0x1F814 | `LDA.l $02FB2E,X` | `LDA.l $28A641,X` |

### Detection

yaze detects expanded pointer tables by checking for the marker byte:

```cpp
// In src/zelda3/overworld/overworld.h
bool HasExpandedPointerTables() const {
  return rom_->data()[0x1423FF] == 0xEA;
}
```

The `overworld-doctor` command reports this status in its diagnostic output.

## Tail Table Seeding (0xA0–0xBF)

When the expansion patch is applied, tail map entries are initialized to blank data.

All addresses are expanded ZSCustom tables unless noted:
- `OverworldCustomAreaSpecificBGPalette` 0x140000: 0x0000
- `OverworldCustomMainPaletteArray` 0x140160: 0x00
- `OverworldCustomMosaicArray` 0x140200: 0x00
- `OverworldCustomAnimatedGFXArray` 0x1402A0: 0x00
- `OverworldCustomSubscreenOverlayArray` 0x140340: 0x00FF
- `OverworldCustomTileGFXGroupArray` 0x140480: eight 0x00 bytes
- `kOverworldMapParentIdExpanded` 0x140998: self-parent (index)
- `kOverworldMessagesExpanded` 0x1417F8: 0x0000
- `kOverworldScreenSize` 0x1788D: 0x00 (small)
- Vanilla tables (length 0xA0, safe to write tail): `kAreaGfxIdPtr` 0x7C9C, `kOverworldMapPaletteIds` 0x7D1C, `kOverworldSpriteset` 0x7A41 (and +0x40/+0x80), `kOverworldSpritePaletteIds` 0x7B41 (and +0x40/+0x80) all set to 0 for 0xA0–0xBF.

## CLI Tools

### overworld-validate

Validates map32 pointers and decompression for all maps:

```bash
z3ed overworld-validate --rom=file.sfc              # Check maps 0x00-0x9F
z3ed overworld-validate --rom=file.sfc --include-tail  # Include 0xA0-0xBF
z3ed overworld-validate --rom=file.sfc --check-tile16  # Check tile16 region
```

### overworld-doctor

Diagnoses ROM features, detects corruption, and applies tail expansion:

```bash
# Diagnose only
z3ed overworld-doctor --rom=file.sfc --verbose

# Compare with baseline
z3ed overworld-doctor --rom=file.sfc --baseline=vanilla.sfc

# Apply tile16 fixes
z3ed overworld-doctor --rom=file.sfc --fix --output=fixed.sfc

# Apply tail map expansion
z3ed overworld-doctor --rom=file.sfc --apply-tail-expansion --output=expanded.sfc

# Preview tail expansion (dry run)
z3ed overworld-doctor --rom=file.sfc --apply-tail-expansion --dry-run
```

Features detected:
- ZSCustomOverworld version (vanilla, v2, v3)
- Expanded tile16/tile32 status
- **Expanded pointer tables** (tail map support)
- Custom feature enables (BG colors, palettes, overlays, etc.)
- Pointer table integrity

Repairs/operations available:
- Zeroes corrupted tile16 entries
- **Applies TailMapExpansion.asm patch**

### rom-compare

Compare two ROMs to identify differences:

```bash
z3ed rom-compare --rom=target.sfc --baseline=vanilla.sfc           # Basic compare
z3ed rom-compare --rom=target.sfc --baseline=vanilla.sfc --verbose --show-diff
```

## Current Status

- **Tail maps (0xA0-0xBF)**: FUNCTIONAL (requires TailMapExpansion.asm patch)
- **Tile16 repair**: Working
- **Diagnostics**: Comprehensive feature detection working
- **Baseline comparison**: Working
- **Pointer table expansion**: Working via `--apply-tail-expansion`

## Prerequisites

The TailMapExpansion.asm patch requires:
1. **ZSCustomOverworld v3** must be applied first
2. ROM must be 2MB (standard expanded size)

The patch will fail with an error if ZSCustomOverworld v3 is not detected.

## Manual Asar Application

If applying the patch manually with Asar:

```bash
# 1. Ensure ZSCustomOverworld v3 is already applied to your ROM
# 2. Apply the tail expansion patch
asar assets/patches/Overworld/TailMapExpansion.asm zelda3.sfc
```

The patch will print success messages showing the new table locations.

## Implementation Details

### Source Files

| File | Purpose |
|------|---------|
| `assets/patches/Overworld/TailMapExpansion.asm` | ASM patch file |
| `src/zelda3/overworld/overworld.h` | `HasExpandedPointerTables()` method |
| `src/zelda3/overworld/overworld.cc` | Tail map guards |
| `src/cli/handlers/tools/overworld_doctor_commands.cc` | CLI apply logic |
| `src/cli/handlers/tools/diagnostic_types.h` | Constants |

### Guard Logic

The overworld loading code checks BOTH conditions before allowing tail map access:

```cpp
const bool allow_special_tail =
    core::FeatureFlags::get().overworld.kEnableSpecialWorldExpansion &&
    HasExpandedPointerTables();
```

If either condition is false, tail maps are filled with blank tiles to prevent corruption.

## Notes / Rationale

- Previous padding accidentally wrote to expanded tile16 region, causing visual corruption
- The doctor tool detects tile16 corruption by checking for invalid tile info patterns
- Safe free space is confirmed between 0x1422B2-0x1DFFFF (before expanded data)
- The editor/runtime has a safety net: invalid pointers fall back to blank tiles
- Oracle-of-Secrets ROM map was analyzed to verify safe placement of new tables

## Follow-up Tests

1) Unit: Construct a ROM buffer with zeroed map32 pointers for 0xA0 and assert `Overworld::Load` fills blank tiles (no crash).
2) Integration: Load expanded ROM, build map 0xA0 in the editor, and assert a 32×32 all-zero tile16 map is produced without exceptions.
3) Regression: Run `overworld-validate` and `overworld-doctor` on the expanded ROM to confirm no corruption detected.
4) Full cycle: Apply patch, edit map 0xA0, save, reload, verify changes persist.
