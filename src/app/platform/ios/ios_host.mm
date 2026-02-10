#include "app/platform/ios/ios_host.h"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
#import <MetalKit/MetalKit.h>
#endif

#include "app/platform/ios/ios_platform_state.h"
#include "util/log.h"

namespace yaze::ios {

IOSHost::~IOSHost() {
  Shutdown();
}

absl::Status IOSHost::Initialize(const IOSHostConfig& config) {
  config_ = config;

#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
  if (!metal_view_) {
    return absl::FailedPreconditionError("Metal view not attached");
  }

  Application::Instance().Initialize(config_.app_config);

  initialized_ = true;
  LOG_INFO("IOSHost", "Initialized iOS host");
  return absl::OkStatus();
#else
  return absl::FailedPreconditionError("IOSHost only available on iOS");
#endif
}

void IOSHost::Tick() {
  if (!initialized_) {
    return;
  }
  Application::Instance().Tick();
}

void IOSHost::Shutdown() {
  if (!initialized_) {
    return;
  }
  Application::Instance().Shutdown();
  initialized_ = false;
}

void IOSHost::SetMetalView(void* view) {
  metal_view_ = view;
  platform::ios::SetMetalView(view);
}

void* IOSHost::GetMetalView() const {
  return metal_view_;
}

}  // namespace yaze::ios
