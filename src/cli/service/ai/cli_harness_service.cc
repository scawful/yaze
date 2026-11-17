#include "cli/service/ai/cli_harness_service.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"
#include "cli/service/ai/subprocess_runner.h"

#ifdef YAZE_WITH_JSON
#include "nlohmann/json.hpp"
#endif

namespace yaze::cli {

namespace {

std::string StripAnsiAndInvisible(absl::string_view input) {
  std::string out;
  out.reserve(input.size());
  for (size_t i = 0; i < input.size();) {
    const unsigned char ch = static_cast<unsigned char>(input[i]);
    if (ch == 0x1B && i + 1 < input.size() && input[i + 1] == '[') {
      i += 2;
      while (i < input.size()) {
        const unsigned char c = static_cast<unsigned char>(input[i]);
        ++i;
        if (c >= 0x40 && c <= 0x7E) {
          break;
        }
      }
      continue;
    }
    // Strip UTF-8 zero-width space/joiners (U+200B..U+200D) and BOM (U+FEFF).
    if (i + 2 < input.size() && ch == 0xE2 &&
        static_cast<unsigned char>(input[i + 1]) == 0x80 &&
        (static_cast<unsigned char>(input[i + 2]) == 0x8B ||
         static_cast<unsigned char>(input[i + 2]) == 0x8C ||
         static_cast<unsigned char>(input[i + 2]) == 0x8D)) {
      i += 3;
      continue;
    }
    if (i + 2 < input.size() && ch == 0xEF &&
        static_cast<unsigned char>(input[i + 1]) == 0xBB &&
        static_cast<unsigned char>(input[i + 2]) == 0xBF) {
      i += 3;
      continue;
    }
    if (ch == '\r') {
      ++i;
      continue;
    }
    out.push_back(static_cast<char>(ch));
    ++i;
  }
  return out;
}

bool IsValidModelToken(absl::string_view token) {
  if (token.empty()) {
    return false;
  }
  for (unsigned char c : token) {
    if (std::isalnum(c) == 0 && c != '-' && c != '_' && c != '.' && c != ':' &&
        c != '/') {
      return false;
    }
  }
  return true;
}

void ExtractInnerCommandsAndText(AgentResponse* response) {
  if (response == nullptr) {
    return;
  }
  std::string text_content =
      std::string(absl::StripAsciiWhitespace(response->text_response));

  if (absl::StartsWith(text_content, "```json")) {
    text_content = text_content.substr(7);
  } else if (absl::StartsWith(text_content, "```")) {
    text_content = text_content.substr(3);
  }
  if (absl::EndsWith(text_content, "```")) {
    text_content = text_content.substr(0, text_content.length() - 3);
  }
  text_content = std::string(absl::StripAsciiWhitespace(text_content));

#ifdef YAZE_WITH_JSON
  auto parsed_inner = nlohmann::json::parse(text_content, nullptr, false);
  if (!parsed_inner.is_discarded() && parsed_inner.is_object()) {
    if (parsed_inner.contains("text_response") &&
        parsed_inner["text_response"].is_string()) {
      response->text_response =
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
          response->commands.push_back(std::move(command));
        }
      }
    }
    return;
  }
#endif

  for (absl::string_view line : absl::StrSplit(text_content, '\n')) {
    std::string trimmed = std::string(absl::StripAsciiWhitespace(line));
    if (!trimmed.empty() && (absl::StartsWith(trimmed, "z3ed ") ||
                             absl::StartsWith(trimmed, "palette ") ||
                             absl::StartsWith(trimmed, "overworld ") ||
                             absl::StartsWith(trimmed, "sprite ") ||
                             absl::StartsWith(trimmed, "dungeon "))) {
      if (absl::StartsWith(trimmed, "z3ed ")) {
        trimmed = trimmed.substr(5);
      }
      response->commands.push_back(std::move(trimmed));
    }
  }
}

#ifdef YAZE_WITH_JSON
void RecordUsageParameters(const nlohmann::json& usage,
                           AgentResponse* response) {
  if (!usage.is_object() || response == nullptr) {
    return;
  }
  for (const auto& [key, value] : usage.items()) {
    if (value.is_number_integer()) {
      response->parameters[key] = absl::StrCat(value.get<int64_t>());
    } else if (value.is_number_unsigned()) {
      response->parameters[key] = absl::StrCat(value.get<uint64_t>());
    } else if (value.is_number_float()) {
      response->parameters[key] = absl::StrCat(value.get<double>());
    } else if (value.is_string()) {
      response->parameters[key] = value.get<std::string>();
    }
  }
}

