// clang-format off
#ifdef __EMSCRIPTEN__

#include "app/platform/wasm/wasm_async_guard.h"

namespace yaze {
namespace platform {

// Static member initialization
std::atomic<bool> WasmAsyncGuard::in_async_op_{false};

}  // namespace platform
}  // namespace yaze

#endif  // __EMSCRIPTEN__
// clang-format on
