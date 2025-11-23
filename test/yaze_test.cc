#define SDL_MAIN_HANDLED

// Must define before any ImGui includes
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include <SDL.h>
#include <gtest/gtest.h>

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"
#include "app/controller.h"
#include "app/gfx/backend/sdl2_renderer.h"
#include "app/platform/window.h"
#include "e2e/canvas_selection_test.h"
#include "e2e/dungeon_e2e_tests.h"
#include "e2e/framework_smoke_test.h"
#include "imgui/backends/imgui_impl_sdl2.h"
#include "imgui/backends/imgui_impl_sdlrenderer2.h"
#include "imgui/imgui.h"
#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_engine.h"
#include "imgui_test_engine/imgui_te_ui.h"

// #include "test_editor.h"  // Not used in main

namespace yaze {
namespace test {

// Test execution modes for AI agents and developers
enum class TestMode {
  kAll,                // Run all tests (default)
  kUnit,               // Run only unit tests
  kIntegration,        // Run only integration tests
  kE2E,                // Run only end-to-end tests
  kRomDependent,       // Run ROM-dependent tests only
  kZSCustomOverworld,  // Run ZSCustomOverworld specific tests
  kCore,               // Run core functionality tests
  kGraphics,           // Run graphics-related tests
  kEditor,             // Run editor tests
  kDeprecated,         // Run deprecated tests (for cleanup)
  kSpecific            // Run specific test pattern
};

struct TestConfig {
  TestMode mode = TestMode::kAll;
  std::string test_pattern;
  std::string rom_path = "zelda3.sfc";
  bool verbose = false;
  bool skip_rom_tests = false;
  bool enable_ui_tests = false;
  bool show_gui = false;
  ImGuiTestRunSpeed test_speed = ImGuiTestRunSpeed_Fast;
};

// Parse command line arguments for better AI agent testing support
TestConfig ParseArguments(int argc, char* argv[]) {
  TestConfig config;

  std::cout << "Available options:\n"
            << "  --ui            : Enable UI tests\n"
            << "  --show-gui      : Show GUI during tests\n"
            << "  --fast          : Run tests at max speed (default)\n"
            << "  --normal        : Run tests at watchable speed\n"
            << "  --cinematic     : Run tests in slow-motion with pauses\n"
            << "  --rom=<path>    : Specify ROM file path\n"
            << "  --pattern=<pat> : Run tests matching pattern\n"
            << std::endl;

  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];

    if (arg == "--help" || arg == "-h") {
      std::cout << "YAZE Test Runner - Enhanced for AI Agent Testing\n\n";
      std::cout << "Usage: yaze_test [options] [test_pattern]\n\n";
      std::cout << "Test Modes:\n";
      std::cout << "  --unit              Run unit tests only\n";
      std::cout << "  --integration       Run integration tests only\n";
      std::cout << "  --e2e               Run end-to-end tests only\n";
      std::cout << "  --rom-dependent     Run ROM-dependent tests only\n";
      std::cout << "  --zscustomoverworld Run ZSCustomOverworld tests only\n";
      std::cout << "  --core              Run core functionality tests\n";
      std::cout << "  --graphics          Run graphics tests\n";
      std::cout << "  --editor            Run editor tests\n";
      std::cout << "  --deprecated        Run deprecated tests\n\n";
      std::cout << "Options:\n";
      std::cout << "  --rom-path PATH     Specify ROM path for testing\n";
      std::cout << "  --skip-rom-tests    Skip tests requiring ROM files\n";
      std::cout << "  --enable-ui-tests   Enable UI tests (requires display)\n";
      std::cout << "  --verbose           Enable verbose output\n";
      std::cout << "  --help              Show this help message\n\n";
      std::cout << "Examples:\n";
      std::cout << "  yaze_test --unit --verbose\n";
      std::cout << "  yaze_test --e2e --rom-path my_rom.sfc\n";
      std::cout << "  yaze_test --zscustomoverworld --verbose\n";
      std::cout << "  yaze_test RomTest.*\n";
      exit(0);
    } else if (arg == "--unit") {
      config.mode = TestMode::kUnit;
    } else if (arg == "--integration") {
      config.mode = TestMode::kIntegration;
    } else if (arg == "--e2e") {
      config.mode = TestMode::kE2E;
    } else if (arg == "--rom-dependent") {
      config.mode = TestMode::kRomDependent;
    } else if (arg == "--zscustomoverworld") {
      config.mode = TestMode::kZSCustomOverworld;
    } else if (arg == "--core") {
      config.mode = TestMode::kCore;
    } else if (arg == "--graphics") {
      config.mode = TestMode::kGraphics;
    } else if (arg == "--editor") {
      config.mode = TestMode::kEditor;
    } else if (arg == "--deprecated") {
      config.mode = TestMode::kDeprecated;
    } else if (arg == "--rom-path") {
      if (i + 1 < argc) {
        config.rom_path = argv[++i];
      }
    } else if (arg == "--skip-rom-tests") {
      config.skip_rom_tests = true;
    } else if (arg == "--enable-ui-tests") {
      config.enable_ui_tests = true;
    } else if (arg == "--verbose") {
      config.verbose = true;
    } else if (arg == "--show-gui") {
      config.show_gui = true;
    } else if (arg == "--fast") {
      config.test_speed = ImGuiTestRunSpeed_Fast;
    } else if (arg == "--normal") {
      config.test_speed = ImGuiTestRunSpeed_Normal;
    } else if (arg == "--cinematic") {
      config.test_speed = ImGuiTestRunSpeed_Cinematic;
    } else if (arg == "--ui") {
      config.enable_ui_tests = true;
    } else if (arg.find("--") != 0) {
      // Test pattern (not a flag)
      config.mode = TestMode::kSpecific;
      config.test_pattern = arg;
    }
  }

