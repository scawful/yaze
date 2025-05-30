#include "rom.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "app/core/features.h"
#include "app/core/platform/renderer.h"
#include "app/gfx/compression.h"
#include "app/gfx/snes_color.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/snes.h"
#include "util/hex.h"
#include "util/log.h"
#include "util/macro.h"

namespace yaze {
using core::Renderer;
constexpr int Uncompressed3BPPSize = 0x0600;

uint32_t GetGraphicsAddress(const uint8_t *data, uint8_t addr, uint32_t ptr1,
                            uint32_t ptr2, uint32_t ptr3) {
  return SnesToPc(AddressFromBytes(data[ptr1 + addr], data[ptr2 + addr],
                                   data[ptr3 + addr]));
}

absl::StatusOr<std::vector<uint8_t>> Load2BppGraphics(const Rom &rom) {
  std::vector<uint8_t> sheet;
  const uint8_t sheets[] = {0x71, 0x72, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE};
  for (const auto &sheet_id : sheets) {
    auto offset = GetGraphicsAddress(rom.data(), sheet_id,
                                     rom.version_constants().kOverworldGfxPtr1,
                                     rom.version_constants().kOverworldGfxPtr2,
                                     rom.version_constants().kOverworldGfxPtr3);
    ASSIGN_OR_RETURN(auto decomp_sheet,
                     gfx::lc_lz2::DecompressV2(rom.data(), offset));
    auto converted_sheet = gfx::SnesTo8bppSheet(decomp_sheet, 2);
    for (const auto &each_pixel : converted_sheet) {
      sheet.push_back(each_pixel);
    }
  }
  return sheet;
}

absl::StatusOr<std::array<gfx::Bitmap, kNumLinkSheets>> LoadLinkGraphics(
    const Rom &rom) {
  const uint32_t kLinkGfxOffset = 0x80000;  // $10:8000
  const uint16_t kLinkGfxLength = 0x800;    // 0x4000 or 0x7000?
  std::array<gfx::Bitmap, kNumLinkSheets> link_graphics;
  for (uint32_t i = 0; i < kNumLinkSheets; i++) {
    ASSIGN_OR_RETURN(
        auto link_sheet_data,
        rom.ReadByteVector(/*offset=*/kLinkGfxOffset + (i * kLinkGfxLength),
                           /*length=*/kLinkGfxLength));
    auto link_sheet_8bpp = gfx::SnesTo8bppSheet(link_sheet_data, /*bpp=*/4);
    link_graphics[i].Create(gfx::kTilesheetWidth, gfx::kTilesheetHeight,
                            gfx::kTilesheetDepth, link_sheet_8bpp);
    link_graphics[i].SetPalette(rom.palette_group().armors[0]);
    Renderer::Get().RenderBitmap(&link_graphics[i]);
  }
  return link_graphics;
}

absl::StatusOr<gfx::Bitmap> LoadFontGraphics(const Rom &rom) {
  std::vector<uint8_t> data(0x2000);
  for (int i = 0; i < 0x2000; i++) {
    data[i] = rom.data()[0x70000 + i];
  }

  std::vector<uint8_t> new_data(0x4000);
  std::vector<uint8_t> mask = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};
  int sheet_position = 0;

  // 8x8 tile
  for (int s = 0; s < 4; s++) {        // Per Sheet
    for (int j = 0; j < 4; j++) {      // Per Tile Line Y
      for (int i = 0; i < 16; i++) {   // Per Tile Line X
        for (int y = 0; y < 8; y++) {  // Per Pixel Line
          uint8_t line_bits0 =
              data[(y * 2) + (i * 16) + (j * 256) + sheet_position];
          uint8_t line_bits1 =
              data[(y * 2) + (i * 16) + (j * 256) + 1 + sheet_position];

          for (int x = 0; x < 4; x++) {  // Per Pixel X
            uint8_t pixdata = 0;
            uint8_t pixdata2 = 0;

            if ((line_bits0 & mask[(x * 2)]) == mask[(x * 2)]) {
              pixdata += 1;
            }
            if ((line_bits1 & mask[(x * 2)]) == mask[(x * 2)]) {
              pixdata += 2;
            }

            if ((line_bits0 & mask[(x * 2) + 1]) == mask[(x * 2) + 1]) {
              pixdata2 += 1;
            }
            if ((line_bits1 & mask[(x * 2) + 1]) == mask[(x * 2) + 1]) {
              pixdata2 += 2;
            }

            new_data[(y * 64) + (x) + (i * 4) + (j * 512) + (s * 2048)] =
                (uint8_t)((pixdata << 4) | pixdata2);
          }
        }
      }
    }

    sheet_position += 0x400;
  }

