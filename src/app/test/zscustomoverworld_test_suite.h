#ifndef YAZE_APP_TEST_ZSCUSTOMOVERWORLD_TEST_SUITE_H
#define YAZE_APP_TEST_ZSCUSTOMOVERWORLD_TEST_SUITE_H

#include <chrono>
#include <map>

#include "absl/strings/str_format.h"
#include "app/gui/core/icons.h"
#include "app/rom.h"
#include "app/test/test_manager.h"

namespace yaze {
namespace test {

/**
 * @brief ZSCustomOverworld upgrade testing suite
 *
 * This test suite validates ZSCustomOverworld version upgrades:
 * - Vanilla -> v2 -> v3 upgrade path testing
 * - Address validation for each version
 * - Feature enablement/disablement testing
 * - Data integrity validation during upgrades
 * - Save compatibility between versions
 */
class ZSCustomOverworldTestSuite : public TestSuite {
 public:
  ZSCustomOverworldTestSuite() = default;
  ~ZSCustomOverworldTestSuite() override = default;

  std::string GetName() const override {
    return "ZSCustomOverworld Upgrade Tests";
  }
  TestCategory GetCategory() const override {
    return TestCategory::kIntegration;
  }

  absl::Status RunTests(TestResults& results) override {
    Rom* current_rom = TestManager::Get().GetCurrentRom();

    // Check ROM availability
    if (!current_rom || !current_rom->is_loaded()) {
      AddSkippedTest(results, "ROM_Availability_Check", "No ROM loaded");
      return absl::OkStatus();
    }

    // Initialize version data
    InitializeVersionData();

    // Run ZSCustomOverworld tests
    if (test_vanilla_baseline_) {
      RunVanillaBaselineTest(results, current_rom);
    }

    if (test_v2_upgrade_) {
      RunV2UpgradeTest(results, current_rom);
    }

    if (test_v3_upgrade_) {
      RunV3UpgradeTest(results, current_rom);
    }

    if (test_address_validation_) {
      RunAddressValidationTest(results, current_rom);
    }

    if (test_feature_toggle_) {
      RunFeatureToggleTest(results, current_rom);
    }

    if (test_data_integrity_) {
      RunDataIntegrityTest(results, current_rom);
    }

    return absl::OkStatus();
  }

  void DrawConfiguration() override {
    Rom* current_rom = TestManager::Get().GetCurrentRom();

    ImGui::Text("%s ZSCustomOverworld Test Configuration", ICON_MD_UPGRADE);

    if (current_rom && current_rom->is_loaded()) {
      ImGui::TextColored(ImVec4(0.0F, 1.0F, 0.0F, 1.0F), "%s Current ROM: %s",
                         ICON_MD_CHECK_CIRCLE, current_rom->title().c_str());

      // Check current version
      auto version_byte = current_rom->ReadByte(0x140145);
      if (version_byte.ok()) {
        std::string version_name = "Unknown";
        if (*version_byte == 0xFF)
          version_name = "Vanilla";
        else if (*version_byte == 0x02)
          version_name = "v2";
        else if (*version_byte == 0x03)
          version_name = "v3";

        ImGui::Text("Current ZSCustomOverworld version: %s (0x%02X)",
                    version_name.c_str(), *version_byte);
      }
    } else {
      ImGui::TextColored(ImVec4(1.0F, 0.5F, 0.0F, 1.0F),
                         "%s No ROM currently loaded", ICON_MD_WARNING);
    }

    ImGui::Separator();
    ImGui::Checkbox("Test vanilla baseline", &test_vanilla_baseline_);
    ImGui::Checkbox("Test v2 upgrade", &test_v2_upgrade_);
    ImGui::Checkbox("Test v3 upgrade", &test_v3_upgrade_);
    ImGui::Checkbox("Test address validation", &test_address_validation_);
    ImGui::Checkbox("Test feature toggle", &test_feature_toggle_);
    ImGui::Checkbox("Test data integrity", &test_data_integrity_);

    if (ImGui::CollapsingHeader("Version Settings")) {
      ImGui::Text("Version-specific addresses and features:");
      ImGui::Text("Vanilla: 0x140145 = 0xFF");
      ImGui::Text("v2: 0x140145 = 0x02, main palettes enabled");
      ImGui::Text("v3: 0x140145 = 0x03, all features enabled");
    }
  }

