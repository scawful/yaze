#include "cli/handlers/game/overworld_copy_writer_commands.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/ascii.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "rom/lyra_layout_descriptor.h"
#include "rom/rom.h"
#include "util/hex.h"
#include "util/macro.h"
#include "zelda3/overworld/overworld.h"

namespace yaze {
namespace cli {
namespace handlers {

namespace {

// Tile16 quadrant word: vhopppcc cccccccc (v/h flip, o priority, ppp palette,
// 10-bit char). Only the char bits are replaced.
constexpr uint16_t kCharMask = 0x03FF;
constexpr uint16_t kKeepMask = 0xFC00;
constexpr int kPaletteShift = 10;
constexpr int kPaletteCount = 8;
constexpr int kCharCount = 0x400;
constexpr size_t kTile16Bytes = 8;
constexpr uint16_t kLyraTile16TableId = 400;

absl::StatusOr<uint16_t> ParseChar(absl::string_view text,
                                   absl::string_view rule) {
  int value = 0;
  if (!::yaze::util::ParseHexString(text, &value) || value < 0 ||
      value >= kCharCount) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "--map '%s': char '%s' must be hex 000-3FF", rule, text));
  }
  return static_cast<uint16_t>(value);
}

std::string RuleText(const Tile16CharRemapRule& rule) {
  std::vector<std::string> chars;
  chars.reserve(rule.chars.size());
  for (uint16_t c : rule.chars)
    chars.push_back(absl::StrFormat("%03X", c));
  return absl::StrFormat("%d:%s=%03X", rule.palette, absl::StrJoin(chars, ","),
                         rule.new_char);
}

std::string Hex(uint64_t value, int width) {
  return absl::StrFormat("0x%0*X", width, value);
}

uint16_t ReadWord(const std::vector<uint8_t>& image, size_t pc) {
  return static_cast<uint16_t>(image[pc] | (image[pc + 1] << 8));
}

resources::CommandInvocationContext ContextFromRom(const Rom* rom) {
  resources::CommandInvocationContext context;
  if (rom != nullptr && !rom->filename().empty()) {
    context.source_rom_path = std::filesystem::path(rom->filename());
    context.active_rom_path = std::filesystem::path(rom->filename());
  }
  return context;
}

std::optional<std::string> OutputArg(const resources::ArgumentParser& parser) {
  if (auto out = parser.GetString("out"); out.has_value())
    return out;
  return parser.GetString("output");
}

// Writes header + image to a sibling .partial file, renames it over `target`
// and compares a fresh read with the intended bytes. Neither path may alias
// the source or active ROM of this invocation.
absl::Status WriteImageCopy(
    const Rom& rom, const std::vector<uint8_t>& image,
    const std::filesystem::path& target,
    const resources::CommandInvocationContext& invocation_context) {
  std::filesystem::path partial = target;
  partial += ".partial";
  RETURN_IF_ERROR(
      resources::RejectArtifactRomAliases("--out", target, invocation_context));
  RETURN_IF_ERROR(resources::RejectArtifactRomAliases("--out", partial,
                                                      invocation_context));
  std::vector<uint8_t> bytes = rom.copier_header();
  bytes.insert(bytes.end(), image.begin(), image.end());
  std::error_code ec;
  {
    std::ofstream out(partial, std::ios::binary | std::ios::trunc);
    if (!out) {
      return absl::PermissionDeniedError(
          absl::StrFormat("Cannot open %s for writing", partial.string()));
    }
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    out.close();
    if (!out) {
      std::filesystem::remove(partial, ec);
      return absl::DataLossError(
          absl::StrFormat("Write failed for %s", partial.string()));
    }
  }
  std::filesystem::rename(partial, target, ec);
  if (ec) {
    std::error_code remove_ec;
    std::filesystem::remove(partial, remove_ec);
    return absl::InternalError(absl::StrFormat("Cannot move %s to %s: %s",
                                               partial.string(),
                                               target.string(), ec.message()));
  }
  std::ifstream in(target, std::ios::binary);
  const std::vector<uint8_t> readback{std::istreambuf_iterator<char>(in),
                                      std::istreambuf_iterator<char>()};
  if (readback != bytes) {
    return absl::DataLossError(absl::StrFormat(
        "Readback of %s does not match the staged image", target.string()));
  }
  return absl::OkStatus();
}

void AddCrcEntry(resources::OutputFormatter& formatter,
                 const rom::LyraCrcEntry& entry) {
  formatter.BeginObject();
  formatter.AddField("index", static_cast<int>(entry.index));
  formatter.AddField("id", static_cast<int>(entry.id));
  formatter.AddField("pc", Hex(entry.pc_address, 6));
  formatter.AddField("length", static_cast<int>(entry.length));
  formatter.AddField("stored", Hex(entry.stored, 8));
  formatter.AddField("computed", Hex(entry.computed, 8));
  formatter.EndObject();
}

void AddCrcReport(resources::OutputFormatter& formatter,
                  const rom::LyraCrcReport& report) {
  formatter.AddField("revision", static_cast<int>(report.revision));
  formatter.AddField("entry_count", static_cast<int>(report.entries.size()));
  formatter.AddField("stale", report.stale());
  formatter.BeginObject("descriptor");
  formatter.AddField("pc", std::string("0x147000"));
  formatter.AddField("stored", Hex(report.descriptor_stored, 8));
  formatter.AddField("computed", Hex(report.descriptor_computed, 8));
  formatter.AddField("stale", report.descriptor_stale());
  formatter.EndObject();
  formatter.AddField("stale_entry_count",
                     static_cast<int>(report.stale_entry_count()));
  formatter.BeginArray("stale_entries");
  for (const auto& entry : report.entries) {
    if (entry.stale())
      AddCrcEntry(formatter, entry);
  }
  formatter.EndArray();
}

}  // namespace

