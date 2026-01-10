#ifdef __EMSCRIPTEN__

#include "app/platform/wasm/wasm_async_guard.h"

#include <gtest/gtest.h>

namespace yaze::platform {
namespace {

TEST(WasmAsyncGuardTest, TryAcquireAndReleaseTogglesState) {
  WasmAsyncGuard::Release();
  EXPECT_FALSE(WasmAsyncGuard::IsInProgress());

  EXPECT_TRUE(WasmAsyncGuard::TryAcquire());
  EXPECT_TRUE(WasmAsyncGuard::IsInProgress());

  WasmAsyncGuard::Release();
  EXPECT_FALSE(WasmAsyncGuard::IsInProgress());
}

TEST(WasmAsyncGuardTest, ScopedGuardReleasesOnDestruct) {
  WasmAsyncGuard::Release();
  {
    WasmAsyncGuard::ScopedGuard guard;
    EXPECT_TRUE(guard.acquired());
    EXPECT_TRUE(WasmAsyncGuard::IsInProgress());
  }
  EXPECT_FALSE(WasmAsyncGuard::IsInProgress());
}

}  // namespace
}  // namespace yaze::platform

#endif  // __EMSCRIPTEN__