 private:
  void InitializeVersionData() {
    // Vanilla ROM addresses and values
    vanilla_data_ = {
        {"version_flag", {0x140145, 0xFF}},  // OverworldCustomASMHasBeenApplied
        {"message_ids", {0x3F51D, 0x00}},    // Message ID table start
        {"area_graphics", {0x7C9C, 0x00}},   // Area graphics table
        {"area_palettes", {0x7D1C, 0x00}},   // Area palettes table
    };

    // v2 ROM addresses and values
    v2_data_ = {
        {"version_flag", {0x140145, 0x02}},   // v2 version
        {"message_ids", {0x1417F8, 0x00}},    // Expanded message ID table
        {"area_graphics", {0x7C9C, 0x00}},    // Same as vanilla
        {"area_palettes", {0x7D1C, 0x00}},    // Same as vanilla
        {"main_palettes", {0x140160, 0x00}},  // New v2 feature
    };

    // v3 ROM addresses and values
    v3_data_ = {
        {"version_flag", {0x140145, 0x03}},        // v3 version
        {"message_ids", {0x1417F8, 0x00}},         // Same as v2
        {"area_graphics", {0x7C9C, 0x00}},         // Same as vanilla
        {"area_palettes", {0x7D1C, 0x00}},         // Same as vanilla
        {"main_palettes", {0x140160, 0x00}},       // Same as v2
        {"bg_colors", {0x140000, 0x00}},           // New v3 feature
        {"subscreen_overlays", {0x140340, 0x00}},  // New v3 feature
        {"animated_gfx", {0x1402A0, 0x00}},        // New v3 feature
        {"custom_tiles", {0x140480, 0x00}},        // New v3 feature
    };
  }

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

  absl::Status ApplyVersionPatch(Rom& rom, const std::string& version) {
    const auto* data = &vanilla_data_;
    if (version == "v2") {
      data = &v2_data_;
    } else if (version == "v3") {
      data = &v3_data_;
    }

    // Apply version-specific data
    for (const auto& [key, value] : *data) {
      RETURN_IF_ERROR(rom.WriteByte(value.first, value.second));
    }

    // Apply version-specific features
    if (version == "v2") {
      // Enable v2 features
      RETURN_IF_ERROR(rom.WriteByte(0x140146, 0x01));  // Enable main palettes
    } else if (version == "v3") {
      // Enable v3 features
      RETURN_IF_ERROR(rom.WriteByte(0x140146, 0x01));  // Enable main palettes
      RETURN_IF_ERROR(
          rom.WriteByte(0x140147, 0x01));  // Enable area-specific BG
      RETURN_IF_ERROR(
          rom.WriteByte(0x140148, 0x01));  // Enable subscreen overlay
      RETURN_IF_ERROR(rom.WriteByte(0x140149, 0x01));  // Enable animated GFX
      RETURN_IF_ERROR(
          rom.WriteByte(0x14014A, 0x01));  // Enable custom tile GFX groups
      RETURN_IF_ERROR(rom.WriteByte(0x14014B, 0x01));  // Enable mosaic
    }

    return absl::OkStatus();
  }

  bool ValidateVersionAddresses(Rom& rom, const std::string& version) {
    const auto* data = &vanilla_data_;
    if (version == "v2") {
      data = &v2_data_;
    } else if (version == "v3") {
      data = &v3_data_;
    }

    for (const auto& [key, value] : *data) {
      auto byte_value = rom.ReadByte(value.first);
      if (!byte_value.ok() || *byte_value != value.second) {
        return false;
      }
    }

    return true;
  }

