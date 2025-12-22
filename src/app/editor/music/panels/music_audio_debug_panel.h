#ifndef YAZE_APP_EDITOR_MUSIC_PANELS_MUSIC_AUDIO_DEBUG_PANEL_H_
#define YAZE_APP_EDITOR_MUSIC_PANELS_MUSIC_AUDIO_DEBUG_PANEL_H_

#include <string>

#include "app/editor/music/music_player.h"
#include "app/editor/system/editor_panel.h"
#include "app/emu/emulator.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

/**
 * @class MusicAudioDebugPanel
 * @brief EditorPanel providing audio diagnostics for debugging the music editor
 *
 * This panel displays detailed information about the audio pipeline including:
 * - Backend configuration (sample rate, channels, buffer size)
 * - Resampling state (32040Hz -> 48000Hz conversion)
 * - Queue status (frames queued, underrun detection)
 * - DSP and APU diagnostic information
 */
class MusicAudioDebugPanel : public EditorPanel {
 public:
  explicit MusicAudioDebugPanel(editor::music::MusicPlayer* player)
      : player_(player) {}

  // ==========================================================================
  // EditorPanel Identity
  // ==========================================================================

  std::string GetId() const override { return "music.audio_debug"; }
  std::string GetDisplayName() const override { return "Audio Debug"; }
  std::string GetIcon() const override { return ICON_MD_BUG_REPORT; }
  std::string GetEditorCategory() const override { return "Music"; }
  int GetPriority() const override { return 95; }  // Just before Help

  // ==========================================================================
  // EditorPanel Drawing
  // ==========================================================================

