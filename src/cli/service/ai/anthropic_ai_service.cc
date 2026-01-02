#include "cli/service/ai/anthropic_ai_service.h"

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "cli/service/agent/conversational_agent_service.h"
#include "util/platform_paths.h"

#ifdef YAZE_WITH_JSON
#include <filesystem>
#include <fstream>

#include "httplib.h"
#include "nlohmann/json.hpp"
#endif

namespace yaze {
namespace cli {

#ifdef YAZE_AI_RUNTIME_AVAILABLE

AnthropicAIService::AnthropicAIService(const AnthropicConfig& config)
    : function_calling_enabled_(config.use_function_calling), config_(config) {
  if (config_.verbose) {
    std::cerr << "[DEBUG] Initializing Anthropic service..." << std::endl;
    std::cerr << "[DEBUG] Model: " << config_.model << std::endl;
  }

  // Load command documentation into prompt builder
  std::string catalogue_path = config_.prompt_version == "v2"
                                   ? "assets/agent/prompt_catalogue_v2.yaml"
                                   : "assets/agent/prompt_catalogue.yaml";
  if (auto status = prompt_builder_.LoadResourceCatalogue(catalogue_path);
      !status.ok()) {
    std::cerr << "⚠️  Failed to load agent prompt catalogue: "
              << status.message() << std::endl;
  }

  if (config_.system_instruction.empty()) {
    // Load system prompt file
    std::string prompt_file;
    if (config_.prompt_version == "v3") {
      prompt_file = "agent/system_prompt_v3.txt";
    } else if (config_.prompt_version == "v2") {
      prompt_file = "agent/system_prompt_v2.txt";
    } else {
      prompt_file = "agent/system_prompt.txt";
    }

    auto prompt_path = util::PlatformPaths::FindAsset(prompt_file);
    if (prompt_path.ok()) {
      std::ifstream file(prompt_path->string());
      if (file.good()) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        config_.system_instruction = buffer.str();
        if (config_.verbose) {
          std::cerr << "[DEBUG] Loaded prompt: " << prompt_path->string()
                    << std::endl;
        }
      }
    }

    if (config_.system_instruction.empty()) {
      config_.system_instruction = BuildSystemInstruction();
    }
  }

