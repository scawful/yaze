#include <vector>
#include <string>
#include <iostream>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "cli/cli.h"
#include "cli/service/command_registry.h"
#include "cli/handlers/agent/todo_commands.h"
#include "cli/service/ai/browser_ai_service.h"
#include "cli/service/agent/conversational_agent_service.h"
#include "cli/service/resources/resource_catalog.h"
#include "app/rom.h"

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

// Simple in-memory storage for the last generated plan
// In a full implementation, this would be a full ProposalRegistry
std::string g_pending_plan;

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

  // Add user message to history
  agent::ChatMessage user_msg;
  user_msg.sender = agent::ChatMessage::Sender::kUser;
  user_msg.message = prompt;
  user_msg.timestamp = absl::Now();
  g_chat_history.push_back(user_msg);

  std::cout << "Thinking..." << std::endl;

  // Generate response using history
  auto response_or = service->GenerateResponse(g_chat_history);
  if (!response_or.ok()) {
    // Remove the failed user message so we don't get stuck in a bad state
    g_chat_history.pop_back();
    return response_or.status();
  }

  auto response = response_or.value();

  // Add agent response to history
  agent::ChatMessage agent_msg;
  agent_msg.sender = agent::ChatMessage::Sender::kModel;
  agent_msg.message = response.text_response;
  agent_msg.timestamp = absl::Now();
  g_chat_history.push_back(agent_msg);

  // Print response
  std::cout << "\nAgent: " << response.text_response << "\n" << std::endl;

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

  std::cout << "Generating plan..." << std::endl;

  auto response_or = service->GenerateResponse(prompt);
  if (!response_or.ok()) {
    return response_or.status();
  }

  g_pending_plan = response_or.value().text_response;

  std::cout << "\nProposed Plan:\n" << std::string(40, '-') << "\n";
  std::cout << g_pending_plan << "\n" << std::string(40, '-') << "\n";
  std::cout << "Run 'agent diff' to review this plan again.\n" << std::endl;

  return absl::OkStatus();
}

absl::Status HandleDiffCommand(Rom&, const std::vector<std::string>&) {
  if (g_pending_plan.empty()) {
    return absl::NotFoundError("No pending plan found. Run 'agent plan <task>' first.");
  }

  std::cout << "\nPending Plan (Conceptual Diff):\n" << std::string(40, '-') << "\n";
  std::cout << g_pending_plan << "\n" << std::string(40, '-') << "\n";
  
  return absl::OkStatus();
}

absl::Status HandleListCommand(const std::vector<std::string>& args) {
  const auto& catalog = resources::ResourceCatalog::Instance();
  std::ostringstream oss;

  if (args.empty()) {
    oss << "Available Resource Categories:\n";
    for (const auto& category : catalog.AllCategories()) {
      oss << "  - " << category << "\n";
    }
    oss << "\nUse 'agent list <category>' to see resources in a category.";
  } else if (args.size() == 1) {
    const std::string& category = args[0];
    const auto& resources = catalog.GetResourcesInCategory(category);
    if (resources.empty()) {
      return absl::NotFoundError(absl::StrCat("No resources found for category: ", category));
    }
    oss << "Resources in category \'" << category << "\':\n";
    for (const auto& res_schema : resources) {
      oss << "  - " << res_schema.name << " (ID: " << res_schema.id << ")\n";
    }
  } else {
    return absl::InvalidArgumentError("Usage: agent list [category]");
  }

  std::cout << oss.str() << std::endl;
  return absl::OkStatus();
}

absl::Status HandleDescribeCommand(const std::vector<std::string>& args) {
  if (args.size() < 2) {
    return absl::InvalidArgumentError("Usage: agent describe <category> <name_or_id>");
  }

  const std::string& category = args[0];
  const std::string& name_or_id = args[1];
  Rom* rom = GetGlobalRom(); // Access the global ROM

  if (!rom || !rom->is_loaded()) {
    return absl::FailedPreconditionError("No ROM loaded. Please load a ROM first.");
  }

  const auto& catalog = resources::ResourceCatalog::Instance();
  auto resource_or = catalog.GetResourceByNameOrId(category, name_or_id);

  if (!resource_or.ok()) {
    return resource_or.status();
  }

  const auto& resource = resource_or.value();

  // Serialize the resource to JSON for detailed output
  // In a full implementation, this might call a specific handler for richer description
  std::string json_output = catalog.SerializeResource(resource, rom);
  
  std::cout << "Description of " << category << ": " << name_or_id << "\n";
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
         "  list [category]    - List resource categories or resources\n"
         "  describe <cat> <id>- Describe a specific resource\n"
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


