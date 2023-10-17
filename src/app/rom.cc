#include "rom.h"

#include <algorithm>      // for remove
#include <chrono>         // for system_clock
#include <cstddef>        // for size_t
#include <cstdint>        // for uint32_t, uint8_t
#include <cstring>        // for memcpy
#include <ctime>          // for ctime
#include <filesystem>     // for copy_options, copy_options...
#include <fstream>        // for string, fstream, ifstream
#include <stack>          // for stack
#include <string>         // for hash, operator==, char_traits
#include <unordered_map>  // for unordered_map, operator!=
#include <utility>        // for tuple_element<>::type
#include <vector>         // for vector, vector<>::value_type

#include "absl/container/flat_hash_map.h"  // for flat_hash_map, BitMask
#include "absl/status/status.h"            // for OkStatus, InternalError
#include "absl/status/statusor.h"          // for StatusOr
#include "absl/strings/str_cat.h"          // for StrCat
#include "absl/strings/string_view.h"      // for string_view, operator==
#include "app/core/constants.h"            // for Bytes, ASSIGN_OR_RETURN
#include "app/gfx/bitmap.h"                // for Bitmap, BitmapTable
#include "app/gfx/compression.h"           // for DecompressV2
#include "app/gfx/snes_palette.h"          // for PaletteGroup, SNESColor
#include "app/gfx/snes_tile.h"             // for SnesTo8bppSheet

namespace yaze {
namespace app {

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

  size_ = std::filesystem::file_size(filename);
  rom_data_.resize(size_);
  for (auto i = 0; i < size_; ++i) {
    char byte_to_read = ' ';
    file.read(&byte_to_read, sizeof(char));
    rom_data_[i] = byte_to_read;
  }

  // Check if the sROM has a header
  constexpr size_t baseROMSize = 1048576;  // 1MB
  constexpr size_t headerSize = 0x200;     // 512 bytes

  if (size_ % baseROMSize == headerSize) {
    has_header_ = true;
  }

  if (has_header_) {
    // remove header
    rom_data_.erase(rom_data_.begin(), rom_data_.begin() + 0x200);
    size_ -= 0x200;
  }

  file.close();
  if (z3_load) {
    // copy ROM title
    memcpy(title_, rom_data_.data() + kTitleStringOffset, kTitleStringLength);
    if (rom_data_[kTitleStringOffset + 0x19] == 0) {
      version_ = Z3_Version::JP;
    } else {
      version_ = Z3_Version::US;
    }
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

  // TODO: Make these grass colors editable color fields
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
        palette_groups_[groupName][paletteIndex][colorIndex].setModified(true);
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
        if (color.isModified()) {
          WriteColor(GetPaletteAddress(groupName, i, j), color);
          color.setModified(false);  // Reset the modified flag after saving
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
  // SaveAllPalettes();
  while (!changes_.empty()) {
    auto change = changes_.top();
    change();
    changes_.pop();
  }

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
    colors[color_offset].SetSNES(gfx::ConvertRGBtoSNES(new_color));
    color_offset++;
    offset += 2;
  }

  gfx::SNESPalette palette(colors);
  return palette;
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

std::shared_ptr<ROM> SharedROM::shared_rom_ = nullptr;

}  // namespace app
}  // namespace yaze