#include "canvas_modals.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include "app/gfx/debug/performance/performance_dashboard.h"
#include "app/gfx/debug/performance/performance_profiler.h"
#include "app/gui/canvas/bpp_format_ui.h"
#include "app/gui/core/icons.h"
#include "app/gui/widgets/palette_editor_widget.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {

// Helper functions for dispatching config callbacks
namespace {
inline void DispatchConfig(
    const std::function<void(const CanvasConfig&)>& callback,
    const CanvasConfig& config) {
  if (callback)
    callback(config);
}

inline void DispatchScale(
    const std::function<void(const CanvasConfig&)>& callback,
    const CanvasConfig& config) {
  if (callback)
    callback(config);
}
}  // namespace

void CanvasModals::ShowAdvancedProperties(const std::string& canvas_id,
                                          const CanvasConfig& config,
                                          const gfx::Bitmap* bitmap) {
  std::string modal_id = canvas_id + "_advanced_properties";

  auto render_func = [=]() mutable {
    CanvasConfig mutable_config = config;  // Create mutable copy
    mutable_config.on_config_changed = config.on_config_changed;
    mutable_config.on_scale_changed = config.on_scale_changed;
    RenderAdvancedPropertiesModal(modal_id, mutable_config, bitmap);
  };

  OpenModal(modal_id, render_func);
}

void CanvasModals::ShowScalingControls(const std::string& canvas_id,
                                       const CanvasConfig& config,
                                       const gfx::Bitmap* bitmap) {
  std::string modal_id = canvas_id + "_scaling_controls";

  auto render_func = [=]() mutable {
    CanvasConfig mutable_config = config;  // Create mutable copy
    mutable_config.on_config_changed = config.on_config_changed;
    mutable_config.on_scale_changed = config.on_scale_changed;
    RenderScalingControlsModal(modal_id, mutable_config, bitmap);
  };

  OpenModal(modal_id, render_func);
}

void CanvasModals::ShowBppConversionDialog(
    const std::string& canvas_id, const BppConversionOptions& options) {
  std::string modal_id = canvas_id + "_bpp_conversion";

  auto render_func = [=]() {
    RenderBppConversionModal(modal_id, options);
  };

  OpenModal(modal_id, render_func);
}

void CanvasModals::ShowPaletteEditor(const std::string& canvas_id,
                                     const PaletteEditorOptions& options) {
  std::string modal_id = canvas_id + "_palette_editor";

  auto render_func = [=]() {
    RenderPaletteEditorModal(modal_id, options);
  };

  OpenModal(modal_id, render_func);
}

void CanvasModals::ShowColorAnalysis(const std::string& canvas_id,
                                     const ColorAnalysisOptions& options) {
  std::string modal_id = canvas_id + "_color_analysis";

  auto render_func = [=]() {
    RenderColorAnalysisModal(modal_id, options);
  };

  OpenModal(modal_id, render_func);
}

void CanvasModals::ShowPerformanceIntegration(
    const std::string& canvas_id, const PerformanceOptions& options) {
  std::string modal_id = canvas_id + "_performance";

  auto render_func = [=]() {
    RenderPerformanceModal(modal_id, options);
  };

  OpenModal(modal_id, render_func);
}

void CanvasModals::Render() {
  for (auto& modal : active_modals_) {
    if (modal.is_open) {
      modal.render_func();
    }
  }

  // Remove closed modals
  active_modals_.erase(
      std::remove_if(active_modals_.begin(), active_modals_.end(),
                     [](const ModalState& modal) { return !modal.is_open; }),
      active_modals_.end());
}

bool CanvasModals::IsAnyModalOpen() const {
  return std::any_of(active_modals_.begin(), active_modals_.end(),
                     [](const ModalState& modal) { return modal.is_open; });
}

