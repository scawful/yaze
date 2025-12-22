# HMagic Reference Documentation

**Consolidated:** 2025-12-08
**Purpose:** Reference notes and regression checklist for porting HMagic data to yaze

## Overview

HMagic is a legacy ALTTP ROM editor. This document consolidates extraction notes and regression testing requirements to ensure yaze parsers/editors avoid known HMagic bugs while reusing valuable data knowledge.

---

## Part 1: Extraction Notes

Goal: reuse hmagic's data knowledge (not its unsafe plumbing) to improve yaze parsers/editors.

### Offsets and Structures to Port

- `structs.h` contains `offsets_ty` with `dungeon_offsets_ty`, `overworld_offsets_ty`, `text_offsets_ty/text_codes_ty`.
- `z3ed.c::LoadOffsets` (currently US-only, hardcoded): torches (`0x2736a`), torch_count (`0x88c1`); text dictionary/regions (`bank=0x0e`, dictionary=0x74703..0x748D9, param_counts=0x7536b, region1=0xe0000..0xe8000, region2=0x75f40..0x77400, codes bounds).
- Action: lift into a region table (US/EU/JP) with file-size validation before use.
- Status: US offsets added at `src/zelda3/formats/offsets.{h,cc}` with basic bounds validation; EU/JP remain TODO.

### Text Decode/Encode Logic (TextLogic.c)

- **Decoder**: walks monologue stream until `abs_terminator`; handles `region_switch`, zchars `< zchar_bound`, dictionary entries (`dict_base`..), `msg_terminator`, and commands with params via `param_counts` table. Appends to `ZTextMessage` buffers.
- **Encoder**: rebuilds messages, searches dictionary for matches (linear search), writes params per `param_counts`, respects `max_message_length`.
- **Bugs/risks**: heavy global reliance on `offsets`, no ROM bounds checks, dictionary search inline and naive. Port by reimplementing with span/bounds checks and unit tests.

### Tile/Palette Rendering Math

Files: `DungeonLogic.c`, `TileMapLogic.c`, `GraphicsLogic.c`

- Helpers like `fill4x2`, `drawXx3/4`, bitplane copies
- Data: tile stride = 64 words per row in buffer; SNES bitplane packing assumed
- **Bugs**: `draw3x2` doesn't advance pointers; many helpers assume globals (`dm_rd`, `dm_wr`, `dm_buf`, `dm_l`)
- If reusing math, reimplement with explicit buffer sizes and tests

### Data Tables/Enums Worth Reusing

- `sprname.dat` (sprite names) - standard format parsed and ported to `src/zelda3/sprite/sprite_names.h` (284 entries, length-prefixed, limit 15 chars)
- Enum headers: `DungeonEnum.h`, `OverworldEnum.h`, `HMagicEnum.h`, `TextEnum.h`, `SampleEnum.h`, etc.
- UI labels (entrance names, dungeon names) via `HM_TextResource entrance_names`

### Known Bugs/Behavior to Test Against

From z3ed.c header:
- **Overworld**: editing items/exits/whirlpools can corrupt pointers; "remove all exits" nukes ending white dots and Ganon->Triforce entrance; global grid editing inserts entrances/crashes
- **Dungeon**: pointer calc fails with too much data (space issues)
- **BG1/BG2**: sprite handling fixed in later epochs; still verify
- **Monologue**: storage corruption fixed once; still regression-test text save/load

---

## Part 2: Regression Checklist

Goal: ensure yaze parsers/editors don't reproduce known hmagic bugs.

### Test Scenarios

#### 1. Overworld Exits/Items/Whirlpools Pointer Integrity
- Edit exits/whirlpools/items, save, reload ROM
- Verify ending white dots (credits) and Ganon->Triforce entrance remain intact
- Validate pointer tables (no mass repoint to empty room)

#### 2. "Remove All Overworld Exits" Safety
- Invoke bulk-delete; ensure credits dots and special entrances remain
- Verify no unintended entrance insertion/crash when editing global grid

#### 3. Dungeon Room Pointer Overflow
- Add room data until near limit
- Ensure pointer calc stays valid and refuses to corrupt or overrun
- Should emit error instead of silent corruption

#### 4. BG1/BG2 Sprite Handling
- Move BG2 markers (red dots) and confirm BG flag persists across save/reload

#### 5. Monologue Storage
- Round-trip text: decode -> modify -> encode
- Ensure no bank overflow, region switch honored, abs terminator respected
- Dictionary bounds enforced

#### 6. Sprite Names
- Decode `sprname.dat` standard format (flag=0, LE size, 0x11c length-prefixed entries)
- Ensure names load
- If alt format (flag!=0) appears, validate 256x9-byte table

### Implementation Plan

Add gtest cases using a US ROM fixture (vanilla.sfc):
- **Text**: round-trip known messages and synthetic long messages; check max length enforcement
- **Sprite names**: decode standard/alt format blobs; compare to reference array
- **Pointer safety**: build synthetic overworld/dungeon tables and assert parsers reject OOB offsets

Add CLI fixtures for bulk operations (remove exits/items) and assert postconditions via serializers.

---

## Porting Approach for yaze

1. Add a `formats/zelda3_offsets.{h,cc}` with a region table (US/EU/JP) + validation (file size, bounds on dictionary/regions/param tables). Expose typed structs matching hmagic but without globals.

2. **Text**: implement a safe decoder/encoder following hmagic's logic, with tests using a known US ROM fixture; include dictionary search as a reusable function. Add CLI or unit tests to compare round-trip.

3. **Tile/metatile**: reimplement only the packing math you need; avoid copying `dm_*` globals. Add tests with sample tile data to verify blits.

4. **Data tables**: convert `sprname.dat` and enums into constexpr arrays/enum classes; include provenance comments.

5. **Regression checklist**: create a test list based on the "Remaining things Puzzledude claims are borked" to ensure yaze doesn't repeat those bugs.

---

## References to Harvest

- `structs.h` for offsets structs
- `z3ed.c::LoadOffsets` for US constants
- `TextLogic.c` for monologue decode/encode flow
- `DungeonLogic.c` / `TileMapLogic.c` / `GraphicsLogic.c` for tile/bitplane math (reimplement safely)
- `sprname.dat` for sprite names
- Header comments in `z3ed.c` for bug/regression notes
