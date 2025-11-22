#include "music_editor.h"

#include "absl/strings/str_format.h"
#include "app/editor/code/assembly_editor.h"
#include "app/editor/system/editor_card_registry.h"
#include "app/emu/emulator.h"
#include "app/gfx/debug/performance/performance_profiler.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "imgui/imgui.h"
#include "util/log.h"

namespace yaze {
namespace editor {

void MusicEditor::Initialize() {
  if (!dependencies_.card_registry)
    return;
  auto* card_registry = dependencies_.card_registry;

  card_registry->RegisterCard({.card_id = "music.tracker",
                               .display_name = "Music Tracker",
                               .icon = ICON_MD_MUSIC_NOTE,
                               .category = "Music",
                               .shortcut_hint = "Ctrl+Shift+M",
                               .priority = 10});
  card_registry->RegisterCard({.card_id = "music.instrument_editor",
                               .display_name = "Instrument Editor",
                               .icon = ICON_MD_PIANO,
                               .category = "Music",
                               .shortcut_hint = "Ctrl+Shift+I",
                               .priority = 20});
  card_registry->RegisterCard({.card_id = "music.assembly",
                               .display_name = "Assembly View",
                               .icon = ICON_MD_CODE,
                               .category = "Music",
                               .shortcut_hint = "Ctrl+Shift+A",
                               .priority = 30});

  // Show tracker by default
  card_registry->ShowCard("music.tracker");
}

absl::Status MusicEditor::Load() {
  gfx::ScopedTimer timer("MusicEditor::Load");
  return absl::OkStatus();
}

absl::Status MusicEditor::Update() {
  if (!dependencies_.card_registry)
    return absl::OkStatus();
  auto* card_registry = dependencies_.card_registry;

  static gui::EditorCard tracker_card("Music Tracker", ICON_MD_MUSIC_NOTE);
  static gui::EditorCard instrument_card("Instrument Editor", ICON_MD_PIANO);
  static gui::EditorCard assembly_card("Assembly View", ICON_MD_CODE);

  tracker_card.SetDefaultSize(900, 700);
  instrument_card.SetDefaultSize(600, 500);
  assembly_card.SetDefaultSize(700, 600);

  // Music Tracker Card - Check visibility flag exists and is true before
  // rendering
  bool* tracker_visible = card_registry->GetVisibilityFlag("music.tracker");
  if (tracker_visible && *tracker_visible) {
    if (tracker_card.Begin(tracker_visible)) {
      DrawTrackerView();
    }
    tracker_card.End();
  }

  // Instrument Editor Card - Check visibility flag exists and is true before
  // rendering
  bool* instrument_visible =
      card_registry->GetVisibilityFlag("music.instrument_editor");
  if (instrument_visible && *instrument_visible) {
    if (instrument_card.Begin(instrument_visible)) {
      DrawInstrumentEditor();
    }
    instrument_card.End();
  }

  // Assembly View Card - Check visibility flag exists and is true before
  // rendering
  bool* assembly_visible = card_registry->GetVisibilityFlag("music.assembly");
  if (assembly_visible && *assembly_visible) {
    if (assembly_card.Begin(assembly_visible)) {
      assembly_editor_.InlineUpdate();
    }
    assembly_card.End();
  }

  return absl::OkStatus();
}

static const int NUM_KEYS = 25;
static bool keys[NUM_KEYS];

static void DrawPianoStaff() {
  if (ImGuiID child_id = ImGui::GetID((void*)(intptr_t)9);
      ImGui::BeginChild(child_id, ImVec2(0, 170), false)) {
    const int NUM_LINES = 5;
    const int LINE_THICKNESS = 2;
    const int LINE_SPACING = 40;

    // Get the draw list for the current window
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // Draw the staff lines
    ImVec2 canvas_p0 =
        ImVec2(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y);
    ImVec2 canvas_p1 = ImVec2(canvas_p0.x + ImGui::GetContentRegionAvail().x,
                              canvas_p0.y + ImGui::GetContentRegionAvail().y);
    draw_list->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(32, 32, 32, 255));
    for (int i = 0; i < NUM_LINES; i++) {
      auto line_start = ImVec2(canvas_p0.x, canvas_p0.y + i * LINE_SPACING);
      auto line_end = ImVec2(canvas_p1.x + ImGui::GetContentRegionAvail().x,
                             canvas_p0.y + i * LINE_SPACING);
      draw_list->AddLine(line_start, line_end, IM_COL32(200, 200, 200, 255),
                         LINE_THICKNESS);
    }

    // Draw the ledger lines
    const int NUM_LEDGER_LINES = 3;
    for (int i = -NUM_LEDGER_LINES; i <= NUM_LINES + NUM_LEDGER_LINES; i++) {
      if (i % 2 == 0)
        continue;  // skip every other line
      auto line_start = ImVec2(canvas_p0.x, canvas_p0.y + i * LINE_SPACING / 2);
      auto line_end = ImVec2(canvas_p1.x + ImGui::GetContentRegionAvail().x,
                             canvas_p0.y + i * LINE_SPACING / 2);
      draw_list->AddLine(line_start, line_end, IM_COL32(150, 150, 150, 255),
                         LINE_THICKNESS);
    }
  }
  ImGui::EndChild();
}

static void DrawPianoRoll() {
  // Render the piano roll
  float key_width = ImGui::GetContentRegionAvail().x / NUM_KEYS;
  float white_key_height = ImGui::GetContentRegionAvail().y * 0.8f;
  float black_key_height = ImGui::GetContentRegionAvail().y * 0.5f;
  ImGui::Text("Piano Roll");
  ImGui::Separator();
  ImDrawList* draw_list = ImGui::GetWindowDrawList();

  // Draw the staff lines
  ImVec2 canvas_p0 =
      ImVec2(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y);
  ImVec2 canvas_p1 = ImVec2(canvas_p0.x + ImGui::GetContentRegionAvail().x,
                            canvas_p0.y + ImGui::GetContentRegionAvail().y);
  draw_list->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(200, 200, 200, 255));

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.f, 0.f));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.f);
  for (int i = 0; i < NUM_KEYS; i++) {
    // Calculate the position and size of the key
    ImVec2 key_pos = ImVec2(i * key_width, 0.0f);
    ImVec2 key_size;
    ImVec4 key_color;
    ImVec4 text_color;
    if (i % 12 == 1 || i % 12 == 3 || i % 12 == 6 || i % 12 == 8 ||
        i % 12 == 10) {
      // This is a black key
      key_size = ImVec2(key_width * 0.6f, black_key_height);
      key_color = ImVec4(0, 0, 0, 255);
      text_color = ImVec4(255, 255, 255, 255);
    } else {
      // This is a white key
      key_size = ImVec2(key_width, white_key_height);
      key_color = ImVec4(255, 255, 255, 255);
      text_color = ImVec4(0, 0, 0, 255);
    }

    ImGui::PushID(i);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_Button, key_color);
    ImGui::PushStyleColor(ImGuiCol_Text, text_color);
    if (ImGui::Button(kSongNotes[i].data(), key_size)) {
      keys[i] ^= 1;
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    ImVec2 button_pos = ImGui::GetItemRectMin();
    ImVec2 button_size = ImGui::GetItemRectSize();
    if (keys[i]) {
      ImVec2 dest;
      dest.x = button_pos.x + button_size.x;
      dest.y = button_pos.y + button_size.y;
      ImGui::GetWindowDrawList()->AddRectFilled(button_pos, dest,
                                                IM_COL32(200, 200, 255, 200));
    }
    ImGui::PopID();
    ImGui::SameLine();
  }
  ImGui::PopStyleVar();
  ImGui::PopStyleVar();
}

