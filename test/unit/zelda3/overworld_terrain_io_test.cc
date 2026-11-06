#include "zelda3/overworld/overworld_terrain_io.h"

#include <algorithm>
#include <vector>

#include "app/gfx/util/compression.h"
#include "gtest/gtest.h"
#include "rom/snes.h"

namespace yaze::zelda3 {
namespace {

class OverworldTerrainIoTest : public testing::Test {
 protected:
  void MakeRom(int count = 192, uint32_t end = 0x18000) {
    storage = {0x1000, 0x1300, count, {{0x10000, end, "test streams"}}};
    std::vector<uint8_t> data(0x200000, 0);
    for (uint32_t pc : {0x10000U, 0x10004U}) {
      data[pc] = 0xE4;
      data[pc + 1] = 0xFF;
      data[pc + 2] = 0;
      data[pc + 3] = 0xFF;
    }
    for (int map = 0; map < count; ++map) {
      PutPointer(&data, storage.high_table_pc + map * 3, 0x10000);
      PutPointer(&data, storage.low_table_pc + map * 3, 0x10004);
    }
    ASSERT_TRUE(rom.LoadFromData(data).ok());
    rom.set_dirty(false);
    pages.assign(count, OverworldTerrainPage{});
  }

  static void PutPointer(std::vector<uint8_t>* data, uint32_t slot,
                         uint32_t pc) {
    const uint32_t address = PcToSnes(pc) | 0x800000;
    for (int i = 0; i < 3; ++i)
      (*data)[slot + i] = static_cast<uint8_t>(address >> (i * 8));
  }

  uint32_t StreamPc(uint32_t slot) const {
    const auto& data = rom.vector();
    return SnesToPc(data[slot] | (data[slot + 1] << 8) |
                    (data[slot + 2] << 16));
  }

  // Reopen and use the existing game-format decompressor, independently of
  // the allocation scanner used to validate the production write plan.
  void CheckColdReadback() {
    Rom cold;
    ASSERT_TRUE(cold.LoadFromData(rom.vector()).ok());
    const auto& data = cold.vector();
    for (size_t map = 0; map < pages.size(); ++map) {
      for (bool high : {true, false}) {
        const uint32_t slot =
            (high ? storage.high_table_pc : storage.low_table_pc) + map * 3;
        const uint32_t pc = SnesToPc(data[slot] | (data[slot + 1] << 8) |
                                     (data[slot + 2] << 16));
        int consumed = 0;
        auto decoded = gfx::HyruleMagicDecompress(
            data.data() + pc, &consumed, 1,
            std::min<size_t>(0x8000 - (pc & 0x7FFF), data.size() - pc));
        ASSERT_EQ(decoded.size(), 256u) << map;
        for (size_t i = 0; i < decoded.size(); ++i)
          ASSERT_EQ(decoded[i], static_cast<uint8_t>(high ? pages[map][i] >> 8
                                                          : pages[map][i]))
              << "map=" << map << " tile=" << i << " high=" << high;
      }
    }
  }

