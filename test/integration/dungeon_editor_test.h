#ifndef YAZE_TEST_INTEGRATION_DUNGEON_EDITOR_TEST_H
#define YAZE_TEST_INTEGRATION_DUNGEON_EDITOR_TEST_H

#include <memory>
#include <string>

#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "rom/rom.h"
#include "gtest/gtest.h"
#include "test/test_utils.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/game_data.h"

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
    TestRomManager::SkipIfRomMissing(RomRole::kVanilla,
                                     "DungeonEditorIntegrationTest");
    rom_ = std::make_unique<Rom>();
    const std::string rom_path = TestRomManager::GetRomPath(RomRole::kVanilla);
    auto status = rom_->LoadFromFile(rom_path);
    ASSERT_TRUE(status.ok()) << "Could not load ROM from " << rom_path;

    // Load Zelda3-specific game data
    game_data_ = std::make_unique<zelda3::GameData>(rom_.get());
    auto load_game_data_status = zelda3::LoadGameData(*rom_, *game_data_);
    ASSERT_TRUE(load_game_data_status.ok())
        << "Failed to load game data: " << load_game_data_status.message();

    // Initialize DungeonEditorV2 with ROM and GameData
    dungeon_editor_ = std::make_unique<editor::DungeonEditorV2>();
    dungeon_editor_->SetRom(rom_.get());
    dungeon_editor_->SetGameData(game_data_.get());

    // Load editor data
    auto load_status = dungeon_editor_->Load();
    ASSERT_TRUE(load_status.ok())
        << "Failed to load dungeon editor: " << load_status.message();
  }

  void TearDown() override {
    dungeon_editor_.reset();
    game_data_.reset();
    rom_.reset();
  }

  std::unique_ptr<Rom> rom_;
  std::unique_ptr<zelda3::GameData> game_data_;
  std::unique_ptr<editor::DungeonEditorV2> dungeon_editor_;

  static constexpr int kTestRoomId = 0x01;
};

}  // namespace test
}  // namespace yaze

#endif  // YAZE_TEST_INTEGRATION_DUNGEON_EDITOR_TEST_H
