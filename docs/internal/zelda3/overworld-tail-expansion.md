# Overworld Tail Map Expansion (0xA0-0xBF)

**Consolidated:** 2025-12-08
**Status:** IMPLEMENTED
**Owner:** zelda3-hacking-expert / overworld-specialist
**Purpose:** Enable editing of special-world tail map slots (0xA0-0xBF) without corrupting existing data

---

## Quick Start

```bash
# Apply tail expansion to a ZSCustomOverworld v3 ROM
z3ed overworld-doctor --rom=zelda3.sfc --apply-tail-expansion --output=expanded.sfc

# Or apply manually with Asar (after ZSCustomOverworld v3)
asar assets/patches/Overworld/TailMapExpansion.asm zelda3.sfc
```

**Prerequisites:**
1. **ZSCustomOverworld v3** must be applied first
2. ROM must be 2MB (standard expanded size)

---

## Problem Statement

**The vanilla pointer tables only have 160 entries (maps 0x00-0x9F).**

```
Original High Table: PC 0x1794D (SNES $02:F94D), 160 entries x 3 bytes
Original Low Table:  PC 0x17B2D (SNES $02:FB2D), 160 entries x 3 bytes
```

Writing to entries for maps 0xA0+ would overwrite existing data, corrupting Light World maps 0x00-0x1F.

---

## Solution: TailMapExpansion.asm

The `TailMapExpansion.asm` patch relocates pointer tables to safe free space with 192 entries:

| Component | PC Address | SNES Address | Size |
|-----------|------------|--------------|------|
| Detection Marker | 0x1423FF | $28:A3FF | 1 byte (value: 0xEA) |
| New High Table | 0x142400 | $28:A400 | 576 bytes (192 x 3) |
| New Low Table | 0x142640 | $28:A640 | 576 bytes (192 x 3) |
| Blank Map High | 0x180000 | $30:8000 | 188 bytes |
| Blank Map Low | 0x181000 | $30:9000 | 4 bytes |

### Game Code Patches

The patch updates 8 LDA instructions in bank $02:

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

---

## ROM Layout (LoROM, no header)

### Expanded Data Regions (DO NOT OVERWRITE)

| Region | PC Start | PC End | SNES | Contents |
|--------|----------|--------|------|----------|
| Tile16 Expanded | 0x1E8000 | 0x1EFFFF | $3D:0000-$3D:FFFF | 4096 tile16 entries |
| Map32 BL Expanded | 0x1F0000 | 0x1F7FFF | $3E:0000-$3E:7FFF | Map32 bottom-left tiles |
| Map32 BR Expanded | 0x1F8000 | 0x1FFFFF | $3E:8000-$3F:FFFF | Map32 bottom-right tiles |
| ZSCustom Tables | 0x140000 | 0x1421FF | $28:8000+ | Custom overworld arrays |
| Overlay Space | 0x120000 | 0x12FFFF | $24:8000+ | Expanded overlay data |

### Safe Free Space

| PC Start | PC End | Size | SNES Bank | Notes |
|----------|--------|------|-----------|-------|
| 0x1422B2 | 0x17FFFF | ~245 KB | $28-$2F | Primary free space |
| 0x180000 | 0x1DFFFF | 384 KB | $30-$3B | Additional free space |

**WARNING**: Do NOT use addresses 0x1E0000+ for new data!

---

## CLI Tools

### overworld-doctor

Full diagnostic and repair:

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

### overworld-validate

Validate map pointers and decompression:

```bash
z3ed overworld-validate --rom=file.sfc              # Check maps 0x00-0x9F
z3ed overworld-validate --rom=file.sfc --include-tail  # Include 0xA0-0xBF
z3ed overworld-validate --rom=file.sfc --check-tile16  # Check tile16 region
```

### rom-compare

Compare two ROMs:

```bash
z3ed rom-compare --rom=target.sfc --baseline=vanilla.sfc --verbose --show-diff
```

---

## Detection

yaze detects expanded pointer tables by checking for the marker byte:

```cpp
// In src/zelda3/overworld/overworld.h
bool HasExpandedPointerTables() const {
  return rom_->data()[0x1423FF] == 0xEA;
}
```

The loading code checks BOTH conditions before allowing tail map access:

```cpp
const bool allow_special_tail =
    core::FeatureFlags::get().overworld.kEnableSpecialWorldExpansion &&
    HasExpandedPointerTables();
```

---

## Source Files

| File | Purpose |
|------|---------|
| `assets/patches/Overworld/TailMapExpansion.asm` | ASM patch file |
| `src/zelda3/overworld/overworld.h` | `HasExpandedPointerTables()` method |
| `src/zelda3/overworld/overworld.cc` | Tail map guards |
| `src/cli/handlers/tools/overworld_doctor_commands.cc` | CLI apply logic |
| `src/cli/handlers/tools/overworld_validate_commands.cc` | Validator command |
| `src/cli/handlers/tools/rom_compare_commands.cc` | ROM comparison |
| `src/cli/handlers/tools/diagnostic_types.h` | Constants |

---

## Testing

```bash
# 1. Check ROM baseline
z3ed rom-doctor --rom vanilla.sfc --format json

# 2. Apply tail expansion patch
z3ed overworld-doctor --rom vanilla.sfc --apply-tail-expansion --output expanded.sfc

# 3. Verify expansion marker
z3ed rom-doctor --rom expanded.sfc --format json | jq '.expanded_pointer_tables'

# 4. Run integration tests
z3ed test-run --label rom_dependent --format json
```

---

## Known Issues / History

### Previous Errors (FIXED)

The original padding attempted to use addresses inside the expanded tile16 region:
- ~~0x1E878B, 0x1E95A3, 0x1ED6F3, 0x1EF540~~

These caused tile corruption. The `overworld-doctor` CLI tool can detect and repair this corruption.

### Remaining Tasks

1. Integration testing with fully patched ROM
2. GUI indicator in Overworld Editor showing tail map availability
3. Unit tests for marker detection and guard logic
