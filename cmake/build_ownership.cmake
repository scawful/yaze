# Reject concurrent raw configure calls before dependency/generated-file writes.
# The agent helper separately holds a lock through configure and compilation.
file(LOCK "${CMAKE_BINARY_DIR}/.yaze-configure.lock"
     GUARD PROCESS TIMEOUT 0 RESULT_VARIABLE YAZE_CONFIGURE_LOCK_RESULT)
if(NOT YAZE_CONFIGURE_LOCK_RESULT STREQUAL "0")
  message(FATAL_ERROR
    "Another configure is using ${CMAKE_BINARY_DIR}; wait for its owner instead of starting a second configure")
endif()
