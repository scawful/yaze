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
#include "cli/service/ai/ai_config_utils.h"
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

std::vector<ModelInfo> CurrentOpenAiModels() {
  return {{.name = "gpt-6.1-sol",
           .display_name = "GPT-6.1 Sol",
           .provider = kProviderOpenAi,
           .description = "Suggested balanced text/tool model"},
          {.name = "gpt-6-luna",
           .display_name = "GPT-6 Luna",
           .provider = kProviderOpenAi,
           .description = "Efficient text/tool model"},
          {.name = "gpt-6-astra",
           .display_name = "GPT-6 Astra",
           .provider = kProviderOpenAi,
           .description = "Advanced reasoning text/tool model"}};
}

}  // namespace

OpenAIAIService::OpenAIAIService(const OpenAIConfig& config)
    : function_calling_enabled_(config.use_function_calling), config_(config) {
  config_.base_url = NormalizeOpenAiBaseUrl(config_.base_url);
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
          "dungeon-edit-inspect", "dungeon-edit-apply",
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
  const bool is_openai_cloud = IsOfficialOpenAiEndpoint(config_.base_url);
  if (config_.api_key.empty() && is_openai_cloud) {
    // Return default known models if API key is missing
    return CurrentOpenAiModels();
  }

  try {
    if (config_.verbose) {
      std::cerr << "[DEBUG] Listing OpenAI models..." << std::endl;
    }

    std::string response_str;
#if defined(YAZE_AI_IOS_URLSESSION)
    std::map<std::string, std::string> headers;
    if (!config_.api_key.empty()) {
      headers.emplace("Authorization", "Bearer " + config_.api_key);
    }
    auto resp_or = ios::UrlSessionHttpRequest(
        "GET", config_.base_url + "/v1/models", headers, "", 8000);
    if (!resp_or.ok()) {
      if (config_.verbose) {
        std::cerr << "[DEBUG] OpenAI /v1/models failed: "
                  << resp_or.status().message() << std::endl;
      }
      // Return defaults on failure so the UI remains usable.
      return ParseModelList("");
    }
    if (resp_or->status_code != 200) {
      if (config_.verbose) {
        std::cerr << "[DEBUG] OpenAI /v1/models HTTP " << resp_or->status_code
                  << std::endl;
      }
      return ParseModelList("");
    }
    response_str = resp_or->body;
#else
    // Use curl to list models from the API
    std::string auth_header =
        config_.api_key.empty()
            ? ""
            : "-H 'Authorization: Bearer " + config_.api_key + "' ";
    std::string curl_cmd = "curl -s -X GET '" + config_.base_url +
                           "/v1/models' " + auth_header + "2>&1";

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
    _pclose(pipe);
#else
    pclose(pipe);
#endif
#endif  // YAZE_AI_IOS_URLSESSION

    return ParseModelList(response_str);

  } catch (const std::exception& e) {
    return absl::InternalError(
        absl::StrCat("Failed to list models: ", e.what()));
  }
#endif
}

