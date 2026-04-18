# Driver invoked by ctest. Runs a child CMake configure that must fail with
# the z3dk "no source tree was found" FATAL_ERROR. Passes only if:
#   1. The child configure exits non-zero, AND
#   2. Its output contains the expected FATAL_ERROR phrase.
#
# Required cache vars (supplied by the caller):
#   YAZE_ROOT         — absolute path to the yaze source tree
#   FIXTURE_BIN_DIR   — scratch build dir for the child configure
#
# Run standalone:
#   cmake -DYAZE_ROOT=/repo -DFIXTURE_BIN_DIR=/tmp/z3dk-fx \
#         -P test/cmake/z3dk_missing_checkout_driver.cmake

if(NOT DEFINED YAZE_ROOT OR NOT EXISTS "${YAZE_ROOT}/CMakeLists.txt")
  message(FATAL_ERROR "YAZE_ROOT must point at the yaze source tree (got '${YAZE_ROOT}')")
endif()
if(NOT DEFINED FIXTURE_BIN_DIR)
  message(FATAL_ERROR "FIXTURE_BIN_DIR must be set")
endif()

set(_fixture_src "${YAZE_ROOT}/test/cmake/z3dk_missing_checkout")
set(_fixture_bin "${FIXTURE_BIN_DIR}")

# Clean any previous run so CMakeCache.toml doesn't hide the failure.
if(EXISTS "${_fixture_bin}")
  file(REMOVE_RECURSE "${_fixture_bin}")
endif()
file(MAKE_DIRECTORY "${_fixture_bin}")

# Deliberately-bogus Z3DK_SRC_DIR. Combined with the fixture source dir
# (which has no ext/z3dk and no ../z3dk sibling), this forces the FATAL_ERROR
# branch in cmake/dependencies/z3dk.cmake.
set(_bogus_src_dir "${_fixture_bin}/does-not-exist")

execute_process(
  COMMAND
    "${CMAKE_COMMAND}"
    -S "${_fixture_src}"
    -B "${_fixture_bin}"
    "-DYAZE_ROOT=${YAZE_ROOT}"
    "-DZ3DK_SRC_DIR=${_bogus_src_dir}"
    -DYAZE_ENABLE_Z3DK=ON
  OUTPUT_VARIABLE _out
  ERROR_VARIABLE _err
  RESULT_VARIABLE _rc
)

set(_expected_phrase "YAZE_ENABLE_Z3DK=ON but no z3dk source tree was found")

if(_rc EQUAL 0)
  message(FATAL_ERROR
    "Expected missing-checkout configure to FAIL, but it succeeded.\n"
    "This means cmake/dependencies/z3dk.cmake is no longer failing fast "
    "when YAZE_ENABLE_Z3DK=ON and no checkout is reachable. "
    "A mac-ai-z3dk build would ship with NO assembler backend.\n"
    "--- stdout ---\n${_out}\n--- stderr ---\n${_err}")
endif()

# FATAL_ERROR typically lands in stderr, but some cmake generators echo it to
# stdout too — check both.
set(_combined "${_out}${_err}")
string(FIND "${_combined}" "${_expected_phrase}" _found_at)
if(_found_at EQUAL -1)
  message(FATAL_ERROR
    "Configure failed (expected), but the z3dk-specific FATAL_ERROR message "
    "was not in the output. If the error text was reworded, update both "
    "cmake/dependencies/z3dk.cmake and this driver.\n"
    "Looking for: ${_expected_phrase}\n"
    "--- stdout ---\n${_out}\n--- stderr ---\n${_err}")
endif()

message(STATUS "z3dk missing-checkout fixture PASSED (child exited ${_rc}, message matched)")
