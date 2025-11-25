# ZScream Custom Overworld (`Overworld/ZSCustomOverworld.asm`)

## 1. Overview

ZSCustomOverworld is a powerful and extensive system that replaces large parts of the vanilla *A Link to the Past* overworld engine. Its primary purpose is to remove hardcoded behaviors and replace them with a data-driven approach, allowing for a highly customizable overworld.

Instead of relying on hardcoded logic for palettes, graphics, and layouts, ZSCustomOverworld reads this information from a large pool of data tables located in expanded ROM space (starting at `$288000`). These tables are designed to be edited by the ZScream overworld editor.

## 2. Key Features

- **Custom Palettes & Colors:** Assign a unique main palette and background color to every overworld screen.
- **Custom Graphics:** Assign custom static tile graphics (GFX groups) and animated tile sets to each area.
- **Custom Overlays:** Add or remove subscreen overlays (like rain, fog, and clouds) on a per-area basis.
- **Flexible Layouts:** Fixes vanilla bugs related to screen transitions and adds support for new area sizes, such as 2x1 "wide" and 1x2 "tall" areas, in addition to the standard 1x1 and 2x2.
- **Expanded Special Worlds:** Allows the normally limited "special world" areas (like the Master Sword grove) to be used as full-featured overworld screens.

## 3. Core Architecture: Data Tables

The system's flexibility comes from a large data pool starting at `org $288000`. Key tables include:

