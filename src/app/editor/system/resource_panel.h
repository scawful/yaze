#ifndef YAZE_APP_EDITOR_SYSTEM_RESOURCE_PANEL_H_
#define YAZE_APP_EDITOR_SYSTEM_RESOURCE_PANEL_H_

#include <string>

#include "absl/strings/str_format.h"
#include "app/editor/system/editor_panel.h"

namespace yaze {
namespace editor {

/**
 * @class ResourcePanel
 * @brief Base class for panels that edit specific ROM resources
 *
 * A ResourcePanel represents a window for editing a specific piece of
 * data within a ROM, such as a dungeon room, a song, or a graphics sheet.
 *
 * Key Features:
 * - **Session-aware**: Can distinguish between same resource in different ROMs
 * - **Multi-instance**: Multiple resources can be open simultaneously
 * - **LRU managed**: Oldest panels auto-close when limit reached
 *
 * @section Resource Panel ID Format
 * Resource panel IDs follow a specific format:
 * ```
 * [{session}.]{category}.{resource_type}_{resource_id}[.{subpanel}]
 *
 * Examples:
 *   dungeon.room_42           -- Room 42 (single session)
 *   s0.dungeon.room_42        -- Room 42 in session 0 (multi-session)
 *   s1.dungeon.room_42        -- Room 42 in session 1 (different ROM)
 *   music.song_5.piano_roll   -- Piano roll subpanel for song 5
 * ```
 *
 * @section Subclasses
 * Typical subclasses include:
 * - DungeonRoomPanel: Edits a specific room (0-295)
 * - MusicSongPanel: Edits a specific song with tracker/piano roll
 * - GraphicsSheetPanel: Edits a specific GFX sheet
 * - OverworldMapPanel: Edits a specific overworld map
 *
 * @section Example Implementation
 * ```cpp
 * class DungeonRoomPanel : public ResourcePanel {
 *  public:
 *   DungeonRoomPanel(size_t session_id, int room_id, zelda3::Room* room)
 *       : room_id_(room_id), room_(room) {
 *     session_id_ = session_id;
 *   }
 *
 *   int GetResourceId() const override { return room_id_; }
 *   std::string GetResourceType() const override { return "room"; }
 *   std::string GetIcon() const override { return ICON_MD_DOOR_FRONT; }
 *   std::string GetEditorCategory() const override { return "Dungeon"; }
 *
 *   void Draw(bool* p_open) override {
 *     // Draw room canvas with objects, sprites, etc.
 *     DrawRoomCanvas();
 *   }
 *
 *  private:
 *   int room_id_;
 *   zelda3::Room* room_;
 * };
 * ```
 *
 * @see EditorPanel - Base interface for all panels
 * @see PanelManager - Manages resource panel lifecycle and limits
 */
class ResourcePanel : public EditorPanel {
 public:
  virtual ~ResourcePanel() = default;

  // ==========================================================================
  // Resource Identity (Required)
  // ==========================================================================

  /**
   * @brief The numeric ID of the resource
   * @return Resource ID (room_id, song_index, sheet_id, map_id, etc.)
   *
   * This is the primary key for the resource within its type.
   */
  virtual int GetResourceId() const = 0;

  /**
   * @brief The resource type name
   * @return Type string (e.g., "room", "song", "sheet", "map")
   *
   * Used in panel ID generation and display.
   */
  virtual std::string GetResourceType() const = 0;

  /**
   * @brief Human-readable resource name
   * @return Friendly name (e.g., "Hyrule Castle Entrance", "Overworld Theme")
   *
   * Default implementation returns "{type} {id}".
   * Override to provide game-specific names from ROM data.
   */
  virtual std::string GetResourceName() const {
    return absl::StrFormat("%s %d", GetResourceType(), GetResourceId());
  }

  // ==========================================================================
  // Panel Identity (from EditorPanel - auto-generated)
  // ==========================================================================

  /**
   * @brief Generated panel ID from resource type and ID
   * @return ID in format "{category}.{type}_{id}"
   */
  std::string GetId() const override {
    return absl::StrFormat("%s.%s_%d", GetEditorCategory(), GetResourceType(),
                           GetResourceId());
  }

  /**
   * @brief Generated display name from resource name
   * @return The resource name
   */
  std::string GetDisplayName() const override { return GetResourceName(); }

  // ==========================================================================
  // Behavior (from EditorPanel - resource-specific defaults)
  // ==========================================================================

  /**
   * @brief Resource panels use CrossEditor category for opt-in persistence
   * @return PanelCategory::CrossEditor
   *
   * Resource panels (rooms, songs, etc.) can be pinned to persist across
   * editor switches. By default, they're NOT pinned and will be hidden
   * (but not closed) when switching to another editor.
   *
   * Pin behavior:
   * - Open a room → NOT pinned, hidden when switching editors
   * - Pin it → stays visible across all editors
   * - Unpin it → hidden when switching editors
   * - Close via X → fully removed regardless of pin state
   *
   * The drawing loops in each editor handle the category filtering.
   */
  PanelCategory GetPanelCategory() const override {
    return PanelCategory::CrossEditor;
  }

  /**
   * @brief Whether multiple instances of this resource type can be open
   * @return true to allow multiple (default), false for singleton behavior
   */
  virtual bool AllowMultipleInstances() const { return true; }

  // ==========================================================================
  // Session Support
  // ==========================================================================

  /**
   * @brief Get the session ID this resource belongs to
   * @return Session ID (0 for single-ROM mode)
   *
   * In multi-ROM editing mode, each loaded ROM gets a session ID.
   * This allows the same resource (e.g., room 42) to be open for
   * different ROMs simultaneously.
   */
  virtual size_t GetSessionId() const { return session_id_; }

  /**
   * @brief Set the session ID for this resource panel
   * @param session_id The session ID to set
   */
  void SetSessionId(size_t session_id) { session_id_ = session_id; }

  // ==========================================================================
  // Resource Lifecycle Hooks
  // ==========================================================================

  /**
   * @brief Called when resource data changes externally
   *
   * Override to refresh panel state when the underlying ROM data
   * is modified by another editor or operation.
   */
  virtual void OnResourceModified() {}

  /**
   * @brief Called when resource is deleted from ROM
   *
   * Default behavior: the panel should be closed.
   * Override to implement custom cleanup or warnings.
   */
  virtual void OnResourceDeleted() {
    // Default: PanelManager will close this panel
  }

 protected:
  /// Session ID for multi-ROM editing (0 = single session)
  size_t session_id_ = 0;
};

/**
 * @namespace ResourcePanelLimits
 * @brief Default limits for resource panel counts
 *
 * To prevent memory bloat, enforce limits on how many resource panels
 * can be open simultaneously. When limits are reached, the oldest
 * (least recently used) panel is automatically closed.
 */
namespace ResourcePanelLimits {
  /// Maximum open room panels (dungeon editor)
  constexpr size_t kMaxRoomPanels = 8;

  /// Maximum open song panels (music editor)
  constexpr size_t kMaxSongPanels = 4;

  /// Maximum open graphics sheet panels
  constexpr size_t kMaxSheetPanels = 6;

  /// Maximum open map panels (overworld editor)
  constexpr size_t kMaxMapPanels = 8;

  /// Maximum total resource panels across all types
  constexpr size_t kMaxTotalResourcePanels = 20;
}  // namespace ResourcePanelLimits

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_RESOURCE_PANEL_H_
