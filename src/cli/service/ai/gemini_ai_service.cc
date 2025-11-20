#include "cli/service/ai/gemini_ai_service.h"

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
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

// Global flag to track OpenSSL initialization
static std::atomic<bool> g_openssl_initialized{false};
static std::mutex g_openssl_init_mutex;

static void InitializeOpenSSL() {
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

GeminiAIService::GeminiAIService(const GeminiConfig& config)
    : function_calling_enabled_(config.use_function_calling), config_(config) {
  if (config_.verbose) {
    std::cerr << "[DEBUG] Initializing Gemini service..." << std::endl;
    std::cerr << "[DEBUG] Function calling: "
              << (function_calling_enabled_ ? "enabled" : "disabled")
              << std::endl;
    std::cerr << "[DEBUG] Prompt version: " << config_.prompt_version
              << std::endl;
  }

#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
  // Initialize OpenSSL for HTTPS support
  InitializeOpenSSL();
  if (config_.verbose) {
    std::cerr << "[DEBUG] OpenSSL initialized for HTTPS" << std::endl;
  }
#endif

  // Load command documentation into prompt builder with specified version
  std::string catalogue_path = config_.prompt_version == "v2"
                                   ? "assets/agent/prompt_catalogue_v2.yaml"
                                   : "assets/agent/prompt_catalogue.yaml";
  if (auto status = prompt_builder_.LoadResourceCatalogue(catalogue_path);
      !status.ok()) {
    std::cerr << "⚠️  Failed to load agent prompt catalogue: "
              << status.message() << std::endl;
  }

  if (config_.verbose) {
    std::cerr << "[DEBUG] Loaded prompt catalogue" << std::endl;
  }

  if (config_.system_instruction.empty()) {
    if (config_.verbose) {
      std::cerr << "[DEBUG] Building system instruction..." << std::endl;
    }

    // Try to load version-specific system prompt file using FindAsset
    std::string prompt_file;
    if (config_.prompt_version == "v3") {
      prompt_file = "agent/system_prompt_v3.txt";
    } else if (config_.prompt_version == "v2") {
      prompt_file = "agent/system_prompt_v2.txt";
    } else {
      prompt_file = "agent/system_prompt.txt";
    }

    auto prompt_path = util::PlatformPaths::FindAsset(prompt_file);
    bool loaded = false;

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
        loaded = true;
      }
    }

    if (!loaded) {
      // Fallback to builder
      if (config_.use_enhanced_prompting) {
        config_.system_instruction =
            prompt_builder_.BuildSystemInstructionWithExamples();
      } else {
        config_.system_instruction = BuildSystemInstruction();
      }
    }
  }

  if (config_.verbose) {
    std::cerr << "[DEBUG] Gemini service initialized" << std::endl;
  }
}

void GeminiAIService::EnableFunctionCalling(bool enable) {
  function_calling_enabled_ = enable;
}

std::vector<std::string> GeminiAIService::GetAvailableTools() const {
  return {"resource-list",        "resource-search",
          "dungeon-list-sprites", "dungeon-describe-room",
          "overworld-find-tile",  "overworld-describe-map",
          "overworld-list-warps"};
}

