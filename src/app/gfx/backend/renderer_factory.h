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
  kDefault,    ///< Use the default backend based on build configuration
  kAutoDetect  ///< Automatically select the best available backend
};

/**
 * @class RendererFactory
 * @brief Factory class for creating IRenderer instances.
 *
 * This factory provides a centralized way to create renderer instances
 * based on the desired backend type. It abstracts away the concrete
 * renderer implementations, allowing the application to be configured
 * for different SDL versions at compile time or runtime.
 *
 * Usage:
 * @code
 *   // Create with default backend (based on build configuration)
 *   auto renderer = RendererFactory::Create();
 *
 *   // Create with specific backend
 *   auto renderer = RendererFactory::Create(RendererBackendType::SDL2);
 * @endcode
 */
class RendererFactory {
 public:
  /**
   * @brief Create a renderer instance with the specified backend type.
   *
   * @param type The desired backend type. If kDefault or kAutoDetect,
   *             the factory will use the backend based on build configuration
   *             (SDL3 if YAZE_USE_SDL3 is defined, SDL2 otherwise).
   * @return A unique pointer to the created IRenderer instance.
   *         Returns nullptr if the requested backend is not available.
   */
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
        // SDL3 not available in this build, fall back to SDL2
        return std::make_unique<SDL2Renderer>();
#endif

      case RendererBackendType::Metal:
#if defined(__APPLE__)
        return std::make_unique<MetalRenderer>();
#else
        return nullptr;
#endif

      case RendererBackendType::kDefault:
      case RendererBackendType::kAutoDetect:
      default:
        // Use the default backend based on build configuration
#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
        return std::make_unique<MetalRenderer>();
#elif defined(YAZE_USE_SDL3)
        return std::make_unique<SDL3Renderer>();
#else
        return std::make_unique<SDL2Renderer>();
#endif
    }
  }

  /**
   * @brief Check if a specific backend type is available in this build.
   *
   * @param type The backend type to check.
   * @return true if the backend is available, false otherwise.
   */
  static bool IsBackendAvailable(RendererBackendType type) {
    switch (type) {
      case RendererBackendType::SDL2:
        // SDL2 is always available (base requirement)
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

      case RendererBackendType::kDefault:
      case RendererBackendType::kAutoDetect:
        // Default/auto-detect is always available
        return true;

      default:
        return false;
    }
  }

  /**
   * @brief Get a string name for a backend type.
   *
   * @param type The backend type.
   * @return A human-readable name for the backend.
   */
  static const char* GetBackendName(RendererBackendType type) {
    switch (type) {
      case RendererBackendType::SDL2:
        return "SDL2";
      case RendererBackendType::SDL3:
        return "SDL3";
      case RendererBackendType::Metal:
        return "Metal";
      case RendererBackendType::kDefault:
        return "Default";
      case RendererBackendType::kAutoDetect:
        return "AutoDetect";
      default:
        return "Unknown";
    }
  }

  /**
   * @brief Get the default backend type for this build.
   *
   * @return The default backend type based on build configuration.
   */
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
