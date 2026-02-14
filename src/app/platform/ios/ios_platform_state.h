#pragma once

#include <string>

namespace yaze {
namespace platform {
namespace ios {

struct SafeAreaInsets {
  float left = 0.0f;
  float right = 0.0f;
  float top = 0.0f;
  float bottom = 0.0f;
};

// Snapshot of current editor state, pushed to SwiftUI overlay each frame.
struct EditorStateSnapshot {
  bool can_undo = false;
  bool can_redo = false;
  bool can_save = false;
  bool is_dirty = false;
  std::string editor_type;
  std::string rom_title;
};

// Post editor state to the SwiftUI overlay via NSNotification.
// Only posts when the state has actually changed (diffed internally).
void PostEditorStateUpdate(const EditorStateSnapshot& state);

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

// Haptic feedback (iOS only, no-op on other platforms).
enum class HapticStyle {
  kLight,
  kMedium,
  kHeavy,
  kSelection,        // Subtle tick for selection changes
  kSuccess,          // Notification: success
  kWarning,          // Notification: warning
  kError,            // Notification: error
};
void TriggerHaptic(HapticStyle style);

// Post an undo/redo request to the application layer.
void PostUndoCommand();
void PostRedoCommand();

// Post a sidebar toggle request.
void PostToggleSidebar();

}  // namespace ios
}  // namespace platform
}  // namespace yaze
