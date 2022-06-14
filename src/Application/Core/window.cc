#include "window.h"

#include <SDL2/SDL.h>

namespace yaze {
namespace Application {
namespace Core {

// TODO: pass in the size of the window as argument
void Window::Create() {
  if (SDL_Init(SDL_INIT_EVERYTHING)) {
    SDL_Log("SDL_Init: %s\n", SDL_GetError());
  } else {
    window = SDL_CreateWindow("Yet Another Zelda3 Editor",  // window title
                              SDL_WINDOWPOS_UNDEFINED,  // initial x position
                              SDL_WINDOWPOS_UNDEFINED,  // initial y position
                              1200,                     // width, in pixels
                              800,                      // height, in pixels
                              SDL_WINDOW_RESIZABLE      // window flags
    );
  }
}

SDL_Window* Window::Get() { return window; }

void Window::Destroy() {
  SDL_DestroyWindow(window);
  window = nullptr;
}

}  // namespace Core
}  // namespace Application
}  // namespace yaze