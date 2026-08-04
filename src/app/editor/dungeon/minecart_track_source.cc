#include "app/editor/dungeon/minecart_track_source.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"

namespace yaze::editor {
namespace {

constexpr std::string_view kPlannedTrackGuard =
    "if !ENABLE_MINECART_PLANNED_TRACK_TABLE == 1";
constexpr std::array<std::string_view, 3> kSectionLabels = {
    ".TrackStartingRooms", ".TrackStartingX", ".TrackStartingY"};

struct SourceLine {
  size_t offset;
  std::string_view text;
};

struct ParsedToken {
  int value;
  MinecartTrackSourceTokenSpan span;
};

struct ParsedSection {
  std::array<int, kMinecartTrackSlotCount> values{};
  MinecartTrackSourceDocument::FieldTokenSpans editable_spans{};
  bool guarded = false;
};

bool IsSpace(char value) {
  return std::isspace(static_cast<unsigned char>(value)) != 0;
}

bool IsHexDigit(char value) {
  return std::isxdigit(static_cast<unsigned char>(value)) != 0;
}

std::string_view Trim(std::string_view value) {
  while (!value.empty() && IsSpace(value.front())) {
    value.remove_prefix(1);
  }
  while (!value.empty() && IsSpace(value.back())) {
    value.remove_suffix(1);
  }
  return value;
}

std::string_view CodeForLine(std::string_view line) {
  const size_t comment = line.find(';');
  return Trim(line.substr(0, comment));
}

std::vector<SourceLine> SplitLines(std::string_view source) {
  std::vector<SourceLine> lines;
  size_t offset = 0;
  while (offset < source.size()) {
    const size_t newline = source.find('\n', offset);
    const size_t end =
        newline == std::string_view::npos ? source.size() : newline;
    lines.push_back({offset, source.substr(offset, end - offset)});
    if (newline == std::string_view::npos) {
      break;
    }
    offset = newline + 1;
  }
  return lines;
}

absl::StatusOr<std::vector<ParsedToken>> ParseDwLine(const SourceLine& line,
                                                     std::string_view label) {
  const size_t comment = line.text.find(';');
  const size_t code_end =
      comment == std::string_view::npos ? line.text.size() : comment;
  size_t cursor = 0;
  while (cursor < code_end && IsSpace(line.text[cursor])) {
    ++cursor;
  }
  if (cursor + 2 > code_end ||
      std::tolower(static_cast<unsigned char>(line.text[cursor])) != 'd' ||
      std::tolower(static_cast<unsigned char>(line.text[cursor + 1])) != 'w' ||
      cursor + 2 == code_end || !IsSpace(line.text[cursor + 2])) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Unexpected statement in %s: %s", label, CodeForLine(line.text)));
  }
  cursor += 2;

