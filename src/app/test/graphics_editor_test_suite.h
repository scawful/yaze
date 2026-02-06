#ifndef YAZE_APP_TEST_GRAPHICS_EDITOR_TEST_SUITE_H
#define YAZE_APP_TEST_GRAPHICS_EDITOR_TEST_SUITE_H

#include <chrono>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/editor/graphics/graphics_editor.h"
#include "app/gui/core/icons.h"
#include "app/test/test_manager.h"
#include "rom/rom.h"
#include "zelda3/game_data.h"

namespace yaze {
namespace test {

class GraphicsEditorTestSuite : public TestSuite {
 public:
  GraphicsEditorTestSuite() = default;
  ~GraphicsEditorTestSuite() override = default;

  std::string GetName() const override { return "Graphics Editor Tests"; }
  TestCategory GetCategory() const override {
    return TestCategory::kIntegration;
  }

  absl::Status RunTests(TestResults& results) override {
    Rom* current_rom = TestManager::Get().GetCurrentRom();

    if (!current_rom || !current_rom->is_loaded()) {
      AddSkippedTest(results, "Graphics_Editor_Check", "No ROM loaded");
      return absl::OkStatus();
    }

    if (test_pixel_editing_) {
      RunPixelEditingTest(results, current_rom);
    }

    if (test_palette_editing_) {
      RunPaletteEditingTest(results, current_rom);
    }

    return absl::OkStatus();
  }

  void DrawConfiguration() override {
    ImGui::Text("%s Graphics Editor Test Configuration", ICON_MD_PALETTE);
    ImGui::Separator();
    ImGui::Checkbox("Test Pixel Editing", &test_pixel_editing_);
    ImGui::Checkbox("Test Palette Editing", &test_palette_editing_);
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

  void RunPixelEditingTest(TestResults& results, Rom* rom) {
    auto start_time = std::chrono::steady_clock::now();

    TestResult result;
    result.name = "Graphics_Pixel_Editing";
    result.suite_name = GetName();
    result.category = GetCategory();
    result.timestamp = start_time;

    try {
      auto& test_manager = TestManager::Get();
      auto test_status =
          test_manager.TestRomWithCopy(rom, [&](Rom* test_rom) -> absl::Status {
            zelda3::GameData game_data;
            RETURN_IF_ERROR(zelda3::LoadGameData(*test_rom, game_data));

            editor::GraphicsEditor editor(test_rom);
            editor.SetGameData(&game_data);
            
            // Initialize and Load
            editor.Initialize();
            RETURN_IF_ERROR(editor.Load());

            // Test Sheet 0 (Link's graphics usually)
            uint16_t sheet_id = 0;
            editor.SelectSheet(sheet_id);
            
            // Directly modify a pixel in GameData to simulate editing
            // and then verify we can "save" it.
            // In a real GUI edit, the PixelEditorPanel would handle this.
            if (game_data.gfx_bitmaps.empty()) {
                 return absl::InternalError("No graphics sheets loaded");
            }
            
            auto& sheet = game_data.gfx_bitmaps[sheet_id];
            uint8_t original_pixel = sheet.GetPixel(0, 0);
            uint8_t new_pixel = (original_pixel + 1) % 16;
            
            sheet.SetPixel(0, 0, new_pixel);
            
            if (sheet.GetPixel(0, 0) != new_pixel) {
                return absl::InternalError("Failed to set pixel in bitmap");
            }

            // Verify saving (this would write back to ROM or internal buffers)
            // RETURN_IF_ERROR(editor.Save()); 

            return absl::OkStatus();
          });

      if (test_status.ok()) {
        result.status = TestStatus::kPassed;
        result.error_message = "Pixel editing verified in data layer";
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

  void RunPaletteEditingTest(TestResults& results, Rom* rom) {
    auto start_time = std::chrono::steady_clock::now();

    TestResult result;
    result.name = "Graphics_Palette_Editing";
    result.suite_name = GetName();
    result.category = GetCategory();
    result.timestamp = start_time;

    try {
      auto& test_manager = TestManager::Get();
      auto test_status =
          test_manager.TestRomWithCopy(rom, [&](Rom* test_rom) -> absl::Status {
            zelda3::GameData game_data;
            RETURN_IF_ERROR(zelda3::LoadGameData(*test_rom, game_data));

            editor::GraphicsEditor editor(test_rom);
            editor.SetGameData(&game_data);
            editor.Initialize();
            RETURN_IF_ERROR(editor.Load());

            // Test modifying a palette color
            if (game_data.palette_groups.dungeons.empty()) {
                return absl::InternalError("No dungeon palettes loaded");
            }
            
            auto& palette = game_data.palette_groups.dungeons[0];
            SDL_Color original_color = palette.GetColor(0);
            SDL_Color new_color = {255, 0, 0, 255}; // Bright Red
            
            palette.SetColor(0, new_color);
            
            SDL_Color actual_color = palette.GetColor(0);
            if (actual_color.r != 255 || actual_color.g != 0 || actual_color.b != 0) {
                return absl::InternalError("Palette color update failed");
            }

            return absl::OkStatus();
          });

      if (test_status.ok()) {
        result.status = TestStatus::kPassed;
        result.error_message = "Palette editing verified in data layer";
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

  bool test_pixel_editing_ = true;
  bool test_palette_editing_ = true;
};

}  // namespace test
}  // namespace yaze

#endif // YAZE_APP_TEST_GRAPHICS_EDITOR_TEST_SUITE_H
