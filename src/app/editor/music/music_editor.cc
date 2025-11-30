#include "music_editor.h"

#include <iomanip>
#include <ctime>
#include <sstream>

#include "absl/strings/str_format.h"
#include <algorithm>
#include <cmath>
#include "app/editor/code/assembly_editor.h"
#include "app/editor/system/editor_card_registry.h"
#include "app/emu/audio/audio_backend.h"
#include "app/emu/emulator.h"
#include "app/gfx/debug/performance/performance_profiler.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/theme_manager.h"
#include "app/gui/core/ui_helpers.h"
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "core/project.h"
#include "nlohmann/json.hpp"
#include "util/log.h"
#include "util/macro.h"

#ifdef __EMSCRIPTEN__
#include "app/platform/wasm/wasm_storage.h"
#endif

namespace yaze {
namespace editor {



void MusicEditor::Initialize() {
  // Configure docking class for song tracker windows (like dungeon rooms)
  // Allow docking with any window for flexibility
  song_window_class_.ClassId = ImGui::GetID("SongTrackerWindowClass");
  song_window_class_.DockingAllowUnclassed = true;
  song_window_class_.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_None;

  music_player_ = std::make_unique<editor::music::MusicPlayer>(&music_bank_);
  if (emulator_) music_player_->SetEmulator(emulator_);
  if (rom_) music_player_->SetRom(rom_);

  if (!dependencies_.card_registry)
    return;
  auto* card_registry = dependencies_.card_registry;

  card_registry->RegisterCard({.card_id = "music.song_browser",
                               .display_name = "Song Browser",
                               .window_title = " Song Browser",
                               .icon = ICON_MD_LIBRARY_MUSIC,
                               .category = "Music",
                               .shortcut_hint = "Ctrl+Shift+B",
                               .priority = 5});
  card_registry->RegisterCard({.card_id = "music.tracker",
                               .display_name = "Playback Control",
                               .window_title = " Playback Control",
                               .icon = ICON_MD_PLAY_CIRCLE,
                               .category = "Music",
                               .shortcut_hint = "Ctrl+Shift+M",
                               .priority = 10});
  card_registry->RegisterCard({.card_id = "music.piano_roll",
                               .display_name = "Piano Roll",
                               .window_title = " Piano Roll",
                               .icon = ICON_MD_PIANO,
                               .category = "Music",
                               .shortcut_hint = "Ctrl+Shift+P",
                               .priority = 15});
  card_registry->RegisterCard({.card_id = "music.instrument_editor",
                               .display_name = "Instrument Editor",
                               .window_title = " Instrument Editor",
                               .icon = ICON_MD_SPEAKER,
                               .category = "Music",
                               .shortcut_hint = "Ctrl+Shift+I",
                               .priority = 20});
  card_registry->RegisterCard({.card_id = "music.sample_editor",
                               .display_name = "Sample Editor",
                               .window_title = " Sample Editor",
                               .icon = ICON_MD_WAVES,
                               .category = "Music",
                               .shortcut_hint = "Ctrl+Shift+S",
                               .priority = 25});
  card_registry->RegisterCard({.card_id = "music.assembly",
                               .display_name = "Assembly View",
                               .window_title = " Music Assembly",
                               .icon = ICON_MD_CODE,
                               .category = "Music",
                               .shortcut_hint = "Ctrl+Shift+A",
                               .priority = 30});
  card_registry->RegisterCard({.card_id = "music.help",
                               .display_name = "Help",
                               .window_title = " Music Editor Help",
                               .icon = ICON_MD_HELP,
                               .category = "Music",
                               .priority = 99});
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

  if (rom_) {
    if (music_player_) music_player_->SetRom(rom_);
    return music_bank_.LoadFromRom(*rom_);
  }
  return absl::OkStatus();
}

absl::Status MusicEditor::Update() {
  // Run emulator frames when playing music (generates audio samples)
  // NOTE: EditorManager::Update() calls emulator_.RunFrameOnly() or Run()
  // so we don't need to call it here. Double-calling causes speedup issues.
  if (music_player_) music_player_->Update();

#ifdef __EMSCRIPTEN__
  if (persist_custom_music_ && !music_storage_key_.empty()) {
    if (music_bank_.HasModifications()) {
      music_dirty_ = true;
    }
    auto now = std::chrono::steady_clock::now();
    const auto elapsed =
        now - last_music_persist_;
    if (music_dirty_ &&
        (last_music_persist_.time_since_epoch().count() == 0 ||
         elapsed > std::chrono::seconds(3))) {
      auto status = PersistMusicState("autosave");
      if (!status.ok()) {
        LOG_WARN("MusicEditor", "Music autosave failed: %s",
                 status.ToString().c_str());
      }
    }
  }
#endif

  if (!dependencies_.card_registry)
    return absl::OkStatus();
  auto* card_registry = dependencies_.card_registry;

  static gui::EditorCard song_browser_card("Song Browser", ICON_MD_LIBRARY_MUSIC);
  static gui::EditorCard playback_card("Playback Control", ICON_MD_PLAY_CIRCLE);
  static gui::EditorCard piano_roll_card("Piano Roll", ICON_MD_PIANO);
  static gui::EditorCard instrument_card("Instrument Editor", ICON_MD_SPEAKER);
  static gui::EditorCard sample_card("Sample Editor", ICON_MD_WAVES);
  static gui::EditorCard assembly_card("Assembly View", ICON_MD_CODE);
  static gui::EditorCard help_card("Music Editor Help", ICON_MD_HELP);
  static bool cards_initialized = false;

  // Initialize card sizes and positions once
  if (!cards_initialized) {
    cards_initialized = true;

    // Set default sizes for cards
    song_browser_card.SetDefaultSize(300, 700);
    playback_card.SetDefaultSize(400, 350);
    piano_roll_card.SetDefaultSize(900, 400);
    instrument_card.SetDefaultSize(600, 500);
    sample_card.SetDefaultSize(600, 500);
    assembly_card.SetDefaultSize(700, 600);
    help_card.SetDefaultSize(400, 500);

    // Set default positions so cards don't stack on top of each other
    song_browser_card.SetPosition(gui::EditorCard::Position::Left);
    playback_card.SetPosition(gui::EditorCard::Position::Floating);
    piano_roll_card.SetPosition(gui::EditorCard::Position::Floating);
    instrument_card.SetPosition(gui::EditorCard::Position::Right);
    sample_card.SetPosition(gui::EditorCard::Position::Floating);
    assembly_card.SetPosition(gui::EditorCard::Position::Bottom);
    help_card.SetPosition(gui::EditorCard::Position::Floating);
  }

  // Song Browser Card (Activity Bar)
  bool* browser_visible = card_registry->GetVisibilityFlag("music.song_browser");
  if (browser_visible && !song_browser_auto_shown_) {
    *browser_visible = true;
    song_browser_auto_shown_ = true;
  }
  if (browser_visible && *browser_visible) {
    if (song_browser_card.Begin(browser_visible)) {
      // Double-click opens unique tracker window (like dungeon rooms)
      song_browser_view_.SetOnSongSelected([this](int index) { OpenSong(index); });
      song_browser_view_.SetOnOpenTracker([this](int index) { OpenSong(index); });
      song_browser_view_.SetOnOpenPianoRoll([this](int index) { OpenSongPianoRoll(index); });
      song_browser_view_.SetOnExportAsm([this](int index) { ExportSongToAsm(index); });
      song_browser_view_.SetOnImportAsm([this](int index) { ImportSongFromAsm(index); });
      song_browser_view_.SetOnEdit([this]() { PushUndoState(); });
      DrawSongBrowser();
    }
    song_browser_card.End();
  }

  // Playback Control Card - Shows playback controls and current song status
  bool* playback_visible = card_registry->GetVisibilityFlag("music.tracker");
  if (playback_visible && !tracker_auto_shown_) {
    *playback_visible = true;
    tracker_auto_shown_ = true;
  }
  if (playback_visible && *playback_visible) {
    // Set initial position: top area, next to song browser
    ImGui::SetNextWindowPos(ImVec2(320, 40), ImGuiCond_FirstUseEver);
    if (playback_card.Begin(playback_visible)) {
      DrawPlaybackControl();
    }
    playback_card.End();
  }

  // Piano Roll Card
  static bool piano_roll_auto_shown = false;
  bool* piano_roll_visible = card_registry->GetVisibilityFlag("music.piano_roll");
  if (piano_roll_visible && !piano_roll_auto_shown) {
    *piano_roll_visible = true;
    piano_roll_auto_shown = true;
  }
  if (piano_roll_visible && *piano_roll_visible) {
    // Set initial position: center area, below playback control
    ImGui::SetNextWindowPos(ImVec2(320, 410), ImGuiCond_FirstUseEver);
    if (piano_roll_card.Begin(piano_roll_visible)) {
      DrawPianoRollView();
    }
    piano_roll_card.End();
  }

  // Instrument Editor Card
  bool* instrument_visible =
      card_registry->GetVisibilityFlag("music.instrument_editor");
  if (instrument_visible && *instrument_visible) {
    if (instrument_card.Begin(instrument_visible)) {
      instrument_editor_view_.SetOnEditCallback([this]() { PushUndoState(); });
      instrument_editor_view_.SetOnPreviewCallback(
          [this](int index) {
            if (music_player_) music_player_->PreviewInstrument(index);
          });
      DrawInstrumentEditor();
    }
    instrument_card.End();
  }

  // Sample Editor Card
  bool* sample_visible =
      card_registry->GetVisibilityFlag("music.sample_editor");
  if (sample_visible && *sample_visible) {
    // Set initial position: offset from instrument editor
    ImGui::SetNextWindowPos(ImVec2(750, 320), ImGuiCond_FirstUseEver);
    if (sample_card.Begin(sample_visible)) {
      sample_editor_view_.SetOnEditCallback([this]() { PushUndoState(); });
      sample_editor_view_.SetOnPreviewCallback(
          [this](int index) {
            if (music_player_) music_player_->PreviewSample(index);
          });
      DrawSampleEditor();
    }
    sample_card.End();
  }

  // Assembly View Card
  bool* assembly_visible = card_registry->GetVisibilityFlag("music.assembly");
  if (assembly_visible && *assembly_visible) {
    if (assembly_card.Begin(assembly_visible)) {
      assembly_editor_.InlineUpdate();
    }
    assembly_card.End();
  }

  // Help Card
  bool* help_visible = card_registry->GetVisibilityFlag("music.help");
  if (help_visible && *help_visible) {
    if (help_card.Begin(help_visible)) {
      ImGui::Text("Yaze Music Editor Guide");
      ImGui::Separator();
      
      if (ImGui::CollapsingHeader("Overview", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextWrapped("The Music Editor allows you to create and modify SNES music for Zelda 3.");
        ImGui::BulletText("Song Browser: Select and manage songs.");
        ImGui::BulletText("Tracker/Piano Roll: Edit note data.");
        ImGui::BulletText("Instrument Editor: Configure ADSR envelopes.");
        ImGui::BulletText("Sample Editor: Import and preview BRR samples.");
      }

      if (ImGui::CollapsingHeader("Tracker / Piano Roll")) {
        ImGui::Text("Controls:");
        ImGui::BulletText("Space: Play/Pause");
        ImGui::BulletText("Z, S, X...: Keyboard piano (C, C#, D...)");
        ImGui::BulletText("Shift+Arrows: Range selection");
        ImGui::BulletText("Ctrl+C/V: Copy/Paste (WIP)");
        ImGui::BulletText("Ctrl+Wheel: Zoom (Piano Roll)");
      }

      if (ImGui::CollapsingHeader("Instruments & Samples")) {
        ImGui::TextWrapped("Instruments use BRR samples with an ADSR volume envelope.");
        ImGui::BulletText("ADSR: Attack, Decay, Sustain, Release.");
        ImGui::BulletText("Loop Points: Define where the sample loops (in blocks of 16 samples).");
        ImGui::BulletText("Tuning: Adjust pitch multiplier ($1000 = 1.0x).");
      }
    }
    help_card.End();
  }

  // Per-Song Tracker Windows (like dungeon room cards)
  for (int i = 0; i < active_songs_.Size; i++) {
    int song_index = active_songs_[i];
    bool open = true;

    // Get song name for window title (icon is handled by EditorCard)
    auto* song = music_bank_.GetSong(song_index);
    std::string song_name = song ? song->name : "Unknown";
    std::string card_title =
        absl::StrFormat("[%02X] %s###SongTracker%d",
                        song_index + 1, song_name, song_index);

    // Create card instance if needed
    if (song_cards_.find(song_index) == song_cards_.end()) {
      song_cards_[song_index] =
          std::make_shared<gui::EditorCard>(card_title.c_str(), ICON_MD_MUSIC_NOTE, &open);
      song_cards_[song_index]->SetDefaultSize(900, 700);

      // Create dedicated tracker view for this song
      song_trackers_[song_index] = std::make_unique<editor::music::TrackerView>();
      song_trackers_[song_index]->SetOnEditCallback([this]() { PushUndoState(); });
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
      song_cards_.erase(song_index);
      song_trackers_.erase(song_index);
      active_songs_.erase(active_songs_.Data + i);
      i--;
    }
  }

  // Per-song piano roll windows
  for (auto it = song_piano_rolls_.begin(); it != song_piano_rolls_.end();) {
    int song_index = it->first;
    auto& window = it->second;
    auto* song = music_bank_.GetSong(song_index);
    bool open = true;

    if (!song || !window.card || !window.view) {
      it = song_piano_rolls_.erase(it);
      continue;
    }

    // Use same docking class as tracker windows so they can dock together
    ImGui::SetNextWindowClass(&song_window_class_);

    if (window.card->Begin(&open)) {
      window.view->SetOnEditCallback([this]() { PushUndoState(); });
      window.view->SetOnNotePreview(
          [this, song_index](const zelda3::music::TrackEvent& evt,
                             int segment_idx, int channel_idx) {
            auto* target = music_bank_.GetSong(song_index);
            if (!target || !music_player_) return;
            music_player_->PreviewNote(*target, evt, segment_idx, channel_idx);
          });
      window.view->SetOnSegmentPreview(
          [this, song_index](const zelda3::music::MusicSong& /*unused*/,
                             int segment_idx) {
            auto* target = music_bank_.GetSong(song_index);
            if (!target || !music_player_) return;
            music_player_->PreviewSegment(*target, segment_idx);
          });
      // Update playback state for cursor visualization
      auto state = music_player_ ? music_player_->GetState() : editor::music::PlaybackState{};
      window.view->SetPlaybackState(state.is_playing, state.is_paused,
                                    state.current_tick);
      window.view->Draw(song);
    }
    window.card->End();

    if (!open) {
      it = song_piano_rolls_.erase(it);
    } else {
      ++it;
    }
  }

  DrawAsmPopups();

  return absl::OkStatus();
}

absl::Status MusicEditor::Save() {
  if (!rom_) return absl::FailedPreconditionError("No ROM loaded");
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

  auto storage_or =
      platform::WasmStorage::LoadProject(music_storage_key_);
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

void MusicEditor::MarkMusicDirty() { music_dirty_ = true; }

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
  return absl::UnimplementedError("Copy not yet implemented - clipboard support coming soon");
}

absl::Status MusicEditor::Paste() {
  // TODO: Paste from clipboard
  // Need to deserialize events and insert at cursor position
  return absl::UnimplementedError("Paste not yet implemented - clipboard support coming soon");
}

absl::Status MusicEditor::Undo() {
  if (undo_stack_.empty()) return absl::FailedPreconditionError("Nothing to undo");
  
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
  if (redo_stack_.empty()) return absl::FailedPreconditionError("Nothing to redo");

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
  if (!song) return;

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
  if (state.song_index >= 0 && state.song_index < static_cast<int>(music_bank_.GetSongCount())) {
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
  std::string card_title = absl::StrFormat(
      "[%02X] %s - Piano Roll###SongPianoRoll%d", song_index + 1,
      song ? song->name.c_str() : "Unknown", song_index);

  SongPianoRollWindow window;
  window.visible_flag = new bool(true);
  window.card =
      std::make_shared<gui::EditorCard>(card_title.c_str(), ICON_MD_PIANO,
                                        window.visible_flag);
  window.card->SetDefaultSize(900, 450);
  window.view = std::make_unique<editor::music::PianoRollView>();
  window.view->SetActiveChannel(0);
  window.view->SetActiveSegment(0);

  song_piano_rolls_[song_index] = std::move(window);
}

void MusicEditor::DrawSongTrackerWindow(int song_index) {
  auto* song = music_bank_.GetSong(song_index);
  if (!song) {
    ImGui::TextDisabled("Song not loaded");
    return;
  }

  // Compact toolbar for this song window
  bool can_play = music_player_ && music_player_->IsAudioReady();
  auto state = music_player_ ? music_player_->GetState() : editor::music::PlaybackState{};
  bool is_playing_this_song = state.is_playing && (state.playing_song_index == song_index);
  bool is_paused_this_song = state.is_paused && (state.playing_song_index == song_index);

  // === Row 1: Playback Transport ===
  if (!can_play) ImGui::BeginDisabled();

  // Play/Pause button with status indication
  if (is_playing_this_song && !is_paused_this_song) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 0.3f, 1.0f));
    if (ImGui::Button(ICON_MD_PAUSE " Pause")) {
      music_player_->Pause();
    }
    ImGui::PopStyleColor(2);
  } else if (is_paused_this_song) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.3f, 1.0f));
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
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("Stop playback");

  if (!can_play) ImGui::EndDisabled();

  // Keyboard shortcuts (when window is focused)
  if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && can_play) {
    // Space: Play/Pause toggle
    if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
      if (is_playing_this_song && !is_paused_this_song) {
        music_player_->Pause();
      } else if (is_paused_this_song) {
        music_player_->Resume();
      } else {
        music_player_->PlaySong(song_index);
      }
    }
    // Escape: Stop
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
      music_player_->Stop();
    }
    // +/=: Speed up (both + and = since + requires shift on most keyboards)
    if (ImGui::IsKeyPressed(ImGuiKey_Equal, false) ||
        ImGui::IsKeyPressed(ImGuiKey_KeypadAdd, false)) {
      music_player_->SetPlaybackSpeed(state.playback_speed + 0.1f);
    }
    // -: Speed down
    if (ImGui::IsKeyPressed(ImGuiKey_Minus, false) ||
        ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract, false)) {
      music_player_->SetPlaybackSpeed(state.playback_speed - 0.1f);
    }
  }

  // Status indicator
  ImGui::SameLine();
  if (is_playing_this_song && !is_paused_this_song) {
    ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), ICON_MD_GRAPHIC_EQ);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Playing");
  } else if (is_paused_this_song) {
    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.3f, 1.0f), ICON_MD_PAUSE_CIRCLE);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Paused");
  }

  // Right side controls
  float right_offset = ImGui::GetWindowWidth() - 200;
  ImGui::SameLine(right_offset);

  // Speed control (with mouse wheel support)
  ImGui::Text(ICON_MD_SPEED);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(55);
  float speed = state.playback_speed;
  if (gui::SliderFloatWheel("##Speed", &speed, 0.25f, 2.0f, "%.1fx",
                            0.1f)) {
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
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("Open Piano Roll view");

  // === Row 2: Song Info ===
  const char* bank_name = nullptr;
  switch (song->bank) {
    case 0: bank_name = "Overworld"; break;
    case 1: bank_name = "Dungeon"; break;
    case 2: bank_name = "Credits"; break;
    case 3: bank_name = "Expanded"; break;
    case 4: bank_name = "Auxiliary"; break;
    default: bank_name = "Unknown"; break;
  }
  ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "[%02X]", song_index + 1);
  ImGui::SameLine();
  ImGui::Text("%s", song->name.c_str());
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.7f, 1.0f), "(%s)", bank_name);

  if (song->modified) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), ICON_MD_EDIT " Modified");
  }

  // Segment count
  ImGui::SameLine(right_offset);
  ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                     "%zu segments", song->segments.size());

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
  auto state = music_player_ ? music_player_->GetState() : editor::music::PlaybackState{};
  
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
      ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), ICON_MD_EDIT " Modified");
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
      
      float progress = (total_duration > 0) 
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
    ImGui::Text("Segment: %d | Tick: %u", 
                state.current_segment_index + 1, state.current_tick);
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
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("Open song in dedicated tracker window");
  
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_PIANO " Open Piano Roll")) {
    OpenSongPianoRoll(current_song_index_);
  }
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("Open piano roll view for this song");

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
  if (song && current_segment_index_ >=
                  static_cast<int>(song->segments.size())) {
    current_segment_index_ = 0;
  }

  piano_roll_view_.SetActiveChannel(current_channel_index_);
  piano_roll_view_.SetActiveSegment(current_segment_index_);
  piano_roll_view_.SetOnEditCallback([this]() { PushUndoState(); });
  piano_roll_view_.SetOnNotePreview(
      [this, song_index = current_song_index_](
          const zelda3::music::TrackEvent& evt, int segment_idx,
          int channel_idx) {
        auto* target = music_bank_.GetSong(song_index);
        if (!target || !music_player_) return;
        music_player_->PreviewNote(*target, evt, segment_idx, channel_idx);
      });
  piano_roll_view_.SetOnSegmentPreview(
      [this, song_index = current_song_index_](
          const zelda3::music::MusicSong& /*unused*/, int segment_idx) {
        auto* target = music_bank_.GetSong(song_index);
        if (!target || !music_player_) return;
        music_player_->PreviewSegment(*target, segment_idx);
      });

  // Update playback state for cursor visualization
  auto state = music_player_ ? music_player_->GetState() : editor::music::PlaybackState{};
  piano_roll_view_.SetPlaybackState(state.is_playing, state.is_paused, state.current_tick);

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
  auto state = music_player_ ? music_player_->GetState() : editor::music::PlaybackState{};
  bool can_play = music_player_ && music_player_->IsAudioReady();

  // Row 1: Transport controls and song info
  auto* song = music_bank_.GetSong(current_song_index_);

  if (!can_play) ImGui::BeginDisabled();

  // Transport: Play/Pause with visual state indication
  const ImVec4 paused_color(0.9f, 0.7f, 0.2f, 1.0f);
  
  if (state.is_playing && !state.is_paused) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.2f, 1.0f));
    if (ImGui::Button(ICON_MD_PAUSE "##Pause")) music_player_->Pause();
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Pause (Space)");
  } else if (state.is_paused) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.4f, 0.1f, 1.0f));
    if (ImGui::Button(ICON_MD_PLAY_ARROW "##Resume")) music_player_->Resume();
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Resume (Space)");
  } else {
    if (ImGui::Button(ICON_MD_PLAY_ARROW "##Play")) music_player_->PlaySong(current_song_index_);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Play (Space)");
  }

  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_STOP "##Stop")) music_player_->Stop();
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("Stop (Escape)");

  if (!can_play) ImGui::EndDisabled();

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
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("Playback speed (+/- keys)");

  ImGui::SameLine();
  ImGui::Text(ICON_MD_VOLUME_UP);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(60);
  if (gui::SliderIntWheel("##Vol", &current_volume, 0, 100, "%d%%", 5)) {
    if (music_player_) music_player_->SetVolume(current_volume / 100.0f);
  }
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("Volume");

  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_REFRESH)) {
    music_bank_.LoadFromRom(*rom_);
    song_names_.clear();
  }
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reload from ROM");

  // Interpolation Control
  ImGui::SameLine();
  ImGui::SetNextItemWidth(100);
  {
    static int interpolation_type = 2;  // Default: Gaussian
    const char* items[] = {"Linear", "Hermite", "Gaussian", "Cosine", "Cubic"};
    if (ImGui::Combo("##Interp", &interpolation_type, items, IM_ARRAYSIZE(items))) {
      if (music_player_) music_player_->SetInterpolationType(interpolation_type);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Audio interpolation quality\nGaussian = authentic SNES sound");
  }

  ImGui::Separator();

  // Mixer / Visualizer Panel
  if (ImGui::BeginTable("MixerPanel", 9, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp)) {
    // Channel Headers
    ImGui::TableSetupColumn("Master", ImGuiTableColumnFlags_WidthFixed, 60.0f);
    for (int i = 0; i < 8; i++) {
      ImGui::TableSetupColumn(absl::StrFormat("Ch %d", i + 1).c_str());
    }
    ImGui::TableHeadersRow();

    ImGui::TableNextRow();

    // Master Oscilloscope (Column 0)
    ImGui::TableSetColumnIndex(0);
    if (emulator_) {
      auto& dsp = emulator_->snes().apu().dsp();
      
      ImGui::Text("Scope");
      
      // Oscilloscope
      const int16_t* buffer = dsp.GetSampleBuffer();
      uint16_t offset = dsp.GetSampleOffset();

      static float scope_values[128];
      // Handle ring buffer wrap-around correctly (buffer size is 0x400 samples)
      constexpr int kBufferSize = 0x400;
      for (int i = 0; i < 128; i++) {
        int sample_idx = ((offset - 128 + i + kBufferSize) & (kBufferSize - 1));
        scope_values[i] = static_cast<float>(buffer[sample_idx * 2]) / 32768.0f;  // Left channel
      }

      ImGui::PlotLines("##Scope", scope_values, 128, 0, nullptr, -1.0f, 1.0f, ImVec2(50, 60));
    }

    // Channel Strips (Columns 1-8)
    for (int i = 0; i < 8; i++) {
      ImGui::TableSetColumnIndex(i + 1);
      
      if (emulator_) {
        auto& dsp = emulator_->snes().apu().dsp();
        const auto& ch = dsp.GetChannel(i);
        
        // Mute/Solo Buttons
        bool is_muted = dsp.GetChannelMute(i);
        bool is_solo = channel_soloed_[i];
        const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();

        if (is_muted) {
          ImGui::PushStyleColor(ImGuiCol_Button, gui::ConvertColorToImVec4(theme.error));
        }
        if (ImGui::Button(absl::StrFormat("M##%d", i).c_str(), ImVec2(25, 20))) {
          dsp.SetChannelMute(i, !is_muted);
        }
        if (is_muted) ImGui::PopStyleColor();

        ImGui::SameLine();

        if (is_solo) {
          ImGui::PushStyleColor(ImGuiCol_Button, gui::ConvertColorToImVec4(theme.warning));
        }
        if (ImGui::Button(absl::StrFormat("S##%d", i).c_str(), ImVec2(25, 20))) {
          channel_soloed_[i] = !channel_soloed_[i];
          
          bool any_solo = false;
          for(int j=0; j<8; j++) if(channel_soloed_[j]) any_solo = true;
          
          for(int j=0; j<8; j++) {
            if (any_solo) {
              dsp.SetChannelMute(j, !channel_soloed_[j]);
            } else {
              dsp.SetChannelMute(j, false);
            }
          }
        }
        if (is_solo) ImGui::PopStyleColor();

        // VU Meter
        float level = std::abs(ch.sampleOut) / 32768.0f;
        ImGui::ProgressBar(level, ImVec2(-1, 60), "");
        
        // Info
        ImGui::Text("Vol: %d %d", ch.volumeL, ch.volumeR);
        ImGui::Text("Pitch: %04X", ch.pitch);
        
        // Key On Indicator
        if (ch.keyOn) {
          ImGui::TextColored(gui::ConvertColorToImVec4(theme.success), "KEY ON");
        } else {
          ImGui::TextDisabled("---");
        }
      } else {
        ImGui::TextDisabled("Offline");
      }
    }
    
    ImGui::EndTable();
  }
}

