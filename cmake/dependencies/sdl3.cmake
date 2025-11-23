# SDL3 dependency management
# Uses CPM.cmake for consistent cross-platform builds

include(cmake/CPM.cmake)
include(cmake/dependencies.lock)

message(STATUS "Setting up SDL3 (experimental) with CPM.cmake")

# SDL3 specific version (using latest stable 3.2 release)
set(SDL3_VERSION "3.2.26")

# Try to use system packages first if requested
if(YAZE_USE_SYSTEM_DEPS)
  find_package(SDL3 QUIET)
  if(SDL3_FOUND)
    message(STATUS "Using system SDL3")
    if(NOT TARGET yaze_sdl3)
      add_library(yaze_sdl3 INTERFACE)
      target_link_libraries(yaze_sdl3 INTERFACE SDL3::SDL3)
      if(TARGET SDL3::SDL3main)
        target_link_libraries(yaze_sdl3 INTERFACE SDL3::SDL3main)
      endif()
    endif()
    set(YAZE_SDL3_TARGETS yaze_sdl3 CACHE INTERNAL "")
    return()
  endif()
endif()

# Use CPM to fetch SDL3
CPMAddPackage(
  NAME SDL3
  VERSION ${SDL3_VERSION}
  GITHUB_REPOSITORY libsdl-org/SDL
  GIT_TAG release-${SDL3_VERSION}
  OPTIONS
    "SDL_SHARED OFF"
    "SDL_STATIC ON"
    "SDL_TEST OFF"
    "SDL_INSTALL OFF"
    "SDL_CMAKE_DEBUG_POSTFIX d"
    "SDL3_DISABLE_INSTALL ON"
    "SDL3_DISABLE_UNINSTALL ON"
)

# Verify SDL3 targets are available
if(NOT TARGET SDL3-static AND NOT TARGET SDL3::SDL3-static AND NOT TARGET SDL3::SDL3)
  message(FATAL_ERROR "SDL3 target not found after CPM fetch")
endif()

# Create convenience targets for the rest of the project
if(NOT TARGET yaze_sdl3)
  add_library(yaze_sdl3 INTERFACE)
  # SDL3 from CPM might use SDL3-static or SDL3::SDL3-static
  if(TARGET SDL3-static)
    message(STATUS "Using SDL3-static target")
    target_link_libraries(yaze_sdl3 INTERFACE SDL3-static)
    # Also explicitly add include directories if they exist
    if(SDL3_SOURCE_DIR)
      target_include_directories(yaze_sdl3 INTERFACE ${SDL3_SOURCE_DIR}/include)
      message(STATUS "Added SDL3 include: ${SDL3_SOURCE_DIR}/include")
    endif()
  elseif(TARGET SDL3::SDL3-static)
    message(STATUS "Using SDL3::SDL3-static target")
    target_link_libraries(yaze_sdl3 INTERFACE SDL3::SDL3-static)
    # For local Homebrew SDL3, also add include path explicitly
    if(APPLE AND EXISTS "/opt/homebrew/opt/sdl3/include/SDL3")
      target_include_directories(yaze_sdl3 INTERFACE /opt/homebrew/opt/sdl3/include/SDL3)
      message(STATUS "Added Homebrew SDL3 include path: /opt/homebrew/opt/sdl3/include/SDL3")
    endif()
  else()
    message(STATUS "Using SDL3::SDL3 target")
    target_link_libraries(yaze_sdl3 INTERFACE SDL3::SDL3)
  endif()
endif()

# Add platform-specific libraries
if(WIN32)
  target_link_libraries(yaze_sdl3 INTERFACE
    winmm
    imm32
    version
    setupapi
    wbemuuid
  )
  target_compile_definitions(yaze_sdl3 INTERFACE SDL_MAIN_HANDLED)
elseif(APPLE)
  target_link_libraries(yaze_sdl3 INTERFACE
    "-framework Cocoa"
    "-framework IOKit"
    "-framework CoreVideo"
    "-framework CoreHaptics"
    "-framework ForceFeedback"
    "-framework GameController"
  )
  target_compile_definitions(yaze_sdl3 INTERFACE SDL_MAIN_HANDLED)
elseif(UNIX)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
  target_link_libraries(yaze_sdl3 INTERFACE ${GTK3_LIBRARIES})
  target_include_directories(yaze_sdl3 INTERFACE ${GTK3_INCLUDE_DIRS})
  target_compile_options(yaze_sdl3 INTERFACE ${GTK3_CFLAGS_OTHER})
endif()

# Export SDL3 targets for use in other CMake files
set(YAZE_SDL3_TARGETS yaze_sdl3)

# Set a flag to indicate SDL3 is being used
set(YAZE_SDL2_TARGETS ${YAZE_SDL3_TARGETS})  # For compatibility with existing code

message(STATUS "SDL3 setup complete - YAZE_SDL3_TARGETS = ${YAZE_SDL3_TARGETS}")