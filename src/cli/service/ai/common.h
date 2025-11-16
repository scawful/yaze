#ifndef YAZE_SRC_CLI_SERVICE_AI_COMMON_H_
#define YAZE_SRC_CLI_SERVICE_AI_COMMON_H_

#include <string>
#include <vector>
#include <map>

namespace yaze {
namespace cli {

// Represents a request from the AI to call a tool.
struct ToolCall {
  std::string tool_name;
  std::map<std::string, std::string> args;
};

// A structured response from an AI service.
struct AgentResponse {
  // A natural language response to the user.
  std::string text_response;

  // A list of tool calls the agent wants to make.
  std::vector<ToolCall> tool_calls;

  // A list of z3ed commands to be executed.
  std::vector<std::string> commands;

  // The AI's explanation of its thought process.
  std::string reasoning;

  // Provider + model metadata so the UI can show badges / filters.
  std::string provider;
  std::string model;

  // Basic timing + parameter telemetry.
  double latency_seconds = 0.0;
  std::map<std::string, std::string> parameters;

  // Optional warnings surfaced by the backend (e.g. truncated context).
  std::vector<std::string> warnings;
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_AI_COMMON_H_
