// Integration tests for EmulatorRenderService
// Tests the shared render service architecture for ALTTP rendering

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "app/emu/render/emulator_render_service.h"
#include "app/emu/render/render_context.h"
#include "app/emu/render/save_state_manager.h"
#include "app/emu/snes.h"
#include "rom/rom.h"
#include "test_utils.h"

namespace yaze {
namespace test {

// =============================================================================
// RenderContext Unit Tests
// =============================================================================

class RenderContextTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(RenderContextTest, SnesToPcConversion_Bank01) {
  // Bank $01 handler tables
  EXPECT_EQ(emu::render::SnesToPc(0x018000), 0x8000u);
  EXPECT_EQ(emu::render::SnesToPc(0x018200), 0x8200u);
  EXPECT_EQ(emu::render::SnesToPc(0x0186F8), 0x86F8u);
  EXPECT_EQ(emu::render::SnesToPc(0x01FFFF), 0xFFFFu);
}

TEST_F(RenderContextTest, SnesToPcConversion_Bank00) {
  // Bank $00 code
  EXPECT_EQ(emu::render::SnesToPc(0x008000), 0x0000u);
  EXPECT_EQ(emu::render::SnesToPc(0x009B52), 0x1B52u);
  EXPECT_EQ(emu::render::SnesToPc(0x00FFFF), 0x7FFFu);
}

TEST_F(RenderContextTest, SnesToPcConversion_Bank0D) {
  // Bank $0D (palettes)
  EXPECT_EQ(emu::render::SnesToPc(0x0D8000), 0x68000u);
  EXPECT_EQ(emu::render::SnesToPc(0x0DD308), 0x6D308u);
  EXPECT_EQ(emu::render::SnesToPc(0x0DD734), 0x6D734u);
}

TEST_F(RenderContextTest, SnesToPcConversion_Bank02) {
  // Bank $02
  EXPECT_EQ(emu::render::SnesToPc(0x028000), 0x10000u);
  EXPECT_EQ(emu::render::SnesToPc(0x02FFFF), 0x17FFFu);
}

TEST_F(RenderContextTest, SnesToPcConversion_LowAddressPassThrough) {
  // Addresses below $8000 pass through unchanged
  EXPECT_EQ(emu::render::SnesToPc(0x000000), 0x0000u);
  EXPECT_EQ(emu::render::SnesToPc(0x007FFF), 0x7FFFu);
  EXPECT_EQ(emu::render::SnesToPc(0x7E0000), 0x7E0000u);  // WRAM
}

TEST_F(RenderContextTest, ConvertLinear8bppToPlanar4bpp_EmptyInput) {
  std::vector<uint8_t> empty;
  auto result = emu::render::ConvertLinear8bppToPlanar4bpp(empty);
  EXPECT_TRUE(result.empty());
}

TEST_F(RenderContextTest, ConvertLinear8bppToPlanar4bpp_SingleTile) {
  // 64 bytes input (one 8x8 tile at 8BPP)
  std::vector<uint8_t> tile(64, 0);
  auto result = emu::render::ConvertLinear8bppToPlanar4bpp(tile);

  // Output should be 32 bytes (4BPP)
  EXPECT_EQ(result.size(), 32u);
}

TEST_F(RenderContextTest, ConvertLinear8bppToPlanar4bpp_AllOnes) {
  // Pixel value 1 = bit 0 set
  std::vector<uint8_t> tile(64, 1);
  auto result = emu::render::ConvertLinear8bppToPlanar4bpp(tile);

  // With all pixels = 1, bitplane 0 should be all 0xFF
  for (int row = 0; row < 8; ++row) {
    EXPECT_EQ(result[row * 2], 0xFF) << "Row " << row << " bp0";
    EXPECT_EQ(result[row * 2 + 1], 0x00) << "Row " << row << " bp1";
    EXPECT_EQ(result[16 + row * 2], 0x00) << "Row " << row << " bp2";
    EXPECT_EQ(result[16 + row * 2 + 1], 0x00) << "Row " << row << " bp3";
  }
}

TEST_F(RenderContextTest, ConvertLinear8bppToPlanar4bpp_Value15) {
  // Pixel value 15 (0xF) = all 4 bits set
  std::vector<uint8_t> tile(64, 15);
  auto result = emu::render::ConvertLinear8bppToPlanar4bpp(tile);

  // All bitplanes should be 0xFF
  for (int row = 0; row < 8; ++row) {
    EXPECT_EQ(result[row * 2], 0xFF) << "Row " << row << " bp0";
    EXPECT_EQ(result[row * 2 + 1], 0xFF) << "Row " << row << " bp1";
    EXPECT_EQ(result[16 + row * 2], 0xFF) << "Row " << row << " bp2";
    EXPECT_EQ(result[16 + row * 2 + 1], 0xFF) << "Row " << row << " bp3";
  }
}

TEST_F(RenderContextTest, RenderRequestDefaultValues) {
  emu::render::RenderRequest req;

  EXPECT_EQ(req.type, emu::render::RenderTargetType::kDungeonObject);
  EXPECT_EQ(req.entity_id, 0);
  EXPECT_EQ(req.x, 0);
  EXPECT_EQ(req.y, 0);
  EXPECT_EQ(req.size, 0);
  EXPECT_EQ(req.room_id, 0);
  EXPECT_EQ(req.blockset, 0);
  EXPECT_EQ(req.palette, 0);
  EXPECT_EQ(req.spriteset, 0);
  EXPECT_EQ(req.output_width, 256);
  EXPECT_EQ(req.output_height, 256);
  EXPECT_TRUE(req.use_room_defaults);
}

TEST_F(RenderContextTest, RenderResultDefaultValues) {
  emu::render::RenderResult result;

  EXPECT_TRUE(result.rgba_pixels.empty());
  EXPECT_EQ(result.width, 0);
  EXPECT_EQ(result.height, 0);
  EXPECT_EQ(result.cycles_executed, 0);
}

TEST_F(RenderContextTest, StateMetadataDefaultValues) {
  emu::render::StateMetadata metadata;

  EXPECT_EQ(metadata.rom_checksum, 0u);
  EXPECT_EQ(metadata.rom_region, 0);
  EXPECT_EQ(metadata.room_id, 0);
  EXPECT_EQ(metadata.game_module, 0);
  EXPECT_EQ(metadata.version, 1u);
}

TEST_F(RenderContextTest, RomAddressConstants) {
  // Verify ROM address constants are defined correctly
  using namespace emu::render::rom_addresses;

  EXPECT_EQ(kType1DataTable, 0x018000u);
  EXPECT_EQ(kType1HandlerTable, 0x018200u);
  EXPECT_EQ(kType2DataTable, 0x018370u);
  EXPECT_EQ(kType2HandlerTable, 0x018470u);
  EXPECT_EQ(kType3DataTable, 0x0184F0u);
  EXPECT_EQ(kType3HandlerTable, 0x0185F0u);
}

TEST_F(RenderContextTest, WramAddressConstants) {
  // Verify WRAM address constants are defined correctly
  using namespace emu::render::wram_addresses;

  EXPECT_EQ(kBG1TilemapBuffer, 0x7E2000u);
  EXPECT_EQ(kBG2TilemapBuffer, 0x7E4000u);
  EXPECT_EQ(kTilemapBufferSize, 0x2000u);
  EXPECT_EQ(kRoomId, 0x7E00A0u);
  EXPECT_EQ(kGameModule, 0x7E0010u);
}

// =============================================================================
// CRC32 Unit Tests
// =============================================================================

class CRC32Test : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(CRC32Test, EmptyData) {
  std::vector<uint8_t> empty;
  uint32_t crc = emu::render::CalculateCRC32(empty.data(), empty.size());