void MusicEditor::DrawTrackerView() {
  DrawToolset();
  DrawPianoRoll();
  DrawPianoStaff();
  // TODO: Add music channel view
  ImGui::Text("Music channels coming soon...");
}

void MusicEditor::DrawInstrumentEditor() {
  ImGui::Text("Instrument Editor");
  ImGui::Separator();
  // TODO: Implement instrument editor UI
  ImGui::Text("Coming soon...");
}

void MusicEditor::DrawToolset() {
  static bool is_playing = false;
  static int selected_option = 0;
  static int current_volume = 0;
  static bool has_loaded_song = false;
  const int MAX_VOLUME = 100;

  if (is_playing && !has_loaded_song) {
    has_loaded_song = true;
  }

  gui::ItemLabel("Select a song to edit: ", gui::ItemLabelFlags::Left);
  ImGui::Combo("#songs_in_game", &selected_option, kGameSongs, 30);

  gui::ItemLabel("Controls: ", gui::ItemLabelFlags::Left);
  if (ImGui::BeginTable("SongToolset", 6, toolset_table_flags_, ImVec2(0, 0))) {
    ImGui::TableSetupColumn("#play");
    ImGui::TableSetupColumn("#rewind");
    ImGui::TableSetupColumn("#fastforward");
    ImGui::TableSetupColumn("#volume");
    ImGui::TableSetupColumn("#debug");

    ImGui::TableSetupColumn("#slider");

    ImGui::TableNextColumn();
    if (ImGui::Button(is_playing ? ICON_MD_STOP : ICON_MD_PLAY_ARROW)) {
      if (is_playing) {
        has_loaded_song = false;
      }
      is_playing = !is_playing;
    }

    ImGui::TableNextColumn();
    if (ImGui::Button(ICON_MD_FAST_REWIND)) {
      // Handle rewind button click
    }

    ImGui::TableNextColumn();
    if (ImGui::Button(ICON_MD_FAST_FORWARD)) {
      // Handle fast forward button click
    }

    ImGui::TableNextColumn();
    if (ImGui::Button(ICON_MD_VOLUME_UP)) {
      // Handle volume up button click
    }

    if (ImGui::Button(ICON_MD_ACCESS_TIME)) {
      music_tracker_.LoadSongs(*rom());
    }
    ImGui::TableNextColumn();
    ImGui::SliderInt("Volume", &current_volume, 0, 100);
    ImGui::EndTable();
  }

  const int SONG_DURATION = 120;  // duration of the song in seconds
  static int current_time = 0;    // current time in the song in seconds

  // Display the current time in the song
  gui::ItemLabel("Current Time: ", gui::ItemLabelFlags::Left);
  ImGui::Text("%d:%02d", current_time / 60, current_time % 60);
  ImGui::SameLine();
  // Display the song duration/progress using a progress bar
  ImGui::ProgressBar((float)current_time / SONG_DURATION);
}

