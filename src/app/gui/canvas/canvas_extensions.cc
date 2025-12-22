#include "canvas_extensions.h"

#include "app/gui/canvas/bpp_format_ui.h"
#include "app/gui/canvas/canvas_automation_api.h"
#include "app/gui/canvas/canvas_modals.h"
#include "app/gui/widgets/palette_editor_widget.h"

namespace yaze::gui {

CanvasExtensions::CanvasExtensions() = default;

CanvasExtensions::~CanvasExtensions() {
  Cleanup();
}

CanvasExtensions::CanvasExtensions(CanvasExtensions&&) noexcept = default;
CanvasExtensions& CanvasExtensions::operator=(CanvasExtensions&&) noexcept = default;

void CanvasExtensions::InitializeModals() {
  if (!modals) {
    modals = std::make_unique<CanvasModals>();
  }
}

void CanvasExtensions::InitializePaletteEditor() {
  if (!palette_editor) {
    palette_editor = std::make_unique<PaletteEditorWidget>();
  }
}

void CanvasExtensions::InitializeBppUI(const std::string& canvas_id) {
  if (!bpp_format_ui) {
    bpp_format_ui = std::make_unique<BppFormatUI>(canvas_id + "_bpp_format");
  }
}

void CanvasExtensions::InitializeAutomation(Canvas* canvas) {
  if (!automation_api) {
    automation_api = std::make_unique<CanvasAutomationAPI>(canvas);
  }
}

void CanvasExtensions::Cleanup() {
  bpp_format_ui.reset();
  bpp_conversion_dialog.reset();
  bpp_comparison_tool.reset();
  modals.reset();
  palette_editor.reset();
  automation_api.reset();
}

bool CanvasExtensions::HasAnyInitialized() const {
  return bpp_format_ui || bpp_conversion_dialog || bpp_comparison_tool ||
         modals || palette_editor || automation_api;
}

}  // namespace yaze::gui

