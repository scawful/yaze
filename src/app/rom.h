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
  absl::Status OpenFromFile(const absl::string_view& filename);

  // absl::Status SaveOverworld();
  // absl::Status SaveDungeons();

  void Close();
  void SetupRenderer(std::shared_ptr<SDL_Renderer> renderer);
  void LoadFromFile(const std::string& path);
  void LoadFromPointer(uchar* data);
  void LoadAllGraphicsData();

  uint GetGraphicsAddress(uint8_t id) const;

  uchar* DecompressGraphics(int pos, int size);
  uchar* DecompressOverworld(int pos, int size);
  uchar* Decompress(int pos, int size = 0x800, bool reversed = false);

  absl::StatusOr<Bytes> DecompressV2(int offset, int size = 0x800,
                                     bool reversed = false);
  absl::StatusOr<Bytes> Convert3bppTo8bppSheet(Bytes sheet, int size = 0x1000);
  absl::Status LoadAllGraphicsDataV2();

  uchar* SNES3bppTo8bppSheet(uchar* buffer_in, int sheet_id = 0,
                             int size = 0x1000);

  SDL_Texture* DrawGraphicsSheet(int offset);

  long getSize() const { return size_; }
  uchar* data() { return current_rom_; }
  const uchar* getTitle() const { return title; }
  bool isLoaded() const { return is_loaded_; }
  auto Renderer() { return renderer_; }
  auto GetGraphicsBin() const { return graphics_bin_; }
  auto GetGraphicsBinV2() const { return graphics_bin_v2_; }
  auto GetMasterGraphicsBin() const { return master_gfx_bin_; }
  auto GetVRAM() const { return pseudo_vram_; }

 private:
  int num_sheets_ = 0;
  long size_ = 0;
  uchar* current_rom_;
  uchar* master_gfx_bin_;
  uchar title[21] = "ROM Not Loaded";
  bool is_loaded_ = false;

  ImVec4 display_palette_[8];

  gfx::pseudo_vram pseudo_vram_;
  std::vector<uchar> rom_data_;

  std::vector<uchar*> decompressed_graphic_sheets_;
  std::vector<uchar*> converted_graphic_sheets_;
  std::shared_ptr<SDL_Renderer> renderer_;
  std::unordered_map<unsigned int, gfx::Bitmap> graphics_bin_;
  absl::flat_hash_map<int, gfx::Bitmap> graphics_bin_v2_;
};

}  // namespace app
}  // namespace yaze

#endif