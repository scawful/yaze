#include "app/editor/music/instrument_editor_view.h"

#include <algorithm>
#include <cmath>

#include "absl/strings/str_format.h"
#include "app/gui/core/icons.h"
#include "app/gui/plots/implot_support.h"
#include "imgui/imgui.h"
#include "implot.h"

namespace yaze {
namespace editor {
namespace music {

using namespace yaze::zelda3::music;

static void HelpMarker(const char* desc) {
  ImGui::TextDisabled("(?)");
  if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
    ImGui::TextUnformatted(desc);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
  }
}

void InstrumentEditorView::Draw(MusicBank& bank) {
  // Layout: List on left (25%), Properties on right (75%)
  float list_width = ImGui::GetContentRegionAvail().x * 0.25f;
  if (list_width < 150.0f)
    list_width = 150.0f;

  ImGui::BeginChild("InstrumentList", ImVec2(list_width, 0), true);
  DrawInstrumentList(bank);
  ImGui::EndChild();

  ImGui::SameLine();

  ImGui::BeginChild("InstrumentProps", ImVec2(0, 0), true);
  if (selected_instrument_index_ >= 0 &&
      selected_instrument_index_ <
          static_cast<int>(bank.GetInstrumentCount())) {
    DrawProperties(*bank.GetInstrument(selected_instrument_index_), bank);
  } else {
    ImGui::TextDisabled("Select an instrument to edit");
  }
  ImGui::EndChild();
}

void InstrumentEditorView::DrawInstrumentList(MusicBank& bank) {
  if (ImGui::Button("Add Instrument")) {
    bank.CreateNewInstrument("New Instrument");
    if (on_edit_)
      on_edit_();
  }

  ImGui::Separator();

  for (size_t i = 0; i < bank.GetInstrumentCount(); ++i) {
    const auto* inst = bank.GetInstrument(i);
    std::string label = absl::StrFormat("%02X: %s", i, inst->name);
    if (ImGui::Selectable(label.c_str(),
                          selected_instrument_index_ == static_cast<int>(i))) {
      selected_instrument_index_ = static_cast<int>(i);
    }
  }
}

void InstrumentEditorView::DrawProperties(MusicInstrument& instrument,
                                          MusicBank& bank) {
  bool changed = false;

  // Name
  char name_buf[64];
  strncpy(name_buf, instrument.name.c_str(), sizeof(name_buf));
  if (ImGui::InputText("Name", name_buf, sizeof(name_buf))) {
    instrument.name = name_buf;
    changed = true;
  }

  ImGui::SameLine();
  if (on_preview_) {
    if (ImGui::Button(ICON_MD_PLAY_ARROW " Preview")) {
      on_preview_(selected_instrument_index_);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          "Play a C4 note with this instrument (requires ROM loaded)");
    }
  } else {
    ImGui::BeginDisabled();
    ImGui::Button(ICON_MD_PLAY_ARROW " Preview");
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      ImGui::SetTooltip("Preview not available - load a ROM first");
    }
  }

