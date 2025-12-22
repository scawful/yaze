#ifndef YAZE_APP_EDITOR_LAYOUT_LAYOUT_MANAGER_H_
#define YAZE_APP_EDITOR_LAYOUT_LAYOUT_MANAGER_H_

#include <string>
#include <unordered_map>

#include "app/editor/editor.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

// Forward declaration
class PanelManager;

/**
 * @enum LayoutType
 * @brief Predefined layout types for different editor workflows
 */
enum class LayoutType {
  kDefault,
  kOverworld,
  kDungeon,
  kGraphics,
  kPalette,
  kScreen,
  kMusic,
  kSprite,
  kMessage,
  kAssembly,
  kSettings,
  kEmulator
};

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
 * - Panel manager integration for window title lookups
 */
class LayoutManager {
 public:
  LayoutManager() = default;
  ~LayoutManager() = default;

  /**
   * @brief Set the panel manager for window title lookups
   * @param registry Pointer to the PanelManager
   */
  void SetPanelManager(PanelManager* manager) {
    panel_manager_ = manager;
  }

  /**
   * @brief Get the panel manager
   * @return Pointer to the PanelManager
   */
  PanelManager* panel_manager() const { return panel_manager_; }

  /**
   * @brief Initialize the default layout for a specific editor type
   * @param type The editor type to initialize
   * @param dockspace_id The ImGui dockspace ID to build the layout in
   */
  void InitializeEditorLayout(EditorType type, ImGuiID dockspace_id);

  /**
   * @brief Force rebuild of layout for a specific editor type
   * @param type The editor type to rebuild
   * @param dockspace_id The ImGui dockspace ID to build the layout in
   * 
   * This method rebuilds the layout even if it was already initialized.
   * Useful for resetting layouts to their default state.
   */
  void RebuildLayout(EditorType type, ImGuiID dockspace_id);

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

  /**
   * @brief Set the current layout type for rebuild
   * @param type The layout type to set
   */
  void SetLayoutType(LayoutType type) { current_layout_type_ = type; }

  /**
   * @brief Get the current layout type
   */
  LayoutType GetLayoutType() const { return current_layout_type_; }

  /**
   * @brief Request a layout rebuild on next frame
   */
  void RequestRebuild() { rebuild_requested_ = true; }

  /**
   * @brief Check if rebuild was requested
   */
  bool IsRebuildRequested() const { return rebuild_requested_; }

  /**
   * @brief Clear rebuild request flag
   */
  void ClearRebuildRequest() { rebuild_requested_ = false; }

  /**
   * @brief Get window title for a card ID from registry
   * @param card_id The card ID to look up
   * @return Window title or empty string if not found
   */
  std::string GetWindowTitle(const std::string& card_id) const;

 private:
  // DockBuilder layout implementations for each editor type
  void BuildLayoutFromPreset(EditorType type, ImGuiID dockspace_id);

  // Deprecated individual builders - kept for compatibility or as wrappers
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
  void BuildEmulatorLayout(ImGuiID dockspace_id);

  // Track which layouts have been initialized
  std::unordered_map<EditorType, bool> layouts_initialized_;

  // Panel manager for window title lookups
  PanelManager* panel_manager_ = nullptr;

  // Current layout type
  LayoutType current_layout_type_ = LayoutType::kDefault;

  // Rebuild flag
  bool rebuild_requested_ = false;

  // Last used dockspace ID (for rebuild operations)
  ImGuiID last_dockspace_id_ = 0;

  // Current editor type being displayed
  EditorType current_editor_type_ = EditorType::kUnknown;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_LAYOUT_LAYOUT_MANAGER_H_

