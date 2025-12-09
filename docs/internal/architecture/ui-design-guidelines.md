# YAZE Design Language & Interface Guidelines

This document defines the standard for User Interface (UI) development in `yaze`. All new components and refactors must adhere to these rules to ensure a consistent, theme-able, and configurable experience.

## 1. Core Philosophy
*   **Configurability First:** Never assume a user's workflow. Every panel must be dockable, movable, and toggleable. Default layouts are just starting points.
*   **Theme Compliance:** **Never** use hardcoded colors (e.g., `ImVec4(1, 0, 0, 1)`). All colors must be derived from the `ThemeManager` or standard `ImGui::GetStyle().Colors`.
*   **Zelda-Native Inputs:** Hexadecimal is the first-class citizen for data. Decimal is for UI settings (window size, scaling).

## 2. Theming & Colors
*   **Semantic Colors:** Use the `gui::Theme` abstraction.
    *   **Do:** `ImGui::PushStyleColor(ImGuiCol_Text, theme->GetColor(gui::ThemeCol_Error))`
    *   **Don't:** `ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f))`
*   **Theme Integrity:** If a custom widget needs a color not in standard ImGui (e.g., "SRAM Modified" highlight), add it to `EnhancedTheme` in `theme_manager.h` rather than defining it locally.
*   **Transparency:** Use `ImGui::GetStyle().Alpha` modifiers for disabled states rather than hardcoded grey values to support dark/light modes equally.

## 3. Layout Structure
The application uses a "VSCode-like" anatomy:
*   **Activity Bar (Left):** Global context switching (Editor, Settings, Agent).
    *   *Rule:* Icons only. No text. Tooltips required.
*   **Sidebar (Left, Docked):** Context-specific tools (e.g., Room List for Dungeon Editor).
    *   *Rule:* Must be collapsible. Width must be persistable.
*   **Primary View (Center):** The canvas or main editor (e.g., Dungeon View).
    *   *Rule:* This is the "Central Node" in ImGui docking terms. It should rarely be hidden.
*   **Panel Area (Bottom/Right):** Auxiliary tools (Log, Hex Inspector).
    *   *Rule:* Tabbed by default to save space.

## 4. Widget Standards

### A. Input Fields
*   **Hexadecimal:** Use `gui::InputHexByte` / `gui::InputHexWord` wrapper.
    *   *Requirement:* Must support Scroll Wheel to increment/decrement values.
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