  std::vector<uint8_t> fontgfx16_data(0x4000);
  for (int i = 0; i < 0x4000; i++) {
    fontgfx16_data[i] = new_data[i];
  }

  gfx::Bitmap font_gfx;
  font_gfx.Create(128, 128, 64, fontgfx16_data);
  return font_gfx;
}

absl::StatusOr<std::array<gfx::Bitmap, kNumGfxSheets>> LoadAllGraphicsData(
    Rom &rom, bool defer_render) {
  std::array<gfx::Bitmap, kNumGfxSheets> graphics_sheets;
  std::vector<uint8_t> sheet;
  bool bpp3 = false;

  for (uint32_t i = 0; i < kNumGfxSheets; i++) {
    if (i >= 115 && i <= 126) {  // uncompressed sheets
      sheet.resize(Uncompressed3BPPSize);
      auto offset = GetGraphicsAddress(
          rom.data(), i, rom.version_constants().kOverworldGfxPtr1,
          rom.version_constants().kOverworldGfxPtr2,
          rom.version_constants().kOverworldGfxPtr3);
      std::copy(rom.data() + offset, rom.data() + offset + Uncompressed3BPPSize,
                sheet.begin());
      bpp3 = true;
    } else if (i == 113 || i == 114 || i >= 218) {
      bpp3 = false;
    } else {
      auto offset = GetGraphicsAddress(
          rom.data(), i, rom.version_constants().kOverworldGfxPtr1,
          rom.version_constants().kOverworldGfxPtr2,
          rom.version_constants().kOverworldGfxPtr3);
      ASSIGN_OR_RETURN(sheet, gfx::lc_lz2::DecompressV2(rom.data(), offset));
      bpp3 = true;
    }

    if (bpp3) {
      auto converted_sheet = gfx::SnesTo8bppSheet(sheet, 3);
      graphics_sheets[i].Create(gfx::kTilesheetWidth, gfx::kTilesheetHeight,
                                gfx::kTilesheetDepth, converted_sheet);
      if (graphics_sheets[i].is_active()) {
        if (i > 115) {
          // Apply sprites palette
          graphics_sheets[i].SetPaletteWithTransparent(
              rom.palette_group().global_sprites[0], 0);
        } else {
          graphics_sheets[i].SetPaletteWithTransparent(
              rom.palette_group().dungeon_main[0], 0);
        }
      }

      if (!defer_render) {
        graphics_sheets[i].CreateTexture(Renderer::Get().renderer());
      }

      for (int j = 0; j < graphics_sheets[i].size(); ++j) {
        rom.mutable_graphics_buffer()->push_back(graphics_sheets[i].at(j));
      }

    } else {
      for (int j = 0; j < graphics_sheets[0].size(); ++j) {
        rom.mutable_graphics_buffer()->push_back(0xFF);
      }
    }
  }
  return graphics_sheets;
}

absl::Status SaveAllGraphicsData(
    Rom &rom, std::array<gfx::Bitmap, kNumGfxSheets> &gfx_sheets) {
  for (int i = 0; i < kNumGfxSheets; i++) {
    if (gfx_sheets[i].is_active()) {
      int to_bpp = 3;
      std::vector<uint8_t> final_data;
      bool compressed = true;
      if (i >= 115 && i <= 126) {
        compressed = false;
      } else if (i == 113 || i == 114 || i >= 218) {
        to_bpp = 2;
        continue;
      }

      std::cout << "Sheet ID " << i << " BPP: " << to_bpp << std::endl;
      auto sheet_data = gfx_sheets[i].vector();
      std::cout << "Sheet data size: " << sheet_data.size() << std::endl;
      final_data = gfx::Bpp8SnesToIndexed(sheet_data, 8);
      int size = 0;
      if (compressed) {
        auto compressed_data = gfx::HyruleMagicCompress(
            final_data.data(), final_data.size(), &size, 1);
        for (int j = 0; j < size; j++) {
          sheet_data[j] = compressed_data[j];
        }
      }
      auto offset = GetGraphicsAddress(
          rom.data(), i, rom.version_constants().kOverworldGfxPtr1,
          rom.version_constants().kOverworldGfxPtr2,
          rom.version_constants().kOverworldGfxPtr3);
      std::copy(final_data.begin(), final_data.end(), rom.begin() + offset);
      std::ranges::copy(final_data, rom.begin() + offset);
    }
  }
  return absl::OkStatus();
}

