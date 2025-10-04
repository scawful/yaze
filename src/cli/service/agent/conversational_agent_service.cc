#include "cli/service/agent/conversational_agent_service.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/time/clock.h"
#include "cli/service/ai/service_factory.h"
#include "cli/util/terminal_colors.h"
#include "nlohmann/json.hpp"

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
  ai_service_ = CreateAIService();
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
}

absl::StatusOr<ChatMessage> ConversationalAgentService::SendMessage(
    const std::string& message) {
  if (message.empty() && history_.empty()) {
    return absl::InvalidArgumentError(
        "Conversation must start with a non-empty message.");
  }

  if (!message.empty()) {
    history_.push_back(CreateMessage(ChatMessage::Sender::kUser, message));
  }

  constexpr int kMaxToolIterations = 4;
  bool waiting_for_text_response = false;
  
  for (int iteration = 0; iteration < kMaxToolIterations; ++iteration) {
    // Show loading indicator while waiting for AI response
    util::LoadingIndicator loader(
        waiting_for_text_response 
            ? "Generating final response..." 
            : "Thinking...", 
        true);
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

    if (!agent_response.tool_calls.empty()) {
      // Check if we were waiting for a text response but got more tool calls instead
      if (waiting_for_text_response) {
        util::PrintWarning(
            absl::StrCat("LLM called tools again instead of providing final response (Iteration: ",
                        iteration, "/", kMaxToolIterations, ")"));
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
          // Add tool result with a clear marker for the LLM
          std::string marked_output = "[TOOL RESULT] " + tool_output;
          history_.push_back(
              CreateMessage(ChatMessage::Sender::kUser, marked_output));
        }
        executed_tool = true;
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
                      iteration, "/", kMaxToolIterations, ")"));
      // Continue to give it another chance
      continue;
    }

    std::string response_text = agent_response.text_response;
    if (!agent_response.reasoning.empty()) {
      if (!response_text.empty()) {
        response_text.append("\n\n");
      }
      response_text.append("Reasoning: ");
      response_text.append(agent_response.reasoning);
    }
    if (!agent_response.commands.empty()) {
      if (!response_text.empty()) {
        response_text.append("\n\n");
      }
      response_text.append("Commands:\n");
      response_text.append(absl::StrJoin(agent_response.commands, "\n"));
    }

  ChatMessage chat_response =
    CreateMessage(ChatMessage::Sender::kAgent, response_text);
    history_.push_back(chat_response);
    return chat_response;
  }

  return absl::InternalError(
      "Agent did not produce a response after executing tools.");
}

const std::vector<ChatMessage>& ConversationalAgentService::GetHistory() const {
  return history_;
}

}  // namespace agent
}  // namespace cli
}  // namespace yaze
