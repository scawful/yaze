#ifndef YAZE_APP_GUI_TOOLSET_H
#define YAZE_APP_GUI_TOOLSET_H

#include <functional>
#include <string>
#include <vector>

#include "imgui/imgui.h"

namespace yaze {
namespace gui {

/**
 * @class Toolset
 * @brief Modern, space-efficient toolset for editor mode switching
 * 
 * A toolset is a horizontal bar of icon buttons for switching between
 * editor modes (Pan, Draw, Select, etc.). This provides a consistent,
 * beautiful interface across all editors.
 * 
 * Features:
 * - Compact, icon-based design
 * - Visual selection indicator
 * - Keyboard shortcut hints
 * - Automatic tooltips
 * - Theme-aware styling
 * - Separators for logical grouping
 * 
 * Usage:
 * ```cpp
 * Toolset toolset;
 * toolset.AddTool("Pan", ICON_MD_PAN_TOOL, "1", []() { mode = PAN; });
 * toolset.AddTool("Draw", ICON_MD_DRAW, "2", []() { mode = DRAW; });
 * toolset.AddSeparator();
 * toolset.AddTool("Zoom+", ICON_MD_ZOOM_IN, "", []() { ZoomIn(); });
 * 
 * if (toolset.Draw()) {
 *   // A tool was clicked
 * }
 * ```
 */
class Toolset {
 public:
  struct Tool {
    std::string id;
    std::string icon;
    std::string tooltip;
    std::string shortcut;
    std::function<void()> callback;
    bool enabled = true;
    bool separator_after = false;
  };

  Toolset() = default;
  
  // Add a tool to the toolset
  void AddTool(const std::string& id, const char* icon, 
               const char* shortcut, std::function<void()> callback,
               const char* tooltip = nullptr);
  
  // Add a separator for visual grouping
  void AddSeparator();
  
  // Set the currently selected tool
  void SetSelected(const std::string& id);
  
  // Get the currently selected tool ID
  const std::string& GetSelected() const { return selected_; }
  
  // Draw the toolset (returns true if a tool was clicked)
  bool Draw();
  
  // Compact mode (smaller buttons, no labels)
  void SetCompactMode(bool compact) { compact_mode_ = compact; }
  
  // Set the number of columns (0 = auto-fit to window width)
  void SetMaxColumns(int columns) { max_columns_ = columns; }
  
  // Clear all tools
  void Clear();
  
 private:
  void DrawTool(const Tool& tool, bool is_selected);
  
  std::vector<Tool> tools_;
  std::string selected_;
  bool compact_mode_ = true;
  int max_columns_ = 0;  // 0 = auto-fit
};

/**
 * @namespace EditorToolset
 * @brief Helper functions for creating common editor toolsets
 */
namespace EditorToolset {

// Create standard editing mode toolset
Toolset CreateStandardToolset();

// Create overworld-specific toolset
Toolset CreateOverworldToolset();

// Create dungeon-specific toolset
Toolset CreateDungeonToolset();

}  // namespace EditorToolset

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_TOOLSET_H
