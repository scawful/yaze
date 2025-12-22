#include <vector>
#include <string>
#include <iostream>
#include <thread>
#include <functional>
#include <mutex>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "cli/cli.h"
#include "cli/service/command_registry.h"
#include "cli/handlers/agent/todo_commands.h"
#include "cli/service/ai/browser_ai_service.h"
#include "cli/service/agent/conversational_agent_service.h"
#include "cli/service/resources/resource_catalog.h"
#include "rom/rom.h"

namespace yaze {
namespace cli {

// Accessor for the global AI service provided by wasm_terminal_bridge
BrowserAIService* GetGlobalBrowserAIService();

// Accessor for the global ROM from wasm_terminal_bridge.cc
extern Rom* GetGlobalRom();

namespace handlers {

namespace {

// Static history for the session
std::vector<agent::ChatMessage> g_chat_history;
std::mutex g_chat_mutex;

// Simple in-memory storage for the last generated plan
// In a full implementation, this would be a full ProposalRegistry
std::string g_pending_plan;
std::mutex g_plan_mutex;

absl::Status HandleChatCommand(const std::vector<std::string>& args) {
  auto* service = GetGlobalBrowserAIService();
  if (!service) {
    return absl::FailedPreconditionError(
        "AI Service not initialized. Please set an API key using the settings menu.");
  }

  if (args.empty()) {
    return absl::InvalidArgumentError("Please provide a message. Usage: agent chat <message>");
  }

  std::string prompt = absl::StrJoin(args, " ");

  {
    std::lock_guard<std::mutex> lock(g_chat_mutex);
    // Add user message to history
    agent::ChatMessage user_msg;
    user_msg.sender = agent::ChatMessage::Sender::kUser;
    user_msg.message = prompt;
    user_msg.timestamp = absl::Now();
    g_chat_history.push_back(user_msg);
  }

  std::cout << "Thinking... (This may take a few seconds)" << std::endl;

  // Run AI request in a background thread
  std::thread([service]() {
    std::vector<agent::ChatMessage> history_copy;
    {
      std::lock_guard<std::mutex> lock(g_chat_mutex);
      history_copy = g_chat_history;
    }

    // Generate response using history
    auto response_or = service->GenerateResponse(history_copy);
    
    if (!response_or.ok()) {
      std::cerr << "\nError: " << response_or.status().message() << "\n" << std::endl;
      // Remove the failed user message so we don't get stuck in a bad state
      std::lock_guard<std::mutex> lock(g_chat_mutex);
      if (!g_chat_history.empty() && 
          g_chat_history.back().sender == agent::ChatMessage::Sender::kUser) {
        g_chat_history.pop_back();
      }
      return;
    }

    auto response = response_or.value();

    {
      std::lock_guard<std::mutex> lock(g_chat_mutex);
      // Add agent response to history
      agent::ChatMessage agent_msg;
      agent_msg.sender = agent::ChatMessage::Sender::kAgent;
      agent_msg.message = response.text_response;
      agent_msg.timestamp = absl::Now();
      g_chat_history.push_back(agent_msg);
    }

    // Print response safely
    // Note: In Emscripten, printf/cout from threads is proxied to main thread
    std::cout << "\nAgent: " << response.text_response << "\n" << std::endl;
  }).detach();

  return absl::OkStatus();
}

absl::Status HandlePlanCommand(const std::vector<std::string>& args) {
  auto* service = GetGlobalBrowserAIService();
  if (!service) {
    return absl::FailedPreconditionError("AI Service not initialized.");
  }

  if (args.empty()) {
    return absl::InvalidArgumentError("Usage: agent plan <task description>");
  }

  std::string task = absl::StrJoin(args, " ");
  std::string prompt = "Create a detailed step-by-step implementation plan for the following ROM hacking task. "
                       "Do not execute it yet, just plan it.\nTask: " + task;

  std::cout << "Generating plan... (Check back in a moment)" << std::endl;

  std::thread([service, prompt]() {
    auto response_or = service->GenerateResponse(prompt);
    if (!response_or.ok()) {
      std::cerr << "\nPlan Error: " << response_or.status().message() << "\n" << std::endl;
      return;
    }

    {
      std::lock_guard<std::mutex> lock(g_plan_mutex);
      g_pending_plan = response_or.value().text_response;
    }

    std::cout << "\nProposed Plan Ready:\n" << std::string(40, '-') << "\n";
    std::cout << response_or.value().text_response << "\n" << std::string(40, '-') << "\n";
    std::cout << "Run 'agent diff' to review this plan again.\n" << std::endl;
  }).detach();

  return absl::OkStatus();
}

absl::Status HandleDiffCommand(Rom&, const std::vector<std::string>&) {
  std::lock_guard<std::mutex> lock(g_plan_mutex);
  if (g_pending_plan.empty()) {
    return absl::NotFoundError("No pending plan found. Run 'agent plan <task>' first.");
  }

  std::cout << "\nPending Plan (Conceptual Diff):\n" << std::string(40, '-') << "\n";
  std::cout << g_pending_plan << "\n" << std::string(40, '-') << "\n";
  
  return absl::OkStatus();
}

absl::Status HandleListCommand(const std::vector<std::string>& /*args*/) {
  const auto& catalog = ResourceCatalog::Instance();
  std::ostringstream oss;

  oss << "Available Resources:\n";
  for (const auto& resource : catalog.AllResources()) {
    oss << "  - " << resource.resource << ": " << resource.description << "\n";
  }

  std::cout << oss.str() << std::endl;
  return absl::OkStatus();
}

absl::Status HandleDescribeCommand(const std::vector<std::string>& args) {
  if (args.empty()) {
    return absl::InvalidArgumentError("Usage: agent describe <resource_name>");
  }

  const std::string& name = args[0];
  const auto& catalog = ResourceCatalog::Instance();
  auto resource_or = catalog.GetResource(name);

  if (!resource_or.ok()) {
    return resource_or.status();
  }

  const auto& resource = resource_or.value();
  std::string json_output = catalog.SerializeResource(resource);

  std::cout << "Description of " << name << ":\n";
  std::cout << json_output << std::endl;

  return absl::OkStatus();
}

}  // namespace

// Forward declarations if needed
namespace agent {
  // Stubs for unsupported commands
  absl::Status HandleRunCommand(const std::vector<std::string>&, Rom&) {
    return absl::UnimplementedError("Agent run command not available in browser yet");
  }
  // HandlePlanCommand and HandleDiffCommand are now implemented in the anonymous namespace above
  // and dispatched below.
  
