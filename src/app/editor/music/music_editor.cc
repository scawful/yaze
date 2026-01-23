#include "music_editor.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "absl/strings/str_format.h"
#include "app/editor/code/assembly_editor.h"
#include "app/editor/music/panels/music_assembly_panel.h"
#include "app/editor/music/panels/music_audio_debug_panel.h"
#include "app/editor/music/panels/music_help_panel.h"
#include "app/editor/music/panels/music_instrument_editor_panel.h"
#include "app/editor/music/panels/music_piano_roll_panel.h"
#include "app/editor/music/panels/music_playback_control_panel.h"
#include "app/editor/music/panels/music_sample_editor_panel.h"
#include "app/editor/music/panels/music_song_browser_panel.h"
#include "app/editor/system/panel_manager.h"
#include "app/emu/audio/audio_backend.h"
#include "app/emu/emulator.h"
#include "app/gfx/debug/performance/performance_profiler.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/theme_manager.h"
#include "app/gui/core/ui_helpers.h"
#include "core/project.h"
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "nlohmann/json.hpp"
#include "util/log.h"
#include "util/macro.h"

#ifdef __EMSCRIPTEN__
#include "app/platform/wasm/wasm_storage.h"
#endif

namespace yaze {
namespace editor {

void MusicEditor::Initialize() {
  LOG_INFO("MusicEditor", "Initialize() START: rom_=%p, emulator_=%p",
           static_cast<void*>(rom_), static_cast<void*>(emulator_));

  // Note: song_window_class_ initialization is deferred to first Update() call
  // because ImGui::GetID() requires a valid window context which doesn't exist
  // during Initialize()
  song_window_class_.DockingAllowUnclassed = true;
  song_window_class_.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_None;

  // ==========================================================================
  // Create SINGLE audio backend - owned here and shared with all emulators
  // This eliminates the dual-backend bug entirely
  // ==========================================================================
  if (!audio_backend_) {
#ifdef __EMSCRIPTEN__
    audio_backend_ = emu::audio::AudioBackendFactory::Create(
        emu::audio::AudioBackendFactory::BackendType::WASM);
#else
    audio_backend_ = emu::audio::AudioBackendFactory::Create(
        emu::audio::AudioBackendFactory::BackendType::SDL2);
#endif

    emu::audio::AudioConfig config;
    config.sample_rate = 48000;
    config.channels = 2;
    config.buffer_frames = 1024;
    config.format = emu::audio::SampleFormat::INT16;

    if (audio_backend_->Initialize(config)) {
      LOG_INFO("MusicEditor", "Created shared audio backend: %s @ %dHz",
               audio_backend_->GetBackendName().c_str(), config.sample_rate);
    } else {
      LOG_ERROR("MusicEditor", "Failed to initialize audio backend!");
      audio_backend_.reset();
    }
  }

  // Share the audio backend with the main emulator (if available)
  if (audio_backend_ && emulator_) {
    emulator_->SetExternalAudioBackend(audio_backend_.get());
    LOG_INFO("MusicEditor", "Shared audio backend with main emulator");
  } else {
    LOG_WARN("MusicEditor",
             "Cannot share with main emulator: backend=%p, emulator=%p",
             static_cast<void*>(audio_backend_.get()),
             static_cast<void*>(emulator_));
  }

  music_player_ = std::make_unique<editor::music::MusicPlayer>(&music_bank_);
  if (rom_) {
    music_player_->SetRom(rom_);
    LOG_INFO("MusicEditor", "Set ROM on MusicPlayer");
  } else {
    LOG_WARN("MusicEditor", "No ROM available for MusicPlayer!");
  }

  // Inject the main emulator into MusicPlayer
  if (emulator_) {
    music_player_->SetEmulator(emulator_);
    LOG_INFO("MusicEditor", "Injected main emulator into MusicPlayer");
  } else {
    LOG_WARN("MusicEditor",
             "No emulator available to inject into MusicPlayer!");
  }

  if (!dependencies_.panel_manager)
    return;
  auto* panel_manager = dependencies_.panel_manager;

  // Register PanelDescriptors for menu/sidebar visibility
  panel_manager->RegisterPanel({.card_id = "music.song_browser",
                                .display_name = "Song Browser",
                                .window_title = " Song Browser",
                                .icon = ICON_MD_LIBRARY_MUSIC,
                                .category = "Music",
                                .shortcut_hint = "Ctrl+Shift+B",
                                .priority = 5});
  panel_manager->RegisterPanel({.card_id = "music.tracker",
                                .display_name = "Playback Control",
                                .window_title = " Playback Control",
                                .icon = ICON_MD_PLAY_CIRCLE,
                                .category = "Music",
                                .shortcut_hint = "Ctrl+Shift+M",
                                .priority = 10});
  panel_manager->RegisterPanel({.card_id = "music.piano_roll",
                                .display_name = "Piano Roll",
                                .window_title = " Piano Roll",
                                .icon = ICON_MD_PIANO,
                                .category = "Music",
                                .shortcut_hint = "Ctrl+Shift+P",
                                .priority = 15});
  panel_manager->RegisterPanel({.card_id = "music.instrument_editor",
                                .display_name = "Instrument Editor",
                                .window_title = " Instrument Editor",
                                .icon = ICON_MD_SPEAKER,
                                .category = "Music",
                                .shortcut_hint = "Ctrl+Shift+I",
                                .priority = 20});
  panel_manager->RegisterPanel({.card_id = "music.sample_editor",
                                .display_name = "Sample Editor",
                                .window_title = " Sample Editor",
                                .icon = ICON_MD_WAVES,
                                .category = "Music",
                                .shortcut_hint = "Ctrl+Shift+S",
                                .priority = 25});
  panel_manager->RegisterPanel({.card_id = "music.assembly",
                                .display_name = "Assembly View",
                                .window_title = " Music Assembly",
                                .icon = ICON_MD_CODE,
                                .category = "Music",
                                .shortcut_hint = "Ctrl+Shift+A",
                                .priority = 30});
  panel_manager->RegisterPanel({.card_id = "music.audio_debug",
                                .display_name = "Audio Debug",
                                .window_title = " Audio Debug",
                                .icon = ICON_MD_BUG_REPORT,
                                .category = "Music",
                                .shortcut_hint = "",
                                .priority = 95});
  panel_manager->RegisterPanel({.card_id = "music.help",
                                .display_name = "Help",
                                .window_title = " Music Editor Help",
                                .icon = ICON_MD_HELP,
                                .category = "Music",
                                .priority = 99});

  // ==========================================================================
  // Phase 5: Create and register EditorPanel instances
  // Note: Callbacks are set up on the view classes during Draw() since
  // PanelManager takes ownership of the panels.
  // ==========================================================================

  // Song Browser Panel - callbacks are set on song_browser_view_ directly
  auto song_browser = std::make_unique<MusicSongBrowserPanel>(
      &music_bank_, &current_song_index_, &song_browser_view_);
  panel_manager->RegisterEditorPanel(std::move(song_browser));

  // Playback Control Panel
  auto playback_control = std::make_unique<MusicPlaybackControlPanel>(
      &music_bank_, &current_song_index_, music_player_.get());
  playback_control->SetOnOpenSong([this](int index) { OpenSong(index); });
  playback_control->SetOnOpenPianoRoll(
      [this](int index) { OpenSongPianoRoll(index); });
  panel_manager->RegisterEditorPanel(std::move(playback_control));

  // Piano Roll Panel
  auto piano_roll = std::make_unique<MusicPianoRollPanel>(
      &music_bank_, &current_song_index_, &current_segment_index_,
      &current_channel_index_, &piano_roll_view_, music_player_.get());
  panel_manager->RegisterEditorPanel(std::move(piano_roll));

  // Instrument Editor Panel - callbacks set on instrument_editor_view_
  auto instrument_editor = std::make_unique<MusicInstrumentEditorPanel>(
      &music_bank_, &instrument_editor_view_);
  panel_manager->RegisterEditorPanel(std::move(instrument_editor));

  // Sample Editor Panel - callbacks set on sample_editor_view_
  auto sample_editor = std::make_unique<MusicSampleEditorPanel>(
      &music_bank_, &sample_editor_view_);
  panel_manager->RegisterEditorPanel(std::move(sample_editor));

  // Assembly Panel
  auto assembly = std::make_unique<MusicAssemblyPanel>(&assembly_editor_);
  panel_manager->RegisterEditorPanel(std::move(assembly));

  // Audio Debug Panel
  auto audio_debug =
      std::make_unique<MusicAudioDebugPanel>(music_player_.get());
  panel_manager->RegisterEditorPanel(std::move(audio_debug));

  // Help Panel
  auto help = std::make_unique<MusicHelpPanel>();
  panel_manager->RegisterEditorPanel(std::move(help));
}

void MusicEditor::set_emulator(emu::Emulator* emulator) {
  LOG_INFO("MusicEditor", "set_emulator(%p): audio_backend_=%p",
           static_cast<void*>(emulator),
           static_cast<void*>(audio_backend_.get()));
  emulator_ = emulator;
  // Share our audio backend with the main emulator (single backend architecture)
  if (emulator_ && audio_backend_) {
    emulator_->SetExternalAudioBackend(audio_backend_.get());
    LOG_INFO("MusicEditor",
             "Shared audio backend with main emulator (deferred)");
  }

  // Inject emulator into MusicPlayer
  if (music_player_) {
    music_player_->SetEmulator(emulator_);
  }
}

void MusicEditor::SetProject(project::YazeProject* project) {
  project_ = project;
  if (project_) {
    persist_custom_music_ = project_->music_persistence.persist_custom_music;
    music_storage_key_ = project_->music_persistence.storage_key;
    if (music_storage_key_.empty()) {
      music_storage_key_ = project_->MakeStorageKey("music");
    }
  }
}

absl::Status MusicEditor::Load() {
  gfx::ScopedTimer timer("MusicEditor::Load");
  if (project_) {
    persist_custom_music_ = project_->music_persistence.persist_custom_music;
    music_storage_key_ = project_->music_persistence.storage_key;
    if (music_storage_key_.empty()) {
      music_storage_key_ = project_->MakeStorageKey("music");
    }
  }

#ifdef __EMSCRIPTEN__
  if (persist_custom_music_ && !music_storage_key_.empty()) {
    auto restore = RestoreMusicState();
    if (restore.ok() && restore.value()) {
      LOG_INFO("MusicEditor", "Restored music state from web storage");
      return absl::OkStatus();
    } else if (!restore.ok()) {
      LOG_WARN("MusicEditor", "Failed to restore music state: %s",
               restore.status().ToString().c_str());
    }
  }
#endif

  if (rom_ && rom_->is_loaded()) {
    if (music_player_) {
      music_player_->SetRom(rom_);
      LOG_INFO("MusicEditor", "Load(): Set ROM on MusicPlayer, IsAudioReady=%d",
               music_player_->IsAudioReady());
    }
    return music_bank_.LoadFromRom(*rom_);
  } else {
    LOG_WARN("MusicEditor", "Load(): No ROM available!");
  }
  return absl::OkStatus();
}

void MusicEditor::TogglePlayPause() {
  if (!music_player_)
    return;
  auto state = music_player_->GetState();
  if (state.is_playing && !state.is_paused) {
    music_player_->Pause();
  } else if (state.is_paused) {
    music_player_->Resume();
  } else {
    music_player_->PlaySong(state.playing_song_index);
  }
}

void MusicEditor::StopPlayback() {
  if (music_player_) {
    music_player_->Stop();
  }
}

void MusicEditor::SpeedUp(float delta) {
  if (music_player_) {
    auto state = music_player_->GetState();
    music_player_->SetPlaybackSpeed(state.playback_speed + delta);
  }
}

void MusicEditor::SlowDown(float delta) {
  if (music_player_) {
    auto state = music_player_->GetState();
    music_player_->SetPlaybackSpeed(state.playback_speed - delta);
  }
}

absl::Status MusicEditor::Update() {
  // Deferred initialization: Initialize song_window_class_.ClassId on first Update()
  // because ImGui::GetID() requires a valid window context
  if (song_window_class_.ClassId == 0) {
    song_window_class_.ClassId = ImGui::GetID("SongTrackerWindowClass");
  }

  // Update MusicPlayer - this runs the emulator's audio frame
  // MusicPlayer now controls the main emulator directly for playback.
  if (music_player_)
    music_player_->Update();

#ifdef __EMSCRIPTEN__
  if (persist_custom_music_ && !music_storage_key_.empty()) {
    if (music_bank_.HasModifications()) {
      music_dirty_ = true;
    }
    auto now = std::chrono::steady_clock::now();
    const auto elapsed = now - last_music_persist_;
    if (music_dirty_ && (last_music_persist_.time_since_epoch().count() == 0 ||
                         elapsed > std::chrono::seconds(3))) {
      auto status = PersistMusicState("autosave");
      if (!status.ok()) {
        LOG_WARN("MusicEditor", "Music autosave failed: %s",
                 status.ToString().c_str());
      }
    }
  }
#endif

  if (!dependencies_.panel_manager)
    return absl::OkStatus();
  auto* panel_manager = dependencies_.panel_manager;

  // ==========================================================================
  // Phase 5 Complete: Static panels now drawn by DrawAllVisiblePanels()
  // Only auto-show logic and dynamic song windows remain here
  // ==========================================================================

  // Auto-show Song Browser on first load
  bool* browser_visible =
      panel_manager->GetVisibilityFlag("music.song_browser");
  if (browser_visible && !song_browser_auto_shown_) {
    *browser_visible = true;
    song_browser_auto_shown_ = true;
  }

  // Auto-show Playback Control on first load
  bool* playback_visible = panel_manager->GetVisibilityFlag("music.tracker");
  if (playback_visible && !tracker_auto_shown_) {
    *playback_visible = true;
    tracker_auto_shown_ = true;
  }

  // Auto-show Piano Roll on first load
  bool* piano_roll_visible =
      panel_manager->GetVisibilityFlag("music.piano_roll");
  if (piano_roll_visible && !piano_roll_auto_shown_) {
    *piano_roll_visible = true;
    piano_roll_auto_shown_ = true;
  }

  // ==========================================================================
  // Dynamic Per-Song Windows (like dungeon room cards)
  // TODO(Phase 6): Migrate to ResourcePanel with LRU limits
  // ==========================================================================

  // Per-Song Tracker Windows - synced with PanelManager for Activity Bar
  for (int i = 0; i < active_songs_.Size; i++) {
    int song_index = active_songs_[i];
    // Use base ID - PanelManager handles session prefixing
    std::string card_id = absl::StrFormat("music.song_%d", song_index);

    // Check if panel was hidden via Activity Bar
    bool panel_visible = true;
    if (dependencies_.panel_manager) {
      panel_visible = dependencies_.panel_manager->IsPanelVisible(card_id);
    }

    // If hidden via Activity Bar, close the song
    if (!panel_visible) {
      if (dependencies_.panel_manager) {
        dependencies_.panel_manager->UnregisterPanel(card_id);
      }
      song_cards_.erase(song_index);
      song_trackers_.erase(song_index);
      active_songs_.erase(active_songs_.Data + i);
      i--;
      continue;
    }

    // Category filtering: only draw if Music is active OR panel is pinned
    bool is_pinned = dependencies_.panel_manager &&
                     dependencies_.panel_manager->IsPanelPinned(card_id);
    std::string active_category =
        dependencies_.panel_manager
            ? dependencies_.panel_manager->GetActiveCategory()
            : "";

    if (active_category != "Music" && !is_pinned) {
      // Not in Music editor and not pinned - skip drawing but keep registered
      // Panel will reappear when user returns to Music editor
      continue;
    }

    bool open = true;

    // Get song name for window title (icon is handled by EditorPanel)
    auto* song = music_bank_.GetSong(song_index);
    std::string song_name = song ? song->name : "Unknown";
    std::string card_title = absl::StrFormat(
        "[%02X] %s###SongTracker%d", song_index + 1, song_name, song_index);

    // Create card instance if needed
    if (song_cards_.find(song_index) == song_cards_.end()) {
      song_cards_[song_index] = std::make_shared<gui::PanelWindow>(
          card_title.c_str(), ICON_MD_MUSIC_NOTE, &open);
      song_cards_[song_index]->SetDefaultSize(900, 700);

      // Create dedicated tracker view for this song
      song_trackers_[song_index] =
          std::make_unique<editor::music::TrackerView>();
      song_trackers_[song_index]->SetOnEditCallback(
          [this]() { PushUndoState(); });
    }

    auto& song_card = song_cards_[song_index];

    // Use docking class to group song windows together
    ImGui::SetNextWindowClass(&song_window_class_);

    if (song_card->Begin(&open)) {
      DrawSongTrackerWindow(song_index);
    }
    song_card->End();

    // Handle close button
    if (!open) {
      // Unregister from PanelManager
      if (dependencies_.panel_manager) {
        dependencies_.panel_manager->UnregisterPanel(card_id);
      }
      song_cards_.erase(song_index);
      song_trackers_.erase(song_index);
      active_songs_.erase(active_songs_.Data + i);
      i--;
    }
  }

  // Per-song piano roll windows - synced with PanelManager for Activity Bar
  for (auto it = song_piano_rolls_.begin(); it != song_piano_rolls_.end();) {
    int song_index = it->first;
    auto& window = it->second;
    auto* song = music_bank_.GetSong(song_index);
    // Use base ID - PanelManager handles session prefixing
    std::string card_id = absl::StrFormat("music.piano_roll_%d", song_index);

    if (!song || !window.card || !window.view) {
      if (dependencies_.panel_manager) {
        dependencies_.panel_manager->UnregisterPanel(card_id);
      }
      it = song_piano_rolls_.erase(it);
      continue;
    }

    // Check if panel was hidden via Activity Bar
    bool panel_visible = true;
    if (dependencies_.panel_manager) {
      panel_visible = dependencies_.panel_manager->IsPanelVisible(card_id);
    }

    // If hidden via Activity Bar, close the piano roll
    if (!panel_visible) {
      if (dependencies_.panel_manager) {
        dependencies_.panel_manager->UnregisterPanel(card_id);
      }
      delete window.visible_flag;
      it = song_piano_rolls_.erase(it);
      continue;
    }

    // Category filtering: only draw if Music is active OR panel is pinned
    bool is_pinned = dependencies_.panel_manager &&
                     dependencies_.panel_manager->IsPanelPinned(card_id);
    std::string active_category =
        dependencies_.panel_manager
            ? dependencies_.panel_manager->GetActiveCategory()
            : "";

    if (active_category != "Music" && !is_pinned) {
      // Not in Music editor and not pinned - skip drawing but keep registered
      ++it;
      continue;
    }

    bool open = true;

    // Use same docking class as tracker windows so they can dock together
    ImGui::SetNextWindowClass(&song_window_class_);

    if (window.card->Begin(&open)) {
      window.view->SetOnEditCallback([this]() { PushUndoState(); });
      window.view->SetOnNotePreview(
          [this, song_index](const zelda3::music::TrackEvent& evt,
                             int segment_idx, int channel_idx) {
            auto* target = music_bank_.GetSong(song_index);
            if (!target || !music_player_)
              return;
            music_player_->PreviewNote(*target, evt, segment_idx, channel_idx);
          });
      window.view->SetOnSegmentPreview(
          [this, song_index](const zelda3::music::MusicSong& /*unused*/,
                             int segment_idx) {
            auto* target = music_bank_.GetSong(song_index);
            if (!target || !music_player_)
              return;
            music_player_->PreviewSegment(*target, segment_idx);
          });
      // Update playback state for cursor visualization
      auto state = music_player_ ? music_player_->GetState()
                                 : editor::music::PlaybackState{};
      window.view->SetPlaybackState(state.is_playing, state.is_paused,
                                    state.current_tick);
      window.view->Draw(song);
    }
    window.card->End();

    if (!open) {
      // Unregister from PanelManager
      if (dependencies_.panel_manager) {
        dependencies_.panel_manager->UnregisterPanel(card_id);
      }
      delete window.visible_flag;
      it = song_piano_rolls_.erase(it);
    } else {
      ++it;
    }
  }

  DrawAsmPopups();

  return absl::OkStatus();
}

absl::Status MusicEditor::Save() {
  if (!rom_)
    return absl::FailedPreconditionError("No ROM loaded");
  RETURN_IF_ERROR(music_bank_.SaveToRom(*rom_));

#ifdef __EMSCRIPTEN__
  auto persist_status = PersistMusicState("save");
  if (!persist_status.ok()) {
    return persist_status;
  }
#endif

  return absl::OkStatus();
}

absl::StatusOr<bool> MusicEditor::RestoreMusicState() {
#ifdef __EMSCRIPTEN__
  if (music_storage_key_.empty()) {
    return false;
  }

  auto storage_or = platform::WasmStorage::LoadProject(music_storage_key_);
  if (!storage_or.ok()) {
    return false;  // Nothing persisted yet
  }

  try {
    auto parsed = nlohmann::json::parse(storage_or.value());
    RETURN_IF_ERROR(music_bank_.LoadFromJson(parsed));
    music_dirty_ = false;
    last_music_persist_ = std::chrono::steady_clock::now();
    return true;
  } catch (const std::exception& e) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Failed to parse stored music state: %s", e.what()));
  }
#else
  return false;
#endif
}

absl::Status MusicEditor::PersistMusicState(const char* reason) {
#ifdef __EMSCRIPTEN__
  if (!persist_custom_music_ || music_storage_key_.empty()) {
    return absl::OkStatus();
  }

  auto serialized = music_bank_.ToJson().dump();
  RETURN_IF_ERROR(
      platform::WasmStorage::SaveProject(music_storage_key_, serialized));

  if (project_) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    project_->music_persistence.storage_key = music_storage_key_;
    project_->music_persistence.last_saved_at = ss.str();
  }