  Rom rom;
  OverworldTerrainStorage storage;
  std::vector<OverworldTerrainPage> pages;
};

TEST_F(OverworldTerrainIoTest, NoOpPreservesExactPointersAndDirtyState) {
  MakeRom(160);
  const auto before = rom.vector();
  auto plan = PlanOverworldTerrainSave(rom, storage, pages);
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_TRUE(plan->writes.empty());
  EXPECT_TRUE(ApplyOverworldTerrainSave(rom, *plan).ok());
  EXPECT_EQ(rom.vector(), before);
  EXPECT_FALSE(rom.dirty());
}

TEST_F(OverworldTerrainIoTest, MixedNativeAndTailPagesColdReopenAll192) {
  MakeRom();
  const auto before = rom.vector();
  pages[0x80][7] = 0x0321;
  pages[0x9F][31] = 0x0444;
  pages[0xA0][127] = 0x05D5;
  pages[0xBF][255] = 0x06E6;
  auto plan = PlanOverworldTerrainSave(rom, storage, pages);
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_TRUE(ApplyOverworldTerrainSave(rom, *plan).ok());
  CheckColdReadback();
  for (size_t pc = 0; pc < before.size(); ++pc) {
    if (rom.vector()[pc] == before[pc])
      continue;
    EXPECT_TRUE(std::any_of(plan->writes.begin(), plan->writes.end(),
                            [pc](const auto& write) {
                              return pc >= write.pc &&
                                     pc < write.pc + write.after.size();
                            }))
        << pc;
  }
}

TEST_F(OverworldTerrainIoTest, SharedStreamsStayIntactAndNewPlanesDeduplicate) {
  MakeRom();
  const auto original_streams = std::vector<uint8_t>(
      rom.vector().begin() + 0x10000, rom.vector().begin() + 0x10008);
  ASSERT_TRUE(rom.WriteByte(0x10008, 0x42).ok());
  pages[0xBF][0] = 0x0101;
  auto plan = PlanOverworldTerrainSave(rom, storage, pages);
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_TRUE(ApplyOverworldTerrainSave(rom, *plan).ok());
  EXPECT_EQ(StreamPc(storage.high_table_pc), 0x10000u);
  EXPECT_EQ(StreamPc(storage.low_table_pc), 0x10004u);
  EXPECT_EQ(StreamPc(storage.high_table_pc + 0xBF * 3),
            StreamPc(storage.low_table_pc + 0xBF * 3));
  EXPECT_EQ(std::vector<uint8_t>(rom.vector().begin() + 0x10000,
                                 rom.vector().begin() + 0x10008),
            original_streams);
  EXPECT_EQ(rom.vector()[0x10008], 0x42);
  CheckColdReadback();
}

TEST_F(OverworldTerrainIoTest, ReclaimsStreamsOnlyWhenEveryUserMoves) {
  MakeRom(160, 0x10008);
  for (auto& page : pages)
    page.fill(0x0101);
  auto plan = PlanOverworldTerrainSave(rom, storage, pages);
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_TRUE(ApplyOverworldTerrainSave(rom, *plan).ok());
  CheckColdReadback();
}

TEST_F(OverworldTerrainIoTest, FastRomLastBanksPreserveAndPlaceChangedPlanes) {
  MakeRom();
  auto bytes = rom.vector();
  bytes.resize(0x400000);
  storage.stream_regions = {{0x3F0000, 0x3F0004, "FE stream"},
                            {0x3F8000, 0x400000, "FF streams"}};
  for (uint32_t pc : {0x3F0000U, 0x3F8000U})
    std::copy_n(bytes.begin() + 0x10000, 4, bytes.begin() + pc);
  for (int map = 0; map < 192; ++map) {
    PutPointer(&bytes, storage.high_table_pc + map * 3, 0x3F0000);
    PutPointer(&bytes, storage.low_table_pc + map * 3, 0x3F8000);
  }
  ASSERT_EQ(bytes[storage.high_table_pc + 2], 0xFE);
  ASSERT_EQ(bytes[storage.low_table_pc + 2], 0xFF);
  ASSERT_TRUE(rom.LoadFromData(bytes).ok());
  auto no_op = PlanOverworldTerrainSave(rom, storage, pages);
  ASSERT_TRUE(no_op.ok()) << no_op.status();
  EXPECT_TRUE(no_op->writes.empty());
  pages[0xBF][0] = 0x0101;
  auto plan = PlanOverworldTerrainSave(rom, storage, pages);
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_TRUE(ApplyOverworldTerrainSave(rom, *plan).ok());
  EXPECT_GE(StreamPc(storage.low_table_pc + 0xBF * 3), 0x3F8004u);
  CheckColdReadback();
  for (uint32_t raw_bank : {0x7EU, 0x7FU}) {
    ASSERT_TRUE(
        rom.WriteLong(storage.low_table_pc + 0xBF * 3, raw_bank << 16 | 0x8000)
            .ok());
    EXPECT_FALSE(PlanOverworldTerrainSave(rom, storage, pages).ok());
  }
}

TEST_F(OverworldTerrainIoTest, PointerInsideAnotherStreamKeepsBothLiveSpans) {
  MakeRom();
  auto bytes = rom.vector();
  // The outer literal256 contains a valid inner fill256 in its first4 bytes.
  bytes[0x10000] = 0xE0;
  bytes[0x10001] = 0xFF;
  std::fill_n(bytes.begin() + 0x10002, 256, 0);
  bytes[0x10002] = 0xE4;
  bytes[0x10003] = 0xFF;
  bytes[0x10004] = 0;
  bytes[0x10005] = 0xFF;
  bytes[0x10102] = 0xFF;
  for (int map = 0; map < 192; ++map) {
    PutPointer(&bytes, storage.high_table_pc + map * 3, 0x10002);
    PutPointer(&bytes, storage.low_table_pc + map * 3, 0x10000);
    pages[map][0] = 0xE4;
    pages[map][1] = 0xFF;
    pages[map][3] = 0xFF;
  }
  ASSERT_TRUE(rom.LoadFromData(bytes).ok());
  pages[0xBF][0] = 0xE5;
  auto plan = PlanOverworldTerrainSave(rom, storage, pages);
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_TRUE(ApplyOverworldTerrainSave(rom, *plan).ok());
  EXPECT_TRUE(std::equal(bytes.begin() + 0x10000, bytes.begin() + 0x10103,
                         rom.vector().begin() + 0x10000));
  EXPECT_EQ(StreamPc(storage.high_table_pc), 0x10002u);
  EXPECT_EQ(StreamPc(storage.low_table_pc), 0x10000u);
  CheckColdReadback();
}

TEST_F(OverworldTerrainIoTest, FullPoolFailsWithoutChangingSource) {
  MakeRom(192, 0x10008);
  const auto before = rom.vector();
  pages[0xBF][0] = 1;
  auto plan = PlanOverworldTerrainSave(rom, storage, pages);
  EXPECT_EQ(plan.status().code(), absl::StatusCode::kResourceExhausted);
  EXPECT_EQ(rom.vector(), before);
  EXPECT_FALSE(rom.dirty());
}

TEST_F(OverworldTerrainIoTest, AllocationDoesNotCrossLoRomBank) {
  MakeRom(192, 0x10008);
  storage.stream_regions.push_back({0x17FFE, 0x18100, "test bank edge"});
  pages[0xBF][0] = 1;
  auto plan = PlanOverworldTerrainSave(rom, storage, pages);
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_TRUE(ApplyOverworldTerrainSave(rom, *plan).ok());
  EXPECT_GE(StreamPc(storage.low_table_pc + 0xBF * 3), 0x18000u);
  CheckColdReadback();
}

TEST_F(OverworldTerrainIoTest, UnrelatedReadSetDriftRefusesStalePlan) {
  MakeRom();
  pages[0xBF][0] = 1;
  auto plan = PlanOverworldTerrainSave(rom, storage, pages);
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_TRUE(rom.WriteByte(0x2345, 0x42).ok());
  const auto changed = rom.vector();
  EXPECT_FALSE(ApplyOverworldTerrainSave(rom, *plan).ok());
  EXPECT_EQ(rom.vector(), changed);
  EXPECT_TRUE(rom.dirty());
}

TEST_F(OverworldTerrainIoTest,
       OuterFenceFailureRestoresPointerWritesAndDirtyState) {
  MakeRom();
  pages[0xBF][0] = 1;
  const auto before = rom.vector();
  auto plan = PlanOverworldTerrainSave(rom, storage, pages);
  ASSERT_TRUE(plan.ok()) << plan.status();
  rom::WriteFence outer;
  ASSERT_TRUE(outer
                  .Allow(storage.high_table_pc, storage.high_table_pc + 192 * 3,
                         "high only")
                  .ok());
  ASSERT_TRUE(outer
                  .Allow(storage.low_table_pc, storage.low_table_pc + 192 * 3,
                         "low only")
                  .ok());
  rom::ScopedWriteFence scope(&rom, &outer);
  EXPECT_EQ(ApplyOverworldTerrainSave(rom, *plan).code(),
            absl::StatusCode::kPermissionDenied);
  EXPECT_EQ(rom.vector(), before);
  EXPECT_FALSE(rom.dirty());
}

TEST_F(OverworldTerrainIoTest, MalformedLastPointerFailsBeforeAnyWrite) {
  MakeRom();
  ASSERT_TRUE(rom.WriteLong(storage.low_table_pc + 0xBF * 3, 0).ok());
  const auto before = rom.vector();
  EXPECT_FALSE(PlanOverworldTerrainSave(rom, storage, pages).ok());
  EXPECT_EQ(rom.vector(), before);
}

TEST_F(OverworldTerrainIoTest, CapacityMismatchAndStorageOverlapAreRejected) {
  MakeRom();
  storage.map_count = 160;
  EXPECT_FALSE(PlanOverworldTerrainSave(rom, storage, pages).ok());
  storage.map_count = 192;
  storage.low_table_pc = storage.high_table_pc + 3;
  EXPECT_FALSE(PlanOverworldTerrainSave(rom, storage, pages).ok());
}

}  // namespace
}  // namespace yaze::zelda3