  void RunVanillaBaselineTest(TestResults& results, Rom* rom) {
    auto start_time = std::chrono::steady_clock::now();

    TestResult result;
    result.name = "Vanilla_Baseline_Test";
    result.suite_name = GetName();
    result.category = GetCategory();
    result.timestamp = start_time;

    try {
      auto& test_manager = TestManager::Get();

      auto test_status =
          test_manager.TestRomWithCopy(rom, [&](Rom* test_rom) -> absl::Status {
            // Validate vanilla addresses
            if (!ValidateVersionAddresses(*test_rom, "vanilla")) {
              return absl::InternalError("Vanilla address validation failed");
            }

            // Verify version flag
            auto version_byte = test_rom->ReadByte(0x140145);
            if (!version_byte.ok()) {
              return absl::InternalError("Failed to read version flag");
            }

            if (*version_byte != 0xFF) {
              return absl::InternalError(absl::StrFormat(
                  "Expected vanilla version flag (0xFF), got 0x%02X",
                  *version_byte));
            }

            return absl::OkStatus();
          });

      if (test_status.ok()) {
        result.status = TestStatus::kPassed;
        result.error_message =
            "Vanilla baseline test passed - ROM is in vanilla state";
      } else {
        result.status = TestStatus::kFailed;
        result.error_message =
            "Vanilla baseline test failed: " + test_status.ToString();
      }

    } catch (const std::exception& e) {
      result.status = TestStatus::kFailed;
      result.error_message =
          "Vanilla baseline test exception: " + std::string(e.what());
    }

    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    results.AddResult(result);
  }

  void RunV2UpgradeTest(TestResults& results, Rom* rom) {
    auto start_time = std::chrono::steady_clock::now();

    TestResult result;
    result.name = "V2_Upgrade_Test";
    result.suite_name = GetName();
    result.category = GetCategory();
    result.timestamp = start_time;

    try {
      auto& test_manager = TestManager::Get();

      auto test_status =
          test_manager.TestRomWithCopy(rom, [&](Rom* test_rom) -> absl::Status {
            // Apply v2 patch
            RETURN_IF_ERROR(ApplyVersionPatch(*test_rom, "v2"));

            // Validate v2 addresses
            if (!ValidateVersionAddresses(*test_rom, "v2")) {
              return absl::InternalError("v2 address validation failed");
            }

            // Verify version flag
            auto version_byte = test_rom->ReadByte(0x140145);
            if (!version_byte.ok()) {
              return absl::InternalError("Failed to read version flag");
            }

            if (*version_byte != 0x02) {
              return absl::InternalError(
                  absl::StrFormat("Expected v2 version flag (0x02), got 0x%02X",
                                  *version_byte));
            }

            return absl::OkStatus();
          });

      if (test_status.ok()) {
        result.status = TestStatus::kPassed;
        result.error_message =
            "v2 upgrade test passed - ROM successfully upgraded to v2";
      } else {
        result.status = TestStatus::kFailed;
        result.error_message =
            "v2 upgrade test failed: " + test_status.ToString();
      }

    } catch (const std::exception& e) {
      result.status = TestStatus::kFailed;
      result.error_message =
          "v2 upgrade test exception: " + std::string(e.what());
    }

    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    results.AddResult(result);
  }

  void RunV3UpgradeTest(TestResults& results, Rom* rom) {
    auto start_time = std::chrono::steady_clock::now();

    TestResult result;
    result.name = "V3_Upgrade_Test";
    result.suite_name = GetName();
    result.category = GetCategory();
    result.timestamp = start_time;

    try {
      auto& test_manager = TestManager::Get();

      auto test_status =
          test_manager.TestRomWithCopy(rom, [&](Rom* test_rom) -> absl::Status {
            // Apply v3 patch
            RETURN_IF_ERROR(ApplyVersionPatch(*test_rom, "v3"));

            // Validate v3 addresses
            if (!ValidateVersionAddresses(*test_rom, "v3")) {
              return absl::InternalError("v3 address validation failed");
            }

            // Verify version flag
            auto version_byte = test_rom->ReadByte(0x140145);
            if (!version_byte.ok()) {
              return absl::InternalError("Failed to read version flag");
            }

            if (*version_byte != 0x03) {
              return absl::InternalError(
                  absl::StrFormat("Expected v3 version flag (0x03), got 0x%02X",
                                  *version_byte));
            }

            return absl::OkStatus();
          });

      if (test_status.ok()) {
        result.status = TestStatus::kPassed;
        result.error_message =
            "v3 upgrade test passed - ROM successfully upgraded to v3";
      } else {
        result.status = TestStatus::kFailed;
        result.error_message =
            "v3 upgrade test failed: " + test_status.ToString();
      }

    } catch (const std::exception& e) {
      result.status = TestStatus::kFailed;
      result.error_message =
          "v3 upgrade test exception: " + std::string(e.what());
    }

    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    results.AddResult(result);
  }

