# SDL2
if (UNIX OR MINGW)
  add_subdirectory(src/lib/SDL)
else()
  find_package(SDL2)
endif()

set(SDL_TARGETS SDL2::SDL2)

if(WIN32 OR MINGW)
    list(PREPEND SDL_TARGETS SDL2::SDL2main ws2_32)
    add_definitions(-DSDL_MAIN_HANDLED)
endif()

# libpng
if (MINGW)
  set(ZLIB_ROOT ${CMAKE_SOURCE_DIR}/build-windows/src/lib/zlib)
  set(ZLIB_LIBRARY ${CMAKE_SOURCE_DIR}/build-windows/lib/libzlib.dll.a)
  include_directories(${CMAKE_SOURCE_DIR}/src/lib/zlib ${CMAKE_SOURCE_DIR}/src/lib/libpng)
  set(ZLIB_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/src/lib/zlib)
  set(YAZE_BUILD_PYTHON OFF)
  set(YAZE_BUILD_EXTENSIONS OFF)
  add_subdirectory(src/lib/zlib)
  add_subdirectory(src/lib/libpng)
else()
  find_package(PNG REQUIRED)
endif()