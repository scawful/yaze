# YAZE Architecture Documentation

This directory contains detailed architectural documentation for the YAZE (Yet Another Zelda3 Editor) codebase. These documents describe the design patterns, component interactions, and best practices used throughout the project.

## Core Architecture Guides

### Graphics System
- **[graphics_system_architecture.md](graphics_system_architecture.md)** - Complete guide to the graphics rendering pipeline
  - Arena resource manager for 223 graphics sheets
  - Bitmap class and texture management
  - LC-LZ2 compression/decompression pipeline
  - Rendering workflow from ROM loading to display
  - Canvas interactions and drawing operations
  - Best practices for graphics modifications

### Editors

#### Dungeon Editor
- **[dungeon_editor_system.md](dungeon_editor_system.md)** - Architecture of the dungeon room editor
  - Component-based design (DungeonEditorV2, DungeonObjectEditor, DungeonCanvasViewer)
  - Object editing workflow (insert, delete, move, resize, layer operations)
  - Coordinate systems and conversion methods
  - Best practices for extending editor modes
  - Contributing guidelines for new features

#### Overworld Editor
- **[overworld_editor_system.md](overworld_editor_system.md)** - Architecture of the overworld map editor
  - Overworld system structure (Light World, Dark World, Special Areas)
  - Map properties and large map configuration
  - Entity handling (sprites, entrances, exits, items)
  - Deferred texture loading for performance
  - ZSCustomOverworld integration

### Data Structures & Persistence

- **[overworld_map_data.md](overworld_map_data.md)** - Overworld map internal structure
  - OverworldMap data model (tiles, graphics, properties)
  - ZSCustomOverworld custom properties and storage
  - Loading and saving process
  - Multi-area map configuration
  - Overlay system for interactive map layers

- **[room_data_persistence.md](room_data_persistence.md)** - Dungeon room loading and saving
  - ROM pointer table system
  - Room decompression and object parsing
  - Multithreaded bulk loading (up to 8 threads)
  - Room size calculation for safe editing
  - Repointing logic for data overflow
  - Bank boundary considerations

### Systems & Utilities

- **[undo_redo_system.md](undo_redo_system.md)** - Undo/redo architecture for editors
  - Snapshot-based undo implementation
  - DungeonObjectEditor undo stack
  - DungeonEditorSystem coordinator integration
  - Batch operation handling
  - Best practices for state management

- **[zscustomoverworld_integration.md](zscustomoverworld_integration.md)** - ZSCustomOverworld v3 support
  - Multi-area map sizing (1x1, 2x1, 1x2, 2x2)
  - Custom graphics and palette per-map
  - Visual effects (mosaic, subscreen overlay)
  - ASM patching and ROM version detection
  - Feature-specific UI adaptation

## Quick Reference by Component

### Graphics (`src/app/gfx/`)
- See: [graphics_system_architecture.md](graphics_system_architecture.md)
- Key Classes: Arena, Bitmap, SnesPalette, IRenderer
- Key Files: `resource/arena.h`, `core/bitmap.h`, `util/compression.h`

### Dungeon Editor (`src/app/editor/dungeon/`, `src/zelda3/dungeon/`)
- See: [dungeon_editor_system.md](dungeon_editor_system.md), [room_data_persistence.md](room_data_persistence.md)
- Key Classes: DungeonEditorV2, DungeonObjectEditor, Room, DungeonRoomLoader
- Key Files: `dungeon_editor_v2.h`, `dungeon_object_editor.h`, `room.h`

### Overworld Editor (`src/app/editor/overworld/`, `src/zelda3/overworld/`)
- See: [overworld_editor_system.md](overworld_editor_system.md), [overworld_map_data.md](overworld_map_data.md)
- Key Classes: OverworldEditor, Overworld, OverworldMap, OverworldEntityRenderer
- Key Files: `overworld_editor.h`, `overworld.h`, `overworld_map.h`

### Undo/Redo
- See: [undo_redo_system.md](undo_redo_system.md)
- Key Classes: DungeonObjectEditor (UndoPoint structure)
- Key Files: `dungeon_object_editor.h`

### ZSCustomOverworld
- See: [zscustomoverworld_integration.md](zscustomoverworld_integration.md), [overworld_map_data.md](overworld_map_data.md)
- Key Classes: OverworldMap, Overworld, OverworldVersionHelper
- Key Files: `overworld.cc`, `overworld_map.cc`, `overworld_version_helper.h`

## Design Patterns Used

### 1. Modular/Component-Based Design
Large systems are decomposed into smaller, single-responsibility classes:
- Example: DungeonEditorV2 (coordinator) → DungeonRoomLoader, DungeonCanvasViewer, DungeonObjectEditor (components)
- See: [dungeon_editor_system.md](dungeon_editor_system.md#high-level-overview)

### 2. Callback-Based Communication
Components communicate without circular dependencies:
- Example: ObjectEditorCard receives callbacks from DungeonObjectEditor
- See: [dungeon_editor_system.md](dungeon_editor_system.md#best-practices-for-contributors)

### 3. Singleton Pattern
Global resource management via Arena:
- Example: `gfx::Arena::Get()` for all graphics sheet access
- See: [graphics_system_architecture.md](graphics_system_architecture.md#core-components)

### 4. Progressive/Deferred Loading
Heavy operations performed asynchronously to maintain responsiveness:
- Example: Graphics sheets loaded on-demand with priority queue
- Example: Overworld map textures created when visible
- See: [overworld_editor_system.md](overworld_editor_system.md#deferred-loading)

### 5. Snapshot-Based Undo/Redo
State snapshots before destructive operations:
- Example: UndoPoint structure captures entire room object state
- See: [undo_redo_system.md](undo_redo_system.md)

## Contributing Guidelines

When adding new functionality:

1. **Follow Existing Patterns**: Use component-based design, callbacks, and RAII principles
2. **Update Documentation**: Add architectural notes to relevant documents
3. **Write Tests**: Create unit tests in `test/unit/` for new components
4. **Use Proper Error Handling**: Employ `absl::Status` and `absl::StatusOr<T>`
5. **Coordinate with State**: Use Arena/Singleton patterns for shared state
6. **Enable Undo/Redo**: Snapshot state before destructive operations
7. **Defer Heavy Work**: Use texture queues and async loading for performance

For detailed guidelines, see the **Best Practices** sections in individual architecture documents.

## Related Documents

- **[../../CLAUDE.md](../../CLAUDE.md)** - Project overview and development guidelines
- **[../../README.md](../../README.md)** - Project introduction
- **[../release-checklist.md](../release-checklist.md)** - Release process documentation

## Architecture Evolution

This architecture reflects the project's maturity at the time of documentation. Key evolution points:

- **DungeonEditorV2**: Replacement for older monolithic DungeonEditor with proper component delegation
- **Arena System**: Centralized graphics resource management replacing scattered SDL operations
- **ZSCustomOverworld v3 Support**: Extended OverworldMap and Overworld to support expanded ROM features
- **Progressive Loading**: Deferred texture creation to prevent UI freezes during large ROM loads

## Status and Maintenance

All architecture documents are maintained alongside the code:
- Documents are reviewed during code reviews
- Architecture changes require documentation updates
- Status field indicates completeness (Draft/In Progress/Complete)
- Last updated timestamp indicates freshness

For questions about architecture decisions, consult:
1. Relevant architecture document
2. Source code comments
3. Commit history for design rationale
