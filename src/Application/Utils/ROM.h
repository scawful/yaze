#ifndef YAZE_APPLICATION_UTILS_ROM_H
#define YAZE_APPLICATION_UTILS_ROM_H

#include <bits/postypes.h>

#include <cstddef>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "Core/Constants.h"
#include "Graphics/Tile.h"

namespace yaze {
namespace Application {
namespace Utils {

using byte = unsigned char;
using ushort = unsigned short;

class ROM {
 public:
  int SnesToPc(int addr);
  int PcToSnes(int addr);
  int AddressFromBytes(byte addr1, byte addr2, byte addr3);
  short AddressFromBytes(byte addr1, byte addr2);
  ushort ReadShort(int addr);
  void Write(int addr, byte value);
  short ReadReverseShort(int addr);
  ushort ReadByte(int addr);
  short ReadRealShort(int addr);
  Graphics::Tile16 ReadTile16(int addr);
  void WriteShort(int addr, int value);
  int ReadLong(int addr);
  void WriteLong(int addr, int value);
  void LoadFromFile(const std::string& path);
  inline const char* GetRawData() { return working_rom_.data(); }

 private:
  std::vector<char> original_rom_;
  std::vector<char> working_rom_;
};

}  // namespace Utils
}  // namespace Application
}  // namespace yaze

#endif