#include "cli/z3ed.h"
#include "cli/modern_cli.h"
#include "cli/service/ai_service.h"
#include "cli/service/proposal_registry.h"
#include "cli/service/resource_catalog.h"
#include "cli/service/rom_sandbox_manager.h"
#ifdef YAZE_WITH_GRPC
#include "cli/service/gui_automation_client.h"
#include "cli/service/test_workflow_generator.h"
#endif
#include "util/macro.h"

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_replace.h"
#include "absl/time/time.h"

#include <cstdlib>  // For EXIT_FAILURE
#include <fstream>
#include <optional>
#include <thread>

#include <algorithm>

// Declare the rom flag so we can access it
ABSL_DECLARE_FLAG(std::string, rom);

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

std::string FormatOptionalTime(const std::optional<absl::Time>& time) {
    if (!time.has_value()) {
        return "n/a";
    }
    return absl::FormatTime("%Y-%m-%dT%H:%M:%SZ", *time, absl::UTCTimeZone());
}

std::string TestRunStatusToString(TestRunStatus status) {
    switch (status) {
        case TestRunStatus::kQueued:
            return "QUEUED";
        case TestRunStatus::kRunning:
            return "RUNNING";
        case TestRunStatus::kPassed:
            return "PASSED";
        case TestRunStatus::kFailed:
            return "FAILED";
        case TestRunStatus::kTimeout:
            return "TIMEOUT";
        case TestRunStatus::kUnknown:
        default:
            return "UNKNOWN";
    }
}

bool IsTerminalStatus(TestRunStatus status) {
    switch (status) {
        case TestRunStatus::kQueued:
        case TestRunStatus::kRunning:
            return false;
        case TestRunStatus::kPassed:
        case TestRunStatus::kFailed:
        case TestRunStatus::kTimeout:
        case TestRunStatus::kUnknown:
        default:
            return true;
    }
}

std::optional<TestRunStatus> ParseStatusFilter(absl::string_view value) {
    std::string lower = std::string(absl::AsciiStrToLower(value));
    if (lower == "queued") return TestRunStatus::kQueued;
    if (lower == "running") return TestRunStatus::kRunning;
    if (lower == "passed") return TestRunStatus::kPassed;
    if (lower == "failed") return TestRunStatus::kFailed;
    if (lower == "timeout") return TestRunStatus::kTimeout;
    if (lower == "unknown") return TestRunStatus::kUnknown;
    return std::nullopt;
}

std::string HarnessAddress(const std::string& host, int port) {
    return absl::StrFormat("%s:%d", host, port);
}