  music_dirty_ = false;
  last_music_persist_ = std::chrono::steady_clock::now();
  if (reason) {
    LOG_DEBUG("MusicEditor", "Persisted music state (%s)", reason);
  }
  return absl::OkStatus();
#else
  (void)reason;
  return absl::OkStatus();
#endif
}

void MusicEditor::MarkMusicDirty() {
  music_dirty_ = true;
}

absl::Status MusicEditor::Cut() {
  Copy();
  // In a real implementation, this would delete the selected events
  // TrackerView::DeleteSelection();
  PushUndoState();
  return absl::OkStatus();
}

absl::Status MusicEditor::Copy() {
  // TODO: Serialize selected events to clipboard
  // TrackerView should expose a GetSelection() method
  return absl::UnimplementedError(
      "Copy not yet implemented - clipboard support coming soon");
}

absl::Status MusicEditor::Paste() {
  // TODO: Paste from clipboard
  // Need to deserialize events and insert at cursor position
  return absl::UnimplementedError(
      "Paste not yet implemented - clipboard support coming soon");
}

absl::Status MusicEditor::Undo() {
  if (undo_stack_.empty())
    return absl::FailedPreconditionError("Nothing to undo");

  // Save current state to redo stack
  UndoState current_state;
  if (auto* song = music_bank_.GetSong(current_song_index_)) {
    current_state.song_snapshot = *song;
    current_state.song_index = current_song_index_;
    redo_stack_.push_back(current_state);
  }

  RestoreState(undo_stack_.back());
  undo_stack_.pop_back();
  return absl::OkStatus();
}

