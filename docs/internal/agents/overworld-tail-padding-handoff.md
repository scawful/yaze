# Overworld Tail Maps & ROM Diagnostics – Handoff

**Status:** Phase 3 Complete - CLI Infrastructure Enhanced
**Last Updated:** Dec 2, 2025
**Owner:** zelda3-hacking-expert / overworld-specialist

---

## Summary

Comprehensive diagnostic and validation tools for overworld data are now complete.
Tail map support (0xA0-0xBF) is now available via the `TailMapExpansion.asm` patch.

## Critical Finding: Pointer Table Limitation

**The vanilla pointer tables only have 160 entries (maps 0x00-0x9F).**

```
Low Table:  PC 0x1794D, 160 entries × 3 bytes = 0x1E0 bytes, ends at 0x17B2D
High Table: PC 0x17B2D, 160 entries × 3 bytes = 0x1E0 bytes, ends at 0x17D0D
```

Writing to tail map entries (0xA0-0xBF) with current tables **overwrites Light World pointers** and corrupts maps 0x00-0x1F.

---

## Completed Tasks

### CLI Tools Created

| Command | Purpose | Files |
|---------|---------|-------|
| `overworld-validate` | Validate map pointers & decompression | `cli/handlers/tools/overworld_validate_commands.{h,cc}` |
| `overworld-doctor` | Diagnose ROM features & corruption | `cli/handlers/tools/overworld_doctor_commands.{h,cc}` |
| `rom-compare` | Compare two ROMs for differences | `cli/handlers/tools/rom_compare_commands.{h,cc}` |

### Features Implemented

1. **ROM Feature Detection**
   - ZSCustomOverworld version (vanilla, v2, v3)
   - Expanded tile16/tile32 flags
   - Custom feature enables (BG colors, palettes, overlays, etc.)
   - Pointer table integrity checking

2. **Baseline Comparison**
   - `--baseline` flag for doctor command
   - `rom-compare` command for detailed diffs
   - Identifies pointer table corruption

3. **UI Integration**
   - `kEnableSpecialWorldExpansion` flag in Settings Panel
   - Tail expansion checkbox in ROM Load Options Dialog

4. **Bug Fixes**
   - Guarded `PadTailMaps` to prevent pointer table corruption
   - Removed unsafe pointer writes for tail maps

### Documentation Updated

- `docs/internal/overworld_third_world_padding.md` - ASM requirements documented
- This handoff document

---

## Pending Work: ASM Pointer Table Expansion

### Requirements

To support tail maps (0xA0-0xBF), the following ASM work is needed:

1. **Relocate pointer tables** to free space with 192 entries each
   - Suggested location: 0x1422B2+ (safe free space)
   - New Low table: 576 bytes (192 × 3)
   - New High table: 576 bytes (192 × 3)

2. **Patch all pointer table references** in game code
   - Find references to $2F:A94D (low) and $2F:B12D (high)
   - Update to new addresses

3. **Add detection marker** for expanded tables
   - The doctor command can then detect and enable tail support

4. **Initialize tail entries** to blank map data at $30:8000/$30:9000

### Suggested New Table Layout

```
New Low Table:  PC 0x142400 (SNES $28:A400), 192 entries
New High Table: PC 0x142640 (SNES $28:A640), 192 entries
```

---

## CLI Usage

```bash
# Full ROM diagnostic
z3ed overworld-doctor --rom=file.sfc --verbose

# Compare with baseline
z3ed overworld-doctor --rom=file.sfc --baseline=vanilla.sfc

# Validate all maps
z3ed overworld-validate --rom=file.sfc --include-tail --check-tile16

# Compare two ROMs
z3ed rom-compare --rom=target.sfc --baseline=vanilla.sfc --show-diff

# Repair tile16 corruption
z3ed overworld-doctor --rom=file.sfc --fix --output=fixed.sfc
```

---

## Test Results

### vanilla.sfc
- ZSCustomOverworld v3
- Expanded Tile16: YES
- All maps (0x00-0x9F): OK
- Tail maps: N/A (no ASM expansion)

### zelda3.sfc vs vanilla.sfc
- 728 bytes differ in pointer tables
- Saved by yaze at some point with modified pointers