std::string JsonEscape(absl::string_view value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (unsigned char c : value) {
        switch (c) {
            case '\\':
                out += "\\\\";
                break;
            case '"':
                out += "\\\"";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (c < 0x20) {
                    absl::StrAppend(&out, absl::StrFormat("\\\\u%04X", static_cast<int>(c)));
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    return out;
}

std::string YamlQuote(absl::string_view value) {
    std::string escaped(value);
    absl::StrReplaceAll({{"\\", "\\\\"}, {"\"", "\\\""}}, &escaped);
    return absl::StrCat("\"", escaped, "\"");
}

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
    
    // Load ROM if not already loaded
    if (!rom.is_loaded()) {
        std::string rom_path = absl::GetFlag(FLAGS_rom);
        if (rom_path.empty()) {
            return absl::FailedPreconditionError(
                "No ROM loaded. Use --rom=<path> to specify ROM file.\n"
                "Example: z3ed agent run --rom=zelda3.sfc --prompt \"Your prompt here\"");
        }
        
        auto status = rom.LoadFromFile(rom_path);
        if (!status.ok()) {
            return absl::FailedPreconditionError(
                absl::StrFormat("Failed to load ROM from '%s': %s", 
                                rom_path, status.message()));
        }
    }

    auto sandbox_or = RomSandboxManager::Instance().CreateSandbox(
        rom, "agent-run");
    if (!sandbox_or.ok()) {
        return sandbox_or.status();
    }
    auto sandbox = sandbox_or.value();

    // Create a proposal to track this agent run
    auto proposal_or = ProposalRegistry::Instance().CreateProposal(
        sandbox.id, prompt, "Agent-generated ROM modifications");
    if (!proposal_or.ok()) {
        return proposal_or.status();
    }
    auto proposal = proposal_or.value();

    // Log the start of execution
    RETURN_IF_ERROR(ProposalRegistry::Instance().AppendLog(
        proposal.id, absl::StrCat("Starting agent run with prompt: ", prompt)));

    MockAIService ai_service;
    auto commands_or = ai_service.GetCommands(prompt);
    if (!commands_or.ok()) {
        RETURN_IF_ERROR(ProposalRegistry::Instance().AppendLog(
            proposal.id, absl::StrCat("AI service error: ", commands_or.status().message())));
        return commands_or.status();
    }
    std::vector<std::string> commands = commands_or.value();

    // Log the planned commands
    RETURN_IF_ERROR(ProposalRegistry::Instance().AppendLog(
        proposal.id, absl::StrCat("Generated ", commands.size(), " commands")));

    ModernCLI cli;
    int commands_executed = 0;
    for (const auto& command : commands) {
        RETURN_IF_ERROR(ProposalRegistry::Instance().AppendLog(
            proposal.id, absl::StrCat("Executing: ", command)));

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
                RETURN_IF_ERROR(ProposalRegistry::Instance().AppendLog(
                    proposal.id, absl::StrCat("Command failed: ", status.message())));
                return status;
            }
            commands_executed++;
            RETURN_IF_ERROR(ProposalRegistry::Instance().AppendLog(
                proposal.id, "Command succeeded"));
        } else {
            auto error_msg = absl::StrCat("Unknown command: ", cmd_name);
            RETURN_IF_ERROR(ProposalRegistry::Instance().AppendLog(
                proposal.id, error_msg));
            return absl::NotFoundError(error_msg);
        }
    }

    // Update proposal with execution stats
    RETURN_IF_ERROR(ProposalRegistry::Instance().AppendLog(
        proposal.id, absl::StrCat("Completed execution of ", commands_executed, " commands")));

    std::cout << "✅ Agent run completed successfully." << std::endl;
    std::cout << "   Proposal ID: " << proposal.id << std::endl;
    std::cout << "   Sandbox: " << sandbox.rom_path << std::endl;
    std::cout << "   Use 'z3ed agent diff' to review changes" << std::endl;

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

absl::Status HandleDiffCommand(Rom& rom, const std::vector<std::string>& args) {
    // Parse optional --proposal-id flag
    std::optional<std::string> proposal_id;
    for (size_t i = 0; i < args.size(); ++i) {
        const std::string& token = args[i];
        if (absl::StartsWith(token, "--proposal-id=")) {
            proposal_id = token.substr(14);  // Length of "--proposal-id="
        } else if (token == "--proposal-id" && i + 1 < args.size()) {
            proposal_id = args[i + 1];
            ++i;
        }
    }
    
    auto& registry = ProposalRegistry::Instance();
    absl::StatusOr<ProposalRegistry::ProposalMetadata> proposal_or;
    
    // Get specific proposal or latest pending
    if (proposal_id.has_value()) {
        proposal_or = registry.GetProposal(proposal_id.value());
    } else {
        proposal_or = registry.GetLatestPendingProposal();
    }
    
    if (proposal_or.ok()) {
        const auto& proposal = proposal_or.value();
        
        std::cout << "\n=== Proposal Diff ===\n";
        std::cout << "Proposal ID: " << proposal.id << "\n";
        std::cout << "Sandbox ID: " << proposal.sandbox_id << "\n";
        std::cout << "Prompt: " << proposal.prompt << "\n";
        std::cout << "Description: " << proposal.description << "\n";
        std::cout << "Status: ";
        switch (proposal.status) {
            case ProposalRegistry::ProposalStatus::kPending:
                std::cout << "Pending";
                break;
            case ProposalRegistry::ProposalStatus::kAccepted:
                std::cout << "Accepted";
                break;
            case ProposalRegistry::ProposalStatus::kRejected:
                std::cout << "Rejected";
                break;
        }
        std::cout << "\n";
        std::cout << "Created: " << absl::FormatTime(proposal.created_at) << "\n";
        std::cout << "Commands Executed: " << proposal.commands_executed << "\n";
        std::cout << "Bytes Changed: " << proposal.bytes_changed << "\n\n";
        
        // Read and display the diff file
        if (std::filesystem::exists(proposal.diff_path)) {
            std::cout << "--- Diff Content ---\n";
            std::ifstream diff_file(proposal.diff_path);
            if (diff_file.is_open()) {
                std::string line;
                while (std::getline(diff_file, line)) {
                    std::cout << line << "\n";
                }
                diff_file.close();
            } else {
                std::cout << "(Unable to read diff file)\n";
            }
        } else {
            std::cout << "(No diff file found)\n";
        }
        
        // Display execution log summary
        std::cout << "\n--- Execution Log ---\n";
        if (std::filesystem::exists(proposal.log_path)) {
            std::ifstream log_file(proposal.log_path);
            if (log_file.is_open()) {
                std::string line;
                int line_count = 0;
                while (std::getline(log_file, line)) {
                    std::cout << line << "\n";
                    line_count++;
                    if (line_count > 50) {  // Limit output for readability
                        std::cout << "... (log truncated, see " << proposal.log_path << " for full output)\n";
                        break;
                    }
                }
                log_file.close();
            } else {
                std::cout << "(Unable to read log file)\n";
            }
        } else {
            std::cout << "(No log file found)\n";
        }
        
        // Display next steps
        std::cout << "\n=== Next Steps ===\n";
        std::cout << "To accept changes: z3ed agent commit\n";
        std::cout << "To reject changes: z3ed agent revert\n";
        std::cout << "To review in GUI: yaze --proposal=" << proposal.id << "\n";
        
        return absl::OkStatus();
    }
    
    // Fallback to old behavior if no proposal found
    if (rom.is_loaded()) {
        auto sandbox_or = RomSandboxManager::Instance().ActiveSandbox();
        if (!sandbox_or.ok()) {
            return absl::NotFoundError(
                "No pending proposals found and no active sandbox. "
                "Run 'z3ed agent run' first.");
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

absl::Status HandleTestRunCommand(const std::vector<std::string>& arg_vec) {
    // Parse arguments
    std::string prompt;
    std::string host = "localhost";
    int port = 50052;
    int timeout_sec = 30;
    
    for (size_t i = 0; i < arg_vec.size(); ++i) {
        const std::string& token = arg_vec[i];
        
        if (token == "--prompt" && i + 1 < arg_vec.size()) {
            prompt = arg_vec[++i];
        } else if (token == "--host" && i + 1 < arg_vec.size()) {
            host = arg_vec[++i];
        } else if (token == "--port" && i + 1 < arg_vec.size()) {
            port = std::stoi(arg_vec[++i]);
        } else if (token == "--timeout" && i + 1 < arg_vec.size()) {
            timeout_sec = std::stoi(arg_vec[++i]);
        } else if (absl::StartsWith(token, "--prompt=")) {
            prompt = token.substr(9);
        } else if (absl::StartsWith(token, "--host=")) {
            host = token.substr(7);
        } else if (absl::StartsWith(token, "--port=")) {
            port = std::stoi(token.substr(7));
        } else if (absl::StartsWith(token, "--timeout=")) {
            timeout_sec = std::stoi(token.substr(10));
        }
    }
    
    if (prompt.empty()) {
        return absl::InvalidArgumentError(
            "Usage: agent test --prompt \"<prompt>\" [--host <host>] [--port <port>] [--timeout <sec>]\n\n"
            "Examples:\n"
            "  z3ed agent test --prompt \"Open Overworld editor\"\n"
            "  z3ed agent test --prompt \"Open Dungeon editor and verify it loads\"\n"
            "  z3ed agent test --prompt \"Click Open ROM button\"");
    }
    
#ifndef YAZE_WITH_GRPC
    return absl::UnimplementedError(
        "GUI automation requires YAZE_WITH_GRPC=ON at build time.\n"
        "Rebuild with: cmake -B build -DYAZE_WITH_GRPC=ON");
#else
    std::cout << "\n=== GUI Automation Test ===\n";
    std::cout << "Prompt: " << prompt << "\n";
    std::cout << "Server: " << host << ":" << port << "\n\n";
    
    // Generate workflow from prompt
    TestWorkflowGenerator generator;
    auto workflow_or = generator.GenerateWorkflow(prompt);
    if (!workflow_or.ok()) {
        return workflow_or.status();
    }
    auto workflow = workflow_or.value();
    
    std::cout << "Generated workflow:\n" << workflow.ToString() << "\n";
    
    // Connect to test harness
    GuiAutomationClient client(HarnessAddress(host, port));
    auto connect_status = client.Connect();
    if (!connect_status.ok()) {
        return absl::UnavailableError(
            absl::StrFormat(
                "Failed to connect to test harness at %s:%d\n"
                "Make sure YAZE is running with:\n"
                "  ./yaze --enable_test_harness --test_harness_port=%d --rom_file=<rom>\n\n"
                "Error: %s",
                host, port, port, connect_status.message()));
    }
    
    std::cout << "✓ Connected to test harness\n\n";
    
    // Execute workflow
    auto start_time = std::chrono::steady_clock::now();
    int step_num = 0;
    std::vector<std::string> emitted_test_ids;
    
    for (const auto& step : workflow.steps) {
        step_num++;
        std::cout << absl::StrFormat("[%d/%d] %s ... ", step_num,
                                     workflow.steps.size(), step.ToString());
        std::cout.flush();
        
        absl::StatusOr<AutomationResult> result;
        
        switch (step.type) {
            case TestStepType::kClick:
                result = client.Click(step.target);
                break;
            case TestStepType::kType:
                result = client.Type(step.target, step.text, step.clear_first);
                break;
            case TestStepType::kWait:
                result = client.Wait(step.condition, step.timeout_ms);
                break;
            case TestStepType::kAssert:
                result = client.Assert(step.condition);
                break;
            case TestStepType::kScreenshot:
                result = client.Screenshot();
                break;
        }
        
        if (!result.ok()) {
            std::cout << "✗ FAILED\n";
            return absl::InternalError(
                absl::StrFormat("Step %d failed: %s", step_num,
                                result.status().message()));
        }
        
        if (!result->success) {
            std::cout << "✗ FAILED\n";
            std::cout << "  Error: " << result->message << "\n";
            return absl::InternalError(
                absl::StrFormat("Step %d failed: %s", step_num, result->message));
        }
        
        std::cout << absl::StrFormat("✓ (%lldms)",
                                     result->execution_time.count());
        if (!result->test_id.empty()) {
            std::cout << " [Test ID: " << result->test_id << "]";
            emitted_test_ids.push_back(result->test_id);
        }
        std::cout << "\n";
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    std::cout << "\n✅ Test passed in " << elapsed.count() << "ms\n";

    if (!emitted_test_ids.empty()) {
        std::cout << "Latest Test ID: " << emitted_test_ids.back() << "\n";
        if (emitted_test_ids.size() > 1) {
            std::cout << "Captured Test IDs:\n";
            for (const auto& id : emitted_test_ids) {
                std::cout << "  - " << id << "\n";
            }
        }
        std::cout << "Use 'z3ed agent test status --test-id " << emitted_test_ids.back()
                  << "' for live status updates." << std::endl;
    }

    return absl::OkStatus();
#endif
}

absl::Status HandleTestStatusCommand(const std::vector<std::string>& arg_vec) {
    std::string host = "localhost";
    int port = 50052;
    std::string test_id;
    bool follow = false;
    int interval_ms = 1000;

    for (size_t i = 0; i < arg_vec.size(); ++i) {
        const std::string& token = arg_vec[i];

        if (token == "--test-id" && i + 1 < arg_vec.size()) {
            test_id = arg_vec[++i];
        } else if (absl::StartsWith(token, "--test-id=")) {
            test_id = token.substr(10);
        } else if (token == "--host" && i + 1 < arg_vec.size()) {
            host = arg_vec[++i];
        } else if (absl::StartsWith(token, "--host=")) {
            host = token.substr(7);
        } else if (token == "--port" && i + 1 < arg_vec.size()) {
            port = std::stoi(arg_vec[++i]);
        } else if (absl::StartsWith(token, "--port=")) {
            port = std::stoi(token.substr(7));
        } else if (token == "--follow") {
            follow = true;
        } else if ((token == "--interval" || token == "--interval-ms") &&
                   i + 1 < arg_vec.size()) {
            interval_ms = std::max(100, std::stoi(arg_vec[++i]));
        } else if (absl::StartsWith(token, "--interval=") ||
                   absl::StartsWith(token, "--interval-ms=")) {
            size_t prefix = token.find('=');
            interval_ms = std::max(100, std::stoi(token.substr(prefix + 1)));
        }
    }

    if (test_id.empty()) {
        return absl::InvalidArgumentError(
            "Usage: agent test status --test-id <id> [--follow] [--host <host>] [--port <port>] [--interval-ms <ms>]");
    }

#ifndef YAZE_WITH_GRPC
    return absl::UnimplementedError(
        "GUI automation requires YAZE_WITH_GRPC=ON at build time.\n"
        "Rebuild with: cmake -B build -DYAZE_WITH_GRPC=ON");
#else
    GuiAutomationClient client(HarnessAddress(host, port));
    RETURN_IF_ERROR(client.Connect());

    std::cout << "\n=== Test Status ===\n";
    std::cout << "Test ID: " << test_id << "\n";
    std::cout << "Server: " << HarnessAddress(host, port) << "\n";
    if (follow) {
        std::cout << "Follow mode: polling every " << interval_ms << "ms\n";
    }
    std::cout << "\n";

    bool first_iteration = true;
    while (true) {
        ASSIGN_OR_RETURN(auto details, client.GetTestStatus(test_id));

        if (!first_iteration) {
            std::cout << "---\n";
        }

        std::cout << "Status: " << TestRunStatusToString(details.status) << "\n";
        std::cout << "Queued At: " << FormatOptionalTime(details.queued_at) << "\n";
        std::cout << "Started At: " << FormatOptionalTime(details.started_at) << "\n";
        std::cout << "Completed At: " << FormatOptionalTime(details.completed_at) << "\n";
        std::cout << "Execution Time (ms): " << details.execution_time_ms << "\n";
        if (!details.error_message.empty()) {
            std::cout << "Error: " << details.error_message << "\n";
        }

        if (!details.assertion_failures.empty()) {
            std::cout << "Assertion Failures (" << details.assertion_failures.size()
                      << "):\n";
            for (const auto& failure : details.assertion_failures) {
                std::cout << "  - " << failure << "\n";
            }
        } else {
            std::cout << "Assertion Failures: 0\n";
        }

        if (!follow || IsTerminalStatus(details.status)) {
            break;
        }

        first_iteration = false;
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }

    return absl::OkStatus();
#endif
}

absl::Status HandleTestListCommand(const std::vector<std::string>& arg_vec) {
    std::string host = "localhost";
    int port = 50052;
    std::string category_filter;
    std::optional<TestRunStatus> status_filter;
    int page_size = 100;
    int limit = -1;
    bool fetch_all = false;

    for (size_t i = 0; i < arg_vec.size(); ++i) {
        const std::string& token = arg_vec[i];

        if (token == "--host" && i + 1 < arg_vec.size()) {
            host = arg_vec[++i];
        } else if (absl::StartsWith(token, "--host=")) {
            host = token.substr(7);
        } else if (token == "--port" && i + 1 < arg_vec.size()) {
            port = std::stoi(arg_vec[++i]);
        } else if (absl::StartsWith(token, "--port=")) {
            port = std::stoi(token.substr(7));
        } else if (token == "--category" && i + 1 < arg_vec.size()) {
            category_filter = arg_vec[++i];
        } else if (absl::StartsWith(token, "--category=")) {
            category_filter = token.substr(11);
        } else if (token == "--status" && i + 1 < arg_vec.size()) {
            auto parsed = ParseStatusFilter(arg_vec[++i]);
            if (!parsed.has_value()) {
                return absl::InvalidArgumentError(
                    "Invalid status filter. Expected: queued, running, passed, failed, timeout, unknown");
            }
            status_filter = parsed;
        } else if (absl::StartsWith(token, "--status=")) {
            auto parsed = ParseStatusFilter(token.substr(9));
            if (!parsed.has_value()) {
                return absl::InvalidArgumentError(
                    "Invalid status filter. Expected: queued, running, passed, failed, timeout, unknown");
            }
            status_filter = parsed;
        } else if (token == "--page-size" && i + 1 < arg_vec.size()) {
            page_size = std::max(1, std::stoi(arg_vec[++i]));
        } else if (absl::StartsWith(token, "--page-size=")) {
            page_size = std::max(1, std::stoi(token.substr(12)));
        } else if (token == "--limit" && i + 1 < arg_vec.size()) {
            limit = std::stoi(arg_vec[++i]);
        } else if (absl::StartsWith(token, "--limit=")) {
            limit = std::stoi(token.substr(8));
        } else if (token == "--all") {
            fetch_all = true;
        }
    }

    if (fetch_all) {
        limit = -1;
    }

#ifndef YAZE_WITH_GRPC
    return absl::UnimplementedError(
        "GUI automation requires YAZE_WITH_GRPC=ON at build time.\n"
        "Rebuild with: cmake -B build -DYAZE_WITH_GRPC=ON");
#else
    GuiAutomationClient client(HarnessAddress(host, port));
    RETURN_IF_ERROR(client.Connect());

    std::cout << "\n=== Harness Test Catalog ===\n";
    std::cout << "Server: " << HarnessAddress(host, port) << "\n";
    if (!category_filter.empty()) {
        std::cout << "Category filter: " << category_filter << "\n";
    }
    if (status_filter.has_value()) {
        std::cout << "Status filter: "
                  << TestRunStatusToString(status_filter.value()) << "\n";
    }
    std::cout << "\n";

    std::vector<HarnessTestSummary> collected;
    collected.reserve(limit > 0 ? limit : page_size);
    std::string page_token;
    int total_count = 0;

    while (true) {
        int request_page_size = page_size > 0 ? page_size : 100;
        if (limit > 0) {
            int remaining = limit - static_cast<int>(collected.size());
            if (remaining <= 0) {
                break;
            }
            request_page_size = std::min(request_page_size, remaining);
        }

        ASSIGN_OR_RETURN(auto batch,
                         client.ListTests(category_filter, request_page_size,
                                          page_token));

        total_count = batch.total_count;

        for (const auto& summary : batch.tests) {
            if (status_filter.has_value()) {
                ASSIGN_OR_RETURN(auto details,
                                 client.GetTestStatus(summary.test_id));
                if (details.status != status_filter.value()) {
                    continue;
                }
            }

            collected.push_back(summary);
            if (limit > 0 && static_cast<int>(collected.size()) >= limit) {
                break;
            }
        }

        if (limit > 0 && static_cast<int>(collected.size()) >= limit) {
            break;
        }

        if (batch.next_page_token.empty()) {
            break;
        }
        page_token = batch.next_page_token;
    }

    if (collected.empty()) {
        std::cout << "No tests found for the specified filters." << std::endl;
        return absl::OkStatus();
    }

    for (const auto& summary : collected) {
        std::cout << "Test ID: " << summary.test_id << "\n";
        std::cout << "  Name: " << summary.name << "\n";
        std::cout << "  Category: " << summary.category << "\n";
        std::cout << "  Last Run: " << FormatOptionalTime(summary.last_run_at)
                  << "\n";
        std::cout << "  Runs: " << summary.total_runs << " ("
                  << summary.pass_count << " pass / " << summary.fail_count
                  << " fail)\n";
        std::cout << "  Average Duration (ms): " << summary.average_duration_ms
                  << "\n\n";
    }

    std::cout << "Displayed " << collected.size() << " test(s)";
    if (total_count > 0) {
        std::cout << " (catalog size: " << total_count << ")";
    }
    std::cout << "." << std::endl;

    return absl::OkStatus();
#endif
}

absl::Status HandleTestResultsCommand(const std::vector<std::string>& arg_vec) {
    std::string host = "localhost";
    int port = 50052;
    std::string test_id;
    bool include_logs = false;
    std::string format = "yaml";

    for (size_t i = 0; i < arg_vec.size(); ++i) {
        const std::string& token = arg_vec[i];

        if (token == "--test-id" && i + 1 < arg_vec.size()) {
            test_id = arg_vec[++i];
        } else if (absl::StartsWith(token, "--test-id=")) {
            test_id = token.substr(10);
        } else if (token == "--host" && i + 1 < arg_vec.size()) {
            host = arg_vec[++i];
        } else if (absl::StartsWith(token, "--host=")) {
            host = token.substr(7);
        } else if (token == "--port" && i + 1 < arg_vec.size()) {
            port = std::stoi(arg_vec[++i]);
        } else if (absl::StartsWith(token, "--port=")) {
            port = std::stoi(token.substr(7));
        } else if (token == "--include-logs") {
            include_logs = true;
        } else if (token == "--format" && i + 1 < arg_vec.size()) {
            format = absl::AsciiStrToLower(arg_vec[++i]);
        } else if (absl::StartsWith(token, "--format=")) {
            format = absl::AsciiStrToLower(token.substr(9));
        }
    }

    if (test_id.empty()) {
        return absl::InvalidArgumentError(
            "Usage: agent test results --test-id <id> [--include-logs] [--format yaml|json] [--host <host>] [--port <port>]");
    }

    if (format != "yaml" && format != "json") {
        return absl::InvalidArgumentError("--format must be either 'yaml' or 'json'");
    }

#ifndef YAZE_WITH_GRPC
    return absl::UnimplementedError(
        "GUI automation requires YAZE_WITH_GRPC=ON at build time.\n"
        "Rebuild with: cmake -B build -DYAZE_WITH_GRPC=ON");
#else
    GuiAutomationClient client(HarnessAddress(host, port));
    RETURN_IF_ERROR(client.Connect());

    ASSIGN_OR_RETURN(auto details,
                     client.GetTestResults(test_id, include_logs));

    if (format == "json") {
        std::cout << "{\n";
        std::cout << "  \"test_id\": \"" << JsonEscape(details.test_id)
                  << "\",\n";
        std::cout << "  \"success\": " << (details.success ? "true" : "false")
                  << ",\n";
        std::cout << "  \"name\": \"" << JsonEscape(details.test_name)
                  << "\",\n";
        std::cout << "  \"category\": \"" << JsonEscape(details.category)
                  << "\",\n";
        std::cout << "  \"executed_at\": \""
                  << JsonEscape(FormatOptionalTime(details.executed_at))
                  << "\",\n";
        std::cout << "  \"duration_ms\": " << details.duration_ms << ",\n";

        std::cout << "  \"assertions\": ";
        if (details.assertions.empty()) {
            std::cout << "[],\n";
        } else {
            std::cout << "[\n";
            for (size_t i = 0; i < details.assertions.size(); ++i) {
                const auto& assertion = details.assertions[i];
                std::cout << "    {\"description\": \""
                          << JsonEscape(assertion.description)
                          << "\", \"passed\": "
                          << (assertion.passed ? "true" : "false");
                if (!assertion.expected_value.empty()) {
                    std::cout << ", \"expected\": \""
                              << JsonEscape(assertion.expected_value) << "\"";
                }
                if (!assertion.actual_value.empty()) {
                    std::cout << ", \"actual\": \""
                              << JsonEscape(assertion.actual_value) << "\"";
                }
                if (!assertion.error_message.empty()) {
                    std::cout << ", \"error\": \""
                              << JsonEscape(assertion.error_message) << "\"";
                }
                std::cout << "}";
                if (i + 1 < details.assertions.size()) {
                    std::cout << ",";
                }
                std::cout << "\n";
            }
            std::cout << "  ],\n";
        }

        std::cout << "  \"logs\": ";
        if (include_logs && !details.logs.empty()) {
            std::cout << "[\n";
            for (size_t i = 0; i < details.logs.size(); ++i) {
                std::cout << "    \"" << JsonEscape(details.logs[i]) << "\"";
                if (i + 1 < details.logs.size()) {
                    std::cout << ",";
                }
                std::cout << "\n";
            }
            std::cout << "  ],\n";
        } else {
            std::cout << "[],\n";
        }

        std::cout << "  \"metrics\": ";
        if (!details.metrics.empty()) {
            std::cout << "{\n";
            size_t index = 0;
            for (const auto& [key, value] : details.metrics) {
                std::cout << "    \"" << JsonEscape(key) << "\": " << value;
                if (index + 1 < details.metrics.size()) {
                    std::cout << ",";
                }
                std::cout << "\n";
                ++index;
            }
            std::cout << "  }\n";
        } else {
            std::cout << "{}\n";
        }

        std::cout << "}" << std::endl;
    } else {
        std::cout << "test_id: " << details.test_id << "\n";
        std::cout << "success: " << (details.success ? "true" : "false")
                  << "\n";
        std::cout << "name: " << YamlQuote(details.test_name) << "\n";
        std::cout << "category: " << YamlQuote(details.category) << "\n";
        std::cout << "executed_at: " << FormatOptionalTime(details.executed_at)
                  << "\n";
        std::cout << "duration_ms: " << details.duration_ms << "\n";

        if (details.assertions.empty()) {
            std::cout << "assertions: []\n";
        } else {
            std::cout << "assertions:\n";
            for (const auto& assertion : details.assertions) {
                std::cout << "  - description: "
                          << YamlQuote(assertion.description) << "\n";
                std::cout << "    passed: "
                          << (assertion.passed ? "true" : "false") << "\n";
                if (!assertion.expected_value.empty()) {
                    std::cout << "    expected: "
                              << YamlQuote(assertion.expected_value) << "\n";
                }
                if (!assertion.actual_value.empty()) {
                    std::cout << "    actual: "
                              << YamlQuote(assertion.actual_value) << "\n";
                }
                if (!assertion.error_message.empty()) {
                    std::cout << "    error: "
                              << YamlQuote(assertion.error_message) << "\n";
                }
            }
        }

        if (include_logs && !details.logs.empty()) {
            std::cout << "logs:\n";
            for (const auto& log : details.logs) {
                std::cout << "  - " << YamlQuote(log) << "\n";
            }
        } else {
            std::cout << "logs: []\n";
        }

        if (details.metrics.empty()) {
            std::cout << "metrics: {}\n";
        } else {
            std::cout << "metrics:\n";
            for (const auto& [key, value] : details.metrics) {
                std::cout << "  " << key << ": " << value << "\n";
            }
        }
    }

    return absl::OkStatus();
#endif
}

absl::Status HandleTestCommand(const std::vector<std::string>& arg_vec) {
    if (!arg_vec.empty()) {
        const std::string& subcommand = arg_vec[0];
        std::vector<std::string> tail(arg_vec.begin() + 1, arg_vec.end());

        if (subcommand == "status") {
            return HandleTestStatusCommand(tail);
        }
        if (subcommand == "list") {
            return HandleTestListCommand(tail);
        }
        if (subcommand == "results") {
            return HandleTestResultsCommand(tail);
        }
    }

    return HandleTestRunCommand(arg_vec);
}

absl::Status HandleLearnCommand() {
    std::cout << "Agent learn not yet implemented." << std::endl;
    return absl::OkStatus();
}

absl::Status HandleListCommand() {
    auto& registry = ProposalRegistry::Instance();
    auto proposals = registry.ListProposals();
    
    if (proposals.empty()) {
        std::cout << "No proposals found.\n";
        std::cout << "Run 'z3ed agent run --prompt \"...\"' to create a proposal.\n";
        return absl::OkStatus();
    }
    
    std::cout << "\n=== Agent Proposals ===\n\n";
    
    for (const auto& proposal : proposals) {
        std::cout << "ID: " << proposal.id << "\n";
        std::cout << "  Status: ";
        switch (proposal.status) {
            case ProposalRegistry::ProposalStatus::kPending:
                std::cout << "Pending";
                break;
            case ProposalRegistry::ProposalStatus::kAccepted:
                std::cout << "Accepted";
                break;
            case ProposalRegistry::ProposalStatus::kRejected:
                std::cout << "Rejected";
                break;
        }
        std::cout << "\n";
        std::cout << "  Created: " << absl::FormatTime(proposal.created_at) << "\n";
        std::cout << "  Prompt: " << proposal.prompt << "\n";
        std::cout << "  Commands: " << proposal.commands_executed << "\n";
        std::cout << "  Bytes Changed: " << proposal.bytes_changed << "\n";
        std::cout << "\n";
    }
    
    std::cout << "Total: " << proposals.size() << " proposal(s)\n";
    std::cout << "\nUse 'z3ed agent diff --proposal-id=<id>' to view details.\n";
    
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
                "Usage: agent <run|plan|diff|test|learn|list|commit|revert|describe> [options]");
  }

  std::string subcommand = arg_vec[0];
  std::vector<std::string> subcommand_args(arg_vec.begin() + 1, arg_vec.end());

  if (subcommand == "run") {
    return HandleRunCommand(subcommand_args, rom_);
  } else if (subcommand == "plan") {
    return HandlePlanCommand(subcommand_args);
  } else if (subcommand == "diff") {
    return HandleDiffCommand(rom_, subcommand_args);
  } else if (subcommand == "test") {
    return HandleTestCommand(subcommand_args);
  } else if (subcommand == "learn") {
    return HandleLearnCommand();
  } else if (subcommand == "list") {
    return HandleListCommand();
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
