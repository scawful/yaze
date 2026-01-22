#include "app/platform/ios/ios_platform_state.h"

namespace yaze {
namespace platform {
namespace ios {

namespace {
void* g_metal_view = nullptr;
SafeAreaInsets g_safe_area_insets = {};
float g_overlay_top_inset = 0.0f;
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

void SetOverlayTopInset(float top) {
  g_overlay_top_inset = top;
}

float GetOverlayTopInset() {
  return g_overlay_top_inset;
}

}  // namespace ios
}  // namespace platform
}  // namespace yaze
