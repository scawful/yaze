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
#include "httplib.h"
#include "nlohmann/json.hpp"
#endif

namespace yaze {
namespace cli {

GeminiAIService::GeminiAIService(const GeminiConfig& config) 
    : config_(config) {
  // Load command documentation into prompt builder
  prompt_builder_.LoadResourceCatalogue("");  // TODO: Pass actual yaml path when available
  
  if (config_.system_instruction.empty()) {
    // Use enhanced prompting by default
    if (config_.use_enhanced_prompting) {
      config_.system_instruction = prompt_builder_.BuildSystemInstructionWithExamples();
    } else {
      config_.system_instruction = BuildSystemInstruction();
    }
  }
}

std::string GeminiAIService::BuildSystemInstruction() {
  // Fallback prompt if enhanced prompting is disabled
  // Use PromptBuilder's basic system instruction
  return prompt_builder_.BuildSystemInstruction();
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
  
  if (agent_response.commands.empty()) {
    return absl::InternalError(
        "❌ No valid commands extracted from Gemini response\n"
        "   Raw response: " + response_body);
  }
  
  return agent_response;
#else
  return absl::UnimplementedError("JSON support required");
#endif
}

}  // namespace cli
}  // namespace yaze