std::string GeminiAIService::BuildFunctionCallSchemas() {
#ifndef YAZE_WITH_JSON
  return "{}";  // Empty object if JSON not available
#else
  // Use the prompt builder's schema generation which reads from
  // prompt_catalogue.yaml
  std::string schemas = prompt_builder_.BuildFunctionCallSchemas();
  if (!schemas.empty() && schemas != "[]") {
    return schemas;
  }

  // Fallback: Search for function_schemas.json using FindAsset
  auto schema_path_or =
      util::PlatformPaths::FindAsset("agent/function_schemas.json");

  if (!schema_path_or.ok()) {
    if (config_.verbose) {
      std::cerr << "⚠️  Function schemas file not found: "
                << schema_path_or.status().message() << std::endl;
    }
    return "[]";  // Return empty array as fallback
  }

  // Load and parse the JSON file
  std::ifstream file(schema_path_or->string());
  if (!file.is_open()) {
    std::cerr << "⚠️  Failed to open function schemas file: "
              << schema_path_or->string() << std::endl;
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

std::string GeminiAIService::BuildSystemInstruction() {
  // Fallback prompt if enhanced prompting is disabled
  // Use PromptBuilder's basic system instruction
  return prompt_builder_.BuildSystemInstruction();
}

void GeminiAIService::SetRomContext(Rom* rom) {
  prompt_builder_.SetRom(rom);
}

absl::StatusOr<std::vector<ModelInfo>> GeminiAIService::ListAvailableModels() {
#ifndef YAZE_WITH_JSON
  return absl::UnimplementedError("Gemini AI service requires JSON support");
#else
  if (config_.api_key.empty()) {
    // Return default known models if API key is missing
    std::vector<ModelInfo> defaults = {
        {.name = "gemini-2.0-flash-exp",
         .display_name = "Gemini 2.0 Flash",
         .provider = "gemini",
         .description = "Fastest multimodal model"},
        {.name = "gemini-1.5-flash",
         .display_name = "Gemini 1.5 Flash",
         .provider = "gemini",
         .description = "Balanced performance and speed"},
        {.name = "gemini-1.5-pro",
         .display_name = "Gemini 1.5 Pro",
         .provider = "gemini",
         .description = "High intelligence for complex tasks"}};
    return defaults;
  }

  try {
    // Use curl to list models from the API
    std::string endpoint =
        "https://generativelanguage.googleapis.com/v1beta/models?key=" +
        config_.api_key;
    std::string curl_cmd = "curl -s -X GET '" + endpoint + "' 2>&1";

    if (config_.verbose) {
      std::cerr << "[DEBUG] Listing models: "
                << curl_cmd.substr(0, curl_cmd.find("key=")) << "...'"
                << std::endl;
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
      return absl::InternalError("Failed to parse Gemini models JSON");
    }

    if (!models_json.contains("models")) {
      // Return defaults on error
      std::vector<ModelInfo> defaults = {{.name = "gemini-2.0-flash-exp",
                                          .display_name = "Gemini 2.0 Flash",
                                          .provider = "gemini"},
                                         {.name = "gemini-1.5-flash",
                                          .display_name = "Gemini 1.5 Flash",
                                          .provider = "gemini"},
                                         {.name = "gemini-1.5-pro",
                                          .display_name = "Gemini 1.5 Pro",
                                          .provider = "gemini"}};
      return defaults;
    }

    std::vector<ModelInfo> models;
    for (const auto& m : models_json["models"]) {
      std::string name = m.value("name", "");
      // Name comes as "models/gemini-pro", strip prefix
      if (absl::StartsWith(name, "models/")) {
        name = name.substr(7);
      }

      // Filter for gemini models
      if (absl::StartsWith(name, "gemini")) {
        ModelInfo info;
        info.name = name;
        info.display_name = m.value("displayName", name);
        info.provider = "gemini";
        info.description = m.value("description", "");
        info.family = "gemini";
        info.is_local = false;
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

absl::Status GeminiAIService::CheckAvailability() {
#ifndef YAZE_WITH_JSON
  return absl::UnimplementedError(
      "Gemini AI service requires JSON support. Build with "
      "-DYAZE_WITH_JSON=ON");
#else
  try {
    if (config_.verbose) {
      std::cerr << "[DEBUG] CheckAvailability: start" << std::endl;
    }

    if (config_.api_key.empty()) {
      return absl::FailedPreconditionError(
          "❌ Gemini API key not configured\n"
          "   Set GEMINI_API_KEY environment variable\n"
          "   Get your API key at: https://makersuite.google.com/app/apikey");
    }

    if (config_.verbose) {
      std::cerr << "[DEBUG] CheckAvailability: creating HTTPS client"
                << std::endl;
    }
    // Test API connectivity with a simple request
    httplib::Client cli("https://generativelanguage.googleapis.com");
    if (config_.verbose) {
      std::cerr << "[DEBUG] CheckAvailability: client created" << std::endl;
    }

    cli.set_connection_timeout(5, 0);  // 5 seconds timeout

    if (config_.verbose) {
      std::cerr << "[DEBUG] CheckAvailability: building endpoint" << std::endl;
    }
    std::string test_endpoint = "/v1beta/models/" + config_.model;
    httplib::Headers headers = {
        {"x-goog-api-key", config_.api_key},
    };

    if (config_.verbose) {
      std::cerr << "[DEBUG] CheckAvailability: making request to "
                << test_endpoint << std::endl;
    }
    auto res = cli.Get(test_endpoint.c_str(), headers);

    if (config_.verbose) {
      std::cerr << "[DEBUG] CheckAvailability: got response" << std::endl;
    }

    if (!res) {
      return absl::UnavailableError(
          "❌ Cannot reach Gemini API\n"
          "   Check your internet connection");
    }

    if (res->status == 401 || res->status == 403) {
      return absl::PermissionDeniedError(
          "❌ Invalid Gemini API key\n"
          "   Verify your key at: https://makersuite.google.com/app/apikey");
    }

    if (res->status == 404) {
      return absl::NotFoundError(
          absl::StrCat("❌ Model '", config_.model, "' not found\n",
                       "   Try: gemini-2.5-flash or gemini-1.5-pro"));
    }

    if (res->status != 200) {
      return absl::InternalError(absl::StrCat(
          "❌ Gemini API error: ", res->status, "\n   ", res->body));
    }

    return absl::OkStatus();
  } catch (const std::exception& e) {
    if (config_.verbose) {
      std::cerr << "[DEBUG] CheckAvailability: EXCEPTION: " << e.what()
                << std::endl;
    }
    return absl::InternalError(
        absl::StrCat("Exception during availability check: ", e.what()));
  } catch (...) {
    if (config_.verbose) {
      std::cerr << "[DEBUG] CheckAvailability: UNKNOWN EXCEPTION" << std::endl;
    }
    return absl::InternalError("Unknown exception during availability check");
  }
#endif
}

absl::StatusOr<AgentResponse> GeminiAIService::GenerateResponse(
    const std::string& prompt) {
  return GenerateResponse(
      {{{agent::ChatMessage::Sender::kUser, prompt, absl::Now()}}});
}

absl::StatusOr<AgentResponse> GeminiAIService::GenerateResponse(
    const std::vector<agent::ChatMessage>& history) {
#ifndef YAZE_WITH_JSON
  return absl::UnimplementedError(
      "Gemini AI service requires JSON support. Build with "
      "-DYAZE_WITH_JSON=ON");
#else
  if (history.empty()) {
    return absl::InvalidArgumentError("History cannot be empty.");
  }

  // Build a structured conversation history for better context
  // Gemini supports multi-turn conversations via the contents array
  std::string prompt = prompt_builder_.BuildPromptFromHistory(history);

  // Skip availability check - causes segfault with current SSL setup
  // TODO: Fix SSL/TLS initialization issue
  // if (auto status = CheckAvailability(); !status.ok()) {
  //   return status;
  // }

  if (config_.api_key.empty()) {
    return absl::FailedPreconditionError("Gemini API key not configured");
  }

  absl::Time request_start = absl::Now();

  try {
    if (config_.verbose) {
      std::cerr << "[DEBUG] Using curl for HTTPS request" << std::endl;
      std::cerr << "[DEBUG] Processing " << history.size()
                << " messages in history" << std::endl;
    }

    // Build conversation history for multi-turn context
    // Gemini supports alternating user/model messages for better context
    nlohmann::json contents = nlohmann::json::array();

    // Add conversation history (up to last 10 messages for context window)
    int start_idx = std::max(0, static_cast<int>(history.size()) - 10);
    for (size_t i = start_idx; i < history.size(); ++i) {
      const auto& msg = history[i];
      std::string role =
          (msg.sender == agent::ChatMessage::Sender::kUser) ? "user" : "model";

      nlohmann::json message = {{"role", role},
                                {"parts", {{{"text", msg.message}}}}};
      contents.push_back(message);
    }

    // If the last message is from the model, we need to ensure the conversation
    // ends with a user message for Gemini
    if (!history.empty() &&
        history.back().sender == agent::ChatMessage::Sender::kAgent) {
      // Add a continuation prompt
      nlohmann::json user_continuation = {
          {"role", "user"},
          {"parts", {{{"text", "Please continue or clarify your response."}}}}};
      contents.push_back(user_continuation);
    }

    // Build request with proper Gemini API v1beta format
    nlohmann::json request_body = {
        {"system_instruction",
         {{"parts", {{"text", config_.system_instruction}}}}},
        {"contents", contents},
        {"generationConfig",
         {{"temperature", config_.temperature},
          {"maxOutputTokens", config_.max_output_tokens}}}};

    if (config_.verbose) {
      std::cerr << "[DEBUG] Sending " << contents.size()
                << " conversation turns to Gemini" << std::endl;
    }

    // Only add responseMimeType if NOT using function calling
    // (Gemini doesn't support both at the same time)
    if (!function_calling_enabled_) {
      request_body["generationConfig"]["responseMimeType"] = "application/json";
    }

    // Add function calling tools if enabled
    if (function_calling_enabled_) {
      try {
        std::string schemas_str = BuildFunctionCallSchemas();
        if (config_.verbose) {
          std::cerr << "[DEBUG] Function calling schemas: "
                    << schemas_str.substr(0, 200) << "..." << std::endl;
        }

        nlohmann::json schemas = nlohmann::json::parse(schemas_str);

        // Build tools array - schemas might be an array of tools or a
        // function_declarations object
        if (schemas.is_array()) {
          // If it's already an array of tools, use it directly
          request_body["tools"] = {{{"function_declarations", schemas}}};
        } else if (schemas.is_object() &&
                   schemas.contains("function_declarations")) {
          // If it's a wrapper object with function_declarations
          request_body["tools"] = {
              {{"function_declarations", schemas["function_declarations"]}}};
        } else {
          // Treat as single tool object
          request_body["tools"] = {
              {{"function_declarations", nlohmann::json::array({schemas})}}};
        }
      } catch (const nlohmann::json::exception& e) {
        std::cerr << "⚠️  Failed to parse function schemas: " << e.what()
                  << std::endl;
      }
    }

    // Write request body to temp file
    std::string temp_file = "/tmp/gemini_request.json";
    std::ofstream out(temp_file);
    out << request_body.dump();
    out.close();

    // Use curl to make the request (avoiding httplib SSL issues)
    std::string endpoint =
        "https://generativelanguage.googleapis.com/v1beta/models/" +
        config_.model + ":generateContent";
    std::string curl_cmd = "curl -s -X POST '" + endpoint +
                           "' "
                           "-H 'Content-Type: application/json' "
                           "-H 'x-goog-api-key: " +
                           config_.api_key +
                           "' "
                           "-d @" +
                           temp_file + " 2>&1";

    if (config_.verbose) {
      std::cerr << "[DEBUG] Executing API request..." << std::endl;
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
      return absl::InternalError("Empty response from Gemini API");
    }

    // Debug: print response
    if (config_.verbose) {
      std::cout << "\n"
                << "\033[35m" << "🔍 Raw Gemini API Response:" << "\033[0m"
                << "\n"
                << "\033[2m" << response_str.substr(0, 500) << "\033[0m"
                << "\n\n";
    }

    if (config_.verbose) {
      std::cerr << "[DEBUG] Parsing response..." << std::endl;
    }
    auto parsed_or = ParseGeminiResponse(response_str);
    if (!parsed_or.ok()) {
      return parsed_or.status();
    }
    AgentResponse agent_response = std::move(parsed_or.value());
    agent_response.provider = "gemini";
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
  } catch (...) {
    if (config_.verbose) {
      std::cerr << "[ERROR] Unknown exception" << std::endl;
    }
    return absl::InternalError("Unknown exception during generation");
  }
#endif
}

absl::StatusOr<AgentResponse> GeminiAIService::ParseGeminiResponse(
    const std::string& response_body) {
#ifndef YAZE_WITH_JSON
  return absl::UnimplementedError("JSON support required");
#else
  AgentResponse agent_response;

  auto response_json = nlohmann::json::parse(response_body, nullptr, false);
  if (response_json.is_discarded()) {
    return absl::InternalError("❌ Failed to parse Gemini response JSON");
  }

  // Navigate Gemini's response structure
  if (!response_json.contains("candidates") ||
      response_json["candidates"].empty()) {
    return absl::InternalError("❌ No candidates in Gemini response");
  }

  for (const auto& candidate : response_json["candidates"]) {
    if (!candidate.contains("content") ||
        !candidate["content"].contains("parts")) {
      continue;
    }

    for (const auto& part : candidate["content"]["parts"]) {
      if (part.contains("text")) {
        std::string text_content = part["text"].get<std::string>();

        // Debug: Print raw LLM output when verbose mode is enabled
        if (config_.verbose) {
          std::cout << "\n"
                    << "\033[35m" << "🔍 Raw LLM Response:" << "\033[0m" << "\n"
                    << "\033[2m" << text_content << "\033[0m" << "\n\n";
        }

        // Strip markdown code blocks if present (```json ... ```)
        text_content = std::string(absl::StripAsciiWhitespace(text_content));
        if (absl::StartsWith(text_content, "```json")) {
          text_content = text_content.substr(7);  // Remove ```json
        } else if (absl::StartsWith(text_content, "```")) {
          text_content = text_content.substr(3);  // Remove ```
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
            agent_response.reasoning =
                parsed_text["reasoning"].get<std::string>();
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

          // Extract tool_calls from the parsed JSON
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
                      tool_call.args[key] =
                          value.get<bool>() ? "true" : "false";
                    }
                  }
                }
                agent_response.tool_calls.push_back(tool_call);
              }
            }
          }
        } else {
          // If parsing the full object fails, fallback to extracting commands
          // from text
          std::vector<std::string> lines = absl::StrSplit(text_content, '\n');
          for (const auto& line : lines) {
            std::string trimmed = std::string(absl::StripAsciiWhitespace(line));
            if (!trimmed.empty() && (absl::StartsWith(trimmed, "z3ed ") ||
                                     absl::StartsWith(trimmed, "palette ") ||
                                     absl::StartsWith(trimmed, "overworld ") ||
                                     absl::StartsWith(trimmed, "sprite ") ||
                                     absl::StartsWith(trimmed, "dungeon "))) {
              if (absl::StartsWith(trimmed, "z3ed ")) {
                trimmed = trimmed.substr(5);
              }
              agent_response.commands.push_back(trimmed);
            }
          }
        }
      } else if (part.contains("functionCall")) {
        const auto& call = part["functionCall"];
        if (call.contains("name") && call["name"].is_string()) {
          ToolCall tool_call;
          tool_call.tool_name = call["name"].get<std::string>();
          if (call.contains("args") && call["args"].is_object()) {
            for (auto& [key, value] : call["args"].items()) {
              if (value.is_string()) {
                tool_call.args[key] = value.get<std::string>();
              } else if (value.is_number()) {
                tool_call.args[key] = std::to_string(value.get<double>());
              }
            }
          }
          agent_response.tool_calls.push_back(tool_call);
        }
      }
    }
  }

  if (agent_response.text_response.empty() && agent_response.commands.empty() &&
      agent_response.tool_calls.empty()) {
    return absl::InternalError(
        "❌ No valid response extracted from Gemini\n"
        "   Expected at least one of: text_response, commands, or tool_calls\n"
        "   Raw response: " +
        response_body);
  }

  return agent_response;
#endif
}

