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
#include "app/gfx/snes_color.h"            // for SNESColor
#include "app/gfx/snes_palette.h"          // for PaletteGroup
#include "app/gfx/snes_tile.h"             // for SnesTo8bppSheet

namespace yaze {
namespace app {

namespace {
absl::Status LoadOverworldMainPalettes(const Bytes& rom_data,
                                       PaletteGroupMap& palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 6; i++) {
    RETURN_IF_ERROR(palette_groups["ow_main"].AddPalette(
        gfx::ReadPaletteFromRom(core::overworldPaletteMain + (i * (35 * 2)),
                                /*num_colors*/ 35, data)))
  }
  return absl::OkStatus();
}

absl::Status LoadOverworldAuxiliaryPalettes(const Bytes& rom_data,
                                            PaletteGroupMap& palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 20; i++) {
    RETURN_IF_ERROR(palette_groups["ow_aux"].AddPalette(gfx::ReadPaletteFromRom(
        core::overworldPaletteAuxialiary + (i * (21 * 2)),
        /*num_colors*/ 21, data)))
  }
  return absl::OkStatus();
}

absl::Status LoadOverworldAnimatedPalettes(const Bytes& rom_data,
                                           PaletteGroupMap& palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 14; i++) {
    RETURN_IF_ERROR(
        palette_groups["ow_animated"].AddPalette(gfx::ReadPaletteFromRom(
            core::overworldPaletteAnimated + (i * (7 * 2)), 7, data)))
  }
  return absl::OkStatus();
}

absl::Status LoadHUDPalettes(const Bytes& rom_data,
                             PaletteGroupMap& palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 2; i++) {
    RETURN_IF_ERROR(palette_groups["hud"].AddPalette(
        gfx::ReadPaletteFromRom(core::hudPalettes + (i * 64), 32, data)))
  }
  return absl::OkStatus();
}

absl::Status LoadGlobalSpritePalettes(const Bytes& rom_data,
                                      PaletteGroupMap& palette_groups) {
  auto data = rom_data.data();
  RETURN_IF_ERROR(palette_groups["global_sprites"].AddPalette(
      gfx::ReadPaletteFromRom(core::globalSpritePalettesLW, 60, data)))
  RETURN_IF_ERROR(palette_groups["global_sprites"].AddPalette(
      gfx::ReadPaletteFromRom(core::globalSpritePalettesDW, 60, data)))
  return absl::OkStatus();
}

absl::Status LoadArmorPalettes(const Bytes& rom_data,
                               PaletteGroupMap& palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 5; i++) {
    RETURN_IF_ERROR(palette_groups["armors"].AddPalette(
        gfx::ReadPaletteFromRom(core::armorPalettes + (i * 30), 15, data)))
  }
  return absl::OkStatus();
}

absl::Status LoadSwordPalettes(const Bytes& rom_data,
                               PaletteGroupMap& palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 4; i++) {
    RETURN_IF_ERROR(palette_groups["swords"].AddPalette(
        gfx::ReadPaletteFromRom(core::swordPalettes + (i * 6), 3, data)))
  }
  return absl::OkStatus();
}

absl::Status LoadShieldPalettes(const Bytes& rom_data,
                                PaletteGroupMap& palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 3; i++) {
    RETURN_IF_ERROR(palette_groups["shields"].AddPalette(
        gfx::ReadPaletteFromRom(core::shieldPalettes + (i * 8), 4, data)))
  }
  return absl::OkStatus();
}

absl::Status LoadSpriteAux1Palettes(const Bytes& rom_data,
                                    PaletteGroupMap& palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 12; i++) {
    RETURN_IF_ERROR(palette_groups["sprites_aux1"].AddPalette(
        gfx::ReadPaletteFromRom(core::spritePalettesAux1 + (i * 14), 7, data)))
  }
  return absl::OkStatus();
}

absl::Status LoadSpriteAux2Palettes(const Bytes& rom_data,
                                    PaletteGroupMap& palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 11; i++) {
    RETURN_IF_ERROR(palette_groups["sprites_aux2"].AddPalette(
        gfx::ReadPaletteFromRom(core::spritePalettesAux2 + (i * 14), 7, data)))
  }
  return absl::OkStatus();
}

