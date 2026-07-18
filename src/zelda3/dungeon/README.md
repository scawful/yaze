# Zelda3 Dungeon Core Module (`src/zelda3/dungeon`)

This directory contains the core business logic, data structures, and rendering algorithms for the `The Legend of Zelda: A Link to the Past` dungeon system. It serves as the backend for the editor UI found in `src/app/editor/dungeon`.

## Current Status (July 2026)

**Core System: Stable** with focused regression coverage around object rendering, room persistence, object tile editing, and editor/save plumbing.

The object rendering pipeline has been validated against the ALTTP disassembly:
- Type 1/2/3 object detection and parsing ✅
- Index calculation for all object tables ✅
- Draw routine mapping (130+ routines) ✅
- BothBG flag propagation ✅
- Tile count lookup tables ✅

**Known Minor Issues**: Some specific objects (vertical rails, doors, certain
edge patterns) may have visual discrepancies that require individual
verification against the game. The global pit-damage table now supports
fixed-capacity membership edits while preserving its protected ROM region for
no-op saves; pushable blocks round-trip through a room-aware encoder but remain
capped to the existing vanilla table unless a future repointing pass expands
it. Successful block-table saves rebase loaded blocks to their compacted table
slots so later no-op saves remain stable, and transaction rollback restores
those identities with the ROM. The encoder fails closed when a dirty room's
blocks were not loaded and rejects deleting the final global block entry
because vanilla's do-while runtime scan cannot safely use a zero-byte limit.

## Architecture Overview

The module is designed to replicate the SNES game engine's logic for loading, parsing, and rendering dungeon rooms, while providing high-level editing capabilities.

### Core Domain Entities

*   **`Room`**: The aggregate root representing a single dungeon room (IDs 0-295). It manages:
    *   **`RoomObject`** list (walls, floors, interactables).
    *   **`Sprite`** list (enemies, NPCs).
    *   **Header data** (collision, palette, blockset).
    *   **State** (chests, doors, etc.).
*   **`RoomObject`**: Represents a tile-based object. It handles the complex bit-packing of the three object subtypes used by the SNES engine:
    *   **Type 1** (0x00-0xFF): Standard scalable objects.
    *   **Type 2** (0x100-0x1FF): Complex/Preset objects.
    *   **Type 3** (0xF80-0xFFF): Special objects and paths.
*   **`RoomLayout`**: Represents the structural layout of a room (walls and floor) separately from the mutable object list.

### The Rendering Pipeline

This module implements a faithful recreation of the ALTTP dungeon rendering engine to ensure what you see in the editor matches the game.

```mermaid
graph TD
    Rom[ROM Data] --> Parser[ObjectParser]
    Parser -->|Tile Data| Drawer[ObjectDrawer]
    
    subgraph "Draw Routine System"
        Drawer --> RoutineLookup{Routine ID}
        RoutineLookup -->|ID 0-4| Rightwards[Rightwards Routines]
        RoutineLookup -->|ID 7-15| Downwards[Downwards Routines]
        RoutineLookup -->|ID 5-6| Diagonal[Diagonal Routines]
        RoutineLookup -->|ID 14-19| Corner[Corner Routines]
        RoutineLookup -->|ID 39+| Special[Special Routines]
    end
    
    Rightwards --> Bitmap[BackgroundBuffer]
    Downwards --> Bitmap
    
    Bitmap --> LayerMgr[RoomLayerManager]
    LayerMgr -->|Composite| FinalImage
```

*   **`ObjectParser`**: Reads raw tile data directly from the ROM's graphics banks based on object IDs.
*   **`ObjectDrawer`**: The core rendering engine. It maps Object IDs to specific "Draw Routines".
*   **`draw_routines/`**: A collection of pure functions that mirror the ASM routines from `bank_01.asm`. They calculate tile positions based on object size and orientation.
    *   *Example*: `DrawRightwards2x2_1to15or32` draws a 2x2 block repeated N times horizontally.

### Editing Subsystems

