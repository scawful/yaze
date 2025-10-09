#ifndef YAZE_APP_GUI_EDITOR_LAYOUT_H
#define YAZE_APP_GUI_EDITOR_LAYOUT_H

#include <functional>
#include <string>
#include <vector>

#include "imgui/imgui.h"

namespace yaze {
namespace gui {

/**
 * @class Toolset
 * @brief Ultra-compact toolbar that merges mode buttons with settings
 * 
 * Design Philosophy:
 * - Single horizontal bar with everything inline
 * - Small icon-only buttons for modes
 * - Inline property editing (InputHex with scroll)
 * - No wasted vertical space
 * - Beautiful, modern appearance
 * 
 * Layout: [Mode Icons] | [ROM Badge] [World] [GFX] [Pal] [Spr] ... | [Quick Actions]
 */
class Toolset {
 public:
  Toolset() = default;
  
  // Begin the toolbar
  void Begin();
  
  // End the toolbar
  void End();
  
  // Add mode button group
  void BeginModeGroup();
  bool ModeButton(const char* icon, bool selected, const char* tooltip);
  void EndModeGroup();
  
  // Add separator
  void AddSeparator();
  
  // Add ROM version badge
  void AddRomBadge(uint8_t version, std::function<void()> on_upgrade = nullptr);
  
  // Add quick property (inline hex editing)
  bool AddProperty(const char* icon, const char* label, uint8_t* value,
                  std::function<void()> on_change = nullptr);
  bool AddProperty(const char* icon, const char* label, uint16_t* value,
                  std::function<void()> on_change = nullptr);
  
  // Add combo selector
  bool AddCombo(const char* icon, int* current, const char* const items[], int count);
  
  // Add toggle button
  bool AddToggle(const char* icon, bool* state, const char* tooltip);
  
  // Add action button
  bool AddAction(const char* icon, const char* tooltip);
  
  // Add collapsible settings section
  bool BeginCollapsibleSection(const char* label, bool* p_open);
  void EndCollapsibleSection();
  
  // Add v3 settings indicator
  void AddV3StatusBadge(uint8_t version, std::function<void()> on_settings);
  
  // Add usage statistics button
  bool AddUsageStatsButton(const char* tooltip);
  
  // Get button count for widget registration
  int GetButtonCount() const { return button_count_; }
  
 private:
  bool in_toolbar_ = false;
  bool in_section_ = false;
  int button_count_ = 0;
  float current_line_width_ = 0.0f;
  float mode_group_button_size_ = 0.0f;
};

/**
 * @class EditorCard
 * @brief Draggable, dockable card for editor sub-windows
 * 
 * Replaces traditional child windows with modern cards that can be:
 * - Dragged and positioned freely
 * - Docked to edges (optional)
 * - Minimized to title bar
 * - Resized responsively
 * - Themed beautifully
 * 
 * Usage:
 * ```cpp
 * EditorCard tile_card("Tile Selector", ICON_MD_GRID_VIEW);
 * tile_card.SetDefaultSize(300, 400);
 * tile_card.SetPosition(CardPosition::Right);
 * 
 * if (tile_card.Begin()) {
 *   // Draw tile selector content
 *   tile_card.End();
 * }
 * ```
 */
class EditorCard {
 public:
  enum class Position {
    Free,        // Floating window
    Right,       // Docked to right side
    Left,        // Docked to left side
    Bottom,      // Docked to bottom
    Floating,    // Floating but position saved
  };
  
  EditorCard(const char* title, const char* icon = nullptr);
  EditorCard(const char* title, const char* icon, bool* p_open);
  
  // Set card properties
  void SetDefaultSize(float width, float height);
  void SetPosition(Position pos);
  void SetMinimizable(bool minimizable) { minimizable_ = minimizable; }
  void SetClosable(bool closable) { closable_ = closable; }
  void SetHeadless(bool headless) { headless_ = headless; }
  void SetDockingAllowed(bool allowed) { docking_allowed_ = allowed; }
  void SetIconCollapsible(bool collapsible) { icon_collapsible_ = collapsible; }
  
  // Begin drawing the card
  bool Begin(bool* p_open = nullptr);
  
  // End drawing
  void End();
  
  // Minimize/maximize
  void SetMinimized(bool minimized) { minimized_ = minimized; }
  bool IsMinimized() const { return minimized_; }
  
  // Focus the card window (bring to front and set focused)
  void Focus();
  bool IsFocused() const { return focused_; }
  
  // Get the window name for ImGui operations
  const char* GetWindowName() const { return window_name_.c_str(); }
  
 private:
  std::string title_;
  std::string icon_;
  std::string window_name_;  // Full window name with icon
  ImVec2 default_size_;
  Position position_ = Position::Free;
  bool minimizable_ = true;
  bool closable_ = true;
  bool minimized_ = false;
  bool first_draw_ = true;
  bool focused_ = false;
  bool* p_open_ = nullptr;
  
  // UX enhancements
  bool headless_ = false;  // Minimal chrome, no title bar
  bool docking_allowed_ = true;  // Allow docking
  bool icon_collapsible_ = false;  // Can collapse to floating icon
  bool collapsed_to_icon_ = false;  // Currently collapsed
  ImVec2 saved_icon_pos_ = ImVec2(10, 100);  // Position when collapsed to icon
  
  void DrawFloatingIconButton();
};

/**
 * @class EditorLayout
 * @brief Modern layout manager for editor components
 * 
 * Manages the overall editor layout with:
 * - Compact toolbar at top
 * - Main canvas in center
 * - Floating/docked cards for tools
 * - No redundant headers
 * - Responsive sizing
 */
class EditorLayout {
 public:
  EditorLayout() = default;
  
  // Begin the editor layout
  void Begin();
  
  // End the editor layout
  void End();
  
  // Get toolbar reference
  Toolset& GetToolbar() { return toolbar_; }
  
  // Begin main canvas area
  void BeginMainCanvas();
  void EndMainCanvas();
  
  // Register a card (for layout management)
  void RegisterCard(EditorCard* card);
  
 private:
  Toolset toolbar_;
  std::vector<EditorCard*> cards_;
  bool in_layout_ = false;
};

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_EDITOR_LAYOUT_H

