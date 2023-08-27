#ifndef MEM_H
#define MEM_H

#include <cstdint>
#include <iostream>
#include <vector>

#include "app/emu/debug/log.h"

// LoROM (Mode 20):

// Banks   Offset        Purpose
// 00-3F   0000-1FFF     LowRAM (shadowed from 7E)
//         2000-2FFF     PPU1, APU
//         3000-3FFF     SFX, DSP, etc.
//         4000-41FF     Controller
//         4200-5FFF     PPU2, DMA, etc.
//         6000-7FFF     Expansion RAM (reserved)
//         8000-FFFF     32k ROM Chunk
// 40-7C   0000-7FFF     32k ROM Chunk
//         8000-FFFF     32k ROM Chunk
// 7D      0000-FFFF     SRAM
// 7E      0000-1FFF     LowRAM
//         2000-FFFF     System RAM
// 7F      0000-FFFF     System RAM

namespace yaze {
namespace app {
namespace emu {

enum ROMSpeed { SLOW_ROM = 0x00, FAST_ROM = 0x07 };

enum BankSize { LOW_ROM = 0x00, HI_ROM = 0x01 };

enum ROMType {
  ROM_DEFAULT = 0x00,
  ROM_RAM = 0x01,
  ROM_SRAM = 0x02,
  ROM_DSP1 = 0x03,
  ROM_DSP1_RAM = 0x04,
  ROM_DSP1_SRAM = 0x05,
  FX = 0x06
};

enum ROMSize {
  SIZE_2_MBIT = 0x08,
  SIZE_4_MBIT = 0x09,
  SIZE_8_MBIT = 0x0A,
  SIZE_16_MBIT = 0x0B,
  SIZE_32_MBIT = 0x0C
};

enum SRAMSize {
  NO_SRAM = 0x00,
  SRAM_16_KBIT = 0x01,
  SRAM_32_KBIT = 0x02,
  SRAM_64_KBIT = 0x03
};

enum CountryCode {
  JAPAN = 0x00,
  USA = 0x01,
  EUROPE_OCEANIA_ASIA = 0x02,
  // ... and other countries
};

enum License {
  INVALID = 0,
  NINTENDO = 1,
  ZAMUSE = 5,
  CAPCOM = 8,
  // ... and other licenses
};

class ROMInfo {
 public:
  std::string title;
  ROMSpeed romSpeed;
  BankSize bankSize;
  ROMType romType;
  ROMSize romSize;
  SRAMSize sramSize;
  CountryCode countryCode;
  License license;
  uint8_t version;
  uint16_t checksumComplement;
  uint16_t checksum;
  uint16_t nmiVblVector;
  uint16_t resetVector;
};

class Observer {
 public:
  virtual ~Observer() = default;
  virtual void Notify(uint32_t address, uint8_t data) = 0;
};

constexpr uint32_t kROMStart = 0xC00000;
constexpr uint32_t kROMSize = 0x400000;
constexpr uint32_t kRAMStart = 0x7E0000;
constexpr uint32_t kRAMSize = 0x20000;
constexpr uint32_t kVRAMStart = 0x210000;
constexpr uint32_t kVRAMSize = 0x10000;
constexpr uint32_t kOAMStart = 0x218000;
constexpr uint32_t kOAMSize = 0x220;

// memory.h
class Memory {
 public:
  virtual ~Memory() = default;
  virtual uint8_t ReadByte(uint16_t address) const = 0;
  virtual uint16_t ReadWord(uint16_t address) const = 0;
  virtual uint32_t ReadWordLong(uint16_t address) const = 0;
  virtual std::vector<uint8_t> ReadByteVector(uint16_t address,
                                              uint16_t length) const = 0;

  virtual void WriteByte(uint32_t address, uint8_t value) = 0;
  virtual void WriteWord(uint32_t address, uint16_t value) = 0;

  virtual void PushByte(uint8_t value) = 0;
  virtual uint8_t PopByte() = 0;
  virtual void PushWord(uint16_t value) = 0;
  virtual uint16_t PopWord() = 0;
  virtual void PushLong(uint32_t value) = 0;
  virtual uint32_t PopLong() = 0;

  virtual int16_t SP() const = 0;
  virtual void SetSP(int16_t value) = 0;

  virtual void SetMemory(const std::vector<uint8_t>& data) = 0;
  virtual void ClearMemory() = 0;
  virtual void LoadData(const std::vector<uint8_t>& data) = 0;

