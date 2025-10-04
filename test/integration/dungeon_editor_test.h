#ifndef YAZE_TEST_INTEGRATION_DUNGEON_EDITOR_TEST_H
#define YAZE_TEST_INTEGRATION_DUNGEON_EDITOR_TEST_H

#include <memory>
#include <string>

#include "app/editor/dungeon/dungeon_editor.h"
#include "app/rom.h"
#include "app/zelda3/dungeon/room.h"
#include "gtest/gtest.h"

namespace yaze {
namespace test {

/**
 * @brief Integration test framework using real ROM data
 */
class DungeonEditorIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Use the real ROM
    rom_ = std::make_unique<Rom>();
    ASSERT_TRUE(rom_->LoadFromFile("build/bin/zelda3.sfc").ok());
    dungeon_editor_ = std::make_unique<editor::DungeonEditor>();
    dungeon_editor_->set_rom(rom_.get());
  }

  void TearDown() override {
    dungeon_editor_.reset();
    rom_.reset();
  }

  std::unique_ptr<Rom> rom_;
  std::unique_ptr<editor::DungeonEditor> dungeon_editor_;
  
  static constexpr int kTestRoomId = 0x01;
};

}  // namespace test
}  // namespace yaze

#endif  // YAZE_TEST_INTEGRATION_DUNGEON_EDITOR_TEST_H
