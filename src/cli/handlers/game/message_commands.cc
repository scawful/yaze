#include "cli/handlers/game/message_commands.h"

#include <fstream>
#include <sstream>

#include "absl/strings/ascii.h"
#include "absl/strings/str_format.h"

#include "app/editor/message/message_data.h"

namespace yaze {
namespace cli {
namespace handlers {

namespace {
std::string NormalizeRange(const std::string& range) {
  return absl::AsciiStrToLower(range);
}

bool IncludeVanilla(const std::string& range) {
  return range == "all" || range == "vanilla";
}

bool IncludeExpanded(const std::string& range) {
  return range == "all" || range == "expanded";
}

std::string BankLabel(editor::MessageBank bank) {
  return editor::MessageBankToString(bank);
}
}  // namespace

// ===========================================================================
// Existing Commands
// ===========================================================================

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

// ===========================================================================
// New: Encode Command
// ===========================================================================

absl::Status MessageEncodeCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto text = parser.GetString("text").value();

  auto parse_result = editor::ParseMessageToDataWithDiagnostics(text);
  auto bytes = parse_result.bytes;

  // Build hex string
  std::string hex_str;
  for (size_t i = 0; i < bytes.size(); ++i) {
    if (i > 0) hex_str += " ";
    hex_str += absl::StrFormat("%02X", bytes[i]);
  }

  formatter.BeginObject("Encoded Message");
  formatter.AddField("input", text);
  formatter.AddField("hex", hex_str);
  formatter.AddField("length", static_cast<int>(bytes.size()));

  // Also output as byte array for JSON consumers
  formatter.BeginArray("bytes");
  for (uint8_t byte : bytes) {
    formatter.AddArrayItem(absl::StrFormat("0x%02X", byte));
  }
  formatter.EndArray();

  // Run line width validation
  auto warnings = editor::ValidateMessageLineWidths(text);
  if (!warnings.empty()) {
    formatter.BeginArray("line_width_warnings");
    for (const auto& warning : warnings) {
      formatter.AddArrayItem(warning);
    }
    formatter.EndArray();
  }
  if (!parse_result.warnings.empty()) {
    formatter.BeginArray("warnings");
    for (const auto& warning : parse_result.warnings) {
      formatter.AddArrayItem(warning);
    }
    formatter.EndArray();
  }
  if (!parse_result.errors.empty()) {
    formatter.BeginArray("errors");
    for (const auto& error : parse_result.errors) {
      formatter.AddArrayItem(error);
    }
    formatter.EndArray();
  }

  formatter.EndObject();

  return absl::OkStatus();
}

// ===========================================================================
// New: Decode Command
// ===========================================================================

absl::Status MessageDecodeCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto hex_input = parser.GetString("hex").value();

  // Parse hex string to bytes
  std::vector<uint8_t> bytes;
  std::istringstream hex_stream(hex_input);
  std::string hex_byte;
  while (hex_stream >> hex_byte) {
    try {
      bytes.push_back(
          static_cast<uint8_t>(std::stoi(hex_byte, nullptr, 16)));
    } catch (const std::exception&) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Invalid hex byte: '%s'", hex_byte));
    }
  }

  // Decode bytes to text, handling commands with arguments
  std::string decoded;
  for (size_t i = 0; i < bytes.size(); ++i) {
    uint8_t byte = bytes[i];

    // Check for command (may consume next byte as argument)
    auto cmd = editor::FindMatchingCommand(byte);
    if (cmd.has_value()) {
      if (cmd->HasArgument && i + 1 < bytes.size()) {
        decoded += cmd->GetParamToken(bytes[++i]);
      } else {
        decoded += cmd->GetParamToken();
      }
      continue;
    }

    // Single-byte lookup for chars, specials, dictionary
    decoded += editor::ParseTextDataByte(byte);
  }

  formatter.BeginObject("Decoded Message");
  formatter.AddField("hex", hex_input);
  formatter.AddField("text", decoded);
  formatter.AddField("length", static_cast<int>(bytes.size()));
  formatter.EndObject();

  return absl::OkStatus();
}

// ===========================================================================
// New: Import Org Command
// ===========================================================================