  virtual uint8_t operator[](int i) const = 0;
  virtual uint8_t at(int i) const = 0;
};

class MemoryImpl : public Memory, public Loggable {
 public:
  void Initialize(const std::vector<uint8_t>& romData) {
    const size_t ROM_CHUNK_SIZE = 0x8000;           // 32 KB
    const size_t SRAM_SIZE = 0x10000;               // 64 KB
    const size_t SYSTEM_RAM_SIZE = 0x20000;         // 128 KB
    const size_t EXPANSION_RAM_SIZE = 0x2000;       // 8 KB
    const size_t HARDWARE_REGISTERS_SIZE = 0x4000;  // 16 KB

    // Clear memory
    std::fill(memory_.begin(), memory_.end(), 0);

    // Load ROM data into memory based on LoROM mapping
    size_t romSize = romData.size();
    size_t romAddress = 0;
    for (size_t bank = 0x00; bank <= 0xBF; bank += 0x80) {
      for (size_t offset = 0x8000; offset <= 0xFFFF; offset += ROM_CHUNK_SIZE) {
        if (romAddress < romSize) {
          std::copy(romData.begin() + romAddress,
                    romData.begin() + romAddress + ROM_CHUNK_SIZE,
                    memory_.begin() + (bank << 16) + offset);
          romAddress += ROM_CHUNK_SIZE;
        }
      }
    }

    // Initialize SRAM at banks 0x7D and 0xFD
    std::fill(memory_.begin() + (0x7D << 16), memory_.begin() + (0x7E << 16),
              0);
    std::fill(memory_.begin() + (0xFD << 16), memory_.begin() + (0xFE << 16),
              0);

    // Initialize System RAM at banks 0x7E and 0x7F
    std::fill(memory_.begin() + (0x7E << 16),
              memory_.begin() + (0x7E << 16) + SYSTEM_RAM_SIZE, 0);

    // Initialize Shadow RAM at banks 0x00-0x3F and 0x80-0xBF
    for (size_t bank = 0x00; bank <= 0xBF; bank += 0x80) {
      std::fill(memory_.begin() + (bank << 16),
                memory_.begin() + (bank << 16) + 0x2000, 0);
    }

    // Initialize Hardware Registers at banks 0x00-0x3F and 0x80-0xBF
    for (size_t bank = 0x00; bank <= 0xBF; bank += 0x80) {
      std::fill(
          memory_.begin() + (bank << 16) + 0x2000,
          memory_.begin() + (bank << 16) + 0x2000 + HARDWARE_REGISTERS_SIZE, 0);
    }

    // Initialize Expansion RAM at banks 0x00-0x3F and 0x80-0xBF
    for (size_t bank = 0x00; bank <= 0xBF; bank += 0x80) {
      std::fill(memory_.begin() + (bank << 16) + 0x6000,
                memory_.begin() + (bank << 16) + 0x6000 + EXPANSION_RAM_SIZE,
                0);
    }

    // Initialize Reset and NMI Vectors at bank 0xFF
    std::fill(memory_.begin() + (0xFF << 16) + 0xFF00,
              memory_.begin() + (0xFF << 16) + 0xFFFF + 1, 0);

    // Copy data into rom_ vector
    rom_.resize(kROMSize);
    std::copy(memory_.begin() + kROMStart,
              memory_.begin() + kROMStart + kROMSize, rom_.begin());

    // Copy data into ram_ vector
    ram_.resize(kRAMSize);
    std::copy(memory_.begin() + kRAMStart,
              memory_.begin() + kRAMStart + kRAMSize, ram_.begin());

    // Copy data into vram_ vector
    vram_.resize(kVRAMSize);
    std::copy(memory_.begin() + kVRAMStart,
              memory_.begin() + kVRAMStart + kVRAMSize, vram_.begin());

    // Copy data into oam_ vector
    oam_.resize(kOAMSize);
    std::copy(memory_.begin() + kOAMStart,
              memory_.begin() + kOAMStart + kOAMSize, oam_.begin());
  }

