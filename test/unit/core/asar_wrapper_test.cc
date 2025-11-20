#include "core/asar_wrapper.h"
#include "test_utils.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

namespace yaze {
namespace core {
namespace {

class AsarWrapperTest : public ::testing::Test {
 protected:
  void SetUp() override {
    wrapper_ = std::make_unique<AsarWrapper>();
    CreateTestFiles();
  }

  void TearDown() override { CleanupTestFiles(); }

  void CreateTestFiles() {
    // Create test directory
    test_dir_ = std::filesystem::temp_directory_path() / "yaze_asar_test";
    std::filesystem::create_directories(test_dir_);

    // Create a simple test assembly file
    test_asm_path_ = test_dir_ / "test_patch.asm";
    std::ofstream asm_file(test_asm_path_);
    asm_file << R"(
; Test assembly patch for yaze
org $008000
testlabel:
    LDA #$42
    STA $7E0000
    RTS

anotherlabel:
    LDA #$FF
    STA $7E0001
    RTL
)";
    asm_file.close();

    // Create invalid assembly file for error testing
    invalid_asm_path_ = test_dir_ / "invalid_patch.asm";
    std::ofstream invalid_file(invalid_asm_path_);
    invalid_file << R"(
; Invalid assembly that should cause errors
org $008000
invalid_instruction_here
LDA unknown_operand
)";
    invalid_file.close();

    // Create test ROM data using utility
    test_rom_ = yaze::test::TestRomManager::CreateMinimalTestRom(1024 * 1024);
  }

  void CleanupTestFiles() {
    try {
      if (std::filesystem::exists(test_dir_)) {
        std::filesystem::remove_all(test_dir_);
      }
    } catch (const std::exception& e) {
      // Ignore cleanup errors in tests
    }
  }

