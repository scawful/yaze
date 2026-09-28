#ifndef YAZE_APP_EDITOR_OVERWORLD_SIDEBAR_H
#define YAZE_APP_EDITOR_OVERWORLD_SIDEBAR_H

#include <array>
#include <functional>
#include <string>

#include "absl/status/status.h"
#include "app/editor/overworld/maps/map_properties.h"
#include "rom/rom.h"
#include "zelda3/overworld/overworld.h"

namespace yaze::project {
struct YazeProject;
}  // namespace yaze::project

namespace yaze {
namespace editor {

/// Map Properties panel: every per-map field, shown once.
///
/// The toolbar only navigates (world, map, tool, view); the canvas context
/// menu only offers quick actions. Anything that edits per-map data lives
/// here, grouped Area / Graphics / Palettes / Music / Effects.
class OverworldSidebar {
 public:
  using RenameLabelCallback =
      std::function<absl::Status(const std::string&, int, const std::string&)>;

  explicit OverworldSidebar(zelda3::Overworld* overworld, Rom* rom,
                            MapPropertiesSystem* map_properties_system);

  void Draw(int& current_world, int& current_map, bool& current_map_lock,
            int& game_state, bool& show_custom_bg_color_editor,
            bool& show_overlay_editor, project::YazeProject* project = nullptr);

  /// Renames a project resource label (map names, gfx/palette/music labels)
  /// through the editor so the change is undoable.
  void SetRenameLabelCallback(RenameLabelCallback callback) {
    rename_label_ = std::move(callback);
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
  void DrawPaletteSection(int property_map, int game_state,
                          bool& show_custom_bg_color_editor);
  void DrawMusicSection(int property_map);
  void DrawEffectsSection(int property_map, bool& show_overlay_editor);

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

  std::string edit_error_;
  int edit_error_map_ = -1;

  // Project-label rename popup state (one popup shared by all fields).
  bool open_label_popup_ = false;
  LabelTarget label_target_;
  std::array<char, 128> label_buffer_{};
  std::string label_error_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_SIDEBAR_H
