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
#include "Data/Tile.h"

namespace yaze {
namespace Application {
namespace Utils {

using byte = unsigned char;
using ushort = unsigned short;
using namespace Data;

class ROM {
 public:
  int SnesToPc(int addr);
  ushort ReadShort(int addr);
  void Write(int addr, byte value);
  short ReadReverseShort(int addr);
  ushort ReadByte(int addr);
  short ReadRealShort(int addr);
  Tile16 ReadTile16(int addr);
  void WriteShort(int addr, int value);
  int ReadLong(int addr);
  void WriteLong(int addr, int value) ;
  void LoadFromFile(const std::string& path);
  inline const char * GetRawData() {
    return working_rom_.data();
  }

 private:
  std::vector<char> original_rom_;
  std::vector<char> working_rom_;

};

}  // namespace Utils
}  // namespace Application
}  // namespace yaze

#endif