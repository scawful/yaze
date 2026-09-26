#include "zelda3/gfx_sheet_png.h"

#include <algorithm>
#include <map>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "app/gfx/types/snes_tile.h"
#include "util/macro.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/game_data.h"
#include "zelda3/gfx_sheet_inventory.h"
#include "zelda3/gfx_sheet_storage.h"

namespace yaze::zelda3 {
namespace {

constexpr int kSheetWidth = 128;
constexpr int kSheetHeight = 32;
constexpr int kBlockSize = 16;

std::pair<int, int> BlockOrigin(int block) {
  return {(block % 8) * kBlockSize, (block / 8) * kBlockSize};
}

SheetColor ToSheetColor(const SDL_Color& color) {
  return {color.r, color.g, color.b};
}

absl::StatusOr<Room> LoadRoomWithPalettes(Rom& rom, GameData& data, int room) {
  if (room < 0 || room >= kNumberOfRooms) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Room 0x%X is out of range", room));
  }
  if (data.palette_groups.dungeon_main.size() == 0) {
    return absl::FailedPreconditionError("Dungeon palettes are not loaded");
  }
  Room loaded = LoadRoomHeaderFromRom(&rom, room);
  loaded.SetGameData(&data);
  return loaded;
}

}  // namespace

SheetPalette GrayscaleSheetPalette() {
  SheetPalette palette;
  for (int i = 0; i < 8; ++i) {
    const auto level = static_cast<uint8_t>(i * 255 / 7);
    palette[i] = {level, level, level};
  }
  return palette;
}

absl::StatusOr<SheetPalette> RoomBackgroundSheetPalette(Rom& rom,
                                                        GameData& data,
                                                        int room, int row) {
  if (row < 0 || row > 7) {
    return absl::InvalidArgumentError("Background palette row must be 0-7");
  }
  ASSIGN_OR_RETURN(Room loaded, LoadRoomWithPalettes(rom, data, room));
  const int palette_id = loaded.ResolveDungeonPaletteId();
  const auto group = BuildDungeonRenderPaletteGroupFromGameData(
      data.palette_groups.dungeon_main.palette_ref(palette_id), &data);
  if (row >= static_cast<int>(group.size()) ||
      group.palette_ref(row).size() < 8) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Room 0x%X has no background palette row %d", room, row));
  }
  SheetPalette palette;
  for (int i = 0; i < 8; ++i) {
    const auto rgb = group.palette_ref(row)[i].rgb();
    palette[i] = {static_cast<uint8_t>(rgb.x), static_cast<uint8_t>(rgb.y),
                  static_cast<uint8_t>(rgb.z)};
  }
  return palette;
}

absl::StatusOr<SheetPalette> RoomSpriteSheetPalette(Rom& rom, GameData& data,
                                                    int room, int row) {
  if (row < 0 || row > 7) {
    return absl::InvalidArgumentError("Sprite palette row must be 0-7");
  }
  ASSIGN_OR_RETURN(Room loaded, LoadRoomWithPalettes(rom, data, room));
  const auto cgram = BuildDungeonSpriteRenderPalette(loaded, &data);
  SheetPalette palette;
  for (int i = 0; i < 8; ++i) {
    palette[i] = ToSheetColor(cgram[(8 + row) * 16 + i]);
  }
  return palette;
}

