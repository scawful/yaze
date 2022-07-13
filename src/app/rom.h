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
#include "app/gfx/snes_tile.h"

namespace yaze {
namespace app {

constexpr uchar kGraphicsBitmap[8] = {0x80, 0x40, 0x20, 0x10,
                                      0x08, 0x04, 0x02, 0x01};

class ROM {
 public:
  void Close();
  void SetupRenderer(std::shared_ptr<SDL_Renderer> renderer);
  void LoadFromFile(const std::string& path);

  uchar* DecompressGraphics(int pos, int size);
  uchar* DecompressOverworld(int pos, int size);
  uchar* Decompress(int pos, int size = 0x800, bool reversed = false);
  uchar* SNES3bppTo8bppSheet(uchar* buffer_in, int sheet_id = 0,
                             int size = 0x1000);
  uint GetGraphicsAddress(uint8_t id) const;
  SDL_Texture* DrawGraphicsSheet(int offset);
  void DrawAllGraphicsData();
  char* CreateAllGfxDataRaw();
  gfx::SNESPalette ExtractPalette(uint addr, int bpp);

  uchar* data() { return current_rom_; }
  auto Renderer() { return sdl_renderer_; }
  const uchar* getTitle() const { return title; }
  long getSize() const { return size_; }
  bool isLoaded() const { return is_loaded_; }

 private:
  bool is_loaded_ = false;
  bool has_header_ = false;
  long size_ = 0;
  int num_sheets_ = 0;
  uint compressed_size_;
  uchar* current_rom_;
  uchar title[21] = "ROM Not Loaded";
  enum rom_type type_ = LoROM;
  bool isbpp3[core::constants::NumberOfSheets];

  std::shared_ptr<uchar> rom_ptr_;
  std::vector<uchar*> decompressed_graphic_sheets_;
  std::vector<uchar*> converted_graphic_sheets_;
  std::vector<SDL_Surface> surfaces_;
  std::shared_ptr<SDL_Renderer> sdl_renderer_;
};

}  // namespace app
}  // namespace yaze

#endif