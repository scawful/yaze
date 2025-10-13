#ifndef YAZE_TEST_INTEGRATION_DUNGEON_EDITOR_TEST_H
#define YAZE_TEST_INTEGRATION_DUNGEON_EDITOR_TEST_H

#include <memory>
#include <string>

#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/rom.h"
#include "zelda3/dungeon/room.h"
#include "gtest/gtest.h"

namespace yaze {
namespace test {

/**
 * @brief Integration test framework using real ROM data
 * 
 * Updated for DungeonEditorV2 with card-based architecture
 */
class DungeonEditorIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Use the real ROM (try multiple locations)
    rom_ = std::make_unique<Rom>();
    auto status = rom_->LoadFromFile("assets/zelda3.sfc");
    if (!status.ok()) {
      status = rom_->LoadFromFile("build/bin/zelda3.sfc");
    }
    if (!status.ok()) {
      status = rom_->LoadFromFile("zelda3.sfc");
    }
    ASSERT_TRUE(status.ok()) << "Could not load zelda3.sfc from any location";
    ASSERT_TRUE(rom_->InitializeForTesting().ok());
    
    // Initialize DungeonEditorV2 with ROM
    dungeon_editor_ = std::make_unique<editor::DungeonEditorV2>();
    dungeon_editor_->set_rom(rom_.get());
    
    // Load editor data
    auto load_status = dungeon_editor_->Load();
    ASSERT_TRUE(load_status.ok()) << "Failed to load dungeon editor: " 
                                   << load_status.message();
  }

  void TearDown() override {
    dungeon_editor_.reset();
    rom_.reset();
  }

  std::unique_ptr<Rom> rom_;
  std::unique_ptr<editor::DungeonEditorV2> dungeon_editor_;
  
  static constexpr int kTestRoomId = 0x01;
};

}  // namespace test
}  // namespace yaze

#endif  // YAZE_TEST_INTEGRATION_DUNGEON_EDITOR_TEST_H
