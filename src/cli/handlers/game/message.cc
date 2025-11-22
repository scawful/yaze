#include "cli/handlers/game/message.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "app/editor/message/message_data.h"
#include "app/rom.h"
#include "util/macro.h"

ABSL_DECLARE_FLAG(std::string, rom);

namespace yaze {
namespace cli {
namespace message {

namespace {

absl::StatusOr<Rom> LoadRomFromFlag() {
  std::string rom_path = absl::GetFlag(FLAGS_rom);
  if (rom_path.empty()) {
    return absl::FailedPreconditionError(
        "No ROM loaded. Use --rom=<path> to specify ROM file.");
  }

  Rom rom;
  auto status = rom.LoadFromFile(rom_path);
  if (!status.ok()) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Failed to load ROM from '%s': %s", rom_path, status.message()));
  }

  return rom;
}

std::vector<editor::MessageData> LoadMessages(Rom* rom) {
  // Fix: Cast away constness for ReadAllTextData, which expects uint8_t*
  return editor::ReadAllTextData(const_cast<uint8_t*>(rom->data()),
                                 editor::kTextData);
}

}  // namespace

absl::Status HandleMessageListCommand(const std::vector<std::string>& arg_vec,
                                      Rom* rom_context) {
  std::string format = "json";
  int start_id = 0;
  int end_id = -1;  // -1 means all

  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& token = arg_vec[i];
    if (token == "--format") {
      if (i + 1 >= arg_vec.size()) {
        return absl::InvalidArgumentError("--format requires a value.");
      }
      format = absl::AsciiStrToLower(arg_vec[++i]);
    } else if (absl::StartsWith(token, "--format=")) {
      format = absl::AsciiStrToLower(token.substr(9));
    } else if (token == "--range") {
      if (i + 1 >= arg_vec.size()) {
        return absl::InvalidArgumentError(
            "--range requires a value (start-end).");
      }
      std::string range = arg_vec[++i];
      size_t dash_pos = range.find('-');
      if (dash_pos == std::string::npos) {
        return absl::InvalidArgumentError(
            "--range format must be start-end (e.g. 0-100)");
      }
      if (!absl::SimpleAtoi(range.substr(0, dash_pos), &start_id) ||
          !absl::SimpleAtoi(range.substr(dash_pos + 1), &end_id)) {
        return absl::InvalidArgumentError("Invalid range format");
      }
    } else if (absl::StartsWith(token, "--range=")) {
      std::string range = token.substr(8);
      size_t dash_pos = range.find('-');
      if (dash_pos == std::string::npos) {
        return absl::InvalidArgumentError(
            "--range format must be start-end (e.g. 0-100)");
      }
      if (!absl::SimpleAtoi(range.substr(0, dash_pos), &start_id) ||
          !absl::SimpleAtoi(range.substr(dash_pos + 1), &end_id)) {
        return absl::InvalidArgumentError("Invalid range format");
      }
    }
  }

  if (format != "json" && format != "text") {
    return absl::InvalidArgumentError("--format must be either json or text");
  }

  Rom rom_storage;
  Rom* rom = nullptr;
  if (rom_context != nullptr && rom_context->is_loaded()) {
    rom = rom_context;
  } else {
    auto rom_or = LoadRomFromFlag();
    if (!rom_or.ok()) {
      return rom_or.status();
    }
    rom_storage = std::move(rom_or.value());
    rom = &rom_storage;
  }

  auto messages = LoadMessages(rom);

  if (end_id < 0) {
    end_id = static_cast<int>(messages.size()) - 1;
  }

  start_id =
      std::max(0, std::min(start_id, static_cast<int>(messages.size()) - 1));
  end_id = std::max(start_id,
                    std::min(end_id, static_cast<int>(messages.size()) - 1));

  if (format == "json") {
    std::cout << "{\n";
    std::cout << absl::StrFormat("  \"total_messages\": %zu,\n",
                                 messages.size());
    std::cout << absl::StrFormat("  \"range\": [%d, %d],\n", start_id, end_id);
    std::cout << "  \"messages\": [\n";

    bool first = true;
    for (int i = start_id; i <= end_id; ++i) {
      const auto& msg = messages[i];
      if (!first)
        std::cout << ",\n";
      std::cout << "    {\n";
      std::cout << absl::StrFormat("      \"id\": %d,\n", msg.ID);
      std::cout << absl::StrFormat("      \"address\": \"0x%06X\",\n",
                                   msg.Address);

      // Escape quotes in the text
      std::string escaped_text = msg.ContentsParsed;
      size_t pos = 0;
      while ((pos = escaped_text.find('"', pos)) != std::string::npos) {
        escaped_text.insert(pos, "\\");
        pos += 2;
      }
      std::cout << absl::StrFormat("      \"text\": \"%s\"\n", escaped_text);
      std::cout << "    }";
      first = false;
    }
    std::cout << "\n  ]\n";
    std::cout << "}\n";
  } else {
    std::cout << absl::StrFormat("📝 Messages %d-%d (Total: %zu)\n", start_id,
                                 end_id, messages.size());
    std::cout << std::string(60, '=') << "\n";
    for (int i = start_id; i <= end_id; ++i) {
      const auto& msg = messages[i];
      std::cout << absl::StrFormat("[%03d] @ 0x%06X\n", msg.ID, msg.Address);
      std::cout << "  " << msg.ContentsParsed << "\n";
      std::cout << std::string(60, '-') << "\n";
    }
  }

