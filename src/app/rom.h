#ifndef YAZE_APP_UTILS_ROM_H
#define YAZE_APP_UTILS_ROM_H

#include <compressions/alttpcompression.h>
#include <rommapping.h>

#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "app/core/common.h"
#include "app/core/constants.h"
#include "app/gfx/pseudo_vram.h"
#include "app/gfx/snes_tile.h"

namespace yaze {
namespace app {

struct OWMapTiles {
  std::vector<std::vector<ushort>> light_world;    // 64 maps * (32*32 tiles)
  std::vector<std::vector<ushort>> dark_world;     // 64 maps * (32*32 tiles)
  std::vector<std::vector<ushort>> special_world;  // 32 maps * (32*32 tiles)
} typedef OWMapTiles;

constexpr int kCommandDirectCopy = 0;
constexpr int kCommandByteFill = 1;
constexpr int kCommandWordFill = 2;
constexpr int kCommandIncreasingFill = 3;
constexpr int kCommandRepeatingBytes = 4;
constexpr uchar kGraphicsBitmap[8] = {0x80, 0x40, 0x20, 0x10,
                                      0x08, 0x04, 0x02, 0x01};
class ROM {
 public:
  void Close();
  void SetupRenderer(std::shared_ptr<SDL_Renderer> renderer);
  void LoadFromFile(const std::string& path);
  void LoadFromPointer(uchar* data);
  void LoadAllGraphicsData();

  uchar* DecompressGraphics(int pos, int size);
  uchar* DecompressOverworld(int pos, int size);
  uchar* Decompress(int pos, int size = 0x800, bool reversed = false);

  uchar* SNES3bppTo8bppSheet(uchar* buffer_in, int sheet_id = 0,
                             int size = 0x1000);
  uint GetGraphicsAddress(uint8_t id) const;
  SDL_Texture* DrawGraphicsSheet(int offset);

  gfx::SNESPalette ExtractPalette(uint addr, int bpp);

  uchar* data() { return current_rom_; }
  const uchar* getTitle() const { return title; }
  long getSize() const { return size_; }
  bool isLoaded() const { return is_loaded_; }
  auto Renderer() { return sdl_renderer_; }
  auto GetGraphicsBin() const { return graphics_bin_; }
  auto GetMasterGraphicsBin() const { return master_gfx_bin_; }
  auto GetVRAM() const { return pseudo_vram_; }

 private:
  int num_sheets_ = 0;
  long size_ = 0;
  uchar* current_rom_;
  uchar* master_gfx_bin_;
  uchar title[21] = "ROM Not Loaded";
  bool is_loaded_ = false;
  bool isbpp3[core::constants::NumberOfSheets];
  enum rom_type type_ = LoROM;

  gfx::pseudo_vram pseudo_vram_;
  std::vector<uchar*> decompressed_graphic_sheets_;
  std::vector<uchar*> converted_graphic_sheets_;
  std::shared_ptr<SDL_Renderer> sdl_renderer_;
  std::unordered_map<unsigned int, gfx::Bitmap> graphics_bin_;
};

}  // namespace app
}  // namespace yaze

#endif