std::string ExtractErrorString(const nlohmann::json& obj) {
  if (obj.contains("error")) {
    const auto& err = obj["error"];
    if (err.is_string()) {
      return err.get<std::string>();
    }
    if (err.is_object()) {
      if (err.contains("message") && err["message"].is_string()) {
        return err["message"].get<std::string>();
      }
      return err.dump();
    }
  }
  if (obj.contains("message") && obj["message"].is_string()) {
    return obj["message"].get<std::string>();
  }
  return "";
}
#endif  // YAZE_WITH_JSON

}  // namespace

std::string NormalizeCliHarnessProviderId(absl::string_view provider) {
  const std::string lower =
      absl::AsciiStrToLower(absl::StripAsciiWhitespace(provider));
  if (lower == kProviderClaudeCode || lower == kProviderClaudeCli) {
    return kProviderClaudeCode;
  }
  if (lower == kProviderCodex || lower == kProviderCodexCli) {
    return kProviderCodex;
  }
  if (lower == kProviderCursorAgent || lower == kProviderCursor) {
    return kProviderCursorAgent;
  }
  if (lower == kProviderAgy || lower == kProviderAntigravity) {
    return kProviderAgy;
  }
  if (lower == kProviderGeminiCli || lower == kProviderLocalGemini) {
    return kProviderGeminiCli;
  }
  return lower;
}

absl::StatusOr<CliHarnessProfile> GetCliHarnessProfile(
    absl::string_view provider) {
  const std::string canonical = NormalizeCliHarnessProviderId(provider);
  if (canonical == kProviderClaudeCode) {
    return CliHarnessProfile{
        .provider_id = kProviderClaudeCode,
        .display_name = "Claude Code",
        .binary_name = "claude",
        .default_model = "claude-sonnet-5-5",
        .read_only_summary = "--permission-mode plan",
        .supports_live_model_list = false,
    };
  }
  if (canonical == kProviderCodex) {
    return CliHarnessProfile{
        .provider_id = kProviderCodex,
        .display_name = "Codex",
        .binary_name = "codex",
        .default_model = "gpt-5.2",
        .read_only_summary = "--sandbox read-only",
        .supports_live_model_list = false,
    };
  }
  if (canonical == kProviderCursorAgent) {
    return CliHarnessProfile{
        .provider_id = kProviderCursorAgent,
        .display_name = "Cursor Agent",
        .binary_name = "cursor-agent",
        .default_model = "auto",
        .read_only_summary = "--mode plan",
        .supports_live_model_list = true,
    };
  }
  if (canonical == kProviderAgy) {
    return CliHarnessProfile{
        .provider_id = kProviderAgy,
        .display_name = "Antigravity (agy)",
        .binary_name = "agy",
        .default_model = "gemini-4-argon-low",
        .read_only_summary = "--mode plan",
        .supports_live_model_list = true,
    };
  }
  if (canonical == kProviderGeminiCli) {
    return CliHarnessProfile{
        .provider_id = kProviderGeminiCli,
        .display_name = "Gemini CLI",
        .binary_name = "gemini",
        .default_model = "gemini-2.5-flash",
        .read_only_summary = "--approval-mode plan",
        .supports_live_model_list = false,
    };
  }
  return absl::InvalidArgumentError(
      absl::StrCat("Unknown CLI harness provider: ", provider));
}

