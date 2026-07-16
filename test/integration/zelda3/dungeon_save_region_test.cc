// Integration tests for dungeon save region preservation (ZScream parity).
// Verifies that SaveAllCollision, SaveAllPits, SaveAllBlocks, and SaveAllTorches
// preserve ROM regions when no edits are made, so validate-yaze --feature=*
// can pass when comparing a yaze-saved ROM to a golden ROM.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <iomanip>
#include <set>
#include <vector>

#include "absl/types/span.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "test_utils.h"
#include "zelda3/dungeon/dungeon_block_codec.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/room.h"

namespace yaze {
namespace zelda3 {
namespace test {

using namespace yaze::zelda3;

class DungeonSaveRegionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    rom_ = std::make_unique<Rom>();
    YAZE_SKIP_IF_ROM_MISSING(yaze::test::RomRole::kVanilla,
                             "DungeonSaveRegionTest");
    const std::string rom_path =
        yaze::test::TestRomManager::GetRomPath(yaze::test::RomRole::kVanilla);
    auto status = rom_->LoadFromFile(rom_path);
    if (!status.ok()) {
      GTEST_SKIP() << "ROM file not available: " << status.message();
    }
    original_rom_data_ = rom_->vector();
  }

  void TearDown() override {
    if (rom_ && !original_rom_data_.empty()) {
      for (size_t i = 0; i < original_rom_data_.size(); ++i) {
        rom_->WriteByte(static_cast<int>(i), original_rom_data_[i]);
      }
    }
  }