### oos168.sfc
- ZSCustomOverworld v3
- 2 corrupted tile16 entries (fixable with doctor)
- Pointer tables differ from vanilla

---

## Files Changed

| File | Changes |
|------|---------|
| `src/cli/handlers/tools/overworld_validate_commands.{h,cc}` | Validator command |
| `src/cli/handlers/tools/overworld_doctor_commands.{h,cc}` | Doctor command + --apply-tail-expansion |
| `src/cli/handlers/tools/diagnostic_types.h` | Expanded pointer table constants |
| `src/cli/handlers/tools/rom_compare_commands.{h,cc}` | ROM comparison command |
| `src/cli/handlers/command_handlers.cc` | Command registration |
| `src/cli/agent.cmake` | Build integration |
| `src/app/gui/app/feature_flags_menu.h` | Settings panel flag |
| `src/app/editor/ui/rom_load_options_dialog.{h,cc}` | Upgrade dialog checkbox |
| `src/core/features.h` | Feature flag (already existed) |
| `src/zelda3/overworld/overworld.h` | HasExpandedPointerTables() + constants |
| `src/zelda3/overworld/overworld.cc` | Tail map guards check ROM marker |
| `assets/patches/Overworld/TailMapExpansion.asm` | **NEW** ASM patch |
| `docs/internal/overworld_third_world_padding.md` | ASM requirements |

---

## Completed Phase 2 Tasks

1. ✅ **Created `TailMapExpansion.asm`** - `assets/patches/Overworld/TailMapExpansion.asm`
   - Relocates pointer tables from $02:F94D to $28:A400
   - Expands from 160 to 192 entries for maps 0xA0-0xBF
   - Patches 8 game code locations to use new tables
   - Writes marker byte 0xEA at $28:A3FF for detection
   - Creates blank map data at $30:8000/$30:9000

2. ✅ **Added marker detection** to `overworld_doctor_commands.cc`
   - `HasExpandedPointerTables()` checks for marker at 0x1423FF
   - Diagnostic output shows expanded table status

3. ✅ **Added `--apply-tail-expansion` flag** to CLI
   - Applies patch via Asar wrapper
   - Validates ZSCustomOverworld v3 prerequisite
   - Dry-run support with `--dry-run`

4. ✅ **Updated tail map guards** in `overworld.cc`
   - Guards now check BOTH feature flag AND ROM marker
   - Prevents pointer table corruption on non-expanded ROMs

## Phase 3: CLI Infrastructure (Dec 2, 2025)

### Completed

1. ✅ **Doctor Suite Expansion**
   - Added `dungeon-doctor` for room data integrity validation
   - Added `rom-doctor` for ROM header/checksum/expansion validation
   - All doctor commands now use `OutputFormatter` for JSON/text output

2. ✅ **Test CLI Infrastructure**
   - Added `test-list` for machine-readable test discovery
   - Added `test-run` for structured test execution
   - Added `test-status` for configuration status
   - Commands work without ROM (`RequiresRom()` fix applied)

3. ✅ **Documentation**
   - Created `docs/public/cli/doctor-commands.md`
   - Created `docs/public/cli/test-commands.md`
   - Updated `docs/internal/agents/cli-ux-proposals.md`

### New Commands

| Command | Description | Requires ROM |
|---------|-------------|--------------|
| `dungeon-doctor` | Room validation (objects, sprites, chests) | Yes |
| `rom-doctor` | Header, checksums, expansion status | Yes |
| `test-list` | List available test suites | No |
| `test-run` | Run tests with structured output | No |
| `test-status` | Show test configuration | No |

### Testing Strategy for Tail Padding

With the new infrastructure, testing tail padding follows this workflow:

```bash
# 1. Check ROM baseline
z3ed rom-doctor --rom vanilla.sfc --format json

# 2. Apply tail expansion patch (when ready)
z3ed overworld-doctor --rom vanilla.sfc --apply-tail-expansion --output expanded.sfc

# 3. Verify expansion marker
z3ed rom-doctor --rom expanded.sfc --format json | jq '.expanded_pointer_tables'

# 4. Run integration tests
z3ed test-run --label rom_dependent --format json
```

---

## Remaining Tasks

1. **Integration testing** with fully patched ROM
2. **GUI indicator** in Overworld Editor showing tail map availability
3. **Unit tests** for marker detection and guard logic
