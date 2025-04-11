#ifndef YAZE_APP_EDITOR_MUSIC_EDITOR_H
#define YAZE_APP_EDITOR_MUSIC_EDITOR_H

#include "app/editor/code/assembly_editor.h"
#include "app/editor/editor.h"
#include "app/rom.h"
#include "app/zelda3/music/tracker.h"
#include "imgui/imgui.h"

namespace yaze {
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

const ImGuiTableFlags toolset_table_flags_ = ImGuiTableFlags_SizingFixedFit;
const ImGuiTableFlags music_editor_flags_ = ImGuiTableFlags_SizingFixedFit |
                                            ImGuiTableFlags_Resizable |
                                            ImGuiTableFlags_Reorderable;
/**
 * @class MusicEditor
 * @brief A class for editing music data in a Rom.
 */
class MusicEditor : public Editor {
 public:
  explicit MusicEditor(Rom* rom = nullptr) : rom_(rom) { 
    type_ = EditorType::kMusic; 
  }

  void Initialize() override;
  absl::Status Load() override;
  absl::Status Save() override { return absl::UnimplementedError("Save"); }
  absl::Status Update() override;
  absl::Status Cut() override { return absl::UnimplementedError("Cut"); }
  absl::Status Copy() override { return absl::UnimplementedError("Copy"); }
  absl::Status Paste() override { return absl::UnimplementedError("Paste"); }
  absl::Status Undo() override { return absl::UnimplementedError("Undo"); }
  absl::Status Redo() override { return absl::UnimplementedError("Redo"); }
  absl::Status Find() override { return absl::UnimplementedError("Find"); }
  
  // Set the ROM pointer
  void set_rom(Rom* rom) { rom_ = rom; }
  
  // Get the ROM pointer
  Rom* rom() const { return rom_; }

 private:
  Rom* rom_;
  void DrawChannels();
  void DrawPianoStaff();
  void DrawPianoRoll();
  void DrawSongToolset();
  void DrawToolset();

  zelda3::music::Tracker music_tracker_;

  AssemblyEditor assembly_editor_;
};

}  // namespace editor
}  // namespace yaze

#endif
