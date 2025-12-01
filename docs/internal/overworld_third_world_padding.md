# Overworld v3 Padding: Special-World Tail (0xA0–0xBF)

Status: **updated** (validated with `overworld-doctor` and `overworld-validate` CLI tools)  
Owner: zelda3-hacking-expert  
Purpose: make the unused special-world map slots (0xA0–0xBF) safe/editable without corrupting existing expanded data. We place blank map data in verified free space and point the tail of the pointer tables to them.

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

## Blank Map Copies (Corrected Location)

Using safe free space in bank $30:

| Data | PC Address | SNES Address | Size |
|------|------------|--------------|------|
| Map32 part1 (high) | 0x180000 | $30:8000 | 0xBC (188 bytes) |
| Map32 part2 (low) | 0x181000 | $30:9000 | 0x04 (4 bytes) |

These addresses are verified to be in the safe free space region (0x1422B2-0x1DFFFF).

## CRITICAL: Pointer Table Limitation

**The vanilla pointer tables only have 160 entries (maps 0x00-0x9F).**

### Current Pointer Table Layout

```
Low Table:  PC 0x1794D, 160 entries × 3 bytes = 0x1E0 bytes, ends at 0x17B2D
High Table: PC 0x17B2D, 160 entries × 3 bytes = 0x1E0 bytes, ends at 0x17D0D
```

**Maps 0xA0-0xBF do NOT have pointer table entries!**

### Why Simple Patching Fails

If you try to write pointers for map 0xA0:
- `ptr_low_addr = 0x1794D + (3 × 0xA0) = 0x17B2D` ← **This is the HIGH table start!**

Writing to "low" pointers for maps 0xA0+ actually **overwrites HIGH pointers for maps 0x00+**, corrupting Light World maps.

### ASM Expansion Required

To support tail maps (0xA0-0xBF), you MUST:

1. **Relocate pointer tables** to free space with room for 192 entries:
   - New Low table: 192 × 3 = 576 bytes (0x240)
   - New High table: 192 × 3 = 576 bytes (0x240)
   - Suggested location: 0x1422B2+ (safe free space)

2. **Patch all pointer table references** in game code:
   - Search for `$2F:A94D` (low table) and `$2F:B12D` (high table)
   - Update to new relocated addresses

3. **Copy existing 160 entries** to new tables

4. **Add marker** to indicate expanded tables are present

### Recommended New Table Layout

```
New Low Table:  PC 0x142400 (SNES $28:A400), 192 entries
New High Table: PC 0x142640 (SNES $28:A640), 192 entries
Tail entries:   Initialize to blank map data at $30:8000/$30:9000
```

## Tail Map Data (When ASM Expansion Is Complete)

Once pointer tables are expanded, tail map pointers should point to:

| Data | PC Address | SNES Address | Size |
|------|------------|--------------|------|
| Map32 part1 (high) | 0x180000 | $30:8000 | 0xBC (188 bytes) |
| Map32 part2 (low) | 0x181000 | $30:9000 | 0x04 (4 bytes) |

## Tail Table Seeding (0xA0–0xBF)

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

Diagnoses ROM features and detects corruption:

```bash
z3ed overworld-doctor --rom=file.sfc --verbose                    # Diagnose only
z3ed overworld-doctor --rom=file.sfc --baseline=vanilla.sfc       # Compare with baseline
z3ed overworld-doctor --rom=file.sfc --fix --output=fixed.sfc     # Apply fixes
```

Features detected:
- ZSCustomOverworld version (vanilla, v2, v3)
- Expanded tile16/tile32 status
- Custom feature enables (BG colors, palettes, overlays, etc.)
- Pointer table integrity

Repairs available:
- Zeroes corrupted tile16 entries

**Note:** Tail map padding is NOT available until ASM expansion is implemented.

### rom-compare

Compare two ROMs to identify differences:

```bash
z3ed rom-compare --rom=target.sfc --baseline=vanilla.sfc           # Basic compare
z3ed rom-compare --rom=target.sfc --baseline=vanilla.sfc --verbose --show-diff
```

## Current Status

- **Tail maps (0xA0-0xBF)**: NOT FUNCTIONAL - requires ASM expansion
- **Tile16 repair**: Working
- **Diagnostics**: Comprehensive feature detection working
- **Baseline comparison**: Working

## Notes / Rationale

- Previous padding accidentally wrote to expanded tile16 region, causing visual corruption
- The doctor tool detects tile16 corruption by checking for invalid tile info patterns
- Safe free space is confirmed between 0x1422B2-0x1DFFFF (before expanded data)
- The editor/runtime has a safety net: invalid pointers fall back to blank tiles

## Follow-up Tests

1) Unit: Construct a ROM buffer with zeroed map32 pointers for 0xA0 and assert `Overworld::Load` fills blank tiles (no crash).
2) Integration: Load fixed ROM, build map 0xA0 in the editor, and assert a 32×32 all-zero tile16 map is produced without exceptions.
3) Regression: Run `overworld-validate` and `overworld-doctor` on the fixed ROM to confirm no corruption detected.
