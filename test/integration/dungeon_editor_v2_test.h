#ifndef YAZE_TEST_INTEGRATION_DUNGEON_EDITOR_V2_TEST_H
#define YAZE_TEST_INTEGRATION_DUNGEON_EDITOR_V2_TEST_H

#include <memory>
#include <string>

#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/rom.h"
#include "gtest/gtest.h"

namespace yaze {
namespace test {

/**
 * @brief Integration test framework for DungeonEditorV2
 * 
 * Tests the simplified component delegation architecture
 */
class DungeonEditorV2IntegrationTest : public ::testing::Test {
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
    
    // Create V2 editor with ROM
    dungeon_editor_v2_ = std::make_unique<editor::DungeonEditorV2>(rom_.get());
  }

  void TearDown() override {
    dungeon_editor_v2_.reset();
    rom_.reset();
  }

  std::unique_ptr<Rom> rom_;
  std::unique_ptr<editor::DungeonEditorV2> dungeon_editor_v2_;
  
  static constexpr int kTestRoomId = 0x01;
};

}  // namespace test
}  // namespace yaze

#endif  // YAZE_TEST_INTEGRATION_DUNGEON_EDITOR_V2_TEST_H

