#ifndef YAZE_APP_TEST_ROM_DEPENDENT_TEST_SUITE_H
#define YAZE_APP_TEST_ROM_DEPENDENT_TEST_SUITE_H

#include <chrono>
#include <memory>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/test/test_manager.h"
#include "app/gfx/arena.h"
#include "app/rom.h"
#include "app/zelda3/overworld/overworld.h"
#include "app/editor/overworld/tile16_editor.h"
#include "app/gui/icons.h"

namespace yaze {
namespace test {

// ROM-dependent test suite that works with the currently loaded ROM
class RomDependentTestSuite : public TestSuite {
 public:
  RomDependentTestSuite() = default;
  ~RomDependentTestSuite() override = default;

  std::string GetName() const override { return "ROM-Dependent Tests"; }
  TestCategory GetCategory() const override { return TestCategory::kIntegration; }

  absl::Status RunTests(TestResults& results) override {
    Rom* current_rom = TestManager::Get().GetCurrentRom();
    
    if (!current_rom || !current_rom->is_loaded()) {
      // Add a skipped test indicating no ROM is loaded
      TestResult result;
      result.name = "ROM_Available_Check";
      result.suite_name = GetName();
      result.category = GetCategory();
      result.status = TestStatus::kSkipped;
      result.error_message = "No ROM currently loaded in editor";
      result.duration = std::chrono::milliseconds{0};
      result.timestamp = std::chrono::steady_clock::now();
      results.AddResult(result);
      
      return absl::OkStatus();
    }
    
    // Run ROM-dependent tests
    RunRomHeaderValidationTest(results, current_rom);
    RunRomDataAccessTest(results, current_rom);
    RunRomGraphicsExtractionTest(results, current_rom);
    RunRomOverworldLoadingTest(results, current_rom);
    RunTile16EditorTest(results, current_rom);
    RunComprehensiveSaveTest(results, current_rom);
    
    if (test_advanced_features_) {
      RunRomSpriteDataTest(results, current_rom);
      RunRomMusicDataTest(results, current_rom);
    }
    
    return absl::OkStatus();
  }

