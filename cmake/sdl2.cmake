# SDL2
# On Windows with vcpkg, prefer vcpkg packages for faster builds
if(WIN32)
  # Disable pkgconfig for SDL on Windows (prevents MSYS2 download failures in vcpkg)
  set(SDL_PKGCONFIG OFF CACHE BOOL "Disable pkgconfig on Windows" FORCE)
  
  # Try to find SDL2 via vcpkg first if toolchain is available
  if(DEFINED CMAKE_TOOLCHAIN_FILE AND EXISTS "${CMAKE_TOOLCHAIN_FILE}")
    find_package(SDL2 CONFIG QUIET)
    if(SDL2_FOUND OR TARGET SDL2::SDL2)
      # Use vcpkg SDL2
      if(TARGET SDL2::SDL2)
        set(SDL_TARGETS SDL2::SDL2)
        if(TARGET SDL2::SDL2main)
          list(PREPEND SDL_TARGETS SDL2::SDL2main)
        endif()
        list(APPEND SDL_TARGETS ws2_32)
        add_definitions("-DSDL_MAIN_HANDLED")
        message(STATUS "✓ Using vcpkg SDL2")
        
        # Get SDL2 include directories for reference
        get_target_property(SDL2_INCLUDE_DIR SDL2::SDL2 INTERFACE_INCLUDE_DIRECTORIES)
        set(SDL2_INCLUDE_DIRS ${SDL2_INCLUDE_DIR})
        return()
      endif()
    endif()
  endif()
  
  # Fall back to bundled SDL if vcpkg not available or SDL2 not found
  if(EXISTS "${CMAKE_SOURCE_DIR}/ext/SDL/CMakeLists.txt")
    message(STATUS "○ vcpkg SDL2 not found, using bundled SDL2")
    add_subdirectory(ext/SDL)
    set(SDL_TARGETS SDL2-static)
    set(SDL2_INCLUDE_DIR 
      ${CMAKE_SOURCE_DIR}/ext/SDL/include
      ${CMAKE_BINARY_DIR}/ext/SDL/include
      ${CMAKE_BINARY_DIR}/ext/SDL/include-config-${CMAKE_BUILD_TYPE}
    )
    set(SDL2_INCLUDE_DIRS ${SDL2_INCLUDE_DIR})
    if(TARGET SDL2main)
      list(PREPEND SDL_TARGETS SDL2main)
    endif()
    list(APPEND SDL_TARGETS ws2_32)
    add_definitions("-DSDL_MAIN_HANDLED")
  else()
    message(FATAL_ERROR "SDL2 not found via vcpkg and bundled SDL2 not available. Please install via vcpkg or ensure submodules are initialized.")
  endif()
elseif(UNIX OR MINGW)
  # Non-Windows: use bundled SDL
  add_subdirectory(ext/SDL)
  set(SDL_TARGETS SDL2-static)
  set(SDL2_INCLUDE_DIR 
    ${CMAKE_SOURCE_DIR}/ext/SDL/include
    ${CMAKE_BINARY_DIR}/ext/SDL/include
    ${CMAKE_BINARY_DIR}/ext/SDL/include-config-${CMAKE_BUILD_TYPE}
  )
  set(SDL2_INCLUDE_DIRS ${SDL2_INCLUDE_DIR})
  message(STATUS "Using bundled SDL2")
else()
  # Fallback: try to find system SDL
  find_package(SDL2 REQUIRED)
  set(SDL_TARGETS SDL2::SDL2)
  message(STATUS "Using system SDL2")
endif()

# PNG and ZLIB dependencies removed