  std::unique_ptr<AsarWrapper> wrapper_;
  std::filesystem::path test_dir_;
  std::filesystem::path test_asm_path_;
  std::filesystem::path invalid_asm_path_;
  std::vector<uint8_t> test_rom_;
};

TEST_F(AsarWrapperTest, InitializationAndShutdown) {
  // Test initialization
  ASSERT_FALSE(wrapper_->IsInitialized());

  auto status = wrapper_->Initialize();
  EXPECT_TRUE(status.ok()) << status.message();
  EXPECT_TRUE(wrapper_->IsInitialized());

  // Test version info
  std::string version = wrapper_->GetVersion();
  EXPECT_FALSE(version.empty());
  EXPECT_NE(version, "Not initialized");

  int api_version = wrapper_->GetApiVersion();
  EXPECT_GT(api_version, 0);

  // Test shutdown
  wrapper_->Shutdown();
  EXPECT_FALSE(wrapper_->IsInitialized());
}

TEST_F(AsarWrapperTest, DoubleInitialization) {
  auto status1 = wrapper_->Initialize();
  EXPECT_TRUE(status1.ok());

  auto status2 = wrapper_->Initialize();
  EXPECT_TRUE(status2.ok());  // Should not fail on double init
  EXPECT_TRUE(wrapper_->IsInitialized());
}

TEST_F(AsarWrapperTest, OperationsWithoutInitialization) {
  // Operations should fail when not initialized
  ASSERT_FALSE(wrapper_->IsInitialized());

  std::vector<uint8_t> rom_copy = test_rom_;
  auto patch_result = wrapper_->ApplyPatch(test_asm_path_.string(), rom_copy);
  EXPECT_FALSE(patch_result.ok());
  EXPECT_THAT(patch_result.status().message(),
              testing::HasSubstr("not initialized"));

  auto symbols_result = wrapper_->ExtractSymbols(test_asm_path_.string());
  EXPECT_FALSE(symbols_result.ok());
  EXPECT_THAT(symbols_result.status().message(),
              testing::HasSubstr("not initialized"));
}

TEST_F(AsarWrapperTest, ValidPatchApplication) {
  ASSERT_TRUE(wrapper_->Initialize().ok());

  std::vector<uint8_t> rom_copy = test_rom_;
  size_t original_size = rom_copy.size();

  auto patch_result = wrapper_->ApplyPatch(test_asm_path_.string(), rom_copy);
  ASSERT_TRUE(patch_result.ok()) << patch_result.status().message();

  const auto& result = patch_result.value();
  EXPECT_TRUE(result.success)
      << "Patch failed: " << testing::PrintToString(result.errors);
  EXPECT_GT(result.rom_size, 0);
  EXPECT_EQ(rom_copy.size(), result.rom_size);

  // Check that ROM was actually modified
  EXPECT_NE(rom_copy, test_rom_);  // Should be different after patching
}

TEST_F(AsarWrapperTest, InvalidPatchHandling) {
  ASSERT_TRUE(wrapper_->Initialize().ok());

  std::vector<uint8_t> rom_copy = test_rom_;

  auto patch_result =
      wrapper_->ApplyPatch(invalid_asm_path_.string(), rom_copy);
  EXPECT_FALSE(patch_result.ok());
  EXPECT_THAT(patch_result.status().message(),
              testing::HasSubstr("Patch failed"));
}

TEST_F(AsarWrapperTest, NonexistentPatchFile) {
  ASSERT_TRUE(wrapper_->Initialize().ok());

  std::vector<uint8_t> rom_copy = test_rom_;
  std::string nonexistent_path = test_dir_.string() + "/nonexistent.asm";

  auto patch_result = wrapper_->ApplyPatch(nonexistent_path, rom_copy);
  EXPECT_FALSE(patch_result.ok());
}

TEST_F(AsarWrapperTest, SymbolExtraction) {
  ASSERT_TRUE(wrapper_->Initialize().ok());

  auto symbols_result = wrapper_->ExtractSymbols(test_asm_path_.string());
  ASSERT_TRUE(symbols_result.ok()) << symbols_result.status().message();

  const auto& symbols = symbols_result.value();
  EXPECT_GT(symbols.size(), 0);

  // Check for expected symbols from our test assembly
  bool found_testlabel = false;
  bool found_anotherlabel = false;

  for (const auto& symbol : symbols) {
    EXPECT_FALSE(symbol.name.empty());
    EXPECT_GT(symbol.address, 0);

    if (symbol.name == "testlabel") {
      found_testlabel = true;
      EXPECT_EQ(symbol.address,
                0x008000);  // Expected address from org directive
    } else if (symbol.name == "anotherlabel") {
      found_anotherlabel = true;
    }
  }

  EXPECT_TRUE(found_testlabel) << "Expected 'testlabel' symbol not found";
  EXPECT_TRUE(found_anotherlabel) << "Expected 'anotherlabel' symbol not found";
}

TEST_F(AsarWrapperTest, SymbolTableOperations) {
  ASSERT_TRUE(wrapper_->Initialize().ok());

  std::vector<uint8_t> rom_copy = test_rom_;
  auto patch_result = wrapper_->ApplyPatch(test_asm_path_.string(), rom_copy);
  ASSERT_TRUE(patch_result.ok());

  // Test symbol table retrieval
  auto symbol_table = wrapper_->GetSymbolTable();
  EXPECT_GT(symbol_table.size(), 0);

  // Test symbol lookup by name
  auto testlabel_symbol = wrapper_->FindSymbol("testlabel");
  EXPECT_TRUE(testlabel_symbol.has_value());
  if (testlabel_symbol) {
    EXPECT_EQ(testlabel_symbol->name, "testlabel");
    EXPECT_GT(testlabel_symbol->address, 0);
  }

  // Test lookup of non-existent symbol
  auto nonexistent_symbol = wrapper_->FindSymbol("nonexistent_symbol");
  EXPECT_FALSE(nonexistent_symbol.has_value());

  // Test symbols at address lookup
  if (testlabel_symbol) {
    auto symbols_at_addr =
        wrapper_->GetSymbolsAtAddress(testlabel_symbol->address);
    EXPECT_GT(symbols_at_addr.size(), 0);

    bool found = false;
    for (const auto& symbol : symbols_at_addr) {
      if (symbol.name == "testlabel") {
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found);
  }
}

TEST_F(AsarWrapperTest, PatchFromString) {
  ASSERT_TRUE(wrapper_->Initialize().ok());

  std::string patch_content = R"(
org $009000
stringpatchlabel:
    LDA #$55
    STA $7E0002
    RTS
)";

  std::vector<uint8_t> rom_copy = test_rom_;
  auto patch_result = wrapper_->ApplyPatchFromString(patch_content, rom_copy,
                                                     test_dir_.string());

  ASSERT_TRUE(patch_result.ok()) << patch_result.status().message();

  const auto& result = patch_result.value();
  EXPECT_TRUE(result.success);
  EXPECT_GT(result.symbols.size(), 0);

  // Check for the symbol we defined
  bool found_symbol = false;
  for (const auto& symbol : result.symbols) {
    if (symbol.name == "stringpatchlabel") {
      found_symbol = true;
      EXPECT_EQ(symbol.address, 0x009000);
      break;
    }
  }
  EXPECT_TRUE(found_symbol);
}

TEST_F(AsarWrapperTest, AssemblyValidation) {
  ASSERT_TRUE(wrapper_->Initialize().ok());

  // Test valid assembly
  auto valid_status = wrapper_->ValidateAssembly(test_asm_path_.string());
  EXPECT_TRUE(valid_status.ok()) << valid_status.message();

  // Test invalid assembly
  auto invalid_status = wrapper_->ValidateAssembly(invalid_asm_path_.string());
  EXPECT_FALSE(invalid_status.ok());
  EXPECT_THAT(invalid_status.message(),
              testing::AnyOf(testing::HasSubstr("validation failed"),
                             testing::HasSubstr("Patch failed"),
                             testing::HasSubstr("Unknown command"),
                             testing::HasSubstr("Label")));
}

TEST_F(AsarWrapperTest, ResetFunctionality) {
  ASSERT_TRUE(wrapper_->Initialize().ok());

  // Apply a patch to generate some state
  std::vector<uint8_t> rom_copy = test_rom_;
  auto patch_result = wrapper_->ApplyPatch(test_asm_path_.string(), rom_copy);
  ASSERT_TRUE(patch_result.ok());

  // Verify we have symbols and potentially warnings/errors
  auto symbol_table_before = wrapper_->GetSymbolTable();
  EXPECT_GT(symbol_table_before.size(), 0);

  // Reset and verify state is cleared
  wrapper_->Reset();

  auto symbol_table_after = wrapper_->GetSymbolTable();
  EXPECT_EQ(symbol_table_after.size(), 0);

  auto errors = wrapper_->GetLastErrors();
  auto warnings = wrapper_->GetLastWarnings();
  EXPECT_EQ(errors.size(), 0);
  EXPECT_EQ(warnings.size(), 0);
}

TEST_F(AsarWrapperTest, CreatePatchNotImplemented) {
  ASSERT_TRUE(wrapper_->Initialize().ok());

  std::vector<uint8_t> original_rom = test_rom_;
  std::vector<uint8_t> modified_rom = test_rom_;
  modified_rom[100] = 0x42;  // Make a small change

  std::string patch_path = test_dir_.string() + "/generated.asm";
  auto status = wrapper_->CreatePatch(original_rom, modified_rom, patch_path);

  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.message(), testing::HasSubstr("not yet implemented"));
}

}  // namespace
}  // namespace core
}  // namespace yaze