absl::StatusOr<std::string> GeminiAIService::EncodeImageToBase64(
    const std::string& image_path) const {
#ifndef YAZE_WITH_JSON
  (void)image_path;  // Suppress unused parameter warning
  return absl::UnimplementedError(
      "Gemini AI service requires JSON support. Build with "
      "-DYAZE_WITH_JSON=ON");
#else
  std::ifstream file(image_path, std::ios::binary);
  if (!file.is_open()) {
    return absl::NotFoundError(
        absl::StrCat("Failed to open image file: ", image_path));
  }

  // Read file into buffer
  file.seekg(0, std::ios::end);
  size_t size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<unsigned char> buffer(size);
  if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
    return absl::InternalError("Failed to read image file");
  }

  // Base64 encode
  static const char* base64_chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  std::string encoded;
  encoded.reserve(((size + 2) / 3) * 4);

  int i = 0;
  int j = 0;
  unsigned char char_array_3[3];
  unsigned char char_array_4[4];

  for (size_t idx = 0; idx < size; idx++) {
    char_array_3[i++] = buffer[idx];
    if (i == 3) {
      char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
      char_array_4[1] =
          ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
      char_array_4[2] =
          ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
      char_array_4[3] = char_array_3[2] & 0x3f;

      for (i = 0; i < 4; i++)
        encoded += base64_chars[char_array_4[i]];
      i = 0;
    }
  }

  if (i) {
    for (j = i; j < 3; j++)
      char_array_3[j] = '\0';

    char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
    char_array_4[1] =
        ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
    char_array_4[2] =
        ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

    for (j = 0; j < i + 1; j++)
      encoded += base64_chars[char_array_4[j]];

    while (i++ < 3)
      encoded += '=';
  }

  return encoded;
