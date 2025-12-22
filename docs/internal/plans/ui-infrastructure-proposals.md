# UI Infrastructure Technical Improvement Proposals

These proposals address the infrastructure changes needed to support the YAZE Design Language.

## Proposal A: Unified Panel System
**Problem:** `PanelManager` currently has hardcoded logic for specific panels, making it hard for users to create custom layouts or for plugins to register new views.

**Solution:** Refactor `PanelManager` to use a Registry pattern.

```cpp
// Concept
struct PanelDef {
    std::string id;
    std::string display_name;
    std::function<void()> draw_fn;
    PanelCategory category; // Editor, Debug, Agent
    ImGuiDir default_dock; // Left, Bottom, Center
};

PanelRegistry::Register("dungeon_properties", { ... });
```

**Benefit:** Allows the `LayoutManager` to generically save/restore *any* panel's open state and position without hardcoded checks.

## Proposal B: Theme Abstraction Layer Refactor
**Problem:** `src/app/gui/core/style.cc` contains `ColorsYaze()` with hardcoded definitions (e.g., `alttpDarkGreen`).

**Solution:**
1.  Deprecate `ColorsYaze`.
2.  Expand `EnhancedTheme` to include a "Game Palette" section (e.g., `GameCol_LinkGreen`, `GameCol_GanonBlue`).
3.  Update `ThemeManager` to populate these from configuration files (JSON/TOML), allowing users to "skin" the editor to look like different Zelda games (e.g., Minish Cap style).

## Proposal C: Layout Persistence Engine
**Problem:** Layouts are currently stuck in `imgui.ini` (binary/opaque to user) or hardcoded presets.

**Solution:**
1.  Create a `LayoutService` that wraps `ImGui::SaveIniSettingsToMemory`.
2.  Allow users to "Save Current Layout As..." which writes the INI data to a named file in `user/layouts/`.
3.  Add a "Reset Layout" command that forces a reload of the default preset for the current context.

## Proposal D: Input Widget Standardization
**Problem:** Scroll wheel support on Hex inputs is inconsistent.

**Solution:**
Modify `src/app/gui/core/input.cc`:

```cpp
bool InputHexByte(const char* label, uint8_t* v) {
    // ... existing input logic ...
    if (ImGui::IsItemHovered()) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0) {
            if (wheel > 0) *v += 1; else *v -= 1;
            return true;
        }
    }
    return false;
}
```

## Proposal E: State Management Audit
**Problem:** `ActivityBar` and `StatusBar` often hold their own state or read global statics.

**Solution:**
*   **Context Injection:** UI components should receive a `EditorContext*` or `ProjectState*` in their `Draw()` method rather than accessing singletons.
*   **Reactive UI:** Implement a simple `EventBus` for UI updates (e.g., `Event::ProjectLoaded`, `Event::SelectionChanged`) so the Status Bar updates only when necessary, rather than polling every frame.
