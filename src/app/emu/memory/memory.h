#ifndef YAZE_APP_EMU_MEMORY_H
#define YAZE_APP_EMU_MEMORY_H

#include <cstdint>
#include <functional>
#include <iostream>
#include <vector>

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
namespace emu {

typedef struct DmaChannel {
  uint8_t b_addr;
  uint16_t a_addr;
  uint8_t a_bank;
  uint16_t size;       // also indirect hdma adr
  uint8_t ind_bank;    // hdma
  uint16_t table_addr; // hdma
  uint8_t rep_count;   // hdma
  uint8_t unusedByte;
  bool dma_active;
  bool hdma_active;
  uint8_t mode;
  bool fixed;
  bool decrement;
  bool indirect; // hdma
  bool from_b;
  bool unusedBit;
  bool do_transfer; // hdma
  bool terminated;  // hdma
} DmaChannel;

typedef struct CpuCallbacks {
  std::function<uint8_t(uint32_t)> read_byte;
  std::function<void(uint32_t, uint8_t)> write_byte;
  std::function<void(bool waiting)> idle;
} CpuCallbacks;

constexpr uint32_t kROMStart = 0x008000;
constexpr uint32_t kROMSize = 0x200000;
constexpr uint32_t kRAMStart = 0x7E0000;
constexpr uint32_t kRAMSize = 0x20000;

/**
 * @brief Memory interface
 */
class Memory {
public:
  virtual ~Memory() = default;
  virtual uint8_t ReadByte(uint32_t address) const = 0;
  virtual uint16_t ReadWord(uint32_t address) const = 0;
  virtual uint32_t ReadWordLong(uint32_t address) const = 0;
  virtual std::vector<uint8_t> ReadByteVector(uint32_t address,
                                              uint16_t length) const = 0;

  virtual void WriteByte(uint32_t address, uint8_t value) = 0;
  virtual void WriteWord(uint32_t address, uint16_t value) = 0;
  virtual void WriteLong(uint32_t address, uint32_t value) = 0;

  virtual void PushByte(uint8_t value) = 0;
  virtual uint8_t PopByte() = 0;
  virtual void PushWord(uint16_t value) = 0;
  virtual uint16_t PopWord() = 0;
  virtual void PushLong(uint32_t value) = 0;
  virtual uint32_t PopLong() = 0;

  virtual uint16_t SP() const = 0;
  virtual void SetSP(uint16_t value) = 0;

  virtual void ClearMemory() = 0;

  virtual uint8_t operator[](int i) const = 0;
  virtual uint8_t at(int i) const = 0;

  virtual uint8_t open_bus() const = 0;
  virtual void set_open_bus(uint8_t value) = 0;

  virtual bool hdma_init_requested() const = 0;
  virtual bool hdma_run_requested() const = 0;
  virtual void init_hdma_request() = 0;
  virtual void run_hdma_request() = 0;
  virtual void set_hdma_run_requested(bool value) = 0;
  virtual void set_hdma_init_requested(bool value) = 0;
  virtual void set_pal_timing(bool value) = 0;
  virtual void set_h_pos(uint16_t value) = 0;
  virtual void set_v_pos(uint16_t value) = 0;

  // get h_pos and v_pos
  virtual auto h_pos() const -> uint16_t = 0;
  virtual auto v_pos() const -> uint16_t = 0;
  // get pal timing
  virtual auto pal_timing() const -> bool = 0;
};

/**
 * @class MemoryImpl
 * @brief Implementation of the Memory interface for emulating memory in a SNES
 * system.
 *
 */
class MemoryImpl : public Memory {
public:
  void Initialize(const std::vector<uint8_t> &romData, bool verbose = false);

  uint16_t GetHeaderOffset() {
    uint16_t offset;
    switch (memory_[(0x00 << 16) + 0xFFD5] & 0x07) {
    case 0: // LoROM
      offset = 0x7FC0;
      break;
    case 1: // HiROM
      offset = 0xFFC0;
      break;
    case 5: // ExHiROM
      offset = 0x40;
      break;
    default:
      throw std::invalid_argument(
          "Unable to locate supported ROM mapping mode in the provided ROM "
          "file. Please try another ROM file.");
    }

    return offset;
  }

  uint8_t cart_read(uint8_t bank, uint16_t adr);
  void cart_write(uint8_t bank, uint16_t adr, uint8_t val);

  uint8_t cart_readLorom(uint8_t bank, uint16_t adr);
  void cart_writeLorom(uint8_t bank, uint16_t adr, uint8_t val);

  uint8_t cart_readHirom(uint8_t bank, uint16_t adr);
  uint8_t cart_readExHirom(uint8_t bank, uint16_t adr);

