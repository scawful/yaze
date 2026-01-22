// window_backend_factory.cc - Window Backend Factory Implementation

#include "app/platform/iwindow.h"

#include "app/platform/sdl2_window_backend.h"
#include "app/platform/null_window_backend.h"
#include "util/log.h"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
#include "app/platform/ios/ios_window_backend.h"
#endif

#ifdef YAZE_USE_SDL3
#include "app/platform/sdl3_window_backend.h"
#endif

namespace yaze {
namespace platform {

std::unique_ptr<IWindowBackend> WindowBackendFactory::Create(
    WindowBackendType type) {
  switch (type) {
    case WindowBackendType::SDL2:
#ifndef YAZE_USE_SDL3
      return std::make_unique<SDL2WindowBackend>();
#else
      LOG_WARN("WindowBackendFactory",
               "SDL2 backend requested but built with SDL3, using SDL3");
      return std::make_unique<SDL3WindowBackend>();
#endif

    case WindowBackendType::SDL3:
#ifdef YAZE_USE_SDL3
      return std::make_unique<SDL3WindowBackend>();
#else
      LOG_WARN("WindowBackendFactory",
               "SDL3 backend requested but not available, using SDL2");
      return std::make_unique<SDL2WindowBackend>();
#endif

    case WindowBackendType::IOS:
#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
      return std::make_unique<IOSWindowBackend>();
#else
      LOG_WARN("WindowBackendFactory",
               "iOS backend requested on non-iOS platform");
      return nullptr;
#endif

    case WindowBackendType::Null:
      return std::make_unique<NullWindowBackend>();

    case WindowBackendType::Auto:
    default:
      return Create(GetDefaultType());
  }
}

WindowBackendType WindowBackendFactory::GetDefaultType() {
#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
  return WindowBackendType::IOS;
#endif
#ifdef YAZE_USE_SDL3
  return WindowBackendType::SDL3;
#else
  return WindowBackendType::SDL2;
#endif
}

bool WindowBackendFactory::IsAvailable(WindowBackendType type) {
  switch (type) {
    case WindowBackendType::SDL2:
#ifdef YAZE_USE_SDL3
      return false;  // Built with SDL3, SDL2 not available
#else
      return true;
#endif

    case WindowBackendType::SDL3:
#ifdef YAZE_USE_SDL3
      return true;
#else
      return false;  // SDL3 not built in
#endif

    case WindowBackendType::Auto:
      return true;  // Auto always available

    case WindowBackendType::IOS:
#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
      return true;
#else
      return false;
#endif

    case WindowBackendType::Null:
      return true;

    default:
      return false;
  }
}

}  // namespace platform
}  // namespace yaze
