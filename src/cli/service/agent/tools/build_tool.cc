#include "cli/service/agent/tools/build_tool.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <memory>
#include <regex>
#include <sstream>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace yaze {
namespace cli {
namespace agent {
namespace tools {

namespace fs = std::filesystem;

// ============================================================================
// BuildTool Implementation
// ============================================================================

BuildTool::BuildTool(const BuildConfig& config) : config_(config) {
  // Ensure build directory is set with a default value
  const char* env_build_dir = std::getenv("YAZE_BUILD_DIR");
  if (env_build_dir != nullptr && env_build_dir[0] != '\0') {
    config_.build_directory = env_build_dir;
  } else if (config_.build_directory.empty()) {
    config_.build_directory = "build";
  }

  // Convert to absolute path if relative
  fs::path build_path(config_.build_directory);
  if (build_path.is_relative()) {
    config_.build_directory =
        (fs::path(GetProjectRoot()) / build_path).string();
  }
}

BuildTool::~BuildTool() {
  // Cancel any running operation on destruction
  if (is_running_) {
    CancelCurrentOperation();
  }

  // Wait for any execution thread to complete
  if (execution_thread_ && execution_thread_->joinable()) {
    execution_thread_->join();
  }
}

// ----------------------------------------------------------------------------
// Public Methods
// ----------------------------------------------------------------------------

absl::StatusOr<BuildTool::BuildResult> BuildTool::Configure(
    const std::string& preset) {
  if (preset.empty()) {
    return absl::InvalidArgumentError("Preset name cannot be empty");
  }

  // Validate preset exists
  if (!IsPresetValid(preset)) {
    auto available = ListAvailablePresets();
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid preset '%s'. Available presets: %s", preset,
                        absl::StrJoin(available, ", ")));
  }

  // Ensure build directory exists
  std::error_code ec;
  fs::create_directories(config_.build_directory, ec);
  if (ec) {
    return absl::InternalError(
        absl::StrFormat("Failed to create build directory: %s", ec.message()));
  }

  // Build cmake command
  std::string command = absl::StrFormat("cmake --preset %s -B \"%s\"", preset,
                                        config_.build_directory);

  if (config_.verbose) {
    command += " --debug-output";
  }

  return ExecuteCommand(
      command, absl::StrFormat("Configuring with preset '%s'", preset));
}

absl::StatusOr<BuildTool::BuildResult> BuildTool::Build(
    const std::string& target, const std::string& config) {

  // Check if build directory exists
  if (!IsBuildDirectoryReady()) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Build directory '%s' not configured. Run Configure first.",
        config_.build_directory));
  }

  // Build cmake command
  std::string command =
      absl::StrFormat("cmake --build \"%s\"", config_.build_directory);

  if (!config.empty()) {
    command += absl::StrFormat(" --config %s", config);
  }

  if (!target.empty()) {
    command += absl::StrFormat(" --target %s", target);
  }

  // Add parallel jobs based on CPU count
  command += " --parallel";

  if (config_.verbose) {
    command += " --verbose";
  }

  return ExecuteCommand(
      command,
      absl::StrFormat("Building %s", target.empty() ? "all targets" : target));
}

absl::StatusOr<BuildTool::BuildResult> BuildTool::RunTests(
    const std::string& filter, const std::string& rom_path) {

  // Check if build directory exists
  if (!IsBuildDirectoryReady()) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Build directory '%s' not configured. Run Configure first.",
        config_.build_directory));
  }

  // Build ctest command
  std::string command = absl::StrFormat(
      "ctest --test-dir \"%s\" --output-on-failure", config_.build_directory);

  // Add filter if specified
  if (!filter.empty()) {
    // Check if filter is a label (unit, integration, etc.) or a pattern
    if (filter == "unit" || filter == "integration" || filter == "e2e" ||
        filter == "stable" || filter == "experimental" ||
        filter == "rom_dependent") {
      command += absl::StrFormat(" -L %s", filter);
    } else {
      // Treat as regex pattern
      command += absl::StrFormat(" -R \"%s\"", filter);
    }
  }

  // Add ROM path environment variable if specified
  std::string env_setup;
  if (!rom_path.empty()) {
    if (!fs::exists(rom_path)) {
      return absl::NotFoundError(
          absl::StrFormat("ROM file not found: %s", rom_path));
    }
#ifdef _WIN32
    env_setup = absl::StrFormat(
        "set YAZE_TEST_ROM_VANILLA=\"%s\" && set YAZE_TEST_ROM_PATH=\"%s\" && ",
        rom_path, rom_path);
#else
    env_setup = absl::StrFormat(
        "YAZE_TEST_ROM_VANILLA=\"%s\" YAZE_TEST_ROM_PATH=\"%s\" ", rom_path,
        rom_path);
#endif
  }

  // Add parallel test execution
  command += " --parallel";

  if (config_.verbose) {
    command += " --verbose";
  }

  std::string full_command = env_setup + command;

  return ExecuteCommand(
      full_command,
      absl::StrFormat(
          "Running tests%s",
          filter.empty() ? "" : absl::StrFormat(" (filter: %s)", filter)));
}