absl::StatusOr<std::vector<std::string>> BuildCliHarnessArgv(
    absl::string_view provider, const std::string& prompt,
    const std::string& model, int timeout_seconds) {
  auto profile_or = GetCliHarnessProfile(provider);
  if (!profile_or.ok()) {
    return profile_or.status();
  }
  const std::string canonical = profile_or->provider_id;
  const std::string trimmed_model =
      std::string(absl::StripAsciiWhitespace(model));
  const int clamped_timeout = std::clamp(timeout_seconds, 5, 3600);

  if (canonical == kProviderClaudeCode) {
    std::vector<std::string> args = {
        "-p", prompt, "--output-format", "json", "--permission-mode", "plan"};
    if (!trimmed_model.empty()) {
      args.push_back("--model");
      args.push_back(trimmed_model);
    }
    return args;
  }

  if (canonical == kProviderCodex) {
    std::vector<std::string> args = {"exec", "--json", "--sandbox",
                                     "read-only"};
    if (!trimmed_model.empty()) {
      args.push_back("-m");
      args.push_back(trimmed_model);
    }
    args.push_back("--");
    args.push_back(prompt);
    return args;
  }

  if (canonical == kProviderCursorAgent) {
    std::vector<std::string> args = {
        "-p", prompt, "--mode", "plan", "--output-format", "json"};
    if (!trimmed_model.empty()) {
      args.push_back("--model");
      args.push_back(trimmed_model);
    }
    return args;
  }

  if (canonical == kProviderAgy) {
    std::vector<std::string> args = {"-p",
                                     prompt,
                                     "--mode",
                                     "plan",
                                     "--output-format",
                                     "json",
                                     "--print-timeout",
                                     absl::StrCat(clamped_timeout, "s")};
    if (!trimmed_model.empty()) {
      args.push_back("--model");
      args.push_back(trimmed_model);
    }
    return args;
  }

  if (canonical == kProviderGeminiCli) {
    std::vector<std::string> args = {
        "-p", prompt, "--output-format", "json", "--approval-mode", "plan"};
    if (!trimmed_model.empty()) {
      args.push_back("--model");
      args.push_back(trimmed_model);
    }
    return args;
  }

  return absl::InvalidArgumentError(
      absl::StrCat("Unhandled CLI harness provider: ", canonical));
}

absl::StatusOr<AgentResponse> ParseCliHarnessOutput(
    absl::string_view provider, absl::string_view model,
    absl::string_view stdout_text, absl::string_view stderr_text) {
  const std::string canonical = NormalizeCliHarnessProviderId(provider);
  const std::string clean_stdout = std::string(
      absl::StripAsciiWhitespace(StripAnsiAndInvisible(stdout_text)));
  const std::string clean_stderr = std::string(
      absl::StripAsciiWhitespace(StripAnsiAndInvisible(stderr_text)));

  if (clean_stdout.empty()) {
    return absl::InternalError(
        clean_stderr.empty() ? absl::StrCat("Empty response from ", canonical)
                             : absl::StrCat("Empty response from ", canonical,
                                            ": ", clean_stderr));
  }

  AgentResponse response;
  response.provider = canonical;
  response.model = std::string(model);

#ifdef YAZE_WITH_JSON
  if (canonical == kProviderCodex) {
    bool parsed_any_json_line = false;
    std::vector<std::string> agent_messages;
    for (absl::string_view raw_line : absl::StrSplit(clean_stdout, '\n')) {
      const absl::string_view line = absl::StripAsciiWhitespace(raw_line);
      if (line.empty() || line.front() != '{') {
        continue;
      }
      auto item = nlohmann::json::parse(line, nullptr, false);
      if (item.is_discarded() || !item.is_object()) {
        continue;
      }
      parsed_any_json_line = true;
      const std::string type = item.value("type", "");
      if (type == "error" || type == "turn.failed") {
        const std::string err_msg = ExtractErrorString(item);
        return absl::InternalError(absl::StrCat(
            canonical, " error: ", err_msg.empty() ? item.dump() : err_msg));
      }
      if (type == "item.completed" && item.contains("item") &&
          item["item"].is_object()) {
        const auto& payload = item["item"];
        const std::string item_type = payload.value("type", "");
        if (item_type == "agent_message") {
          if (payload.contains("text") && payload["text"].is_string()) {
            agent_messages.push_back(payload["text"].get<std::string>());
          } else if (payload.contains("content") &&
                     payload["content"].is_array()) {
            for (const auto& block : payload["content"]) {
              if (block.is_object() && block.contains("text") &&
                  block["text"].is_string()) {
                agent_messages.push_back(block["text"].get<std::string>());
              }
            }
          }
        }
      } else if (type == "turn.completed" && item.contains("usage")) {
        RecordUsageParameters(item["usage"], &response);
      } else if (item.contains("response") && item["response"].is_string()) {
        agent_messages.push_back(item["response"].get<std::string>());
      } else if (item.contains("result") && item["result"].is_string()) {
        agent_messages.push_back(item["result"].get<std::string>());
      }
    }

    if (parsed_any_json_line) {
      if (agent_messages.empty()) {
        return absl::InternalError(
            "Codex JSONL stream completed without an agent_message");
      }
      response.text_response = agent_messages.front();
      for (size_t i = 1; i < agent_messages.size(); ++i) {
        response.text_response += "\n\n" + agent_messages[i];
      }
      ExtractInnerCommandsAndText(&response);
      return response;
    }
  }

  const size_t json_start = clean_stdout.find('{');
  if (json_start != std::string::npos) {
    auto json =
        nlohmann::json::parse(clean_stdout.substr(json_start), nullptr, false);
    if (!json.is_discarded() && json.is_object()) {
      if (json.contains("is_error") && json["is_error"].is_boolean() &&
          json["is_error"].get<bool>()) {
        const std::string err =
            json.contains("result") && json["result"].is_string()
                ? json["result"].get<std::string>()
                : ExtractErrorString(json);
        return absl::InternalError(absl::StrCat(
            canonical, " returned error: ", err.empty() ? clean_stdout : err));
      }
      if (json.contains("status") && json["status"].is_string()) {
        const std::string status_upper =
            absl::AsciiStrToUpper(json["status"].get<std::string>());
        if (status_upper != "SUCCESS" && status_upper != "OK") {
          const std::string err = ExtractErrorString(json);
          return absl::InternalError(
              absl::StrCat(canonical, " status ", status_upper,
                           err.empty() ? "" : absl::StrCat(": ", err)));
        }
      }

      bool found_text = false;
      for (const char* key : {"response", "result", "text", "output"}) {
        if (json.contains(key) && json[key].is_string()) {
          response.text_response = json[key].get<std::string>();
          found_text = true;
          break;
        }
      }
      if (!found_text && json.contains("error")) {
        const std::string err = ExtractErrorString(json);
        return absl::InternalError(absl::StrCat(
            canonical, " error: ", err.empty() ? clean_stdout : err));
      }
      if (found_text) {
        if (json.contains("duration_seconds") &&
            json["duration_seconds"].is_number()) {
          response.latency_seconds = json["duration_seconds"].get<double>();
        } else if (json.contains("duration_ms") &&
                   json["duration_ms"].is_number()) {
          response.latency_seconds = json["duration_ms"].get<double>() / 1000.0;
        }
        if (json.contains("usage")) {
          RecordUsageParameters(json["usage"], &response);
        } else if (json.contains("stats") && json["stats"].is_object()) {
          RecordUsageParameters(json["stats"], &response);
        }
        ExtractInnerCommandsAndText(&response);
        return response;
      }
    }
  }
#endif  // YAZE_WITH_JSON

  // JSON parse failed or non-JSON stdout: fall back to raw stdout text.
  response.text_response = clean_stdout;
  response.parameters["raw_output"] = "true";
  ExtractInnerCommandsAndText(&response);
  return response;
}

