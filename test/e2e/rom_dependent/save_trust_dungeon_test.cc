#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/source_artifact_publisher.h"
#include "e2e/rom_dependent/editor_save_test_base.h"
#include "zelda3/dungeon/custom_collision.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/dungeon_stream_allocator.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/track_collision_generator.h"
#include "zelda3/overworld/overworld_version_helper.h"

namespace yaze::test {
namespace {

enum class DungeonRomClass { kVanilla, kExpanded, kOracle };

std::string DungeonRomClassName(DungeonRomClass role) {
  switch (role) {
    case DungeonRomClass::kVanilla:
      return "Vanilla";
    case DungeonRomClass::kExpanded:
      return "Expanded";
    case DungeonRomClass::kOracle:
      return "Oracle";
  }
  return "Unknown";
}

class SaveTrustDungeonTest
    : public EditorSaveTestBase,
      public ::testing::WithParamInterface<DungeonRomClass> {
 protected:
  using Range = std::pair<uint32_t, uint32_t>;
  static constexpr int kCollisionRoom = 0x83;

  static std::vector<uint8_t> FileBytes(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>()};
  }

  void SetUp() override {
    // Reuse the base writer without requiring vanilla for every ROM class.
    if (std::getenv("YAZE_SKIP_ROM_TESTS")) {
      GTEST_SKIP() << "ROM tests disabled by YAZE_SKIP_ROM_TESTS";
    }
    if (GetParam() == DungeonRomClass::kOracle) {
      if (const char* path = std::getenv("YAZE_TEST_ROM_OOS")) {
        source_path_ = path;
      }
    } else {
      source_path_ = TestRomManager::GetRomPath(
          GetParam() == DungeonRomClass::kVanilla ? RomRole::kVanilla
                                                  : RomRole::kExpanded);
    }
    if (source_path_.empty() || !std::filesystem::exists(source_path_)) {
      GTEST_SKIP() << DungeonRomClassName(GetParam())
                   << " unavailable; Oracle requires YAZE_TEST_ROM_OOS";
    }
    source_path_ = std::filesystem::weakly_canonical(source_path_);
    if (GetParam() == DungeonRomClass::kExpanded) {
      if (const char* path = std::getenv("YAZE_TEST_ROM_OOS");
          path && std::filesystem::exists(path) &&
          std::filesystem::equivalent(source_path_, path)) {
        GTEST_SKIP() << "Expanded role aliases Oracle; a distinct v3 ROM "
                        "is required";
      }
    }
    ASSERT_EQ(source_path_.string().find("Oracle-of-Secrets.yazeproj"),
              std::string::npos);
    const auto before = core::ComputeSourceArtifactFileSha256(source_path_);
    ASSERT_TRUE(before.ok()) << before.status();
    source_hash_ = *before;
    RecordProperty("rom_class", DungeonRomClassName(GetParam()));
    RecordProperty("source_rom_sha256", source_hash_);

    directory_ =
        std::filesystem::temp_directory_path() /
        ("yaze_save_trust_dungeon_" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    ASSERT_TRUE(std::filesystem::create_directory(directory_));
    test_rom_path_ = (directory_ / "edit.sfc").string();
    ASSERT_TRUE(std::filesystem::copy_file(source_path_, test_rom_path_));
    const auto after = core::ComputeSourceArtifactFileSha256(source_path_);
    ASSERT_TRUE(after.ok()) << after.status();
    ASSERT_EQ(*after, source_hash_);
    original_file_ = FileBytes(test_rom_path_);
    ASSERT_FALSE(original_file_.empty());
    const auto loaded = rom_.LoadFromFile(test_rom_path_);
    ASSERT_TRUE(loaded.ok()) << loaded;
    const auto profile = zelda3::DetectOverworldRomProfile(rom_);
    if (GetParam() == DungeonRomClass::kVanilla) {
      ASSERT_TRUE(profile.is_vanilla);
    } else if (GetParam() == DungeonRomClass::kExpanded) {
      ASSERT_GE(profile.asm_version, 3);
      ASSERT_NE(profile.asm_version, 0xFF);
    }
    original_data_ = rom_.vector();
  }

  void TearDown() override {
    if (!source_hash_.empty()) {
      const auto after = core::ComputeSourceArtifactFileSha256(source_path_);
      EXPECT_TRUE(after.ok()) << after.status();
      if (after.ok()) {
        EXPECT_EQ(*after, source_hash_) << "Read-only ROM source changed";
      }
    }
    std::error_code error;
    if (!test_rom_path_.empty()) {
      std::filesystem::remove(test_rom_path_, error);
    }
    if (!directory_.empty()) {
      std::filesystem::remove(directory_, error);
    }
  }

  // Select a structurally valid, uniquely owned stream before editing. Shared
  // streams require an allocation manifest and are a separate acceptance case.
  std::optional<int> FindRoom(bool sprite, bool require_door = false) {
    std::map<uint32_t, int> owners;
    std::vector<zelda3::DungeonStreamRecord> streams;
    for (int id = 0; id < zelda3::kNumberOfRooms; ++id) {
      const auto record = sprite ? zelda3::ReadDungeonSpriteStream(rom_, id)
                                 : zelda3::ReadDungeonObjectStream(rom_, id);
      if (record.ok() && record->valid) {
        ++owners[record->data_pc];
        streams.push_back(*record);
      }
    }
    for (const auto& stream : streams) {
      if (owners[stream.data_pc] != 1)
        continue;
      auto room = zelda3::LoadRoomFromRom(&rom_, stream.room_id);
      if (sprite) {
        room.LoadSprites();
        if (!room.GetSprites().empty())
          return stream.room_id;
      } else if (require_door) {
        if (!room.GetDoors().empty())
          return stream.room_id;
      } else {
        for (const auto& object : room.GetTileObjects()) {
          if (object.options() == zelda3::ObjectOption::Nothing &&
              object.id_ < 0xF8 && object.x() < 60) {
            return stream.room_id;
          }
        }
      }
    }
    return std::nullopt;
  }

  size_t ObjectIndex(const zelda3::Room& room) {
    const auto& objects = room.GetTileObjects();
    for (size_t i = 0; i < objects.size(); ++i) {
      if (objects[i].options() == zelda3::ObjectOption::Nothing &&
          objects[i].id_ < 0xF8 && objects[i].x() < 60) {
        return i;
      }
    }
    return objects.size();
  }

  ::testing::AssertionResult ObjectOnDiskEquals(int room_id, size_t index,
                                                uint8_t expected_x) {
    Rom fresh;
    const auto loaded = fresh.LoadFromFile(test_rom_path_);
    if (!loaded.ok())
      return ::testing::AssertionFailure() << loaded;
    const auto room = zelda3::LoadRoomFromRom(&fresh, room_id);
    const auto& objects = room.GetTileObjects();
    if (index >= objects.size()) {
      return ::testing::AssertionFailure() << "Missing reopened object";
    }
    if (objects[index].x() != expected_x) {
      return ::testing::AssertionFailure()
             << "Fresh-file object x is " << +objects[index].x()
             << ", expected " << +expected_x;
    }
    return ::testing::AssertionSuccess();
  }

  void ExpectContained(const Rom& fresh, const std::vector<Range>& ranges) {
    ASSERT_EQ(fresh.size(), original_data_.size());
    for (size_t pc = 0; pc < original_data_.size(); ++pc) {
      const bool permitted =
          std::any_of(ranges.begin(), ranges.end(), [pc](const auto& range) {
            return pc >= range.first && pc < range.second;
          });
      if (!permitted && fresh.vector()[pc] != original_data_[pc]) {
        FAIL() << "I4 byte changed outside declared domain at PC 0x" << std::hex
               << pc;
      }
    }
  }

  std::vector<Range> CollisionRanges() const {
    // Only this room's pointer and the documented collision arena are writable.
    // In particular, the reserved WaterFill tail is outside these ranges.
    const uint32_t slot =
        zelda3::kCustomCollisionRoomPointers + kCollisionRoom * 3;
    return {{slot, slot + 3},
            {zelda3::kCustomCollisionDataPosition,
             zelda3::kCustomCollisionDataSoftEnd}};
  }

  bool HasCollisionRegion() const {
    return zelda3::HasCustomCollisionPointerTable(rom_.size()) &&
           zelda3::HasCustomCollisionDataRegion(rom_.size());
  }

  Rom rom_;
  std::filesystem::path source_path_;
  std::filesystem::path directory_;
  std::string source_hash_;
  std::vector<uint8_t> original_file_;
  std::vector<uint8_t> original_data_;
};

TEST_P(SaveTrustDungeonTest, I1NoOpObjectsSpritesDoorsAndCollision) {
  auto room = zelda3::LoadRoomFromRom(&rom_, 1);
  room.LoadSprites();
  ASSERT_TRUE(room.SaveObjects().ok());
  ASSERT_TRUE(room.SaveSprites().ok());
  std::vector<zelda3::Room> rooms;
  rooms.push_back(std::move(room));
  const auto collision = zelda3::SaveAllCollision(&rom_, absl::MakeSpan(rooms));
  ASSERT_TRUE(collision.ok()) << collision;
  ASSERT_TRUE(SaveRomToFile(&rom_, test_rom_path_).ok());
  EXPECT_EQ(FileBytes(test_rom_path_), original_file_);
}

TEST_P(SaveTrustDungeonTest, I2I4ObjectEditFreshDiskReopen) {
  const auto id = FindRoom(false);
  if (!id)
    GTEST_SKIP() << "No unique editable object stream in fixture";
  auto room = zelda3::LoadRoomFromRom(&rom_, *id);
  const size_t index = ObjectIndex(room);
  ASSERT_LT(index, room.GetTileObjects().size());
  const uint8_t expected = room.GetTileObjects()[index].x() + 1;
  room.GetTileObjects()[index].set_x(expected);
  room.MarkObjectStreamDirty();
  const auto stream = zelda3::ReadDungeonObjectStream(rom_, *id);
  ASSERT_TRUE(stream.ok()) << stream.status();
  const uint32_t door_slot = zelda3::kDoorPointers + *id * 3;
  const std::vector<Range> ranges{{stream->data_pc, stream->logical_end_pc},
                                  {door_slot, door_slot + 3}};
  const auto saved = room.SaveObjects();
  ASSERT_TRUE(saved.ok()) << saved;
  ASSERT_TRUE(SaveRomToFile(&rom_, test_rom_path_).ok());
  EXPECT_TRUE(ObjectOnDiskEquals(*id, index, expected));
  Rom fresh;
  ASSERT_TRUE(fresh.LoadFromFile(test_rom_path_).ok());
  ExpectContained(fresh, ranges);
}

TEST_P(SaveTrustDungeonTest, I2I4DoorEditFreshDiskReopen) {
  const auto id = FindRoom(false, true);
  if (!id)
    GTEST_SKIP() << "No unique door stream in fixture";
  auto room = zelda3::LoadRoomFromRom(&rom_, *id);
  auto& door = room.GetDoors().front();
  const uint8_t expected = door.position == 0 ? 1 : 0;
  door.position = expected;
  room.MarkObjectStreamDirty();
  const auto stream = zelda3::ReadDungeonObjectStream(rom_, *id);
  ASSERT_TRUE(stream.ok()) << stream.status();
  const uint32_t door_slot = zelda3::kDoorPointers + *id * 3;
  const auto saved = room.SaveObjects();
  ASSERT_TRUE(saved.ok()) << saved;
  ASSERT_TRUE(SaveRomToFile(&rom_, test_rom_path_).ok());
  Rom fresh;
  ASSERT_TRUE(fresh.LoadFromFile(test_rom_path_).ok());
  const auto reopened = zelda3::LoadRoomFromRom(&fresh, *id);
  ASSERT_FALSE(reopened.GetDoors().empty());
  EXPECT_EQ(reopened.GetDoors().front().position, expected);
  ExpectContained(fresh, {{stream->data_pc, stream->logical_end_pc},
                          {door_slot, door_slot + 3}});
}

TEST_P(SaveTrustDungeonTest, I2I4SpriteEditFreshDiskReopen) {
  const auto id = FindRoom(true);
  if (!id)
    GTEST_SKIP() << "No unique sprite stream in fixture";
  auto room = zelda3::LoadRoomFromRom(&rom_, *id);
  room.LoadSprites();
  const auto original = room.GetSprites().front();
  const uint8_t expected = original.x() == 0 ? 1 : 0;
  zelda3::Sprite moved(original.id(), expected, original.y(),
                       original.subtype(), original.layer());
  moved.set_key_drop(original.key_drop());
  room.GetSprites().front() = moved;
  room.MarkSpritesDirty();
  const auto stream = zelda3::ReadDungeonSpriteStream(rom_, *id);
  ASSERT_TRUE(stream.ok()) << stream.status();
  const auto saved = room.SaveSprites();
  ASSERT_TRUE(saved.ok()) << saved;
  ASSERT_TRUE(SaveRomToFile(&rom_, test_rom_path_).ok());
  Rom fresh;
  ASSERT_TRUE(fresh.LoadFromFile(test_rom_path_).ok());
  auto reopened = zelda3::LoadRoomFromRom(&fresh, *id);
  reopened.LoadSprites();
  ASSERT_FALSE(reopened.GetSprites().empty());
  EXPECT_EQ(reopened.GetSprites().front().x(), expected);
  EXPECT_EQ(reopened.GetSprites().front().key_drop(), original.key_drop());
  ExpectContained(fresh, {{stream->data_pc, stream->logical_end_pc}});
}

TEST_P(SaveTrustDungeonTest, I2I4ExplicitZeroCollisionFreshDiskReopen) {
  if (!HasCollisionRegion()) {
    GTEST_SKIP() << "Fixture has no expanded custom collision region";
  }
  const auto original = zelda3::LoadCustomCollisionMap(&rom_, kCollisionRoom);
  ASSERT_TRUE(original.ok()) << original.status();
  size_t offset = 0;
  while (offset < original->tiles.size() && original->HasOverride(offset) &&
         original->tiles[offset] == 0) {
    ++offset;
  }
  if (offset == original->tiles.size()) {
    GTEST_SKIP() << "Every fixture cell is already an explicit floor override";
  }
  std::vector<zelda3::Room> rooms;
  rooms.emplace_back(kCollisionRoom, &rom_);
  rooms.front().custom_collision() = *original;
  rooms.front().SetCollisionTile(offset % 64, offset / 64, 0);
  const auto expected = rooms.front().custom_collision();
  const auto saved = zelda3::SaveAllCollision(&rom_, absl::MakeSpan(rooms));
  ASSERT_TRUE(saved.ok()) << saved;
  ASSERT_TRUE(SaveRomToFile(&rom_, test_rom_path_).ok());
  Rom fresh;
  ASSERT_TRUE(fresh.LoadFromFile(test_rom_path_).ok());
  const auto reopened = zelda3::LoadCustomCollisionMap(&fresh, kCollisionRoom);
  ASSERT_TRUE(reopened.ok()) << reopened.status();
  EXPECT_TRUE(reopened->HasOverride(offset));
  EXPECT_EQ(reopened->tiles[offset], 0);
  EXPECT_EQ(*reopened, expected);
  ExpectContained(fresh, CollisionRanges());
}

TEST_P(SaveTrustDungeonTest, I2I4MinecartTrackFreshDiskReopen) {
  if (!HasCollisionRegion()) {
    GTEST_SKIP() << "Fixture has no expanded custom collision region";
  }
  const auto original = zelda3::LoadCustomCollisionMap(&rom_, kCollisionRoom);
  ASSERT_TRUE(original.ok()) << original.status();
  std::optional<std::pair<uint8_t, uint8_t>> position;
  for (int y = 8; y < 56 && !position; ++y) {
    for (int x = 8; x < 56 && !position; ++x) {
      const size_t offset = y * 64 + x;
      if (!original->HasOverride(offset) &&
          !original->HasOverride(offset + 1) &&
          !original->HasOverride(offset + 64) &&
          !original->HasOverride(offset + 65)) {
        position = {static_cast<uint8_t>(x), static_cast<uint8_t>(y)};
      }
    }
  }
  if (!position)
    GTEST_SKIP() << "No unowned 2x2 fixture collision area";
  const std::vector<zelda3::RoomObject> before;
  const std::vector<zelda3::RoomObject> after{
      zelda3::RoomObject(0x31, position->first, position->second, 0)};
  const auto edit =
      zelda3::SynchronizeMinecartTrackCollision(before, after, *original);
  ASSERT_TRUE(edit.ok()) << edit.status();
  ASSERT_EQ(edit->ownership.size(), 4u);
  const auto saved =
      zelda3::WriteTrackCollision(&rom_, kCollisionRoom, edit->collision);
  ASSERT_TRUE(saved.ok()) << saved;
  ASSERT_TRUE(SaveRomToFile(&rom_, test_rom_path_).ok());
  Rom fresh;
  ASSERT_TRUE(fresh.LoadFromFile(test_rom_path_).ok());
  const auto reopened = zelda3::LoadCustomCollisionMap(&fresh, kCollisionRoom);
  ASSERT_TRUE(reopened.ok()) << reopened.status();
  EXPECT_EQ(*reopened, edit->collision);
  ExpectContained(fresh, CollisionRanges());
}

TEST_P(SaveTrustDungeonTest, I3EditorUndoNotRun) {
  GTEST_SKIP() << "Domain serializers have no editor Undo action; a real "
                  "DungeonEditorV2 Undo/save/reopen case remains required";
}

TEST_P(SaveTrustDungeonTest, MustFailControlOmittingSaveToFile) {
  const auto id = FindRoom(false);
  if (!id)
    GTEST_SKIP() << "No unique editable object stream in fixture";
  auto room = zelda3::LoadRoomFromRom(&rom_, *id);
  const size_t index = ObjectIndex(room);
  ASSERT_LT(index, room.GetTileObjects().size());
  const uint8_t original = room.GetTileObjects()[index].x();
  const uint8_t expected = original + 1;
  room.GetTileObjects()[index].set_x(expected);
  room.MarkObjectStreamDirty();
  const auto saved = room.SaveObjects();
  ASSERT_TRUE(saved.ok()) << saved;
  // Same fresh-file predicate as the positive case, with only disk Save omitted.
  EXPECT_FALSE(ObjectOnDiskEquals(*id, index, expected));
  EXPECT_TRUE(ObjectOnDiskEquals(*id, index, original));
  EXPECT_EQ(FileBytes(test_rom_path_), original_file_);
}

INSTANTIATE_TEST_SUITE_P(
    SaveTrustRomClasses, SaveTrustDungeonTest,
    ::testing::Values(DungeonRomClass::kVanilla, DungeonRomClass::kExpanded,
                      DungeonRomClass::kOracle),
    [](const ::testing::TestParamInfo<DungeonRomClass>& info) {
      return DungeonRomClassName(info.param);
    });

}  // namespace
}  // namespace yaze::test
