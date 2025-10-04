#include "integration/dungeon_editor_test.h"

#include "app/zelda3/dungeon/room.h"
#include "app/zelda3/dungeon/room_object.h"

namespace yaze {
namespace test {

using namespace yaze::zelda3;

// Test cases using real ROM
TEST_F(DungeonEditorIntegrationTest, LoadRoomFromRealRom) {
  auto room = zelda3::LoadRoomFromRom(rom_.get(), kTestRoomId);
  EXPECT_NE(room.rom(), nullptr);
  room.LoadObjects();
  EXPECT_FALSE(room.GetTileObjects().empty());
}

TEST_F(DungeonEditorIntegrationTest, DungeonEditorInitialization) {
  ASSERT_TRUE(dungeon_editor_->Load().ok());
}

TEST_F(DungeonEditorIntegrationTest, ObjectEncodingRoundTrip) {
  auto room = zelda3::LoadRoomFromRom(rom_.get(), kTestRoomId);
  room.LoadObjects();
  
  auto encoded = room.EncodeObjects();
  EXPECT_FALSE(encoded.empty());
  EXPECT_EQ(encoded[encoded.size()-1], 0xFF); // Terminator
}

}  // namespace test
}  // namespace yaze