BuildTool::BuildStatus BuildTool::GetBuildStatus() const {
  std::lock_guard<std::mutex> lock(status_mutex_);

  BuildStatus status;
  status.is_running = is_running_;
  status.current_operation = current_operation_;
  status.start_time = operation_start_time_;
  status.progress_percent = -1;  // Unknown

  if (last_result_.has_value()) {
    status.last_result_summary = absl::StrFormat(
        "Last operation: %s (exit code: %d, duration: %lds)",
        last_result_->success ? "SUCCESS" : "FAILED", last_result_->exit_code,
        last_result_->duration.count());
  } else {
    status.last_result_summary = "No operations executed yet";
  }

  return status;
}

absl::StatusOr<BuildTool::BuildResult> BuildTool::Clean() {
  if (!IsBuildDirectoryReady()) {
    // Build directory doesn't exist, nothing to clean
    BuildResult result;
    result.success = true;
    result.output = "Build directory does not exist, nothing to clean";
    result.exit_code = 0;
    result.duration = std::chrono::seconds(0);
    result.command_executed = "Clean";
    return result;
  }

  std::string command = absl::StrFormat("cmake --build \"%s\" --target clean",
                                        config_.build_directory);

  return ExecuteCommand(command, "Cleaning build directory");
}

bool BuildTool::IsBuildDirectoryReady() const {
  fs::path build_path(config_.build_directory);

  // Check if directory exists and contains CMakeCache.txt
  return fs::exists(build_path) && fs::exists(build_path / "CMakeCache.txt");
}

std::vector<std::string> BuildTool::ListAvailablePresets() const {
  return ParsePresetsFile();
}

std::optional<BuildTool::BuildResult> BuildTool::GetLastResult() const {
  std::lock_guard<std::mutex> lock(status_mutex_);
  return last_result_;
}

absl::Status BuildTool::CancelCurrentOperation() {
  cancel_requested_ = true;

  if (execution_thread_ && execution_thread_->joinable()) {
    execution_thread_->join();
  }

  return absl::OkStatus();
}

// ----------------------------------------------------------------------------
// Private Methods
// ----------------------------------------------------------------------------

absl::StatusOr<BuildTool::BuildResult> BuildTool::ExecuteCommand(
    const std::string& command, const std::string& operation_name) {

  // Check if another operation is running
  if (is_running_.exchange(true)) {
    return absl::UnavailableError("Another build operation is in progress");
  }

  // Update status
  UpdateStatus(operation_name, true);
  auto start_time = std::chrono::steady_clock::now();

  // Execute command
  auto result = ExecuteCommandInternal(command, config_.timeout);

  // Calculate duration
  auto end_time = std::chrono::steady_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);

  if (result.ok()) {
    auto& build_result = *result;
    build_result.duration = duration;
    build_result.command_executed = command;

    // Store last result
    {
      std::lock_guard<std::mutex> lock(status_mutex_);
      last_result_ = build_result;
    }
  }

  // Update status
  UpdateStatus("", false);
  is_running_ = false;

  return result;
}