absl::StatusOr<std::vector<uint8_t>> ExportSheetBlocksPng(
    const std::vector<uint8_t>& snes_3bpp, const SheetPalette& palette,
    int first_block, int block_count) {
  if (snes_3bpp.size() != kGfxSheet3bppBytes) {
    return absl::InvalidArgumentError("A 3bpp sheet is 0x600 bytes");
  }
  if (first_block < 0 || block_count <= 0 ||
      first_block + block_count > kGfxSheetBlockCount) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Blocks %d-%d are outside the sheet's 16 blocks",
                        first_block, first_block + block_count - 1));
  }
  const auto sheet = gfx::SnesTo8bppSheet(snes_3bpp, 3);
  const int columns = std::min(block_count, 8);
  const int rows = (block_count + 7) / 8;
  const int width = columns * kBlockSize;
  const int height = rows * kBlockSize;
  std::vector<uint8_t> indices(static_cast<size_t>(width) * height, 0);
  for (int k = 0; k < block_count; ++k) {
    const auto [sx, sy] = BlockOrigin(first_block + k);
    const int dx = (k % 8) * kBlockSize;
    const int dy = (k / 8) * kBlockSize;
    for (int y = 0; y < kBlockSize; ++y) {
      for (int x = 0; x < kBlockSize; ++x) {
        indices[(dy + y) * width + dx + x] =
            sheet[(sy + y) * kSheetWidth + sx + x] & 0x07;
      }
    }
  }
  std::vector<std::array<uint8_t, 4>> png_palette;
  for (int i = 0; i < 8; ++i) {
    png_palette.push_back({palette[i][0], palette[i][1], palette[i][2],
                           static_cast<uint8_t>(i == 0 ? 0 : 255)});
  }
  return util::EncodeIndexedPng(width, height, indices, png_palette);
}

absl::StatusOr<SheetImportResult> ImportSheetBlocksPng(
    const std::vector<uint8_t>& snes_3bpp, const util::PngImage& png,
    int first_block, const SheetPalette& palette) {
  if (snes_3bpp.size() != kGfxSheet3bppBytes) {
    return absl::InvalidArgumentError("A 3bpp sheet is 0x600 bytes");
  }
  if (png.width % kBlockSize != 0 || png.height % kBlockSize != 0) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "PNG is %dx%d; width and height must be multiples of 16", png.width,
        png.height));
  }
  const int columns = png.width / kBlockSize;
  const int block_count = columns * (png.height / kBlockSize);
  if (first_block < 0 || first_block + block_count > kGfxSheetBlockCount) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "The PNG holds %d 16x16 blocks; starting at block %d it runs past "
        "block 15",
        block_count, first_block));
  }

  // Resolve every pixel to a 3bpp index before touching the sheet.
  std::vector<uint8_t> values(static_cast<size_t>(png.width) * png.height);
  std::vector<std::string> offenders;
  int bad_pixels = 0;
  for (int y = 0; y < png.height; ++y) {
    for (int x = 0; x < png.width; ++x) {
      const size_t i = static_cast<size_t>(y) * png.width + x;
      int value = -1;
      std::string seen;
      if (png.indexed) {
        value = png.indices[i];
        seen = absl::StrFormat("index %d", value);
        if (value > 7) {
          value = -1;
        }
      } else {
        const uint8_t* p = &png.rgba[i * 4];
        seen = absl::StrFormat("#%02X%02X%02X", p[0], p[1], p[2]);
        if (p[3] == 0) {
          value = 0;
        } else if (p[3] == 255) {
          for (int c = 0; c < 8; ++c) {
            if (palette[c][0] == p[0] && palette[c][1] == p[1] &&
                palette[c][2] == p[2]) {
              value = c;
              break;
            }
          }
        }
      }
      if (value < 0) {
        ++bad_pixels;
        if (offenders.size() < 8) {
          offenders.push_back(absl::StrFormat("(%d,%d) %s", x, y, seen));
        }
        continue;
      }
      values[i] = static_cast<uint8_t>(value);
    }
  }
  if (bad_pixels > 0) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "%d pixel(s) are not 3bpp indices 0-7 of the sheet palette; nothing "
        "was imported. First: %s",
        bad_pixels, absl::StrJoin(offenders, ", ")));
  }

  const auto old_sheet = gfx::SnesTo8bppSheet(snes_3bpp, 3);
  std::vector<uint8_t> sheet = old_sheet;
  for (int k = 0; k < block_count; ++k) {
    const auto [dx, dy] = BlockOrigin(first_block + k);
    const int sx = (k % columns) * kBlockSize;
    const int sy = (k / columns) * kBlockSize;
    for (int y = 0; y < kBlockSize; ++y) {
      for (int x = 0; x < kBlockSize; ++x) {
        sheet[(dy + y) * kSheetWidth + dx + x] =
            values[(sy + y) * png.width + sx + x];
      }
    }
  }

  SheetImportResult result;
  result.snes_3bpp = gfx::IndexedToSnesSheet(sheet, 3);
  result.snes_3bpp.resize(kGfxSheet3bppBytes);
  for (int block = 0; block < kGfxSheetBlockCount; ++block) {
    const auto [bx, by] = BlockOrigin(block);
    bool changed = false;
    for (int y = 0; y < kBlockSize && !changed; ++y) {
      for (int x = 0; x < kBlockSize; ++x) {
        const size_t i = (by + y) * kSheetWidth + bx + x;
        if ((sheet[i] & 7) != (old_sheet[i] & 7)) {
          changed = true;
          break;
        }
      }
    }
    if (changed) {
      result.changed_blocks.push_back(block);
    }
  }
  for (int tile = 0; tile < 64; ++tile) {
    if (!std::equal(result.snes_3bpp.begin() + tile * 24,
                    result.snes_3bpp.begin() + tile * 24 + 24,
                    snes_3bpp.begin() + tile * 24)) {
      result.changed_tiles.push_back(tile);
    }
  }
  for (size_t i = 0; i < snes_3bpp.size();) {
    if (result.snes_3bpp[i] == snes_3bpp[i]) {
      ++i;
      continue;
    }
    size_t end = i;
    while (end < snes_3bpp.size() && result.snes_3bpp[end] != snes_3bpp[end]) {
      ++end;
    }
    result.changed_ranges.emplace_back(static_cast<int>(i),
                                       static_cast<int>(end));
    i = end;
  }
  return result;
}

