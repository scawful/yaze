/**
 * @file disassembler_test.cc
 * @brief Unit tests for the 65816 disassembler
 *
 * These tests validate the disassembler that enables AI-assisted
 * assembly debugging for ROM hacking.
 */

#include "app/emu/debug/disassembler.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace yaze {
namespace emu {
namespace debug {
namespace {

class Disassembler65816Test : public ::testing::Test {
 protected:
  // Helper to create a memory reader from a buffer
  Disassembler65816::MemoryReader CreateMemoryReader(
      const std::vector<uint8_t>& buffer, uint32_t base_address = 0) {
    return [buffer, base_address](uint32_t addr) -> uint8_t {
      uint32_t offset = addr - base_address;
      if (offset < buffer.size()) {
        return buffer[offset];
      }
      return 0;
    };
  }

  Disassembler65816 disassembler_;
};

// --- Basic Instruction Tests ---

TEST_F(Disassembler65816Test, DisassembleNOP) {
  std::vector<uint8_t> code = {0xEA};  // NOP
  auto reader = CreateMemoryReader(code);

  auto result = disassembler_.Disassemble(0, reader, true, true);

  EXPECT_EQ(result.address, 0u);
  EXPECT_EQ(result.opcode, 0xEA);
  EXPECT_EQ(result.mnemonic, "NOP");
  EXPECT_EQ(result.size, 1u);
}

TEST_F(Disassembler65816Test, DisassembleSEI) {
  std::vector<uint8_t> code = {0x78};  // SEI
  auto reader = CreateMemoryReader(code);

  auto result = disassembler_.Disassemble(0, reader, true, true);

  EXPECT_EQ(result.opcode, 0x78);
  EXPECT_EQ(result.mnemonic, "SEI");
  EXPECT_EQ(result.size, 1u);
}

// --- Immediate Addressing Tests ---

TEST_F(Disassembler65816Test, DisassembleLDAImmediate8Bit) {
  std::vector<uint8_t> code = {0xA9, 0x42};  // LDA #$42
  auto reader = CreateMemoryReader(code);

  // m_flag = true means 8-bit accumulator
  auto result = disassembler_.Disassemble(0, reader, true, true);

  EXPECT_EQ(result.mnemonic, "LDA");
  EXPECT_EQ(result.size, 2u);
  EXPECT_TRUE(result.operand_str.find("42") != std::string::npos);
}

TEST_F(Disassembler65816Test, DisassembleLDAImmediate16Bit) {
  std::vector<uint8_t> code = {0xA9, 0x34, 0x12};  // LDA #$1234
  auto reader = CreateMemoryReader(code);

  // m_flag = false means 16-bit accumulator
  auto result = disassembler_.Disassemble(0, reader, false, true);

  EXPECT_EQ(result.mnemonic, "LDA");
  EXPECT_EQ(result.size, 3u);
}

TEST_F(Disassembler65816Test, DisassembleLDXImmediate8Bit) {
  std::vector<uint8_t> code = {0xA2, 0x10};  // LDX #$10
  auto reader = CreateMemoryReader(code);

  // x_flag = true means 8-bit index registers
  auto result = disassembler_.Disassemble(0, reader, true, true);

  EXPECT_EQ(result.mnemonic, "LDX");
  EXPECT_EQ(result.size, 2u);
}

TEST_F(Disassembler65816Test, DisassembleLDXImmediate16Bit) {
  std::vector<uint8_t> code = {0xA2, 0x00, 0x80};  // LDX #$8000
  auto reader = CreateMemoryReader(code);

  // x_flag = false means 16-bit index registers
  auto result = disassembler_.Disassemble(0, reader, true, false);

  EXPECT_EQ(result.mnemonic, "LDX");
  EXPECT_EQ(result.size, 3u);
}

// --- Absolute Addressing Tests ---

TEST_F(Disassembler65816Test, DisassembleLDAAbsolute) {
  std::vector<uint8_t> code = {0xAD, 0x00, 0x80};  // LDA $8000
  auto reader = CreateMemoryReader(code);

  auto result = disassembler_.Disassemble(0, reader, true, true);

  EXPECT_EQ(result.mnemonic, "LDA");
  EXPECT_EQ(result.size, 3u);
  EXPECT_TRUE(result.operand_str.find("8000") != std::string::npos);
}

TEST_F(Disassembler65816Test, DisassembleSTAAbsoluteLong) {
  std::vector<uint8_t> code = {0x8F, 0x00, 0x80, 0x7E};  // STA $7E8000
  auto reader = CreateMemoryReader(code);

  auto result = disassembler_.Disassemble(0, reader, true, true);

  EXPECT_EQ(result.mnemonic, "STA");
  EXPECT_EQ(result.size, 4u);
  EXPECT_TRUE(result.operand_str.find("7E8000") != std::string::npos);
}

// --- Jump/Call Instruction Tests ---

TEST_F(Disassembler65816Test, DisassembleJSR) {
  std::vector<uint8_t> code = {0x20, 0x00, 0x80};  // JSR $8000
  auto reader = CreateMemoryReader(code);

  auto result = disassembler_.Disassemble(0, reader, true, true);

  EXPECT_EQ(result.mnemonic, "JSR");
  EXPECT_EQ(result.size, 3u);
  EXPECT_TRUE(result.is_call);
  EXPECT_FALSE(result.is_return);
}

TEST_F(Disassembler65816Test, DisassembleJSL) {
  std::vector<uint8_t> code = {0x22, 0x00, 0x80, 0x00};  // JSL $008000
  auto reader = CreateMemoryReader(code);

  auto result = disassembler_.Disassemble(0, reader, true, true);

  EXPECT_EQ(result.mnemonic, "JSL");
  EXPECT_EQ(result.size, 4u);
  EXPECT_TRUE(result.is_call);
}

TEST_F(Disassembler65816Test, DisassembleRTS) {
  std::vector<uint8_t> code = {0x60};  // RTS
  auto reader = CreateMemoryReader(code);

  auto result = disassembler_.Disassemble(0, reader, true, true);

  EXPECT_EQ(result.mnemonic, "RTS");
  EXPECT_EQ(result.size, 1u);
  EXPECT_FALSE(result.is_call);
  EXPECT_TRUE(result.is_return);
}

TEST_F(Disassembler65816Test, DisassembleRTL) {
  std::vector<uint8_t> code = {0x6B};  // RTL
  auto reader = CreateMemoryReader(code);

  auto result = disassembler_.Disassemble(0, reader, true, true);

  EXPECT_EQ(result.mnemonic, "RTL");
  EXPECT_EQ(result.size, 1u);
  EXPECT_TRUE(result.is_return);
}

// --- Branch Instruction Tests ---

TEST_F(Disassembler65816Test, DisassembleBNE) {
  std::vector<uint8_t> code = {0xD0, 0x10};  // BNE +16
  auto reader = CreateMemoryReader(code);

  auto result = disassembler_.Disassemble(0, reader, true, true);

  EXPECT_EQ(result.mnemonic, "BNE");
  EXPECT_EQ(result.size, 2u);
  EXPECT_TRUE(result.is_branch);
}

TEST_F(Disassembler65816Test, DisassembleBRA) {
  std::vector<uint8_t> code = {0x80, 0xFE};  // BRA -2 (infinite loop)
  auto reader = CreateMemoryReader(code);

  auto result = disassembler_.Disassemble(0, reader, true, true);

  EXPECT_EQ(result.mnemonic, "BRA");
  EXPECT_EQ(result.size, 2u);
  EXPECT_TRUE(result.is_branch);
}

TEST_F(Disassembler65816Test, DisassembleJMPAbsolute) {
  std::vector<uint8_t> code = {0x4C, 0x00, 0x80};  // JMP $8000
  auto reader = CreateMemoryReader(code);

  auto result = disassembler_.Disassemble(0, reader, true, true);

  EXPECT_EQ(result.mnemonic, "JMP");
  EXPECT_EQ(result.size, 3u);
  EXPECT_TRUE(result.is_branch);
}

// --- Range Disassembly Tests ---

TEST_F(Disassembler65816Test, DisassembleRange) {
  // Small program:
  // 8000: SEI           ; Disable interrupts
  // 8001: CLC           ; Clear carry
  // 8002: XCE           ; Exchange carry and emulation
  // 8003: LDA #$00      ; Load 0 into A
  // 8005: STA $2100     ; Store to PPU brightness register
  std::vector<uint8_t> code = {0x78, 0x18, 0xFB, 0xA9, 0x00, 0x8D, 0x00, 0x21};
  auto reader = CreateMemoryReader(code, 0x008000);

  auto result =
      disassembler_.DisassembleRange(0x008000, 5, reader, true, true);

  ASSERT_EQ(result.size(), 5u);
  EXPECT_EQ(result[0].mnemonic, "SEI");
  EXPECT_EQ(result[1].mnemonic, "CLC");
  EXPECT_EQ(result[2].mnemonic, "XCE");
  EXPECT_EQ(result[3].mnemonic, "LDA");
  EXPECT_EQ(result[4].mnemonic, "STA");
}

// --- Indexed Addressing Tests ---

TEST_F(Disassembler65816Test, DisassembleLDAAbsoluteX) {
  std::vector<uint8_t> code = {0xBD, 0x00, 0x80};  // LDA $8000,X
  auto reader = CreateMemoryReader(code);

  auto result = disassembler_.Disassemble(0, reader, true, true);

  EXPECT_EQ(result.mnemonic, "LDA");
  EXPECT_EQ(result.size, 3u);
  EXPECT_TRUE(result.operand_str.find("X") != std::string::npos);
}

TEST_F(Disassembler65816Test, DisassembleLDADirectPageIndirectY) {
  std::vector<uint8_t> code = {0xB1, 0x10};  // LDA ($10),Y
  auto reader = CreateMemoryReader(code);

  auto result = disassembler_.Disassemble(0, reader, true, true);

  EXPECT_EQ(result.mnemonic, "LDA");
  EXPECT_EQ(result.size, 2u);
  EXPECT_TRUE(result.operand_str.find("Y") != std::string::npos);
}

// --- Special Instructions Tests ---

TEST_F(Disassembler65816Test, DisassembleREP) {
  std::vector<uint8_t> code = {0xC2, 0x30};  // REP #$30 (16-bit A, X, Y)
  auto reader = CreateMemoryReader(code);

  auto result = disassembler_.Disassemble(0, reader, true, true);

  EXPECT_EQ(result.mnemonic, "REP");
  EXPECT_EQ(result.size, 2u);
}

TEST_F(Disassembler65816Test, DisassembleSEP) {
  std::vector<uint8_t> code = {0xE2, 0x20};  // SEP #$20 (8-bit A)
  auto reader = CreateMemoryReader(code);

  auto result = disassembler_.Disassemble(0, reader, true, true);

  EXPECT_EQ(result.mnemonic, "SEP");
  EXPECT_EQ(result.size, 2u);
}

// --- Instruction Size Tests ---

TEST_F(Disassembler65816Test, GetInstructionSizeImplied) {
  // NOP, RTS, RTL all have size 1
  EXPECT_EQ(disassembler_.GetInstructionSize(0xEA, true, true), 1u);  // NOP
  EXPECT_EQ(disassembler_.GetInstructionSize(0x60, true, true), 1u);  // RTS
  EXPECT_EQ(disassembler_.GetInstructionSize(0x6B, true, true), 1u);  // RTL
}

TEST_F(Disassembler65816Test, GetInstructionSizeAbsolute) {
  // Absolute addressing is 3 bytes
  EXPECT_EQ(disassembler_.GetInstructionSize(0xAD, true, true), 3u);  // LDA abs
  EXPECT_EQ(disassembler_.GetInstructionSize(0x8D, true, true), 3u);  // STA abs
  EXPECT_EQ(disassembler_.GetInstructionSize(0x20, true, true), 3u);  // JSR abs
}

TEST_F(Disassembler65816Test, GetInstructionSizeLong) {
  // Long addressing is 4 bytes
  EXPECT_EQ(disassembler_.GetInstructionSize(0xAF, true, true), 4u);  // LDA long
  EXPECT_EQ(disassembler_.GetInstructionSize(0x22, true, true), 4u);  // JSL long
}

// --- Symbol Resolution Tests ---

TEST_F(Disassembler65816Test, DisassembleWithSymbolResolver) {
  std::vector<uint8_t> code = {0x20, 0x00, 0x80};  // JSR $8000
  auto reader = CreateMemoryReader(code);

  // Set up a symbol resolver that knows about $8000
  disassembler_.SetSymbolResolver([](uint32_t addr) -> std::string {
    if (addr == 0x008000) {
      return "Reset";
    }
    return "";
  });

  auto result = disassembler_.Disassemble(0, reader, true, true);

  EXPECT_EQ(result.mnemonic, "JSR");
  // The operand_str should contain the symbol name
  EXPECT_TRUE(result.operand_str.find("Reset") != std::string::npos ||
              result.operand_str.find("8000") != std::string::npos);
}

}  // namespace
}  // namespace debug
}  // namespace emu
}  // namespace yaze
