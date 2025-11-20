#ifndef YAZE_APP_EDITOR_UI_LAYOUT_MANAGER_H_
#define YAZE_APP_EDITOR_UI_LAYOUT_MANAGER_H_

#include <string>
#include <unordered_map>

#include "app/editor/editor.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

/**
 * @class LayoutManager
 * @brief Manages ImGui DockBuilder layouts for each editor type
 * 
 * Provides professional default layouts using ImGui's DockBuilder API,
 * similar to VSCode's workspace layouts. Each editor type has a custom
 * layout optimized for its workflow.
 * 
 * Features:
 * - Per-editor default layouts (Overworld, Dungeon, Graphics, etc.)
 * - Layout persistence and restoration
 * - Workspace presets (Developer, Designer, Modder)
 * - Dynamic layout initialization on first editor switch
 */
class LayoutManager {
 public:
  LayoutManager() = default;
  ~LayoutManager() = default;

  /**
   * @brief Initialize the default layout for a specific editor type
   * @param type The editor type to initialize
   * @param dockspace_id The ImGui dockspace ID to build the layout in
   */
  void InitializeEditorLayout(EditorType type, ImGuiID dockspace_id);

  /**
   * @brief Save the current layout with a custom name
   * @param name The name to save the layout under
   */
  void SaveCurrentLayout(const std::string& name);

  /**
   * @brief Load a saved layout by name
   * @param name The name of the layout to load
   */
  void LoadLayout(const std::string& name);

  /**
   * @brief Reset the layout for an editor to its default
   * @param type The editor type to reset
   */
  void ResetToDefaultLayout(EditorType type);

  /**
   * @brief Check if a layout has been initialized for an editor
   * @param type The editor type to check
   * @return True if layout is initialized
   */
  bool IsLayoutInitialized(EditorType type) const;

  /**
   * @brief Mark a layout as initialized
   * @param type The editor type to mark
   */
  void MarkLayoutInitialized(EditorType type);

  /**
   * @brief Clear all initialization flags (for testing)
   */
  void ClearInitializationFlags();

 private:
  // DockBuilder layout implementations for each editor type
  void BuildOverworldLayout(ImGuiID dockspace_id);
  void BuildDungeonLayout(ImGuiID dockspace_id);
  void BuildGraphicsLayout(ImGuiID dockspace_id);
  void BuildPaletteLayout(ImGuiID dockspace_id);
  void BuildScreenLayout(ImGuiID dockspace_id);
  void BuildMusicLayout(ImGuiID dockspace_id);
  void BuildSpriteLayout(ImGuiID dockspace_id);
  void BuildMessageLayout(ImGuiID dockspace_id);
  void BuildAssemblyLayout(ImGuiID dockspace_id);
  void BuildSettingsLayout(ImGuiID dockspace_id);

  // Track which layouts have been initialized
  std::unordered_map<EditorType, bool> layouts_initialized_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_UI_LAYOUT_MANAGER_H_
