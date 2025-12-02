// Integration tests for Room object load/save cycle with real ROM data
// Phase 1, Task 2.1: Full round-trip verification

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "rom/rom.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"

// Helper function for SNES to PC address conversion
inline int SnesToPc(int addr) {
  int temp = (addr & 0x7FFF) + ((addr / 2) & 0xFF8000);
  return (temp + 0x0);
}

namespace yaze {
namespace zelda3 {
namespace test {

class RoomIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Load the ROM file
    rom_ = std::make_unique<Rom>();

    // Check if ROM file exists
    const char* rom_path = std::getenv("YAZE_TEST_ROM_PATH");
    if (!rom_path) {
      rom_path = "zelda3.sfc";
    }

    auto status = rom_->LoadFromFile(rom_path);
    if (!status.ok()) {
      GTEST_SKIP() << "ROM file not available: " << status.message();
    }

    // Create backup of ROM data for restoration after tests
    original_rom_data_ = rom_->vector();
  }

  void TearDown() override {
    // Restore original ROM data
    if (rom_ && !original_rom_data_.empty()) {
      for (size_t i = 0; i < original_rom_data_.size(); i++) {
        rom_->WriteByte(i, original_rom_data_[i]);
      }
    }
  }

  std::unique_ptr<Rom> rom_;
  std::vector<uint8_t> original_rom_data_;
};

// ============================================================================
// Test 1: Basic Load/Save Round-Trip
// ============================================================================

TEST_F(RoomIntegrationTest, BasicLoadSaveRoundTrip) {
  // Load room 0 (Hyrule Castle Entrance)
  Room room1(0x00, rom_.get());

  // Get original object count
  size_t original_count = room1.GetTileObjects().size();
  ASSERT_GT(original_count, 0) << "Room should have objects";

  // Store original objects
  auto original_objects = room1.GetTileObjects();

  // Save the room (should write same data back)
  auto save_status = room1.SaveObjects();
  ASSERT_TRUE(save_status.ok()) << save_status.message();

  // Load the room again
  Room room2(0x00, rom_.get());

  // Verify object count matches
  EXPECT_EQ(room2.GetTileObjects().size(), original_count);

  // Verify each object matches
  auto reloaded_objects = room2.GetTileObjects();
  ASSERT_EQ(reloaded_objects.size(), original_objects.size());

  for (size_t i = 0; i < original_objects.size(); i++) {
    SCOPED_TRACE("Object " + std::to_string(i));

    const auto& orig = original_objects[i];
    const auto& reload = reloaded_objects[i];

    EXPECT_EQ(reload.id_, orig.id_) << "ID mismatch";
    EXPECT_EQ(reload.x(), orig.x()) << "X position mismatch";
    EXPECT_EQ(reload.y(), orig.y()) << "Y position mismatch";
    EXPECT_EQ(reload.size(), orig.size()) << "Size mismatch";
    EXPECT_EQ(reload.GetLayerValue(), orig.GetLayerValue()) << "Layer mismatch";
  }
}

// ============================================================================
// Test 2: Multi-Room Verification
// ============================================================================

TEST_F(RoomIntegrationTest, MultiRoomLoadSaveRoundTrip) {
  // Test several different rooms to ensure broad coverage
  std::vector<int> test_rooms = {0x00, 0x01, 0x02, 0x10, 0x20};

  for (int room_id : test_rooms) {
    SCOPED_TRACE("Room " + std::to_string(room_id));

    // Load room
    Room room1(room_id, rom_.get());
    auto original_objects = room1.GetTileObjects();

    if (original_objects.empty()) {
      continue;  // Skip empty rooms
    }

    // Save objects
    auto save_status = room1.SaveObjects();
    ASSERT_TRUE(save_status.ok()) << save_status.message();

    // Reload and verify
    Room room2(room_id, rom_.get());
    auto reloaded_objects = room2.GetTileObjects();

    EXPECT_EQ(reloaded_objects.size(), original_objects.size());

    // Verify objects match
    for (size_t i = 0;
         i < std::min(original_objects.size(), reloaded_objects.size()); i++) {
      const auto& orig = original_objects[i];
      const auto& reload = reloaded_objects[i];

      EXPECT_EQ(reload.id_, orig.id_);
      EXPECT_EQ(reload.x(), orig.x());
      EXPECT_EQ(reload.y(), orig.y());
      EXPECT_EQ(reload.size(), orig.size());
      EXPECT_EQ(reload.GetLayerValue(), orig.GetLayerValue());
    }
  }
}

// ============================================================================
// Test 3: Layer Verification
// ============================================================================

TEST_F(RoomIntegrationTest, LayerPreservation) {
  // Load a room known to have multiple layers
  Room room(0x01, rom_.get());

  auto objects = room.GetTileObjects();
  ASSERT_GT(objects.size(), 0);

  // Count objects per layer
  int layer0_count = 0, layer1_count = 0, layer2_count = 0;
  for (const auto& obj : objects) {
    switch (obj.GetLayerValue()) {
      case 0:
        layer0_count++;
        break;
      case 1:
        layer1_count++;
        break;
      case 2:
        layer2_count++;
        break;
    }
  }

  // Save and reload
  ASSERT_TRUE(room.SaveObjects().ok());

  Room room2(0x01, rom_.get());
  auto reloaded = room2.GetTileObjects();

  // Verify layer counts match
  int reload_layer0 = 0, reload_layer1 = 0, reload_layer2 = 0;
  for (const auto& obj : reloaded) {
    switch (obj.GetLayerValue()) {
      case 0:
        reload_layer0++;
        break;
      case 1:
        reload_layer1++;
        break;
      case 2:
        reload_layer2++;
        break;
    }
  }

  EXPECT_EQ(reload_layer0, layer0_count);
  EXPECT_EQ(reload_layer1, layer1_count);
  EXPECT_EQ(reload_layer2, layer2_count);
}

// ============================================================================
// Test 4: Object Type Distribution
// ============================================================================

TEST_F(RoomIntegrationTest, ObjectTypeDistribution) {
  Room room(0x00, rom_.get());

  auto objects = room.GetTileObjects();
  ASSERT_GT(objects.size(), 0);

  // Count object types
  int type1_count = 0;  // ID < 0x100
  int type2_count = 0;  // ID 0x100-0x13F
  int type3_count = 0;  // ID >= 0xF00

  for (const auto& obj : objects) {
    if (obj.id_ >= 0xF00) {
      type3_count++;
    } else if (obj.id_ >= 0x100) {
      type2_count++;
    } else {
      type1_count++;
    }
  }

  // Save and reload
  ASSERT_TRUE(room.SaveObjects().ok());

  Room room2(0x00, rom_.get());
  auto reloaded = room2.GetTileObjects();

  // Verify type distribution matches
  int reload_type1 = 0, reload_type2 = 0, reload_type3 = 0;
  for (const auto& obj : reloaded) {
    if (obj.id_ >= 0xF00) {
      reload_type3++;
    } else if (obj.id_ >= 0x100) {
      reload_type2++;
    } else {
      reload_type1++;
    }
  }

  EXPECT_EQ(reload_type1, type1_count);
  EXPECT_EQ(reload_type2, type2_count);
  EXPECT_EQ(reload_type3, type3_count);
}

// ============================================================================
// Test 5: Binary Data Verification
// ============================================================================

TEST_F(RoomIntegrationTest, BinaryDataExactMatch) {
  // This test verifies that saving doesn't change ROM data
  // when no modifications are made

  Room room(0x02, rom_.get());

  // Get the ROM location where objects are stored
  auto rom_data = rom_->vector();
  int object_pointer = (rom_data[0x874C + 2] << 16) +
                       (rom_data[0x874C + 1] << 8) + (rom_data[0x874C]);
  object_pointer = SnesToPc(object_pointer);

  int room_address = object_pointer + (0x02 * 3);
  int tile_address = (rom_data[room_address + 2] << 16) +
                     (rom_data[room_address + 1] << 8) + rom_data[room_address];
  int objects_location = SnesToPc(tile_address);

  // Read original bytes (up to 500 bytes should cover most rooms)
  std::vector<uint8_t> original_bytes;
  for (int i = 0; i < 500 && objects_location + i < (int)rom_data.size(); i++) {
    original_bytes.push_back(rom_data[objects_location + i]);
    // Stop at final terminator
    if (i > 0 && original_bytes[i] == 0xFF && original_bytes[i - 1] == 0xFF) {
      // Check if this is the final terminator (3rd layer end)
      bool might_be_final = true;
      for (int j = i - 10; j < i - 1; j += 2) {
        if (j >= 0 && original_bytes[j] == 0xFF &&
            original_bytes[j + 1] == 0xFF) {
          // Found another FF FF marker, keep going
          break;
        }
      }
      if (might_be_final)
        break;
    }
  }

  // Save objects (should write identical data)
  ASSERT_TRUE(room.SaveObjects().ok());

  // Read bytes after save
  rom_data = rom_->vector();
  std::vector<uint8_t> saved_bytes;
  for (size_t i = 0;
       i < original_bytes.size() && objects_location + i < rom_data.size();
       i++) {
    saved_bytes.push_back(rom_data[objects_location + i]);
  }

  // Verify binary match
  ASSERT_EQ(saved_bytes.size(), original_bytes.size());
  for (size_t i = 0; i < original_bytes.size(); i++) {
    EXPECT_EQ(saved_bytes[i], original_bytes[i])
        << "Byte mismatch at offset " << i;
  }
}

// ============================================================================
// Test 6: Known Room Data Verification
// ============================================================================

TEST_F(RoomIntegrationTest, KnownRoomData) {
  // Room 0x00 (Hyrule Castle Entrance) - verify known objects exist
  Room room(0x00, rom_.get());

  auto objects = room.GetTileObjects();
  ASSERT_GT(objects.size(), 0) << "Room 0x00 should have objects";

  // Verify we can find common object types
  bool found_type1 = false;
  bool found_layer0 = false;
  bool found_layer1 = false;

  for (const auto& obj : objects) {
    if (obj.id_ < 0x100)
      found_type1 = true;
    if (obj.GetLayerValue() == 0)
      found_layer0 = true;
    if (obj.GetLayerValue() == 1)
      found_layer1 = true;
  }

  EXPECT_TRUE(found_type1) << "Should have Type 1 objects";
  EXPECT_TRUE(found_layer0) << "Should have Layer 0 objects";

  // Verify coordinates are in valid range (0-63)
  for (const auto& obj : objects) {
    EXPECT_GE(obj.x(), 0);
    EXPECT_LE(obj.x(), 63);
    EXPECT_GE(obj.y(), 0);
    EXPECT_LE(obj.y(), 63);
  }
}

}  // namespace test
}  // namespace zelda3
}  // namespace yaze