void CanvasModals::RenderAdvancedPropertiesModal(const std::string& canvas_id,
                                                 CanvasConfig& config,
                                                 const gfx::Bitmap* bitmap) {
  std::string modal_title = "Advanced Canvas Properties";
  ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_FirstUseEver);

  if (ImGui::BeginPopupModal(modal_title.c_str(), nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    // Header with icon
    ImGui::Text("%s %s", ICON_MD_SETTINGS, modal_title.c_str());
    ImGui::Separator();

    // Canvas Information Section
    if (ImGui::CollapsingHeader(ICON_MD_ANALYTICS " Canvas Information",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Columns(2, "CanvasInfo");

      RenderMetricCard(
          "Canvas Size",
          std::to_string(static_cast<int>(config.canvas_size.x)) + " x " +
              std::to_string(static_cast<int>(config.canvas_size.y)),
          ICON_MD_STRAIGHTEN, ImVec4(0.2F, 0.8F, 1.0F, 1.0F));

      RenderMetricCard(
          "Content Size",
          std::to_string(static_cast<int>(config.content_size.x)) + " x " +
              std::to_string(static_cast<int>(config.content_size.y)),
          ICON_MD_IMAGE, ImVec4(0.8F, 0.2F, 1.0F, 1.0F));

      ImGui::NextColumn();

      RenderMetricCard(
          "Global Scale",
          std::to_string(static_cast<int>(config.global_scale * 100)) + "%",
          ICON_MD_ZOOM_IN, ImVec4(1.0F, 0.8F, 0.2F, 1.0F));

      RenderMetricCard(
          "Grid Step",
          std::to_string(static_cast<int>(config.grid_step)) + "px",
          ICON_MD_GRID_ON, ImVec4(0.2F, 1.0F, 0.2F, 1.0F));

      ImGui::Columns(1);
    }

    // View Settings Section
    if (ImGui::CollapsingHeader("👁️ View Settings",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Checkbox("Show Grid", &config.enable_grid);
      ImGui::SameLine();
      RenderMaterialIcon("grid_on");

      ImGui::Checkbox("Show Hex Labels", &config.enable_hex_labels);
      ImGui::SameLine();
      RenderMaterialIcon("label");

      ImGui::Checkbox("Show Custom Labels", &config.enable_custom_labels);
      ImGui::SameLine();
      RenderMaterialIcon("edit");

      ImGui::Checkbox("Enable Context Menu", &config.enable_context_menu);
      ImGui::SameLine();
      RenderMaterialIcon("menu");

      ImGui::Checkbox("Draggable Canvas", &config.is_draggable);
      ImGui::SameLine();
      RenderMaterialIcon("drag_indicator");

      ImGui::Checkbox("Auto Resize for Tables", &config.auto_resize);
      ImGui::SameLine();
      RenderMaterialIcon("fit_screen");
    }

    // Scale Controls Section
    if (ImGui::CollapsingHeader(ICON_MD_BUILD " Scale Controls",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      RenderSliderWithIcon("Global Scale", "zoom_in", &config.global_scale,
                           0.1f, 10.0f, "%.2f");
      RenderSliderWithIcon("Grid Step", "grid_on", &config.grid_step, 1.0f,
                           128.0f, "%.1f");

      // Preset scale buttons
      ImGui::Text("Preset Scales:");
      ImGui::SameLine();

      const char* preset_labels[] = {"0.25x", "0.5x", "1x", "2x", "4x", "8x"};
      const float preset_values[] = {0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f};

      for (int i = 0; i < 6; ++i) {
        if (i > 0)
          ImGui::SameLine();
        if (ImGui::Button(preset_labels[i])) {
          config.global_scale = preset_values[i];
          DispatchConfig(config.on_config_changed, config);
        }
      }
    }

    // Scrolling Controls Section
    if (ImGui::CollapsingHeader("📜 Scrolling Controls")) {
      ImGui::Text("Current Scroll: %.1f, %.1f", config.scrolling.x,
                  config.scrolling.y);

      if (ImGui::Button("Reset Scroll")) {
        config.scrolling = ImVec2(0, 0);
        DispatchConfig(config.on_config_changed, config);
      }
      ImGui::SameLine();

      if (ImGui::Button("Center View") && bitmap) {
        config.scrolling = ImVec2(
            -(bitmap->width() * config.global_scale - config.canvas_size.x) /
                2.0f,
            -(bitmap->height() * config.global_scale - config.canvas_size.y) /
                2.0f);
        DispatchConfig(config.on_config_changed, config);
      }
    }

    // Performance Integration Section
    if (ImGui::CollapsingHeader(ICON_MD_TRENDING_UP " Performance")) {
      auto& profiler = gfx::PerformanceProfiler::Get();

      // Get stats for canvas operations
      auto canvas_stats = profiler.GetStats("canvas_operations");
      auto draw_stats = profiler.GetStats("canvas_draw");

      RenderMetricCard("Canvas Operations",
                       std::to_string(canvas_stats.sample_count) + " ops",
                       "speed", ImVec4(0.2F, 1.0F, 0.2F, 1.0F));

      RenderMetricCard("Average Time",
                       std::to_string(draw_stats.avg_time_us / 1000.0) + " ms",
                       "timer", ImVec4(1.0F, 0.8F, 0.2F, 1.0F));

      if (ImGui::Button("Open Performance Dashboard")) {
        gfx::PerformanceDashboard::Get().SetVisible(true);
      }
    }

    // Action Buttons
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Apply Changes", ImVec2(120, 0))) {
      DispatchConfig(config.on_config_changed, config);
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();

    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();

    if (ImGui::Button("Reset to Defaults", ImVec2(150, 0))) {
      config.global_scale = 1.0f;
      config.grid_step = 32.0f;
      config.enable_grid = true;
      config.enable_hex_labels = false;
      config.enable_custom_labels = false;
      config.enable_context_menu = true;
      config.is_draggable = false;
      config.auto_resize = false;
      config.scrolling = ImVec2(0, 0);
      DispatchConfig(config.on_config_changed, config);
    }

    ImGui::EndPopup();
  }
}

void CanvasModals::RenderScalingControlsModal(const std::string& canvas_id,
                                              CanvasConfig& config,
                                              const gfx::Bitmap* bitmap) {
  std::string modal_title = "Canvas Scaling Controls";
  ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);

  if (ImGui::BeginPopupModal(modal_title.c_str(), nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    // Header with icon
    ImGui::Text("%s %s", ICON_MD_ZOOM_IN, modal_title.c_str());
    ImGui::Separator();

    // Global Scale Section
    ImGui::Text("Global Scale: %.3f", config.global_scale);
    RenderSliderWithIcon("##GlobalScale", "zoom_in", &config.global_scale, 0.1f,
                         10.0f, "%.2f");

    // Preset scale buttons
    ImGui::Text("Preset Scales:");
    const char* preset_labels[] = {"0.25x", "0.5x", "1x", "2x", "4x", "8x"};
    const float preset_values[] = {0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f};

    for (int i = 0; i < 6; ++i) {
      if (i > 0)
        ImGui::SameLine();
      if (ImGui::Button(preset_labels[i])) {
        config.global_scale = preset_values[i];
        DispatchScale(config.on_scale_changed, config);
      }
    }

    ImGui::Separator();

    // Grid Configuration Section
    ImGui::Text("Grid Step: %.1f", config.grid_step);
    RenderSliderWithIcon("##GridStep", "grid_on", &config.grid_step, 1.0f,
                         128.0f, "%.1f");

    // Grid size presets
    ImGui::Text("Grid Presets:");
    const char* grid_labels[] = {"8x8", "16x16", "32x32", "64x64"};
    const float grid_values[] = {8.0f, 16.0f, 32.0f, 64.0f};

    for (int i = 0; i < 4; ++i) {
      if (i > 0)
        ImGui::SameLine();
      if (ImGui::Button(grid_labels[i])) {
        config.grid_step = grid_values[i];
        DispatchScale(config.on_scale_changed, config);
      }
    }

    ImGui::Separator();

    // Canvas Information Section
    ImGui::Text("Canvas Information");
    ImGui::Text("Canvas Size: %.0f x %.0f", config.canvas_size.x,
                config.canvas_size.y);
    ImGui::Text("Scaled Size: %.0f x %.0f",
                config.canvas_size.x * config.global_scale,
                config.canvas_size.y * config.global_scale);

    if (bitmap) {
      ImGui::Text("Bitmap Size: %d x %d", bitmap->width(), bitmap->height());
      ImGui::Text(
          "Effective Scale: %.3f x %.3f",
          (config.canvas_size.x * config.global_scale) / bitmap->width(),
          (config.canvas_size.y * config.global_scale) / bitmap->height());
    }

    // Action Buttons
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Apply", ImVec2(120, 0))) {
      DispatchScale(config.on_scale_changed, config);
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();

    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

void CanvasModals::RenderBppConversionModal(
    const std::string& canvas_id, const BppConversionOptions& options) {
  std::string modal_title = "BPP Format Conversion";
  ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_FirstUseEver);

  if (ImGui::BeginPopupModal(modal_title.c_str(), nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    // Header with icon
    ImGui::Text("%s %s", ICON_MD_SWAP_HORIZ, modal_title.c_str());
    ImGui::Separator();

    // Use the existing BppFormatUI for the conversion dialog
    static std::unique_ptr<gui::BppFormatUI> bpp_ui =
        std::make_unique<gui::BppFormatUI>(canvas_id + "_bpp_ui");

    // Render the format selector
    if (options.bitmap && options.palette) {
      bpp_ui->RenderFormatSelector(const_cast<gfx::Bitmap*>(options.bitmap),
                                   *options.palette, options.on_convert);
    }

    // Action Buttons
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Close", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

void CanvasModals::RenderPaletteEditorModal(
    const std::string& canvas_id, const PaletteEditorOptions& options) {
  std::string modal_title =
      options.title.empty() ? "Palette Editor" : options.title;
  ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);

  if (ImGui::BeginPopupModal(modal_title.c_str(), nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    // Header with icon
    ImGui::Text("%s %s", ICON_MD_PALETTE, modal_title.c_str());
    ImGui::Separator();

    // Use the existing PaletteWidget
    static std::unique_ptr<gui::PaletteEditorWidget> palette_editor =
        std::make_unique<gui::PaletteEditorWidget>();

    if (options.palette) {
      palette_editor->ShowPaletteEditor(*options.palette, modal_title);
    }

    // Action Buttons
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Close", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

void CanvasModals::RenderColorAnalysisModal(
    const std::string& canvas_id, const ColorAnalysisOptions& options) {
  std::string modal_title = "Color Analysis";
  ImGui::SetNextWindowSize(ImVec2(700, 500), ImGuiCond_FirstUseEver);

  if (ImGui::BeginPopupModal(modal_title.c_str(), nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    // Header with icon
    ImGui::Text("%s %s", ICON_MD_ZOOM_IN, modal_title.c_str());
    ImGui::Separator();

    // Use the existing PaletteWidget for color analysis
    static std::unique_ptr<gui::PaletteEditorWidget> palette_editor =
        std::make_unique<gui::PaletteEditorWidget>();

    if (options.bitmap) {
      palette_editor->ShowColorAnalysis(*options.bitmap, modal_title);
    }

    // Action Buttons
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Close", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

void CanvasModals::RenderPerformanceModal(const std::string& canvas_id,
                                          const PerformanceOptions& options) {
  std::string modal_title = "Canvas Performance";
  ImGui::SetNextWindowSize(ImVec2(500, 300), ImGuiCond_FirstUseEver);

  if (ImGui::BeginPopupModal(modal_title.c_str(), nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    // Header with icon
    ImGui::Text("%s %s", ICON_MD_TRENDING_UP, modal_title.c_str());
    ImGui::Separator();

    // Performance metrics
    RenderMetricCard("Operation", options.operation_name, "speed",
                     ImVec4(0.2f, 1.0f, 0.2f, 1.0f));
    RenderMetricCard("Time", std::to_string(options.operation_time_ms) + " ms",
                     "timer", ImVec4(1.0f, 0.8f, 0.2f, 1.0f));

    // Get overall performance stats
    auto& profiler = gfx::PerformanceProfiler::Get();
    auto canvas_stats = profiler.GetStats("canvas_operations");
    auto draw_stats = profiler.GetStats("canvas_draw");

    RenderMetricCard("Total Operations",
                     std::to_string(canvas_stats.sample_count), "functions",
                     ImVec4(0.2F, 0.8F, 1.0F, 1.0F));
    RenderMetricCard("Average Time",
                     std::to_string(draw_stats.avg_time_us / 1000.0) + " ms",
                     "schedule", ImVec4(0.8F, 0.2F, 1.0F, 1.0F));

    // Action Buttons
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Open Dashboard", ImVec2(150, 0))) {
      gfx::PerformanceDashboard::Get().SetVisible(true);
    }
    ImGui::SameLine();

    if (ImGui::Button("Close", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

void CanvasModals::OpenModal(const std::string& id,
                             std::function<void()> render_func) {
  // Check if modal already exists
  auto it =
      std::find_if(active_modals_.begin(), active_modals_.end(),
                   [&id](const ModalState& modal) { return modal.id == id; });

  if (it != active_modals_.end()) {
    it->is_open = true;
    it->render_func = render_func;
  } else {
    active_modals_.push_back({true, id, render_func});
  }

  // Open the popup
  ImGui::OpenPopup(id.c_str());
}

void CanvasModals::CloseModal(const std::string& id) {
  auto it =
      std::find_if(active_modals_.begin(), active_modals_.end(),
                   [&id](const ModalState& modal) { return modal.id == id; });

  if (it != active_modals_.end()) {
    it->is_open = false;
  }
}

bool CanvasModals::IsModalOpen(const std::string& id) const {
  auto it =
      std::find_if(active_modals_.begin(), active_modals_.end(),
                   [&id](const ModalState& modal) { return modal.id == id; });

  return it != active_modals_.end() && it->is_open;
}

void CanvasModals::RenderMaterialIcon(const std::string& icon_name,
                                      const ImVec4& color) {
  // Simple material icon rendering using Unicode symbols
  // In a real implementation, you'd use a proper icon font
  static std::unordered_map<std::string, const char*> icon_map = {
      {"grid_on", ICON_MD_GRID_ON},
      {"label", ICON_MD_LABEL},
      {"edit", ICON_MD_EDIT},
      {"menu", ICON_MD_MENU},
      {"drag_indicator", ICON_MD_DRAG_INDICATOR},
      {"fit_screen", ICON_MD_FIT_SCREEN},
      {"zoom_in", ICON_MD_ZOOM_IN},
      {"speed", ICON_MD_SPEED},
      {"timer", ICON_MD_TIMER},
      {"functions", ICON_MD_FUNCTIONS},
      {"schedule", ICON_MD_SCHEDULE},
      {"refresh", ICON_MD_REFRESH},
      {"settings", ICON_MD_SETTINGS},
      {"info", ICON_MD_INFO}};

  auto it = icon_map.find(icon_name);
  if (it != icon_map.end()) {
    ImGui::TextColored(color, "%s", it->second);
  }
}

void CanvasModals::RenderMetricCard(const std::string& title,
                                    const std::string& value,
                                    const std::string& icon,
                                    const ImVec4& color) {
  ImGui::BeginGroup();

  // Icon and title
  ImGui::Text("%s %s", icon.c_str(), title.c_str());

  // Value with color
  ImGui::TextColored(color, "%s", value.c_str());

  ImGui::EndGroup();
}

void CanvasModals::RenderSliderWithIcon(const std::string& label,
                                        const std::string& icon, float* value,
                                        float min_val, float max_val,
                                        const char* format) {
  ImGui::Text("%s %s", icon.c_str(), label.c_str());
  ImGui::SameLine();
  ImGui::SetNextItemWidth(200);
  ImGui::SliderFloat(("##" + label).c_str(), value, min_val, max_val, format);
}

}  // namespace gui
}  // namespace yaze