absl::Status MusicEditor::Redo() {
  if (redo_stack_.empty())
    return absl::FailedPreconditionError("Nothing to redo");

  // Save current state to undo stack
  UndoState current_state;
  if (auto* song = music_bank_.GetSong(current_song_index_)) {
    current_state.song_snapshot = *song;
    current_state.song_index = current_song_index_;
    undo_stack_.push_back(current_state);
  }

  RestoreState(redo_stack_.back());
  redo_stack_.pop_back();
  return absl::OkStatus();
}

void MusicEditor::PushUndoState() {
  auto* song = music_bank_.GetSong(current_song_index_);
  if (!song)
    return;

  UndoState state;
  state.song_snapshot = *song;
  state.song_index = current_song_index_;
  undo_stack_.push_back(state);
  MarkMusicDirty();

  // Limit undo stack size to prevent unbounded memory growth
  constexpr size_t kMaxUndoStates = 50;
  while (undo_stack_.size() > kMaxUndoStates) {
    undo_stack_.erase(undo_stack_.begin());
  }

  // Clear redo stack on new action
  redo_stack_.clear();
}

void MusicEditor::RestoreState(const UndoState& state) {
  // Ensure we are on the correct song
  if (state.song_index >= 0 &&
      state.song_index < static_cast<int>(music_bank_.GetSongCount())) {
    current_song_index_ = state.song_index;
    // This is a heavy copy, but safe for now
    *music_bank_.GetSong(current_song_index_) = state.song_snapshot;
    MarkMusicDirty();
  }
}

