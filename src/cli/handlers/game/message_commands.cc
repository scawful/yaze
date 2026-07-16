#include "cli/handlers/game/message_commands.h"

#include <algorithm>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"

#include "app/editor/message/message_data.h"
#include "rom/snes.h"
#include "rom/transaction.h"
#include "zelda3/resource_labels.h"

ABSL_DECLARE_FLAG(std::string, rom);

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

std::vector<editor::MessageData> ReadExpandedMessages(const Rom& rom) {
  const int start = editor::GetExpandedTextDataStart();
  if (start < 0 || static_cast<size_t>(start) >= rom.size()) {
    return {};
  }
  const int end = std::min(editor::GetExpandedTextDataEnd(),
                           static_cast<int>(rom.size()) - 1);
  return editor::ReadExpandedTextData(const_cast<uint8_t*>(rom.data()), start,
                                      end);
}

std::vector<editor::MessageData> ReadExpandedMessages(const Rom& rom, int start,
                                                      int end) {
  if (start < 0 || end < start || static_cast<size_t>(start) >= rom.size()) {
    return {};
  }
  end = std::min(end, static_cast<int>(rom.size()) - 1);
  return editor::ReadExpandedTextData(const_cast<uint8_t*>(rom.data()), start,
                                      end);
}

struct ExpandedMutationContext {
  project::YazeProject project;
  int start = 0;
  int end = 0;
  int message_limit = -1;
  std::string policy_warning;
};

class ScopedHackManifestBindingRestore {
 public:
  explicit ScopedHackManifestBindingRestore(
      zelda3::ResourceLabelProvider& provider)
      : provider_(provider), previous_(provider.hack_manifest()) {}

  ScopedHackManifestBindingRestore(const ScopedHackManifestBindingRestore&) =
      delete;
  ScopedHackManifestBindingRestore& operator=(
      const ScopedHackManifestBindingRestore&) = delete;

  ~ScopedHackManifestBindingRestore() { provider_.SetHackManifest(previous_); }

 private:
  zelda3::ResourceLabelProvider& provider_;
  const core::HackManifest* previous_ = nullptr;
};

absl::StatusOr<std::filesystem::path> CanonicalExistingPath(
    const std::string& path, absl::string_view label) {
  if (path.empty()) {
    return absl::FailedPreconditionError(
        absl::StrFormat("%s path is empty", label));
  }

  std::error_code ec;
  auto canonical_path = std::filesystem::canonical(path, ec);
  if (ec) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Cannot resolve %s path '%s': %s", label, path, ec.message()));
  }
  return canonical_path;
}

std::string FormatManifestConflict(const core::WriteConflict& conflict) {
  std::string result =
      absl::StrFormat("address 0x%06X is %s", conflict.address,
                      core::AddressOwnershipToString(conflict.ownership));
  if (!conflict.module.empty()) {
    absl::StrAppend(&result, " (Module: ", conflict.module, ")");
  }
  return result;
}

