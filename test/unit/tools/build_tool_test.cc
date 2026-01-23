/**
 * @file build_tool_test.cc
 * @brief Unit tests for the BuildTool AI agent tool
 *
 * Tests the BuildTool functionality including preset listing, validation,
 * build status tracking, project root detection, and timeout protection.
 */

#include "cli/service/agent/tools/build_tool.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace yaze {
namespace cli {
namespace agent {
namespace tools {
namespace {

using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::SizeIs;

// Test fixture for BuildTool tests
class BuildToolTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create a temporary test directory
    test_dir_ = std::filesystem::temp_directory_path() / "yaze_build_tool_test";
    std::filesystem::create_directories(test_dir_);

    // Create a minimal CMakePresets.json for testing
    CreateTestPresetsFile();

    original_dir_ = std::filesystem::current_path();
    std::filesystem::current_path(test_dir_);
  }

  void TearDown() override {
    std::filesystem::current_path(original_dir_);
    // Clean up test directory
    std::filesystem::remove_all(test_dir_);
  }

  void CreateTestPresetsFile() {
    std::ofstream presets_file(test_dir_ / "CMakePresets.json");
    presets_file << R"({
  "version": 6,
  "configurePresets": [
    {
      "name": "mac-dbg",
      "displayName": "macOS Debug",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug"
      },
      "condition": {
        "type": "equals",
        "lhs": "${hostSystemName}",
        "rhs": "Darwin"
      }
    },
    {
      "name": "mac-ai",
      "displayName": "macOS AI Build",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build_ai",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug"
      },
      "condition": {
        "type": "equals",
        "lhs": "${hostSystemName}",
        "rhs": "Darwin"
      }
    },
    {
      "name": "lin-dbg",
      "displayName": "Linux Debug",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug"
      },
      "condition": {
        "type": "equals",
        "lhs": "${hostSystemName}",
        "rhs": "Linux"
      }
    },
    {
      "name": "lin-ai",
      "displayName": "Linux AI Build",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build_ai",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug"
      },
      "condition": {
        "type": "equals",
        "lhs": "${hostSystemName}",
        "rhs": "Linux"
      }
    },
    {
      "name": "win-dbg",
      "displayName": "Windows Debug",
      "generator": "Visual Studio 17 2022",
      "binaryDir": "${sourceDir}/build",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug"
      },
      "condition": {
        "type": "equals",
        "lhs": "${hostSystemName}",
        "rhs": "Windows"
      }
    },
    {
      "name": "win-ai",
      "displayName": "Windows AI Build",
      "generator": "Visual Studio 17 2022",
      "binaryDir": "${sourceDir}/build_ai",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug"
      },
      "condition": {
        "type": "equals",
        "lhs": "${hostSystemName}",
        "rhs": "Windows"
      }
    }
  ]
})";
    presets_file.close();
  }

  std::filesystem::path test_dir_;
  std::filesystem::path original_dir_;
};

// =============================================================================
// BuildTool Configuration Tests
// =============================================================================

TEST_F(BuildToolTest, DefaultConfigUsesCorrectBuildDirectory) {
  BuildTool::BuildConfig config;
  EXPECT_EQ(config.build_directory, "build");
}

TEST_F(BuildToolTest, DefaultConfigUsesCorrectTimeout) {
  BuildTool::BuildConfig config;
  EXPECT_EQ(config.timeout, std::chrono::seconds(600));
}

TEST_F(BuildToolTest, DefaultConfigEnablesCaptureOutput) {
  BuildTool::BuildConfig config;
  EXPECT_TRUE(config.capture_output);
}

TEST_F(BuildToolTest, DefaultConfigUsesCorrectMaxOutputSize) {
  BuildTool::BuildConfig config;
  EXPECT_EQ(config.max_output_size, 1024 * 1024);  // 1MB
}

// =============================================================================
// BuildTool Preset Tests
// =============================================================================