void MusicEditor::DrawSongBrowser() {
  song_browser_view_.SetSelectedSongIndex(current_song_index_);
  song_browser_view_.Draw(music_bank_);
  // Update current song if selection changed
  if (song_browser_view_.GetSelectedSongIndex() != current_song_index_) {
    current_song_index_ = song_browser_view_.GetSelectedSongIndex();
  }
}

void MusicEditor::OpenSong(int song_index) {
  // Update current selection
  current_song_index_ = song_index;
  current_segment_index_ = 0;

  // Check if already open
  for (int i = 0; i < active_songs_.Size; i++) {
    if (active_songs_[i] == song_index) {
      // Focus the existing window
      FocusSong(song_index);
      return;
    }
  }

  // Add new song to active list
  active_songs_.push_back(song_index);

  // Register with PanelManager so it appears in Activity Bar
  if (dependencies_.panel_manager) {
    auto* song = music_bank_.GetSong(song_index);
    std::string song_name =
        song ? song->name : absl::StrFormat("Song %02X", song_index);
    // Use base ID - RegisterPanel handles session prefixing
    std::string card_id = absl::StrFormat("music.song_%d", song_index);

    dependencies_.panel_manager->RegisterPanel(
        {.card_id = card_id,
         .display_name = song_name,
         .window_title = ICON_MD_MUSIC_NOTE " " + song_name,
         .icon = ICON_MD_MUSIC_NOTE,
         .category = "Music",
         .shortcut_hint = "",
         .visibility_flag = nullptr,
         .priority = 200 + song_index});

    dependencies_.panel_manager->ShowPanel(card_id);

    // NOT auto-pinned - user must explicitly pin to persist across editors
  }

  LOG_INFO("MusicEditor", "Opened song %d tracker window", song_index);
}

void MusicEditor::FocusSong(int song_index) {
  auto it = song_cards_.find(song_index);
  if (it != song_cards_.end()) {
    it->second->Focus();
  }
}

void MusicEditor::OpenSongPianoRoll(int song_index) {
  if (song_index < 0 ||
      song_index >= static_cast<int>(music_bank_.GetSongCount())) {
    return;
  }

  auto it = song_piano_rolls_.find(song_index);
  if (it != song_piano_rolls_.end()) {
    if (it->second.card && it->second.visible_flag) {
      *it->second.visible_flag = true;
      it->second.card->Focus();
    }
    return;
  }

  auto* song = music_bank_.GetSong(song_index);
  std::string song_name =
      song ? song->name : absl::StrFormat("Song %02X", song_index);
  std::string card_title =
      absl::StrFormat("[%02X] %s - Piano Roll###SongPianoRoll%d",
                      song_index + 1, song_name, song_index);

  SongPianoRollWindow window;
  window.visible_flag = new bool(true);
  window.card = std::make_shared<gui::PanelWindow>(
      card_title.c_str(), ICON_MD_PIANO, window.visible_flag);
  window.card->SetDefaultSize(900, 450);
  window.view = std::make_unique<editor::music::PianoRollView>();
  window.view->SetActiveChannel(0);
  window.view->SetActiveSegment(0);

  song_piano_rolls_[song_index] = std::move(window);

  // Register with PanelManager so it appears in Activity Bar
  if (dependencies_.panel_manager) {
    // Use base ID - RegisterPanel handles session prefixing
    std::string card_id = absl::StrFormat("music.piano_roll_%d", song_index);

    dependencies_.panel_manager->RegisterPanel(
        {.card_id = card_id,
         .display_name = song_name + " (Piano)",
         .window_title = ICON_MD_PIANO " " + song_name + " (Piano)",
         .icon = ICON_MD_PIANO,
         .category = "Music",
         .shortcut_hint = "",
         .visibility_flag = nullptr,
         .priority = 250 + song_index});

    dependencies_.panel_manager->ShowPanel(card_id);
    // NOT auto-pinned - user must explicitly pin to persist across editors
  }
}

