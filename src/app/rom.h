#ifndef YAZE_APP_UTILS_ROM_H
#define YAZE_APP_UTILS_ROM_H

#include <compressions/alttpcompression.h>
#include <rommapping.h>
#include <tile.h>

#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "app/core/constants.h"
#include "app/gfx/tile.h"

namespace yaze {
namespace app {

int AddressFromBytes(uchar addr1, uchar addr2, uchar addr3);

class ROM {
 public:
  ~ROM();

  void SetupRenderer(std::shared_ptr<SDL_Renderer> renderer);
  void LoadFromFile(const std::string& path);
  std::vector<tile8> ExtractTiles(gfx::TilePreset& preset);
  gfx::SNESPalette ExtractPalette(gfx::TilePreset& preset);
  uint32_t GetRomPosition(int direct_addr, uint snes_addr) const;
  char* Decompress(int pos, int size = 0x800, bool reversed = false);
  uchar* SNES3bppTo8bppSheet(uchar* buffer_in, int sheet_id = 0);
  SDL_Texture* DrawgfxSheet(int offset);

  inline uchar* GetRawData() { return current_rom_; }
  const uchar* getTitle() const { return title; }
  long int getSize() const { return size_; }
  char getVersion() const { return version_; }
  bool isLoaded() const { return loaded; }

  unsigned int SnesToPc(unsigned int addr) {
    if (addr >= 0x808000) {
      addr -= 0x808000;
    }
    unsigned int temp = (addr & 0x7FFF) + ((addr / 2) & 0xFF8000);
    return (temp + 0x0);
  }

 private:
  bool loaded = false;
  bool has_header_ = false;
  long int size_;
  uint uncompressed_size_;
  uint compressed_size_;
  uint compress_size_;
  uchar* current_rom_;
  uchar version_;
  uchar title[21] = "ROM Not Loaded";
  enum rom_type type_ = LoROM;

  std::shared_ptr<uchar> rom_ptr_;
  std::vector<char*> decompressed_graphic_sheets_;
  std::vector<uchar*> converted_graphic_sheets_;
  std::vector<SDL_Surface> surfaces_;
  std::shared_ptr<SDL_Renderer> sdl_renderer_;
};

}  // namespace app
}  // namespace yaze

#endif