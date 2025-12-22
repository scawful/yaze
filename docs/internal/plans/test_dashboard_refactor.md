  YAZE GUI Test Integration Refactoring Plan

  Author: Gemini
  Date: 2025-10-11
  Status: Proposed

  1. Introduction & Motivation

  The yaze application includes a valuable feature for developers: an in-application "Test Dashboard" that allows for
  viewing and running various test suites directly within the GUI. However, the current implementation, located primarily
  in src/app/test/, is tightly coupled with both the main application and the command-line test executables.

  This tight coupling has led to several architectural and practical problems:
   * Conditional Compilation Complexity: Excluding the test dashboard from release or CI/CD builds is difficult, as its code
     is intertwined with core application logic. This unnecessarily bloats release binaries with test code.
   * Circular Dependencies: The yaze_test_support library, which contains the TestManager, links against nearly all other
     application libraries (yaze_editor, yaze_gui, etc.). When the main application also links against yaze_test_support to
     display the dashboard, it creates a confusing and potentially circular dependency graph that complicates the build
     process.
   * Mixed Concerns: The current TestManager is responsible for both the core logic of running tests and the UI logic for
     displaying the dashboard. This violates the Single-Responsibility Principle and makes the code harder to maintain.

  This document proposes a plan to refactor the test integration system into a modular, layered, and conditionally
  compiled architecture.

  2. Goals

   * Decouple Test Infrastructure: Separate the core test framework from the test suites and the GUI dashboard.
   * Create an Optional Test Dashboard: Make the in-app test dashboard a compile-time feature that can be easily enabled for
     development builds and disabled for release builds.
   * Eliminate Complex Dependencies: Remove the need for the main application to link against the entire suite of test
     implementations, simplifying the build graph.
   * Improve Maintainability: Create a clean and logical structure for the test system that is easier to understand and
     extend.

  3. Proposed Architecture

  The test system will be decomposed into three distinct libraries, clearly separating the framework, the UI, and the
  tests themselves.

    1 +-----------------------------------------------------------------+
    2 | Main Application ("yaze")                                       |
    3 | (Conditionally links against test_dashboard)                    |
    4 +-----------------------------------------------------------------+
    5       |                                       ^
    6       | Optionally depends on                 |
    7       v                                       |
    8 +-----------------+     +-----------------+     +-----------------+
    9 | test_dashboard  | --> | test_framework  | <-- | test_suites     |
   10 | (GUI Component) |     | (Core Logic)    |     | (Test Cases)    |
   11 +-----------------+     +-----------------+     +-----------------+
   12       ^                                                 ^
   13       |                                                 |
   14       |-------------------------------------------------|
   15       |
   16       v
   17 +-----------------------------------------------------------------+
   18 | Test Executables (yaze_test_stable, etc.)                       |
   19 | (Link against test_framework and test_suites)                   |
   20 +-----------------------------------------------------------------+

  3.1. test_framework (New Core Library)
   * Location: src/test/framework/
   * Responsibility: Provides the core, non-GUI logic for managing and executing tests.
   * Contents:
       * TestManager (core logic only: RunTests, RegisterTestSuite, GetLastResults, etc.).
       * TestSuite base class and related structs (TestResult, TestResults, etc.).
   * Dependencies: yaze_util, absl. It will not depend on yaze_gui or any specific test suites.

  3.2. test_suites (New Library)
   * Location: src/test/suites/
   * Responsibility: Contains all the actual test implementations.
   * Contents:
       * E2ETestSuite, EmulatorTestSuite, IntegratedTestSuite, RomDependentTestSuite, ZSCustomOverworldTestSuite,
         Z3edAIAgentTestSuite.
   * Dependencies: test_framework, and any yaze libraries required for testing (e.g., yaze_zelda3, yaze_gfx).

  3.3. test_dashboard (New Conditional GUI Library)
   * Location: src/app/gui/testing/
   * Responsibility: Contains all ImGui code for the in-application test dashboard. This library will be conditionally
     compiled and linked.
   * Contents:
       * A new TestDashboard class containing the DrawTestDashboard method (migrated from TestManager).
       * UI-specific logic for displaying results, configuring tests, and interacting with the TestManager.
   * Dependencies: test_framework, yaze_gui.

  4. Migration & Refactoring Plan

   1. Create New Directory Structure:
       * Create src/test/framework/.
       * Create src/test/suites/.
       * Create src/app/gui/testing/.

   2. Split `TestManager`:
       * Move test_manager.h and test_manager.cc to src/test/framework/.
       * Create a new TestDashboard class in src/app/gui/testing/test_dashboard.h/.cc.
       * Move the DrawTestDashboard method and all its UI-related helper functions from TestManager into the new
         TestDashboard class.
       * The TestDashboard will hold a reference to the TestManager singleton to access results and trigger test runs.

   3. Relocate Test Suites:
       * Move all ..._test_suite.h files from src/app/test/ to the new src/test/suites/ directory.
       * Move z3ed_test_suite.cc to src/test/suites/.

   4. Update CMake Configuration:
       * `src/test/framework/CMakeLists.txt`: Create this file to define the yaze_test_framework static library.
       * `src/test/suites/CMakeLists.txt`: Create this file to define the yaze_test_suites static library, linking it
         against yaze_test_framework and other necessary yaze libraries.
       * `src/app/gui/testing/CMakeLists.txt`: Create this file to define the yaze_test_dashboard static library.
       * Root `CMakeLists.txt`: Introduce a new option: option(YAZE_WITH_TEST_DASHBOARD "Build the in-application test
         dashboard" ON).
       * `src/app/app.cmake`: Modify the yaze executable's target_link_libraries to conditionally link yaze_test_dashboard
         based on the YAZE_WITH_TEST_DASHBOARD flag.
       * `test/CMakeLists.txt`: Update the test executables to link against yaze_test_framework and yaze_test_suites.
       * Remove `src/app/test/test.cmake`: The old yaze_test_support library will be completely replaced by this new
         structure.

  5. Expected Outcomes

  This plan will resolve the current architectural issues by:
   * Enabling Clean Builds: Release and CI builds can set YAZE_WITH_TEST_DASHBOARD=OFF, which will prevent the
     test_dashboard and test_suites libraries from being compiled or linked into the final yaze executable, resulting in a
     smaller, cleaner binary.
   * Simplifying Dependencies: The main application will no longer have a convoluted dependency on its own test suites. The
     dependency graph will be clear and acyclic.
   * Improving Developer Experience: Developers can enable the dashboard for convenient in-app testing, while the core test
     infrastructure remains robust and decoupled for command-line execution.