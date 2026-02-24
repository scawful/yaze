# cmake/platform/homebrew.cmake
# Centralized Homebrew detection utilities for macOS builds.
#
# Provides:
#   yaze_homebrew_detect()           - One-time detection of brew + architecture
#   yaze_homebrew_find_package(PKG)  - Find a Homebrew package prefix
#   yaze_homebrew_add_prefix(PKG)    - Find + append to CMAKE_PREFIX_PATH
#
# Cache variables set by yaze_homebrew_detect():
#   YAZE_HOMEBREW_AVAILABLE  - TRUE if brew is found
#   YAZE_HOMEBREW_EXECUTABLE - Path to brew binary
#   YAZE_HOMEBREW_ARCH       - Host architecture (arm64 or x86_64)
#   YAZE_HOMEBREW_ROOT       - Homebrew root (/opt/homebrew or /usr/local)

include_guard(GLOBAL)

# ---------------------------------------------------------------------------
# yaze_homebrew_detect()
# One-time detection of Homebrew and host architecture.
# Safe to call multiple times (uses include_guard + cache).
# ---------------------------------------------------------------------------
macro(yaze_homebrew_detect)
  if(NOT DEFINED YAZE_HOMEBREW_AVAILABLE)
    set(YAZE_HOMEBREW_AVAILABLE FALSE CACHE BOOL "Homebrew detected" FORCE)

    if(NOT APPLE)
      return()
    endif()

    find_program(YAZE_HOMEBREW_EXECUTABLE brew)
    if(NOT YAZE_HOMEBREW_EXECUTABLE)
      message(STATUS "[homebrew] brew not found, skipping Homebrew integration")
      return()
    endif()

    # Detect host architecture
    set(_yaze_hw_arch "${CMAKE_SYSTEM_PROCESSOR}")
    if(NOT _yaze_hw_arch)
      execute_process(
        COMMAND uname -m
        OUTPUT_VARIABLE _yaze_hw_arch
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    endif()
    set(YAZE_HOMEBREW_ARCH "${_yaze_hw_arch}" CACHE STRING "Host architecture" FORCE)

    # Determine Homebrew root based on architecture
    if(YAZE_HOMEBREW_ARCH STREQUAL "arm64")
      set(YAZE_HOMEBREW_ROOT "/opt/homebrew" CACHE PATH "Homebrew root" FORCE)
    else()
      set(YAZE_HOMEBREW_ROOT "/usr/local" CACHE PATH "Homebrew root" FORCE)
    endif()

    set(YAZE_HOMEBREW_AVAILABLE TRUE CACHE BOOL "Homebrew detected" FORCE)
    message(STATUS "[homebrew] Detected: arch=${YAZE_HOMEBREW_ARCH} root=${YAZE_HOMEBREW_ROOT}")
  endif()
endmacro()

# ---------------------------------------------------------------------------
# yaze_homebrew_find_package(<package_name> [RESULT_VAR <var>])
# Queries `brew --prefix <package>` and validates the path exists.
# Falls back to hardcoded candidates if brew query fails.
# Sets <RESULT_VAR> (default: _YAZE_HOMEBREW_<PACKAGE>_PREFIX) to the path
# or empty string if not found.
# ---------------------------------------------------------------------------
function(yaze_homebrew_find_package _pkg)
  cmake_parse_arguments(_arg "" "RESULT_VAR" "" ${ARGN})

  string(TOUPPER "${_pkg}" _pkg_upper)
  string(REPLACE "-" "_" _pkg_upper "${_pkg_upper}")
  if(_arg_RESULT_VAR)
    set(_out_var "${_arg_RESULT_VAR}")
  else()
    set(_out_var "_YAZE_HOMEBREW_${_pkg_upper}_PREFIX")
  endif()

  yaze_homebrew_detect()
  if(NOT YAZE_HOMEBREW_AVAILABLE)
    set(${_out_var} "" PARENT_SCOPE)
    return()
  endif()

  # Try dynamic detection first
  execute_process(
    COMMAND "${YAZE_HOMEBREW_EXECUTABLE}" --prefix "${_pkg}"
    OUTPUT_VARIABLE _brew_prefix
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _brew_result
    ERROR_QUIET)

  if(_brew_result EQUAL 0 AND EXISTS "${_brew_prefix}")
    # On arm64, skip Intel-era paths from brew
    if(YAZE_HOMEBREW_ARCH STREQUAL "arm64" AND NOT _brew_prefix MATCHES "^/opt/homebrew")
      message(STATUS "[homebrew] Skipping ${_pkg} at ${_brew_prefix} (wrong arch)")
    else()
      message(STATUS "[homebrew] Found ${_pkg}: ${_brew_prefix}")
      set(${_out_var} "${_brew_prefix}" PARENT_SCOPE)
      return()
    endif()
  endif()

  # Fallback: check hardcoded candidate matching host architecture only.
  # Do NOT fall back to the other arch's prefix (/usr/local on arm64 or
  # /opt/homebrew on x86_64) as those libraries are the wrong architecture.
  set(_candidates
    "${YAZE_HOMEBREW_ROOT}/opt/${_pkg}")

  foreach(_candidate IN LISTS _candidates)
    if(EXISTS "${_candidate}")
      message(STATUS "[homebrew] Found ${_pkg} (fallback): ${_candidate}")
      set(${_out_var} "${_candidate}" PARENT_SCOPE)
      return()
    endif()
  endforeach()

  message(STATUS "[homebrew] ${_pkg} not found via Homebrew")
  set(${_out_var} "" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# yaze_homebrew_add_prefix(<package_name>)
# Convenience: finds a Homebrew package and appends its prefix to
# CMAKE_PREFIX_PATH if found.
# ---------------------------------------------------------------------------
macro(yaze_homebrew_add_prefix _pkg)
  yaze_homebrew_find_package("${_pkg}" RESULT_VAR _yaze_hb_prefix)
  if(_yaze_hb_prefix)
    list(APPEND CMAKE_PREFIX_PATH "${_yaze_hb_prefix}")
    message(STATUS "[homebrew] Added ${_pkg} to CMAKE_PREFIX_PATH: ${_yaze_hb_prefix}")
  endif()
endmacro()
