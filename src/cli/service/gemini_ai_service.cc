#include "cli/service/gemini_ai_service.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"

#ifdef YAZE_WITH_JSON
#include "incl/httplib.h"
#include "third_party/json/src/json.hpp"
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
                     "   Try: gemini-1.5-flash or gemini-1.5-pro"));
  }
  
  if (res->status != 200) {
    return absl::InternalError(
        absl::StrCat("❌ Gemini API error: ", res->status, "\n   ", res->body));
  }
  
  return absl::OkStatus();
#endif
}

absl::StatusOr<std::vector<std::string>> GeminiAIService::GetCommands(
    const std::string& prompt) {
#ifndef YAZE_WITH_JSON
  return absl::UnimplementedError(
      "Gemini AI service requires JSON support. Build with -DYAZE_WITH_JSON=ON");
#else
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

absl::StatusOr<std::vector<std::string>> GeminiAIService::ParseGeminiResponse(
    const std::string& response_body) {
#ifdef YAZE_WITH_JSON
  std::vector<std::string> commands;
  
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
        
        // Parse as JSON array
        try {
          nlohmann::json commands_array = nlohmann::json::parse(text_content);
          
          if (commands_array.is_array()) {
            for (const auto& cmd : commands_array) {
              if (cmd.is_string()) {
                std::string command = cmd.get<std::string>();
                // Remove "z3ed " prefix if LLM included it
                if (absl::StartsWith(command, "z3ed ")) {
                  command = command.substr(5);
                }
                commands.push_back(command);
              }
            }
          }
        } catch (const nlohmann::json::exception& inner_e) {
          // Fallback: Try to extract commands line by line
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
              commands.push_back(trimmed);
            }
          }
        }
      }
    }
  } catch (const nlohmann::json::exception& e) {
    return absl::InternalError(
        absl::StrCat("❌ Failed to parse Gemini response: ", e.what()));
  }
  
  if (commands.empty()) {
    return absl::InternalError(
        "❌ No valid commands extracted from Gemini response\n"
        "   Raw response: " + response_body);
  }
  
  return commands;
#else
  return absl::UnimplementedError("JSON support required");
#endif
}

}  // namespace cli
}  // namespace yaze
