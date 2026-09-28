#include "app/emu/memory/memory.h"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <vector>

#include "util/log.h"

namespace yaze {
namespace emu {

namespace {

// Largest cartridge address space any mapper reaches (ExHiROM, 8 MB). A larger
// mapping window adds no reachable offsets.
constexpr uint32_t kMaxMappedRomSize = 0x800000;

}  // namespace

void MemoryImpl::Initialize(const std::vector<uint8_t>& rom_data,
                            bool verbose) {
  verbose_ = verbose;
  type_ = 1;  // LoROM

  // Validate ROM data size before accessing header
  // LoROM header is at 0x7FC0, and we need to access bytes at 0x7FD7 and 0x7FD8
  constexpr uint32_t kLoRomHeaderLocation = 0x7FC0;
  constexpr uint32_t kMinRomSizeForHeader = 0x7FD9;  // 0x7FC0 + 0x18 + 1

  uint32_t header_rom_size = 0;
  if (rom_data.size() < kMinRomSizeForHeader) {
    LOG_DEBUG(
        "Memory",
        "ROM too small for header access: %zu bytes (need at least %u bytes)",
        rom_data.size(), kMinRomSizeForHeader);
    // Fallback: size ROM from the file alone if header is not accessible
    sram_size_ = 0x2000;  // Default 8KB SRAM
  } else {
    auto location = kLoRomHeaderLocation;  // LoROM header location
    uint8_t rom_size_shift = rom_data[location + 0x17];
    uint8_t sram_size_shift = rom_data[location + 0x18];

    // Validate shift values to prevent excessive memory allocation
    if (rom_size_shift > 15) {  // Max reasonable shift (0x400 << 15 = 32MB)
      LOG_DEBUG("Memory", "Invalid ROM size shift: %u, using file size",
                rom_size_shift);
    } else {
      header_rom_size = 0x400u << rom_size_shift;
    }

    if (sram_size_shift > 7) {  // Max reasonable shift (0x400 << 7 = 512KB)
      LOG_DEBUG("Memory", "Invalid SRAM size shift: %u, using default",
                sram_size_shift);
      sram_size_ = 0x2000;  // Default 8KB SRAM
    } else {
      sram_size_ = 0x400 << sram_size_shift;
    }
  }

  // Size the mapping window from the larger of the header size and the file
  // size, rounded up to a power of two so the address mask is well formed.
  // Expanded hacks keep the vanilla header byte (0x0A = 1 MB); trusting it
  // alone would fold every bank past the first 1 MB back onto it. Reads past
  // the loaded bytes return open bus (see ReadRom).
  const uint32_t file_rom_size = static_cast<uint32_t>(
      std::min<size_t>(rom_data.size(), kMaxMappedRomSize));
  rom_size_ =
      std::min(std::bit_ceil(std::max({header_rom_size, file_rom_size, 1u})),
               kMaxMappedRomSize);

  // Keep every file byte; never truncate to the header size.
  rom_.assign(rom_data.begin(), rom_data.end());

  ram_.resize(sram_size_);
  std::fill(ram_.begin(), ram_.end(), 0);

  LOG_DEBUG("Memory",
            "LoROM initialized: ROM map size=$%06X (%uKB) file=$%06zX "
            "header=$%06X SRAM size=$%04X",
            rom_size_, rom_size_ / 1024, rom_data.size(), header_rom_size,
            sram_size_);

  // Log reset vector if ROM is large enough
  if (rom_data.size() >= 0x7FFE) {
    LOG_DEBUG("Memory", "Reset vector at ROM offset $7FFC-$7FFD = $%02X%02X",
              rom_data[0x7FFD], rom_data[0x7FFC]);
  } else {
    LOG_DEBUG("Memory", "ROM too small to read reset vector (size: %zu bytes)",
              rom_data.size());
  }
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
  // SRAM access: banks 70-7d and f0-ff, addresses 0000-7fff
  if (((bank >= 0x70 && bank < 0x7e) || bank >= 0xf0) && adr < 0x8000 &&
      sram_size_ > 0) {
    return ram_[(((bank & 0xf) << 15) | adr) & (sram_size_ - 1)];
  }

  // ROM access: banks 00-7f (mirrored to 80-ff), addresses 8000-ffff
  //             OR banks 40-7f, all addresses
  bank &= 0x7f;
  if (adr >= 0x8000 || bank >= 0x40) {
    return ReadRom((bank << 15) | (adr & 0x7fff));
  }

  return open_bus_;
}

void MemoryImpl::cart_writeLorom(uint8_t bank, uint16_t adr, uint8_t val) {
  // Must match the SRAM window in cart_readLorom.
  if (((bank >= 0x70 && bank < 0x7e) || bank >= 0xf0) && adr < 0x8000 &&
      sram_size_ > 0) {
    // banks 70-7d and f0-ff, adr 0000-7fff
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
    return ReadRom(((bank & 0x3f) << 16) | adr);
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
    return ReadRom(((bank & 0x3f) << 16) | (secondHalf ? 0x400000 : 0) | adr);
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
  // Returns identity mapping for now - full implementation not needed for
  // emulator.
  return address;
}

}  // namespace emu
}  // namespace yaze
