#include "cli/service/ai/gemini_ai_service.h"
#include "cli/service/agent/conversational_agent_service.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"

#ifdef YAZE_WITH_JSON
#include <filesystem>
#include <fstream>
#include "httplib.h"
#include "nlohmann/json.hpp"
namespace fs = std::filesystem;
#endif

namespace yaze {
namespace cli {

GeminiAIService::GeminiAIService(const GeminiConfig& config) 
    : config_(config), function_calling_enabled_(true) {
  // Load command documentation into prompt builder
  if (auto status = prompt_builder_.LoadResourceCatalogue(""); !status.ok()) {
    std::cerr << "⚠️  Failed to load agent prompt catalogue: "
              << status.message() << std::endl;
  }
  
  if (config_.system_instruction.empty()) {
    // Use enhanced prompting by default
    if (config_.use_enhanced_prompting) {
      config_.system_instruction = prompt_builder_.BuildSystemInstructionWithExamples();
    } else {
      config_.system_instruction = BuildSystemInstruction();
    }
  }
}

void GeminiAIService::EnableFunctionCalling(bool enable) {
  function_calling_enabled_ = enable;
}

std::vector<std::string> GeminiAIService::GetAvailableTools() const {
  return {
    "resource_list",
    "dungeon_list_sprites",
    "overworld_find_tile",
    "overworld_describe_map",
    "overworld_list_warps"
  };
}

std::string GeminiAIService::BuildFunctionCallSchemas() {
#ifndef YAZE_WITH_JSON
  return "[]";  // Empty array if JSON not available
#else
  // Search for function_schemas.json in multiple locations
  const std::vector<std::string> search_paths = {
      "assets/agent/function_schemas.json",
      "../assets/agent/function_schemas.json",
      "../../assets/agent/function_schemas.json",
  };
  
  fs::path schema_path;
  bool found = false;
  
  for (const auto& candidate : search_paths) {
    fs::path resolved = fs::absolute(candidate);
    if (fs::exists(resolved)) {
      schema_path = resolved;
      found = true;
      break;
    }
  }
  
  if (!found) {
    std::cerr << "⚠️  Function schemas file not found. Tried paths:" << std::endl;
    for (const auto& path : search_paths) {
      std::cerr << "   - " << fs::absolute(path).string() << std::endl;
    }
    return "[]";  // Return empty array as fallback
  }
  
  // Load and parse the JSON file
  std::ifstream file(schema_path);
  if (!file.is_open()) {
    std::cerr << "⚠️  Failed to open function schemas file: " 
              << schema_path.string() << std::endl;
    return "[]";
  }
  
  try {
    nlohmann::json schemas_json;
    file >> schemas_json;
    return schemas_json.dump();
  } catch (const nlohmann::json::exception& e) {
    std::cerr << "⚠️  Failed to parse function schemas JSON: " 
              << e.what() << std::endl;
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

absl::Status GeminiAIService::CheckAvailability() {
#ifndef YAZE_WITH_JSON
  return absl::UnimplementedError(
      "Gemini AI service requires JSON support. Build with -DYAZE_WITH_JSON=ON");
#else
  if (config_.api_key.empty()) {
    return absl::FailedPreconditionError(
        "❌ Gemini API key not configured\n"
        "   Set GEMINI_API_KEY environment variable\n"
        "   Get your API key at: https://makersuite.google.com/app/apikey");
  }
  
  // Test API connectivity with a simple request
  httplib::Client cli("https://generativelanguage.googleapis.com");
  cli.set_connection_timeout(5, 0);  // 5 seconds timeout
  
  std::string test_endpoint = "/v1beta/models/" + config_.model;
  httplib::Headers headers = {
      {"x-goog-api-key", config_.api_key},
  };
  
  auto res = cli.Get(test_endpoint.c_str(), headers);
  
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
    return absl::InternalError(
        absl::StrCat("❌ Gemini API error: ", res->status, "\n   ", res->body));
  }
  
  return absl::OkStatus();
#endif
}

absl::StatusOr<AgentResponse> GeminiAIService::GenerateResponse(
    const std::string& prompt) {
  return GenerateResponse({{{agent::ChatMessage::Sender::kUser, prompt, absl::Now()}}});
}

absl::StatusOr<AgentResponse> GeminiAIService::GenerateResponse(
    const std::vector<agent::ChatMessage>& history) {
#ifndef YAZE_WITH_JSON
  return absl::UnimplementedError(
      "Gemini AI service requires JSON support. Build with -DYAZE_WITH_JSON=ON");
#else
  // TODO: Implement history-aware prompting.
  if (history.empty()) {
    return absl::InvalidArgumentError("History cannot be empty.");
  }
  
  std::string prompt = prompt_builder_.BuildPromptFromHistory(history);

  // Validate configuration
  if (auto status = CheckAvailability(); !status.ok()) {
    return status;
  }

  httplib::Client cli("https://generativelanguage.googleapis.com");
  cli.set_connection_timeout(30, 0);  // 30 seconds for generation
  
  // Build request with proper Gemini API v1beta format
  nlohmann::json request_body = {
      {"system_instruction", {
          {"parts", {
              {"text", config_.system_instruction}
          }}
      }},
      {"contents", {{
          {"parts", {{
              {"text", prompt}
          }}}
      }}},
      {"generationConfig", {
          {"temperature", config_.temperature},
          {"maxOutputTokens", config_.max_output_tokens},
          {"responseMimeType", "application/json"}
      }}
  };
  
  // Add function calling tools if enabled
  if (function_calling_enabled_) {
    try {
      nlohmann::json tools = nlohmann::json::parse(BuildFunctionCallSchemas());
      request_body["tools"] = {{
        {"function_declarations", tools}
      }};
    } catch (const nlohmann::json::exception& e) {
      std::cerr << "⚠️  Failed to parse function schemas: " << e.what() << std::endl;
    }
  }

  httplib::Headers headers = {
      {"Content-Type", "application/json"},
      {"x-goog-api-key", config_.api_key},
  };

  std::string endpoint = "/v1beta/models/" + config_.model + ":generateContent";
  auto res = cli.Post(endpoint.c_str(), headers, request_body.dump(), "application/json");

  if (!res) {
    return absl::InternalError("❌ Failed to connect to Gemini API");
  }

  if (res->status != 200) {
    return absl::InternalError(
        absl::StrCat("❌ Gemini API error: ", res->status, "\n   ", res->body));
  }

  return ParseGeminiResponse(res->body);
#endif
}

absl::StatusOr<AgentResponse> GeminiAIService::ParseGeminiResponse(
    const std::string& response_body) {
#ifdef YAZE_WITH_JSON
  AgentResponse agent_response;
  
  try {
    nlohmann::json response_json = nlohmann::json::parse(response_body);
    
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
        if (!part.contains("text")) {
          continue;
        }
        
        std::string text_content = part["text"].get<std::string>();
        
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
        
        // Parse as JSON object
        try {
          nlohmann::json response_json = nlohmann::json::parse(text_content);
          if (response_json.contains("text_response") &&
              response_json["text_response"].is_string()) {
            agent_response.text_response =
                response_json["text_response"].get<std::string>();
          }
          if (response_json.contains("reasoning") &&
              response_json["reasoning"].is_string()) {
            agent_response.reasoning =
                response_json["reasoning"].get<std::string>();
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
                std::string command = cmd.get<std::string>();
                if (absl::StartsWith(command, "z3ed ")) {
                  command = command.substr(5);
                }
                agent_response.commands.push_back(command);
              }
            }
          }
        } catch (const nlohmann::json::exception& inner_e) {
          // If parsing the full object fails, fallback to just commands
          std::vector<std::string> lines = absl::StrSplit(text_content, '\n');
          for (const auto& line : lines) {
            std::string trimmed = std::string(absl::StripAsciiWhitespace(line));
            if (!trimmed.empty() && 
                (absl::StartsWith(trimmed, "z3ed ") || 
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
      }
    }
  } catch (const nlohmann::json::exception& e) {
    return absl::InternalError(
        absl::StrCat("❌ Failed to parse Gemini response: ", e.what()));
  }
  
  if (agent_response.text_response.empty() && 
      agent_response.commands.empty() && 
      agent_response.tool_calls.empty()) {
    return absl::InternalError(
        "❌ No valid response extracted from Gemini\n"
        "   Expected at least one of: text_response, commands, or tool_calls\n"
        "   Raw response: " + response_body);
  }
  
  return agent_response;
#else
  return absl::UnimplementedError("JSON support required");
#endif
}

}  // namespace cli
}  // namespace yaze
