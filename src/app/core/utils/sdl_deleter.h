#ifndef YAZE_APP_CORE_UTILS_SDL_DELETER_H_
#define YAZE_APP_CORE_UTILS_SDL_DELETER_H_

#include <SDL.h>

namespace yaze {
namespace app {
namespace core {

struct SDL_Deleter {
  void operator()(SDL_Window *p) const { SDL_DestroyWindow(p); }
  void operator()(SDL_Renderer *p) const { SDL_DestroyRenderer(p); }
  void operator()(SDL_Texture *p) const { SDL_DestroyTexture(p); }
};

}  // namespace core
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_CORE_UTILS_SDL_DELETER_H_