#include "cli/handlers/agent/simple_chat_command.h"

#include <optional>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "cli/service/agent/simple_chat_session.h"

ABSL_DECLARE_FLAG(bool, quiet);

namespace yaze {
namespace cli {
namespace handlers {

absl::Status SimpleChatCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  agent::SimpleChatSession session;
  session.SetRomContext(rom);

  agent::AgentConfig config;
  config.verbose = parser.HasFlag("verbose");
  config.enable_vim_mode = parser.HasFlag("vim");

  // Determine desired output format
  std::optional<std::string> format_arg = parser.GetString("format");
  if (parser.HasFlag("json")) format_arg = "json";
  if (parser.HasFlag("markdown") || parser.HasFlag("md"))
    format_arg = "markdown";
  if (parser.HasFlag("compact") || parser.HasFlag("raw"))
    format_arg = "compact";

  auto select_format =
      [](absl::string_view value) -> std::optional<agent::AgentOutputFormat> {
    std::string normalized = absl::AsciiStrToLower(value);
    if (normalized == "json") return agent::AgentOutputFormat::kJson;
    if (normalized == "markdown" || normalized == "md")
      return agent::AgentOutputFormat::kMarkdown;
    if (normalized == "compact" || normalized == "raw")
      return agent::AgentOutputFormat::kCompact;
    if (normalized == "text" || normalized == "friendly" ||
        normalized == "pretty") {
      return agent::AgentOutputFormat::kFriendly;
    }
    return std::nullopt;
  };

  if (format_arg.has_value()) {
    if (auto output_format = select_format(*format_arg);
        output_format.has_value()) {
      config.output_format = *output_format;
    } else {
      return absl::InvalidArgumentError(
          absl::StrCat("Unsupported chat format: ", *format_arg,
                       ". Supported formats: text, markdown, json, compact"));
    }
  } else if (absl::GetFlag(FLAGS_quiet)) {
    config.output_format = agent::AgentOutputFormat::kCompact;
  }

  std::optional<std::string> prompt = parser.GetString("prompt");
  auto positional = parser.GetPositional();
  for (const auto& token : positional) {
    if (token == "-v") {
      config.verbose = true;
      continue;
    }
    if (!prompt.has_value() && !token.empty() && token.front() != '-') {
      prompt = token;
    }
  }

  session.SetConfig(config);

  if (auto batch = parser.GetString("file")) {
    formatter.AddField("mode", "batch");
    formatter.AddField("file", *batch);
    auto status = session.RunBatch(*batch);
    if (status.ok()) {
      formatter.AddField("status", "completed");
    }
    return status;
  }

  if (prompt.has_value()) {
    formatter.AddField("mode", "single");
    formatter.AddField("prompt", *prompt);

    std::string response;
    auto status = session.SendAndWaitForResponse(*prompt, &response);
    if (!status.ok()) {
      return status;
    }
    formatter.AddField("response", response);
    return absl::OkStatus();
  }

  formatter.AddField("mode", "interactive");
  auto status = session.RunInteractive();
  if (status.ok()) {
    formatter.AddField("status", "completed");
  }
  return status;
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