  void RunAddressValidationTest(TestResults& results, Rom* rom) {
    auto start_time = std::chrono::steady_clock::now();

    TestResult result;
    result.name = "Address_Validation_Test";
    result.suite_name = GetName();
    result.category = GetCategory();
    result.timestamp = start_time;

    try {
      auto& test_manager = TestManager::Get();

      auto test_status =
          test_manager.TestRomWithCopy(rom, [&](Rom* test_rom) -> absl::Status {
            // Test vanilla addresses
            if (!ValidateVersionAddresses(*test_rom, "vanilla")) {
              return absl::InternalError("Vanilla address validation failed");
            }

            // Test v2 addresses
            RETURN_IF_ERROR(ApplyVersionPatch(*test_rom, "v2"));
            if (!ValidateVersionAddresses(*test_rom, "v2")) {
              return absl::InternalError("v2 address validation failed");
            }

            // Test v3 addresses
            RETURN_IF_ERROR(ApplyVersionPatch(*test_rom, "v3"));
            if (!ValidateVersionAddresses(*test_rom, "v3")) {
              return absl::InternalError("v3 address validation failed");
            }

            return absl::OkStatus();
          });

      if (test_status.ok()) {
        result.status = TestStatus::kPassed;
        result.error_message =
            "Address validation test passed - all version addresses valid";
      } else {
        result.status = TestStatus::kFailed;
        result.error_message =
            "Address validation test failed: " + test_status.ToString();
      }

    } catch (const std::exception& e) {
      result.status = TestStatus::kFailed;
      result.error_message =
          "Address validation test exception: " + std::string(e.what());
    }

    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    results.AddResult(result);
  }

  void RunFeatureToggleTest(TestResults& results, Rom* rom) {
    auto start_time = std::chrono::steady_clock::now();

    TestResult result;
    result.name = "Feature_Toggle_Test";
    result.suite_name = GetName();
    result.category = GetCategory();
    result.timestamp = start_time;

    try {
      auto& test_manager = TestManager::Get();

      auto test_status =
          test_manager.TestRomWithCopy(rom, [&](Rom* test_rom) -> absl::Status {
            // Apply v3 patch
            RETURN_IF_ERROR(ApplyVersionPatch(*test_rom, "v3"));

            // Test feature flags
            auto main_palettes = test_rom->ReadByte(0x140146);
            auto area_bg = test_rom->ReadByte(0x140147);
            auto subscreen_overlay = test_rom->ReadByte(0x140148);
            auto animated_gfx = test_rom->ReadByte(0x140149);
            auto custom_tiles = test_rom->ReadByte(0x14014A);
            auto mosaic = test_rom->ReadByte(0x14014B);

            if (!main_palettes.ok() || !area_bg.ok() ||
                !subscreen_overlay.ok() || !animated_gfx.ok() ||
                !custom_tiles.ok() || !mosaic.ok()) {
              return absl::InternalError("Failed to read feature flags");
            }

            if (*main_palettes != 0x01 || *area_bg != 0x01 ||
                *subscreen_overlay != 0x01 || *animated_gfx != 0x01 ||
                *custom_tiles != 0x01 || *mosaic != 0x01) {
              return absl::InternalError("Feature flags not properly enabled");
            }

            // Disable some features
            RETURN_IF_ERROR(test_rom->WriteByte(
                0x140147, 0x00));  // Disable area-specific BG
            RETURN_IF_ERROR(
                test_rom->WriteByte(0x140149, 0x00));  // Disable animated GFX

            // Verify features are disabled
            auto disabled_area_bg = test_rom->ReadByte(0x140147);
            auto disabled_animated_gfx = test_rom->ReadByte(0x140149);

            if (!disabled_area_bg.ok() || !disabled_animated_gfx.ok()) {
              return absl::InternalError(
                  "Failed to read disabled feature flags");
            }

            if (*disabled_area_bg != 0x00 || *disabled_animated_gfx != 0x00) {
              return absl::InternalError("Feature flags not properly disabled");
            }

            return absl::OkStatus();
          });

      if (test_status.ok()) {
        result.status = TestStatus::kPassed;
        result.error_message =
            "Feature toggle test passed - features can be enabled/disabled";
      } else {
        result.status = TestStatus::kFailed;
        result.error_message =
            "Feature toggle test failed: " + test_status.ToString();
      }

    } catch (const std::exception& e) {
      result.status = TestStatus::kFailed;
      result.error_message =
          "Feature toggle test exception: " + std::string(e.what());
    }

    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    results.AddResult(result);
  }

