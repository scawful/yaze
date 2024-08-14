#ifndef YAZE_APP_ROM_H
#define YAZE_APP_ROM_H

#include <SDL.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <stack>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "app/core/common.h"
#include "app/core/constants.h"
#include "app/core/labeling.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/compression.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"

namespace yaze {
namespace app {

// Define an enum class for the different versions of the game
enum class Z3_Version {
  US = 1,     // US version
  JP = 2,     // JP version
  SD = 3,     // Super Donkey Proto (Experimental)
  RANDO = 4,  // Randomizer (Unimplemented)
};

/**
 * @brief A struct to hold version constants for each version of the game.
 */
struct VersionConstants {
  uint32_t kGfxAnimatedPointer;
  uint32_t kOverworldGfxGroups1;
  uint32_t kOverworldGfxGroups2;
  uint32_t kCompressedAllMap32PointersHigh;
  uint32_t kCompressedAllMap32PointersLow;
  uint32_t overworldMapPaletteGroup;
  uint32_t overlayPointers;
  uint32_t overlayPointersBank;
  uint32_t overworldTilesType;
  uint32_t kOverworldGfxPtr1;
  uint32_t kOverworldGfxPtr2;
  uint32_t kOverworldGfxPtr3;
  uint32_t kMap32TileTL;
  uint32_t kMap32TileTR;
  uint32_t kMap32TileBL;
  uint32_t kMap32TileBR;
  uint32_t kSpriteBlocksetPointer;
  uint32_t kDungeonPalettesGroups;
};

/**
 * @brief A map of version constants for each version of the game.
 */
static const std::map<Z3_Version, VersionConstants> kVersionConstantsMap = {
    {Z3_Version::US,
     {
         0x10275,  // kGfxAnimatedPointer
         0x5D97,   // kOverworldGfxGroups1
         0x6073,   // kOverworldGfxGroups2
         0x1794D,  // kCompressedAllMap32PointersHigh
         0x17B2D,  // kCompressedAllMap32PointersLow
         0x75504,  // overworldMapPaletteGroup
         0x77664,  // overlayPointers
         0x0E,     // overlayPointersBank
         0x71459,  // overworldTilesType
         0x4F80,   // kOverworldGfxPtr1
         0x505F,   // kOverworldGfxPtr2
         0x513E,   // kOverworldGfxPtr3
         0x18000,  // kMap32TileTL
         0x1B400,  // kMap32TileTR
         0x20000,  // kMap32TileBL
         0x23400,  // kMap32TileBR
         0x5B57,   // kSpriteBlocksetPointer
         0x75460,  // kDungeonPalettesGroups
     }},
    {Z3_Version::JP,
     {
         0x10624,  // kGfxAnimatedPointer
         0x5DD7,   // kOverworldGfxGroups1
         0x60B3,   // kOverworldGfxGroups2
         0x176B1,  // kCompressedAllMap32PointersHigh
         0x17891,  // kCompressedAllMap32PointersLow
         0x67E74,  // overworldMapPaletteGroup
         0x3FAF4,  // overlayPointers
         0x07,     // overlayPointersBank
         0x7FD94,  // overworldTilesType
         0x4FC0,   // kOverworldGfxPtr1
         0x509F,   // kOverworldGfxPtr2
         0x517E,   // kOverworldGfxPtr3
         0x18000,  // kMap32TileTL
         0x1B3C0,  // kMap32TileTR
         0x20000,  // kMap32TileBL
         0x233C0,  // kMap32TileBR
         0x5B97,   // kSpriteBlocksetPointer
         0x67DD0,  // kDungeonPalettesGroups
     }}};

constexpr uint32_t kNumGfxSheets = 223;
constexpr uint32_t kNumLinkSheets = 14;
constexpr uint32_t kNormalGfxSpaceStart = 0x87000;
constexpr uint32_t kNormalGfxSpaceEnd = 0xC4200;
constexpr uint32_t kFontSpriteLocation = 0x70000;
constexpr uint32_t kGfxGroupsPointer = 0x6237;

/**
 * @brief The Rom class is used to load, save, and modify Rom data.
 */
class Rom : public core::ExperimentFlags {
 public:
  /**
   * @brief Loads 2bpp graphics from Rom data.
   *
   * This function loads 2bpp graphics from Rom data by iterating over a list of
   * sheet IDs, decompressing the sheet data, converting it to 8bpp format, and
   * appending the converted sheet data to a byte vector.
   *
   */
  absl::StatusOr<Bytes> Load2BppGraphics();