  void cart_writeHirom(uint8_t bank, uint16_t adr, uint8_t val);

  uint8_t ReadByte(uint32_t address) const override {
    uint32_t mapped_address = GetMappedAddress(address);
    return memory_.at(mapped_address);
  }
  uint16_t ReadWord(uint32_t address) const override {
    uint32_t mapped_address = GetMappedAddress(address);
    return static_cast<uint16_t>(memory_.at(mapped_address)) |
           (static_cast<uint16_t>(memory_.at(mapped_address + 1)) << 8);
  }
  uint32_t ReadWordLong(uint32_t address) const override {
    uint32_t mapped_address = GetMappedAddress(address);
    return static_cast<uint32_t>(memory_.at(mapped_address)) |
           (static_cast<uint32_t>(memory_.at(mapped_address + 1)) << 8) |
           (static_cast<uint32_t>(memory_.at(mapped_address + 2)) << 16);
  }
  std::vector<uint8_t> ReadByteVector(uint32_t address,
                                      uint16_t length) const override {
    uint32_t mapped_address = GetMappedAddress(address);
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
  void WriteLong(uint32_t address, uint32_t value) override {
    uint32_t mapped_address = GetMappedAddress(address);
    memory_.at(mapped_address) = value & 0xFF;
    memory_.at(mapped_address + 1) = (value >> 8) & 0xFF;
    memory_.at(mapped_address + 2) = (value >> 16) & 0xFF;
  }

  // Stack operations
  void PushByte(uint8_t value) override {
    if (SP_ > 0x0100) {
      memory_.at(SP_--) = value;
    } else {
      // Handle stack underflow
      std::cout << "Stack underflow!" << std::endl;
      throw std::runtime_error("Stack underflow!");
    }
  }

  uint8_t PopByte() override {
    if (SP_ < 0x1FF) {
      return memory_.at(++SP_);
    } else {
      // Handle stack overflow
      std::cout << "Stack overflow!" << std::endl;
      throw std::runtime_error("Stack overflow!");
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

  // Stack Pointer access.
  uint16_t SP() const override { return SP_; }
  auto mutable_sp() -> uint16_t & { return SP_; }
  void SetSP(uint16_t value) override { SP_ = value; }
  void ClearMemory() override { std::fill(memory_.begin(), memory_.end(), 0); }

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
  auto data() const { return memory_.data(); }
  void set_open_bus(uint8_t value) override { open_bus_ = value; }
  auto open_bus() const -> uint8_t override { return open_bus_; }
  auto hdma_init_requested() const -> bool override {
    return hdma_init_requested_;
  }
  auto hdma_run_requested() const -> bool override {
    return hdma_run_requested_;
  }
  void init_hdma_request() override { hdma_init_requested_ = true; }
  void run_hdma_request() override { hdma_run_requested_ = true; }
  void set_hdma_run_requested(bool value) override {
    hdma_run_requested_ = value;
  }
  void set_hdma_init_requested(bool value) override {
    hdma_init_requested_ = value;
  }
  void set_pal_timing(bool value) override { pal_timing_ = value; }
  void set_h_pos(uint16_t value) override { h_pos_ = value; }
  void set_v_pos(uint16_t value) override { v_pos_ = value; }
  auto h_pos() const -> uint16_t override { return h_pos_; }
  auto v_pos() const -> uint16_t override { return v_pos_; }
  auto pal_timing() const -> bool override { return pal_timing_; }

  auto dma_state() -> uint8_t & { return dma_state_; }
  void set_dma_state(uint8_t value) { dma_state_ = value; }
  auto dma_channels() -> DmaChannel * { return channel; }

  // Define memory regions
  std::vector<uint8_t> rom_;
  std::vector<uint8_t> ram_;

private:
  uint32_t GetMappedAddress(uint32_t address) const;

  bool verbose_ = false;

  // DMA requests
  bool hdma_run_requested_ = false;
  bool hdma_init_requested_ = false;

  bool pal_timing_ = false;

  // Memory regions
  uint32_t rom_size_;
  uint32_t sram_size_;

  // Frame timing
  uint16_t h_pos_ = 0;
  uint16_t v_pos_ = 0;

  // Dma State
  uint8_t dma_state_ = 0;

  // Open bus
  uint8_t open_bus_ = 0;

  // Stack Pointer
  uint16_t SP_ = 0;

  // Cart Type
  uint8_t type_ = 1;

  // Dma Channels
  DmaChannel channel[8];

  // Memory (64KB)
  std::vector<uint8_t> memory_;
};

} // namespace emu
} // namespace yaze

#endif // YAZE_APP_EMU_MEMORY_H
