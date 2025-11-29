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
#include "app/rom.h"
#include "app/emu/audio/audio_backend.h"
#include "imgui/imgui.h"
#include "app/editor/music/instrument_editor_view.h"
#include "app/editor/music/piano_roll_view.h"
#include "app/editor/music/sample_editor_view.h"
#include "app/editor/music/song_browser_view.h"
#include "app/editor/music/tracker_view.h"
#include "zelda3/music/music_bank.h"

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
  void set_emulator(emu::Emulator* emulator) { emulator_ = emulator; }
  emu::Emulator* emulator() const { return emulator_; }

  // Audio control methods
  void PlaySong(int song_id);
  void PauseSong();
  void ResumeSong();
  void StopSong();
  void SetVolume(float volume);  // 0.0 to 1.0
  void SetProject(project::YazeProject* project);
  
  // API for sub-views
  void LoadSong(int index);

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
  void StartPlayback();
  void StopPlayback();
  void UpdatePlayback();

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
  int current_song_index_ = 0;
  int current_pattern_index_ = 0;
  int current_channel_index_ = 0;
  int current_segment_index_ = 0;
  bool is_playing_ = false;
  bool is_paused_ = false;  // Track pause state separate from stop
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
  bool song_browser_auto_shown_ = false;
  bool tracker_auto_shown_ = false;
  bool music_dirty_ = false;
  bool persist_custom_music_ = true;
  std::string music_storage_key_;
  std::chrono::steady_clock::time_point last_music_persist_;

  // Per-song tracker windows (like dungeon room cards)
  ImVector<int> active_songs_;  // Song indices that are currently open
  std::unordered_map<int, std::shared_ptr<gui::EditorCard>> song_cards_;
  std::unordered_map<int, std::unique_ptr<editor::music::TrackerView>>
      song_trackers_;
  struct SongPianoRollWindow {
    std::shared_ptr<gui::EditorCard> card;
    std::unique_ptr<editor::music::PianoRollView> view;
    bool* visible_flag = nullptr;
  };
  std::unordered_map<int, SongPianoRollWindow> song_piano_rolls_;

  // Docking class for song windows to dock together
  ImGuiWindowClass song_window_class_;

  // Audio preview (uses authentic SPC emulation)
  void PreviewNote(const zelda3::music::MusicSong& song,
                   const zelda3::music::TrackEvent& event, int segment_index,
                   int channel_index);
  void PreviewSegment(const zelda3::music::MusicSong& song,
                      int segment_index);
  const zelda3::music::MusicInstrument* ResolveInstrumentForEvent(
      const zelda3::music::MusicSegment& segment, int channel_index,
      uint16_t tick) const;
  void OpenSongPianoRoll(int song_index);

  // Direct SPC playback (bypasses game music system)
  bool use_direct_spc_ = true;      // Use direct SPC vs game-based playback
  bool spc_initialized_ = false;    // Track if SPC has sound bank loaded
  uint8_t current_spc_bank_ = 0xFF; // Currently loaded bank (0xFF = none)
  float playback_speed_ = 1.0f;     // Playback speed multiplier (0.25 - 2.0)

  void InitializeDirectSpc();
  void PlaySongDirect(int song_id);
  void UploadSoundBankFromRom(uint32_t rom_offset);
  uint32_t GetBankRomOffset(uint8_t bank) const;
  int GetSongIndexInBank(int song_id, uint8_t bank) const;
};

}  // namespace editor
}  // namespace yaze

#endif