absl::Status MessageImportOrgCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto file_path = parser.GetString("file").value();

  // Read the .org file
  std::ifstream file(file_path);
  if (!file.is_open()) {
    return absl::NotFoundError(
        absl::StrFormat("Cannot open file: %s", file_path));
  }

  std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  file.close();

  auto messages = editor::ParseOrgContent(content);

  formatter.BeginObject("Org Import Results");
  formatter.AddField("file", file_path);
  formatter.AddField("messages_parsed", static_cast<int>(messages.size()));

  formatter.BeginArray("messages");
  for (const auto& [msg_id, body] : messages) {
    // Encode the body text to bytes with diagnostics
    auto parse_result = editor::ParseMessageToDataWithDiagnostics(body);
    auto bytes = parse_result.bytes;
    auto warnings = editor::ValidateMessageLineWidths(body);

    std::string hex_str;
    for (size_t i = 0; i < bytes.size(); ++i) {
      if (i > 0) hex_str += " ";
      hex_str += absl::StrFormat("%02X", bytes[i]);
    }

    formatter.BeginObject();
    formatter.AddHexField("id", msg_id, 2);
    formatter.AddField("text", body);
    formatter.AddField("hex", hex_str);
    formatter.AddField("encoded_length", static_cast<int>(bytes.size()));
    if (!warnings.empty()) {
      formatter.BeginArray("warnings");
      for (const auto& warning : warnings) {
        formatter.AddArrayItem(warning);
      }
      formatter.EndArray();
    }
    if (!parse_result.warnings.empty()) {
      formatter.BeginArray("parse_warnings");
      for (const auto& warning : parse_result.warnings) {
        formatter.AddArrayItem(warning);
      }
      formatter.EndArray();
    }
    if (!parse_result.errors.empty()) {
      formatter.BeginArray("errors");
      for (const auto& error : parse_result.errors) {
        formatter.AddArrayItem(error);
      }
      formatter.EndArray();
    }
    formatter.EndObject();
  }
  formatter.EndArray();
  formatter.EndObject();

  return absl::OkStatus();
}

// ===========================================================================
// New: Export Org Command
// ===========================================================================

absl::Status MessageExportOrgCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto output_path = parser.GetString("output").value();

  auto messages = editor::ReadAllTextData(const_cast<uint8_t*>(rom->data()),
                                          editor::kTextData);

  // Build message pairs and labels
  std::vector<std::pair<int, std::string>> msg_pairs;
  std::vector<std::string> labels;
  for (const auto& msg : messages) {
    msg_pairs.push_back({msg.ID, msg.RawString});
    labels.push_back(absl::StrFormat("Message %02X", msg.ID));
  }

  std::string org_content = editor::ExportToOrgFormat(msg_pairs, labels);

  // Write to file
  std::ofstream file(output_path);
  if (!file.is_open()) {
    return absl::InternalError(
        absl::StrFormat("Cannot write to file: %s", output_path));
  }
  file << org_content;
  file.close();

  formatter.BeginObject("Org Export Results");
  formatter.AddField("output", output_path);
  formatter.AddField("messages_exported", static_cast<int>(messages.size()));
  formatter.AddField("status", "success");
  formatter.EndObject();

  return absl::OkStatus();
}

// ===========================================================================
// New: Export Bundle Command
// ===========================================================================

absl::Status MessageExportBundleCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto output_path = parser.GetString("output").value();
  auto range = NormalizeRange(parser.GetString("range").value_or("all"));
  if (!IncludeVanilla(range) && !IncludeExpanded(range)) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid range: %s", range));
  }

  std::vector<editor::MessageData> vanilla;
  std::vector<editor::MessageData> expanded;

  if (IncludeVanilla(range)) {
    vanilla = editor::ReadAllTextData(const_cast<uint8_t*>(rom->data()),
                                      editor::kTextData);
  }

  if (IncludeExpanded(range)) {
    expanded = editor::ReadExpandedTextData(
        const_cast<uint8_t*>(rom->data()), editor::GetExpandedTextDataStart());
  }

  auto status =
      editor::ExportMessageBundleToJson(output_path, vanilla, expanded);
  if (!status.ok()) {
    return status;
  }

  formatter.BeginObject("Message Bundle Export");
  formatter.AddField("output", output_path);
  formatter.AddField("range", range);
  formatter.AddField("vanilla_count", static_cast<int>(vanilla.size()));
  formatter.AddField("expanded_count", static_cast<int>(expanded.size()));
  formatter.AddField("status", "success");
  formatter.EndObject();

  return absl::OkStatus();
}