  return absl::OkStatus();
}

absl::Status HandleMessageReadCommand(const std::vector<std::string>& arg_vec,
                                      Rom* rom_context) {
  int message_id = -1;
  std::string format = "json";

  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& token = arg_vec[i];
    if (token == "--id") {
      if (i + 1 >= arg_vec.size()) {
        return absl::InvalidArgumentError("--id requires a value.");
      }
      if (!absl::SimpleAtoi(arg_vec[++i], &message_id)) {
        return absl::InvalidArgumentError("Invalid message ID format.");
      }
    } else if (absl::StartsWith(token, "--id=")) {
      if (!absl::SimpleAtoi(token.substr(5), &message_id)) {
        return absl::InvalidArgumentError("Invalid message ID format.");
      }
    } else if (token == "--format") {
      if (i + 1 >= arg_vec.size()) {
        return absl::InvalidArgumentError("--format requires a value.");
      }
      format = absl::AsciiStrToLower(arg_vec[++i]);
    } else if (absl::StartsWith(token, "--format=")) {
      format = absl::AsciiStrToLower(token.substr(9));
    }
  }

  if (message_id < 0) {
    return absl::InvalidArgumentError(
        "Usage: message-read --id <message_id> [--format <json|text>]");
  }

  if (format != "json" && format != "text") {
    return absl::InvalidArgumentError("--format must be either json or text");
  }

  Rom rom_storage;
  Rom* rom = nullptr;
  if (rom_context != nullptr && rom_context->is_loaded()) {
    rom = rom_context;
  } else {
    auto rom_or = LoadRomFromFlag();
    if (!rom_or.ok()) {
      return rom_or.status();
    }
    rom_storage = std::move(rom_or.value());
    rom = &rom_storage;
  }

  auto messages = LoadMessages(rom);

  if (message_id >= static_cast<int>(messages.size())) {
    return absl::NotFoundError(absl::StrFormat(
        "Message ID %d not found (max: %d)", message_id, messages.size() - 1));
  }

  const auto& msg = messages[message_id];

  if (format == "json") {
    std::cout << "{\n";
    std::cout << absl::StrFormat("  \"id\": %d,\n", msg.ID);
    std::cout << absl::StrFormat("  \"address\": \"0x%06X\",\n", msg.Address);

    // Escape quotes
    std::string escaped_text = msg.ContentsParsed;
    size_t pos = 0;
    while ((pos = escaped_text.find('"', pos)) != std::string::npos) {
      escaped_text.insert(pos, "\\");
      pos += 2;
    }
    std::cout << absl::StrFormat("  \"text\": \"%s\",\n", escaped_text);
    std::cout << absl::StrFormat("  \"length\": %zu\n", msg.Data.size());
    std::cout << "}\n";
  } else {
    std::cout << absl::StrFormat("📝 Message #%d\n", msg.ID);
    std::cout << absl::StrFormat("Address: 0x%06X\n", msg.Address);
    std::cout << absl::StrFormat("Length: %zu bytes\n", msg.Data.size());
    std::cout << std::string(60, '-') << "\n";
    std::cout << msg.ContentsParsed << "\n";
  }

  return absl::OkStatus();
}

