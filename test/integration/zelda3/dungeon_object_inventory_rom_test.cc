// Checks the Object Coverage panel's inventory against a real vanilla ROM.
//
// The panel lists GetMappedObjectIds() and finds rooms through
// ObjectUsageIndex. If vanilla placed an object ID the registry does not map,
// the panel would hide it, and nobody would ever be asked to check it.

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "app/editor/dungeon/object_coverage_model.h"
#include "rom/rom.h"
#include "test_utils.h"
#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/room.h"

namespace yaze::test {
namespace {

class DungeonObjectInventoryRomTest : public ::testing::Test {
 protected:
  void SetUp() override {
    YAZE_SKIP_IF_ROM_MISSING(RomRole::kVanilla,
                             "DungeonObjectInventoryRomTest");
    rom_ = std::make_unique<Rom>();
    const auto status =
        rom_->LoadFromFile(TestRomManager::GetRomPath(RomRole::kVanilla));
    if (!status.ok()) {
      GTEST_SKIP() << "Vanilla ROM not loadable: " << status.message();
    }
    for (int room_id = 0; room_id < zelda3::kNumberOfRooms; ++room_id) {
      zelda3::Room room(room_id, rom_.get());
      room.EnsureObjectsLoaded();
      usage_.AddRoom(room_id, room.GetTileObjects());
    }
  }

  std::unique_ptr<Rom> rom_;
  editor::ObjectUsageIndex usage_;
};

TEST_F(DungeonObjectInventoryRomTest, EveryPlacedObjectIsInTheInventory) {
  auto& registry = zelda3::DrawRoutineRegistry::Get();
  registry.Initialize();
  const std::vector<int16_t> mapped = registry.GetMappedObjectIds();
  const std::set<int> inventory(mapped.begin(), mapped.end());

  ASSERT_EQ(usage_.rooms_scanned(), zelda3::kNumberOfRooms);
  int placed_ids = 0;
  for (int object_id = 0; object_id <= 0xFFF; ++object_id) {
    if (usage_.Find(object_id) == nullptr) {
      continue;
    }
    ++placed_ids;
    EXPECT_EQ(inventory.count(object_id), 1u)
        << "vanilla places " << editor::FormatObjectId(object_id)
        << " but the registry does not map it";
  }
  // Vanilla uses a few hundred distinct IDs; a handful means the scan broke.
  EXPECT_GT(placed_ids, 100);
  RecordProperty("placed_object_ids", placed_ids);
  RecordProperty("inventory_ids", static_cast<int>(inventory.size()));
}

// "Go" in the panel selects the object by its index in the room. A fresh
// load must put the same object at that index.
TEST_F(DungeonObjectInventoryRomTest, OccurrenceIndicesMatchAFreshLoad) {
  int checked = 0;
  for (int room_id : {0x001, 0x042, 0x077}) {
    zelda3::Room room(room_id, rom_.get());
    room.EnsureObjectsLoaded();
    const auto& objects = room.GetTileObjects();
    for (size_t i = 0; i < objects.size(); ++i) {
      const auto* occurrences = usage_.Find(objects[i].id_);
      ASSERT_NE(occurrences, nullptr);
      bool found = false;
      for (const auto& occurrence : *occurrences) {
        if (occurrence.room_id == room_id && occurrence.object_index == i) {
          EXPECT_EQ(occurrence.x, objects[i].x());
          EXPECT_EQ(occurrence.y, objects[i].y());
          found = true;
        }
      }
      EXPECT_TRUE(found) << "room " << room_id << " object " << i;
      ++checked;
    }
  }
  EXPECT_GT(checked, 0);
}

}  // namespace
}  // namespace yaze::test