RoomBackgroundSet ResolveRoomBackgroundSet(const GameData& data, int room,
                                           uint8_t header_blockset) {
  RoomBackgroundSet set;
  set.room = room;
  set.header_blockset = header_blockset;
  const size_t mains = data.main_blockset_ids.size();
  const uint8_t entrance_main =
      room >= 0 && room < static_cast<int>(data.room_default_entrances.size())
          ? data.room_default_entrances[room].main_blockset
          : 0xFF;
  if (entrance_main != 0xFF && entrance_main < mains) {
    set.main_blockset = entrance_main;
  } else if (header_blockset < mains) {
    set.main_blockset = header_blockset;
  }
  for (int i = 0; i < 8; ++i) {
    set.sheets[i] = data.main_blockset_ids[set.main_blockset][i];
    if (i >= 3 && i <= 6 && header_blockset < data.room_blockset_ids.size()) {
      const uint8_t room_sheet = data.room_blockset_ids[header_blockset][i - 3];
      if (room_sheet != 0) {
        set.sheets[i] = room_sheet;
        set.from_room_blockset[i] = true;
      }
    }
  }
  return set;
}

std::vector<RoomBackgroundSet> ResolveAllRoomBackgroundSets(
    const Rom& rom, const GameData& data) {
  std::vector<RoomBackgroundSet> sets;
  for (const auto& [room, info] : CollectRoomGfx(rom)) {
    sets.push_back(ResolveRoomBackgroundSet(data, room, info.blockset));
  }
  return sets;
}