  return config;
}

// Set up test environment based on configuration
void SetupTestEnvironment(const TestConfig& config) {
  // Set environment variables for tests using SDL's cross-platform function
  if (!config.rom_path.empty()) {
    SDL_setenv("YAZE_TEST_ROM_PATH", config.rom_path.c_str(), 1);
  }

  if (config.skip_rom_tests) {
    SDL_setenv("YAZE_SKIP_ROM_TESTS", "1", 1);
  }

  if (config.enable_ui_tests) {
    SDL_setenv("YAZE_ENABLE_UI_TESTS", "1", 1);
  }

  if (config.verbose) {
    SDL_setenv("YAZE_VERBOSE_TESTS", "1", 1);
  }
}

// Configure Google Test filters based on test mode
void ConfigureTestFilters(const TestConfig& config) {
  std::vector<std::string> filters;

  switch (config.mode) {
    case TestMode::kUnit:
      filters.push_back("UnitTest.*");
      break;
    case TestMode::kIntegration:
      filters.push_back("IntegrationTest.*");
      break;
    case TestMode::kE2E:
      filters.push_back("E2ETest.*");
      break;
    case TestMode::kRomDependent:
      filters.push_back("*RomDependent*");
      break;
    case TestMode::kZSCustomOverworld:
      filters.push_back("*ZSCustomOverworld*");
      break;
    case TestMode::kCore:
      filters.push_back("*Core*");
      filters.push_back("*Asar*");
      filters.push_back("*Rom*");
      break;
    case TestMode::kGraphics:
      filters.push_back("*Graphics*");
      filters.push_back("*Gfx*");
      filters.push_back("*Palette*");
      filters.push_back("*Tile*");
      break;
    case TestMode::kEditor:
      filters.push_back("*Editor*");
      break;
    case TestMode::kDeprecated:
      filters.push_back("*Deprecated*");
      break;
    case TestMode::kSpecific:
      if (!config.test_pattern.empty()) {
        filters.push_back(config.test_pattern);
      }
      break;
    case TestMode::kAll:
    default:
      // No filters - run all tests
      break;
  }

  if (!filters.empty()) {
    std::string filter_string;
    for (size_t i = 0; i < filters.size(); i++) {
      if (i > 0)
        filter_string += ":";
      filter_string += filters[i];
    }

    ::testing::GTEST_FLAG(filter) = filter_string;

    if (config.verbose) {
      std::cout << "Test filter: " << filter_string << std::endl;
    }
  }
}

}  // namespace test
}  // namespace yaze

