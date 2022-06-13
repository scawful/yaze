#ifndef YAZE_APPLICATION_UTILS_ROM_H
#define YAZE_APPLICATION_UTILS_ROM_H

#include <cstddef>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "Core/Constants.h"
#include "Graphics/tile.h"
#include "compressions/alttpcompression.h"
#include "rommapping.h"
#include "tile.h"

namespace yaze {
namespace Application {
namespace Data {

using byte = unsigned char;
using ushort = unsigned short;

using namespace Graphics;

int AddressFromBytes(byte addr1, byte addr2, byte addr3);

class ROM {
 public:
  void LoadFromFile(const std::string& path);
  std::vector<tile8> ExtractTiles(TilePreset& preset);
  SNESPalette ExtractPalette(TilePreset& preset);
  unsigned int getRomPosition(const TilePreset& preset, int directAddr,
                              unsigned int snesAddr);
  short AddressFromBytes(byte addr1, byte addr2);
  char* Decompress(int pos, bool reversed = false);

  inline byte* GetRawData() { return current_rom_; }
  const unsigned char* getTitle() const { return title; }
  unsigned int getSize() const { return size_; }
  char getVersion() const { return version_; }
  bool isLoaded() const { return loaded; }

 private:
  bool loaded = false;

  byte* current_rom_;
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