  /**
   * @brief Loads the players 4bpp graphics sheet from Rom data.
   */
  absl::Status LoadLinkGraphics();

  /**
   * @brief This function iterates over all graphics sheets in the Rom and loads
   * them into memory. Depending on the sheet's index, it may be uncompressed or
   * compressed using the LC-LZ2 algorithm. The uncompressed sheets are 3 bits
   * per pixel (BPP), while the compressed sheets are 4 BPP. The loaded graphics
   * data is converted to 8 BPP and stored in a bitmap.
   *
   * The graphics sheets are divided into the following ranges:
   *
   * | Range   | Compression Type | Decompressed Size | Number of Chars |
   * |---------|------------------|------------------|-----------------|
   * | 0-112   | Compressed 3bpp BGR | 0x600 chars | Decompressed each |
   * | 113-114 | Compressed 2bpp | 0x800 chars | Decompressed each |
   * | 115-126 | Uncompressed 3bpp sprites | 0x600 chars | Each |
   * | 127-217 | Compressed 3bpp sprites | 0x600 chars | Decompressed each |
   * | 218-222 | Compressed 2bpp | 0x800 chars | Decompressed each |
   *
   */
  absl::Status LoadAllGraphicsData();

  /**
   * Load Rom data from a file.
   *
   * @param filename The name of the file to load.
   * @param z3_load Whether to load data specific to Zelda 3.
   *
   */
  absl::Status LoadFromFile(const std::string& filename, bool z3_load = true);
  absl::Status LoadFromPointer(uchar* data, size_t length, bool z3_load = true);
  absl::Status LoadFromBytes(const Bytes& data);

  /**
   * @brief Saves the Rom data to a file
   *
   * @param backup If true, creates a backup file with timestamp in its name
   * @param filename The name of the file to save the Rom data to
   *
   * @return absl::Status Returns an OK status if the save was successful,
   * otherwise returns an error status
   */
  absl::Status SaveToFile(bool backup, bool save_new = false,
                          std::string filename = "");

  /**
   * Saves the given palette to the Rom if any of its colors have been modified.
   *
   * @param index The index of the palette to save.
   * @param group_name The name of the group containing the palette.
   * @param palette The palette to save.
   */
  absl::Status SavePalette(int index, const std::string& group_name,
                           gfx::SnesPalette& palette);

  /**
   * @brief Saves all palettes in the Rom.
   *
   * This function iterates through all palette groups and all palettes in each
   * group, and saves each palette using the SavePalette() function.
   */
  absl::Status SaveAllPalettes();

  // Read functions
  absl::StatusOr<uint8_t> ReadByte(int offset) {
    if (offset >= rom_data_.size()) {
      return absl::InvalidArgumentError("Offset out of range");
    }
    return rom_data_[offset];
  }

  absl::StatusOr<uint16_t> ReadWord(int offset) {
    if (offset + 1 >= rom_data_.size()) {
      return absl::InvalidArgumentError("Offset out of range");
    }
    auto result = (uint16_t)(rom_data_[offset] | (rom_data_[offset + 1] << 8));
    return result;
  }

  uint16_t toint16(int offset) {
    return (uint16_t)(rom_data_[offset] | (rom_data_[offset + 1] << 8));
  }

  absl::StatusOr<uint32_t> ReadLong(int offset) {
    if (offset + 2 >= rom_data_.size()) {
      return absl::InvalidArgumentError("Offset out of range");
    }
    auto result = (uint32_t)(rom_data_[offset] | (rom_data_[offset + 1] << 8) |
                             (rom_data_[offset + 2] << 16));
    return result;
  }

