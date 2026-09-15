# Keep application help out of the machine-readable GoogleTest inventory.
if(NOT EXISTS "${TEST_EXECUTABLE}")
  message(FATAL_ERROR "Test runner does not exist: ${TEST_EXECUTABLE}")
endif()

execute_process(
  COMMAND "${TEST_EXECUTABLE}" --gtest_list_tests
  RESULT_VARIABLE result
  OUTPUT_VARIABLE inventory
  ERROR_VARIABLE errors
  TIMEOUT 20
)
if(NOT "${result}" STREQUAL "0")
  message(FATAL_ERROR "Test discovery failed: ${result}\n${errors}")
endif()
if(inventory MATCHES "Available options:|YAZE Test Runner")
  message(FATAL_ERROR "Test discovery contains application help text")
endif()
if(NOT inventory MATCHES "(^|\n)[^ \t\r\n][^\r\n]*\\.\r?\n  [^\r\n]+")
  message(FATAL_ERROR "Test discovery did not list a GoogleTest suite and test")
endif()

execute_process(
  COMMAND "${TEST_EXECUTABLE}" --help
  RESULT_VARIABLE result
  OUTPUT_VARIABLE help
  ERROR_VARIABLE errors
  TIMEOUT 20
)
if(NOT "${result}" STREQUAL "0" OR NOT help MATCHES "YAZE Test Runner"
   OR NOT help MATCHES "--show-gui")
  message(FATAL_ERROR "Explicit test runner help is unavailable: ${result}\n${errors}")
endif()
