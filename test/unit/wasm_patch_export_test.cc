#include "app/platform/wasm/wasm_patch_export.h"

#include <gtest/gtest.h>
#include <vector>

namespace yaze {
namespace platform {
namespace {

// Test fixture for patch export tests
class WasmPatchExportTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create sample ROM data for testing
    original_.resize(1024, 0x00);
    modified_ = original_;

    // Make some modifications
    modified_[0x100] = 0xFF;  // Single byte change
    modified_[0x101] = 0xEE;
    modified_[0x102] = 0xDD;

    // Another region of changes
    for (int i = 0x200; i < 0x210; ++i) {
      modified_[i] = 0xAA;
    }
  }

  std::vector<uint8_t> original_;
  std::vector<uint8_t> modified_;
};

// Test GetPatchPreview functionality
TEST_F(WasmPatchExportTest, GetPatchPreview) {
  auto patch_info = WasmPatchExport::GetPatchPreview(original_, modified_);

#ifdef __EMSCRIPTEN__
  // In WASM builds, we expect actual functionality
  EXPECT_EQ(patch_info.changed_bytes, 19);  // 3 + 16 bytes changed
  EXPECT_EQ(patch_info.num_regions, 2);     // Two distinct regions
  ASSERT_EQ(patch_info.changed_regions.size(), 2);

  // Check first region
  EXPECT_EQ(patch_info.changed_regions[0].first, 0x100);  // Offset
  EXPECT_EQ(patch_info.changed_regions[0].second, 3);     // Length

  // Check second region
  EXPECT_EQ(patch_info.changed_regions[1].first, 0x200);  // Offset
  EXPECT_EQ(patch_info.changed_regions[1].second, 16);    // Length
#else
  // In non-WASM builds, expect stub implementation (empty results)
  EXPECT_EQ(patch_info.changed_bytes, 0);
  EXPECT_EQ(patch_info.num_regions, 0);
  EXPECT_TRUE(patch_info.changed_regions.empty());
#endif
}

// Test BPS export (stub in non-WASM)
TEST_F(WasmPatchExportTest, ExportBPS) {
  auto status = WasmPatchExport::ExportBPS(original_, modified_, "test.bps");

#ifdef __EMSCRIPTEN__
  // In WASM builds, should succeed (though download won't work in test env)
  EXPECT_FALSE(status.ok());  // Will fail in test environment without browser
#else
  // In non-WASM builds, expect unimplemented error
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kUnimplemented);
#endif
}

// Test IPS export (stub in non-WASM)
TEST_F(WasmPatchExportTest, ExportIPS) {
  auto status = WasmPatchExport::ExportIPS(original_, modified_, "test.ips");

#ifdef __EMSCRIPTEN__
  // In WASM builds, should succeed (though download won't work in test env)
  EXPECT_FALSE(status.ok());  // Will fail in test environment without browser
#else
  // In non-WASM builds, expect unimplemented error
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kUnimplemented);
#endif
}

// Test with empty data
TEST_F(WasmPatchExportTest, EmptyDataHandling) {
  std::vector<uint8_t> empty;

  auto patch_info = WasmPatchExport::GetPatchPreview(empty, modified_);
  EXPECT_EQ(patch_info.changed_bytes, 0);
  EXPECT_EQ(patch_info.num_regions, 0);

  auto status = WasmPatchExport::ExportBPS(empty, modified_, "test.bps");
  EXPECT_FALSE(status.ok());

  status = WasmPatchExport::ExportIPS(original_, empty, "test.ips");
  EXPECT_FALSE(status.ok());
}

// Test with identical ROMs (no changes)
TEST_F(WasmPatchExportTest, NoChanges) {
  auto patch_info = WasmPatchExport::GetPatchPreview(original_, original_);

#ifdef __EMSCRIPTEN__
  EXPECT_EQ(patch_info.changed_bytes, 0);
  EXPECT_EQ(patch_info.num_regions, 0);
  EXPECT_TRUE(patch_info.changed_regions.empty());
#else
  EXPECT_EQ(patch_info.changed_bytes, 0);
  EXPECT_EQ(patch_info.num_regions, 0);
#endif
}

// Test with ROM size increase
TEST_F(WasmPatchExportTest, ROMSizeIncrease) {
  // Expand modified ROM
  modified_.resize(2048, 0xBB);

  auto patch_info = WasmPatchExport::GetPatchPreview(original_, modified_);

#ifdef __EMSCRIPTEN__
  // Should detect the original changes plus the new data
  EXPECT_GT(patch_info.changed_bytes, 1000);  // At least 1024 new bytes
  EXPECT_GE(patch_info.num_regions, 3);       // Original 2 + expansion
#else
  EXPECT_EQ(patch_info.changed_bytes, 0);
  EXPECT_EQ(patch_info.num_regions, 0);
#endif
}

}  // namespace
}  // namespace platform
}  // namespace yaze