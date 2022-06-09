#ifndef YAZE_APPLICATION_DEBUG_DEBUG_H
#define YAZE_APPLICATION_DEBUG_DEBUG_H

#include "imgui/backends/imgui_impl_sdl.h"
#include "imgui/backends/imgui_impl_sdlrenderer.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/misc/cpp/imgui_stdlib.h"

namespace yaze {
namespace Application {
namespace View {

class Debug {
  public:
    Debug()=default;
    void UpdateScreen() const;
  private:
    ImGuiWindowFlags flags =
      ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar;
};

}
}
}

#endif 