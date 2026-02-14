#ifndef YAZE_APP_EDITOR_LAYOUT_LAYOUT_MANAGER_H_
#define YAZE_APP_EDITOR_LAYOUT_LAYOUT_MANAGER_H_

#include <string>
#include <unordered_map>
#include <vector>

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
 * @enum LayoutScope
 * @brief Storage scope for saved layouts
 */
enum class LayoutScope {
  kGlobal,
  kProject
};

/**
 * @brief Built-in workflow-oriented layout profiles.
 */
struct LayoutProfile {
  std::string id;          // Stable id (e.g. "code", "debug")
  std::string label;       // UI-facing label
  std::string description; // Short profile summary
  std::string preset_name; // Backing LayoutPresets name
  bool open_agent_chat = false;
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
   * @param persist Whether to write the layout to disk
   */
  void SaveCurrentLayout(const std::string& name, bool persist = true);

  /**
   * @brief Load a saved layout by name
   * @param name The name of the layout to load
   */
  void LoadLayout(const std::string& name);

  /**
   * @brief Capture the current workspace as a temporary session layout.
   *
   * Temporary layouts are held in memory only and are never persisted to disk.
   */
  void CaptureTemporarySessionLayout(size_t session_id);

  /**
   * @brief Restore the temporary session layout if one has been captured.
   * @return true when a captured layout was restored
   */
  bool RestoreTemporarySessionLayout(size_t session_id,
                                     bool clear_after_restore = false);

  /**
   * @brief Whether a temporary session layout is available.
   */
  bool HasTemporarySessionLayout() const { return has_temp_session_layout_; }

  /**
   * @brief Clear temporary session layout snapshot state.
   */
  void ClearTemporarySessionLayout();

  /**
   * @brief Delete a saved layout by name
   * @param name The name of the layout to delete
   * @return true if layout was deleted, false if not found
   */
  bool DeleteLayout(const std::string& name);

  /**
   * @brief Get built-in layout profiles.
   */
  static std::vector<LayoutProfile> GetBuiltInProfiles();

  /**
   * @brief Apply a built-in layout profile by profile ID.
   * @param profile_id Stable profile id from GetBuiltInProfiles()
   * @param session_id Active session id
   * @param editor_type Current editor type (for context-sensitive profiles)
   * @param out_profile Optional resolved profile metadata
   * @return true when a matching profile was applied
   */
  bool ApplyBuiltInProfile(const std::string& profile_id, size_t session_id,
                           EditorType editor_type,
                           LayoutProfile* out_profile = nullptr);

  /**
   * @brief Get list of all saved layout names
   * @return Vector of layout names
   */
  std::vector<std::string> GetSavedLayoutNames() const;

  /**
   * @brief Check if a layout exists
   * @param name The layout name to check
   * @return true if layout exists
   */
  bool HasLayout(const std::string& name) const;

  /**
   * @brief Load layouts for the active scope (global + optional project)
   */
  void LoadLayoutsFromDisk();

  /**
   * @brief Set the active project layout key (enables project scope)
   * @param key Stable project storage key (sanitized)
   */
  void SetProjectLayoutKey(const std::string& key);

  /**
   * @brief Clear project scope and return to global layouts only
   */
  void UseGlobalLayouts();

  /**
   * @brief Get the active layout scope
   */
  LayoutScope GetActiveScope() const;

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

  void LoadLayoutsFromDiskInternal(LayoutScope scope, bool merge);
  void SaveLayoutsToDisk(LayoutScope scope) const;

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

  // Saved layouts (panel visibility state)
  std::unordered_map<std::string, std::unordered_map<std::string, bool>>
      saved_layouts_;

  // In-memory temporary session snapshot (never persisted).
  bool has_temp_session_layout_ = false;
  size_t temp_session_id_ = 0;
  std::unordered_map<std::string, bool> temp_session_visibility_;
  std::unordered_map<std::string, bool> temp_session_pinned_;
  std::string temp_session_imgui_layout_;

  // Saved ImGui docking layouts (INI data)
  std::unordered_map<std::string, std::string> saved_imgui_layouts_;

  // Saved pinned panel state for layouts
  std::unordered_map<std::string, std::unordered_map<std::string, bool>>
      saved_pinned_layouts_;

  // Layout scope tracking
  std::unordered_map<std::string, LayoutScope> layout_scopes_;

  // Project storage key for project-scoped layouts
  std::string project_layout_key_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_LAYOUT_LAYOUT_MANAGER_H_