TEST_F(BuildToolTest, ListAvailablePresetsNotEmpty) {
  BuildTool tool;
  auto presets = tool.ListAvailablePresets();

  // At least some presets should be available on any platform
  // The actual presets depend on the project's CMakePresets.json
  // This test verifies the mechanism works
  EXPECT_THAT(presets, Not(IsEmpty()));
}

TEST_F(BuildToolTest, ListAvailablePresetsContainsPlatformSpecificPresets) {
  BuildTool tool;
  auto presets = tool.ListAvailablePresets();

#if defined(__APPLE__)
  // On macOS, we should have mac-* presets
  bool has_mac_preset = false;
  for (const auto& preset : presets) {
    if (preset.find("mac") != std::string::npos) {
      has_mac_preset = true;
      break;
    }
  }
  EXPECT_TRUE(has_mac_preset) << "Expected mac-* preset on macOS";
#elif defined(__linux__)
  // On Linux, we should have lin-* presets
  bool has_lin_preset = false;
  for (const auto& preset : presets) {
    if (preset.find("lin") != std::string::npos) {
      has_lin_preset = true;
      break;
    }
  }
  EXPECT_TRUE(has_lin_preset) << "Expected lin-* preset on Linux";
#elif defined(_WIN32)
  // On Windows, we should have win-* presets
  bool has_win_preset = false;
  for (const auto& preset : presets) {
    if (preset.find("win") != std::string::npos) {
      has_win_preset = true;
      break;
    }
  }
  EXPECT_TRUE(has_win_preset) << "Expected win-* preset on Windows";
#endif
}

// =============================================================================
// BuildTool Status Tests
// =============================================================================

TEST_F(BuildToolTest, InitialBuildStatusNotRunning) {
  BuildTool tool;
  auto status = tool.GetBuildStatus();

  EXPECT_FALSE(status.is_running);
  EXPECT_TRUE(status.current_operation.empty());
  EXPECT_EQ(status.progress_percent, -1);  // Unknown progress
}

TEST_F(BuildToolTest, BuildStatusTrackingDuringOperation) {
  BuildTool tool;

  // Get initial status
  auto initial_status = tool.GetBuildStatus();
  EXPECT_FALSE(initial_status.is_running);

  // Note: We don't actually start a build here since it would require
  // a properly configured build environment. This test verifies the
  // status tracking interface is accessible.
}

TEST_F(BuildToolTest, GetLastResultInitiallyEmpty) {
  BuildTool tool;
  auto last_result = tool.GetLastResult();

  EXPECT_FALSE(last_result.has_value());
}

// =============================================================================
// BuildTool Build Directory Tests
// =============================================================================

TEST_F(BuildToolTest, IsBuildDirectoryReadyInitiallyFalse) {
  BuildTool::BuildConfig config;
  config.build_directory = (test_dir_ / "nonexistent_build").string();

  BuildTool tool(config);

  EXPECT_FALSE(tool.IsBuildDirectoryReady());
}

TEST_F(BuildToolTest, IsBuildDirectoryReadyAfterCreation) {
  BuildTool::BuildConfig config;
  auto build_dir = test_dir_ / "test_build";
  std::filesystem::create_directories(build_dir);
  config.build_directory = build_dir.string();

  BuildTool tool(config);

  // Note: Just having the directory doesn't mean it's "ready" (configured)
  // A real build directory would have CMakeCache.txt
  EXPECT_FALSE(tool.IsBuildDirectoryReady());

  // Create a minimal CMakeCache.txt to simulate a configured build
  std::ofstream cache_file(build_dir / "CMakeCache.txt");
  cache_file << "# Minimal CMake cache for testing\n";
  cache_file.close();

  // Now it should be ready
  EXPECT_TRUE(tool.IsBuildDirectoryReady());
}

// =============================================================================
// BuildResult Structure Tests
// =============================================================================

