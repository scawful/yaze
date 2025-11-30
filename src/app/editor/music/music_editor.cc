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
    return music_bank_.LoadFromRom(*rom_);
  }
  return absl::OkStatus();
}

absl::Status MusicEditor::Update() {
  // Run emulator frames when playing music (generates audio samples)
  // NOTE: EditorManager::Update() calls emulator_.RunFrameOnly() or Run()
  // so we don't need to call it here. Double-calling causes speedup issues.

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

  song_browser_card.SetDefaultSize(300, 700);
  playback_card.SetDefaultSize(400, 350);
  piano_roll_card.SetDefaultSize(900, 400);
  instrument_card.SetDefaultSize(600, 500);
  sample_card.SetDefaultSize(600, 500);
  assembly_card.SetDefaultSize(700, 600);

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
    if (playback_card.Begin(playback_visible)) {
      DrawPlaybackControl();
    }
    playback_card.End();
  }

  // Piano Roll Card
  bool* piano_roll_visible = card_registry->GetVisibilityFlag("music.piano_roll");
  if (piano_roll_visible && *piano_roll_visible) {
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
      DrawInstrumentEditor();
    }
    instrument_card.End();
  }

  // Sample Editor Card
  bool* sample_visible =
      card_registry->GetVisibilityFlag("music.sample_editor");
  if (sample_visible && *sample_visible) {
    if (sample_card.Begin(sample_visible)) {
      sample_editor_view_.SetOnEditCallback([this]() { PushUndoState(); });
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
            if (!target) return;
            PreviewNote(*target, evt, segment_idx, channel_idx);
          });
      window.view->SetOnSegmentPreview(
          [this, song_index](const zelda3::music::MusicSong& /*unused*/,
                             int segment_idx) {
            auto* target = music_bank_.GetSong(song_index);
            if (!target) return;
            PreviewSegment(*target, segment_idx);
          });
      window.view->Draw(song);
    }
    window.card->End();

    if (!open) {
      it = song_piano_rolls_.erase(it);
    } else {
      ++it;
    }
  }

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
  bool can_play = emulator_ && rom_;
  bool is_playing_this_song = is_playing_ && (current_song_index_ == song_index);
  bool is_paused_this_song = is_paused_ && (current_song_index_ == song_index);

  // === Row 1: Playback Transport ===
  if (!can_play) ImGui::BeginDisabled();

  // Play/Pause button with status indication
  if (is_playing_this_song && !is_paused_this_song) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 0.3f, 1.0f));
    if (ImGui::Button(ICON_MD_PAUSE " Pause")) {
      PauseSong();
    }
    ImGui::PopStyleColor(2);
  } else if (is_paused_this_song) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.3f, 1.0f));
    if (ImGui::Button(ICON_MD_PLAY_ARROW " Resume")) {
      ResumeSong();
    }
    ImGui::PopStyleColor(2);
  } else {
    if (ImGui::Button(ICON_MD_PLAY_ARROW " Play")) {
      current_song_index_ = song_index;
      PlaySong(song_index + 1);
    }
  }

  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_STOP)) {
    StopSong();
  }
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("Stop playback");

  if (!can_play) ImGui::EndDisabled();

  // Keyboard shortcuts (when window is focused)
  if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && can_play) {
    // Space: Play/Pause toggle
    if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
      if (is_playing_this_song && !is_paused_this_song) {
        PauseSong();
      } else if (is_paused_this_song) {
        ResumeSong();
      } else {
        current_song_index_ = song_index;
        PlaySong(song_index + 1);
      }
    }
    // Escape: Stop
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
      StopSong();
    }
    // +/=: Speed up (both + and = since + requires shift on most keyboards)
    if (ImGui::IsKeyPressed(ImGuiKey_Equal, false) ||
        ImGui::IsKeyPressed(ImGuiKey_KeypadAdd, false)) {
      playback_speed_ = std::clamp(playback_speed_ + 0.1f, 0.25f, 2.0f);
      if (emulator_) emulator_->set_playback_speed(playback_speed_);
    }
    // -: Speed down
    if (ImGui::IsKeyPressed(ImGuiKey_Minus, false) ||
        ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract, false)) {
      playback_speed_ = std::clamp(playback_speed_ - 0.1f, 0.25f, 2.0f);
      if (emulator_) emulator_->set_playback_speed(playback_speed_);
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
  if (gui::SliderFloatWheel("##Speed", &playback_speed_, 0.25f, 2.0f, "%.1fx",
                            0.1f)) {
    if (emulator_) {
      emulator_->set_playback_speed(playback_speed_);
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
  const char* bank_name = song->bank == 0   ? "Overworld"
                         : song->bank == 1 ? "Dungeon"
                                           : "Credits";
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
  if (song) {
    ImGui::Text("Current Song:");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "[%02X] %s",
                       current_song_index_ + 1, song->name.c_str());
  }

  // Channel overview when playing
  if (is_playing_) {
    ImGui::Separator();
    DrawChannelOverview();
  }

  ImGui::Separator();

  // Help text
  ImGui::TextWrapped(
      "Double-click a song in the Song Browser to open a dedicated "
      "tracker window for editing. Use this panel for playback controls.");

  // Quick action buttons
  ImGui::Spacing();
  if (ImGui::Button(ICON_MD_OPEN_IN_NEW " Open Current Song")) {
    OpenSong(current_song_index_);
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_PIANO " Open Piano Roll")) {
    OpenSongPianoRoll(current_song_index_);
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
        if (!target) return;
        PreviewNote(*target, evt, segment_idx, channel_idx);
      });
  piano_roll_view_.SetOnSegmentPreview(
      [this, song_index = current_song_index_](
          const zelda3::music::MusicSong& /*unused*/, int segment_idx) {
        auto* target = music_bank_.GetSong(song_index);
        if (!target) return;
        PreviewSegment(*target, segment_idx);
      });

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

  // Check if we have the prerequisites for playback
  bool can_play = emulator_ && rom_;
  bool is_emu_running = emulator_ && emulator_->running() && emulator_->is_snes_initialized();

  // Row 1: Song info and playback controls
  auto* song = music_bank_.GetSong(current_song_index_);

  if (!can_play) ImGui::BeginDisabled();

  // Compact playback controls
  if (is_playing_ && !is_paused_) {
    if (ImGui::Button(ICON_MD_PAUSE "##Pause")) PauseSong();
  } else if (is_paused_) {
    if (ImGui::Button(ICON_MD_PLAY_ARROW "##Resume")) ResumeSong();
  } else {
    if (ImGui::Button(ICON_MD_PLAY_ARROW "##Play")) PlaySong(current_song_index_ + 1);
  }
  if (ImGui::IsItemHovered()) ImGui::SetTooltip(is_playing_ ? "Pause" : "Play");

  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_STOP "##Stop")) StopSong();
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("Stop");

  if (!can_play) ImGui::EndDisabled();

  // Song label with status
  ImGui::SameLine();
  if (song) {
    if (is_playing_ && current_song_index_ >= 0) {
      ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), ICON_MD_VOLUME_UP);
      ImGui::SameLine();
    }
    ImGui::Text("%s", song->name.c_str());
    if (song->modified) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "*");
    }
  } else {
    ImGui::TextDisabled("No song");
  }

  // Volume and options on same line, right-aligned
  ImGui::SameLine(ImGui::GetWindowWidth() - 320);

  // Speed control (with mouse wheel)
  ImGui::SetNextItemWidth(60);
  if (gui::SliderFloatWheel("##Speed", &playback_speed_, 0.25f, 2.0f, "%.2fx",
                            0.1f)) {
    if (emulator_) {
      emulator_->set_playback_speed(playback_speed_);
    }
  }
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("Playback speed - use wheel");

  ImGui::SameLine();
  ImGui::SetNextItemWidth(60);
  if (gui::SliderIntWheel("##Vol", &current_volume, 0, 100, "%d%%", 5)) {
    SetVolume(current_volume / 100.0f);
  }
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("Volume - use wheel");

  ImGui::SameLine();
  if (!can_play) {
    ImGui::BeginDisabled();
    if (ImGui::Button(ICON_MD_REFRESH)) {}
    ImGui::EndDisabled();
  } else {
    if (ImGui::Button(ICON_MD_REFRESH)) {
      music_bank_.LoadFromRom(*rom_);
      song_names_.clear();
    }
  }
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reload from ROM");

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
  const bool have_dsp = emulator_ && emulator_->is_snes_initialized();

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
    if (!have_dsp) {
      ImGui::TextDisabled("Offline");
    } else {
      ImGui::Text("DSP Live");
    }

    for (int ch = 0; ch < 8; ++ch) {
      ImGui::TableSetColumnIndex(ch + 1);
      if (!have_dsp) {
        ImGui::TextDisabled("No audio");
        continue;
      }

      auto& dsp = emulator_->snes().apu().dsp();
      const auto& channel = dsp.GetChannel(ch);
      float level = std::abs(channel.sampleOut) / 32768.0f;
      level = std::clamp(level, 0.0f, 1.0f);
      ImGui::ProgressBar(level, ImVec2(-1, 14.0f));
      ImGui::Text("V %d/%d", channel.volumeL, channel.volumeR);
      ImGui::Text("P %04X", channel.pitch);
    }

    ImGui::EndTable();
  }
}