void MusicEditor::DrawSongTrackerWindow(int song_index) {
  auto* song = music_bank_.GetSong(song_index);
  if (!song) {
    ImGui::TextDisabled("Song not loaded");
    return;
  }

  // Compact toolbar for this song window
  bool can_play = music_player_ && music_player_->IsAudioReady();
  auto state = music_player_ ? music_player_->GetState()
                             : editor::music::PlaybackState{};
  bool is_playing_this_song =
      state.is_playing && (state.playing_song_index == song_index);
  bool is_paused_this_song =
      state.is_paused && (state.playing_song_index == song_index);

  // === Row 1: Playback Transport ===
  if (!can_play)
    ImGui::BeginDisabled();

  // Play/Pause button with status indication
  if (is_playing_this_song && !is_paused_this_song) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          ImVec4(0.3f, 0.6f, 0.3f, 1.0f));
    if (ImGui::Button(ICON_MD_PAUSE " Pause")) {
      music_player_->Pause();
    }
    ImGui::PopStyleColor(2);
  } else if (is_paused_this_song) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          ImVec4(0.6f, 0.6f, 0.3f, 1.0f));
    if (ImGui::Button(ICON_MD_PLAY_ARROW " Resume")) {
      music_player_->Resume();
    }
    ImGui::PopStyleColor(2);
  } else {
    if (ImGui::Button(ICON_MD_PLAY_ARROW " Play")) {
      music_player_->PlaySong(song_index);
    }
  }

  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_STOP)) {
    music_player_->Stop();
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Stop playback");

  if (!can_play)
    ImGui::EndDisabled();

  // Keyboard shortcuts (when window is focused)
  if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
      can_play) {
    // Focused-window shortcuts remain as fallbacks; also registered with ShortcutManager.
    if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
      TogglePlayPause();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
      StopPlayback();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Equal, false) ||
        ImGui::IsKeyPressed(ImGuiKey_KeypadAdd, false)) {
      SpeedUp();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Minus, false) ||
        ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract, false)) {
      SlowDown();
    }
  }

  // Status indicator
  ImGui::SameLine();
  if (is_playing_this_song && !is_paused_this_song) {
    ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), ICON_MD_GRAPHIC_EQ);
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("Playing");
  } else if (is_paused_this_song) {
    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.3f, 1.0f), ICON_MD_PAUSE_CIRCLE);
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("Paused");
  }

  // Right side controls
  float right_offset = ImGui::GetWindowWidth() - 200;
  ImGui::SameLine(right_offset);

  // Speed control (with mouse wheel support)
  ImGui::Text(ICON_MD_SPEED);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(55);
  float speed = state.playback_speed;
  if (gui::SliderFloatWheel("##Speed", &speed, 0.25f, 2.0f, "%.1fx", 0.1f)) {
    if (music_player_) {
      music_player_->SetPlaybackSpeed(speed);
    }
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Playback speed (0.25x - 2.0x) - use mouse wheel");

  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_PIANO)) {
    OpenSongPianoRoll(song_index);
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Open Piano Roll view");

  // === Row 2: Song Info ===
  const char* bank_name = nullptr;
  switch (song->bank) {
    case 0:
      bank_name = "Overworld";
      break;
    case 1:
      bank_name = "Dungeon";
      break;
    case 2:
      bank_name = "Credits";
      break;
    case 3:
      bank_name = "Expanded";
      break;
    case 4:
      bank_name = "Auxiliary";
      break;
    default:
      bank_name = "Unknown";
      break;
  }
  ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "[%02X]", song_index + 1);
  ImGui::SameLine();
  ImGui::Text("%s", song->name.c_str());
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.7f, 1.0f), "(%s)", bank_name);

  if (song->modified) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f),
                       ICON_MD_EDIT " Modified");
  }

  // Segment count
  ImGui::SameLine(right_offset);
  ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%zu segments",
                     song->segments.size());

  ImGui::Separator();

  // Channel overview shows DSP state when playing
  if (is_playing_this_song) {
    DrawChannelOverview();
    ImGui::Separator();
  }

  // Draw the tracker view for this specific song
  auto it = song_trackers_.find(song_index);
  if (it != song_trackers_.end()) {
    it->second->Draw(song, &music_bank_);
  } else {
    // Fallback - shouldn't happen but just in case
    tracker_view_.Draw(song, &music_bank_);
  }
}

// Playback Control panel - focused on audio playback and current song status
void MusicEditor::DrawPlaybackControl() {
  DrawToolset();

  ImGui::Separator();

  // Current song info
  auto* song = music_bank_.GetSong(current_song_index_);
  auto state = music_player_ ? music_player_->GetState()
                             : editor::music::PlaybackState{};

  if (song) {
    ImGui::Text("Selected Song:");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "[%02X] %s",
                       current_song_index_ + 1, song->name.c_str());

    // Song details
    ImGui::SameLine();
    ImGui::TextDisabled("| %zu segments", song->segments.size());
    if (song->modified) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f),
                         ICON_MD_EDIT " Modified");
    }
  }

  // Playback status bar
  if (state.is_playing || state.is_paused) {
    ImGui::Separator();

    // Timeline progress
    if (song && !song->segments.empty()) {
      uint32_t total_duration = 0;
      for (const auto& seg : song->segments) {
        total_duration += seg.GetDuration();
      }

      float progress =
          (total_duration > 0)
              ? static_cast<float>(state.current_tick) / total_duration
              : 0.0f;
      progress = std::clamp(progress, 0.0f, 1.0f);

      // Time display
      float current_seconds = state.ticks_per_second > 0
                                  ? state.current_tick / state.ticks_per_second
                                  : 0.0f;
      float total_seconds = state.ticks_per_second > 0
                                ? total_duration / state.ticks_per_second
                                : 0.0f;

      int cur_min = static_cast<int>(current_seconds) / 60;
      int cur_sec = static_cast<int>(current_seconds) % 60;
      int tot_min = static_cast<int>(total_seconds) / 60;
      int tot_sec = static_cast<int>(total_seconds) % 60;

      ImGui::Text("%d:%02d / %d:%02d", cur_min, cur_sec, tot_min, tot_sec);
      ImGui::SameLine();

      // Progress bar
      ImGui::ProgressBar(progress, ImVec2(-1, 0), "");
    }

    // Segment info
    ImGui::Text("Segment: %d | Tick: %u", state.current_segment_index + 1,
                state.current_tick);
    ImGui::SameLine();
    ImGui::TextDisabled("| %.1f ticks/sec | %.2fx speed",
                        state.ticks_per_second, state.playback_speed);
  }

  // Channel overview when playing
  if (state.is_playing) {
    ImGui::Separator();
    DrawChannelOverview();
  }

  ImGui::Separator();

  // Quick action buttons
  if (ImGui::Button(ICON_MD_OPEN_IN_NEW " Open Tracker")) {
    OpenSong(current_song_index_);
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Open song in dedicated tracker window");

  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_PIANO " Open Piano Roll")) {
    OpenSongPianoRoll(current_song_index_);
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Open piano roll view for this song");

  // Help section (collapsed by default)
  if (ImGui::CollapsingHeader(ICON_MD_KEYBOARD " Keyboard Shortcuts")) {
    ImGui::BulletText("Space: Play/Pause toggle");
    ImGui::BulletText("Escape: Stop playback");
    ImGui::BulletText("+/-: Increase/decrease speed");
    ImGui::BulletText("Arrow keys: Navigate in tracker/piano roll");
    ImGui::BulletText("Z,S,X,D,C,V,G,B,H,N,J,M: Piano keyboard (C to B)");
    ImGui::BulletText("Ctrl+Wheel: Zoom (Piano Roll)");
  }
}

