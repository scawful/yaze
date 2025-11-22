#include "cli/handlers/game/dialogue_commands.h"

#include "absl/strings/str_format.h"

namespace yaze {
namespace cli {
namespace handlers {

absl::Status DialogueListCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto limit = parser.GetInt("limit").value_or(10);

  formatter.BeginObject("Dialogue Messages");
  formatter.AddField("total_messages", 0);
  formatter.AddField("limit", limit);
  formatter.AddField("status", "not_implemented");
  formatter.AddField("message",
                     "Dialogue listing requires dialogue system integration");

  formatter.BeginArray("messages");
  formatter.EndArray();
  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status DialogueReadCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto message_id = parser.GetString("id").value();

  formatter.BeginObject("Dialogue Message");
  formatter.AddField("message_id", message_id);
  formatter.AddField("status", "not_implemented");
  formatter.AddField("message",
                     "Dialogue reading requires dialogue system integration");
  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status DialogueSearchCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto query = parser.GetString("query").value();
  auto limit = parser.GetInt("limit").value_or(10);

  formatter.BeginObject("Dialogue Search Results");
  formatter.AddField("query", query);
  formatter.AddField("limit", limit);
  formatter.AddField("matches_found", 0);
  formatter.AddField("status", "not_implemented");
  formatter.AddField("message",
                     "Dialogue search requires dialogue system integration");

  formatter.BeginArray("matches");
  formatter.EndArray();
  formatter.EndObject();

  return absl::OkStatus();
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