absl::Status LoadSpriteAux3Palettes(const Bytes& rom_data,
                                    PaletteGroupMap& palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 24; i++) {
    RETURN_IF_ERROR(palette_groups["sprites_aux3"].AddPalette(
        gfx::ReadPaletteFromRom(core::spritePalettesAux3 + (i * 14), 7, data)))
  }
  return absl::OkStatus();
}

absl::Status LoadDungeonMainPalettes(const Bytes& rom_data,
                                     PaletteGroupMap& palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 20; i++) {
    RETURN_IF_ERROR(
        palette_groups["dungeon_main"].AddPalette(gfx::ReadPaletteFromRom(
            core::dungeonMainPalettes + (i * 180), 90, data)))
  }
  return absl::OkStatus();
}

absl::Status LoadGrassColors(const Bytes& rom_data,
                             PaletteGroupMap& palette_groups) {
  RETURN_IF_ERROR(palette_groups["grass"].AddColor(
      gfx::ReadColorFromRom(core::hardcodedGrassLW, rom_data.data())))
  RETURN_IF_ERROR(palette_groups["grass"].AddColor(
      gfx::ReadColorFromRom(core::hardcodedGrassDW, rom_data.data())))
  RETURN_IF_ERROR(palette_groups["grass"].AddColor(
      gfx::ReadColorFromRom(core::hardcodedGrassSpecial, rom_data.data())))
  return absl::OkStatus();
}

absl::Status Load3DObjectPalettes(const Bytes& rom_data,
                                  PaletteGroupMap& palette_groups) {
  auto data = rom_data.data();
  RETURN_IF_ERROR(palette_groups["3d_object"].AddPalette(
      gfx::ReadPaletteFromRom(core::triforcePalette, 8, data)))
  RETURN_IF_ERROR(palette_groups["3d_object"].AddPalette(
      gfx::ReadPaletteFromRom(core::crystalPalette, 8, data)))
  return absl::OkStatus();
}

absl::Status LoadOverworldMiniMapPalettes(const Bytes& rom_data,
                                          PaletteGroupMap& palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 2; i++) {
    RETURN_IF_ERROR(
        palette_groups["ow_mini_map"].AddPalette(gfx::ReadPaletteFromRom(
            core::overworldMiniMapPalettes + (i * 256), 128, data)))
  }
  return absl::OkStatus();
}
}  // namespace

absl::StatusOr<Bytes> ROM::Load2BppGraphics() {
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

// TODO: Load Links graphics from the ROM
absl::Status ROM::LoadLinkGraphics() {
  const auto link_gfx_offset = 81920;  // $10:8000
  const auto link_gfx_length = 0x800;

  // Load Links graphics from the ROM
  for (int i = 0; i < 14; i++) {
    ASSIGN_OR_RETURN(
        auto link_sheet_data,
        ReadByteVector(/*offset=*/link_gfx_offset + (i * link_gfx_length),
                       /*length=*/link_gfx_length))
    auto link_sheet_8bpp = gfx::SnesTo8bppSheet(link_sheet_data, /*bpp=*/4);
    link_graphics_[i].Create(core::kTilesheetWidth, core::kTilesheetHeight,
                             core::kTilesheetDepth, link_sheet_8bpp);
    link_graphics_[i].ApplyPalette(link_palette_);
    RenderBitmap(&link_graphics_[i]);
  }

  return absl::OkStatus();
}

absl::Status ROM::LoadAllGraphicsData() {
  Bytes sheet;
  bool bpp3 = false;

  for (int i = 0; i < kNumGfxSheets; i++) {
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
      if (flags()->kUseBitmapManager) {
        graphics_manager_.LoadBitmap(i, converted_sheet, core::kTilesheetWidth,
                                     core::kTilesheetHeight,
                                     core::kTilesheetDepth);
        if (i > 115) {
          // Apply sprites palette
          graphics_manager_[i]->ApplyPaletteWithTransparent(
              palette_groups_["global_sprites"][0], 0);
        } else {
          graphics_manager_[i]->ApplyPaletteWithTransparent(
              palette_groups_["dungeon_main"][0], 0);
        }
        graphics_manager_[i]->CreateTexture(renderer_);
      }
      graphics_bin_[i] =
          gfx::Bitmap(core::kTilesheetWidth, core::kTilesheetHeight,
                      core::kTilesheetDepth, converted_sheet);
      graphics_bin_.at(i).CreateTexture(renderer_);

      if (flags()->kUseBitmapManager) {
        for (int j = 0; j < graphics_manager_[i].get()->size(); ++j) {
          graphics_buffer_.push_back(graphics_manager_[i]->at(j));
        }
      }
    } else {
      for (int j = 0; j < graphics_bin_[0].size(); ++j) {
        graphics_buffer_.push_back(0xFF);
      }
    }
  }
  return absl::OkStatus();
}

