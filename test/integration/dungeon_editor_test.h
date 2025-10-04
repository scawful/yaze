#ifndef YAZE_TEST_INTEGRATION_DUNGEON_EDITOR_TEST_H
#define YAZE_TEST_INTEGRATION_DUNGEON_EDITOR_TEST_H

#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include "absl/status/status.h"
#include "app/editor/dungeon/dungeon_editor.h"
#include "app/rom.h"
#include "gtest/gtest.h"

namespace yaze {
namespace test {

class MockRom;

/**
 * @brief Integration test framework for dungeon editor components
 * 
 * This class provides a comprehensive testing framework for the dungeon editor,
 * allowing modular testing of individual components and their interactions.
 */
class DungeonEditorIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override;
  void TearDown() override;

  // Test data setup
  absl::Status CreateMockRom();
  absl::Status LoadTestRoomData();
  
  // Component testing helpers
  absl::Status TestObjectParsing();
  absl::Status TestObjectRendering();
  absl::Status TestRoomGraphics();
  absl::Status TestPaletteHandling();
  
  // Mock data generators
  std::vector<uint8_t> GenerateMockRoomHeader(int room_id);
  std::vector<uint8_t> GenerateMockObjectData();
  std::vector<uint8_t> GenerateMockGraphicsData();

  std::unique_ptr<MockRom> mock_rom_;
  std::unique_ptr<editor::DungeonEditor> dungeon_editor_;
  
  // Test constants
  static constexpr int kTestRoomId = 0x01;
  static constexpr int kTestObjectId = 0x10;
  static constexpr size_t kMockRomSize = 0x200000; // 2MB mock ROM
};

/**
 * @brief Mock ROM class for testing without real ROM files
 */
class MockRom : public Rom {
 public:
  MockRom() = default;

  // Test data injection
  absl::Status SetMockData(const std::vector<uint8_t>& data);
  absl::Status LoadAndOwnData(const std::vector<uint8_t>& data);
  void SetMockRoomData(int room_id, const std::vector<uint8_t>& data);
  void SetMockObjectData(int object_id, const std::vector<uint8_t>& data);
  void SetMockGraphicsData(const std::vector<uint8_t>& data);
  
  // Validation helpers
  bool ValidateRoomData(int room_id) const;
  bool ValidateObjectData(int object_id) const;

 private:
  void EnsureBufferCapacity(uint32_t size);
  void InitializeMemoryLayout();

  std::vector<uint8_t> backing_buffer_;
  std::map<int, std::vector<uint8_t>> mock_room_data_;
  std::map<int, std::vector<uint8_t>> mock_object_data_;
  std::vector<uint8_t> mock_graphics_data_;

  uint32_t room_header_table_pc_ = 0;
  uint32_t room_header_data_base_pc_ = 0;
  uint32_t room_object_table_pc_ = 0;
  uint32_t room_object_data_base_pc_ = 0;

  static constexpr uint32_t kRoomHeaderStride = 0x40;
  static constexpr uint32_t kRoomObjectStride = 0x100;
};

}  // namespace test
}  // namespace yaze

#endif  // YAZE_TEST_INTEGRATION_DUNGEON_EDITOR_TEST_H