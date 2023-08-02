#include "rom.h"

#include <SDL.h>
// #include <asar/src/asar/interface-lib.h>

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "app/core/common.h"
#include "app/core/constants.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/compression.h"
#include "app/gfx/snes_tile.h"

namespace yaze {
namespace app {

using gfx::lc_lz2::CompressionPiece;
using gfx::lc_lz2::kCommandDirectCopy;
using gfx::lc_lz2::kCommandMod;
using gfx::lc_lz2::kSnesByteMax;

namespace {

int GetGraphicsAddress(const uchar* data, uint8_t offset) {
  auto part_one = data[kOverworldGraphicsPos1 + offset] << 16;
  auto part_two = data[kOverworldGraphicsPos2 + offset] << 8;
  auto part_three = data[kOverworldGraphicsPos3 + offset];
  auto snes_addr = (part_one | part_two | part_three);
  return core::SnesToPc(snes_addr);
}

}  // namespace

// TODO TEST compressed data border for each cmd
absl::StatusOr<Bytes> ROM::Compress(const int start, const int length, int mode,
                                    bool check) {
  if (length == 0) {
    return Bytes();
  }
  // Worse case should be a copy of the string with extended header
  auto compressed_chain = std::make_shared<CompressionPiece>(1, 1, "aaa", 2);
  auto compressed_chain_start = compressed_chain;

  gfx::lc_lz2::CommandArgumentArray cmd_args = {{}};
  gfx::lc_lz2::DataSizeArray data_size_taken = {0, 0, 0, 0, 0};
  gfx::lc_lz2::CommandSizeArray cmd_size = {0, 1, 2, 1, 2};

  uint src_data_pos = start;
  uint last_pos = start + length - 1;
  uint comp_accumulator = 0;  // Used when skipping using copy

  while (true) {
    data_size_taken.fill({});
    cmd_args.fill({{}});

    gfx::lc_lz2::CheckByteRepeat(rom_data_.data(), data_size_taken, cmd_args,
                                 src_data_pos, last_pos);
    gfx::lc_lz2::CheckWordRepeat(rom_data_.data(), data_size_taken, cmd_args,
                                 src_data_pos, last_pos);
    gfx::lc_lz2::CheckIncByte(rom_data_.data(), data_size_taken, cmd_args,
                              src_data_pos, last_pos);
    gfx::lc_lz2::CheckIntraCopy(rom_data_.data(), data_size_taken, cmd_args,
                                src_data_pos, last_pos, start);

    uint max_win = 2;
    uint cmd_with_max = kCommandDirectCopy;
    gfx::lc_lz2::ValidateForByteGain(data_size_taken, cmd_size, max_win,
                                     cmd_with_max);

    if (cmd_with_max == kCommandDirectCopy) {
      // This is the worst case scenario
      // Progress through the next byte, in case there's a different
      // compression command we can implement before we hit 32 bytes.
      src_data_pos++;
      comp_accumulator++;

      // Arbitrary choice to do a 32 bytes grouping for copy.
      if (comp_accumulator == 32 || src_data_pos > last_pos) {
        std::string buffer;
        for (int i = 0; i < comp_accumulator; ++i) {
          buffer.push_back(rom_data_[i + src_data_pos - comp_accumulator]);
        }
        auto new_comp_piece = std::make_shared<CompressionPiece>(
            kCommandDirectCopy, comp_accumulator, buffer, comp_accumulator);
        compressed_chain->next = new_comp_piece;
        compressed_chain = new_comp_piece;
        comp_accumulator = 0;
      }
    } else {
      gfx::lc_lz2::CompressionCommandAlternative(
          rom_data_.data(), compressed_chain, cmd_size, cmd_args, src_data_pos,
          comp_accumulator, cmd_with_max, max_win);
    }

    if (src_data_pos > last_pos) {
      printf("Breaking compression loop\n");
      break;
    }

    if (check) {
      RETURN_IF_ERROR(gfx::lc_lz2::ValidateCompressionResult(
          compressed_chain_start, mode, start, src_data_pos))
    }
  }

  // Skipping compression chain header
  gfx::lc_lz2::MergeCopy(compressed_chain_start->next);
  gfx::lc_lz2::PrintCompressionChain(compressed_chain_start);
  return gfx::lc_lz2::CreateCompressionString(compressed_chain_start->next,
                                              mode);
}

absl::StatusOr<Bytes> ROM::CompressGraphics(const int pos, const int length) {
  return Compress(pos, length, gfx::lc_lz2::kNintendoMode2);
}

absl::StatusOr<Bytes> ROM::CompressOverworld(const int pos, const int length) {
  return Compress(pos, length, gfx::lc_lz2::kNintendoMode1);
}

// ============================================================================

absl::StatusOr<Bytes> ROM::Decompress(int offset, int size, int mode) {
  if (size == 0) {
    return Bytes();
  }

  Bytes buffer(size, 0);
  uint length = 0;
  uint buffer_pos = 0;
  uchar command = 0;
  uchar header = rom_data_[offset];

  while (header != kSnesByteMax) {
    if ((header & gfx::lc_lz2::kExpandedMod) == gfx::lc_lz2::kExpandedMod) {
      // Expanded Command
      command = ((header >> 2) & kCommandMod);
      length = (((header << 8) | rom_data_[offset + 1]) &
                gfx::lc_lz2::kExpandedLengthMod);
      offset += 2;  // Advance 2 bytes in ROM
    } else {
      // Normal Command
      command = ((header >> 5) & kCommandMod);
      length = (header & gfx::lc_lz2::kNormalLengthMod);
      offset += 1;  // Advance 1 byte in ROM
    }
    length += 1;  // each commands is at least of size 1 even if index 00

    switch (command) {
      case gfx::lc_lz2::kCommandDirectCopy:  // Does not advance in the ROM
        memcpy(buffer.data() + buffer_pos, rom_data_.data() + offset, length);
        buffer_pos += length;
        offset += length;
        break;
      case gfx::lc_lz2::kCommandByteFill:
        memset(buffer.data() + buffer_pos, (int)(rom_data_[offset]), length);
        buffer_pos += length;
        offset += 1;  // Advances 1 byte in the ROM
        break;
      case gfx::lc_lz2::kCommandWordFill: {
        auto a = rom_data_[offset];
        auto b = rom_data_[offset + 1];
        for (int i = 0; i < length; i = i + 2) {
          buffer[buffer_pos + i] = a;
          if ((i + 1) < length) buffer[buffer_pos + i + 1] = b;
        }
        buffer_pos += length;
        offset += 2;  // Advance 2 byte in the ROM
      } break;
      case gfx::lc_lz2::kCommandIncreasingFill: {
        auto inc_byte = rom_data_[offset];
        for (int i = 0; i < length; i++) {
          buffer[buffer_pos] = inc_byte++;
          buffer_pos++;
        }
        offset += 1;  // Advance 1 byte in the ROM
      } break;
      case gfx::lc_lz2::kCommandRepeatingBytes: {
        ushort s1 = ((rom_data_[offset + 1] & kSnesByteMax) << 8);
        ushort s2 = (rom_data_[offset] & kSnesByteMax);
        int addr = (s1 | s2);
        if (mode == gfx::lc_lz2::kNintendoMode1) {  // Reversed byte order for
                                                    // overworld maps
          addr = (rom_data_[offset + 1] & kSnesByteMax) |
                 ((rom_data_[offset] & kSnesByteMax) << 8);
        }
        if (addr > offset) {
          return absl::InternalError(absl::StrFormat(
              "Decompress: Offset for command copy exceeds current position "
              "(Offset : %#04x | Pos : %#06x)\n",
              addr, offset));
        }
        if (buffer_pos + length >= size) {
          size *= 2;
          buffer.resize(size);
        }
        memcpy(buffer.data() + buffer_pos, buffer.data() + addr, length);
        buffer_pos += length;
        offset += 2;
      } break;
      default: {
        std::cout << absl::StrFormat(
            "Decompress: Invalid header (Offset : %#06x, Command: %#04x)\n",
            offset, command);
      } break;
    }
    // check next byte
    header = rom_data_[offset];
  }

  return buffer;
}

absl::StatusOr<Bytes> ROM::DecompressGraphics(int pos, int size) {
  return Decompress(pos, size, gfx::lc_lz2::kNintendoMode2);
}

absl::StatusOr<Bytes> ROM::DecompressOverworld(int pos, int size) {
  return Decompress(pos, size, gfx::lc_lz2::kNintendoMode1);
}

// ============================================================================

absl::StatusOr<Bytes> ROM::Load2bppGraphics() {
  Bytes sheet;
  const uint8_t sheets[] = {113, 114, 218, 219, 220, 221};

  for (const auto& sheet_id : sheets) {
    auto offset = GetGraphicsAddress(data(), sheet_id);
    ASSIGN_OR_RETURN(auto decomp_sheet,
                     gfx::lc_lz2::DecompressV2(data(), offset))
    auto converted_sheet = gfx::SnesTo8bppSheet(decomp_sheet, 2);
    for (const auto& each_pixel : converted_sheet) {
      sheet.push_back(each_pixel);
    }
  }
  return sheet;
}

// ============================================================================

// 0-112 -> compressed 3bpp bgr -> (decompressed each) 0x600 chars
// 113-114 -> compressed 2bpp -> (decompressed each) 0x800 chars
// 115-126 -> uncompressed 3bpp sprites -> (each) 0x600 chars
// 127-217 -> compressed 3bpp sprites -> (decompressed each) 0x600 chars
// 218-222 -> compressed 2bpp -> (decompressed each) 0x800 chars
absl::Status ROM::LoadAllGraphicsData() {
  Bytes sheet;
  bool bpp3 = false;

  for (int i = 0; i < core::NumberOfSheets; i++) {
    if (i >= 115 && i <= 126) {  // uncompressed sheets
      sheet.resize(core::Uncompressed3BPPSize);
      auto offset = GetGraphicsAddress(data(), i);
      for (int j = 0; j < core::Uncompressed3BPPSize; j++) {
        sheet[j] = rom_data_[j + offset];
      }
      bpp3 = true;
    } else if (i == 113 || i == 114 || i >= 218) {
      bpp3 = false;
    } else {
      auto offset = GetGraphicsAddress(data(), i);
      ASSIGN_OR_RETURN(sheet, gfx::lc_lz2::DecompressV2(data(), offset))
      bpp3 = true;
    }

    if (bpp3) {
      auto converted_sheet = gfx::SnesTo8bppSheet(sheet, 3);
      graphics_bin_[i] =
          gfx::Bitmap(core::kTilesheetWidth, core::kTilesheetHeight,
                      core::kTilesheetDepth, converted_sheet.data(), 0x1000);
      graphics_bin_.at(i).CreateTexture(renderer_);

      for (int j = 0; j < graphics_bin_.at(i).size(); ++j) {
        graphics_buffer_.push_back(graphics_bin_.at(i).at(j));
      }
    } else {
      for (int j = 0; j < graphics_bin_.at(0).size(); ++j) {
        graphics_buffer_.push_back(0xFF);
      }
    }
  }
  return absl::OkStatus();
}

// ============================================================================

absl::Status ROM::LoadFromFile(const absl::string_view& filename,
                               bool z3_load) {
  filename_ = filename;
  std::ifstream file(filename.data(), std::ios::binary);
  if (!file.is_open()) {
    return absl::InternalError(
        absl::StrCat("Could not open ROM file: ", filename));
  }

  bool has_header = false;
  int header_count = 0x200;
  size_ = std::filesystem::file_size(filename);
  rom_data_.resize(size_);
  for (auto i = 0; i < size_; ++i) {
    char byte_to_read = ' ';
    file.read(&byte_to_read, sizeof(char));
    if (byte_to_read == 0x00) {
      has_header = true;
    }
    rom_data_[i] = byte_to_read;
  }

  file.close();
  if (z3_load) {
    // copy ROM title
    memcpy(title_, rom_data_.data() + kTitleStringOffset, kTitleStringLength);
    LoadAllPalettes();
  }
  is_loaded_ = true;
  return absl::OkStatus();
}

// ============================================================================

absl::Status ROM::LoadFromPointer(uchar* data, size_t length) {
  if (!data)
    return absl::InvalidArgumentError(
        "Could not load ROM: parameter `data` is empty.");

  for (int i = 0; i < length; ++i) rom_data_.push_back(data[i]);

  return absl::OkStatus();
}

// ============================================================================

absl::Status ROM::LoadFromBytes(const Bytes& data) {
  if (data.empty()) {
    return absl::InvalidArgumentError(
        "Could not load ROM: parameter `data` is empty.");
  }
  rom_data_ = data;
  size_ = data.size();
  is_loaded_ = true;
  return absl::OkStatus();
}

// ============================================================================

void ROM::LoadAllPalettes() {
  // 35 colors each, 7x5 (0,2 on grid)
  for (int i = 0; i < 6; i++) {
    palette_groups_["ow_main"].AddPalette(
        ReadPalette(core::overworldPaletteMain + (i * (35 * 2)), 35));
  }
  // 21 colors each, 7x3 (8,2 and 8,5 on grid)
  for (int i = 0; i < 20; i++) {
    palette_groups_["ow_aux"].AddPalette(
        ReadPalette(core::overworldPaletteAuxialiary + (i * (21 * 2)), 21));
  }
  // 7 colors each 7x1 (0,7 on grid)
  for (int i = 0; i < 14; i++) {
    palette_groups_["ow_animated"].AddPalette(
        ReadPalette(core::overworldPaletteAnimated + (i * (7 * 2)), 7));
  }
  // 32 colors each 16x2 (0,0 on grid)
  for (int i = 0; i < 2; i++) {
    palette_groups_["hud"].AddPalette(
        ReadPalette(core::hudPalettes + (i * 64), 32));
  }

  palette_groups_["global_sprites"].AddPalette(
      ReadPalette(core::globalSpritePalettesLW, 60));
  palette_groups_["global_sprites"].AddPalette(
      ReadPalette(core::globalSpritePalettesDW, 60));

  for (int i = 0; i < 5; i++) {
    palette_groups_["armors"].AddPalette(
        ReadPalette(core::armorPalettes + (i * 30), 15));
  }
  for (int i = 0; i < 4; i++) {
    palette_groups_["swords"].AddPalette(
        ReadPalette(core::swordPalettes + (i * 6), 3));
  }
  for (int i = 0; i < 3; i++) {
    palette_groups_["shields"].AddPalette(
        ReadPalette(core::shieldPalettes + (i * 8), 4));
  }
  for (int i = 0; i < 12; i++) {
    palette_groups_["sprites_aux1"].AddPalette(
        ReadPalette(core::spritePalettesAux1 + (i * 14), 7));
  }
  for (int i = 0; i < 11; i++) {
    palette_groups_["sprites_aux2"].AddPalette(
        ReadPalette(core::spritePalettesAux2 + (i * 14), 7));
  }
  for (int i = 0; i < 24; i++) {
    palette_groups_["sprites_aux3"].AddPalette(
        ReadPalette(core::spritePalettesAux3 + (i * 14), 7));
  }
  for (int i = 0; i < 20; i++) {
    palette_groups_["dungeon_main"].AddPalette(
        ReadPalette(core::dungeonMainPalettes + (i * 180), 90));
  }

  palette_groups_["grass"].AddColor(ReadColor(core::hardcodedGrassLW));
  palette_groups_["grass"].AddColor(ReadColor(core::hardcodedGrassDW));
  palette_groups_["grass"].AddColor(ReadColor(core::hardcodedGrassSpecial));

  palette_groups_["3d_object"].AddPalette(
      ReadPalette(core::triforcePalette, 8));
  palette_groups_["3d_object"].AddPalette(ReadPalette(core::crystalPalette, 8));

  for (int i = 0; i < 2; i++) {
    palette_groups_["ow_mini_map"].AddPalette(
        ReadPalette(core::overworldMiniMapPalettes + (i * 256), 128));
  }
}

// ============================================================================

absl::Status ROM::UpdatePaletteColor(const std::string& groupName,
                                     size_t paletteIndex, size_t colorIndex,
                                     const gfx::SNESColor& newColor) {
  // Check if the groupName exists in the palette_groups_ map
  if (palette_groups_.find(groupName) != palette_groups_.end()) {
    // Check if the paletteIndex is within the range of available palettes in
    // the group
    if (paletteIndex < palette_groups_[groupName].size()) {
      // Check if the colorIndex is within the range of available colors in the
      // palette
      if (colorIndex < palette_groups_[groupName][paletteIndex].size()) {
        // Update the color value in the palette
        palette_groups_[groupName][paletteIndex][colorIndex] = newColor;
      } else {
        return absl::AbortedError(
            "Error: Invalid color index in UpdatePaletteColor.");
      }
    } else {
      return absl::AbortedError(
          "Error: Invalid palette index in UpdatePaletteColor.");
    }
  } else {
    return absl::AbortedError(
        "Error: Invalid group name in UpdatePaletteColor");
  }
  return absl::OkStatus();
}

// ============================================================================

void ROM::SaveAllPalettes() {
  // Iterate through all palette_groups_
  for (auto& [groupName, palettes] : palette_groups_) {
    // Iterate through all palettes in the group
    for (size_t i = 0; i < palettes.size(); ++i) {
      auto palette = palettes[i];

      // Iterate through all colors in the palette
      for (size_t j = 0; j < palette.size(); ++j) {
        gfx::SNESColor color = palette[j];
        // If the color is modified, save the color to the ROM
        if (color.modified) {
          WriteColor(GetPaletteAddress(groupName, i, j), color);
          color.modified = false;  // Reset the modified flag after saving
        }
      }
    }
  }
}

// ============================================================================

absl::Status ROM::SaveToFile(bool backup, absl::string_view filename) {
  if (rom_data_.empty()) {
    return absl::InternalError("ROM data is empty.");
  }

  // Check if filename is empty
  // If it is, use the filename_ member variable
  if (filename == "") {
    filename = filename_;
  }

  // Check if backup is enabled
  if (backup) {
    // Create a backup file with timestamp in its name
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::string backup_filename =
        absl::StrCat(filename, "_backup_", std::ctime(&now_c));

    // Remove newline character from ctime()
    backup_filename.erase(
        std::remove(backup_filename.begin(), backup_filename.end(), '\n'),
        backup_filename.end());

    // Replace spaces with underscores
    std::replace(backup_filename.begin(), backup_filename.end(), ' ', '_');

    // Now, copy the original file to the backup file
    std::filesystem::copy(filename, backup_filename,
                          std::filesystem::copy_options::overwrite_existing);
  }

  // Run the other save functions
  SaveAllPalettes();

  // Open the file that we know exists for writing
  std::fstream file(filename.data(), std::ios::binary | std::ios::out);
  if (!file.is_open()) {
    return absl::InternalError(
        absl::StrCat("Could not open ROM file: ", filename));
  }

  // Save the data to the file
  for (auto i = 0; i < size_; ++i) {
    file << rom_data_[i];
  }

  // Check for write errors
  if (!file.good()) {
    return absl::InternalError(
        absl::StrCat("Error while writing to ROM file: ", filename));
  }

  return absl::OkStatus();
}

// ============================================================================

gfx::SNESColor ROM::ReadColor(int offset) {
  short color = toint16(offset);
  gfx::snes_color new_color;
  new_color.red = (color & 0x1F) * 8;
  new_color.green = ((color >> 5) & 0x1F) * 8;
  new_color.blue = ((color >> 10) & 0x1F) * 8;
  gfx::SNESColor snes_color(new_color);
  return snes_color;
}

// ============================================================================

gfx::SNESPalette ROM::ReadPalette(int offset, int num_colors) {
  int color_offset = 0;
  std::vector<gfx::SNESColor> colors(num_colors);

  while (color_offset < num_colors) {
    short color = toint16(offset);
    gfx::snes_color new_color;
    new_color.red = (color & 0x1F) * 8;
    new_color.green = ((color >> 5) & 0x1F) * 8;
    new_color.blue = ((color >> 10) & 0x1F) * 8;
    colors[color_offset].setSNES(new_color);
    color_offset++;
    offset += 2;
  }

  gfx::SNESPalette palette(colors);
  return palette;
}

// ============================================================================

void ROM::Write(int addr, int value) { rom_data_[addr] = value; }

void ROM::WriteShort(int addr, int value) {
  rom_data_[addr] = (uchar)(value & 0xFF);
  rom_data_[addr + 1] = (uchar)((value >> 8) & 0xFF);
}

// ============================================================================

void ROM::WriteColor(uint32_t address, const gfx::SNESColor& color) {
  uint16_t bgr = ((color.snes >> 10) & 0x1F) | ((color.snes & 0x1F) << 10) |
                 (color.snes & 0x7C00);

  // Write the 16-bit color value to the ROM at the specified address
  rom_data_[address] = static_cast<uint8_t>(bgr & 0xFF);
  rom_data_[address + 1] = static_cast<uint8_t>((bgr >> 8) & 0xFF);
}

// ============================================================================

uint32_t ROM::GetPaletteAddress(const std::string& groupName,
                                size_t paletteIndex, size_t color_index) const {
  // Retrieve the base address for the palette group
  uint32_t base_address = paletteGroupAddresses.at(groupName);

  // Retrieve the number of colors for each palette in the group
  uint32_t colors_per_palette = paletteGroupColorCounts.at(groupName);

  // Calculate the address for the specified color in the ROM
  uint32_t address = base_address + (paletteIndex * colors_per_palette * 2) +
                     (color_index * 2);

  return address;
}

// ============================================================================

absl::Status ROM::ApplyAssembly(const absl::string_view& filename,
                                uint32_t size) {
  // int count = 0;
  // auto patch = filename.data();
  // auto data = (char*)rom_data_.data();
  // if (int size = size_; !asar_patch(patch, data, patch_size, &size)) {
  //   auto asar_error = asar_geterrors(&count);
  //   auto full_error = asar_error->fullerrdata;
  //   return absl::InternalError(absl::StrCat("ASAR Error: ", full_error));
  // }
  return absl::OkStatus();
}

}  // namespace app
}  // namespace yaze