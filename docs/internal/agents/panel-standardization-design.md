# Panel Standardization Design Note

_Author: Claude Code audit pass — 2026-02-23_

## What EditorPanel already provides

`app/editor/system/editor_panel.h` defines the full interface:

- **Identity (required):** `GetId()`, `GetDisplayName()`, `GetIcon()`, `GetEditorCategory()`
- **Drawing (required):** `Draw(bool*)`
- **Lifecycle (optional):** `OnOpen()`, `OnClose()`, `OnFocus()`, `OnFirstDraw()`
- **Behavior (optional, defaulted):** `GetPanelCategory()` → `EditorBound`,
  `GetContextScope()` → `kNone`, `GetScope()` → `kSession`, `IsEnabled()` → `true`
- **Caching infrastructure:** `GetCached<T>()`, `InvalidateCache()` (for derived panels)

`ResourcePanel` (`system/resource_panel.h`) extends `EditorPanel` with:
- Auto-generated `GetId()` / `GetDisplayName()` from `GetResourceType()` + `GetResourceId()`
- `GetPanelCategory()` overridden to `CrossEditor` (opt-in persistence)

## What "standardized" means

A panel is considered standard when it:
1. Inherits `EditorPanel` (or `ResourcePanel`) directly — no bypass
2. Overrides all four identity methods with non-empty, category-appropriate values
3. Returns a valid `ICON_MD_*` constant from `app/gui/core/icons.h` in `GetIcon()`
4. Overrides `GetPanelCategory()` only when the default `EditorBound` is wrong
   (e.g., `CrossEditor` for resource panels, `Persistent` for tools that persist globally)

## Audit result: dungeon panels (all 20 inspected)

All panels under `src/app/editor/dungeon/panels/` already conform. No migrations needed.

| Panel | GetIcon | Category override | Status |
|---|---|---|---|
| ChestEditorPanel | ICON_MD_INVENTORY_2 | none (EditorBound) | OK |
| CustomCollisionPanel | ICON_MD_GRID_ON | none (EditorBound) | OK |
| DungeonEmulatorPreviewPanel | ICON_MD_MONITOR | none (EditorBound) | OK |
| DungeonEntranceListPanel | ICON_MD_DOOR_FRONT | none (EditorBound) | OK |
| DungeonEntrancesPanel | ICON_MD_TUNE | none (EditorBound) | OK |
| DungeonMapPanel | ICON_MD_MAP | none (EditorBound) | OK |
| DungeonPaletteEditorPanel | ICON_MD_PALETTE | none (EditorBound) | OK |
| DungeonRoomGraphicsPanel | ICON_MD_IMAGE | none (EditorBound) | OK |
| DungeonRoomMatrixPanel | ICON_MD_GRID_VIEW | none (EditorBound) | OK |
| DungeonRoomPanel | ICON_MD_GRID_ON | CrossEditor (via ResourcePanel) | OK |
| DungeonRoomSelectorPanel | ICON_MD_LIST | none (EditorBound) | OK |
| DungeonSettingsPanel | ICON_MD_SETTINGS | none (EditorBound) | OK |
| DungeonWorkbenchPanel | ICON_MD_WORKSPACES | none (EditorBound) | OK |
| ItemEditorPanel | ICON_MD_INVENTORY | none (EditorBound) | OK |
| MinecartTrackEditorPanel | ICON_MD_TRAIN | none (EditorBound) | OK |
| ObjectEditorPanel | ICON_MD_CONSTRUCTION | none (EditorBound) | OK |
| ObjectTileEditorPanel | ICON_MD_GRID_ON | none (EditorBound) | OK |
| RoomTagEditorPanel | ICON_MD_LABEL | none (EditorBound) | OK |
| SpriteEditorPanel | ICON_MD_PERSON | none (EditorBound) | OK |
| WaterFillPanel | ICON_MD_WATER_DROP | none (EditorBound) | OK |

## No structural changes needed

The existing interface is sufficient. Future panels should follow the pattern shown
in `OracleValidationPanel` (oracle panels) and `DungeonRoomSelectorPanel` (dungeon panels)
as canonical reference implementations.
