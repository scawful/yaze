#include "cli/service/ai/anthropic_ai_service.h"

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <map>
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
          "dungeon-edit-inspect", "dungeon-edit-apply",
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
  const std::vector<ModelInfo> suggestions = {
      {.name = "claude-sonnet-5-5",
       .display_name = "Claude Sonnet 5.5",
       .provider = kProviderAnthropic,
       .description = "Suggested balanced text/tool model"},
      {.name = "claude-haiku-5-5",
       .display_name = "Claude Haiku 5.5",
       .provider = kProviderAnthropic,
       .description = "Efficient text/tool model"},
      {.name = "claude-opus-5-5",
       .display_name = "Claude Opus 5.5",
       .provider = kProviderAnthropic,
       .description = "Advanced reasoning text/tool model"}};
#ifdef YAZE_WITH_JSON
  if (!config_.api_key.empty()) {
    // The existing native transport uses curl through a shell. Reject header
    // delimiters before constructing this new discovery command, without ever
    // echoing a supplied credential into diagnostics.
    if (config_.api_key.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghij"
                                          "klmnopqrstuvwxyz0123456789-_./+=") !=
        std::string::npos) {
      return absl::InvalidArgumentError(
          "Anthropic API key contains unsupported characters");
    }
    std::string response;
#if defined(YAZE_AI_IOS_URLSESSION)
    auto result = ios::UrlSessionHttpRequest(
        "GET", "https://api.anthropic.com/v1/models?limit=1000",
        {{"x-api-key", config_.api_key}, {"anthropic-version", "2023-06-01"}},
        "", 8000);
    if (result.ok() && result->status_code == 200)
      response = result->body;
#else
    const std::string command =
        "curl -s --max-time 8 'https://api.anthropic.com/v1/models?limit=1000' "
        "-H 'anthropic-version: 2023-06-01' -H 'x-api-key: " +
        config_.api_key + "'";
#ifdef _WIN32
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif
    if (pipe) {
      char buffer[4096];
      while (fgets(buffer, sizeof(buffer), pipe))
        response += buffer;
#ifdef _WIN32
      _pclose(pipe);
#else
      pclose(pipe);
#endif
    }
#endif
    const auto json = nlohmann::json::parse(response, nullptr, false);
    if (json.is_object() && json.contains("data") && json["data"].is_array()) {
      std::vector<ModelInfo> models;
      for (const auto& item : json["data"]) {
        const std::string id = item.value("id", "");
        if (!absl::StartsWith(id, "claude-"))
          continue;
        models.push_back({.name = id,
                          .display_name = item.value("display_name", id),
                          .provider = kProviderAnthropic,
                          .family = "claude"});
      }
      if (!models.empty())
        return models;
    }
  }
#endif
  return suggestions;
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

absl::StatusOr<std::string> AnthropicAIService::BuildRequestBody(
    const std::vector<agent::ChatMessage>& history) {
#ifndef YAZE_WITH_JSON
  return absl::UnimplementedError("JSON support required");
#else
  nlohmann::json messages = nlohmann::json::array();
  const size_t start = agent::CompleteTurnHistoryStart(history, 10);
  for (size_t i = start; i < history.size(); ++i) {
    const auto& msg = history[i];
    if (function_calling_enabled_ && msg.native_content &&
        msg.native_content->provider == kProviderAnthropic &&
        msg.native_content->model == config_.model &&
        msg.native_content->api == "messages") {
      auto content = nlohmann::json::parse(msg.native_content->content_json,
                                           nullptr, false);
      if (!content.is_array())
        return absl::InvalidArgumentError(
            "Invalid Anthropic assistant history");
      messages.push_back({{"role", "assistant"}, {"content", content}});
      continue;
    }
    if (function_calling_enabled_ && msg.native_tool_result &&
        msg.native_tool_result->provider == kProviderAnthropic &&
        msg.native_tool_result->model == config_.model &&
        msg.native_tool_result->api == "messages") {
      const auto& result = *msg.native_tool_result;
      nlohmann::json block = {{"type", "tool_result"},
                              {"tool_use_id", result.call_id},
                              {"content", result.output},
                              {"is_error", result.is_error}};
      if (!messages.empty() && messages.back().value("role", "") == "user" &&
          messages.back()["content"].is_array() &&
          messages.back()["content"][0].value("type", "") == "tool_result") {
        messages.back()["content"].push_back(std::move(block));
      } else {
        messages.push_back({{"role", "user"}, {"content", {block}}});
      }
      continue;
    }
    if (msg.message.empty())
      continue;
    messages.push_back({{"role", msg.sender == agent::ChatMessage::Sender::kUser
                                     ? "user"
                                     : "assistant"},
                        {"content", msg.message}});
  }
  nlohmann::json request_body = {{"model", config_.model},
                                 {"max_tokens", config_.max_output_tokens},
                                 {"system", config_.system_instruction},
                                 {"messages", messages}};
  if (config_.model == "claude-sonnet-5-5" ||
      config_.model == "claude-opus-5-5" ||
      config_.model == "claude-haiku-5-5") {
    request_body["output_config"] = {{"effort", "medium"}};
  }
  if (function_calling_enabled_) {
    auto tools_or = BuildAnthropicToolPayload(prompt_builder_);
    if (!tools_or.ok())
      return tools_or.status();
    if (!tools_or->empty())
      request_body["tools"] = *tools_or;
  }
  return request_body.dump();
#endif
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

    auto request_or = BuildRequestBody(history);
    if (!request_or.ok())
      return request_or.status();
    const std::string& request_body = *request_or;

    std::string response_str;
#if defined(YAZE_AI_IOS_URLSESSION)
    std::map<std::string, std::string> headers;
    headers.emplace("x-api-key", config_.api_key);
    headers.emplace("anthropic-version", "2023-06-01");
    headers.emplace("content-type", "application/json");
    auto resp_or = ios::UrlSessionHttpRequest(
        "POST", "https://api.anthropic.com/v1/messages", headers, request_body,
        60000);
    if (!resp_or.ok()) {
      return resp_or.status();
    }
    if (resp_or->status_code != 200) {
      return absl::InternalError(absl::StrCat(
          "Anthropic API error: ", resp_or->status_code, "\n", resp_or->body));
    }
    response_str = resp_or->body;
#else
    // Write request body to temp file
    std::string temp_file = "/tmp/anthropic_request.json";
    std::ofstream out(temp_file);
    out << request_body;
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
#endif  // YAZE_AI_IOS_URLSESSION

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

  if (function_calling_enabled_) {
    agent_response.native_content =
        NativeAIContent{kProviderAnthropic, config_.model, "messages",
                        response_json["content"].dump()};
  }
  const std::string stop = response_json.value("stop_reason", "");
  if (!stop.empty() && stop != "end_turn" && stop != "tool_use") {
    agent_response.warnings.push_back("Anthropic stop_reason: " + stop);
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
      tool_call.id = block.value("id", "");
      if (function_calling_enabled_ && tool_call.id.empty()) {
        return absl::InvalidArgumentError(
            "Anthropic native tool call is missing its ID");
      }

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

}  // namespace cli
}  // namespace yaze
