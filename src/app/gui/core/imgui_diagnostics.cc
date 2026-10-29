#include "app/gui/core/imgui_diagnostics.h"

namespace yaze {
namespace gui {

void ConfigureImGuiDiagnostics(ImGuiIO& io, ImGuiDiagnosticsBuild build) {
  const bool debug_build = build == ImGuiDiagnosticsBuild::kDebug;

  // ImGui requires at least one of assert, log or tooltip while recovery is
  // on. Log and tooltip stay on in every build so a release build still
  // shows the error after the assert is turned off.
  io.ConfigErrorRecovery = true;
  io.ConfigErrorRecoveryEnableDebugLog = true;
  io.ConfigErrorRecoveryEnableTooltip = true;
  io.ConfigErrorRecoveryEnableAssert = debug_build;

  // ImGui's own guidance is to turn the ID-conflict popup off for
  // non-programmer builds.
  io.ConfigDebugHighlightIdConflicts = debug_build;
}

void ConfigureImGuiDiagnostics(ImGuiIO& io) {
#ifdef NDEBUG
  ConfigureImGuiDiagnostics(io, ImGuiDiagnosticsBuild::kRelease);
#else
  ConfigureImGuiDiagnostics(io, ImGuiDiagnosticsBuild::kDebug);
#endif
}

}  // namespace gui
}  // namespace yaze