// ---------------------------------------------------------------------------
// Pure tile16 remap
// ---------------------------------------------------------------------------

absl::StatusOr<Tile16CharRemapRule> ParseTile16CharRemapRule(
    const std::string& text) {
  const size_t colon = text.find(':');
  const size_t equals = text.find('=');
  if (colon == std::string::npos || equals == std::string::npos ||
      equals < colon || text.find(':', colon + 1) != std::string::npos ||
      text.find('=', equals + 1) != std::string::npos) {
    return absl::InvalidArgumentError(
        absl::StrFormat("--map '%s' must be PAL:CHAR[,CHAR..]=NEWCHAR", text));
  }
  Tile16CharRemapRule rule;
  int palette = -1;
  const absl::string_view palette_text =
      absl::StripAsciiWhitespace(absl::string_view(text).substr(0, colon));
  if (!absl::SimpleAtoi(palette_text, &palette) || palette < 0 ||
      palette >= kPaletteCount) {
    return absl::InvalidArgumentError(
        absl::StrFormat("--map '%s': palette must be 0-7", text));
  }
  rule.palette = static_cast<uint8_t>(palette);
  const absl::string_view chars_text =
      absl::string_view(text).substr(colon + 1, equals - colon - 1);
  for (absl::string_view part : absl::StrSplit(chars_text, ',')) {
    part = absl::StripAsciiWhitespace(part);
    ASSIGN_OR_RETURN(const uint16_t c, ParseChar(part, text));
    rule.chars.push_back(c);
  }
  ASSIGN_OR_RETURN(rule.new_char,
                   ParseChar(absl::string_view(text).substr(equals + 1), text));
  return rule;
}

absl::StatusOr<std::pair<int, int>> ParseTile16Range(const std::string& text,
                                                     int count) {
  const std::vector<std::string> parts = absl::StrSplit(text, '-');
  int first = -1;
  int last = -1;
  if (parts.empty() || parts.size() > 2 ||
      !::yaze::util::ParseHexString(parts.front(), &first) ||
      !::yaze::util::ParseHexString(parts.back(), &last) || first > last ||
      last >= count) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "--tiles '%s' must be hex A-B within 0-%X", text, count - 1));
  }
  return std::make_pair(first, last);
}

