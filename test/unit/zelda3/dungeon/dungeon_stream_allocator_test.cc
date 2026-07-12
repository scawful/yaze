#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "rom/write_fence.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/dungeon_stream_allocator.h"

namespace yaze::zelda3::test {
namespace {

class DungeonStreamAllocatorTest : public ::testing::Test {
 protected:
  static constexpr uint32_t kRomSize = 0x100000;
  static constexpr uint32_t kPotData = 0x00E000;
  static constexpr uint32_t kObjectTable = 0x020000;
  static constexpr uint32_t kObjectData = 0x022000;
  static constexpr uint32_t kSpriteTable = 0x048000;
  static constexpr uint32_t kSpriteData = 0x049000;

  void SetUp() override {
    rom_ = std::make_unique<Rom>();
    ASSERT_TRUE(rom_->LoadFromData(std::vector<uint8_t>(kRomSize, 0)).ok());
  }

  DungeonStreamLayout PotLayout(
      uint32_t count, std::vector<DungeonStreamPcRange> data_ranges,
      std::vector<DungeonStreamPcRange> allocation_ranges) {
    DungeonStreamLayout layout;
    layout.kind = DungeonStreamKind::kPotItem;
    layout.pointer_table_pc = kRoomItemsPointers;
    layout.pointer_count = count;
    layout.pointer_encoding = DungeonPointerEncoding::kFixedBank16;
    layout.pointer_bank = 0x01;
    layout.data_ranges = std::move(data_ranges);
    layout.allocation_ranges = std::move(allocation_ranges);
    return layout;
  }

  DungeonStreamLayout ObjectLayout(uint32_t count,
                                   DungeonStreamPcRange data_range) {
    WriteLong(kRoomObjectPointer, PcToSnes(kObjectTable));
    DungeonStreamLayout layout;
    layout.kind = DungeonStreamKind::kObject;
    layout.pointer_table_pc = kObjectTable;
    layout.pointer_count = count;
    layout.pointer_encoding = DungeonPointerEncoding::kLong24;
    layout.data_ranges = {data_range};
    return layout;
  }

  DungeonStreamLayout SpriteLayout(uint32_t count,
                                   DungeonStreamPcRange data_range) {
    const uint32_t table_snes = PcToSnes(kSpriteTable);
    WriteWord(kRoomsSpritePointer, table_snes & 0xFFFF);
    DungeonStreamLayout layout;
    layout.kind = DungeonStreamKind::kSprite;
    layout.pointer_table_pc = kSpriteTable;
    layout.pointer_count = count;
    layout.pointer_encoding = DungeonPointerEncoding::kFixedBank16;
    layout.pointer_bank = 0x09;
    layout.data_ranges = {data_range};
    return layout;
  }

  void WriteBytes(uint32_t address, const std::vector<uint8_t>& bytes) {
    ASSERT_LE(address + bytes.size(), rom_->size());
    std::copy(bytes.begin(), bytes.end(), rom_->mutable_data() + address);
  }

  void WriteWord(uint32_t address, uint16_t value) {
    rom_->mutable_data()[address] = value & 0xFF;
    rom_->mutable_data()[address + 1] = (value >> 8) & 0xFF;
  }

  void WriteLong(uint32_t address, uint32_t value) {
    rom_->mutable_data()[address] = value & 0xFF;
    rom_->mutable_data()[address + 1] = (value >> 8) & 0xFF;
    rom_->mutable_data()[address + 2] = (value >> 16) & 0xFF;
  }

  void SetPointer(const DungeonStreamLayout& layout, uint32_t room_id,
                  uint32_t data_pc) {
    const uint32_t snes = PcToSnes(data_pc);
    const uint32_t width =
        layout.pointer_encoding == DungeonPointerEncoding::kLong24 ? 3 : 2;
    const uint32_t slot = layout.pointer_table_pc + room_id * width;
    if (width == 3) {
      WriteLong(slot, snes);
    } else {
      ASSERT_EQ(SnesToPc((static_cast<uint32_t>(layout.pointer_bank) << 16) |
                         (snes & 0xFFFF)),
                data_pc);
      WriteWord(slot, snes & 0xFFFF);
    }
  }

  uint32_t ReadFixedPointer(const DungeonStreamLayout& layout,
                            uint32_t room_id) const {
    const uint32_t slot = layout.pointer_table_pc + room_id * 2;
    const uint32_t word = rom_->data()[slot] | (rom_->data()[slot + 1] << 8);
    return SnesToPc((static_cast<uint32_t>(layout.pointer_bank) << 16) | word);
  }

