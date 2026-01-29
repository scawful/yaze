#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <fstream>
#include <cstdio>

#include "core/asar_wrapper.h"

namespace yaze {
namespace core {
namespace {

// This test suite aims to reproduce the "AsarCompilerTest" failures reported by the user.
// Attempting to match the likely test cases: BasicCompilation, IncludeSearch, MultipleLabels.

class AsarCompilerReproTest : public ::testing::Test {
 protected:
  void SetUp() override {
    wrapper_ = std::make_unique<AsarWrapper>();
    ASSERT_TRUE(wrapper_->Initialize().ok());
  }

  std::unique_ptr<AsarWrapper> wrapper_;
};

TEST_F(AsarCompilerReproTest, BasicCompilation) {
  // Simple "lorom" patch
  std::string patch = R"(
    lorom
    org $008000
    db $EA ; NOP
  )";

  std::vector<uint8_t> rom_data(1024 * 1024, 0); // 1MB ROM
  auto result = wrapper_->ApplyPatchFromString(patch, rom_data, ".");
  
  EXPECT_TRUE(result.ok()) << result.status().message();
  if (result.ok()) {
    EXPECT_TRUE(result->success);
    // basic check
    EXPECT_EQ(rom_data[0], 0xEA); 
  }
}

TEST_F(AsarCompilerReproTest, IncludeSearch) {
  // Test incsrc resolution
  // We need to create a temporary include file
  std::string include_content = "db $FF";
  std::string include_filename = "temp_include.asm";
  
  // Write include file
  {
    std::ofstream out(include_filename);
    out << include_content;
  }

  std::string patch = R"(
    lorom
    org $008000
    incsrc "temp_include.asm"
  )";

  std::vector<uint8_t> rom_data(1024 * 1024, 0);
  auto result = wrapper_->ApplyPatchFromString(patch, rom_data, ".");

  EXPECT_TRUE(result.ok()) << result.status().message();
  if (result.ok()) {
    EXPECT_TRUE(result->success);
    EXPECT_EQ(rom_data[0], 0xFF);
  }

  // Cleanup
  std::remove(include_filename.c_str());
}

TEST_F(AsarCompilerReproTest, MultipleLabels) {
  // Test symbol extraction with multiple labels
  std::string patch = R"(
    lorom
    org $008000
    Label1:
      db $EA
    Label2:
      db $EA
  )";

  std::vector<uint8_t> rom_data(1024 * 1024, 0);
  auto result = wrapper_->ApplyPatchFromString(patch, rom_data, ".");

  EXPECT_TRUE(result.ok());
  if (result.ok()) {
    EXPECT_TRUE(result->success);
    bool found1 = false;
    bool found2 = false;
    for (const auto& sym : result->symbols) {
      if (sym.name == "Label1") found1 = true;
      if (sym.name == "Label2") found2 = true;
    }
    EXPECT_TRUE(found1) << "Label1 not found";
    EXPECT_TRUE(found2) << "Label2 not found";
  }
}

}  // namespace
}  // namespace core
}  // namespace yaze
