// Writes a ROM copy with deterministic dungeon edits made through yaze's own
// save paths, for the edit -> save -> play check:
//   1. YAZE_EDIT_FIXTURE_OUT=<rom> yaze_test_integration \
//          --gtest_filter='DungeonEditSaveFixture.*'
//   2. Capture the edited rooms from that ROM in Mesen
//      (scripts/agents/capture-game-room-tilemaps.py --rom <rom> --rooms ...).
//   3. Run DungeonGameTilemapParityReport / DungeonGameStateParityTest with
//      YAZE_TEST_ROM_VANILLA=<rom> and the capture folder.
// If yaze saves an edit wrongly, the game renders the edited ROM differently
// from what yaze shows. The edited ROM is ROM-derived data: never commit it.
// The output must be a new scratch filename. Existing files (including the
// source ROM and links to it) are rejected; publication requires hard links.

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "absl/strings/str_format.h"
#include "fresh_rom_fixture_output.h"
#include "rom/rom.h"
#include "test_utils.h"
#include "zelda3/dungeon/room.h"

namespace yaze::test {
namespace {

// Rooms across dungeons; edits keep each room's data the same size.
constexpr int kEditedRooms[] = {0x001, 0x002, 0x012, 0x024, 0x042, 0x055,
                                0x065, 0x0A8, 0x0C9, 0x0DB, 0x0E0, 0x107};

bool IsPlainStreamObject(const zelda3::RoomObject& object) {
  return object.options() == zelda3::ObjectOption::Nothing;
}

TEST(DungeonEditSaveFixture, WriteEditedRom) {
  const char* out = std::getenv("YAZE_EDIT_FIXTURE_OUT");
  if (out == nullptr) {
    GTEST_SKIP() << "Set YAZE_EDIT_FIXTURE_OUT to write the edited ROM.";
  }
  const auto output_status = ValidateFreshFixtureOutput(out);
  ASSERT_TRUE(output_status.ok()) << output_status;
  YAZE_SKIP_IF_ROM_MISSING(RomRole::kVanilla, "DungeonEditSaveFixture");
  auto rom = std::make_unique<Rom>();
  ASSERT_TRUE(
      rom->LoadFromFile(TestRomManager::GetRomPath(RomRole::kVanilla)).ok());

  for (int room_id : kEditedRooms) {
    zelda3::Room room = zelda3::LoadRoomFromRom(rom.get(), room_id);
    room.LoadSprites();
    auto& objects = room.GetTileObjects();
    ASSERT_FALSE(objects.empty()) << room_id;

    // 1. Move the first plain type-1 object one tile right.
    for (auto& object : objects) {
      if (IsPlainStreamObject(object) && object.id_ < 0x100 &&
          object.x() < 60) {
        std::cout << absl::StrFormat(
            "room 0x%03X: object 0x%03X (%d,%d) -> (%d,%d)\n", room_id,
            object.id_, object.x(), object.y(), object.x() + 1, object.y());
        object.set_x(static_cast<uint8_t>(object.x() + 1));
        break;
      }
    }
    // 2. Grow a later type-1 object's size by one.
    for (size_t i = objects.size() / 2; i < objects.size(); ++i) {
      auto& object = objects[i];
      if (IsPlainStreamObject(object) && object.id_ < 0xF8 &&
          (object.size_ & 0x0F) < 0x0F) {
        std::cout << absl::StrFormat(
            "room 0x%03X: object %zu 0x%03X size %d -> %d\n", room_id, i,
            object.id_, object.size_, object.size_ + 1);
        object.set_size(static_cast<uint8_t>(object.size_ + 1));
        break;
      }
    }
    // Direct mutation needs the dirty mark the editor's edit paths set.
    room.MarkObjectStreamDirty();
    ASSERT_TRUE(room.SaveObjects().ok()) << room_id;

    // 3. Move the first sprite one tile right.
    auto& sprites = room.GetSprites();
    if (!sprites.empty() && sprites[0].x() < 30) {
      std::cout << absl::StrFormat("room 0x%03X: sprite 0x%02X x %d -> %d\n",
                                   room_id, sprites[0].id(), sprites[0].x(),
                                   sprites[0].x() + 1);
      const zelda3::Sprite original = sprites[0];
      zelda3::Sprite moved(original.id(),
                           static_cast<uint8_t>(original.x() + 1), original.y(),
                           original.subtype(), original.layer());
      moved.set_key_drop(original.key_drop());
      sprites[0] = moved;
      room.MarkSpritesDirty();
      ASSERT_TRUE(room.SaveSprites().ok()) << room_id;
    }
  }

  const auto saved = SaveFreshFixtureRom(*rom, out);
  ASSERT_TRUE(saved.ok()) << saved;
  std::cout << "Wrote " << out << "\n";
}

}  // namespace
}  // namespace yaze::test
