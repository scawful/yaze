#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_MINECART_TRACK_EDITOR_PANEL_H
#define YAZE_APP_EDITOR_DUNGEON_PANELS_MINECART_TRACK_EDITOR_PANEL_H

#include <string>
#include <vector>

namespace yaze::editor {

struct MinecartTrack {
  int id;
  int room_id;
  int start_x;
  int start_y;
};

} // namespace yaze::editor

#include "app/editor/system/editor_panel.h"

namespace yaze::editor {

class MinecartTrackEditorPanel : public EditorPanel {
 public:
  explicit MinecartTrackEditorPanel(const std::string& start_root = "") : project_root_(start_root) {}

  // EditorPanel overrides
  std::string GetId() const override { return "dungeon.minecart_tracks"; }
  std::string GetDisplayName() const override { return "Minecart Tracks"; }
  std::string GetIcon() const override { return "M"; } // Using simple string for now, should include icons header
  std::string GetEditorCategory() const override { return "Dungeon"; }
  
  void Draw(bool* p_open) override;

  // Custom methods
  void SetProjectRoot(const std::string& root);
  void SaveTracks();

 private:
  void LoadTracks();
  bool ParseSection(const std::string& content, const std::string& label, std::vector<int>& out_values);
  std::string FormatSection(const std::string& label, const std::vector<int>& values);

  std::vector<MinecartTrack> tracks_;
  std::string project_root_;
  bool loaded_ = false;
  std::string status_message_;
  bool show_success_ = false;
  float success_timer_ = 0.0f;
};

} // namespace yaze::editor

#endif
