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

    if(YAZE_SUPPRESS_WARNINGS)
        if(MSVC)
            add_compile_options(/w)
        else()
            add_compile_options(-w)
        endif()
        message(STATUS "✓ Warnings suppressed (use -v preset suffix for verbose builds)")
    else()
        message(STATUS "○ Verbose warnings enabled")
    endif()

    # Common interface target for shared settings
    add_library(yaze_common INTERFACE)
    target_compile_features(yaze_common INTERFACE cxx_std_23)

    # Platform-specific definitions
    if(YAZE_PLATFORM_LINUX)
        target_compile_definitions(yaze_common INTERFACE linux stricmp=strcasecmp)
    elseif(YAZE_PLATFORM_MACOS)
        target_compile_definitions(yaze_common INTERFACE MACOS)
    elseif(YAZE_PLATFORM_IOS)
        target_compile_definitions(yaze_common INTERFACE YAZE_IOS)
    elseif(YAZE_PLATFORM_WINDOWS)
        target_compile_definitions(yaze_common INTERFACE WINDOWS)
    endif()

    # Compiler-specific settings
    if(MSVC)
        target_compile_options(yaze_common INTERFACE
            /EHsc
            /W4 /permissive-
            /bigobj
            /utf-8
        )
        target_compile_definitions(yaze_common INTERFACE
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
        target_compile_options(yaze_common INTERFACE
            -Wall -Wextra -Wpedantic
            -Wno-deprecated-declarations
            -Wno-c++23-compat
        )
        target_compile_definitions(yaze_common INTERFACE
            _SILENCE_CXX23_DEPRECATION_WARNING
            _SILENCE_ALL_CXX23_DEPRECATION_WARNINGS
        )
    endif()
endfunction()
