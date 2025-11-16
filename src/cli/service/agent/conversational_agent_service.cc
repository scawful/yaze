#include "cli/service/agent/conversational_agent_service.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "app/rom.h"
#include "cli/service/agent/proposal_executor.h"
#include "cli/service/agent/advanced_routing.h"
#include "cli/service/agent/agent_pretraining.h"
#include "cli/service/ai/service_factory.h"
#include "cli/util/terminal_colors.h"
#include "nlohmann/json.hpp"

#ifdef SendMessage
#undef SendMessage
#endif

ABSL_DECLARE_FLAG(std::string, ai_provider);

namespace yaze {
namespace cli {
namespace agent {

namespace {

std::string TrimWhitespace(const std::string& input) {
  auto begin = std::find_if_not(input.begin(), input.end(),
                                [](unsigned char c) { return std::isspace(c); });
  auto end = std::find_if_not(input.rbegin(), input.rend(),
                              [](unsigned char c) { return std::isspace(c); })
                 .base();
  if (begin >= end) {
    return "";
  }
  return std::string(begin, end);
}

std::string JsonValueToString(const nlohmann::json& value) {
  if (value.is_string()) {
    return value.get<std::string>();
  }
  if (value.is_boolean()) {
    return value.get<bool>() ? "true" : "false";
  }
  if (value.is_number()) {
    return value.dump();
  }
  if (value.is_null()) {
    return "null";
  }
  return value.dump();
}

std::set<std::string> CollectObjectKeys(const nlohmann::json& array) {
  std::set<std::string> keys;
  for (const auto& item : array) {
    if (!item.is_object()) {
      continue;
    }
    for (const auto& [key, _] : item.items()) {
      keys.insert(key);
    }
  }
  return keys;
}

std::optional<ChatMessage::TableData> BuildTableData(const nlohmann::json& data) {
  using TableData = ChatMessage::TableData;

  if (data.is_object()) {
    TableData table;
    table.headers = {"Key", "Value"};
    table.rows.reserve(data.size());
    for (const auto& [key, value] : data.items()) {
      table.rows.push_back({key, JsonValueToString(value)});
    }
    return table;
  }

  if (data.is_array()) {
    TableData table;
    if (data.empty()) {
      table.headers = {"Value"};
      return table;
    }

    const bool all_objects = std::all_of(data.begin(), data.end(), [](const nlohmann::json& item) {
      return item.is_object();
    });

    if (all_objects) {
      auto keys = CollectObjectKeys(data);
      if (keys.empty()) {
        table.headers = {"Value"};
        for (const auto& item : data) {
          table.rows.push_back({JsonValueToString(item)});
        }
        return table;
      }

      table.headers.assign(keys.begin(), keys.end());
      table.rows.reserve(data.size());
      for (const auto& item : data) {
        std::vector<std::string> row;
        row.reserve(table.headers.size());
        for (const auto& key : table.headers) {
          if (item.contains(key)) {
            row.push_back(JsonValueToString(item.at(key)));
          } else {
            row.emplace_back("-");
          }
        }
        table.rows.push_back(std::move(row));
      }
      return table;
    }

    table.headers = {"Value"};
    table.rows.reserve(data.size());
    for (const auto& item : data) {
      table.rows.push_back({JsonValueToString(item)});
    }
    return table;
  }

  return std::nullopt;
}

bool IsExecutableCommand(absl::string_view command) {
  return !command.empty() && command.front() != '#';
}

int CountExecutableCommands(const std::vector<std::string>& commands) {
  int count = 0;
  for (const auto& command : commands) {
    if (IsExecutableCommand(command)) {
      ++count;
    }
  }
  return count;
}

ChatMessage CreateMessage(ChatMessage::Sender sender, const std::string& content) {
  ChatMessage message;
  message.sender = sender;
  message.message = content;
  message.timestamp = absl::Now();

  if (sender == ChatMessage::Sender::kAgent) {
    const std::string trimmed = TrimWhitespace(content);
    if (!trimmed.empty() && (trimmed.front() == '{' || trimmed.front() == '[')) {
      try {
        nlohmann::json parsed = nlohmann::json::parse(trimmed);
        message.table_data = BuildTableData(parsed);
        message.json_pretty = parsed.dump(2);
      } catch (const nlohmann::json::parse_error&) {
        // Ignore parse errors, fall back to raw text.
      }
    }
  }

  return message;
}

}  // namespace

ConversationalAgentService::ConversationalAgentService() {
  provider_config_.provider = "auto";
  ai_service_ = CreateAIService();
  tool_dispatcher_.SetToolPreferences(tool_preferences_);
  
#ifdef Z3ED_AI
  // Initialize advanced features
  auto learn_status = learned_knowledge_.Initialize();
  if (!learn_status.ok() && config_.verbose) {
    std::cerr << "Warning: Failed to initialize learned knowledge: " 
              << learn_status.message() << std::endl;
  }
  
  auto todo_status = todo_manager_.Initialize();
  if (!todo_status.ok() && config_.verbose) {
    std::cerr << "Warning: Failed to initialize TODO manager: " 
              << todo_status.message() << std::endl;
  }
#endif
}

ConversationalAgentService::ConversationalAgentService(const AgentConfig& config)
    : config_(config) {
  provider_config_.provider = "auto";
  ai_service_ = CreateAIService();
  tool_dispatcher_.SetToolPreferences(tool_preferences_);
  
#ifdef Z3ED_AI
  // Initialize advanced features
  auto learn_status = learned_knowledge_.Initialize();
  if (!learn_status.ok() && config_.verbose) {
    std::cerr << "Warning: Failed to initialize learned knowledge: " 
              << learn_status.message() << std::endl;
  }
  
  auto todo_status = todo_manager_.Initialize();
  if (!todo_status.ok() && config_.verbose) {
    std::cerr << "Warning: Failed to initialize TODO manager: " 
              << todo_status.message() << std::endl;
  }
#endif
}

void ConversationalAgentService::SetRomContext(Rom* rom) {
  rom_context_ = rom;

  tool_dispatcher_.SetRomContext(rom_context_);
  if (ai_service_) {
    ai_service_->SetRomContext(rom_context_);
  }
}

void ConversationalAgentService::ResetConversation() {
  history_.clear();
  metrics_ = InternalMetrics{};
}

void ConversationalAgentService::TrimHistoryIfNeeded() {
  if (!config_.trim_history || config_.max_history_messages == 0) {
    return;
  }

  while (history_.size() > config_.max_history_messages) {
    history_.erase(history_.begin());
  }
}

ChatMessage::SessionMetrics ConversationalAgentService::BuildMetricsSnapshot() const {
  ChatMessage::SessionMetrics snapshot;
  snapshot.turn_index = metrics_.turns_completed;
  snapshot.total_user_messages = metrics_.user_messages;
  snapshot.total_agent_messages = metrics_.agent_messages;
  snapshot.total_tool_calls = metrics_.tool_calls;
  snapshot.total_commands = metrics_.commands_generated;
  snapshot.total_proposals = metrics_.proposals_created;
  snapshot.total_elapsed_seconds = absl::ToDoubleSeconds(metrics_.total_latency);
  snapshot.average_latency_seconds =
      metrics_.turns_completed > 0
          ? snapshot.total_elapsed_seconds /
                static_cast<double>(metrics_.turns_completed)
          : 0.0;
  return snapshot;
}

ChatMessage::SessionMetrics ConversationalAgentService::GetMetrics() const {
  return BuildMetricsSnapshot();
}

absl::StatusOr<ChatMessage> ConversationalAgentService::SendMessage(
    const std::string& message) {
  if (message.empty() && history_.empty()) {
    return absl::InvalidArgumentError(
        "Conversation must start with a non-empty message.");
  }

  if (!message.empty()) {
    history_.push_back(CreateMessage(ChatMessage::Sender::kUser, message));
    TrimHistoryIfNeeded();
    ++metrics_.user_messages;
  }

  const int max_iterations = config_.max_tool_iterations;
  bool waiting_for_text_response = false;
  absl::Time turn_start = absl::Now();
  std::vector<std::string> executed_tools;
  
  if (config_.verbose) {
    util::PrintInfo(absl::StrCat("Starting agent loop (max ", max_iterations, " iterations)"));
    util::PrintInfo(absl::StrCat("History size: ", history_.size(), " messages"));
  }
  
  for (int iteration = 0; iteration < max_iterations; ++iteration) {
    if (config_.verbose) {
      util::PrintSeparator();
      std::cout << util::colors::kCyan << "Iteration " << (iteration + 1) 
                << "/" << max_iterations << util::colors::kReset << std::endl;
    }
    
    // Show loading indicator while waiting for AI response
    util::LoadingIndicator loader(
        waiting_for_text_response 
            ? "Generating final response..." 
            : "Thinking...", 
        !config_.verbose);  // Hide spinner in verbose mode
    loader.Start();
    
    auto response_or = ai_service_->GenerateResponse(history_);
    loader.Stop();
    
    if (!response_or.ok()) {
      util::PrintError(absl::StrCat(
          "Failed to get AI response: ", response_or.status().message()));
      return absl::InternalError(absl::StrCat(
          "Failed to get AI response: ", response_or.status().message()));
    }

    const auto& agent_response = response_or.value();

    if (config_.verbose) {
      util::PrintInfo("Received agent response:");
      std::cout << util::colors::kDim << "  - Tool calls: " 
                << agent_response.tool_calls.size() << util::colors::kReset << std::endl;
      std::cout << util::colors::kDim << "  - Commands: " 
                << agent_response.commands.size() << util::colors::kReset << std::endl;
      std::cout << util::colors::kDim << "  - Text response: "
                << (agent_response.text_response.empty() ? "empty" : "present")
                << util::colors::kReset << std::endl;
      if (!agent_response.reasoning.empty() && config_.show_reasoning) {
        std::cout << util::colors::kYellow << "  💭 Reasoning: " 
                  << util::colors::kDim << agent_response.reasoning 
                  << util::colors::kReset << std::endl;
      }
    }

    if (!agent_response.tool_calls.empty()) {
      // Check if we were waiting for a text response but got more tool calls instead
      if (waiting_for_text_response) {
        util::PrintWarning(
            absl::StrCat("LLM called tools again instead of providing final response (Iteration: ",
                        iteration + 1, "/", max_iterations, ")"));
      }
      
      bool executed_tool = false;
      for (const auto& tool_call : agent_response.tool_calls) {
        // Format tool arguments for display
        std::vector<std::string> arg_parts;
        for (const auto& [key, value] : tool_call.args) {
          arg_parts.push_back(absl::StrCat(key, "=", value));
        }
        std::string args_str = absl::StrJoin(arg_parts, ", ");
        
        util::PrintToolCall(tool_call.tool_name, args_str);
        
      auto tool_result_or = tool_dispatcher_.Dispatch(tool_call);
        if (!tool_result_or.ok()) {
          util::PrintError(absl::StrCat(
              "Tool execution failed: ", tool_result_or.status().message()));
          return absl::InternalError(absl::StrCat(
              "Tool execution failed: ", tool_result_or.status().message()));
        }

        const std::string& tool_output = tool_result_or.value();
        if (!tool_output.empty()) {
          util::PrintSuccess("Tool executed successfully");
          ++metrics_.tool_calls;
          
          if (config_.verbose) {
            std::cout << util::colors::kDim << "Tool output (truncated):" 
                      << util::colors::kReset << std::endl;
            std::string preview = tool_output.substr(0, std::min(size_t(200), tool_output.size()));
            if (tool_output.size() > 200) preview += "...";
            std::cout << util::colors::kDim << preview << util::colors::kReset << std::endl;
          }
          
          // Add tool result with a clear marker for the LLM
          // Format as plain text to avoid confusing the LLM with nested JSON
          std::string marked_output = absl::StrCat(
              "[TOOL RESULT for ", tool_call.tool_name, "]\n",
              "The tool returned the following data:\n",
              tool_output, "\n\n",
              "Please provide a text_response field in your JSON to summarize this information for the user.");
          auto tool_result_msg = CreateMessage(ChatMessage::Sender::kUser, marked_output);
          tool_result_msg.is_internal = true;  // Don't show this to the human user
          history_.push_back(tool_result_msg);
        }
        executed_tool = true;
        executed_tools.push_back(tool_call.tool_name);
      }

      if (executed_tool) {
        // Now we're waiting for the LLM to provide a text response
        waiting_for_text_response = true;
        // Re-query the AI with updated context.
        continue;
      }
    }

    // Check if we received a text response after tool execution
    if (waiting_for_text_response && agent_response.text_response.empty() && 
        agent_response.commands.empty()) {
      util::PrintWarning(
          absl::StrCat("LLM did not provide text_response after receiving tool results (Iteration: ",
                      iteration + 1, "/", max_iterations, ")"));
      // Continue to give it another chance
      continue;
    }

    std::optional<ProposalCreationResult> proposal_result;
    absl::Status proposal_status = absl::OkStatus();
    bool attempted_proposal = false;

    if (!agent_response.commands.empty()) {
      attempted_proposal = true;

      if (rom_context_ == nullptr) {
        proposal_status = absl::FailedPreconditionError(
            "No ROM context available for proposal creation");
        util::PrintWarning(
            "Cannot create proposal because no ROM context is active.");
      } else if (!rom_context_->is_loaded()) {
        proposal_status = absl::FailedPreconditionError(
            "ROM context is not loaded");
        util::PrintWarning(
            "Cannot create proposal because the ROM context is not loaded.");
      } else {
        ProposalCreationRequest request;
        request.prompt = message;
        request.response = &agent_response;
        request.rom = rom_context_;
        request.sandbox_label = "agent-chat";
        request.ai_provider = absl::GetFlag(FLAGS_ai_provider);

        auto creation_or = CreateProposalFromAgentResponse(request);
        if (!creation_or.ok()) {
          proposal_status = creation_or.status();
          util::PrintError(absl::StrCat(
              "Failed to create proposal: ", proposal_status.message()));
        } else {
          proposal_result = std::move(creation_or.value());
          if (config_.verbose) {
            util::PrintSuccess(absl::StrCat(
                "Created proposal ", proposal_result->metadata.id,
                " with ", proposal_result->change_count, " change(s)."));
          }
        }
      }
    }

    std::string response_text = agent_response.text_response;
    if (!agent_response.reasoning.empty()) {
      if (!response_text.empty()) {
        response_text.append("\n\n");
      }
      response_text.append("Reasoning: ");
      response_text.append(agent_response.reasoning);
    }
    const int executable_commands =
        CountExecutableCommands(agent_response.commands);
    if (!agent_response.commands.empty()) {
      if (!response_text.empty()) {
        response_text.append("\n\n");
      }
      response_text.append("Commands:\n");
      response_text.append(absl::StrJoin(agent_response.commands, "\n"));
    }
    metrics_.commands_generated += executable_commands;

    if (proposal_result.has_value()) {
      const auto& metadata = proposal_result->metadata;
      if (!response_text.empty()) {
        response_text.append("\n\n");
      }
      response_text.append(absl::StrFormat(
          "✅ Proposal %s ready with %d change%s (%d command%s).\n"
          "Review it in the Proposal drawer or run `z3ed agent diff --proposal-id %s`.\n"
          "Sandbox ROM: %s\nProposal JSON: %s",
          metadata.id, proposal_result->change_count,
          proposal_result->change_count == 1 ? "" : "s",
          proposal_result->executed_commands,
          proposal_result->executed_commands == 1 ? "" : "s",
          metadata.id, metadata.sandbox_rom_path.string(),
          proposal_result->proposal_json_path.string()));
      ++metrics_.proposals_created;
    } else if (attempted_proposal && !proposal_status.ok()) {
      if (!response_text.empty()) {
        response_text.append("\n\n");
      }
      response_text.append(absl::StrCat(
          "⚠️ Failed to prepare a proposal automatically: ",
          proposal_status.message()));
    }
    ChatMessage chat_response =
        CreateMessage(ChatMessage::Sender::kAgent, response_text);
    if (proposal_result.has_value()) {
      ChatMessage::ProposalSummary summary;
      summary.id = proposal_result->metadata.id;
      summary.change_count = proposal_result->change_count;
      summary.executed_commands = proposal_result->executed_commands;
      summary.sandbox_rom_path = proposal_result->metadata.sandbox_rom_path;
      summary.proposal_json_path = proposal_result->proposal_json_path;
      chat_response.proposal = summary;
    }
    ++metrics_.agent_messages;
    ++metrics_.turns_completed;
    metrics_.total_latency += absl::Now() - turn_start;
    chat_response.metrics = BuildMetricsSnapshot();
    if (!agent_response.warnings.empty()) {
      chat_response.warnings = agent_response.warnings;
    }
    ChatMessage::ModelMetadata meta;
    meta.provider = !agent_response.provider.empty()
                        ? agent_response.provider
                        : provider_config_.provider;
    meta.model = !agent_response.model.empty() ? agent_response.model
                                               : provider_config_.model;
    meta.latency_seconds =
        agent_response.latency_seconds > 0.0
            ? agent_response.latency_seconds
            : absl::ToDoubleSeconds(absl::Now() - turn_start);
    meta.tool_iterations = metrics_.tool_calls;
    meta.tool_names = executed_tools;
    meta.parameters = agent_response.parameters;
    chat_response.model_metadata = meta;
    history_.push_back(chat_response);
    TrimHistoryIfNeeded();
    return chat_response;
  }

  return absl::InternalError(
      "Agent did not produce a response after executing tools.");
}

absl::Status ConversationalAgentService::ConfigureProvider(
    const AIServiceConfig& config) {
  auto service_or = CreateAIServiceStrict(config);
  if (!service_or.ok()) {
    return service_or.status();
  }

  ai_service_ = std::move(service_or.value());
  provider_config_ = config;
  if (rom_context_) {
    ai_service_->SetRomContext(rom_context_);
  }
  return absl::OkStatus();
}

void ConversationalAgentService::SetToolPreferences(
    const ToolDispatcher::ToolPreferences& prefs) {
  tool_preferences_ = prefs;
  tool_dispatcher_.SetToolPreferences(tool_preferences_);
}

const std::vector<ChatMessage>& ConversationalAgentService::GetHistory() const {
  return history_;
}

void ConversationalAgentService::ReplaceHistory(
    std::vector<ChatMessage> history) {
  history_ = std::move(history);
  TrimHistoryIfNeeded();
  RebuildMetricsFromHistory();
}

void ConversationalAgentService::RebuildMetricsFromHistory() {
  metrics_ = InternalMetrics{};

  ChatMessage::SessionMetrics snapshot{};
  bool has_snapshot = false;

  for (const auto& message : history_) {
    if (message.sender == ChatMessage::Sender::kUser) {
      ++metrics_.user_messages;
    } else if (message.sender == ChatMessage::Sender::kAgent) {
      ++metrics_.agent_messages;
      ++metrics_.turns_completed;
    }

    if (message.proposal.has_value()) {
      ++metrics_.proposals_created;
    }

    if (message.metrics.has_value()) {
      snapshot = *message.metrics;
      has_snapshot = true;
    }
  }

  if (has_snapshot) {
    metrics_.user_messages = snapshot.total_user_messages;
    metrics_.agent_messages = snapshot.total_agent_messages;
    metrics_.tool_calls = snapshot.total_tool_calls;
    metrics_.commands_generated = snapshot.total_commands;
    metrics_.proposals_created = snapshot.total_proposals;
    metrics_.turns_completed = snapshot.turn_index;
    metrics_.total_latency = absl::Seconds(snapshot.total_elapsed_seconds);
  }
}

#ifdef Z3ED_AI
// === Advanced Feature Integration ===

std::string ConversationalAgentService::BuildEnhancedPrompt(const std::string& user_message) {
  std::ostringstream enhanced;
  
  // Inject pretraining on first message
  if (inject_pretraining_ && !pretraining_injected_ && rom_context_) {
    enhanced << InjectPretraining() << "\n\n";
    pretraining_injected_ = true;
  }
  
  // Inject learned context
  if (inject_learned_context_) {
    enhanced << InjectLearnedContext(user_message) << "\n";
  }
  
  enhanced << user_message;
  return enhanced.str();
}

std::string ConversationalAgentService::InjectLearnedContext(const std::string& message) {
  std::ostringstream context;
  
  // Add relevant preferences
  auto prefs = learned_knowledge_.GetAllPreferences();
  if (!prefs.empty() && prefs.size() <= 5) {  // Don't overwhelm with too many
    context << "[User Preferences: ";
    std::vector<std::string> pref_strings;
    for (const auto& [key, value] : prefs) {
      pref_strings.push_back(absl::StrCat(key, "=", value));
    }
    context << absl::StrJoin(pref_strings, ", ") << "]\n";
  }
  
  // Add ROM-specific patterns
  if (rom_context_ && rom_context_->is_loaded()) {
    // TODO: Get ROM hash
    // auto patterns = learned_knowledge_.QueryPatterns("", rom_hash);
  }
  
  // Add recent relevant memories
  std::vector<std::string> keywords;
  // Extract keywords from message (simple word splitting)
  for (const auto& word : absl::StrSplit(message, ' ')) {
    if (word.length() > 4) {  // Only meaningful words
      keywords.push_back(std::string(word));
    }
  }
  
  if (!keywords.empty()) {
    auto memories = learned_knowledge_.SearchMemories(keywords[0]);
    if (!memories.empty() && memories.size() <= 3) {
      context << "[Relevant Past Context:\n";
      for (const auto& mem : memories) {
        context << "- " << mem.topic << ": " << mem.summary << "\n";
      }
      context << "]\n";
    }
  }
  
  return context.str();
}

std::string ConversationalAgentService::InjectPretraining() {
  if (!rom_context_) {
    return "";
  }
  
  std::ostringstream pretraining;
  pretraining << "[SYSTEM KNOWLEDGE INJECTION - Read this first]\n\n";
  pretraining << AgentPretraining::GeneratePretrainingPrompt(rom_context_);
  pretraining << "\n[END KNOWLEDGE INJECTION]\n";
  
  return pretraining.str();
}

ChatMessage ConversationalAgentService::EnhanceResponse(
    const ChatMessage& response, 
    const std::string& user_message) {
  // Use AdvancedRouter to enhance tool-based responses
  // This would synthesize multi-tool results into coherent insights
  
  // For now, return response as-is
  // TODO: Integrate AdvancedRouter here
  return response;
}
#endif  // Z3ED_AI

}  // namespace agent
}  // namespace cli
}  // namespace yaze