std::vector<ModelInfo> ParseAgyModelsOutput(absl::string_view output) {
  std::vector<ModelInfo> models;
  const std::string sanitized = StripAnsiAndInvisible(output);
  for (absl::string_view raw_line : absl::StrSplit(sanitized, '\n')) {
    const absl::string_view line = absl::StripAsciiWhitespace(raw_line);
    const size_t tab = line.find('\t');
    if (tab == absl::string_view::npos) {
      continue;
    }
    const absl::string_view id =
        absl::StripAsciiWhitespace(line.substr(0, tab));
    const absl::string_view display =
        absl::StripAsciiWhitespace(line.substr(tab + 1));
    if (!IsValidModelToken(id)) {
      continue;
    }
    ModelInfo info;
    info.name = std::string(id);
    info.display_name =
        display.empty() ? std::string(id) : std::string(display);
    info.provider = kProviderAgy;
    info.family = "agy-cli";
    info.description = "Antigravity CLI harness model";
    info.is_local = false;
    models.push_back(std::move(info));
  }
  return models;
}

std::vector<ModelInfo> ParseCursorAgentModelsOutput(absl::string_view output) {
  std::vector<ModelInfo> models;
  const std::string sanitized = StripAnsiAndInvisible(output);
  for (absl::string_view raw_line : absl::StrSplit(sanitized, '\n')) {
    const absl::string_view line = absl::StripAsciiWhitespace(raw_line);
    if (line.empty()) {
      continue;
    }
    const size_t sep = line.find(" - ");
    if (sep == absl::string_view::npos) {
      continue;
    }
    const absl::string_view id =
        absl::StripAsciiWhitespace(line.substr(0, sep));
    const absl::string_view display =
        absl::StripAsciiWhitespace(line.substr(sep + 3));
    if (!IsValidModelToken(id)) {
      continue;
    }
    ModelInfo info;
    info.name = std::string(id);
    info.display_name =
        display.empty() ? std::string(id) : std::string(display);
    info.provider = kProviderCursorAgent;
    info.family = "cursor-cli";
    info.description = "Cursor Agent CLI harness model";
    info.is_local = false;
    models.push_back(std::move(info));
  }
  return models;
}