// ============================================================================
// Audio Control Methods (Emulator Integration)
// ============================================================================

void MusicEditor::LoadSong(int index) {
  if (index >= 0 && index < static_cast<int>(music_bank_.GetSongCount())) {
    current_song_index_ = index;
    // If we wanted to auto-play or reset views, do it here
  }
}

void MusicEditor::PlaySong(int song_id) {
  if (!emulator_) {
    LOG_WARN("MusicEditor", "No emulator instance - cannot play song");
    return;
  }

  if (!rom_) {
    LOG_WARN("MusicEditor", "No ROM loaded - cannot play song");
    return;
  }

  // Use direct SPC mode by default (bypasses game, immediate playback)
  if (use_direct_spc_) {
    PlaySongDirect(song_id);
    return;
  }

  // Fallback: game-based playback (requires game to be running)
  // Auto-initialize emulator if needed
  if (!emulator_->is_snes_initialized()) {
    if (!emulator_->EnsureInitialized(rom_)) {
      LOG_ERROR("MusicEditor", "Failed to initialize emulator for playback");
      return;
    }
    LOG_INFO("MusicEditor", "Auto-initialized emulator for music playback");
  }

  // Start emulator running if not already
  if (!emulator_->running()) {
    emulator_->set_running(true);
    LOG_INFO("MusicEditor", "Started emulator for music playback");
  }

  try {
    // Start audio backend playback first
    if (auto* audio = emulator_->audio_backend()) {
      auto status = audio->GetStatus();
      if (!status.is_playing) {
        audio->Play();
        LOG_INFO("MusicEditor", "Started audio backend playback");
      }
    }

    // Write song ID to the music register
    // The game will pick this up and trigger the music change
    emulator_->snes().Write(0x7E012C, static_cast<uint8_t>(song_id));
    LOG_INFO("MusicEditor", "Requested song %d (game-based)", song_id);

    is_playing_ = true;
  } catch (const std::exception& e) {
    LOG_ERROR("MusicEditor", "Failed to play song: %s", e.what());
  }
}

