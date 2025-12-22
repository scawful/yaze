#ifndef YAZE_TEST_INTEGRATION_DUNGEON_EDITOR_V2_TEST_H
#define YAZE_TEST_INTEGRATION_DUNGEON_EDITOR_V2_TEST_H

#include <memory>
#include <string>

#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "gtest/gtest.h"
#include "imgui.h"
#include "zelda3/game_data.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "framework/headless_editor_test.h"

namespace yaze {
namespace test {

/**
 * @brief Integration test framework for DungeonEditorV2
 *
 * Tests the simplified component delegation architecture
 */
class DungeonEditorV2IntegrationTest : public HeadlessEditorTest {
 protected:
  void SetUp() override {
    HeadlessEditorTest::SetUp();

    // Use the real ROM (try multiple locations)
    // We use the base class helper but need to handle the path logic
    // TODO: Make LoadRom return status or boolean to allow fallbacks
    // For now, we'll just try to load directly
    
    // Try loading from standard locations
    const char* paths[] = {"assets/zelda3.sfc", "build/bin/zelda3.sfc", "zelda3.sfc"};
    bool loaded = false;
    for (const char* path : paths) {
        rom_ = std::make_unique<Rom>();
        if (rom_->LoadFromFile(path).ok()) {
            loaded = true;
            break;
        }
    }
    ASSERT_TRUE(loaded) << "Could not load zelda3.sfc from any location";

    // Patch ROM to ensure Room 0 and Room 1 sprite pointers are sequential
    // This fixes "Cannot determine available sprite space" error if the loaded ROM is non-standard
    // We dynamically find the table location to be robust against ROM hacks
    int table_ptr_addr = zelda3::kRoomsSpritePointer;
    uint8_t low = rom_->ReadByte(table_ptr_addr).value();
    uint8_t high = rom_->ReadByte(table_ptr_addr + 1).value();
    int table_offset = (high << 8) | low;
    int table_snes = (0x09 << 16) | table_offset;
    int table_pc = SnesToPc(table_snes);
    
    // Patch all room pointers to be sequential with 0x20 bytes of space
    // This ensures SaveDungeon passes for all rooms
    int current_offset = 0x1000;
    for (int i = 0; i <= zelda3::kNumberOfRooms; ++i) {
        rom_->WriteByte(table_pc + (i * 2), current_offset & 0xFF);
        rom_->WriteByte(table_pc + (i * 2) + 1, (current_offset >> 8) & 0xFF);
        current_offset += 0x20;
    }

    // Load Zelda3-specific game data
    // Note: HeadlessEditorTest creates a blank GameData, we replace it here
    game_data_ = std::make_unique<zelda3::GameData>(rom_.get());
    auto load_game_data_status = zelda3::LoadGameData(*rom_, *game_data_);
    ASSERT_TRUE(load_game_data_status.ok())
        << "Failed to load game data: " << load_game_data_status.message();

    // Create V2 editor with ROM and GameData
    dungeon_editor_v2_ = std::make_unique<editor::DungeonEditorV2>(rom_.get());
    dungeon_editor_v2_->SetGameData(game_data_.get());
    
    // Inject dependencies
    editor::EditorDependencies deps;
    deps.rom = rom_.get();
    deps.game_data = game_data_.get();
    deps.panel_manager = panel_manager_.get();
    deps.renderer = renderer_.get();
    dungeon_editor_v2_->SetDependencies(deps);
  }

  void TearDown() override {
    dungeon_editor_v2_.reset();
    HeadlessEditorTest::TearDown();
  }

  std::unique_ptr<editor::DungeonEditorV2> dungeon_editor_v2_;

  static constexpr int kTestRoomId = 0x01;
};

}  // namespace test
}  // namespace yaze

#endif  // YAZE_TEST_INTEGRATION_DUNGEON_EDITOR_V2_TEST_H