  if (config_.verbose) {
    std::cerr << "[DEBUG] Anthropic service initialized" << std::endl;
  }
}

void AnthropicAIService::EnableFunctionCalling(bool enable) {
  function_calling_enabled_ = enable;
}

std::vector<std::string> AnthropicAIService::GetAvailableTools() const {
  return {"resource-list",        "resource-search",
          "dungeon-list-sprites", "dungeon-describe-room",
          "overworld-find-tile",  "overworld-describe-map",
          "overworld-list-warps"};
}

std::string AnthropicAIService::BuildFunctionCallSchemas() {
#ifndef YAZE_WITH_JSON
  return "[]";
#else
  std::string schemas = prompt_builder_.BuildFunctionCallSchemas();
  if (!schemas.empty() && schemas != "[]") {
    return schemas;
  }

  auto schema_path_or =
      util::PlatformPaths::FindAsset("agent/function_schemas.json");

  if (!schema_path_or.ok()) {
    return "[]";
  }

  std::ifstream file(schema_path_or->string());
  if (!file.is_open()) {
    return "[]";
  }

  try {
    nlohmann::json schemas_json;
    file >> schemas_json;
    return schemas_json.dump();
  } catch (const nlohmann::json::exception& e) {
    std::cerr << "⚠️  Failed to parse function schemas JSON: " << e.what()
              << std::endl;
    return "[]";
  }
#endif
}

std::string AnthropicAIService::BuildSystemInstruction() {
  return prompt_builder_.BuildSystemInstruction();
}

void AnthropicAIService::SetRomContext(Rom* rom) {
  prompt_builder_.SetRom(rom);
}

absl::StatusOr<std::vector<ModelInfo>>
AnthropicAIService::ListAvailableModels() {
  // Anthropic doesn't have a simple public "list models" endpoint like OpenAI/Gemini
  // We'll return a hardcoded list of supported models
  std::vector<ModelInfo> defaults = {
      {.name = "claude-3-5-sonnet-20241022",
       .display_name = "Claude 3.5 Sonnet",
       .provider = "anthropic",
       .description = "Most intelligent model"},
      {.name = "claude-3-5-haiku-20241022",
       .display_name = "Claude 3.5 Haiku",
       .provider = "anthropic",
       .description = "Fastest and most cost-effective"},
      {.name = "claude-3-opus-20240229",
       .display_name = "Claude 3 Opus",
       .provider = "anthropic",
       .description = "Strong reasoning model"}};
  return defaults;
}

absl::Status AnthropicAIService::CheckAvailability() {
#ifndef YAZE_WITH_JSON
  return absl::UnimplementedError(
      "Anthropic AI service requires JSON support. Build with "
      "-DYAZE_WITH_JSON=ON");
#else
  if (config_.api_key.empty()) {
    return absl::FailedPreconditionError(
        "❌ Anthropic API key not configured\n"
        "   Set ANTHROPIC_API_KEY environment variable\n"
        "   Get your API key at: https://console.anthropic.com/");
  }
  return absl::OkStatus();
#endif
}

absl::StatusOr<AgentResponse> AnthropicAIService::GenerateResponse(
    const std::string& prompt) {
  return GenerateResponse(
      {{{agent::ChatMessage::Sender::kUser, prompt, absl::Now()}}});
}

absl::StatusOr<AgentResponse> AnthropicAIService::GenerateResponse(
    const std::vector<agent::ChatMessage>& history) {
#ifndef YAZE_WITH_JSON
  return absl::UnimplementedError(
      "Anthropic AI service requires JSON support. Build with "
      "-DYAZE_WITH_JSON=ON");
#else
  if (history.empty()) {
    return absl::InvalidArgumentError("History cannot be empty.");
  }

  if (config_.api_key.empty()) {
    return absl::FailedPreconditionError("Anthropic API key not configured");
  }

  absl::Time request_start = absl::Now();

  try {
    if (config_.verbose) {
      std::cerr << "[DEBUG] Using curl for Anthropic HTTPS request"
                << std::endl;
    }

    // Build messages array
    nlohmann::json messages = nlohmann::json::array();

    // Add conversation history
    int start_idx = std::max(0, static_cast<int>(history.size()) - 10);
    for (size_t i = start_idx; i < history.size(); ++i) {
      const auto& msg = history[i];
      std::string role = (msg.sender == agent::ChatMessage::Sender::kUser)
                             ? "user"
                             : "assistant";

      messages.push_back({{"role", role}, {"content", msg.message}});
    }

    // Build request body
    nlohmann::json request_body = {{"model", config_.model},
                                   {"max_tokens", config_.max_output_tokens},
                                   {"system", config_.system_instruction},
                                   {"messages", messages}};

    // Add function calling tools if enabled
    if (function_calling_enabled_) {
      try {
        std::string schemas_str = BuildFunctionCallSchemas();
        if (config_.verbose) {
          std::cerr << "[DEBUG] Function calling schemas: "
                    << schemas_str.substr(0, 200) << "..." << std::endl;
        }

        nlohmann::json schemas = nlohmann::json::parse(schemas_str);

        if (schemas.is_array() && !schemas.empty()) {
          // Convert OpenAI-style tools to Anthropic format
          nlohmann::json tools = nlohmann::json::array();
          for (const auto& schema : schemas) {
            // Check if it's already in tool format or just the function schema
            nlohmann::json tool_def;

            // Handle both bare schema and wrapped "function" schema
            nlohmann::json func_schema = schema;
            if (schema.contains("function")) {
              func_schema = schema["function"];
            }

            tool_def = {
                {"name", func_schema.value("name", "")},
                {"description", func_schema.value("description", "")},
                {"input_schema",
                 func_schema.value("parameters", nlohmann::json::object())}};

            tools.push_back(tool_def);
          }
          request_body["tools"] = tools;
        }
      } catch (const nlohmann::json::exception& e) {
        std::cerr << "⚠️  Failed to parse function schemas: " << e.what()
                  << std::endl;
      }
    }

    if (config_.verbose) {
      std::cerr << "[DEBUG] Sending " << messages.size()
                << " messages to Anthropic" << std::endl;
    }

    // Write request body to temp file
    std::string temp_file = "/tmp/anthropic_request.json";
    std::ofstream out(temp_file);
    out << request_body.dump();
    out.close();

    // Use curl to make the request
    std::string curl_cmd =
        "curl -s -X POST 'https://api.anthropic.com/v1/messages' "
        "-H 'x-api-key: " +
        config_.api_key +
        "' "
        "-H 'anthropic-version: 2023-06-01' "
        "-H 'content-type: application/json' "
        "-d @" +
        temp_file + " 2>&1";

    if (config_.verbose) {
      std::cerr << "[DEBUG] Executing Anthropic API request..." << std::endl;
    }

#ifdef _WIN32
    FILE* pipe = _popen(curl_cmd.c_str(), "r");
#else
    FILE* pipe = popen(curl_cmd.c_str(), "r");
#endif
    if (!pipe) {
      return absl::InternalError("Failed to execute curl command");
    }

    std::string response_str;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
      response_str += buffer;
    }

#ifdef _WIN32
    int status = _pclose(pipe);
#else
    int status = pclose(pipe);
#endif
    std::remove(temp_file.c_str());

    if (status != 0) {
      return absl::InternalError(
          absl::StrCat("Curl failed with status ", status));
    }

    if (response_str.empty()) {
      return absl::InternalError("Empty response from Anthropic API");
    }

    if (config_.verbose) {
      std::cout << "\n"
                << "\033[35m"
                << "🔍 Raw Anthropic API Response:"
                << "\033[0m"
                << "\n"
                << "\033[2m" << response_str.substr(0, 500) << "\033[0m"
                << "\n\n";
    }

    if (config_.verbose) {
      std::cerr << "[DEBUG] Parsing response..." << std::endl;
    }

    auto parsed_or = ParseAnthropicResponse(response_str);
    if (!parsed_or.ok()) {
      return parsed_or.status();
    }

    AgentResponse agent_response = std::move(parsed_or.value());
    agent_response.provider = "anthropic";
    agent_response.model = config_.model;
    agent_response.latency_seconds =
        absl::ToDoubleSeconds(absl::Now() - request_start);
    agent_response.parameters["prompt_version"] = config_.prompt_version;
    agent_response.parameters["temperature"] =
        absl::StrFormat("%.2f", config_.temperature);
    agent_response.parameters["max_output_tokens"] =
        absl::StrFormat("%d", config_.max_output_tokens);
    agent_response.parameters["function_calling"] =
        function_calling_enabled_ ? "true" : "false";

    return agent_response;

  } catch (const std::exception& e) {
    if (config_.verbose) {
      std::cerr << "[ERROR] Exception: " << e.what() << std::endl;
    }
    return absl::InternalError(
        absl::StrCat("Exception during generation: ", e.what()));
  }
#endif
}