  // CRC32 of empty data should be 0
  EXPECT_EQ(crc, 0x00000000u);
}

TEST_F(CRC32Test, KnownValue) {
  // "123456789" has a known CRC32 value
  const uint8_t test_data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  uint32_t crc = emu::render::CalculateCRC32(test_data, sizeof(test_data));

  // Known CRC32 of "123456789" is 0xCBF43926
  EXPECT_EQ(crc, 0xCBF43926u);
}

TEST_F(CRC32Test, Deterministic) {
  std::vector<uint8_t> data = {0xAB, 0xCD, 0xEF, 0x12, 0x34};
  uint32_t crc1 = emu::render::CalculateCRC32(data.data(), data.size());
  uint32_t crc2 = emu::render::CalculateCRC32(data.data(), data.size());

  EXPECT_EQ(crc1, crc2);
}

TEST_F(CRC32Test, DifferentData) {
  std::vector<uint8_t> data1 = {0x00, 0x01, 0x02};
  std::vector<uint8_t> data2 = {0x00, 0x01, 0x03};  // One byte different

  uint32_t crc1 = emu::render::CalculateCRC32(data1.data(), data1.size());
  uint32_t crc2 = emu::render::CalculateCRC32(data2.data(), data2.size());

  EXPECT_NE(crc1, crc2);
}

// =============================================================================
// EmulatorRenderService Unit Tests (no ROM required)
// =============================================================================

class EmulatorRenderServiceTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(EmulatorRenderServiceTest, NullRomReturnsNotReady) {
  emu::render::EmulatorRenderService service(nullptr);

