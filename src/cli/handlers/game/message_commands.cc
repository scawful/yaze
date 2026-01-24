#include "cli/handlers/game/message_commands.h"

#include "absl/strings/ascii.h"
#include "absl/strings/str_format.h"

#include "app/editor/message/message_data.h"

namespace yaze {
namespace cli {
namespace handlers {

absl::Status MessageListCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto limit = parser.GetInt("limit").value_or(50);
  if (limit < 0) {
    limit = 0;
  }

  auto messages =
      editor::ReadAllTextData(const_cast<uint8_t*>(rom->data()),
                              editor::kTextData);
  if (limit > static_cast<int>(messages.size())) {
    limit = static_cast<int>(messages.size());
  }

  formatter.BeginObject("Message List");
  formatter.AddField("limit", limit);
  formatter.AddField("total_messages", static_cast<int>(messages.size()));
  formatter.AddField("status", "success");

  formatter.BeginArray("messages");
  for (int i = 0; i < limit; ++i) {
    const auto& msg = messages[i];
    formatter.BeginObject();
    formatter.AddField("id", msg.ID);
    formatter.AddHexField("address", msg.Address, 6);
    formatter.AddField("text", msg.ContentsParsed);
    formatter.AddField("length", static_cast<int>(msg.Data.size()));
    formatter.EndObject();
  }
  formatter.EndArray();
  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status MessageReadCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto message_id_or = parser.GetInt("id");
  if (!message_id_or.ok()) {
    return message_id_or.status();
  }
  int message_id = message_id_or.value();

  auto messages =
      editor::ReadAllTextData(const_cast<uint8_t*>(rom->data()),
                              editor::kTextData);
  if (message_id < 0 || message_id >= static_cast<int>(messages.size())) {
    return absl::NotFoundError(absl::StrFormat(
        "Message ID %d not found (max: %d)", message_id,
        static_cast<int>(messages.size()) - 1));
  }

  const auto& msg = messages[message_id];

  formatter.BeginObject("Message");
  formatter.AddField("id", msg.ID);
  formatter.AddHexField("address", msg.Address, 6);
  formatter.AddField("text", msg.ContentsParsed);
  formatter.AddField("length", static_cast<int>(msg.Data.size()));
  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status MessageSearchCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto query = parser.GetString("query").value();
  auto limit = parser.GetInt("limit").value_or(10);
  if (limit < 0) {
    limit = 0;
  }

  auto messages =
      editor::ReadAllTextData(const_cast<uint8_t*>(rom->data()),
                              editor::kTextData);

  formatter.BeginObject("Message Search Results");
  formatter.AddField("query", query);
  formatter.AddField("limit", limit);
  formatter.AddField("status", "success");

  std::string lowered_query = absl::AsciiStrToLower(query);
  std::vector<const editor::MessageData*> matches;
  for (const auto& msg : messages) {
    std::string lowered_text = absl::AsciiStrToLower(msg.ContentsParsed);
    if (lowered_text.find(lowered_query) == std::string::npos) {
      continue;
    }
    matches.push_back(&msg);
  }

  formatter.AddField("matches_found", static_cast<int>(matches.size()));
  formatter.BeginArray("matches");
  int match_count = 0;
  for (const auto* msg : matches) {
    if (limit > 0 && match_count >= limit) {
      break;
    }
    formatter.BeginObject();
    formatter.AddField("id", msg->ID);
    formatter.AddHexField("address", msg->Address, 6);
    formatter.AddField("text", msg->ContentsParsed);
    formatter.EndObject();
    match_count++;
  }
  formatter.EndArray();
  formatter.EndObject();

  return absl::OkStatus();
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
