#ifndef YAZE_APP_EDITOR_LAYOUT_DESIGNER_LAYOUT_DEFINITION_H_
#define YAZE_APP_EDITOR_LAYOUT_DESIGNER_LAYOUT_DEFINITION_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "imgui/imgui.h"
#include "app/editor/editor.h"

namespace yaze {
namespace editor {
namespace layout_designer {

/**
 * @enum DockNodeType
 * @brief Type of dock node in the layout tree
 */
enum class DockNodeType {
  Root,   // Root dockspace node
  Split,  // Container with horizontal/vertical split
  Leaf    // Leaf node containing panels
};

/**
 * @struct LayoutPanel
 * @brief Represents a single panel in a layout
 * 
 * Contains all metadata needed to recreate the panel in a layout,
 * including size, position, flags, and visual properties.
 */
struct LayoutPanel {
  std::string panel_id;        // Unique panel identifier (e.g., "dungeon.room_selector")
  std::string display_name;    // Human-readable name
  std::string icon;            // Icon identifier (e.g., "ICON_MD_LIST")
  
  // Size configuration
  ImVec2 size = ImVec2(-1, -1);  // Size in pixels (-1 = auto)
  float size_ratio = 0.0f;       // Size ratio for dock splits (0.0 to 1.0)
  
  // Visibility and priority
  bool visible_by_default = true;
  int priority = 100;
  
  // Panel flags
  bool closable = true;
  bool minimizable = true;
  bool pinnable = true;
  bool headless = false;
  bool docking_allowed = true;
  ImGuiWindowFlags flags = ImGuiWindowFlags_None;
  
  // Runtime state (for preview)
  ImGuiID dock_id = 0;
  bool is_floating = false;
  ImVec2 floating_pos = ImVec2(100, 100);
  ImVec2 floating_size = ImVec2(400, 300);
};

/**
 * @struct DockNode
 * @brief Represents a dock node in the layout tree
 * 
 * Hierarchical structure representing the docking layout.
 * Can be a split (with two children) or a leaf (containing panels).
 */
struct DockNode {
  DockNodeType type = DockNodeType::Leaf;
  ImGuiID node_id = 0;
  
  // Split configuration (for type == Split)
  ImGuiDir split_dir = ImGuiDir_None;  // Left, Right, Up, Down
  float split_ratio = 0.5f;             // Split ratio (0.0 to 1.0)
  
  // Children (for type == Split)
  std::unique_ptr<DockNode> child_left;
  std::unique_ptr<DockNode> child_right;
  
  // Panels (for type == Leaf)
  std::vector<LayoutPanel> panels;
  
  // Dock node flags
  ImGuiDockNodeFlags flags = ImGuiDockNodeFlags_None;
  
  // Helper methods
  bool IsSplit() const { return type == DockNodeType::Split; }
  bool IsLeaf() const { return type == DockNodeType::Leaf || type == DockNodeType::Root; }
  bool IsRoot() const { return type == DockNodeType::Root; }
  
  // Add a panel to this leaf node
  void AddPanel(const LayoutPanel& panel);
  
  // Split this node in a direction
  void Split(ImGuiDir direction, float ratio);
  
  // Find a panel by ID in the tree
  LayoutPanel* FindPanel(const std::string& panel_id);
  
  // Count total panels in the tree
  size_t CountPanels() const;
  
  // Clone the node and its children
  std::unique_ptr<DockNode> Clone() const;
};

/**
 * @struct LayoutDefinition
 * @brief Complete layout definition with metadata
 * 
 * Represents a full workspace layout that can be saved, loaded,
 * and applied to the editor. Includes the dock tree and metadata.
 */
struct LayoutDefinition {
  // Identity
  std::string name;
  std::string description;
  EditorType editor_type = EditorType::kUnknown;
  
  // Layout structure
  std::unique_ptr<DockNode> root;
  ImVec2 canvas_size = ImVec2(1920, 1080);
  
  // Metadata
  std::string author;
  std::string version = "1.0.0";
  int64_t created_timestamp = 0;
  int64_t modified_timestamp = 0;
  
  // Helper methods
  
  /**
   * @brief Create a default empty layout
   */
  static LayoutDefinition CreateEmpty(const std::string& name);
  
  /**
   * @brief Clone the layout definition
   */
  std::unique_ptr<LayoutDefinition> Clone() const;
  
  /**
   * @brief Find a panel by ID anywhere in the layout
   */
  LayoutPanel* FindPanel(const std::string& panel_id) const;
  
  /**
   * @brief Get all panels in the layout
   */
  std::vector<LayoutPanel*> GetAllPanels() const;
  
  /**
   * @brief Validate the layout structure
   * @return true if layout is valid
   */
  bool Validate(std::string* error_message = nullptr) const;
  
  /**
   * @brief Update the modified timestamp to current time
   */
  void Touch();
};

}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_LAYOUT_DESIGNER_LAYOUT_DEFINITION_H_
