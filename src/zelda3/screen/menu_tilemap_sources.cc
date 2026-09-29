#include "zelda3/screen/menu_tilemap_sources.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "app/emu/debug/symbol_provider.h"
#include "app/gfx/types/snes_tile.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "util/macro.h"
#include "zelda3/game_data.h"

namespace yaze::zelda3 {
namespace {

// Both the ROM-symbol source and the raw-file source ultimately provide a
// flat sequence of SNES color words that starts at "CGRAM color 1" (the
// menu upload's +1 offset -- see menu_tilemap_sources.h). Given that
// sequence, sub-palette p / color c (c=1..3) is table[p*4 + c - 1] and
// color 0 of every sub-palette is always transparent (an SNES BG
// convention, independent of whatever happens to be stored at that CGRAM
// slot), so it doesn't matter what we put there.
std::array<gfx::SnesColor, 32> BuildSubpaletteArray(
    const std::vector<uint16_t>& table_words) {
  std::array<gfx::SnesColor, 32> out{};
  for (int i = 0; i < 32; ++i) {
    if (i % 4 == 0 || i - 1 >= static_cast<int>(table_words.size())) {
      out[i] = gfx::SnesColor();  // transparent slot; value is irrelevant
      continue;
    }
    out[i] = gfx::SnesColor(table_words[i - 1]);
  }
  return out;
}

uint16_t ReadWordLe(const uint8_t* p) {
  return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

// menu_palette.asm defines exactly 31 entries (8 sub-palettes x 3 colors +
// the unused final slot); accept up to 32 so a 32-word table also parses.
constexpr size_t kMenuPaletteMinEntries = 31;
constexpr size_t kMenuPaletteMaxEntries = 32;

// asar-style number: $HEX, 0xHEX or decimal. nullopt if it isn't one.
std::optional<uint32_t> ParseAsmNumber(std::string_view s) {
  s = absl::StripAsciiWhitespace(s);
  if (s.empty())
    return std::nullopt;
  uint64_t value = 0;
  if (s.front() == '$') {
    s.remove_prefix(1);
    if (s.empty() || !absl::SimpleHexAtoi(s, &value))
      return std::nullopt;
  } else if (absl::StartsWithIgnoreCase(s, "0x")) {
    s.remove_prefix(2);
    if (s.empty() || !absl::SimpleHexAtoi(s, &value))
      return std::nullopt;
  } else if (!absl::SimpleAtoi(s, &value)) {
    return std::nullopt;
  }
  if (value > 0xFFFFFFFFull)
    return std::nullopt;
  return static_cast<uint32_t>(value);
}

// One operand of a `dw`: hexto555($RRGGBB) or a raw 16-bit word.
absl::StatusOr<uint16_t> ParseDwOperand(std::string_view op) {
  op = absl::StripAsciiWhitespace(op);
  if (op.empty()) {
    return absl::InvalidArgumentError("empty dw operand (stray comma?)");
  }
  constexpr std::string_view kMacro = "hexto555";
  if (absl::StartsWithIgnoreCase(op, kMacro)) {
    std::string_view rest =
        absl::StripAsciiWhitespace(op.substr(kMacro.size()));
    if (rest.size() < 2 || rest.front() != '(' || rest.back() != ')') {
      return absl::InvalidArgumentError(absl::StrFormat(
          "expected hexto555($RRGGBB), got '%s'", std::string(op)));
    }
    std::string_view inner = rest.substr(1, rest.size() - 2);
    std::optional<uint32_t> rgb = ParseAsmNumber(inner);
    if (!rgb.has_value() || *rgb > 0xFFFFFF) {
      return absl::InvalidArgumentError(
          absl::StrFormat("hexto555 argument '%s' is not a $RRGGBB value",
                          std::string(absl::StripAsciiWhitespace(inner))));
    }
    return Hexto555(*rgb);
  }
  std::optional<uint32_t> word = ParseAsmNumber(op);
  if (!word.has_value()) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "unsupported dw operand '%s' (expected hexto555($RRGGBB) or a "
        "$XXXX word)",
        std::string(op)));
  }
  if (*word > 0xFFFF) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "dw operand '%s' does not fit in 16 bits", std::string(op)));
  }
  return static_cast<uint16_t>(*word);
}

bool IsDwDirective(std::string_view line, std::string_view* operands) {
  size_t end = line.find_first_of(" \t");
  std::string_view token = line.substr(0, end);
  if (!absl::EqualsIgnoreCase(token, "dw"))
    return false;
  *operands =
      end == std::string_view::npos ? std::string_view() : line.substr(end);
  return true;
}

}  // namespace

uint16_t Hexto555(uint32_t rgb) {
  const uint32_t r = (rgb >> 16) & 0xFF;
  const uint32_t g = (rgb >> 8) & 0xFF;
  const uint32_t b = rgb & 0xFF;
  return static_cast<uint16_t>(((b / 8) << 10) | ((g / 8) << 5) | (r / 8));
}

