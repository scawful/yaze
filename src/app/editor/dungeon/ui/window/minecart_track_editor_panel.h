#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_MINECART_TRACK_EDITOR_PANEL_H
#define YAZE_APP_EDITOR_DUNGEON_PANELS_MINECART_TRACK_EDITOR_PANEL_H

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/editor/dungeon/dungeon_room_store.h"
#include "app/editor/dungeon/minecart_track_source.h"
#include "app/editor/system/workspace/editor_panel.h"
#include "app/gui/core/icons.h"
#include "core/project.h"
#include "rom/rom.h"
#include "zelda3/dungeon/room.h"

namespace yaze::editor {

class MinecartTrackEditorPanel : public WindowContent {
 public:
  using ProjectChangedCallback =
      std::function<absl::Status(const project::DungeonOverlaySettings&)>;
  using ProjectDraftChangedCallback = std::function<absl::Status()>;
  using ProjectSaveCallback = std::function<absl::Status()>;

  MinecartTrackEditorPanel() = default;

  // WindowContent overrides
  std::string GetId() const override { return "dungeon.minecart_tracks"; }
  std::string GetDisplayName() const override { return "Minecart Tracks"; }
  std::string GetIcon() const override { return ICON_MD_TRAIN; }
  std::string GetEditorCategory() const override { return "Dungeon"; }

  void Draw(bool* p_open) override;

  // Custom methods
  void SetRooms(DungeonRoomStore* rooms) {
    rooms_ = rooms;
    audit_dirty_ = true;
  }
  absl::Status SetProject(project::YazeProject* project);
  // Reapply project-backed panel state even when the stable session project
  // pointer and descriptor path did not change.
  absl::Status RebindProjectContext(project::YazeProject* project);
  void SetRom(Rom* rom) { rom_ = rom; }
  absl::Status SaveTracks();
  absl::Status ReloadTracks();
  absl::Status DiscardUnpublishedChanges();
  bool HasUnpublishedChanges() const;
  absl::StatusOr<std::filesystem::path> ResolveTrackSourcePath() const;
  absl::Status UpdateTrack(size_t track_index, const MinecartTrack& track);
  bool HasPendingProjectDraftChanges() const;
  absl::Status PrepareProjectSave();

  // Coordinate picking from dungeon canvas
  // When picking mode is active, the next canvas click will set the coordinates
  // for the selected track slot
  void SetPickedCoordinates(int room_id, uint16_t camera_x, uint16_t camera_y);
  bool IsPickingCoordinates() const { return picking_mode_; }
  int GetPickingTrackIndex() const { return picking_track_index_; }
  const std::vector<MinecartTrack>& GetTracks();

  // Callback to navigate to a specific room for coordinate picking
  using RoomNavigationCallback = std::function<void(int room_id)>;
  void SetRoomNavigationCallback(RoomNavigationCallback callback) {
    room_navigation_callback_ = std::move(callback);
  }
  void SetProjectChangedCallback(ProjectChangedCallback callback) {
    project_changed_callback_ = std::move(callback);
  }
  void SetProjectDraftChangedCallback(ProjectDraftChangedCallback callback) {
    project_draft_changed_callback_ = std::move(callback);
  }
  void SetProjectSaveCallback(ProjectSaveCallback callback) {
    project_save_callback_ = std::move(callback);
  }

 private:
  friend class MinecartTrackEditorPanelTestPeer;

  absl::Status LoadTracks();
  absl::Status ValidateLoadedManifestIdentity() const;
  absl::Status RefreshProjectBinding();
  void ResetTrackSession();
  void StartCoordinatePicking(int track_index);
  void CancelCoordinatePicking();
  void RebuildAuditCache();
  bool IsDefaultTrack(const MinecartTrack& track) const;
  void DrawOverlaySettings();
  void InitializeOverlayInputs();
  void ClearOverlayInputs();

  using OverlayListMember =
      std::vector<uint16_t> project::DungeonOverlaySettings::*;
  bool UpdateOverlayList(const char* label, std::string& input,
                         OverlayListMember member);
  absl::StatusOr<bool> CommitOverlayList(std::string& input,
                                         OverlayListMember member);
  absl::StatusOr<bool> CommitOverlayInputsForSave();
  absl::StatusOr<bool> ResetOverlaySettings();
  absl::Status NotifyProjectChanged(
      const project::DungeonOverlaySettings& overlay);
  absl::Status NotifyProjectDraftChanged();
  absl::Status SaveProjectSettings();

  struct RoomTrackAudit {
    bool has_track_collision = false;
    bool has_stop_tiles = false;
    bool has_minecart_sprite = false;
    bool has_minecart_on_stop = false;
    std::vector<int> track_subtypes;
  };

  std::vector<MinecartTrack> tracks_;
  std::vector<MinecartTrack> loaded_tracks_;
  std::optional<MinecartTrackSourceDocument> source_document_;
  std::optional<core::MinecartTrackLayout::Source> loaded_source_identity_;
  std::filesystem::path loaded_source_path_;
  std::string loaded_source_sha256_;
  std::string bound_project_filepath_;
  std::optional<core::MinecartTrackLayout::Source> bound_source_identity_;
  Rom* rom_ = nullptr;
  DungeonRoomStore* rooms_ = nullptr;
  project::YazeProject* project_ = nullptr;
  std::unordered_map<int, RoomTrackAudit> room_audit_;
  std::unordered_map<int, std::vector<int>> track_usage_rooms_;
  std::vector<bool> track_subtype_used_;
  bool audit_dirty_ = true;
  bool load_attempted_ = false;
  bool loaded_ = false;
  std::string status_message_;
  bool show_success_ = false;
  float success_timer_ = 0.0f;

  // Coordinate picking state
  bool picking_mode_ = false;
  int picking_track_index_ = -1;

  // Last picked coordinates (for display)
  uint16_t last_picked_x_ = 0;
  uint16_t last_picked_y_ = 0;
  bool has_picked_coords_ = false;

  RoomNavigationCallback room_navigation_callback_;
  ProjectChangedCallback project_changed_callback_;
  ProjectDraftChangedCallback project_draft_changed_callback_;
  ProjectSaveCallback project_save_callback_;

  // Overlay config input state
  bool overlay_inputs_initialized_ = false;
  std::optional<project::DungeonOverlaySettings> overlay_inputs_model_;
  std::string overlay_track_tiles_input_;
  std::string overlay_track_stop_tiles_input_;
  std::string overlay_track_switch_tiles_input_;
  std::string overlay_track_object_ids_input_;
  std::string overlay_minecart_sprite_ids_input_;
};

}  // namespace yaze::editor

#endif
