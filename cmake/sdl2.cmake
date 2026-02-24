# Legacy SDL2 compatibility wrapper.
#
# Historical builds included this file directly and expected SDL_TARGETS and
# SDL2_INCLUDE_DIRS variables. The canonical path is now:
#   cmake/dependencies/window_backend.cmake
# which resolves SDL2/SDL3 and exports YAZE_WINDOW_TARGETS.

include(cmake/dependencies/sdl2.cmake)

# Legacy variable compatibility for older includes.
set(SDL_TARGETS ${YAZE_SDL2_TARGETS})
set(SDL2_INCLUDE_DIRS "")

message(STATUS "cmake/sdl2.cmake compatibility shim loaded (use dependencies/window_backend.cmake)")
