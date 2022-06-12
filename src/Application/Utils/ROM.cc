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
  int bytes_read = fread(current_rom_, sizeof(unsigned char), size, file);
  fclose(file);

  memcpy(title, current_rom_ + 32704, 21);

  type = LoROM;
  fastrom = (current_rom_[21] & 0b00110000) == 0b00110000;
  if (current_rom_[21] & 1) type = HiROM;
  if ((current_rom_[21] & 0b00000111) == 0b00000111) type = ExHiROM;

  sram_size = 0x400 << current_rom_[24];
  creator_id = (current_rom_[26] << 8) | current_rom_[25];
  version = current_rom_[27];
  loaded = true;
}


std::vector<tile8> ROM::ExtractTiles(unsigned int bpp, unsigned int length) {
  std::vector<tile8> rawTiles;
  unsigned int lastCompressedSize;
  unsigned int size = length;
  char *data = alttp_decompress_gfx((char*)current_rom_, 0, length, &size, &lastCompressedSize);

  if (data == NULL) {
    return rawTiles;
  }

  unsigned tileCpt = 0;
  for (unsigned int tilePos = 0; tilePos < size; tilePos += bpp * 8) {
    tile8 newTile = unpack_bpp_tile(data, tilePos, bpp);
    newTile.id = tileCpt;
    rawTiles.push_back(newTile);
    tileCpt++;
  }

  free(data);
  return rawTiles;
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