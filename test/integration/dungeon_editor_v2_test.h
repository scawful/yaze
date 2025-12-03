#ifndef YAZE_TEST_INTEGRATION_DUNGEON_EDITOR_V2_TEST_H
#define YAZE_TEST_INTEGRATION_DUNGEON_EDITOR_V2_TEST_H

#include <memory>
#include <string>

#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "rom/rom.h"
#include "gtest/gtest.h"
#include "imgui.h"
#include "zelda3/game_data.h"

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
    // Initialize ImGui context for GUI components
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(1920, 1080);
    io.DeltaTime = 1.0f / 60.0f;

    // Build font atlas to satisfy NewFrame
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    // Start a frame so we can call ImGui functions
    ImGui::NewFrame();

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

    // Load Zelda3-specific game data
    game_data_ = std::make_unique<zelda3::GameData>(rom_.get());
    auto load_game_data_status = zelda3::LoadGameData(*rom_, *game_data_);
    ASSERT_TRUE(load_game_data_status.ok())
        << "Failed to load game data: " << load_game_data_status.message();

    // Create V2 editor with ROM and GameData
    dungeon_editor_v2_ = std::make_unique<editor::DungeonEditorV2>(rom_.get());
    dungeon_editor_v2_->SetGameData(game_data_.get());
  }

  void TearDown() override {
    dungeon_editor_v2_.reset();
    game_data_.reset();
    rom_.reset();

    // End frame and cleanup ImGui context
    ImGui::Render();
    ImGui::DestroyContext();
  }

  std::unique_ptr<Rom> rom_;
  std::unique_ptr<zelda3::GameData> game_data_;
  std::unique_ptr<editor::DungeonEditorV2> dungeon_editor_v2_;

  static constexpr int kTestRoomId = 0x01;
};

}  // namespace test
}  // namespace yaze

#endif  // YAZE_TEST_INTEGRATION_DUNGEON_EDITOR_V2_TEST_H