std::vector<ModelInfo> GetFallbackCliModels(absl::string_view provider) {
  const std::string canonical = NormalizeCliHarnessProviderId(provider);
  auto make_list = [&](std::vector<std::pair<const char*, const char*>> entries,
                       const char* family) {
    std::vector<ModelInfo> out;
    out.reserve(entries.size());
    for (const auto& [id, display] : entries) {
      out.push_back({.name = id,
                     .display_name = display,
                     .provider = canonical,
                     .family = family,
                     .is_local = false});
    }
    return out;
  };

  if (canonical == kProviderClaudeCode) {
    return make_list({{"claude-sonnet-5-5", "Claude Sonnet 5.5"},
                      {"claude-opus-5-5", "Claude Opus 5.5"},
                      {"claude-haiku-5-5", "Claude Haiku 5.5"},
                      {"claude-fable-5-2", "Claude Fable 5.2"}},
                     "claude-cli");
  }
  if (canonical == kProviderCodex) {
    return make_list({{"gpt-5.2", "GPT-5.2"},
                      {"gpt-5.1", "GPT-5.1"},
                      {"gpt-5", "GPT-5"},
                      {"gpt-5-mini", "GPT-5 Mini"}},
                     "codex-cli");
  }
  if (canonical == kProviderCursorAgent) {
    return make_list(
        {{"auto", "Auto (default)"},
         {"composer-2.5", "Composer 2.5"},
         {"claude-opus-5-thinking-high", "Claude Opus 5 Thinking"},
         {"claude-sonnet-5-thinking-high", "Claude Sonnet 5 Thinking"},
         {"gpt-5.3-codex", "Codex 5.3"},
         {"gpt-5.2", "GPT-5.2"}},
        "cursor-cli");
  }
  if (canonical == kProviderAgy) {
    return make_list(
        {{"gemini-4-argon-low", "Gemini 4 Argon (Low)"},
         {"gemini-4-argon-medium", "Gemini 4 Argon (Medium)"},
         {"gemini-4-carbon-medium", "Gemini 4 Carbon (Medium)"},
         {"gemini-4-hydrogen-medium", "Gemini 4 Hydrogen (Medium)"},
         {"gemini-3.8-flash-medium", "Gemini 3.8 Flash (Medium)"},
         {"claude-sonnet-5-5-medium", "Claude Sonnet 5.5 (Medium)"},
         {"claude-opus-5-5-medium", "Claude Opus 5.5 (Medium)"}},
        "agy-cli");
  }
  if (canonical == kProviderGeminiCli) {
    return make_list({{"gemini-2.5-flash", "Gemini 2.5 Flash"},
                      {"gemini-2.5-pro", "Gemini 2.5 Pro"},
                      {"gemini-3.5-flash", "Gemini 3.5 Flash"},
                      {"gemini-2.5-flash-lite", "Gemini 2.5 Flash-Lite"}},
                     "gemini-cli");
  }
  return {};
}

CliHarnessService::CliHarnessService(CliHarnessConfig config)
    : config_(std::move(config)) {
  config_.provider = NormalizeCliHarnessProviderId(config_.provider);
  if (auto profile_or = GetCliHarnessProfile(config_.provider);
      profile_or.ok() && config_.model.empty()) {
    config_.model = profile_or->default_model;
  }
}

void CliHarnessService::SetRomContext(Rom* rom) {
#ifdef YAZE_AI_RUNTIME_AVAILABLE
  prompt_builder_.SetRom(rom);
#else
  (void)rom;
#endif
}

std::string CliHarnessService::GetProviderName() const {
  return config_.provider;
}

