// Integration tests for DungeonObjectEmulatorPreview
// Tests the SNES emulator-based object rendering pipeline

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "app/emu/snes.h"
#include "app/rom.h"
#include "test_utils.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace test {

namespace {

// Convert 8BPP linear tile data to 4BPP SNES planar format
// This is a copy of the function in dungeon_object_emulator_preview.cc for testing
std::vector<uint8_t> ConvertLinear8bppToPlanar4bpp(
    const std::vector<uint8_t>& linear_data) {
  size_t num_tiles = linear_data.size() / 64;  // 64 bytes per 8x8 tile
  std::vector<uint8_t> planar_data(num_tiles * 32);  // 32 bytes per tile

  for (size_t tile = 0; tile < num_tiles; ++tile) {
    const uint8_t* src = linear_data.data() + tile * 64;
    uint8_t* dst = planar_data.data() + tile * 32;

    for (int row = 0; row < 8; ++row) {
      uint8_t bp0 = 0, bp1 = 0, bp2 = 0, bp3 = 0;

      for (int col = 0; col < 8; ++col) {
        uint8_t pixel = src[row * 8 + col] & 0x0F;  // Low 4 bits only
        int bit = 7 - col;  // MSB first

        bp0 |= ((pixel >> 0) & 1) << bit;
        bp1 |= ((pixel >> 1) & 1) << bit;
        bp2 |= ((pixel >> 2) & 1) << bit;
        bp3 |= ((pixel >> 3) & 1) << bit;
      }

      // SNES 4BPP interleaving: bp0,bp1 for rows 0-7 first, then bp2,bp3
      dst[row * 2] = bp0;
      dst[row * 2 + 1] = bp1;
      dst[16 + row * 2] = bp2;
      dst[16 + row * 2 + 1] = bp3;
    }
  }

  return planar_data;
}

}  // namespace

// =============================================================================
// Unit Tests for 8BPP to 4BPP Conversion
// =============================================================================

class BppConversionTest : public ::testing::Test {
 protected:
  // Create a simple test tile with known pixel values
  std::vector<uint8_t> CreateTestTile(uint8_t fill_value) {
    std::vector<uint8_t> tile(64, fill_value);
    return tile;
  }

  // Create a gradient tile for testing bit extraction
  std::vector<uint8_t> CreateGradientTile() {
    std::vector<uint8_t> tile(64);
    for (int i = 0; i < 64; ++i) {
      tile[i] = i % 16;  // Values 0-15 repeating
    }
    return tile;
  }
};

TEST_F(BppConversionTest, EmptyInputProducesEmptyOutput) {
  std::vector<uint8_t> empty;
  auto result = ConvertLinear8bppToPlanar4bpp(empty);
  EXPECT_TRUE(result.empty());
}

TEST_F(BppConversionTest, SingleTileProducesCorrectSize) {
  auto tile = CreateTestTile(0);
  auto result = ConvertLinear8bppToPlanar4bpp(tile);

  // 64 bytes input (8BPP) -> 32 bytes output (4BPP)
  EXPECT_EQ(result.size(), 32u);
}

TEST_F(BppConversionTest, MultipleTilesProduceCorrectSize) {
  // Create 4 tiles (256 bytes)
  std::vector<uint8_t> tiles(256, 0);
  auto result = ConvertLinear8bppToPlanar4bpp(tiles);

  // 256 bytes input -> 128 bytes output
  EXPECT_EQ(result.size(), 128u);
}

TEST_F(BppConversionTest, AllZerosProducesAllZeros) {
  auto tile = CreateTestTile(0);
  auto result = ConvertLinear8bppToPlanar4bpp(tile);

  for (uint8_t byte : result) {
    EXPECT_EQ(byte, 0u);
  }
}

TEST_F(BppConversionTest, AllOnesProducesCorrectPattern) {
  // Pixel value 1 = bit 0 set
  auto tile = CreateTestTile(1);
  auto result = ConvertLinear8bppToPlanar4bpp(tile);

  // With all pixels = 1, bitplane 0 should be all 0xFF
  // Bitplanes 1, 2, 3 should be all 0x00
  for (int row = 0; row < 8; ++row) {
    EXPECT_EQ(result[row * 2], 0xFF) << "Row " << row << " bp0";
    EXPECT_EQ(result[row * 2 + 1], 0x00) << "Row " << row << " bp1";
    EXPECT_EQ(result[16 + row * 2], 0x00) << "Row " << row << " bp2";
    EXPECT_EQ(result[16 + row * 2 + 1], 0x00) << "Row " << row << " bp3";
  }
}

