#include "cli/global_flags.h"

#include <cstddef>
#include <string>

#include "absl/algorithm/container.h"
#include "absl/flags/commandlineflag.h"
#include "absl/flags/reflection.h"
#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"

namespace yaze::cli {
namespace {

// absl flags z3ed accepts anywhere on the command line. Command-specific
// flags such as --format are not listed and reach the command untouched.
// A listed flag that is not linked into this build (the HTTP flags when the
// HTTP API is compiled out) is forwarded to the command like any other token.
constexpr absl::string_view kGlobalFlagNames[] = {
    "rom",
    "quiet",
    "sandbox",
    "ai_provider",
    "ai_model",
    "gemini_api_key",
    "anthropic_api_key",
    "openai_base_url",
    "ollama_host",
    "gui_server_address",
    "mesen_socket",
    "prompt_version",
    "use_function_calling",
    "http_port",
    "http_host",
};

struct ActionFlag {
  absl::string_view spelling;
  bool GlobalOptions::* field;
};

constexpr ActionFlag kActionFlags[] = {
    {"--version", &GlobalOptions::show_version},
    {"-v", &GlobalOptions::show_version},
    {"--list-commands", &GlobalOptions::list_commands},
    {"--list", &GlobalOptions::list_commands},
    {"--export-schemas", &GlobalOptions::export_schemas},
    {"--export_schemas", &GlobalOptions::export_schemas},
    {"--self-test", &GlobalOptions::self_test},
    {"--selftest", &GlobalOptions::self_test},
};

bool* FindActionFlag(absl::string_view token, GlobalOptions& options) {
  for (const ActionFlag& action : kActionFlags) {
    if (token == action.spelling) {
      return &(options.*action.field);
    }
  }
  return nullptr;
}

// A bare boolean flag may be followed by an explicit value, as in
// "--use_function_calling false"; any other next token is a positional.
bool IsBoolLiteral(absl::string_view token) {
  return token == "true" || token == "false" || token == "1" || token == "0";
}

// Applies args[index] if it is a global flag. Returns the number of tokens
// consumed, which is 0 when args[index] is not a global flag.
absl::StatusOr<size_t> ApplyGlobalFlag(absl::Span<const std::string> args,
                                       size_t index) {
  absl::string_view token = args[index];
  if (token == "-q") {
    token = "--quiet";
  }
  if (!absl::ConsumePrefix(&token, "--")) {
    return 0;
  }

  const size_t equals = token.find('=');
  const absl::string_view spelled_name = token.substr(0, equals);
  const std::string name = absl::StrReplaceAll(spelled_name, {{"-", "_"}});
  if (!absl::c_linear_search(kGlobalFlagNames, name)) {
    return 0;
  }
  absl::CommandLineFlag* flag = absl::FindCommandLineFlag(name);
  if (flag == nullptr) {
    return 0;
  }

  size_t consumed = 1;
  std::string value;
  if (equals != absl::string_view::npos) {
    value = std::string(token.substr(equals + 1));
  } else if (flag->IsOfType<bool>()) {
    value = "true";
    if (index + 1 < args.size() && IsBoolLiteral(args[index + 1])) {
      value = args[index + 1];
      consumed = 2;
    }
  } else if (index + 1 < args.size()) {
    value = args[index + 1];
    consumed = 2;
  } else {
    return absl::InvalidArgumentError(
        absl::StrCat("--", spelled_name, " flag requires a value"));
  }

  std::string error;
  if (!flag->ParseFrom(value, &error)) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid value for --", spelled_name, ": ", error));
  }
  return consumed;
}

}  // namespace

absl::StatusOr<GlobalOptions> ParseGlobalFlags(
    absl::Span<const std::string> args) {
  GlobalOptions options;
  options.positional.reserve(args.size());

  for (size_t i = 0; i < args.size(); ++i) {
    const std::string& token = args[i];

    if (token == "--") {
      options.positional.insert(options.positional.end(), args.begin() + i + 1,
                                args.end());
      break;
    }

    if (token == "--help" || token == "-h") {
      if (i + 1 < args.size() && !absl::StartsWith(args[i + 1], "-")) {
        options.help_target = args[++i];
      } else {
        options.show_help = true;
      }
      continue;
    }
    if (absl::StartsWith(token, "--help=")) {
      std::string target = token.substr(7);
      if (target.empty()) {
        options.show_help = true;
      } else {
        options.help_target = std::move(target);
      }
      continue;
    }

    if (bool* action = FindActionFlag(token, options)) {
      *action = true;
      continue;
    }

    if (token == "--tui" || token == "--interactive") {
      return absl::InvalidArgumentError(
          "--tui/--interactive was removed; use `z3ed help` for CLI "
          "workflows");
    }

    absl::StatusOr<size_t> consumed = ApplyGlobalFlag(args, i);
    if (!consumed.ok()) {
      return consumed.status();
    }
    if (*consumed > 0) {
      i += *consumed - 1;
      continue;
    }

    options.positional.push_back(token);
  }

  return options;
}

}  // namespace yaze::cli
