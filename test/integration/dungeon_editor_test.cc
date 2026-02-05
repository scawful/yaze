#include "integration/dungeon_editor_test.h"

#include <algorithm>

#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace test {

using namespace yaze::zelda3;

// ============================================================================
// Basic Room Loading Tests
// ============================================================================

TEST_F(DungeonEditorIntegrationTest, LoadRoomFromRealRom) {
  auto room = zelda3::LoadRoomFromRom(rom_.get(), kTestRoomId);
  EXPECT_NE(room.rom(), nullptr);
  room.LoadObjects();
  EXPECT_FALSE(room.GetTileObjects().empty());
}

TEST_F(DungeonEditorIntegrationTest, LoadMultipleRooms) {
  // Test loading several different rooms
  for (int room_id : {0x00, 0x01, 0x02, 0x10, 0x20}) {
    auto room = zelda3::LoadRoomFromRom(rom_.get(), room_id);
    EXPECT_NE(room.rom(), nullptr)
        << "Failed to load room " << std::hex << room_id;
    room.LoadObjects();
    // Some rooms may be empty, but loading should not fail
  }
}

TEST_F(DungeonEditorIntegrationTest, DungeonEditorInitialization) {
  // Initialize the editor before loading
  dungeon_editor_->Initialize();

  // Now load should succeed
  auto status = dungeon_editor_->Load();
  ASSERT_TRUE(status.ok()) << "Load failed: " << status.message();
}

// ============================================================================
// Object Encoding/Decoding Tests
// ============================================================================

TEST_F(DungeonEditorIntegrationTest, ObjectEncodingRoundTrip) {
  auto room = zelda3::LoadRoomFromRom(rom_.get(), kTestRoomId);
  room.LoadObjects();

  auto encoded = room.EncodeObjects();
  EXPECT_FALSE(encoded.empty());
  // Expect door marker (0xF0, 0xFF) after final object terminator (0xFF, 0xFF)
  const std::vector<uint8_t> marker{0xFF, 0xFF, 0xF0, 0xFF};
  auto it = std::search(encoded.begin(), encoded.end(), marker.begin(),
                        marker.end());
  EXPECT_NE(it, encoded.end()) << "Missing object terminator/door marker";
}

TEST_F(DungeonEditorIntegrationTest, EncodeType1Object) {
  // Type 1: xxxxxxss yyyyyyss iiiiiiii (ID < 0x100)
  zelda3::RoomObject obj(0x10, 5, 7, 0x12, 0);  // id, x, y, size, layer
  auto bytes = obj.EncodeObjectToBytes();

  // Verify encoding format
  EXPECT_EQ((bytes.b1 >> 2), 5)
      << "X coordinate should be in upper 6 bits of b1";
  EXPECT_EQ((bytes.b2 >> 2), 7)
      << "Y coordinate should be in upper 6 bits of b2";
  EXPECT_EQ(bytes.b3, 0x10) << "Object ID should be in b3";
}

TEST_F(DungeonEditorIntegrationTest, EncodeType2Object) {
  // Type 2: 111111xx xxxxyyyy yyiiiiii (ID >= 0x100 && < 0x200)
  zelda3::RoomObject obj(0x150, 12, 8, 0, 0);
  auto bytes = obj.EncodeObjectToBytes();

  // Verify Type 2 marker
  EXPECT_EQ((bytes.b1 & 0xFC), 0xFC)
      << "Type 2 objects should have 111111 prefix";
}

TEST_F(DungeonEditorIntegrationTest, EncodeType3Object) {
  // Type 3: xxxxxxii yyyyyyii 11111iii (ID >= 0xF00)
  zelda3::RoomObject obj(0xF23, 3, 4, 0, 0);
  auto bytes = obj.EncodeObjectToBytes();

  // Verify Type 3 encoding: bytes.b3 = (id_ >> 4) & 0xFF
  // For ID 0xF23: (0xF23 >> 4) = 0xF2
  EXPECT_EQ(bytes.b3, 0xF2) << "Type 3: (ID >> 4) should be in b3";
}

// ============================================================================
// Object Manipulation Tests
// ============================================================================

TEST_F(DungeonEditorIntegrationTest, AddObjectToRoom) {
  auto room = zelda3::LoadRoomFromRom(rom_.get(), kTestRoomId);
  room.LoadObjects();

  size_t initial_count = room.GetTileObjects().size();

  // Add a new object (Type 1, so size must be <= 15)
  zelda3::RoomObject new_obj(0x20, 10, 10, 5, 0);
  new_obj.SetRom(rom_.get());
  auto status = room.AddObject(new_obj);

  EXPECT_TRUE(status.ok()) << "Failed to add object: " << status.message();
  EXPECT_EQ(room.GetTileObjects().size(), initial_count + 1);
}

