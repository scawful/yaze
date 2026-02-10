#ifndef YAZE_APP_EDITOR_MUSIC_PANELS_MUSIC_PLAYBACK_CONTROL_PANEL_H_
#define YAZE_APP_EDITOR_MUSIC_PANELS_MUSIC_PLAYBACK_CONTROL_PANEL_H_

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>

#include "app/editor/music/music_player.h"
#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/theme_manager.h"
#include "app/gui/core/ui_helpers.h"
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

    // Debug controls (collapsed by default)
    DrawDebugControls();

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
      gui::StyleColorGuard pause_guard(ImGuiCol_Button,
                                       gui::GetSuccessButtonColors().button);
      if (ImGui::Button(ICON_MD_PAUSE "##Pause")) music_player_->Pause();
      if (ImGui::IsItemHovered()) ImGui::SetTooltip("Pause (Space)");
    } else if (state.is_paused) {
      gui::StyleColorGuard resume_guard(ImGuiCol_Button,
                                        gui::GetWarningColor());
      if (ImGui::Button(ICON_MD_PLAY_ARROW "##Resume")) music_player_->Resume();
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
        auto c = gui::GetSuccessColor();
        ImGui::TextColored(ImVec4(c.x, c.y, c.z, alpha), ICON_MD_GRAPHIC_EQ);
        ImGui::SameLine();
      } else if (state.is_paused) {
        ImGui::TextColored(gui::GetWarningColor(),
                           ICON_MD_PAUSE_CIRCLE);
        ImGui::SameLine();
      }
      ImGui::Text("%s", song->name.c_str());
      if (song->modified) {
        ImGui::SameLine();
        ImGui::TextColored(gui::GetWarningColor(), ICON_MD_EDIT);
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
      ImGui::TextColored(gui::GetInfoColor(), " %d:%02d", mins,
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
      ImGui::TextColored(gui::GetInfoColor(), "[%02X] %s",
                         *current_song_index_ + 1, song->name.c_str());

      ImGui::SameLine();
      ImGui::TextDisabled("| %zu segments", song->segments.size());
      if (song->modified) {
        ImGui::SameLine();
        ImGui::TextColored(gui::GetWarningColor(),
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

  void DrawDebugControls() {
    if (!music_player_) return;

    if (!ImGui::CollapsingHeader(ICON_MD_BUG_REPORT " Debug Controls")) return;

    ImGui::Indent();

    // Pause updates checkbox
    ImGui::Checkbox("Pause Updates", &debug_paused_);
    ImGui::SameLine();
    if (ImGui::Button("Snapshot")) {
      // Force capture current values
      debug_paused_ = true;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(Freeze display to read values)");

    // Capture current state (unless paused)
    if (!debug_paused_) {
      cached_dsp_ = music_player_->GetDspStatus();
      cached_audio_ = music_player_->GetAudioQueueStatus();
      cached_apu_ = music_player_->GetApuStatus();
      cached_channels_ = music_player_->GetChannelStates();

      // Track statistics using wall-clock time for accuracy
      if (cached_audio_.is_playing) {
        auto now = std::chrono::steady_clock::now();

        // Initialize on first call
        if (last_stats_time_.time_since_epoch().count() == 0) {
          last_stats_time_ = now;
          last_cycles_for_rate_ = cached_apu_.cycles;
          last_queued_for_rate_ = cached_audio_.queued_frames;
        }

        auto elapsed = std::chrono::duration<double>(now - last_stats_time_).count();

        // Update stats every 0.5 seconds
        if (elapsed >= 0.5) {
          uint64_t cycle_delta = cached_apu_.cycles - last_cycles_for_rate_;
          int32_t queue_delta = static_cast<int32_t>(cached_audio_.queued_frames) -
                                static_cast<int32_t>(last_queued_for_rate_);

          // Calculate actual rates based on elapsed wall-clock time
          avg_cycle_rate_ = static_cast<uint64_t>(cycle_delta / elapsed);
          avg_queue_delta_ = static_cast<int32_t>(queue_delta / elapsed);

          // Reset for next measurement
          last_stats_time_ = now;
          last_cycles_for_rate_ = cached_apu_.cycles;
          last_queued_for_rate_ = cached_audio_.queued_frames;
        }
      } else {
        // Reset when stopped
        last_stats_time_ = std::chrono::steady_clock::time_point();
      }
    }

    // === Quick Summary (always visible) ===
    ImGui::Separator();
    ImVec4 status_color = cached_audio_.is_playing
                              ? gui::GetSuccessColor()
                              : gui::GetDisabledColor();
    ImGui::TextColored(status_color, cached_audio_.is_playing ? "PLAYING" : "STOPPED");
    ImGui::SameLine();
    ImGui::Text("| Queue: %u frames", cached_audio_.queued_frames);
    ImGui::SameLine();
    ImGui::Text("| DSP: %u/2048", cached_dsp_.sample_offset);

    // Queue trend indicator
    ImGui::SameLine();
    if (avg_queue_delta_ > 50) {
      ImGui::TextColored(gui::GetErrorColor(),
                         ICON_MD_TRENDING_UP " GROWING (too fast!)");
    } else if (avg_queue_delta_ < -50) {
      ImGui::TextColored(gui::GetWarningColor(),
                         ICON_MD_TRENDING_DOWN " DRAINING");
    } else {
      ImGui::TextColored(gui::GetSuccessColor(),
                         ICON_MD_TRENDING_FLAT " STABLE");
    }

    // Cycle rate check (should be ~1,024,000/sec)
    if (avg_cycle_rate_ > 0) {
      float rate_ratio = avg_cycle_rate_ / 1024000.0f;
      ImGui::Text("APU Rate: %.2fx expected", rate_ratio);
      if (rate_ratio > 1.1f) {
        ImGui::SameLine();
        ImGui::TextColored(gui::GetErrorColor(),
                           "(APU running too fast!)");
      }
    }

    ImGui::Separator();

    // === DSP Buffer Status ===
    if (ImGui::TreeNode("DSP Buffer")) {
      auto& dsp = cached_dsp_;

      ImGui::Text("Sample Offset: %u / 2048", dsp.sample_offset);
      ImGui::Text("Frame Boundary: %u", dsp.frame_boundary);

      // Buffer fill progress bar
      float fill = dsp.sample_offset / 2048.0f;
      char overlay[32];
      snprintf(overlay, sizeof(overlay), "%.1f%%", fill * 100.0f);
      ImGui::ProgressBar(fill, ImVec2(-1, 0), overlay);

      // Drift indicator
      int32_t drift = static_cast<int32_t>(dsp.sample_offset) -
                      static_cast<int32_t>(dsp.frame_boundary);
      ImVec4 drift_color = (std::abs(drift) > 100)
                               ? gui::GetErrorColor()
                               : gui::GetSuccessColor();
      ImGui::TextColored(drift_color, "Drift: %+d samples", drift);

      ImGui::Text("Master Vol: L=%d R=%d", dsp.master_vol_l, dsp.master_vol_r);

      // Status flags
      if (dsp.mute) {
        ImGui::TextColored(gui::GetWarningColor(), ICON_MD_VOLUME_OFF " MUTED");
        ImGui::SameLine();
      }
      if (dsp.reset) {
        ImGui::TextColored(gui::GetErrorColor(), ICON_MD_RESTART_ALT " RESET");
        ImGui::SameLine();
      }
      if (dsp.echo_enabled) {
        ImGui::TextColored(gui::GetInfoColor(),
                           ICON_MD_SURROUND_SOUND " Echo (delay=%u)", dsp.echo_delay);
      }

      ImGui::TreePop();
    }

    // === Audio Queue Status ===
    if (ImGui::TreeNode("Audio Queue")) {
      auto& audio = cached_audio_;

      // Status indicator
      if (audio.is_playing) {
        ImGui::TextColored(gui::GetSuccessColor(),
                           ICON_MD_PLAY_CIRCLE " Playing");
      } else {
        ImGui::TextColored(gui::GetDisabledColor(),
                           ICON_MD_STOP_CIRCLE " Stopped");
      }

      ImGui::Text("Queued: %u frames (%u bytes)",
                  audio.queued_frames, audio.queued_bytes);
      ImGui::Text("Sample Rate: %d Hz", audio.sample_rate);
      ImGui::Text("Backend: %s", audio.backend_name.c_str());

      // Underrun warning
      if (audio.has_underrun) {
        ImGui::TextColored(gui::GetErrorColor(),
                           ICON_MD_WARNING " UNDERRUN DETECTED");
      }

      // Queue level indicator
      float queue_level = audio.queued_frames / 6000.0f;  // ~100ms worth
      queue_level = std::clamp(queue_level, 0.0f, 1.0f);
      ImVec4 queue_color = (queue_level < 0.2f)
                               ? gui::GetErrorColor()
                               : gui::GetSuccessColor();
      {
        gui::StyleColorGuard queue_guard(ImGuiCol_PlotHistogram, queue_color);
        ImGui::ProgressBar(queue_level, ImVec2(-1, 0), "Queue Level");
      }

      ImGui::TreePop();
    }

    // === APU Timing ===
    if (ImGui::TreeNode("APU Timing")) {
      auto& apu = cached_apu_;

      ImGui::Text("Cycles: %llu", static_cast<unsigned long long>(apu.cycles));

      // Timers in a table
      if (ImGui::BeginTable("Timers", 4, ImGuiTableFlags_Borders)) {
        ImGui::TableSetupColumn("Timer");
        ImGui::TableSetupColumn("Enabled");
        ImGui::TableSetupColumn("Counter");
        ImGui::TableSetupColumn("Target");
        ImGui::TableHeadersRow();

        // Timer 0
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("T0");
        ImGui::TableNextColumn();
        ImGui::TextColored(apu.timer0_enabled ? gui::GetSuccessColor()
                                               : gui::GetDisabledColor(),
                           apu.timer0_enabled ? "ON" : "off");
        ImGui::TableNextColumn(); ImGui::Text("%u", apu.timer0_counter);
        ImGui::TableNextColumn(); ImGui::Text("%u", apu.timer0_target);

        // Timer 1
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("T1");
        ImGui::TableNextColumn();
        ImGui::TextColored(apu.timer1_enabled ? gui::GetSuccessColor()
                                               : gui::GetDisabledColor(),
                           apu.timer1_enabled ? "ON" : "off");
        ImGui::TableNextColumn(); ImGui::Text("%u", apu.timer1_counter);
        ImGui::TableNextColumn(); ImGui::Text("%u", apu.timer1_target);

        // Timer 2
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("T2");
        ImGui::TableNextColumn();
        ImGui::TextColored(apu.timer2_enabled ? gui::GetSuccessColor()
                                               : gui::GetDisabledColor(),
                           apu.timer2_enabled ? "ON" : "off");
        ImGui::TableNextColumn(); ImGui::Text("%u", apu.timer2_counter);
        ImGui::TableNextColumn(); ImGui::Text("%u", apu.timer2_target);

        ImGui::EndTable();
      }

      // Port state
      ImGui::Text("Ports IN:  [0]=%02X [1]=%02X", apu.port0_in, apu.port1_in);
      ImGui::Text("Ports OUT: [0]=%02X [1]=%02X", apu.port0_out, apu.port1_out);

      ImGui::TreePop();
    }

    // === Channel Overview ===
    if (ImGui::TreeNode("Channels")) {
      auto& channels = cached_channels_;

      ImGui::Text("Key Status:");
      ImGui::SameLine();
      for (int i = 0; i < 8; i++) {
        ImVec4 color = channels[i].key_on
                           ? gui::GetSuccessColor()
                           : gui::GetDisabledColor();
        ImGui::TextColored(color, "%d", i);
        if (i < 7) ImGui::SameLine();
      }

      // Detailed channel info
      if (ImGui::BeginTable("ChannelDetails", 6,
                            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Ch", ImGuiTableColumnFlags_WidthFixed, 25);
        ImGui::TableSetupColumn("Key");
        ImGui::TableSetupColumn("Sample");
        ImGui::TableSetupColumn("Pitch");
        ImGui::TableSetupColumn("Vol L/R");
        ImGui::TableSetupColumn("ADSR");
        ImGui::TableHeadersRow();

        const char* adsr_names[] = {"Atk", "Dec", "Sus", "Rel"};
        for (int i = 0; i < 8; i++) {
          ImGui::TableNextRow();
          ImGui::TableNextColumn(); ImGui::Text("%d", i);
          ImGui::TableNextColumn();
          ImGui::TextColored(channels[i].key_on ? gui::GetSuccessColor()
                                                 : gui::GetDisabledColor(),
                             channels[i].key_on ? "ON" : "--");
          ImGui::TableNextColumn(); ImGui::Text("%02X", channels[i].sample_index);
          ImGui::TableNextColumn(); ImGui::Text("%04X", channels[i].pitch);
          ImGui::TableNextColumn();
          ImGui::Text("%02X/%02X", channels[i].volume_l, channels[i].volume_r);
          ImGui::TableNextColumn();
          int state = channels[i].adsr_state & 0x03;
          ImGui::Text("%s", adsr_names[state]);
        }

        ImGui::EndTable();
      }

      ImGui::TreePop();
    }

    // === Action Buttons ===
    ImGui::Separator();
    ImGui::Text("Actions:");

    if (ImGui::Button(ICON_MD_CLEAR_ALL " Clear Queue")) {
      music_player_->ClearAudioQueue();
    }
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("Clear SDL audio queue immediately");

    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_REFRESH " Reset DSP")) {
      music_player_->ResetDspBuffer();
    }
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("Reset DSP sample ring buffer");

    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_SKIP_NEXT " NewFrame")) {
      music_player_->ForceNewFrame();
    }
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("Force DSP NewFrame() call");

    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_REPLAY " Reinit Audio")) {
      music_player_->ReinitAudio();
    }
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("Full audio system reinitialization");

    ImGui::Unindent();
  }

  zelda3::music::MusicBank* music_bank_ = nullptr;
  int* current_song_index_ = nullptr;
  music::MusicPlayer* music_player_ = nullptr;
  int current_volume_ = 100;

  std::function<void(int)> on_open_song_;
  std::function<void(int)> on_open_piano_roll_;

  // Debug state
  bool debug_paused_ = false;
  music::DspDebugStatus cached_dsp_;
  music::AudioQueueStatus cached_audio_;
  music::ApuDebugStatus cached_apu_;
  std::array<music::ChannelState, 8> cached_channels_;
  int32_t avg_queue_delta_ = 0;
  uint64_t avg_cycle_rate_ = 0;

  // Wall-clock timing for rate measurement
  std::chrono::steady_clock::time_point last_stats_time_;
  uint64_t last_cycles_for_rate_ = 0;
  uint32_t last_queued_for_rate_ = 0;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_MUSIC_PANELS_MUSIC_PLAYBACK_CONTROL_PANEL_H_