void MusicEditor::DrawChannelOverview() {
  if (!music_player_ || !music_player_->IsAudioReady()) {
    ImGui::TextDisabled("Audio not ready");
    return;
  }

  auto channel_states = music_player_->GetChannelStates();

  if (ImGui::BeginTable("ChannelOverview", 9,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp |
                            ImGuiTableFlags_NoHostExtendY)) {
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
        case 0: adsr_str = "Att"; break;
        case 1: adsr_str = "Dec"; break;
        case 2: adsr_str = "Sus"; break;
        case 3: adsr_str = "Rel"; break;
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
  if (music_player_) music_player_->SeekToSegment(segment_index);
}

// ============================================================================
// ASM Export/Import
// ============================================================================

void MusicEditor::ExportSongToAsm(int song_index) {
  auto* song = music_bank_.GetSong(song_index);
  if (!song) {
    LOG_WARN("MusicEditor", "ExportSongToAsm: Invalid song index %d", song_index);
    return;
  }

  // Configure export options
  zelda3::music::AsmExportOptions options;
  options.label_prefix = song->name;
  // Remove spaces and special characters from label
  std::replace(options.label_prefix.begin(), options.label_prefix.end(), ' ', '_');
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
    asm_import_error_ = result.status().message();
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
    ImGui::InputTextMultiline("##AsmExportText", &asm_buffer_,
                              ImVec2(520, 260),
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
    int song_slot = (asm_import_target_index_ >= 0)
                        ? asm_import_target_index_ + 1
                        : -1;
    if (song_slot > 0) {
      ImGui::Text("Target Song: [%02X]", song_slot);
    } else {
      ImGui::TextDisabled("Select a song to import into");
    }
    ImGui::TextWrapped("Paste Oracle of Secrets-compatible ASM here.");

    ImGui::InputTextMultiline("##AsmImportText", &asm_buffer_,
                              ImVec2(520, 260),
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
