#ifndef MEM_H
#define MEM_H

// memory.h
class Memory {
 public:
  virtual ~Memory() = default;
  virtual uint8_t ReadByte(uint16_t address) const = 0;
  virtual uint16_t ReadWord(uint16_t address) const = 0;
  virtual uint32_t ReadWordLong(uint16_t address) const = 0;
  virtual void SetMemory(const std::vector<uint8_t>& data) = 0;

  virtual uint8_t operator[](int i) const = 0;
  virtual uint8_t at(int i) const = 0;
};

class MemoryImpl : public Memory {
 public:
  uint8_t ReadByte(uint16_t address) const override {
    return memory_.at(address);
  }
  uint16_t ReadWord(uint16_t address) const override {
    return static_cast<uint16_t>(memory_.at(address)) |
           (static_cast<uint16_t>(memory_.at(address + 1)) << 8);
  }
  void SetMemory(const std::vector<uint8_t>& data) override { memory_ = data; }

  uint8_t at(int i) const override { return memory_[i]; }
  auto size() const { return memory_.size(); }
  auto begin() const { return memory_.begin(); }
  auto end() const { return memory_.end(); }

  uint8_t operator[](int i) const override {
    if (i > memory_.size()) {
      std::cout << i << " out of bounds \n";
      return memory_[0];
    }
    return memory_[i];
  }

  // Memory (64KB)
  std::vector<uint8_t> memory_;
};

#endif  // MEM_H