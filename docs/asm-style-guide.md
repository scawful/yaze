# Asm Style Guide

65816 Assembly is the assembly language used by the Super Nintendo Entertainment System (SNES) and its Ricoh 5A22 processor. This style guide provides conventions and best practices for writing 65816 assembly code in the context of the yaze project. Following these guidelines will help maintain consistency and readability across the codebase.

This guide is based primarily on the [Oracle of Secrets](https://github.com/scawful/Oracle-of-Secrets) codebase and is meant for the [Asar](https://github.com/RPGHacker/asar) assembler and derives influence from the [Asar 1.9 Manual](https://rpghacker.github.io/asar/asar_19/manual/).

Custom assembly code applied to the game should be included through the [yaze.asm](../assets/asm/yaze.asm) file. This file can be applied to the ROM by the editor using the Asar library or included into a projects codebase for use with the Asar assembler.

## Table of Contents

- [File Structure](#file-structure)
- [Labels and Symbols](#labels-and-symbols)
- [Comments](#comments)
- [Directives](#directives)
- [Instructions](#instructions)
- [Macros](#macros)
- [Loops and Branching](#loops-and-branching)
- [Data Structures](#data-structures)
- [Code Organization](#code-organization)
- [Custom Code](#custom-code)


## File Structure

- **File Extension**: Use `.asm` as the file extension for 65816 assembly files.
- **Header Comments**: Include a header comment at the beginning of each file describing its purpose and the author. 

Example:

```asm
; =========================================================
; File: my_file.asm
; Purpose: [Brief description of the file’s functionality]
; Author: [Your Name]
; =========================================================
```

- **Section Headers**: Use clear and consistent section headers to divide code into logical blocks. Each major section (e.g., sprite properties, main logic, subroutines) should start with a delineated header.

Example:

```asm
; =========================================================
; Minecart Sprite Properties
; =========================================================
```

- **Macro Definitions and Includes**: Place macros and include directives at the beginning of the file to keep them organized and easily accessible.

## Labels and Symbols

- **Naming Conventions**:
  - **Global Labels**: Use descriptive names in `PascalCase` for global labels (e.g., `Sprite_Minecart_Main`).
  - **Local Labels**: Prefix local labels with a dot (`.`) to indicate their limited scope (e.g., `.check_direction`).
  - **Constants and Flags**: Use `ALL_CAPS_WITH_UNDERSCORES` for constants and flags (e.g., `!MINECART_SPEED`, `!HARVESTING_FLAG`).
  - **Variables**: Use `CamelCase` for variable names to maintain readability (e.g., `LinkInCart`, `SpriteDirection`).

- **Alignment**: Align labels to the left margin for better readability. Indent instructions and comments to separate them from labels.

Example:

```asm
Sprite_Minecart_Main:
{
    JSR HandleTileDirections
    JSR HandleDynamicSwitchTileDirections
    RTS
}
```


## Comments

- **Purpose**: Comments should explain why the code exists and what it is intended to do, especially for complex logic.
- **Placement**: 
  - Comments can be placed above the code block they describe for longer explanations.
  - Inline comments can be used for single lines of code where the purpose might not be immediately clear.
- **Clarity**: Avoid stating the obvious. Focus on explaining the logic rather than restating the code.

Example:

```asm
LDA $22 : SEC : SBC $3F : STA $31   ; Adjust X position for camera movement
```

## Directives

- **Organization**: Use `%macro`, `include`, and other Asar directives in a structured manner, keeping related directives grouped together.
- **Usage**: Ensure all directives are used consistently throughout the codebase, following the naming conventions and formatting rules established.

Example:

```asm
%macro InitMovement
    LDA.b $22 : STA.b $3F
    LDA.b $23 : STA.b $41
    LDA.b $20 : STA.b $3E
    LDA.b $21 : STA.b $40
endmacro
```

## Instructions

- **Single Line Instructions**: Combine multiple instructions on a single line using colons (`:`) where appropriate for related operations.
- **Separation**: Use line breaks to separate distinct sections of code logically, improving readability.
- **Optimization**: Always consider the most efficient instruction for the task at hand, especially in performance-critical sections.

Example:

```asm
LDA #$01 : STA !LinkInCart  ; Set Link in cart flag
```

## Macros

- **Naming**: Use `PascalCase` for macro names, with the first letter of each word capitalized (e.g., `InitMovement`, `MoveCart`).
- **Parameters**: Clearly define and document parameters within macros to ensure they are used correctly.
- **Reuse**: Encourage the reuse of macros to avoid code duplication and simplify maintenance.

Example:

```asm
%macro HandlePlayerCamera
    LDA $22 : SEC : SBC $3F : STA $31
    LDA $20 : SEC : SBC $3E : STA $30
    JSL Link_HandleMovingAnimation_FullLongEntry
    JSL HandleIndoorCameraAndDoors
    RTS
endmacro
```

## Loops and Branching

- **Branch Labels**: Use meaningful names for branch labels, prefixed with a dot (`.`) for local branches.
- **Optimization**: Minimize the number of instructions within loops and branches to improve performance.

Example:

```asm
.loop_start
    LDA $00 : CMP #$10 : BEQ .end_loop
    INC $00
    BRA .loop_start
.end_loop
    RTS
```

## Data Structures

- **Alignment**: Align data tables and structures clearly, and use comments to describe the purpose and layout of each.
- **Access**: Ensure that data structures are accessed consistently, with clear boundaries between read and write operations.

Example:

```asm
.DirectionTileLookup
{
    db $02, $00, $04, $00 ; North
    db $00, $00, $03, $01 ; East
    db $00, $02, $00, $04 ; South
    db $03, $01, $00, $00 ; West
}
```

- **Structs**: Use structs to group related data together, improving readability and maintainability.

Example:

```asm
struct AncillaAdd_HookshotData $099AF8
  .speed_y: skip 4
  .speed_x: skip 4
  .offset_y: skip 8
  .offset_x: skip 8
endstruct

...

AncillaAdd_Hookshot:
.speed_y
  #_099AF8: db -64 ; up
  #_099AF9: db  64 ; down
  #_099AFA: db   0 ; left
  #_099AFB: db   0 ; right
.speed_x
  #_099AFC: db   0 ; up
  #_099AFD: db   0 ; down
  #_099AFE: db -64 ; left
  #_099AFF: db  64 ; right
.offset_y
  #_099B00: dw   4 ; up
  #_099B02: dw  20 ; down
  #_099B04: dw   8 ; left
  #_099B06: dw   8 ; right
.offset_x
  #_099B08: dw   0 ; up
  #_099B0A: dw   0 ; down
  #_099B0C: dw  -4 ; left
  #_099B0E: dw  11 ; right
```

## Code Organization

- **Logical Grouping**: Organize code into logical sections, with related routines and macros grouped together.
- **Separation of Concerns**: Ensure that each section of code is responsible for a specific task or set of related tasks, avoiding tightly coupled code.
- **Modularity**: Write code in a modular way, making it easier to reuse and maintain.

Example:

```asm
; =========================================================
; Minecart Sprite Logic
; =========================================================
Sprite_Minecart_Main:
{
    JSR HandleTileDirections
    JSR HandleDynamicSwitchTileDirections
    RTS
}
```

## Custom Code

- **Integration**: Include custom assembly code in the `yaze.asm` file to ensure it is applied correctly to the ROM. The module should include a define and conditional statement to allow users to disable the module if needed. 

Example:

```asm
!YAZE_CUSTOM_MOSAIC = 1

if !YAZE_CUSTOM_MOSAIC != 0
  incsrc "mosaic_change.asm"
endif
```