# YAZE Design Language & Interface Guidelines

This document guides UI work in `yaze`. The
[UI module pattern](editor-ui-module-pattern.md) defines composition; the
[capability plan](../plans/editor-capability-parity-plan.md) defines the editing
workflows to complete. These are design targets, not a claim that all existing
controls already comply. Last reviewed: 2026-09-22.

## 1. Core Philosophy
*   **Keep the editing context visible:** The Workbench canvas, selection inspector, and local tools form one workflow. Use configurable layouts where useful, but do not turn every control into a separate dockable window. Default placement and density are design decisions that need real editing feedback.
*   **Theme Compliance:** **Never** use hardcoded colors (e.g., `ImVec4(1, 0, 0, 1)`). All colors must be derived from the `ThemeManager` or standard `ImGui::GetStyle().Colors`.
*   **Named choices first:** Known sprite, object, item, and property values should have readable names with IDs available alongside them. Use explicit tile/pixel units for position and dimensions. Hexadecimal remains useful for addresses, encoded values, and expert inspection; users should not need to memorize IDs to perform ordinary edits.

## 2. Theming & Colors
*   **Semantic Colors:** Use the theme API already used by the surrounding feature or standard ImGui style colors. Check current call sites before adding theme access; old snippets are not proof that an API exists. Do not introduce a second theme provider during a layout change.
*   **Theme Integrity:** If a custom widget needs a color not in standard ImGui (e.g., "SRAM Modified" highlight), add it to `EnhancedTheme` in `theme_manager.h` rather than defining it locally.
*   **Transparency:** Use `ImGui::GetStyle().Alpha` modifiers for disabled states rather than hardcoded grey values to support dark/light modes equally.

## 3. Layout Structure
The application uses a "VSCode-like" anatomy:
*   **Activity Bar (Left):** Global context switching (Editor, Settings, Agent).
    *   *Rule:* Icons only. No text. Tooltips required.
*   **Sidebar (Left, Docked):** Context-specific tools (e.g., Room List for Dungeon Editor).
    *   *Rule:* Must be collapsible. Width must be persistable.
*   **Primary View (Center):** The canvas or main editor (e.g., Dungeon Workbench).
    *   *Rule:* Selection controls and local tools should preserve the canvas and selected room context. Follow the existing workspace composition; a feature does not require a new central dock node.
*   **Panel Area (Bottom/Right):** Auxiliary tools (Log, Hex Inspector).
    *   *Rule:* Tabbed by default to save space.

## 4. Widget Standards

### A. Input Fields
*   **Hexadecimal:** Use `gui::InputHexByte` / `gui::InputHexWord` wrapper.
    *   *Requirement:* Wheel editing must have a clear hovered/focused owner. Do not consume the same wheel event for a property, canvas zoom, and placement sizing. Follow the placement handler's object-specific sizing semantics rather than applying a universal width/height rule.
    *   *Requirement:* Monospace font is mandatory for hex values.
*   **Text:** Use `gui::InputText` wrappers that handle `std::string` resizing automatically.

### B. Icons
*   **Library:** Use Material Design icons via `ICON_MD_...` macros.
*   **Alignment:** Icons must be vertically aligned with text. Use `ImGui::AlignTextToFramePadding()` before text if the icon causes misalignment.

### C. Containers
*   **Collapsibles:** Prefer `ImGui::CollapsingHeader` for major sections and `ImGui::TreeNode` for hierarchy.
*   **Tabs:** Use `ImGui::BeginTabBar` only for switching between distinct *views* (e.g., "Visual Editor" vs "Text Editor"). Do not use tabs for property categorization (use headers instead).
*   **Tables:** Use `ImGui::BeginTable` with `ImGuiTableFlags_BordersInnerV` for property lists.
    *   *Format:* 2 Columns (Label, Control). Column 1 fixed width, Column 2 stretch.

## 5. Interaction Patterns
*   **Hover:** All non-obvious interactions must have a `gui::Tooltip`.
*   **Context Menus:** Right-click on *any* game object (sprite, tile) must show a context menu.
*   **Drag & Drop:** "Source" and "Target" payloads must be strictly typed (e.g., `"PAYLOAD_SPRITE_ID"`).

## 6. Selector workflow and design ownership

A selector has three separate responsibilities: finding a type, previewing the
choice, and committing an edit. Keep those steps visible and connect the commit
to the existing interaction handler and undo path. Filtering or hovering must
not mutate the room. Cancel must preserve the previous state; a committed edit
must refresh the canvas and inspector together.

For a new named chooser, design the empty/search/no-result states, current-value
display, keyboard navigation, and narrow-width layout together. Show the name
and ID consistently; preserve unsupported or unknown encoded values rather than
silently selecting the first known entry. Category labels must reflect actual
data semantics; legacy numeric ranges need review before reuse.

The maintainer owns these interaction and layout choices. A useful first coding
slice is a searchable sprite-type chooser in
`src/app/editor/dungeon/inspectors/dungeon_entity_inspector.cc`, reusing the
existing mutation handler. The legacy placement selector's layout lives in
`src/app/editor/dungeon/ui/window/sprite_editor_panel.cc`; use it to study current
behavior without treating its categories or fixed widths as the new design.

Agents can first isolate drawing code, remove duplicate implementation, and
check mutation/undo contracts. They should not absorb an explicitly human-owned
feature while doing that preparation. Compiler and tidy success do not establish
layout quality. Record live acceptance at representative widths and scale,
including keyboard and pointer use; screenshots can support that review but
cannot prove focus, wheel, drag, or selection behavior.