absl::StatusOr<BuildTool::BuildResult> BuildTool::ExecuteCommandInternal(
    const std::string& command, const std::chrono::seconds& timeout) {

  BuildResult result;
  result.command_executed = command;
  result.success = false;
  result.exit_code = -1;

  // Change to project root before executing
  fs::path original_dir = fs::current_path();
  fs::path project_root(GetProjectRoot());

  std::error_code ec;
  fs::current_path(project_root, ec);
  if (ec) {
    return absl::InternalError(
        absl::StrFormat("Failed to change directory: %s", ec.message()));
  }

  // Platform-specific command execution
#ifdef _WIN32
  // Windows implementation using _popen
  std::string full_command = absl::StrFormat("cmd /c %s 2>&1", command);
  FILE* pipe = _popen(full_command.c_str(), "r");
#else
  // Unix implementation using popen
  std::string full_command = command + " 2>&1";
  FILE* pipe = popen(full_command.c_str(), "r");
#endif

  if (!pipe) {
    fs::current_path(original_dir, ec);
    return absl::InternalError("Failed to execute command");
  }

  // Read output with timeout protection
  std::stringstream output_stream;
  std::stringstream error_stream;
  std::array<char, 4096> buffer;
  size_t total_output = 0;
  auto start_time = std::chrono::steady_clock::now();

  // Set non-blocking mode for better timeout handling (Unix only)
#ifndef _WIN32
  int pipe_fd = fileno(pipe);
  int flags = fcntl(pipe_fd, F_GETFL, 0);
  fcntl(pipe_fd, F_SETFL, flags | O_NONBLOCK);
#endif

  while (!cancel_requested_) {
    // Check timeout
    auto current_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        current_time - start_time);

    if (elapsed >= timeout) {
      // Timeout reached
#ifdef _WIN32
      _pclose(pipe);
#else
      pclose(pipe);
#endif
      fs::current_path(original_dir, ec);
      return absl::DeadlineExceededError(absl::StrFormat(
          "Command timed out after %ld seconds", timeout.count()));
    }

    // Read from pipe
    if (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
      size_t len = strlen(buffer.data());
      total_output += len;

      // Check output size limit
      if (config_.capture_output &&
          total_output <= static_cast<size_t>(config_.max_output_size)) {
        output_stream << buffer.data();

        // Try to separate errors (lines containing "error", "warning", "failed")
        std::string line(buffer.data());
        std::transform(line.begin(), line.end(), line.begin(), ::tolower);
        if (line.find("error") != std::string::npos ||
            line.find("failed") != std::string::npos ||
            line.find("fatal") != std::string::npos) {
          error_stream << buffer.data();
        }
      }
    } else {
      // Check if it's EOF or just no data available
      if (feof(pipe)) {
        break;  // End of stream
      }
      // On Unix, sleep briefly if no data available
#ifndef _WIN32
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        clearerr(pipe);
        continue;
      }
#endif
      break;  // Error or EOF
    }
  }

  // Get exit code
#ifdef _WIN32
  result.exit_code = _pclose(pipe);
#else
  int status = pclose(pipe);
  if (WIFEXITED(status)) {
    result.exit_code = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    result.exit_code = 128 + WTERMSIG(status);
  } else {
    result.exit_code = -1;
  }
#endif

  // Restore original directory
  fs::current_path(original_dir, ec);

  // Set results
  result.success = (result.exit_code == 0);
  result.output = output_stream.str();
  result.error_output = error_stream.str();

  // If we were cancelled, return cancelled error
  if (cancel_requested_) {
    return absl::CancelledError("Operation was cancelled");
  }

  return result;
}

std::string BuildTool::GetProjectRoot() const {
  // Look for common project markers to find the root
  fs::path current = fs::current_path();
  fs::path root = current;

  // Walk up the directory tree looking for project markers
  while (!root.empty() && root != root.root_path()) {
    // Check for yaze-specific markers
    if (fs::exists(root / "CMakeLists.txt") &&
        fs::exists(root / "src" / "yaze.cc") &&
        fs::exists(root / "CMakePresets.json")) {
      return root.string();
    }
    // Also check for .git directory as a fallback
    if (fs::exists(root / ".git")) {
      // Verify this is the yaze project
      if (fs::exists(root / "src" / "cli") &&
          fs::exists(root / "src" / "app")) {
        return root.string();
      }
    }
    root = root.parent_path();
  }

  // Default to current directory if project root not found
  return current.string();
}

std::string BuildTool::GetCurrentPlatform() const {
#ifdef _WIN32
  return "Windows";
#elif defined(__APPLE__)
  return "Darwin";
#elif defined(__linux__)
  return "Linux";
#else
  return "Unknown";
#endif
}