TEST_F(DungeonEditorIntegrationTest, RemoveObjectFromRoom) {
  auto room = zelda3::LoadRoomFromRom(rom_.get(), kTestRoomId);
  room.LoadObjects();

  size_t initial_count = room.GetTileObjects().size();
  ASSERT_GT(initial_count, 0) << "Room should have at least one object";

  // Remove first object
  auto status = room.RemoveObject(0);

  EXPECT_TRUE(status.ok()) << "Failed to remove object: " << status.message();
  EXPECT_EQ(room.GetTileObjects().size(), initial_count - 1);
}

TEST_F(DungeonEditorIntegrationTest, UpdateObjectInRoom) {
  auto room = zelda3::LoadRoomFromRom(rom_.get(), kTestRoomId);
  room.LoadObjects();

  ASSERT_FALSE(room.GetTileObjects().empty());

  // Update first object's position
  zelda3::RoomObject updated_obj = room.GetTileObjects()[0];
  updated_obj.x_ = 15;
  updated_obj.y_ = 15;

  auto status = room.UpdateObject(0, updated_obj);

  EXPECT_TRUE(status.ok()) << "Failed to update object: " << status.message();
  EXPECT_EQ(room.GetTileObjects()[0].x_, 15);
  EXPECT_EQ(room.GetTileObjects()[0].y_, 15);
}

// ============================================================================
// Object Validation Tests
// ============================================================================

TEST_F(DungeonEditorIntegrationTest, ValidateObjectBounds) {
  auto room = zelda3::LoadRoomFromRom(rom_.get(), kTestRoomId);

  // Test objects within valid bounds (0-63 for x and y)
  zelda3::RoomObject valid_obj(0x10, 0, 0, 0, 0);
  EXPECT_TRUE(room.ValidateObject(valid_obj));

  zelda3::RoomObject valid_obj2(0x10, 31, 31, 0, 0);
  EXPECT_TRUE(room.ValidateObject(valid_obj2));

  zelda3::RoomObject valid_obj3(0x10, 63, 63, 0, 0);
  EXPECT_TRUE(room.ValidateObject(valid_obj3));

  // Test objects outside bounds (> 63)
  zelda3::RoomObject invalid_obj(0x10, 64, 64, 0, 0);
  EXPECT_FALSE(room.ValidateObject(invalid_obj));

  zelda3::RoomObject invalid_obj2(0x10, 100, 100, 0, 0);
  EXPECT_FALSE(room.ValidateObject(invalid_obj2));
}

// ============================================================================
// Save/Load Round-Trip Tests
// ============================================================================

TEST_F(DungeonEditorIntegrationTest, SaveAndReloadRoom) {
  auto room = zelda3::LoadRoomFromRom(rom_.get(), kTestRoomId);
  room.LoadObjects();

  size_t original_count = room.GetTileObjects().size();

  // Encode objects
  auto encoded = room.EncodeObjects();
  EXPECT_FALSE(encoded.empty());

  // Create a new room and decode
  auto room2 = zelda3::LoadRoomFromRom(rom_.get(), kTestRoomId);
  room2.LoadObjects();

  // Verify object count matches
  EXPECT_EQ(room2.GetTileObjects().size(), original_count);
}

// ============================================================================
// Object Rendering Tests
// ============================================================================

TEST_F(DungeonEditorIntegrationTest, RenderObjectWithTiles) {
  auto room = zelda3::LoadRoomFromRom(rom_.get(), kTestRoomId);
  room.LoadObjects();

  ASSERT_FALSE(room.GetTileObjects().empty());

  // Ensure tiles are loaded for first object
  auto& obj = room.GetTileObjects()[0];
  const_cast<zelda3::RoomObject&>(obj).SetRom(rom_.get());
  const_cast<zelda3::RoomObject&>(obj).EnsureTilesLoaded();

  EXPECT_FALSE(obj.tiles_.empty()) << "Object should have tiles after loading";
}

// ============================================================================
// Multi-Layer Tests
// ============================================================================

TEST_F(DungeonEditorIntegrationTest, ObjectsOnDifferentLayers) {
  auto room = zelda3::LoadRoomFromRom(rom_.get(), kTestRoomId);

  // Add objects on different layers
  zelda3::RoomObject obj_bg1(0x10, 5, 5, 0, 0);  // Layer 0 (BG2)
  zelda3::RoomObject obj_bg2(0x11, 6, 6, 0, 1);  // Layer 1 (BG1)
  zelda3::RoomObject obj_bg3(0x12, 7, 7, 0, 2);  // Layer 2 (BG3)

  room.AddObject(obj_bg1);
  room.AddObject(obj_bg2);
  room.AddObject(obj_bg3);

  // Encode and verify layer separation
  auto encoded = room.EncodeObjects();

  // Should have layer terminators (0xFF 0xFF between layers)
  int terminator_count = 0;
  for (size_t i = 0; i < encoded.size() - 1; i++) {
    if (encoded[i] == 0xFF && encoded[i + 1] == 0xFF) {
      terminator_count++;
    }
  }

  EXPECT_GE(terminator_count, 2) << "Should have at least 2 layer terminators";
}

}  // namespace test
}  // namespace yaze
