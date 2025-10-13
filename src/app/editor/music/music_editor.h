#ifndef YAZE_APP_EDITOR_MUSIC_EDITOR_H
#define YAZE_APP_EDITOR_MUSIC_EDITOR_H

#include "app/editor/code/assembly_editor.h"
#include "app/editor/editor.h"
#include "app/emu/audio/apu.h"
#include "app/gui/editor_card_manager.h"
#include "app/gui/editor_layout.h"
#include "app/rom.h"
#include "app/zelda3/music/tracker.h"
#include "imgui/imgui.h"

namespace yaze {

// Forward declaration
namespace emu {
class Emulator;
}

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
  
  // Emulator integration for live audio playback
  void set_emulator(emu::Emulator* emulator) { emulator_ = emulator; }
  emu::Emulator* emulator() const { return emulator_; }
  
  // Audio control methods
  void PlaySong(int song_id);
  void StopSong();
  void SetVolume(float volume);  // 0.0 to 1.0

 private:
  // UI Drawing Methods
  void DrawTrackerView();
  void DrawInstrumentEditor();
  void DrawSampleEditor();
  void DrawToolset();

  // Playback Control
  void StartPlayback();
  void StopPlayback();
  void UpdatePlayback();

  AssemblyEditor assembly_editor_;
  zelda3::music::Tracker music_tracker_;
  // Note: APU requires ROM memory, will be initialized when needed

  // UI State
  int current_song_index_ = 0;
  int current_pattern_index_ = 0;
  int current_channel_index_ = 0;
  bool is_playing_ = false;
  std::vector<bool> channel_muted_ = std::vector<bool>(8, false);
  std::vector<bool> channel_soloed_ = std::vector<bool>(8, false);
  std::vector<std::string> song_names_;

  ImGuiTableFlags music_editor_flags_ =
      ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersOuter |
      ImGuiTableFlags_BordersV | ImGuiTableFlags_SizingFixedFit;

  ImGuiTableFlags toolset_table_flags_ =
      ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersOuter |
      ImGuiTableFlags_BordersV | ImGuiTableFlags_PadOuterX;

  Rom* rom_;
  emu::Emulator* emulator_ = nullptr;  // For live audio playback
};

}  // namespace editor
}  // namespace yaze

#endif
