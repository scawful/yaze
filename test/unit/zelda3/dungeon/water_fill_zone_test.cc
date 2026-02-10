#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include <unistd.h>

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
    char tmpl[] = "/tmp/yaze_water_fill_zone_test_XXXXXX";
    const int fd = mkstemp(tmpl);
    EXPECT_GE(fd, 0);
    if (fd >= 0) {
      close(fd);
      path = tmpl;
    }
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
  EXPECT_EQ(loaded[0].fill_offsets,
            (std::vector<uint16_t>{2, 3, 1234}));

  EXPECT_EQ(loaded[1].room_id, 0x27);
  EXPECT_EQ(loaded[1].sram_bit_mask, 0x01);
  EXPECT_EQ(loaded[1].fill_offsets,
            (std::vector<uint16_t>{0, 10, 4095}));
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

TEST(WaterFillZoneTest, WriteRejectsTileCountLimit) {
  auto rom = MakeDummyRom(0x200000);

  WaterFillZoneEntry z;
  z.room_id = 0x25;
  z.sram_bit_mask = 0x01;
  z.fill_offsets.resize(256);
  for (int i = 0; i < 256; ++i) {
    z.fill_offsets[i] = static_cast<uint16_t>(i);
  }

  auto status = WriteWaterFillTable(rom.get(), {z});
  EXPECT_FALSE(status.ok());
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

#if defined(YAZE_WITH_JSON)

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