absl::StatusOr<std::vector<Tile16CharRemapChange>> RemapTile16Chars(
    std::vector<uint8_t>& image, size_t table_pc, int first, int last,
    const std::vector<Tile16CharRemapRule>& rules) {
  if (first < 0 || last < first) {
    return absl::InvalidArgumentError("Tile16 range is empty or negative");
  }
  const size_t end = table_pc + (static_cast<size_t>(last) + 1) * kTile16Bytes;
  if (table_pc > image.size() || end > image.size()) {
    return absl::OutOfRangeError(absl::StrFormat(
        "Tile16 table 0x%06X..0x%06X lies outside the %d-byte image", table_pc,
        end, image.size()));
  }
  std::array<int, kPaletteCount * kCharCount> target;
  target.fill(-1);
  for (const auto& rule : rules) {
    if (rule.palette >= kPaletteCount || rule.new_char >= kCharCount) {
      return absl::InvalidArgumentError(
          "Remap rule outside palette/char range");
    }
    for (uint16_t c : rule.chars) {
      if (c >= kCharCount) {
        return absl::InvalidArgumentError("Remap rule char outside 000-3FF");
      }
      int& slot = target[rule.palette * kCharCount + c];
      if (slot >= 0 && slot != rule.new_char) {
        return absl::InvalidArgumentError(absl::StrFormat(
            "Remap rules overlap: palette %d char %03X maps to both %03X and "
            "%03X",
            rule.palette, c, slot, rule.new_char));
      }
      slot = rule.new_char;
    }
  }
  std::vector<Tile16CharRemapChange> changes;
  for (int tile = first; tile <= last; ++tile) {
    const size_t base = table_pc + static_cast<size_t>(tile) * kTile16Bytes;
    Tile16CharRemapChange change;
    change.tile16 = tile;
    bool changed = false;
    for (size_t quadrant = 0; quadrant < 4; ++quadrant) {
      const uint16_t word = ReadWord(image, base + quadrant * 2);
      const int palette = (word >> kPaletteShift) & (kPaletteCount - 1);
      const int to = target[palette * kCharCount + (word & kCharMask)];
      const uint16_t next =
          to < 0 ? word : static_cast<uint16_t>((word & kKeepMask) | to);
      change.old_words[quadrant] = word;
      change.new_words[quadrant] = next;
      changed = changed || next != word;
    }
    if (!changed)
      continue;
    for (size_t quadrant = 0; quadrant < 4; ++quadrant) {
      image[base + quadrant * 2] =
          static_cast<uint8_t>(change.new_words[quadrant]);
      image[base + quadrant * 2 + 1] =
          static_cast<uint8_t>(change.new_words[quadrant] >> 8);
    }
    changes.push_back(change);
  }
  return changes;
}

// ---------------------------------------------------------------------------
// overworld-tile16-remap
// ---------------------------------------------------------------------------

resources::CommandHandler::Descriptor
OverworldTile16RemapCommandHandler::Describe() const {
  Descriptor descriptor;
  descriptor.display_name = "Overworld Tile16 Char Remap";
  descriptor.summary =
      "Replace the char of matching tile16 quadrant words (palette + char "
      "rules), keeping flip, priority and palette. Dry run by default; "
      "--out writes a copy, never the input. Lyra images keep valid CRCs.";
  descriptor.todo_reference = "todo#oracle-z3ed-behavior";
  descriptor.entries = {
      {"--map", "Repeatable rule PAL:CHAR[,CHAR..]=NEWCHAR (hex chars)", ""},
      {"--tiles", "Optional hex tile16 range A-B (default: whole table)", ""},
      {"--out", "Write the remapped image to this new file", ""},
  };
  return descriptor;
}

absl::Status OverworldTile16RemapCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  const auto maps = parser.GetStrings("map");
  if (maps.empty()) {
    return absl::InvalidArgumentError(
        "At least one --map=PAL:CHAR[,CHAR..]=NEWCHAR is required");
  }
  for (const auto& map : maps) {
    RETURN_IF_ERROR(ParseTile16CharRemapRule(map).status());
  }
  if (const auto tiles = parser.GetString("tiles"); tiles.has_value()) {
    RETURN_IF_ERROR(ParseTile16Range(*tiles, zelda3::NumberOfMap16Ex).status());
  }
  if (const auto out = OutputArg(parser); out.has_value() && out->empty()) {
    return absl::InvalidArgumentError("--out must name a file");
  }
  return absl::OkStatus();
}

absl::Status OverworldTile16RemapCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  return ExecuteWithContext(rom, parser, formatter, ContextFromRom(rom));
}

