#ifndef YAZE_APP_GUI_CANVAS_CANVAS_MODALS_H
#define YAZE_APP_GUI_CANVAS_CANVAS_MODALS_H

#include <functional>
#include <string>
#include <utility>

#include "app/gfx/core/bitmap.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gfx/util/bpp_format_manager.h"
#include "app/gui/canvas/canvas_utils.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {

// Note: DispatchConfigCallback and DispatchScaleCallback are internal helpers
// defined in canvas_modals.cc (not part of public API)

/**
 * @brief BPP conversion options
 */
struct BppConversionOptions {
  const gfx::Bitmap* bitmap = nullptr;
  const gfx::SnesPalette* palette = nullptr;
  std::function<void(gfx::BppFormat)> on_convert;
};

/**
 * @brief Palette editor options
 */
struct PaletteEditorOptions {
  gfx::SnesPalette* palette = nullptr;
  std::string title = "Palette Editor";
  std::function<void()> on_palette_changed;
};

/**
 * @brief Color analysis options
 */
struct ColorAnalysisOptions {
  const gfx::Bitmap* bitmap = nullptr;
  const gfx::SnesPalette* palette = nullptr;
  std::string title = "Color Analysis";
};

/**
 * @brief Performance integration options
 */
struct PerformanceOptions {
  std::string operation_name;
  double operation_time_ms = 0.0;
  std::function<void()> on_dashboard_request;
};

/**
 * @brief Modal dialog management for canvas operations
 */
class CanvasModals {
 public:
  CanvasModals() = default;

  /**
   * @brief Show advanced canvas properties modal
   */
  void ShowAdvancedProperties(const std::string& canvas_id,
                              const CanvasConfig& config,
                              const gfx::Bitmap* bitmap = nullptr);

  /**
   * @brief Show scaling controls modal
   */
  void ShowScalingControls(const std::string& canvas_id,
                           const CanvasConfig& config,
                           const gfx::Bitmap* bitmap = nullptr);

  /**
   * @brief Show BPP format conversion dialog
   */
  void ShowBppConversionDialog(const std::string& canvas_id,
                               const BppConversionOptions& options);

  /**
   * @brief Show palette editor modal
   */
  void ShowPaletteEditor(const std::string& canvas_id,
                         const PaletteEditorOptions& options);

  /**
   * @brief Show color analysis modal
   */
  void ShowColorAnalysis(const std::string& canvas_id,
                         const ColorAnalysisOptions& options);

  /**
   * @brief Show performance dashboard integration
   */
  void ShowPerformanceIntegration(const std::string& canvas_id,
                                  const PerformanceOptions& options);

  /**
   * @brief Render all active modals
   */
  void Render();

  /**
   * @brief Check if any modal is open
   */
  bool IsAnyModalOpen() const;

 private:
  struct ModalState {
    bool is_open = false;
    std::string id;
    std::function<void()> render_func;
  };

  std::vector<ModalState> active_modals_;

  // Modal rendering functions
  void RenderAdvancedPropertiesModal(const std::string& canvas_id,
                                     CanvasConfig& config,
                                     const gfx::Bitmap* bitmap);

  void RenderScalingControlsModal(const std::string& canvas_id,
                                  CanvasConfig& config,
                                  const gfx::Bitmap* bitmap);

  void RenderBppConversionModal(const std::string& canvas_id,
                                const BppConversionOptions& options);

  void RenderPaletteEditorModal(const std::string& canvas_id,
                                const PaletteEditorOptions& options);

  void RenderColorAnalysisModal(const std::string& canvas_id,
                                const ColorAnalysisOptions& options);

  void RenderPerformanceModal(const std::string& canvas_id,
                              const PerformanceOptions& options);

  // Helper methods
  void OpenModal(const std::string& id, std::function<void()> render_func);
  void CloseModal(const std::string& id);
  bool IsModalOpen(const std::string& id) const;

  // UI helper methods
  void RenderMaterialIcon(const std::string& icon_name,
                          const ImVec4& color = ImVec4(1, 1, 1, 1));
  void RenderMetricPanel(const std::string& title, const std::string& value,
                        const std::string& icon,
                        const ImVec4& color = ImVec4(1, 1, 1, 1));
  void RenderSliderWithIcon(const std::string& label, const std::string& icon,
                            float* value, float min_val, float max_val,
                            const char* format = "%.2f");
};

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_CANVAS_CANVAS_MODALS_H