absl::StatusOr<ExpandedMutationContext> PreflightExpandedMutation(
    const resources::ArgumentParser& parser, const Rom& rom) {
  auto project_path = parser.GetString("project");
  if (!project_path.has_value() || project_path->empty()) {
    return absl::FailedPreconditionError(
        "Expanded message mutation requires explicit --project so Yaze can "
        "verify ROM ownership and write policy");
  }
  if (!rom.is_loaded()) {
    return absl::FailedPreconditionError("ROM is not loaded");
  }

  ExpandedMutationContext context;
  auto& resource_labels = zelda3::GetResourceLabels();
  absl::Status open_status;
  {
    // YazeProject::Open temporarily installs its manifest in the
    // process-global label provider. Restore the embedding application's prior
    // non-owning pointer on every path, including malformed project data that
    // throws from a parser.
    ScopedHackManifestBindingRestore restore_manifest(resource_labels);
    try {
      open_status = context.project.Open(*project_path);
    } catch (const std::exception& error) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Cannot load project '%s': %s", *project_path, error.what()));
    } catch (...) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Cannot load project '%s': unknown project parse error",
          *project_path));
    }
  }
  if (!open_status.ok()) {
    return absl::Status(open_status.code(),
                        absl::StrFormat("Cannot load project '%s': %s",
                                        *project_path, open_status.message()));
  }

  auto active_rom_path_or = CanonicalExistingPath(rom.filename(), "active ROM");
  if (!active_rom_path_or.ok()) {
    return active_rom_path_or.status();
  }
  auto project_rom_path_or = CanonicalExistingPath(
      context.project.GetAbsolutePath(context.project.rom_filename),
      "project ROM");
  if (!project_rom_path_or.ok()) {
    return project_rom_path_or.status();
  }
  if (*active_rom_path_or != *project_rom_path_or) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Project ROM mismatch: active ROM is '%s' but --project binds to '%s'",
        active_rom_path_or->string(), project_rom_path_or->string()));
  }

  const auto& manifest = context.project.hack_manifest;
  if (!manifest.loaded()) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Project '%s' has no loaded hack manifest; configure a valid "
        "hack_manifest_file before mutating expanded messages",
        *project_path));
  }

  const auto& layout = manifest.message_layout();
  if (layout.data_start == 0 || layout.data_end == 0 ||
      layout.data_end < layout.data_start ||
      (layout.data_start & 0xFFFFu) < 0x8000u ||
      (layout.data_end & 0xFFFFu) < 0x8000u) {
    return absl::FailedPreconditionError(
        "Hack manifest does not define a valid LoROM expanded message region");
  }

  const uint32_t start = SnesToPc(layout.data_start);
  const uint32_t end = SnesToPc(layout.data_end);
  if (end < start ||
      end > static_cast<uint32_t>(std::numeric_limits<int>::max()) ||
      end >= rom.size()) {
    return absl::OutOfRangeError(absl::StrFormat(
        "Configured expanded message region [0x%06X, 0x%06X] is outside the "
        "active ROM (size=0x%zX)",
        start, end, rom.size()));
  }
  context.start = static_cast<int>(start);
  context.end = static_cast<int>(end);

  // Even a manifest without ID metadata has a strict physical upper bound:
  // each message requires at least a 0x7F terminator and the bank needs a final
  // 0xFF. Keeping this limit finite prevents hostile IDs from driving a huge
  // sparse-vector allocation.
  const int region_capacity = context.end - context.start + 1;
  const int capacity_message_limit = std::max(0, region_capacity - 1);
  context.message_limit = capacity_message_limit;
  if (layout.expanded_count > 0) {
    context.message_limit =
        std::min(layout.expanded_count, capacity_message_limit);
  }
  if (layout.last_expanded_id >= layout.first_expanded_id &&
      (layout.first_expanded_id != 0 || layout.last_expanded_id != 0)) {
    const int declared_limit =
        static_cast<int>(layout.last_expanded_id - layout.first_expanded_id) +
        1;
    context.message_limit = std::min(context.message_limit, declared_limit);
  }

  const std::string lowered_hack_name =
      absl::AsciiStrToLower(manifest.hack_name());
  if (context.project.rom_metadata.write_policy ==
          project::RomWritePolicy::kBlock &&
      absl::StrContains(lowered_hack_name, "oracle of secrets")) {
    return absl::PermissionDeniedError(
        "Oracle of Secrets expanded messages are owned by ASM. No message "
        "bytes were written; edit Core/message.asm, rebuild Oracle of Secrets, "
        "and reopen the rebuilt ROM before retrying.");
  }

  const auto conflicts =
      manifest.AnalyzePcWriteRanges({{start, static_cast<uint32_t>(end + 1u)}});
  if (!conflicts.empty()) {
    const std::string detail = FormatManifestConflict(conflicts.front());
    if (context.project.rom_metadata.write_policy ==
        project::RomWritePolicy::kBlock) {
      return absl::PermissionDeniedError(absl::StrFormat(
          "Expanded message mutation blocked by hack manifest: %s", detail));
    }
    if (context.project.rom_metadata.write_policy ==
        project::RomWritePolicy::kWarn) {
      context.policy_warning = absl::StrFormat(
          "Expanded message region conflicts with hack manifest: %s", detail);
    }
  }

  return context;
}

absl::Status ValidateExpandedMessageId(int id,
                                       const ExpandedMutationContext& context) {
  if (id < 0) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid expanded message ID: %d", id));
  }
  if (context.message_limit >= 0 && id >= context.message_limit) {
    return absl::OutOfRangeError(absl::StrFormat(
        "Expanded message ID %d is outside the manifest range (count=%d)", id,
        context.message_limit));
  }
  return absl::OkStatus();
}