absl::Status OverworldTile16RemapCommandHandler::ExecuteWithContext(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter,
    const resources::CommandInvocationContext& invocation_context) {
  RETURN_IF_ERROR(ValidateArgs(parser));
  if (rom == nullptr || !rom->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  std::vector<Tile16CharRemapRule> rules;
  for (const auto& map : parser.GetStrings("map")) {
    ASSIGN_OR_RETURN(auto rule, ParseTile16CharRemapRule(map));
    rules.push_back(std::move(rule));
  }
  std::optional<std::filesystem::path> output;
  if (const auto out = OutputArg(parser); out.has_value()) {
    ASSIGN_OR_RETURN(output, resources::ResolveStableArtifactPath(*out));
    RETURN_IF_ERROR(resources::RejectArtifactRomAliases("--out", *output,
                                                        invocation_context));
  }

  // Session attribution: an image loaded as Lyra stays protected even if its
  // descriptor was damaged in memory (Rom::InspectLyraLayout reports kInvalid).
  const auto layout = rom->InspectLyraLayout();
  if (layout.presence == rom::LyraLayoutPresence::kInvalid) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Lyra layout is invalid (%s). Run `z3ed lyra-crc-check --rom=<image>`; "
        "if only CRCs are stale, `z3ed lyra-crc-refresh --rom=<image> "
        "--out=<copy>` writes a copy to remap instead.",
        layout.error));
  }
  const bool lyra = layout.presence == rom::LyraLayoutPresence::kValid;

  size_t table_pc = 0;
  int tile_count = zelda3::NumberOfMap16Ex;
  std::string table_source;
  if (rom::IsNativeSpecialOverworldLayout(layout)) {
    // The descriptor owns table addresses; ROM settings are not consulted.
    const auto entry =
        std::find_if(layout.entries.begin(), layout.entries.end(),
                     [](const auto& e) { return e.id == kLyraTile16TableId; });
    if (entry == layout.entries.end() || entry->stride != kTile16Bytes) {
      return absl::FailedPreconditionError(
          "Lyra directory has no 8-byte tile16 table (entry 400)");
    }
    table_pc = entry->pc_address;
    tile_count = entry->count;
    table_source = "lyra_directory_entry_400";
  } else {
    const auto profile = zelda3::DetectOverworldRomProfile(*rom);
    if (!profile.has_expanded_tile16) {
      return absl::FailedPreconditionError(
          "overworld-tile16-remap needs expanded tile16 definitions "
          "(ZSCustomOverworld v3 or the expanded tile16 flag)");
    }
    table_pc = static_cast<size_t>(zelda3::GetMap16TilesExpanded());
    table_source = "expanded_map16";
  }
  int first = 0;
  int last = tile_count - 1;
  if (const auto tiles = parser.GetString("tiles"); tiles.has_value()) {
    ASSIGN_OR_RETURN(const auto range, ParseTile16Range(*tiles, tile_count));
    first = range.first;
    last = range.second;
  }

  std::vector<uint8_t> staged = rom->vector();
  ASSIGN_OR_RETURN(const auto changes,
                   RemapTile16Chars(staged, table_pc, first, last, rules));

  std::vector<rom::LyraCrcEntry> refreshed;
  if (lyra) {
    // The input passed the strict validator, so every stale CRC now comes
    // from the remap. Refresh through the shared writer and prove it.
    rom::LyraCrcReport before;
    ASSIGN_OR_RETURN(staged, rom::RefreshLyraLayoutCrcs(staged, &before));
    const size_t table_end =
        table_pc + static_cast<size_t>(tile_count) * kTile16Bytes;
    for (const auto& entry : before.entries) {
      if (!entry.stale())
        continue;
      if (entry.pc_address >= table_end ||
          entry.pc_address + entry.length <= table_pc) {
        return absl::InternalError(absl::StrFormat(
            "Remap staled Lyra entry %d outside the tile16 table", entry.id));
      }
      refreshed.push_back(entry);
    }
  }

  bool verified = false;
  if (output.has_value()) {
    RETURN_IF_ERROR(WriteImageCopy(*rom, staged, *output, invocation_context));
    verified = true;
  }

  formatter.AddField("mode", std::string(output ? "copy" : "dry-run"));
  formatter.AddField("tile16_table_pc", Hex(table_pc, 6));
  formatter.AddField("tile16_table_source", table_source);
  formatter.AddField("tile_range",
                     absl::StrFormat("0x%03X-0x%03X", first, last));
  formatter.BeginArray("rules");
  for (const auto& rule : rules)
    formatter.AddArrayItem(RuleText(rule));
  formatter.EndArray();
  formatter.AddField("lyra_layout", std::string(lyra ? "valid" : "absent"));
  formatter.AddField("changed_tile16_count", static_cast<int>(changes.size()));
  formatter.BeginArray("changes");
  for (const auto& change : changes) {
    formatter.BeginObject();
    formatter.AddField("tile16", Hex(change.tile16, 3));
    formatter.BeginArray("old");
    for (uint16_t word : change.old_words)
      formatter.AddArrayItem(Hex(word, 4));
    formatter.EndArray();
    formatter.BeginArray("new");
    for (uint16_t word : change.new_words)
      formatter.AddArrayItem(Hex(word, 4));
    formatter.EndArray();
    formatter.EndObject();
  }
  formatter.EndArray();
  formatter.BeginArray("lyra_crc_refreshed");
  for (const auto& entry : refreshed)
    AddCrcEntry(formatter, entry);
  formatter.EndArray();
  if (output.has_value()) {
    formatter.AddField("output", output->string());
    formatter.AddField("verified_reload", verified);
  }
  return absl::OkStatus();
}

