#ifndef YAZE_APP_ROM_H
#define YAZE_APP_ROM_H

#include <SDL.h>
#include <asar/src/asar/interface-lib.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
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

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "app/core/common.h"
#include "app/core/constants.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"

#define BUILD_HEADER(command, length) (command << 5) + (length - 1)

namespace yaze {
namespace app {

constexpr int kCommandDirectCopy = 0;
constexpr int kCommandByteFill = 1;
constexpr int kCommandWordFill = 2;
constexpr int kCommandIncreasingFill = 3;
constexpr int kCommandRepeatingBytes = 4;
constexpr int kCommandLongLength = 7;
constexpr int kMaxLengthNormalHeader = 32;
constexpr int kMaxLengthCompression = 1024;
constexpr int kNintendoMode1 = 0;
constexpr int kNintendoMode2 = 1;
constexpr int kTile32Num = 4432;
constexpr int kTitleStringOffset = 0x7FC0;
constexpr int kTitleStringLength = 20;
constexpr int kOverworldGraphicsPos1 = 0x4F80;
constexpr int kOverworldGraphicsPos2 = 0x505F;
constexpr int kOverworldGraphicsPos3 = 0x513E;
constexpr int kSnesByteMax = 0xFF;
constexpr int kCommandMod = 0x07;
constexpr int kExpandedMod = 0xE0;
constexpr int kExpandedLengthMod = 0x3FF;
constexpr int kNormalLengthMod = 0x1F;
constexpr int kCompressionStringMod = 7 << 5;

const std::string kMosaicChangeOffset = "$02AADB";
constexpr int kSNESToPCOffset = 0x138000;

using CommandArgumentArray = std::array<std::array<char, 2>, 5>;
using CommandSizeArray = std::array<uint, 5>;
using DataSizeArray = std::array<uint, 5>;
struct CompressionPiece {
  char command;
  int length;
  int argument_length;
  std::string argument;
  std::shared_ptr<CompressionPiece> next = nullptr;
  CompressionPiece() = default;
  CompressionPiece(int cmd, int len, std::string args, int arg_len)
      : command(cmd), length(len), argument_length(arg_len), argument(args) {}
};
using CompressionPiece = struct CompressionPiece;
using CompressionPiecePointer = std::shared_ptr<CompressionPiece>;

const std::map<std::string, uint32_t> paletteGroupBaseAddresses = {
    {"ow_main", core::overworldPaletteMain},
    {"ow_aux", core::overworldPaletteAuxialiary},
    {"ow_animated", core::overworldPaletteAnimated},
    {"hud", core::hudPalettes},
    {"global_sprites",
     core::globalSpritePalettesLW},  // Assuming LW is the first palette
    {"armors", core::armorPalettes},
    {"swords", core::swordPalettes},
    {"shields", core::shieldPalettes},
    {"sprites_aux1", core::spritePalettesAux1},
    {"sprites_aux2", core::spritePalettesAux2},
    {"sprites_aux3", core::spritePalettesAux3},
    {"dungeon_main", core::dungeonMainPalettes},
    {"grass", core::hardcodedGrassLW},  // Assuming LW is the first color
    {"3d_object",
     core::triforcePalette},  // Assuming triforcePalette is the first palette
    {"ow_mini_map", core::overworldMiniMapPalettes},
};

const std::map<std::string, size_t> paletteGroupColorCounts = {
    {"ow_main", 35},
    {"ow_aux", 21},
    {"ow_animated", 7},
    {"hud", 32},
    {"global_sprites", 60},  // Assuming both LW and DW
                             // palettes have the same
                             // color count
    {"armors", 15},
    {"swords", 3},
    {"shields", 4},
    {"sprites_aux1", 7},
    {"sprites_aux2", 7},
    {"sprites_aux3", 7},
    {"dungeon_main", 90},
    {"grass", 1},      // Assuming grass palettes are
                       // individual colors
    {"3d_object", 8},  // Assuming both triforcePalette and crystalPalette have
                       // the same color count
    {"ow_mini_map", 128},
};

class ROM {
 public:
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
  absl::Status LoadFromFile(const absl::string_view& filename);
  absl::Status LoadFromPointer(uchar* data, size_t length);
  absl::Status LoadFromBytes(const Bytes& data);
  void LoadAllPalettes();

  // Save functions
  absl::Status SaveToFile(bool backup, absl::string_view filename = "");
  void UpdatePaletteColor(const std::string& groupName, size_t paletteIndex,
                          size_t colorIndex, const gfx::SNESColor& newColor);
  void SaveAllPalettes();

  gfx::SNESColor ReadColor(int offset);
  gfx::SNESPalette ReadPalette(int offset, int num_colors);

  void Write(int addr, int value);
  void WriteShort(int addr, int value);
  void WriteColor(uint32_t address, const gfx::SNESColor& color);

  uint32_t GetPaletteAddress(const std::string& groupName, size_t paletteIndex,
                             size_t colorIndex) const;

  absl::Status ApplyAssembly(const absl::string_view& filename,
                             size_t patch_size);
  absl::Status PatchOverworldMosaic(char mosaic_tiles[core::kNumOverworldMaps],
                                    int routine_offset);

  gfx::BitmapTable GetGraphicsBin() const { return graphics_bin_; }
  auto GetGraphicsBuffer() const { return graphics_buffer_; }

  auto GetPaletteGroup(const std::string& group) {
    return palette_groups_[group];
  }
  auto GetTitle() const { return title; }
  void SetupRenderer(std::shared_ptr<SDL_Renderer> renderer) {
    renderer_ = renderer;
  }
  auto size() const { return size_; }
  auto isLoaded() const { return is_loaded_; }
  auto begin() { return rom_data_.begin(); }
  auto end() { return rom_data_.end(); }
  auto data() { return rom_data_.data(); }
  auto char_data() { return reinterpret_cast<char*>(rom_data_.data()); }

  uchar& operator[](int i) {
    if (i > size_) {
      std::cout << "ROM: Index out of bounds" << std::endl;
      return rom_data_[0];
    }
    return rom_data_[i];
  }
  uchar& operator+(int i) {
    if (i > size_) {
      std::cout << "ROM: Index out of bounds" << std::endl;
      return rom_data_[0];
    }
    return rom_data_[i];
  }
  const uchar* operator&() { return rom_data_.data(); }

  ushort toint16(int offset) {
    return (ushort)((rom_data_[offset + 1]) << 8) | rom_data_[offset];
  }

  void RenderBitmap(gfx::Bitmap* bitmap) const {
    bitmap->CreateTexture(renderer_);
  }

 private:
  long size_ = 0;
  uchar title[21] = "ROM Not Loaded";
  bool isbpp3[223];
  bool is_loaded_ = false;
  std::string filename_;

  Bytes rom_data_;
  Bytes graphics_buffer_;

  gfx::BitmapTable graphics_bin_;

  std::shared_ptr<SDL_Renderer> renderer_;
  std::unordered_map<std::string, gfx::PaletteGroup> palette_groups_;
};

}  // namespace app
}  // namespace yaze

#endif