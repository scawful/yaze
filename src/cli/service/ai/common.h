#ifndef YAZE_SRC_CLI_SERVICE_AI_COMMON_H_
#define YAZE_SRC_CLI_SERVICE_AI_COMMON_H_

#include <map>
#include <string>
#include <vector>

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

// Represents a model available for use
struct ModelInfo {
  std::string name;            // Unique identifier / name to use in API calls
  std::string display_name;    // User-friendly name
  std::string provider;        // "ollama", "gemini", etc.
  std::string description;     // Short description or family
  std::string family;          // Model family (e.g. "llama", "gemini")
  std::string parameter_size;  // Size string (e.g. "7B")
  std::string quantization;    // e.g. "Q4_K_M"
  uint64_t size_bytes = 0;
  bool is_local = false;  // true if running locally
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_AI_COMMON_H_
