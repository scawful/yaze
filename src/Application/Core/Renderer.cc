#include "Renderer.h"

namespace yaze {
namespace Application {
namespace Core {

void Renderer::Create(SDL_Window* window) {
  if (window == nullptr) {
    SDL_Log("SDL_CreateWindow: %s\n", SDL_GetError());
    SDL_Quit();
  }

  *gl_context_ = SDL_GL_CreateContext(window);
  SDL_GL_MakeCurrent(window, gl_context_);
  SDL_GL_SetSwapInterval(1);  // Enable vsync

  // Create the ImGui
  ImGui::CreateContext();

  // Initialize ImGui for SDL
  ImGui_ImplSDL2_InitForOpenGL(window, gl_context_);
  ImGui_ImplOpenGL2_Init();

  // Build a new ImGui frame
  ImGui_ImplOpenGL2_NewFrame();
  ImGui_ImplSDL2_NewFrame();
}

void Renderer::Render() {
  const ImGuiIO& io = ImGui::GetIO();
  ImGui::Render();
  glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
  glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w,
               clear_color.z * clear_color.w, clear_color.w);
  glClear(GL_COLOR_BUFFER_BIT);
  ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
}

void Renderer::Destroy() {
  SDL_GL_DeleteContext(gl_context_);
  gl_context_ = nullptr;
}

}  // namespace Core
}  // namespace Application
}  // namespace yaze