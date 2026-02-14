#include "app/platform/ios/ios_platform_state.h"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <dispatch/dispatch.h>
#endif

namespace yaze {
namespace platform {
namespace ios {

namespace {
void* g_metal_view = nullptr;
SafeAreaInsets g_safe_area_insets = {};
float g_overlay_top_inset = 0.0f;
float g_touch_scale = 1.0f;

// Last-pushed state for diffing (avoids redundant notifications).
EditorStateSnapshot g_last_editor_state = {};
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

void SetTouchScale(float scale) {
  g_touch_scale = scale;
}

float GetTouchScale() {
  return g_touch_scale;
}

void PostOverlayCommand(const char* command) {
#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
  if (!command || command[0] == '\0') {
    return;
  }
  NSString* cmd = [NSString stringWithUTF8String:command];
  if (!cmd || cmd.length == 0) {
    return;
  }

  auto post = ^{
    [[NSNotificationCenter defaultCenter]
        postNotificationName:@"yaze.overlay.command"
                      object:nil
                    userInfo:@{@"command" : cmd}];
  };

  if ([NSThread isMainThread]) {
    post();
  } else {
    dispatch_async(dispatch_get_main_queue(), post);
  }
#else
  (void)command;
#endif
}

void TriggerHaptic(HapticStyle style) {
#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
  dispatch_async(dispatch_get_main_queue(), ^{
    switch (style) {
      case HapticStyle::kLight: {
        auto* gen = [[UIImpactFeedbackGenerator alloc]
            initWithStyle:UIImpactFeedbackStyleLight];
        [gen impactOccurred];
        break;
      }
      case HapticStyle::kMedium: {
        auto* gen = [[UIImpactFeedbackGenerator alloc]
            initWithStyle:UIImpactFeedbackStyleMedium];
        [gen impactOccurred];
        break;
      }
      case HapticStyle::kHeavy: {
        auto* gen = [[UIImpactFeedbackGenerator alloc]
            initWithStyle:UIImpactFeedbackStyleHeavy];
        [gen impactOccurred];
        break;
      }
      case HapticStyle::kSelection: {
        auto* gen = [[UISelectionFeedbackGenerator alloc] init];
        [gen selectionChanged];
        break;
      }
      case HapticStyle::kSuccess: {
        auto* gen = [[UINotificationFeedbackGenerator alloc] init];
        [gen notificationOccurred:UINotificationFeedbackTypeSuccess];
        break;
      }
      case HapticStyle::kWarning: {
        auto* gen = [[UINotificationFeedbackGenerator alloc] init];
        [gen notificationOccurred:UINotificationFeedbackTypeWarning];
        break;
      }
      case HapticStyle::kError: {
        auto* gen = [[UINotificationFeedbackGenerator alloc] init];
        [gen notificationOccurred:UINotificationFeedbackTypeError];
        break;
      }
    }
  });
#else
  (void)style;
#endif
}

void PostUndoCommand() {
#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
  dispatch_async(dispatch_get_main_queue(), ^{
    [[NSNotificationCenter defaultCenter]
        postNotificationName:@"yaze.input.undo"
                      object:nil];
  });
#endif
}

void PostRedoCommand() {
#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
  dispatch_async(dispatch_get_main_queue(), ^{
    [[NSNotificationCenter defaultCenter]
        postNotificationName:@"yaze.input.redo"
                      object:nil];
  });
#endif
}

void PostToggleSidebar() {
#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
  dispatch_async(dispatch_get_main_queue(), ^{
    [[NSNotificationCenter defaultCenter]
        postNotificationName:@"yaze.input.toggle_sidebar"
                      object:nil];
  });
#endif
}

void PostEditorStateUpdate(const EditorStateSnapshot& state) {
#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
  // Diff against last-pushed state to avoid redundant notifications.
  if (state.can_undo == g_last_editor_state.can_undo &&
      state.can_redo == g_last_editor_state.can_redo &&
      state.can_save == g_last_editor_state.can_save &&
      state.is_dirty == g_last_editor_state.is_dirty &&
      state.editor_type == g_last_editor_state.editor_type &&
      state.rom_title == g_last_editor_state.rom_title) {
    return;
  }
  g_last_editor_state = state;

  // Capture values for the block (C strings must be copied to NSString now).
  NSNumber* canUndo = @(state.can_undo);
  NSNumber* canRedo = @(state.can_redo);
  NSNumber* canSave = @(state.can_save);
  NSNumber* isDirty = @(state.is_dirty);
  NSString* editorType = [NSString stringWithUTF8String:state.editor_type.c_str()];
  NSString* romTitle = [NSString stringWithUTF8String:state.rom_title.c_str()];
  if (!editorType) {
    editorType = @"";
  }
  if (!romTitle) {
    romTitle = @"";
  }

  auto post = ^{
    [[NSNotificationCenter defaultCenter]
        postNotificationName:@"yaze.state.editor"
                      object:nil
                    userInfo:@{
                      @"canUndo" : canUndo,
                      @"canRedo" : canRedo,
                      @"canSave" : canSave,
                      @"isDirty" : isDirty,
                      @"editorType" : editorType,
                      @"romTitle" : romTitle,
                    }];
  };

  if ([NSThread isMainThread]) {
    post();
  } else {
    dispatch_async(dispatch_get_main_queue(), post);
  }
#else
  (void)state;
#endif
}

}  // namespace ios
}  // namespace platform
}  // namespace yaze
