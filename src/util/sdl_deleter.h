#ifndef YAZE_UTIL_SDL_DELETER_H_
#define YAZE_UTIL_SDL_DELETER_H_

#include <SDL.h>

namespace yaze {
namespace util {

/**
 * @brief Deleter for SDL_Window and SDL_Renderer.
 */
struct SDL_Deleter {
  void operator()(SDL_Window* p) const { SDL_DestroyWindow(p); }
  void operator()(SDL_Renderer* p) const { SDL_DestroyRenderer(p); }
};

// Custom deleter for SDL_Surface
struct SDL_Surface_Deleter {
  void operator()(SDL_Surface* p) const {
    if (p) {
      SDL_FreeSurface(p);
    }
  }
};

// Custom deleter for SDL_Texture
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
