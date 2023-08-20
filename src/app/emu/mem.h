#ifndef MEM_H
#define MEM_H

#include <cstdint>
#include <iostream>
#include <vector>

namespace yaze {
namespace app {
namespace emu {

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

 private:
  uint32_t GetMappedAddress(uint32_t address) const {
    uint32_t bank = address >> 16;
    uint32_t offset = address & 0xFFFF;

    switch (bank) {
      case 0x00:  // Direct Page / Stack
        return 0x0000 + offset;
      case 0x01:  // Main RAM
        return 0x2000 + offset;
      case 0x02:  // ROM (LoROM)
        return 0x4000 + offset;
      case 0x03:  // ROM (HiROM)
        return 0x8000 + offset;
      default:
        return address;  // Return the original address if no mapping is defined
    }
  }

  // Define memory regions
  std::vector<uint8_t> rom_;
  std::vector<uint8_t> ram_;
  std::vector<uint8_t> vram_;
  std::vector<uint8_t> oam_;

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