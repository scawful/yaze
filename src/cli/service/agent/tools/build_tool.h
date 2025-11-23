#ifndef YAZE_SRC_CLI_SERVICE_AGENT_TOOLS_BUILD_TOOL_H_
#define YAZE_SRC_CLI_SERVICE_AGENT_TOOLS_BUILD_TOOL_H_

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace agent {
namespace tools {

/**
 * @brief Build tool for AI agents to compile and test the project
 *
 * This tool provides safe access to build operations with:
 * - CMake configuration with presets
 * - Building specific targets
 * - Running tests with filters
 * - Build status monitoring
 * - Timeout protection
 * - Output capture and streaming
 */
class BuildTool {
 public:
  // Build operation results
  struct BuildResult {
    bool success;
    std::string output;
    std::string error_output;
    int exit_code;
    std::chrono::seconds duration;
    std::string command_executed;
  };

  // Build status information
  struct BuildStatus {
    bool is_running;
    std::string current_operation;
    std::string last_result_summary;
    std::chrono::system_clock::time_point start_time;
    int progress_percent;  // -1 if unknown
  };

  // Build configuration
  struct BuildConfig {
    std::string build_directory = "build_ai";  // Default AI agent build dir
    std::chrono::seconds timeout = std::chrono::seconds(600);  // 10 min default
    bool capture_output = true;
    bool verbose = false;
    int max_output_size = 1024 * 1024;  // 1MB max output

    BuildConfig() = default;
  };

  BuildTool() : BuildTool(BuildConfig{}) {}
  explicit BuildTool(const BuildConfig& config);
  ~BuildTool();

  /**
   * @brief Configure the build system with a CMake preset
   * @param preset The preset name (e.g., "mac-ai", "lin-dbg", "win-ai")
   * @return Build result with configuration output
   */
  absl::StatusOr<BuildResult> Configure(const std::string& preset);

  /**
   * @brief Build a specific target or all targets
   * @param target The target name (empty for all, "yaze", "yaze_test", "z3ed")
   * @param config The build configuration (Debug, Release, RelWithDebInfo)
   * @return Build result with compilation output
   */
  absl::StatusOr<BuildResult> Build(const std::string& target = "",
                                    const std::string& config = "");

  /**
   * @brief Run tests with optional filter
   * @param filter Test filter pattern (e.g., "*Canvas*", "unit", "integration")
   * @param rom_path Optional ROM path for ROM-dependent tests
   * @return Test execution result
   */
  absl::StatusOr<BuildResult> RunTests(const std::string& filter = "",
                                       const std::string& rom_path = "");

  /**
   * @brief Get current build status
   * @return Current build operation status
   */
  BuildStatus GetBuildStatus() const;

  /**
   * @brief Clean the build directory
   * @return Cleanup result
   */
  absl::StatusOr<BuildResult> Clean();

  /**
   * @brief Check if a build directory exists and is configured
   * @return true if build directory is ready
   */
  bool IsBuildDirectoryReady() const;

  /**
   * @brief List available CMake presets
   * @return Vector of preset names available for current platform
   */
  std::vector<std::string> ListAvailablePresets() const;

  /**
   * @brief Get the last build result
   * @return The most recent build operation result
   */
  std::optional<BuildResult> GetLastResult() const;

  /**
   * @brief Cancel the current build operation
   * @return Status of cancellation attempt
   */
  absl::Status CancelCurrentOperation();

 private:
  // Execute a command with timeout and output capture
  absl::StatusOr<BuildResult> ExecuteCommand(
      const std::string& command,
      const std::string& operation_name);

  // Platform-specific command execution
  absl::StatusOr<BuildResult> ExecuteCommandInternal(
      const std::string& command,
      const std::chrono::seconds& timeout);

  // Get the project root directory
  std::string GetProjectRoot() const;

  // Detect the current platform
  std::string GetCurrentPlatform() const;

  // Parse CMakePresets.json for available presets
  std::vector<std::string> ParsePresetsFile() const;

  // Validate preset exists and is compatible
  bool IsPresetValid(const std::string& preset) const;

  // Thread-safe status updates
  void UpdateStatus(const std::string& operation, bool is_running);

  BuildConfig config_;
  mutable std::mutex status_mutex_;
  std::atomic<bool> is_running_{false};
  std::string current_operation_;
  std::optional<BuildResult> last_result_;
  std::chrono::system_clock::time_point operation_start_time_;
  std::unique_ptr<std::thread> execution_thread_;
  std::atomic<bool> cancel_requested_{false};
};

// Command handler implementations for integration with ToolDispatcher

/**
 * @brief Configure command handler
 */
class BuildConfigureCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "build-configure"; }

  std::string GetDescription() const {
    return "Configure the build system with a CMake preset";
  }

  std::string GetUsage() const override {
    return "build-configure --preset <preset> [--build-dir <dir>] [--verbose]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                      resources::OutputFormatter& formatter) override;

 private:
  std::unique_ptr<BuildTool> build_tool_;
};

/**
 * @brief Build command handler
 */
class BuildCompileCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "build-compile"; }

  std::string GetDescription() const {
    return "Build a specific target or all targets";
  }

  std::string GetUsage() const override {
    return "build-compile [--target <target>] [--config <config>] [--build-dir <dir>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                      resources::OutputFormatter& formatter) override;

 private:
  std::unique_ptr<BuildTool> build_tool_;
};

/**
 * @brief Test execution command handler
 */
class BuildTestCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "build-test"; }

  std::string GetDescription() const {
    return "Run tests with optional filter";
  }

  std::string GetUsage() const override {
    return "build-test [--filter <pattern>] [--rom-path <path>] [--build-dir <dir>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                      resources::OutputFormatter& formatter) override;

 private:
  std::unique_ptr<BuildTool> build_tool_;
};

/**
 * @brief Build status command handler
 */
class BuildStatusCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "build-status"; }

  std::string GetDescription() const {
    return "Get current build operation status";
  }

  std::string GetUsage() const override {
    return "build-status [--build-dir <dir>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                      resources::OutputFormatter& formatter) override;

 private:
  std::unique_ptr<BuildTool> build_tool_;
};

}  // namespace tools
}  // namespace agent
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_AGENT_TOOLS_BUILD_TOOL_H_