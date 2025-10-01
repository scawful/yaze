#include "cli/z3ed.h"
#include "cli/modern_cli.h"
#include "cli/service/ai_service.h"

#include "absl/status/status.h"

#include <cstdlib>  // For EXIT_FAILURE

// Platform-specific includes for process management and executable path detection
#if !defined(_WIN32)
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

namespace yaze {
namespace cli {

// Mock AI service is defined in ai_service.h

namespace {

absl::Status HandleRunCommand(const std::vector<std::string>& arg_vec, Rom& rom) {
    if (arg_vec.size() < 2 || arg_vec[0] != "--prompt") {
        return absl::InvalidArgumentError("Usage: agent run --prompt <prompt>");
    }
    std::string prompt = arg_vec[1];
    
    // Save a temporary copy of the ROM
    if (rom.is_loaded()) {
        auto status = rom.SaveToFile({.save_new = true, .filename = "temp_rom.sfc"});
        if (!status.ok()) {
            return status;
        }
    }

    MockAIService ai_service;
    auto commands_or = ai_service.GetCommands(prompt);
    if (!commands_or.ok()) {
        return commands_or.status();
    }
    std::vector<std::string> commands = commands_or.value();

    ModernCLI cli;
    for (const auto& command : commands) {
        std::vector<std::string> command_parts;
        std::string current_part;
        bool in_quotes = false;
        for (char c : command) {
            if (c == '\"') {
                in_quotes = !in_quotes;
            } else if (c == ' ' && !in_quotes) {
                command_parts.push_back(current_part);
                current_part.clear();
            } else {
                current_part += c;
            }
        }
        command_parts.push_back(current_part);

        std::string cmd_name = command_parts[0] + " " + command_parts[1];
        std::vector<std::string> cmd_args(command_parts.begin() + 2, command_parts.end());
        
        auto it = cli.commands_.find(cmd_name);
        if (it != cli.commands_.end()) {
            auto status = it->second.handler(cmd_args);
            if (!status.ok()) {
                return status;
            }
        }
    }
    return absl::OkStatus();
}

absl::Status HandlePlanCommand(const std::vector<std::string>& arg_vec) {
    if (arg_vec.size() < 2 || arg_vec[0] != "--prompt") {
        return absl::InvalidArgumentError("Usage: agent plan --prompt <prompt>");
    }
    std::string prompt = arg_vec[1];
    MockAIService ai_service;
    auto commands_or = ai_service.GetCommands(prompt);
    if (!commands_or.ok()) {
        return commands_or.status();
    }
    std::vector<std::string> commands = commands_or.value();

    std::cout << "AI Agent Plan:" << std::endl;
    for (const auto& command : commands) {
        std::cout << "  - " << command << std::endl;
    }
    return absl::OkStatus();
}

absl::Status HandleDiffCommand(Rom& rom) {
    if (rom.is_loaded()) {
        RomDiff diff_handler;
        auto status = diff_handler.Run({rom.filename(), "temp_rom.sfc"});
        if (!status.ok()) {
            return status;
        }
    } else {
        return absl::AbortedError("No ROM loaded.");
    }
    return absl::OkStatus();
}

absl::Status HandleTestCommand(const std::vector<std::string>& arg_vec) {
    if (arg_vec.size() < 2 || arg_vec[0] != "--test") {
        return absl::InvalidArgumentError("Usage: agent test --test <test_name>");
    }
    
#ifdef _WIN32
    // Windows doesn't support fork/exec, so users must run tests directly
    return absl::UnimplementedError(
        "GUI test command is not supported on Windows. "
        "Please run yaze_test.exe directly with --enable-ui-tests flag.");
#else
    // Unix-like systems (macOS, Linux) support fork/exec for process spawning
    std::string test_name = arg_vec[1];
    
    // Get the executable path using platform-specific methods
    char exe_path[1024];
#ifdef __APPLE__
    uint32_t size = sizeof(exe_path);
    if (_NSGetExecutablePath(exe_path, &size) != 0) {
        return absl::InternalError("Could not get executable path");
    }
#elif defined(__linux__)
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len == -1) {
        return absl::InternalError("Could not get executable path");
    }
    exe_path[len] = '\0';
#else
    return absl::UnimplementedError(
        "GUI test command is not supported on this platform. "
        "Please run yaze_test directly with --enable-ui-tests flag.");
#endif

