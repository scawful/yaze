#ifndef YAZE_CLI_CLI_H
#define YAZE_CLI_CLI_H

#include <functional>
#include <iosfwd>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "cli/util/ansi_style.h"

namespace yaze::cli {

class CommandRegistry;

// Routes a z3ed command line to its CommandRegistry handler and renders the
// overview, category and command help.
class Cli {
 public:
  using AgentHandler =
      std::function<absl::Status(const std::vector<std::string>& args)>;

  struct Options {
    // Runs `z3ed agent ...`; empty when the build has no agent CLI.
    AgentHandler agent_handler;
    bool http_api_available = false;
    util::AnsiStyle style{false};
  };

  Cli(CommandRegistry& registry, Options options, std::ostream& out);

  // Runs `args`: the command name followed by its arguments.
  absl::Status Run(absl::Span<const std::string> args) const;

  void PrintOverview() const;
  // Prints help for the command line `words`, such as {"rom-info"},
  // {"dungeon"} (a category) or {"debug", "mem", "read"} (an alias).
  absl::Status PrintScopedHelp(absl::Span<const std::string> words) const;
  void PrintCategoryHelp(const std::string& category) const;
  void PrintCommandSummary() const;

 private:
  absl::Status RunAgent(absl::Span<const std::string> args) const;
  // `group` names an alias group such as "rom" or "debug".
  absl::Status RunAlias(absl::string_view group,
                        absl::Span<const std::string> args) const;
  void PrintAgentHelp() const;
  void PrintAliasHelp(absl::string_view group) const;

  CommandRegistry& registry_;
  Options options_;
  std::ostream& out_;
};

}  // namespace yaze::cli

#endif  // YAZE_CLI_CLI_H
