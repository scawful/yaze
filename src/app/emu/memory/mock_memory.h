#ifndef YAZE_TEST_MOCK_MOCK_MEMORY_H
#define YAZE_TEST_MOCK_MOCK_MEMORY_H

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "app/emu/clock.h"
#include "app/emu/cpu.h"
#include "app/emu/memory/memory.h"

using yaze::app::emu::Clock;
using yaze::app::emu::CPU;
using yaze::app::emu::Memory;

class MockClock : public Clock {
 public:
  MOCK_METHOD(void, UpdateClock, (double delta), (override));
  MOCK_METHOD(unsigned long long, GetCycleCount, (), (const, override));
  MOCK_METHOD(void, ResetAccumulatedTime, (), (override));
  MOCK_METHOD(void, SetFrequency, (float new_frequency), (override));
  MOCK_METHOD(float, GetFrequency, (), (const, override));
};

class MockMemory : public Memory {
 public:
  MOCK_CONST_METHOD1(ReadByte, uint8_t(uint32_t address));
  MOCK_CONST_METHOD1(ReadWord, uint16_t(uint32_t address));
  MOCK_CONST_METHOD1(ReadWordLong, uint32_t(uint32_t address));
  MOCK_METHOD(std::vector<uint8_t>, ReadByteVector,
              (uint32_t address, uint16_t length), (const, override));

  MOCK_METHOD2(WriteByte, void(uint32_t address, uint8_t value));
  MOCK_METHOD2(WriteWord, void(uint32_t address, uint16_t value));

  MOCK_METHOD1(PushByte, void(uint8_t value));
  MOCK_METHOD0(PopByte, uint8_t());
  MOCK_METHOD1(PushWord, void(uint16_t value));
  MOCK_METHOD0(PopWord, uint16_t());
  MOCK_METHOD1(PushLong, void(uint32_t value));
  MOCK_METHOD0(PopLong, uint32_t());

  MOCK_CONST_METHOD0(SP, int16_t());
  MOCK_METHOD1(SetSP, void(int16_t value));

  MOCK_METHOD1(SetMemory, void(const std::vector<uint8_t>& data));
  MOCK_METHOD1(LoadData, void(const std::vector<uint8_t>& data));

  MOCK_METHOD0(ClearMemory, void());

  MOCK_CONST_METHOD1(at, uint8_t(int i));
  uint8_t operator[](int i) const override { return at(i); }

  void SetMemoryContents(const std::vector<uint8_t>& data) {
    memory_.resize(64000);
    std::copy(data.begin(), data.end(), memory_.begin());
  }

  void SetMemoryContents(const std::vector<uint16_t>& data) {
    memory_.resize(64000);
    int i = 0;
    for (const auto& each : data) {
      memory_[i] = each & 0xFF;
      memory_[i + 1] = (each >> 8) & 0xFF;
      i += 2;
    }
  }

  void InsertMemory(const uint64_t address, const std::vector<uint8_t>& data) {
    int i = 0;
    for (const auto& each : data) {
      memory_[address + i] = each;
      i++;
    }
  }

  void Init() {
    ON_CALL(*this, ReadByte(::testing::_))
        .WillByDefault(
            [this](uint32_t address) { return memory_.at(address); });
    ON_CALL(*this, ReadWord(::testing::_))
        .WillByDefault([this](uint32_t address) {
          return static_cast<uint16_t>(memory_.at(address)) |
                 (static_cast<uint16_t>(memory_.at(address + 1)) << 8);
        });
    ON_CALL(*this, ReadWordLong(::testing::_))
        .WillByDefault([this](uint32_t address) {
          return static_cast<uint32_t>(memory_.at(address)) |
                 (static_cast<uint32_t>(memory_.at(address + 1)) << 8) |
                 (static_cast<uint32_t>(memory_.at(address + 2)) << 16);
        });
    ON_CALL(*this, ReadByteVector(::testing::_, ::testing::_))
        .WillByDefault([this](uint32_t address, uint16_t length) {
          std::vector<uint8_t> data;
          for (int i = 0; i < length; i++) {
            data.push_back(memory_.at(address + i));
          }
          return data;
        });
    ON_CALL(*this, WriteByte(::testing::_, ::testing::_))
        .WillByDefault([this](uint32_t address, uint8_t value) {
          memory_[address] = value;
        });
    ON_CALL(*this, WriteWord(::testing::_, ::testing::_))
        .WillByDefault([this](uint32_t address, uint16_t value) {
          memory_[address] = value & 0xFF;
          memory_[address + 1] = (value >> 8) & 0xFF;
        });
    ON_CALL(*this, PushByte(::testing::_)).WillByDefault([this](uint8_t value) {
      memory_.at(SP_) = value;
    });
    ON_CALL(*this, PopByte()).WillByDefault([this]() {
      uint8_t value = memory_.at(SP_);
      this->SetSP(SP_ + 1);
      return value;
    });
    ON_CALL(*this, PushWord(::testing::_))
        .WillByDefault([this](uint16_t value) {
          memory_.at(SP_) = value & 0xFF;
          memory_.at(SP_ + 1) = (value >> 8) & 0xFF;
        });
    ON_CALL(*this, PopWord()).WillByDefault([this]() {
      uint16_t value = static_cast<uint16_t>(memory_.at(SP_)) |
                       (static_cast<uint16_t>(memory_.at(SP_ + 1)) << 8);
      this->SetSP(SP_ + 2);
      return value;
    });
    ON_CALL(*this, PushLong(::testing::_))
        .WillByDefault([this](uint32_t value) {
          memory_.at(SP_) = value & 0xFF;
          memory_.at(SP_ + 1) = (value >> 8) & 0xFF;
          memory_.at(SP_ + 2) = (value >> 16) & 0xFF;
        });
    ON_CALL(*this, PopLong()).WillByDefault([this]() {
      uint32_t value = static_cast<uint32_t>(memory_.at(SP_)) |
                       (static_cast<uint32_t>(memory_.at(SP_ + 1)) << 8) |
                       (static_cast<uint32_t>(memory_.at(SP_ + 2)) << 16);
      this->SetSP(SP_ + 3);
      return value;
    });
    ON_CALL(*this, ClearMemory()).WillByDefault([this]() {
      memory_.resize(64000, 0x00);
    });
  }

  std::vector<uint8_t> memory_;
  uint16_t SP_ = 0x01FF;
};

#endif  // YAZE_TEST_MOCK_MOCK_MEMORY_H