  uint8_t ReadByte(uint16_t address) const override {
    uint32_t mapped_address = GetMappedAddress(address);
    NotifyObservers(mapped_address, /*data=*/0);
    return memory_.at(mapped_address);
  }
  uint16_t ReadWord(uint16_t address) const override {
    uint32_t mapped_address = GetMappedAddress(address);
    NotifyObservers(mapped_address, /*data=*/0);
    return static_cast<uint16_t>(memory_.at(mapped_address)) |
           (static_cast<uint16_t>(memory_.at(mapped_address + 1)) << 8);
  }
  uint32_t ReadWordLong(uint16_t address) const override {
    uint32_t mapped_address = GetMappedAddress(address);
    NotifyObservers(mapped_address, /*data=*/0);
    return static_cast<uint32_t>(memory_.at(mapped_address)) |
           (static_cast<uint32_t>(memory_.at(mapped_address + 1)) << 8) |
           (static_cast<uint32_t>(memory_.at(mapped_address + 2)) << 16);
  }
  std::vector<uint8_t> ReadByteVector(uint16_t address,
                                      uint16_t length) const override {
    uint32_t mapped_address = GetMappedAddress(address);
    NotifyObservers(mapped_address, /*data=*/0);
    return std::vector<uint8_t>(memory_.begin() + mapped_address,
                                memory_.begin() + mapped_address + length);
  }

  void WriteByte(uint32_t address, uint8_t value) override {
    uint32_t mapped_address = GetMappedAddress(address);
    memory_[mapped_address] = value;
  }
  void WriteWord(uint32_t address, uint16_t value) override {
    uint32_t mapped_address = GetMappedAddress(address);
    memory_.at(mapped_address) = value & 0xFF;
    memory_.at(mapped_address + 1) = (value >> 8) & 0xFF;
  }

  // Stack operations
  void PushByte(uint8_t value) override {
    if (SP_ > 0x0100) {
      memory_.at(SP_--) = value;
    } else {
      // Handle stack underflow
      std::cout << "Stack underflow!" << std::endl;
    }
  }

  uint8_t PopByte() override {
    if (SP_ < 0x1FF) {
      return memory_.at(++SP_);
    } else {
      // Handle stack overflow
      std::cout << "Stack overflow!" << std::endl;
      return 0;
    }
  }

  void PushWord(uint16_t value) override {
    PushByte(value >> 8);
    PushByte(value & 0xFF);
  }

  uint16_t PopWord() override {
    uint8_t low = PopByte();
    uint8_t high = PopByte();
    return (static_cast<uint16_t>(high) << 8) | low;
  }

  void PushLong(uint32_t value) override {
    PushByte(value >> 16);
    PushByte(value >> 8);
    PushByte(value & 0xFF);
  }

  uint32_t PopLong() override {
    uint8_t low = PopByte();
    uint8_t mid = PopByte();
    uint8_t high = PopByte();
    return (static_cast<uint32_t>(high) << 16) |
           (static_cast<uint32_t>(mid) << 8) | low;
  }

  void AddObserver(Observer* observer) { observers_.push_back(observer); }

  // Stack Pointer access.
  int16_t SP() const override { return SP_; }
  void SetSP(int16_t value) override { SP_ = value; }
  void ClearMemory() override { std::fill(memory_.begin(), memory_.end(), 0); }
  void SetMemory(const std::vector<uint8_t>& data) override {
    std::copy(data.begin(), data.end(), memory_.begin());
  }
  void LoadData(const std::vector<uint8_t>& data) override {
    std::copy(data.begin(), data.end(), memory_.begin());
  }

  uint8_t at(int i) const override { return memory_[i]; }
  uint8_t operator[](int i) const override {
    if (i > memory_.size()) {
      std::cout << i << " out of bounds \n";
      return memory_[0];
    }
    return memory_[i];
  }

  auto size() const { return memory_.size(); }
  auto begin() const { return memory_.begin(); }
  auto end() const { return memory_.end(); }

  // Define memory regions
  std::vector<uint8_t> rom_;
  std::vector<uint8_t> ram_;
  std::vector<uint8_t> vram_;
  std::vector<uint8_t> oam_;

 private:
  uint32_t GetMappedAddress(uint32_t address) const {
    uint32_t bank = address >> 16;
    uint32_t offset = address & 0xFFFF;

    if (bank <= 0x3F) {
      if (offset <= 0x1FFF) {
        return offset;  // Shadow RAM
      } else if (offset <= 0x5FFF) {
        return offset - 0x2000 + 0x2000;  // Hardware Registers
      } else if (offset <= 0x7FFF) {
        return offset - 0x6000 + 0x6000;  // Expansion RAM
      } else {
        return (bank << 15) + (offset - 0x8000) + 0x8000;  // ROM
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

  void NotifyObservers(uint32_t address, uint8_t data) const {
    for (auto observer : observers_) {
      observer->Notify(address, data);
    }
  }

  std::vector<Observer*> observers_;

  // Memory (64KB)
  std::array<uint8_t, 0x10000> memory_;

  // Stack Pointer
  uint16_t SP_ = 0x01FF;
};

}  // namespace emu
}  // namespace app
}  // namespace yaze

#endif  // MEM_H