absl::Status Rom::LoadFromFile(const std::string &filename, bool z3_load) {
  if (filename.empty()) {
    return absl::InvalidArgumentError(
        "Could not load ROM: parameter `filename` is empty.");
  }
  filename_ = std::filesystem::absolute(filename).string();
  short_name_ = filename_.substr(filename_.find_last_of("/\\") + 1);

  std::ifstream file(filename_, std::ios::binary);
  if (!file.is_open()) {
    return absl::NotFoundError(
        absl::StrCat("Could not open ROM file: ", filename_));
  }

  // Get file size and resize rom_data_
  try {
    size_ = std::filesystem::file_size(filename_);
  } catch (const std::filesystem::filesystem_error &e) {
    // Try to get the file size from the open file stream
    file.seekg(0, std::ios::end);
    if (!file) {
      return absl::InternalError(absl::StrCat(
          "Could not get file size: ", filename_, " - ", e.what()));
    }
    size_ = file.tellg();
  }
  rom_data_.resize(size_);

  // Read file into rom_data_
  file.read(reinterpret_cast<char *>(rom_data_.data()), size_);
  file.close();

  if (z3_load) {
    RETURN_IF_ERROR(LoadZelda3());
    resource_label_manager_.LoadLabels(absl::StrFormat("%s.labels", filename));
  }

  return absl::OkStatus();
}

absl::Status Rom::LoadFromData(const std::vector<uint8_t> &data, bool z3_load) {
  if (data.empty()) {
    return absl::InvalidArgumentError(
        "Could not load ROM: parameter `data` is empty.");
  }
  rom_data_ = data;
  size_ = data.size();
  if (z3_load) {
    RETURN_IF_ERROR(LoadZelda3());
  }
  return absl::OkStatus();
}

absl::Status Rom::LoadZelda3() {
  // Check if the ROM has a header
  constexpr size_t kBaseRomSize = 1048576;  // 1MB
  constexpr size_t kHeaderSize = 0x200;     // 512 bytes
  if (size_ % kBaseRomSize == kHeaderSize) {
    auto header = std::vector<uint8_t>(rom_data_.begin(),
                                       rom_data_.begin() + kHeaderSize);
    rom_data_.erase(rom_data_.begin(), rom_data_.begin() + kHeaderSize);
    size_ -= 0x200;
  }

  // Copy ROM title
  constexpr uint32_t kTitleStringOffset = 0x7FC0;
  constexpr uint32_t kTitleStringLength = 20;
  title_.resize(kTitleStringLength);
  std::copy(rom_data_.begin() + kTitleStringOffset,
            rom_data_.begin() + kTitleStringOffset + kTitleStringLength,
            title_.begin());
  if (rom_data_[kTitleStringOffset + 0x19] == 0) {
    version_ = zelda3_version::JP;
  } else {
    version_ = zelda3_version::US;
  }

  // Load additional resources
  RETURN_IF_ERROR(gfx::LoadAllPalettes(rom_data_, palette_groups_));
  // TODO Load gfx groups or expanded ZS values
  RETURN_IF_ERROR(LoadGfxGroups());

  // Expand the ROM data to 2MB without changing the data in the first 1MB
  rom_data_.resize(kBaseRomSize * 2);
  size_ = kBaseRomSize * 2;

  return absl::OkStatus();
}

absl::Status Rom::LoadGfxGroups() {
  ASSIGN_OR_RETURN(auto main_blockset_ptr, ReadWord(kGfxGroupsPointer));
  main_blockset_ptr = SnesToPc(main_blockset_ptr);

  for (uint32_t i = 0; i < kNumMainBlocksets; i++) {
    for (int j = 0; j < 8; j++) {
      main_blockset_ids[i][j] = rom_data_[main_blockset_ptr + (i * 8) + j];
    }
  }

  for (uint32_t i = 0; i < kNumRoomBlocksets; i++) {
    for (int j = 0; j < 4; j++) {
      room_blockset_ids[i][j] = rom_data_[kEntranceGfxGroup + (i * 4) + j];
    }
  }

  for (uint32_t i = 0; i < kNumSpritesets; i++) {
    for (int j = 0; j < 4; j++) {
      spriteset_ids[i][j] =
          rom_data_[version_constants().kSpriteBlocksetPointer + (i * 4) + j];
    }
  }

  for (uint32_t i = 0; i < kNumPalettesets; i++) {
    for (int j = 0; j < 4; j++) {
      paletteset_ids[i][j] =
          rom_data_[version_constants().kDungeonPalettesGroups + (i * 4) + j];
    }
  }

  return absl::OkStatus();
}