  uint32_t ReadLongPointer(uint32_t slot) const {
    const uint32_t snes = rom_->data()[slot] | (rom_->data()[slot + 1] << 8) |
                          (rom_->data()[slot + 2] << 16);
    return SnesToPc(snes);
  }

  std::unique_ptr<Rom> rom_;
};

TEST_F(DungeonStreamAllocatorTest, InventoriesExactAliasesAndSuffixOverlap) {
  auto layout = PotLayout(3, {{kPotData, kPotData + 0x40}},
                          {{kPotData + 0x20, kPotData + 0x40}});
  // Two 3-byte entries followed by the 0xFFFF terminator. A pointer to the
  // second entry is a valid suffix stream.
  WriteBytes(kPotData, {0x01, 0x00, 0x10, 0x02, 0x00, 0x20, 0xFF, 0xFF});
  SetPointer(layout, 0, kPotData);
  SetPointer(layout, 1, kPotData);
  SetPointer(layout, 2, kPotData + 3);

  auto inventory = InventoryDungeonStreams(*rom_, layout);
  ASSERT_TRUE(inventory.ok()) << inventory.status();
  ASSERT_TRUE(inventory->ok());
  ASSERT_EQ(inventory->aliases.size(), 1);
  EXPECT_EQ(inventory->aliases[0].data_pc, kPotData);
  EXPECT_EQ(inventory->aliases[0].room_ids, (std::vector<uint32_t>{0, 1}));
  ASSERT_EQ(inventory->overlaps.size(), 1);
  EXPECT_EQ(inventory->overlaps[0].kind, DungeonStreamOverlapKind::kSuffix);
  EXPECT_EQ(inventory->overlaps[0].first_room_ids,
            (std::vector<uint32_t>{0, 1}));
  EXPECT_EQ(inventory->overlaps[0].second_room_ids, (std::vector<uint32_t>{2}));
  EXPECT_EQ(inventory->occupied_intervals,
            (std::vector<DungeonStreamPcRange>{{kPotData, kPotData + 8}}));
  EXPECT_EQ(
      inventory->allocatable_free_intervals,
      (std::vector<DungeonStreamPcRange>{{kPotData + 0x20, kPotData + 0x40}}));
}

TEST_F(DungeonStreamAllocatorTest, InventoriesInteriorOverlap) {
  auto layout = SpriteLayout(2, {kSpriteData, kSpriteData + 0x20});
  // Room 0: sort, one record, terminator => [data,data+5).
  // Room 1 starts at byte 2; byte 3 is a terminator at its record boundary,
  // yielding [data+2,data+4), an interior (not suffix) overlap.
  WriteBytes(kSpriteData, {0x00, 0x11, 0x22, 0xFF, 0xFF});
  SetPointer(layout, 0, kSpriteData);
  SetPointer(layout, 1, kSpriteData + 2);

  auto inventory = InventoryDungeonStreams(*rom_, layout);
  ASSERT_TRUE(inventory.ok()) << inventory.status();
  ASSERT_TRUE(inventory->ok());
  ASSERT_EQ(inventory->overlaps.size(), 1);
  EXPECT_EQ(inventory->overlaps[0].kind, DungeonStreamOverlapKind::kInterior);
  EXPECT_EQ(inventory->overlaps[0].intersection,
            (DungeonStreamPcRange{kSpriteData + 2, kSpriteData + 4}));
}

TEST_F(DungeonStreamAllocatorTest, ReportsMalformedObjectTerminator) {
  auto layout = ObjectLayout(1, {kObjectData, kObjectData + 9});
  // Valid header and first two list terminators, then a door marker with only
  // one byte left instead of the final 0xFFFF terminator.
  WriteBytes(kObjectData,
             {0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0, 0xFF, 0xFF});
  SetPointer(layout, 0, kObjectData);

  auto inventory = InventoryDungeonStreams(*rom_, layout);
  ASSERT_TRUE(inventory.ok()) << inventory.status();
  EXPECT_FALSE(inventory->ok());
  ASSERT_EQ(inventory->issues.size(), 1);
  EXPECT_EQ(inventory->issues[0].code,
            DungeonStreamIssueCode::kMalformedStream);
  EXPECT_EQ(inventory->issues[0].room_id, 0);
}

TEST_F(DungeonStreamAllocatorTest, RejectsPcRangesThatMapThroughWramBanks) {
  ASSERT_TRUE(rom_->LoadFromData(std::vector<uint8_t>(0x400000, 0)).ok());
  auto layout = ObjectLayout(1, {0x3F0000, 0x3F0020});
  SetPointer(layout, 0, 0x3F0000);

  const auto inventory = InventoryDungeonStreams(*rom_, layout);

  EXPECT_EQ(inventory.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_NE(std::string(inventory.status().message()).find("WRAM"),
            std::string::npos);
}

TEST_F(DungeonStreamAllocatorTest,
       AcceptsCanonicalDoorlessObjectAndRepointsDoorTerminator) {
  auto layout = ObjectLayout(1, {kObjectData, kObjectData + 0x200});
  layout.allocation_ranges = {{kObjectData + 0x100, kObjectData + 0x120}};
  const std::vector<uint8_t> canonical_empty = {
      0x00, 0x00,  // floor/layout header
      0xFF, 0xFF,  // list 0
      0xFF, 0xFF,  // list 1
      0xFF, 0xFF,  // doorless list 2 / final terminator
  };
  WriteBytes(kObjectData, canonical_empty);
  SetPointer(layout, 0, kObjectData);

  auto inventory = InventoryDungeonStreams(*rom_, layout);
  ASSERT_TRUE(inventory.ok()) << inventory.status();
  ASSERT_TRUE(inventory->ok());
  ASSERT_EQ(inventory->streams.size(), 1);
  EXPECT_EQ(inventory->streams[0].size(), canonical_empty.size());

  auto plan = PlanDungeonStreamWrites(*inventory, {{0, canonical_empty}});
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_EQ(plan->payload_writes.size(), 1);
  ASSERT_EQ(plan->auxiliary_pointer_writes.size(), 1);
  const uint32_t relocated_pc = kObjectData + 0x100;
  EXPECT_EQ(plan->payload_writes[0].address, relocated_pc);
  EXPECT_EQ(plan->auxiliary_pointer_writes[0].address, kDoorPointers);

  auto status = ApplyDungeonStreamWritePlan(rom_.get(), *plan);
  ASSERT_TRUE(status.ok()) << status;
  EXPECT_EQ(ReadLongPointer(kObjectTable), relocated_pc);
  // With no F0 FF marker, the door pointer targets the third/final 0xFFFF.
  EXPECT_EQ(ReadLongPointer(kDoorPointers), relocated_pc + 6);
}

TEST_F(DungeonStreamAllocatorTest, ObjectDoorPointerTargetsFirstDoorByte) {
  auto layout = ObjectLayout(1, {kObjectData, kObjectData + 0x200});
  layout.allocation_ranges = {{kObjectData + 0x100, kObjectData + 0x140}};
  WriteBytes(kObjectData, {0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF});
  SetPointer(layout, 0, kObjectData);
  auto inventory = InventoryDungeonStreams(*rom_, layout);
  ASSERT_TRUE(inventory.ok()) << inventory.status();
  ASSERT_TRUE(inventory->ok());

  // Header, two empty lists, F0 FF marker, one door, final terminator.
  const std::vector<uint8_t> with_door = {
      0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0, 0xFF, 0x12, 0x34, 0xFF, 0xFF,
  };
  auto plan = PlanDungeonStreamWrites(*inventory, {{0, with_door}});
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_EQ(plan->auxiliary_pointer_writes.size(), 1);
  const auto& door_write = plan->auxiliary_pointer_writes[0];
  const uint32_t door_snes = door_write.bytes[0] | (door_write.bytes[1] << 8) |
                             (door_write.bytes[2] << 16);
  EXPECT_EQ(SnesToPc(door_snes), plan->payload_writes[0].address + 8);
}

TEST_F(DungeonStreamAllocatorTest,
       PlansDeterministicFirstFitAndAppliesPayloadsAndPointers) {
  auto layout = PotLayout(
      3, {{kPotData, kPotData + 0x80}},
      {{kPotData + 0x20, kPotData + 0x28}, {kPotData + 0x40, kPotData + 0x50}});
  WriteBytes(kPotData, {0xFF, 0xFF});
  for (uint32_t room = 0; room < 3; ++room) {
    SetPointer(layout, room, kPotData);
  }
  auto inventory = InventoryDungeonStreams(*rom_, layout);
  ASSERT_TRUE(inventory.ok()) << inventory.status();
  ASSERT_TRUE(inventory->ok());

  // Input order is deliberately reversed. Planning order is room ID.
  auto plan = PlanDungeonStreamWrites(
      *inventory, {{2, {0x01, 0x00, 0x33, 0xFF, 0xFF}}, {0, {0xFF, 0xFF}}});
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_EQ(plan->payload_writes.size(), 2);
  EXPECT_EQ(plan->payload_writes[0].room_id, 0);
  EXPECT_EQ(plan->payload_writes[0].address, kPotData + 0x20);
  EXPECT_EQ(plan->payload_writes[1].room_id, 2);
  EXPECT_EQ(plan->payload_writes[1].address, kPotData + 0x22);

  auto status = ApplyDungeonStreamWritePlan(rom_.get(), *plan);
  ASSERT_TRUE(status.ok()) << status;
  EXPECT_EQ(ReadFixedPointer(layout, 0), kPotData + 0x20);
  EXPECT_EQ(ReadFixedPointer(layout, 1), kPotData);
  EXPECT_EQ(ReadFixedPointer(layout, 2), kPotData + 0x22);
  EXPECT_EQ(rom_->data()[kPotData + 0x22], 0x01);
  EXPECT_EQ(rom_->data()[kPotData + 0x26], 0xFF);
}

TEST_F(DungeonStreamAllocatorTest, NoFitLeavesRomUnchanged) {
  auto layout = PotLayout(1, {{kPotData, kPotData + 0x30}},
                          {{kPotData + 0x20, kPotData + 0x24}});
  WriteBytes(kPotData, {0xFF, 0xFF});
  SetPointer(layout, 0, kPotData);
  auto inventory = InventoryDungeonStreams(*rom_, layout);
  ASSERT_TRUE(inventory.ok()) << inventory.status();
  const auto before = rom_->vector();

  auto plan = PlanDungeonStreamWrites(*inventory,
                                      {{0, {0x01, 0x00, 0x44, 0xFF, 0xFF}}});
  EXPECT_FALSE(plan.ok());
  EXPECT_EQ(plan.status().code(), absl::StatusCode::kResourceExhausted);
  EXPECT_EQ(rom_->vector(), before);
}

TEST_F(DungeonStreamAllocatorTest, RejectsStalePlanWithoutMutation) {
  auto layout = PotLayout(1, {{kPotData, kPotData + 0x40}},
                          {{kPotData + 0x20, kPotData + 0x40}});
  WriteBytes(kPotData, {0xFF, 0xFF});
  SetPointer(layout, 0, kPotData);
  auto inventory = InventoryDungeonStreams(*rom_, layout);
  ASSERT_TRUE(inventory.ok()) << inventory.status();
  auto plan = PlanDungeonStreamWrites(*inventory, {{0, {0xFF, 0xFF}}});
  ASSERT_TRUE(plan.ok()) << plan.status();

  ASSERT_TRUE(rom_->WriteByte(0x10, 0x7A).ok());
  const auto before_apply = rom_->vector();
  auto status = ApplyDungeonStreamWritePlan(rom_.get(), *plan);
  EXPECT_EQ(status.code(), absl::StatusCode::kAborted);
  EXPECT_EQ(rom_->vector(), before_apply);
}

TEST_F(DungeonStreamAllocatorTest,
       FixedBankPlannerRejectsFreeSpaceInAnotherBank) {
  auto layout =
      PotLayout(1, {{kPotData, kPotData + 0x10}, {0x010000, 0x010020}},
                {{0x010000, 0x010020}});
  WriteBytes(kPotData, {0xFF, 0xFF});
  SetPointer(layout, 0, kPotData);
  auto inventory = InventoryDungeonStreams(*rom_, layout);
  ASSERT_TRUE(inventory.ok()) << inventory.status();
  ASSERT_TRUE(inventory->ok());

  auto plan = PlanDungeonStreamWrites(*inventory, {{0, {0xFF, 0xFF}}});
  EXPECT_FALSE(plan.ok());
  EXPECT_EQ(plan.status().code(), absl::StatusCode::kResourceExhausted);
}

TEST_F(DungeonStreamAllocatorTest, ApplyRollsBackPayloadWhenPointerWriteFails) {
  auto layout = PotLayout(1, {{kPotData, kPotData + 0x40}},
                          {{kPotData + 0x20, kPotData + 0x40}});
  WriteBytes(kPotData, {0xFF, 0xFF});
  SetPointer(layout, 0, kPotData);
  auto inventory = InventoryDungeonStreams(*rom_, layout);
  ASSERT_TRUE(inventory.ok()) << inventory.status();
  auto plan = PlanDungeonStreamWrites(*inventory,
                                      {{0, {0x01, 0x00, 0x55, 0xFF, 0xFF}}});
  ASSERT_TRUE(plan.ok()) << plan.status();
  const auto before = rom_->vector();
  const bool was_dirty = rom_->dirty();

  rom::WriteFence payload_only;
  ASSERT_TRUE(payload_only
                  .Allow(plan->payload_writes[0].address,
                         plan->payload_writes[0].address +
                             plan->payload_writes[0].bytes.size(),
                         "test payload")
                  .ok());
  rom::ScopedWriteFence fence_scope(rom_.get(), &payload_only);
  auto status = ApplyDungeonStreamWritePlan(rom_.get(), *plan);
  EXPECT_EQ(status.code(), absl::StatusCode::kPermissionDenied);
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_EQ(rom_->dirty(), was_dirty);
}

TEST_F(DungeonStreamAllocatorTest,
       RepackPlanIsDeterministicAndCanonicalizesByLowestRoom) {
  auto layout = PotLayout(4, {{kPotData, kPotData + 0x80}},
                          {{kPotData + 0x20, kPotData + 0x50}});
  const std::vector<uint8_t> empty = {0xFF, 0xFF};
  const std::vector<uint8_t> item_a = {0x01, 0x00, 0x11, 0xFF, 0xFF};
  const std::vector<uint8_t> item_b = {0x02, 0x00, 0x22, 0xFF, 0xFF};
  const std::vector<uint8_t> item_c = {0x03, 0x00, 0x33, 0xFF, 0xFF};
  WriteBytes(kPotData, empty);
  WriteBytes(kPotData + 0x08, item_a);
  WriteBytes(kPotData + 0x10, item_b);
  SetPointer(layout, 0, kPotData);
  SetPointer(layout, 1, kPotData);
  SetPointer(layout, 2, kPotData + 0x08);
  SetPointer(layout, 3, kPotData + 0x10);

  auto inventory = InventoryDungeonStreams(*rom_, layout);
  ASSERT_TRUE(inventory.ok()) << inventory.status();
  ASSERT_TRUE(inventory->ok());

  auto reversed =
      PlanDungeonStreamRepack(*inventory, {{3, item_c}, {1, item_a}});
  auto ordered =
      PlanDungeonStreamRepack(*inventory, {{1, item_a}, {3, item_c}});
  ASSERT_TRUE(reversed.ok()) << reversed.status();
  ASSERT_TRUE(ordered.ok()) << ordered.status();
  EXPECT_EQ(reversed->mode, DungeonStreamWriteMode::kRepackAll);
  EXPECT_EQ(reversed->payload_writes, ordered->payload_writes);
  EXPECT_EQ(reversed->pointer_writes, ordered->pointer_writes);
  ASSERT_EQ(reversed->payload_writes.size(), 3);
  EXPECT_EQ(reversed->payload_writes[0].room_id, 0);
  EXPECT_EQ(reversed->payload_writes[1].room_id, 1);
  EXPECT_EQ(reversed->payload_writes[2].room_id, 3);
  EXPECT_EQ(reversed->payload_writes[0].address, kPotData + 0x20);
  EXPECT_EQ(reversed->payload_writes[1].address, kPotData + 0x22);
  EXPECT_EQ(reversed->payload_writes[2].address, kPotData + 0x27);
  ASSERT_EQ(reversed->pointer_writes.size(), 4);
  EXPECT_EQ(reversed->pointer_writes[1].bytes,
            reversed->pointer_writes[2].bytes);
}

TEST_F(DungeonStreamAllocatorTest, RepackDeduplicatesSharedEmptyStream) {
  auto layout = PotLayout(3, {{kPotData, kPotData + 0x60}},
                          {{kPotData + 0x20, kPotData + 0x40}});
  const std::vector<uint8_t> empty = {0xFF, 0xFF};
  const std::vector<uint8_t> item = {0x01, 0x00, 0x44, 0xFF, 0xFF};
  WriteBytes(kPotData, empty);
  WriteBytes(kPotData + 0x08, item);
  SetPointer(layout, 0, kPotData);
  SetPointer(layout, 1, kPotData);
  SetPointer(layout, 2, kPotData + 0x08);

  auto inventory = InventoryDungeonStreams(*rom_, layout);
  ASSERT_TRUE(inventory.ok()) << inventory.status();
  auto plan = PlanDungeonStreamRepack(*inventory, {});
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_EQ(plan->payload_writes.size(), 2);
  EXPECT_EQ(plan->payload_writes[0].room_id, 0);
  EXPECT_EQ(plan->payload_writes[1].room_id, 2);
  ASSERT_TRUE(ApplyDungeonStreamWritePlan(rom_.get(), *plan).ok());
  EXPECT_EQ(ReadFixedPointer(layout, 0), ReadFixedPointer(layout, 1));
  EXPECT_NE(ReadFixedPointer(layout, 0), ReadFixedPointer(layout, 2));
}

TEST_F(DungeonStreamAllocatorTest, RepackPlansAll296RoomPointers) {
  auto layout = PotLayout(kNumberOfRooms, {{kPotData, kPotData + 0x40}},
                          {{kPotData + 0x20, kPotData + 0x30}});
  WriteBytes(kPotData, {0xFF, 0xFF});
  for (uint32_t room_id = 0; room_id < layout.pointer_count; ++room_id) {
    SetPointer(layout, room_id, kPotData);
  }

  auto inventory = InventoryDungeonStreams(*rom_, layout);
  ASSERT_TRUE(inventory.ok()) << inventory.status();
  auto plan = PlanDungeonStreamRepack(*inventory, {});
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_EQ(plan->payload_writes.size(), 1);
  EXPECT_EQ(plan->payload_writes[0].room_id, 0);
  ASSERT_EQ(plan->pointer_writes.size(), kNumberOfRooms);
  EXPECT_EQ(plan->pointer_writes.back().room_id, kNumberOfRooms - 1);
  EXPECT_EQ(plan->pointer_writes.front().bytes,
            plan->pointer_writes.back().bytes);
}

TEST_F(DungeonStreamAllocatorTest,
       RepackAcceptsExactFitAndRejectsOneByteShortRange) {
  const std::vector<uint8_t> empty = {0xFF, 0xFF};
  const std::vector<uint8_t> item = {0x01, 0x00, 0x55, 0xFF, 0xFF};
  auto exact_layout = PotLayout(2, {{kPotData, kPotData + 0x40}},
                                {{kPotData + 0x20, kPotData + 0x27}});
  WriteBytes(kPotData, empty);
  WriteBytes(kPotData + 0x08, item);
  SetPointer(exact_layout, 0, kPotData);
  SetPointer(exact_layout, 1, kPotData + 0x08);

  auto exact_inventory = InventoryDungeonStreams(*rom_, exact_layout);
  ASSERT_TRUE(exact_inventory.ok()) << exact_inventory.status();
  auto exact_plan = PlanDungeonStreamRepack(*exact_inventory, {});
  ASSERT_TRUE(exact_plan.ok()) << exact_plan.status();
  ASSERT_EQ(exact_plan->payload_writes.size(), 2);
  EXPECT_EQ(exact_plan->payload_writes[1].address +
                exact_plan->payload_writes[1].bytes.size(),
            kPotData + 0x27);

  auto short_layout = exact_layout;
  short_layout.allocation_ranges = {{kPotData + 0x20, kPotData + 0x26}};
  auto short_inventory = InventoryDungeonStreams(*rom_, short_layout);
  ASSERT_TRUE(short_inventory.ok()) << short_inventory.status();
  auto short_plan = PlanDungeonStreamRepack(*short_inventory, {});
  EXPECT_EQ(short_plan.status().code(), absl::StatusCode::kResourceExhausted);
}

TEST_F(DungeonStreamAllocatorTest,
       RepackPreservesUntouchedStreamsAndUpdatesEveryPointer) {
  auto layout = PotLayout(3, {{kPotData, kPotData + 0x80}},
                          {{kPotData + 0x20, kPotData + 0x60}});
  const std::vector<uint8_t> item_a = {0x01, 0x00, 0x10, 0xFF, 0xFF};
  const std::vector<uint8_t> item_b = {0x02, 0x00, 0x20, 0xFF, 0xFF};
  const std::vector<uint8_t> empty = {0xFF, 0xFF};
  const std::vector<uint8_t> replacement = {0x03, 0x00, 0x30, 0xFF, 0xFF};
  WriteBytes(kPotData, item_a);
  WriteBytes(kPotData + 0x08, item_b);
  WriteBytes(kPotData + 0x10, empty);
  SetPointer(layout, 0, kPotData);
  SetPointer(layout, 1, kPotData + 0x08);
  SetPointer(layout, 2, kPotData + 0x10);

  auto inventory = InventoryDungeonStreams(*rom_, layout);
  ASSERT_TRUE(inventory.ok()) << inventory.status();
  auto plan = PlanDungeonStreamRepack(*inventory, {{1, replacement}});
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_EQ(plan->pointer_writes.size(), layout.pointer_count);
  ASSERT_TRUE(ApplyDungeonStreamWritePlan(rom_.get(), *plan).ok());

  auto after = InventoryDungeonStreams(*rom_, layout);
  ASSERT_TRUE(after.ok()) << after.status();
  ASSERT_TRUE(after->ok());
  EXPECT_EQ(after->streams[0].encoded_stream, item_a);
  EXPECT_EQ(after->streams[1].encoded_stream, replacement);
  EXPECT_EQ(after->streams[2].encoded_stream, empty);
}

TEST_F(DungeonStreamAllocatorTest,
       RepackUsesMultipleRangesButRejectsAnotherPointerBank) {
  const std::vector<uint8_t> item_a = {0x01, 0x00, 0x10, 0xFF, 0xFF};
  const std::vector<uint8_t> item_b = {0x02, 0x00, 0x20, 0xFF, 0xFF};
  auto layout = PotLayout(
      2, {{kPotData, kPotData + 0x80}},
      {{kPotData + 0x20, kPotData + 0x25}, {kPotData + 0x30, kPotData + 0x35}});
  WriteBytes(kPotData, item_a);
  WriteBytes(kPotData + 0x08, item_b);
  SetPointer(layout, 0, kPotData);
  SetPointer(layout, 1, kPotData + 0x08);

  auto inventory = InventoryDungeonStreams(*rom_, layout);
  ASSERT_TRUE(inventory.ok()) << inventory.status();
  auto plan = PlanDungeonStreamRepack(*inventory, {});
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_EQ(plan->payload_writes.size(), 2);
  EXPECT_EQ(plan->payload_writes[0].address, kPotData + 0x20);
  EXPECT_EQ(plan->payload_writes[1].address, kPotData + 0x30);

  auto wrong_bank_layout =
      PotLayout(2, {{kPotData, kPotData + 0x20}, {0x010000, 0x010020}},
                {{0x010000, 0x010020}});
  SetPointer(wrong_bank_layout, 0, kPotData);
  SetPointer(wrong_bank_layout, 1, kPotData + 0x08);
  auto wrong_bank_inventory = InventoryDungeonStreams(*rom_, wrong_bank_layout);
  ASSERT_TRUE(wrong_bank_inventory.ok()) << wrong_bank_inventory.status();
  auto wrong_bank_plan = PlanDungeonStreamRepack(*wrong_bank_inventory, {});
  EXPECT_EQ(wrong_bank_plan.status().code(),
            absl::StatusCode::kInvalidArgument);
}

TEST_F(DungeonStreamAllocatorTest, RepackApplyRejectsStaleCrcWithoutMutation) {
  auto layout = PotLayout(1, {{kPotData, kPotData + 0x40}},
                          {{kPotData + 0x20, kPotData + 0x30}});
  WriteBytes(kPotData, {0xFF, 0xFF});
  SetPointer(layout, 0, kPotData);
  auto inventory = InventoryDungeonStreams(*rom_, layout);
  ASSERT_TRUE(inventory.ok()) << inventory.status();
  auto plan = PlanDungeonStreamRepack(*inventory, {});
  ASSERT_TRUE(plan.ok()) << plan.status();

  ASSERT_TRUE(rom_->WriteByte(0x10, 0x7A).ok());
  const auto before_apply = rom_->vector();
  const auto status = ApplyDungeonStreamWritePlan(rom_.get(), *plan);
  EXPECT_EQ(status.code(), absl::StatusCode::kAborted);
  EXPECT_EQ(rom_->vector(), before_apply);
}

}  // namespace
}  // namespace yaze::zelda3::test
