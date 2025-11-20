#include "cli/handlers/tools/resource_commands.h"

#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "cli/service/resources/resource_context_builder.h"
#include "util/macro.h"

namespace yaze {
namespace cli {
namespace handlers {

absl::Status ResourceListCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto type = parser.GetString("type").value();

  ResourceContextBuilder builder(rom);
  ASSIGN_OR_RETURN(auto labels, builder.GetLabels(type));

  formatter.BeginObject(
      absl::StrFormat("%s Labels", absl::AsciiStrToUpper(type)));
  for (const auto& [key, value] : labels) {
    formatter.AddField(key, value);
  }
  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status ResourceSearchCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto query = parser.GetString("query").value();
  auto type = parser.GetString("type").value_or("all");

  ResourceContextBuilder builder(rom);

  std::vector<std::string> categories = {
      "overworld", "dungeon", "entrance", "room", "sprite", "palette", "item"};
  if (type != "all") {
    categories = {type};
  }

  formatter.BeginObject("Resource Search Results");
  formatter.AddField("query", query);
  formatter.AddField("search_type", type);

  int total_matches = 0;
  formatter.BeginArray("matches");

  for (const auto& category : categories) {
    auto labels_or = builder.GetLabels(category);
    if (!labels_or.ok()) continue;

    auto labels = labels_or.value();
    for (const auto& [key, value] : labels) {
      if (absl::StrContains(absl::AsciiStrToLower(value),
                            absl::AsciiStrToLower(query))) {
        formatter.AddArrayItem(
            absl::StrFormat("%s:%s = %s", category, key, value));
        total_matches++;
      }
    }
  }

  formatter.EndArray();
  formatter.AddField("total_matches", total_matches);
  formatter.EndObject();

  return absl::OkStatus();
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