  EXPECT_FALSE(service.IsReady());
}

TEST_F(EmulatorRenderServiceTest, InitializeWithNullRomFails) {
  emu::render::EmulatorRenderService service(nullptr);
  auto status = service.Initialize();

  EXPECT_FALSE(status.ok());
}

TEST_F(EmulatorRenderServiceTest, DefaultRenderModeIsHybrid) {
  emu::render::EmulatorRenderService service(nullptr);

  EXPECT_EQ(service.GetRenderMode(), emu::render::RenderMode::kHybrid);
}

TEST_F(EmulatorRenderServiceTest, SetRenderMode) {
  emu::render::EmulatorRenderService service(nullptr);

  service.SetRenderMode(emu::render::RenderMode::kStatic);
  EXPECT_EQ(service.GetRenderMode(), emu::render::RenderMode::kStatic);

  service.SetRenderMode(emu::render::RenderMode::kEmulated);
  EXPECT_EQ(service.GetRenderMode(), emu::render::RenderMode::kEmulated);

  service.SetRenderMode(emu::render::RenderMode::kHybrid);
  EXPECT_EQ(service.GetRenderMode(), emu::render::RenderMode::kHybrid);
}

// =============================================================================
// EmulatorRenderService Integration Tests (require ROM)
// =============================================================================

class EmulatorRenderServiceIntegrationTest
    : public TestRomManager::BoundRomTest {
 protected:
  void SetUp() override {
    BoundRomTest::SetUp();
    if (!rom_available()) {
      return;
    }
    service_ = std::make_unique<emu::render::EmulatorRenderService>(rom());
  }

  void TearDown() override {
    service_.reset();
    BoundRomTest::TearDown();
  }

  std::unique_ptr<emu::render::EmulatorRenderService> service_;
};

TEST_F(EmulatorRenderServiceIntegrationTest, InitializeSucceeds) {
  auto status = service_->Initialize();
  EXPECT_TRUE(status.ok()) << status.message();
  EXPECT_TRUE(service_->IsReady());
}

TEST_F(EmulatorRenderServiceIntegrationTest, SnesInstanceCreated) {
  auto status = service_->Initialize();
  ASSERT_TRUE(status.ok());

  EXPECT_NE(service_->snes(), nullptr);
}

TEST_F(EmulatorRenderServiceIntegrationTest, StateManagerCreated) {
  auto status = service_->Initialize();
  ASSERT_TRUE(status.ok());

  EXPECT_NE(service_->state_manager(), nullptr);
}

TEST_F(EmulatorRenderServiceIntegrationTest, RenderWithoutInitializeFails) {
  // Don't call Initialize()
  emu::render::RenderRequest request;
  request.type = emu::render::RenderTargetType::kDungeonObject;
  request.entity_id = 0x00;

  auto result = service_->Render(request);
  EXPECT_FALSE(result.ok());
}

TEST_F(EmulatorRenderServiceIntegrationTest, RenderStaticModeSucceeds) {
  auto status = service_->Initialize();
  ASSERT_TRUE(status.ok());

  service_->SetRenderMode(emu::render::RenderMode::kStatic);

  emu::render::RenderRequest request;
  request.type = emu::render::RenderTargetType::kDungeonObject;
  request.entity_id = 0x00;  // Object ID 0 (ceiling)
  request.room_id = 0;
  request.output_width = 64;
  request.output_height = 64;

  auto result = service_->Render(request);
  EXPECT_TRUE(result.ok()) << result.status().message();

  if (result.ok()) {
    EXPECT_EQ(result->width, 64);
    EXPECT_EQ(result->height, 64);
    // RGBA = 4 bytes per pixel
    EXPECT_EQ(result->rgba_pixels.size(), 64u * 64u * 4u);
  }
}

TEST_F(EmulatorRenderServiceIntegrationTest, RenderBatchEmpty) {
  auto status = service_->Initialize();
  ASSERT_TRUE(status.ok());

  std::vector<emu::render::RenderRequest> requests;
  auto results = service_->RenderBatch(requests);

  EXPECT_TRUE(results.ok());
  EXPECT_TRUE(results->empty());
}

TEST_F(EmulatorRenderServiceIntegrationTest, RenderBatchMultipleObjects) {
  auto status = service_->Initialize();
  ASSERT_TRUE(status.ok());

  service_->SetRenderMode(emu::render::RenderMode::kStatic);

  std::vector<emu::render::RenderRequest> requests;

  // Add a few different object types
  emu::render::RenderRequest req1;
  req1.type = emu::render::RenderTargetType::kDungeonObject;
  req1.entity_id = 0x00;
  req1.output_width = 32;
  req1.output_height = 32;
  requests.push_back(req1);

  emu::render::RenderRequest req2;
  req2.type = emu::render::RenderTargetType::kDungeonObject;
  req2.entity_id = 0x01;
  req2.output_width = 32;
  req2.output_height = 32;
  requests.push_back(req2);

  auto results = service_->RenderBatch(requests);
  EXPECT_TRUE(results.ok()) << results.status().message();

  if (results.ok()) {
    EXPECT_EQ(results->size(), 2u);
    for (const auto& result : *results) {
      EXPECT_EQ(result.width, 32);
      EXPECT_EQ(result.height, 32);
    }
  }
}

// =============================================================================
// SaveStateManager Integration Tests (require ROM)
// =============================================================================

class SaveStateManagerIntegrationTest : public TestRomManager::BoundRomTest {
 protected:
  void SetUp() override {
    BoundRomTest::SetUp();
    if (!rom_available()) {
      return;
    }
    snes_ = std::make_unique<emu::Snes>();
    snes_->Init(rom()->vector());
    manager_ = std::make_unique<emu::render::SaveStateManager>(snes_.get(), rom());
  }