void MusicEditor::PauseSong() {
  if (!emulator_ || !is_playing_ || is_paused_) return;

  // Actually pause emulator execution (not just mute)
  emulator_->set_running(false);

  if (auto* audio = emulator_->audio_backend()) {
    audio->Pause();
  }

  is_paused_ = true;
  LOG_INFO("MusicEditor", "Paused playback");
}

void MusicEditor::ResumeSong() {
  if (!emulator_ || !is_paused_) return;

  // Reset timing to prevent time buildup during pause
  emulator_->ResetFrameTiming();

  emulator_->set_running(true);

  if (auto* audio = emulator_->audio_backend()) {
    audio->Play();
  }

  is_paused_ = false;
  LOG_INFO("MusicEditor", "Resumed playback");
}

void MusicEditor::StopSong() {
  if (!emulator_)
    return;

  try {
    if (use_direct_spc_ && spc_initialized_) {
      // Direct SPC: Send stop command via ports
      auto& apu = emulator_->snes().apu();
      apu.in_ports_[0] = 0x00;  // Song 0 = silence/stop
      apu.in_ports_[1] = 0xFF;  // Stop command

      // Run cycles to let driver process the stop command
      for (int i = 0; i < 16000; i++) {
        apu.Cycle();
      }

      LOG_INFO("MusicEditor", "Stopped music (direct SPC)");
    } else {
      // Game-based: Write stop to music register
      emulator_->snes().Write(0x7E012C, 0xFF);
      LOG_INFO("MusicEditor", "Stopped music (game-based)");
    }

    // Actually stop the emulator, not just pause audio
    emulator_->set_running(false);

    if (auto* audio = emulator_->audio_backend()) {
      audio->Stop();  // Full stop, not just pause
    }

    is_playing_ = false;
    is_paused_ = false;
  } catch (const std::exception& e) {
    LOG_ERROR("MusicEditor", "Failed to stop song: %s", e.what());
  }
}