std::vector<std::string> BuildTool::ParsePresetsFile() const {
  std::vector<std::string> presets;

  fs::path presets_file = fs::path(GetProjectRoot()) / "CMakePresets.json";
  if (!fs::exists(presets_file)) {
    return presets;
  }

  // Read the file
  std::ifstream file(presets_file);
  if (!file) {
    return presets;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string content = buffer.str();

  // Get current platform for filtering
  std::string platform = GetCurrentPlatform();

  // Parse JSON to find configure presets
  // We need to track whether we're in configurePresets section
  bool in_configure_presets = false;
  bool in_preset_object = false;
  bool is_hidden = false;
  std::string current_preset_name;

  // Simple state machine for JSON parsing
  std::regex configure_presets_regex("\"configurePresets\"\\s*:\\s*\\[");
  std::regex preset_name_regex("\"name\"\\s*:\\s*\"([^\"]+)\"");
  std::regex hidden_regex("\"hidden\"\\s*:\\s*true");
  std::regex condition_regex("\"condition\"\\s*:");

  std::istringstream stream(content);
  std::string line;
  int brace_count = 0;

  while (std::getline(stream, line)) {
    // Check if entering configurePresets
    if (std::regex_search(line, configure_presets_regex)) {
      in_configure_presets = true;
      continue;
    }

    if (!in_configure_presets)
      continue;

    // Track braces to know when we're in a preset object
    for (char c : line) {
      if (c == '{') {
        brace_count++;
        if (brace_count == 1) {
          in_preset_object = true;
          is_hidden = false;
          current_preset_name.clear();
        }
      } else if (c == '}') {
        brace_count--;
        if (brace_count == 0 && in_preset_object) {
          // End of preset object, add if valid
          if (!current_preset_name.empty() && !is_hidden) {
            // Filter by platform
            bool include = false;

            if (platform == "Windows") {
              // Include Windows presets and generic ones
              if (absl::StartsWith(current_preset_name, "win-") ||
                  absl::StartsWith(current_preset_name, "ci-windows") ||
                  (!absl::StartsWith(current_preset_name, "mac-") &&
                   !absl::StartsWith(current_preset_name, "lin-") &&
                   !absl::StartsWith(current_preset_name, "ci-"))) {
                include = true;
              }
            } else if (platform == "Darwin") {
              // Include macOS presets and generic ones
              if (absl::StartsWith(current_preset_name, "mac-") ||
                  absl::StartsWith(current_preset_name, "ci-macos") ||
                  (!absl::StartsWith(current_preset_name, "win-") &&
                   !absl::StartsWith(current_preset_name, "lin-") &&
                   !absl::StartsWith(current_preset_name, "ci-"))) {
                include = true;
              }
            } else if (platform == "Linux") {
              // Include Linux presets and generic ones
              if (absl::StartsWith(current_preset_name, "lin-") ||
                  absl::StartsWith(current_preset_name, "ci-linux") ||
                  (!absl::StartsWith(current_preset_name, "win-") &&
                   !absl::StartsWith(current_preset_name, "mac-") &&
                   !absl::StartsWith(current_preset_name, "ci-"))) {
                include = true;
              }
            }

            if (include) {
              presets.push_back(current_preset_name);
            }
          }
          in_preset_object = false;
        }
      } else if (c == ']' && brace_count == -1) {
        // End of configurePresets array
        in_configure_presets = false;
        break;
      }
    }

    if (in_preset_object) {
      // Look for preset name
      std::smatch match;
      if (std::regex_search(line, match, preset_name_regex)) {
        current_preset_name = match[1].str();
      }

      // Check if hidden
      if (std::regex_search(line, hidden_regex)) {
        is_hidden = true;
      }
    }
  }

  // Sort presets alphabetically
  std::sort(presets.begin(), presets.end());

  return presets;
}

bool BuildTool::IsPresetValid(const std::string& preset) const {
  auto available = ListAvailablePresets();
  return std::find(available.begin(), available.end(), preset) !=
         available.end();
}

void BuildTool::UpdateStatus(const std::string& operation, bool is_running) {
  std::lock_guard<std::mutex> lock(status_mutex_);
  current_operation_ = operation;
  is_running_ = is_running;
  if (is_running) {
    operation_start_time_ = std::chrono::system_clock::now();
  }
}

// ============================================================================
// Command Handler Implementations
// ============================================================================

absl::Status BuildConfigureCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  if (!parser.GetString("preset").has_value()) {
    return absl::InvalidArgumentError("--preset is required");
  }
  return absl::OkStatus();
}

