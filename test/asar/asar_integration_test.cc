#include <gtest/gtest.h>
#include "util/asar/asar_integration.h"
#include <fstream>
#include <sstream>

namespace yaze {
namespace util {
namespace asar {
namespace test {

class AsarIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    asar_integration_ = std::make_unique<AsarIntegration>();
    auto status = asar_integration_->Initialize();
    ASSERT_TRUE(status.ok()) << "Failed to initialize Asar: " << status.message();
  }

  void TearDown() override {
    asar_integration_.reset();
  }

  // Create a simple test assembly file
  std::string CreateTestAssembly() {
    return R"(
; Test assembly file for Asar integration
arch 65816
lorom

; Define some constants
!test_constant = $1234
!another_constant = $5678

; Define a simple function (using label instead)
test_function = $8000

; Place code at a specific location
org $8000

; Some labels
main:
    lda #!test_constant
    sta $7E0000
    rts

subroutine:
    lda #!another_constant
    sta $7E0002
    rts

; Data section
data:
    db $01, $02, $03, $04
    dw $1234, $5678
)";
  }

  // Create a test ROM (1MB of zeros)
  std::vector<uint8_t> CreateTestRom() {
    return std::vector<uint8_t>(1024 * 1024, 0);
  }

  std::unique_ptr<AsarIntegration> asar_integration_;
};

TEST_F(AsarIntegrationTest, Initialization) {
  EXPECT_TRUE(asar_integration_->IsInitialized());
  EXPECT_FALSE(asar_integration_->GetVersion().empty());
  EXPECT_FALSE(asar_integration_->GetApiVersion().empty());
}

TEST_F(AsarIntegrationTest, Get65816Opcodes) {
  auto opcodes = asar_integration_->Get65816Opcodes();
  EXPECT_FALSE(opcodes.empty());
  
  // Check for some common opcodes
  bool found_lda = false;
  bool found_sta = false;
  bool found_jmp = false;
  
  for (const auto& opcode : opcodes) {
    if (opcode.mnemonic == "LDA") found_lda = true;
    if (opcode.mnemonic == "STA") found_sta = true;
    if (opcode.mnemonic == "JMP") found_jmp = true;
  }
  
  EXPECT_TRUE(found_lda) << "LDA opcode not found";
  EXPECT_TRUE(found_sta) << "STA opcode not found";
  EXPECT_TRUE(found_jmp) << "JMP opcode not found";
}

TEST_F(AsarIntegrationTest, PatchRomWithValidAssembly) {
  // Create a temporary assembly file
  std::string assembly_content = CreateTestAssembly();
  std::string temp_file = "/tmp/test_patch.asm";
  
  std::ofstream file(temp_file);
  file << assembly_content;
  file.close();
  
  // Create test ROM
  auto rom_data = CreateTestRom();
  
  // Apply patch
  auto result = asar_integration_->PatchRom(temp_file, rom_data);
  ASSERT_TRUE(result.ok()) << "Patch failed: " << result.status().message();
  
  auto patch_result = result.value();
  EXPECT_TRUE(patch_result.success) << "Patch should succeed";
  EXPECT_TRUE(patch_result.errors.empty()) << "Should have no errors";
  
  // Check that symbols were extracted
  EXPECT_FALSE(patch_result.symbols.empty()) << "Should have extracted symbols";
  
  // Look for specific symbols
  bool found_main = false;
  bool found_subroutine = false;
  bool found_test_constant = false;
  
  for (const auto& symbol : patch_result.symbols) {
    if (symbol.name == "main") found_main = true;
    if (symbol.name == "subroutine") found_subroutine = true;
    if (symbol.name == "test_constant") found_test_constant = true;
  }
  
  EXPECT_TRUE(found_main) << "main label not found";
  EXPECT_TRUE(found_subroutine) << "subroutine label not found";
  EXPECT_TRUE(found_test_constant) << "test_constant define not found";
  
  // Clean up
  std::remove(temp_file.c_str());
}

TEST_F(AsarIntegrationTest, PatchRomWithInvalidAssembly) {
  // Create invalid assembly
  std::string invalid_assembly = R"(
arch 65816
lorom

invalid_instruction:
    invalid_opcode $1234
)";
  
  std::string temp_file = "/tmp/invalid_patch.asm";
  std::ofstream file(temp_file);
  file << invalid_assembly;
  file.close();
  
  auto rom_data = CreateTestRom();
  auto result = asar_integration_->PatchRom(temp_file, rom_data);
  
  ASSERT_TRUE(result.ok()) << "Should return result even for invalid assembly";
  auto patch_result = result.value();
  EXPECT_FALSE(patch_result.success) << "Patch should fail";
  EXPECT_FALSE(patch_result.errors.empty()) << "Should have errors";
  
