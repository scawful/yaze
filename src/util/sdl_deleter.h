#ifndef YAZE_UTIL_SDL_DELETER_H_
#define YAZE_UTIL_SDL_DELETER_H_

#ifdef YAZE_USE_SDL3
#include <SDL3/SDL.h>
#else
#include "app/platform/sdl_compat.h"
#endif

namespace yaze {
namespace util {

/**
 * @brief Deleter for SDL_Window and SDL_Renderer.
 *
 * Works with both SDL2 and SDL3 as the destroy functions have the same
 * signatures.
 */
struct SDL_Deleter {
  void operator()(SDL_Window* p) const {
    if (p) SDL_DestroyWindow(p);
  }
  void operator()(SDL_Renderer* p) const {
    if (p) SDL_DestroyRenderer(p);
  }
};

/**
 * @brief Custom deleter for SDL_Surface.
 *
 * SDL2: SDL_FreeSurface()
 * SDL3: SDL_DestroySurface()
 */
struct SDL_Surface_Deleter {
  void operator()(SDL_Surface* p) const {
    if (p) {
#ifdef YAZE_USE_SDL3
      SDL_DestroySurface(p);
#else
      SDL_FreeSurface(p);
#endif
    }
  }
};

/**
 * @brief Custom deleter for SDL_Texture.
 *
 * Works with both SDL2 and SDL3 as SDL_DestroyTexture has the same signature.
 */
struct SDL_Texture_Deleter {
  void operator()(SDL_Texture* p) const {
    if (p) {
      SDL_DestroyTexture(p);
    }
  }
};

}  // namespace util
}  // namespace yaze

#endif  // YAZE_UTIL_SDL_DELETER_H_
