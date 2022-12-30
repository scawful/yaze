#ifndef YAZE_APP_EDITOR_MUSIC_EDITOR_H
#define YAZE_APP_EDITOR_MUSIC_EDITOR_H

#include <imgui/imgui.h>

#include "absl/strings/str_format.h"
#include "app/editor/assembly_editor.h"
#include "gui/canvas.h"
#include "gui/icons.h"

namespace yaze {
namespace app {
namespace editor {

static constexpr absl::string_view kGameSongs[] = {"Title",
                                                   "Light World",
                                                   "Beginning",
                                                   "Rabbit",
                                                   "Forest",
                                                   "Intro",
                                                   "Town",
                                                   "Warp",
                                                   "Dark world",
                                                   "Master sword",
                                                   "File select",
                                                   "Soldier",
                                                   "Mountain",
                                                   "Shop",
                                                   "Fanfare",
                                                   "Castle",
                                                   "Palace (Pendant)",
                                                   "Cave (Same as Secret Way)",
                                                   "Clear (Dungeon end)",
                                                   "Church",
                                                   "Boss",
                                                   "Dungeon (Crystal)",
                                                   "Psychic",
                                                   "Secret Way (Same as Cave)",
                                                   "Rescue",
                                                   "Crystal",
                                                   "Fountain",
                                                   "Pyramid",
                                                   "Kill Agahnim",
                                                   "Ganon Room",
                                                   "Last Boss"};

static constexpr absl::string_view kSongNotes[] = {
    "C", "D", "E", "F", "G", "A", "B", "C", "D", "E", "F", "G", "A",
    "B", "C", "D", "E", "F", "G", "A", "B", "C", "D", "E", "F"};
class MusicEditor {
 public:
  void Update();

 private:
  void DrawPianoStaff();
  void DrawPianoRoll();
  void DrawSongList() const;
  void DrawToolset();

  AssemblyEditor assembly_editor_;
  ImGuiTableFlags toolset_table_flags_ =
      ImGuiTableFlags_SizingFixedFit;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif
