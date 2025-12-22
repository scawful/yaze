#include "cli/handlers/tools/test_cli_commands.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"

namespace yaze::cli {

namespace {

// Test suite definitions
struct TestSuite {
  const char* label;
  const char* description;
  const char* requirements;
  bool requires_rom;
  bool requires_ai;
};

const TestSuite kTestSuites[] = {
    {"stable", "Core unit and integration tests (fast, reliable)", "None",
     false, false},
    {"gui", "GUI smoke tests (ImGui framework validation)",
     "SDL display or headless", false, false},
    {"z3ed", "z3ed CLI self-test and smoke tests", "z3ed target built", false,
     false},
    {"headless_gui", "GUI tests in headless mode (CI-safe)", "None", false,
     false},
    {"rom_dependent", "Tests requiring actual Zelda3 ROM",
     "YAZE_ENABLE_ROM_TESTS=ON + ROM path", true, false},
    {"experimental", "AI runtime features and experiments",
     "YAZE_ENABLE_AI_RUNTIME=ON", false, true},
    {"benchmark", "Performance and optimization tests", "None", false, false},
};

// Execute command and capture output
std::string ExecuteCommand(const std::string& cmd) {
  std::array<char, 256> buffer;
  std::string result;
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"),
                                                 pclose);
  if (!pipe) {
    return "";
  }
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result += buffer.data();
  }
  return result;
}

// Check if a build directory exists
bool BuildDirExists(const std::string& dir) {
  std::string cmd = "test -d " + dir + " && echo yes";
  std::string result = ExecuteCommand(cmd);
  return result.find("yes") != std::string::npos;
}

// Get environment variable or default
std::string GetEnvOrDefault(const char* name, const std::string& default_val) {
  const char* val = std::getenv(name);
  return val ? val : default_val;
}

}  // namespace

absl::Status TestListCommandHandler::Execute(
    Rom* /*rom*/, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto filter_label = parser.GetString("label");
  bool is_json = formatter.IsJson();

  // Output available test suites
  formatter.BeginArray("suites");
  for (const auto& suite : kTestSuites) {
    if (filter_label.has_value() && suite.label != filter_label.value()) {
      continue;
    }

    if (is_json) {
      std::string json = absl::StrFormat(
          R"({"label":"%s","description":"%s","requirements":"%s",)"
          R"("requires_rom":%s,"requires_ai":%s})",
          suite.label, suite.description, suite.requirements,
          suite.requires_rom ? "true" : "false",
          suite.requires_ai ? "true" : "false");
      formatter.AddArrayItem(json);
    } else {
      std::string entry = absl::StrFormat(
          "%s: %s [%s]", suite.label, suite.description, suite.requirements);
      formatter.AddArrayItem(entry);
    }
  }
  formatter.EndArray();

  // Try to get test count from ctest
  std::string build_dir = "build";
  if (!BuildDirExists(build_dir)) {
    build_dir = "build_fast";
  }

  if (BuildDirExists(build_dir)) {
    std::string ctest_cmd =
        "ctest --test-dir " + build_dir + " -N 2>/dev/null | tail -1";
    std::string ctest_output = ExecuteCommand(ctest_cmd);

    // Parse "Total Tests: N"
    if (ctest_output.find("Total Tests:") != std::string::npos) {
      size_t pos = ctest_output.find("Total Tests:");
      std::string count_str = ctest_output.substr(pos + 13);
      int total_tests = std::atoi(count_str.c_str());
      formatter.AddField("total_tests_discovered", total_tests);
    }

    formatter.AddField("build_directory", build_dir);
  } else {
    formatter.AddField("build_directory", "not_found");
    formatter.AddField("note",
                       "Run 'cmake --preset mac-test && cmake --build "
                       "--preset mac-test' to build tests");
  }

  // Text output
  if (!is_json) {
    std::cout << "\n=== Available Test Suites ===\n\n";
    for (const auto& suite : kTestSuites) {
      if (filter_label.has_value() && suite.label != filter_label.value()) {
        continue;
      }
      std::cout << absl::StrFormat("  %-15s %s\n", suite.label,
                                   suite.description);
      std::cout << absl::StrFormat("                  Requirements: %s\n",
                                   suite.requirements);
      if (suite.requires_rom) {
        std::cout << "                  ⚠ Requires ROM file\n";
      }
      if (suite.requires_ai) {
        std::cout << "                  ⚠ Requires AI runtime\n";
      }
      std::cout << "\n";
    }

    std::cout << "=== Quick Commands ===\n\n";
    std::cout << "  ctest --test-dir build -L stable    # Run stable tests\n";
    std::cout << "  ctest --test-dir build -L gui       # Run GUI tests\n";
    std::cout << "  ctest --test-dir build              # Run all tests\n";
    std::cout << "\n";
  }

  return absl::OkStatus();
}

absl::Status TestRunCommandHandler::Execute(
    Rom* /*rom*/, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto label = parser.GetString("label").value_or("stable");
  auto preset = parser.GetString("preset").value_or("");
  bool verbose = parser.HasFlag("verbose");
  bool is_json = formatter.IsJson();

  // Determine build directory
  std::string build_dir = "build";
  if (!preset.empty()) {
    if (preset.find("test") != std::string::npos) {
      build_dir = "build_fast";
    } else if (preset.find("ai") != std::string::npos) {
      build_dir = "build_ai";
    }
  }

  if (!BuildDirExists(build_dir)) {
    return absl::NotFoundError(absl::StrFormat(
        "Build directory '%s' not found. Run cmake to configure first.",
        build_dir));
  }

  formatter.AddField("build_directory", build_dir);
  formatter.AddField("label", label);
  formatter.AddField("preset", preset.empty() ? "default" : preset);

  // Build ctest command
  std::string ctest_cmd = "ctest --test-dir " + build_dir + " -L " + label;
  if (verbose) {
    ctest_cmd += " --output-on-failure";
  }
  ctest_cmd += " 2>&1";

  if (!is_json) {
    std::cout << "\n=== Running Tests ===\n\n";
    std::cout << "Command: " << ctest_cmd << "\n\n";
  }

  // Execute ctest
  std::string output = ExecuteCommand(ctest_cmd);

  // Parse results
  int passed = 0;
  int failed = 0;
  int total = 0;

  // Look for "X tests passed, Y tests failed out of Z"
  // or "100% tests passed, 0 tests failed out of N"
  std::vector<std::string> lines = absl::StrSplit(output, '\n');
  for (const auto& line : lines) {
    if (line.find("tests passed") != std::string::npos) {
      // Parse "X tests passed, Y tests failed out of Z"
      if (line.find("100%") != std::string::npos) {
        // "100% tests passed, 0 tests failed out of N"
        size_t out_of_pos = line.find("out of");
        if (out_of_pos != std::string::npos) {
          total = std::atoi(line.substr(out_of_pos + 7).c_str());
          passed = total;
          failed = 0;
        }
      } else {
        // "X tests passed, Y tests failed out of Z"
        sscanf(line.c_str(), "%d tests passed, %d tests failed out of %d",
               &passed, &failed, &total);
      }
    }
  }

  formatter.AddField("tests_passed", passed);
  formatter.AddField("tests_failed", failed);
  formatter.AddField("tests_total", total);
  formatter.AddField("success", failed == 0 && total > 0);

  if (!is_json) {
    std::cout << output << "\n";
    std::cout << "=== Summary ===\n";
    std::cout << absl::StrFormat("  Passed: %d\n", passed);
    std::cout << absl::StrFormat("  Failed: %d\n", failed);
    std::cout << absl::StrFormat("  Total:  %d\n", total);

    if (failed > 0) {
      std::cout << "\n⚠ Some tests failed. Run with --verbose for details.\n";
    } else if (total > 0) {
      std::cout << "\n✓ All tests passed!\n";
    } else {
      std::cout << "\n⚠ No tests found for label '" << label << "'\n";
    }
  }

  return absl::OkStatus();
}

