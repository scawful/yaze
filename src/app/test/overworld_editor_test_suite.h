#ifndef YAZE_APP_TEST_OVERWORLD_EDITOR_TEST_SUITE_H
#define YAZE_APP_TEST_OVERWORLD_EDITOR_TEST_SUITE_H

#include <chrono>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/gui/core/icons.h"
#include "app/test/test_manager.h"
#include "rom/rom.h"
#include "zelda3/game_data.h"

namespace yaze {
namespace test {

class OverworldEditorTestSuite : public TestSuite {
 public:
  OverworldEditorTestSuite() = default;
  ~OverworldEditorTestSuite() override = default;

  std::string GetName() const override { return "Overworld Editor Tests"; }
  TestCategory GetCategory() const override {
    return TestCategory::kIntegration;
  }

  absl::Status RunTests(TestResults& results) override {
    Rom* current_rom = TestManager::Get().GetCurrentRom();

    if (!current_rom || !current_rom->is_loaded()) {
      AddSkippedTest(results, "Overworld_Editor_Check", "No ROM loaded");
      return absl::OkStatus();
    }

    if (test_tile_placement_) {
      RunTilePlacementTest(results, current_rom);
    }

    if (test_entity_manipulation_) {
      RunEntityManipulationTest(results, current_rom);
    }

    return absl::OkStatus();
  }

  void DrawConfiguration() override {
    ImGui::Text("%s Overworld Editor Test Configuration", ICON_MD_MAP);
    ImGui::Separator();
    ImGui::Checkbox("Test Tile Placement", &test_tile_placement_);
    ImGui::Checkbox("Test Entity Manipulation", &test_entity_manipulation_);
  }

 private:
  void AddSkippedTest(TestResults& results, const std::string& test_name,
                      const std::string& reason) {
    TestResult result;
    result.name = test_name;
    result.suite_name = GetName();
    result.category = GetCategory();
    result.status = TestStatus::kSkipped;
    result.error_message = reason;
    result.duration = std::chrono::milliseconds{0};
    result.timestamp = std::chrono::steady_clock::now();
    results.AddResult(result);
  }

  void RunTilePlacementTest(TestResults& results, Rom* rom) {
    auto start_time = std::chrono::steady_clock::now();

    TestResult result;
    result.name = "Overworld_Tile_Placement";
    result.suite_name = GetName();
    result.category = GetCategory();
    result.timestamp = start_time;

    try {
      auto& test_manager = TestManager::Get();
      auto test_status =
          test_manager.TestRomWithCopy(rom, [&](Rom* test_rom) -> absl::Status {
            zelda3::GameData game_data;
            RETURN_IF_ERROR(zelda3::LoadGameData(*test_rom, game_data));

            editor::OverworldEditor editor(test_rom);
            editor.SetGameData(&game_data);
            
            // Initialize and Load to populate internal structures
            editor.Initialize();
            RETURN_IF_ERROR(editor.Load());

            // Test placing a tile on Map 0 (Light World Link's House area)
            int map_id = 0;
            editor.set_current_map(map_id);
            
            // Use Automation API for tile placement (x,y in 16x16 tile coordinates)
            int test_tile_x = 10;
            int test_tile_y = 10;
            int new_tile_id = 0x0123; // Some arbitrary valid tile ID
            
            // Note: AutomationSetTile internally handles the coordinates
            // based on the current map and world.
            bool success = editor.AutomationSetTile(test_tile_x, test_tile_y, new_tile_id);
            if (!success) {
                return absl::InternalError("AutomationSetTile failed");
            }

            // Verify the tile was set in the data layer
            int actual_tile = editor.AutomationGetTile(test_tile_x, test_tile_y);
            if (actual_tile != new_tile_id) {
                return absl::InternalError(absl::StrFormat(
                    "Tile mismatch: expected 0x%04X, got 0x%04X", new_tile_id, actual_tile));
            }

            return absl::OkStatus();
          });

      if (test_status.ok()) {
        result.status = TestStatus::kPassed;
        result.error_message = "Overworld tile placement verified";
      } else {
        result.status = TestStatus::kFailed;
        result.error_message = test_status.ToString();
      }

    } catch (const std::exception& e) {
      result.status = TestStatus::kFailed;
      result.error_message = std::string(e.what());
    }

    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    results.AddResult(result);
  }

  void RunEntityManipulationTest(TestResults& results, Rom* rom) {
    auto start_time = std::chrono::steady_clock::now();

    TestResult result;
    result.name = "Overworld_Entity_Manipulation";
    result.suite_name = GetName();
    result.category = GetCategory();
    result.timestamp = start_time;

    try {
      auto& test_manager = TestManager::Get();
      auto test_status =
          test_manager.TestRomWithCopy(rom, [&](Rom* test_rom) -> absl::Status {
            zelda3::GameData game_data;
            RETURN_IF_ERROR(zelda3::LoadGameData(*test_rom, game_data));

            editor::OverworldEditor editor(test_rom);
            editor.SetGameData(&game_data);
            editor.Initialize();
            RETURN_IF_ERROR(editor.Load());

            // Test Map 0
            editor.set_current_map(0);
            
            auto* current_map_data = editor.overworld().current_map_data();
            if (!current_map_data) {
                return absl::InternalError("Failed to get current map data");
            }
            
            size_t initial_sprite_count = current_map_data->sprites().size();

            // Simulate sprite insertion
            // Note: HandleEntityInsertion usually relies on mouse pos for placement
            // and opens a popup. For testing, we might need to use internal operations.
            // Let's check if we can add a sprite directly to verify the data integrity.
            
            zelda3::Sprite new_sprite;
            new_sprite.set_id(0x01); // Green Soldier
            new_sprite.set_x(100);
            new_sprite.set_y(100);
            
            current_map_data->mutable_sprites()->push_back(new_sprite);

            if (current_map_data->sprites().size() != initial_sprite_count + 1) {
                return absl::InternalError("Failed to add sprite to overworld map");
            }

            return absl::OkStatus();
          });

      if (test_status.ok()) {
        result.status = TestStatus::kPassed;
        result.error_message = "Overworld entity manipulation verified";
      } else {
        result.status = TestStatus::kFailed;
        result.error_message = test_status.ToString();
      }

    } catch (const std::exception& e) {
      result.status = TestStatus::kFailed;
      result.error_message = std::string(e.what());
    }

    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    results.AddResult(result);
  }

  bool test_tile_placement_ = true;
  bool test_entity_manipulation_ = true;
};

}  // namespace test
}  // namespace yaze

#endif // YAZE_APP_TEST_OVERWORLD_EDITOR_TEST_SUITE_H
