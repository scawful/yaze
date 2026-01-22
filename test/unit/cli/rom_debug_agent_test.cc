#include "cli/service/agent/rom_debug_agent.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "app/service/emulator_service_impl.h"
#include "protos/emulator_service.grpc.pb.h"

namespace yaze {
namespace cli {
namespace agent {
namespace {

using ::testing::_;
using ::testing::Return;

// Mock emulator service for testing
class MockEmulatorService : public EmulatorServiceImpl {
 public:
  explicit MockEmulatorService() : EmulatorServiceImpl(nullptr) {}

  MOCK_METHOD(grpc::Status, ReadMemory,
              (grpc::ServerContext*, const MemoryRequest*, MemoryResponse*),
              (override));
  MOCK_METHOD(grpc::Status, GetDisassembly,
              (grpc::ServerContext*, const DisassemblyRequest*, DisassemblyResponse*),
              (override));
  MOCK_METHOD(grpc::Status, GetExecutionTrace,
              (grpc::ServerContext*, const TraceRequest*, TraceResponse*),
              (override));
};

class RomDebugAgentTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_emulator_ = std::make_unique<MockEmulatorService>();
    agent_ = std::make_unique<RomDebugAgent>(mock_emulator_.get());
  }

  std::unique_ptr<MockEmulatorService> mock_emulator_;
  std::unique_ptr<RomDebugAgent> agent_;
};

TEST_F(RomDebugAgentTest, AnalyzeBreakpoint_BasicAnalysis) {
  // Setup breakpoint hit
  BreakpointHitResponse hit;
  hit.set_address(0x008034);  // Example ROM address
  hit.set_a_register(0x1234);
  hit.set_x_register(0x5678);
  hit.set_y_register(0x9ABC);
  hit.set_stack_pointer(0x01FF);
  hit.set_program_counter(0x008034);
  hit.set_processor_status(0x30);  // N and V flags set
  hit.set_data_bank(0x00);
  hit.set_program_bank(0x00);

  // Mock disassembly response
  DisassemblyResponse disasm_resp;
  auto* inst = disasm_resp.add_instructions();
  inst->set_address(0x008034);
  inst->set_mnemonic("LDA");
  inst->set_operand("$12");
  inst->set_bytes("\xA5\x12");

  EXPECT_CALL(*mock_emulator_, GetDisassembly(_, _, _))
      .WillOnce([&disasm_resp](grpc::ServerContext*, const DisassemblyRequest*,
                               DisassemblyResponse* response) {
        *response = disasm_resp;
        return grpc::Status::OK;
      });

  // Analyze breakpoint
  auto result = agent_->AnalyzeBreakpoint(hit);

  ASSERT_TRUE(result.ok());
  auto& analysis = result.value();

  // Verify basic fields
  EXPECT_EQ(analysis.address, 0x008034);
  EXPECT_EQ(analysis.disassembly, "LDA $12");
  EXPECT_EQ(analysis.registers["A"], 0x1234);
  EXPECT_EQ(analysis.registers["X"], 0x5678);
  EXPECT_EQ(analysis.registers["Y"], 0x9ABC);
  EXPECT_EQ(analysis.registers["S"], 0x01FF);
  EXPECT_EQ(analysis.registers["P"], 0x30);

  // Should have suggestions based on processor flags
  EXPECT_FALSE(analysis.suggestions.empty());
}

TEST_F(RomDebugAgentTest, AnalyzeMemory_SpriteData) {
  // Setup memory response for sprite data
  MemoryResponse mem_resp;
  std::string sprite_data = {
      0x01,  // State (active)
      0x80, 0x00,  // X position
      0x90, 0x00,  // Y position
      0x00, 0x00, 0x00, 0x00,  // Other sprite fields
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  mem_resp.set_data(sprite_data);

  EXPECT_CALL(*mock_emulator_, ReadMemory(_, _, _))
      .WillOnce([&mem_resp](grpc::ServerContext*, const MemoryRequest*,
                           MemoryResponse* response) {
        *response = mem_resp;
        return grpc::Status::OK;
      });

  // Analyze sprite memory
  auto result = agent_->AnalyzeMemory(0x7E0D00, 16);  // Sprite table start

  ASSERT_TRUE(result.ok());
  auto& analysis = result.value();

  EXPECT_EQ(analysis.address, 0x7E0D00);
  EXPECT_EQ(analysis.length, 16);
  EXPECT_EQ(analysis.data_type, "sprite");
  EXPECT_FALSE(analysis.description.empty());
  EXPECT_EQ(analysis.data.size(), 16);

  // Should have parsed sprite fields
  EXPECT_EQ(analysis.fields["sprite_index"], 0);
  EXPECT_EQ(analysis.fields["state"], 1);
  EXPECT_EQ(analysis.fields["x_pos_low"], 0x80);
}

TEST_F(RomDebugAgentTest, AnalyzeMemory_DetectsAnomalies) {
  // Setup memory response with corrupted data
  MemoryResponse mem_resp;
  std::string corrupted_data(16, 0xFF);  // All 0xFF indicates corruption
  mem_resp.set_data(corrupted_data);

  EXPECT_CALL(*mock_emulator_, ReadMemory(_, _, _))
      .WillOnce([&mem_resp](grpc::ServerContext*, const MemoryRequest*,
                           MemoryResponse* response) {
        *response = mem_resp;
        return grpc::Status::OK;
      });

  // Analyze corrupted sprite memory
  auto result = agent_->AnalyzeMemory(0x7E0D00, 16);

  ASSERT_TRUE(result.ok());
  auto& analysis = result.value();

  // Should detect the corruption pattern
  EXPECT_FALSE(analysis.anomalies.empty());
  bool found_corruption = false;
  for (const auto& anomaly : analysis.anomalies) {
    if (anomaly.find("0xFF") != std::string::npos ||
        anomaly.find("corruption") != std::string::npos) {
      found_corruption = true;
      break;
    }
  }
  EXPECT_TRUE(found_corruption);
}

TEST_F(RomDebugAgentTest, ComparePatch_DetectsChanges) {
  // Original code
  std::vector<uint8_t> original = {
      0xA9, 0x10,  // LDA #$10
      0x8D, 0x00, 0x21,  // STA $2100
      0x60  // RTS
  };

  // Patched code (different value)
  MemoryResponse mem_resp;
  std::string patched_data = {
      (char)0xA9, (char)0x0F,  // LDA #$0F (changed)
      (char)0x8D, (char)0x00, (char)0x21,  // STA $2100
      (char)0x60  // RTS
  };
  mem_resp.set_data(patched_data);

  EXPECT_CALL(*mock_emulator_, ReadMemory(_, _, _))
      .WillOnce([&mem_resp](grpc::ServerContext*, const MemoryRequest*,
                           MemoryResponse* response) {
        *response = mem_resp;
        return grpc::Status::OK;
      });

  // Compare patch
  auto result = agent_->ComparePatch(0x008000, original.size(), original);

  ASSERT_TRUE(result.ok());
  auto& comparison = result.value();

  EXPECT_EQ(comparison.address, 0x008000);
  EXPECT_EQ(comparison.length, original.size());
  EXPECT_FALSE(comparison.differences.empty());

  // Should detect the changed immediate value
  bool found_change = false;
  for (const auto& diff : comparison.differences) {
    if (diff.find("LDA") != std::string::npos) {
      found_change = true;
      break;
    }
  }
  EXPECT_TRUE(found_change);
}

TEST_F(RomDebugAgentTest, ComparePatch_DetectsDangerousPatterns) {
  // Original safe code
  std::vector<uint8_t> original = {
      0xA9, 0x10,  // LDA #$10
      0x60  // RTS
  };

  // Patched code with BRK (dangerous)
  MemoryResponse mem_resp;
  std::string patched_data = {
      (char)0x00,  // BRK (dangerous!)
      (char)0xEA   // NOP
  };
  mem_resp.set_data(patched_data);

  EXPECT_CALL(*mock_emulator_, ReadMemory(_, _, _))
      .WillOnce([&mem_resp](grpc::ServerContext*, const MemoryRequest*,
                           MemoryResponse* response) {
        *response = mem_resp;
        return grpc::Status::OK;
      });

  // Compare patch
  auto result = agent_->ComparePatch(0x008000, original.size(), original);

  ASSERT_TRUE(result.ok());
  auto& comparison = result.value();

  // Should detect BRK as a potential issue
  EXPECT_FALSE(comparison.potential_issues.empty());
  EXPECT_FALSE(comparison.is_safe);

  bool found_brk_issue = false;
  for (const auto& issue : comparison.potential_issues) {
    if (issue.find("BRK") != std::string::npos) {
      found_brk_issue = true;
      break;
    }
  }
  EXPECT_TRUE(found_brk_issue);
}

TEST_F(RomDebugAgentTest, ScanForIssues_DetectsInfiniteLoop) {
  // Code with infinite loop: BRA $-2
  MemoryResponse mem_resp;
  std::string code_with_loop = {
      (char)0x80, (char)0xFE,  // BRA $-2 (infinite loop)
      (char)0xEA  // NOP (unreachable)
  };
  mem_resp.set_data(code_with_loop);

  EXPECT_CALL(*mock_emulator_, ReadMemory(_, _, _))
      .WillOnce([&mem_resp](grpc::ServerContext*, const MemoryRequest*,
                           MemoryResponse* response) {
        *response = mem_resp;
        return grpc::Status::OK;
      });

  // Scan for issues
  auto issues = agent_->ScanForIssues(0x008000, 0x008003);

  ASSERT_FALSE(issues.empty());

  // Should detect the infinite loop
  bool found_loop = false;
  for (const auto& issue : issues) {
    if (issue.type == RomDebugAgent::IssueType::kInfiniteLoop) {
      found_loop = true;
      EXPECT_EQ(issue.address, 0x008000);
      EXPECT_EQ(issue.severity, 5);  // High severity
      break;
    }
  }
  EXPECT_TRUE(found_loop);
}

TEST_F(RomDebugAgentTest, IsValidJumpTarget) {
  // Test various address ranges
  EXPECT_TRUE(agent_->IsValidJumpTarget(0x008000));  // ROM start
  EXPECT_TRUE(agent_->IsValidJumpTarget(0x7E0000));  // WRAM start
  EXPECT_TRUE(agent_->IsValidJumpTarget(0x7EFFFF));  // WRAM end
  EXPECT_TRUE(agent_->IsValidJumpTarget(0x808000));  // Extended ROM

  // Invalid addresses
  EXPECT_FALSE(agent_->IsValidJumpTarget(0x700000));  // Invalid range
  EXPECT_FALSE(agent_->IsValidJumpTarget(0xF00000));  // Too high
}

TEST_F(RomDebugAgentTest, IsMemoryWriteSafe) {
  // Test critical areas
  EXPECT_FALSE(agent_->IsMemoryWriteSafe(0x00FFFA, 6));  // Interrupt vectors
  EXPECT_FALSE(agent_->IsMemoryWriteSafe(0x7E0012, 1));  // NMI flag
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
  EXPECT_EQ(agent_->IdentifyDataType(0x7EF340), "inventory");
  EXPECT_EQ(agent_->IdentifyDataType(0x008000), "code");
  EXPECT_EQ(agent_->IdentifyDataType(0x7E2000), "ram");
}

TEST_F(RomDebugAgentTest, FormatRegisterState) {
  std::map<std::string, uint16_t> regs = {
      {"A", 0x1234},
      {"X", 0x5678},
      {"Y", 0x9ABC},
      {"S", 0x01FF},
      {"PC", 0x8034},
      {"P", 0x30},
      {"DB", 0x00},
      {"PB", 0x00}
  };

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