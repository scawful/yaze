#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "rom/rom.h"
#include "rom/snes.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/water_fill_zone.h"

namespace yaze {
namespace zelda3 {
namespace test {

namespace {

std::unique_ptr<Rom> MakeDummyRom(size_t size_bytes) {
  auto rom = std::make_unique<Rom>();
  std::vector<uint8_t> dummy(size_bytes, 0);
  auto status = rom->LoadFromData(dummy);
  EXPECT_TRUE(status.ok()) << status.message();
  return rom;
}

struct TempFile {
  std::string path;

  TempFile() {
    const auto temp_dir = std::filesystem::temp_directory_path();
    const auto seed =
        std::chrono::steady_clock::now().time_since_epoch().count();
    for (int attempt = 0; attempt < 16; ++attempt) {
      const auto candidate =
          temp_dir / ("yaze_water_fill_zone_test_" +
                      std::to_string(seed + attempt) + ".tmp");
      if (std::filesystem::exists(candidate)) {
        continue;
      }
      std::ofstream touch(candidate);
      if (!touch.good()) {
        continue;
      }
      touch.close();
      path = candidate.string();
      break;
    }
    EXPECT_FALSE(path.empty());
  }

  ~TempFile() {
    if (!path.empty()) {
      std::remove(path.c_str());
    }
  }
};

}  // namespace

TEST(WaterFillZoneTest, WriteThenLoadRoundTrip) {
  auto rom = MakeDummyRom(0x200000);

  std::vector<WaterFillZoneEntry> zones;
  {
    WaterFillZoneEntry z;
    z.room_id = 0x27;
    z.sram_bit_mask = 0x01;
    z.fill_offsets = {4095, 10, 10, 0};
    zones.push_back(std::move(z));
  }
  {
    WaterFillZoneEntry z;
    z.room_id = 0x25;
    z.sram_bit_mask = 0x02;
    z.fill_offsets = {1234, 2, 3};
    zones.push_back(std::move(z));
  }

  ASSERT_TRUE(WriteWaterFillTable(rom.get(), zones).ok());

  auto loaded_or = LoadWaterFillTable(rom.get());
  ASSERT_TRUE(loaded_or.ok()) << loaded_or.status().message();
  auto loaded = std::move(loaded_or.value());
  ASSERT_EQ(loaded.size(), 2u);

  // Writer normalizes by room_id and de-dups/sorts offsets.
  EXPECT_EQ(loaded[0].room_id, 0x25);
  EXPECT_EQ(loaded[0].sram_bit_mask, 0x02);
  EXPECT_EQ(loaded[0].fill_offsets, (std::vector<uint16_t>{2, 3, 1234}));

  EXPECT_EQ(loaded[1].room_id, 0x27);
  EXPECT_EQ(loaded[1].sram_bit_mask, 0x01);
  EXPECT_EQ(loaded[1].fill_offsets, (std::vector<uint16_t>{0, 10, 4095}));
}

TEST(WaterFillZoneTest, WriteRejectsInvalidMask) {
  auto rom = MakeDummyRom(0x200000);

  WaterFillZoneEntry z;
  z.room_id = 0x25;
  z.sram_bit_mask = 0x03;  // Not a single bit
  z.fill_offsets = {0};

  auto status = WriteWaterFillTable(rom.get(), {z});
  EXPECT_FALSE(status.ok());
}

TEST(WaterFillZoneTest, WriteRejectsRoomIdOutsideRuntimeByteRange) {
  auto rom = MakeDummyRom(0x200000);
  const auto rom_before = rom->vector();

  WaterFillZoneEntry z;
  z.room_id = 0x125;
  z.sram_bit_mask = 0x01;
  z.fill_offsets = {0};

  const auto status = WriteWaterFillTable(rom.get(), {z});
  EXPECT_EQ(status.code(), absl::StatusCode::kOutOfRange);
  EXPECT_EQ(rom->vector(), rom_before);
}

TEST(WaterFillZoneTest, WriteAcceptsMaximumRuntimeRoomId) {
  auto rom = MakeDummyRom(0x200000);

  WaterFillZoneEntry z;
  z.room_id = 0xFF;
  z.sram_bit_mask = 0x01;
  z.fill_offsets = {0};

  ASSERT_TRUE(WriteWaterFillTable(rom.get(), {z}).ok());
  const auto loaded_or = LoadWaterFillTable(rom.get());
  ASSERT_TRUE(loaded_or.ok()) << loaded_or.status().message();
  ASSERT_EQ(loaded_or->size(), 1u);
  EXPECT_EQ(loaded_or->front().room_id, 0xFF);
}

TEST(WaterFillZoneTest, WriteRejectsDuplicateMask) {
  auto rom = MakeDummyRom(0x200000);

  WaterFillZoneEntry a;
  a.room_id = 0x25;
  a.sram_bit_mask = 0x01;
  a.fill_offsets = {0};

  WaterFillZoneEntry b;
  b.room_id = 0x27;
  b.sram_bit_mask = 0x01;  // Duplicate
  b.fill_offsets = {1};

  auto status = WriteWaterFillTable(rom.get(), {a, b});
  EXPECT_FALSE(status.ok());
}

TEST(WaterFillZoneTest, WriteRejectsTooManyZones) {
  auto rom = MakeDummyRom(0x200000);

  std::vector<WaterFillZoneEntry> zones;
  for (int i = 0; i < 9; ++i) {
    WaterFillZoneEntry z;
    z.room_id = i;
    z.sram_bit_mask = static_cast<uint8_t>(1u << (i % 8));
    z.fill_offsets = {0};
    zones.push_back(std::move(z));
  }

  auto status = WriteWaterFillTable(rom.get(), zones);
  EXPECT_FALSE(status.ok());
}

TEST(WaterFillZoneTest, FullD4PoolsUseWordCountsAndRoundTrip) {
  auto rom = MakeDummyRom(0x200000);

  WaterFillZoneEntry z;
  z.room_id = 0x25;
  z.sram_bit_mask = 0x02;
  z.fill_offsets.resize(594);
  for (int i = 0; i < 594; ++i) {
    z.fill_offsets[i] = static_cast<uint16_t>(i);
  }

  WaterFillZoneEntry peer{0x27, 0x01, {}};
  for (int i = 0; i < 296; ++i)
    peer.fill_offsets.push_back(static_cast<uint16_t>(i));
  const auto before = rom->vector();
  ASSERT_TRUE(
      WriteWaterFillTable(rom.get(), {z, peer}, WaterFillTableFormat::kV2)
          .ok());
  EXPECT_EQ(rom->vector()[kWaterFillTableStart], 0x82);
  EXPECT_EQ(rom->vector()[kWaterFillTableStart + 9], 0x52);
  EXPECT_EQ(rom->vector()[kWaterFillTableStart + 10], 0x02);
  const auto loaded = LoadWaterFillTable(rom.get());
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  ASSERT_EQ(loaded->size(), 2u);
  EXPECT_EQ((*loaded)[0].fill_offsets, z.fill_offsets);
  EXPECT_EQ((*loaded)[1].fill_offsets, peer.fill_offsets);
  TempFile saved;
  ASSERT_FALSE(saved.path.empty());
  ASSERT_TRUE(rom->SaveToFile({.save_new = true, .filename = saved.path}).ok());
  Rom reopened;
  ASSERT_TRUE(reopened.LoadFromFile(saved.path).ok());
  const auto from_disk = LoadWaterFillTable(&reopened);
  ASSERT_TRUE(from_disk.ok()) << from_disk.status();
  ASSERT_EQ(from_disk->size(), 2u);
  EXPECT_EQ((*from_disk)[0].fill_offsets, z.fill_offsets);
  EXPECT_EQ((*from_disk)[1].fill_offsets, peer.fill_offsets);
  EXPECT_TRUE(std::equal(before.begin(), before.begin() + kWaterFillTableStart,
                         rom->vector().begin()));
  EXPECT_TRUE(std::equal(before.begin() + kWaterFillTableEnd, before.end(),
                         rom->vector().begin() + kWaterFillTableEnd));
}

TEST(WaterFillZoneTest, DefaultWriterKeepsLegacyRuntimeCompatible) {
  auto rom = MakeDummyRom(0x200000);
  WaterFillZoneEntry z{0x25, 2, {0, 4095}};
  ASSERT_TRUE(WriteWaterFillTable(rom.get(), {z}).ok());
  const std::vector<uint8_t> expected{1, 0x25, 2, 5, 0, 2, 0, 0, 0xFF, 0x0F};
  EXPECT_TRUE(std::equal(expected.begin(), expected.end(),
                         rom->vector().begin() + kWaterFillTableStart));
  ASSERT_TRUE(WriteWaterFillTable(rom.get(), {}).ok());
  EXPECT_EQ(rom->vector()[kWaterFillTableStart], 0);
}

TEST(WaterFillZoneTest, LegacyWriterRejectsLargeZonesAndV2Downgrade) {
  auto rom = MakeDummyRom(0x200000);
  WaterFillZoneEntry z{0x25, 2, {}};
  for (int i = 0; i < 594; ++i)
    z.fill_offsets.push_back(static_cast<uint16_t>(i));
  const auto before = rom->vector();
  EXPECT_EQ(WriteWaterFillTable(rom.get(), {z}).code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(rom->vector(), before);
  ASSERT_TRUE(
      WriteWaterFillTable(rom.get(), {z}, WaterFillTableFormat::kV2).ok());
  const auto saved = rom->vector();
  EXPECT_EQ(WriteWaterFillTable(rom.get(), {}).code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(rom->vector(), saved);
  ASSERT_TRUE(
      WriteWaterFillTable(rom.get(), {z}, WaterFillTableFormat::kV2).ok());
  EXPECT_EQ(rom->vector(), saved);
}

TEST(WaterFillZoneTest, LegacyByteCountTableStillLoads) {
  auto rom = MakeDummyRom(0x200000);
  ASSERT_TRUE(rom->WriteVector(kWaterFillTableStart,
                               {1, 0x25, 2, 5, 0, 2, 0, 0, 0xFF, 0x0F})
                  .ok());
  const auto loaded = LoadWaterFillTable(rom.get());
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  ASSERT_EQ(loaded->size(), 1u);
  EXPECT_EQ(loaded->front().fill_offsets, (std::vector<uint16_t>{0, 4095}));
}

TEST(WaterFillZoneTest, V2CountCannotCrossReservedEnd) {
  auto rom = MakeDummyRom(0x200000);
  ASSERT_TRUE(
      rom->WriteVector(kWaterFillTableStart, {0x81, 0x25, 2, 0xFF, 0x1F}).ok());
  const auto loaded = LoadWaterFillTable(rom.get());
  EXPECT_FALSE(loaded.ok());
}

TEST(WaterFillZoneTest, V2RejectsCountBeyondCollisionGrid) {
  auto rom = MakeDummyRom(0x200000);
  ASSERT_TRUE(
      rom->WriteVector(kWaterFillTableStart, {0x81, 0x25, 2, 5, 0, 1, 0x10})
          .ok());
  EXPECT_FALSE(LoadWaterFillTable(rom.get()).ok());
}

TEST(WaterFillZoneTest, OversizedV2TableRefusesWithoutMutation) {
  auto rom = MakeDummyRom(0x200000);
  WaterFillZoneEntry z{0x25, 2, {}};
  for (int i = 0; i < 4096; ++i)
    z.fill_offsets.push_back(static_cast<uint16_t>(i));
  const auto before = rom->vector();
  EXPECT_EQ(
      WriteWaterFillTable(rom.get(), {z}, WaterFillTableFormat::kV2).code(),
      absl::StatusCode::kResourceExhausted);
  EXPECT_EQ(rom->vector(), before);
}

TEST(WaterFillZoneTest, SavedEmptyV2SetHasAnExplicitHeader) {
  auto rom = MakeDummyRom(0x200000);
  ASSERT_TRUE(
      WriteWaterFillTable(rom.get(), {}, WaterFillTableFormat::kV2).ok());
  EXPECT_EQ(rom->vector()[kWaterFillTableStart], 0x80);
  const auto loaded = LoadWaterFillTable(rom.get());
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  EXPECT_TRUE(loaded->empty());
}

TEST(WaterFillZoneTest, LoadReturnsEmptyWhenAbsent) {
  auto rom = MakeDummyRom(0x200000);

  auto loaded_or = LoadWaterFillTable(rom.get());
  ASSERT_TRUE(loaded_or.ok()) << loaded_or.status().message();
  EXPECT_TRUE(loaded_or.value().empty());
}

TEST(WaterFillZoneTest, WriteRejectsWhenReservedRegionMissing) {
  auto rom = MakeDummyRom(0x100000);  // Smaller than reserved region end

  WaterFillZoneEntry z;
  z.room_id = 0x25;
  z.sram_bit_mask = 0x01;
  z.fill_offsets = {0};

  auto status = WriteWaterFillTable(rom.get(), {z});
  EXPECT_FALSE(status.ok());
}

TEST(WaterFillZoneTest, WriteRejectsWhenCollisionPointerOverlapsReserved) {
  auto rom = MakeDummyRom(0x200000);

  // Make room 0 custom collision pointer point into the reserved region.
  const uint32_t snes = PcToSnes(static_cast<uint32_t>(kWaterFillTableStart));
  const int ptr_offset = kCustomCollisionRoomPointers + 0;
  ASSERT_TRUE(rom->WriteByte(ptr_offset + 0, snes & 0xFF).ok());
  ASSERT_TRUE(rom->WriteByte(ptr_offset + 1, (snes >> 8) & 0xFF).ok());
  ASSERT_TRUE(rom->WriteByte(ptr_offset + 2, (snes >> 16) & 0xFF).ok());

  WaterFillZoneEntry z;
  z.room_id = 0x25;
  z.sram_bit_mask = 0x01;
  z.fill_offsets = {0};

  auto status = WriteWaterFillTable(rom.get(), {z});
  EXPECT_FALSE(status.ok());
}

TEST(WaterFillZoneTest, LoadRejectsWhenCollisionPointerOverlapsReserved) {
  auto rom = MakeDummyRom(0x200000);

  // Make room 0 custom collision pointer point into the reserved region.
  const uint32_t snes = PcToSnes(static_cast<uint32_t>(kWaterFillTableStart));
  const int ptr_offset = kCustomCollisionRoomPointers + 0;
  ASSERT_TRUE(rom->WriteByte(ptr_offset + 0, snes & 0xFF).ok());
  ASSERT_TRUE(rom->WriteByte(ptr_offset + 1, (snes >> 8) & 0xFF).ok());
  ASSERT_TRUE(rom->WriteByte(ptr_offset + 2, (snes >> 16) & 0xFF).ok());

  auto loaded_or = LoadWaterFillTable(rom.get());
  EXPECT_FALSE(loaded_or.ok());
}

TEST(WaterFillZoneTest, WriteRejectsWhenCollisionDataOverlapsReserved) {
  auto rom = MakeDummyRom(0x200000);

  // Create a minimal single-tile collision blob that crosses into the reserved
  // WaterFill region. Start just before reserved start.
  const uint32_t start_pc = static_cast<uint32_t>(kWaterFillTableStart - 4);
  const uint32_t snes = PcToSnes(start_pc);
  const int ptr_offset = kCustomCollisionRoomPointers + 0;
  ASSERT_TRUE(rom->WriteByte(ptr_offset + 0, snes & 0xFF).ok());
  ASSERT_TRUE(rom->WriteByte(ptr_offset + 1, (snes >> 8) & 0xFF).ok());
  ASSERT_TRUE(rom->WriteByte(ptr_offset + 2, (snes >> 16) & 0xFF).ok());

  // Blob: F0 F0 (single tile mode), 00 00 08, FF FF.
  // Total 7 bytes; end marker lands after kWaterFillTableStart.
  std::vector<uint8_t> blob = {0xF0, 0xF0, 0x00, 0x00, 0x08, 0xFF, 0xFF};
  ASSERT_TRUE(rom->WriteVector(static_cast<int>(start_pc), blob).ok());

  WaterFillZoneEntry z;
  z.room_id = 0x25;
  z.sram_bit_mask = 0x01;
  z.fill_offsets = {0};

  auto status = WriteWaterFillTable(rom.get(), {z});
  EXPECT_FALSE(status.ok());
}

TEST(WaterFillZoneTest, LoadRejectsWhenCollisionDataOverlapsReserved) {
  auto rom = MakeDummyRom(0x200000);

  // Create a minimal single-tile collision blob that crosses into the reserved
  // WaterFill region. Start just before reserved start.
  const uint32_t start_pc = static_cast<uint32_t>(kWaterFillTableStart - 4);
  const uint32_t snes = PcToSnes(start_pc);
  const int ptr_offset = kCustomCollisionRoomPointers + 0;
  ASSERT_TRUE(rom->WriteByte(ptr_offset + 0, snes & 0xFF).ok());
  ASSERT_TRUE(rom->WriteByte(ptr_offset + 1, (snes >> 8) & 0xFF).ok());
  ASSERT_TRUE(rom->WriteByte(ptr_offset + 2, (snes >> 16) & 0xFF).ok());

  // Blob: F0 F0 (single tile mode), 00 00 08, FF FF.
  // Total 7 bytes; end marker lands after kWaterFillTableStart.
  std::vector<uint8_t> blob = {0xF0, 0xF0, 0x00, 0x00, 0x08, 0xFF, 0xFF};
  ASSERT_TRUE(rom->WriteVector(static_cast<int>(start_pc), blob).ok());

  auto loaded_or = LoadWaterFillTable(rom.get());
  EXPECT_FALSE(loaded_or.ok());
}

TEST(WaterFillZoneTest, LoadLegacyWaterGateZonesParsesSym) {
  auto rom = MakeDummyRom(0x200000);

  // Put two legacy tables in ROM.
  const uint32_t room25_snes = 0x258000;
  const uint32_t room27_snes = 0x258100;
  const uint32_t room25_pc = SnesToPc(room25_snes);
  const uint32_t room27_pc = SnesToPc(room27_snes);

  ASSERT_LT(room25_pc + 10u, rom->vector().size());
  ASSERT_LT(room27_pc + 10u, rom->vector().size());

  // room25: count=2, offsets 0, 1
  ASSERT_TRUE(rom->WriteByte(static_cast<int>(room25_pc + 0), 2).ok());
  ASSERT_TRUE(rom->WriteByte(static_cast<int>(room25_pc + 1), 0x00).ok());
  ASSERT_TRUE(rom->WriteByte(static_cast<int>(room25_pc + 2), 0x00).ok());
  ASSERT_TRUE(rom->WriteByte(static_cast<int>(room25_pc + 3), 0x01).ok());
  ASSERT_TRUE(rom->WriteByte(static_cast<int>(room25_pc + 4), 0x00).ok());

  // room27: count=1, offset 42
  ASSERT_TRUE(rom->WriteByte(static_cast<int>(room27_pc + 0), 1).ok());
  ASSERT_TRUE(rom->WriteByte(static_cast<int>(room27_pc + 1), 42).ok());
  ASSERT_TRUE(rom->WriteByte(static_cast<int>(room27_pc + 2), 0x00).ok());

  TempFile sym;
  ASSERT_FALSE(sym.path.empty());
  {
    std::FILE* f = std::fopen(sym.path.c_str(), "w");
    ASSERT_NE(f, nullptr);
    std::fprintf(f, "25:8000 Oracle_WaterGate_Room25_Data\n");
    std::fprintf(f, "25:8100 Oracle_WaterGate_Room27_Data\n");
    std::fclose(f);
  }

  auto zones_or = LoadLegacyWaterGateZones(rom.get(), sym.path);
  ASSERT_TRUE(zones_or.ok()) << zones_or.status().message();
  auto zones = std::move(zones_or.value());
  ASSERT_EQ(zones.size(), 2u);

  EXPECT_EQ(zones[0].room_id, 0x25);
  EXPECT_EQ(zones[0].sram_bit_mask, 0x02);
  EXPECT_EQ(zones[0].fill_offsets, (std::vector<uint16_t>{0, 1}));

  EXPECT_EQ(zones[1].room_id, 0x27);
  EXPECT_EQ(zones[1].sram_bit_mask, 0x01);
  EXPECT_EQ(zones[1].fill_offsets, (std::vector<uint16_t>{42}));
}

TEST(WaterFillZoneTest, LoadLegacyWaterGateZonesIgnoresEmptyRecords) {
  auto rom = MakeDummyRom(0x200000);
  TempFile sym;
  ASSERT_FALSE(sym.path.empty());
  {
    std::ofstream file(sym.path);
    ASSERT_TRUE(file.good());
    file << "25:8000 Oracle_WaterGate_Room25_Data\n"
            "25:8100 Oracle_WaterGate_Room27_Data\n";
  }
  const auto bytes_before = rom->vector();
  const bool dirty_before = rom->dirty();

  const auto zones = LoadLegacyWaterGateZones(rom.get(), sym.path);

  ASSERT_TRUE(zones.ok()) << zones.status();
  EXPECT_TRUE(zones->empty());
  EXPECT_EQ(rom->vector(), bytes_before);
  EXPECT_EQ(rom->dirty(), dirty_before);
}

TEST(WaterFillZoneTest,
     LoadLegacyWaterGateZonesKeepsPopulatedPeerOfEmptyRecord) {
  for (const bool empty_room25 : {false, true}) {
    SCOPED_TRACE(empty_room25);
    auto rom = MakeDummyRom(0x200000);
    const int populated_pc = SnesToPc(empty_room25 ? 0x258100 : 0x258000);
    // Offset zero is a real fill tile, distinct from a zero tile count.
    ASSERT_TRUE(rom->WriteByte(populated_pc, 1).ok());
    ASSERT_TRUE(rom->WriteByte(populated_pc + 1, 0).ok());
    ASSERT_TRUE(rom->WriteByte(populated_pc + 2, 0).ok());
    TempFile sym;
    ASSERT_FALSE(sym.path.empty());
    {
      std::ofstream file(sym.path);
      ASSERT_TRUE(file.good());
      file << "25:8000 Oracle_WaterGate_Room25_Data\n"
              "25:8100 Oracle_WaterGate_Room27_Data\n";
    }
    const auto bytes_before = rom->vector();
    const bool dirty_before = rom->dirty();

    const auto zones = LoadLegacyWaterGateZones(rom.get(), sym.path);

    ASSERT_TRUE(zones.ok()) << zones.status();
    ASSERT_EQ(zones->size(), 1u);
    EXPECT_EQ(zones->front().room_id, empty_room25 ? 0x27 : 0x25);
    EXPECT_EQ(zones->front().sram_bit_mask, empty_room25 ? 0x01 : 0x02);
    EXPECT_EQ(zones->front().fill_offsets, (std::vector<uint16_t>{0}));
    EXPECT_EQ(rom->vector(), bytes_before);
    EXPECT_EQ(rom->dirty(), dirty_before);
  }
}

TEST(WaterFillZoneTest, NormalizeMasksAssignsAutoAndFixesDuplicates) {
  std::vector<WaterFillZoneEntry> zones;
  {
    WaterFillZoneEntry z;
    z.room_id = 0x27;
    z.sram_bit_mask = 0x01;
    z.fill_offsets = {0};
    zones.push_back(std::move(z));
  }
  {
    WaterFillZoneEntry z;
    z.room_id = 0x25;
    z.sram_bit_mask = 0x01;  // Duplicate with 0x27, should be kept for 0x25.
    z.fill_offsets = {1};
    zones.push_back(std::move(z));
  }
  {
    WaterFillZoneEntry z;
    z.room_id = 0x37;
    z.sram_bit_mask = 0x00;  // Auto
    z.fill_offsets = {2};
    zones.push_back(std::move(z));
  }
  {
    WaterFillZoneEntry z;
    z.room_id = 0x38;
    z.sram_bit_mask = 0x03;  // Invalid (not single bit)
    z.fill_offsets = {3};
    zones.push_back(std::move(z));
  }

  auto st = NormalizeWaterFillZoneMasks(&zones);
  ASSERT_TRUE(st.ok()) << st.message();
  ASSERT_EQ(zones.size(), 4u);

  // Normalization sorts by room_id for stable output.
  EXPECT_EQ(zones[0].room_id, 0x25);
  EXPECT_EQ(zones[0].sram_bit_mask, 0x01);
  EXPECT_EQ(zones[1].room_id, 0x27);
  EXPECT_EQ(zones[1].sram_bit_mask, 0x02);
  EXPECT_EQ(zones[2].room_id, 0x37);
  EXPECT_EQ(zones[2].sram_bit_mask, 0x04);
  EXPECT_EQ(zones[3].room_id, 0x38);
  EXPECT_EQ(zones[3].sram_bit_mask, 0x08);
}

TEST(WaterFillZoneTest, NormalizeMasksRejectsTooManyZones) {
  std::vector<WaterFillZoneEntry> zones;
  zones.reserve(9);
  for (int i = 0; i < 9; ++i) {
    WaterFillZoneEntry z;
    z.room_id = i;
    z.sram_bit_mask = 0;
    z.fill_offsets = {0};
    zones.push_back(std::move(z));
  }

  auto st = NormalizeWaterFillZoneMasks(&zones);
  EXPECT_EQ(st.code(), absl::StatusCode::kInvalidArgument);
}

TEST(WaterFillZoneTest, NormalizeMasksRejectsRoomIdOutsideRuntimeByteRange) {
  WaterFillZoneEntry invalid;
  invalid.room_id = 0x125;
  invalid.sram_bit_mask = 0;
  invalid.fill_offsets = {2, 1, 1};
  WaterFillZoneEntry valid;
  valid.room_id = 0x25;
  valid.sram_bit_mask = 0;
  valid.fill_offsets = {0};
  std::vector<WaterFillZoneEntry> zones = {invalid, valid};

  const auto status = NormalizeWaterFillZoneMasks(&zones);
  EXPECT_EQ(status.code(), absl::StatusCode::kOutOfRange);
  ASSERT_EQ(zones.size(), 2u);
  EXPECT_EQ(zones[0].room_id, 0x125);
  EXPECT_EQ(zones[0].fill_offsets, (std::vector<uint16_t>{2, 1, 1}));
}

#if defined(YAZE_WITH_JSON)

TEST(WaterFillZoneTest, FullPoolJsonKeepsAllOffsets) {
  WaterFillZoneEntry zone{0x25, 2, {}};
  for (int i = 0; i < 594; ++i) {
    zone.fill_offsets.push_back(static_cast<uint16_t>(i));
  }
  const auto json = DumpWaterFillZonesToJsonString({zone});
  ASSERT_TRUE(json.ok()) << json.status();
  const auto zones = LoadWaterFillZonesFromJsonString(*json);
  ASSERT_TRUE(zones.ok()) << zones.status();
  ASSERT_EQ(zones->size(), 1u);
  EXPECT_EQ(zones->front().fill_offsets, zone.fill_offsets);
}

TEST(WaterFillZoneTest, JsonDumpThenLoadRoundTripNormalizes) {
  std::vector<WaterFillZoneEntry> zones;
  {
    WaterFillZoneEntry z;
    z.room_id = 0x27;
    z.sram_bit_mask = 0x00;  // Auto allowed in JSON
    z.fill_offsets = {4095, 10, 10, 0};
    zones.push_back(std::move(z));
  }
  {
    WaterFillZoneEntry z;
    z.room_id = 0x25;
    z.sram_bit_mask = 0x02;
    z.fill_offsets = {1234, 2, 3};
    zones.push_back(std::move(z));
  }

  auto json_or = DumpWaterFillZonesToJsonString(zones);
  ASSERT_TRUE(json_or.ok()) << json_or.status().message();

  auto loaded_or = LoadWaterFillZonesFromJsonString(*json_or);
  ASSERT_TRUE(loaded_or.ok()) << loaded_or.status().message();
  auto loaded = std::move(loaded_or.value());
  ASSERT_EQ(loaded.size(), 2u);

  EXPECT_EQ(loaded[0].room_id, 0x25);
  EXPECT_EQ(loaded[0].sram_bit_mask, 0x02);
  EXPECT_EQ(loaded[0].fill_offsets, (std::vector<uint16_t>{2, 3, 1234}));

  EXPECT_EQ(loaded[1].room_id, 0x27);
  EXPECT_EQ(loaded[1].sram_bit_mask, 0x00);
  EXPECT_EQ(loaded[1].fill_offsets, (std::vector<uint16_t>{0, 10, 4095}));
}

TEST(WaterFillZoneTest, JsonDumpRejectsRoomIdOutsideRuntimeByteRange) {
  WaterFillZoneEntry z;
  z.room_id = 0x125;
  z.sram_bit_mask = 0x01;
  z.fill_offsets = {0};

  const auto json_or = DumpWaterFillZonesToJsonString({z});
  EXPECT_EQ(json_or.status().code(), absl::StatusCode::kOutOfRange);
}

TEST(WaterFillZoneTest, JsonLoadRejectsDuplicateRoom) {
  constexpr const char* kJson = R"json(
{
  "version": 1,
  "zones": [
    { "room_id": "0x25", "mask": "0x01", "offsets": [ 0 ] },
    { "room_id": "0x25", "mask": "0x02", "offsets": [ 1 ] }
  ]
}
)json";

  auto zones_or = LoadWaterFillZonesFromJsonString(kJson);
  EXPECT_FALSE(zones_or.ok());
}

TEST(WaterFillZoneTest, JsonLoadRejectsOutOfRangeOffset) {
  constexpr const char* kJson = R"json(
{
  "version": 1,
  "zones": [
    { "room_id": 37, "mask": 1, "offsets": [ 4096 ] }
  ]
}
)json";

  auto zones_or = LoadWaterFillZonesFromJsonString(kJson);
  EXPECT_FALSE(zones_or.ok());
}

TEST(WaterFillZoneTest, JsonLoadRejectsRoomIdOutsideRuntimeByteRange) {
  constexpr const char* kJson = R"json(
{
  "version": 1,
  "zones": [
    { "room_id": "0x125", "mask": 1, "offsets": [ 0 ] }
  ]
}
)json";

  const auto zones_or = LoadWaterFillZonesFromJsonString(kJson);
  EXPECT_EQ(zones_or.status().code(), absl::StatusCode::kInvalidArgument);
}

#else

TEST(WaterFillZoneTest, JsonHelpersReturnUnimplementedWhenJsonDisabled) {
  WaterFillZoneEntry z;
  z.room_id = 0x25;
  z.sram_bit_mask = 0x01;
  z.fill_offsets = {0};

  auto dump_or = DumpWaterFillZonesToJsonString({z});
  EXPECT_EQ(dump_or.status().code(), absl::StatusCode::kUnimplemented);

  auto load_or = LoadWaterFillZonesFromJsonString("{}");
  EXPECT_EQ(load_or.status().code(), absl::StatusCode::kUnimplemented);
}

#endif  // YAZE_WITH_JSON

}  // namespace test
}  // namespace zelda3
}  // namespace yaze
