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
namespace Application {
namespace Data {

int AddressFromBytes(uchar addr1, uchar addr2, uchar addr3);

class ROM {
 public:
  ~ROM();

  void LoadFromFile(const std::string& path);
  std::vector<tile8> ExtractTiles(Graphics::TilePreset& preset);
  Graphics::SNESPalette ExtractPalette(Graphics::TilePreset& preset);
  uint32_t GetRomPosition(const Graphics::TilePreset& preset, int directAddr,
                          unsigned int snesAddr) const;
  inline uchar* GetRawData() { return current_rom_; }
  const unsigned char* getTitle() const { return title; }
  long int getSize() const { return size_; }
  char getVersion() const { return version_; }
  bool isLoaded() const { return loaded; }

 private:
  bool loaded = false;
  bool rom_has_header_ = false;

  uchar* current_rom_;
  char* data_;

  long int size_;
  enum rom_type type_;

  unsigned int uncompressed_size_;
  unsigned int compressed_size_;
  unsigned int compress_size_;
  unsigned char version_;
  unsigned char title[21] = "ROM Not Loaded";
};

}  // namespace Data
}  // namespace Application
}  // namespace yaze

#endif