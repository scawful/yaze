#include "zelda3/music/asm_importer.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <regex>
#include <sstream>

#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"

namespace yaze {
namespace zelda3 {
namespace music {

absl::StatusOr<AsmParseResult> AsmImporter::ImportSong(
    const std::string& asm_source, const AsmImportOptions& options) {
  AsmParseResult result;
  ParseState state;

  // Initialize song with default segment
  result.song.segments.push_back(MusicSegment{});
  for (int i = 0; i < 8; ++i) {
    result.song.segments[0].tracks[i].is_empty = true;
  }

  // Parse line by line
  std::istringstream stream(asm_source);
  std::string line;

  while (std::getline(stream, line)) {
    state.line_number++;
    result.lines_parsed++;

    auto status = ParseLine(line, result.song, state, options);
    if (!status.ok()) {
      if (options.strict_mode) {
        return status;
      }
      state.errors.push_back(
          absl::StrFormat("Line %d: %s", state.line_number, status.message()));
    }
  }

  // Finalize song metadata
  if (!state.song_label.empty()) {
    result.song.name = state.song_label;
  }

  result.warnings = std::move(state.warnings);
  result.errors = std::move(state.errors);
  result.bytes_generated = state.bytes_generated;

  return result;
}

absl::StatusOr<AsmParseResult> AsmImporter::ImportFromFile(
    const std::string& path, const AsmImportOptions& options) {
  std::ifstream file(path);
  if (!file.is_open()) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Failed to open file: %s", path));
  }

  std::ostringstream buffer;
  buffer << file.rdbuf();
  file.close();

  auto result = ImportSong(buffer.str(), options);
  if (result.ok()) {
    // Use filename as song name if no label found
    if (result->song.name.empty()) {
      size_t pos = path.find_last_of("/\\");
      std::string filename =
          (pos == std::string::npos) ? path : path.substr(pos + 1);
      // Remove extension
      pos = filename.find_last_of('.');
      if (pos != std::string::npos) {
        filename = filename.substr(0, pos);
      }
      result->song.name = filename;
    }
  }

