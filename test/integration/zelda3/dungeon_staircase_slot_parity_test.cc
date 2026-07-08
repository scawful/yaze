// ROM-backed parity test for the connected-mode staircase slot consumption
// assumption.
//
// `CollectDungeonConnectedRoomLinkDiagnostics` (dungeon_canvas_connected_view.
// cc) assumes the Nth placed header-backed staircase tile object consumes
// `room.staircase_room(N)`. This file proves the assumption against vanilla
// ROM data along three axes:
//
//   1. The placement-order walk matches `Room::HandleSpecialObjects`'s
//      existing `nbr_of_staircase` increment in room.cc (which is the
//      production code path used by the dungeon editor itself).
//   2. For every vanilla room with placed header-backed staircase objects,
//      each placed object lands at a slot whose `staircase_rooms_[]` entry
//      is non-zero — i.e. no vanilla room would emit a `MissingDestination`
//      diagnostic.
//   3. At least one vanilla room has multiple placed header-backed staircase
//      objects, exercising the >1-stair branch.
//
// The test deliberately scopes its filter to the conservative set used by
// `kStairsObjects` (room.cc 0x12D, 0x12E, 0x138, 0x139, 0x13B). Connected-
// mode's broader filter — straight inter-room stairs (0xF9E-0xFA1, 0xFA6-
// 0xFA9) and the unused 0x12F / 0x13A spiral lower variants — is documented
// at `dungeon_canvas_connected_view.cc:HEAD assumption block` but not
// proven here because vanilla rooms don't combine the two sets in a way
// that distinguishes them. If a custom hack ever places a straight stair
// alongside a fat/spiral stair and reports "missing connection", this is
// the test that should grow a fixture.
//
// Safety: never checks ROM fixtures into the repo; reads ROMs from
// TestRomManager-managed local paths and skips cleanly when absent.

#include <gtest/gtest.h>

#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "rom/rom.h"
#include "test_utils.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/room.h"

namespace yaze::zelda3::test {
namespace {

// The `kStairsObjects` set used by `Room::HandleSpecialObjects` — this is the
// production-code definition of "header-backed inter-room staircase".
// Connected mode's filter is a superset; see the file header comment.
bool IsConservativeStairObject(int16_t object_id) {
  for (uint16_t id : kStairsObjects) {
    if (id == object_id) {
      return true;
    }
  }
  return false;
}

struct StaircaseSummary {
  int room_id = -1;
  int placed_count = 0;
  int populated_slots = 0;
  std::vector<int16_t> placed_ids;
  std::vector<uint8_t> slot_destinations;  // staircase_rooms_[0..3]
};

StaircaseSummary SummarizeRoom(Rom* rom, int room_id) {
  StaircaseSummary summary;
  summary.room_id = room_id;
  Room room = LoadRoomFromRom(rom, room_id);

  for (const auto& object : room.GetTileObjects()) {
    if (IsConservativeStairObject(object.id_)) {
      summary.placed_ids.push_back(object.id_);
      ++summary.placed_count;
    }
  }
  for (int i = 0; i < 4; ++i) {
    summary.slot_destinations.push_back(room.staircase_room(i));
    if (room.staircase_room(i) != 0) {
      ++summary.populated_slots;
    }
  }
  return summary;
}

class DungeonStaircaseSlotParityTest : public ::testing::Test {
 protected:
  void SetUp() override {
    YAZE_SKIP_IF_ROM_MISSING(::yaze::test::RomRole::kVanilla,
                             "DungeonStaircaseSlotParityTest");
    const std::string rom_path = ::yaze::test::TestRomManager::GetRomPath(
        ::yaze::test::RomRole::kVanilla);
    ASSERT_TRUE(rom_.LoadFromFile(rom_path).ok())
        << "Failed to load ROM from " << rom_path;
  }

