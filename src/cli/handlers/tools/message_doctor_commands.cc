#include "cli/handlers/tools/message_doctor_commands.h"

#include <iostream>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/editor/message/message_data.h"
#include "cli/handlers/tools/diagnostic_types.h"
#include "rom/rom.h"

namespace yaze {
namespace cli {

namespace {

// Validate control codes in message data
void ValidateControlCodes(const editor::MessageData& msg,
                          DiagnosticReport& report) {
  for (size_t i = 0; i < msg.Data.size(); ++i) {
    uint8_t byte = msg.Data[i];
    // Control codes are in range 0x67-0x80
    if (byte >= 0x67 && byte <= 0x80) {
      auto cmd = editor::FindMatchingCommand(byte);
      if (!cmd.has_value()) {
        DiagnosticFinding finding;
        finding.id = "unknown_control_code";
        finding.severity = DiagnosticSeverity::kWarning;
        finding.message = absl::StrFormat(
            "Unknown control code 0x%02X in message %d", byte, msg.ID);
        finding.location = absl::StrFormat("Message %d offset %zu", msg.ID, i);
        finding.fixable = false;
        report.AddFinding(finding);
      } else if (cmd->HasArgument) {
        // Check if command argument follows when required
        if (i + 1 >= msg.Data.size()) {
          DiagnosticFinding finding;
          finding.id = "missing_command_arg";
          finding.severity = DiagnosticSeverity::kError;
          finding.message = absl::StrFormat(
              "Control code [%s] missing argument in message %d", cmd->Token,
              msg.ID);
          finding.location =
              absl::StrFormat("Message %d offset %zu", msg.ID, i);
          finding.fixable = false;
          report.AddFinding(finding);
        } else {
          // Skip the argument byte
          ++i;
        }
      }
    }
  }
}

// Check for missing terminators
void ValidateTerminators(const editor::MessageData& msg,
                         DiagnosticReport& report) {
  if (msg.Data.empty()) {
    DiagnosticFinding finding;
    finding.id = "empty_message";
    finding.severity = DiagnosticSeverity::kWarning;
    finding.message = absl::StrFormat("Message %d is empty", msg.ID);
    finding.location = absl::StrFormat("Message %d", msg.ID);
    finding.fixable = false;
    report.AddFinding(finding);
    return;
  }

  // Check that message ends with terminator (0x7F)
  if (msg.Data.back() != editor::kMessageTerminator) {
    DiagnosticFinding finding;
    finding.id = "missing_terminator";
    finding.severity = DiagnosticSeverity::kError;
    finding.message =
        absl::StrFormat("Message %d missing 0x7F terminator (ends with 0x%02X)",
                        msg.ID, msg.Data.back());
    finding.location = absl::StrFormat("Message %d", msg.ID);
    finding.suggested_action = "Add 0x7F terminator to end of message";
    finding.fixable = true;
    report.AddFinding(finding);
  }
}

// Validate dictionary references
void ValidateDictionaryRefs(const editor::MessageData& msg,
                            const std::vector<editor::DictionaryEntry>& dict,
                            DiagnosticReport& report) {
  for (size_t i = 0; i < msg.Data.size(); ++i) {
    uint8_t byte = msg.Data[i];
    if (byte >= editor::DICTOFF) {
      int dict_idx = byte - editor::DICTOFF;
      if (dict_idx >= static_cast<int>(dict.size())) {
        DiagnosticFinding finding;
        finding.id = "invalid_dict_ref";
        finding.severity = DiagnosticSeverity::kError;
        finding.message = absl::StrFormat(
            "Invalid dictionary reference [D:%02X] in message %d (max valid: "
            "%02X)",
            dict_idx, msg.ID, static_cast<int>(dict.size()) - 1);
        finding.location = absl::StrFormat("Message %d offset %zu", msg.ID, i);
        finding.fixable = false;
        report.AddFinding(finding);
      }
    }
  }
}

// Check for common corruption patterns
void CheckCorruptionPatterns(const editor::MessageData& msg,
                             DiagnosticReport& report) {
  // Check for large runs of 0x00 or 0xFF
  int zero_run = 0;
  int ff_run = 0;
  const int kCorruptionThreshold = 10;

  for (uint8_t byte : msg.Data) {
    if (byte == 0x00) {
      zero_run++;
      ff_run = 0;
    } else if (byte == 0xFF) {
      ff_run++;
      zero_run = 0;
    } else {
      zero_run = 0;
      ff_run = 0;
    }

    if (zero_run >= kCorruptionThreshold) {
      DiagnosticFinding finding;
      finding.id = "possible_corruption_zeros";
      finding.severity = DiagnosticSeverity::kWarning;
      finding.message = absl::StrFormat(
          "Large block of zeros (%d bytes) in message %d", zero_run, msg.ID);
      finding.location = absl::StrFormat("Message %d", msg.ID);
      finding.suggested_action = "Check if message data is corrupted";
      finding.fixable = false;
      report.AddFinding(finding);
      break;  // Only report once per message
    }

    if (ff_run >= kCorruptionThreshold) {
      DiagnosticFinding finding;
      finding.id = "possible_corruption_ff";
      finding.severity = DiagnosticSeverity::kWarning;
      finding.message = absl::StrFormat(
          "Large block of 0xFF (%d bytes) in message %d", ff_run, msg.ID);
      finding.location = absl::StrFormat("Message %d", msg.ID);
      finding.suggested_action = "Check if message data is corrupted or erased";
      finding.fixable = false;
      report.AddFinding(finding);
      break;  // Only report once per message
    }
  }
}

}  // namespace

absl::Status MessageDoctorCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& args,
    resources::OutputFormatter& formatter) {
  bool verbose = args.HasFlag("verbose");

  DiagnosticReport report;

  if (!rom || !rom->is_loaded()) {
    return absl::InvalidArgumentError("ROM not loaded");
  }

  // 1. Build Dictionary
  std::vector<editor::DictionaryEntry> dictionary;
  try {
    dictionary = editor::BuildDictionaryEntries(rom);
  } catch (const std::exception& e) {
    DiagnosticFinding finding;
    finding.id = "dictionary_build_failed";
    finding.severity = DiagnosticSeverity::kCritical;
    finding.message =
        absl::StrFormat("Failed to build dictionary: %s", e.what());
    finding.location = "Dictionary Tables";
    finding.fixable = false;
    report.AddFinding(finding);

    // Cannot proceed without dictionary
    formatter.AddField("total_findings", report.TotalFindings());
    formatter.AddField("critical_count", report.critical_count);
    return absl::OkStatus();
  }

  // 2. Scan Messages
  std::vector<editor::MessageData> messages;
  try {
    uint8_t* mutable_data = const_cast<uint8_t*>(rom->data());
    messages = editor::ReadAllTextData(mutable_data, editor::kTextData);

  } catch (const std::exception& e) {
    DiagnosticFinding finding;
    finding.id = "message_scan_failed";
    finding.severity = DiagnosticSeverity::kCritical;
    finding.message = absl::StrFormat("Failed to scan messages: %s", e.what());
    finding.location = "Message Data Region";
    finding.fixable = false;
    report.AddFinding(finding);
  }

  // 3. Analyze Each Message
  int valid_count = 0;
  int empty_count = 0;

  for (const auto& msg : messages) {
    if (msg.Data.empty()) {
      empty_count++;
      continue;
    }

    // Run all validations
    ValidateControlCodes(msg, report);
    ValidateTerminators(msg, report);
    ValidateDictionaryRefs(msg, dictionary, report);
    CheckCorruptionPatterns(msg, report);

    valid_count++;
  }

  // 4. Output results
  formatter.AddField("messages_scanned", static_cast<int>(messages.size()));
  formatter.AddField("valid_messages", valid_count);
  formatter.AddField("empty_messages", empty_count);
  formatter.AddField("dictionary_entries", static_cast<int>(dictionary.size()));
  formatter.AddField("total_findings", report.TotalFindings());
  formatter.AddField("critical_count", report.critical_count);
  formatter.AddField("error_count", report.error_count);
  formatter.AddField("warning_count", report.warning_count);
  formatter.AddField("info_count", report.info_count);
  formatter.AddField("fixable_count", report.fixable_count);

  // Output findings array for JSON
  if (formatter.IsJson()) {
    formatter.BeginArray("findings");
    for (const auto& finding : report.findings) {
      formatter.AddArrayItem(finding.FormatJson());
    }
    formatter.EndArray();
  }

  // Text output
  if (!formatter.IsJson()) {
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════"
                 "═══╗\n";
    std::cout << "║                     MESSAGE DOCTOR                         "
                 "   ║\n";
    std::cout << "╠════════════════════════════════════════════════════════════"
                 "═══╣\n";
    std::cout << absl::StrFormat("║  Messages Scanned: %-42d ║\n",
                                 static_cast<int>(messages.size()));
    std::cout << absl::StrFormat("║  Valid Messages: %-44d ║\n", valid_count);
    std::cout << absl::StrFormat("║  Empty Messages: %-44d ║\n", empty_count);
    std::cout << absl::StrFormat("║  Dictionary Entries: %-40d ║\n",
                                 static_cast<int>(dictionary.size()));
    std::cout << "╠════════════════════════════════════════════════════════════"
                 "═══╣\n";
    std::cout << absl::StrFormat(
        "║  Findings: %d total (%d errors, %d warnings, %d info)%-8s ║\n",
        report.TotalFindings(), report.error_count, report.warning_count,
        report.info_count, "");
    if (report.fixable_count > 0) {
      std::cout << absl::StrFormat("║  Fixable Issues: %-44d ║\n",
                                   report.fixable_count);
    }
    std::cout << "╚════════════════════════════════════════════════════════════"
                 "═══╝\n";

    if (verbose && !report.findings.empty()) {
      std::cout << "\n=== Detailed Findings ===\n";
      for (const auto& finding : report.findings) {
        std::cout << "  " << finding.FormatText() << "\n";
      }
    } else if (!verbose && report.HasProblems()) {
      std::cout << "\nUse --verbose to see detailed findings.\n";
    }

    if (!report.HasProblems()) {
      std::cout << "\n  \033[1;32mNo critical issues found.\033[0m\n";
    }
    std::cout << "\n";
  }

  return absl::OkStatus();
}

}  // namespace cli
}  // namespace yaze