  std::vector<ParsedToken> tokens;
  bool requires_token = true;
  while (true) {
    while (cursor < code_end && IsSpace(line.text[cursor])) {
      ++cursor;
    }
    if (cursor == code_end) {
      if (requires_token) {
        return absl::InvalidArgumentError(
            absl::StrFormat("Empty operand in %s", label));
      }
      break;
    }

    const size_t token_start = cursor;
    if (line.text[cursor] != '$' || cursor + 5 > code_end ||
        !std::all_of(
            line.text.begin() + static_cast<std::ptrdiff_t>(cursor + 1),
            line.text.begin() + static_cast<std::ptrdiff_t>(cursor + 5),
            IsHexDigit)) {
      return absl::InvalidArgumentError(
          absl::StrFormat("%s values must use exact 16-bit $hhhh operands: %s",
                          label, CodeForLine(line.text)));
    }
    cursor += 5;
    if (cursor < code_end && IsHexDigit(line.text[cursor])) {
      return absl::InvalidArgumentError(
          absl::StrFormat("%s contains an over-wide 16-bit operand: %s", label,
                          CodeForLine(line.text)));
    }

    int value = 0;
    for (size_t digit = token_start + 1; digit < token_start + 5; ++digit) {
      const char hex = line.text[digit];
      value <<= 4;
      if (hex >= '0' && hex <= '9') {
        value += hex - '0';
      } else {
        value += 10 + std::tolower(static_cast<unsigned char>(hex)) - 'a';
      }
    }
    tokens.push_back({value, {line.offset + token_start, 5}});
    requires_token = false;

    while (cursor < code_end && IsSpace(line.text[cursor])) {
      ++cursor;
    }
    if (cursor == code_end) {
      break;
    }
    if (line.text[cursor] != ',') {
      return absl::InvalidArgumentError(
          absl::StrFormat("Unexpected operand separator in %s: %s", label,
                          CodeForLine(line.text)));
    }
    ++cursor;
    requires_token = true;
  }
  if (tokens.empty()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Empty dw statement in %s", label));
  }
  return tokens;
}

absl::StatusOr<ParsedSection> ParseSection(const std::vector<SourceLine>& lines,
                                           size_t first_line, size_t end_line,
                                           std::string_view label) {
  enum class GuardState { kPrefix, kEnabled, kDisabled, kComplete };
  GuardState state = GuardState::kPrefix;
  bool saw_guard = false;
  bool saw_else = false;
  bool saw_endif = false;
  std::vector<ParsedToken> prefix;
  std::vector<ParsedToken> enabled;
  std::vector<ParsedToken> disabled;

  for (size_t line_index = first_line + 1; line_index < end_line;
       ++line_index) {
    const std::string_view code = CodeForLine(lines[line_index].text);
    if (code.empty()) {
      continue;
    }
    if (code == kPlannedTrackGuard) {
      if (state != GuardState::kPrefix || saw_guard) {
        return absl::InvalidArgumentError(
            absl::StrFormat("Nested or repeated guard in %s", label));
      }
      saw_guard = true;
      state = GuardState::kEnabled;
      continue;
    }
    if (code == "else") {
      if (!saw_guard || state != GuardState::kEnabled || saw_else) {
        return absl::InvalidArgumentError(
            absl::StrFormat("Unexpected else in %s", label));
      }
      saw_else = true;
      state = GuardState::kDisabled;
      continue;
    }
    if (code == "endif") {
      if (!saw_else || state != GuardState::kDisabled || saw_endif) {
        return absl::InvalidArgumentError(
            absl::StrFormat("Unexpected endif in %s", label));
      }
      saw_endif = true;
      state = GuardState::kComplete;
      continue;
    }
    if (state == GuardState::kComplete) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Unexpected content after guarded table in %s: %s", label, code));
    }

    auto tokens_or = ParseDwLine(lines[line_index], label);
    if (!tokens_or.ok()) {
      return tokens_or.status();
    }
    std::vector<ParsedToken>* destination = &prefix;
    if (state == GuardState::kEnabled) {
      destination = &enabled;
    } else if (state == GuardState::kDisabled) {
      destination = &disabled;
    }
    destination->insert(destination->end(), tokens_or->begin(),
                        tokens_or->end());
  }

  ParsedSection result;
  std::vector<ParsedToken> active;
  if (!saw_guard) {
    if (prefix.size() != kMinecartTrackSlotCount) {
      return absl::InvalidArgumentError(
          absl::StrFormat("%s must contain exactly %zu values; found %zu",
                          label, kMinecartTrackSlotCount, prefix.size()));
    }
    active = std::move(prefix);
  } else {
    if (!saw_else || !saw_endif) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Incomplete planned-track guard in %s", label));
    }
    if (prefix.size() != 4 || enabled.size() != 28 || disabled.size() != 28) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "%s guarded layout requires exactly 4 prefix, 28 enabled, and 28 "
          "disabled values; found %zu, %zu, and %zu",
          label, prefix.size(), enabled.size(), disabled.size()));
    }
    active = std::move(prefix);
    active.insert(active.end(), enabled.begin(), enabled.end());
    result.guarded = true;
  }

  for (size_t index = 0; index < kMinecartTrackSlotCount; ++index) {
    result.values[index] = active[index].value;
    result.editable_spans[index] = active[index].span;
  }
  return result;
}