void MusicEditor::SetVolume(float volume) {
  if (!emulator_)
    return;

  volume = std::clamp(volume, 0.0f, 1.0f);

  if (auto* audio = emulator_->audio_backend()) {
    audio->SetVolume(volume);
    LOG_DEBUG("MusicEditor", "Set volume to %.2f", volume);
  } else {
    LOG_WARN("MusicEditor", "No audio backend available");
  }
}

const zelda3::music::MusicInstrument* MusicEditor::ResolveInstrumentForEvent(
    const zelda3::music::MusicSegment& segment, int channel_index,
    uint16_t tick) const {
  if (channel_index < 0 || channel_index >= 8) return nullptr;
  const auto& track = segment.tracks[channel_index];

  int instrument_index = -1;
  for (const auto& evt : track.events) {
    if (evt.tick > tick) break;
    if (evt.type == zelda3::music::TrackEvent::Type::Command &&
        evt.command.opcode == 0xE0) {
      instrument_index = evt.command.params[0];
    }
  }

  return music_bank_.GetInstrument(instrument_index);
}

void MusicEditor::PreviewNote(const zelda3::music::MusicSong& song,
                              const zelda3::music::TrackEvent& event,
                              int segment_index, int channel_index) {
  // Authentic SPC preview - requires emulator and ROM
  if (!emulator_ || !rom_) {
    LOG_DEBUG("MusicEditor", "Note preview requires ROM to be loaded");
    return;
  }

  if (event.type != zelda3::music::TrackEvent::Type::Note ||
      !event.note.IsNote()) {
    return;
  }

  // Ensure SPC is initialized with driver
  InitializeDirectSpc();

  auto& apu = emulator_->snes().apu();

  // Get instrument from track context
  const zelda3::music::MusicSegment* segment = nullptr;
  if (segment_index >= 0 &&
      segment_index < static_cast<int>(song.segments.size())) {
    segment = &song.segments[segment_index];
  }

  const auto* instrument =
      segment ? ResolveInstrumentForEvent(*segment, channel_index, event.tick)
              : nullptr;

  // Direct DSP register writes for single note preview
  // DSP register layout per channel (base + offset):
  //   +$00: VOL(L) - Left volume
  //   +$01: VOL(R) - Right volume
  //   +$02: P(L)   - Pitch (low byte)
  //   +$03: P(H)   - Pitch (high byte, bits 0-5)
  //   +$04: SRCN   - Source number (sample index)
  //   +$05: ADSR1  - ADSR settings 1
  //   +$06: ADSR2  - ADSR settings 2
  //   +$07: GAIN   - Gain settings

  int ch_base = channel_index * 0x10;

  // Set source number (instrument sample)
  int inst_idx = instrument ? instrument->sample_index : 0;
  apu.WriteToDsp(ch_base + 0x04, inst_idx);  // SRCN

  // Convert note pitch to SPC pitch format
  // Note: This is a simplified conversion; actual N-SPC uses pitch tables
  uint16_t pitch = static_cast<uint16_t>(event.note.pitch) << 4;
  apu.WriteToDsp(ch_base + 0x02, pitch & 0xFF);        // P(L)
  apu.WriteToDsp(ch_base + 0x03, (pitch >> 8) & 0x3F); // P(H)

  // Set volume (max for preview)
  apu.WriteToDsp(ch_base + 0x00, 0x7F);  // VOL(L)
  apu.WriteToDsp(ch_base + 0x01, 0x7F);  // VOL(R)

  // Set ADSR for quick attack
  apu.WriteToDsp(ch_base + 0x05, 0xFF);  // ADSR1: Enable ADSR, fast attack
  apu.WriteToDsp(ch_base + 0x06, 0xE0);  // ADSR2: Sustain level

  // Trigger key-on for this channel only
  apu.WriteToDsp(0x4C, 1 << channel_index);  // KON register

  // Run cycles to generate some audio
  for (int i = 0; i < 5000; i++) {
    apu.Cycle();
  }

  // Start audio playback
  if (auto* audio = emulator_->audio_backend()) {
    audio->Play();
  }

  // Start emulator to continue generating audio
  emulator_->set_running(true);

  LOG_DEBUG("MusicEditor", "Note preview: channel=%d, pitch=%d, inst=%d",
            channel_index, event.note.pitch, inst_idx);
}