absl::StatusOr<std::vector<ModelInfo>> OpenAIAIService::ParseModelList(
    const std::string& body) {
#ifndef YAZE_WITH_JSON
  return absl::UnimplementedError("JSON support required");
#else
  auto models_json = nlohmann::json::parse(body, nullptr, false);
  if (models_json.is_discarded()) {
    if (IsOfficialOpenAiEndpoint(config_.base_url))
      return CurrentOpenAiModels();
    return absl::InternalError("Failed to parse OpenAI-compatible models JSON");
  }

  if (!models_json.contains("data") || !models_json["data"].is_array()) {
    if (IsOfficialOpenAiEndpoint(config_.base_url))
      return CurrentOpenAiModels();
    return absl::UnavailableError(
        "OpenAI-compatible endpoint did not return a model list");
  }

  std::vector<ModelInfo> models;
  for (const auto& m : models_json["data"]) {
    std::string id = m.value("id", "");

    // Keep custom endpoints open to their own model names. Cloud discovery
    // excludes specialized models that cannot perform native dungeon tools.
    bool is_local = !IsOfficialOpenAiEndpoint(config_.base_url);
    const bool chat = absl::StartsWith(id, "gpt-") ||
                      absl::StartsWith(id, "chatgpt") ||
                      absl::StartsWith(id, "o1") ||
                      absl::StartsWith(id, "o3") || absl::StartsWith(id, "o4");
    bool specialized = false;
    for (const std::string marker :
         {"embedding", "tts", "transcribe", "realtime", "audio", "image",
          "moderation", "search"}) {
      specialized = specialized || absl::StrContains(id, marker);
    }
    if (is_local || (chat && !specialized)) {
      ModelInfo info;
      info.name = id;
      info.display_name = id;
      info.provider = kProviderOpenAi;
      info.family = is_local ? "local" : "gpt";
      info.is_local = is_local;

      // Set display name based on model
      if (id == "gpt-4o")
        info.display_name = "GPT-4o";
      else if (id == "gpt-4o-mini")
        info.display_name = "GPT-4o Mini";
      else if (id == "gpt-4-turbo")
        info.display_name = "GPT-4 Turbo";
      else if (id == "gpt-3.5-turbo")
        info.display_name = "GPT-3.5 Turbo";
      else if (id == "o1-preview")
        info.display_name = "o1 Preview";
      else if (id == "o1-mini")
        info.display_name = "o1 Mini";

      models.push_back(std::move(info));
    }
  }
  return models;

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
    bool is_local_server = !IsOfficialOpenAiEndpoint(config_.base_url);
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

bool OpenAIAIService::UsesResponsesApi() const {
  if (!IsOfficialOpenAiEndpoint(config_.base_url))
    return false;
  // These native tool-capable model families require Responses. Explicit older
  // models and all OpenAI-compatible endpoints retain Chat Completions.
  for (const std::string family :
       {"gpt-6.1-sol", "gpt-6-astra", "gpt-6-sol", "gpt-6-luna"}) {
    if (config_.model == family ||
        absl::StartsWith(config_.model, family + "-"))
      return true;
  }
  return false;
}

absl::StatusOr<std::string> OpenAIAIService::BuildRequestBody(
    const std::vector<agent::ChatMessage>& history) {
#ifndef YAZE_WITH_JSON
  return absl::UnimplementedError("JSON support required");
#else
  const bool responses = UsesResponsesApi();
  const std::string api = responses ? "responses" : "chat.completions";
  nlohmann::json messages = nlohmann::json::array();
  if (!responses)
    messages.push_back(
        {{"role", "system"}, {"content", config_.system_instruction}});
  const size_t start = agent::CompleteTurnHistoryStart(history, 10);
  for (size_t i = start; i < history.size(); ++i) {
    const auto& msg = history[i];
    if (function_calling_enabled_ && msg.native_content &&
        msg.native_content->provider == kProviderOpenAi &&
        msg.native_content->model == config_.model &&
        msg.native_content->api == api) {
      auto content = nlohmann::json::parse(msg.native_content->content_json,
                                           nullptr, false);
      if (responses) {
        if (!content.is_array())
          return absl::InvalidArgumentError("Invalid OpenAI Responses history");
        for (const auto& item : content)
          messages.push_back(item);
      } else {
        if (!content.is_object())
          return absl::InvalidArgumentError("Invalid OpenAI assistant history");
        messages.push_back(std::move(content));
      }
      continue;
    }
    if (function_calling_enabled_ && msg.native_tool_result &&
        msg.native_tool_result->provider == kProviderOpenAi &&
        msg.native_tool_result->model == config_.model &&
        msg.native_tool_result->api == api) {
      const auto& result = *msg.native_tool_result;
      if (responses) {
        messages.push_back({{"type", "function_call_output"},
                            {"call_id", result.call_id},
                            {"output", result.output}});
      } else {
        messages.push_back({{"role", "tool"},
                            {"tool_call_id", result.call_id},
                            {"content", result.output}});
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
  nlohmann::json request_body = {{"model", config_.model}};
  if (responses) {
    request_body["input"] = messages;
    request_body["instructions"] = config_.system_instruction;
    request_body["max_output_tokens"] = config_.max_output_tokens;
    request_body["reasoning"] = {{"effort", "low"}};
    request_body["store"] = false;
    request_body["include"] = {"reasoning.encrypted_content"};
  } else {
    request_body["messages"] = messages;
    request_body["temperature"] = config_.temperature;
    request_body["max_tokens"] = config_.max_output_tokens;
  }
  if (function_calling_enabled_) {
    auto declarations_or =
        ToolSchemaBuilder::ResolveFunctionDeclarations(prompt_builder_);
    if (!declarations_or.ok())
      return declarations_or.status();
    if (responses) {
      nlohmann::json tools = nlohmann::json::array();
      for (auto function : *declarations_or) {
        function["type"] = "function";
        // Existing catalogue parameters include optional fields; do not silently
        // make them required through the Responses default strict mode.
        function["strict"] = false;
        tools.push_back(std::move(function));
      }
      if (!tools.empty()) {
        request_body["tools"] = std::move(tools);
        request_body["parallel_tool_calls"] = false;
      }
    } else if (!declarations_or->empty()) {
      request_body["tools"] =
          ToolSchemaBuilder::BuildOpenAITools(*declarations_or);
    }
  }
  return request_body.dump();
#endif
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

  const bool is_openai_cloud = IsOfficialOpenAiEndpoint(config_.base_url);
  if (config_.api_key.empty() && is_openai_cloud) {
    return absl::FailedPreconditionError("OpenAI API key not configured");
  }

  absl::Time request_start = absl::Now();

  try {
    if (config_.verbose) {
      std::cerr << "[DEBUG] Using curl for OpenAI HTTPS request" << std::endl;
      std::cerr << "[DEBUG] Processing " << history.size()
                << " messages in history" << std::endl;
    }

    auto request_or = BuildRequestBody(history);
    if (!request_or.ok())
      return request_or.status();
    const std::string& request_body = *request_or;

    const std::string endpoint =
        config_.base_url +
        (UsesResponsesApi() ? "/v1/responses" : "/v1/chat/completions");
    std::string response_str;
#if defined(YAZE_AI_IOS_URLSESSION)
    std::map<std::string, std::string> headers;
    headers.emplace("Content-Type", "application/json");
    if (!config_.api_key.empty()) {
      headers.emplace("Authorization", "Bearer " + config_.api_key);
    }
    auto resp_or = ios::UrlSessionHttpRequest("POST", endpoint, headers,
                                              request_body, 60000);
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
    response_str = resp_or->body;
#else
    // Write request body to temp file
    std::string temp_file = "/tmp/openai_request.json";
    std::ofstream out(temp_file);
    out << request_body;
    out.close();

    // Use curl to make the request
    std::string auth_header =
        config_.api_key.empty()
            ? ""
            : "-H 'Authorization: Bearer " + config_.api_key + "' ";
    std::string curl_cmd = "curl -s -X POST '" + endpoint +
                           "' "
                           "-H 'Content-Type: application/json' " +
                           auth_header + "-d @" + temp_file + " 2>&1";

    if (config_.verbose) {
      std::cerr << "[DEBUG] Executing OpenAI API request..." << std::endl;
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
    agent_response.parameters["api"] =
        UsesResponsesApi() ? "responses" : "chat.completions";
    if (!UsesResponsesApi()) {
      agent_response.parameters["temperature"] =
          absl::StrFormat("%.2f", config_.temperature);
    } else {
      agent_response.parameters["reasoning_effort"] = "low";
    }
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

  if (response_json.contains("output") && response_json["output"].is_array()) {
    nlohmann::json calls = nlohmann::json::array();
    std::string text;
    for (const auto& item : response_json["output"]) {
      const std::string type = item.value("type", "");
      if (type == "function_call") {
        calls.push_back({{"id", item.value("call_id", "")},
                         {"type", "function"},
                         {"function",
                          {{"name", item.value("name", "")},
                           {"arguments", item.value("arguments", "{}")}}}});
      } else if (type == "message" && item.contains("content")) {
        for (const auto& part : item["content"]) {
          if (part.value("type", "") == "output_text")
            text += part.value("text", "");
          else if (part.value("type", "") == "refusal")
            text += part.value("refusal", "");
        }
      }
    }
    if (response_json.value("status", "") == "incomplete" && text.empty() &&
        calls.empty()) {
      return absl::ResourceExhaustedError(
          "OpenAI response incomplete before text or tool calls; check "
          "max_output_tokens");
    }
    auto parsed = ParseOpenAIResponse(nlohmann::json{
        {"choices",
         {{{"message",
            {{"role", "assistant"},
             {"content", text},
             {"tool_calls", calls}}}}}}}.dump());
    if (!parsed.ok())
      return parsed.status();
    if (function_calling_enabled_) {
      parsed->native_content =
          NativeAIContent{kProviderOpenAi, config_.model, "responses",
                          response_json["output"].dump()};
    }
    if (response_json.value("status", "") == "incomplete") {
      parsed->warnings.push_back(
          "OpenAI response incomplete: " +
          response_json.value("incomplete_details", nlohmann::json::object())
              .dump());
    }
    return parsed;
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
  if (function_calling_enabled_) {
    agent_response.native_content = NativeAIContent{
        kProviderOpenAi, config_.model, "chat.completions", message.dump()};
  }
  if (choice.value("finish_reason", "") == "length") {
    agent_response.warnings.push_back("OpenAI finish_reason: length");
  }

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
      if (!function_calling_enabled_ && parsed_text.contains("tool_calls") &&
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
        tool_call.id = call.value("id", "");
        if (function_calling_enabled_ && tool_call.id.empty()) {
          return absl::InvalidArgumentError(
              "OpenAI native tool call is missing its call ID");
        }

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

}  // namespace cli
}  // namespace yaze