  return result;
}

absl::Status AsmImporter::ParseLine(const std::string& line, MusicSong& song,
                                    ParseState& state,
                                    const AsmImportOptions& options) {
  std::string trimmed = Trim(line);

  // Skip empty lines and comments
  if (trimmed.empty() || trimmed[0] == ';') {
    return absl::OkStatus();
  }

  // Remove inline comments
  size_t comment_pos = trimmed.find(';');
  if (comment_pos != std::string::npos) {
    trimmed = Trim(trimmed.substr(0, comment_pos));
  }

  if (trimmed.empty()) {
    return absl::OkStatus();
  }

  // Check for label
  if (ParseLabel(trimmed, state)) {
    return absl::OkStatus();
  }

  // Check for directive (!ARAMAddr, etc.)
  if (ParseDirective(trimmed, state)) {
    return absl::OkStatus();
  }

  // Check for data bytes (db, dw)
  if (trimmed.substr(0, 2) == "db" || trimmed.substr(0, 2) == "dw") {
    return ParseDataBytes(trimmed, song, state, options);
  }

  // Check for macro call
  if (trimmed[0] == '%') {
    auto events_result = ParseMacro(trimmed, state, options);
    if (!events_result.ok()) {
      return events_result.status();
    }

    // Add events to current channel
    if (state.current_channel >= 0 && state.current_channel < 8) {
      auto& track = song.segments[0].tracks[state.current_channel];
      track.is_empty = false;
      for (const auto& event : *events_result) {
        track.events.push_back(event);
      }
    }
    return absl::OkStatus();
  }

  // Unknown line - just warn
  if (options.verbose_errors) {
    state.warnings.push_back(absl::StrFormat("Line %d: Unrecognized: %s",
                                             state.line_number, trimmed));
  }

  return absl::OkStatus();
}

bool AsmImporter::ParseLabel(const std::string& line, ParseState& state) {
  // Check for label definition (ends with : or starts with .)
  if (line.back() == ':') {
    std::string label = line.substr(0, line.length() - 1);

    // Is this a channel label?
    if (label.substr(0, 8) == ".Channel" && label.length() == 9) {
      int ch = label[8] - '0';
      if (ch >= 0 && ch < 8) {
        state.current_channel = ch;
        state.label_to_channel[label] = ch;
        return true;
      }
    }

    // Is this a subroutine label?
    if (label[0] == '.') {
      // Mark as subroutine location
      state.label_to_channel[label] = -1;  // Will be filled by data
      return true;
    }

    // Must be the song label
    if (state.song_label.empty()) {
      state.song_label = label;
    }
    return true;
  }

  // Label without colon (some assemblers)
  if (line[0] == '.') {
    std::string label = line;
    if (label.substr(0, 8) == ".Channel" && label.length() >= 9) {
      int ch = label[8] - '0';
      if (ch >= 0 && ch < 8) {
        state.current_channel = ch;
        state.label_to_channel[label] = ch;
        return true;
      }
    }
    return true;  // Other dot labels
  }

  return false;
}

bool AsmImporter::ParseDirective(const std::string& line, ParseState& state) {
  // !ARAMAddr = $XXXX
  if (line[0] == '!') {
    size_t eq_pos = line.find('=');
    if (eq_pos != std::string::npos) {
      std::string name = Trim(line.substr(1, eq_pos - 1));
      std::string value = Trim(line.substr(eq_pos + 1));

      if (name == "ARAMAddr") {
        auto parsed = ParseHexValue(value);
        if (parsed.ok()) {
          // For 16-bit values, parse differently
          if (value.length() > 3) {
            uint16_t addr = 0;
            if (value[0] == '$') {
              addr = std::stoul(value.substr(1), nullptr, 16);
            } else if (value.substr(0, 2) == "0x") {
              addr = std::stoul(value.substr(2), nullptr, 16);
            }
            state.aram_address = addr;
          }
        }
        return true;
      }
      // Other directives (!ARAMC, etc.) - just acknowledge
      return true;
    }
  }

  return false;
}

absl::Status AsmImporter::ParseDataBytes(const std::string& line,
                                         MusicSong& song, ParseState& state,
                                         const AsmImportOptions& options) {
  // Skip "db " or "dw "
  std::string data = Trim(line.substr(2));
  if (data.empty()) {
    return absl::OkStatus();
  }

  // Split by comma
  std::vector<std::string> parts = absl::StrSplit(data, ',');

  for (const auto& part : parts) {
    std::string value = Trim(part);
    if (value.empty())
      continue;

    // Check for note name
    auto note_result = ParseNoteName(value);
    if (note_result.ok()) {
      if (state.current_channel >= 0 && state.current_channel < 8) {
        auto& track = song.segments[0].tracks[state.current_channel];
        track.is_empty = false;

        TrackEvent event;
        if (*note_result == kTrackEnd) {
          event.type = TrackEvent::Type::End;
        } else {
          event.type = TrackEvent::Type::Note;
          event.note.pitch = *note_result;
          event.note.duration = state.current_duration;
          event.note.has_duration_prefix = false;
        }
        track.events.push_back(event);
        state.bytes_generated++;
      }
      continue;
    }

    // Check for duration constant
    auto duration_result = ParseDurationConstant(value);
    if (duration_result.ok()) {
      state.current_duration = *duration_result;
      state.bytes_generated++;
      continue;
    }

    // Check for hex value
    auto hex_result = ParseHexValue(value);
    if (hex_result.ok()) {
      uint8_t byte_val = *hex_result;

      // Determine what this byte represents
      if (state.current_channel >= 0 && state.current_channel < 8) {
        auto& track = song.segments[0].tracks[state.current_channel];
        track.is_empty = false;

        // Is it a note?
        if (byte_val >= kNoteMinPitch && byte_val <= kNoteMaxPitch) {
          TrackEvent event;
          event.type = TrackEvent::Type::Note;
          event.note.pitch = byte_val;
          event.note.duration = state.current_duration;
          event.note.has_duration_prefix = false;
          track.events.push_back(event);
        } else if (byte_val == kNoteTie || byte_val == kNoteRest) {
          TrackEvent event;
          event.type = TrackEvent::Type::Note;
          event.note.pitch = byte_val;
          event.note.duration = state.current_duration;
          track.events.push_back(event);
        } else if (byte_val == kTrackEnd) {
          TrackEvent event;
          event.type = TrackEvent::Type::End;
          track.events.push_back(event);
        } else if (byte_val >= 0xE0 && byte_val <= 0xFF) {
          // Command byte - we'd need to read following bytes for params
          // For now, just record as raw command
          TrackEvent event;
          event.type = TrackEvent::Type::Command;
          event.command.opcode = byte_val;
          track.events.push_back(event);
        } else if (byte_val < 0x80) {
          // Duration byte
          state.current_duration = byte_val;
        }
        state.bytes_generated++;
      }
      continue;
    }

    // Unknown value
    if (options.verbose_errors) {
      state.warnings.push_back(absl::StrFormat("Line %d: Unknown value: %s",
                                               state.line_number, value));
    }
  }

  return absl::OkStatus();
}

absl::StatusOr<std::vector<TrackEvent>> AsmImporter::ParseMacro(
    const std::string& macro_call, ParseState& state,
    const AsmImportOptions& options) {
  std::vector<TrackEvent> events;

  std::string macro_name;
  std::vector<std::string> params;

  if (!ParseMacroCall(macro_call, macro_name, params)) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid macro call: %s", macro_call));
  }

  // Check for instrument macro
  auto inst_result = ResolveInstrumentMacro(macro_name);
  if (inst_result.ok()) {
    TrackEvent event;
    event.type = TrackEvent::Type::Command;
    event.command = *inst_result;
    events.push_back(event);
    state.bytes_generated += 2;  // opcode + param
    return events;
  }

