#include "zelda3/screen/menu_tilemap_sources.h"

#include <algorithm>
#include <fstream>

#include "absl/strings/str_format.h"
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

}  // namespace

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