TEST_F(BppConversionTest, Value15ProducesAllBitsSet) {
  // Pixel value 15 (0xF) = all 4 bits set
  auto tile = CreateTestTile(15);
  auto result = ConvertLinear8bppToPlanar4bpp(tile);

  // All bitplanes should be 0xFF
  for (int row = 0; row < 8; ++row) {
    EXPECT_EQ(result[row * 2], 0xFF) << "Row " << row << " bp0";
    EXPECT_EQ(result[row * 2 + 1], 0xFF) << "Row " << row << " bp1";
    EXPECT_EQ(result[16 + row * 2], 0xFF) << "Row " << row << " bp2";
    EXPECT_EQ(result[16 + row * 2 + 1], 0xFF) << "Row " << row << " bp3";
  }
}

TEST_F(BppConversionTest, HighBitsAreIgnored) {
  // Pixel value 0xFF should be treated as 0x0F (low 4 bits only)
  auto tile_ff = CreateTestTile(0xFF);
  auto tile_0f = CreateTestTile(0x0F);

  auto result_ff = ConvertLinear8bppToPlanar4bpp(tile_ff);
  auto result_0f = ConvertLinear8bppToPlanar4bpp(tile_0f);

  EXPECT_EQ(result_ff, result_0f);
}

TEST_F(BppConversionTest, SinglePixelBitplaneExtraction) {
  // Create a tile with just the first pixel set to value 5 (0101 binary)
  std::vector<uint8_t> tile(64, 0);
  tile[0] = 5;  // First pixel = 0101

  auto result = ConvertLinear8bppToPlanar4bpp(tile);

  // First pixel is at MSB position (bit 7) of first row
  // Value 5 = 0101 = bp0=1, bp1=0, bp2=1, bp3=0
  EXPECT_EQ(result[0] & 0x80, 0x80) << "bp0 bit 7 should be set";
  EXPECT_EQ(result[1] & 0x80, 0x00) << "bp1 bit 7 should be clear";
  EXPECT_EQ(result[16] & 0x80, 0x80) << "bp2 bit 7 should be set";
  EXPECT_EQ(result[17] & 0x80, 0x00) << "bp3 bit 7 should be clear";
}

TEST_F(BppConversionTest, GradientTileConversion) {
  auto tile = CreateGradientTile();
  auto result = ConvertLinear8bppToPlanar4bpp(tile);

  // Verify size
  EXPECT_EQ(result.size(), 32u);

  // The gradient should produce non-trivial bitplane data
  bool has_nonzero_bp0 = false;
  bool has_nonzero_bp1 = false;
  bool has_nonzero_bp2 = false;
  bool has_nonzero_bp3 = false;

  for (int row = 0; row < 8; ++row) {
    if (result[row * 2] != 0) has_nonzero_bp0 = true;
    if (result[row * 2 + 1] != 0) has_nonzero_bp1 = true;
    if (result[16 + row * 2] != 0) has_nonzero_bp2 = true;
    if (result[16 + row * 2 + 1] != 0) has_nonzero_bp3 = true;
  }

  EXPECT_TRUE(has_nonzero_bp0) << "Gradient should have non-zero bp0";
  EXPECT_TRUE(has_nonzero_bp1) << "Gradient should have non-zero bp1";
  EXPECT_TRUE(has_nonzero_bp2) << "Gradient should have non-zero bp2";
  EXPECT_TRUE(has_nonzero_bp3) << "Gradient should have non-zero bp3";
}

// =============================================================================
// Integration Tests with SNES Emulator (requires ROM)
// =============================================================================

class EmulatorObjectPreviewTest : public TestRomManager::BoundRomTest {
 protected:
  void SetUp() override {
    BoundRomTest::SetUp();

    // Initialize SNES emulator with ROM
    snes_ = std::make_unique<emu::Snes>();
    snes_->Init(rom()->vector());
  }

  void TearDown() override {
    snes_.reset();
    BoundRomTest::TearDown();
  }

  // Setup CPU state for object handler execution
  void SetupCpuForHandler(uint16_t handler_offset) {
    auto& cpu = snes_->cpu();

    // Reset and configure
    snes_->Reset(true);

    cpu.PB = 0x01;      // Program bank
    cpu.DB = 0x7E;      // Data bank (WRAM)
    cpu.D = 0x0000;     // Direct page
    cpu.SetSP(0x01FF);  // Stack pointer
    cpu.status = 0x30;  // 8-bit A/X/Y mode

    // Set PC to handler
    cpu.PC = handler_offset;
  }