// Legacy DrawTrackerView for compatibility (calls the tracker view directly)
void MusicEditor::DrawTrackerView() {
  auto* song = music_bank_.GetSong(current_song_index_);
  tracker_view_.Draw(song, &music_bank_);
}

void MusicEditor::DrawPianoRollView() {
  auto* song = music_bank_.GetSong(current_song_index_);
  if (song &&
      current_segment_index_ >= static_cast<int>(song->segments.size())) {
    current_segment_index_ = 0;
  }

  piano_roll_view_.SetActiveChannel(current_channel_index_);
  piano_roll_view_.SetActiveSegment(current_segment_index_);
  piano_roll_view_.SetOnEditCallback([this]() { PushUndoState(); });
  piano_roll_view_.SetOnNotePreview([this, song_index = current_song_index_](
                                        const zelda3::music::TrackEvent& evt,
                                        int segment_idx, int channel_idx) {
    auto* target = music_bank_.GetSong(song_index);
    if (!target || !music_player_)
      return;
    music_player_->PreviewNote(*target, evt, segment_idx, channel_idx);
  });
  piano_roll_view_.SetOnSegmentPreview(
      [this, song_index = current_song_index_](
          const zelda3::music::MusicSong& /*unused*/, int segment_idx) {
        auto* target = music_bank_.GetSong(song_index);
        if (!target || !music_player_)
          return;
        music_player_->PreviewSegment(*target, segment_idx);
      });

  // Update playback state for cursor visualization
  auto state = music_player_ ? music_player_->GetState()
                             : editor::music::PlaybackState{};
  piano_roll_view_.SetPlaybackState(state.is_playing, state.is_paused,
                                    state.current_tick);

  piano_roll_view_.Draw(song, &music_bank_);
  current_segment_index_ = piano_roll_view_.GetActiveSegment();
  current_channel_index_ = piano_roll_view_.GetActiveChannel();
}

void MusicEditor::DrawInstrumentEditor() {
  instrument_editor_view_.Draw(music_bank_);
}

void MusicEditor::DrawSampleEditor() {
  sample_editor_view_.Draw(music_bank_);
}