  std::unique_ptr<Rom> rom_;
  std::vector<uint8_t> original_rom_data_;
};

// --- Collision (ZScream CompareCollisionRegions: 0x128090..0x130000) ---

TEST_F(DungeonSaveRegionTest, SaveAllCollisionPreservesRegion) {
  const auto& data = rom_->vector();
  const int ptrs_size = kNumberOfRooms * 3;
  const int data_size = kCustomCollisionDataEnd - kCustomCollisionDataPosition;
  if (kCustomCollisionRoomPointers + ptrs_size >
          static_cast<int>(data.size()) ||
      kCustomCollisionDataPosition + data_size >
          static_cast<int>(data.size())) {
    GTEST_SKIP() << "ROM too small for collision region";
  }

  std::vector<uint8_t> ptrs_before(
      data.begin() + kCustomCollisionRoomPointers,
      data.begin() + kCustomCollisionRoomPointers + ptrs_size);
  std::vector<uint8_t> data_before(
      data.begin() + kCustomCollisionDataPosition,
      data.begin() + kCustomCollisionDataPosition + data_size);

  // Avoid large stack allocations: Room is a heavy type (e.g. 64KB gfx buffer).
  // A full kNumberOfRooms array can overflow the default ~8MB main-thread stack
  // when running tests directly (outside of ctest).
  std::vector<Room> rooms(static_cast<size_t>(kNumberOfRooms));
  auto status = SaveAllCollision(rom_.get(), absl::MakeSpan(rooms));
  ASSERT_TRUE(status.ok()) << status.message();

  const auto& after = rom_->vector();
  std::vector<uint8_t> ptrs_after(
      after.begin() + kCustomCollisionRoomPointers,
      after.begin() + kCustomCollisionRoomPointers + ptrs_size);
  std::vector<uint8_t> data_after(
      after.begin() + kCustomCollisionDataPosition,
      after.begin() + kCustomCollisionDataPosition + data_size);

  EXPECT_THAT(ptrs_after, ::testing::ContainerEq(ptrs_before));
  EXPECT_THAT(data_after, ::testing::ContainerEq(data_before));
}

// --- RoomsWithPitDamage (immediate @ 0x394A6, long operand @ 0x394AB, data at
//     SnesToPc(operand)) ---
//
// `kPitCount` (legacy name) is the LDX.w immediate that bounds the
// runtime CMP loop in `DetermineConsequencesOfFalling`
// (bank_07.asm:4193). The loop starts with X = #$0070, compares
// `RoomsWithPitDamage,X`, then `DEX : DEX : BPL` — so it checks
// offsets 0x70, 0x6E, ..., 0x00 inclusive. That's
// `(kPitCount / 2) + 1` entries — 57 in vanilla, NOT 56. The byte
// length of the table is `kPitCount + 2` (max_offset + word size),
// i.e., 114 bytes covering PC 0x0190C..0x0197D.

TEST_F(DungeonSaveRegionTest, SaveAllPitsPreservesRegion) {
  const auto& data = rom_->vector();
  if (kPitCount >= static_cast<int>(data.size()) ||
      kPitPointer + 2 >= static_cast<int>(data.size())) {
    GTEST_SKIP() << "ROM too small for pits region";
  }

  int max_offset_before = data[kPitCount];
  int data_len = max_offset_before + 2;  // covers offsets 0..max_offset
  // `data` aliases the same internal vector that `Rom::WriteByte` mutates,
  // so reading `data[kPitPointer + i]` after the save reflects the
  // post-save state. Capture the operand bytes BEFORE save so the
  // post-save assertions actually compare to the pre-save snapshot.
  const std::array<uint8_t, 3> pit_ptr_before = {
      data[kPitPointer], data[kPitPointer + 1], data[kPitPointer + 2]};

  int pit_ptr_snes = (data[kPitPointer + 2] << 16) |
                     (data[kPitPointer + 1] << 8) | data[kPitPointer];
  int pit_data_pc =
      static_cast<int>(SnesToPc(static_cast<uint32_t>(pit_ptr_snes)));
  std::vector<uint8_t> pit_data_before;
  if (data_len > 0 && pit_data_pc >= 0 &&
      pit_data_pc + data_len <= static_cast<int>(data.size())) {
    pit_data_before.assign(data.begin() + pit_data_pc,
                           data.begin() + pit_data_pc + data_len);
  }

  auto status = SaveAllPits(rom_.get());
  ASSERT_TRUE(status.ok()) << status.message();

  const auto& after = rom_->vector();
  EXPECT_EQ(after[kPitCount], max_offset_before);
  EXPECT_EQ(after[kPitPointer], pit_ptr_before[0]);
  EXPECT_EQ(after[kPitPointer + 1], pit_ptr_before[1]);
  EXPECT_EQ(after[kPitPointer + 2], pit_ptr_before[2]);
  if (!pit_data_before.empty()) {
    ASSERT_GE(static_cast<int>(after.size()), pit_data_pc + data_len);
    for (int i = 0; i < data_len; ++i) {
      EXPECT_EQ(after[pit_data_pc + i], pit_data_before[i])
          << "Pit data mismatch at offset " << i;
    }
  }
}

// --- RoomsWithPitDamage table format pins (vanilla-ROM-dependent) ---
//
// The audit (zelda3-hacking-expert, 2026-04-25; reviewer follow-up
// caught the off-by-one) found that the "pit table" at `kPitCount`
// (0x394A6) / `kPitPointer` (0x394AB) is NOT a pit-position list. It
// is the `RoomsWithPitDamage` set consumed by
// `DetermineConsequencesOfFalling` at bank_07.asm:4193 — a membership
// check that decides whether falling in a given room deals damage
// (`UnderworldPitDoDamage`) versus transitioning Link to a different
// floor via the per-room `pits_target_layer`. The table has **57**
// entries in vanilla US v1.0, hardcoded since 1991. yaze now has a
// fixed-capacity table encoder, but still no room-editor UI toggle or
// `Room::pit_does_damage_` field.
//
// Critically, `kPitCount` and `kPitPointer` are not data fields. They
// are immediate operands inside instructions:
//   PC 0x394A5 = 0xA2  (LDX.w opcode)
//   PC 0x394A6 = 0x70  (low byte of #$0070 immediate)  ← kPitCount
//   PC 0x394A7 = 0x00  (high byte; high half of LDX immediate)
//   PC 0x394AA = 0xDF  (CMP.l ,X opcode)
//   PC 0x394AB..D     = 0x0C 0x99 0x00 (3-byte SNES long $00:990C)  ← kPitPointer
//   SnesToPc($00990C) = 0x0190C  → `RoomsWithPitDamage` table base
//
// Off-by-one: the `#$0070` immediate is the **maximum X offset** the
// runtime loop will test, NOT a byte count. The CMP loop runs:
//   LDX.w #$0070 / CMP.l RoomsWithPitDamage,X / DEX / DEX / BPL
// so X visits 0x70, 0x6E, ..., 0x00 inclusive. That's
// `(kPitCount / 2) + 1` = 57 entries. Total table byte length is
// `kPitCount + 2` = 114 bytes spanning PC 0x0190C..0x0197D. The final
// entry at offset 0x70 (PC 0x0197C) is `0x0123`.
//
// The legacy no-argument `SaveAllPits` path is intentional
// region-preservation. Edited membership should go through `PitDamageTable`;
// these tests pin the format invariants discovered by the audit so future
// changes (new hacks, an asar patch that grows the table) deliberately flip
// these assertions instead of silently corrupting the surrounding instructions.

namespace {

// Vanilla US v1.0 RoomsWithPitDamage table — extracted directly from
// `roms/alttp_vanilla.sfc` at PC 0x0190C (SnesToPc($00:990C)).
// Authoring order, NOT sorted. 57 entries (offsets 0x00..0x70 inclusive).
// The sequence is locked in for vanilla; hacks that grow or reorder
// the table must deliberately update this literal alongside the ROM
// edit.
constexpr std::array<uint16_t, 57> kVanillaRoomsWithPitDamage = {
    0x0072, 0x0082, 0x0040, 0x00C0, 0x0112, 0x0056, 0x0057, 0x0058, 0x0067,
    0x0068, 0x0049, 0x0098, 0x00D1, 0x00C3, 0x00A3, 0x00A2, 0x0092, 0x00A0,
    0x004E, 0x007F, 0x00AF, 0x00F0, 0x00F1, 0x00E6, 0x00E7, 0x00C6, 0x00C7,
    0x00D6, 0x00B4, 0x00B5, 0x00C5, 0x0024, 0x00D5, 0x00C9, 0x002A, 0x001A,
    0x004B, 0x00BC, 0x0044, 0x00FB, 0x007B, 0x007C, 0x008B, 0x008D, 0x009B,
    0x009C, 0x009D, 0x00A5, 0x0095, 0x001C, 0x005C, 0x007D, 0x004C, 0x0096,
    0x0120, 0x003C, 0x0123,
};

// Number of entries consumed by the runtime CMP loop, derived from
// the LDX immediate: max_offset / 2 + 1.
inline int PitTableEntryCount(uint8_t count_byte) {
  return static_cast<int>(count_byte) / 2 + 1;
}

}  // namespace

TEST_F(DungeonSaveRegionTest, RoomsWithPitDamageVanillaShape) {
  const auto& data = rom_->vector();
  if (kPitPointer + 2 >= static_cast<int>(data.size())) {
    GTEST_SKIP() << "ROM too small for pit table";
  }
  // LDX.w + immediate.
  EXPECT_EQ(data[kPitCount - 1], 0xA2) << "Expected LDX.w opcode at PC 0x394A5";
  EXPECT_EQ(data[kPitCount], 0x70)
      << "Expected #$0070 low byte (max X offset = 56*2; entry count = 57)";
  EXPECT_EQ(data[kPitCount + 1], 0x00) << "Expected #$0070 high byte";
  // CMP.l ,X + 3-byte operand.
  EXPECT_EQ(data[kPitPointer - 1], 0xDF)
      << "Expected CMP.l ,X opcode at PC 0x394AA";
  const int snes = (data[kPitPointer + 2] << 16) |
                   (data[kPitPointer + 1] << 8) | data[kPitPointer];
  EXPECT_EQ(snes, 0x00990C)
      << "Expected SNES $00:990C operand pointing at RoomsWithPitDamage";
  const int pc = static_cast<int>(SnesToPc(static_cast<uint32_t>(snes)));
  EXPECT_EQ(pc, 0x0190C) << "RoomsWithPitDamage base in PC space";
}

TEST_F(DungeonSaveRegionTest, RoomsWithPitDamageEntriesAreValidRoomIds) {
  const auto& data = rom_->vector();
  if (kPitPointer + 2 >= static_cast<int>(data.size())) {
    GTEST_SKIP() << "ROM too small for pit table";
  }
  const int snes = (data[kPitPointer + 2] << 16) |
                   (data[kPitPointer + 1] << 8) | data[kPitPointer];
  const int pc = static_cast<int>(SnesToPc(static_cast<uint32_t>(snes)));
  const int entry_count = PitTableEntryCount(data[kPitCount]);
  ASSERT_GE(static_cast<int>(data.size()), pc + entry_count * 2);

  for (int i = 0; i < entry_count; ++i) {
    const uint16_t room_id =
        static_cast<uint16_t>(data[pc + i * 2] | (data[pc + i * 2 + 1] << 8));
    EXPECT_LT(room_id, kNumberOfRooms)
        << "RoomsWithPitDamage entry " << i << " (room_id 0x" << std::hex
        << room_id << ") exceeds dungeon room count " << std::dec
        << kNumberOfRooms
        << "; runtime CMP would never match a live room and the entry "
           "is dead.";
  }
}

TEST_F(DungeonSaveRegionTest, RoomsWithPitDamageHasNoDuplicates) {
  // The runtime CMP-loop matches on the first hit, so a duplicate
  // would be inert but bloat the membership check. Vanilla has no
  // duplicates; pin the invariant so future table edits don't
  // regress it.
  const auto& data = rom_->vector();
  if (kPitPointer + 2 >= static_cast<int>(data.size())) {
    GTEST_SKIP() << "ROM too small for pit table";
  }
  const int snes = (data[kPitPointer + 2] << 16) |
                   (data[kPitPointer + 1] << 8) | data[kPitPointer];
  const int pc = static_cast<int>(SnesToPc(static_cast<uint32_t>(snes)));
  const int entry_count = PitTableEntryCount(data[kPitCount]);
  ASSERT_GE(static_cast<int>(data.size()), pc + entry_count * 2);

  std::set<uint16_t> seen;
  for (int i = 0; i < entry_count; ++i) {
    const uint16_t room_id =
        static_cast<uint16_t>(data[pc + i * 2] | (data[pc + i * 2 + 1] << 8));
    EXPECT_TRUE(seen.insert(room_id).second)
        << "RoomsWithPitDamage entry " << i << " (room_id 0x" << std::hex
        << room_id << ") duplicates an earlier entry.";
  }
  EXPECT_EQ(static_cast<int>(seen.size()), entry_count)
      << "Total distinct entries should equal max_offset / 2 + 1.";
}

TEST_F(DungeonSaveRegionTest, RoomsWithPitDamageMatchesUsdasm) {
  // Locks the exact 57-entry sequence against the bytes at PC 0x0190C.
  // If a hack grows this table (e.g., adds bottomless rooms in
  // expansion content), this test must be deliberately updated
  // alongside the table edit. Until then, the sequence is immutable.
  const auto& data = rom_->vector();
  if (kPitPointer + 2 >= static_cast<int>(data.size())) {
    GTEST_SKIP() << "ROM too small for pit table";
  }
  const int expected_max_offset =
      (static_cast<int>(kVanillaRoomsWithPitDamage.size()) - 1) * 2;
  ASSERT_EQ(data[kPitCount], static_cast<uint8_t>(expected_max_offset))
      << "Vanilla LDX.w immediate must equal (entries - 1) * 2 = 0x70";
  const int snes = (data[kPitPointer + 2] << 16) |
                   (data[kPitPointer + 1] << 8) | data[kPitPointer];
  const int pc = static_cast<int>(SnesToPc(static_cast<uint32_t>(snes)));
  const int entry_count = static_cast<int>(kVanillaRoomsWithPitDamage.size());
  ASSERT_GE(static_cast<int>(data.size()), pc + entry_count * 2);

  for (int i = 0; i < entry_count; ++i) {
    const uint16_t room_id =
        static_cast<uint16_t>(data[pc + i * 2] | (data[pc + i * 2 + 1] << 8));
    EXPECT_EQ(room_id, kVanillaRoomsWithPitDamage[i])
        << "RoomsWithPitDamage entry " << i << " (offset 0x" << std::hex
        << (i * 2) << ") diverges from the dump at PC 0x0190C.";
  }
}

// --- Blocks (length @ kBlocksLength, four pointers, four 0x80 regions) ---

TEST_F(DungeonSaveRegionTest, BlocksLoaderPointerOperandsMatchUsdasmShape) {
  const auto& data = rom_->vector();
  const int ptrs[4] = {kBlocksPointer1, kBlocksPointer2, kBlocksPointer3,
                       kBlocksPointer4};
  constexpr int kRegionSize = 0x80;
  constexpr int kVanillaPushableBlockSnes = 0x04F1DE;

  for (int r = 0; r < 4; ++r) {
    const int slot = ptrs[r];
    ASSERT_GT(slot, 0);
    ASSERT_LT(slot + 3, static_cast<int>(data.size()));
    EXPECT_EQ(data[slot - 1], 0xBF)
        << "bank_02.asm expects LDA.l before pushable-block operand " << r;
    EXPECT_EQ(data[slot + 3], 0x9D)
        << "bank_02.asm expects STA.w after pushable-block operand " << r;

    const int snes =
        (data[slot + 2] << 16) | (data[slot + 1] << 8) | data[slot];
    const int expected_snes = kVanillaPushableBlockSnes + (r * kRegionSize);
    EXPECT_EQ(snes, expected_snes)
        << "US USDASM pins SpecialUnderworldObjects_pushable_block at "
           "$04:F1DE and the loader copies four 0x80-byte pages.";
  }
}

TEST_F(DungeonSaveRegionTest, SaveAllBlocksPreservesRegion) {
  const auto& data = rom_->vector();
  if (kBlocksLength + 1 >= static_cast<int>(data.size())) {
    GTEST_SKIP() << "ROM too small for blocks region";
  }

  int blocks_count = (data[kBlocksLength + 1] << 8) | data[kBlocksLength];
  if (blocks_count <= 0) {
    auto status = SaveAllBlocks(rom_.get());
    ASSERT_TRUE(status.ok()) << status.message();
    return;
  }

  const int kRegionSize = 0x80;
  int ptrs[4] = {kBlocksPointer1, kBlocksPointer2, kBlocksPointer3,
                 kBlocksPointer4};
  std::vector<std::vector<uint8_t>> regions_before;
  for (int r = 0; r < 4; ++r) {
    if (ptrs[r] + 2 >= static_cast<int>(data.size()))
      break;
    int snes =
        (data[ptrs[r] + 2] << 16) | (data[ptrs[r] + 1] << 8) | data[ptrs[r]];
    int pc = static_cast<int>(SnesToPc(static_cast<uint32_t>(snes)));
    int off = r * kRegionSize;
    int len = std::min(kRegionSize, blocks_count - off);
    if (len <= 0)
      break;
    if (pc < 0 || pc + len > static_cast<int>(data.size()))
      break;
    regions_before.emplace_back(data.begin() + pc, data.begin() + pc + len);
  }

  auto status = SaveAllBlocks(rom_.get());
  ASSERT_TRUE(status.ok()) << status.message();

  const auto& after = rom_->vector();
  EXPECT_EQ((after[kBlocksLength + 1] << 8) | after[kBlocksLength],
            blocks_count);
  for (size_t r = 0; r < regions_before.size(); ++r) {
    if (ptrs[r] + 2 >= static_cast<int>(after.size()))
      break;
    int snes =
        (after[ptrs[r] + 2] << 16) | (after[ptrs[r] + 1] << 8) | after[ptrs[r]];
    int pc = static_cast<int>(SnesToPc(static_cast<uint32_t>(snes)));
    int len = static_cast<int>(regions_before[r].size());
    ASSERT_GE(static_cast<int>(after.size()), pc + len);
    for (int i = 0; i < len; ++i) {
      EXPECT_EQ(after[pc + i], regions_before[r][i])
          << "Blocks region " << r << " offset " << i;
    }
  }
}

TEST_F(DungeonSaveRegionTest,
       VanillaPushableBlockWordsDecodeAndReencodeByteExactly) {
  const auto& data = rom_->vector();
  ASSERT_LT(kBlocksLength + 1, static_cast<int>(data.size()));
  const int blocks_count = (data[kBlocksLength + 1] << 8) | data[kBlocksLength];
  ASSERT_GT(blocks_count, 0);
  ASSERT_EQ(blocks_count % 4, 0);

  constexpr int kRegionSize = 0x80;
  const int pointers[4] = {kBlocksPointer1, kBlocksPointer2, kBlocksPointer3,
                           kBlocksPointer4};
  std::vector<uint8_t> block_bytes;
  block_bytes.reserve(blocks_count);
  for (int region = 0; region < 4; ++region) {
    const int operand = pointers[region];
    ASSERT_LT(operand + 2, static_cast<int>(data.size()));
    const int snes =
        (data[operand + 2] << 16) | (data[operand + 1] << 8) | data[operand];
    const int pc = static_cast<int>(SnesToPc(static_cast<uint32_t>(snes)));
    const int offset = region * kRegionSize;
    const int length = std::min(kRegionSize, blocks_count - offset);
    if (length <= 0) {
      break;
    }
    ASSERT_GE(pc, 0);
    ASSERT_LE(pc + length, static_cast<int>(data.size()));
    block_bytes.insert(block_bytes.end(), data.begin() + pc,
                       data.begin() + pc + length);
  }
  ASSERT_EQ(block_bytes.size(), static_cast<size_t>(blocks_count));

  struct ExpectedSample {
    uint16_t room_id;
    uint16_t word;
    uint8_t px;
    uint8_t py;
    uint8_t draw_layer;
    uint8_t behavior_layer;
    bool found = false;
  };
  std::array<ExpectedSample, 4> samples = {{
      {0x00A8, 0x36E0, 48, 45, 1, 0},
      {0x0066, 0x383C, 30, 48, 1, 0},
      {0x002C, 0x2814, 10, 16, 1, 0},
      {0x00CA, 0x56B2, 25, 45, 0, 1},
  }};

  for (size_t offset = 0; offset < block_bytes.size(); offset += 4) {
    const PushableBlockBytes bytes{
        block_bytes[offset + 0], block_bytes[offset + 1],
        block_bytes[offset + 2], block_bytes[offset + 3]};
    const auto entry = DecodePushableBlockEntry(bytes);
    const auto encoded = EncodePushableBlockEntry(entry);
    EXPECT_EQ(encoded.b1, bytes.b1) << "entry offset " << offset;
    EXPECT_EQ(encoded.b2, bytes.b2) << "entry offset " << offset;
    EXPECT_EQ(encoded.b3, bytes.b3) << "entry offset " << offset;
    EXPECT_EQ(encoded.b4, bytes.b4) << "entry offset " << offset;

    const uint16_t word = static_cast<uint16_t>(bytes.b3 | (bytes.b4 << 8));
    for (auto& sample : samples) {
      if (entry.room_id != sample.room_id || word != sample.word) {
        continue;
      }
      sample.found = true;
      EXPECT_EQ(entry.px, sample.px);
      EXPECT_EQ(entry.py, sample.py);
      EXPECT_EQ(entry.draw_layer, sample.draw_layer);
      EXPECT_EQ(entry.behavior_layer, sample.behavior_layer);
    }
  }

  for (const auto& sample : samples) {
    EXPECT_TRUE(sample.found) << "missing vanilla room 0x" << std::hex
                              << sample.room_id << " word 0x" << sample.word;
  }
}

// --- Blocks: room-aware encoder vanilla round-trip ---
//
// Materialize every room with `LoadBlocks` so the new encoder's
// "owned by editor" path executes for all 99 vanilla entries, then
// run the room-aware `SaveAllBlocks` overload with no in-memory
// edits. The output must be byte-identical to the vanilla bytes.
//
// This test pins the load-order ordering invariant on real data: the
// vanilla pushable-block table groups entries by authoring order
// (interleaved across rooms), not by room_id. If
// `RoomObject::block_load_order_` were dropped or mis-tracked, the
// re-encoded buffer would reshuffle bytes (e.g., if blocks ended up
// sorted by room_id) and one or more of the 396 byte assertions
// would fail.

TEST_F(DungeonSaveRegionTest,
       SaveAllBlocksRoomAware_VanillaNoOpPreservesAllBytes) {
  const auto& data = rom_->vector();
  if (kBlocksLength + 1 >= static_cast<int>(data.size())) {
    GTEST_SKIP() << "ROM too small for blocks region";
  }

  const int blocks_count_before =
      (data[kBlocksLength + 1] << 8) | data[kBlocksLength];
  ASSERT_GT(blocks_count_before, 0)
      << "Vanilla ROM should have at least one pushable block";

  // Capture each region's bytes from the dereferenced PC, mirroring
  // the codec's region layout. We hold both the PC and the bytes so
  // the post-save assertions can resolve the region's PC again
  // (after-image; the four operand slots stay pinned by the encoder).
  const int kRegionSize = 0x80;
  const int ptrs[4] = {kBlocksPointer1, kBlocksPointer2, kBlocksPointer3,
                       kBlocksPointer4};
  std::vector<std::vector<uint8_t>> regions_before;
  std::vector<int> region_pcs;
  for (int r = 0; r < 4; ++r) {
    if (ptrs[r] + 2 >= static_cast<int>(data.size()))
      break;
    const int snes =
        (data[ptrs[r] + 2] << 16) | (data[ptrs[r] + 1] << 8) | data[ptrs[r]];
    const int pc = static_cast<int>(SnesToPc(static_cast<uint32_t>(snes)));
    const int off = r * kRegionSize;
    const int len = std::min(kRegionSize, blocks_count_before - off);
    if (len <= 0)
      break;
    if (pc < 0 || pc + len > static_cast<int>(data.size()))
      break;
    region_pcs.push_back(pc);
    regions_before.emplace_back(data.begin() + pc, data.begin() + pc + len);
  }
  ASSERT_FALSE(regions_before.empty())
      << "Vanilla blocks data must be reachable through the four pointer "
         "slots before the round-trip can be exercised.";

  // Materialize every room and load its blocks so the encoder's
  // "owned" path runs for all 99 vanilla entries. Same pattern as
  // SaveAllTorchesPreservesRegionWhenRoomsUnmodified above; Room is
  // a heavy type so heap-allocate the vector to avoid stack
  // overflow on test direct-execution.
  std::vector<Room> rooms;
  rooms.reserve(kNumberOfRooms);
  for (int i = 0; i < kNumberOfRooms; ++i) {
    rooms.emplace_back(i, rom_.get());
    rooms.back().LoadBlocks();
  }

  auto status = SaveAllBlocks(
      rom_.get(), kNumberOfRooms, [&rooms](int rid) -> const Room* {
        if (rid < 0 || rid >= static_cast<int>(rooms.size()))
          return nullptr;
        return &rooms[rid];
      });
  ASSERT_TRUE(status.ok()) << status.message();

  const auto& after = rom_->vector();
  const int blocks_count_after =
      (after[kBlocksLength + 1] << 8) | after[kBlocksLength];
  EXPECT_EQ(blocks_count_after, blocks_count_before)
      << "kBlocksLength immediate must not move on a no-op vanilla save.";

  for (size_t r = 0; r < regions_before.size(); ++r) {
    const int pc = region_pcs[r];
    const int len = static_cast<int>(regions_before[r].size());
    ASSERT_GE(static_cast<int>(after.size()), pc + len);
    for (int i = 0; i < len; ++i) {
      ASSERT_EQ(after[pc + i], regions_before[r][i])
          << "Pushable-block byte mismatch in region " << r << " offset " << i
          << " (entry " << ((r * kRegionSize + i) / 4) << ", byte " << (i % 4)
          << "). load_order ordering must preserve vanilla authoring "
             "order across all "
          << (blocks_count_before / 4) << " entries.";
    }
  }
}

// --- Torches (length @ kTorchesLengthPointer, data @ kTorchData, max 0x120) ---

TEST_F(DungeonSaveRegionTest,
       SaveAllTorchesPreservesRegionWhenRoomsUnmodified) {
  const auto& data = rom_->vector();
  constexpr int kTorchesMaxSize = 0x120;
  if (kTorchesLengthPointer + 1 >= static_cast<int>(data.size()) ||
      kTorchData + kTorchesMaxSize > static_cast<int>(data.size())) {
    GTEST_SKIP() << "ROM too small for torches region";
  }

  int len_before =
      (data[kTorchesLengthPointer + 1] << 8) | data[kTorchesLengthPointer];
  if (len_before > kTorchesMaxSize)
    len_before = kTorchesMaxSize;
  std::vector<uint8_t> torch_region_before(
      data.begin() + kTorchesLengthPointer,
      data.begin() + kTorchesLengthPointer + 2);
  torch_region_before.insert(torch_region_before.end(),
                             data.begin() + kTorchData,
                             data.begin() + kTorchData + len_before);

  std::vector<Room> rooms;
  rooms.reserve(kNumberOfRooms);
  size_t loaded_torch_count = 0;
  for (int i = 0; i < kNumberOfRooms; ++i) {
    rooms.emplace_back(i, rom_.get());
    rooms.back().LoadTorches();
    ASSERT_TRUE(rooms.back().AreTorchesLoaded());
    loaded_torch_count += rooms.back().GetTileObjects().size();
  }
  ASSERT_GT(loaded_torch_count, 0u)
      << "The ROM-backed test must exercise decoded torch entries";

  auto status = SaveAllTorches(rom_.get(), rooms);
  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_FALSE(rom_->dirty())
      << "A byte-identical torch round-trip must not dirty the ROM";

  const auto& after = rom_->vector();
  int len_after =
      (after[kTorchesLengthPointer + 1] << 8) | after[kTorchesLengthPointer];
  EXPECT_EQ(len_after, len_before) << "Torch length changed";
  for (int i = 0; i < len_before && i < len_after; ++i) {
    EXPECT_EQ(after[kTorchData + i], torch_region_before[2 + i])
        << "Torch data mismatch at offset " << i;
  }
}

}  // namespace test
}  // namespace zelda3
}  // namespace yaze