absl::StatusOr<std::vector<uint16_t>> ParseMenuPaletteAsmText(
    std::string_view text, std::string_view label) {
  std::vector<uint16_t> words;
  bool in_table = false;
  int label_line = 0;
  int last_table_line = 0;
  int line_no = 0;
  bool done = false;

  for (std::string_view raw : absl::StrSplit(text, '\n')) {
    ++line_no;
    // Strip the `;` comment, then whitespace (this also drops a CRLF's \r).
    std::string_view line =
        absl::StripAsciiWhitespace(raw.substr(0, raw.find(';')));
    if (line.empty())
      continue;

    if (!in_table) {
      size_t end = line.find_first_of(": \t");
      if (line.substr(0, end) != label)
        continue;
      in_table = true;
      label_line = line_no;
      std::string_view rest =
          end == std::string_view::npos ? std::string_view() : line.substr(end);
      if (!rest.empty() && rest.front() == ':')
        rest.remove_prefix(1);
      line = absl::StripAsciiWhitespace(rest);
      if (line.empty())
        continue;  // `Menu_Palette:` alone on its line
    }

    std::string_view operands;
    if (!IsDwDirective(line, &operands)) {
      break;  // first non-`dw` line ends the table
    }
    last_table_line = line_no;
    for (std::string_view op : absl::StrSplit(operands, ',')) {
      absl::StatusOr<uint16_t> word = ParseDwOperand(op);
      if (!word.ok()) {
        return absl::InvalidArgumentError(
            absl::StrFormat("line %d: %s", line_no, word.status().message()));
      }
      words.push_back(*word);
      if (words.size() >= kMenuPaletteMaxEntries) {
        done = true;
        break;
      }
    }
    if (done)
      break;
  }

  if (!in_table) {
    return absl::NotFoundError(
        absl::StrFormat("label '%s' not found", std::string(label)));
  }
  if (words.size() < kMenuPaletteMinEntries) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "line %d: '%s' has only %zu dw entries; at least %zu are needed "
        "(8 sub-palettes x 3 colors + the unused final slot)",
        last_table_line > 0 ? last_table_line : label_line, std::string(label),
        words.size(), kMenuPaletteMinEntries));
  }
  return words;
}

absl::StatusOr<std::array<gfx::SnesColor, 32>> ResolveMenuPaletteFromAsm(
    const std::string& path, const std::string& label) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return absl::NotFoundError(
        absl::StrFormat("could not open '%s' for reading", path));
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  const std::string text = buffer.str();

  absl::StatusOr<std::vector<uint16_t>> words =
      ParseMenuPaletteAsmText(text, label);
  constexpr std::string_view kSymbolPrefix = "Oracle_";
  if (!words.ok() && absl::IsNotFound(words.status()) &&
      absl::StartsWith(label, kSymbolPrefix)) {
    words = ParseMenuPaletteAsmText(text, label.substr(kSymbolPrefix.size()));
  }
  if (!words.ok()) {
    return absl::Status(words.status().code(),
                        absl::StrCat(path, ": ", words.status().message()));
  }
  return BuildSubpaletteArray(*words);
}

std::optional<std::string> FindMenuPaletteAsm(const std::string& project_dir,
                                              const std::string& code_folder) {
  namespace fs = std::filesystem;
  std::vector<fs::path> candidates;
  if (!project_dir.empty()) {
    candidates.push_back(fs::path(project_dir) / "Menu" / "menu_palette.asm");
  }
  if (!code_folder.empty()) {
    fs::path code(code_folder);
    if (code.is_absolute()) {
      candidates.push_back(code / "Menu" / "menu_palette.asm");
    } else if (!project_dir.empty()) {
      candidates.push_back(fs::path(project_dir) / code / "Menu" /
                           "menu_palette.asm");
    }
  }
  for (const fs::path& candidate : candidates) {
    std::error_code ec;
    if (fs::is_regular_file(candidate, ec) && !ec) {
      return candidate.string();
    }
  }
  return std::nullopt;
}

absl::StatusOr<std::array<gfx::SnesColor, 32>> ResolveMenuPaletteFromSymbol(
    const Rom& rom, const emu::debug::SymbolProvider& symbols,
    const std::string& label) {
  std::optional<emu::debug::Symbol> symbol = symbols.FindSymbol(label);
  if (!symbol.has_value()) {
    return absl::NotFoundError(absl::StrFormat(
        "symbol '%s' not found in the loaded symbol table", label));
  }
  uint32_t pc = SnesToPc(symbol->address);
  // Table is 32 words = 64 bytes, matching Oracle's Menu_Palette.
  ASSIGN_OR_RETURN(std::vector<uint8_t> raw, rom.ReadByteVector(pc, 64));
  if (raw.size() < 64) {
    return absl::OutOfRangeError(absl::StrFormat(
        "symbol '%s' resolved to PC offset 0x%06X, which is too close to "
        "the end of the ROM (%zu bytes) to hold a 32-word palette table",
        label, pc, rom.size()));
  }
  std::vector<uint16_t> table_words(32);
  bool all_zero = true;
  for (int i = 0; i < 32; ++i) {
    table_words[i] = ReadWordLe(&raw[i * 2]);
    if (table_words[i] != 0)
      all_zero = false;
  }
  if (all_zero) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "symbol '%s' resolved to PC offset 0x%06X, but every byte there is "
        "0. For Oracle of Secrets, labels like this are produced by "
        "assembling the ASM sources and only exist in the *patched* ROM "
        "(oos168x.sfc) -- the 'clean' base ROM a yaze project normally "
        "points at (oos168.sfc) does not contain this data. Point this "
        "source at the patched ROM, or use the HUD-palette or CGRAM-dump "
        "source instead.",
        label, pc));
  }
  return BuildSubpaletteArray(table_words);
}

