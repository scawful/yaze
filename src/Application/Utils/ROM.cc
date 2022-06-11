#include "ROM.h"

#define ROMMAPPING_LOCATION_SNES_RESERVED -1
#define ROMMAPPING_LOCATION_SRAM -2
#define ROMMAPPING_LOCATION_WRAM -3

namespace yaze {
namespace Application {
namespace Utils {

using namespace Graphics;

char *rommapping_error_text;
/*
 * LoRoM is quite easy
 */
int	lorom_snes_to_pc(const unsigned int snes_addr, char** info)
{
  unsigned char	bank = snes_addr >> 16;
  unsigned int	offset = snes_addr & 0x00FFFF;
  //printf("snes_addr: %X - Bank: %X - Offset: %X\n", snes_addr, bank, offset);
  
  // 80-FD is a mirror to the start
  if (bank >= 0x80 && bank <= 0xFD)
    bank -= 0x80;
  if (bank >= 0x00 && bank <= 0x3F && offset < 0x8000 && offset >= 0x2000)
  {
    *info = "SNES Reserved";
    return ROMMAPPING_LOCATION_SNES_RESERVED;
  }
  if ((((bank >= 0x70 && bank <= 0x7D) || bank == 0xFE || bank == 0xFF) && offset < 0x8000))
  {
    *info = "SRAM";
    return ROMMAPPING_LOCATION_SRAM;
  }
  if (bank == 0x7E || bank == 0x7F || 
     (bank >= 0x00 && bank <= 0x3F && offset < 0x2000)
  )
  {
    *info = "WRAM section";
    return ROMMAPPING_LOCATION_WRAM;
  }
  
  if (bank >= 0x40 && bank <= 0x6F && offset < 0x8000)
    return bank * 0x8000 + offset;
  if (bank == 0xFE || bank == 0xFF) // this work as if 7E was regular bank
    bank -= 0xFE - 0x7E;
  return bank * 0x8000 + (offset - 0x8000);
}

int	hirom_snes_to_pc(const unsigned int snes_addr, char **info)
{
  unsigned char	bank = snes_addr >> 16;
  unsigned int	offset = snes_addr & 0x00FFFF;
  
  // 80-FD is a mirror to the start
  if (bank >= 0x80 && bank <= 0xFD)
    bank -= 0x80;
  
  if ((bank >= 0x00 && bank <= 0x1F && offset < 0x8000 && offset >= 0x2000) ||
      (bank >= 0x20 && bank <= 0x3F && offset < 0x6000 && offset >= 0x2000)
  )
  {
    *info = "SNES Reserved";
    return ROMMAPPING_LOCATION_SNES_RESERVED;
  }
  if (bank >= 0x20 && bank <= 0x3F && offset >= 0x6000 && offset < 0x8000)
  {
    *info = "SRAM";
    return ROMMAPPING_LOCATION_SRAM;
  }
  if ((bank == 0x7E || bank == 0x7F) ||
      (bank >= 0x00 && bank <= 0x3F && offset < 0x2000))
  {
    *info = "WRAM Section";
    return ROMMAPPING_LOCATION_WRAM;
  }
/*#include <stdio.h>
  printf("%02X:%04X\n", bank, offset);*/
  if (bank >= 0xFE)
    bank -= 0xFE - 0x3E;
  if (bank >= 0x40 && bank <= 0x7D)
    bank -= 0x40;
  return (bank << 16) + offset;
}

int	hirom_pc_to_snes(const unsigned int pc_addr)
{
  unsigned int bank = pc_addr >> 16;
  unsigned int offset = pc_addr & 0x00FFFF;

  //printf("%02X:%04X\n", bank, offset);
  if (bank <= 0x3F && offset >= 0x8000)
      return pc_addr;
  if (bank <= 0x3D)
      return pc_addr + 0x400000;
  return pc_addr + 0xFE0000;
}

int	hirom_sram_snes_to_pc(const unsigned int snes_addr)
{
    unsigned int bank = snes_addr >> 16;
    unsigned int offset = snes_addr & 0x00FFFF;

    if (bank >= 0x20 && bank <= 0x3F && offset >= 0x6000 && offset < 0x8000)
        return (bank - 0x20) * 0x2000 + (offset - 0x6000);
    return -1;
}

int	hirom_sram_pc_to_snes(const unsigned int pc_addr)
{
    unsigned int chuck_nb = pc_addr / 0x2000;
    unsigned int rest = pc_addr % 0x2000;

    return ((0x20 + chuck_nb) << 16) + 0x6000 + rest;
}

int	lorom_pc_to_snes(const unsigned int pc_addr)
{
   
   unsigned int bank = pc_addr / 0x8000;
   unsigned int offset = pc_addr % 0x8000 + 0x8000;
   
   //printf("pc_addr: %X - Bank: %X - Offset: %X\n", pc_addr, bank, offset);
   
   return (bank << 16) + offset;
}

int lorom_sram_pc_to_snes(const unsigned int pc_addr)
{   
    int chuck_nb = pc_addr / 0x8000;
    int rest = pc_addr % 0x8000;

    if (chuck_nb <= 0xD)
        return ((0x70 + chuck_nb) << 16) + rest;
    if (chuck_nb == 0xE || chuck_nb == 0xF)
        return ((0xF0 + chuck_nb) << 16) + rest;
    return -1;
}

int lorom_sram_snes_to_pc(const unsigned int snes_addr)
{
    unsigned char	bank = snes_addr >> 16;
    unsigned int	offset = snes_addr & 0x00FFFF;

    // F0-FD are mirror of 70-7D
    if (bank >= 0xF0 && bank <= 0xFD)
        bank -= 0x80;
    if (bank >= 0x70 && bank <= 0x7D && offset < 0x8000)
        return (bank - 0x70) * 0x8000 + offset;
    if ((bank == 0xFE || bank == 0xFF) && offset < 0x8000)
        return ((bank - 0xFE) + 0xE) * 0x8000 + offset;
    return -1;
}

int	rommapping_snes_to_pc(const unsigned int snes_addr, enum rom_type rom_type, bool header)
{
  int	pc_addr;
  char	*info;
  switch (rom_type)
  {
    case LoROM:
      pc_addr = lorom_snes_to_pc(snes_addr, &info);
      break;
    case HiROM:
      pc_addr = hirom_snes_to_pc(snes_addr, &info);
      break;
    default:
      return -1;
  }

  if (pc_addr < 0) {
    rommapping_error_text = (char *) malloc(strlen(info) + 1);
    strcpy(rommapping_error_text, info);
    return pc_addr;
  }
  if (header)
    pc_addr += 0x200;
  return pc_addr;
}

int	rommapping_pc_to_snes(const unsigned int pc_addr, enum rom_type rom_type, bool header)
{
  int snes_addr;
  
  switch (rom_type)
  {
      case LoROM:
        snes_addr = lorom_pc_to_snes(header ? pc_addr - 0x200 : pc_addr);
        break;
      case HiROM:
        snes_addr = hirom_pc_to_snes(header ? pc_addr - 0x200 : pc_addr);
        break;
      default:
        return -1;
  }
  return snes_addr;
}


int rommapping_sram_snes_to_pc(const unsigned int snes_addr, enum rom_type rom_type, bool header)
{
  int pc_addr;
  switch (rom_type)
  {
    case LoROM:
      pc_addr = lorom_sram_snes_to_pc(snes_addr);
      break;
    case HiROM:
      pc_addr = hirom_sram_snes_to_pc(snes_addr);
      break;
    default:
      return -1;
  }
  if (header)
    pc_addr += 0x200;
  return pc_addr;
}

int	rommapping_sram_pc_to_snes(const unsigned int pc_addr, enum rom_type rom_type, bool header)
{
  int snes_addr;
  
  switch (rom_type)
  {
      case LoROM:
        snes_addr = lorom_sram_pc_to_snes(header ? pc_addr - 0x200 : pc_addr);
        break;
      case HiROM:
        snes_addr = hirom_sram_pc_to_snes(header ? pc_addr - 0x200 : pc_addr);
        break;
      default:
        return -1;
  }
  return snes_addr;
}

void ROM::LoadFromFile(const std::string& path) {

  FILE * file = fopen(path.c_str(), "r+");
  if (file == NULL) return;
  fseek(file, 0, SEEK_END);
  size = ftell(file);
  fclose(file);

  // Reading data to array of unsigned chars
  file = fopen(path.c_str(), "r+");
  current_rom_ = (unsigned char *) malloc(size);
  int bytes_read = fread(current_rom_, sizeof(unsigned char), size, file);
  fclose(file);

  memcpy(title, current_rom_, 21);
  type = LoROM;
  fastrom = (current_rom_[21] & 0b00110000) == 0b00110000;
  if (current_rom_[21] & 1)
      type = HiROM;
  if ((current_rom_[21] & 0b00000111) == 0b00000111)
      type = ExHiROM;

  sram_size = 0x400 << current_rom_[24];
  creator_id =  (current_rom_[26] << 8) | current_rom_[25];
  version = current_rom_[27];
  checksum_comp = (current_rom_[29] << 8) | current_rom_[28];
  checksum = (current_rom_[31] << 8) | current_rom_[30];
  make_sense = false;
  if ((checksum ^ checksum_comp) == 0xFFFF)
      make_sense = true;
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