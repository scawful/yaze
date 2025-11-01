# SDL2 dependency management
# Uses CPM.cmake for consistent cross-platform builds

include(cmake/CPM.cmake)
include(cmake/dependencies.lock)

message(STATUS "Setting up SDL2 ${SDL2_VERSION} with CPM.cmake")

# Try to use system packages first if requested
if(YAZE_USE_SYSTEM_DEPS)
  find_package(SDL2 QUIET)
  if(SDL2_FOUND)
    message(STATUS "Using system SDL2")
    if(NOT TARGET yaze_sdl2)
      add_library(yaze_sdl2 INTERFACE)
      target_link_libraries(yaze_sdl2 INTERFACE SDL2::SDL2)
      if(TARGET SDL2::SDL2main)
        target_link_libraries(yaze_sdl2 INTERFACE SDL2::SDL2main)
      endif()
    endif()
    set(YAZE_SDL2_TARGETS yaze_sdl2 CACHE INTERNAL "")
    return()
  endif()
endif()

# Use CPM to fetch SDL2
CPMAddPackage(
  NAME SDL2
  VERSION ${SDL2_VERSION}
  GITHUB_REPOSITORY libsdl-org/SDL
  GIT_TAG release-${SDL2_VERSION}
  OPTIONS
    "SDL_SHARED OFF"
    "SDL_STATIC ON"
    "SDL_TEST OFF"
    "SDL_INSTALL OFF"
    "SDL_CMAKE_DEBUG_POSTFIX d"
)

# Verify SDL2 targets are available  
if(NOT TARGET SDL2-static AND NOT TARGET SDL2::SDL2-static AND NOT TARGET SDL2::SDL2)
  message(FATAL_ERROR "SDL2 target not found after CPM fetch")
endif()

# Create convenience targets for the rest of the project
if(NOT TARGET yaze_sdl2)
  add_library(yaze_sdl2 INTERFACE)
  # SDL2 from CPM might use SDL2-static or SDL2::SDL2-static
  if(TARGET SDL2-static)
    message(STATUS "Using SDL2-static target")
    target_link_libraries(yaze_sdl2 INTERFACE SDL2-static)
    # Also explicitly add include directories if they exist
    if(SDL2_SOURCE_DIR)
      target_include_directories(yaze_sdl2 INTERFACE ${SDL2_SOURCE_DIR}/include)
      message(STATUS "Added SDL2 include: ${SDL2_SOURCE_DIR}/include")
    endif()
  elseif(TARGET SDL2::SDL2-static)
    message(STATUS "Using SDL2::SDL2-static target")
    target_link_libraries(yaze_sdl2 INTERFACE SDL2::SDL2-static)
    # For local Homebrew SDL2, also add include path explicitly
    # SDL headers are in the SDL2 subdirectory
    if(APPLE AND EXISTS "/opt/homebrew/opt/sdl2/include/SDL2")
      target_include_directories(yaze_sdl2 INTERFACE /opt/homebrew/opt/sdl2/include/SDL2)
      message(STATUS "Added Homebrew SDL2 include path: /opt/homebrew/opt/sdl2/include/SDL2")
    endif()
  else()
    message(STATUS "Using SDL2::SDL2 target")
    target_link_libraries(yaze_sdl2 INTERFACE SDL2::SDL2)
  endif()
endif()

# Add platform-specific libraries
if(WIN32)
  target_link_libraries(yaze_sdl2 INTERFACE
    winmm
    imm32
    version
    setupapi
    wbemuuid
  )
  target_compile_definitions(yaze_sdl2 INTERFACE SDL_MAIN_HANDLED)
elseif(APPLE)
  target_link_libraries(yaze_sdl2 INTERFACE
    "-framework Cocoa"
    "-framework IOKit"
    "-framework CoreVideo"
    "-framework ForceFeedback"
  )
  target_compile_definitions(yaze_sdl2 INTERFACE SDL_MAIN_HANDLED)
elseif(UNIX)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
  target_link_libraries(yaze_sdl2 INTERFACE ${GTK3_LIBRARIES})
  target_include_directories(yaze_sdl2 INTERFACE ${GTK3_INCLUDE_DIRS})
  target_compile_options(yaze_sdl2 INTERFACE ${GTK3_CFLAGS_OTHER})
endif()

# Export SDL2 targets for use in other CMake files
# Use PARENT_SCOPE to set in the calling scope (dependencies.cmake)
set(YAZE_SDL2_TARGETS yaze_sdl2 PARENT_SCOPE)
# Also set locally for use in this file
set(YAZE_SDL2_TARGETS yaze_sdl2)

message(STATUS "SDL2 setup complete - YAZE_SDL2_TARGETS = ${YAZE_SDL2_TARGETS}")
