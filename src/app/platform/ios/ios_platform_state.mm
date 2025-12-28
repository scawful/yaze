#include "app/platform/ios/ios_platform_state.h"

namespace yaze {
namespace platform {
namespace ios {

namespace {
void* g_metal_view = nullptr;
SafeAreaInsets g_safe_area_insets = {};
}  // namespace

void SetMetalView(void* view) {
  g_metal_view = view;
}

void* GetMetalView() {
  return g_metal_view;
}

void SetSafeAreaInsets(float left, float right, float top, float bottom) {
  g_safe_area_insets = {left, right, top, bottom};
}

SafeAreaInsets GetSafeAreaInsets() {
  return g_safe_area_insets;
}

}  // namespace ios
}  // namespace platform
}  // namespace yaze