  void Draw(bool* p_open) override {
    if (!player_) {
      ImGui::TextDisabled("Music player not available");
      return;
    }

    emu::Emulator* debug_emu = player_->emulator();
    if (!debug_emu || !debug_emu->is_snes_initialized()) {
      ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.4f, 1.0f),
                         ICON_MD_INFO " Play a song to initialize audio");
      ImGui::Separator();
      ImGui::TextDisabled("Audio emulator not initialized");
      return;
    }

    auto* audio_backend = debug_emu->audio_backend();
    if (!audio_backend) {
      ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                         ICON_MD_ERROR " No audio backend!");
      return;
    }

    DrawBackendInfo(audio_backend);
    ImGui::Separator();
    DrawQueueStatus(audio_backend);
    ImGui::Separator();
    DrawResamplingStatus(audio_backend);
    ImGui::Separator();
    DrawDspStatus();
    ImGui::Separator();
    DrawApuStatus();
    ImGui::Separator();
    DrawDebugActions();
  }

 private:
  void DrawBackendInfo(emu::audio::IAudioBackend* backend) {
    auto config = backend->GetConfig();

    ImGui::Text(ICON_MD_SPEAKER " Backend Configuration");
    ImGui::Indent();
    ImGui::Text("Backend: %s", backend->GetBackendName().c_str());
    ImGui::Text("Device Rate: %d Hz", config.sample_rate);
    ImGui::Text("Native Rate: 32040 Hz (SPC700)");
    ImGui::Text("Channels: %d", config.channels);
    ImGui::Text("Buffer Frames: %d", config.buffer_frames);
    ImGui::Unindent();
  }

  void DrawQueueStatus(emu::audio::IAudioBackend* backend) {
    auto status = backend->GetStatus();

    ImGui::Text(ICON_MD_QUEUE_MUSIC " Queue Status");
    ImGui::Indent();

    if (status.is_playing) {
      ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "Status: Playing");
    } else {
      ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.3f, 1.0f), "Status: Stopped");
    }

    ImGui::Text("Queued Frames: %u", status.queued_frames);
    ImGui::Text("Queued Bytes: %u", status.queued_bytes);

    if (status.has_underrun) {
      ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                         ICON_MD_WARNING " Underrun detected!");
    }

    ImGui::Unindent();
  }

  void DrawResamplingStatus(emu::audio::IAudioBackend* backend) {
    auto config = backend->GetConfig();
    bool resampling_enabled = backend->IsAudioStreamEnabled();

    ImGui::Text(ICON_MD_TRANSFORM " Resampling");
    ImGui::Indent();

    if (resampling_enabled) {
      float ratio = static_cast<float>(config.sample_rate) / 32040.0f;
      ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f),
                         "Status: ENABLED (32040 -> %d Hz)", config.sample_rate);
      ImGui::Text("Ratio: %.3f", ratio);

      // Check for correct ratio (should be ~1.498 for 32040->48000)
      if (ratio < 1.4f || ratio > 1.6f) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                           ICON_MD_WARNING " Unexpected ratio!");
      }
    } else {
      ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                         "Status: DISABLED");
      ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                         ICON_MD_WARNING " Audio will play at 1.5x speed!");
    }

    // Playback speed info
    auto player_state = player_->GetState();
    ImGui::Text("Playback Speed: %.2fx", player_state.playback_speed);
    ImGui::Text("Effective Rate: %.0f Hz", 32040.0f * player_state.playback_speed);

    ImGui::Unindent();
  }

  void DrawDspStatus() {
    auto dsp_status = player_->GetDspStatus();

    ImGui::Text(ICON_MD_MEMORY " DSP Status");
    ImGui::Indent();

    ImGui::Text("Sample Offset: %u", dsp_status.sample_offset);
    ImGui::Text("Frame Boundary: %u", dsp_status.frame_boundary);
    ImGui::Text("Master Vol L/R: %d / %d", dsp_status.master_vol_l,
                dsp_status.master_vol_r);

    if (dsp_status.mute) {
      ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Muted");
    }
    if (dsp_status.reset) {
      ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Reset");
    }

    ImGui::Text("Echo: %s (delay: %u)", dsp_status.echo_enabled ? "ON" : "OFF",
                dsp_status.echo_delay);

    ImGui::Unindent();
  }

  void DrawApuStatus() {
    auto apu_status = player_->GetApuStatus();

    ImGui::Text(ICON_MD_TIMER " APU Status");
    ImGui::Indent();

    ImGui::Text("Cycles: %llu", apu_status.cycles);

    // Timers in columns
    if (ImGui::BeginTable("ApuTimers", 4, ImGuiTableFlags_Borders)) {
      ImGui::TableSetupColumn("Timer");
      ImGui::TableSetupColumn("Enabled");
      ImGui::TableSetupColumn("Counter");
      ImGui::TableSetupColumn("Target");
      ImGui::TableHeadersRow();

      // Timer 0
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::Text("T0");
      ImGui::TableSetColumnIndex(1);
      ImGui::Text("%s", apu_status.timer0_enabled ? "ON" : "OFF");
      ImGui::TableSetColumnIndex(2);
      ImGui::Text("%u", apu_status.timer0_counter);
      ImGui::TableSetColumnIndex(3);
      ImGui::Text("%u", apu_status.timer0_target);

      // Timer 1
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::Text("T1");
      ImGui::TableSetColumnIndex(1);
      ImGui::Text("%s", apu_status.timer1_enabled ? "ON" : "OFF");
      ImGui::TableSetColumnIndex(2);
      ImGui::Text("%u", apu_status.timer1_counter);
      ImGui::TableSetColumnIndex(3);
      ImGui::Text("%u", apu_status.timer1_target);

      // Timer 2
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::Text("T2");
      ImGui::TableSetColumnIndex(1);
      ImGui::Text("%s", apu_status.timer2_enabled ? "ON" : "OFF");
      ImGui::TableSetColumnIndex(2);
      ImGui::Text("%u", apu_status.timer2_counter);
      ImGui::TableSetColumnIndex(3);
      ImGui::Text("%u", apu_status.timer2_target);

      ImGui::EndTable();
    }

    // Ports
    ImGui::Text("Ports In:  %02X %02X", apu_status.port0_in, apu_status.port1_in);
    ImGui::Text("Ports Out: %02X %02X", apu_status.port0_out, apu_status.port1_out);

    ImGui::Unindent();
  }

  void DrawDebugActions() {
    ImGui::Text(ICON_MD_BUILD " Debug Actions");
    ImGui::Indent();

    if (ImGui::Button("Clear Audio Queue")) {
      player_->ClearAudioQueue();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset DSP Buffer")) {
      player_->ResetDspBuffer();
    }
    ImGui::SameLine();
    if (ImGui::Button("Force NewFrame")) {
      player_->ForceNewFrame();
    }

    if (ImGui::Button("Reinit Audio")) {
      player_->ReinitAudio();
    }

    ImGui::Unindent();
  }

  editor::music::MusicPlayer* player_ = nullptr;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_MUSIC_PANELS_MUSIC_AUDIO_DEBUG_PANEL_H_

