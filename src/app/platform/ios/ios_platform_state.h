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

void SetTouchScale(float scale);
float GetTouchScale();

// Post a command to the SwiftUI overlay (iOS only).
// Commands map to OverlayCommand in `src/ios/iOS/YazeOverlayView.swift`.
void PostOverlayCommand(const char* command);

}  // namespace ios
}  // namespace platform
}  // namespace yaze
