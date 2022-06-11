#include "ROM.h"

namespace yaze {
namespace Application {
namespace Utils {

using namespace Graphics;

void ROM::LoadFromFile(const std::string& path) {

  FILE * file = fopen(path.c_str(), "r+");
  if (file == NULL) return;
  fseek(file, 0, SEEK_END);
  long int size = ftell(file);
  fclose(file);

  std::cout << "size: " << size << std::endl;

  // Reading data to array of unsigned chars
  file = fopen(path.c_str(), "r+");
  current_rom_ = (unsigned char *) malloc(size);
  int bytes_read = fread(current_rom_, sizeof(unsigned char), size, file);
  fclose(file);

}

int ROM::SnesToPc(int addr) {
  if (addr >= 0x808000) {
    addr -= 0x808000;
  }
  int temp = (addr & 0x7FFF) + ((addr / 2) & 0xFF8000);
  return (temp + 0x0);
}

// TODO: FIXME
int ROM::PcToSnes(int addr) {
  byte b[4];
  // = BitConverter.GetBytes(addr)
  b[2] = (byte)(b[2] * 2);

  if (b[1] >= 0x80) {
    b[2] += 1;
  } else {
    b[1] += 0x80;
  }

  //return BitConverter.ToInt32(b, 0);
  // snes always have + 0x8000 no matter what, the bank on pc is always / 2

  return ((addr * 2) & 0xFF0000) + (addr & 0x7FFF) + 0x8000;
}

int ROM::AddressFromBytes(byte addr1, byte addr2, byte addr3) {
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

Tile16 ROM::ReadTile16(int addr) {
  ushort t1 = (ushort)((current_rom_[addr + 1] << 8) + current_rom_[addr]);
  ushort t2 = (ushort)((current_rom_[addr + 3] << 8) + current_rom_[addr + 2]);
  ushort t3 = (ushort)((current_rom_[addr + 5] << 8) + current_rom_[addr + 4]);
  ushort t4 = (ushort)((current_rom_[addr + 7] << 8) + current_rom_[addr + 6]);
  return Tile16((unsigned long)((t1 << 48) + (t2 << 32) + (t3 << 16) + t4));
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