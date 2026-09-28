# Context Menus

All right-click menus in yaze use one renderer: `gui::RenderMenuItem` in
`src/app/gui/canvas/canvas_menu.{h,cc}`. Canvases feed it through
`Canvas::editor_menu()`; every other surface (list rows, tabs, chips,
swatches) uses `gui::ItemContextMenu` from
`src/app/gui/canvas/item_context_menu.{h,cc}`.

## API

```cpp
#include "app/gui/canvas/item_context_menu.h"

ImGui::Selectable(label, selected);
gui::ItemContextMenu(nullptr, [&]() {
  std::vector<gui::MenuItemSpec> items;
  items.emplace_back("Open Room", ICON_MD_OPEN_IN_NEW, [&]() { Open(id); });
  items.push_back(gui::CopyToClipboardItem("Copy Room ID", "0x012"));
  items.back().separator_after = true;
  items.push_back(gui::MenuItemSpec::Destructive(
      "Delete Room...", ICON_MD_DELETE, [&]() { Delete(id); }));
  return items;
});
```

- `gui::MenuItemSpec` is an alias of `gui::CanvasMenuItem`.
- The builder runs only while the popup is open.
- Pass a `std::function<gui::CanvasMenuDefinition()>` builder when the menu
  needs section titles.
- `id == nullptr` binds the popup to the last submitted item, like
  `ImGui::BeginPopupContextItem()`.
- Inside an existing `BeginPopup*` block, call `gui::RenderMenuItems(items)`.

## Destructive items

Set `destructive = true` (or use `MenuItemSpec::Destructive`) for actions that
delete, forget, overwrite many things, or discard unsaved work. The label
renders in `gui::GetErrorColor()`.

Set `requires_confirmation = true` (the `Destructive` factory default) when
the action cannot be undone. Confirmation is two-step, inside the menu:

1. First click arms the item. The menu stays open, the label becomes
   `Confirm <Label>?`, the shortcut column shows `click again`, and hovering
   shows the item's `tooltip`, or "This cannot be undone." when it has none.
2. Second click runs the callback and closes the menu.
3. Closing the menu, or arming another item, disarms it.

The state machine is `gui::MenuConfirmState` (ImGui-free, unit-tested in
`test/unit/gui/item_context_menu_test.cc`). Actions that the undo stack can
revert (for example dungeon object deletion) should use `destructive = true`
with confirmation only when the scope is large (Delete All).

## Section order

Separate groups with `separator_after = true` on the last item of a group.
Order groups top to bottom:

1. Open / Navigate: `Open ...`, `Jump to ...`, `Show in ...`, `Pin`.
2. Edit / Clipboard: `Copy ...`, `Paste`, `Duplicate`, `Rename...`.
3. Organize: layers, z-order, sort, export/import, `Remove from ...`.
4. Destructive: `Delete...`, `Forget...`, `Close`, `Clear All...`.

## Naming

- Verb first, Title Case: `Open Tracker`, `Copy Room ID`, `Delete Song...`.
- Copy items name the thing copied: `Copy <Thing> ID`, `Copy Text`,
  `Copy as SNES ($XXXX)`.
- `...` (three ASCII dots) marks items that open a dialog or ask for
  confirmation. Items that act immediately have no ellipsis.
- Shortcut hints use the platform-neutral spelling used elsewhere:
  `Ctrl+C`, `Ctrl+D`, `Delete`, `Esc`. Show a hint only when the shortcut
  actually works in that context.

## Icons

| Action | Icon |
| --- | --- |
| Open / open in new view | `ICON_MD_OPEN_IN_NEW` |
| Jump / navigate | `ICON_MD_ARROW_FORWARD` |
| Copy / Copy ID | `ICON_MD_CONTENT_COPY` |
| Paste | `ICON_MD_CONTENT_PASTE` |
| Duplicate | `ICON_MD_FILE_COPY` |
| Rename | `ICON_MD_DRIVE_FILE_RENAME_OUTLINE` |
| Pin / Unpin | `ICON_MD_PUSH_PIN` |
| Export / Import | `ICON_MD_FILE_DOWNLOAD` / `ICON_MD_FILE_UPLOAD` |
| Delete one | `ICON_MD_DELETE` |
| Delete all / forget | `ICON_MD_DELETE_FOREVER` / `ICON_MD_DELETE_SWEEP` |
| Close | `ICON_MD_CLOSE` |

## Popup padding

Canvases draw inside `gui::BeginNoPadding()` (WindowPadding and FramePadding
0). ImGui copies WindowPadding into a popup window when the popup begins, so
a menu begun there used to lose its padding. `gui::PopupStyleScope`
(`canvas_menu.h`) pushes the theme's WindowPadding, FramePadding, ItemSpacing
and ItemInnerSpacing (the values below every `PushStyleVar` still on the
stack). It must be constructed before `BeginPopup*` and live until
`EndPopup`. The shared sites already do this: `CanvasContextMenu::Render`
(every canvas menu), `gui::ItemContextMenu`, and
`PopupRegistry::RenderAll`. A hand-written `BeginPopup` inside a
zero-padding region needs its own scope.

## Overworld map canvas

Built by `MapPropertiesSystem::SetupCanvasContextMenu`
(`src/app/editor/overworld/maps/map_properties.cc`) from a target captured
when the menu opens (`OverworldContextTarget`). Select tool: right-click.
Brush/Fill: Shift+right-click (a plain right-click samples the Tile16 and
selects the map). Right-clicking an entity opens the entity menu instead.

```text
[map]  0x00 | Light World | Lost Woods          (disabled header)
[grid] Tile16 0x0E3 | (26, 30)                   (disabled header)
-----------------------------------------------
Sample Tile16                            I
Edit Tile16...
-----------------------------------------------
Select This Map        (hidden when this map's area is current)
Map Properties...                        Double-click
Pin Map  [checked when pinned]           Ctrl+L
Related Maps  >  Area parent / Same area screens, Other world screen
-----------------------------------------------
Copy Map Properties
Paste Map Properties   (disabled until something was copied)
-----------------------------------------------
Insert  >  Entrance, Hole, Exit, Item, Sprite    (at the clicked tile)
View    >  Grid, Entities, Overlay Preview (checked, same state as the
           toolbar); Zoom In, Zoom Out, Zoom to Fit, Center on Map
```

- At most 10 top-level rows; "Select This Map" is usually hidden (unpinned,
  the map under the cursor is already current).
- Related Maps selects the map, switches world when needed and centers it
  (`OverworldEditor::JumpToMap`).
- Map/Pin/Select act on the clicked map, not the hovered or selected one.
- Not in the menu, by design: Transport (no insert path), Select All Matching
  Tiles (no API), and rows that duplicated the Map Properties panel (custom
  background color, visual effects, rename label, per-group metadata
  copy/paste), did nothing on this canvas (hex/custom labels, grid size),
  or were trimmed to keep the list short (Reset View; the toolbar zoom and
  View > Zoom to Fit remain).
