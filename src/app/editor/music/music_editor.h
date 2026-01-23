#ifndef YAZE_APP_EDITOR_MUSIC_EDITOR_H
#define YAZE_APP_EDITOR_MUSIC_EDITOR_H

#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>

#include "absl/status/statusor.h"
#include "app/editor/code/assembly_editor.h"
#include "app/editor/editor.h"
#include "app/gui/app/editor_layout.h"
#include "rom/rom.h"
#include "app/emu/audio/audio_backend.h"
#include "imgui/imgui.h"
#include "app/editor/music/instrument_editor_view.h"
#include "app/editor/music/piano_roll_view.h"
#include "app/editor/music/sample_editor_view.h"
#include "app/editor/music/song_browser_view.h"
#include "app/editor/music/tracker_view.h"
#include "zelda3/music/asm_exporter.h"
#include "zelda3/music/asm_importer.h"
#include "zelda3/music/music_bank.h"
#include "zelda3/music/spc_serializer.h"
#include "app/editor/music/music_player.h"

namespace yaze {

// Forward declaration
namespace emu {
class Emulator;
namespace audio {
class IAudioBackend;
struct AudioConfig;
}
}

namespace project {
struct YazeProject;
}

namespace editor {

// TODO(user): Remove this when MusicBank provides song names
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
  absl::Status Save() override;
  absl::Status Update() override;
  absl::Status Cut() override;
  absl::Status Copy() override;
  absl::Status Paste() override;
  absl::Status Undo() override;
  absl::Status Redo() override;
  absl::Status Find() override { return absl::UnimplementedError("Find"); }

  // Set the ROM pointer
  void set_rom(Rom* rom) { rom_ = rom; }

  // Get the ROM pointer
  Rom* rom() const { return rom_; }

  // Emulator integration for live audio playback
  void set_emulator(emu::Emulator* emulator);
  emu::Emulator* emulator() const { return emulator_; }

  void SetProject(project::YazeProject* project);

  // Scoped editor actions (for ShortcutManager)
  void TogglePlayPause();
  void StopPlayback();
  void SpeedUp(float delta = 0.1f);
  void SlowDown(float delta = 0.1f);
  
  // API for sub-views


  // Song window management (like dungeon rooms)
  void OpenSong(int song_index);
  void FocusSong(int song_index);

 private:
  // UI Drawing Methods
  void DrawSongTrackerWindow(int song_index);
  void DrawPlaybackControl();  // Playback control panel
  void DrawTrackerView();      // Legacy tracker view
  void DrawPianoRollView();
  void DrawInstrumentEditor();
  void DrawSampleEditor();
  void DrawSongBrowser();
  void DrawToolset();
  void DrawChannelOverview();
  absl::StatusOr<bool> RestoreMusicState();
  absl::Status PersistMusicState(const char* reason = nullptr);
  void MarkMusicDirty();

  // Playback Control
  // Delegated to music_player_

  AssemblyEditor assembly_editor_;
  
  // New Data Model
  zelda3::music::MusicBank music_bank_;
  editor::music::TrackerView tracker_view_;
  editor::music::PianoRollView piano_roll_view_;
  editor::music::InstrumentEditorView instrument_editor_view_;
  editor::music::SampleEditorView sample_editor_view_;
  editor::music::SongBrowserView song_browser_view_;
  
  // Undo/Redo
  struct UndoState {
    zelda3::music::MusicSong song_snapshot;
    int song_index;
  };
  std::vector<UndoState> undo_stack_;
  std::vector<UndoState> redo_stack_;
  
  void PushUndoState();
  void RestoreState(const UndoState& state);

  // Note: APU requires ROM memory, will be initialized when needed

  // UI State
  int current_song_index_ = 0;      // Selected song in browser (UI selection)
  int current_pattern_index_ = 0;
  int current_channel_index_ = 0;
  int current_segment_index_ = 0;
  std::vector<bool> channel_muted_ = std::vector<bool>(8, false);
  std::vector<bool> channel_soloed_ = std::vector<bool>(8, false);
  std::vector<std::string> song_names_;

  // Accessors for UI Views
  int current_channel() const { return current_channel_index_; }
  void set_current_channel(int channel) { 
    if (channel >= 0 && channel < 8) current_channel_index_ = channel; 
  }

  ImGuiTableFlags music_editor_flags_ =
      ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersOuter |
      ImGuiTableFlags_BordersV | ImGuiTableFlags_SizingFixedFit;

  ImGuiTableFlags toolset_table_flags_ =
      ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersOuter |
      ImGuiTableFlags_BordersV | ImGuiTableFlags_PadOuterX;

  Rom* rom_;
  emu::Emulator* emulator_ = nullptr;  // For live audio playback
  project::YazeProject* project_ = nullptr;

  // Single shared audio backend - owned here and shared with emulators
  // This avoids the dual-backend bug where two SDL audio devices conflict
  std::unique_ptr<emu::audio::IAudioBackend> audio_backend_;
  bool song_browser_auto_shown_ = false;
  bool tracker_auto_shown_ = false;
  bool piano_roll_auto_shown_ = false;
  bool music_dirty_ = false;
  bool persist_custom_music_ = true;
  std::string music_storage_key_;
  std::chrono::steady_clock::time_point last_music_persist_;

  // Per-song tracker windows (like dungeon room cards)
  ImVector<int> active_songs_;  // Song indices that are currently open
  std::unordered_map<int, std::shared_ptr<gui::PanelWindow>> song_cards_;
  std::unordered_map<int, std::unique_ptr<editor::music::TrackerView>>
      song_trackers_;
  struct SongPianoRollWindow {
    std::shared_ptr<gui::PanelWindow> card;
    std::unique_ptr<editor::music::PianoRollView> view;
    bool* visible_flag = nullptr;
  };
  std::unordered_map<int, SongPianoRollWindow> song_piano_rolls_;

  // Docking class for song windows to dock together
  ImGuiWindowClass song_window_class_;

  void OpenSongPianoRoll(int song_index);

  // ASM export/import
  void ExportSongToAsm(int song_index);
  void ImportSongFromAsm(int song_index);
  bool ImportAsmBufferToSong(int song_index);
  void DrawAsmPopups();

  std::string asm_buffer_;              // Buffer for ASM text
  bool show_asm_export_popup_ = false;  // Show export dialog
  bool show_asm_import_popup_ = false;  // Show import dialog
  int asm_import_target_index_ = -1;    // Song index for import target
  std::string asm_import_error_;        // Last ASM import error (UI)
  // Segment seeking
  void SeekToSegment(int segment_index);

  std::unique_ptr<editor::music::MusicPlayer> music_player_;

  // Note: EditorPanel instances are owned by PanelManager after registration
};

}  // namespace editor
}  // namespace yaze

#endif
