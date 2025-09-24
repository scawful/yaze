# SDL2
if (UNIX OR MINGW)
  # Try to find SDL2 as a system package first
  find_package(SDL2 QUIET)
  if(NOT SDL2_FOUND)
    # If not found, try to use bundled SDL2
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/lib/SDL/CMakeLists.txt")
      add_subdirectory(src/lib/SDL)
    else()
      message(FATAL_ERROR "SDL2 not found and no bundled SDL2 available. Please install libsdl2-dev")
    endif()
  endif()
else()
  find_package(SDL2 REQUIRED)
endif()

set(SDL_TARGETS SDL2::SDL2)

if(WIN32 OR MINGW)
    list(PREPEND SDL_TARGETS SDL2::SDL2main ws2_32)
    add_definitions("-DSDL_MAIN_HANDLED")
endif()

# libpng
find_package(PNG REQUIRED)