  Rom rom_;
};

// Every placed header-backed staircase object in every vanilla room must land
// at a slot whose `staircase_room` destination is non-zero. If this fails,
// either the vanilla data has a "MissingDestination" inconsistency we should
// know about, or the placement-order → slot-index mapping is wrong.
TEST_F(DungeonStaircaseSlotParityTest,
       EveryVanillaPlacedStairConsumesNonZeroHeaderSlot) {
  std::vector<StaircaseSummary> rooms_with_stairs;
  for (int room_id = 0; room_id < kNumberOfRooms; ++room_id) {
    auto summary = SummarizeRoom(&rom_, room_id);
    if (summary.placed_count == 0)
      continue;
    rooms_with_stairs.push_back(summary);

    for (int slot = 0; slot < summary.placed_count && slot < 4; ++slot) {
      EXPECT_NE(summary.slot_destinations[slot], 0u)
          << "Vanilla room 0x" << std::hex << room_id << std::dec
          << " has placed staircase #" << slot << " (object 0x" << std::hex
          << summary.placed_ids[slot] << std::dec << ") but staircase_room["
          << slot << "] is 0 (unset). Either "
          << "the placement-order assumption is wrong here, or vanilla "
          << "data needs a MissingDestination diagnostic carve-out.";
    }
    EXPECT_LE(summary.placed_count, 4)
        << "Vanilla room 0x" << std::hex << room_id << std::dec
        << " has more than 4 placed staircase objects (" << summary.placed_count
        << ") — the runtime can only consume 4 "
        << "header slots; extras would surface as ExtraPlacedObject "
        << "diagnostics. If this triggers, vanilla itself is malformed.";
  }
  EXPECT_FALSE(rooms_with_stairs.empty())
      << "Vanilla ROM should have at least one room with placed staircases; "
      << "if this fails the conservative kStairsObjects filter is wrong.";
}

// At least one vanilla room must exercise the multi-staircase branch in
// `Room::HandleSpecialObjects` so the placement-order → slot-index walk is
// actually tested against ROM data, not just single-stair rooms (which are
// trivial: placement 0 → slot 0).
TEST_F(DungeonStaircaseSlotParityTest,
       AtLeastOneVanillaRoomExercisesMultiStaircaseSlots) {
  std::vector<StaircaseSummary> multi_stair_rooms;
  for (int room_id = 0; room_id < kNumberOfRooms; ++room_id) {
    auto summary = SummarizeRoom(&rom_, room_id);
    if (summary.placed_count >= 2) {
      multi_stair_rooms.push_back(std::move(summary));
    }
  }

  ASSERT_FALSE(multi_stair_rooms.empty())
      << "No vanilla room has 2+ placed header-backed staircase objects. "
      << "Either the test's filter is too narrow or vanilla content has "
      << "drifted; either way the placement-order assumption is no longer "
      << "exercised. Investigate before relying on the diagnostic.";

  // Cross-check each multi-stair room's mapping against
  // `Room::HandleSpecialObjects` semantics: incrementing nbr_of_staircase
  // (0..3) per placed match means placed[i] consumes slot i.
  for (const auto& summary : multi_stair_rooms) {
    for (int slot = 0; slot < summary.placed_count && slot < 4; ++slot) {
      EXPECT_NE(summary.slot_destinations[slot], 0u)
          << "Room 0x" << std::hex << summary.room_id << std::dec << " placed["
          << slot << "] (object 0x" << std::hex << summary.placed_ids[slot]
          << std::dec << ") expects "
          << "staircase_room[" << slot << "] != 0";
    }
  }
}

// Pin the placement-order walk against `Room::HandleSpecialObjects`'s actual
// behavior: when the room is loaded, every placed header-backed staircase
// object should have `ObjectOption::Stairs` set on it, indicating the
// production code path successfully matched it to a slot.
TEST_F(DungeonStaircaseSlotParityTest,
       PlacedStairsCarryStairsOptionAfterRoomLoad) {
  bool found_any = false;
  for (int room_id = 0; room_id < kNumberOfRooms; ++room_id) {
    Room room = LoadRoomFromRom(&rom_, room_id);
    int placed_index = 0;
    for (const auto& object : room.GetTileObjects()) {
      if (!IsConservativeStairObject(object.id_))
        continue;
      found_any = true;
      EXPECT_TRUE(static_cast<bool>(object.options() & ObjectOption::Stairs))
          << "Room 0x" << std::hex << room_id << std::dec << " placed["
          << placed_index << "] (object 0x" << std::hex << object.id_
          << std::dec << ") was not tagged with ObjectOption::Stairs by "
          << "Room::HandleSpecialObjects — the production code path didn't "
          << "match it to a header slot, so the diagnostic's placement-"
          << "order assumption may be inconsistent for this object id.";
      ++placed_index;
    }
  }
  EXPECT_TRUE(found_any)
      << "No vanilla rooms had header-backed staircase objects to tag. "
      << "If the filter regressed, this test would silently pass.";
}

}  // namespace
}  // namespace yaze::zelda3::test
