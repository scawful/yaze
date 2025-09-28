# SDL2
# On Windows, try to use vcpkg first, then fall back to bundled SDL
if(WIN32)
  # Try to find SDL2 via vcpkg first
  find_package(SDL2 QUIET)
  if(SDL2_FOUND)
    # Use vcpkg SDL2
    set(SDL_TARGETS SDL2::SDL2)
    list(PREPEND SDL_TARGETS SDL2::SDL2main ws2_32)
    add_definitions("-DSDL_MAIN_HANDLED")
    message(STATUS "Using vcpkg SDL2")
  else()
    # Fall back to bundled SDL
    add_subdirectory(src/lib/SDL)
    set(SDL_TARGETS SDL2-static)
    set(SDL2_INCLUDE_DIR 
      ${CMAKE_SOURCE_DIR}/src/lib/SDL/include
      ${CMAKE_BINARY_DIR}/src/lib/SDL/include
      ${CMAKE_BINARY_DIR}/src/lib/SDL/include-config-${CMAKE_BUILD_TYPE}
    )
    set(SDL2_INCLUDE_DIRS ${SDL2_INCLUDE_DIR})
    list(PREPEND SDL_TARGETS SDL2main ws2_32)
    add_definitions("-DSDL_MAIN_HANDLED")
    message(STATUS "Using bundled SDL2")
  endif()
elseif(UNIX OR MINGW)
  # Non-Windows: use bundled SDL
  add_subdirectory(src/lib/SDL)
  set(SDL_TARGETS SDL2-static)
  set(SDL2_INCLUDE_DIR 
    ${CMAKE_SOURCE_DIR}/src/lib/SDL/include
    ${CMAKE_BINARY_DIR}/src/lib/SDL/include
    ${CMAKE_BINARY_DIR}/src/lib/SDL/include-config-${CMAKE_BUILD_TYPE}
  )
  set(SDL2_INCLUDE_DIRS ${SDL2_INCLUDE_DIR})
  message(STATUS "Using bundled SDL2")
else()
  # Fallback: try to find system SDL
  find_package(SDL2)
  set(SDL_TARGETS SDL2::SDL2)
  message(STATUS "Using system SDL2")
endif()

# PNG and ZLIB dependencies removed