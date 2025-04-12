#ifndef YAZE_APP_CORE_UTILS_SDL_DELETER_H_
#define YAZE_APP_CORE_UTILS_SDL_DELETER_H_

#include <SDL.h>

#include "app/core/platform/memory_tracker.h"

namespace yaze {
namespace core {

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
    if (p && !MemoryTracker::GetInstance().IsFreed(p)) {
      MemoryTracker::GetInstance().TrackDeallocation(p);
      SDL_FreeSurface(p);
    }
  }
};

// Custom deleter for SDL_Texture
struct SDL_Texture_Deleter {
  void operator()(SDL_Texture* p) const {
    if (p && !MemoryTracker::GetInstance().IsFreed(p)) {
      MemoryTracker::GetInstance().TrackDeallocation(p);
      SDL_DestroyTexture(p);
    }
  }
};

}  // namespace core
}  // namespace yaze

#endif  // YAZE_APP_CORE_UTILS_SDL_DELETER_H_