  // Sample Selection
  if (ImGui::BeginCombo(
          "Sample", absl::StrFormat("%02X", instrument.sample_index).c_str())) {
    for (size_t i = 0; i < bank.GetSampleCount(); ++i) {
      bool is_selected = (instrument.sample_index == i);
      const auto* sample = bank.GetSample(i);
      std::string label = absl::StrFormat(
          "%02X: %s", i, sample ? sample->name.c_str() : "Unknown");
      if (ImGui::Selectable(label.c_str(), is_selected)) {
        instrument.sample_index = static_cast<uint8_t>(i);
        changed = true;
      }
      if (is_selected)
        ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }
  ImGui::SameLine();
  HelpMarker("The BRR sample used by this instrument.");

  // Pitch Multiplier (Tuning)
  int pitch = instrument.pitch_mult;
  if (ImGui::InputInt("Pitch Multiplier", &pitch, 1, 16,
                      ImGuiInputTextFlags_CharsHexadecimal)) {
    instrument.pitch_mult = static_cast<uint16_t>(std::clamp(pitch, 0, 0xFFFF));
    changed = true;
  }
  ImGui::SameLine();
  HelpMarker(
      "Base pitch adjustment. $1000 = 1.0x (Standard C). Lower values lower "
      "the pitch.");

  ImGui::Separator();
  ImGui::Text("Envelope (ADSR)");
  ImGui::SameLine();
  HelpMarker("Attack, Decay, Sustain, Release envelope controls.");

  // ADSR Controls
  // Attack: 0-15
  int attack = instrument.attack;
  if (ImGui::SliderInt("Attack Rate", &attack, 0, 15)) {
    instrument.attack = static_cast<uint8_t>(attack);
    changed = true;
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip(
        "How fast the volume reaches peak. 15 = Fastest (Instant), 0 = "
        "Slowest.");

  // Decay: 0-7
  int decay = instrument.decay;
  if (ImGui::SliderInt("Decay Rate", &decay, 0, 7)) {
    instrument.decay = static_cast<uint8_t>(decay);
    changed = true;
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip(
        "How fast volume drops from peak to Sustain Level. 7 = Fastest, 0 = "
        "Slowest.");

  // Sustain Level: 0-7
  int sustain_level = instrument.sustain_level;
  if (ImGui::SliderInt("Sustain Level", &sustain_level, 0, 7)) {
    instrument.sustain_level = static_cast<uint8_t>(sustain_level);
    changed = true;
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip(
        "The volume level (1/8ths) to sustain at. 7 = Max Volume, 0 = "
        "Silence.");

  // Sustain Rate: 0-31
  int sustain_rate = instrument.sustain_rate;
  if (ImGui::SliderInt("Sustain Rate", &sustain_rate, 0, 31)) {
    instrument.sustain_rate = static_cast<uint8_t>(sustain_rate);
    changed = true;
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip(
        "How fast volume decays WHILE holding the key (after reaching Sustain "
        "Level). 0 = Infinite sustain, 31 = Fast fade out.");

  // Gain (if not using ADSR, but usually ADSR is preferred for instruments)
  // TODO: Add Gain Mode toggle

  if (changed && on_edit_) {
    on_edit_();
  }

  ImGui::Separator();
  DrawAdsrGraph(instrument);
}

void InstrumentEditorView::DrawAdsrGraph(const MusicInstrument& instrument) {
  // Visualize ADSR
  // Attack: Linear increase to max
  // Decay: Exponential decrease to Sustain Level
  // Sustain: Exponential decrease at Sustain Rate

  // Ensure ImPlot context exists before plotting
  yaze::gui::plotting::EnsureImPlotContext();

  // Helper to convert SNES rates to time/slope
  // (Simplified for visualization)

  plot_x_.clear();
  plot_y_.clear();

  float t = 0.0f;

  // Attack Phase
  // Rate N: (Rate * 2 + 1) ms to full volume? Roughly.
  // Simplification: t += 1.0 / (attack + 1)
  float attack_time = 1.0f / (instrument.attack + 1.0f);
  if (instrument.attack == 15)
    attack_time = 0.0f;  // Instant

  plot_x_.push_back(0.0f);
  plot_y_.push_back(0.0f);

  // Attack is linear in SNES DSP (add 1/64 per tick)
  plot_x_.push_back(attack_time);
  plot_y_.push_back(1.0f);  // Max volume
  t = attack_time;

  // Decay Phase
  // Exponential decay to Sustain Level
  // Sustain Level is instrument.sustain_level/8.0f + 1
  float s_level = (instrument.sustain_level + 1) / 8.0f;

  // Simulate exponential decay
  // k = decay rate factor
  for (int i = 0; i < 20; ++i) {
    t += 0.02f;
    // Fake exponential: vol = s_level + (1 - s_level) * exp(-k * dt)
    // Or simple lerp for now
    float alpha = (float)i / 20.0f;
    float curve = alpha * alpha;  // Quadratic approximation for exponential
    float vol = 1.0f - (1.0f - s_level) * curve;

    plot_x_.push_back(t);
    plot_y_.push_back(vol);
  }

  // Sustain Phase (Decrease)
  // Decreases at sustain_rate until key off
  float sustain_time = 1.0f;  // Show 1 second of sustain
  float sustain_drop_per_sec = instrument.sustain_rate / 31.0f;

  plot_x_.push_back(t + sustain_time);
  plot_y_.push_back(std::max(0.0f, s_level - sustain_drop_per_sec));

  if (ImPlot::BeginPlot("ADSR Envelope", ImVec2(-1, 200))) {
    ImPlot::SetupAxes("Time", "Volume");
    ImPlot::SetupAxesLimits(0, 2.0, 0, 1.1);
    ImPlot::PlotLine("Volume", plot_x_.data(), plot_y_.data(),
                     static_cast<int>(plot_x_.size()));

    // Mark phases
    ImPlot::TagX(attack_time, ImVec4(1, 1, 0, 0.5), "Decay Start");

    ImPlot::EndPlot();
  }
}

}  // namespace music
}  // namespace editor
}  // namespace yaze