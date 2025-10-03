#include "cli/service/agent/conversational_agent_service.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/time/clock.h"
#include "cli/service/ai/service_factory.h"
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
  for (int iteration = 0; iteration < kMaxToolIterations; ++iteration) {
    auto response_or = ai_service_->GenerateResponse(history_);
    if (!response_or.ok()) {
      return absl::InternalError(absl::StrCat(
          "Failed to get AI response: ", response_or.status().message()));
    }

    const auto& agent_response = response_or.value();

    if (!agent_response.tool_calls.empty()) {
      bool executed_tool = false;
      for (const auto& tool_call : agent_response.tool_calls) {
        auto tool_result_or = tool_dispatcher_.Dispatch(tool_call);
        if (!tool_result_or.ok()) {
          return absl::InternalError(absl::StrCat(
              "Tool execution failed: ", tool_result_or.status().message()));
        }

        const std::string& tool_output = tool_result_or.value();
        if (!tool_output.empty()) {
          history_.push_back(
              CreateMessage(ChatMessage::Sender::kAgent, tool_output));
        }
        executed_tool = true;
      }

      if (executed_tool) {
        // Re-query the AI with updated context.
        continue;
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
