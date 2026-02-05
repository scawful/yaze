# ZScreamDungeon Behavior Capture Log

**Owner:** scawful  
**Started:** 2026-02-04  
**Purpose:** Record observed ZScreamDungeon behaviors for selection, drag
modifiers, and context menus to align yaze UX.

## Environment
- ZScreamDungeon version/build: local repo checkout
- OS: macOS (code-skim only)
- Capture method (screen recording / notes): code-skim (SceneUW.cs + DungeonMain.cs)
- ROM used: N/A (no runtime)

## Capture Matrix

### Selection Priority (overlapping objects)
- Setup: Dungeon room with stacked objects on same tile (BG1/BG2/BG3/Overlay).
- Steps: Click repeatedly without modifiers.
- Observed (code-skim, needs runtime validation):
  - Iterates `room.tilesObjects` from end to start (last added/highest Z wins).
  - If `selectedMode != Bgallmode`, skips objects not on active layer.
  - `isMouseCollidingWith(obj)` uses object bounds; for non-Bgr/Door/Torch/Block objects, a second pass checks `collisionPoint` tiles to confirm.
  - Shift/Ctrl preserve prior selection; Alt clears selection.
- Notes: See `ZeldaFullEditor/Gui/Scene/SceneUW.cs` `onMouseDown()` selection loops.

### Marquee Selection Rules
- Setup: No selection; click-drag a rectangle over objects.
- Steps: Drag in any direction (negative dx/dy supported).
- Observed (code-skim, needs runtime validation):
  - Triggered only when `room.selectedObject.Count == 0` on mouse up.
  - Uses `Rectangle.IntersectsWith` between object bounds and drag rectangle.
  - Sprites: bounding box vs rectangle in 16px grid.
  - Pot items: 16x16 box at `(x*8, y*8)` vs rectangle in 8px grid.
  - BG objects: rectangle uses `(X+offsetX, Y+offsetY+yfix)` with width/height; filtered by layer (unless Bgallmode) and options (excludes Bgr/Door/Torch/Block).
  - Overlay mode: uses `DungeonOverlays.loadedOverlay` with same intersect logic.
  - No additive marquee (Shift/Ctrl not consulted).
- Notes: `getObjectsRectangle()` in `ZeldaFullEditor/Gui/Scene/SceneUW.cs`.

### Drag Modifiers
- Setup: Select object(s); drag to move.
- Steps: Drag with/without Shift/Ctrl/Alt.
- Observed (code-skim, needs runtime validation):
  - No explicit drag modifiers/axis lock/duplicate in `SceneUW`.
  - Movement is tile-based deltas (`move_x/move_y`), applied on drag.
  - ModifierKeys only used for selection add/clear (Shift/Ctrl/Alt).
- Notes: `move_objects()` + `onMouseDown()` in `SceneUW.cs`.

### Room Context Menu (canvas)
- Setup: Dungeon canvas, no selection.
- Steps: Right-click empty space (no selection).
- Observed (code-skim, needs runtime validation):
  - Base menu items come from `nothingselectedcontextMenu` (Insert/Paste/Delete/Delete All).
  - Menu item visibility toggles by mode (e.g., Spritemode hides Insert; CollisionMap hides Insert/Paste).
  - Insert label text becomes "Insert new <mode>" (door, torch, block, etc).
- Notes: `ZeldaFullEditor/Gui/Scene/SceneUW.cs` `onMouseUp()` mode switch + `DungeonMain.Designer.cs`.

### Object Context Menu
- Setup: Dungeon canvas with single or multi selection.
- Steps: Right-click selection.
- Observed (code-skim, needs runtime validation):
  - Single selection:
    - Insert
    - Cut
    - Copy
    - Paste
    - Delete
    - Increase Z (Send to Front, Increase Z by 1 [disabled])
    - Decrease Z (Send to Back, Decrease Z by 1 [disabled])
    - Send to Layer 1
    - Send to Layer 2
    - Send to Layer 3
    - Edit Graphics (disabled)
  - Group selection:
    - Insert
    - Cut
    - Copy
    - Paste
    - Delete
    - Send to Front
    - Send to Back
    - Save As New Layout…
    - Send to Layer 1
    - Send to Layer 2
    - Send to Layer 3
- Notes: From `ZeldaFullEditor/Gui/DungeonMain.Designer.cs` (`singleselectedcontextMenu`, `groupselectedcontextMenu`).

## Evidence Links
- Screenshots: TODO
- Video: TODO
