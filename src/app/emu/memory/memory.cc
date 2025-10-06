#include "app/emu/memory/memory.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "util/log.h"

namespace yaze {
namespace emu {

void MemoryImpl::Initialize(const std::vector<uint8_t>& rom_data,
                            bool verbose) {
  verbose_ = verbose;
  type_ = 1;  // LoROM

  auto location = 0x7FC0;  // LoROM header location
  rom_size_ = 0x400 << rom_data[location + 0x17];
  sram_size_ = 0x400 << rom_data[location + 0x18];
  
  // Allocate ROM and SRAM storage
  rom_.resize(rom_size_);
  const size_t copy_size = std::min<size_t>(rom_size_, rom_data.size());
  std::copy(rom_data.begin(), rom_data.begin() + copy_size, rom_.begin());
  
  ram_.resize(sram_size_);
  std::fill(ram_.begin(), ram_.end(), 0);
  
  LOG_DEBUG("Memory", "LoROM initialized: ROM size=$%06X (%zuKB) SRAM size=$%04X", 
           rom_size_, rom_size_ / 1024, sram_size_);
  LOG_DEBUG("Memory", "Reset vector at ROM offset $7FFC-$7FFD = $%02X%02X", 
           rom_data[0x7FFD], rom_data[0x7FFC]);
}

uint8_t MemoryImpl::cart_read(uint8_t bank, uint16_t adr) {
  // Emulator uses this path for all ROM/cart reads
  switch (type_) {
    case 0:
      return open_bus_;
    case 1:
      return cart_readLorom(bank, adr);
    case 2:
      return cart_readHirom(bank, adr);
    case 3:
      return cart_readExHirom(bank, adr);
  }
  return open_bus_;
}

void MemoryImpl::cart_write(uint8_t bank, uint16_t adr, uint8_t val) {
  switch (type_) {
    case 0:
      break;
    case 1:
      cart_writeLorom(bank, adr, val);
      break;
    case 2:
      cart_writeHirom(bank, adr, val);
      break;
    case 3:
      cart_writeHirom(bank, adr, val);
      break;
  }
}

uint8_t MemoryImpl::cart_readLorom(uint8_t bank, uint16_t adr) {
  // SRAM access: banks 70-7e and f0-ff, addresses 0000-7fff
  if (((bank >= 0x70 && bank < 0x7e) || bank >= 0xf0) && adr < 0x8000 &&
      sram_size_ > 0) {
    return ram_[(((bank & 0xf) << 15) | adr) & (sram_size_ - 1)];
  }
  
  // ROM access: banks 00-7f (mirrored to 80-ff), addresses 8000-ffff
  //             OR banks 40-7f, all addresses
  bank &= 0x7f;
  if (adr >= 0x8000 || bank >= 0x40) {
    uint32_t rom_offset = ((bank << 15) | (adr & 0x7fff)) & (rom_size_ - 1);
    return rom_[rom_offset];
  }
  
  return open_bus_;
}

void MemoryImpl::cart_writeLorom(uint8_t bank, uint16_t adr, uint8_t val) {
  if (((bank >= 0x70 && bank < 0x7e) || bank > 0xf0) && adr < 0x8000 &&
      sram_size_ > 0) {
    // banks 70-7e and f0-ff, adr 0000-7fff
    ram_[(((bank & 0xf) << 15) | adr) & (sram_size_ - 1)] = val;
  }
}

uint8_t MemoryImpl::cart_readHirom(uint8_t bank, uint16_t adr) {
  bank &= 0x7f;
  if (bank < 0x40 && adr >= 0x6000 && adr < 0x8000 && sram_size_ > 0) {
    // banks 00-3f and 80-bf, adr 6000-7fff
    return ram_[(((bank & 0x3f) << 13) | (adr & 0x1fff)) & (sram_size_ - 1)];
  }
  if (adr >= 0x8000 || bank >= 0x40) {
    // adr 8000-ffff in all banks or all addresses in banks 40-7f and c0-ff
    return rom_[(((bank & 0x3f) << 16) | adr) & (rom_size_ - 1)];
  }
  return open_bus_;
}

uint8_t MemoryImpl::cart_readExHirom(uint8_t bank, uint16_t adr) {
  if ((bank & 0x7f) < 0x40 && adr >= 0x6000 && adr < 0x8000 && sram_size_ > 0) {
    // banks 00-3f and 80-bf, adr 6000-7fff
    return ram_[(((bank & 0x3f) << 13) | (adr & 0x1fff)) & (sram_size_ - 1)];
  }
  bool secondHalf = bank < 0x80;
  bank &= 0x7f;
  if (adr >= 0x8000 || bank >= 0x40) {
    // adr 8000-ffff in all banks or all addresses in banks 40-7f and c0-ff
    return rom_[(((bank & 0x3f) << 16) | (secondHalf ? 0x400000 : 0) | adr) &
                (rom_size_ - 1)];
  }
  return open_bus_;
}

void MemoryImpl::cart_writeHirom(uint8_t bank, uint16_t adr, uint8_t val) {
  bank &= 0x7f;
  if (bank < 0x40 && adr >= 0x6000 && adr < 0x8000 && sram_size_ > 0) {
    // banks 00-3f and 80-bf, adr 6000-7fff
    ram_[(((bank & 0x3f) << 13) | (adr & 0x1fff)) & (sram_size_ - 1)] = val;
  }
}

uint32_t MemoryImpl::GetMappedAddress(uint32_t address) const {
  // NOTE: This function is only used by ROM editor via Memory interface.
  // The emulator core uses cart_read/cart_write instead.
  // Returns identity mapping for now - full implementation not needed for emulator.
  return address;
}

}  // namespace emu
}  // namespace yaze