absl::Status ValidateExpandedMessageBankSize(
    size_t message_count, const ExpandedMutationContext& context) {
  if (context.message_limit >= 0 &&
      message_count > static_cast<size_t>(context.message_limit)) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Expanded message bank contains %zu messages, exceeding the manifest "
        "limit of %d; no changes were applied",
        message_count, context.message_limit));
  }
  return absl::OkStatus();
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

  auto messages = editor::ReadAllTextData(const_cast<uint8_t*>(rom->data()),
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

  auto messages = editor::ReadAllTextData(const_cast<uint8_t*>(rom->data()),
                                          editor::kTextData);
  if (message_id < 0 || message_id >= static_cast<int>(messages.size())) {
    return absl::NotFoundError(
        absl::StrFormat("Message ID %d not found (max: %d)", message_id,
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

  auto messages = editor::ReadAllTextData(const_cast<uint8_t*>(rom->data()),
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
    if (i > 0)
      hex_str += " ";
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
      bytes.push_back(static_cast<uint8_t>(std::stoi(hex_byte, nullptr, 16)));
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
      if (i > 0)
        hex_str += " ";
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
    expanded = ReadExpandedMessages(*rom);
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

    ParsedEntry parsed{entry,
                       editor::ParseMessageToDataWithDiagnostics(entry.text),
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

  Rom owned_rom;
  Rom* active_rom = rom;

  if (apply) {
    if (active_rom == nullptr || !active_rom->is_loaded()) {
      auto rom_path = parser.GetString("rom");
      if (!rom_path.has_value() || rom_path->empty()) {
        std::string global_rom_path = absl::GetFlag(FLAGS_rom);
        if (!global_rom_path.empty()) {
          rom_path = global_rom_path;
        }
      }
      if (!rom_path.has_value() || rom_path->empty()) {
        error_count++;
        formatter.AddField("status", "error");
        formatter.AddField("error",
                           "ROM not loaded; provide --rom when using --apply");
        formatter.AddField("parse_error_count", parse_error_count);
        formatter.AddField("error_count", error_count);
        formatter.EndObject();
        return absl::OkStatus();
      }
      auto load_status = owned_rom.LoadFromFile(*rom_path);
      if (!load_status.ok()) {
        error_count++;
        formatter.AddField("status", "error");
        formatter.AddField("error", std::string(load_status.message()));
        formatter.AddField("parse_error_count", parse_error_count);
        formatter.AddField("error_count", error_count);
        formatter.EndObject();
        return absl::OkStatus();
      }
      active_rom = &owned_rom;
    }

    if (has_errors) {
      formatter.AddField("status", "error");
      formatter.AddField("error", "Parse errors present; no changes applied");
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

    std::optional<ExpandedMutationContext> expanded_context;
    if (IncludeExpanded(range) && has_expanded_entries) {
      auto context_or = PreflightExpandedMutation(parser, *active_rom);
      if (!context_or.ok()) {
        error_count++;
        formatter.AddField("status", "error");
        formatter.AddField("error", std::string(context_or.status().message()));
        formatter.AddField("parse_error_count", parse_error_count);
        formatter.AddField("error_count", error_count);
        formatter.EndObject();
        return context_or.status();
      }
      expanded_context.emplace(std::move(context_or.value()));
      if (!expanded_context->policy_warning.empty()) {
        formatter.AddField("write_policy_warning",
                           expanded_context->policy_warning);
      }
    }

    // Build both replacement banks and validate every selected ID before the
    // first ROM write. This keeps a bad expanded ID from landing after a
    // successful vanilla mutation in a mixed bundle.
    std::vector<editor::MessageData> vanilla_messages;
    if (IncludeVanilla(range) && has_vanilla_entries) {
      vanilla_messages = editor::ReadAllTextData(
          const_cast<uint8_t*>(active_rom->data()), editor::kTextData);
    }

    std::vector<std::string> expanded_texts;
    if (expanded_context.has_value()) {
      const auto expanded_messages = ReadExpandedMessages(
          *active_rom, expanded_context->start, expanded_context->end);
      auto bank_size_status = ValidateExpandedMessageBankSize(
          expanded_messages.size(), *expanded_context);
      if (!bank_size_status.ok()) {
        formatter.AddField("status", "error");
        formatter.AddField("error", std::string(bank_size_status.message()));
        formatter.EndObject();
        return bank_size_status;
      }
      expanded_texts.reserve(expanded_messages.size());
      for (const auto& message : expanded_messages) {
        expanded_texts.push_back(message.RawString);
      }
    }

    for (const auto& parsed : parsed_entries) {
      if (parsed.entry.bank == editor::MessageBank::kVanilla) {
        if (parsed.entry.id < 0 ||
            parsed.entry.id >= static_cast<int>(vanilla_messages.size())) {
          has_errors = true;
          error_count++;
          continue;
        }
        auto& message = vanilla_messages[parsed.entry.id];
        message.RawString = parsed.entry.text;
        message.ContentsParsed = parsed.entry.text;
        message.Data = parsed.parse.bytes;
        message.DataParsed = parsed.parse.bytes;
      } else {
        if (!expanded_context.has_value()) {
          continue;
        }
        const auto id_status =
            ValidateExpandedMessageId(parsed.entry.id, *expanded_context);
        if (!id_status.ok()) {
          has_errors = true;
          error_count++;
          continue;
        }
        if (parsed.entry.id >= static_cast<int>(expanded_texts.size())) {
          expanded_texts.resize(parsed.entry.id + 1);
        }
        expanded_texts[parsed.entry.id] = parsed.entry.text;
      }
      applied_updates++;
    }

    if (has_errors) {
      formatter.AddField("status", "error");
      formatter.AddField("error", "Invalid message IDs; no changes applied");
    } else {
      ScopedRomTransaction transaction(*active_rom);
      if (IncludeVanilla(range) && has_vanilla_entries) {
        auto status = editor::WriteAllTextData(active_rom, vanilla_messages);
        if (!status.ok()) {
          formatter.AddField("status", "error");
          formatter.AddField("error", std::string(status.message()));
          formatter.EndObject();
          return status;
        }
      }

      if (expanded_context.has_value()) {
        auto status = editor::WriteExpandedTextData(
            active_rom, expanded_context->start, expanded_context->end,
            expanded_texts);
        if (!status.ok()) {
          formatter.AddField("status", "error");
          formatter.AddField("error", std::string(status.message()));
          formatter.EndObject();
          return status;
        }
      }

      if (active_rom->dirty()) {
        auto save_status = active_rom->SaveToFile({.save_new = false});
        if (!save_status.ok()) {
          formatter.AddField("status", "error");
          formatter.AddField("error", std::string(save_status.message()));
          formatter.EndObject();
          return save_status;
        }
      }
      transaction.Commit();
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
  if (!id_or.ok())
    return id_or.status();
  int msg_id = id_or.value();

  auto text = parser.GetString("text").value();

  // Validate line widths first
  auto warnings = editor::ValidateMessageLineWidths(text);

  // Encode to bytes
  auto bytes = editor::ParseMessageToData(text);
  if (bytes.empty() && !text.empty()) {
    return absl::InvalidArgumentError("Encoding produced no bytes");
  }

  auto context_or = PreflightExpandedMutation(parser, *rom);
  if (!context_or.ok()) {
    return context_or.status();
  }
  auto context = std::move(context_or.value());
  auto id_status = ValidateExpandedMessageId(msg_id, context);
  if (!id_status.ok()) {
    return id_status;
  }

  // Read existing expanded messages to find the target
  auto expanded = ReadExpandedMessages(*rom, context.start, context.end);
  auto bank_size_status =
      ValidateExpandedMessageBankSize(expanded.size(), context);
  if (!bank_size_status.ok()) {
    return bank_size_status;
  }

  // Build the full message list, inserting/replacing at msg_id
  std::vector<std::string> all_texts;
  all_texts.reserve(expanded.size());
  for (const auto& msg : expanded) {
    all_texts.push_back(msg.RawString);
  }
  if (msg_id >= static_cast<int>(all_texts.size())) {
    all_texts.resize(msg_id + 1);
  }
  all_texts[msg_id] = text;

  // Write back
  ScopedRomTransaction transaction(*rom);
  auto status =
      editor::WriteExpandedTextData(rom, context.start, context.end, all_texts);
  if (!status.ok())
    return status;
  transaction.Commit();

  formatter.BeginObject("Message Write Result");
  formatter.AddField("id", msg_id);
  formatter.AddField("text", text);
  formatter.AddField("encoded_length", static_cast<int>(bytes.size()));
  formatter.AddField("status", "success");
  if (!context.policy_warning.empty()) {
    formatter.AddField("write_policy_warning", context.policy_warning);
  }
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
  auto messages =
      editor::ReadAllTextData(const_cast<uint8_t*>(rom->data()), start);

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
      if (i > 0)
        file << ", ";
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