- **`.BGColorTable`:** A table of 16-bit color values for the background of each overworld screen.
- **`.EnableTable`:** A series of flags to enable or disable specific features of ZSCustomOverworld, such as custom palettes or overlays.
- **`.MainPaletteTable`:** An index (`$00` to `$05`) into the game's main overworld palette sets for each screen.
- **`.MosaicTable`:** A bitfield for each screen to control mosaic transitions on a per-direction basis.
- **`.AnimatedTable`:** The GFX sheet ID for animated tiles for each screen.
- **`.OverlayTable`:** The overlay ID (e.g., `$9F` for rain) for each screen. `$FF` means no overlay.
- **`.OWGFXGroupTable`:** A large table defining the 8 GFX group sheets to be loaded for each overworld screen.
- **`.Overworld_ActualScreenID_New`:** A table that defines the "parent" screen for multi-screen areas (e.g., for a 2x2 area, all four screens point to the top-left screen's ID).
- **`.ByScreen..._New` Tables:** Four tables (`ByScreen1` for right, `2` for left, `3` for down, `4` for up) that define the camera boundaries for screen transitions. These are crucial for supporting non-standard area sizes.
- **`.Overworld_SpritePointers_state_..._New` Tables:** These tables define which sprite set to load for each overworld area based on the game state (`state_0` for the intro, `state_1` for post-Agahnim 1, `state_2` for post-Ganon). This allows for different enemy and NPC populations as the story progresses.

## 4. Key Hooks & Functions

ZSCustomOverworld replaces dozens of vanilla routines. Some of the most critical hooks are:

- `org $0283EE` (**`PreOverworld_LoadProperties_Interupt`**):
  - **Original:** `Overworld_LoadProperties`. This function loads music, palettes, and GFX when transitioning from a dungeon/house to the overworld.
  - **New Logic:** The ZS version is heavily modified to read from the custom data tables for palettes and GFX instead of using hardcoded logic. It also removes hardcoded music changes for certain exits.

- `org $02C692` (**`Overworld_LoadAreaPalettes`**):
  - **Original:** A routine to load overworld palettes.
  - **New Logic:** Reads the main palette index from the `.MainPaletteTable` instead of using a hardcoded value.

- `org $02A9C4` (**`OverworldHandleTransitions`**):
  - **Original:** The main logic for handling screen-to-screen transitions on the overworld.
  - **New Logic:** This is one of the most heavily modified sections. The new logic uses the custom tables (`.ByScreen...`, `.Overworld_ActualScreenID_New`, etc.) to handle transitions between areas of different sizes, fixing vanilla bugs and allowing for new layouts.

- `org $02AF58` (**`Overworld_ReloadSubscreenOverlay_Interupt`**):
  - **Original:** Logic for loading subscreen overlays.
  - **New Logic:** Reads the overlay ID from the `.OverlayTable` instead of using hardcoded checks for specific areas (like the Misery Mire rain).

- `org $09C4C7` (**`LoadOverworldSprites_Interupt`**):
    - **Original:** `LoadOverworldSprites`. This function determines which sprites to load for the current overworld screen.
    - **New Logic:** The ZS version reads from the `.Overworld_SpritePointers_state_..._New` tables based on the current game state (`$7EF3C5`) to get a pointer to the correct sprite set for the area. This allows for dynamic sprite populations.

## 5. Configuration

- **`!UseVanillaPool`:** A flag that, when set to 1, forces the system to use data tables that mimic the vanilla game's behavior. This is useful for debugging.
- **`!Func...` Flags:** A large set of individual flags that allow for enabling or disabling specific hooks. This provides granular control for debugging and compatibility testing.

## 6. Analysis & Future Work: Sprite Loading

The `LoadOverworldSprites_Interupt` hook at `org $09C4C7` is a critical component that requires further investigation to support dynamic sprite sets, such as those needed for a day/night cycle.

- **Identified Conflict:** The current ZScream implementation for sprite loading conflicts with external logic that attempts to swap sprite sets based on in-game conditions (e.g., time of day). The original hook's design, which calls `JSL.l Sprite_OverworldReloadAll`, can lead to recursive loops and stack overflows if not handled carefully.

- **Investigation Goal:** The primary goal is to modify `LoadOverworldSprites_Interupt` to accommodate multiple sprite sets for a single area. The system needs to be able to check a condition (like whether it is currently night) and then select the appropriate sprite pointer, rather than relying solely on the static `state_..._New` tables.

- **Technical Challenges:** A previous attempt to integrate this functionality was reverted due to build system issues where labels from other modules (like `Oracle_CheckIfNight16Bit`) were not visible to `ZSCustomOverworld.asm`. A successful solution will require resolving these cross-module dependencies and carefully merging the day/night selection logic with ZScream's existing data-driven approach to sprite loading.

# ZScream Custom Overworld - Advanced Technical Documentation

**Target Audience**: Developers modifying ZScream internals or integrating complex systems  
**Prerequisites**: Understanding of 65816 assembly, ALTTP memory architecture, and basic ZScream concepts  
**Last Updated**: October 3, 2025

---

## Table of Contents

1. [Internal Hook Architecture](#1-internal-hook-architecture)
2. [Memory Management & State Tracking](#2-memory-management--state-tracking)
3. [Graphics Loading Pipeline](#3-graphics-loading-pipeline)
4. [Sprite Loading System Deep Dive](#4-sprite-loading-system-deep-dive)
5. [Cross-Namespace Integration](#5-cross-namespace-integration)
6. [Performance Considerations](#6-performance-considerations)
7. [Adding Custom Features](#7-adding-custom-features)
8. [Debugging & Troubleshooting](#8-debugging--troubleshooting)

---

## 1. Internal Hook Architecture

### 1.1 Hook Categories

ZScream replaces **38+ vanilla routines** across multiple ROM banks. These hooks fall into distinct categories:

| Category | Count | Purpose |
|----------|-------|---------|
| **Palette Loading** | 7 | Load custom palettes per area |
| **Graphics Decompression** | 12 | Load custom static/animated GFX |
| **Subscreen Overlays** | 8 | Control rain, fog, pyramid BG |
| **Screen Transitions** | 9 | Handle camera, scrolling, mosaic |
| **Sprite Loading** | 2 | Load sprites based on area+state |

### 1.2 Hook Execution Order During Transition

When Link transitions between overworld screens, hooks fire in this precise order:

```
[TRANSITION START]
    ↓
1. Overworld_OperateCameraScroll_Interupt ($02BC44)
   └─ Controls camera movement, checks for pyramid BG special scrolling
    ↓
2. OverworldScrollTransition_Interupt ($02C02D)
   └─ Aligns BG layers during scroll, prevents BG1 flicker for pyramid
    ↓
3. OverworldHandleTransitions ($02A9C4) [CRITICAL]
   └─ Calculates new screen ID using .ByScreen tables
   └─ Handles staggered layouts, 2x1/1x2 areas
   └─ Triggers mosaic if .MosaicTable has bit set
    ↓
4. NewOverworld_FinishTransGfx (Custom Function)
   ├─ Frame 0: CheckForChangeGraphicsTransitionLoad
   │  └─ Reads .OWGFXGroupTable for new area
   ├─ Frame 0: LoadTransMainGFX
   │  └─ Decompresses 3 static sheets (if changed)
   ├─ Frame 0: PrepTransMainGFX
   │  └─ Stages GFX in buffer for DMA
   ├─ Frames 1-7: BlockGFXCheck
   │  └─ DMA's one 0x0600-byte block per frame
   └─ Frame 8: Complete, move to next module
    ↓
5. NewLoadTransAuxGFX ($00D673)
   └─ Decompresses variable GFX sheets 3-6 (if changed)
   └─ Stages in $7E6000 buffer
    ↓
6. NMI_UpdateChr_Bg2HalfAndAnimated (Custom NMI Handler)
   └─ DMA's variable sheets to VRAM during NMI
    ↓
7. Overworld_ReloadSubscreenOverlay_Interupt ($02AF58)
   └─ Reads .OverlayTable, activates BG1 for subscreen if needed
    ↓
8. Overworld_LoadAreaPalettes ($02C692)
   └─ Reads .MainPaletteTable, loads sprite/BG palettes
    ↓
9. Palette_SetOwBgColor_Long ($0ED618)
   └─ Reads .BGColorTable, sets transparent color
    ↓
10. LoadOverworldSprites_Interupt ($09C4C7)
    └─ Reads .Overworld_SpritePointers_state_X_New
    └─ Integrates day/night check (Oracle_ZSO_CheckIfNight)
    ↓
[TRANSITION COMPLETE]
```

### 1.3 Critical Path: Transition GFX Loading

The **most performance-sensitive** part of ZScream is the graphics decompression pipeline:

```asm
; Vanilla: Immediate 3BPP decompression, ~2 frames
; ZScream: Conditional decompression + DMA staging, ~1-8 frames

NewOverworld_FinishTransGfx:
{
    ; Frame 0: Decision + Decompression
    LDA.w TransGFXModuleFrame : BNE .notFirstFrame
        ; Read new area's GFX group (8 sheets)
        JSR.w CheckForChangeGraphicsTransitionLoad
        
        ; Decompress sheets 0-2 (static) if changed
        JSR.w LoadTransMainGFX
        
        ; If any sheets changed, prep for DMA
        LDA.b $04 : BEQ .dontPrep
            JSR.w PrepTransMainGFX
    
    .notFirstFrame
    
    ; Frames 1-7: DMA one block per frame (saves CPU time)
    LDA.b #$08 : STA.b $06
    JSR.w BlockGFXCheck
    
    ; Frame 8: Complete
    CPY.b #$08 : BCC .return
        INC.b $11  ; Move to next submodule
}
```

**Key Insight**: The `TransGFXModule_PriorSheets` array ($04CB[0x08]) caches the last loaded GFX group. If the new area uses the same sheets, decompression is **skipped entirely**, saving ~1-2 frames.

---

## 2. Memory Management & State Tracking

### 2.1 Free RAM Usage

ZScream claims specific free RAM regions for state tracking:

| Address | Size | Label | Purpose |
|---------|------|-------|---------|
| `$04CB` | 8 bytes | `TransGFXModule_PriorSheets` | Cache of last loaded GFX sheets (0-7) |
| `$04D3` | 2 bytes | `NewNMITarget1` | VRAM target for NMI DMA (sheet 1) |
| `$04D5` | 2 bytes | `NewNMISource1` | Source address for NMI DMA (sheet 1) |
| `$04D7` | 2 bytes | `NewNMICount1` | Byte count for NMI DMA (sheet 1) |
| `$04D9` | 2 bytes | `NewNMITarget2` | VRAM target for NMI DMA (sheet 2) |
| `$04DB` | 2 bytes | `NewNMISource2` | Source address for NMI DMA (sheet 2) |
| `$04DD` | 2 bytes | `NewNMICount2` | Byte count for NMI DMA (sheet 2) |
| `$0716` | 2 bytes | `OWCameraBoundsS` | Custom camera bounds (south/left) |
| `$0718` | 2 bytes | `OWCameraBoundsE` | Custom camera bounds (east/right) |
| `$0CF3` | 1 byte | `TransGFXModuleFrame` | Frame counter for GFX loading |
| `$0FC0` | 1 byte | `AnimatedTileGFXSet` | Current animated tile set ID |
| `$7EFDC0` | 64 bytes | `ExpandedSpritePalArray` | Expanded sprite palette array |

### 2.2 Data Pool Memory Map (Bank $28)

All custom data tables reside in a reserved block:

```
org $288000 ; PC $140000
Pool:
{
    ; PALETTE DATA
    .BGColorTable                   ; [0x0180] - 16-bit BG colors per area
    .MainPaletteTable               ; [0x00C0] - Palette set indices (0-5)
    
    ; FEATURE TOGGLES
    .EnableTable                    ; [0x00C0] - Enable/disable features
    .EnableTransitionGFXGroupLoad   ; [0x0001] - Global GFX load toggle
    .MosaicTable                    ; [0x0180] - Mosaic bitfields per area
    
    ; GRAPHICS DATA
    .AnimatedTable                  ; [0x00C0] - Animated tile set IDs
    .OverlayTable                   ; [0x0180] - Overlay IDs ($9F=rain, $FF=none)
    .OWGFXGroupTable                ; [0x0600] - 8 sheets per area (8*$C0)
        .OWGFXGroupTable_sheet0     ; [0x00C0]
        .OWGFXGroupTable_sheet1     ; [0x00C0]
        // ... sheets 2-7
    
    ; LAYOUT & TRANSITION DATA
    .DefaultGFXGroups               ; [0x0018] - 8 sheets * 3 worlds
    .Overworld_ActualScreenID_New   ; [0x0180] - Parent screen for multi-tile
    
    ; CAMERA BOUNDARIES
    .ByScreen1_New                  ; [0x0180] - Right transition bounds
    .ByScreen2_New                  ; [0x0180] - Left transition bounds
    .ByScreen3_New                  ; [0x0180] - Down transition bounds
    .ByScreen4_New                  ; [0x0180] - Up transition bounds
    
    ; SPRITE POINTERS
    .Overworld_SpritePointers_state_0_New ; [0x0140] - Intro sprites
    .Overworld_SpritePointers_state_1_New ; [0x0140] - Post-Aga1 sprites
    .Overworld_SpritePointers_state_2_New ; [0x0140] - Post-Ganon sprites
}
assert pc() <= $289938 ; Must not exceed this boundary!
```

**⚠️ CRITICAL**: The pool ends at `$289938` (`$141938`). Exceeding this boundary will corrupt other code!

### 2.3 Table Index Calculation

Most ZScream tables are indexed by area ID (`$8A`):

```asm
; BYTE TABLES (1 byte per area)
LDA.b $8A                    ; Load current area
TAX                          ; Use as index
LDA.l Pool_MainPaletteTable, X  ; Read palette index

; WORD TABLES (2 bytes per area)
LDA.b $8A : ASL : TAX        ; Area * 2 for 16-bit index
LDA.l Pool_BGColorTable, X   ; Read 16-bit color

; GFX GROUP TABLE (8 bytes per area)
REP #$30
LDA.b $8A : AND.w #$00FF : ASL #3 : TAX  ; Area * 8
SEP #$20
LDA.w Pool_OWGFXGroupTable_sheet0, X  ; Read sheet 0
LDA.w Pool_OWGFXGroupTable_sheet1, X  ; Read sheet 1
// etc.
```

---

## 3. Graphics Loading Pipeline

### 3.1 Static GFX Sheets (0-2)

Sheets 0-2 are **always loaded together** because they decompress quickly (~1 frame):

```
Sheet 0: Main tileset (walls, ground, trees)
Sheet 1: Secondary tileset (decorative, objects)
Sheet 2: Tertiary tileset (area-specific)
```

**Loading Process**:
1. `LoadTransMainGFX` reads `.OWGFXGroupTable_sheet0-2` for new area
2. Compares against `TransGFXModule_PriorSheets[0-2]`
3. If changed, calls `Decomp_bg_variable` for each sheet
4. `PrepTransMainGFX` stages decompressed data in `$7F0000` buffer
5. `BlockGFXCheck` DMA's one 0x0600-byte block per frame (8 frames total)

### 3.2 Variable GFX Sheets (3-6)

Sheets 3-6 are **loaded separately** because they're larger (~2-3 frames total):

```
Sheet 3: Variable tileset slot 0
Sheet 4: Variable tileset slot 1
Sheet 5: Variable tileset slot 2
Sheet 6: Variable tileset slot 3
```

**Loading Process**:
1. `NewLoadTransAuxGFX` reads `.OWGFXGroupTable_sheet3-6` for new area
2. Compares against `TransGFXModule_PriorSheets[3-6]`
3. If changed, calls `Decomp_bg_variableLONG` for each sheet
4. Decompressed data staged in `$7E6000` buffer
5. `NMI_UpdateChr_Bg2HalfAndAnimatedLONG` DMA's during NMI using:
   - `NewNMISource1/2` - Source addresses in `$7F` bank
   - `NewNMITarget1/2` - VRAM targets
   - `NewNMICount1/2` - Transfer sizes

### 3.3 Animated Tile GFX

Animated tiles (waterfalls, lava, spinning tiles) use a **separate system**:

```asm
ReadAnimatedTable:
{
    PHB : PHK : PLB
    
    ; Index into .AnimatedTable (1 byte per area)
    LDA.b $8A : TAX
    LDA.w Pool_AnimatedTable, X
    
    ; Store in $0FC0 for game to use
    STA.w AnimatedTileGFXSet
    
    PLB
    RTL
}
```

**Animated GFX Set IDs**:
- `$58` - Light World water/waterfalls
- `$59` - Dark World lava/skulls
- `$5A` - Special World animations
- `$FF` - No animated tiles

**Decompression Timing**:
- **On transition**: `ReadAnimatedTable : DEC : TAY` → `DecompOwAnimatedTiles`
- **On mirror warp**: `AnimateMirrorWarp_DecompressAnimatedTiles`
- **After bird travel**: Hook at `$0AB8F5`
- **After map close**: Hook at `$0ABC5A`

### 3.4 GFX Sheet $FF - Skip Loading

If any sheet in `.OWGFXGroupTable` is set to `$FF`, ZScream **skips** loading that sheet:

```asm
; Example: Area uses default LW sheet 0, custom sheets 1-2
.OWGFXGroupTable_sheet0:
    db $FF  ; Don't change sheet 0
    
.OWGFXGroupTable_sheet1:
    db $12  ; Load custom sheet $12
    
.OWGFXGroupTable_sheet2:
    db $34  ; Load custom sheet $34
```

This allows selective GFX changes without re-decompressing unchanged sheets.

---

## 4. Sprite Loading System Deep Dive

### 4.1 The Three-State System

ZScream extends vanilla's 2-state sprite system to support **3 game states**:

| State | SRAM `$7EF3C5` | Trigger | Typical Use |
|-------|----------------|---------|-------------|
| **0** | `$00` | Game start | Pre-Zelda rescue (intro) |
| **1** | `$01-$02` | Uncle reached / Zelda rescued | Mid-game progression |
| **2** | `$03+` | Agahnim defeated | Post-Agahnim world |

**State Pointer Tables** (192 areas × 2 bytes each):
```
Pool_Overworld_SpritePointers_state_0_New  ; $140 bytes
Pool_Overworld_SpritePointers_state_1_New  ; $140 bytes
Pool_Overworld_SpritePointers_state_2_New  ; $140 bytes
```

Each entry is a **16-bit pointer** to a sprite list in ROM.

### 4.2 LoadOverworldSprites_Interupt Implementation

This is the **core sprite loading hook** at `$09C4C7`:

```asm
LoadOverworldSprites_Interupt:
{
    ; Get current area's size (1x1, 2x2, 2x1, 1x2)
    LDX.w $040A
    LDA.l Pool_BufferAndBuildMap16Stripes_overworldScreenSize, X : TAY
    
    ; Store X/Y boundaries for sprite loading
    LDA.w .xSize, Y : STA.w $0FB9 : STZ.w $0FB8
    LDA.w .ySize, Y : STA.w $0FBB : STZ.w $0FBA
    
    ; === DAY/NIGHT CHECK ===
    ; Check if it's night time
    JSL Oracle_ZSO_CheckIfNight
    ASL : TAY  ; Y = 0 (day) or 2 (night)
    
    REP #$30
    
    ; Calculate state table offset
    ; .phaseOffset: dw $0000 (state 0), $0140 (state 1), $0280 (state 2)
    TXA : ASL : CLC : ADC.w .phaseOffset, Y : TAX
    
    ; Get sprite pointer for (area, state)
    LDA.l Pool_Overworld_SpritePointers_state_0_New, X : STA.b $00
    
    SEP #$20
    
    ; Continue to vanilla sprite loading code...
}
```

**Key Insight**: The `ASL : TAY` after day/night check doubles the state index, allowing the `.phaseOffset` table to select between **6 possible sprite sets** (3 states × 2 times of day).

### 4.3 Day/Night Integration: The Challenge

The **original conflict** occurred because:

1. ZScream's `LoadOverworldSprites_Interupt` lives in **bank $09** (`$04C4C7`)
2. `Oracle_CheckIfNight16Bit` lives in **bank $34** (`Overworld/time_system.asm`)
3. ZScream is **outside** the `Oracle` namespace
4. The `Oracle_` prefix is **not visible** to ZScream during assembly

**Failed Approach #1**: Direct JSL call
```asm
; In ZSCustomOverworld.asm (outside Oracle namespace)
JSL Oracle_CheckIfNight16Bit  ; ❌ Label not found during assembly
```

**Failed Approach #2**: Recursive loop
```asm
; Calling Sprite_OverworldReloadAll from within the sprite loading hook
JSL.l Sprite_OverworldReloadAll  ; ❌ This calls LoadOverworldSprites again!
                                  ; Stack overflows after ~200 recursions
```

**Working Solution**: Self-contained day/night check

```asm
; In time_system.asm (inside Oracle namespace)
Oracle_ZSO_CheckIfNight:  ; Exported with Oracle_ prefix
{
    ; Self-contained logic that doesn't depend on other Oracle functions
    PHB : PHK : PLB
    
    ; Special area checks (Tail Palace, Zora Sanctuary)
    LDA $8A
    CMP.b #$2E : BEQ .tail_palace
    CMP.b #$2F : BEQ .tail_palace
    CMP.b #$1E : BEQ .zora_sanctuary
    JMP .continue_check
    
    .tail_palace
        LDA.l $7EF37A : AND #$10 : BNE .load_peacetime
        JMP .continue_check
    
    .zora_sanctuary
        LDA.l $7EF37A : AND #$20 : BNE .load_peacetime
        JMP .continue_check
    
    .load_peacetime
        ; Return original GameState
        LDA.l $7EF3C5
        PLB
        RTL
    
    .continue_check
        REP #$30
        
        ; Don't change during intro
        LDA.l $7EF3C5 : AND.w #$00FF : CMP.w #$0002 : BCC .day_time
        
        ; Check time ($7EE000 = hour)
        LDA.l $7EE000 : AND.w #$00FF
        CMP.w #$0012 : BCS .night_time  ; >= 6 PM
        CMP.w #$0006 : BCC .night_time  ; < 6 AM
    
    .day_time
        LDA.l $7EF3C5
        BRA .done
    
    .night_time
        ; Return GameState + 1 (load next state's sprites)
        LDA.l $7EF3C5 : CLC : ADC #$0001
        ; NOTE: Does NOT permanently modify SRAM!
    
    .done
        SEP #$30
        PLB
        RTL
}
```

**Why This Works**:
1. Function is **fully self-contained** - no dependencies on other Oracle functions
2. Uses only SRAM reads (`$7EF37A`, `$7EF3C5`, `$7EE000`)
3. Returns modified state **without writing to SRAM** (temporary for sprite loading only)
4. Can be called from **any** context, including outside Oracle namespace

### 4.4 Sprite Table Organization Strategy

To support day/night cycles, organize sprite pointers like this:

```
; Example: Area $03 (Lost Woods)
; State 0 (Intro): No day/night distinction
Pool_Overworld_SpritePointers_state_0_New:
    dw LostWoods_Intro_Sprites

; State 1 (Mid-game): Day sprites
Pool_Overworld_SpritePointers_state_1_New:
    dw LostWoods_Day_Sprites

; State 2 (Post-Ganon): Night sprites
Pool_Overworld_SpritePointers_state_2_New:
    dw LostWoods_Night_Sprites

; Actual sprite data in expanded space
LostWoods_Day_Sprites:
    db $05  ; 5 sprites
    db $04, $12, $34, $56  ; Moblin at (4, 12), Octorok at (34, 56)
    // ... more sprites
    
LostWoods_Night_Sprites:
    db $06  ; 6 sprites
    db $04, $12, $78, $9A  ; Stalfos at (4, 12), Poe at (78, 9A)
    // ... more sprites
```

**Advanced Technique**: Use the **same pointer** for areas that don't change:

```asm
; Area $10 (Hyrule Castle) - No day/night changes
Pool_Overworld_SpritePointers_state_1_New:
    dw HyruleCastle_AllTimes_Sprites
    
Pool_Overworld_SpritePointers_state_2_New:
    dw HyruleCastle_AllTimes_Sprites  ; Same pointer, saves ROM space
```

---

## 5. Cross-Namespace Integration

### 5.1 Understanding Asar Namespaces

Asar's `namespace` directive creates label isolation:

```asm
; In Oracle_main.asm
namespace Oracle
{
    incsrc Core/ram.asm        ; Inside namespace
    incsrc Sprites/all_sprites.asm
    incsrc Items/all_items.asm
}

; ZScream is OUTSIDE the namespace
incsrc Overworld/ZSCustomOverworld.asm  ; Outside namespace
```

**Visibility Rules**:
| Call Type | From Inside Namespace | From Outside Namespace |
|-----------|----------------------|------------------------|
| **Local Label** (`.label`) | ✅ Same function only | ✅ Same function only |
| **Function Label** (no `.`) | ✅ Visible within namespace | ❌ **NOT VISIBLE** |
| **Exported Label** (`Oracle_FunctionName`) | ✅ Visible | ✅ **VISIBLE** |

### 5.2 Calling Oracle Functions from ZScream

**❌ WRONG** - This will fail during assembly:

```asm
; In ZSCustomOverworld.asm (outside namespace)
LoadDayNightSprites:
{
    JSL CheckIfNight16Bit  ; ❌ ERROR: Label not found
    BCS .is_night
    // ...
}
```

**✅ CORRECT** - Use the exported `Oracle_` prefix:

```asm
; In ZSCustomOverworld.asm (outside namespace)
LoadDayNightSprites:
{
    JSL Oracle_CheckIfNight16Bit  ; ✅ Works! 
    BCS .is_night
        ; Load day sprites
        BRA .done
    .is_night
        ; Load night sprites
    .done
    RTL
}
```

### 5.3 Exporting Functions for ZScream

To make a function callable from ZScream, export it with the `Oracle_` prefix:

```asm
; In Core/symbols.asm or similar (inside Oracle namespace)
namespace Oracle
{
    CheckIfNight16Bit:
    {
        ; Implementation...
        RTL
    }
    
    ; Export with Oracle_ prefix for external callers
    Oracle_CheckIfNight16Bit:
        JML CheckIfNight16Bit
}
```

**Alternative**: Use `autoclean namespace` to auto-export:

```asm
autoclean namespace Oracle
{
    ; All labels automatically get Oracle_ prefix
    CheckIfNight16Bit:  ; Becomes Oracle_CheckIfNight16Bit externally
    {
        RTL
    }
}
```

### 5.4 Build Order Dependencies

Asar processes files **sequentially**. If ZScream needs Oracle labels, Oracle must be included **first**:

```asm
; In Oracle_main.asm - CORRECT ORDER

namespace Oracle
{
    ; 1. Define all Oracle functions first
    incsrc Core/symbols.asm
    incsrc Core/patches.asm
    incsrc Overworld/time_system.asm  ; Defines Oracle_ZSO_CheckIfNight
}

; 2. THEN include ZScream (which references Oracle functions)
incsrc Overworld/ZSCustomOverworld.asm
```

**❌ WRONG ORDER**:
```asm
; This will fail!
incsrc Overworld/ZSCustomOverworld.asm  ; References Oracle_ZSO_CheckIfNight
namespace Oracle
{
    incsrc Overworld/time_system.asm  ; Too late! Already referenced
}
```

### 5.5 Data Access Patterns

**Accessing Oracle RAM from ZScream**:

```asm
; Oracle defines custom WRAM
; In Core/ram.asm (inside namespace):
MenuScrollLevelV = $7E0730

; ZScream can access via full address:
LDA.l $7E0730  ; ✅ Direct memory access works
STA.w $0100

; But NOT via label:
LDA.w MenuScrollLevelV  ; ❌ Label not visible
```

**Best Practice**: Define shared memory labels in **both** locations:

```asm
; In ZSCustomOverworld.asm
OracleMenuScrollV = $7E0730  ; Local copy of label

LoadMenuState:
{
    LDA.l OracleMenuScrollV  ; ✅ Use local label
    // ...
}
```

---

## 6. Performance Considerations

### 6.1 Frame Budget Analysis

Overworld transitions have a strict **frame budget** before the player notices lag:

| Operation | Frames | Cumulative | Notes |
|-----------|--------|------------|-------|
| Camera scroll | 1-16 | 1-16 | Depends on Link's speed |
| GFX decompression (sheets 0-2) | 1-2 | 2-18 | Only if sheets changed |
| GFX staging (PrepTransMainGFX) | 1 | 3-19 | Only if sheets changed |
| Block DMA (8 blocks × 0x0600) | 8 | 11-27 | One per frame |
| Variable GFX decompress (sheets 3-6) | 2-3 | 13-30 | Only if sheets changed |
| Animated tile decompress | 1 | 14-31 | Always runs |
| Sprite loading | 1 | 15-32 | Always runs |
| **Total (worst case)** | **~32** | | Sheets 0-6 all changed |
| **Total (best case)** | **~10** | | No GFX changes |

**Optimization Strategy**: ZScream **caches** loaded sheets to avoid unnecessary decompression:

```asm
; Only decompress if sheet ID changed
LDA.w Pool_OWGFXGroupTable_sheet0, X : CMP.b #$FF : BEQ .skip
    CMP.w TransGFXModule_PriorSheets+0 : BEQ .skip
        ; Sheet changed, decompress
        TAY
        JSL.l Decomp_bg_variableLONG
    
.skip
```

**Result**: Areas using the same GFX group load **2-3 frames faster**.

### 6.2 Optional Feature Toggles

ZScream provides **38 debug flags** (`!Func...`) to disable hooks:

```asm
; In ZSCustomOverworld.asm

; Disable GFX decompression (use vanilla)
!Func00D585 = $00  ; Disables NewLoadTransAuxGFX

; Disable subscreen overlays (faster)
!Func00DA63 = $00  ; Disables ActivateSubScreen
```

**When to disable**:
- **During development**: Test if a specific hook is causing issues
- **For speed hacks**: Remove overlays, custom palettes for faster transitions
- **Compatibility testing**: Verify other mods don't conflict with specific hooks

### 6.3 DMA Optimization: Block Transfer

Instead of DMA'ing all GFX at once (expensive), ZScream spreads it across **8 frames**:

```asm
BlockGFXCheck:
{
    ; DMA one 0x0600-byte block per frame
    LDA.b $06 : DEC : STA.b $06  ; Decrement frame counter
    
    ; Calculate source address
    ; Block N = $7F0000 + (N * 0x0600)
    LDX.b $06
    LDA.l BlockSourceTable, X : STA.w $04D3
    
    ; DMA during NMI
    LDA.b #$01 : STA.w SNES.DMAChannelEnable
    
    RTL
}
```

**Trade-off**: Transitions take slightly longer, but **no frame drops**.

### 6.4 GFX Sheet Size Guidelines

Keep custom GFX sheets **under 0x0600 bytes** (decompressed) to fit in buffer:

```
Compression Ratios (typical):
- 3BPP tileset: 0x0600 bytes decompressed
- Compressed size: ~0x0300-0x0400 bytes (50-66% ratio)
- Compression time: ~15,000-20,000 cycles (~0.5 frames)
```

**Exceeding 0x0600 bytes**: Data will overflow buffer, corrupting WRAM!

---

## 7. Adding Custom Features

### 7.1 Adding a New Data Table

**Example**: Add a "music override" table to force specific songs per area.

**Step 1**: Reserve space in the data pool

```asm
; In ZSCustomOverworld.asm, Pool section
org $288000
Pool:
{
    ; ... existing tables ...
    
    ; NEW: Music override table (1 byte per area)
    .MusicOverrideTable
    db $02  ; Area $00 - Song $02
    db $FF  ; Area $01 - No override
    db $05  ; Area $02 - Song $05
    // ... 189 more entries (192 total)
}
```

**Step 2**: Create a function to read the table

```asm
pullpc  ; Save current org position

ReadMusicOverrideTable:
{
    PHB : PHK : PLB  ; Use local bank
    
    LDA.b $8A : TAX  ; Current area
    LDA.w Pool_MusicOverrideTable, X
    CMP.b #$FF : BEQ .noOverride
        ; Store in music queue
        STA.w $0132
    
    .noOverride
    PLB
    RTL
}

pushpc  ; Restore org position
```

**Step 3**: Hook into existing code

```asm
; Hook the music loading routine
org $0283EE  ; PreOverworld_LoadProperties
    JSL ReadMusicOverrideTable
    NOP : NOP  ; Fill unused space
```

**Step 4**: Update ZScream data in editor

In your level editor (ZScream tool):
```python
# Add UI for music override
music_override_table = [0xFF] * 192  # Default: no override
music_override_table[0x00] = 0x02    # Area 0: Song 2
music_override_table[0x02] = 0x05    # Area 2: Song 5

# Save to ROM at $288000 + offset
```

### 7.2 Adding a New Hook

**Example**: Trigger custom code when entering specific areas.

**Step 1**: Find injection point

Use a debugger to find where vanilla code runs during area entry. Example: `$02AB08` (`Overworld_LoadMapProperties`).

**Step 2**: Create your custom function

```asm
pullpc

OnAreaEntry_Custom:
{
    ; Save context
    PHA : PHX : PHY
    
    ; Check if this is area $03 (Lost Woods)
    LDA.b $8A : CMP.b #$03 : BNE .notLostWoods
        ; Trigger custom event
        LDA.b #$01 : STA.l $7EF400  ; Set custom flag
        
        ; Play custom music
        LDA.b #$10 : STA.w $0132
    
    .notLostWoods
    
    ; Restore context
    PLY : PLX : PLA
    RTL
}

pushpc
```

**Step 3**: Hook into vanilla code

```asm
org $02AB08
    JSL OnAreaEntry_Custom
    NOP  ; Fill unused bytes if needed
```

**Step 4**: Add debug toggle (optional)

```asm
; At top of file with other !Func... flags
!EnableAreaEntryHook = $01

; In hook
if !EnableAreaEntryHook == $01
org $02AB08
    JSL OnAreaEntry_Custom
    NOP
else
org $02AB08
    ; Original code
    db $A5, $8A, $29
endif
```

### 7.3 Modifying Transition Behavior

**Example**: Add diagonal screen transitions.

**Challenge**: Vanilla only supports 4 directions (up, down, left, right). ZScream uses `.ByScreen1-4` tables for these.

**Solution**: Create a 5th table for diagonal data.

**Step 1**: Add diagonal data table

```asm
Pool:
{
    ; ... existing tables ...
    
    ; NEW: Diagonal transition table
    ; Format: %UDLR where U=up-right, D=down-right, L=down-left, R=up-left
    .DiagonalTransitionTable
    db %0000  ; Area $00 - No diagonal transitions
    db %0001  ; Area $01 - Up-left allowed
    db %1100  ; Area $02 - Up-right, down-right allowed
    // ... 189 more entries
}
```

**Step 2**: Modify transition handler

```asm
; Hook into OverworldHandleTransitions ($02A9C4)
org $02A9C4
    JML HandleDiagonalTransitions

pullpc
HandleDiagonalTransitions:
{
    ; Check if player is moving diagonally
    LDA.b $26 : BEQ .notMoving  ; X velocity
    LDA.b $27 : BEQ .notMoving  ; Y velocity
    
    ; Both non-zero = diagonal movement
    JSR.w CheckDiagonalAllowed
    BCS .allowDiagonal
        ; Not allowed, fall back to vanilla
        JML $02A9C8  ; Original code
    
    .allowDiagonal
        ; Calculate new screen ID for diagonal
        JSR.w CalculateDiagonalScreen
        STA.b $8A  ; Set new area
        
        ; Trigger transition
        JML $02AA00  ; Continue transition code
    
    .notMoving
        JML $02A9C8  ; Original code
}

CheckDiagonalAllowed:
{
    LDA.b $8A : TAX
    LDA.l Pool_DiagonalTransitionTable, X
    ; Check appropriate bit based on direction
    // ... implementation ...
    RTS
}

CalculateDiagonalScreen:
{
    ; Calculate screen ID for diagonal move
    // ... implementation ...
    RTS
}
pushpc
```

---

## 8. Debugging & Troubleshooting

### 8.1 Common Issues & Solutions

#### Issue: "Screen turns black during transition"

**Cause**: GFX decompression exceeded buffer size.

**Debug Steps**:
1. Check `Pool_OWGFXGroupTable` for the affected area
2. Measure compressed size of each sheet (should be < 0x0400 bytes)
3. Check for sheet ID `> $7F` (invalid)

**Solution**:
```asm
; Add bounds checking to decompression
LDA.w Pool_OWGFXGroupTable_sheet0, X
CMP.b #$80 : BCS .invalidSheet  ; Sheet ID too high
CMP.b #$FF : BEQ .skipSheet     ; Skip marker
    ; Valid sheet, decompress
    TAY
    JSL.l Decomp_bg_variableLONG
```

#### Issue: "Sprites don't load in new area"

**Cause**: Sprite pointer table points to invalid address.

**Debug Steps**:
1. Check `Pool_Overworld_SpritePointers_state_X_New` for the area
2. Verify pointer points to valid ROM address (PC `$140000`+)
3. Check sprite list format (count byte, then sprite data)

**Solution**:
```asm
; Add validation to sprite loader
LDA.l Pool_Overworld_SpritePointers_state_0_New, X : STA.b $00
LDA.l Pool_Overworld_SpritePointers_state_0_New+1, X : STA.b $01

; Validate pointer is in ROM range ($00-$7F banks)
AND.b #$7F : CMP.b #$40 : BCC .invalidPointer
    ; Valid, continue
    BRA .loadSprites

.invalidPointer
    ; Use default sprite list
    LDA.w #DefaultSpriteList : STA.b $00
    LDA.w #DefaultSpriteList>>8 : STA.b $01
```

#### Issue: "Day/night sprites don't switch"

**Cause**: `Oracle_ZSO_CheckIfNight` not returning correct value.

**Debug Steps**:
1. Check `$7EE000` (current hour) in RAM viewer
2. Verify `$7EF3C5` (GameState) is >= $02
3. Check sprite tables for states 2 and 3

**Solution**:
```asm
; Add debug output to ZSO_CheckIfNight
.night_time
    ; Log to unused RAM for debugging
    LDA.l $7EE000 : STA.l $7F0000  ; Hour
    LDA.l $7EF3C5 : STA.l $7F0001  ; Original state
    
    ; Return state + 1
    CLC : ADC #$0001
    STA.l $7F0002  ; Modified state
```

### 8.2 Emulator Debugging Tools

**Mesen-S Debugger**:
```
1. Set breakpoint: $09C4C7 (LoadOverworldSprites_Interupt)
2. Watch expressions:
   - $8A (current area)
   - $7EF3C5 (GameState)
   - $7EE000 (current hour)
   - $00-$01 (sprite pointer)
3. Memory viewer: $288000 (ZScream data pool)
```

**bsnes-plus Debugger**:
```
1. Memory breakpoint: Write to $8A (area change)
2. Trace logger: Enable, filter for "JSL", search for ZScream functions
3. VRAM viewer: Check tile uploads after transition
```

### 8.3 Assertion Failures

ZScream uses `assert` directives to catch data overflow:

```asm
assert pc() <= $289938  ; Must not exceed data pool boundary
```

**If this fails**:
```
Error: Assertion failed at ZSCustomOverworld.asm line 1393
  PC: $289A00 (exceeds $289938 by $C8 bytes)
```

**Solution**: Reduce data table sizes or move tables to different bank.

### 8.4 Build System Troubleshooting

**Issue**: "Label not found: Oracle_ZSO_CheckIfNight"

**Cause**: Build order issue - ZScream assembled before Oracle functions defined.

**Solution**: Check `Oracle_main.asm` include order:
```asm
; CORRECT:
namespace Oracle
{
    incsrc Overworld/time_system.asm  ; Defines label
}
incsrc Overworld/ZSCustomOverworld.asm  ; Uses label

; WRONG:
incsrc Overworld/ZSCustomOverworld.asm  ; Uses label
namespace Oracle
{
    incsrc Overworld/time_system.asm  ; Too late!
}
```

---

## 9. Future Enhancement Possibilities

### 9.1 Multi-Layer Backgrounds

**Concept**: Support BG3 parallax scrolling (like mountains in distance).

**Implementation**:
- Add `.BG3LayerTable` to data pool
- Hook `Overworld_OperateCameraScroll` to update BG3 scroll
- Modify `InitTilesets` to load BG3 graphics

**Challenges**:
- SNES Mode 1 supports BG1/BG2/BG3, but subscreen uses BG1
- Would need to disable overlays when BG3 parallax active

### 9.2 Weather System Integration

**Concept**: Dynamic weather per area (rain, snow, wind).

**Implementation**:
- Add `.WeatherTable` (rain intensity, snow, wind direction)
- Extend `RainAnimation` hook to support multiple weather types
- Add particle systems for snow/leaves

**Challenges**:
- Performance impact (60 particles @ 60 FPS = 3600 calcs/sec)
- Would need sprite optimization

### 9.3 Area-Specific Camera Boundaries

**Concept**: Custom camera scroll limits per area (like Master Sword grove).

**Implementation**:
- Add `.CameraBoundsTable` (4 bytes per area: top, bottom, left, right)
- Hook camera scroll functions to read table
- Apply limits before updating `$E0-$E7` scroll positions

**Already Partially Implemented**: `OWCameraBoundsS/E` at `$0716/$0718`.

---

## 10. Reference: Complete Hook List

| Address | Bank | Function | Purpose |
|---------|------|----------|---------|
| `$00D585` | $00 | `Decomp_bg_variableLONG` | Decompress variable GFX sheets |
| `$00D673` | $00 | `NewLoadTransAuxGFX` | Load variable sheets 3-6 |
| `$00D8D5` | $00 | `AnimateMirrorWarp_DecompressAnimatedTiles` | Load animated tiles on mirror warp |
| `$00DA63` | $00 | `AnimateMirrorWarp_LoadSubscreen` | Enable/disable subscreen overlay |
| `$00E221` | $00 | `InitTilesetsLongCalls` | Load GFX groups from custom tables |
| `$00EEBB` | $00 | `Palette_InitWhiteFilter_Interupt` | Zero BG color for pyramid area |
| `$00FF7C` | $00 | `MirrorWarp_BuildDewavingHDMATable_Interupt` | BG scrolling for pyramid |
| `$0283EE` | $02 | `PreOverworld_LoadProperties_Interupt` | Load area properties on dungeon exit |
| `$028632` | $02 | `Credits_LoadScene_Overworld_PrepGFX_Interupt` | Load GFX for credits scenes |
| `$029A37` | $02 | `Spotlight_ConfigureTableAndControl_Interupt` | Fixed color setup |
| `$02A4CD` | $02 | `RainAnimation` | Rain overlay animation |
| `$02A9C4` | $02 | `OverworldHandleTransitions` | Screen transition logic |
| `$02ABBE` | $02 | `NewOverworld_FinishTransGfx` | Multi-frame GFX loading |
| `$02AF58` | $02 | `Overworld_ReloadSubscreenOverlay_Interupt` | Load subscreen overlays |
| `$02B391` | $02 | `MirrorWarp_LoadSpritesAndColors_Interupt` | Pyramid warp special handling |
| `$02BC44` | $02 | `Overworld_OperateCameraScroll_Interupt` | Camera scroll control |
| `$02C02D` | $02 | `OverworldScrollTransition_Interupt` | BG alignment during scroll |
| `$02C692` | $02 | `Overworld_LoadAreaPalettes` | Load custom palettes |
| `$09C4C7` | $09 | `LoadOverworldSprites_Interupt` | Load sprites with day/night support |
| `$0AB8F5` | $0A | Bird travel animated tile reload | Load animated tiles after bird |
| `$0ABC5A` | $0A | Map close animated tile reload | Load animated tiles after map |
| `$0BFEB6` | $0B | `Overworld_SetFixedColorAndScroll` | Set overlay colors and scroll |
| `$0ED627` | $0E | Custom BG color on warp | Set transparent color |
| `$0ED8AE` | $0E | Reset area color after flash | Restore BG color post-warp |

---

## Appendix A: Memory Map Quick Reference

```
=== WRAM ===
$04CB[8] - TransGFXModule_PriorSheets (cached GFX IDs)
$04D3[2] - NewNMITarget1 (VRAM target for sheet 1)
$04D5[2] - NewNMISource1 (Source for sheet 1)
$04D7[2] - NewNMICount1 (Size for sheet 1)
$04D9[2] - NewNMITarget2 (VRAM target for sheet 2)
$04DB[2] - NewNMISource2 (Source for sheet 2)
$04DD[2] - NewNMICount2 (Size for sheet 2)
$0716[2] - OWCameraBoundsS (Camera south/left bounds)
$0718[2] - OWCameraBoundsE (Camera east/right bounds)
$0CF3[1] - TransGFXModuleFrame (GFX loading frame counter)
$0FC0[1] - AnimatedTileGFXSet (Current animated set)

=== SRAM ===
$7EE000[1] - Current hour (0-23)
$7EF3C5[1] - GameState (0=intro, 1-2=midgame, 3+=postgame)
$7EF37A[1] - Crystals (dungeon completion flags)

=== ROM ===
$288000-$289938 - ZScream data pool (Bank $28)
$289940+ - ZScream functions
```

---

## Appendix B: Sprite Pointer Format

Each sprite list in ROM follows this format:

```
[COUNT] [SPRITE_0] [SPRITE_1] ... [SPRITE_N]
  └─ 1 byte      └──────── 4 bytes each ────────┘

Sprite Entry (4 bytes):
  Byte 0: Y coordinate (high 4 bits) + X coordinate (high 4 bits)
  Byte 1: Y coordinate (low 8 bits)
  Byte 2: X coordinate (low 8 bits)
  Byte 3: Sprite ID
  
Example:
  db $03           ; 3 sprites
  db $00, $12, $34, $05  ; Sprite $05 at ($034, $012)
  db $01, $56, $78, $0A  ; Sprite $0A at ($178, $156)
  db $00, $9A, $BC, $12  ; Sprite $12 at ($0BC, $09A)
```

---

**End of Advanced Documentation**

For basic ZScream usage, see `ZSCustomOverworld.md`.  
For general overworld documentation, see `Overworld.md`.  
For troubleshooting ALTTP issues, see `Docs/General/Troubleshooting.md` (Task 4).