absl::Status ROM::LoadAllPalettes() {
  RETURN_IF_ERROR(LoadOverworldMainPalettes(rom_data_, palette_groups_))
  RETURN_IF_ERROR(LoadOverworldAuxiliaryPalettes(rom_data_, palette_groups_))
  RETURN_IF_ERROR(LoadOverworldAnimatedPalettes(rom_data_, palette_groups_))
  RETURN_IF_ERROR(LoadHUDPalettes(rom_data_, palette_groups_))
  RETURN_IF_ERROR(LoadGlobalSpritePalettes(rom_data_, palette_groups_))
  RETURN_IF_ERROR(LoadArmorPalettes(rom_data_, palette_groups_))
  RETURN_IF_ERROR(LoadSwordPalettes(rom_data_, palette_groups_))
  RETURN_IF_ERROR(LoadShieldPalettes(rom_data_, palette_groups_))
  RETURN_IF_ERROR(LoadSpriteAux1Palettes(rom_data_, palette_groups_))
  RETURN_IF_ERROR(LoadSpriteAux2Palettes(rom_data_, palette_groups_))
  RETURN_IF_ERROR(LoadSpriteAux3Palettes(rom_data_, palette_groups_))
  RETURN_IF_ERROR(LoadDungeonMainPalettes(rom_data_, palette_groups_))
  RETURN_IF_ERROR(LoadGrassColors(rom_data_, palette_groups_))
  RETURN_IF_ERROR(Load3DObjectPalettes(rom_data_, palette_groups_))
  RETURN_IF_ERROR(LoadOverworldMiniMapPalettes(rom_data_, palette_groups_))
  return absl::OkStatus();
}

absl::Status ROM::LoadFromFile(const absl::string_view& filename,
                               bool z3_load) {
  // Set filename
  filename_ = filename;

  // Open file
  std::ifstream file(filename.data(), std::ios::binary);
  if (!file.is_open()) {
    return absl::InternalError(
        absl::StrCat("Could not open ROM file: ", filename));
  }

  // Get file size and resize rom_data_
  size_ = std::filesystem::file_size(filename);
  rom_data_.resize(size_);

  // Read file into rom_data_
  file.read(reinterpret_cast<char*>(rom_data_.data()), size_);

  // Check if the sROM has a header
  constexpr size_t baseROMSize = 1048576;  // 1MB
  constexpr size_t headerSize = 0x200;     // 512 bytes
  if (size_ % baseROMSize == headerSize) {
    has_header_ = true;
  }

  // Remove header if present
  if (has_header_) {
    auto header =
        std::vector<uchar>(rom_data_.begin(), rom_data_.begin() + 0x200);
    rom_data_.erase(rom_data_.begin(), rom_data_.begin() + 0x200);
    size_ -= 0x200;
  }

  // Close file
  file.close();

  // Load Zelda 3 specific data if requested
  if (z3_load) {
    // Copy ROM title
    memcpy(title_, rom_data_.data() + kTitleStringOffset, kTitleStringLength);
    if (rom_data_[kTitleStringOffset + 0x19] == 0) {
      version_ = Z3_Version::JP;
    } else {
      version_ = Z3_Version::US;
    }
    RETURN_IF_ERROR(LoadAllPalettes())
    LoadGfxGroups();
  }

  // Expand the ROM data to 2MB without changing the data in the first 1MB
  rom_data_.resize(baseROMSize * 2);
  size_ = baseROMSize * 2;

  // Set up the resource labels
  std::string resource_label_filename = absl::StrFormat("%s.labels", filename);
  resource_label_manager_.LoadLabels(resource_label_filename);

  // Set is_loaded_ flag and return success
  is_loaded_ = true;
  return absl::OkStatus();
}

