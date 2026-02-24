# Window backend dependency selection.
#
# This file is the single selector for GUI window backend dependencies.
# Current implementations:
#   - SDL2 (stable)
#   - SDL3 (experimental)
# Future scaffold:
#   - GLFW (not yet implemented in runtime/platform layer)

set(YAZE_WINDOW_BACKEND "")
set(YAZE_WINDOW_TARGETS "")

if(YAZE_USE_GLFW)
  set(YAZE_WINDOW_BACKEND "glfw")
  include(cmake/dependencies/glfw.cmake)
  set(YAZE_WINDOW_TARGETS ${YAZE_GLFW_TARGETS})
elseif(YAZE_USE_SDL3)
  set(YAZE_WINDOW_BACKEND "sdl3")
  include(cmake/dependencies/sdl3.cmake)
  set(YAZE_WINDOW_TARGETS ${YAZE_SDL3_TARGETS})
else()
  set(YAZE_WINDOW_BACKEND "sdl2")
  include(cmake/dependencies/sdl2.cmake)
  set(YAZE_WINDOW_TARGETS ${YAZE_SDL2_TARGETS})
endif()

if(NOT YAZE_WINDOW_TARGETS)
  message(FATAL_ERROR
    "No window backend targets configured for backend '${YAZE_WINDOW_BACKEND}'")
endif()

# Compatibility aliases: most of the project still links via YAZE_SDL2_TARGETS.
if(YAZE_WINDOW_BACKEND STREQUAL "sdl3")
  set(YAZE_SDL2_TARGETS ${YAZE_SDL3_TARGETS})
elseif(YAZE_WINDOW_BACKEND STREQUAL "glfw")
  set(YAZE_SDL2_TARGETS ${YAZE_WINDOW_TARGETS})
endif()

set(YAZE_WINDOW_BACKEND ${YAZE_WINDOW_BACKEND} CACHE INTERNAL "Active window backend")
set(YAZE_WINDOW_TARGETS ${YAZE_WINDOW_TARGETS} CACHE INTERNAL "Window backend link targets")

message(STATUS "Window backend selected: ${YAZE_WINDOW_BACKEND}")
