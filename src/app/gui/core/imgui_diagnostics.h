#ifndef YAZE_APP_GUI_CORE_IMGUI_DIAGNOSTICS_H
#define YAZE_APP_GUI_CORE_IMGUI_DIAGNOSTICS_H

#include "imgui/imgui.h"

namespace yaze {
namespace gui {

/**
 * @enum ImGuiDiagnosticsBuild
 * @brief Which set of Dear ImGui diagnostics a build gets
 */
enum class ImGuiDiagnosticsBuild {
  kDebug,    // Assert on recoverable errors, highlight conflicting IDs
  kRelease,  // Report recoverable errors without aborting
};

/**
 * @brief Set Dear ImGui's error-recovery and ID-conflict options
 *
 * Call right after ImGui::CreateContext(), before the first NewFrame().
 *
 * Every build keeps error recovery on, so a missing End(), PopID() or
 * PopStyleColor() is repaired at EndFrame() and reported in ImGui's debug
 * log (which ImGui also prints to stdout) and in an on-screen error tooltip.
 * - kDebug: a recoverable error also asserts and items with conflicting IDs
 *   are highlighted, so a developer stops at the mistake.
 * - kRelease: recoverable errors do not assert, so a user gets the log line
 *   and the tooltip instead of an abort. ID conflicts are not highlighted.
 */
void ConfigureImGuiDiagnostics(ImGuiIO& io, ImGuiDiagnosticsBuild build);

/**
 * @brief Same as above for this binary's build type
 *
 * Uses kRelease when NDEBUG is defined and kDebug otherwise.
 */
void ConfigureImGuiDiagnostics(ImGuiIO& io);

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_CORE_IMGUI_DIAGNOSTICS_H