absl::Status ROM::LoadFromPointer(uchar* data, size_t length) {
  if (!data)
    return absl::InvalidArgumentError(
        "Could not load ROM: parameter `data` is empty.");

  for (int i = 0; i < length; ++i) rom_data_.push_back(data[i]);

  return absl::OkStatus();
}

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

absl::Status ROM::SaveToFile(bool backup, bool save_new, std::string filename) {
  absl::Status non_firing_status;
  if (rom_data_.empty()) {
    return absl::InternalError("ROM data is empty.");
  }

  // Check if filename is empty
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
    try {
      std::filesystem::copy(filename, backup_filename,
                            std::filesystem::copy_options::overwrite_existing);
    } catch (const std::filesystem::filesystem_error& e) {
      non_firing_status = absl::InternalError(absl::StrCat(
          "Could not create backup file: ", backup_filename, " - ", e.what()));
    }
  }

  // Run the other save functions
  if (flags()->kSaveAllPalettes) {
    SaveAllPalettes();
  }

  if (save_new) {
    // Create a file of the same name and append the date between the filename
    // and file extension
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    auto filename_no_ext = filename.substr(0, filename.find_last_of("."));
    std::cout << filename_no_ext << std::endl;
    filename = absl::StrCat(filename_no_ext, "_", std::ctime(&now_c));
    // Remove spaces from new_filename and replace with _
    filename.erase(std::remove(filename.begin(), filename.end(), ' '),
                   filename.end());
    // Remove newline character from ctime()
    filename.erase(std::remove(filename.begin(), filename.end(), '\n'),
                   filename.end());
    // Add the file extension back to the new_filename
    filename = filename + ".sfc";
    std::cout << filename << std::endl;
  }

  // Open the file that we know exists for writing
  std::ofstream file(filename.data(), std::ios::binary | std::ios::app);
  if (!file) {
    // Create the file if it does not exist
    file.open(filename.data(), std::ios::binary);
    if (!file) {
      return absl::InternalError(
          absl::StrCat("Could not open or create ROM file: ", filename));
    }
  }

  // Save the data to the file
  try {
    file.write(
        static_cast<const char*>(static_cast<const void*>(rom_data_.data())),
        rom_data_.size());
  } catch (const std::ofstream::failure& e) {
    return absl::InternalError(absl::StrCat(
        "Error while writing to ROM file: ", filename, " - ", e.what()));
  }

  // Check for write errors
  if (!file) {
    return absl::InternalError(
        absl::StrCat("Error while writing to ROM file: ", filename));
  }

  if (!non_firing_status.ok()) {
    return non_firing_status;
  }

  return absl::OkStatus();
}

void ROM::SavePalette(int index, const std::string& group_name,
                      gfx::SnesPalette& palette) {
  // Iterate through all colors in the palette
  for (size_t j = 0; j < palette.size(); ++j) {
    gfx::SnesColor color = palette[j];
    // If the color is modified, save the color to the ROM
    if (color.IsModified()) {
      WriteColor(gfx::GetPaletteAddress(group_name, index, j), color);
      color.SetModified(false);  // Reset the modified flag after saving
    }
  }
}

void ROM::SaveAllPalettes() {
  // Iterate through all palette_groups_
  for (auto& [group_name, palettes] : palette_groups_) {
    // Iterate through all palettes in the group
    for (size_t i = 0; i < palettes.size(); ++i) {
      auto palette = palettes[i];
      SavePalette(i, group_name, palette);
    }
  }
}

absl::Status ROM::UpdatePaletteColor(const std::string& groupName,
                                     size_t paletteIndex, size_t colorIndex,
                                     const gfx::SnesColor& newColor) {
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
        palette_groups_[groupName][paletteIndex][colorIndex].SetModified(true);
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

std::shared_ptr<ROM> SharedROM::shared_rom_ = nullptr;

}  // namespace app
}  // namespace yaze