absl::Status Rom::SaveGfxGroups() {
  ASSIGN_OR_RETURN(auto main_blockset_ptr, ReadWord(kGfxGroupsPointer));
  main_blockset_ptr = SnesToPc(main_blockset_ptr);

  for (uint32_t i = 0; i < kNumMainBlocksets; i++) {
    for (int j = 0; j < 8; j++) {
      rom_data_[main_blockset_ptr + (i * 8) + j] = main_blockset_ids[i][j];
    }
  }

  for (uint32_t i = 0; i < kNumRoomBlocksets; i++) {
    for (int j = 0; j < 4; j++) {
      rom_data_[kEntranceGfxGroup + (i * 4) + j] = room_blockset_ids[i][j];
    }
  }

  for (uint32_t i = 0; i < kNumSpritesets; i++) {
    for (int j = 0; j < 4; j++) {
      rom_data_[version_constants().kSpriteBlocksetPointer + (i * 4) + j] =
          spriteset_ids[i][j];
    }
  }

  for (uint32_t i = 0; i < kNumPalettesets; i++) {
    for (int j = 0; j < 4; j++) {
      rom_data_[version_constants().kDungeonPalettesGroups + (i * 4) + j] =
          paletteset_ids[i][j];
    }
  }

  return absl::OkStatus();
}

absl::Status Rom::SaveToFile(const SaveSettings &settings) {
  absl::Status non_firing_status;
  if (rom_data_.empty()) {
    return absl::InternalError("ROM data is empty.");
  }

  std::string filename = settings.filename;
  auto backup = settings.backup;
  auto save_new = settings.save_new;

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
    } catch (const std::filesystem::filesystem_error &e) {
      non_firing_status = absl::InternalError(absl::StrCat(
          "Could not create backup file: ", backup_filename, " - ", e.what()));
    }
  }

  // Run the other save functions
  if (settings.z3_save) {
    if (core::FeatureFlags::get().kSaveAllPalettes)
      RETURN_IF_ERROR(SaveAllPalettes());
    if (core::FeatureFlags::get().kSaveGfxGroups)
      RETURN_IF_ERROR(SaveGfxGroups());
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
        static_cast<const char *>(static_cast<const void *>(rom_data_.data())),
        rom_data_.size());
  } catch (const std::ofstream::failure &e) {
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

absl::Status Rom::SavePalette(int index, const std::string &group_name,
                              gfx::SnesPalette &palette) {
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
      palette_groups_.for_each([&](gfx::PaletteGroup &group) -> absl::Status {
        for (size_t i = 0; i < group.size(); ++i) {
          RETURN_IF_ERROR(
              SavePalette(i, group.name(), *group.mutable_palette(i)));
        }
        return absl::OkStatus();
      }));

  return absl::OkStatus();
}

absl::StatusOr<uint8_t> Rom::ReadByte(int offset) {
  if (offset >= static_cast<int>(rom_data_.size())) {
    return absl::FailedPreconditionError("Offset out of range");
  }
  return rom_data_[offset];
}

absl::StatusOr<uint16_t> Rom::ReadWord(int offset) {
  if (offset + 1 >= static_cast<int>(rom_data_.size())) {
    return absl::FailedPreconditionError("Offset out of range");
  }
  auto result = (uint16_t)(rom_data_[offset] | (rom_data_[offset + 1] << 8));
  return result;
}

absl::StatusOr<uint32_t> Rom::ReadLong(int offset) {
  if (offset + 2 >= static_cast<int>(rom_data_.size())) {
    return absl::OutOfRangeError("Offset out of range");
  }
  auto result = (uint32_t)(rom_data_[offset] | (rom_data_[offset + 1] << 8) |
                           (rom_data_[offset + 2] << 16));
  return result;
}

absl::StatusOr<std::vector<uint8_t>> Rom::ReadByteVector(
    uint32_t offset, uint32_t length) const {
  if (offset + length > static_cast<uint32_t>(rom_data_.size())) {
    return absl::OutOfRangeError("Offset and length out of range");
  }
  std::vector<uint8_t> result;
  for (uint32_t i = offset; i < offset + length; i++) {
    result.push_back(rom_data_[i]);
  }
  return result;
}

absl::StatusOr<gfx::Tile16> Rom::ReadTile16(uint32_t tile16_id) {
  // Skip 8 bytes per tile.
  auto tpos = kTile16Ptr + (tile16_id * 0x08);
  gfx::Tile16 tile16 = {};
  ASSIGN_OR_RETURN(auto new_tile0, ReadWord(tpos));
  tile16.tile0_ = gfx::WordToTileInfo(new_tile0);
  tpos += 2;
  ASSIGN_OR_RETURN(auto new_tile1, ReadWord(tpos));
  tile16.tile1_ = gfx::WordToTileInfo(new_tile1);
  tpos += 2;
  ASSIGN_OR_RETURN(auto new_tile2, ReadWord(tpos));
  tile16.tile2_ = gfx::WordToTileInfo(new_tile2);
  tpos += 2;
  ASSIGN_OR_RETURN(auto new_tile3, ReadWord(tpos));
  tile16.tile3_ = gfx::WordToTileInfo(new_tile3);
  return tile16;
}

