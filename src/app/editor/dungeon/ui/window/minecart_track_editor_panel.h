#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_MINECART_TRACK_EDITOR_PANEL_H
#define YAZE_APP_EDITOR_DUNGEON_PANELS_MINECART_TRACK_EDITOR_PANEL_H

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/editor/dungeon/dungeon_room_store.h"
#include "app/gui/core/icons.h"
#include "core/project.h"
#include "rom/rom.h"
#include "zelda3/dungeon/room.h"

namespace yaze::editor {

struct MinecartTrack {
  int id;
  int room_id;
  int start_x;
  int start_y;

  bool operator==(const MinecartTrack&) const = default;
};

}  // namespace yaze::editor

#include "app/editor/system/workspace/editor_panel.h"

namespace yaze::editor {

class MinecartTrackEditorPanel : public WindowContent {
 public:
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
  void SetRom(Rom* rom) { rom_ = rom; }
  absl::Status SaveTracks();
  absl::Status ReloadTracks();
  absl::Status DiscardUnpublishedChanges();
  bool HasUnpublishedChanges() const;
  absl::StatusOr<std::filesystem::path> ResolveTrackSourcePath() const;
  absl::Status UpdateTrack(size_t track_index, const MinecartTrack& track);

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

 private:
  struct ParsedSection {
    std::vector<int> values;
    bool guarded = false;
  };

  absl::Status LoadTracks();
  static absl::StatusOr<ParsedSection> ParseSection(const std::string& content,
                                                    const std::string& label);
  absl::Status RefreshProjectBinding();
  void ResetTrackSession();
  void StartCoordinatePicking(int track_index);
  void CancelCoordinatePicking();
  void RebuildAuditCache();
  bool IsDefaultTrack(const MinecartTrack& track) const;
  void DrawOverlaySettings();
  void InitializeOverlayInputs();
  bool UpdateOverlayList(const char* label, std::string& input,
                         std::vector<uint16_t>& target);

  struct RoomTrackAudit {
    bool has_track_collision = false;
    bool has_stop_tiles = false;
    bool has_minecart_sprite = false;
    bool has_minecart_on_stop = false;
    std::vector<int> track_subtypes;
  };

  std::vector<MinecartTrack> tracks_;
  std::vector<MinecartTrack> loaded_tracks_;
  std::string bound_project_filepath_;
  Rom* rom_ = nullptr;
  DungeonRoomStore* rooms_ = nullptr;
  project::YazeProject* project_ = nullptr;
  std::unordered_map<int, RoomTrackAudit> room_audit_;
  std::unordered_map<int, std::vector<int>> track_usage_rooms_;
  std::vector<bool> track_subtype_used_;
  bool audit_dirty_ = true;
  bool load_attempted_ = false;
  bool loaded_ = false;
  bool source_is_guarded_ = false;
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

  // Overlay config input state
  bool overlay_inputs_initialized_ = false;
  std::string overlay_track_tiles_input_;
  std::string overlay_track_stop_tiles_input_;
  std::string overlay_track_switch_tiles_input_;
  std::string overlay_track_object_ids_input_;
  std::string overlay_minecart_sprite_ids_input_;
};

}  // namespace yaze::editor

#endif
