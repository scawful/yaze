#include "app/platform/ios/ios_platform_state.h"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
#import <Foundation/Foundation.h>
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

}  // namespace ios
}  // namespace platform
}  // namespace yaze
