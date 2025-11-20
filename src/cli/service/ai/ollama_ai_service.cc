#include "cli/service/ai/ollama_ai_service.h"

#include <cstdlib>
#include <iostream>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "cli/service/agent/conversational_agent_service.h"

#ifdef YAZE_WITH_JSON
#include "httplib.h"
#include "nlohmann/json.hpp"
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
      config_.system_prompt =
          prompt_builder_.BuildSystemInstructionWithExamples();
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

void OllamaAIService::SetRomContext(Rom* rom) { prompt_builder_.SetRom(rom); }

absl::Status OllamaAIService::CheckAvailability() {
#ifndef YAZE_WITH_JSON
  return absl::UnimplementedError(
      "Ollama service requires JSON support. "
      "Build with -DZ3ED_AI=ON or -DYAZE_WITH_JSON=ON");
#else
  try {
    httplib::Client cli(config_.base_url);
    cli.set_connection_timeout(5);  // 5 second timeout

    auto res = cli.Get("/api/tags");
    if (!res) {
      return absl::UnavailableError(
          absl::StrFormat("Cannot connect to Ollama server at %s.\n"
                          "Make sure Ollama is installed and running:\n"
                          "  1. Install: brew install ollama (macOS) or "
                          "https://ollama.com/download\n"
                          "  2. Start: ollama serve\n"
                          "  3. Verify: curl http://localhost:11434/api/tags",
                          config_.base_url));
    }

    if (res->status != 200) {
      return absl::InternalError(
          absl::StrFormat("Ollama server error: HTTP %d\nResponse: %s",
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
      return absl::NotFoundError(
          absl::StrFormat("Model '%s' not found on Ollama server.\n"
                          "Pull it with: ollama pull %s\n"
                          "Available models: ollama list",
                          config_.model, config_.model));
    }

    return absl::OkStatus();
  } catch (const std::exception& e) {
    return absl::InternalError(
        absl::StrCat("Ollama health check failed: ", e.what()));
  }
#endif
}

absl::StatusOr<std::vector<ModelInfo>> OllamaAIService::ListAvailableModels() {
#ifndef YAZE_WITH_JSON
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
    std::vector<ModelInfo> models;

    if (models_json.contains("models") && models_json["models"].is_array()) {
      for (const auto& model : models_json["models"]) {
        ModelInfo info;
        info.provider = "ollama";
        info.is_local = true;

        if (model.contains("name") && model["name"].is_string()) {
          info.name = model["name"].get<std::string>();
          info.display_name = info.name;
        }

        if (model.contains("size")) {
          if (model["size"].is_string()) {
            info.size_bytes = std::strtoull(
                model["size"].get<std::string>().c_str(), nullptr, 10);
          } else if (model["size"].is_number_unsigned()) {
            info.size_bytes = model["size"].get<uint64_t>();
          }
        }

        if (model.contains("details") && model["details"].is_object()) {
          const auto& details = model["details"];
          info.parameter_size = details.value("parameter_size", "");
          info.quantization = details.value("quantization_level", "");
          info.family = details.value("family", "");

          // Build description
          std::string desc;
          if (!info.family.empty()) desc += info.family + " ";
          if (!info.parameter_size.empty()) desc += info.parameter_size + " ";
          if (!info.quantization.empty()) desc += "(" + info.quantization + ")";
          info.description = desc;
        }
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
    return absl::InternalError(
        absl::StrCat("Failed to parse Ollama response: ", e.what()));
  }
#endif
}

absl::StatusOr<AgentResponse> OllamaAIService::GenerateResponse(
    const std::string& prompt) {
  return GenerateResponse(
      {{{agent::ChatMessage::Sender::kUser, prompt, absl::Now()}}});
}

absl::StatusOr<AgentResponse> OllamaAIService::GenerateResponse(
    const std::vector<agent::ChatMessage>& history) {
#ifndef YAZE_WITH_JSON
  return absl::UnimplementedError(
      "Ollama service requires httplib and JSON support. "
      "Install vcpkg dependencies or use bundled libraries.");
#else
  if (history.empty()) {
    return absl::InvalidArgumentError("History cannot be empty.");
  }

  nlohmann::json messages = nlohmann::json::array();
  for (const auto& chat_msg : history) {
    if (chat_msg.is_internal) {
      continue;
    }
    nlohmann::json entry;
    entry["role"] = chat_msg.sender == agent::ChatMessage::Sender::kUser
                        ? "user"
                        : "assistant";
    entry["content"] = chat_msg.message;
    messages.push_back(std::move(entry));
  }

  if (messages.empty()) {
    return absl::InvalidArgumentError(
        "History does not contain any user/assistant messages.");
  }

  std::string fallback_prompt = prompt_builder_.BuildPromptFromHistory(history);

  nlohmann::json request_body;
  request_body["model"] = config_.model;
  request_body["system"] = config_.system_prompt;
  request_body["stream"] = config_.stream;
  request_body["format"] = "json";

  if (config_.use_chat_completions) {
    request_body["messages"] = messages;
  } else {
    request_body["prompt"] = fallback_prompt;
  }

  nlohmann::json options = {{"temperature", config_.temperature},
                            {"top_p", config_.top_p},
                            {"top_k", config_.top_k},
                            {"num_predict", config_.max_tokens},
                            {"num_ctx", config_.num_ctx}};
  request_body["options"] = options;

  AgentResponse agent_response;
  agent_response.provider = "ollama";

  try {
    httplib::Client cli(config_.base_url);
    cli.set_read_timeout(60);  // Longer timeout for inference

    const char* endpoint =
        config_.use_chat_completions ? "/api/chat" : "/api/generate";
    absl::Time request_start = absl::Now();
    auto res = cli.Post(endpoint, request_body.dump(), "application/json");

    if (!res) {
      return absl::UnavailableError(
          "Failed to connect to Ollama. Is 'ollama serve' running?\n"
          "Start with: ollama serve");
    }

    if (res->status != 200) {
      return absl::InternalError(absl::StrFormat(
          "Ollama API error: HTTP %d\nResponse: %s", res->status, res->body));
    }

    // Parse Ollama's wrapper JSON
    nlohmann::json ollama_wrapper;
    try {
      ollama_wrapper = nlohmann::json::parse(res->body);
    } catch (const nlohmann::json::exception& e) {
      return absl::InternalError(
          absl::StrFormat("Failed to parse Ollama response: %s\nBody: %s",
                          e.what(), res->body));
    }

    // Extract the LLM's response from Ollama's "response" field
    // For chat completions API, it's inside "message" -> "content"
    std::string llm_output;
    if (config_.use_chat_completions) {
      if (ollama_wrapper.contains("message") &&
          ollama_wrapper["message"].is_object() &&
          ollama_wrapper["message"].contains("content")) {
        llm_output = ollama_wrapper["message"]["content"].get<std::string>();
      } else {
        return absl::InvalidArgumentError(
            "Ollama chat response missing 'message.content'");
      }
    } else {
      if (ollama_wrapper.contains("response") &&
          ollama_wrapper["response"].is_string()) {
        llm_output = ollama_wrapper["response"].get<std::string>();
      } else {
        return absl::InvalidArgumentError(
            "Ollama response missing 'response' field");
      }
    }

    // Debug: Print raw LLM output when verbose mode is enabled
    const char* verbose_env = std::getenv("Z3ED_VERBOSE");
    if (verbose_env && std::string(verbose_env) == "1") {
      std::cout << "\n"
                << "\033[35m" << "🔍 Raw LLM Response:" << "\033[0m" << "\n"
                << "\033[2m" << llm_output << "\033[0m" << "\n\n";
    }

    // Parse the LLM's JSON response (the agent structure)
    nlohmann::json response_json;
    try {
      response_json = nlohmann::json::parse(llm_output);
    } catch (const nlohmann::json::exception& e) {
      // Sometimes the LLM includes extra text - try to extract JSON object
      size_t start = llm_output.find('{');
      size_t end = llm_output.rfind('}');

      if (start != std::string::npos && end != std::string::npos &&
          end > start) {
        std::string json_only = llm_output.substr(start, end - start + 1);
        try {
          response_json = nlohmann::json::parse(json_only);
        } catch (const nlohmann::json::exception&) {
          agent_response.warnings.push_back(
              "LLM response was not valid JSON; returning raw text.");
          agent_response.text_response = llm_output;
          return agent_response;
        }
      } else {
        agent_response.warnings.push_back(
            "LLM response did not contain a JSON object; returning raw text.");
        agent_response.text_response = llm_output;
        return agent_response;
      }
    }

    agent_response.model = ollama_wrapper.value("model", config_.model);
    agent_response.latency_seconds =
        absl::ToDoubleSeconds(absl::Now() - request_start);
    agent_response.parameters["temperature"] =
        absl::StrFormat("%.2f", config_.temperature);
    agent_response.parameters["top_p"] = absl::StrFormat("%.2f", config_.top_p);
    agent_response.parameters["top_k"] = absl::StrFormat("%d", config_.top_k);
    agent_response.parameters["num_predict"] =
        absl::StrFormat("%d", config_.max_tokens);
    agent_response.parameters["num_ctx"] =
        absl::StrFormat("%d", config_.num_ctx);
    agent_response.parameters["endpoint"] = endpoint;
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