  absl::StatusOr<std::vector<uint8_t>> ReadByteVector(uint32_t offset,
                                                      uint32_t length) {
    if (offset + length > rom_data_.size()) {
      return absl::InvalidArgumentError("Offset and length out of range");
    }
    std::vector<uint8_t> result;
    for (int i = offset; i < offset + length; i++) {
      result.push_back(rom_data_[i]);
    }
    return result;
  }

  absl::StatusOr<gfx::Tile16> ReadTile16(uint32_t tile16_id) {
    // Skip 8 bytes per tile.
    auto tpos = 0x78000 + (tile16_id * 0x08);
    gfx::Tile16 tile16;
    ASSIGN_OR_RETURN(auto new_tile0, ReadWord(tpos))
    tile16.tile0_ = gfx::WordToTileInfo(new_tile0);
    tpos += 2;
    ASSIGN_OR_RETURN(auto new_tile1, ReadWord(tpos))
    tile16.tile1_ = gfx::WordToTileInfo(new_tile1);
    tpos += 2;
    ASSIGN_OR_RETURN(auto new_tile2, ReadWord(tpos))
    tile16.tile2_ = gfx::WordToTileInfo(new_tile2);
    tpos += 2;
    ASSIGN_OR_RETURN(auto new_tile3, ReadWord(tpos))
    tile16.tile3_ = gfx::WordToTileInfo(new_tile3);
    return tile16;
  }

  absl::Status WriteTile16(int tile16_id, const gfx::Tile16& tile) {
    // Skip 8 bytes per tile.
    auto tpos = 0x78000 + (tile16_id * 0x08);
    RETURN_IF_ERROR(WriteShort(tpos, gfx::TileInfoToWord(tile.tile0_)));
    tpos += 2;
    RETURN_IF_ERROR(WriteShort(tpos, gfx::TileInfoToWord(tile.tile1_)));
    tpos += 2;
    RETURN_IF_ERROR(WriteShort(tpos, gfx::TileInfoToWord(tile.tile2_)));
    tpos += 2;
    RETURN_IF_ERROR(WriteShort(tpos, gfx::TileInfoToWord(tile.tile3_)));
    return absl::OkStatus();
  }

