#ifndef YAZE_APPLICATION_UTILS_ROM_H
#define YAZE_APPLICATION_UTILS_ROM_H

#include <compressions/alttpcompression.h>
#include <rommapping.h>
#include <tile.h>

#include <cstddef>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "Core/constants.h"
#include "Graphics/tile.h"

namespace yaze {
namespace application {
namespace Data {

int AddressFromBytes(uchar addr1, uchar addr2, uchar addr3);

class ROM {
 public:
  ~ROM();

  void LoadFromFile(const std::string& path);
  std::vector<tile8> ExtractTiles(Graphics::TilePreset& preset);
  Graphics::SNESPalette ExtractPalette(Graphics::TilePreset& preset);

  uint32_t GetRomPosition(int direct_addr, uint snes_addr) const;
  inline uchar* GetRawData() { return current_rom_; }
  const uchar* getTitle() const { return title; }
  long int getSize() const { return size_; }
  char getVersion() const { return version_; }
  bool isLoaded() const { return loaded; }  
  
  uchar* LoadGraphicsSheet(int offset);
  uchar* SNES3bppTo8bppSheet(uchar *sheet_buffer_in);
  char* Decompress(int pos, bool reversed = false);

 private:
  bool loaded = false;
  bool has_header_ = false;
  uchar* current_rom_;
  uchar version_;
  uchar title[21] = "ROM Not Loaded";
  uint uncompressed_size_;
  uint compressed_size_;
  uint compress_size_;
  long int size_;
  enum rom_type type_ = LoROM;

  std::shared_ptr<uchar> rom_ptr_;
  std::unordered_map<unsigned int, std::shared_ptr<uchar[2048]>>
      decompressed_sheets;
};

}  // namespace Data
}  // namespace application
}  // namespace yaze

#endif