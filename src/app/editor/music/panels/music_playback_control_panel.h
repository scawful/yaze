#ifndef YAZE_APP_EDITOR_MUSIC_PANELS_MUSIC_PLAYBACK_CONTROL_PANEL_H_
#define YAZE_APP_EDITOR_MUSIC_PANELS_MUSIC_PLAYBACK_CONTROL_PANEL_H_

#include <functional>
#include <string>

#include "app/editor/music/music_player.h"
#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/theme_manager.h"
#include "imgui/imgui.h"
#include "zelda3/music/music_bank.h"

namespace yaze {
namespace editor {

/**
 * @class MusicPlaybackControlPanel
 * @brief EditorPanel for music playback controls and status display
 */
class MusicPlaybackControlPanel : public EditorPanel {
 public:
  MusicPlaybackControlPanel(zelda3::music::MusicBank* music_bank,
                            int* current_song_index,
                            music::MusicPlayer* music_player)
      : music_bank_(music_bank),
        current_song_index_(current_song_index),
        music_player_(music_player) {}

  // ==========================================================================
  // EditorPanel Identity
  // ==========================================================================

  std::string GetId() const override { return "music.tracker"; }
  std::string GetDisplayName() const override { return "Playback Control"; }
  std::string GetIcon() const override { return ICON_MD_PLAY_CIRCLE; }
  std::string GetEditorCategory() const override { return "Music"; }
  int GetPriority() const override { return 10; }

  // ==========================================================================
  // Callback Setters
  // ==========================================================================

  void SetOnOpenSong(std::function<void(int)> callback) {
    on_open_song_ = callback;
  }

  void SetOnOpenPianoRoll(std::function<void(int)> callback) {
    on_open_piano_roll_ = callback;
  }

  // ==========================================================================
  // EditorPanel Drawing
  // ==========================================================================

  void Draw(bool* p_open) override {
    if (!music_bank_ || !current_song_index_) {
      ImGui::TextDisabled("Music system not initialized");
      return;
    }

    DrawToolset();
    ImGui::Separator();
    DrawSongInfo();
    DrawPlaybackStatus();
    DrawQuickActions();

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

 private:
  void DrawToolset() {
    auto state =
        music_player_ ? music_player_->GetState() : music::PlaybackState{};
    bool can_play = music_player_ && music_player_->IsAudioReady();
    auto* song = music_bank_->GetSong(*current_song_index_);

    if (!can_play) ImGui::BeginDisabled();

    // Transport controls
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
      if (ImGui::Button(ICON_MD_PLAY_ARROW "##Play"))
        music_player_->PlaySong(*current_song_index_);
      if (ImGui::IsItemHovered()) ImGui::SetTooltip("Play (Space)");
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_STOP "##Stop")) music_player_->Stop();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Stop (Escape)");

    if (!can_play) ImGui::EndDisabled();

    // Song label with playing indicator
    ImGui::SameLine();
    if (song) {
      if (state.is_playing && !state.is_paused) {
        float t = static_cast<float>(ImGui::GetTime() * 3.0);
        float alpha = 0.5f + 0.5f * std::sin(t);
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, alpha), ICON_MD_GRAPHIC_EQ);
        ImGui::SameLine();
      } else if (state.is_paused) {
        ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f),
                           ICON_MD_PAUSE_CIRCLE);
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

    // Time display
    if (state.is_playing || state.is_paused) {
      ImGui::SameLine();
      float seconds = state.ticks_per_second > 0
                          ? state.current_tick / state.ticks_per_second
                          : 0.0f;
      int mins = static_cast<int>(seconds) / 60;
      int secs = static_cast<int>(seconds) % 60;
      ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.8f, 1.0f), " %d:%02d", mins,
                         secs);
    }

    // Right-aligned controls
    float right_offset = ImGui::GetWindowWidth() - 200;
    if (right_offset > 200) {
      ImGui::SameLine(right_offset);

      ImGui::Text(ICON_MD_SPEED);
      ImGui::SameLine();
      ImGui::SetNextItemWidth(70);
      float speed = state.playback_speed;
      if (gui::SliderFloatWheel("##Speed", &speed, 0.25f, 2.0f, "%.2fx",
                                0.1f)) {
        if (music_player_) music_player_->SetPlaybackSpeed(speed);
      }
      if (ImGui::IsItemHovered()) ImGui::SetTooltip("Playback speed (+/- keys)");

      ImGui::SameLine();
      ImGui::Text(ICON_MD_VOLUME_UP);
      ImGui::SameLine();
      ImGui::SetNextItemWidth(60);
      if (gui::SliderIntWheel("##Vol", &current_volume_, 0, 100, "%d%%", 5)) {
        if (music_player_) music_player_->SetVolume(current_volume_ / 100.0f);
      }
      if (ImGui::IsItemHovered()) ImGui::SetTooltip("Volume");
    }
  }

  void DrawSongInfo() {
    auto* song = music_bank_->GetSong(*current_song_index_);

    if (song) {
      ImGui::Text("Selected Song:");
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "[%02X] %s",
                         *current_song_index_ + 1, song->name.c_str());

      ImGui::SameLine();
      ImGui::TextDisabled("| %zu segments", song->segments.size());
      if (song->modified) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f),
                           ICON_MD_EDIT " Modified");
      }
    }
  }

  void DrawPlaybackStatus() {
    auto state =
        music_player_ ? music_player_->GetState() : music::PlaybackState{};
    auto* song = music_bank_->GetSong(*current_song_index_);

    if (state.is_playing || state.is_paused) {
      ImGui::Separator();

      // Timeline progress
      if (song && !song->segments.empty()) {
        uint32_t total_duration = 0;
        for (const auto& seg : song->segments) {
          total_duration += seg.GetDuration();
        }

        float progress = (total_duration > 0)
                             ? static_cast<float>(state.current_tick) /
                                   total_duration
                             : 0.0f;
        progress = std::clamp(progress, 0.0f, 1.0f);

        float current_seconds =
            state.ticks_per_second > 0
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
        ImGui::ProgressBar(progress, ImVec2(-1, 0), "");
      }

      ImGui::Text("Segment: %d | Tick: %u", state.current_segment_index + 1,
                  state.current_tick);
      ImGui::SameLine();
      ImGui::TextDisabled("| %.1f ticks/sec | %.2fx speed",
                          state.ticks_per_second, state.playback_speed);
    }
  }

  void DrawQuickActions() {
    ImGui::Separator();

    if (ImGui::Button(ICON_MD_OPEN_IN_NEW " Open Tracker")) {
      if (on_open_song_) on_open_song_(*current_song_index_);
    }
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("Open song in dedicated tracker window");

    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_PIANO " Open Piano Roll")) {
      if (on_open_piano_roll_) on_open_piano_roll_(*current_song_index_);
    }
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("Open piano roll view for this song");
  }

  zelda3::music::MusicBank* music_bank_ = nullptr;
  int* current_song_index_ = nullptr;
  music::MusicPlayer* music_player_ = nullptr;
  int current_volume_ = 100;

  std::function<void(int)> on_open_song_;
  std::function<void(int)> on_open_piano_roll_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_MUSIC_PANELS_MUSIC_PLAYBACK_CONTROL_PANEL_H_
