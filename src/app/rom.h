#ifndef YAZE_APP_ROM_H
#define YAZE_APP_ROM_H

#include <SDL.h>
#include <asar/src/asar/interface-lib.h>

#include <algorithm>
#include <chrono>
#include <cstddef>  // for size_t
#include <cstdint>  // for uint32_t, uint8_t, uint16_t
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>     // for function
#include <iostream>       // for string, operator<<, basic_...
#include <map>            // for map
#include <memory>         // for shared_ptr, make_shared
#include <stack>          // for stack
#include <string>         // for hash, operator==
#include <unordered_map>  // for unordered_map
#include <variant>
#include <vector>  // for vector

#include "SDL_render.h"                    // for SDL_Renderer
#include "absl/container/flat_hash_map.h"  // for flat_hash_map
#include "absl/status/status.h"            // for Status
#include "absl/status/statusor.h"          // for StatusOr
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"  // for string_view
#include "app/core/common.h"           // for SnesToPc
#include "app/core/constants.h"        // for Bytes, uchar, armorPalettes
#include "app/gfx/bitmap.h"            // for Bitmap, BitmapTable
#include "app/gfx/compression.h"
#include "app/gfx/snes_palette.h"  // for PaletteGroup, SNESColor
#include "app/gfx/snes_tile.h"

namespace yaze {
namespace app {

using PaletteGroupMap = std::unordered_map<std::string, gfx::PaletteGroup>;

// Define an enum class for the different versions of the game
enum class Z3_Version {
  US = 1,
  JP = 2,
  SD = 3,
  RANDO = 4,
};

// Define a struct to hold the version-specific constants
struct VersionConstants {
  uint32_t kGgxAnimatedPointer;
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
};

// Define a map to hold the version constants for each version
static const std::map<Z3_Version, VersionConstants> kVersionConstantsMap = {
    {Z3_Version::US,
     {
         0x10275,  // kGgxAnimatedPointer
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
     }},
    {Z3_Version::JP,
     {
         0x10624,  // kGgxAnimatedPointer
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
     }}};

// Define some constants used throughout the ROM class
constexpr uint32_t kOverworldGraphicsPos1 = 0x4F80;
constexpr uint32_t kOverworldGraphicsPos2 = 0x505F;
constexpr uint32_t kOverworldGraphicsPos3 = 0x513E;
constexpr uint32_t kTile32Num = 4432;
constexpr uint32_t kTitleStringOffset = 0x7FC0;
constexpr uint32_t kTitleStringLength = 20;
constexpr uint32_t kNumGfxSheets = 223;
constexpr uint32_t kNormalGfxSpaceStart = 0x87000;
constexpr uint32_t kNormalGfxSpaceEnd = 0xC4200;
constexpr uint32_t kLinkSpriteLocation = 0x80000;
constexpr uint32_t kFontSpriteLocation = 0x70000;

struct WriteAction {
  int address;
  std::variant<int, uint8_t, uint16_t, std::vector<uint8_t>, gfx::SNESColor>
      value;
};

class ROM {
 public:
  template <typename... Args>
  absl::Status RunTransaction(Args... args) {
    absl::Status status;
    // Fold expression to apply the Write function on each argument
    ((status = WriteHelper(args)), ...);
    return status;
  }

  absl::Status WriteHelper(const WriteAction& action) {
    if (std::holds_alternative<uint8_t>(action.value) ||
        std::holds_alternative<int>(action.value)) {
      return Write(action.address, std::get<uint8_t>(action.value));
    } else if (std::holds_alternative<uint16_t>(action.value)) {
      return WriteShort(action.address, std::get<uint16_t>(action.value));
    } else if (std::holds_alternative<std::vector<uint8_t>>(action.value)) {
      return WriteVector(action.address,
                         std::get<std::vector<uint8_t>>(action.value));
    } else if (std::holds_alternative<gfx::SNESColor>(action.value)) {
      return WriteColor(action.address, std::get<gfx::SNESColor>(action.value));
    }
    return absl::InvalidArgumentError("Invalid write argument type");
  }

  /**
   * Loads 2bpp graphics from ROM data.
   *
   * This function loads 2bpp graphics from ROM data by iterating over a list of
   * sheet IDs, decompressing the sheet data, converting it to 8bpp format, and
   * appending the converted sheet data to a byte vector.
   *
   */
  absl::StatusOr<Bytes> Load2BppGraphics();

  /**
   * This function iterates over all graphics sheets in the ROM and loads them
   * into memory. Depending on the sheet's index, it may be uncompressed or
   * compressed using the LC-LZ2 algorithm. The uncompressed sheets are 3 bits
   * per pixel (BPP), while the compressed sheets are 4 BPP. The loaded graphics
   * data is converted to 8 BPP and stored in a bitmap.
   *
   * The graphics sheets are divided into the following ranges:
   * 0-112 -> compressed 3bpp bgr -> (decompressed each) 0x600 chars
   * 113-114 -> compressed 2bpp -> (decompressed each) 0x800 chars
   * 115-126 -> uncompressed 3bpp sprites -> (each) 0x600 chars
   * 127-217 -> compressed 3bpp sprites -> (decompressed each) 0x600 chars
   * 218-222 -> compressed 2bpp -> (decompressed each) 0x800 chars
   *
   */
  absl::Status LoadAllGraphicsData();

