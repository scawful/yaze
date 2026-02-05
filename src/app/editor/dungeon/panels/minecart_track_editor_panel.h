#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_MINECART_TRACK_EDITOR_PANEL_H
#define YAZE_APP_EDITOR_DUNGEON_PANELS_MINECART_TRACK_EDITOR_PANEL_H

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/project.h"
#include "zelda3/dungeon/room.h"

namespace yaze::editor {

struct MinecartTrack {
  int id;
  int room_id;
  int start_x;
  int start_y;
};

}  // namespace yaze::editor

#include "app/editor/system/editor_panel.h"

namespace yaze::editor {

class MinecartTrackEditorPanel : public EditorPanel {
 public:
  explicit MinecartTrackEditorPanel(const std::string& start_root = "")
      : project_root_(start_root) {}

  // EditorPanel overrides
  std::string GetId() const override { return "dungeon.minecart_tracks"; }
  std::string GetDisplayName() const override { return "Minecart Tracks"; }
  std::string GetIcon() const override {
    return "M";
  }  // Using simple string for now, should include icons header
  std::string GetEditorCategory() const override { return "Dungeon"; }

  void Draw(bool* p_open) override;

  // Custom methods
  void SetProjectRoot(const std::string& root);
  void SetRooms(std::array<zelda3::Room, 0x128>* rooms) {
    rooms_ = rooms;
    audit_dirty_ = true;
  }
  void SetProject(project::YazeProject* project) {
    project_ = project;
    overlay_inputs_initialized_ = false;
    audit_dirty_ = true;
  }
  void SaveTracks();

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
  void LoadTracks();
  bool ParseSection(const std::string& content, const std::string& label,
                    std::vector<int>& out_values);
  std::string FormatSection(const std::string& label,
                            const std::vector<int>& values);
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
  std::string project_root_;
  std::array<zelda3::Room, 0x128>* rooms_ = nullptr;
  project::YazeProject* project_ = nullptr;
  std::unordered_map<int, RoomTrackAudit> room_audit_;
  std::unordered_map<int, std::vector<int>> track_usage_rooms_;
  std::vector<bool> track_subtype_used_;
  bool audit_dirty_ = true;
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
