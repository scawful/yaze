#ifndef YAZE_APPLICATION_CORE_WINDOW_H
#define YAZE_APPLICATION_CORE_WINDOW_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

namespace yaze {
namespace Application {
namespace Core {

class Window {
 public:
  void Create();
  SDL_Window* Get();
  void Swap();
  void Destroy();

 private:
  SDL_Window* window = nullptr;
};

}  // namespace Core
}  // namespace Application
}  // namespace yaze

#endif