#ifndef YAZE_APPLICATION_CORE_WINDOW_H
#define YAZE_APPLICATION_CORE_WINDOW_H

#include <SDL2/SDL.h>

namespace yaze {
namespace Application {
namespace Core {

class Window {
 public:
  void Create();
  SDL_Window* Get();
  void Destroy();

 private:
  SDL_Window* window = nullptr;
};

}  // namespace Core
}  // namespace Application
}  // namespace yaze

#endif