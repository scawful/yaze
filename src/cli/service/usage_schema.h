#ifndef YAZE_CLI_SERVICE_USAGE_SCHEMA_H_
#define YAZE_CLI_SERVICE_USAGE_SCHEMA_H_

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "nlohmann/json.hpp"

namespace yaze::cli {

// One argument recovered from a command usage string such as
// "dungeon-list-sprites --room <hex> [--format <json|text>]".
struct UsageParameter {
  std::string name;  // Flag name without dashes, or a positional placeholder.
  bool positional = false;
  bool required = false;
  bool takes_value = false;
  std::string value_hint;            // "hex" for "--room <hex>".
  std::vector<std::string> choices;  // {"json", "text"} for "<json|text>".

  bool operator==(const UsageParameter&) const = default;
};

// Parses the conventional usage grammar: `[...]` marks optional groups,
// `a | b` marks alternatives, `--flag <hint>` / `--flag=<hint>` / `--flag
// literal` take values, and bare `--flag` is a boolean switch. The leading
// command name is skipped. Unrecognized tokens are ignored, so free-form
// usage text degrades to fewer parameters rather than an error.
std::vector<UsageParameter> ParseUsageParameters(absl::string_view usage);

// JSON Schema object compatible with function-calling `parameters`. Property
// names are the literal flag names so a tool call maps back to `--name=value`.
nlohmann::ordered_json UsageParametersToJsonSchema(
    const std::vector<UsageParameter>& parameters);

}  // namespace yaze::cli

#endif  // YAZE_CLI_SERVICE_USAGE_SCHEMA_H_