absl::Status ValidateTrack(const MinecartTrack& track, size_t index) {
  if (track.id != static_cast<int>(index)) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Minecart track ID at slot %zu must be %zu; found %d",
                        index, index, track.id));
  }
  for (const auto& [field_name, value] :
       std::array<std::pair<std::string_view, int>, 3>{
           {{"room_id", track.room_id},
            {"start_x", track.start_x},
            {"start_y", track.start_y}}}) {
    if (value < 0 || value > 0xFFFF) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Minecart track %zu %s must be a 16-bit value; found %d", index,
          field_name, value));
    }
  }
  return absl::OkStatus();
}

std::string FormatHexWord(int value) {
  return absl::StrFormat("$%04X", value);
}

}  // namespace

absl::StatusOr<MinecartTrackSourceDocument> MinecartTrackSourceDocument::Parse(
    std::string source_bytes) {
  const std::vector<SourceLine> lines = SplitLines(source_bytes);
  std::array<size_t, kSectionLabels.size()> section_lines{};
  for (size_t section = 0; section < kSectionLabels.size(); ++section) {
    int matches = 0;
    for (size_t line = 0; line < lines.size(); ++line) {
      if (CodeForLine(lines[line].text) == kSectionLabels[section]) {
        section_lines[section] = line;
        ++matches;
      }
    }
    if (matches != 1) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Expected exactly one %s label, found %d",
                          kSectionLabels[section], matches));
    }
    if (section > 0 && section_lines[section] <= section_lines[section - 1]) {
      return absl::InvalidArgumentError(
          "Minecart table sections must be ordered Rooms, X, then Y");
    }
  }

  std::array<ParsedSection, kSectionLabels.size()> parsed_sections;
  for (size_t section = 0; section < kSectionLabels.size(); ++section) {
    const size_t end_line = section + 1 < kSectionLabels.size()
                                ? section_lines[section + 1]
                                : lines.size();
    auto parsed_or = ParseSection(lines, section_lines[section], end_line,
                                  kSectionLabels[section]);
    if (!parsed_or.ok()) {
      return parsed_or.status();
    }
    parsed_sections[section] = std::move(*parsed_or);
  }
  if (parsed_sections[0].guarded != parsed_sections[1].guarded ||
      parsed_sections[0].guarded != parsed_sections[2].guarded) {
    return absl::InvalidArgumentError(
        "Minecart sections must use the same flat or guarded layout");
  }

  MinecartTrackSourceDocument document;
  document.source_bytes_ = std::move(source_bytes);
  document.guarded_ = parsed_sections[0].guarded;
  document.tracks_.reserve(kMinecartTrackSlotCount);
  for (size_t index = 0; index < kMinecartTrackSlotCount; ++index) {
    document.tracks_.push_back(
        {static_cast<int>(index), parsed_sections[0].values[index],
         parsed_sections[1].values[index], parsed_sections[2].values[index]});
  }
  for (size_t field = 0; field < parsed_sections.size(); ++field) {
    document.editable_token_spans_[field] =
        parsed_sections[field].editable_spans;
  }
  return document;
}

absl::StatusOr<std::string> MinecartTrackSourceDocument::Render(
    const std::vector<MinecartTrack>& tracks) const {
  if (tracks.size() != kMinecartTrackSlotCount) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Minecart source requires exactly %zu tracks; found %zu",
        kMinecartTrackSlotCount, tracks.size()));
  }
  for (size_t index = 0; index < tracks.size(); ++index) {
    const absl::Status status = ValidateTrack(tracks[index], index);
    if (!status.ok()) {
      return status;
    }
  }

  std::string rendered = source_bytes_;
  for (size_t index = 0; index < tracks.size(); ++index) {
    const std::array<int, 3> original = {
        tracks_[index].room_id, tracks_[index].start_x, tracks_[index].start_y};
    const std::array<int, 3> updated = {
        tracks[index].room_id, tracks[index].start_x, tracks[index].start_y};
    for (size_t field = 0; field < updated.size(); ++field) {
      if (updated[field] == original[field]) {
        continue;
      }
      const MinecartTrackSourceTokenSpan span =
          editable_token_spans_[field][index];
      if (span.length != 5 || span.offset + span.length > rendered.size() ||
          rendered[span.offset] != '$') {
        return absl::DataLossError(
            "Minecart source token span no longer matches the parsed bytes");
      }
      rendered.replace(span.offset, span.length, FormatHexWord(updated[field]));
    }
  }
  return rendered;
}

}  // namespace yaze::editor