// ============================================================================
// Audio Control Methods (Emulator Integration)
// ============================================================================

void MusicEditor::PlaySong(int song_id) {
  if (!emulator_) {
    LOG_WARN("MusicEditor", "No emulator instance - cannot play song");
    return;
  }

  if (!emulator_->snes().running()) {
    LOG_WARN("MusicEditor", "Emulator not running - cannot play song");
    return;
  }

  // Write song request to game memory ($7E012C)
  // This triggers the NMI handler to send the song to APU
  try {
    emulator_->snes().Write(0x7E012C, static_cast<uint8_t>(song_id));
    LOG_INFO("MusicEditor", "Requested song %d (%s)", song_id,
             song_id < 30 ? kGameSongs[song_id] : "Unknown");

    // Ensure audio backend is playing
    if (auto* audio = emulator_->audio_backend()) {
      auto status = audio->GetStatus();
      if (!status.is_playing) {
        audio->Play();
        LOG_INFO("MusicEditor", "Started audio backend playback");
      }
    }

    is_playing_ = true;
  } catch (const std::exception& e) {
    LOG_ERROR("MusicEditor", "Failed to play song: %s", e.what());
  }
}

void MusicEditor::StopSong() {
  if (!emulator_)
    return;

  // Write stop command to game memory
  try {
    emulator_->snes().Write(0x7E012C, 0xFF);  // 0xFF = stop music
    LOG_INFO("MusicEditor", "Stopped music playback");

    // Optional: pause audio backend to save CPU
    if (auto* audio = emulator_->audio_backend()) {
      audio->Pause();
    }

    is_playing_ = false;
  } catch (const std::exception& e) {
    LOG_ERROR("MusicEditor", "Failed to stop song: %s", e.what());
  }
}

void MusicEditor::SetVolume(float volume) {
  if (!emulator_)
    return;

  // Clamp volume to valid range
  volume = std::clamp(volume, 0.0f, 1.0f);

  if (auto* audio = emulator_->audio_backend()) {
    audio->SetVolume(volume);
    LOG_DEBUG("MusicEditor", "Set volume to %.2f", volume);
  } else {
    LOG_WARN("MusicEditor", "No audio backend available");
  }
}

}  // namespace editor
}  // namespace yaze
