# This file contains utility functions for the yaze build system.

# ============================================================================
# yaze_add_compiler_flags
#
# Sets standard compiler flags for C++ and C.
# Also handles platform-specific and compiler-specific flags.
# ============================================================================
function(yaze_add_compiler_flags)
    # Set C++ and C standards in parent scope
    set(CMAKE_CXX_STANDARD 23 PARENT_SCOPE)
    set(CMAKE_CXX_STANDARD_REQUIRED ON PARENT_SCOPE)
    set(CMAKE_CXX_EXTENSIONS OFF PARENT_SCOPE)
    set(CMAKE_C_STANDARD 99 PARENT_SCOPE)
    set(CMAKE_C_STANDARD_REQUIRED ON PARENT_SCOPE)

    # Split common settings into small interface targets.
    # Keep `yaze_common` as the stable umbrella most targets already link.
    if(NOT TARGET yaze_includes)
        add_library(yaze_includes INTERFACE)
        target_include_directories(yaze_includes INTERFACE
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/inc
            ${CMAKE_BINARY_DIR}
            ${CMAKE_BINARY_DIR}/gens
        )
    endif()

    if(NOT TARGET yaze_options)
        add_library(yaze_options INTERFACE)
        target_compile_features(yaze_options INTERFACE cxx_std_23)
    endif()

    if(NOT TARGET yaze_warnings)
        add_library(yaze_warnings INTERFACE)
    endif()

    if(YAZE_SUPPRESS_WARNINGS)
        if(MSVC)
            target_compile_options(yaze_warnings INTERFACE /w)
        else()
            target_compile_options(yaze_warnings INTERFACE -w)
        endif()
        message(STATUS "✓ Warnings suppressed (use -v preset suffix for verbose builds)")
    else()
        message(STATUS "○ Verbose warnings enabled")
    endif()

    if(NOT TARGET yaze_common)
        add_library(yaze_common INTERFACE)
    endif()
    target_link_libraries(yaze_common INTERFACE yaze_includes yaze_options yaze_warnings)

    # Platform-specific definitions
    if(YAZE_PLATFORM_LINUX)
        target_compile_definitions(yaze_options INTERFACE linux stricmp=strcasecmp)
    elseif(YAZE_PLATFORM_MACOS)
        target_compile_definitions(yaze_options INTERFACE MACOS)
    elseif(YAZE_PLATFORM_IOS)
        target_compile_definitions(yaze_options INTERFACE YAZE_IOS)
    elseif(YAZE_PLATFORM_WINDOWS)
        target_compile_definitions(yaze_options INTERFACE WINDOWS)
    endif()

    # Compiler-specific settings
    if(MSVC)
        target_compile_options(yaze_warnings INTERFACE
            /EHsc
            /W4 /permissive-
            /bigobj
            /utf-8
        )
        target_compile_definitions(yaze_options INTERFACE
            _CRT_SECURE_NO_WARNINGS
            _CRT_NONSTDC_NO_WARNINGS
            SILENCE_CXX23_DEPRECATIONS
            _SILENCE_CXX23_DEPRECATION_WARNING
            _SILENCE_ALL_CXX23_DEPRECATION_WARNINGS
            NOMINMAX
            WIN32_LEAN_AND_MEAN
            strncasecmp=_strnicmp
            strcasecmp=_stricmp
        )
    else()
        target_compile_options(yaze_warnings INTERFACE
            -Wall -Wextra -Wpedantic
            -Wno-deprecated-declarations
            -Wno-c++23-compat
        )
        target_compile_definitions(yaze_options INTERFACE
            _SILENCE_CXX23_DEPRECATION_WARNING
            _SILENCE_ALL_CXX23_DEPRECATION_WARNINGS
        )
    endif()
endfunction()
