#include "cli/service/ai/local_gemini_cli_service.h"

#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"
#include "cli/service/ai/subprocess_runner.h"
#include "nlohmann/json.hpp"

namespace yaze {
namespace cli {

LocalGeminiCliService::LocalGeminiCliService(const std::string& model)
    : model_(model) {}

void LocalGeminiCliService::SetRomContext(Rom* rom) {
#ifdef YAZE_AI_RUNTIME_AVAILABLE
  prompt_builder_.SetRom(rom);
#else
  (void)rom;
#endif
}

absl::StatusOr<AgentResponse> LocalGeminiCliService::ExecuteGeminiCli(
    const std::string& prompt) {
  auto executable_or = ResolveCliBinary("gemini");
  if (!executable_or.ok()) {
    return executable_or.status();
  }

  SubprocessOptions options;
  options.executable = *executable_or;
  options.args = {"-p",  prompt, "--output-format", "json", "--approval-mode",
                  "plan"};
  if (!model_.empty()) {
    options.args.push_back("--model");
    options.args.push_back(model_);
  }

  auto exec_or = RunSubprocess(options);
  if (!exec_or.ok()) {
    return exec_or.status();
  }

  const SubprocessResult& exec_result = *exec_or;
  if (exec_result.exit_code != 0) {
    const std::string& details = exec_result.stderr_output.empty()
                                     ? exec_result.stdout_output
                                     : exec_result.stderr_output;
    return absl::InternalError(absl::StrCat("gemini-cli exited with status ",
                                            exec_result.exit_code,
                                            ". Output: ", details));
  }

  const std::string& result = exec_result.stdout_output;
  if (result.empty()) {
    return absl::InternalError("Empty response from gemini-cli");
  }

  try {
    // Find the start of JSON object (skip any preamble logs)
    size_t json_start = result.find('{');
    if (json_start == std::string::npos) {
      return absl::InternalError("Invalid JSON response from gemini-cli: " +
                                 result);
    }
    std::string json_str = result.substr(json_start);

    auto json = nlohmann::json::parse(json_str);
    AgentResponse response;

    if (json.contains("response")) {
      response.text_response = json["response"].get<std::string>();
    } else {
      return absl::InternalError("JSON response missing 'response' field");
    }

    response.provider = "gemini-cli";
    response.model = model_;

    // Parse response for commands (basic heuristic since we don't have the structured parser here yet)
    // Same logic as ParseGeminiResponse in GeminiAIService
    std::string text_content = response.text_response;

    // Strip markdown code blocks
    if (absl::StartsWith(text_content, "```json")) {
      text_content = text_content.substr(7);
    } else if (absl::StartsWith(text_content, "```")) {
      text_content = text_content.substr(3);
    }
    if (absl::EndsWith(text_content, "```")) {
      text_content = text_content.substr(0, text_content.length() - 3);
    }

    // Try to parse inner JSON if the LLM output JSON
    auto parsed_inner = nlohmann::json::parse(text_content, nullptr, false);
    if (!parsed_inner.is_discarded()) {
      if (parsed_inner.contains("text_response") &&
          parsed_inner["text_response"].is_string()) {
        response.text_response =
            parsed_inner["text_response"].get<std::string>();
      }
      if (parsed_inner.contains("commands") &&
          parsed_inner["commands"].is_array()) {
        for (const auto& cmd : parsed_inner["commands"]) {
          if (cmd.is_string()) {
            std::string command = cmd.get<std::string>();
            if (absl::StartsWith(command, "z3ed ")) {
              command = command.substr(5);
            }
            response.commands.push_back(command);
          }
        }
      }
    } else {
      // Fallback: Line scanning
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
          response.commands.push_back(trimmed);
        }
      }
    }

    return response;

  } catch (const std::exception& e) {
    return absl::InternalError(absl::StrCat("JSON parse error: ", e.what()));
  }
}

absl::StatusOr<AgentResponse> LocalGeminiCliService::GenerateResponse(
    const std::string& prompt) {
  return ExecuteGeminiCli(prompt);
}

absl::StatusOr<AgentResponse> LocalGeminiCliService::GenerateResponse(
    const std::vector<agent::ChatMessage>& history) {
#ifdef YAZE_AI_RUNTIME_AVAILABLE
  std::string full_prompt = prompt_builder_.BuildPromptFromHistory(history);
  return ExecuteGeminiCli(full_prompt);
#else
  return absl::UnimplementedError("AI Runtime disabled");
#endif
}

}  // namespace cli
}  // namespace yaze
