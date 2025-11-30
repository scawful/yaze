#include "app/editor/music/instrument_editor_view.h"

#include <algorithm>
#include <cmath>

#include "absl/strings/str_format.h"
#include "app/gui/plots/implot_support.h"
#include "imgui/imgui.h"
#include "implot.h"

namespace yaze {
namespace editor {
namespace music {

using namespace yaze::zelda3::music;

void InstrumentEditorView::Draw(MusicBank& bank) {
  // Layout: List on left (25%), Properties on right (75%)
  float list_width = ImGui::GetContentRegionAvail().x * 0.25f;
  if (list_width < 150.0f) list_width = 150.0f;

  ImGui::BeginChild("InstrumentList", ImVec2(list_width, 0), true);
  DrawInstrumentList(bank);
  ImGui::EndChild();

  ImGui::SameLine();

  ImGui::BeginChild("InstrumentProps", ImVec2(0, 0), true);
  if (selected_instrument_index_ >= 0 &&
      selected_instrument_index_ < static_cast<int>(bank.GetInstrumentCount())) {
    DrawProperties(*bank.GetInstrument(selected_instrument_index_), bank);
  } else {
    ImGui::TextDisabled("Select an instrument to edit");
  }
  ImGui::EndChild();
}

void InstrumentEditorView::DrawInstrumentList(MusicBank& bank) {
  if (ImGui::Button("Add Instrument")) {
    bank.CreateNewInstrument("New Instrument");
    if (on_edit_) on_edit_();
  }
  
  ImGui::Separator();

  for (size_t i = 0; i < bank.GetInstrumentCount(); ++i) {
    const auto* inst = bank.GetInstrument(i);
    std::string label = absl::StrFormat("%02X: %s", i, inst->name);
    if (ImGui::Selectable(label.c_str(), selected_instrument_index_ == static_cast<int>(i))) {
      selected_instrument_index_ = static_cast<int>(i);
    }
  }
}

void InstrumentEditorView::DrawProperties(MusicInstrument& instrument, MusicBank& bank) {
  bool changed = false;

  // Name
  char name_buf[64];
  strncpy(name_buf, instrument.name.c_str(), sizeof(name_buf));
  if (ImGui::InputText("Name", name_buf, sizeof(name_buf))) {
    instrument.name = name_buf;
    changed = true;
  }

  ImGui::SameLine();
  if (ImGui::Button(" Preview")) {
    if (on_preview_) on_preview_(selected_instrument_index_);
  }

  // Sample Selection
  if (ImGui::BeginCombo("Sample", absl::StrFormat("%02X", instrument.sample_index).c_str())) {
    for (size_t i = 0; i < bank.GetSampleCount(); ++i) {
      bool is_selected = (instrument.sample_index == i);
      if (ImGui::Selectable(absl::StrFormat("%02X: Sample %d", i, i).c_str(), is_selected)) {
        instrument.sample_index = static_cast<uint8_t>(i);
        changed = true;
      }
      if (is_selected) ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }

  ImGui::Separator();
  ImGui::Text("Envelope (ADSR)");

  // ADSR Controls
  // Attack: 0-15
  int attack = instrument.attack;
  if (ImGui::SliderInt("Attack Rate", &attack, 0, 15)) {
    instrument.attack = static_cast<uint8_t>(attack);
    changed = true;
  }

  // Decay: 0-7
  int decay = instrument.decay;
  if (ImGui::SliderInt("Decay Rate", &decay, 0, 7)) {
    instrument.decay = static_cast<uint8_t>(decay);
    changed = true;
  }

  // Sustain Level: 0-7
  int sustain_level = instrument.sustain_level;
  if (ImGui::SliderInt("Sustain Level", &sustain_level, 0, 7)) {
    instrument.sustain_level = static_cast<uint8_t>(sustain_level);
    changed = true;
  }

  // Sustain Rate: 0-31
  int sustain_rate = instrument.sustain_rate;
  if (ImGui::SliderInt("Sustain Rate", &sustain_rate, 0, 31)) {
    instrument.sustain_rate = static_cast<uint8_t>(sustain_rate);
    changed = true;
  }

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
  float vol = 0.0f;
  
  // Attack Phase
  // Rate N: (Rate * 2 + 1) ms to full volume? Roughly.
  // Simplification: t += 1.0 / (attack + 1)
  float attack_time = 1.0f / (instrument.attack + 1.0f); 
  
  plot_x_.push_back(0.0f);
  plot_y_.push_back(0.0f);
  
  // Attack is linear in SNES DSP (add 1/64 per tick)
  plot_x_.push_back(attack_time);
  plot_y_.push_back(1.0f); // Max volume
  t = attack_time;
  
  // Decay Phase
  // Exponential decay to Sustain Level
  // Sustain Level is instrument.sustain_level/8.0f + 1
  float s_level = (instrument.sustain_level + 1) / 8.0f;
  
  // Simulate exponential decay
  // k = decay rate factor
  float decay_rate = (instrument.decay * 2.0f + 1.0f); 
  float current_vol = 1.0f;
  for (int i = 0; i < 20; ++i) {
    t += 0.05f;
    // Fake exponential: vol = s_level + (1 - s_level) * exp(-k * dt)
    // Or simple lerp for now
    float alpha = (float)i / 20.0f;
    // plot_x_.push_back(t);
    // plot_y_.push_back(1.0f - (1.0f - s_level) * alpha); // Linear fall for visualization
  }
  plot_x_.push_back(t + 0.5f); // Approx decay time
  plot_y_.push_back(s_level);
  t += 0.5f;
  
  // Sustain Phase (Decrease)
  // Decreases at sustain_rate until key off
  float sustain_time = 2.0f; // Show 2 seconds of sustain
  float sustain_slope = instrument.sustain_rate / 31.0f;
  
  plot_x_.push_back(t + sustain_time);
  plot_y_.push_back(std::max(0.0f, s_level - sustain_slope));
  
  if (ImPlot::BeginPlot("ADSR Envelope", ImVec2(-1, 200))) {
    ImPlot::SetupAxes("Time", "Volume");
    ImPlot::SetupAxesLimits(0, 4.0, 0, 1.1);
    ImPlot::PlotLine("Volume", plot_x_.data(), plot_y_.data(), static_cast<int>(plot_x_.size()));
    ImPlot::EndPlot();
  }
}

}  // namespace music
}  // namespace editor
}  // namespace yaze