absl::StatusOr<AgentResponse> AnthropicAIService::ParseAnthropicResponse(
    const std::string& response_body) {
#ifndef YAZE_WITH_JSON
  return absl::UnimplementedError("JSON support required");
#else
  AgentResponse agent_response;

  auto response_json = nlohmann::json::parse(response_body, nullptr, false);
  if (response_json.is_discarded()) {
    return absl::InternalError("❌ Failed to parse Anthropic response JSON");
  }

  // Check for errors
  if (response_json.contains("error")) {
    std::string error_msg =
        response_json["error"].value("message", "Unknown error");
    return absl::InternalError(
        absl::StrCat("❌ Anthropic API error: ", error_msg));
  }

  // Navigate Anthropic's response structure (Messages API)
  if (!response_json.contains("content") ||
      !response_json["content"].is_array()) {
    return absl::InternalError("❌ No content in Anthropic response");
  }

  for (const auto& block : response_json["content"]) {
    std::string type = block.value("type", "");

    if (type == "text") {
      std::string text_content = block.value("text", "");

      if (config_.verbose) {
        std::cout << "\n"
                  << "\033[35m"
                  << "🔍 Raw LLM Text:"
                  << "\033[0m"
                  << "\n"
                  << "\033[2m" << text_content << "\033[0m"
                  << "\n\n";
      }

      // Try to parse structured command format if present in text
      // (similar to OpenAI logic)

      // Strip markdown code blocks
      std::string clean_text =
          std::string(absl::StripAsciiWhitespace(text_content));
      if (absl::StartsWith(clean_text, "```json")) {
        clean_text = clean_text.substr(7);
      } else if (absl::StartsWith(clean_text, "```")) {
        clean_text = clean_text.substr(3);
      }
      if (absl::EndsWith(clean_text, "```")) {
        clean_text = clean_text.substr(0, clean_text.length() - 3);
      }
      clean_text = std::string(absl::StripAsciiWhitespace(clean_text));

      // Try to parse as JSON object
      auto parsed_text = nlohmann::json::parse(clean_text, nullptr, false);
      if (!parsed_text.is_discarded()) {
        if (parsed_text.contains("text_response") &&
            parsed_text["text_response"].is_string()) {
          agent_response.text_response =
              parsed_text["text_response"].get<std::string>();
        }
        if (parsed_text.contains("commands") &&
            parsed_text["commands"].is_array()) {
          for (const auto& cmd : parsed_text["commands"]) {
            if (cmd.is_string()) {
              std::string command = cmd.get<std::string>();
              if (absl::StartsWith(command, "z3ed ")) {
                command = command.substr(5);
              }
              agent_response.commands.push_back(command);
            }
          }
        }
      } else {
        // Use raw text as response if JSON parsing fails
        if (agent_response.text_response.empty()) {
          agent_response.text_response = text_content;
        } else {
          agent_response.text_response += "\n\n" + text_content;
        }
      }
    } else if (type == "tool_use") {
      ToolCall tool_call;
      tool_call.tool_name = block.value("name", "");

      if (block.contains("input") && block["input"].is_object()) {
        for (auto& [key, value] : block["input"].items()) {
          if (value.is_string()) {
            tool_call.args[key] = value.get<std::string>();
          } else if (value.is_number()) {
            tool_call.args[key] = std::to_string(value.get<double>());
          } else if (value.is_boolean()) {
            tool_call.args[key] = value.get<bool>() ? "true" : "false";
          }
        }
      }
      agent_response.tool_calls.push_back(tool_call);
    }
  }

  if (agent_response.text_response.empty() && agent_response.commands.empty() &&
      agent_response.tool_calls.empty()) {
    return absl::InternalError(
        "❌ No valid response extracted from Anthropic\n"
        "   Expected text or tool use");
  }

  return agent_response;
#endif
}

#endif  // YAZE_AI_RUNTIME_AVAILABLE

}  // namespace cli
}  // namespace yaze
