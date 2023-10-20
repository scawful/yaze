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
#include <vector>         // for vector

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

enum class Z3_Version {
  US = 1,
  JP = 2,
  SD = 3,
  RANDO = 4,
};

struct VersionConstants {
  uint32_t kGgxAnimatedPointer;
  uint32_t kOverworldGfxGroups1;
  uint32_t kOverworldGfxGroups2;
  // long ptrs all tiles of maps[high/low] (mapid* 3)
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
     }}

};

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

const absl::flat_hash_map<std::string, uint32_t> paletteGroupAddresses = {
    {"ow_main", core::overworldPaletteMain},
    {"ow_aux", core::overworldPaletteAuxialiary},
    {"ow_animated", core::overworldPaletteAnimated},
    {"hud", core::hudPalettes},
    {"global_sprites", core::globalSpritePalettesLW},
    {"armors", core::armorPalettes},
    {"swords", core::swordPalettes},
    {"shields", core::shieldPalettes},
    {"sprites_aux1", core::spritePalettesAux1},
    {"sprites_aux2", core::spritePalettesAux2},
    {"sprites_aux3", core::spritePalettesAux3},
    {"dungeon_main", core::dungeonMainPalettes},
    {"grass", core::hardcodedGrassLW},
    {"3d_object", core::triforcePalette},
    {"ow_mini_map", core::overworldMiniMapPalettes},
};

const absl::flat_hash_map<std::string, uint32_t> paletteGroupColorCounts = {
    {"ow_main", 35},     {"ow_aux", 21},         {"ow_animated", 7},
    {"hud", 32},         {"global_sprites", 60}, {"armors", 15},
    {"swords", 3},       {"shields", 4},         {"sprites_aux1", 7},
    {"sprites_aux2", 7}, {"sprites_aux3", 7},    {"dungeon_main", 90},
    {"grass", 1},        {"3d_object", 8},       {"ow_mini_map", 128},
};

class ROM {
 public:
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
   * @brief Loads all the palettes for the game.
   *
   * This function loads all the palettes for the game, including overworld,
   * HUD, armor, swords, shields, sprites, dungeon, grass, and 3D object
   * palettes. It also adds the loaded palettes to their respective palette
   * groups.
   *
   */
  void LoadAllPalettes();

  // Save functions
  absl::Status UpdatePaletteColor(const std::string& group_name,
                                  size_t palette_index, size_t colorIndex,
                                  const gfx::SNESColor& newColor);
  void SavePalette(int index, const std::string& group_name,
                   gfx::SNESPalette& palette);
  void SaveAllPalettes();

  absl::Status SaveToFile(bool backup, absl::string_view filename = "");

  // Read functions
  gfx::SNESColor ReadColor(int offset);
  gfx::SNESPalette ReadPalette(int offset, int num_colors);
  uint8_t ReadByte(int offset) { return rom_data_[offset]; }
  uint16_t ReadWord(int offset) {
    return (uint16_t)(rom_data_[offset] | (rom_data_[offset + 1] << 8));
  }
  uint16_t ReadShort(int offset) {
    return (uint16_t)(rom_data_[offset] | (rom_data_[offset + 1] << 8));
  }
  std::vector<uint8_t> ReadByteVector(uint32_t offset, uint32_t length) {
    std::vector<uint8_t> result;
    for (int i = offset; i < length; i++) {
      result.push_back(rom_data_[i]);
    }
    return result;
  }

  gfx::Tile16 ReadTile16(uint32_t tile16_id) {
    // Skip 8 bytes per tile.
    auto tpos = 0x78000 + (tile16_id * 0x08);
    gfx::Tile16 tile16;
    tile16.tile0_ = gfx::WordToTileInfo(ReadShort(tpos));
    tpos += 2;
    tile16.tile1_ = gfx::WordToTileInfo(ReadShort(tpos));
    tpos += 2;
    tile16.tile2_ = gfx::WordToTileInfo(ReadShort(tpos));
    tpos += 2;
    tile16.tile3_ = gfx::WordToTileInfo(ReadShort(tpos));
    return tile16;
  }

  void WriteTile16(int tile16_id, const gfx::Tile16& tile) {
    // Skip 8 bytes per tile.
    auto tpos = 0x78000 + (tile16_id * 0x08);
    WriteShort(tpos, gfx::TileInfoToWord(tile.tile0_));
    tpos += 2;
    WriteShort(tpos, gfx::TileInfoToWord(tile.tile1_));
    tpos += 2;
    WriteShort(tpos, gfx::TileInfoToWord(tile.tile2_));
    tpos += 2;
    WriteShort(tpos, gfx::TileInfoToWord(tile.tile3_));
  }

  // Write functions
  void Write(int addr, int value) { rom_data_[addr] = value; }

  void WriteShort(uint32_t addr, uint16_t value) {
    rom_data_[addr] = (uint8_t)(value & 0xFF);
    rom_data_[addr + 1] = (uint8_t)((value >> 8) & 0xFF);
  }

  void WriteVector(int addr, std::vector<uint8_t> data) {
    for (int i = 0; i < data.size(); i++) {
      rom_data_[addr + i] = data[i];
    }
  }

  void WriteColor(uint32_t address, const gfx::SNESColor& color) {
    uint16_t bgr = ((color.GetSNES() >> 10) & 0x1F) |
                   ((color.GetSNES() & 0x1F) << 10) |
                   (color.GetSNES() & 0x7C00);

    // Write the 16-bit color value to the ROM at the specified address
    WriteShort(address, bgr);
  }

  void Expand(int size) {
    rom_data_.resize(size);
    size_ = size;
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

  uint32_t GetPaletteAddress(const std::string& groupName, size_t paletteIndex,
                             size_t colorIndex) const;
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

  std::stack<std::function<void()>> changes_;
  std::shared_ptr<SDL_Renderer> renderer_;
  std::unordered_map<std::string, gfx::PaletteGroup> palette_groups_;
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