# CLAUDE.md

Purpose: Claude-specific routing for the yaze GUI library.

## Widget catalog

Before authoring a new widget or re-implementing a pattern, check the
[widget library index](widgets/README.md). It covers buttons, inputs
(`DrawProperty`), selectors (`FontPicker`, `IconBrowser`), canvas
primitives (`ResizeHandles`), layout, and colors — each with its
commit-contract and persistence expectations.

## Rules

1. Widgets are primitive — never depend on editor state. Return `bool` or a
   value on commit; leave persistence and undo wiring to the call site.
2. Read theme colors via `ThemeManager::Get().GetCurrentTheme()` at draw
   time so theme swaps take effect immediately.
3. Put pure-logic helpers (hit tests, snap math, query parsing) in
   `*_internal` namespaces so unit tests can exercise them without an
   ImGui context.
4. Follow existing anti-patterns — no hardcoded ImVec4 colors; use
   `ConvertColorToImVec4(theme.token)` or the `gui::Get*Color()` helpers
   in `core/ui_helpers.h`.

## Reference material

- [widgets/README.md](widgets/README.md) — widget library index.
- [core/theme_manager.h](core/theme_manager.h) — theme tokens + callback API.
- [core/ui_helpers.h](core/ui_helpers.h) — semantic color accessors.
- [core/common_icons.h](core/common_icons.h) — curated Material Design icon catalog used by `IconBrowser`.
