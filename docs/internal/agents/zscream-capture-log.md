# ZScreamDungeon Behavior Capture Log

**Owner:** TBD  
**Started:** 2026-02-04  
**Purpose:** Record observed ZScreamDungeon behaviors for selection, drag
modifiers, and context menus to align yaze UX.

## Environment
- ZScreamDungeon version/build: TODO
- OS: TODO
- Capture method (screen recording / notes): TODO
- ROM used: TODO

## Capture Matrix

### Selection Priority (overlapping objects)
- Setup: TODO
- Steps: TODO
- Observed: TODO
- Notes: Pending runtime capture.

### Marquee Selection Rules
- Setup: TODO
- Steps: TODO (edge-only / corner-only / inside cases)
- Observed: TODO
- Notes: Pending runtime capture.

### Drag Modifiers
- Setup: TODO
- Steps: TODO (Shift/Alt/Ctrl, duplicate, axis lock, snap)
- Observed: TODO
- Notes: Pending runtime capture.

### Room Context Menu (canvas)
- Setup: Dungeon canvas, no selection.
- Steps: Right-click empty space (no selection).
- Observed (code-skim, needs runtime validation):
  - Insert
  - Paste
  - Delete
  - Delete All
- Notes: From `ZeldaFullEditor/Gui/DungeonMain.Designer.cs` (ContextMenuStrip `nothingselectedcontextMenu`).

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
