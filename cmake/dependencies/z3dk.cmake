# z3dk-core integration (experimental, opt-in)
#
# z3dk (https://github.com/scawful/z3dk) ships a C++20 static library
# `z3dk-core` that wraps Asar with structured diagnostics, a linter,
# the SNES knowledge base, and .mlb symbol export. This file brings the
# library into yaze's build when YAZE_ENABLE_Z3DK=ON.
#
# Location resolution order:
#   1. Z3DK_SRC_DIR cache var (user override)
#   2. ${CMAKE_SOURCE_DIR}/ext/z3dk  (git submodule, if present)
#   3. ${CMAKE_SOURCE_DIR}/../z3dk   (sibling checkout — default dev path)
#
# Policy for yaze 0.8.0: we build against a sibling z3dk checkout rather
# than vendoring it at ext/z3dk. This keeps the yaze repo slim during the
# incubation period and lets both projects iterate independently. Once the
# ABI stabilizes we'll re-evaluate the submodule option; until then, a
# matching `git clone` of z3dk next to yaze/ is the expected setup.
#
# If no z3dk source tree is found, we emit a warning and leave
# YAZE_Z3DK_FOUND=FALSE so the rest of the build skips integration code
# gated on that variable.

if(Z3DK_CMAKE_INCLUDED)
  return()
endif()
set(Z3DK_CMAKE_INCLUDED TRUE)

set(YAZE_Z3DK_FOUND FALSE CACHE BOOL "z3dk-core static library resolved" FORCE)

# 1. Allow user override via -DZ3DK_SRC_DIR=...
if(DEFINED Z3DK_SRC_DIR AND EXISTS "${Z3DK_SRC_DIR}/src/z3dk_core/CMakeLists.txt")
  set(_z3dk_src "${Z3DK_SRC_DIR}")
# 2. Submodule location
elseif(EXISTS "${CMAKE_SOURCE_DIR}/ext/z3dk/src/z3dk_core/CMakeLists.txt")
  set(_z3dk_src "${CMAKE_SOURCE_DIR}/ext/z3dk")
# 3. Sibling checkout (dev convenience)
elseif(EXISTS "${CMAKE_SOURCE_DIR}/../z3dk/src/z3dk_core/CMakeLists.txt")
  set(_z3dk_src "${CMAKE_SOURCE_DIR}/../z3dk")
else()
  # FATAL_ERROR here (not WARNING): YAZE_ENABLE_Z3DK=ON turns ASAR off via
  # the mutex in cmake/options-simple.cmake, so falling through to
  # "z3dk disabled" would leave the build with NO assembler backend at
  # all — patches would silently fail at runtime. Fail loudly at
  # configure time instead.
  message(FATAL_ERROR
    "YAZE_ENABLE_Z3DK=ON but no z3dk source tree was found. Looked in:\n"
    "  \${Z3DK_SRC_DIR} (unset or invalid)\n"
    "  ${CMAKE_SOURCE_DIR}/ext/z3dk\n"
    "  ${CMAKE_SOURCE_DIR}/../z3dk\n"
    "Clone z3dk as a sibling (`git clone git@github.com:scawful/z3dk.git "
    "${CMAKE_SOURCE_DIR}/../z3dk`) or pass -DZ3DK_SRC_DIR=/path/to/z3dk. "
    "Set -DYAZE_ENABLE_Z3DK=OFF if you want the vanilla ASAR backend.")
endif()

message(STATUS "z3dk source: ${_z3dk_src}")

# z3dk builds a full suite (z3asm CLI, z3lsp, z3disasm, Asar test
# executables) by default. We only need z3dk-core for embedded assembly —
# suppress the rest to keep configure time and binary footprint down.
set(Z3DK_BUILD_LSP OFF CACHE BOOL "Skip z3lsp when embedded in yaze" FORCE)
set(ASAR_GEN_EXE OFF CACHE BOOL "Skip Asar CLI when embedded in yaze" FORCE)
set(ASAR_GEN_DLL OFF CACHE BOOL "Skip Asar DLL when embedded in yaze" FORCE)
set(ASAR_GEN_EXE_TEST OFF CACHE BOOL "Skip Asar CLI test harness" FORCE)
set(ASAR_GEN_DLL_TEST OFF CACHE BOOL "Skip Asar DLL test harness" FORCE)

# Pull z3dk into the build as a subproject. EXCLUDE_FROM_ALL keeps the
# CLI executables out of 'all' — z3dk-core is the only target we actually
# link.
add_subdirectory(${_z3dk_src} ${CMAKE_BINARY_DIR}/z3dk EXCLUDE_FROM_ALL)

if(TARGET z3dk-core)
  # Force C++20 (z3dk requires it, yaze runs C++23 globally; this
  # localizes the standard to the z3dk subtree).
  set_target_properties(z3dk-core PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
  )

  add_library(yaze::z3dk ALIAS z3dk-core)
  set(YAZE_Z3DK_FOUND TRUE CACHE BOOL "z3dk-core static library resolved" FORCE)
  set(YAZE_Z3DK_TARGETS yaze::z3dk CACHE STRING "z3dk link targets" FORCE)

  message(STATUS "z3dk-core integration configured (experimental)")
else()
  message(WARNING
    "z3dk subproject added but z3dk-core target is not defined. "
    "Check that ${_z3dk_src}/src/z3dk_core/CMakeLists.txt built correctly.")
endif()

# Convenience: yaze targets that opt into z3dk support add the YAZE_WITH_Z3DK
# compile definition so their source can #ifdef on it.
function(yaze_add_z3dk_support target_name)
  if(YAZE_Z3DK_FOUND)
    # Library linkage stays PRIVATE — only z3dk_wrapper.cc (inside core_lib)
    # pulls z3dk headers. Consumers down the dep chain don't need them.
    target_link_libraries(${target_name} PRIVATE yaze::z3dk)
    # The compile definition, however, must be visible to editor code so it
    # can #ifdef on YAZE_WITH_Z3DK. PUBLIC propagates it through the dep
    # graph (yaze_core_lib -> yaze_app_core_lib -> yaze_editor).
    target_compile_definitions(${target_name} PUBLIC YAZE_WITH_Z3DK=1)
  endif()
endfunction()