int main(int argc, char* argv[]) {
  absl::InitializeSymbolizer(argv[0]);

  // Configure failure signal handler to be less aggressive for testing
  absl::FailureSignalHandlerOptions options;
  options.symbolize_stacktrace = true;
  options.use_alternate_stack = false;
  options.alarm_on_failure_secs = false;
  options.call_previous_handler = true;
  options.writerfn = nullptr;
  absl::InstallFailureSignalHandler(options);

  // Initialize SDL to prevent crashes in graphics components
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
    // Continue anyway for tests that don't need graphics
  }

  // Parse command line arguments
  auto config = yaze::test::ParseArguments(argc, argv);

  // Set up test environment
  yaze::test::SetupTestEnvironment(config);

  // Configure test filters
  yaze::test::ConfigureTestFilters(config);

  // Initialize Google Test
  ::testing::InitGoogleTest(&argc, argv);

  if (config.enable_ui_tests) {
#ifdef YAZE_GUI_TEST_TARGET
    // Use the Controller to create window and initialize everything
    // This ensures the yaze UI is properly set up for testing
    yaze::Controller controller;

    // Initialize the controller - this creates window, renderer, ImGui context
    auto init_status = controller.OnEntry();
    if (!init_status.ok()) {
      std::cerr << "Failed to initialize controller: " << init_status.message() << std::endl;
      return 1;
    }

    // Get ImGui IO for additional test-specific configuration
    ImGuiIO& io = ImGui::GetIO();

    // Get SDL renderer for test engine setup
    SDL_Renderer* sdl_renderer =
        static_cast<SDL_Renderer*>(controller.renderer()->GetBackendRenderer());

    // Setup test engine
    ImGuiTestEngine* engine = ImGuiTestEngine_CreateContext();
    ImGuiTestEngineIO& test_io = ImGuiTestEngine_GetIO(engine);
    test_io.ConfigRunSpeed = config.test_speed;  // Use configured speed
    test_io.ConfigVerboseLevel = ImGuiTestVerboseLevel_Info;
    test_io.ConfigVerboseLevelOnError = ImGuiTestVerboseLevel_Debug;

    // Log test speed mode
    const char* speed_name = "Fast";
    if (config.test_speed == ImGuiTestRunSpeed_Normal)
      speed_name = "Normal";
    else if (config.test_speed == ImGuiTestRunSpeed_Cinematic)
      speed_name = "Cinematic";
    std::cout << "Running tests in " << speed_name << " mode" << std::endl;

    // Register E2E tests only for GUI test targets (they have the source files)

    // Framework smoke tests
    ImGuiTest* smoke_test =
        IM_REGISTER_TEST(engine, "E2ETest", "FrameworkSmokeTest");
    smoke_test->TestFunc = E2ETest_FrameworkSmokeTest;

    ImGuiTest* canvas_test =
        IM_REGISTER_TEST(engine, "E2ETest", "CanvasSelectionTest");
    canvas_test->TestFunc = E2ETest_CanvasSelectionTest;
    canvas_test->UserData = &controller;

    // Register all dungeon E2E tests via unified registration function
    // This includes: smoke tests, visual verification, object drawing,
    // canvas interaction, and layer rendering tests (18 total)
    yaze::test::e2e::RegisterDungeonE2ETests(engine, &controller);


    // Queue all registered tests to run automatically
    ImGuiTestEngine_QueueTests(engine, ImGuiTestGroup_Tests, nullptr, 0);

    // Main loop - runs the full yaze UI with test engine overlay
    while (controller.IsActive()) {
      // Process input events via controller
      controller.OnInput();

      // Load/update the yaze UI (draws menus, editors, etc.)
      // This is where the actual application UI gets rendered
      auto load_status = controller.OnLoad();
      if (!load_status.ok()) {
        std::cerr << "OnLoad error: " << load_status.message() << std::endl;
        break;
      }

      // Show the test engine windows as overlay (if requested)
      if (config.show_gui) {
        ImGuiTestEngine_ShowTestEngineWindows(engine, &config.show_gui);
      }

      // Render everything
      controller.DoRender();

      // Run test engine post-swap processing
      ImGuiTestEngine_PostSwap(engine);

      // Check if all tests have completed (auto-exit when done)
      if (ImGuiTestEngine_IsTestQueueEmpty(engine)) {
        // All tests finished, exit
        break;
      }
    }

    // Get test result
    ImGuiTestEngineResultSummary summary;
    ImGuiTestEngine_GetResultSummary(engine, &summary);
    int result = (summary.CountSuccess == summary.CountTested) ? 0 : 1;

    std::cout << "Test Results: " << summary.CountSuccess << "/"
              << summary.CountTested << " passed" << std::endl;

    // Cleanup
    ImGuiTestEngine_DestroyContext(engine);
    controller.OnExit();

    return result;
#else
    std::cerr << "UI tests are not supported in this build configuration." << std::endl;
    return 1;
#endif
  } else {
    // Run tests
    int result = RUN_ALL_TESTS();

    // Cleanup SDL
    SDL_Quit();

    return result;
  }
}