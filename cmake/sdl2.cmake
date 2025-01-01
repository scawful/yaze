# SDL2
if (UNIX OR MINGW)
  add_subdirectory(src/lib/SDL)
else()
  find_package(SDL2)
endif()

set(SDL_TARGETS SDL2::SDL2)

if(WIN32 OR MINGW)
    list(PREPEND SDL_TARGETS SDL2::SDL2main ws2_32)
    add_definitions("-DSDL_MAIN_HANDLED")
endif()

# libpng
find_package(PNG REQUIRED)