# Graphics System Improvement Plan

**Status**: Proposal
**Last Updated**: 2025-11-21

This document outlines a plan for a robust, professional-grade graphics editing system in YAZE.

## 1. Unified Graphics Editor

**Goal**: Consolidate scattered editing tools into a cohesive workspace.

*   **Workspace**: A main window with dockable panels:
    *   **Canvas**: The main drawing area.
    *   **Sheet Browser**: Thumbnail list of all 223 sheets.
    *   **Palette Editor**: Advanced color management.
    *   **Tile Inspector**: Properties of the selected tile (flip, priority, raw value).
*   **Mode Switching**: Toggle between "Pixel Mode" (raw sheet editing) and "Assembly Mode" (viewing sprites/objects as they appear in-game).

## 2. Advanced Palette Management

**Goal**: Full control over the game's complex palette system.

*   **Real-time Swapping**: Allow the user to instantly switch the active palette for the current sheet from a dropdown of game contexts (e.g., "Overworld - Death Mountain", "Dungeon - Ice Palace").
*   **Import/Export**: Support `.tpl` (Tile Layer Pro), `.pal` (JASC), and `.clr` (SNES) formats.
*   **Gradient Tools**: Generators for smooth color ramps.

## 3. Sprite Assembly & OAM Editor

**Goal**: Edit sprites as they are seen in game, not just as disjointed tiles.

*   **OAM Visualization**: Parse the `SpritePrep` and `AnimationSteps` tables to render sprites composed of multiple 8x8/16x16 tiles.
*   **Pose Editor**: UI to modify the X/Y offsets and tile IDs for each frame of an animation.
*   **Live Preview**: Animate the sprite using the game's timer logic.

## 4. Robust Editing Tools

**Goal**: Professional pixel-art features.

*   **Undo/Redo Stack**: Implement a command pattern for all pixel operations (Pencil, Fill, Paste).
*   **Selection Tools**: Magic Wand (select by color), Lasso, Marquee.
*   **Grid System**: Configurable grids for 8x8 (Tile8), 16x16 (Tile16), and custom metatiles.
*   **Tile Stamping**: Select a group of tiles and "stamp" them elsewhere.

## 5. External Workflow Integration

**Goal**: seamless interoperability with external tools (Aseprite, Photoshop).

*   **Live Reload**: Watch for changes in exported PNG files and auto-reload them into the editor.
*   **Batch Export/Import**: One-click export of all graphics to a folder structure, and one-click import back to ROM.

## 6. Technical Debt & Optimization

*   **Texture Atlas**: Combine individual sheet textures into a single large texture atlas to reduce draw calls (handled partially by `Arena` but could be optimized).
*   **Shader Support**: Use shaders for palette swapping instead of CPU-side pixel manipulation, enabling instant palette previews without texture updates.