absl::Status BuildConfigureCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {

  // Get parameters
  std::string preset = parser.GetString("preset").value();
  std::string build_dir = parser.GetString("build-dir").value_or("build_ai");
  bool verbose = parser.HasFlag("verbose");

  // Create build tool with config
  BuildTool::BuildConfig config;
  config.build_directory = build_dir;
  config.verbose = verbose;
  config.capture_output = true;

  build_tool_ = std::make_unique<BuildTool>(config);

  // Execute configuration
  auto result = build_tool_->Configure(preset);
  if (!result.ok()) {
    return result.status();
  }

  // Format output
  formatter.BeginObject("Build Configuration");
  formatter.AddField("preset", preset);
  formatter.AddField("build_directory", config.build_directory);
  formatter.AddField("success", result->success ? "true" : "false");
  formatter.AddField("exit_code", std::to_string(result->exit_code));
  formatter.AddField("duration",
                     absl::StrFormat("%ld seconds", result->duration.count()));

  if (!result->output.empty()) {
    // Truncate output if too long
    const size_t max_lines = 100;
    std::vector<std::string> lines = absl::StrSplit(result->output, '\n');
    if (lines.size() > max_lines) {
      std::vector<std::string> truncated(lines.end() - max_lines, lines.end());
      formatter.AddField("output",
                         absl::StrFormat("[...truncated %zu lines...]\n%s",
                                         lines.size() - max_lines,
                                         absl::StrJoin(truncated, "\n")));
    } else {
      formatter.AddField("output", result->output);
    }
  }

  if (!result->error_output.empty()) {
    formatter.AddField("errors", result->error_output);
  }

  formatter.EndObject();

  if (!result->success) {
    return absl::InternalError("Configuration failed");
  }

  return absl::OkStatus();
}

absl::Status BuildCompileCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  // All arguments are optional
  return absl::OkStatus();
}

absl::Status BuildCompileCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {

  // Get parameters
  std::string target = parser.GetString("target").value_or("");
  std::string config = parser.GetString("config").value_or("");
  std::string build_dir = parser.GetString("build-dir").value_or("build_ai");
  bool verbose = parser.HasFlag("verbose");

  // Create build tool
  BuildTool::BuildConfig tool_config;
  tool_config.build_directory = build_dir;
  tool_config.verbose = verbose;
  tool_config.capture_output = true;

  build_tool_ = std::make_unique<BuildTool>(tool_config);

  // Execute build
  auto result = build_tool_->Build(target, config);
  if (!result.ok()) {
    return result.status();
  }

  // Format output
  formatter.BeginObject("Build Compilation");
  formatter.AddField("target", target.empty() ? "all" : target);
  if (!config.empty()) {
    formatter.AddField("configuration", config);
  }
  formatter.AddField("build_directory", build_dir);
  formatter.AddField("success", result->success ? "true" : "false");
  formatter.AddField("exit_code", std::to_string(result->exit_code));
  formatter.AddField("duration",
                     absl::StrFormat("%ld seconds", result->duration.count()));

  // Limit output size for readability
  if (!result->output.empty()) {
    const size_t max_lines = 100;
    std::vector<std::string> lines = absl::StrSplit(result->output, '\n');
    if (lines.size() > max_lines) {
      std::vector<std::string> truncated(lines.end() - max_lines, lines.end());
      formatter.AddField("output",
                         absl::StrFormat("[...truncated %zu lines...]\n%s",
                                         lines.size() - max_lines,
                                         absl::StrJoin(truncated, "\n")));
      formatter.AddField("output_truncated", "true");
    } else {
      formatter.AddField("output", result->output);
    }
  }

  if (!result->error_output.empty()) {
    formatter.AddField("errors", result->error_output);
  }

  formatter.EndObject();

  if (!result->success) {
    return absl::InternalError("Build failed");
  }

  return absl::OkStatus();
}

absl::Status BuildTestCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  // All arguments are optional
  return absl::OkStatus();
}

