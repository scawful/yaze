#include "cli/service/ai/anthropic_ai_service.h"

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "cli/service/agent/conversational_agent_service.h"
#include "cli/service/ai/ai_http_client.h"
#include "cli/service/ai/provider_ids.h"
#include "cli/service/ai/tool_call_argument_codec.h"
#include "cli/service/ai/tool_schema_builder.h"
#include "util/platform_paths.h"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if defined(__APPLE__) && \
    (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
#include "cli/service/ai/ios_urlsession_http_client.h"
#define YAZE_AI_IOS_URLSESSION 1
#endif

#ifdef YAZE_WITH_JSON
#include <filesystem>
#include <fstream>

#include "httplib.h"
#include "nlohmann/json.hpp"
#endif

namespace yaze {
namespace cli {

#ifdef YAZE_AI_RUNTIME_AVAILABLE

namespace {

absl::StatusOr<nlohmann::json> BuildAnthropicToolPayload(
    const PromptBuilder& prompt_builder) {
  auto declarations_or =
      ToolSchemaBuilder::ResolveFunctionDeclarations(prompt_builder);
  if (!declarations_or.ok()) {
    return declarations_or.status();
  }
  return ToolSchemaBuilder::BuildAnthropicTools(*declarations_or);
}

}  // namespace

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

std::string AnthropicAIService::BuildSystemInstruction() {
  return prompt_builder_.BuildSystemInstruction();
}

void AnthropicAIService::SetRomContext(Rom* rom) {
  prompt_builder_.SetRom(rom);
}

absl::StatusOr<std::vector<ModelInfo>>
AnthropicAIService::ListAvailableModels() {
#ifndef YAZE_WITH_JSON
  return absl::UnimplementedError("Anthropic AI service requires JSON support");
#else
  if (config_.api_key.empty()) {
    return GetFallbackAnthropicModels();
  }

  std::map<std::string, std::string> headers = {
      {"x-api-key", config_.api_key},
      {"anthropic-version", "2023-06-01"},
  };
  auto resp_or = PerformAiHttpRequest("GET", "https://api.anthropic.com",
                                      "/v1/models", headers, "", 8000);
  if (!resp_or.ok() || resp_or->status_code != 200) {
    return GetFallbackAnthropicModels();
  }

  auto parsed = ParseAnthropicModelsJson(resp_or->body);
  if (parsed.empty()) {
    return GetFallbackAnthropicModels();
  }
  return parsed;
#endif
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
      std::cerr << "[DEBUG] Sending Anthropic API HTTPS request" << std::endl;
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
      auto tools_or = BuildAnthropicToolPayload(prompt_builder_);
      if (!tools_or.ok()) {
        if (config_.verbose) {
          std::cerr << "[DEBUG] Function calling schemas unavailable: "
                    << tools_or.status().message() << std::endl;
        }
      } else if (!tools_or->empty()) {
        if (config_.verbose) {
          std::string tools_str = tools_or->dump();
          std::cerr << "[DEBUG] Function calling schemas: "
                    << tools_str.substr(0, 200) << "..." << std::endl;
        }

        request_body["tools"] = *tools_or;
      }
    }

    if (config_.verbose) {
      std::cerr << "[DEBUG] Sending " << messages.size()
                << " messages to Anthropic" << std::endl;
    }

    std::map<std::string, std::string> headers = {
        {"x-api-key", config_.api_key},
        {"anthropic-version", "2023-06-01"},
        {"content-type", "application/json"},
    };
    auto resp_or = PerformAiHttpRequest("POST", "https://api.anthropic.com",
                                        "/v1/messages", headers,
                                        request_body.dump(), 60000);
    if (!resp_or.ok()) {
      return resp_or.status();
    }
    if (resp_or->status_code != 200) {
      return absl::InternalError(absl::StrCat(
          "Anthropic API error: ", resp_or->status_code, "\n", resp_or->body));
    }
    std::string response_str = std::move(resp_or->body);

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
    agent_response.provider = kProviderAnthropic;
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
        tool_call.args = ai::DecodeToolCallArguments(block["input"]);
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

std::vector<ModelInfo> GetFallbackAnthropicModels() {
  return {
      {.name = "claude-opus-5-5",
       .display_name = "Claude Opus 5.5",
       .provider = kProviderAnthropic,
       .description = "Highest capability Anthropic model",
       .family = "claude",
       .is_local = false},
      {.name = "claude-sonnet-5-5",
       .display_name = "Claude Sonnet 5.5",
       .provider = kProviderAnthropic,
       .description = "Balanced intelligence and speed",
       .family = "claude",
       .is_local = false},
      {.name = "claude-haiku-5-5",
       .display_name = "Claude Haiku 5.5",
       .provider = kProviderAnthropic,
       .description = "Fastest Anthropic model",
       .family = "claude",
       .is_local = false},
      {.name = "claude-fable-5-2",
       .display_name = "Claude Fable 5.2",
       .provider = kProviderAnthropic,
       .description = "Anthropic Fable 5.2",
       .family = "claude",
       .is_local = false},
  };
}

std::vector<ModelInfo> ParseAnthropicModelsJson(
    absl::string_view response_body) {
  std::vector<ModelInfo> models;
#ifdef YAZE_WITH_JSON
  auto json = nlohmann::json::parse(response_body, nullptr, false);
  if (json.is_discarded() || !json.contains("data") ||
      !json["data"].is_array()) {
    return models;
  }
  for (const auto& entry : json["data"]) {
    if (!entry.is_object()) {
      continue;
    }
    const std::string id = entry.value("id", "");
    if (id.empty() || absl::StartsWith(id, "claude-3")) {
      continue;
    }
    ModelInfo info;
    info.name = id;
    info.display_name = entry.value("display_name", id);
    if (info.display_name.empty()) {
      info.display_name = id;
    }
    info.provider = kProviderAnthropic;
    info.family = "claude";
    info.is_local = false;
    models.push_back(std::move(info));
  }
#else
  (void)response_body;
#endif
  return models;
}

}  // namespace cli
}  // namespace yaze