  /**
   * @brief Loads all the palettes for the game.
   *
   * This function loads all the palettes for the game, including overworld,
   * HUD, armor, swords, shields, sprites, dungeon, grass, and 3D object
   * palettes. It also adds the loaded palettes to their respective palette
   * groups.
   *
   */
  absl::Status LoadAllPalettes();

  /**
   * Load ROM data from a file.
   *
   * @param filename The name of the file to load.
   * @param z3_load Whether to load data specific to Zelda 3.
   *
   */
  absl::Status LoadFromFile(const absl::string_view& filename,
                            bool z3_load = true);
  absl::Status LoadFromPointer(uchar* data, size_t length);
  absl::Status LoadFromBytes(const Bytes& data);

  /**
   * @brief Saves the ROM data to a file
   *
   * @param backup If true, creates a backup file with timestamp in its name
   * @param filename The name of the file to save the ROM data to
   *
   * @return absl::Status Returns an OK status if the save was successful,
   * otherwise returns an error status
   */
  absl::Status SaveToFile(bool backup, absl::string_view filename = "");

  /**
   * Saves the given palette to the ROM if any of its colors have been modified.
   *
   * @param index The index of the palette to save.
   * @param group_name The name of the group containing the palette.
   * @param palette The palette to save.
   */
  void SavePalette(int index, const std::string& group_name,
                   gfx::SNESPalette& palette);

  /**
   * @brief Saves all palettes in the ROM.
   *
   * This function iterates through all palette groups and all palettes in each
   * group, and saves each palette using the SavePalette() function.
   */
  void SaveAllPalettes();

  /**
   * @brief Updates a color in a specified palette group.
   *
   * This function updates the color at the specified `colorIndex` in the
   * palette at `palette_index` within the palette group with the given
   * `group_name`. If the group, palette, or color indices are invalid, an error
   * is returned.
   *
   * @param group_name The name of the palette group to update.
   * @param palette_index The index of the palette within the group to update.
   * @param colorIndex The index of the color within the palette to update.
   * @param newColor The new color value to set.
   *
   * @return An `absl::Status` indicating whether the update was successful.
   *         Returns `absl::OkStatus()` if successful, or an error status if the
   *         group, palette, or color indices are invalid.
   */
  absl::Status UpdatePaletteColor(const std::string& group_name,
                                  size_t palette_index, size_t colorIndex,
                                  const gfx::SNESColor& newColor);

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

  absl::StatusOr<uint16_t> ReadShort(int offset) {
    if (offset + 1 >= rom_data_.size()) {
      return absl::InvalidArgumentError("Offset out of range");
    }
    auto result = (uint16_t)(rom_data_[offset] | (rom_data_[offset + 1] << 8));
    return result;
  }

  absl::StatusOr<std::vector<uint8_t>> ReadByteVector(uint32_t offset,
                                                      uint32_t length) {
    if (offset + length > rom_data_.size()) {
      return absl::InvalidArgumentError("Offset and length out of range");
    }
    std::vector<uint8_t> result;
    for (int i = offset; i < length; i++) {
      result.push_back(rom_data_[i]);
    }
    return result;
  }

  absl::StatusOr<gfx::Tile16> ReadTile16(uint32_t tile16_id) {
    // Skip 8 bytes per tile.
    auto tpos = 0x78000 + (tile16_id * 0x08);
    gfx::Tile16 tile16;
    ASSIGN_OR_RETURN(auto new_tile0, ReadShort(tpos))
    tile16.tile0_ = gfx::WordToTileInfo(new_tile0);
    tpos += 2;
    ASSIGN_OR_RETURN(auto new_tile1, ReadShort(tpos))
    tile16.tile1_ = gfx::WordToTileInfo(new_tile1);
    tpos += 2;
    ASSIGN_OR_RETURN(auto new_tile2, ReadShort(tpos))
    tile16.tile2_ = gfx::WordToTileInfo(new_tile2);
    tpos += 2;
    ASSIGN_OR_RETURN(auto new_tile3, ReadShort(tpos))
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
      return absl::InvalidArgumentError("Address out of range");
    }
    rom_data_[addr] = value;
    return absl::OkStatus();
  }

  absl::Status WriteShort(uint32_t addr, uint16_t value) {
    if (addr + 1 >= rom_data_.size()) {
      return absl::InvalidArgumentError("Address out of range");
    }
    rom_data_[addr] = (uint8_t)(value & 0xFF);
    rom_data_[addr + 1] = (uint8_t)((value >> 8) & 0xFF);
    return absl::OkStatus();
  }

