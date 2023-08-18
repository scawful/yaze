#ifndef YAZE_APP_ROM_H
#define YAZE_APP_ROM_H

#include <SDL.h>
#include <asar/src/asar/interface-lib.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "app/core/common.h"
#include "app/core/constants.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"

namespace yaze {
namespace app {

constexpr int kOverworldGraphicsPos1 = 0x4F80;
constexpr int kOverworldGraphicsPos2 = 0x505F;
constexpr int kOverworldGraphicsPos3 = 0x513E;
constexpr int kTile32Num = 4432;
constexpr int kTitleStringOffset = 0x7FC0;
constexpr int kTitleStringLength = 20;
constexpr int kSNESToPCOffset = 0x138000;

constexpr uint32_t kNumGfxSheets = 223;
constexpr uint32_t kNormalGfxSpaceStart = 0x87000;
constexpr uint32_t kNormalGfxSpaceEnd = 0xC4200;
constexpr uint32_t kPtrTableStart = 0x4F80;
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
  // Load functions
  absl::StatusOr<Bytes> Load2bppGraphics();
  absl::Status LoadAllGraphicsData();
  absl::Status LoadFromFile(const absl::string_view& filename,
                            bool z3_load = true);
  absl::Status LoadFromPointer(uchar* data, size_t length);
  absl::Status LoadFromBytes(const Bytes& data);
  void LoadAllPalettes();

  // Save functions
  absl::Status SaveToFile(bool backup, absl::string_view filename = "");
  absl::Status UpdatePaletteColor(const std::string& groupName,
                                  size_t paletteIndex, size_t colorIndex,
                                  const gfx::SNESColor& newColor);
  void SaveAllPalettes();

  // Read functions
  gfx::SNESColor ReadColor(int offset);
  gfx::SNESPalette ReadPalette(int offset, int num_colors);

  // Write functions
  void Write(int addr, int value) { rom_data_[addr] = value; }

  void WriteShort(int addr, int value) {
    rom_data_[addr] = (uint16_t)(value & 0xFF);
    rom_data_[addr + 1] = (uint16_t)((value >> 8) & 0xFF);
  }

  void WriteColor(uint32_t address, const gfx::SNESColor& color) {
    uint16_t bgr = ((color.GetSNES() >> 10) & 0x1F) |
                   ((color.GetSNES() & 0x1F) << 10) |
                   (color.GetSNES() & 0x7C00);

    // Write the 16-bit color value to the ROM at the specified address
    WriteShort(address, bgr);
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
  auto char_data() { return reinterpret_cast<char*>(rom_data_.data()); }

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
  uchar title_[21] = "ROM Not Loaded";
  std::string filename_;

  Bytes rom_data_;
  Bytes graphics_buffer_;

  core::Z3_Version version_;
  gfx::BitmapTable graphics_bin_;

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
    ROM *rom = shared_rom_.get();
    return rom;
  }

 private:
  static std::shared_ptr<ROM> shared_rom_;
};

}  // namespace app
}  // namespace yaze

#endif