absl::StatusOr<std::vector<uint8_t>> ExportRoomBackgroundPng(
    const Rom& rom, const RoomBackgroundSet& set, const SheetPalette& palette) {
  constexpr int kHeight = kSheetHeight * 8;
  std::vector<uint8_t> indices(static_cast<size_t>(kSheetWidth) * kHeight, 0);
  for (int slot = 0; slot < 8; ++slot) {
    const uint16_t sheet = set.sheets[slot];
    if (sheet >= kGfxSheetCount ||
        GetGfxSheetStorageKind(sheet) == GfxSheetStorageKind::kCompressed2bpp) {
      continue;
    }
    ASSIGN_OR_RETURN(const auto data, ReadGfxSheetData(rom, sheet));
    const auto pixels = gfx::SnesTo8bppSheet(data, 3);
    for (int y = 0; y < kSheetHeight; ++y) {
      for (int x = 0; x < kSheetWidth; ++x) {
        indices[(slot * kSheetHeight + y) * kSheetWidth + x] =
            pixels[y * kSheetWidth + x] & 0x07;
      }
    }
  }
  std::vector<std::array<uint8_t, 4>> png_palette;
  for (int i = 0; i < 8; ++i) {
    png_palette.push_back({palette[i][0], palette[i][1], palette[i][2],
                           static_cast<uint8_t>(i == 0 ? 0 : 255)});
  }
  return util::EncodeIndexedPng(kSheetWidth, kHeight, indices, png_palette);
}

absl::StatusOr<std::vector<RoomSheetImport>> ImportRoomBackgroundPng(
    const Rom& rom, const RoomBackgroundSet& set, const util::PngImage& png,
    const SheetPalette& palette) {
  if (png.width != kSheetWidth || png.height != kSheetHeight * 8) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "A room background PNG is 128x256 (8 stacked sheets); got %dx%d",
        png.width, png.height));
  }
  std::vector<RoomSheetImport> imports;
  std::map<uint16_t, std::pair<int, std::vector<uint8_t>>> seen;
  for (int slot = 0; slot < 8; ++slot) {
    const uint16_t sheet = set.sheets[slot];
    // Crop this slot's 128x32 band.
    util::PngImage band;
    band.width = kSheetWidth;
    band.height = kSheetHeight;
    band.indexed = png.indexed;
    band.palette = png.palette;
    const size_t begin = static_cast<size_t>(slot) * kSheetHeight * kSheetWidth;
    const size_t count = static_cast<size_t>(kSheetHeight) * kSheetWidth;
    if (png.indexed) {
      band.indices.assign(png.indices.begin() + begin,
                          png.indices.begin() + begin + count);
    } else {
      band.rgba.assign(png.rgba.begin() + begin * 4,
                       png.rgba.begin() + (begin + count) * 4);
    }

    const bool writable =
        sheet < kGfxSheetCount &&
        GetGfxSheetStorageKind(sheet) != GfxSheetStorageKind::kCompressed2bpp;
    std::vector<uint8_t> current(kGfxSheet3bppBytes, 0);
    if (writable) {
      ASSIGN_OR_RETURN(current, ReadGfxSheetData(rom, sheet));
    }
    auto result = ImportSheetBlocksPng(current, band, 0, palette);
    if (!result.ok()) {
      return absl::Status(result.status().code(),
                          absl::StrFormat("Slot %d (sheet 0x%02X): %s", slot,
                                          sheet, result.status().message()));
    }
    if (!writable) {
      if (!result->changed_blocks.empty()) {
        return absl::FailedPreconditionError(absl::StrFormat(
            "Slot %d shows 2bpp sheet 0x%02X, which cannot be edited here",
            slot, sheet));
      }
      continue;
    }
    // Every slot showing the same sheet must end up with the same pixels,
    // including slots left unedited.
    auto first = seen.find(sheet);
    if (first != seen.end()) {
      if (first->second.second != result->snes_3bpp) {
        return absl::InvalidArgumentError(absl::StrFormat(
            "Sheet 0x%02X appears in slots %d and %d with different pixels",
            sheet, first->second.first, slot));
      }
    } else {
      seen.emplace(sheet, std::make_pair(slot, result->snes_3bpp));
    }
    if (result->changed_blocks.empty()) {
      continue;
    }
    auto existing = std::find_if(
        imports.begin(), imports.end(),
        [sheet](const RoomSheetImport& other) { return other.sheet == sheet; });
    if (existing != imports.end()) {
      existing->slots.push_back(slot);
      continue;
    }
    imports.push_back({sheet, {slot}, std::move(*result)});
  }
  return imports;
}

}  // namespace yaze::zelda3
