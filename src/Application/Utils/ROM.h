#ifndef YAZE_APPLICATION_UTILS_ROM_H
#define YAZE_APPLICATION_UTILS_ROM_H

#include <cstddef>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "Compression.h"
#include "Core/Constants.h"
#include "Graphics/Tile.h"
#include "compressions/alttpcompression.h"
#include "compressions/stdnintendo.h"
#include "rommapping.h"
#include "tile.h"

namespace yaze {
namespace Application {
namespace Utils {

using byte = unsigned char;
using ushort = unsigned short;

using namespace Graphics;

int AddressFromBytes(byte addr1, byte addr2, byte addr3);

class ROM {
 public:
  void LoadFromFile(const std::string& path);
  std::vector<tile8> ExtractTiles(unsigned int bpp, unsigned int length);

  int SnesToPc(int addr);
  short AddressFromBytes(byte addr1, byte addr2);
  ushort ReadShort(int addr);
  void Write(int addr, byte value);
  short ReadReverseShort(int addr);
  ushort ReadByte(int addr);
  short ReadRealShort(int addr);
  void WriteShort(int addr, int value);
  int ReadLong(int addr);
  void WriteLong(int addr, int value);
  inline byte* GetRawData() { return current_rom_; }

  const unsigned char* getTitle() const { return title; }
  unsigned int getSize() const { return size; }
  char getVersion() const { return version; }

  bool isLoaded() const { return loaded; }

 private:
  bool loaded = false;

  byte* current_rom_;

  enum rom_type type;
  bool fastrom;
  bool make_sense;
  unsigned char title[21];
  long int size;
  unsigned int sram_size;
  uint16_t creator_id;
  unsigned char version;
  unsigned char checksum_comp;
  unsigned char checksum;
};

}  // namespace Utils
}  // namespace Application
}  // namespace yaze

#endif