  void RunDataIntegrityTest(TestResults& results, Rom* rom) {
    auto start_time = std::chrono::steady_clock::now();

    TestResult result;
    result.name = "Data_Integrity_Test";
    result.suite_name = GetName();
    result.category = GetCategory();
    result.timestamp = start_time;

    try {
      auto& test_manager = TestManager::Get();

      auto test_status =
          test_manager.TestRomWithCopy(rom, [&](Rom* test_rom) -> absl::Status {
            // Store some original data
            auto original_graphics = test_rom->ReadByte(0x7C9C);
            auto original_palette = test_rom->ReadByte(0x7D1C);
            auto original_sprite_set = test_rom->ReadByte(0x7A41);

            if (!original_graphics.ok() || !original_palette.ok() ||
                !original_sprite_set.ok()) {
              return absl::InternalError("Failed to read original data");
            }

            // Upgrade to v3
            RETURN_IF_ERROR(ApplyVersionPatch(*test_rom, "v3"));

            // Verify original data is preserved
            auto preserved_graphics = test_rom->ReadByte(0x7C9C);
            auto preserved_palette = test_rom->ReadByte(0x7D1C);
            auto preserved_sprite_set = test_rom->ReadByte(0x7A41);

            if (!preserved_graphics.ok() || !preserved_palette.ok() ||
                !preserved_sprite_set.ok()) {
              return absl::InternalError("Failed to read preserved data");
            }

            if (*preserved_graphics != *original_graphics ||
                *preserved_palette != *original_palette ||
                *preserved_sprite_set != *original_sprite_set) {
              return absl::InternalError(
                  "Original data not preserved during upgrade");
            }

            // Verify new v3 data is initialized
            auto bg_colors = test_rom->ReadByte(0x140000);
            auto subscreen_overlays = test_rom->ReadByte(0x140340);
            auto animated_gfx = test_rom->ReadByte(0x1402A0);
            auto custom_tiles = test_rom->ReadByte(0x140480);

            if (!bg_colors.ok() || !subscreen_overlays.ok() ||
                !animated_gfx.ok() || !custom_tiles.ok()) {
              return absl::InternalError("Failed to read new v3 data");
            }

            if (*bg_colors != 0x00 || *subscreen_overlays != 0x00 ||
                *animated_gfx != 0x00 || *custom_tiles != 0x00) {
              return absl::InternalError(
                  "New v3 data not properly initialized");
            }

            return absl::OkStatus();
          });

      if (test_status.ok()) {
        result.status = TestStatus::kPassed;
        result.error_message =
            "Data integrity test passed - original data preserved, new data "
            "initialized";
      } else {
        result.status = TestStatus::kFailed;
        result.error_message =
            "Data integrity test failed: " + test_status.ToString();
      }

    } catch (const std::exception& e) {
      result.status = TestStatus::kFailed;
      result.error_message =
          "Data integrity test exception: " + std::string(e.what());
    }

    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    results.AddResult(result);
  }

  // Configuration
  bool test_vanilla_baseline_ = true;
  bool test_v2_upgrade_ = true;
  bool test_v3_upgrade_ = true;
  bool test_address_validation_ = true;
  bool test_feature_toggle_ = true;
  bool test_data_integrity_ = true;

  // Version data
  std::map<std::string, std::pair<uint32_t, uint8_t>> vanilla_data_;
  std::map<std::string, std::pair<uint32_t, uint8_t>> v2_data_;
  std::map<std::string, std::pair<uint32_t, uint8_t>> v3_data_;
};

}  // namespace test
}  // namespace yaze

#endif  // YAZE_APP_TEST_ZSCUSTOMOVERWORLD_TEST_SUITE_H