absl::Status HandleMessageSearchCommand(const std::vector<std::string>& arg_vec,
                                        Rom* rom_context) {
  std::string query;
  std::string format = "json";

  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& token = arg_vec[i];
    if (token == "--query") {
      if (i + 1 >= arg_vec.size()) {
        return absl::InvalidArgumentError("--query requires a value.");
      }
      query = arg_vec[++i];
    } else if (absl::StartsWith(token, "--query=")) {
      query = token.substr(8);
    } else if (token == "--format") {
      if (i + 1 >= arg_vec.size()) {
        return absl::InvalidArgumentError("--format requires a value.");
      }
      format = absl::AsciiStrToLower(arg_vec[++i]);
    } else if (absl::StartsWith(token, "--format=")) {
      format = absl::AsciiStrToLower(token.substr(9));
    }
  }

  if (query.empty()) {
    return absl::InvalidArgumentError(
        "Usage: message-search --query <text> [--format <json|text>]");
  }

  if (format != "json" && format != "text") {
    return absl::InvalidArgumentError("--format must be either json or text");
  }

  Rom rom_storage;
  Rom* rom = nullptr;
  if (rom_context != nullptr && rom_context->is_loaded()) {
    rom = rom_context;
  } else {
    auto rom_or = LoadRomFromFlag();
    if (!rom_or.ok()) {
      return rom_or.status();
    }
    rom_storage = std::move(rom_or.value());
    rom = &rom_storage;
  }

  auto messages = LoadMessages(rom);
  std::string lowered_query = absl::AsciiStrToLower(query);

  std::vector<int> matches;
  for (const auto& msg : messages) {
    std::string lowered_text = absl::AsciiStrToLower(msg.ContentsParsed);
    if (lowered_text.find(lowered_query) != std::string::npos) {
      matches.push_back(msg.ID);
    }
  }

  if (format == "json") {
    std::cout << "{\n";
    std::cout << absl::StrFormat("  \"query\": \"%s\",\n", query);
    std::cout << absl::StrFormat("  \"match_count\": %zu,\n", matches.size());
    std::cout << "  \"matches\": [\n";

    for (size_t i = 0; i < matches.size(); ++i) {
      const auto& msg = messages[matches[i]];
      if (i > 0)
        std::cout << ",\n";

      std::string escaped_text = msg.ContentsParsed;
      size_t pos = 0;
      while ((pos = escaped_text.find('"', pos)) != std::string::npos) {
        escaped_text.insert(pos, "\\");
        pos += 2;
      }

      std::cout << "    {\n";
      std::cout << absl::StrFormat("      \"id\": %d,\n", msg.ID);
      std::cout << absl::StrFormat("      \"address\": \"0x%06X\",\n",
                                   msg.Address);
      std::cout << absl::StrFormat("      \"text\": \"%s\"\n", escaped_text);
      std::cout << "    }";
    }
    std::cout << "\n  ]\n";
    std::cout << "}\n";
  } else {
    std::cout << absl::StrFormat("🔍 Search: \"%s\" → %zu match(es)\n", query,
                                 matches.size());
    std::cout << std::string(60, '=') << "\n";

    for (int match_id : matches) {
      const auto& msg = messages[match_id];
      std::cout << absl::StrFormat("[%03d] @ 0x%06X\n", msg.ID, msg.Address);
      std::cout << "  " << msg.ContentsParsed << "\n";
      std::cout << std::string(60, '-') << "\n";
    }
  }

  return absl::OkStatus();
}

absl::Status HandleMessageStatsCommand(const std::vector<std::string>& arg_vec,
                                       Rom* rom_context) {
  std::string format = "json";

  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& token = arg_vec[i];
    if (token == "--format") {
      if (i + 1 >= arg_vec.size()) {
        return absl::InvalidArgumentError("--format requires a value.");
      }
      format = absl::AsciiStrToLower(arg_vec[++i]);
    } else if (absl::StartsWith(token, "--format=")) {
      format = absl::AsciiStrToLower(token.substr(9));
    }
  }

  if (format != "json" && format != "text") {
    return absl::InvalidArgumentError("--format must be either json or text");
  }

  Rom rom_storage;
  Rom* rom = nullptr;
  if (rom_context != nullptr && rom_context->is_loaded()) {
    rom = rom_context;
  } else {
    auto rom_or = LoadRomFromFlag();
    if (!rom_or.ok()) {
      return rom_or.status();
    }
    rom_storage = std::move(rom_or.value());
    rom = &rom_storage;
  }

  auto messages = LoadMessages(rom);

  size_t total_bytes = 0;
  size_t max_length = 0;
  size_t min_length = SIZE_MAX;

  for (const auto& msg : messages) {
    size_t len = msg.Data.size();
    total_bytes += len;
    max_length = std::max(max_length, len);
    min_length = std::min(min_length, len);
  }

  double avg_length = messages.empty()
                          ? 0.0
                          : static_cast<double>(total_bytes) / messages.size();

  if (format == "json") {
    std::cout << "{\n";
    std::cout << absl::StrFormat("  \"total_messages\": %zu,\n",
                                 messages.size());
    std::cout << absl::StrFormat("  \"total_bytes\": %zu,\n", total_bytes);
    std::cout << absl::StrFormat("  \"average_length\": %.2f,\n", avg_length);
    std::cout << absl::StrFormat("  \"min_length\": %zu,\n", min_length);
    std::cout << absl::StrFormat("  \"max_length\": %zu\n", max_length);
    std::cout << "}\n";
  } else {
    std::cout << "📊 Message Statistics\n";
    std::cout << std::string(40, '=') << "\n";
    std::cout << absl::StrFormat("Total Messages: %zu\n", messages.size());
    std::cout << absl::StrFormat("Total Bytes:    %zu\n", total_bytes);
    std::cout << absl::StrFormat("Average Length: %.2f bytes\n", avg_length);
    std::cout << absl::StrFormat("Min Length:     %zu bytes\n", min_length);
    std::cout << absl::StrFormat("Max Length:     %zu bytes\n", max_length);
  }

  return absl::OkStatus();
}

}  // namespace message
}  // namespace cli
}  // namespace yaze
