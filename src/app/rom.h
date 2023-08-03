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
  // Assembly functions
  absl::Status ApplyAssembly(const absl::string_view& filename, uint32_t size);

  // Compression function
  absl::StatusOr<Bytes> Compress(const int start, const int length,
                                 int mode = 1, bool check = false);
  absl::StatusOr<Bytes> CompressGraphics(const int pos, const int length);
  absl::StatusOr<Bytes> CompressOverworld(const int pos, const int length);

  absl::StatusOr<Bytes> Decompress(int offset, int size = 0x800, int mode = 1);
  absl::StatusOr<Bytes> DecompressGraphics(int pos, int size);
  absl::StatusOr<Bytes> DecompressOverworld(int pos, int size);

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
  void Write(int addr, int value);
  void WriteShort(int addr, int value);
  void WriteColor(uint32_t address, const gfx::SNESColor& color);

  Bytes GetGraphicsBuffer() const { return graphics_buffer_; }
  gfx::BitmapTable GetGraphicsBin() const { return graphics_bin_; }

  uint32_t GetPaletteAddress(const std::string& groupName, size_t paletteIndex,
                             size_t colorIndex) const;

  gfx::PaletteGroup GetPaletteGroup(const std::string& group) {
    return palette_groups_[group];
  }

  auto title() const { return title_; }
  auto size() const { return size_; }
  auto begin() { return rom_data_.begin(); }
  auto end() { return rom_data_.end(); }
  auto data() { return rom_data_.data(); }
  auto vector() const { return rom_data_; }
  auto isLoaded() const { return is_loaded_; }
  auto char_data() { return reinterpret_cast<char*>(rom_data_.data()); }

  auto push_back(uchar byte) { rom_data_.push_back(byte); }

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

 private:
  long size_ = 0;
  bool is_loaded_ = false;
  uchar title_[21] = "ROM Not Loaded";
  std::string filename_;

  Bytes rom_data_;
  Bytes graphics_buffer_;

  gfx::BitmapTable graphics_bin_;

  std::shared_ptr<SDL_Renderer> renderer_;
  std::unordered_map<std::string, gfx::PaletteGroup> palette_groups_;
};

class SharedROM {
 public:
  SharedROM() = default;
  virtual ~SharedROM() = default;

 protected:
  std::shared_ptr<ROM> shared_rom_;
};

}  // namespace app
}  // namespace yaze

#endif