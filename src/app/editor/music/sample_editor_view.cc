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

void SampleEditorView::Draw(MusicBank& bank) {
  // Layout: List (20%), Properties (25%), Waveform (Rest)
  float total_w = ImGui::GetContentRegionAvail().x;
  float list_w = std::max(150.0f, total_w * 0.2f);
  float props_w = std::max(220.0f, total_w * 0.25f);
  
  ImGui::BeginChild("SampleList", ImVec2(list_w, 0), true);
  DrawSampleList(bank);
  ImGui::EndChild();

  ImGui::SameLine();

  ImGui::BeginChild("SampleProps", ImVec2(props_w, 0), true);
  if (selected_sample_index_ >= 0 &&
      selected_sample_index_ < static_cast<int>(bank.GetSampleCount())) {
    DrawProperties(*bank.GetSample(selected_sample_index_));
  } else {
    ImGui::TextDisabled("Select a sample");
  }
  ImGui::EndChild();

  ImGui::SameLine();

  ImGui::BeginChild("SampleWaveform", ImVec2(0, 0), true);
  if (selected_sample_index_ >= 0 &&
      selected_sample_index_ < static_cast<int>(bank.GetSampleCount())) {
    DrawWaveform(*bank.GetSample(selected_sample_index_));
  }
  ImGui::EndChild();
}

void SampleEditorView::DrawSampleList(MusicBank& bank) {
  if (ImGui::Button("Import WAV/BRR")) {
    // TODO: Implement file dialog for BRR import
    // For now, we simulate an import with a dummy sine wave
    auto result = bank.ImportSampleFromWav("dummy.wav", "New Sample");
    if (result.ok()) {
      selected_sample_index_ = result.value();
      if (on_edit_) on_edit_();
    }
  }
  
  ImGui::Separator();

  for (size_t i = 0; i < bank.GetSampleCount(); ++i) {
    const auto* sample = bank.GetSample(i);
    std::string label = absl::StrFormat("%02X: %s", i, sample->name.c_str());
    if (ImGui::Selectable(label.c_str(), selected_sample_index_ == static_cast<int>(i))) {
      selected_sample_index_ = static_cast<int>(i);
    }
  }
}

void SampleEditorView::DrawProperties(MusicSample& sample) {
  bool changed = false;

  ImGui::Text("Sample Properties");
  ImGui::Separator();

  // Name
  char name_buf[64];
  strncpy(name_buf, sample.name.c_str(), sizeof(name_buf));
  if (ImGui::InputText("Name", name_buf, sizeof(name_buf))) {
    sample.name = name_buf;
    changed = true;
  }

  ImGui::Spacing();

  // Size Info
  ImGui::Text("BRR Size: %zu bytes", sample.brr_data.size());
  int blocks = static_cast<int>(sample.brr_data.size() / 9);
  ImGui::Text("Blocks: %d", blocks);
  ImGui::Text("Duration: %.3f s", (blocks * 16) / 32040.0f);
  
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("Loop Settings");
  ImGui::SameLine();
  HelpMarker("SNES samples can loop. The loop point is defined in BRR blocks (groups of 16 samples).");

  // Loop Flag
  bool loops = sample.loops;
  if (ImGui::Checkbox("Loop Enabled", &loops)) {
    sample.loops = loops;
    changed = true;
  }

  // Loop Point
  // Stored as byte offset in brr_data (must be multiple of 9)
  int loop_block = sample.loop_point / 9;
  int max_block = std::max(0, blocks - 1);
  
  if (loops) {
    if (ImGui::SliderInt("Loop Start (Block)", &loop_block, 0, max_block)) {
      sample.loop_point = loop_block * 9;
      changed = true;
    }
    ImGui::TextDisabled("Offset: $%04X bytes", sample.loop_point);
    ImGui::TextDisabled("Sample: %d", loop_block * 16);
  } else {
    ImGui::BeginDisabled();
    ImGui::SliderInt("Loop Start (Block)", &loop_block, 0, max_block);
    ImGui::EndDisabled();
  }

  ImGui::Spacing();
  ImGui::Separator();

  if (on_preview_) {
    if (ImGui::Button(ICON_MD_PLAY_ARROW " Preview Sample")) {
      on_preview_(selected_sample_index_);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Play this sample at default pitch (requires ROM loaded)");
    }
  } else {
    ImGui::BeginDisabled();
    ImGui::Button(ICON_MD_PLAY_ARROW " Preview Sample");
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      ImGui::SetTooltip("Preview not available - load a ROM first");
    }
  }

  if (changed && on_edit_) {
    on_edit_();
  }
}

void SampleEditorView::DrawWaveform(const MusicSample& sample) {
  // Ensure ImPlot context exists before plotting
  yaze::gui::plotting::EnsureImPlotContext();

  if (sample.pcm_data.empty()) {
    ImGui::TextDisabled("Empty sample (No PCM data)");
    ImGui::TextWrapped("Import a WAV file or BRR sample to view waveform.");
    return;
  }

  // Decode BRR for visualization (simplified)
  // For now, just plot raw bytes as signed values to show *something*
  // A real BRR decoder is needed for accurate waveform
  
  plot_x_.clear();
  plot_y_.clear();
  
  // Downsample for performance if needed
  int step = 1;
  if (sample.pcm_data.size() > 4000) step = static_cast<int>(sample.pcm_data.size()) / 4000;
  if (step < 1) step = 1;
  
  for (size_t i = 0; i < sample.pcm_data.size(); i += step) {
    plot_x_.push_back(static_cast<float>(i));
    plot_y_.push_back(static_cast<float>(sample.pcm_data[i]) / 32768.0f);
  }
  
  if (ImPlot::BeginPlot("Waveform", ImVec2(-1, -1))) {
    ImPlot::SetupAxes("Sample", "Amplitude");
    ImPlot::SetupAxesLimits(0, sample.pcm_data.size(), -1.1, 1.1);
    
    ImPlot::PlotLine("PCM", plot_x_.data(), plot_y_.data(), static_cast<int>(plot_x_.size()));

    // Draw Loop Point
    if (sample.loops) {
      double loop_sample = (sample.loop_point / 9.0) * 16.0;
      ImPlot::TagX(loop_sample, ImVec4(0, 1, 0, 1), "Loop Start");
      ImPlot::SetNextLineStyle(ImVec4(0, 1, 0, 0.5));
      double loop_x[] = {loop_sample, loop_sample};
      double loop_y[] = {-1.0, 1.0};
      ImPlot::PlotLine("Loop", loop_x, loop_y, 2);
    }

    ImPlot::EndPlot();
  }
}

}  // namespace music
}  // namespace editor
}  // namespace yaze