// ===========================================================================
// New: Import Bundle Command
// ===========================================================================

absl::Status MessageImportBundleCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto file_path = parser.GetString("file").value();
  const bool apply = parser.HasFlag("apply");
  const bool strict = parser.HasFlag("strict");
  auto range = NormalizeRange(parser.GetString("range").value_or("all"));
  if (!IncludeVanilla(range) && !IncludeExpanded(range)) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid range: %s", range));
  }

  auto entries_or = editor::LoadMessageBundleFromJson(file_path);
  if (!entries_or.ok()) {
    return entries_or.status();
  }
  auto entries = entries_or.value();

  formatter.BeginObject("Message Bundle Import");
  formatter.AddField("file", file_path);
  formatter.AddField("range", range);
  formatter.AddField("apply", apply);
  formatter.AddField("strict", strict);
  formatter.AddField("entries", static_cast<int>(entries.size()));

  bool has_errors = false;
  int parse_error_count = 0;
  int error_count = 0;
  int applied_updates = 0;
  bool has_vanilla_entries = false;
  bool has_expanded_entries = false;

  struct ParsedEntry {
    editor::MessageBundleEntry entry;
    editor::MessageParseResult parse;
    std::vector<std::string> line_warnings;
  };
  std::vector<ParsedEntry> parsed_entries;
  parsed_entries.reserve(entries.size());

  for (const auto& entry : entries) {
    if ((entry.bank == editor::MessageBank::kVanilla &&
         !IncludeVanilla(range)) ||
        (entry.bank == editor::MessageBank::kExpanded &&
         !IncludeExpanded(range))) {
      continue;
    }

    ParsedEntry parsed{entry, editor::ParseMessageToDataWithDiagnostics(entry.text),
                       editor::ValidateMessageLineWidths(entry.text)};
    if (!parsed.parse.ok()) {
      has_errors = true;
      parse_error_count += static_cast<int>(parsed.parse.errors.size());
      error_count += static_cast<int>(parsed.parse.errors.size());
    }
    if (entry.bank == editor::MessageBank::kVanilla) {
      has_vanilla_entries = true;
    } else {
      has_expanded_entries = true;
    }
    parsed_entries.push_back(std::move(parsed));
  }

  formatter.BeginArray("messages");
  for (const auto& parsed : parsed_entries) {
    formatter.BeginObject();
    formatter.AddField("id", parsed.entry.id);
    formatter.AddField("bank", BankLabel(parsed.entry.bank));
    formatter.AddField("text", parsed.entry.text);
    formatter.AddField("encoded_length",
                       static_cast<int>(parsed.parse.bytes.size()));
    if (!parsed.line_warnings.empty()) {
      formatter.BeginArray("line_width_warnings");
      for (const auto& warning : parsed.line_warnings) {
        formatter.AddArrayItem(warning);
      }
      formatter.EndArray();
    }
    if (!parsed.parse.warnings.empty()) {
      formatter.BeginArray("warnings");
      for (const auto& warning : parsed.parse.warnings) {
        formatter.AddArrayItem(warning);
      }
      formatter.EndArray();
    }
    if (!parsed.parse.errors.empty()) {
      formatter.BeginArray("errors");
      for (const auto& error : parsed.parse.errors) {
        formatter.AddArrayItem(error);
      }
      formatter.EndArray();
    }
    formatter.EndObject();
  }
  formatter.EndArray();

  if (apply) {
    if (rom == nullptr || !rom->is_loaded()) {
      error_count++;
      formatter.AddField("status", "error");
      formatter.AddField("error", "ROM not loaded; cannot apply changes");
      formatter.AddField("parse_error_count", parse_error_count);
      formatter.AddField("error_count", error_count);
      formatter.EndObject();
      return absl::OkStatus();
    }

    if (has_errors) {
      formatter.AddField("status", "error");
      formatter.AddField("error",
                          "Parse errors present; no changes applied");
      formatter.AddField("parse_error_count", parse_error_count);
      formatter.AddField("error_count", error_count);
      formatter.EndObject();
      if (strict && parse_error_count > 0) {
        formatter.EndObject();
        formatter.Print();
        return absl::FailedPreconditionError(
            "Strict validation failed due to parse errors");
      }
      return absl::OkStatus();
    }

    if (IncludeVanilla(range) && has_vanilla_entries) {
      auto vanilla_messages = editor::ReadAllTextData(
          const_cast<uint8_t*>(rom->data()), editor::kTextData);
      for (const auto& parsed : parsed_entries) {
        if (parsed.entry.bank != editor::MessageBank::kVanilla) {
          continue;
        }
        if (parsed.entry.id < 0 ||
            parsed.entry.id >= static_cast<int>(vanilla_messages.size())) {
          has_errors = true;
          error_count++;
          continue;
        }
        auto& msg = vanilla_messages[parsed.entry.id];
        msg.RawString = parsed.entry.text;
        msg.ContentsParsed = parsed.entry.text;
        msg.Data = parsed.parse.bytes;
        msg.DataParsed = parsed.parse.bytes;
        applied_updates++;
      }

      if (!has_errors) {
        auto status = editor::WriteAllTextData(rom, vanilla_messages);
        if (!status.ok()) {
          formatter.AddField("status", "error");
          formatter.AddField("error", std::string(status.message()));
          formatter.EndObject();
          return absl::OkStatus();
        }
      }
    }

    if (IncludeExpanded(range) && has_expanded_entries) {
      auto expanded_messages = editor::ReadExpandedTextData(
          const_cast<uint8_t*>(rom->data()),
          editor::GetExpandedTextDataStart());
      std::vector<std::string> expanded_texts;
      expanded_texts.reserve(expanded_messages.size());
      for (const auto& msg : expanded_messages) {
        expanded_texts.push_back(msg.RawString);
      }

      for (const auto& parsed : parsed_entries) {
        if (parsed.entry.bank != editor::MessageBank::kExpanded) {
          continue;
        }
        if (parsed.entry.id < 0) {
          has_errors = true;
          error_count++;
          continue;
        }
        if (parsed.entry.id >= static_cast<int>(expanded_texts.size())) {
          expanded_texts.resize(parsed.entry.id + 1);
        }
        expanded_texts[parsed.entry.id] = parsed.entry.text;
        applied_updates++;
      }

      if (!has_errors) {
        auto status = editor::WriteExpandedTextData(
            rom, editor::GetExpandedTextDataStart(),
            editor::GetExpandedTextDataEnd(), expanded_texts);
        if (!status.ok()) {
          formatter.AddField("status", "error");
          formatter.AddField("error", std::string(status.message()));
          formatter.EndObject();
          return absl::OkStatus();
        }
      }
    }

    if (has_errors) {
      formatter.AddField("status", "error");
      formatter.AddField("error",
                          "Invalid message IDs; no changes applied");
    } else {
      formatter.AddField("status", "success");
      formatter.AddField("applied_messages", applied_updates);
    }
  } else {
    formatter.AddField("status", has_errors ? "error" : "success");
  }

  formatter.AddField("error_count", error_count);
  formatter.AddField("parse_error_count", parse_error_count);
  formatter.EndObject();
  if (strict && parse_error_count > 0) {
    formatter.EndObject();
    formatter.Print();
    return absl::FailedPreconditionError("Strict validation failed");
  }
  return absl::OkStatus();
}