// ---------------------------------------------------------------------------
// lyra-crc-check
// ---------------------------------------------------------------------------

resources::CommandHandler::Descriptor LyraCrcCheckCommandHandler::Describe()
    const {
  Descriptor descriptor;
  descriptor.display_name = "Lyra CRC Check";
  descriptor.summary =
      "List stale OOSOW192 directory entry CRCs and the descriptor CRC. "
      "Nonzero exit when any CRC is stale.";
  descriptor.todo_reference = "todo#oracle-z3ed-behavior";
  return descriptor;
}

absl::Status LyraCrcCheckCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser&,
    resources::OutputFormatter& formatter) {
  if (rom == nullptr || !rom->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  const auto& image = rom->vector();
  ASSIGN_OR_RETURN(const auto report,
                   rom::CheckLyraLayoutCrcs(image.data(), image.size()));
  AddCrcReport(formatter, report);
  if (report.stale()) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Lyra CRCs are stale: %d entr%s, descriptor %s. `z3ed "
        "lyra-crc-refresh --rom=<image> --out=<copy>` writes a refreshed copy.",
        report.stale_entry_count(),
        report.stale_entry_count() == 1 ? "y" : "ies",
        report.descriptor_stale() ? "stale" : "current"));
  }
  return absl::OkStatus();
}

// ---------------------------------------------------------------------------
// lyra-crc-refresh
// ---------------------------------------------------------------------------

resources::CommandHandler::Descriptor LyraCrcRefreshCommandHandler::Describe()
    const {
  Descriptor descriptor;
  descriptor.display_name = "Lyra CRC Refresh";
  descriptor.summary =
      "Write a copy with recomputed OOSOW192 entry CRCs and descriptor CRC. "
      "Only those CRC fields change; the input is never written.";
  descriptor.todo_reference = "todo#oracle-z3ed-behavior";
  descriptor.entries = {
      {"--out", "Required: new file for the refreshed image", ""},
  };
  return descriptor;
}

absl::Status LyraCrcRefreshCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  const auto out = OutputArg(parser);
  if (!out.has_value() || out->empty()) {
    return absl::InvalidArgumentError(
        "--out=<copy.sfc> is required; lyra-crc-refresh never writes in place");
  }
  return absl::OkStatus();
}

absl::Status LyraCrcRefreshCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  return ExecuteWithContext(rom, parser, formatter, ContextFromRom(rom));
}

absl::Status LyraCrcRefreshCommandHandler::ExecuteWithContext(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter,
    const resources::CommandInvocationContext& invocation_context) {
  RETURN_IF_ERROR(ValidateArgs(parser));
  if (rom == nullptr || !rom->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  ASSIGN_OR_RETURN(const auto output,
                   resources::ResolveStableArtifactPath(*OutputArg(parser)));
  RETURN_IF_ERROR(
      resources::RejectArtifactRomAliases("--out", output, invocation_context));
  rom::LyraCrcReport before;
  ASSIGN_OR_RETURN(const auto staged,
                   rom::RefreshLyraLayoutCrcs(rom->vector(), &before));
  ASSIGN_OR_RETURN(const auto after,
                   rom::CheckLyraLayoutCrcs(staged.data(), staged.size()));
  if (after.stale()) {
    return absl::InternalError("Refreshed Lyra image still has stale CRCs");
  }
  RETURN_IF_ERROR(WriteImageCopy(*rom, staged, output, invocation_context));

  formatter.AddField("mode", std::string("refresh"));
  formatter.AddField("unchanged", !before.stale());
  formatter.BeginObject("before");
  AddCrcReport(formatter, before);
  formatter.EndObject();
  formatter.AddField("descriptor_crc_after", Hex(after.descriptor_stored, 8));
  formatter.AddField("output", output.string());
  formatter.AddField("verified_reload", true);
  return absl::OkStatus();
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
