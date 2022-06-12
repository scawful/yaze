#include "ROM.h"

#define ROMMAPPING_LOCATION_SNES_RESERVED -1
#define ROMMAPPING_LOCATION_SRAM -2
#define ROMMAPPING_LOCATION_WRAM -3

namespace yaze {
namespace Application {
namespace Utils {

using namespace Graphics;

void ROM::LoadFromFile(const std::string &path) {
  FILE *file = fopen(path.c_str(), "r+");
  if (file == NULL) return;
  fseek(file, 0, SEEK_END);
  size = ftell(file);
  fclose(file);

  // Reading data to array of unsigned chars
  file = fopen(path.c_str(), "r+");
  current_rom_ = (unsigned char *)malloc(size);
  rom_data_ = (char *) malloc(size);
  fread(rom_data_, sizeof(char), size, file);
  int bytes_read = fread(current_rom_, sizeof(unsigned char), size, file);
  fclose(file);

  memcpy(title, rom_data_ + 32704, 21);
  type = LoROM;
  version = current_rom_[27];
  loaded = true;
}

std::vector<tile8> ROM::ExtractTiles(TilePreset &preset) {
  std::cout << "Begin ROM::ExtractTiles" << std::endl;
  std::vector<tile8> rawTiles;
  uint filePos = 0;
  unsigned int size = preset.length;
  filePos =
      getRomPosition(preset, preset.pcTilesLocation, preset.SNESTilesLocation);
  char *data = (char *)rom_data_ + filePos;
  memcpy(data, rom_data_ + filePos, preset.length);

  data =
      alttp_decompress_gfx(data, 0, preset.length, &size, &lastCompressedSize);
  std::cout << "size: " << size << std::endl;
  std::cout << "lastCompressedSize: " << lastCompressedSize << std::endl;

  if (data == NULL) {
    std:: cout << alttp_decompression_error << std::endl;
    return rawTiles;
  }

  unsigned tileCpt = 0;
  for (unsigned int tilePos = 0; tilePos < size; tilePos += preset.bpp * 8) {
    std::cout << "Unpacking tile..." << std::endl;
    tile8 newTile = unpack_bpp_tile(data, tilePos, preset.bpp);
    newTile.id = tileCpt;
    rawTiles.push_back(newTile);
    tileCpt++;
  }

  free(data);
  std::cout << "End ROM::ExtractTiles" << std::endl;
  return rawTiles;
}

unsigned int ROM::getRomPosition(const TilePreset &preset, int directAddr,
                                 unsigned int snesAddr) {
  bool romHasHeader = false;  // romInfo.hasHeader
  if (overrideHeaderInfo) romHasHeader = overridenHeaderInfo;
  unsigned int filePos = -1;
  enum rom_type rType = LoROM;
  std::cout << "ROM::getRomPosition: directAddr:" << directAddr << std::endl;
  if (directAddr == -1) {
    filePos = rommapping_snes_to_pc(snesAddr, rType, romHasHeader);
  } else {
    filePos = directAddr;
    if (romHasHeader) filePos += 0x200;
  }
  std::cout << "ROM::getRomPosition: filePos:" << filePos << std::endl;
  return filePos;
}

int ROM::SnesToPc(int addr) {
  if (addr >= 0x808000) {
    addr -= 0x808000;
  }
  int temp = (addr & 0x7FFF) + ((addr / 2) & 0xFF8000);
  return (temp + 0x0);
}

int AddressFromBytes(byte addr1, byte addr2, byte addr3) {
  return (addr1 << 16) | (addr2 << 8) | addr3;
}

short ROM::AddressFromBytes(byte addr1, byte addr2) {
  return (short)((addr1 << 8) | (addr2));
}

void ROM::Write(int addr, byte value) { current_rom_[addr] = value; }

void ROM::WriteLong(int addr, int value) {
  current_rom_[addr] = (byte)(value & 0xFF);
  current_rom_[addr + 1] = (byte)((value >> 8) & 0xFF);
  current_rom_[addr + 2] = (byte)((value >> 16) & 0xFF);
}

void ROM::WriteShort(int addr, int value) {
  current_rom_[addr] = (byte)(value & 0xFF);
  current_rom_[addr + 1] = (byte)((value >> 8) & 0xFF);
}

int ROM::ReadLong(int addr) {
  return ((current_rom_[addr + 2] << 16) + (current_rom_[addr + 1] << 8) +
          current_rom_[addr]);
}

ushort ROM::ReadShort(int addr) {
  return (ushort)((current_rom_[addr + 1] << 8) + current_rom_[addr]);
}

short ROM::ReadRealShort(int addr) {
  return (short)((current_rom_[addr + 1] << 8) + current_rom_[addr]);
}

ushort ROM::ReadByte(int addr) { return (ushort)(current_rom_[addr]); }

short ROM::ReadReverseShort(int addr) {
  return (short)((current_rom_[addr] << 8) + current_rom_[addr + 1]);
}

}  // namespace Utils
}  // namespace Application
}  // namespace yaze