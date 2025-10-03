#include "cli/service/ai/ollama_ai_service.h"

#include <cstdlib>
#include <iostream>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "cli/service/agent/conversational_agent_service.h"

// Check if we have httplib available (from vcpkg or bundled)
#if __has_include("httplib.h")
#define YAZE_HAS_HTTPLIB 1
#include "httplib.h"
#elif __has_include("incl/httplib.h")
#define YAZE_HAS_HTTPLIB 1
#include "incl/httplib.h"
#else
#define YAZE_HAS_HTTPLIB 0
#endif

// Check if we have JSON library available
#if __has_include("third_party/json/src/json.hpp")
#define YAZE_HAS_JSON 1
#include "third_party/json/src/json.hpp"
#elif __has_include("json.hpp")
#define YAZE_HAS_JSON 1
#include "json.hpp"
#else
#define YAZE_HAS_JSON 0
#endif

namespace yaze {
namespace cli {

OllamaAIService::OllamaAIService(const OllamaConfig& config) : config_(config) {
  // Load command documentation into prompt builder
  if (auto status = prompt_builder_.LoadResourceCatalogue(""); !status.ok()) {
    std::cerr << "⚠️  Failed to load agent prompt catalogue: "
              << status.message() << std::endl;
  }
  
  if (config_.system_prompt.empty()) {
    // Use enhanced prompting by default
    if (config_.use_enhanced_prompting) {
      config_.system_prompt = prompt_builder_.BuildSystemInstructionWithExamples();
    } else {
      config_.system_prompt = BuildSystemPrompt();
    }
  }
}

std::string OllamaAIService::BuildSystemPrompt() {
  // Fallback prompt if enhanced prompting is disabled
  // Use PromptBuilder's basic system instruction
  return prompt_builder_.BuildSystemInstruction();
}

void OllamaAIService::SetRomContext(Rom* rom) {
  prompt_builder_.SetRom(rom);
}

absl::Status OllamaAIService::CheckAvailability() {
#if !YAZE_HAS_HTTPLIB || !YAZE_HAS_JSON
  return absl::UnimplementedError(
      "Ollama service requires httplib and JSON support. "
      "Install vcpkg dependencies or use bundled libraries.");
#else
  try {
    httplib::Client cli(config_.base_url);
    cli.set_connection_timeout(5);  // 5 second timeout
    
    auto res = cli.Get("/api/tags");
    if (!res) {
      return absl::UnavailableError(absl::StrFormat(
          "Cannot connect to Ollama server at %s.\n"
          "Make sure Ollama is installed and running:\n"
          "  1. Install: brew install ollama (macOS) or https://ollama.com/download\n"
          "  2. Start: ollama serve\n"
          "  3. Verify: curl http://localhost:11434/api/tags",
          config_.base_url));
    }
    
    if (res->status != 200) {
      return absl::InternalError(absl::StrFormat(
          "Ollama server error: HTTP %d\nResponse: %s", 
          res->status, res->body));
    }
    
    // Check if requested model is available
    nlohmann::json models_json = nlohmann::json::parse(res->body);
    bool model_found = false;
    
    if (models_json.contains("models") && models_json["models"].is_array()) {
      for (const auto& model : models_json["models"]) {
        if (model.contains("name")) {
          std::string model_name = model["name"].get<std::string>();
          if (model_name.find(config_.model) != std::string::npos) {
            model_found = true;
            break;
          }
        }
      }
    }
    
    if (!model_found) {
      return absl::NotFoundError(absl::StrFormat(
          "Model '%s' not found on Ollama server.\n"
          "Pull it with: ollama pull %s\n"
          "Available models: ollama list",
          config_.model, config_.model));
    }
    
    return absl::OkStatus();
  } catch (const std::exception& e) {
    return absl::InternalError(absl::StrCat(
        "Ollama health check failed: ", e.what()));
  }
#endif
}

absl::StatusOr<std::vector<std::string>> OllamaAIService::ListAvailableModels() {
#if !YAZE_HAS_HTTPLIB || !YAZE_HAS_JSON
  return absl::UnimplementedError("Requires httplib and JSON support");
#else
  try {
    httplib::Client cli(config_.base_url);
    cli.set_connection_timeout(5);
    
    auto res = cli.Get("/api/tags");
    
    if (!res || res->status != 200) {
      return absl::UnavailableError(
          "Cannot list Ollama models. Is the server running?");
    }
    
    nlohmann::json models_json = nlohmann::json::parse(res->body);
    std::vector<std::string> models;
    
    if (models_json.contains("models") && models_json["models"].is_array()) {
      for (const auto& model : models_json["models"]) {
        if (model.contains("name")) {
          models.push_back(model["name"].get<std::string>());
        }
      }
    }
    
    return models;
  } catch (const std::exception& e) {
    return absl::InternalError(absl::StrCat(
        "Failed to list models: ", e.what()));
  }
#endif
}

absl::StatusOr<std::string> OllamaAIService::ParseOllamaResponse(
    const std::string& json_response) {
#if !YAZE_HAS_JSON
  return absl::UnimplementedError("Requires JSON support");
#else
  try {
    nlohmann::json response_json = nlohmann::json::parse(json_response);
    
    if (!response_json.contains("response")) {
      return absl::InvalidArgumentError(
          "Ollama response missing 'response' field");
    }
    
    return response_json["response"].get<std::string>();
  } catch (const nlohmann::json::exception& e) {
    return absl::InternalError(absl::StrCat(
        "Failed to parse Ollama response: ", e.what()));
  }
#endif
}

absl::StatusOr<AgentResponse> OllamaAIService::GenerateResponse(
    const std::string& prompt) {
  return GenerateResponse({{{agent::ChatMessage::Sender::kUser, prompt, absl::Now()}}});
}

absl::StatusOr<AgentResponse> OllamaAIService::GenerateResponse(
    const std::vector<agent::ChatMessage>& history) {
#if !YAZE_HAS_HTTPLIB || !YAZE_HAS_JSON
  return absl::UnimplementedError(
      "Ollama service requires httplib and JSON support. "
      "Install vcpkg dependencies or use bundled libraries.");
#else
  // TODO: Implement history-aware prompting.
  if (history.empty()) {
    return absl::InvalidArgumentError("History cannot be empty.");
  }
  std::string prompt = prompt_builder_.BuildPromptFromHistory(history);

  // Build request payload
  nlohmann::json request_body = {
      {"model", config_.model},
      {"system", config_.system_prompt},
      {"prompt", prompt},
      {"stream", false},
      {"options",
       {{"temperature", config_.temperature},
        {"num_predict", config_.max_tokens}}},
      {"format", "json"}  // Force JSON output
  };
  
  try {
    httplib::Client cli(config_.base_url);
    cli.set_read_timeout(60);  // Longer timeout for inference
    
    auto res = cli.Post("/api/generate", request_body.dump(), "application/json");
    
    if (!res) {
      return absl::UnavailableError(
          "Failed to connect to Ollama. Is 'ollama serve' running?\n"
          "Start with: ollama serve");
    }
    
    if (res->status != 200) {
      return absl::InternalError(absl::StrFormat(
          "Ollama API error: HTTP %d\nResponse: %s", 
          res->status, res->body));
    }
    
    // Parse response to extract generated text
    nlohmann::json response_json;
    try {
      response_json = nlohmann::json::parse(res->body);
    } catch (const nlohmann::json::exception& e) {
      // Sometimes the LLM includes extra text - try to extract JSON object
      size_t start = res->body.find('{');
      size_t end = res->body.rfind('}');
      
      if (start != std::string::npos && end != std::string::npos && end > start) {
        std::string json_only = res->body.substr(start, end - start + 1);
        try {
          response_json = nlohmann::json::parse(json_only);
        } catch (const nlohmann::json::exception&) {
          return absl::InvalidArgumentError(
              "LLM did not return valid JSON. Response:\n" + res->body);
        }
      } else {
        return absl::InvalidArgumentError(
            "LLM did not return a JSON object. Response:\n" + res->body);
      }
    }
    
    AgentResponse agent_response;
    if (response_json.contains("text_response") &&
        response_json["text_response"].is_string()) {
      agent_response.text_response =
          response_json["text_response"].get<std::string>();
    }
    if (response_json.contains("reasoning") &&
        response_json["reasoning"].is_string()) {
      agent_response.reasoning = response_json["reasoning"].get<std::string>();
    }
    if (response_json.contains("tool_calls") &&
        response_json["tool_calls"].is_array()) {
      for (const auto& call : response_json["tool_calls"]) {
        if (call.contains("tool_name") && call["tool_name"].is_string()) {
          ToolCall tool_call;
          tool_call.tool_name = call["tool_name"].get<std::string>();
          if (call.contains("args") && call["args"].is_object()) {
            for (auto& [key, value] : call["args"].items()) {
              if (value.is_string()) {
                tool_call.args[key] = value.get<std::string>();
              }
            }
          }
          agent_response.tool_calls.push_back(tool_call);
        }
      }
    }
    if (response_json.contains("commands") &&
        response_json["commands"].is_array()) {
      for (const auto& cmd : response_json["commands"]) {
        if (cmd.is_string()) {
          agent_response.commands.push_back(cmd.get<std::string>());
        }
      }
    }

    return agent_response;

  } catch (const std::exception& e) {
    return absl::InternalError(
        absl::StrCat("Ollama request failed: ", e.what()));
  }
#endif
}

}  // namespace cli
}  // namespace yaze