// ===========================================================================
// New: Message Write Command
// ===========================================================================

absl::Status MessageWriteCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto id_or = parser.GetInt("id");
  if (!id_or.ok()) return id_or.status();
  int msg_id = id_or.value();

  auto text = parser.GetString("text").value();

  // Validate line widths first
  auto warnings = editor::ValidateMessageLineWidths(text);

  // Encode to bytes
  auto bytes = editor::ParseMessageToData(text);
  if (bytes.empty() && !text.empty()) {
    return absl::InvalidArgumentError("Encoding produced no bytes");
  }

  // Read existing expanded messages to find the target
  auto expanded = editor::ReadExpandedTextData(
      const_cast<uint8_t*>(rom->data()), editor::GetExpandedTextDataStart());

  // Build the full message list, inserting/replacing at msg_id
  std::vector<std::string> all_texts;
  bool replaced = false;
  for (const auto& msg : expanded) {
    if (msg.ID == msg_id) {
      all_texts.push_back(text);
      replaced = true;
    } else {
      all_texts.push_back(msg.RawString);
    }
  }
  if (!replaced) {
    // Append as new message
    all_texts.push_back(text);
  }

  // Write back
  auto status = editor::WriteExpandedTextData(
      rom, editor::GetExpandedTextDataStart(), editor::GetExpandedTextDataEnd(),
      all_texts);
  if (!status.ok()) return status;

  formatter.BeginObject("Message Write Result");
  formatter.AddField("id", msg_id);
  formatter.AddField("text", text);
  formatter.AddField("encoded_length", static_cast<int>(bytes.size()));
  formatter.AddField("status", "success");
  if (!warnings.empty()) {
    formatter.BeginArray("line_width_warnings");
    for (const auto& warning : warnings) {
      formatter.AddArrayItem(warning);
    }
    formatter.EndArray();
  }
  formatter.EndObject();

  return absl::OkStatus();
}

