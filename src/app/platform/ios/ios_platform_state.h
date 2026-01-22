#pragma once

namespace yaze {
namespace platform {
namespace ios {

struct SafeAreaInsets {
  float left = 0.0f;
  float right = 0.0f;
  float top = 0.0f;
  float bottom = 0.0f;
};

void SetMetalView(void* view);
void* GetMetalView();

void SetSafeAreaInsets(float left, float right, float top, float bottom);
SafeAreaInsets GetSafeAreaInsets();

void SetOverlayTopInset(float top);
float GetOverlayTopInset();

}  // namespace ios
}  // namespace platform
}  // namespace yaze
