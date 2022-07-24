#ifndef YAZE_APP_ROM_H
#define YAZE_APP_ROM_H

#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "app/core/common.h"
#include "app/core/constants.h"
#include "app/gfx/pseudo_vram.h"
#include "app/gfx/snes_tile.h"

namespace yaze {
namespace app {

constexpr int kCommandDirectCopy = 0;
constexpr int kCommandByteFill = 1;
constexpr int kCommandWordFill = 2;
constexpr int kCommandIncreasingFill = 3;
constexpr int kCommandRepeatingBytes = 4;
constexpr uchar kGraphicsBitmap[8] = {0x80, 0x40, 0x20, 0x10,
                                      0x08, 0x04, 0x02, 0x01};

struct OWMapTiles {
  std::vector<std::vector<ushort>> light_world;    // 64 maps * (32*32 tiles)
  std::vector<std::vector<ushort>> dark_world;     // 64 maps * (32*32 tiles)
  std::vector<std::vector<ushort>> special_world;  // 32 maps * (32*32 tiles)
} typedef OWMapTiles;

class ROM {
 public:
  absl::Status LoadFromFile(const absl::string_view& filename);
  absl::Status LoadFromPointer(uchar* data, size_t length);
  absl::Status LoadAllGraphicsData();

  // absl::Status SaveOverworld();

  absl::StatusOr<Bytes> DecompressGraphics(int pos, int size);
  absl::StatusOr<Bytes> DecompressOverworld(int pos, int size);
  absl::StatusOr<Bytes> Decompress(int offset, int size = 0x800,
                                   bool reversed = false);

  absl::StatusOr<Bytes> Convert3bppTo8bppSheet(Bytes sheet, int size = 0x1000);

  uint GetGraphicsAddress(uint8_t id) const;
  auto GetSize() const { return size_; }
  auto GetTitle() const { return title; }
  auto GetGraphicsBin() const { return graphics_bin_; }
  auto GetMasterGraphicsBin() const { return master_gfx_bin_; }
  auto GetVRAM() const { return pseudo_vram_; }
  auto GetBytes() const { return rom_data_; }

  auto data() { return rom_data_.data(); }
  auto isLoaded() const { return is_loaded_; }

  auto Renderer() { return renderer_; }
  void SetupRenderer(std::shared_ptr<SDL_Renderer> renderer) {
    renderer_ = renderer;
  }
  uchar& operator[](int i) {
    if (i > size_) {
      std::cout << "Index out of bounds" << std::endl;
      return rom_data_[0];
    }
    return rom_data_[i];
  }

 private:
  int num_sheets_ = 0;
  long size_ = 0;
  uchar* current_rom_;
  uchar* master_gfx_bin_;
  uchar title[21] = "ROM Not Loaded";
  bool is_loaded_ = false;

  ImVec4 display_palette_[8];

  gfx::pseudo_vram pseudo_vram_;
  Bytes rom_data_;

  std::shared_ptr<SDL_Renderer> renderer_;
  absl::flat_hash_map<int, gfx::Bitmap> graphics_bin_;
};

}  // namespace app
}  // namespace yaze

#endif