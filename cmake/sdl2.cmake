# SDL2
if (UNIX OR MINGW OR WIN32)
  add_subdirectory(src/lib/SDL)
  # When using bundled SDL, use the static target and set include directories
  set(SDL_TARGETS SDL2-static)
  set(SDL2_INCLUDE_DIR 
    ${CMAKE_SOURCE_DIR}/src/lib/SDL/include
    ${CMAKE_BINARY_DIR}/src/lib/SDL/include
    ${CMAKE_BINARY_DIR}/src/lib/SDL/include-config-${CMAKE_BUILD_TYPE}
  )
  # Also set for consistency with bundled SDL
  set(SDL2_INCLUDE_DIRS ${SDL2_INCLUDE_DIR})
  
  # Add SDL2main for Windows builds when using bundled SDL
  if(WIN32)
    list(PREPEND SDL_TARGETS SDL2main ws2_32)
    add_definitions("-DSDL_MAIN_HANDLED")
  endif()
else()
  find_package(SDL2)
  # When using system SDL, use the imported targets
  set(SDL_TARGETS SDL2::SDL2)
  if(WIN32)
    list(PREPEND SDL_TARGETS SDL2::SDL2main ws2_32)
    add_definitions("-DSDL_MAIN_HANDLED")
  endif()
endif()

# PNG and ZLIB dependencies removed