absl::Status TestStatusCommandHandler::Execute(
    Rom* /*rom*/, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  bool is_json = formatter.IsJson();

  // Check environment variables
  std::string rom_vanilla = GetEnvOrDefault("YAZE_TEST_ROM_VANILLA", "");
  std::string rom_expanded = GetEnvOrDefault("YAZE_TEST_ROM_EXPANDED", "");
  std::string rom_path_legacy = GetEnvOrDefault("YAZE_TEST_ROM_PATH", "");
  if (rom_vanilla.empty()) {
    rom_vanilla = rom_path_legacy;
  }
  std::string skip_rom = GetEnvOrDefault("YAZE_SKIP_ROM_TESTS", "");
  std::string enable_ui = GetEnvOrDefault("YAZE_ENABLE_UI_TESTS", "");

  formatter.AddField("rom_vanilla", rom_vanilla.empty() ? "not set" : rom_vanilla);
  formatter.AddField("rom_expanded", rom_expanded.empty() ? "not set" : rom_expanded);
  formatter.AddField("rom_path", rom_path_legacy.empty() ? "not set" : rom_path_legacy);
  formatter.AddField("skip_rom_tests", !skip_rom.empty());
  formatter.AddField("ui_tests_enabled", !enable_ui.empty());

  // Check available build directories
  std::vector<std::string> build_dirs = {"build",      "build_fast", "build_ai",
                                         "build_test", "build_agent"};

  formatter.BeginArray("build_directories");
  for (const auto& dir : build_dirs) {
    if (BuildDirExists(dir)) {
      formatter.AddArrayItem(dir);
    }
  }
  formatter.EndArray();

  // Determine active preset (heuristic based on build dirs)
  std::string active_preset = "unknown";
  if (BuildDirExists("build_fast")) {
    active_preset = "mac-test (fast)";
  } else if (BuildDirExists("build_ai")) {
    active_preset = "mac-ai";
  } else if (BuildDirExists("build")) {
    active_preset = "mac-dbg (default)";
  }
  formatter.AddField("active_preset", active_preset);

  // Check which test suites are available
  formatter.BeginArray("available_suites");
  for (const auto& suite : kTestSuites) {
    bool available = true;
    if (suite.requires_rom && rom_vanilla.empty()) {
      available = false;
    }
    if (available) {
      formatter.AddArrayItem(suite.label);
    }
  }
  formatter.EndArray();

  // Text output
  if (!is_json) {
    std::cout << "\n";
    std::cout << "╔═══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                    TEST CONFIGURATION                         ║\n";
    std::cout << "╠═══════════════════════════════════════════════════════════════╣\n";
    std::cout << absl::StrFormat(
        "║  ROM Vanilla: %-47s ║\n",
        rom_vanilla.empty() ? "(not set)" : rom_vanilla.substr(0, 47));
    std::cout << absl::StrFormat(
        "║  ROM Expanded: %-46s ║\n",
        rom_expanded.empty() ? "(not set)" : rom_expanded.substr(0, 46));
    std::cout << absl::StrFormat("║  Skip ROM Tests: %-43s ║\n",
                                 skip_rom.empty() ? "NO" : "YES");
    std::cout << absl::StrFormat("║  UI Tests Enabled: %-41s ║\n",
                                 enable_ui.empty() ? "NO" : "YES");
    std::cout << absl::StrFormat("║  Active Preset: %-44s ║\n", active_preset);
    std::cout << "╠═══════════════════════════════════════════════════════════════╣\n";
    std::cout << "║  Available Build Directories:                                 ║\n";
    for (const auto& dir : build_dirs) {
      if (BuildDirExists(dir)) {
        std::cout << absl::StrFormat("║    ✓ %-55s ║\n", dir);
      }
    }
    std::cout << "╠═══════════════════════════════════════════════════════════════╣\n";
    std::cout << "║  Available Test Suites:                                       ║\n";
    for (const auto& suite : kTestSuites) {
      bool available = true;
      std::string reason;
      if (suite.requires_rom && rom_vanilla.empty()) {
        available = false;
        reason = " (needs ROM)";
      }
      if (suite.requires_ai) {
        reason = " (needs AI)";
      }
      std::cout << absl::StrFormat("║    %s %-15s%-40s ║\n",
                                   available ? "✓" : "✗", suite.label, reason);
    }
    std::cout << "╚═══════════════════════════════════════════════════════════════╝\n";

    if (rom_vanilla.empty()) {
      std::cout << "\nTo enable ROM-dependent tests:\n";
      std::cout << "  export YAZE_TEST_ROM_VANILLA=/path/to/alttp_vanilla.sfc\n";
      std::cout << "  cmake ... -DYAZE_ENABLE_ROM_TESTS=ON\n";
    }
  }

  return absl::OkStatus();
}

}  // namespace yaze::cli