absl::Status Rom::WriteTile16(int tile16_id, const gfx::Tile16 &tile) {
  // Skip 8 bytes per tile.
  auto tpos = kTile16Ptr + (tile16_id * 0x08);
  RETURN_IF_ERROR(WriteShort(tpos, gfx::TileInfoToWord(tile.tile0_)));
  tpos += 2;
  RETURN_IF_ERROR(WriteShort(tpos, gfx::TileInfoToWord(tile.tile1_)));
  tpos += 2;
  RETURN_IF_ERROR(WriteShort(tpos, gfx::TileInfoToWord(tile.tile2_)));
  tpos += 2;
  RETURN_IF_ERROR(WriteShort(tpos, gfx::TileInfoToWord(tile.tile3_)));
  return absl::OkStatus();
}

absl::Status Rom::WriteByte(int addr, uint8_t value) {
  if (addr >= static_cast<int>(rom_data_.size())) {
    return absl::OutOfRangeError(absl::StrFormat(
        "Attempt to write byte %#02x value failed, address %d out of range",
        value, addr));
  }
  rom_data_[addr] = value;
  util::logf("WriteByte: %#06X: %s", addr, util::HexByte(value).data());
  return absl::OkStatus();
}

absl::Status Rom::WriteWord(int addr, uint16_t value) {
  if (addr + 1 >= static_cast<int>(rom_data_.size())) {
    return absl::OutOfRangeError(absl::StrFormat(
        "Attempt to write word %#04x value failed, address %d out of range",
        value, addr));
  }
  rom_data_[addr] = (uint8_t)(value & 0xFF);
  rom_data_[addr + 1] = (uint8_t)((value >> 8) & 0xFF);
  util::logf("WriteWord: %#06X: %s", addr, util::HexWord(value).data());
  return absl::OkStatus();
}

absl::Status Rom::WriteShort(int addr, uint16_t value) {
  if (addr + 1 >= static_cast<int>(rom_data_.size())) {
    return absl::OutOfRangeError(absl::StrFormat(
        "Attempt to write short %#04x value failed, address %d out of range",
        value, addr));
  }
  rom_data_[addr] = (uint8_t)(value & 0xFF);
  rom_data_[addr + 1] = (uint8_t)((value >> 8) & 0xFF);
  util::logf("WriteShort: %#06X: %s", addr, util::HexWord(value).data());
  return absl::OkStatus();
}

absl::Status Rom::WriteLong(uint32_t addr, uint32_t value) {
  if (addr + 2 >= static_cast<uint32_t>(rom_data_.size())) {
    return absl::OutOfRangeError(absl::StrFormat(
        "Attempt to write long %#06x value failed, address %d out of range",
        value, addr));
  }
  rom_data_[addr] = (uint8_t)(value & 0xFF);
  rom_data_[addr + 1] = (uint8_t)((value >> 8) & 0xFF);
  rom_data_[addr + 2] = (uint8_t)((value >> 16) & 0xFF);
  util::logf("WriteLong: %#06X: %s", addr, util::HexLong(value).data());
  return absl::OkStatus();
}

absl::Status Rom::WriteVector(int addr, std::vector<uint8_t> data) {
  if (addr + static_cast<int>(data.size()) >
      static_cast<int>(rom_data_.size())) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Attempt to write vector value failed, address %d out of range", addr));
  }
  for (int i = 0; i < static_cast<int>(data.size()); i++) {
    rom_data_[addr + i] = data[i];
  }
  util::logf("WriteVector: %#06X: %s", addr, util::HexByte(data[0]).data());
  return absl::OkStatus();
}

absl::Status Rom::WriteColor(uint32_t address, const gfx::SnesColor &color) {
  uint16_t bgr = ((color.snes() >> 10) & 0x1F) | ((color.snes() & 0x1F) << 10) |
                 (color.snes() & 0x7C00);

  // Write the 16-bit color value to the ROM at the specified address
  util::logf("WriteColor: %#06X: %s", address, util::HexWord(bgr).data());
  return WriteShort(address, bgr);
}

std::shared_ptr<Rom> SharedRom::shared_rom_ = nullptr;

}  // namespace yaze
