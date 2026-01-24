#include "cli/service/ai/openai_ai_service.h"

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/match.h"
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

std::string OpenAIAIService::BuildFunctionCallSchemas() {
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
  if (config_.api_key.empty()) {
    // Return default known models if API key is missing
    std::vector<ModelInfo> defaults = {
        {.name = "gpt-4o",
         .display_name = "GPT-4o",
         .provider = "openai",
         .description = "Most capable GPT-4 model"},
        {.name = "gpt-4o-mini",
         .display_name = "GPT-4o Mini",
         .provider = "openai",
         .description = "Fast and cost-effective"},
        {.name = "gpt-4-turbo",
         .display_name = "GPT-4 Turbo",
         .provider = "openai",
         .description = "GPT-4 with larger context"},
        {.name = "gpt-3.5-turbo",
         .display_name = "GPT-3.5 Turbo",
         .provider = "openai",
         .description = "Fast and efficient"}};
    return defaults;
  }

  try {
    // Use curl to list models from the API
    std::string auth_header = config_.api_key.empty()
        ? ""
        : "-H 'Authorization: Bearer " + config_.api_key + "' ";
    std::string curl_cmd =
        "curl -s -X GET '" + config_.base_url + "/v1/models' " +
        auth_header + "2>&1";

    if (config_.verbose) {
      std::cerr << "[DEBUG] Listing OpenAI models..." << std::endl;
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
    _pclose(pipe);
#else
    pclose(pipe);
#endif

    auto models_json = nlohmann::json::parse(response_str, nullptr, false);
    if (models_json.is_discarded()) {
      return absl::InternalError("Failed to parse OpenAI models JSON");
    }

    if (!models_json.contains("data")) {
      // Return defaults on error
      std::vector<ModelInfo> defaults = {
          {.name = "gpt-4o-mini",
           .display_name = "GPT-4o Mini",
           .provider = "openai"},
          {.name = "gpt-4o", .display_name = "GPT-4o", .provider = "openai"},
          {.name = "gpt-3.5-turbo",
           .display_name = "GPT-3.5 Turbo",
           .provider = "openai"}};
      return defaults;
    }

    std::vector<ModelInfo> models;
    for (const auto& m : models_json["data"]) {
      std::string id = m.value("id", "");

      // Filter for chat models (gpt-4*, gpt-3.5-turbo*, o1*, chatgpt*)
      // For local servers (LM Studio), we accept all models.
      bool is_local = !absl::StrContains(config_.base_url, "api.openai.com");
      
      if (is_local || absl::StartsWith(id, "gpt-4") || absl::StartsWith(id, "gpt-3.5") ||
          absl::StartsWith(id, "o1") || absl::StartsWith(id, "chatgpt")) {
        ModelInfo info;
        info.name = id;
        info.display_name = id;
        info.provider = "openai";
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

  if (config_.api_key.empty()) {
    return absl::FailedPreconditionError("OpenAI API key not configured");
  }

  absl::Time request_start = absl::Now();

  try {
    if (config_.verbose) {
      std::cerr << "[DEBUG] Using curl for OpenAI HTTPS request" << std::endl;
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
      try {
        std::string schemas_str = BuildFunctionCallSchemas();
        if (config_.verbose) {
          std::cerr << "[DEBUG] Function calling schemas: "
                    << schemas_str.substr(0, 200) << "..." << std::endl;
        }

        nlohmann::json schemas = nlohmann::json::parse(schemas_str);

        if (schemas.is_array() && !schemas.empty()) {
          // Convert to OpenAI tools format
          nlohmann::json tools = nlohmann::json::array();
          for (const auto& schema : schemas) {
            tools.push_back({{"type", "function"}, {"function", schema}});
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
                << " messages to OpenAI" << std::endl;
    }

    // Write request body to temp file
    std::string temp_file = "/tmp/openai_request.json";
    std::ofstream out(temp_file);
    out << request_body.dump();
    out.close();

    // Use curl to make the request
    std::string auth_header = config_.api_key.empty()
        ? ""
        : "-H 'Authorization: Bearer " + config_.api_key + "' ";
    std::string curl_cmd =
        "curl -s -X POST '" + config_.base_url + "/v1/chat/completions' "
        "-H 'Content-Type: application/json' " +
        auth_header +
        "-d @" +
        temp_file + " 2>&1";

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
    agent_response.provider = "openai";
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
    return absl::InternalError(absl::StrCat("❌ OpenAI API error: ", error_msg));
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
              for (auto& [key, value] : call["args"].items()) {
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
            for (auto& [key, value] : args_json.items()) {
              if (value.is_string()) {
                tool_call.args[key] = value.get<std::string>();
              } else if (value.is_number()) {
                tool_call.args[key] = std::to_string(value.get<double>());
              }
            }
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
