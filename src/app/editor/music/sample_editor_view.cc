#include "app/editor/music/sample_editor_view.h"

#include <algorithm>
#include <cmath>

#include "absl/strings/str_format.h"
#include "app/gui/plots/implot_support.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"
#include "implot.h"

namespace yaze {
namespace editor {
namespace music {

using namespace yaze::zelda3::music;

void SampleEditorView::Draw(MusicBank& bank) {
  // Layout: List on left (25%), Waveform on right (75%)
  float list_width = ImGui::GetContentRegionAvail().x * 0.25f;
  if (list_width < 150.0f) list_width = 150.0f;

  ImGui::BeginChild("SampleList", ImVec2(list_width, 0), true);
  DrawSampleList(bank);
  ImGui::EndChild();

  ImGui::SameLine();

  ImGui::BeginChild("SampleWaveform", ImVec2(0, 0), true);
  if (selected_sample_index_ >= 0 &&
      selected_sample_index_ < static_cast<int>(bank.GetSampleCount())) {
    DrawWaveform(*bank.GetSample(selected_sample_index_));
  } else {
    ImGui::TextDisabled("Select a sample to view");
  }
  ImGui::EndChild();
}

void SampleEditorView::DrawSampleList(MusicBank& bank) {
  if (ImGui::Button("Import Sample")) {
    // TODO: Implement file dialog for BRR import
  }
  
  ImGui::Separator();

  for (size_t i = 0; i < bank.GetSampleCount(); ++i) {
    const auto* sample = bank.GetSample(i);
    std::string label = absl::StrFormat("%02X: Sample %d (%d bytes)", i, i, sample->brr_data.size());
    if (ImGui::Selectable(label.c_str(), selected_sample_index_ == static_cast<int>(i))) {
      selected_sample_index_ = static_cast<int>(i);
    }
  }
}

void SampleEditorView::DrawWaveform(const MusicSample& sample) {
  if (ImGui::Button(ICON_MD_PLAY_ARROW " Play Sample")) {
    if (on_preview_) on_preview_(selected_sample_index_);
  }
  ImGui::SameLine();
  ImGui::Text("%s (%zu bytes)", sample.name.c_str(), sample.brr_data.size());

  // Ensure ImPlot context exists before plotting
  yaze::gui::plotting::EnsureImPlotContext();

  if (sample.pcm_data.empty()) {
    ImGui::TextDisabled("Empty sample (No PCM data)");
    return;
  }

  // Decode BRR for visualization (simplified)
  // For now, just plot raw bytes as signed values to show *something*
  // A real BRR decoder is needed for accurate waveform
  
  plot_x_.clear();
  plot_y_.clear();
  
  // Downsample for performance if needed
  int step = 1;
  if (sample.pcm_data.size() > 2000) step = static_cast<int>(sample.pcm_data.size()) / 2000;
  
  for (size_t i = 0; i < sample.pcm_data.size(); i += step) {
    plot_x_.push_back(static_cast<float>(i));
    plot_y_.push_back(static_cast<float>(sample.pcm_data[i]) / 32768.0f);
  }
  
  if (ImPlot::BeginPlot("Waveform (PCM)", ImVec2(-1, -1))) {
    ImPlot::SetupAxes("Sample", "Amplitude");
    ImPlot::SetupAxesLimits(0, sample.pcm_data.size(), -1.1, 1.1);
    ImPlot::PlotLine("PCM", plot_x_.data(), plot_y_.data(), static_cast<int>(plot_x_.size()));
    ImPlot::EndPlot();
  }
}

}  // namespace music
}  // namespace editor
}  // namespace yaze