absl::StatusOr<std::array<gfx::SnesColor, 32>> ResolveMenuPaletteFromHud(
    const GameData& game_data) {
  if (game_data.palette_groups.hud.empty()) {
    return absl::FailedPreconditionError(
        "GameData::palette_groups.hud is empty; load a ROM/GameData first");
  }
  std::vector<uint16_t> table_words(32);
  for (int i = 0; i < 32; ++i) {
    // hud[0] has up to 32 colors already (see snes_palette.cc); colors
    // past the group's actual size come back as SnesColor() == 0, which
    // is harmless (transparent slots don't care, and a short HUD group
    // simply yields black for palettes it doesn't define).
    gfx::SnesColor color = game_data.palette_groups.hud.GetColor(0, i);
    table_words[i] = color.snes();
  }
  return BuildSubpaletteArray(table_words);
}

absl::StatusOr<std::array<gfx::SnesColor, 32>> ResolveMenuPaletteFromFile(
    const std::string& path, size_t byte_offset) {
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in) {
    return absl::NotFoundError(
        absl::StrFormat("could not open '%s' for reading", path));
  }
  std::streamsize size = in.tellg();
  if (size < 0 ||
      static_cast<size_t>(size) < byte_offset + 62 /* 31 words min */) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "'%s' is too small (%lld bytes) to hold 31 colors starting at byte "
        "offset %zu",
        path, static_cast<long long>(size), byte_offset));
  }
  in.seekg(static_cast<std::streamoff>(byte_offset), std::ios::beg);
  std::vector<uint8_t> raw(64);
  size_t available = static_cast<size_t>(size) - byte_offset;
  size_t to_read = std::min<size_t>(64, available);
  if (!in.read(reinterpret_cast<char*>(raw.data()),
               static_cast<std::streamsize>(to_read))) {
    return absl::InternalError(absl::StrFormat("short read on '%s'", path));
  }
  std::vector<uint16_t> table_words(31, 0);
  for (size_t i = 0; i < table_words.size() && (i * 2 + 1) < to_read; ++i) {
    table_words[i] = ReadWordLe(&raw[i * 2]);
  }
  return BuildSubpaletteArray(table_words);
}

absl::StatusOr<std::vector<uint8_t>> ResolveMenuChrFromRom(const Rom& rom) {
  return Load2BppGraphics(rom);
}

absl::StatusOr<std::vector<uint8_t>> ResolveMenuChrFromFile(
    const std::string& path) {
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in) {
    return absl::NotFoundError(
        absl::StrFormat("could not open '%s' for reading", path));
  }
  std::streamsize size = in.tellg();
  if (size <= 0) {
    return absl::InvalidArgumentError(absl::StrFormat("'%s' is empty", path));
  }
  in.seekg(0, std::ios::beg);
  std::vector<uint8_t> raw(static_cast<size_t>(size));
  if (!in.read(reinterpret_cast<char*>(raw.data()), size)) {
    return absl::InternalError(absl::StrFormat("short read on '%s'", path));
  }
  if (raw.size() % 16 != 0) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "'%s' is %zu bytes, not a multiple of 16 (one 2bpp 8x8 tile is 16 "
        "bytes)",
        path, raw.size()));
  }
  // Decode in 0x800-byte (128-tile) chunks, the same chunking
  // Load2BppGraphics() uses, so MakeChrPixelFn() works uniformly over
  // either source. A short final chunk is padded with zero tiles.
  constexpr size_t kChunkBytes = 0x800;
  size_t num_chunks = (raw.size() + kChunkBytes - 1) / kChunkBytes;
  std::vector<uint8_t> out;
  out.reserve(num_chunks * 8192);
  for (size_t c = 0; c < num_chunks; ++c) {
    size_t begin = c * kChunkBytes;
    size_t end = std::min(raw.size(), begin + kChunkBytes);
    std::vector<uint8_t> chunk(raw.begin() + begin, raw.begin() + end);
    chunk.resize(kChunkBytes, 0);
    auto converted = gfx::SnesTo8bppSheet(chunk, 2);
    out.insert(out.end(), converted.begin(), converted.end());
  }
  return out;
}

}  // namespace yaze::zelda3
