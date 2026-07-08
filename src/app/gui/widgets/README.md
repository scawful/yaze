# Widget Library Index

Reusable UI primitives living under `src/app/gui/widgets/`. Each entry is a
drop-in widget with a bool/commit-value return contract and no dependency
on editor state — call sites own persistence and undo wiring.

## Buttons

From [`themed_widgets.h`](themed_widgets.h):

- `bool RippleButton(label, size, ripple_color, panel_id, anim_id)` — animated click ripple.
- `bool BouncyButton(label, size, ...)` — scale-bounce feedback on hover/click.
- `bool ThemedButton(label, size, ...)` — neutral button routed through `ThemeManager`.
- `bool PrimaryButton(label, size, ...)` — `theme.primary`-tinted CTA.
- `bool DangerButton(label, size, ...)` — `theme.error`-tinted destructive action.
- `bool SuccessButton(label, size, ...)` — `theme.success`-tinted confirm.
- `bool ThemedIconButton(icon, tooltip, size, ...)` — icon-only themed button.
- `bool TransparentIconButton(icon, size, tooltip)` — no-chrome icon button.
- `bool ToolbarIconButton(icon, tooltip, ...)` / `bool InlineIconButton(icon, tooltip, ...)` — toolbar + text-flow variants.

## Inputs

From [`property_inspector.h`](property_inspector.h):

- `bool DrawProperty(label, bool*, opts)` / `bool DrawProperty(label, int*, opts)` — typed input widgets (bool, int, u8, u16, u32, float, double, std::string, ImVec4, Color).
- `bool DrawPropertyHex(label, u8*|u16*|u32*, opts)` — hex-formatted integer input with uppercase hex filter.
- `bool DrawPropertyCombo<EnumT>(label, EnumT*, items, opts)` — enum combo with null-terminated labels.
- `PropertyOptions { min, max, step, step_fast, format, tooltip, read_only, text_flags }` — clamp + formatting.

Call these inside a `BeginTable("grid", 2)` to get auto label/value column routing, or call inline for ImGui-default layout.

## Selectors

- [`font_picker.h`](font_picker.h): `bool FontPicker(label, int* index)` — dropdown over `ImGui::GetIO().Fonts->Fonts`; preview renders each entry in its own face.
- [`icon_browser.h`](icon_browser.h): `const char* IconBrowser(label)` — searchable grid of Material Design glyphs from the curated `kCommonIcons[]` catalog (6 categories × ~15 icons).
- [`tile_selector_widget.h`](tile_selector_widget.h): tile palette browsing + selection.
- [`asset_browser.h`](asset_browser.h): filesystem-style asset picker.
- [`palette_editor_widget.h`](palette_editor_widget.h): SNES palette row/swatch editor.

## Canvas

- [`resize_handles.h`](resize_handles.h): `bool ResizeHandles(ImRect*, HandleMask, snap, id, style)` — 8-zone hit-tested resize affordance; returns true on drag release, mutates the rect in place during drag. Corner priority + cursor flips + min-size clamp.
- [`dungeon_object_emulator_preview.h`](dungeon_object_emulator_preview.h): render a live SNES object preview with ArenaRom.

## Layout

From [`themed_widgets.h`](themed_widgets.h):

- `void PanelHeader(title, icon, show_close, on_close)` — panel title bar with optional icon + close button.
- `void SectionHeader(label)` — mid-panel section divider with text.
- `bool BeginThemedTabBar(id, flags)` / `void EndThemedTabBar()` — themed tab group.
- `void DrawCanvasHUD(label, pos, size, body_fn)` — canvas overlay HUD positioned at absolute pos.

## Colors

- [`themed_widgets.h`](themed_widgets.h): `bool PaletteColorButton(id, SnesColor, ...)` — clickable swatch for a single SNES color.
- [`palette_editor_widget.h`](palette_editor_widget.h): full palette row editor.

## Common patterns

- **Commit contract**: widgets return `true` on the frame the user commits a change. Mid-interaction frames may mutate the pointee but return `false` (e.g. `ResizeHandles` during drag).
- **Theme pickup**: colors default to reading from `ThemeManager::Get().GetCurrentTheme()` at draw time so theme swaps take effect immediately — don't cache themed colors across frames.
- **Undo integration**: widgets do not post undo commands. Wrap the return value at the call site to bind into your editor's command manager.
- **Internal helpers**: pure-logic helpers (snap math, string matching, query parsing) live in `*_internal` namespaces so unit tests can exercise them without mouse input.

## Text / code

- [`text_editor.h`](text_editor.h): multi-line syntax-highlighted editor (ImGuiColorTextEdit fork).