  // Check for SetDuration macro
  if (macro_name == "SetDuration" && params.size() >= 1) {
    auto duration = ParseDurationConstant(params[0]);
    if (duration.ok()) {
      state.current_duration = *duration;
      state.bytes_generated++;
      return events;  // No event, just state change
    }
  }

  // Check for SetDurationN macro (with velocity)
  if (macro_name == "SetDurationN" && params.size() >= 2) {
    auto duration = ParseDurationConstant(params[0]);
    if (duration.ok()) {
      state.current_duration = *duration;
      state.bytes_generated += 2;
      return events;
    }
  }

  // Check for command macro
  std::vector<uint8_t> byte_params;
  for (const auto& p : params) {
    auto hex = ParseHexValue(p);
    if (hex.ok()) {
      byte_params.push_back(*hex);
    }
  }

  auto cmd_result = ResolveCommandMacro(macro_name, byte_params);
  if (cmd_result.ok()) {
    TrackEvent event;
    event.type = TrackEvent::Type::Command;
    event.command = *cmd_result;
    events.push_back(event);
    state.bytes_generated += 1 + byte_params.size();
    return events;
  }

  if (options.strict_mode) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Unknown macro: %s", macro_name));
  }

  state.warnings.push_back(absl::StrFormat("Line %d: Unknown macro: %s",
                                           state.line_number, macro_name));
  return events;
}

absl::StatusOr<uint8_t> AsmImporter::ParseNoteName(
    const std::string& note_name) {
  for (const auto& mapping : kAsmNoteNames) {
    if (note_name == mapping.name) {
      return mapping.pitch;
    }
  }
  return absl::NotFoundError("Unknown note name");
}

absl::StatusOr<uint8_t> AsmImporter::ParseDurationConstant(
    const std::string& duration) {
  // Check if it's a constant name (with or without !)
  std::string name = duration;
  if (!name.empty() && name[0] == '!') {
    name = name.substr(1);
  }

  // Add back the ! for comparison
  std::string full_name = "!" + name;

  for (const auto& dc : kDurationConstants) {
    if (full_name == dc.name) {
      return dc.value;
    }
  }

  return absl::NotFoundError("Unknown duration constant");
}

absl::StatusOr<MusicCommand> AsmImporter::ResolveInstrumentMacro(
    const std::string& macro_name) {
  for (const auto& mapping : kInstrumentMacroImport) {
    if (macro_name == mapping.macro) {
      MusicCommand cmd;
      cmd.opcode = 0xE0;  // SetInstrument
      cmd.params[0] = mapping.id;
      return cmd;
    }
  }
  return absl::NotFoundError("Unknown instrument macro");
}

absl::StatusOr<MusicCommand> AsmImporter::ResolveCommandMacro(
    const std::string& macro_name, const std::vector<uint8_t>& params) {
  for (const auto& mapping : kCommandMacros) {
    if (macro_name == mapping.name) {
      MusicCommand cmd;
      cmd.opcode = mapping.opcode;
      for (size_t i = 0; i < params.size() && i < 3; ++i) {
        cmd.params[i] = params[i];
      }
      return cmd;
    }
  }
  return absl::NotFoundError("Unknown command macro");
}

bool AsmImporter::ParseMacroCall(const std::string& call,
                                 std::string& macro_name,
                                 std::vector<std::string>& params) {
  // Format: %MacroName(param1, param2, ...)
  if (call.empty() || call[0] != '%') {
    return false;
  }

  size_t paren_start = call.find('(');
  if (paren_start == std::string::npos) {
    // Macro without parameters
    macro_name = call.substr(1);
    return true;
  }

  macro_name = call.substr(1, paren_start - 1);

  size_t paren_end = call.find(')', paren_start);
  if (paren_end == std::string::npos) {
    return false;
  }

  std::string params_str =
      call.substr(paren_start + 1, paren_end - paren_start - 1);
  if (!params_str.empty()) {
    std::vector<std::string> parts = absl::StrSplit(params_str, ',');
    for (const auto& p : parts) {
      params.push_back(Trim(p));
    }
  }

  return true;
}

absl::StatusOr<uint8_t> AsmImporter::ParseHexValue(const std::string& value) {
  if (value.empty()) {
    return absl::InvalidArgumentError("Empty value");
  }

  try {
    if (value[0] == '$') {
      return static_cast<uint8_t>(std::stoul(value.substr(1), nullptr, 16));
    } else if (value.length() >= 2 && value.substr(0, 2) == "0x") {
      return static_cast<uint8_t>(std::stoul(value.substr(2), nullptr, 16));
    } else if (std::isdigit(value[0])) {
      return static_cast<uint8_t>(std::stoul(value, nullptr, 10));
    }
  } catch (...) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid hex value: %s", value));
  }

  return absl::InvalidArgumentError(
      absl::StrFormat("Invalid hex value: %s", value));
}

std::string AsmImporter::Trim(const std::string& s) {
  size_t start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos)
    return "";
  size_t end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
}

}  // namespace music
}  // namespace zelda3
}  // namespace yaze
