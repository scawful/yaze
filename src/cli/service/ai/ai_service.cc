#include "cli/service/ai/ai_service.h"

#include <algorithm>
#include <cctype>
#include <sstream>

#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "cli/service/agent/conversational_agent_service.h"

namespace yaze {
namespace cli {

namespace {

std::string ExtractRoomId(const std::string& normalized_prompt) {
  size_t hex_pos = normalized_prompt.find("0x");
  if (hex_pos != std::string::npos) {
    std::string hex_value;
    for (size_t i = hex_pos; i < normalized_prompt.size(); ++i) {
      char c = normalized_prompt[i];
      if (std::isxdigit(static_cast<unsigned char>(c)) || c == 'x') {
        hex_value.push_back(c);
      } else {
        break;
      }
    }
    if (hex_value.size() > 2) {
      return hex_value;
    }
  }

  // Fallback: look for decimal digits, then convert to hex string.
  std::string digits;
  for (char c : normalized_prompt) {
    if (std::isdigit(static_cast<unsigned char>(c))) {
      digits.push_back(c);
    } else if (!digits.empty()) {
      break;
    }
  }

  if (!digits.empty()) {
    int value = 0;
    if (absl::SimpleAtoi(digits, &value)) {
      return absl::StrFormat("0x%03X", value);
    }
  }

  return "0x000";
}

std::string ExtractKeyword(const std::string& normalized_prompt) {
  static const char* kStopwords[] = {
      "search", "for", "resource", "resources", "label", "labels", "please",
      "the",    "a",   "an",       "list",      "of",    "in",     "find"};

  auto is_stopword = [](const std::string& word) {
    for (const char* stop : kStopwords) {
      if (word == stop) {
        return true;
      }
    }
    return false;
  };

  std::istringstream stream(normalized_prompt);
  std::string token;
  while (stream >> token) {
    token.erase(std::remove_if(token.begin(), token.end(),
                               [](unsigned char c) {
                                 return !std::isalnum(c) && c != '_' &&
                                        c != '-';
                               }),
                token.end());
    if (token.empty()) {
      continue;
    }
    if (!is_stopword(token)) {
      return token;
    }
  }

  return "all";
}

}  // namespace

absl::StatusOr<AgentResponse> MockAIService::GenerateResponse(
    const std::string& prompt) {
  AgentResponse response;
  response.provider = "mock";
  response.model = "mock";
  response.parameters["mode"] = "scripted";
  response.parameters["temperature"] = "0.0";
  const std::string normalized = absl::AsciiStrToLower(prompt);

  if (normalized.empty()) {
    response.text_response =
        "Let's start with a prompt about the overworld or dungeons.";
    return response;
  }

  if (absl::StrContains(normalized, "place") &&
      absl::StrContains(normalized, "tree")) {
    response.text_response =
        "Sure, I can do that. Here's the command to place a tree.";
    response.commands.push_back(
        "overworld set-tile --map 0 --x 10 --y 20 --tile 0x02E");
    response.reasoning =
        "The user asked to place a tree tile16, so I generated the matching "
        "set-tile command.";
    return response;
  }

  if (absl::StrContains(normalized, "list") &&
      absl::StrContains(normalized, "resource")) {
    std::string resource_type = "dungeon";
    if (absl::StrContains(normalized, "overworld")) {
      resource_type = "overworld";
    } else if (absl::StrContains(normalized, "sprite")) {
      resource_type = "sprite";
    } else if (absl::StrContains(normalized, "palette")) {
      resource_type = "palette";
    }

    ToolCall call;
    call.tool_name = "resource-list";
    call.args.emplace("type", resource_type);
    response.text_response =
        absl::StrFormat("Fetching %s labels from the ROM...", resource_type);
    response.reasoning =
        "Using the resource-list tool keeps the LLM in sync with project "
        "labels.";
    response.tool_calls.push_back(call);
    return response;
  }

  if (absl::StrContains(normalized, "search") &&
      (absl::StrContains(normalized, "resource") ||
       absl::StrContains(normalized, "label"))) {
    ToolCall call;
    call.tool_name = "resource-search";
    call.args.emplace("query", ExtractKeyword(normalized));
    response.text_response =
        "Let me look through the labelled resources for matches.";
    response.reasoning =
        "Resource search provides fuzzy matching against the ROM label "
        "catalogue.";
    response.tool_calls.push_back(call);
    return response;
  }

  if (absl::StrContains(normalized, "sprite") &&
      absl::StrContains(normalized, "room")) {
    ToolCall call;
    call.tool_name = "dungeon-list-sprites";
    call.args.emplace("room", ExtractRoomId(normalized));
    response.text_response = "Let me inspect the dungeon room sprites for you.";
    response.reasoning =
        "Calling the sprite inspection tool provides precise coordinates for "
        "the agent.";
    response.tool_calls.push_back(call);
    return response;
  }

  if (absl::StrContains(normalized, "describe") &&
      absl::StrContains(normalized, "room")) {
    ToolCall call;
    call.tool_name = "dungeon-describe-room";
    call.args.emplace("room", ExtractRoomId(normalized));
    response.text_response = "I'll summarize the room's metadata and hazards.";
    response.reasoning =
        "Room description tool surfaces lighting, effects, and object counts "
        "before planning edits.";
    response.tool_calls.push_back(call);
    return response;
  }

  response.text_response =
      "I'm just a mock service. Please load a provider like ollama or gemini.";
  return response;
}

absl::StatusOr<AgentResponse> MockAIService::GenerateResponse(
    const std::vector<agent::ChatMessage>& history) {
  if (history.empty()) {
    return absl::InvalidArgumentError("History cannot be empty.");
  }

  // If the last message in history is a tool output, synthesize a summary.
  for (auto it = history.rbegin(); it != history.rend(); ++it) {
    if (it->sender == agent::ChatMessage::Sender::kAgent &&
        (absl::StrContains(it->message, "=== ") ||
         absl::StrContains(it->message, "\"id\"") ||
         absl::StrContains(it->message, "\n{"))) {
      AgentResponse response;
      response.provider = "mock";
      response.model = "mock";
      response.parameters["mode"] = "scripted";
      response.parameters["temperature"] = "0.0";
      response.text_response = "Here's what I found:\n" + it->message +
                               "\nLet me know if you'd like to make a change.";
      response.reasoning = "Summarized the latest tool output for the user.";
      return response;
    }
  }

  auto user_it = std::find_if(
      history.rbegin(), history.rend(), [](const agent::ChatMessage& message) {
        return message.sender == agent::ChatMessage::Sender::kUser;
      });
  if (user_it == history.rend()) {
    return absl::InvalidArgumentError(
        "History does not contain a user message.");
  }

  return GenerateResponse(user_it->message);
}

}  // namespace cli
}  // namespace yaze