  // Lookup object handler from ROM
  uint16_t LookupObjectHandler(int object_id) {
    auto rom_data = rom()->data();
    uint32_t table_addr = 0;

    if (object_id < 0x100) {
      table_addr = 0x018200 + (object_id * 2);
    } else if (object_id < 0x200) {
      table_addr = 0x018470 + ((object_id - 0x100) * 2);
    } else {
      table_addr = 0x0185F0 + ((object_id - 0x200) * 2);
    }

    if (table_addr < rom()->size() - 1) {
      return rom_data[table_addr] | (rom_data[table_addr + 1] << 8);
    }
    return 0;
  }

  std::unique_ptr<emu::Snes> snes_;
};

TEST_F(EmulatorObjectPreviewTest, SnesInitializesCorrectly) {
  ASSERT_NE(snes_, nullptr);

  // Verify CPU is accessible
  auto& cpu = snes_->cpu();
  EXPECT_EQ(cpu.PB, 0x00);  // After init, PB should be 0
}

TEST_F(EmulatorObjectPreviewTest, ObjectHandlerTableLookup) {
  // Test that handler table addresses are valid

  // Object 0x00 should have a handler
  uint16_t handler_0 = LookupObjectHandler(0x00);
  EXPECT_NE(handler_0, 0x0000) << "Object 0x00 should have a handler";

  // Object 0x100 (Type 2)
  uint16_t handler_100 = LookupObjectHandler(0x100);
  // May or may not have handler, just verify lookup doesn't crash

  // Object 0x200 (Type 3)
  uint16_t handler_200 = LookupObjectHandler(0x200);
  // May or may not have handler

  printf("[TEST] Handler 0x00 = $%04X\n", handler_0);
  printf("[TEST] Handler 0x100 = $%04X\n", handler_100);
  printf("[TEST] Handler 0x200 = $%04X\n", handler_200);
}

// DISABLED: CPU execution from manually-set PC doesn't work as expected.
// After Init(), the emulator's internal state causes RunOpcode() to
// jump to the reset vector ($8000) instead of executing from the set PC.
// This documents a limitation in using the emulator for isolated code execution.
TEST_F(EmulatorObjectPreviewTest, DISABLED_CpuCanExecuteInstructions) {
  auto& cpu = snes_->cpu();

  // Write a NOP (EA) instruction to WRAM at a known location
  snes_->Write(0x7E1000, 0xEA);  // NOP
  snes_->Write(0x7E1001, 0xEA);  // NOP
  snes_->Write(0x7E1002, 0xEA);  // NOP

  // Verify the writes worked
  EXPECT_EQ(snes_->Read(0x7E1000), 0xEA) << "WRAM write should persist";

  // Setup CPU to execute from WRAM
  cpu.PB = 0x7E;
  cpu.PC = 0x1000;
  cpu.DB = 0x7E;
  cpu.SetSP(0x01FF);
  cpu.status = 0x30;

  uint16_t initial_pc = cpu.PC;

  // Execute one NOP instruction
  cpu.RunOpcode();

  // NOP is a 1-byte instruction, so PC should advance by 1
  EXPECT_EQ(cpu.PC, initial_pc + 1)
      << "PC should advance by 1 after NOP (was " << initial_pc
      << ", now " << cpu.PC << ")";
}

TEST_F(EmulatorObjectPreviewTest, WramReadWrite) {
  // Test WRAM access
  const uint32_t test_addr = 0x7E2000;

  // Write test pattern
  snes_->Write(test_addr, 0xAB);
  snes_->Write(test_addr + 1, 0xCD);

  // Read back
  uint8_t lo = snes_->Read(test_addr);
  uint8_t hi = snes_->Read(test_addr + 1);

  EXPECT_EQ(lo, 0xAB);
  EXPECT_EQ(hi, 0xCD);

  uint16_t word = lo | (hi << 8);
  EXPECT_EQ(word, 0xCDAB);
}

TEST_F(EmulatorObjectPreviewTest, VramCanBeWritten) {
  auto& ppu = snes_->ppu();

  // Write test data to VRAM
  ppu.vram[0] = 0x1234;
  ppu.vram[1] = 0x5678;

  EXPECT_EQ(ppu.vram[0], 0x1234);
  EXPECT_EQ(ppu.vram[1], 0x5678);
}

