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
#include "Graphics/Tile.h"
#include "compressions/alttpcompression.h"
#include "compressions/stdnintendo.h"
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
  inline byte* GetRawData() { return current_rom_; }
  const unsigned char* getTitle() const { return title; }
  unsigned int getSize() const { return size; }
  char getVersion() const { return version; }
  bool isLoaded() const { return loaded; }

 private:
  bool loaded = false;

  byte* current_rom_;
  char* rom_data_;

  bool fastrom;
  long int size;
  enum rom_type type;

  bool overrideHeaderInfo;
  bool overridenHeaderInfo;
  unsigned int lastUnCompressSize;
  unsigned int lastCompressedSize;
  unsigned int lastCompressSize;
  unsigned char version;
  unsigned char title[21] = "ROM Not Loaded";
};

}  // namespace Data
}  // namespace Application
}  // namespace yaze

#endif