  // Write functions
  absl::Status Write(int addr, int value) {
    if (addr >= rom_data_.size()) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Attempt to write %d value failed, address %d out of range", value,
          addr));
    }
    rom_data_[addr] = value;
    return absl::OkStatus();
  }

  absl::Status WriteByte(int addr, uint8_t value) {
    if (addr >= rom_data_.size()) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Attempt to write byte %#02x value failed, address %d out of range",
          value, addr));
    }
    rom_data_[addr] = value;
    std::string log_str = absl::StrFormat("WriteByte: %#06X: %s", addr,
                                          core::UppercaseHexByte(value).data());
    core::Logger::log(log_str);
    return absl::OkStatus();
  }

  absl::Status WriteWord(int addr, uint16_t value) {
    if (addr + 1 >= rom_data_.size()) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Attempt to write word %#04x value failed, address %d out of range",
          value, addr));
    }
    rom_data_[addr] = (uint8_t)(value & 0xFF);
    rom_data_[addr + 1] = (uint8_t)((value >> 8) & 0xFF);
    core::Logger::log(absl::StrFormat("WriteWord: %#06X: %s", addr,
                                      core::UppercaseHexWord(value)));
    return absl::OkStatus();
  }

  absl::Status WriteShort(int addr, uint16_t value) {
    if (addr + 1 >= rom_data_.size()) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Attempt to write short %#04x value failed, address %d out of range",
          value, addr));
    }
    rom_data_[addr] = (uint8_t)(value & 0xFF);
    rom_data_[addr + 1] = (uint8_t)((value >> 8) & 0xFF);
    core::Logger::log(absl::StrFormat("WriteShort: %#06X: %s", addr,
                                      core::UppercaseHexWord(value)));
    return absl::OkStatus();
  }

  absl::Status WriteLong(uint32_t addr, uint32_t value) {
    if (addr + 2 >= rom_data_.size()) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Attempt to write long %#06x value failed, address %d out of range",
          value, addr));
    }
    rom_data_[addr] = (uint8_t)(value & 0xFF);
    rom_data_[addr + 1] = (uint8_t)((value >> 8) & 0xFF);
    rom_data_[addr + 2] = (uint8_t)((value >> 16) & 0xFF);
    core::Logger::log(absl::StrFormat("WriteLong: %#06X: %s", addr,
                                      core::UppercaseHexLong(value)));
    return absl::OkStatus();
  }

  absl::Status WriteVector(int addr, std::vector<uint8_t> data) {
    if (addr + data.size() > rom_data_.size()) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Attempt to write vector value failed, address %d out of range",
          addr));
    }
    for (int i = 0; i < data.size(); i++) {
      rom_data_[addr + i] = data[i];
    }
    core::Logger::log(absl::StrFormat("WriteVector: %#06X: %s", addr,
                                      core::UppercaseHexByte(data[0])));
    return absl::OkStatus();
  }

  absl::Status WriteColor(uint32_t address, const gfx::SnesColor& color) {
    uint16_t bgr = ((color.snes() >> 10) & 0x1F) |
                   ((color.snes() & 0x1F) << 10) | (color.snes() & 0x7C00);

    // Write the 16-bit color value to the ROM at the specified address
    core::Logger::log(absl::StrFormat("WriteColor: %#06X: %s", address,
                                      core::UppercaseHexWord(bgr)));
    return WriteShort(address, bgr);
  }

  template <typename... Args>
  absl::Status WriteTransaction(Args... args) {
    absl::Status status;
    // Fold expression to apply the Write function on each argument
    ((status = WriteHelper(args)), ...);
    return status;
  }

  template <typename T, typename... Args>
  absl::Status ReadTransaction(T& var, int address, Args&&... args) {
    absl::Status status = ReadHelper<T>(var, address);
    if (!status.ok()) {
      return status;
    }

    if constexpr (sizeof...(args) > 0) {
      status = ReadTransaction(std::forward<Args>(args)...);
    }

    return status;
  }

  void Expand(int size) {
    rom_data_.resize(size);
    size_ = size;
  }

  absl::Status Close() {
    rom_data_.clear();
    size_ = 0;
    is_loaded_ = false;
    return absl::OkStatus();
  }

  core::ResourceLabelManager* resource_label() {
    return &resource_label_manager_;
  }
  VersionConstants version_constants() const {
    return kVersionConstantsMap.at(version_);
  }

  // Full graphical data for the game
  Bytes graphics_buffer() const { return graphics_buffer_; }

  auto link_graphics() { return link_graphics_; }
  auto mutable_link_graphics() { return &link_graphics_; }

  auto gfx_sheets() { return graphics_sheets_; }
  auto mutable_gfx_sheets() { return &graphics_sheets_; }

  auto palette_group() { return palette_groups_; }
  auto mutable_palette_group() { return &palette_groups_; }
  auto dungeon_palette(int i) { return palette_groups_.dungeon_main[i]; }
  auto mutable_dungeon_palette(int i) {
    return palette_groups_.dungeon_main.mutable_palette(i);
  }

  auto title() const { return title_; }
  auto size() const { return size_; }
  auto begin() { return rom_data_.begin(); }
  auto end() { return rom_data_.end(); }
  auto data() { return rom_data_.data(); }
  auto vector() const { return rom_data_; }
  auto filename() const { return filename_; }
  auto is_loaded() const {
    if (!absl::StrContains(filename_, ".sfc") &&
        !absl::StrContains(filename_, ".smc")) {
      return false;
    }
    return is_loaded_;
  }
  auto set_filename(std::string name) { filename_ = name; }
  auto version() const { return version_; }

  uint8_t& operator[](int i) {
    if (i > size_) {
      std::cout << "ROM: Index " << i << " out of bounds, size: " << size_
                << std::endl;
      return rom_data_[0];
    }
    return rom_data_[i];
  }

  std::vector<std::vector<uint8_t>> main_blockset_ids;
  std::vector<std::vector<uint8_t>> room_blockset_ids;
  std::vector<std::vector<uint8_t>> spriteset_ids;
  std::vector<std::vector<uint8_t>> paletteset_ids;

  void LoadGfxGroups();
  void SaveGroupsToRom();

 private:
  struct WriteAction {
    int address;
    std::variant<int, uint8_t, uint16_t, short, std::vector<uint8_t>,
                 gfx::SnesColor, std::vector<gfx::SnesColor>>
        value;
  };

  absl::Status WriteHelper(const WriteAction& action) {
    if (std::holds_alternative<uint8_t>(action.value)) {
      return Write(action.address, std::get<uint8_t>(action.value));
    } else if (std::holds_alternative<uint16_t>(action.value) ||
               std::holds_alternative<short>(action.value)) {
      return WriteShort(action.address, std::get<uint16_t>(action.value));
    } else if (std::holds_alternative<std::vector<uint8_t>>(action.value)) {
      return WriteVector(action.address,
                         std::get<std::vector<uint8_t>>(action.value));
    } else if (std::holds_alternative<gfx::SnesColor>(action.value)) {
      return WriteColor(action.address, std::get<gfx::SnesColor>(action.value));
    } else if (std::holds_alternative<std::vector<gfx::SnesColor>>(
                   action.value)) {
      return absl::UnimplementedError(
          "WriteHelper: std::vector<gfx::SnesColor>");
    }
    auto error_message = absl::StrFormat("Invalid write argument type: %s",
                                         typeid(action.value).name());
    throw std::runtime_error(error_message);
    return absl::InvalidArgumentError(error_message);
  }

  template <typename T>
  absl::Status ReadHelper(T& var, int address) {
    if constexpr (std::is_same_v<T, uint8_t>) {
      ASSIGN_OR_RETURN(auto result, ReadByte(address));
      var = result;
    } else if constexpr (std::is_same_v<T, uint16_t>) {
      ASSIGN_OR_RETURN(auto result, ReadWord(address));
      var = result;
    } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
      ASSIGN_OR_RETURN(auto result, ReadByteVector(address, var.size()));
      var = result;
    }
    return absl::OkStatus();
  }

  long size_ = 0;
  bool is_loaded_ = false;
  bool has_header_ = false;
  std::string title_ = "ROM Not Loaded";
  std::string filename_ = "";

  // Full contiguous rom space
  Bytes rom_data_;

  // Full contiguous graphics space
  Bytes graphics_buffer_;

  // All graphics sheets in the game
  std::array<gfx::Bitmap, kNumGfxSheets> graphics_sheets_;

  // All graphics sheets for Link
  std::array<gfx::Bitmap, kNumLinkSheets> link_graphics_;

  // Label manager for unique resource names.
  core::ResourceLabelManager resource_label_manager_;

  // Link's palette
  gfx::SnesPalette link_palette_;

  // All palette groups in the game
  gfx::PaletteGroupMap palette_groups_;

  // Version of the game
  Z3_Version version_ = Z3_Version::US;
};

/**
 * @brief A class to hold a shared pointer to a Rom object.
 */
class SharedRom {
 public:
  SharedRom() = default;
  virtual ~SharedRom() = default;

  std::shared_ptr<Rom> shared_rom() {
    if (!shared_rom_) {
      shared_rom_ = std::make_shared<Rom>();
    }
    return shared_rom_;
  }

  auto rom() {
    if (!shared_rom_) {
      shared_rom_ = std::make_shared<Rom>();
    }
    Rom* rom = shared_rom_.get();
    return rom;
  }

  // private:
  static std::shared_ptr<Rom> shared_rom_;
};

}  // namespace app
}  // namespace yaze

#endif
