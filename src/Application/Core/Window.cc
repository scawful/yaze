#include "Window.h"

namespace yaze {
namespace Application {
namespace Core {

// TODO: pass in the size of the window as argument
void Window::Create() {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER)) {
    SDL_Log("SDL_Init: %s\n", SDL_GetError());
  } else {
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

    SDL_WindowFlags window_flags =
        (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                          SDL_WINDOW_ALLOW_HIGHDPI);
    window = SDL_CreateWindow("Yet Another Zelda3 Editor",  // window title
                              SDL_WINDOWPOS_CENTERED,  // initial x position
                              SDL_WINDOWPOS_CENTERED,  // initial y position
                              800,                     // width, in pixels
                              600,                     // height, in pixels
                              window_flags             // window flags
    );
  }
}

SDL_Window* Window::Get() { return window; }

void Window::Swap() { SDL_GL_SwapWindow(window); }

void Window::Destroy() {
  SDL_DestroyWindow(window);
  window = nullptr;
}

}  // namespace Core
}  // namespace Application
}  // namespace yaze