#endif
}

absl::StatusOr<AgentResponse> GeminiAIService::GenerateMultimodalResponse(
    const std::string& image_path, const std::string& prompt) {
#ifndef YAZE_WITH_JSON
  (void)image_path;  // Suppress unused parameter warnings
  (void)prompt;
  return absl::UnimplementedError(
      "Gemini AI service requires JSON support. Build with "
      "-DYAZE_WITH_JSON=ON");
#else
  if (config_.api_key.empty()) {
    return absl::FailedPreconditionError("Gemini API key not configured");
  }

  // Determine MIME type from file extension
  std::string mime_type = "image/png";
  if (image_path.ends_with(".jpg") || image_path.ends_with(".jpeg")) {
    mime_type = "image/jpeg";
  } else if (image_path.ends_with(".bmp")) {
    mime_type = "image/bmp";
  } else if (image_path.ends_with(".webp")) {
    mime_type = "image/webp";
  }

  // Encode image to base64
  auto encoded_or = EncodeImageToBase64(image_path);
  if (!encoded_or.ok()) {
    return encoded_or.status();
  }
  std::string encoded_image = std::move(encoded_or.value());

  try {
    if (config_.verbose) {
      std::cerr << "[DEBUG] Preparing multimodal request with image"
                << std::endl;
    }

    // Build multimodal request with image and text
    nlohmann::json request_body = {
        {"contents",
         {{{"parts",
            {{{"inline_data",
               {{"mime_type", mime_type}, {"data", encoded_image}}}},
             {{"text", prompt}}}}}}},
        {"generationConfig",
         {{"temperature", config_.temperature},
          {"maxOutputTokens", config_.max_output_tokens}}}};

    // Write request body to temp file
    std::string temp_file = "/tmp/gemini_multimodal_request.json";
    std::ofstream out(temp_file);
    out << request_body.dump();
    out.close();

    // Use curl to make the request
    std::string endpoint =
        "https://generativelanguage.googleapis.com/v1beta/models/" +
        config_.model + ":generateContent";
    std::string curl_cmd = "curl -s -X POST '" + endpoint +
                           "' "
                           "-H 'Content-Type: application/json' "
                           "-H 'x-goog-api-key: " +
                           config_.api_key +
                           "' "
                           "-d @" +
                           temp_file + " 2>&1";

    if (config_.verbose) {
      std::cerr << "[DEBUG] Executing multimodal API request..." << std::endl;
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
      return absl::InternalError("Empty response from Gemini API");
    }

    if (config_.verbose) {
      std::cout << "\n"
                << "\033[35m"
                << "🔍 Raw Gemini Multimodal Response:" << "\033[0m" << "\n"
                << "\033[2m" << response_str.substr(0, 500) << "\033[0m"
                << "\n\n";
    }

    return ParseGeminiResponse(response_str);

  } catch (const std::exception& e) {
    if (config_.verbose) {
      std::cerr << "[ERROR] Exception: " << e.what() << std::endl;
    }
    return absl::InternalError(
        absl::StrCat("Exception during multimodal generation: ", e.what()));
  }
#endif
}

}  // namespace cli
}  // namespace yaze