  void TearDown() override {
    manager_.reset();
    snes_.reset();
    BoundRomTest::TearDown();
  }

  std::unique_ptr<emu::Snes> snes_;
  std::unique_ptr<emu::render::SaveStateManager> manager_;
};

TEST_F(SaveStateManagerIntegrationTest, CalculateRomChecksum) {
  uint32_t checksum = manager_->CalculateRomChecksum();

  // Checksum should be non-zero for a valid ROM
  EXPECT_NE(checksum, 0u);

  // Checksum should be deterministic
  uint32_t checksum2 = manager_->CalculateRomChecksum();
  EXPECT_EQ(checksum, checksum2);
}

TEST_F(SaveStateManagerIntegrationTest, NoCachedStatesInitially) {
  EXPECT_FALSE(manager_->HasCachedState(emu::render::StateType::kRoomLoaded));
  EXPECT_FALSE(manager_->HasCachedState(emu::render::StateType::kOverworldLoaded));
  EXPECT_FALSE(manager_->HasCachedState(emu::render::StateType::kBlankCanvas));
}

TEST_F(SaveStateManagerIntegrationTest, LoadStateWithoutCacheFails) {
  auto result = manager_->LoadState(emu::render::StateType::kRoomLoaded);
  EXPECT_FALSE(result.ok());
}

TEST_F(SaveStateManagerIntegrationTest, GetStateMetadataWithoutCacheFails) {
  auto result = manager_->GetStateMetadata(emu::render::StateType::kRoomLoaded);
  EXPECT_FALSE(result.ok());
}

TEST_F(SaveStateManagerIntegrationTest, SetAndGetStateDirectory) {
  const std::string test_path = "/tmp/test_states";
  manager_->SetStateDirectory(test_path);
  EXPECT_EQ(manager_->GetStateDirectory(), test_path);
}

// =============================================================================
// Button Constants Tests
// =============================================================================

TEST(ButtonConstantsTest, ButtonValuesCorrect) {
  using namespace emu::render::buttons;

  // Verify button bit indices match SNES controller layout (0-11)
  EXPECT_EQ(kB, 0);
  EXPECT_EQ(kY, 1);
  EXPECT_EQ(kSelect, 2);
  EXPECT_EQ(kStart, 3);
  EXPECT_EQ(kUp, 4);
  EXPECT_EQ(kDown, 5);
  EXPECT_EQ(kLeft, 6);
  EXPECT_EQ(kRight, 7);
  EXPECT_EQ(kA, 8);
  EXPECT_EQ(kX, 9);
  EXPECT_EQ(kL, 10);
  EXPECT_EQ(kR, 11);
}

TEST(ButtonConstantsTest, ButtonsAreMutuallyExclusive) {
  using namespace emu::render::buttons;

  // Build bitmask from bit indices and ensure no overlap
  uint16_t mask = 0;
  mask |= (1 << kA);
  mask |= (1 << kB);
  mask |= (1 << kX);
  mask |= (1 << kY);
  mask |= (1 << kL);
  mask |= (1 << kR);
  mask |= (1 << kStart);
  mask |= (1 << kSelect);
  mask |= (1 << kUp);
  mask |= (1 << kDown);
  mask |= (1 << kLeft);
  mask |= (1 << kRight);

  // All twelve unique bits should be set exactly once
  EXPECT_EQ(__builtin_popcount(mask), 12);
}

}  // namespace test
}  // namespace yaze