TEST_F(BuildToolTest, BuildResultStructureContainsExpectedFields) {
  BuildTool::BuildResult result;

  // Verify default values
  EXPECT_FALSE(result.success);
  EXPECT_TRUE(result.output.empty());
  EXPECT_TRUE(result.error_output.empty());
  EXPECT_EQ(result.exit_code, 0);
  EXPECT_EQ(result.duration, std::chrono::seconds(0));
  EXPECT_TRUE(result.command_executed.empty());
}

// =============================================================================
// BuildStatus Structure Tests
// =============================================================================

TEST_F(BuildToolTest, BuildStatusStructureContainsExpectedFields) {
  BuildTool::BuildStatus status;

  // Verify default values
  EXPECT_FALSE(status.is_running);
  EXPECT_TRUE(status.current_operation.empty());
  EXPECT_TRUE(status.last_result_summary.empty());
  EXPECT_EQ(status.progress_percent, 0);
}

// =============================================================================
// Cancel Operation Tests
// =============================================================================

TEST_F(BuildToolTest, CancelOperationWhenNotRunning) {
  BuildTool tool;

  // Canceling when nothing is running should succeed
  auto status = tool.CancelCurrentOperation();
  EXPECT_TRUE(status.ok());
}

// =============================================================================
// Command Handler Tests
// =============================================================================

TEST(BuildConfigureCommandHandlerTest, GetNameReturnsCorrectName) {
  BuildConfigureCommandHandler handler;
  EXPECT_EQ(handler.GetName(), "build-configure");
}

TEST(BuildConfigureCommandHandlerTest, GetUsageReturnsValidUsage) {
  BuildConfigureCommandHandler handler;
  std::string usage = handler.GetUsage();

  EXPECT_THAT(usage, HasSubstr("--preset"));
}

TEST(BuildCompileCommandHandlerTest, GetNameReturnsCorrectName) {
  BuildCompileCommandHandler handler;
  EXPECT_EQ(handler.GetName(), "build-compile");
}

TEST(BuildCompileCommandHandlerTest, GetUsageReturnsValidUsage) {
  BuildCompileCommandHandler handler;
  std::string usage = handler.GetUsage();

  EXPECT_THAT(usage, HasSubstr("--target"));
}

TEST(BuildTestCommandHandlerTest, GetNameReturnsCorrectName) {
  BuildTestCommandHandler handler;
  EXPECT_EQ(handler.GetName(), "build-test");
}

TEST(BuildTestCommandHandlerTest, GetUsageReturnsValidUsage) {
  BuildTestCommandHandler handler;
  std::string usage = handler.GetUsage();

  EXPECT_THAT(usage, HasSubstr("--filter"));
}

TEST(BuildStatusCommandHandlerTest, GetNameReturnsCorrectName) {
  BuildStatusCommandHandler handler;
  EXPECT_EQ(handler.GetName(), "build-status");
}

TEST(BuildStatusCommandHandlerTest, GetUsageReturnsValidUsage) {
  BuildStatusCommandHandler handler;
  std::string usage = handler.GetUsage();

  EXPECT_THAT(usage, HasSubstr("--build-dir"));
}

// =============================================================================
// Timeout Configuration Tests
// =============================================================================

TEST_F(BuildToolTest, CustomTimeoutConfiguration) {
  BuildTool::BuildConfig config;
  config.timeout = std::chrono::seconds(300);  // 5 minutes

  BuildTool tool(config);

  // Verify the tool was created successfully with custom config
  auto status = tool.GetBuildStatus();
  EXPECT_FALSE(status.is_running);
}

TEST_F(BuildToolTest, VerboseConfigurationOption) {
  BuildTool::BuildConfig config;
  config.verbose = true;

  BuildTool tool(config);

  // Verify the tool was created successfully with verbose mode
  auto status = tool.GetBuildStatus();
  EXPECT_FALSE(status.is_running);
}

}  // namespace
}  // namespace tools
}  // namespace agent
}  // namespace cli
}  // namespace yaze
