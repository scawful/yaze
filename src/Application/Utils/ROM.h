#ifndef YAZE_APPLICATION_UTILS_ROM_H
#define YAZE_APPLICATION_UTILS_ROM_H

#include <bits/postypes.h>

#include <cstddef>
#include <cstring>
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

extern "C" {

enum rom_type { LoROM, HiROM, ExLoROM, ExHiROM };

extern char* rommapping_error_text;

int rommapping_snes_to_pc(const unsigned int snes_addr, enum rom_type rom_type,
                          bool header);
int rommapping_pc_to_snes(const unsigned int pc_addr, enum rom_type rom_type,
                          bool header);

int rommapping_sram_snes_to_pc(const unsigned int snes_addr,
                               enum rom_type rom_type, bool header);
int rommapping_sram_pc_to_snes(const unsigned int pc_addr,
                               enum rom_type rom_type, bool header);

int lorom_snes_to_pc(const unsigned int snes_addr, char** info);
int lorom_sram_snes_to_pc(const unsigned int snes_addr);
int lorom_pc_to_snes(const unsigned int pc_addr);
int lorom_sram_pc_to_snes(const unsigned int pc_addr);

int hirom_snes_to_pc(const unsigned int snes_addr, char** info);
int hirom_sram_snes_to_pc(const unsigned int snes_addr);
int hirom_pc_to_snes(const unsigned int pc_addr);
int hirom_sram_pc_to_snes(const unsigned int pc_addr);
}

int AddressFromBytes(byte addr1, byte addr2, byte addr3);

class ROM {
 public:
  int SnesToPc(int addr);
  int PcToSnes(int addr);
  short AddressFromBytes(byte addr1, byte addr2);
  ushort ReadShort(int addr);
  void Write(int addr, byte value);
  short ReadReverseShort(int addr);
  ushort ReadByte(int addr);
  short ReadRealShort(int addr);
  void WriteShort(int addr, int value);
  int ReadLong(int addr);
  void WriteLong(int addr, int value);
  void LoadFromFile(const std::string& path);
  inline byte* GetRawData() { return current_rom_; }

  const unsigned char* getTitle() const { return title; }
  unsigned int getSize() const { return size; }
  char getVersion() const { return version; }

  bool isLoaded() const { return loaded; }

 private:
  std::vector<char> original_rom_;
  std::vector<char> working_rom_;

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