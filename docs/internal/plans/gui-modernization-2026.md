# YAZE GUI and Theming Modernization Plan (2026)

**Status:** ⚠️ In Progress (core phases landed; polish follow-ups remain)
**Created:** 2026-02-13
**Updated:** 2026-02-14

---

## Overview

This plan aims to unify and modernize the YAZE editor's user interface, moving from fragmented theming and standard widgets to a cohesive, high-performance, and visually polished experience.

| Phase | Focus | Complexity | Status |
|-------|-------|------------|--------|
| 1 | Uniform Theming & Widget Migration | Medium | ✅ Done |
| 2 | Advanced Visual Feedback & Micro-interactions | Medium-High | ⚠️ In Progress |
| 3 | Visual Identity & Custom Assets | Medium | ⚠️ In Progress |
| 4 | Layout & Workspace Modernization | High | ✅ Done |

---

## Phase 1: Uniform Theming & Widget Migration

**Goal:** Establish a consistent look and feel across all application components.

### Task 1.1: Standardize Editor Colors
- [x] Refactor `DungeonCanvasViewer` to use `ThemeManager` semantic colors instead of `AgentUI::GetTheme()`.
- [x] Audit `OverworldEditor` and related panels for hardcoded `ImVec4`/`IM_COL32` values.
- [x] Consolidate editor-specific semantic colors into the core `Theme` struct if missing.

### Task 1.2: Universal Widget Adoption
- [x] Replace standard `ImGui::Button` and `ImGui::SmallButton` with `ThemedButton` and `ThemedIconButton`.
- [x] Update all panel headers to use `PanelHeader()`.
- [x] Migrate headers and separators to `SectionHeader()`.

### Task 1.3: Style Consistency Check
- [x] Verify that all popups and modals respect theme-defined rounding and padding.
- [x] Ensure tooltips use theme-aware colors and borders.

---

## Phase 2: Advanced Visual Feedback & Micro-interactions

**Goal:** Enhance the user experience with subtle animations and better feedback.

### Task 2.1: Canvas Micro-interactions
- [x] Add pulsing selection borders to selected objects.
- [ ] Implement smooth hover scaling for interactive canvas elements.
- [ ] Add animated feedback for "nudge" and "alignment" actions.

### Task 2.2: Smart Hover Previews
- [x] Implement floating tooltips with graphical previews for object/sprite selectors.
- [ ] Add "Quick Info" overlays when hovering over canvas tiles.

### Task 2.3: Mode Transitions
- [x] Implement smooth fade transitions when switching between editors.
- [x] Add sliding animations for sidebar toggles using the `Animator` class.
- [x] Add user-configurable switch motion profiles (`Snappy`, `Standard`, `Relaxed`) and a reduced-motion toggle wired through `UserSettings`.

---

## Phase 3: Visual Identity & Custom Assets

**Goal:** Infuse the UI with Zelda-specific character and professional polish.

### Task 3.1: Zelda-Specific Iconography
- [x] Consolidate icon usage and prepare for custom font integration.
- [ ] Replace generic Material Design icons in high-visibility areas with Zelda-themed variants.

### Task 3.2: Canvas Aesthetics
- [x] Implement theme-aware canvas backgrounds (subtle patterns or glows).
- [x] Ensure grid lines adapt dynamically to the theme's accent color.

### Task 3.3: Theme-Aware Graphics
- [x] Make placeholder graphics and drag-and-drop previews adapt to theme colors.
- [x] Standardize the look of "Nothing" items and empty slots across all editors.

---

## Phase 4: Layout & Workspace Modernization

**Goal:** Improve workflow efficiency through flexible workspace management.

### Task 4.1: Workspace Presets
- [x] Define and implement "Dungeon Master," "Overworld Artist," and "Logic Debugger" presets.
- [x] Add workspace toggles to the Command Palette.

### Task 4.2: Floating & Dockable Toolbars
- [x] Integrate themed icon buttons into side panels for consistent interaction.
- [x] Allow sidebars to be collapsed into thin icon strips (via RightPanelManager animations).

### Task 4.3: Modernized Property Panels
- [x] Refactor dense tables into "Property Cards" (using SectionHeader and ThemedButton).
- [x] Standardize all property panels to use unified theme colors and widgets.