void MusicEditor::DrawToolset() {
  static int current_volume = 100;
  auto state = music_player_ ? music_player_->GetState()
                             : editor::music::PlaybackState{};
  bool can_play = music_player_ && music_player_->IsAudioReady();

  // Row 1: Transport controls and song info
  auto* song = music_bank_.GetSong(current_song_index_);

  if (!can_play)
    ImGui::BeginDisabled();

  // Transport: Play/Pause with visual state indication
  const ImVec4 paused_color(0.9f, 0.7f, 0.2f, 1.0f);

  if (state.is_playing && !state.is_paused) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.2f, 1.0f));
    if (ImGui::Button(ICON_MD_PAUSE "##Pause"))
      music_player_->Pause();
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("Pause (Space)");
  } else if (state.is_paused) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.4f, 0.1f, 1.0f));
    if (ImGui::Button(ICON_MD_PLAY_ARROW "##Resume"))
      music_player_->Resume();
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("Resume (Space)");
  } else {
    if (ImGui::Button(ICON_MD_PLAY_ARROW "##Play"))
      music_player_->PlaySong(current_song_index_);
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("Play (Space)");
  }

  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_STOP "##Stop"))
    music_player_->Stop();
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Stop (Escape)");

  if (!can_play)
    ImGui::EndDisabled();

  // Song label with animated playing indicator
  ImGui::SameLine();
  if (song) {
    if (state.is_playing && !state.is_paused) {
      // Animated playing indicator
      float t = static_cast<float>(ImGui::GetTime() * 3.0);
      float alpha = 0.5f + 0.5f * std::sin(t);
      ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, alpha), ICON_MD_GRAPHIC_EQ);
      ImGui::SameLine();
    } else if (state.is_paused) {
      ImGui::TextColored(paused_color, ICON_MD_PAUSE_CIRCLE);
      ImGui::SameLine();
    }
    ImGui::Text("%s", song->name.c_str());
    if (song->modified) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), ICON_MD_EDIT);
    }
  } else {
    ImGui::TextDisabled("No song selected");
  }

  // Time display (when playing)
  if (state.is_playing || state.is_paused) {
    ImGui::SameLine();
    float seconds = state.ticks_per_second > 0
                        ? state.current_tick / state.ticks_per_second
                        : 0.0f;
    int mins = static_cast<int>(seconds) / 60;
    int secs = static_cast<int>(seconds) % 60;
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.8f, 1.0f), " %d:%02d", mins, secs);
  }

  // Right-aligned controls
  float right_offset = ImGui::GetWindowWidth() - 380;
  ImGui::SameLine(right_offset);

  // Speed control with visual feedback
  ImGui::Text(ICON_MD_SPEED);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(70);
  float speed = state.playback_speed;
  if (gui::SliderFloatWheel("##Speed", &speed, 0.25f, 2.0f, "%.2fx", 0.1f)) {
    if (music_player_) {
      music_player_->SetPlaybackSpeed(speed);
    }
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Playback speed (+/- keys)");

  ImGui::SameLine();
  ImGui::Text(ICON_MD_VOLUME_UP);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(60);
  if (gui::SliderIntWheel("##Vol", &current_volume, 0, 100, "%d%%", 5)) {
    if (music_player_)
      music_player_->SetVolume(current_volume / 100.0f);
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Volume");

  ImGui::SameLine();
  const bool rom_loaded = rom_ && rom_->is_loaded();
  if (!rom_loaded) {
    ImGui::BeginDisabled();
  }
  if (ImGui::Button(ICON_MD_REFRESH)) {
    music_bank_.LoadFromRom(*rom_);
    song_names_.clear();
  }
  if (!rom_loaded) {
    ImGui::EndDisabled();
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Reload from ROM");

  // Interpolation Control
  ImGui::SameLine();
  ImGui::SetNextItemWidth(100);
  {
    static int interpolation_type = 2;  // Default: Gaussian
    const char* items[] = {"Linear", "Hermite", "Gaussian", "Cosine", "Cubic"};
    if (ImGui::Combo("##Interp", &interpolation_type, items,
                     IM_ARRAYSIZE(items))) {
      if (music_player_)
        music_player_->SetInterpolationType(interpolation_type);
    }
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip(
          "Audio interpolation quality\nGaussian = authentic SNES sound");
  }

  ImGui::Separator();

  // Mixer / Visualizer Panel
  if (ImGui::BeginTable(
          "MixerPanel", 9,
          ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp)) {
    // Channel Headers
    ImGui::TableSetupColumn("Master", ImGuiTableColumnFlags_WidthFixed, 60.0f);
    for (int i = 0; i < 8; i++) {
      ImGui::TableSetupColumn(absl::StrFormat("Ch %d", i + 1).c_str());
    }
    ImGui::TableHeadersRow();

    ImGui::TableNextRow();

    // Master Oscilloscope (Column 0)
    ImGui::TableSetColumnIndex(0);
    // Use MusicPlayer's emulator for visualization
    emu::Emulator* audio_emu =
        music_player_ ? music_player_->emulator() : nullptr;
    if (audio_emu && audio_emu->is_snes_initialized()) {
      auto& dsp = audio_emu->snes().apu().dsp();

      ImGui::Text("Scope");

      // Oscilloscope
      const int16_t* buffer = dsp.GetSampleBuffer();
      uint16_t offset = dsp.GetSampleOffset();

      static float scope_values[128];
      // Handle ring buffer wrap-around correctly (buffer size is 0x400 samples)
      constexpr int kBufferSize = 0x400;
      for (int i = 0; i < 128; i++) {
        int sample_idx = ((offset - 128 + i + kBufferSize) & (kBufferSize - 1));
        scope_values[i] = static_cast<float>(buffer[sample_idx * 2]) /
                          32768.0f;  // Left channel
      }

      ImGui::PlotLines("##Scope", scope_values, 128, 0, nullptr, -1.0f, 1.0f,
                       ImVec2(50, 60));
    }

    // Channel Strips (Columns 1-8)
    for (int i = 0; i < 8; i++) {
      ImGui::TableSetColumnIndex(i + 1);

      if (audio_emu && audio_emu->is_snes_initialized()) {
        auto& dsp = audio_emu->snes().apu().dsp();
        const auto& ch = dsp.GetChannel(i);

        // Mute/Solo Buttons
        bool is_muted = dsp.GetChannelMute(i);
        bool is_solo = channel_soloed_[i];
        const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();

        if (is_muted) {
          ImGui::PushStyleColor(ImGuiCol_Button,
                                gui::ConvertColorToImVec4(theme.error));
        }
        if (ImGui::Button(absl::StrFormat("M##%d", i).c_str(),
                          ImVec2(25, 20))) {
          dsp.SetChannelMute(i, !is_muted);
        }
        if (is_muted)
          ImGui::PopStyleColor();

        ImGui::SameLine();

        if (is_solo) {
          ImGui::PushStyleColor(ImGuiCol_Button,
                                gui::ConvertColorToImVec4(theme.warning));
        }
        if (ImGui::Button(absl::StrFormat("S##%d", i).c_str(),
                          ImVec2(25, 20))) {
          channel_soloed_[i] = !channel_soloed_[i];

          bool any_solo = false;
          for (int j = 0; j < 8; j++)
            if (channel_soloed_[j])
              any_solo = true;

          for (int j = 0; j < 8; j++) {
            if (any_solo) {
              dsp.SetChannelMute(j, !channel_soloed_[j]);
            } else {
              dsp.SetChannelMute(j, false);
            }
          }
        }
        if (is_solo)
          ImGui::PopStyleColor();

        // VU Meter
        float level = std::abs(ch.sampleOut) / 32768.0f;
        ImGui::ProgressBar(level, ImVec2(-1, 60), "");

        // Info
        ImGui::Text("Vol: %d %d", ch.volumeL, ch.volumeR);
        ImGui::Text("Pitch: %04X", ch.pitch);

        // Key On Indicator
        if (ch.keyOn) {
          ImGui::TextColored(gui::ConvertColorToImVec4(theme.success),
                             "KEY ON");
        } else {
          ImGui::TextDisabled("---");
        }
      } else {
        ImGui::TextDisabled("Offline");
      }
    }

    ImGui::EndTable();
  }

  // Quick audio status (detailed debug in Audio Debug panel)
  if (ImGui::CollapsingHeader(ICON_MD_BUG_REPORT " Audio Status")) {
    emu::Emulator* debug_emu =
        music_player_ ? music_player_->emulator() : nullptr;
    if (debug_emu && debug_emu->is_snes_initialized()) {
      auto* audio_backend = debug_emu->audio_backend();
      if (audio_backend) {
        auto status = audio_backend->GetStatus();
        auto config = audio_backend->GetConfig();
        bool resampling = audio_backend->IsAudioStreamEnabled();

        // Compact status line
        ImGui::Text("Backend: %s @ %dHz | Queue: %u frames",
                    audio_backend->GetBackendName().c_str(), config.sample_rate,
                    status.queued_frames);

        // Resampling indicator with warning if disabled
        if (resampling) {
          ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f),
                             "Resampling: 32040 -> %d Hz", config.sample_rate);
        } else {
          ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), ICON_MD_WARNING
                             " Resampling DISABLED - 1.5x speed bug!");
        }

        if (status.has_underrun) {
          ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                             ICON_MD_WARNING " Buffer underrun");
        }

        ImGui::TextDisabled("Open Audio Debug panel for full diagnostics");
      }
    } else {
      ImGui::TextDisabled("Play a song to see audio status");
    }
  }
}

void MusicEditor::DrawChannelOverview() {
  if (!music_player_) {
    ImGui::TextDisabled("Music player not initialized");
    return;
  }

  // Check if audio emulator is initialized (created on first play)
  auto* audio_emu = music_player_->emulator();
  if (!audio_emu || !audio_emu->is_snes_initialized()) {
    ImGui::TextDisabled("Play a song to see channel activity");
    return;
  }

  // Check available space to avoid ImGui table assertion
  ImVec2 avail = ImGui::GetContentRegionAvail();
  if (avail.y < 50.0f) {
    ImGui::TextDisabled("(Channel view - expand for details)");
    return;
  }

  auto channel_states = music_player_->GetChannelStates();

  if (ImGui::BeginTable(
          "ChannelOverview", 9,
          ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableSetupColumn("Master", ImGuiTableColumnFlags_WidthFixed, 70.0f);
    for (int i = 0; i < 8; i++) {
      ImGui::TableSetupColumn(absl::StrFormat("Ch %d", i + 1).c_str());
    }
    ImGui::TableHeadersRow();

    ImGui::TableNextRow();

    ImGui::TableSetColumnIndex(0);
    ImGui::Text("DSP Live");

    for (int ch = 0; ch < 8; ++ch) {
      ImGui::TableSetColumnIndex(ch + 1);
      const auto& state = channel_states[ch];

      // Visual indicator for Key On
      if (state.key_on) {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "ON");
      } else {
        ImGui::TextDisabled("OFF");
      }

      // Volume bars
      float vol_l = state.volume_l / 128.0f;
      float vol_r = state.volume_r / 128.0f;
      ImGui::ProgressBar(vol_l, ImVec2(-1, 6.0f), "");
      ImGui::ProgressBar(vol_r, ImVec2(-1, 6.0f), "");

      // Info
      ImGui::Text("S: %02X", state.sample_index);
      ImGui::Text("P: %04X", state.pitch);

      // ADSR State
      const char* adsr_str = "???";
      switch (state.adsr_state) {
        case 0:
          adsr_str = "Att";
          break;
        case 1:
          adsr_str = "Dec";
          break;
        case 2:
          adsr_str = "Sus";
          break;
        case 3:
          adsr_str = "Rel";
          break;
      }
      ImGui::Text("%s", adsr_str);
    }

    ImGui::EndTable();
  }
}