absl::Status CliHarnessService::CheckAvailability() {
  auto profile_or = GetCliHarnessProfile(config_.provider);
  if (!profile_or.ok()) {
    return profile_or.status();
  }
  return ResolveCliBinary(profile_or->binary_name, config_.binary_override)
      .status();
}

absl::StatusOr<std::vector<ModelInfo>>
CliHarnessService::ListAvailableModels() {
  auto profile_or = GetCliHarnessProfile(config_.provider);
  if (!profile_or.ok()) {
    return profile_or.status();
  }
  const CliHarnessProfile& profile = *profile_or;

  if (profile.supports_live_model_list) {
    auto binary_or =
        ResolveCliBinary(profile.binary_name, config_.binary_override);
    if (binary_or.ok()) {
      SubprocessOptions opts;
      opts.executable = *binary_or;
      opts.working_directory = config_.working_directory;
      opts.timeout = std::chrono::seconds(8);
      if (profile.provider_id == kProviderAgy) {
        opts.args = {"models"};
      } else if (profile.provider_id == kProviderCursorAgent) {
        opts.args = {"--list-models"};
      }
      auto run_or = RunSubprocess(opts);
      if (run_or.ok() && run_or->exit_code == 0) {
        std::vector<ModelInfo> parsed;
        if (profile.provider_id == kProviderAgy) {
          parsed = ParseAgyModelsOutput(run_or->stdout_output);
        } else if (profile.provider_id == kProviderCursorAgent) {
          parsed = ParseCursorAgentModelsOutput(run_or->stdout_output);
        }
        if (!parsed.empty()) {
          return parsed;
        }
      }
    }
  }

  return GetFallbackCliModels(profile.provider_id);
}

absl::StatusOr<AgentResponse> CliHarnessService::ExecuteHarness(
    const std::string& prompt) {
  auto profile_or = GetCliHarnessProfile(config_.provider);
  if (!profile_or.ok()) {
    return profile_or.status();
  }
  const CliHarnessProfile& profile = *profile_or;

  auto binary_or =
      ResolveCliBinary(profile.binary_name, config_.binary_override);
  if (!binary_or.ok()) {
    return binary_or.status();
  }

  const std::string effective_model =
      config_.model.empty() ? profile.default_model : config_.model;
  auto args_or = BuildCliHarnessArgv(profile.provider_id, prompt,
                                     effective_model, config_.timeout_seconds);
  if (!args_or.ok()) {
    return args_or.status();
  }

  SubprocessOptions opts;
  opts.executable = *binary_or;
  opts.args = std::move(*args_or);
  opts.working_directory = config_.working_directory;
  opts.timeout =
      std::chrono::seconds(std::clamp(config_.timeout_seconds, 5, 3600));
  opts.cancel_requested = config_.cancel_requested;

  auto run_or = RunSubprocess(opts);
  if (!run_or.ok()) {
    return run_or.status();
  }
  const SubprocessResult& run_result = *run_or;
  if (run_result.exit_code != 0) {
    const std::string details =
        run_result.stderr_output.empty()
            ? StripAnsiAndInvisible(run_result.stdout_output)
            : StripAnsiAndInvisible(run_result.stderr_output);
    return absl::InternalError(absl::StrCat(
        profile.provider_id, " exited with status ", run_result.exit_code,
        details.empty() ? "" : absl::StrCat(": ", details)));
  }

  auto parsed_or =
      ParseCliHarnessOutput(profile.provider_id, effective_model,
                            run_result.stdout_output, run_result.stderr_output);
  if (parsed_or.ok() && parsed_or->latency_seconds <= 0.0) {
    parsed_or->latency_seconds = run_result.duration_seconds;
  }
  return parsed_or;
}

absl::StatusOr<AgentResponse> CliHarnessService::GenerateResponse(
    const std::string& prompt) {
  return ExecuteHarness(prompt);
}

absl::StatusOr<AgentResponse> CliHarnessService::GenerateResponse(
    const std::vector<agent::ChatMessage>& history) {
#ifdef YAZE_AI_RUNTIME_AVAILABLE
  std::string full_prompt = prompt_builder_.BuildPromptFromHistory(history);
  if (!config_.system_instruction.empty()) {
    full_prompt = absl::StrCat(config_.system_instruction, "\n\n", full_prompt);
  }
  return ExecuteHarness(full_prompt);
#else
  (void)history;
  return absl::UnimplementedError("AI Runtime disabled");
#endif
}

}  // namespace yaze::cli
