# GLFW dependency scaffold.
#
# GLFW is not wired into the runtime/platform backends yet.
# This file exists to centralize future integration work:
# - window creation abstraction
# - renderer backend selection
# - ImGui GLFW backend wiring
# - multi-viewport lifecycle integration

set(YAZE_GLFW_TARGETS "")

message(FATAL_ERROR
  "YAZE_USE_GLFW=ON is not implemented yet. "
  "Use SDL2/SDL3 for now. "
  "GLFW integration scaffold exists at cmake/dependencies/glfw.cmake and "
  "cmake/dependencies/window_backend.cmake.")