// ============================================================================
// Audio Control Methods (Emulator Integration)
// ============================================================================

void MusicEditor::SeekToSegment(int segment_index) {
  if (music_player_)
    music_player_->SeekToSegment(segment_index);
}

// ============================================================================
// ASM Export/Import
// ============================================================================

void MusicEditor::ExportSongToAsm(int song_index) {
  auto* song = music_bank_.GetSong(song_index);
  if (!song) {
    LOG_WARN("MusicEditor", "ExportSongToAsm: Invalid song index %d",
             song_index);
    return;
  }

  // Configure export options
  zelda3::music::AsmExportOptions options;
  options.label_prefix = song->name;
  // Remove spaces and special characters from label
  std::replace(options.label_prefix.begin(), options.label_prefix.end(), ' ',
               '_');
  options.include_comments = true;
  options.use_instrument_macros = true;

  // Set ARAM address based on bank
  if (music_bank_.IsExpandedSong(song_index)) {
    options.base_aram_address = zelda3::music::kAuxSongTableAram;
  } else {
    options.base_aram_address = zelda3::music::kSongTableAram;
  }

  // Export to string
  zelda3::music::AsmExporter exporter;
  auto result = exporter.ExportSong(*song, options);
  if (!result.ok()) {
    LOG_ERROR("MusicEditor", "ExportSongToAsm failed: %s",
              result.status().message().data());
    return;
  }

  // For now, copy to assembly editor buffer
  // TODO: Add native file dialog for export path selection
  asm_buffer_ = *result;
  show_asm_export_popup_ = true;

  LOG_INFO("MusicEditor", "Exported song '%s' to ASM (%zu bytes)",
           song->name.c_str(), asm_buffer_.size());
}

void MusicEditor::ImportSongFromAsm(int song_index) {
  asm_import_target_index_ = song_index;

  // If no source is present, open the import dialog for user input
  if (asm_buffer_.empty()) {
    LOG_INFO("MusicEditor", "No ASM source to import - showing import dialog");
    asm_import_error_.clear();
    show_asm_import_popup_ = true;
    return;
  }

  // Attempt immediate import using existing buffer
  if (!ImportAsmBufferToSong(song_index)) {
    show_asm_import_popup_ = true;
    return;
  }

  show_asm_import_popup_ = false;
  asm_import_target_index_ = -1;
}

bool MusicEditor::ImportAsmBufferToSong(int song_index) {
  auto* song = music_bank_.GetSong(song_index);
  if (!song) {
    asm_import_error_ = absl::StrFormat("Invalid song index %d", song_index);
    LOG_WARN("MusicEditor", "%s", asm_import_error_.c_str());
    return false;
  }

  // Configure import options
  zelda3::music::AsmImportOptions options;
  options.strict_mode = false;
  options.verbose_errors = true;

  // Parse the ASM source
  zelda3::music::AsmImporter importer;
  auto result = importer.ImportSong(asm_buffer_, options);
  if (!result.ok()) {
    const auto message = result.status().message();
    asm_import_error_.assign(message.data(), message.size());
    LOG_ERROR("MusicEditor", "ImportSongFromAsm failed: %s",
              asm_import_error_.c_str());
    return false;
  }

  // Log any warnings
  for (const auto& warning : result->warnings) {
    LOG_WARN("MusicEditor", "ASM import warning: %s", warning.c_str());
  }

  // Copy parsed song data to target song
  // Keep original name if import didn't provide one
  std::string original_name = song->name;
  *song = result->song;
  if (song->name.empty()) {
    song->name = original_name;
  }
  song->modified = true;

  LOG_INFO("MusicEditor", "Imported ASM to song '%s' (%d lines, %d bytes)",
           song->name.c_str(), result->lines_parsed, result->bytes_generated);

  // Notify that edits occurred
  PushUndoState();
  asm_import_error_.clear();
  return true;
}

// ============================================================================
// Custom Song Preview (In-Memory Playback)
// ============================================================================

void MusicEditor::DrawAsmPopups() {
  if (show_asm_export_popup_) {
    ImGui::OpenPopup("Export Song ASM");
    show_asm_export_popup_ = false;
  }
  if (show_asm_import_popup_) {
    ImGui::OpenPopup("Import Song ASM");
    // Keep flag true until user closes
  }

  if (ImGui::BeginPopupModal("Export Song ASM", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextWrapped("Copy the generated ASM below or tweak before saving.");
    ImGui::InputTextMultiline("##AsmExportText", &asm_buffer_, ImVec2(520, 260),
                              ImGuiInputTextFlags_AllowTabInput);

    if (ImGui::Button("Copy to Clipboard")) {
      ImGui::SetClipboardText(asm_buffer_.c_str());
    }
    ImGui::SameLine();
    if (ImGui::Button("Close")) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  if (ImGui::BeginPopupModal("Import Song ASM", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    int song_slot =
        (asm_import_target_index_ >= 0) ? asm_import_target_index_ + 1 : -1;
    if (song_slot > 0) {
      ImGui::Text("Target Song: [%02X]", song_slot);
    } else {
      ImGui::TextDisabled("Select a song to import into");
    }
    ImGui::TextWrapped("Paste Oracle of Secrets-compatible ASM here.");

    ImGui::InputTextMultiline("##AsmImportText", &asm_buffer_, ImVec2(520, 260),
                              ImGuiInputTextFlags_AllowTabInput);

    if (!asm_import_error_.empty()) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
      ImGui::TextWrapped("%s", asm_import_error_.c_str());
      ImGui::PopStyleColor();
    }

    bool can_import = asm_import_target_index_ >= 0 && !asm_buffer_.empty();
    if (!can_import) {
      ImGui::BeginDisabled();
    }
    if (ImGui::Button("Import")) {
      if (ImportAsmBufferToSong(asm_import_target_index_)) {
        show_asm_import_popup_ = false;
        asm_import_target_index_ = -1;
        ImGui::CloseCurrentPopup();
      }
    }
    if (!can_import) {
      ImGui::EndDisabled();
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      asm_import_error_.clear();
      show_asm_import_popup_ = false;
      asm_import_target_index_ = -1;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  } else if (!show_asm_import_popup_) {
    // Clear stale error when popup is closed
    asm_import_error_.clear();
  }
}

}  // namespace editor
}  // namespace yaze
