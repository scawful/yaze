#ifndef MEM_H
#define MEM_H

#include <cstdint>
#include <iostream>
#include <vector>

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

  // Additional methods and constructors
};

// memory.h
class Memory {
 public:
  virtual ~Memory() = default;
  virtual uint8_t ReadByte(uint16_t address) const = 0;
  virtual uint16_t ReadWord(uint16_t address) const = 0;
  virtual uint32_t ReadWordLong(uint16_t address) const = 0;

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

class MemoryImpl : public Memory {
 public:
  uint8_t ReadByte(uint16_t address) const override {
    uint32_t mapped_address = GetMappedAddress(address);
    return memory_.at(mapped_address);
  }
  uint16_t ReadWord(uint16_t address) const override {
    uint32_t mapped_address = GetMappedAddress(address);
    return static_cast<uint16_t>(memory_.at(mapped_address)) |
           (static_cast<uint16_t>(memory_.at(mapped_address + 1)) << 8);
  }
  uint32_t ReadWordLong(uint16_t address) const override {
    uint32_t mapped_address = GetMappedAddress(address);
    return static_cast<uint32_t>(memory_.at(mapped_address)) |
           (static_cast<uint32_t>(memory_.at(mapped_address + 1)) << 8) |
           (static_cast<uint32_t>(memory_.at(mapped_address + 2)) << 16);
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

  int16_t SP() const override { return SP_; }
  void SetSP(int16_t value) override { SP_ = value; }

  void SetMemory(const std::vector<uint8_t>& data) override { memory_ = data; }
  void ClearMemory() override { memory_.resize(64000, 0x00); }
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

  static const uint32_t kROMStart = 0xC00000;
  static const uint32_t kROMSize = 0x400000;
  static const uint32_t kRAMStart = 0x7E0000;
  static const uint32_t kRAMSize = 0x20000;
  static const uint32_t kVRAMStart = 0x210000;
  static const uint32_t kVRAMSize = 0x10000;
  static const uint32_t kOAMStart = 0x218000;
  static const uint32_t kOAMSize = 0x220;

  // Memory (64KB)
  std::vector<uint8_t> memory_;

  // Stack Pointer
  uint16_t SP_ = 0x01FF;
};

}  // namespace emu
}  // namespace app
}  // namespace yaze

#endif  // MEM_H