  absl::Status WriteVector(int addr, std::vector<uint8_t> data) {
    if (addr + data.size() > rom_data_.size()) {
      return absl::InvalidArgumentError("Address and data size out of range");
    }
    for (int i = 0; i < data.size(); i++) {
      rom_data_[addr + i] = data[i];
    }
    return absl::OkStatus();
  }

  absl::Status WriteColor(uint32_t address, const gfx::SNESColor& color) {
    uint16_t bgr = ((color.GetSNES() >> 10) & 0x1F) |
                   ((color.GetSNES() & 0x1F) << 10) |
                   (color.GetSNES() & 0x7C00);

    // Write the 16-bit color value to the ROM at the specified address
    return WriteShort(address, bgr);
  }

  void Expand(int size) {
    rom_data_.resize(size);
    size_ = size;
  }

  absl::Status Reload() {
    if (filename_.empty()) {
      return absl::InvalidArgumentError("No filename specified");
    }
    return LoadFromFile(filename_);
  }

  absl::Status Close() {
    rom_data_.clear();
    size_ = 0;
    is_loaded_ = false;
    return absl::OkStatus();
  }

  void QueueChanges(std::function<void()> const& function) {
    changes_.push(function);
  }

  VersionConstants GetVersionConstants() const {
    return kVersionConstantsMap.at(version_);
  }

  int GetGraphicsAddress(const uchar* data, uint8_t addr) const {
    auto part_one = data[GetVersionConstants().kOverworldGfxPtr1 + addr] << 16;
    auto part_two = data[GetVersionConstants().kOverworldGfxPtr2 + addr] << 8;
    auto part_three = data[GetVersionConstants().kOverworldGfxPtr3 + addr];
    auto snes_addr = (part_one | part_two | part_three);
    return core::SnesToPc(snes_addr);
  }

  gfx::PaletteGroup GetPaletteGroup(const std::string& group) {
    return palette_groups_[group];
  }
  Bytes GetGraphicsBuffer() const { return graphics_buffer_; }
  gfx::BitmapTable GetGraphicsBin() const { return graphics_bin_; }

  auto title() const { return title_; }
  auto size() const { return size_; }
  auto begin() { return rom_data_.begin(); }
  auto end() { return rom_data_.end(); }
  auto data() { return rom_data_.data(); }
  auto vector() const { return rom_data_; }
  auto filename() const { return filename_; }
  auto isLoaded() const { return is_loaded_; }
  auto char_data() {
    return static_cast<char*>(static_cast<void*>(rom_data_.data()));
  }

  auto push_back(uchar byte) { rom_data_.push_back(byte); }

  auto version() const { return version_; }

  void malloc(int n_bytes) {
    rom_data_.clear();
    rom_data_.reserve(n_bytes);
    rom_data_.resize(n_bytes);
    for (int i = 0; i < n_bytes; i++) {
      rom_data_.push_back(0x00);
    }
    size_ = n_bytes;
  }

  uchar& operator[](int i) {
    if (i > size_) {
      std::cout << "ROM: Index " << i << " out of bounds, size: " << size_
                << std::endl;
      return rom_data_[0];
    }
    return rom_data_[i];
  }
  uchar& operator+(int i) {
    if (i > size_) {
      std::cout << "ROM: Index " << i << " out of bounds, size: " << size_
                << std::endl;
      return rom_data_[0];
    }
    return rom_data_[i];
  }
  const uchar* operator&() { return rom_data_.data(); }

  ushort toint16(int offset) {
    return (ushort)((rom_data_[offset + 1]) << 8) | rom_data_[offset];
  }

  void SetupRenderer(std::shared_ptr<SDL_Renderer> renderer) {
    renderer_ = renderer;
  }

  void RenderBitmap(gfx::Bitmap* bitmap) const {
    bitmap->CreateTexture(renderer_);
  }

  void UpdateBitmap(gfx::Bitmap* bitmap) const {
    bitmap->UpdateTexture(renderer_);
  }

 private:
  long size_ = 0;
  bool is_loaded_ = false;
  bool has_header_ = false;
  uchar title_[21] = "ROM Not Loaded";
  std::string filename_;

  Bytes rom_data_;
  Bytes graphics_buffer_;

  Z3_Version version_ = Z3_Version::US;
  gfx::BitmapTable graphics_bin_;
  PaletteGroupMap palette_groups_;

  std::stack<std::function<void()>> changes_;
  std::shared_ptr<SDL_Renderer> renderer_;
};

class SharedROM {
 public:
  SharedROM() = default;
  virtual ~SharedROM() = default;

  std::shared_ptr<ROM> shared_rom() {
    if (!shared_rom_) {
      shared_rom_ = std::make_shared<ROM>();
    }
    return shared_rom_;
  }

  auto rom() {
    if (!shared_rom_) {
      shared_rom_ = std::make_shared<ROM>();
    }
    ROM* rom = shared_rom_.get();
    return rom;
  }

 private:
  static std::shared_ptr<ROM> shared_rom_;
};

}  // namespace app
}  // namespace yaze

#endif