    // Extract directory from executable path
    std::string exe_dir = std::string(exe_path);
    exe_dir = exe_dir.substr(0, exe_dir.find_last_of("/"));
    std::string yaze_test_path = exe_dir + "/yaze_test";

    // Prepare command arguments for execv
    std::vector<std::string> command_args;
    command_args.push_back(yaze_test_path);
    command_args.push_back("--enable-ui-tests");
    command_args.push_back("--test=" + test_name);

    std::vector<char*> argv;
    for (const auto& arg : command_args) {
        argv.push_back((char*)arg.c_str());
    }
    argv.push_back(nullptr);

    // Fork and execute the test process
    pid_t pid = fork();
    if (pid == -1) {
        return absl::InternalError("Failed to fork process");
    }

    if (pid == 0) {
        // Child process: execute the test binary
        execv(yaze_test_path.c_str(), argv.data());
        // If execv returns, it must have failed
        _exit(EXIT_FAILURE);  // Use _exit in child process after failed exec
    } else {
        // Parent process: wait for child to complete
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            return absl::InternalError("Failed to wait for child process");
        }
        
        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            if (exit_code == 0) {
                return absl::OkStatus();
            } else {
                return absl::InternalError(
                    absl::StrFormat("yaze_test exited with code %d", exit_code));
            }
        } else if (WIFSIGNALED(status)) {
            return absl::InternalError(
                absl::StrFormat("yaze_test terminated by signal %d", WTERMSIG(status)));
        } else {
            return absl::InternalError("yaze_test terminated abnormally");
        }
    }
#endif
}

absl::Status HandleLearnCommand() {
    std::cout << "Agent learn not yet implemented." << std::endl;
    return absl::OkStatus();
}

absl::Status HandleCommitCommand(Rom& rom) {
    if (rom.is_loaded()) {
        auto status = rom.SaveToFile({.save_new = false});
        if (!status.ok()) {
            return status;
        }
        std::cout << "✅ Changes committed successfully." << std::endl;
    } else {
        return absl::AbortedError("No ROM loaded.");
    }
    return absl::OkStatus();
}

absl::Status HandleRevertCommand(Rom& rom) {
    if (rom.is_loaded()) {
        auto status = rom.LoadFromFile(rom.filename());
        if (!status.ok()) {
            return status;
        }
        std::cout << "✅ Changes reverted successfully." << std::endl;
    } else {
        return absl::AbortedError("No ROM loaded.");
    }
    return absl::OkStatus();
}

} // namespace

absl::Status Agent::Run(const std::vector<std::string>& arg_vec) {
  if (arg_vec.empty()) {
    return absl::InvalidArgumentError("Usage: agent <run|plan|diff|test|learn|commit|revert> [options]");
  }

  std::string subcommand = arg_vec[0];
  std::vector<std::string> subcommand_args(arg_vec.begin() + 1, arg_vec.end());

  if (subcommand == "run") {
    return HandleRunCommand(subcommand_args, rom_);
  } else if (subcommand == "plan") {
    return HandlePlanCommand(subcommand_args);
  } else if (subcommand == "diff") {
    return HandleDiffCommand(rom_);
  } else if (subcommand == "test") {
    return HandleTestCommand(subcommand_args);
  } else if (subcommand == "learn") {
    return HandleLearnCommand();
  } else if (subcommand == "commit") {
    return HandleCommitCommand(rom_);
  } else if (subcommand == "revert") {
    return HandleRevertCommand(rom_);
  } else {
    return absl::InvalidArgumentError("Invalid subcommand for agent command.");
  }

  return absl::OkStatus();
}

}  // namespace cli
}  // namespace yaze
