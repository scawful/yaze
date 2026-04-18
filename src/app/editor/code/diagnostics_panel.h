#ifndef YAZE_APP_EDITOR_CODE_DIAGNOSTICS_PANEL_H
#define YAZE_APP_EDITOR_CODE_DIAGNOSTICS_PANEL_H

// Renders a structured diagnostics list (file:line:col + severity) for the
// assembly editor's build-output panel. When z3dk is the active backend,
// diagnostics carry full location data; when Asar is active, AsarWrapper
// best-effort populates the same shape so this panel is backend-agnostic.

#include <functional>
#include <span>
#include <string>

#include "core/assembly_diagnostic.h"

namespace yaze {
namespace editor {

struct DiagnosticsPanelCallbacks {
  // Invoked when the user clicks a diagnostic row. Host is responsible for
  // navigation (focus file tab, move cursor to line/column).
  std::function<void(const std::string& file, int line, int column)>
      on_diagnostic_activated;
};

// Draws the diagnostics list inline. Expected to be hosted inside an
// ImGui child or panel window by the caller. Returns nothing — activation
// is routed via the callback.
void DrawDiagnosticsPanel(std::span<const core::AssemblyDiagnostic> diagnostics,
                          const DiagnosticsPanelCallbacks& callbacks);

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_CODE_DIAGNOSTICS_PANEL_H
