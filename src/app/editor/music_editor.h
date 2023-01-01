#ifndef YAZE_APP_EDITOR_MUSIC_EDITOR_H
#define YAZE_APP_EDITOR_MUSIC_EDITOR_H

#include <imgui/imgui.h>

#include "absl/strings/str_format.h"
#include "app/editor/assembly_editor.h"
#include "gui/canvas.h"
#include "gui/icons.h"
#include "gui/input.h"
#include "snes_spc/demo/demo_util.h"
#include "snes_spc/demo/wave_writer.h"
#include "snes_spc/snes_spc/spc.h"

namespace yaze {
namespace app {
namespace editor {

static const char* kGameSongs[] = {"Title",
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
    "C",  "C#", "D",  "D#", "E", "F",  "F#", "G",  "G#", "A",  "A#", "B", "C",
    "C#", "D",  "D#", "E",  "F", "F#", "G",  "G#", "A",  "A#", "B",  "C"};
class MusicEditor {
 public:
  void Update();

 private:
  void DrawChannels();
  void DrawPianoStaff();
  void DrawPianoRoll();
  void DrawSongToolset();
  void DrawToolset();

  AssemblyEditor assembly_editor_;
  ImGuiTableFlags toolset_table_flags_ = ImGuiTableFlags_SizingFixedFit;
  ImGuiTableFlags music_editor_flags_ = ImGuiTableFlags_SizingFixedFit |
                                        ImGuiTableFlags_Resizable |
                                        ImGuiTableFlags_Reorderable;

  ImGuiTableFlags channel_table_flags_ =
      ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
      ImGuiTableFlags_Hideable | ImGuiTableFlags_Sortable |
      ImGuiTableFlags_SortMulti | ImGuiTableFlags_RowBg |
      ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
      ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_ScrollY;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif
