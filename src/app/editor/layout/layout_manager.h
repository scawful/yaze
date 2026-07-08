#ifndef YAZE_APP_EDITOR_LAYOUT_LAYOUT_MANAGER_H_
#define YAZE_APP_EDITOR_LAYOUT_LAYOUT_MANAGER_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/editor/editor.h"
#include "app/editor/layout/layout_designer/dock_tree.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

// Forward declaration
class WorkspaceWindowManager;
class UserSettings;

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
enum class LayoutScope { kGlobal, kProject };

/**
 * @brief Built-in workflow-oriented layout profiles.
 */
struct LayoutProfile {
  std::string id;           // Stable id (e.g. "code", "debug")
  std::string label;        // UI-facing label
  std::string description;  // Short profile summary
  std::string preset_name;  // Backing LayoutPresets name
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
   * @brief Set the window manager for window title lookups
   * @param manager Pointer to the WorkspaceWindowManager
   */
  void SetWindowManager(WorkspaceWindowManager* manager) {
    window_manager_ = manager;
  }

  /**
   * @brief Get the window manager
   * @return Pointer to the WorkspaceWindowManager
   */
  WorkspaceWindowManager* window_manager() const { return window_manager_; }

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
   * @brief Capture the current workspace under a named, session-scoped slot.
   *
   * Named snapshots are held in memory only (never persisted) and identified
   * by the caller-supplied name. Names are used as the stable lookup key;
   * duplicate saves overwrite the existing entry.
   */
  bool SaveNamedSnapshot(const std::string& name, size_t session_id);

  /**
   * @brief Restore a named snapshot by name.
   */
  bool RestoreNamedSnapshot(const std::string& name, size_t session_id,
                            bool remove_after_restore = false);

  /**
   * @brief Delete a named snapshot. Returns true if an entry was removed.
   */
  bool DeleteNamedSnapshot(const std::string& name);

  /**
   * @brief List all named snapshots for the given session.
   */
  std::vector<std::string> ListNamedSnapshots(size_t session_id) const;

  /**
   * @brief Whether a named snapshot with the given name exists.
   */
  bool HasNamedSnapshot(const std::string& name) const;

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

  // ==========================================================================
  // DockTree integration (Layout Designer)
  // ==========================================================================

  /**
   * @brief Apply a DockTree to the given dockspace.
   *
   * Removes and rebuilds `dockspace_id` from scratch, walking the tree
   * and calling `DockBuilder{SplitNode,DockWindow,Finish}`.
   *
   * Side effects beyond DockBuilder, added in Phase 8 review (2026-04-24):
   *   - Calls `WorkspaceWindowManager::OpenWindow(panel_id)` for every
   *     panel referenced in the tree, so the user actually sees the
   *     panels they docked rather than empty slots.
   *   - Marks every `EditorType`'s layout as initialized so the lazy
   *     `InitializeEditorLayout` path in EditorActivator doesn't blow
   *     the custom layout away on the first panel-based editor
   *     activation. The Layout Designer's apply is an explicit "this
   *     is my workspace" — the per-editor preset must yield to it.
   *
   * Panels in the tree whose `panel_id` is not registered against the
   * bound WorkspaceWindowManager are skipped with a best-effort fallback
   * that reconstructs the window title from the PanelEntry's cached
   * icon + display_name + id.
   *
   * @param tree          Tree to apply. Must pass `Validate()`.
   * @param dockspace_id  ImGui dockspace to rebuild.
   * @return FailedPrecondition when no WorkspaceWindowManager is bound;
   *         InvalidArgument when the tree fails validation; OK otherwise.
   */
  absl::Status ApplyDockTree(const layout_designer::DockTree& tree,
                             ImGuiID dockspace_id);

  /**
   * @brief Record the ImGui ID of the main dockspace from its authoring
   *        scope.
   *
   * ImGui IDs are salted by the current window's ID stack, so
   * `ImGui::GetID("MainDockSpace")` only resolves to the live dockspace
   * when called from inside the window that hosts it (see
   * `controller.cc::Render`). Panels drawn in other windows (e.g. the
   * Layout Designer, which lives in its own PanelWindow) would hash a
   * different ID and end up driving an orphan dock node. The controller
   * calls this setter every frame from the correct scope; consumers
   * read via `GetMainDockspaceId()`.
   */
  void SetMainDockspaceId(ImGuiID id) { main_dockspace_id_ = id; }

  /**
   * @brief Get the cached main dockspace ID.
   * @return The ID cached by `SetMainDockspaceId`, or 0 before the first
   *         frame of the DockSpaceWindow has rendered.
   */
  ImGuiID GetMainDockspaceId() const { return main_dockspace_id_; }

  /**
   * @brief Test-only: reset the one-shot startup-reapply guard so a
   *        single test fixture can drive `MaybeReapplyStartupLayout`
   *        through multiple scenarios without standing up a fresh
   *        manager each time.
   */
  void reset_startup_layout_consumed_for_test() {
    startup_layout_consumed_ = false;
  }
  bool startup_layout_consumed_for_test() const {
    return startup_layout_consumed_;
  }
  bool startup_reapply_pending_protection_for_test() const {
    return startup_reapply_pending_protection_;
  }
  void set_current_editor_type_for_test(EditorType type) {
    current_editor_type_ = type;
  }

  /**
   * @brief One-shot startup hook that re-applies the user's last
   *        applied named layout, if any.
   *
   * Drives idempotently from `UserSettings::last_applied_layout_name`
   * + `named_layouts`. The first frame this is called with a valid
   * `main_dockspace_id_` (i.e. after the controller has rendered at
   * least one DockSpaceWindow), the matching layout is parsed,
   * validated, and applied to the live dockspace; subsequent calls
   * are no-ops.
   *
   * Failure modes are absorbed (logged, internally consumed) rather
   * than propagated, because there is no useful caller-side recovery
   * for "the layout the user picked yesterday no longer parses": the
   * editor falls through to whatever default layout the workspace
   * shell already built. The Status return is for observability and
   * tests.
   *
   * Skipped paths (return OK, leave the consumed flag clear so the
   * caller can retry next frame):
   *   - `main_dockspace_id_ == 0` (dockspace not bound yet).
   *
   * Skipped paths (return OK, set consumed):
   *   - `settings == nullptr`.
   *   - `last_applied_layout_name` is empty.
   *
   * Failure paths (return error status, set consumed):
   *   - The named layout is missing from `named_layouts`.
   *   - Its body fails to parse / validate.
   *   - `ApplyDockTree` returns non-OK.
   *
   * @param settings  UserSettings to read `named_layouts` and
   *                  `last_applied_layout_name` from. May be null.
   * @return OK on legitimate skip or successful apply; the underlying
   *         error otherwise.
   */
  absl::Status MaybeReapplyStartupLayout(UserSettings* settings);

  /**
   * @brief Capture the current docking state into a DockTree.
   *
   * Walks ImGui's internal dock node tree rooted at `dockspace_id` and
   * samples splits + leaf windows. Unknown windows (those not registered
   * against the bound WorkspaceWindowManager) are dropped. Split ratios
   * are recovered from measured node sizes; X-axis splits always emit
   * `SplitDirection::kLeft` and Y-axis splits emit `kUp` (direction is
   * lossy, since a 0.4 right-placed split is visually identical to a
   * 0.4 left-placed split).
   *
   * @param dockspace_id  ImGui dockspace to sample.
   * @return FailedPrecondition when no WorkspaceWindowManager is bound;
   *         NotFound when no node exists at `dockspace_id`; the captured
   *         tree otherwise.
   */
  absl::StatusOr<layout_designer::DockTree> CaptureDockTree(
      ImGuiID dockspace_id) const;

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
  WorkspaceWindowManager* window_manager_ = nullptr;

  // Current layout type
  LayoutType current_layout_type_ = LayoutType::kDefault;

  // Rebuild flag
  bool rebuild_requested_ = false;

  // Last used dockspace ID (for rebuild operations)
  ImGuiID last_dockspace_id_ = 0;

  // Main dockspace ID captured from the authoritative scope (see
  // `SetMainDockspaceId`). 0 until the controller has rendered at least
  // one DockSpaceWindow frame.
  ImGuiID main_dockspace_id_ = 0;

  // True after `MaybeReapplyStartupLayout` has fired for this session
  // (whether successfully or with a logged error). Guards the one-shot
  // semantics across the per-frame call cadence.
  bool startup_layout_consumed_ = false;

  // Set by `MaybeReapplyStartupLayout` after a successful apply, then
  // consumed by the very next `InitializeEditorLayout` call (any
  // editor type). Phase 8.2 review (2026-04-25): without this flag the
  // first editor activation post-startup would blow away the
  // freshly-reapplied custom layout via lazy preset init. The flag is
  // intentionally global-one-shot rather than per-editor because at
  // startup we don't know which editor the user will activate first;
  // the very first lazy init is always the dangerous one.
  bool startup_reapply_pending_protection_ = false;

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

  // Multi-slot named session snapshots (in-memory only, never persisted).
  // The user-supplied name is the stable key; duplicate saves overwrite.
  struct SessionSnapshot {
    size_t session_id = 0;
    std::unordered_map<std::string, bool> visibility;
    std::unordered_map<std::string, bool> pinned;
    std::string imgui_layout;
  };
  std::unordered_map<std::string, SessionSnapshot> named_snapshots_;

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
