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

absl::StatusOr<Bytes> Rom::Load2BppGraphics() {
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

absl::Status Rom::LoadLinkGraphics() {
  const auto link_gfx_offset = 0x80000;  // $10:8000
  const auto link_gfx_length = 0x800;    // 0x4000 or 0x7000?
  link_palette_ = palette_groups_.armors[0];

  // Load Links graphics from the ROM
  for (int i = 0; i < 14; i++) {
    ASSIGN_OR_RETURN(
        auto link_sheet_data,
        ReadByteVector(/*offset=*/link_gfx_offset + (i * link_gfx_length),
                       /*length=*/link_gfx_length))
    // auto link_sheet_8bpp = gfx::SnesTo8bppSheet(link_sheet_data, /*bpp=*/4);
    // Convert to 3bpp, then from 3bpp to 8bpp before creating bitmap.
    auto link_sheet_3bpp = gfx::Convert4bppTo3bpp(link_sheet_data);
    auto link_sheet_8bpp = gfx::SnesTo8bppSheet(link_sheet_3bpp, /*bpp=*/3);
    link_graphics_[i].Create(core::kTilesheetWidth, core::kTilesheetHeight,
                             core::kTilesheetDepth, link_sheet_8bpp);
    RETURN_IF_ERROR(
        link_graphics_[i].ApplyPaletteWithTransparent(link_palette_, 0));
    RenderBitmap(&link_graphics_[i]);
  }

  return absl::OkStatus();
}

absl::Status Rom::LoadAllGraphicsData() {
  constexpr uint32_t kNumGfxSheets = 223;
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
          RETURN_IF_ERROR(graphics_manager_[i]->ApplyPaletteWithTransparent(
              palette_groups_.global_sprites[0], 0));
        } else {
          RETURN_IF_ERROR(graphics_manager_[i]->ApplyPaletteWithTransparent(
              palette_groups_.dungeon_main[0], 0));
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

absl::Status Rom::LoadFromFile(const std::string& filename, bool z3_load) {
  std::string full_filename = filename;
#ifdef __linux__
  std::string const HOME = std::getenv("HOME") ? std::getenv("HOME") : ".";
  std::cout << "Home directory: " << HOME << std::endl;
  full_filename = absl::StrCat(HOME, "/", filename);
  std::cout << "Full path: " << full_path << std::endl;
#endif
  if (full_filename.empty()) {
    return absl::InvalidArgumentError(
        "Could not load ROM: parameter `filename` is empty.");
  }
  // Set filename
  filename_ = full_filename;

  // Open file
  std::ifstream file(filename_, std::ios::binary);
  if (!file.is_open()) {
    return absl::InternalError(
        absl::StrCat("Could not open ROM file: ", filename));
  }

  // Get file size and resize rom_data_
  try {
    size_ = std::filesystem::file_size(filename_);
  } catch (const std::filesystem::filesystem_error& e) {
    return absl::InternalError(
        absl::StrCat("Could not get file size: ", filename, " - ", e.what()));
  }
  rom_data_.resize(size_);

  // Read file into rom_data_
  file.read(reinterpret_cast<char*>(rom_data_.data()), size_);

  // Check if the sROM has a header
  constexpr size_t baseROMSize = 1048576;  // 1MB
  constexpr size_t headerSize = 0x200;     // 512 bytes
  if (size_ % baseROMSize == headerSize) {
    std::cout << "ROM has a header" << std::endl;
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
    constexpr uint32_t kTitleStringOffset = 0x7FC0;
    constexpr uint32_t kTitleStringLength = 20;
    memcpy(title_, rom_data_.data() + kTitleStringOffset, kTitleStringLength);
    if (rom_data_[kTitleStringOffset + 0x19] == 0) {
      version_ = Z3_Version::JP;
    } else {
      version_ = Z3_Version::US;
    }
    RETURN_IF_ERROR(gfx::LoadAllPalettes(rom_data_, palette_groups_));
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

absl::Status Rom::LoadFromPointer(uchar* data, size_t length) {
  if (!data)
    return absl::InvalidArgumentError(
        "Could not load ROM: parameter `data` is empty.");

  for (int i = 0; i < length; ++i) rom_data_.push_back(data[i]);

  return absl::OkStatus();
}

absl::Status Rom::LoadFromBytes(const Bytes& data) {
  if (data.empty()) {
    return absl::InvalidArgumentError(
        "Could not load ROM: parameter `data` is empty.");
  }
  rom_data_ = data;
  size_ = data.size();
  is_loaded_ = true;
  return absl::OkStatus();
}

absl::Status Rom::SaveToFile(bool backup, bool save_new, std::string filename) {
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
      std::filesystem::copy(filename_, backup_filename,
                            std::filesystem::copy_options::overwrite_existing);
    } catch (const std::filesystem::filesystem_error& e) {
      non_firing_status = absl::InternalError(absl::StrCat(
          "Could not create backup file: ", backup_filename, " - ", e.what()));
    }
  }

  // Run the other save functions
  if (flags()->kSaveAllPalettes) {
    RETURN_IF_ERROR(SaveAllPalettes());
  }

  if (flags()->kSaveGfxGroups) {
    SaveGroupsToRom();
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

absl::Status Rom::SavePalette(int index, const std::string& group_name,
                              gfx::SnesPalette& palette) {
  // Iterate through all colors in the palette
  for (size_t j = 0; j < palette.size(); ++j) {
    gfx::SnesColor color = palette[j];
    // If the color is modified, save the color to the ROM
    if (color.is_modified()) {
      RETURN_IF_ERROR(
          WriteColor(gfx::GetPaletteAddress(group_name, index, j), color));
      color.set_modified(false);  // Reset the modified flag after saving
    }
  }
  return absl::OkStatus();
}

absl::Status Rom::SaveAllPalettes() {
  RETURN_IF_ERROR(
      palette_groups_.for_each([&](gfx::PaletteGroup& group) -> absl::Status {
        for (size_t i = 0; i < group.size(); ++i) {
          RETURN_IF_ERROR(
              SavePalette(i, group.name(), *group.mutable_palette(i)));
        }
        return absl::OkStatus();
      }));

  return absl::OkStatus();
}

void Rom::LoadGfxGroups() {
  main_blockset_ids.resize(37, std::vector<uint8_t>(8));
  room_blockset_ids.resize(82, std::vector<uint8_t>(4));
  spriteset_ids.resize(144, std::vector<uint8_t>(4));
  paletteset_ids.resize(72, std::vector<uint8_t>(4));

  int gfxPointer =
      (rom_data_[kGfxGroupsPointer + 1] << 8) + rom_data_[kGfxGroupsPointer];
  gfxPointer = core::SnesToPc(gfxPointer);

  for (int i = 0; i < 37; i++) {
    for (int j = 0; j < 8; j++) {
      main_blockset_ids[i][j] = rom_data_[gfxPointer + (i * 8) + j];
    }
  }

  for (int i = 0; i < 82; i++) {
    for (int j = 0; j < 4; j++) {
      room_blockset_ids[i][j] =
          rom_data_[core::entrance_gfx_group + (i * 4) + j];
    }
  }

  for (int i = 0; i < 144; i++) {
    for (int j = 0; j < 4; j++) {
      spriteset_ids[i][j] =
          rom_data_[version_constants().kSpriteBlocksetPointer + (i * 4) + j];
    }
  }

  for (int i = 0; i < 72; i++) {
    for (int j = 0; j < 4; j++) {
      paletteset_ids[i][j] =
          rom_data_[version_constants().kDungeonPalettesGroups + (i * 4) + j];
    }
  }
}

void Rom::SaveGroupsToRom() {
  int gfxPointer =
      (rom_data_[kGfxGroupsPointer + 1] << 8) + rom_data_[kGfxGroupsPointer];
  gfxPointer = core::SnesToPc(gfxPointer);

  for (int i = 0; i < 37; i++) {
    for (int j = 0; j < 8; j++) {
      rom_data_[gfxPointer + (i * 8) + j] = main_blockset_ids[i][j];
    }
  }

  for (int i = 0; i < 82; i++) {
    for (int j = 0; j < 4; j++) {
      rom_data_[core::entrance_gfx_group + (i * 4) + j] =
          room_blockset_ids[i][j];
    }
  }

  for (int i = 0; i < 144; i++) {
    for (int j = 0; j < 4; j++) {
      rom_data_[version_constants().kSpriteBlocksetPointer + (i * 4) + j] =
          spriteset_ids[i][j];
    }
  }

  for (int i = 0; i < 72; i++) {
    for (int j = 0; j < 4; j++) {
      rom_data_[version_constants().kDungeonPalettesGroups + (i * 4) + j] =
          paletteset_ids[i][j];
    }
  }
}

std::shared_ptr<Rom> SharedRom::shared_rom_ = nullptr;

}  // namespace app
}  // namespace yaze