void MusicEditor::PreviewSegment(const zelda3::music::MusicSong& song,
                                 int segment_index) {
  // N-SPC driver doesn't support seeking to arbitrary segments.
  // For authentic playback, we play the full song using the SPC emulator.
  // Parameters kept for API compatibility.
  (void)song;
  (void)segment_index;

  if (!emulator_ || !rom_) {
    LOG_DEBUG("MusicEditor", "Segment preview requires ROM to be loaded");
    return;
  }

  // Play the song using authentic SPC emulation
  int song_id = current_song_index_;
  if (song_id >= 0 && song_id < static_cast<int>(music_bank_.GetSongCount())) {
    PlaySongDirect(song_id);
  }
}

// ============================================================================
// Direct SPC Playback (bypasses game music system)
// ============================================================================

uint32_t MusicEditor::GetBankRomOffset(uint8_t bank) const {
  // ROM offsets for sound bank block headers
  // Each bank has blocks: [size:2][aram_addr:2][data:size]
  //
  // IMPORTANT: The 'bank' parameter here is the ROM bank index (0-3):
  //   0 = Common bank (driver, samples, instruments) at 0xC8000
  //   1 = Overworld song data at 0xD1EF5
  //   2 = Dungeon song data at 0xD8000
  //   3 = Credits song data at 0xD5380
  //
  // This is DIFFERENT from song.bank enum values (0=overworld, 1=dungeon, 2=credits)!
  // Use GetSongBankRomOffset() to convert song.bank to ROM offset.
  constexpr uint32_t kSoundBankOffsets[] = {
      0xC8000,   // ROM Bank 0 (common) - driver + samples + instruments
      0xD1EF5,   // ROM Bank 1 (overworld songs)
      0xD8000,   // ROM Bank 2 (dungeon songs)
      0xD5380    // ROM Bank 3 (credits songs)
  };

  if (bank < 4) {
    return kSoundBankOffsets[bank];
  }
  return kSoundBankOffsets[0];  // Default to common bank
}

int MusicEditor::GetSongIndexInBank(int song_id, uint8_t bank) const {
  // Convert global song ID (1-based) to bank-local index (0-based)
  // Bank 0 (overworld): songs 1-11 → indices 0-10
  // Bank 1 (dungeon):   songs 12-31 → indices 0-19
  // Bank 2 (credits):   songs 32-34 → indices 0-2

  switch (bank) {
    case 0:  // Overworld
      return song_id - 1;  // Songs 1-11 → 0-10
    case 1:  // Dungeon
      return song_id - 12; // Songs 12-31 → 0-19
    case 2:  // Credits
      return song_id - 32; // Songs 32-34 → 0-2
    default:
      return 0;
  }
}

void MusicEditor::UploadSoundBankFromRom(uint32_t rom_offset) {
  if (!emulator_ || !rom_) return;

  auto& apu = emulator_->snes().apu();
  const uint8_t* rom_data = rom_->data();
  const size_t rom_size = rom_->size();

  LOG_INFO("MusicEditor", "Uploading sound bank from ROM offset 0x%X", rom_offset);

  // Parse and upload blocks: [size:2][aram_addr:2][data:size]
  int block_count = 0;
  while (rom_offset + 4 < rom_size) {
    uint16_t block_size = rom_data[rom_offset] | (rom_data[rom_offset + 1] << 8);
    uint16_t aram_addr = rom_data[rom_offset + 2] | (rom_data[rom_offset + 3] << 8);

    // End of blocks marker (size = 0) or invalid size
    if (block_size == 0 || block_size > 0x10000) {
      LOG_INFO("MusicEditor", "End of blocks at offset 0x%X (size=%d)",
               rom_offset, block_size);
      break;
    }

    // Validate we have enough data
    if (rom_offset + 4 + block_size > rom_size) {
      LOG_WARN("MusicEditor", "Block at 0x%X extends past ROM end", rom_offset);
      break;
    }

    // Upload block to ARAM
    apu.WriteDma(aram_addr, &rom_data[rom_offset + 4], block_size);

    LOG_DEBUG("MusicEditor", "  Block %d: %d bytes -> ARAM $%04X",
              block_count, block_size, aram_addr);

    rom_offset += 4 + block_size;
    block_count++;
  }

  LOG_INFO("MusicEditor", "Uploaded %d blocks to ARAM", block_count);
}