*   **`DungeonEditorSystem`**: The high-level facade for the application layer. It manages the editing session, undo/redo history, room caching, external-room binding, and persistence for objects, sprites, room headers, torches, pushable blocks, collision, chests, pot items, and dungeon entrances.
*   **`DungeonObjectEditor`**: Specialized logic for manipulating `RoomObject` entities (Insert, Delete, Move, Resize). Handles grid snapping and collision checks.
*   **`ObjectTileEditor`**: Captures rendered 8x8 tile layouts for standard and custom objects, builds preview/atlas bitmaps, and writes tile edits back to ROM or custom `.bin` files.
*   **`ObjectDimensionTable`**: Provides hit-testing bounds for objects. This is distinct from the visual rendering size and is derived from ROM data tables.
*   **`ObjectTemplateManager`**: Allows creating and instantiating groups of objects (templates).

## Key Files & Components

### Data & State
*   **`dungeon_rom_addresses.h`**: Central registry of ROM memory offsets.
*   **`dungeon_state.h` / `editor_dungeon_state.h`**: Abstracts game state (e.g., "Is chest X open?") to allow the editor to simulate different gameplay scenarios.
*   **`room_entrance.h`**: Defines entrance/exit data structures.

### Rendering
*   **`room_layer_manager.h`**: Controls the visibility and blending of the SNES background layers (BG1, BG2, BG3). Handles complex "Layer Merge" logic used in the game.
*   **`palette_debug.cc/h`**: Instrumentation tools for diagnosing palette loading and application issues.

### Logic
*   **`dungeon_validator.cc/h`**: Enforces game limits (e.g., max sprites per room, max chests).
*   **`dungeon_object_registry.cc/h`**: A registry for object names and metadata.

### Focused Test Coverage
*   **`dungeon_save_test.cc`**: Save-path coverage for objects, sprites, torches, pushable blocks, chests, pot items, collision preconditions, entrances, and pit-region preservation.
*   **`dungeon_editor_system_test.cc`**: Managed-room and external-room persistence coverage for `DungeonEditorSystem`.
*   **`object_tile_editor_test.cc`**: Object tile layout, writeback roundtrips, palette-sensitive preview generation, and custom object serialization.

## Comparison: Backend vs. Frontend

| Feature | `src/zelda3/dungeon` (Backend) | `src/app/editor/dungeon` (Frontend) |
| :--- | :--- | :--- |
| **Focus** | Logic, Data, Rendering Algorithms | UI, Input, State Coordination |
| **Dependencies** | `rom/`, `app/gfx/` (Base types) | `zelda3/dungeon`, `imgui` |
| **Rendering** | Draws tiles to memory buffers | Displays buffers on Canvas, handles Pan/Zoom |
| **Input** | N/A | Handles Mouse/Keyboard events |
| **State** | Holds the "Truth" (Room data) | Manages Selection state, Tool modes |

## Areas for Improvement

1.  **Unified Dimension Logic**:
    Currently, `ObjectDimensionTable` (hitboxes) and `draw_routines` (visuals) maintain separate logic for object sizes. This can lead to desyncs where the click area doesn't match the drawn object.
    *Action*: Consolidate logic so `ObjectDimensionTable` queries the draw routines or vice-versa.

2.  **Hardcoded ROM Offsets**:
    `dungeon_rom_addresses.h` contains hardcoded offsets for the US ROM.
    *Action*: Move these to a version-aware configuration system to support JP/EU ROMs.

3.  **Sprite Rendering**:
    Dungeon canvas sprites now use the room's combined sprite graphics buffer to draw static vanilla tile previews when possible, with simple boxes retained as a fallback for sprites with missing graphics or palette coverage.
    *Action*: Extend this into a dedicated sprite renderer with full runtime OAM/state coverage and project-editable custom layouts.

4.  **Legacy Code**:
    Some classes (e.g., in `room.cc`) contain legacy loading logic that might duplicate the newer `ObjectParser`.
    *Action*: Audit and deprecate old loading paths in favor of the robust parser.

5.  **Pit/Block Persistence**:
    The pit-damage table preserves its existing ROM blob after pointer
    validation unless the fixed-capacity workbench membership editor marks the
    table dirty. Pushable blocks are editable and save through a room-aware
    encoder, including add/delete/move cases, but do not yet repoint or expand
    the four vanilla data regions.
    *Action*: Treat pit/block repointing and capacity expansion as separate
    ROM-layout features.
