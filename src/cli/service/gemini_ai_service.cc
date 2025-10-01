#include "cli/service/gemini_ai_service.h"

#include <cstdlib>

#include "absl/strings/str_cat.h"

#ifdef YAZE_WITH_JSON
#include "incl/httplib.h"
#include "third_party/json/src/json.hpp"
#endif

namespace yaze {
namespace cli {

GeminiAIService::GeminiAIService(const std::string& api_key) : api_key_(api_key) {}

absl::StatusOr<std::vector<std::string>> GeminiAIService::GetCommands(
    const std::string& prompt) {
#ifndef YAZE_WITH_JSON
  return absl::UnimplementedError(
      "Gemini AI service requires JSON support. Build with -DYAZE_WITH_JSON=ON");
#else
  if (api_key_.empty()) {
    return absl::FailedPreconditionError("GEMINI_API_KEY not set.");
  }

  httplib::Client cli("https://generativelanguage.googleapis.com");
  nlohmann::json request_body = {
      {"contents",
       {{"parts",
         {{"text",
           "You are an expert ROM hacker for The Legend of Zelda: A Link to the Past. "
           "Your task is to generate a sequence of `z3ed` CLI commands to achieve the user's request. "
           "Respond only with a JSON array of strings, where each string is a `z3ed` command. "
           "Do not include any other text or explanation. "
           "Available commands: "
           "palette export --group <group> --id <id> --to <file>, "
           "palette import --group <group> --id <id> --from <file>, "
           "palette set-color --file <file> --index <index> --color <hex_color>, "
           "overworld set-tile --map <map_id> --x <x> --y <y> --tile <tile_id>. "
           "User request: " + prompt}}}}}
  };

  httplib::Headers headers = {
      {"Content-Type", "application/json"},
      {"x-goog-api-key", api_key_},
  };

  auto res = cli.Post("/v1beta/models/gemini-pro:generateContent", headers, request_body.dump(), "application/json");

  if (!res) {
    return absl::InternalError("Failed to connect to Gemini API.");
  }

  if (res->status != 200) {
    return absl::InternalError(absl::StrCat("Gemini API error: ", res->status, " ", res->body));
  }

  nlohmann::json response_json = nlohmann::json::parse(res->body);
  std::vector<std::string> commands;

  try {
    for (const auto& candidate : response_json["candidates"]) {
      for (const auto& part : candidate["content"]["parts"]) {
        std::string text_content = part["text"];
        // Assuming the AI returns a JSON array of strings directly in the text content
        // This might need more robust parsing depending on actual AI output format
        nlohmann::json commands_array = nlohmann::json::parse(text_content);
        if (commands_array.is_array()) {
          for (const auto& cmd : commands_array) {
            if (cmd.is_string()) {
              commands.push_back(cmd.get<std::string>());
            }
          }
        }
      }
    }
  } catch (const nlohmann::json::exception& e) {
    return absl::InternalError(absl::StrCat("Failed to parse Gemini API response: ", e.what()));
  }

  return commands;
#endif
}

}  // namespace cli
}  // namespace yaze
