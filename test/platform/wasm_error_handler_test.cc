#ifdef __EMSCRIPTEN__

#include <gtest/gtest.h>
#include "app/platform/wasm/wasm_error_handler.h"

namespace yaze {
namespace platform {
namespace {

// Note: These tests are primarily compile-time checks and basic API validation
// since we can't actually test browser UI interactions in a unit test

TEST(WasmErrorHandlerTest, InitializeDoesNotCrash) {
  // Should be safe to call multiple times
  WasmErrorHandler::Initialize();
  WasmErrorHandler::Initialize();
}

TEST(WasmErrorHandlerTest, ShowErrorAPIWorks) {
  WasmErrorHandler::ShowError("Test Error", "This is a test error message");
}

TEST(WasmErrorHandlerTest, ShowWarningAPIWorks) {
  WasmErrorHandler::ShowWarning("Test Warning", "This is a test warning");
}

TEST(WasmErrorHandlerTest, ShowInfoAPIWorks) {
  WasmErrorHandler::ShowInfo("Test Info", "This is informational");
}

TEST(WasmErrorHandlerTest, ToastAPIWorks) {
  WasmErrorHandler::Toast("Test toast", ToastType::kInfo, 1000);
  WasmErrorHandler::Toast("Success!", ToastType::kSuccess);
  WasmErrorHandler::Toast("Warning", ToastType::kWarning, 2000);
  WasmErrorHandler::Toast("Error", ToastType::kError, 500);
}

TEST(WasmErrorHandlerTest, ProgressAPIWorks) {
  WasmErrorHandler::ShowProgress("Loading ROM", 0.0f);
  WasmErrorHandler::ShowProgress("Loading ROM", 0.5f);
  WasmErrorHandler::ShowProgress("Loading ROM", 1.0f);
  WasmErrorHandler::HideProgress();
}

TEST(WasmErrorHandlerTest, ConfirmAPIWorks) {
  bool callback_called = false;
  WasmErrorHandler::Confirm("Are you sure?", [&callback_called](bool result) {
    callback_called = true;
    // In a real browser environment, this would be called when the user clicks
  });

  // Note: callback won't actually be called in unit test environment
  // This just tests that the API compiles and doesn't crash
}

}  // namespace
}  // namespace platform
}  // namespace yaze

#endif  // __EMSCRIPTEN__