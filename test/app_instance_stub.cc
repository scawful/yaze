// Minimal stub implementations for non-GUI test binaries.
//
// The test link chain (yaze_test_support → yaze_grpc_support → editor lib)
// references Application::Instance() and Controller::RequestScreenshot() which
// are defined in application.cc and controller.cc respectively. Those files
// can't be linked into non-GUI test binaries because controller.cc has circular
// editor-library dependencies.
//
// This file provides no-op stubs so the non-GUI test binary links cleanly.
// GetEmulatorBackend() returns nullptr, which callers in the editor guard with
// an `if (backend)` check. RequestScreenshot() is a no-op in tests.

#include "app/application.h"
#include "app/controller.h"

namespace yaze {

Application& Application::Instance() {
  static Application instance;
  return instance;
}

void Controller::RequestScreenshot(const ScreenshotRequest& /*request*/) {
  // No-op in non-GUI test builds.
}

}  // namespace yaze
