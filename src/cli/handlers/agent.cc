#include "cli/z3ed.h"
#include "cli/modern_cli.h"
#include "cli/service/ai_service.h"
#include "cli/service/resource_catalog.h"
#include "cli/service/rom_sandbox_manager.h"
#include "util/macro.h"

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/time/time.h"

#include <cstdlib>  // For EXIT_FAILURE
#include <fstream>
#include <optional>

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

struct DescribeOptions {
    std::optional<std::string> resource;
    std::string format = "json";
    std::optional<std::string> output_path;
    std::string version = "0.1.0";
    std::optional<std::string> last_updated;
};

absl::StatusOr<DescribeOptions> ParseDescribeArgs(
        const std::vector<std::string>& args) {
    DescribeOptions options;
    for (size_t i = 0; i < args.size(); ++i) {
        const std::string& token = args[i];
        std::string flag = token;
        std::optional<std::string> inline_value;

        if (absl::StartsWith(token, "--")) {
            auto eq_pos = token.find('=');
            if (eq_pos != std::string::npos) {
                flag = token.substr(0, eq_pos);
                inline_value = token.substr(eq_pos + 1);
            }
        }

        auto require_value = [&](absl::string_view flag_name) -> absl::StatusOr<std::string> {
            if (inline_value.has_value()) {
                return *inline_value;
            }
            if (i + 1 >= args.size()) {
                return absl::InvalidArgumentError(
                        absl::StrFormat("Flag %s requires a value", flag_name));
            }
            return args[++i];
        };

        if (flag == "--resource") {
            ASSIGN_OR_RETURN(auto value, require_value("--resource"));
            options.resource = std::move(value);
        } else if (flag == "--format") {
            ASSIGN_OR_RETURN(auto value, require_value("--format"));
            options.format = std::move(value);
        } else if (flag == "--output") {
            ASSIGN_OR_RETURN(auto value, require_value("--output"));
            options.output_path = std::move(value);
        } else if (flag == "--version") {
            ASSIGN_OR_RETURN(auto value, require_value("--version"));
            options.version = std::move(value);
        } else if (flag == "--last-updated") {
            ASSIGN_OR_RETURN(auto value, require_value("--last-updated"));
            options.last_updated = std::move(value);
        } else {
            return absl::InvalidArgumentError(
                    absl::StrFormat("Unknown flag for agent describe: %s", token));
        }
    }

    options.format = absl::AsciiStrToLower(options.format);
    if (options.format != "json" && options.format != "yaml") {
        return absl::InvalidArgumentError("--format must be either json or yaml");
    }

    return options;
}

absl::Status HandleRunCommand(const std::vector<std::string>& arg_vec, Rom& rom) {
    if (arg_vec.size() < 2 || arg_vec[0] != "--prompt") {
        return absl::InvalidArgumentError("Usage: agent run --prompt <prompt>");
    }
    std::string prompt = arg_vec[1];
    
    // Save a sandbox copy of the ROM for proposal tracking.
    if (rom.is_loaded()) {
        auto sandbox_or = RomSandboxManager::Instance().CreateSandbox(
            rom, "agent-run");
        if (!sandbox_or.ok()) {
            return sandbox_or.status();
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
        auto sandbox_or = RomSandboxManager::Instance().ActiveSandbox();
        if (!sandbox_or.ok()) {
            return sandbox_or.status();
        }
        RomDiff diff_handler;
        auto status = diff_handler.Run(
            {rom.filename(), sandbox_or->rom_path.string()});
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

absl::Status HandleDescribeCommand(const std::vector<std::string>& arg_vec) {
    ASSIGN_OR_RETURN(auto options, ParseDescribeArgs(arg_vec));

    const auto& catalog = ResourceCatalog::Instance();
    std::optional<ResourceSchema> resource_schema;
    if (options.resource.has_value()) {
        auto resource_or = catalog.GetResource(*options.resource);
        if (!resource_or.ok()) {
            return resource_or.status();
        }
        resource_schema = resource_or.value();
    }

    std::string payload;
    if (options.format == "json") {
        if (resource_schema.has_value()) {
            payload = catalog.SerializeResource(*resource_schema);
        } else {
            payload = catalog.SerializeResources(catalog.AllResources());
        }
    } else {
        std::string last_updated = options.last_updated.has_value()
                                                                     ? *options.last_updated
                                                                     : absl::FormatTime("%Y-%m-%d", absl::Now(),
                                                                                                            absl::LocalTimeZone());
        if (resource_schema.has_value()) {
            std::vector<ResourceSchema> schemas{*resource_schema};
            payload = catalog.SerializeResourcesAsYaml(
                    schemas, options.version, last_updated);
        } else {
            payload = catalog.SerializeResourcesAsYaml(
                    catalog.AllResources(), options.version, last_updated);
        }
    }

    if (options.output_path.has_value()) {
        std::ofstream out(*options.output_path, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            return absl::InternalError(absl::StrFormat(
                    "Failed to open %s for writing", *options.output_path));
        }
        out << payload;
        out.close();
        if (!out) {
            return absl::InternalError(absl::StrFormat(
                    "Failed to write schema to %s", *options.output_path));
        }
        std::cout << absl::StrFormat("Wrote %s schema to %s", options.format,
                                                                 *options.output_path)
                            << std::endl;
        return absl::OkStatus();
    }

    std::cout << payload << std::endl;
    return absl::OkStatus();
}

} // namespace

absl::Status Agent::Run(const std::vector<std::string>& arg_vec) {
  if (arg_vec.empty()) {
        return absl::InvalidArgumentError(
                "Usage: agent <run|plan|diff|test|learn|commit|revert|describe> [options]");
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
    } else if (subcommand == "describe") {
        return HandleDescribeCommand(subcommand_args);
  } else {
    return absl::InvalidArgumentError("Invalid subcommand for agent command.");
  }

  return absl::OkStatus();
}

}  // namespace cli
}  // namespace yaze