// ===========================================================================
// New: Export BIN Command
// ===========================================================================

absl::Status MessageExportBinCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto output_path = parser.GetString("output").value();
  auto range = parser.GetString("range").value_or("expanded");

  int start, end_addr;
  if (range == "expanded") {
    start = editor::GetExpandedTextDataStart();
    end_addr = editor::GetExpandedTextDataEnd();
  } else {
    start = editor::kTextData;
    end_addr = editor::kTextDataEnd;
  }

  // Find the actual end of data (scan for 0xFF terminator)
  const uint8_t* data = rom->data();
  int data_end = start;
  while (data_end <= end_addr && data[data_end] != 0xFF) {
    data_end++;
  }
  if (data_end <= end_addr) {
    data_end++;  // Include the 0xFF terminator
  }

  int size = data_end - start;

  std::ofstream file(output_path, std::ios::binary);
  if (!file.is_open()) {
    return absl::InternalError(
        absl::StrFormat("Cannot write to file: %s", output_path));
  }
  file.write(reinterpret_cast<const char*>(data + start), size);
  file.close();

  formatter.BeginObject("BIN Export Result");
  formatter.AddField("output", output_path);
  formatter.AddField("range", range);
  formatter.AddHexField("start_address", start, 6);
  formatter.AddHexField("end_address", data_end - 1, 6);
  formatter.AddField("size_bytes", size);
  formatter.AddField("status", "success");
  formatter.EndObject();

  return absl::OkStatus();
}

// ===========================================================================
// New: Export ASM Command
// ===========================================================================

absl::Status MessageExportAsmCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto output_path = parser.GetString("output").value();
  auto range = parser.GetString("range").value_or("expanded");

  int start;
  uint32_t snes_addr;
  if (range == "expanded") {
    start = editor::GetExpandedTextDataStart();
    snes_addr = 0x2F8000;
  } else {
    start = editor::kTextData;
    snes_addr = 0x1C0000;
  }

  // Read messages from the specified region
  auto messages = editor::ReadAllTextData(
      const_cast<uint8_t*>(rom->data()), start);

  std::ofstream file(output_path);
  if (!file.is_open()) {
    return absl::InternalError(
        absl::StrFormat("Cannot write to file: %s", output_path));
  }

  // Write ASM header
  file << "; Auto-generated message data\n";
  file << absl::StrFormat("; Source: %s region\n", range);
  file << absl::StrFormat("; Messages: %d\n\n", messages.size());
  file << absl::StrFormat("org $%06X\n\n", snes_addr);

  // Write each message as db directives
  for (const auto& msg : messages) {
    file << absl::StrFormat("; Message $%02X: %s\n", msg.ID,
                            msg.ContentsParsed.substr(0, 60));
    file << "db ";
    for (size_t i = 0; i < msg.Data.size(); ++i) {
      if (i > 0) file << ", ";
      file << absl::StrFormat("$%02X", msg.Data[i]);
    }
    file << ", $7F  ; terminator\n\n";
  }

  // End-of-region marker
  file << "db $FF  ; end of message data\n";
  file.close();

  formatter.BeginObject("ASM Export Result");
  formatter.AddField("output", output_path);
  formatter.AddField("range", range);
  formatter.AddField("messages_exported", static_cast<int>(messages.size()));
  formatter.AddField("status", "success");
  formatter.EndObject();

  return absl::OkStatus();
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
