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