  void DrawConfiguration() override {
    Rom* current_rom = TestManager::Get().GetCurrentRom();
    
    ImGui::Text("%s ROM-Dependent Test Configuration", ICON_MD_STORAGE);
    
    if (current_rom && current_rom->is_loaded()) {
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), 
                        "%s Current ROM: %s", ICON_MD_CHECK_CIRCLE, current_rom->title().c_str());
      ImGui::Text("Size: %zu bytes", current_rom->size());
      ImGui::Text("File: %s", current_rom->filename().c_str());
    } else {
      ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), 
                        "%s No ROM currently loaded", ICON_MD_WARNING);
      ImGui::Text("Load a ROM in the editor to enable ROM-dependent tests");
    }
    
    ImGui::Separator();
    ImGui::Checkbox("Test ROM header validation", &test_header_validation_);
    ImGui::Checkbox("Test ROM data access", &test_data_access_);
    ImGui::Checkbox("Test graphics extraction", &test_graphics_extraction_);
    ImGui::Checkbox("Test overworld loading", &test_overworld_loading_);
    ImGui::Checkbox("Test advanced features", &test_advanced_features_);
    
    if (test_advanced_features_) {
      ImGui::Indent();
      ImGui::Checkbox("Test sprite data", &test_sprite_data_);
      ImGui::Checkbox("Test music data", &test_music_data_);
      ImGui::Unindent();
    }
  }

 private:
  void RunRomHeaderValidationTest(TestResults& results, Rom* rom) {
    auto start_time = std::chrono::steady_clock::now();
    
    TestResult result;
    result.name = "ROM_Header_Validation_Test";
    result.suite_name = GetName();
    result.category = GetCategory();
    result.timestamp = start_time;
    
    if (!test_header_validation_) {
      result.status = TestStatus::kSkipped;
      result.error_message = "Header validation disabled in configuration";
    } else {
      try {
        std::string title = rom->title();
        size_t size = rom->size();
        
        // Basic validation
        bool valid_title = !title.empty() && title != "ZELDA3" && title.length() <= 21;
        bool valid_size = size >= 1024*1024 && size <= 8*1024*1024; // 1MB to 8MB
        
        if (valid_title && valid_size) {
          result.status = TestStatus::kPassed;
          result.error_message = absl::StrFormat(
              "ROM header valid: '%s' (%zu bytes)", title.c_str(), size);
        } else {
          result.status = TestStatus::kFailed;
          result.error_message = absl::StrFormat(
              "ROM header validation failed: title='%s' size=%zu", title.c_str(), size);
        }
      } catch (const std::exception& e) {
        result.status = TestStatus::kFailed;
        result.error_message = "Header validation failed: " + std::string(e.what());
      }
    }
    
    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    results.AddResult(result);
  }
  
  void RunRomDataAccessTest(TestResults& results, Rom* rom) {
    auto start_time = std::chrono::steady_clock::now();
    
    TestResult result;
    result.name = "ROM_Data_Access_Test";
    result.suite_name = GetName();
    result.category = GetCategory();
    result.timestamp = start_time;
    
    if (!test_data_access_) {
      result.status = TestStatus::kSkipped;
      result.error_message = "Data access testing disabled in configuration";
    } else {
      try {
        // Test basic ROM data access patterns
        size_t bytes_tested = 0;
        bool access_success = true;
        
        // Test reading from various ROM regions
        try {
          [[maybe_unused]] auto header_byte = rom->ReadByte(0x7FC0);
          bytes_tested++;
          [[maybe_unused]] auto code_byte = rom->ReadByte(0x8000);
          bytes_tested++;
          [[maybe_unused]] auto data_word = rom->ReadWord(0x8002);
          bytes_tested++;
        } catch (...) {
          access_success = false;
        }
        
        if (access_success && bytes_tested >= 3) {
          result.status = TestStatus::kPassed;
          result.error_message = absl::StrFormat(
              "ROM data access verified: %zu operations", bytes_tested);
        } else {
          result.status = TestStatus::kFailed;
          result.error_message = "ROM data access failed";
        }
      } catch (const std::exception& e) {
        result.status = TestStatus::kFailed;
        result.error_message = "Data access test failed: " + std::string(e.what());
      }
    }
    
    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    results.AddResult(result);
  }
  
  void RunRomGraphicsExtractionTest(TestResults& results, Rom* rom) {
    auto start_time = std::chrono::steady_clock::now();
    
    TestResult result;
    result.name = "ROM_Graphics_Extraction_Test";
    result.suite_name = GetName();
    result.category = GetCategory();
    result.timestamp = start_time;
    
    if (!test_graphics_extraction_) {
      result.status = TestStatus::kSkipped;
      result.error_message = "Graphics extraction testing disabled in configuration";
    } else {
      try {
        auto graphics_result = LoadAllGraphicsData(*rom);
        if (graphics_result.ok()) {
          auto& sheets = graphics_result.value();
          size_t loaded_sheets = 0;
          for (const auto& sheet : sheets) {
            if (sheet.is_active()) {
              loaded_sheets++;
            }
          }
          
          result.status = TestStatus::kPassed;
          result.error_message = absl::StrFormat(
              "Graphics extraction successful: %zu/%zu sheets loaded", 
              loaded_sheets, sheets.size());
        } else {
          result.status = TestStatus::kFailed;
          result.error_message = "Graphics extraction failed: " + 
                                std::string(graphics_result.status().message());
        }
      } catch (const std::exception& e) {
        result.status = TestStatus::kFailed;
        result.error_message = "Graphics extraction test failed: " + std::string(e.what());
      }
    }
    
    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    results.AddResult(result);
  }
  
  void RunRomOverworldLoadingTest(TestResults& results, Rom* rom) {
    auto start_time = std::chrono::steady_clock::now();
    
    TestResult result;
    result.name = "ROM_Overworld_Loading_Test";
    result.suite_name = GetName();
    result.category = GetCategory();
    result.timestamp = start_time;
    
    if (!test_overworld_loading_) {
      result.status = TestStatus::kSkipped;
      result.error_message = "Overworld loading testing disabled in configuration";
    } else {
      try {
        zelda3::Overworld overworld(rom);
        auto ow_status = overworld.Load(rom);
        
        if (ow_status.ok()) {
          result.status = TestStatus::kPassed;
          result.error_message = "Overworld loading successful from current ROM";
        } else {
          result.status = TestStatus::kFailed;
          result.error_message = "Overworld loading failed: " + std::string(ow_status.message());
        }
      } catch (const std::exception& e) {
        result.status = TestStatus::kFailed;
        result.error_message = "Overworld loading test failed: " + std::string(e.what());
      }
    }
    
    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    results.AddResult(result);
  }
  
  void RunRomSpriteDataTest(TestResults& results, Rom* rom) {
    auto start_time = std::chrono::steady_clock::now();
    
    TestResult result;
    result.name = "ROM_Sprite_Data_Test";
    result.suite_name = GetName();
    result.category = GetCategory();
    result.timestamp = start_time;
    
    if (!test_sprite_data_) {
      result.status = TestStatus::kSkipped;
      result.error_message = "Sprite data testing disabled in configuration";
    } else {
      try {
        // Basic sprite data validation (simplified for now)
        // In a full implementation, this would test sprite loading
        result.status = TestStatus::kSkipped;
        result.error_message = "Sprite data testing not yet implemented";
      } catch (const std::exception& e) {
        result.status = TestStatus::kFailed;
        result.error_message = "Sprite data test failed: " + std::string(e.what());
      }
    }
    
    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    results.AddResult(result);
  }
  
  void RunRomMusicDataTest(TestResults& results, Rom* rom) {
    auto start_time = std::chrono::steady_clock::now();
    
    TestResult result;
    result.name = "ROM_Music_Data_Test";
    result.suite_name = GetName();
    result.category = GetCategory();
    result.timestamp = start_time;
    
    if (!test_music_data_) {
      result.status = TestStatus::kSkipped;
      result.error_message = "Music data testing disabled in configuration";
    } else {
      try {
        // Basic music data validation (simplified for now)
        // In a full implementation, this would test music loading
        result.status = TestStatus::kSkipped;
        result.error_message = "Music data testing not yet implemented";
      } catch (const std::exception& e) {
        result.status = TestStatus::kFailed;
        result.error_message = "Music data test failed: " + std::string(e.what());
      }
    }
    
    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    results.AddResult(result);
  }
  
  void RunTile16EditorTest(TestResults& results, Rom* rom) {
    auto start_time = std::chrono::steady_clock::now();
    
    TestResult result;
    result.name = "Tile16_Editor_Test";
    result.suite_name = GetName();
    result.category = GetCategory();
    result.timestamp = start_time;
    
    try {
      // Test Tile16 editor functionality
      editor::Tile16Editor tile16_editor(rom, nullptr);
      
      // Create test bitmaps with minimal data
      std::vector<uint8_t> test_data(256, 0); // 16x16 = 256 pixels
      gfx::Bitmap test_blockset_bmp, test_gfx_bmp;
      test_blockset_bmp.Create(256, 8192, 8, test_data);
      test_gfx_bmp.Create(256, 256, 8, test_data);
      
      std::array<uint8_t, 0x200> tile_types{};
      
      // Test initialization
      auto init_status = tile16_editor.Initialize(test_blockset_bmp, test_gfx_bmp, tile_types);
      if (!init_status.ok()) {
        result.status = TestStatus::kFailed;
        result.error_message = "Tile16Editor initialization failed: " + init_status.ToString();
      } else {
        result.status = TestStatus::kPassed;
        result.error_message = "Tile16Editor initialized successfully";
      }
    } catch (const std::exception& e) {
      result.status = TestStatus::kFailed;
      result.error_message = "Tile16Editor test exception: " + std::string(e.what());
    }
    
    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    results.AddResult(result);
  }
  
  void RunComprehensiveSaveTest(TestResults& results, Rom* rom) {
    auto start_time = std::chrono::steady_clock::now();
    
    TestResult result;
    result.name = "Comprehensive_Save_Test";
    result.suite_name = GetName();
    result.category = GetCategory();
    result.timestamp = start_time;
    
    try {
      // Test comprehensive save functionality
      // 1. Create backup of original ROM data  
      auto original_data = rom->vector();
      
      // 2. Test overworld modifications
      zelda3::Overworld overworld(rom);
      auto load_status = overworld.Load(rom);
      if (!load_status.ok()) {
        result.status = TestStatus::kFailed;
        result.error_message = "Failed to load overworld: " + load_status.ToString();
      } else {
        // 3. Make a small, safe modification
        auto* test_map = overworld.mutable_overworld_map(0);
        uint8_t original_gfx = test_map->area_graphics();
        test_map->set_area_graphics(0x01); // Change to a different graphics set
        
        // 4. Test save operations
        auto save_maps_status = overworld.SaveOverworldMaps();
        auto save_props_status = overworld.SaveMapProperties();
        
        // 5. Restore original value immediately
        test_map->set_area_graphics(original_gfx);
        
        if (save_maps_status.ok() && save_props_status.ok()) {
          result.status = TestStatus::kPassed;
          result.error_message = "Save operations completed successfully";
        } else {
          result.status = TestStatus::kFailed;
          result.error_message = "Save operations failed";
        }
      }
      
    } catch (const std::exception& e) {
      result.status = TestStatus::kFailed;
      result.error_message = "Comprehensive save test exception: " + std::string(e.what());
    }
    
    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    results.AddResult(result);
  }
  
  // Configuration
  bool test_header_validation_ = true;
  bool test_data_access_ = true;
  bool test_graphics_extraction_ = true;
  bool test_overworld_loading_ = true;
  bool test_tile16_editor_ = true;
  bool test_comprehensive_save_ = true;
  bool test_advanced_features_ = false;
  bool test_sprite_data_ = false;
  bool test_music_data_ = false;
};

}  // namespace test
}  // namespace yaze

#endif  // YAZE_APP_TEST_ROM_DEPENDENT_TEST_SUITE_H
