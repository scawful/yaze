#include "app/editor/dungeon/dungeon_editor_v2.h"

#include <vector>

#include "core/features.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mocks/mock_rom.h"

namespace yaze::editor {

namespace {

struct DungeonSaveFlagsGuard {
  decltype(core::FeatureFlags::get().dungeon) prev = core::FeatureFlags::get().dungeon;
  ~DungeonSaveFlagsGuard() { core::FeatureFlags::get().dungeon = prev; }
};

void ConfigureMinimalDungeonSave() {
  auto& d = core::FeatureFlags::get().dungeon;
  d.kSaveObjects = true;
  d.kSaveSprites = false;
  d.kSaveRoomHeaders = false;
  d.kSaveTorches = false;
  d.kSavePits = false;
  d.kSaveBlocks = false;
  d.kSaveCollision = false;
  d.kSaveChests = false;
  d.kSavePotItems = false;
  d.kSavePalettes = false;
}

}  // namespace

TEST(DungeonEditorV2RomSafetyTest, SaveUsesRoomIndicesNotInternalIds) {
  test::MockRom rom;
  ASSERT_TRUE(rom.SetTestData(std::vector<uint8_t>(0x8000, 0)).ok());

  // No rooms are loaded, so SaveRoomData should be a no-op and never write.
  EXPECT_CALL(rom, WriteHelper(testing::_)).Times(0);

  DungeonEditorV2 editor(&rom);

  // Inject a room with an out-of-range internal ID. Save must not use
  // room.id() for iteration (otherwise it would try SaveRoomData(999)).
  editor.rooms()[5] = zelda3::Room(999, &rom, nullptr);
  editor.rooms()[5].SetLoaded(false);

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();

  auto status = editor.Save();
  EXPECT_TRUE(status.ok()) << status.message();
}

TEST(DungeonEditorV2RomSafetyTest, SaveRoomBlocksInvalidRoomBeforeWriting) {
  test::MockRom rom;
  ASSERT_TRUE(rom.SetTestData(std::vector<uint8_t>(0x8000, 0)).ok());
  EXPECT_CALL(rom, WriteHelper(testing::_)).Times(0);

  DungeonEditorV2 editor(&rom);

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();

  auto& room = editor.rooms()[0];
  room.SetLoaded(true);
  room.ClearTileObjects();
  room.AddTileObject(zelda3::RoomObject(/*id=*/0x01, /*x=*/70, /*y=*/0,
                                        /*size=*/0, /*layer=*/0));

  auto status = editor.SaveRoom(0);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
}

}  // namespace yaze::editor
