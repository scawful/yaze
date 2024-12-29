#include "app/emu/memory/memory.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "imgui/imgui.h"

namespace yaze {
namespace emu {
namespace memory {

void MemoryImpl::Initialize(const std::vector<uint8_t>& rom_data,
                            bool verbose) {
  verbose_ = verbose;
  type_ = 1;

  auto location = 0x7FC0;  // GetHeaderOffset();
  rom_size_ = 0x400 << rom_data[location + 0x17];
  sram_size_ = 0x400 << rom_data[location + 0x18];
  rom_.resize(rom_size_);

  // Copy memory into rom_
  for (size_t i = 0; i < rom_size_; i++) {
    rom_[i] = rom_data[i];
  }
  ram_.resize(sram_size_);
  for (size_t i = 0; i < sram_size_; i++) {
    ram_[i] = 0;
  }

  // Clear memory
  memory_.resize(0x1000000);  // 16 MB
  std::fill(memory_.begin(), memory_.end(), 0);

  // Load ROM data into memory based on LoROM mapping
  size_t rom_data_size = rom_data.size();
  size_t rom_address = 0;
  const size_t ROM_CHUNK_SIZE = 0x8000;  // 32 KB
  for (size_t bank = 0x00; bank <= 0x3F; ++bank) {
    for (size_t offset = 0x8000; offset <= 0xFFFF; offset += ROM_CHUNK_SIZE) {
      if (rom_address < rom_data_size) {
        std::copy(rom_data.begin() + rom_address,
                  rom_data.begin() + rom_address + ROM_CHUNK_SIZE,
                  memory_.begin() + (bank << 16) + offset);
        rom_address += ROM_CHUNK_SIZE;
      }
    }
  }
}

uint8_t MemoryImpl::cart_read(uint8_t bank, uint16_t adr) {
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
  if (((bank >= 0x70 && bank < 0x7e) || bank >= 0xf0) && adr < 0x8000 &&
      sram_size_ > 0) {
    // banks 70-7e and f0-ff, adr 0000-7fff
    return ram_[(((bank & 0xf) << 15) | adr) & (sram_size_ - 1)];
  }
  bank &= 0x7f;
  if (adr >= 0x8000 || bank >= 0x40) {
    // adr 8000-ffff in all banks or all addresses in banks 40-7f and c0-ff
    return rom_[((bank << 15) | (adr & 0x7fff)) & (rom_size_ - 1)];
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
  uint8_t bank = address >> 16;
  uint32_t offset = address & 0xFFFF;

  if (bank <= 0x3F) {
    if (address <= 0x1FFF) {
      return (0x7E << 16) + offset;  // Shadow RAM
    } else if (address <= 0x5FFF) {
      return (bank << 16) + (offset - 0x2000) + 0x2000;  // Hardware Registers
    } else if (address <= 0x7FFF) {
      return offset - 0x6000 + 0x6000;  // Expansion RAM
    } else {
      // Return lorom mapping
      return (bank << 16) + (offset - 0x8000) + 0x8000;  // ROM
    }
  } else if (bank == 0x7D) {
    return offset + 0x7D0000;  // SRAM
  } else if (bank == 0x7E || bank == 0x7F) {
    return offset + 0x7E0000;  // System RAM
  } else if (bank >= 0x80) {
    // Handle HiROM and mirrored areas
  }

  return address;  // Return the original address if no mapping is defined
}

}  // namespace memory
}  // namespace emu

}  // namespace yaze