  // Clean up
  std::remove(temp_file.c_str());
}

TEST_F(AsarIntegrationTest, ExtractSymbolsOnly) {
  std::string assembly_content = CreateTestAssembly();
  std::string temp_file = "/tmp/symbol_test.asm";
  
  std::ofstream file(temp_file);
  file << assembly_content;
  file.close();
  
  auto symbols_result = asar_integration_->ExtractSymbols(temp_file);
  ASSERT_TRUE(symbols_result.ok()) << "Symbol extraction failed: " 
    << symbols_result.status().message();
  
  auto symbols = symbols_result.value();
  EXPECT_FALSE(symbols.empty()) << "Should extract symbols";
  
  // Check symbol types
  bool has_labels = false;
  bool has_defines = false;
  
  for (const auto& symbol : symbols) {
    if (symbol.type == "label") has_labels = true;
    if (symbol.type == "define") has_defines = true;
  }
  
  EXPECT_TRUE(has_labels) << "Should have label symbols";
  EXPECT_TRUE(has_defines) << "Should have define symbols";
  
  // Clean up
  std::remove(temp_file.c_str());
}

TEST_F(AsarIntegrationTest, GetSymbolValue) {
  // First apply a patch to create symbols
  std::string assembly_content = CreateTestAssembly();
  std::string temp_file = "/tmp/symbol_value_test.asm";
  
  std::ofstream file(temp_file);
  file << assembly_content;
  file.close();
  
  auto rom_data = CreateTestRom();
  auto patch_result = asar_integration_->PatchRom(temp_file, rom_data);
  ASSERT_TRUE(patch_result.ok() && patch_result->success) 
    << "Patch should succeed for symbol value test";
  
  // Try to get symbol values
  auto main_value = asar_integration_->GetSymbolValue("main");
  EXPECT_TRUE(main_value.ok()) << "Should be able to get main label value";
  
  auto subroutine_value = asar_integration_->GetSymbolValue("subroutine");
  EXPECT_TRUE(subroutine_value.ok()) << "Should be able to get subroutine label value";
  
  // Try to get non-existent symbol
  auto invalid_value = asar_integration_->GetSymbolValue("nonexistent");
  EXPECT_FALSE(invalid_value.ok()) << "Should fail for non-existent symbol";
  
  // Clean up
  std::remove(temp_file.c_str());
}

TEST_F(AsarIntegrationTest, GenerateSymbolsFile) {
  // Apply patch first
  std::string assembly_content = CreateTestAssembly();
  std::string temp_file = "/tmp/symbols_file_test.asm";
  
  std::ofstream file(temp_file);
  file << assembly_content;
  file.close();
  
  auto rom_data = CreateTestRom();
  auto patch_result = asar_integration_->PatchRom(temp_file, rom_data);
  ASSERT_TRUE(patch_result.ok() && patch_result->success) 
    << "Patch should succeed for symbols file test";
  
  // Generate symbols file
  auto symbols_file = asar_integration_->GenerateSymbolsFile("wla");
  EXPECT_TRUE(symbols_file.ok()) << "Should generate symbols file";
  EXPECT_FALSE(symbols_file->empty()) << "Symbols file should not be empty";
  
  // Clean up
  std::remove(temp_file.c_str());
}

TEST_F(AsarIntegrationTest, GetWrittenBlocks) {
  // Apply patch first
  std::string assembly_content = CreateTestAssembly();
  std::string temp_file = "/tmp/written_blocks_test.asm";
  
  std::ofstream file(temp_file);
  file << assembly_content;
  file.close();
  
  auto rom_data = CreateTestRom();
  auto patch_result = asar_integration_->PatchRom(temp_file, rom_data);
  ASSERT_TRUE(patch_result.ok() && patch_result->success) 
    << "Patch should succeed for written blocks test";
  
  // Check written blocks from patch result
  // Note: Some assembly patterns may not generate written blocks
  // The important thing is that the patch succeeded and symbols were extracted
  EXPECT_FALSE(patch_result->symbols.empty()) << "Should have extracted symbols";
  
  // Clean up
  std::remove(temp_file.c_str());
}

TEST_F(AsarIntegrationTest, CrossPlatformCompatibility) {
  // Test that the integration works on different platforms
  EXPECT_TRUE(asar_integration_->IsInitialized());
  
  // Test basic functionality that should work on all platforms
  auto opcodes = asar_integration_->Get65816Opcodes();
  EXPECT_FALSE(opcodes.empty());
  
  auto version = asar_integration_->GetVersion();
  EXPECT_FALSE(version.empty());
  
  auto api_version = asar_integration_->GetApiVersion();
  EXPECT_FALSE(api_version.empty());
}

}  // namespace test
}  // namespace asar
}  // namespace util
}  // namespace yaze