TEST_F(EmulatorObjectPreviewTest, CgramCanBeWritten) {
  auto& ppu = snes_->ppu();

  // Write test palette data to CGRAM
  ppu.cgram[0] = 0x0000;  // Black
  ppu.cgram[1] = 0x7FFF;  // White
  ppu.cgram[2] = 0x001F;  // Red

  EXPECT_EQ(ppu.cgram[0], 0x0000);
  EXPECT_EQ(ppu.cgram[1], 0x7FFF);
  EXPECT_EQ(ppu.cgram[2], 0x001F);
}

TEST_F(EmulatorObjectPreviewTest, RoomGraphicsCanBeLoaded) {
  // Load room 0
  zelda3::Room room = zelda3::LoadRoomFromRom(rom(), 0);

  // Load graphics
  room.LoadRoomGraphics(room.blockset);
  room.CopyRoomGraphicsToBuffer();

  const auto& gfx_buffer = room.get_gfx_buffer();

  // Verify buffer is populated
  EXPECT_EQ(gfx_buffer.size(), 65536u) << "Graphics buffer should be 64KB";

  // Count non-zero bytes
  int nonzero_count = 0;
  for (uint8_t byte : gfx_buffer) {
    if (byte != 0) nonzero_count++;
  }

  EXPECT_GT(nonzero_count, 0) << "Graphics buffer should have non-zero data";
  printf("[TEST] Graphics buffer: %d non-zero bytes out of 65536\n", nonzero_count);
}

TEST_F(EmulatorObjectPreviewTest, GraphicsConversionProducesValidData) {
  // Load room graphics
  zelda3::Room room = zelda3::LoadRoomFromRom(rom(), 0);
  room.LoadRoomGraphics(room.blockset);
  room.CopyRoomGraphicsToBuffer();

  const auto& gfx_buffer = room.get_gfx_buffer();

  // Convert to 4BPP planar
  std::vector<uint8_t> linear_data(gfx_buffer.begin(), gfx_buffer.end());
  auto planar_data = ConvertLinear8bppToPlanar4bpp(linear_data);

  // Verify conversion
  EXPECT_EQ(planar_data.size(), 32768u) << "4BPP should be half the size of 8BPP";

  // Count non-zero bytes in converted data
  int nonzero_count = 0;
  for (uint8_t byte : planar_data) {
    if (byte != 0) nonzero_count++;
  }

  EXPECT_GT(nonzero_count, 0) << "Converted data should have non-zero bytes";
  printf("[TEST] Planar data: %d non-zero bytes out of 32768\n", nonzero_count);
}

// Test documenting current limitation - handlers require full game state
TEST_F(EmulatorObjectPreviewTest, DISABLED_HandlerExecutionRequiresGameState) {
  // This test documents that object handlers require more game state
  // than we currently provide. The handlers jump to routines in bank 0
  // that depend on game variables being initialized.

  uint16_t handler = LookupObjectHandler(0x00);
  ASSERT_NE(handler, 0x0000) << "Object 0x00 should have a handler";

  SetupCpuForHandler(handler);
  auto& cpu = snes_->cpu();

  // Initialize WRAM tilemap area
  for (uint32_t i = 0; i < 0x2000; i++) {
    snes_->Write(0x7E2000 + i, 0x00);
  }

  // Setup return address at $01:8000
  snes_->Write(0x018000, 0x6B);  // RTL
  uint16_t sp = cpu.SP();
  snes_->Write(0x010000 | sp--, 0x01);
  snes_->Write(0x010000 | sp--, 0x7F);
  snes_->Write(0x010000 | sp--, 0xFF);
  cpu.SetSP(sp);

  // Execute some opcodes
  int opcodes = 0;
  int max_opcodes = 1000;
  while (opcodes < max_opcodes) {
    if (cpu.PB == 0x01 && cpu.PC == 0x8000) {
      break;
    }
    cpu.RunOpcode();
    opcodes++;
  }

  printf("[TEST] Executed %d opcodes, final PC=$%02X:%04X\n",
         opcodes, cpu.PB, cpu.PC);

  // Check if anything was written to WRAM tilemap
  bool has_tilemap_data = false;
  for (uint32_t i = 0; i < 0x100; i++) {
    if (snes_->Read(0x7E2000 + i) != 0) {
      has_tilemap_data = true;
      break;
    }
  }

  // Currently expected to fail - handlers need more game state
  EXPECT_TRUE(has_tilemap_data)
      << "Handler should write to tilemap (currently fails - needs game state)";
}

}  // namespace test
}  // namespace yaze