void MusicEditor::InitializeDirectSpc() {
  if (!emulator_ || !rom_) return;
  if (spc_initialized_) return;

  auto& apu = emulator_->snes().apu();

  LOG_INFO("MusicEditor", "Initializing direct SPC playback");

  // 1. Reset APU to clean state
  apu.Reset();

  // 2. Upload common bank (Bank 0) - contains:
  // - SPC driver code ($0800)
  // - Sample pointers ($3C00)
  // - Instrument data ($3D00)
  // - BRR sample data ($4000+)
  UploadSoundBankFromRom(GetBankRomOffset(0));

  // 3. Bootstrap SPC directly to driver entry point
  //    N-SPC driver entry is at $0800 after IPL transfer
  apu.BootstrapDirect(0x0800);

  // 4. Run cycles to let driver initialize
  //    Driver needs time to set up its internal state
  //    Give it a full frame worth of cycles (~32000)
  for (int i = 0; i < 32000; i++) {
    apu.Cycle();
  }

  spc_initialized_ = true;
  current_spc_bank_ = 0xFF;  // No song bank loaded yet

  LOG_INFO("MusicEditor", "Direct SPC initialized - driver running at $0800");
}

void MusicEditor::PlaySongDirect(int song_id) {
  if (!emulator_ || !rom_) {
    LOG_WARN("MusicEditor", "Cannot play direct - no emulator/ROM");
    return;
  }

  // Ensure emulator is initialized
  if (!emulator_->is_snes_initialized()) {
    if (!emulator_->EnsureInitialized(rom_)) {
      LOG_ERROR("MusicEditor", "Failed to initialize emulator");
      return;
    }
  }

  // Initialize SPC with common bank and bootstrap driver if needed
  InitializeDirectSpc();

  // Get song info
  auto* song = music_bank_.GetSong(song_id - 1);  // Convert to 0-based index
  if (!song) {
    LOG_ERROR("MusicEditor", "Song %d not found", song_id);
    return;
  }

  // song.bank is the enum value: 0=overworld, 1=dungeon, 2=credits
  uint8_t song_bank = song->bank;
  LOG_INFO("MusicEditor", "Playing song %d (%s) from song_bank=%d",
           song_id, song->name.c_str(), song_bank);

  auto& apu = emulator_->snes().apu();

  // Upload song bank if different from current
  // Map song.bank enum to ROM bank offset index:
  //   song.bank 0 (overworld) → ROM bank 1 (0xD1EF5)
  //   song.bank 1 (dungeon)   → ROM bank 2 (0xD8000)
  //   song.bank 2 (credits)   → ROM bank 3 (0xD5380)
  if (current_spc_bank_ != song_bank) {
    uint8_t rom_bank = song_bank + 1;  // Convert: 0→1, 1→2, 2→3
    LOG_INFO("MusicEditor", "Uploading song bank: song_bank=%d -> rom_bank=%d",
             song_bank, rom_bank);
    UploadSoundBankFromRom(GetBankRomOffset(rom_bank));
    current_spc_bank_ = song_bank;
  }

  // ALTTP N-SPC expects the global song ID (1-based), not bank-local index
  // The driver internally maps this to the correct bank and song data
  //
  // N-SPC port protocol:
  // Port $F4 (in_ports_[0]): Global song ID (1-34 for vanilla songs)
  // Port $F5 (in_ports_[1]): Command/acknowledge byte
  //
  // Note: Setting in_ports_[1] to a non-matching value triggers the change
  uint8_t global_song_id = static_cast<uint8_t>(song_id);

  // Toggle port 1 to trigger driver to notice the change
  static uint8_t trigger_byte = 0x00;
  trigger_byte ^= 0x01;  // Alternate between 0x00 and 0x01

  apu.in_ports_[0] = global_song_id;
  apu.in_ports_[1] = trigger_byte;

  LOG_INFO("MusicEditor", "Sent play command: global_song_id=%d (0x%02X) to SPC ports",
           global_song_id, global_song_id);

  // Run cycles to let driver process the command
  // The SPC700 runs at ~1.024MHz, so ~32000 cycles per frame at 60fps
  // Give it half a frame worth to process the command
  for (int i = 0; i < 16000; i++) {
    apu.Cycle();
  }

  // Reset frame timing to prevent accumulated time from causing fast playback
  emulator_->ResetFrameTiming();

  // Sync playback speed with emulator
  emulator_->set_playback_speed(playback_speed_);

  // Start audio backend
  if (auto* audio = emulator_->audio_backend()) {
    auto status = audio->GetStatus();
    if (!status.is_playing) {
      audio->Play();
    }
  }

  // Ensure emulator is running
  emulator_->set_running(true);
  is_playing_ = true;
  is_paused_ = false;

  LOG_INFO("MusicEditor", "PlaySongDirect complete - emulator running at %.2fx speed",
           playback_speed_);
}

}  // namespace editor
}  // namespace yaze