absl::Status BuildTestCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {

  // Get parameters
  std::string filter = parser.GetString("filter").value_or("");
  std::string rom_path = parser.GetString("rom-path").value_or("");
  std::string build_dir = parser.GetString("build-dir").value_or("build_ai");
  bool verbose = parser.HasFlag("verbose");

  // Create build tool
  BuildTool::BuildConfig config;
  config.build_directory = build_dir;
  config.verbose = verbose;
  config.capture_output = true;
  config.timeout = std::chrono::seconds(300);  // 5 minutes for tests

  build_tool_ = std::make_unique<BuildTool>(config);

  // Execute tests
  auto result = build_tool_->RunTests(filter, rom_path);
  if (!result.ok()) {
    return result.status();
  }

  // Format output
  formatter.BeginObject("Test Execution");
  if (!filter.empty()) {
    formatter.AddField("filter", filter);
  }
  if (!rom_path.empty()) {
    formatter.AddField("rom_path", rom_path);
  }
  formatter.AddField("build_directory", build_dir);
  formatter.AddField("success", result->success ? "true" : "false");
  formatter.AddField("exit_code", std::to_string(result->exit_code));
  formatter.AddField("duration",
                     absl::StrFormat("%ld seconds", result->duration.count()));

  // Parse test results from output
  if (!result->output.empty()) {
    // Look for test summary
    std::regex summary_regex(
        R"((\d+)% tests passed, (\d+) tests failed out of (\d+))");
    std::smatch match;
    if (std::regex_search(result->output, match, summary_regex)) {
      formatter.AddField("tests_total", match[3].str());
      formatter.AddField("tests_failed", match[2].str());
      formatter.AddField("pass_rate", match[1].str() + "%");
    }

    // Include last part of output for visibility
    const size_t max_lines = 50;
    std::vector<std::string> lines = absl::StrSplit(result->output, '\n');
    if (lines.size() > max_lines) {
      std::vector<std::string> truncated(lines.end() - max_lines, lines.end());
      formatter.AddField("output",
                         absl::StrFormat("[...truncated %zu lines...]\n%s",
                                         lines.size() - max_lines,
                                         absl::StrJoin(truncated, "\n")));
    } else {
      formatter.AddField("output", result->output);
    }
  }

  if (!result->error_output.empty()) {
    formatter.AddField("errors", result->error_output);
  }

  formatter.EndObject();

  if (!result->success) {
    return absl::InternalError("Tests failed");
  }

  return absl::OkStatus();
}

absl::Status BuildStatusCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  // All arguments are optional
  return absl::OkStatus();
}

absl::Status BuildStatusCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {

  std::string build_dir = parser.GetString("build-dir").value_or("build_ai");

  // Create build tool
  BuildTool::BuildConfig config;
  config.build_directory = build_dir;

  build_tool_ = std::make_unique<BuildTool>(config);

  // Get status
  auto status = build_tool_->GetBuildStatus();

  formatter.BeginObject("Build Status");
  formatter.AddField("build_directory", build_dir);
  formatter.AddField("directory_ready",
                     build_tool_->IsBuildDirectoryReady() ? "true" : "false");
  formatter.AddField("operation_running", status.is_running ? "true" : "false");

  if (status.is_running) {
    formatter.AddField("current_operation", status.current_operation);

    // Calculate elapsed time
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - status.start_time);
    formatter.AddField("elapsed_time",
                       absl::StrFormat("%ld seconds", elapsed.count()));
  }

  if (!status.last_result_summary.empty()) {
    formatter.AddField("last_result", status.last_result_summary);
  }

  // List available presets
  auto presets = build_tool_->ListAvailablePresets();
  if (!presets.empty()) {
    formatter.BeginArray("available_presets");
    for (const auto& preset : presets) {
      formatter.AddArrayItem(preset);
    }
    formatter.EndArray();
  }

  // Check for last build result
  auto last_result = build_tool_->GetLastResult();
  if (last_result.has_value()) {
    formatter.BeginObject("last_build");
    formatter.AddField("command", last_result->command_executed);
    formatter.AddField("success", last_result->success ? "true" : "false");
    formatter.AddField("exit_code", std::to_string(last_result->exit_code));
    formatter.AddField(
        "duration",
        absl::StrFormat("%ld seconds", last_result->duration.count()));
    formatter.EndObject();
  }

  formatter.EndObject();

  return absl::OkStatus();
}

}  // namespace tools
}  // namespace agent
}  // namespace cli
}  // namespace yaze
