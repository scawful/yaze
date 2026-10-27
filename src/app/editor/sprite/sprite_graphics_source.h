#ifndef YAZE_APP_EDITOR_SPRITE_SPRITE_GRAPHICS_SOURCE_H_
#define YAZE_APP_EDITOR_SPRITE_SPRITE_GRAPHICS_SOURCE_H_

#include <filesystem>
#include <fstream>
#include <span>
#include <vector>

#include "absl/status/statusor.h"
#include "app/gfx/types/snes_tile.h"
#include "core/sprite_asset.h"
#include "rom/rom.h"
#include "zelda3/game_data.h"

namespace yaze::editor::sprite_graphics_source {

// Uses the common per-tile decoder: its exact byte requirement avoids partial
// sheet padding and preserves all four bitplanes for custom sprite graphics.
inline absl::StatusOr<std::vector<uint8_t>> Decode(
    std::span<const uint8_t> bytes, int bpp) {
  if (bpp < 2 || bpp > 4 || bytes.size() != static_cast<size_t>(64 * 8 * bpp))
    return absl::InvalidArgumentError(
        "A sprite graphics slot needs 64 complete 2bpp, 3bpp, or 4bpp tiles");
  std::vector<uint8_t> pixels(4096, 0);
  for (int tile = 0; tile < 64; ++tile) {
    const auto decoded = gfx::UnpackBppTile(bytes, tile * 8 * bpp, bpp);
    for (int y = 0; y < 8; ++y)
      for (int x = 0; x < 8; ++x)
        pixels[((tile / 16) * 8 + y) * 128 + (tile % 16) * 8 + x] =
            decoded.data[y * 8 + x];
  }
  return pixels;
}

// Reads at most one slot (2048 bytes). No source is written or allocated here.
inline absl::StatusOr<std::vector<uint8_t>> Resolve(
    const project::SpriteGraphicsSource& source, uint16_t sheet,
    const zelda3::GameData* data, const Rom* rom,
    const std::filesystem::path& project_directory) {
  if (source.kind == "sheet") {
    if (!data || !data->sheet_store.HasSheet(sheet))
      return absl::FailedPreconditionError(
          "Sprite graphics sheet is unavailable in this ROM session");
    const auto pixels = data->sheet_store.Sheet(sheet);
    return std::vector<uint8_t>(pixels.begin(), pixels.end());
  }
  if (source.bpp < 2 || source.bpp > 4)
    return absl::InvalidArgumentError(
        "Sprite graphics depth must be 2, 3, or 4");
  const size_t length = 64 * 8 * source.bpp;
  if (source.kind == "raw_rom") {
    if (!rom || !rom->is_loaded())
      return absl::FailedPreconditionError("Load a ROM for raw ROM graphics");
    if (source.offset > rom->size() || length > rom->size() - source.offset)
      return absl::OutOfRangeError(
          "Sprite graphics range extends past the ROM");
    return Decode(
        std::span<const uint8_t>(rom->vector().data() + source.offset, length),
        source.bpp);
  }
  if (source.kind != "bin" || source.path.empty() ||
      source.path.find('\0') != std::string::npos)
    return absl::InvalidArgumentError("Select a BIN path or raw ROM source");
  auto path = std::filesystem::path(source.path);
  if (path.is_relative()) {
    if (project_directory.empty())
      return absl::FailedPreconditionError(
          "Save a project or use an absolute BIN path");
    path = project_directory / path;
  }
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path, ec))
    return absl::NotFoundError(
        "Sprite graphics BIN is missing or unreadable: " + path.string());
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input.is_open())
    return absl::PermissionDeniedError("Cannot open sprite graphics BIN: " +
                                       path.string());
  const auto end = input.tellg();
  if (end < 0 || static_cast<uint64_t>(end) < source.offset ||
      length > static_cast<uint64_t>(end) - source.offset)
    return absl::OutOfRangeError("Sprite graphics range extends past the BIN");
  std::vector<uint8_t> bytes(length);
  input.seekg(source.offset);
  if (!input.read(reinterpret_cast<char*>(bytes.data()), bytes.size()))
    return absl::DataLossError("Could not read complete sprite graphics slot");
  return Decode(bytes, source.bpp);
}

}  // namespace yaze::editor::sprite_graphics_source

#endif  // YAZE_APP_EDITOR_SPRITE_SPRITE_GRAPHICS_SOURCE_H_
