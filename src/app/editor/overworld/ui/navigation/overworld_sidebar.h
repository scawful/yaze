#ifndef YAZE_APP_EDITOR_OVERWORLD_SIDEBAR_H
#define YAZE_APP_EDITOR_OVERWORLD_SIDEBAR_H

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "app/editor/overworld/maps/map_properties.h"
#include "cli/handlers/game/overworld_tile_reference.h"
#include "rom/rom.h"
#include "zelda3/overworld/overworld.h"

namespace yaze::project {
struct YazeProject;
}  // namespace yaze::project

namespace yaze {
namespace editor {

/// Map properties and canvas appearance controls share the same edit path.
class OverworldSidebar {
 public:
  using RenameLabelCallback =
      std::function<absl::Status(const std::string&, int, const std::string&)>;

  explicit OverworldSidebar(zelda3::Overworld* overworld, Rom* rom,
                            MapPropertiesSystem* map_properties_system);

  void Draw(int& current_world, int& current_map, bool& current_map_lock,
            int& game_state, bool& show_custom_bg_color_editor,
            bool& show_overlay_editor, project::YazeProject* project = nullptr);

  void DrawQuickProperties(int current_map, bool current_map_lock,
                           project::YazeProject* project = nullptr,
                           bool compact = false);

  std::function<void(int)> on_story_changed;
  std::function<void(int, int&)> draw_project_tools;

  /// Renames a project resource label (map names, gfx/palette/music labels)
  /// through the editor so the change is undoable.
  void SetRenameLabelCallback(RenameLabelCallback callback) {
    rename_label_ = std::move(callback);
  }

  /// Discards an in-progress screen color picker draft (ROM reload, etc.).
  void CancelScreenColorEdit();

  // Live canvas hover updates the selected occurrence unless pinned. Leaving
  // the canvas clears only the live-hover flag; the last valid selection stays
  // so the sidebar Pin control remains usable. Clear on world/project/ROM
  // invalidation via InvalidateTile16ReferenceContext.
  void SetTile16ReferenceHover(int map_id, int area_x, int area_y);
  void ClearTile16ReferenceHover();
  void InvalidateTile16ReferenceContext();
  void SetTile16ReferencePinned(bool pinned);
  bool tile16_reference_pinned() const { return tile16_reference_pinned_; }
  bool tile16_reference_has_selection() const {
    return tile16_reference_has_selection_;
  }
  bool tile16_reference_live_hover() const {
    return tile16_reference_live_hover_;
  }
  int tile16_reference_map_for_test() const { return tile16_reference_map_; }
  int tile16_reference_x_for_test() const { return tile16_reference_x_; }
  int tile16_reference_y_for_test() const { return tile16_reference_y_; }
  void SetTile16ReferencePolicyPathForTest(const std::string& path);
  void ReloadTile16ReferencePolicyForTest();
  bool tile16_reference_policy_loaded_for_test() const {
    return tile16_reference_policy_.has_value();
  }
  const std::string& tile16_reference_policy_error_for_test() const {
    return tile16_reference_policy_error_;
  }
  int tile16_reference_policy_load_count_for_test() const {
    return tile16_reference_policy_load_count_;
  }

 private:
  struct LabelTarget {
    std::string title;
    std::string type;
    int id = 0;
    int hex_width = 2;
  };

  void DrawHeader(int current_world, int current_map, int property_map,
                  bool& current_map_lock, int game_state);
  void DrawAreaSection(int property_map, int& game_state);
  void DrawGraphicsSection(int property_map, int game_state);
  void DrawPaletteSection(int property_map, int current_map, int game_state,
                          bool& show_custom_bg_color_editor);
  void DrawMusicSection(int property_map);
  void DrawEffectsSection(int property_map, bool& show_overlay_editor);
  void DrawTile16ReferenceSection();
  void NotifyTile16ReferenceMapContext(int current_world);
  void CommitTile16ReferencePolicyPath(bool force_reload);

  // Two-column row: label (with tooltip) then a full-width control cell.
  bool BeginFieldTable(const char* id);
  void FieldLabel(const char* label, const char* tooltip);
  bool HexByteField(const char* id, const char* label, const char* tooltip,
                    OverworldPropertyField field, int index, int property_map,
                    uint8_t value, const LabelTarget* label_target = nullptr);
  void LabelContextMenu(const LabelTarget& target);
  void DrawLabelPopup();
  void Apply(OverworldPropertyField field, int index, int property_map,
             int value);

  zelda3::Overworld* overworld_;
  Rom* rom_;
  MapPropertiesSystem* map_properties_system_;
  project::YazeProject* project_ = nullptr;
  RenameLabelCallback rename_label_;

  // Native revision-4 screen appearance colors: committed only on picker
  // release; one undo step per committed change.
  bool screen_color_picker_open_ = false;
  int screen_color_picker_map_ = -1;
  int screen_color_picker_index_ = -1;
  std::vector<uint16_t> screen_color_picker_source_;
  uint16_t screen_color_picker_original_bgr_ = 0;
  uint16_t screen_color_picker_draft_bgr_ = 0;

  std::string edit_error_;
  int edit_error_map_ = -1;

  // Project-label rename popup state (one popup shared by all fields).
  bool open_label_popup_ = false;
  LabelTarget label_target_;
  std::array<char, 128> label_buffer_{};
  std::string label_error_;

  bool tile16_reference_has_selection_ = false;
  bool tile16_reference_live_hover_ = false;
  bool tile16_reference_pinned_ = false;
  int tile16_reference_map_ = 0;
  int tile16_reference_x_ = 0;
  int tile16_reference_y_ = 0;
  std::array<char, 512> tile16_reference_policy_path_{};
  std::string tile16_reference_policy_committed_path_;
  std::optional<cli::overworld::TileReferencePolicy> tile16_reference_policy_;
  std::string tile16_reference_policy_error_;
  int tile16_reference_policy_load_count_ = 0;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_SIDEBAR_H
