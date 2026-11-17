#include "cli/service/ai/openai_ai_service.h"

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

// OpenSSL initialization for HTTPS support
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

// OpenSSL initialization guards (local to this TU)
static std::atomic<bool> g_openssl_initialized{false};
static std::mutex g_openssl_init_mutex;

static void EnsureOpenSSLInitialized() {
  std::lock_guard<std::mutex> lock(g_openssl_init_mutex);
  if (!g_openssl_initialized.exchange(true)) {
    OPENSSL_init_ssl(
        OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS,
        nullptr);
    std::cerr << "✓ OpenSSL initialized for HTTPS support" << std::endl;
  }
}
#endif
#endif

namespace yaze {
namespace cli {

#ifdef YAZE_AI_RUNTIME_AVAILABLE

namespace {

absl::StatusOr<nlohmann::json> BuildOpenAIToolPayload(
    const PromptBuilder& prompt_builder) {
  auto declarations_or =
      ToolSchemaBuilder::ResolveFunctionDeclarations(prompt_builder);
  if (!declarations_or.ok()) {
    return declarations_or.status();
  }
  return ToolSchemaBuilder::BuildOpenAITools(*declarations_or);
}

}  // namespace

OpenAIAIService::OpenAIAIService(const OpenAIConfig& config)
    : function_calling_enabled_(config.use_function_calling), config_(config) {
  if (config_.verbose) {
    std::cerr << "[DEBUG] Initializing OpenAI service..." << std::endl;
    std::cerr << "[DEBUG] Model: " << config_.model << std::endl;
    std::cerr << "[DEBUG] Function calling: "
              << (function_calling_enabled_ ? "enabled" : "disabled")
              << std::endl;
  }

#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
  EnsureOpenSSLInitialized();
  if (config_.verbose) {
    std::cerr << "[DEBUG] OpenSSL initialized for HTTPS" << std::endl;
  }
#endif

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
    std::cerr << "[DEBUG] OpenAI service initialized" << std::endl;
  }
}

void OpenAIAIService::EnableFunctionCalling(bool enable) {
  function_calling_enabled_ = enable;
}

std::vector<std::string> OpenAIAIService::GetAvailableTools() const {
  return {"resource-list",        "resource-search",
          "dungeon-list-sprites", "dungeon-describe-room",
          "overworld-find-tile",  "overworld-describe-map",
          "overworld-list-warps"};
}

std::string OpenAIAIService::BuildSystemInstruction() {
  return prompt_builder_.BuildSystemInstruction();
}

void OpenAIAIService::SetRomContext(Rom* rom) {
  prompt_builder_.SetRom(rom);
}

absl::StatusOr<std::vector<ModelInfo>> OpenAIAIService::ListAvailableModels() {
#ifndef YAZE_WITH_JSON
  return absl::UnimplementedError("OpenAI AI service requires JSON support");
#else
  const bool is_openai_cloud =
      absl::StrContains(config_.base_url, "api.openai.com");
  if (config_.api_key.empty() && is_openai_cloud) {
    return GetFallbackOpenAIModels();
  }

  try {
    if (config_.verbose) {
      std::cerr << "[DEBUG] Listing OpenAI-compatible models..." << std::endl;
    }

    std::map<std::string, std::string> headers;
    if (!config_.api_key.empty()) {
      headers.emplace("Authorization", "Bearer " + config_.api_key);
    }

    auto resp_or = PerformAiHttpRequest("GET", config_.base_url, "/v1/models",
                                        headers, "", 8000);
    if (!resp_or.ok() || resp_or->status_code != 200) {
      if (!is_openai_cloud) {
        return absl::UnavailableError(
            absl::StrCat("Cannot reach local OpenAI-compatible server at ",
                         config_.base_url));
      }
      return GetFallbackOpenAIModels();
    }

    auto models = ParseOpenAIModelsJson(resp_or->body, !is_openai_cloud);
    if (models.empty()) {
      if (!is_openai_cloud) {
        return models;
      }
      return GetFallbackOpenAIModels();
    }
    return models;
  } catch (const std::exception& e) {
    return absl::InternalError(
        absl::StrCat("Failed to list models: ", e.what()));
  }
#endif
}

absl::Status OpenAIAIService::CheckAvailability() {
#ifndef YAZE_WITH_JSON
  return absl::UnimplementedError(
      "OpenAI AI service requires JSON support. Build with "
      "-DYAZE_WITH_JSON=ON");
#else
  try {
    // LMStudio and other local servers don't require API keys
    bool is_local_server = config_.base_url != "https://api.openai.com";
    if (config_.api_key.empty() && !is_local_server) {
      return absl::FailedPreconditionError(
          "❌ OpenAI API key not configured\n"
          "   Set OPENAI_API_KEY environment variable\n"
          "   Get your API key at: https://platform.openai.com/api-keys\n"
          "   For LMStudio, use --openai_base_url=http://localhost:1234");
    }

    // Test API connectivity with a simple request
#if defined(YAZE_AI_IOS_URLSESSION)
    std::map<std::string, std::string> headers;
    if (!config_.api_key.empty()) {
      headers.emplace("Authorization", "Bearer " + config_.api_key);
    }
    auto resp_or = ios::UrlSessionHttpRequest(
        "GET", config_.base_url + "/v1/models", headers, "", 8000);
    if (!resp_or.ok()) {
      return absl::UnavailableError(absl::StrCat(
          "❌ Cannot reach OpenAI API\n   ", resp_or.status().message()));
    }
    if (resp_or->status_code == 401) {
      return absl::PermissionDeniedError(
          "❌ Invalid OpenAI API key\n"
          "   Verify your key at: https://platform.openai.com/api-keys");
    }
    if (resp_or->status_code != 200) {
      return absl::InternalError(
          absl::StrCat("❌ OpenAI API error: ", resp_or->status_code, "\n   ",
                       resp_or->body));
    }
#else
    httplib::Client cli(config_.base_url);
    cli.set_connection_timeout(5, 0);

    httplib::Headers headers = {};
    if (!config_.api_key.empty()) {
      headers.emplace("Authorization", "Bearer " + config_.api_key);
    }

    auto res = cli.Get("/v1/models", headers);

    if (!res) {
      return absl::UnavailableError(
          "❌ Cannot reach OpenAI API\n"
          "   Check your internet connection");
    }

    if (res->status == 401) {
      return absl::PermissionDeniedError(
          "❌ Invalid OpenAI API key\n"
          "   Verify your key at: https://platform.openai.com/api-keys");
    }

    if (res->status != 200) {
      return absl::InternalError(absl::StrCat(
          "❌ OpenAI API error: ", res->status, "\n   ", res->body));
    }
#endif

    return absl::OkStatus();
  } catch (const std::exception& e) {
    return absl::InternalError(
        absl::StrCat("Exception during availability check: ", e.what()));
  }
#endif
}

absl::StatusOr<AgentResponse> OpenAIAIService::GenerateResponse(
    const std::string& prompt) {
  return GenerateResponse(
      {{{agent::ChatMessage::Sender::kUser, prompt, absl::Now()}}});
}

absl::StatusOr<AgentResponse> OpenAIAIService::GenerateResponse(
    const std::vector<agent::ChatMessage>& history) {
#ifndef YAZE_WITH_JSON
  return absl::UnimplementedError(
      "OpenAI AI service requires JSON support. Build with "
      "-DYAZE_WITH_JSON=ON");
#else
  if (history.empty()) {
    return absl::InvalidArgumentError("History cannot be empty.");
  }

  const bool is_openai_cloud =
      absl::StrContains(config_.base_url, "api.openai.com");
  if (config_.api_key.empty() && is_openai_cloud) {
    return absl::FailedPreconditionError("OpenAI API key not configured");
  }

  absl::Time request_start = absl::Now();

  try {
    if (config_.verbose) {
      std::cerr << "[DEBUG] Sending OpenAI-compatible HTTP request"
                << std::endl;
      std::cerr << "[DEBUG] Processing " << history.size()
                << " messages in history" << std::endl;
    }

    // Build messages array for OpenAI format
    nlohmann::json messages = nlohmann::json::array();

    // Add system message
    messages.push_back(
        {{"role", "system"}, {"content", config_.system_instruction}});

    // Add conversation history (up to last 10 messages for context window)
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
                                   {"messages", messages},
                                   {"temperature", config_.temperature},
                                   {"max_tokens", config_.max_output_tokens}};

    // Add function calling tools if enabled
    if (function_calling_enabled_) {
      auto tools_or = BuildOpenAIToolPayload(prompt_builder_);
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
                << " messages to OpenAI" << std::endl;
    }

    std::map<std::string, std::string> headers = {
        {"Content-Type", "application/json"},
    };
    if (!config_.api_key.empty()) {
      headers.emplace("Authorization", "Bearer " + config_.api_key);
    }
    auto resp_or =
        PerformAiHttpRequest("POST", config_.base_url, "/v1/chat/completions",
                             headers, request_body.dump(), 60000);
    if (!resp_or.ok()) {
      return resp_or.status();
    }
    if (resp_or->status_code == 401) {
      return absl::PermissionDeniedError(
          "❌ Invalid OpenAI API key\n"
          "   Verify your key at: https://platform.openai.com/api-keys");
    }
    if (resp_or->status_code != 200) {
      return absl::InternalError(
          absl::StrCat("❌ OpenAI API error: ", resp_or->status_code, "\n   ",
                       resp_or->body));
    }
    std::string response_str = std::move(resp_or->body);

    if (response_str.empty()) {
      return absl::InternalError("Empty response from OpenAI API");
    }

    if (config_.verbose) {
      std::cout << "\n"
                << "\033[35m"
                << "🔍 Raw OpenAI API Response:"
                << "\033[0m"
                << "\n"
                << "\033[2m" << response_str.substr(0, 500) << "\033[0m"
                << "\n\n";
    }

    if (config_.verbose) {
      std::cerr << "[DEBUG] Parsing response..." << std::endl;
    }

    auto parsed_or = ParseOpenAIResponse(response_str);
    if (!parsed_or.ok()) {
      return parsed_or.status();
    }

    AgentResponse agent_response = std::move(parsed_or.value());
    agent_response.provider = kProviderOpenAi;
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

absl::StatusOr<AgentResponse> OpenAIAIService::ParseOpenAIResponse(
    const std::string& response_body) {
#ifndef YAZE_WITH_JSON
  return absl::UnimplementedError("JSON support required");
#else
  AgentResponse agent_response;

  auto response_json = nlohmann::json::parse(response_body, nullptr, false);
  if (response_json.is_discarded()) {
    return absl::InternalError("❌ Failed to parse OpenAI response JSON");
  }

  // Check for errors
  if (response_json.contains("error")) {
    std::string error_msg =
        response_json["error"].value("message", "Unknown error");
    return absl::InternalError(
        absl::StrCat("❌ OpenAI API error: ", error_msg));
  }

  // Navigate OpenAI's response structure
  if (!response_json.contains("choices") || response_json["choices"].empty()) {
    return absl::InternalError("❌ No choices in OpenAI response");
  }

  const auto& choice = response_json["choices"][0];
  if (!choice.contains("message")) {
    return absl::InternalError("❌ No message in OpenAI response");
  }

  const auto& message = choice["message"];

  // Extract text content
  if (message.contains("content") && !message["content"].is_null()) {
    std::string text_content = message["content"].get<std::string>();

    if (config_.verbose) {
      std::cout << "\n"
                << "\033[35m"
                << "🔍 Raw LLM Response:"
                << "\033[0m"
                << "\n"
                << "\033[2m" << text_content << "\033[0m"
                << "\n\n";
    }

    // Strip markdown code blocks if present
    text_content = std::string(absl::StripAsciiWhitespace(text_content));
    if (absl::StartsWith(text_content, "```json")) {
      text_content = text_content.substr(7);
    } else if (absl::StartsWith(text_content, "```")) {
      text_content = text_content.substr(3);
    }
    if (absl::EndsWith(text_content, "```")) {
      text_content = text_content.substr(0, text_content.length() - 3);
    }
    text_content = std::string(absl::StripAsciiWhitespace(text_content));

    // Try to parse as JSON object
    auto parsed_text = nlohmann::json::parse(text_content, nullptr, false);
    if (!parsed_text.is_discarded()) {
      // Extract text_response
      if (parsed_text.contains("text_response") &&
          parsed_text["text_response"].is_string()) {
        agent_response.text_response =
            parsed_text["text_response"].get<std::string>();
      }

      // Extract reasoning
      if (parsed_text.contains("reasoning") &&
          parsed_text["reasoning"].is_string()) {
        agent_response.reasoning = parsed_text["reasoning"].get<std::string>();
      }

      // Extract commands
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

      // Extract tool_calls from parsed JSON
      if (parsed_text.contains("tool_calls") &&
          parsed_text["tool_calls"].is_array()) {
        for (const auto& call : parsed_text["tool_calls"]) {
          if (call.contains("tool_name") && call["tool_name"].is_string()) {
            ToolCall tool_call;
            tool_call.tool_name = call["tool_name"].get<std::string>();
            if (call.contains("args") && call["args"].is_object()) {
              tool_call.args = ai::DecodeToolCallArguments(call["args"]);
            }
            agent_response.tool_calls.push_back(tool_call);
          }
        }
      }
    } else {
      // Use raw text as response
      agent_response.text_response = text_content;
    }
  }

  // Handle native OpenAI tool calls
  if (message.contains("tool_calls") && message["tool_calls"].is_array()) {
    for (const auto& call : message["tool_calls"]) {
      if (call.contains("function")) {
        const auto& func = call["function"];
        ToolCall tool_call;
        tool_call.tool_name = func.value("name", "");

        if (func.contains("arguments") && func["arguments"].is_string()) {
          auto args_json = nlohmann::json::parse(
              func["arguments"].get<std::string>(), nullptr, false);
          if (!args_json.is_discarded() && args_json.is_object()) {
            tool_call.args = ai::DecodeToolCallArguments(args_json);
          }
        }
        agent_response.tool_calls.push_back(tool_call);
      }
    }
  }

  if (agent_response.text_response.empty() && agent_response.commands.empty() &&
      agent_response.tool_calls.empty()) {
    return absl::InternalError(
        "❌ No valid response extracted from OpenAI\n"
        "   Expected at least one of: text_response, commands, or tool_calls");
  }

  return agent_response;
#endif
}

#endif  // YAZE_AI_RUNTIME_AVAILABLE

std::vector<ModelInfo> GetFallbackOpenAIModels() {
  return {
      {.name = "gpt-5.2",
       .display_name = "GPT-5.2",
       .provider = kProviderOpenAi,
       .description = "Flagship OpenAI reasoning model",
       .family = "gpt-5",
       .is_local = false},
      {.name = "gpt-5.1",
       .display_name = "GPT-5.1",
       .provider = kProviderOpenAi,
       .description = "High capability OpenAI model",
       .family = "gpt-5",
       .is_local = false},
      {.name = "gpt-5",
       .display_name = "GPT-5",
       .provider = kProviderOpenAi,
       .description = "General purpose GPT-5 model",
       .family = "gpt-5",
       .is_local = false},
      {.name = "gpt-5-mini",
       .display_name = "GPT-5 Mini",
       .provider = kProviderOpenAi,
       .description = "Fast and cost-effective GPT-5 model",
       .family = "gpt-5",
       .is_local = false},
      {.name = "gpt-4.1",
       .display_name = "GPT-4.1",
       .provider = kProviderOpenAi,
       .description = "GPT-4.1",
       .family = "gpt-4.1",
       .is_local = false},
      {.name = "gpt-4.1-mini",
       .display_name = "GPT-4.1 Mini",
       .provider = kProviderOpenAi,
       .description = "GPT-4.1 Mini",
       .family = "gpt-4.1",
       .is_local = false},
      {.name = "gpt-4o",
       .display_name = "GPT-4o",
       .provider = kProviderOpenAi,
       .description = "Multimodal GPT-4o",
       .family = "gpt-4o",
       .is_local = false},
      {.name = "gpt-4o-mini",
       .display_name = "GPT-4o Mini",
       .provider = kProviderOpenAi,
       .description = "Lightweight GPT-4o Mini",
       .family = "gpt-4o",
       .is_local = false},
  };
}

std::vector<ModelInfo> ParseOpenAIModelsJson(absl::string_view response_body,
                                             bool is_local_endpoint,
                                             absl::string_view provider_label) {
  std::vector<ModelInfo> models;
#ifdef YAZE_WITH_JSON
  auto models_json = nlohmann::json::parse(response_body, nullptr, false);
  if (models_json.is_discarded() || !models_json.contains("data") ||
      !models_json["data"].is_array()) {
    return models;
  }

  for (const auto& m : models_json["data"]) {
    if (!m.is_object()) {
      continue;
    }
    const std::string id = m.value("id", "");
    if (id.empty()) {
      continue;
    }
    if (!is_local_endpoint) {
      if (absl::StartsWith(id, "gpt-3.5") || absl::StrContains(id, "audio") ||
          absl::StrContains(id, "realtime") ||
          absl::StrContains(id, "transcribe") || absl::StrContains(id, "tts") ||
          absl::StrContains(id, "image") || absl::StrContains(id, "search")) {
        continue;
      }
      const bool allowed =
          absl::StartsWith(id, "gpt-5") || absl::StartsWith(id, "gpt-4.1") ||
          absl::StartsWith(id, "gpt-4o") || absl::StartsWith(id, "o3") ||
          absl::StartsWith(id, "o4") || absl::StartsWith(id, "o1") ||
          absl::StartsWith(id, "chatgpt");
      if (!allowed) {
        continue;
      }
    }

    ModelInfo info;
    info.name = id;
    info.display_name = id;
    info.provider = std::string(provider_label);
    info.family = is_local_endpoint ? "local" : "gpt";
    info.is_local = is_local_endpoint;

    if (id == "gpt-5.2") {
      info.display_name = "GPT-5.2";
    } else if (id == "gpt-5.1") {
      info.display_name = "GPT-5.1";
    } else if (id == "gpt-5") {
      info.display_name = "GPT-5";
    } else if (id == "gpt-5-mini") {
      info.display_name = "GPT-5 Mini";
    } else if (id == "gpt-4o") {
      info.display_name = "GPT-4o";
    } else if (id == "gpt-4o-mini") {
      info.display_name = "GPT-4o Mini";
    }

    models.push_back(std::move(info));
  }
#else
  (void)response_body;
  (void)is_local_endpoint;
  (void)provider_label;
#endif
  return models;
}

}  // namespace cli
}  // namespace yaze
