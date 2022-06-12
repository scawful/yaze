#ifndef YAZE_APPLICATION_CORE_WINDOW_H
#define YAZE_APPLICATION_CORE_WINDOW_H

#include <SDL2/SDL.h>

namespace yaze {
namespace Application {
namespace Core {

class Window {
 public:
  void Create();
  void Destroy();
  SDL_Window* Get();

 private:
  SDL_Window* window = nullptr;
};

}  // namespace Core
}  // namespace Application
}  // namespace yaze

#endif