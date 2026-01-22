#ifndef YAZE_APP_GFX_BACKEND_RENDERER_FACTORY_H_
#define YAZE_APP_GFX_BACKEND_RENDERER_FACTORY_H_

#include <memory>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#include "app/gfx/backend/irenderer.h"
#include "app/gfx/backend/sdl2_renderer.h"

#ifdef YAZE_USE_SDL3
#include "app/gfx/backend/sdl3_renderer.h"
#endif

#if defined(__APPLE__)
#include "app/gfx/backend/metal_renderer.h"
#endif

#include "app/gfx/backend/null_renderer.h"

namespace yaze {
namespace gfx {

/**
 * @enum RendererBackendType
 * @brief Enumeration of available rendering backend types.
 */
enum class RendererBackendType {
  SDL2,        ///< SDL2 renderer backend
  SDL3,        ///< SDL3 renderer backend
  Metal,       ///< Metal renderer backend (Apple platforms)
  Null,        ///< Null renderer for headless/server mode
  kDefault,    ///< Use the default backend based on build configuration
  kAutoDetect  ///< Automatically select the best available backend
};

/**
 * @class RendererFactory
 * @brief Factory class for creating IRenderer instances.
 */
class RendererFactory {
 public:
  static std::unique_ptr<IRenderer> Create(
      RendererBackendType type = RendererBackendType::kDefault) {
    switch (type) {
      case RendererBackendType::SDL2:
#ifndef YAZE_USE_SDL3
        return std::make_unique<SDL2Renderer>();
#else
        return nullptr;
#endif

      case RendererBackendType::SDL3:
#ifdef YAZE_USE_SDL3
        return std::make_unique<SDL3Renderer>();
#else
        return std::make_unique<SDL2Renderer>();
#endif

      case RendererBackendType::Metal:
#if defined(__APPLE__)
        return std::make_unique<MetalRenderer>();
#else
        return nullptr;
#endif

      case RendererBackendType::Null:
        return std::make_unique<NullRenderer>();

      case RendererBackendType::kDefault:
      case RendererBackendType::kAutoDetect:
      default:
#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
        return std::make_unique<MetalRenderer>();
#elif defined(YAZE_USE_SDL3)
        return std::make_unique<SDL3Renderer>();
#else
        return std::make_unique<SDL2Renderer>();
#endif
    }
  }

  static bool IsBackendAvailable(RendererBackendType type) {
    switch (type) {
      case RendererBackendType::SDL2:
        return true;
      case RendererBackendType::SDL3:
#ifdef YAZE_USE_SDL3
        return true;
#else
        return false;
#endif
      case RendererBackendType::Metal:
#if defined(__APPLE__)
        return true;
#else
        return false;
#endif
      case RendererBackendType::Null:
        return true;
      case RendererBackendType::kDefault:
      case RendererBackendType::kAutoDetect:
        return true;
      default:
        return false;
    }
  }

  static const char* GetBackendName(RendererBackendType type) {
    switch (type) {
      case RendererBackendType::SDL2:
        return "SDL2";
      case RendererBackendType::SDL3:
        return "SDL3";
      case RendererBackendType::Metal:
        return "Metal";
      case RendererBackendType::Null:
        return "Null";
      case RendererBackendType::kDefault:
        return "Default";
      case RendererBackendType::kAutoDetect:
        return "AutoDetect";
      default:
        return "Unknown";
    }
  }

  static RendererBackendType GetDefaultBackendType() {
#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
    return RendererBackendType::Metal;
#elif defined(YAZE_USE_SDL3)
    return RendererBackendType::SDL3;
#else
    return RendererBackendType::SDL2;
#endif
  }
};

}  // namespace gfx
}  // namespace yaze

#endif  // YAZE_APP_GFX_BACKEND_RENDERER_FACTORY_H_
