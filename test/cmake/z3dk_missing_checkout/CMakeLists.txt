# Minimal fixture project used by the Z3dkMissingCheckoutFailsFast ctest.
#
# This project exists solely to include cmake/dependencies/z3dk.cmake under
# conditions where none of the z3dk source-tree candidates resolve — so the
# FATAL_ERROR path fires at configure time. If a future CMake refactor
# downgrades that error back to a warning (or silently succeeds with no
# assembler backend), this fixture catches it before it ships.
#
# Expected invocation (see test/CMakeLists.txt):
#   cmake -S this-dir -B <tmp>
#         -DYAZE_ROOT=<repo>
#         -DZ3DK_SRC_DIR=<path that does not exist>
#         -DYAZE_ENABLE_Z3DK=ON

cmake_minimum_required(VERSION 3.15)
project(z3dk_missing_checkout_fixture NONE)

if(NOT DEFINED YAZE_ROOT)
  message(FATAL_ERROR "YAZE_ROOT must be supplied to the missing-checkout fixture")
endif()

# We don't include it directly in the top level — the fixture's purpose is
# to confirm this include aborts when no z3dk checkout is reachable from
# ${CMAKE_SOURCE_DIR} (this fixture dir) via ext/z3dk, ../z3dk, or
# Z3DK_SRC_DIR. The test driver sets Z3DK_SRC_DIR to a missing path, and the
# fixture dir has no siblings/submodules named z3dk, so all three lookups
# must fail.
include("${YAZE_ROOT}/cmake/dependencies/z3dk.cmake")
