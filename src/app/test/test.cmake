# Testing system components for YAZE

set(YAZE_TEST_CORE_SOURCES
  app/test/test_manager.cc
  app/test/test_manager.h
  app/test/unit_test_suite.h
  app/test/integrated_test_suite.h
  app/test/rom_dependent_test_suite.h
  app/test/e2e_test_suite.h
  app/test/zscustomoverworld_test_suite.h
)

# Add test sources to the main app target if testing is enabled
if(BUILD_TESTING)
  list(APPEND YAZE_APP_SRC ${YAZE_TEST_CORE_SOURCES})
endif()

# Set up test-specific compiler flags and definitions
if(BUILD_TESTING)
  target_compile_definitions(yaze_lib PRIVATE YAZE_ENABLE_TESTING=1)
endif()
