# SDL2
if (UNIX OR MINGW OR WIN32)
  add_subdirectory(src/lib/SDL)
  # When using bundled SDL, use the static target
  set(SDL_TARGETS SDL2-static)
else()
  find_package(SDL2)
  # When using system SDL, use the imported targets
  set(SDL_TARGETS SDL2::SDL2)
  if(WIN32)
    list(PREPEND SDL_TARGETS SDL2::SDL2main ws2_32)
    add_definitions("-DSDL_MAIN_HANDLED")
  endif()
endif()

# libpng and ZLIB dependencies  
if(WIN32 AND NOT YAZE_MINIMAL_BUILD)
  # Use vcpkg on Windows
  find_package(ZLIB REQUIRED)
  find_package(PNG REQUIRED)
elseif(YAZE_MINIMAL_BUILD)
  # For CI builds, try to find but don't require
  find_package(ZLIB QUIET)
  find_package(PNG QUIET)
  if(NOT ZLIB_FOUND OR NOT PNG_FOUND)
    message(STATUS "PNG/ZLIB not found in minimal build, some features may be disabled")
    set(PNG_FOUND FALSE)
    set(PNG_LIBRARIES "")
    set(PNG_INCLUDE_DIRS "")
  endif()
else()
  # Regular builds require these dependencies
  find_package(ZLIB REQUIRED)
  find_package(PNG REQUIRED)
endif()