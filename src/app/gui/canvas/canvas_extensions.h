#ifndef YAZE_APP_GUI_CANVAS_CANVAS_EXTENSIONS_H
#define YAZE_APP_GUI_CANVAS_CANVAS_EXTENSIONS_H

#include <memory>
#include <string>

namespace yaze {

class Rom;

namespace gui {

// Forward declarations to avoid heavy includes
class BppFormatUI;
class BppConversionDialog;
class BppComparisonTool;
class CanvasModals;
class PaletteEditorWidget;
class CanvasAutomationAPI;
class Canvas;

/**
 * @brief Optional extension modules for Canvas
 *
 * Contains heavy optional subsystems that are lazy-initialized only when
 * needed. This keeps the core Canvas lean for simple use cases like
 * previews and selectors.
 *
 * Components:
 * - BPP format UI (format selector, conversion dialog, comparison tool)
 * - Modals system (advanced properties, scaling controls)
 * - Palette editor widget
 * - Automation API (for testing/scripting)
 */
struct CanvasExtensions {
  // BPP format components (lazy-initialized)
  std::unique_ptr<BppFormatUI> bpp_format_ui;
  std::unique_ptr<BppConversionDialog> bpp_conversion_dialog;
  std::unique_ptr<BppComparisonTool> bpp_comparison_tool;

  // Modals system
  std::unique_ptr<CanvasModals> modals;

  // Palette editor
  std::unique_ptr<PaletteEditorWidget> palette_editor;

  // Automation API
  std::unique_ptr<CanvasAutomationAPI> automation_api;

  // Construction/destruction
  CanvasExtensions();
  ~CanvasExtensions();

  // Prevent copying
  CanvasExtensions(const CanvasExtensions&) = delete;
  CanvasExtensions& operator=(const CanvasExtensions&) = delete;

  // Move is allowed
  CanvasExtensions(CanvasExtensions&&) noexcept;
  CanvasExtensions& operator=(CanvasExtensions&&) noexcept;

  // Lazy initialization helpers
  void InitializeModals();
  void InitializePaletteEditor();
  void InitializeBppUI(const std::string& canvas_id);
  void InitializeAutomation(Canvas* canvas);

  // Cleanup
  void Cleanup();

  // Check if any component is initialized
  bool HasAnyInitialized() const;
};

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_CANVAS_CANVAS_EXTENSIONS_H

