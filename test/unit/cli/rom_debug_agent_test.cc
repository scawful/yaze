// Coverage for RomDebugAgent's address-classification helpers, including the
// write-safety rule that keeps agent-driven patches away from interrupt
// vectors, the NMI flag and direct page.
//
// This file was unregistered (listed only in the orphaned test/test.cmake) and
// was deleted in #241 as API-stale. The staleness was confined to its mock:
// MockEmulatorService derived from an unqualified EmulatorServiceImpl while the
// real type is yaze::net::EmulatorServiceImpl. The cases below need no mock at
// all -- the constructor only stores the service pointer -- so they are
// restored with a null service and registered.

#include "cli/service/agent/rom_debug_agent.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <memory>
#include <string>

namespace yaze {
namespace cli {
namespace agent {
namespace {

class RomDebugAgentTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // These helpers are pure address classification: they never dereference
    // the emulator service.
    agent_ = std::make_unique<RomDebugAgent>(nullptr);
  }

  std::unique_ptr<RomDebugAgent> agent_;
};

TEST_F(RomDebugAgentTest, IsValidJumpTarget) {
  // Test various address ranges
  EXPECT_TRUE(agent_->IsValidJumpTarget(0x008000));  // ROM start
  EXPECT_TRUE(agent_->IsValidJumpTarget(0x7E0000));  // WRAM start
  EXPECT_TRUE(agent_->IsValidJumpTarget(0x7EFFFF));  // WRAM end
  EXPECT_TRUE(agent_->IsValidJumpTarget(0x808000));  // Extended ROM

  // Banks 0x70-0x7D are SRAM in LoROM, and the helper currently treats the
  // whole 0x008000-0x7DFFFF window as ROM space, so they classify as valid.
  // Pinned as-is rather than changed: narrowing it is a semantic decision.
  EXPECT_TRUE(agent_->IsValidJumpTarget(0x700000));

  // Invalid addresses
  EXPECT_FALSE(agent_->IsValidJumpTarget(0xF00000));  // Too high
}

TEST_F(RomDebugAgentTest, IsMemoryWriteSafe) {
  // Test critical areas
  EXPECT_FALSE(agent_->IsMemoryWriteSafe(0x00FFFA, 6));    // Interrupt vectors
  EXPECT_FALSE(agent_->IsMemoryWriteSafe(0x7E0012, 1));    // NMI flag
  EXPECT_FALSE(agent_->IsMemoryWriteSafe(0x7E0050, 100));  // Direct page

  // Safe areas
  EXPECT_TRUE(agent_->IsMemoryWriteSafe(0x7E2000, 100));  // General WRAM
  EXPECT_TRUE(agent_->IsMemoryWriteSafe(0x7EF340, 10));   // Inventory (safe)
}

TEST_F(RomDebugAgentTest, DescribeMemoryLocation) {
  // Test known locations
  EXPECT_EQ(agent_->DescribeMemoryLocation(0x7E0010), "Game Mode");
  EXPECT_EQ(agent_->DescribeMemoryLocation(0x7E0011), "Submodule");
  EXPECT_EQ(agent_->DescribeMemoryLocation(0x7E0022), "Link X Position");
  EXPECT_EQ(agent_->DescribeMemoryLocation(0x7E0020), "Link Y Position");

  // Sprite table
  auto sprite_desc = agent_->DescribeMemoryLocation(0x7E0D00);
  EXPECT_TRUE(sprite_desc.find("Sprite") != std::string::npos);

  // Save data
  EXPECT_EQ(agent_->DescribeMemoryLocation(0x7EF36D), "Player Current Health");
  EXPECT_EQ(agent_->DescribeMemoryLocation(0x7EF36C), "Player Max Health");

  // DMA registers
  auto dma_desc = agent_->DescribeMemoryLocation(0x004300);
  EXPECT_TRUE(dma_desc.find("DMA") != std::string::npos);
}

TEST_F(RomDebugAgentTest, IdentifyDataType) {
  EXPECT_EQ(agent_->IdentifyDataType(0x7E0D00), "sprite");
  EXPECT_EQ(agent_->IdentifyDataType(0x7E0800), "oam");
  EXPECT_EQ(agent_->IdentifyDataType(0x004300), "dma");
  EXPECT_EQ(agent_->IdentifyDataType(0x002100), "ppu");
  EXPECT_EQ(agent_->IdentifyDataType(0x002140), "audio");
  EXPECT_EQ(agent_->IdentifyDataType(0x7EF000), "save");
  // Inventory slots sit inside the SRAM range but report their own type.
  EXPECT_EQ(agent_->IdentifyDataType(0x7EF340), "inventory");
  EXPECT_EQ(agent_->IdentifyDataType(0x7EF37F), "inventory");
  EXPECT_EQ(agent_->IdentifyDataType(0x7EF380), "save");
  EXPECT_EQ(agent_->IdentifyDataType(0x008000), "code");
  EXPECT_EQ(agent_->IdentifyDataType(0x7E2000), "ram");
}

TEST_F(RomDebugAgentTest, FormatRegisterState) {
  std::map<std::string, uint16_t> regs = {
      {"A", 0x1234},  {"X", 0x5678}, {"Y", 0x9ABC}, {"S", 0x01FF},
      {"PC", 0x8034}, {"P", 0x30},   {"DB", 0x00},  {"PB", 0x00}};

  auto formatted = agent_->FormatRegisterState(regs);

  // Check that all registers are present in the formatted string
  EXPECT_TRUE(formatted.find("A=1234") != std::string::npos);
  EXPECT_TRUE(formatted.find("X=5678") != std::string::npos);
  EXPECT_TRUE(formatted.find("Y=9ABC") != std::string::npos);
  EXPECT_TRUE(formatted.find("S=01FF") != std::string::npos);
  EXPECT_TRUE(formatted.find("PC=8034") != std::string::npos);
  EXPECT_TRUE(formatted.find("P=30") != std::string::npos);
  EXPECT_TRUE(formatted.find("DB=00") != std::string::npos);
  EXPECT_TRUE(formatted.find("PB=00") != std::string::npos);
}

}  // namespace
}  // namespace agent
}  // namespace cli
}  // namespace yaze