  absl::Status HandleCommitCommand(Rom&) {
    return absl::UnimplementedError("Agent commit command not available in browser yet");
  }
  absl::Status HandleRevertCommand(Rom&) {
    return absl::UnimplementedError("Agent revert command not available in browser yet");
  }
  absl::Status HandleAcceptCommand(const std::vector<std::string>&, Rom&) {
    return absl::UnimplementedError("Agent accept command not available in browser yet");
  }
  absl::Status HandleTestCommand(const std::vector<std::string>&) {
    return absl::UnimplementedError("Agent test command not available in browser");
  }
  absl::Status HandleTestConversationCommand(const std::vector<std::string>&) {
    return absl::UnimplementedError("Agent test-conversation command not available in browser");
  }
  absl::Status HandleLearnCommand(const std::vector<std::string>&) {
    return absl::UnimplementedError("Agent learn command not available in browser");
  }
}

std::string GenerateAgentHelp() {
  return "Available Agent Commands (Browser):\n"
         "  todo               - Manage todo list\n"
         "  chat <message>     - Chat with the AI agent\n"
         "  list               - List available resources\n"
         "  describe <name>    - Describe a specific resource\n"
         "  plan <task>        - Generate an implementation plan\n"
         "  diff               - View pending plan/changes\n";
}

Rom& AgentRom() {
  static Rom rom;
  return rom;
}

absl::Status HandleAgentCommand(const std::vector<std::string>& arg_vec) {
  if (arg_vec.empty()) {
    std::cout << GenerateAgentHelp();
    return absl::InvalidArgumentError("No subcommand specified");
  }

  const std::string& subcommand = arg_vec[0];
  std::vector<std::string> subcommand_args(arg_vec.begin() + 1, arg_vec.end());

  if (subcommand == "simple-chat" || subcommand == "chat") {
    return HandleChatCommand(subcommand_args);
  }

  if (subcommand == "plan") {
    return HandlePlanCommand(subcommand_args);
  }

  if (subcommand == "diff") {
    // Diff usually takes args but we ignore them for this simple version
    return HandleDiffCommand(AgentRom(), subcommand_args);
  }

  if (subcommand == "todo") {
    return handlers::HandleTodoCommand(subcommand_args);
  }

  if (subcommand == "list") {
    return HandleListCommand(subcommand_args);
  }

  if (subcommand == "describe") {
    return HandleDescribeCommand(subcommand_args);
  }

  std::cout << GenerateAgentHelp();
  return absl::InvalidArgumentError(absl::StrCat("Unknown subcommand: ", subcommand));
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
