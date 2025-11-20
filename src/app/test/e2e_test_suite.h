#ifndef YAZE_APP_TEST_E2E_TEST_SUITE_H
#define YAZE_APP_TEST_E2E_TEST_SUITE_H

#include <chrono>
#include <filesystem>

#include "absl/strings/str_format.h"
#include "app/gui/core/icons.h"
#include "app/rom.h"
#include "app/test/test_manager.h"
#include "app/transaction.h"

namespace yaze {
namespace test {

/**
 * @brief End-to-End test suite for comprehensive ROM testing
 *
 * This test suite provides comprehensive E2E testing capabilities including:
 * - ROM loading/saving validation
 * - Data integrity testing
 * - Transaction system testing
 * - Large-scale editing validation
 */
class E2ETestSuite : public TestSuite {
 public:
  E2ETestSuite() = default;
  ~E2ETestSuite() override = default;

  std::string GetName() const override { return "End-to-End ROM Tests"; }
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

    // Run E2E tests
    if (test_rom_load_save_) {
      RunRomLoadSaveTest(results, current_rom);
    }

    if (test_data_integrity_) {
      RunDataIntegrityTest(results, current_rom);
    }

    if (test_transaction_system_) {
      RunTransactionSystemTest(results, current_rom);
    }

    if (test_large_scale_editing_) {
      RunLargeScaleEditingTest(results, current_rom);
    }

    if (test_corruption_detection_) {
      RunCorruptionDetectionTest(results, current_rom);
    }

    return absl::OkStatus();
  }

  void DrawConfiguration() override {
    Rom* current_rom = TestManager::Get().GetCurrentRom();

    ImGui::Text("%s E2E Test Configuration", ICON_MD_VERIFIED_USER);

    if (current_rom && current_rom->is_loaded()) {
      ImGui::TextColored(ImVec4(0.0F, 1.0F, 0.0F, 1.0F), "%s Current ROM: %s",
                         ICON_MD_CHECK_CIRCLE, current_rom->title().c_str());
      ImGui::Text("Size: %.2F MB", current_rom->size() / 1048576.0F);
    } else {
      ImGui::TextColored(ImVec4(1.0F, 0.5F, 0.0F, 1.0F),
                         "%s No ROM currently loaded", ICON_MD_WARNING);
    }

    ImGui::Separator();
    ImGui::Checkbox("Test ROM load/save", &test_rom_load_save_);
    ImGui::Checkbox("Test data integrity", &test_data_integrity_);
    ImGui::Checkbox("Test transaction system", &test_transaction_system_);
    ImGui::Checkbox("Test large-scale editing", &test_large_scale_editing_);
    ImGui::Checkbox("Test corruption detection", &test_corruption_detection_);

    if (test_large_scale_editing_) {
      ImGui::Indent();
      ImGui::InputInt("Number of edits", &num_edits_);
      if (num_edits_ < 1) num_edits_ = 1;
      if (num_edits_ > 100) num_edits_ = 100;
      ImGui::Unindent();
    }
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

  void RunRomLoadSaveTest(TestResults& results, Rom* rom) {
    auto start_time = std::chrono::steady_clock::now();

    TestResult result;
    result.name = "ROM_Load_Save_Test";
    result.suite_name = GetName();
    result.category = GetCategory();
    result.timestamp = start_time;

    try {
      auto& test_manager = TestManager::Get();

      auto test_status =
          test_manager.TestRomWithCopy(rom, [&](Rom* test_rom) -> absl::Status {
            // Test basic ROM operations
            if (test_rom->size() != rom->size()) {
              return absl::InternalError("ROM copy size mismatch");
            }

            // Test save and reload
            std::string test_filename =
                test_manager.GenerateTestRomFilename("e2e_test");
            auto save_status = test_rom->SaveToFile(
                Rom::SaveSettings{.filename = test_filename});
            if (!save_status.ok()) {
              return save_status;
            }

            // Clean up test file
            std::filesystem::remove(test_filename);

            return absl::OkStatus();
          });

      if (test_status.ok()) {
        result.status = TestStatus::kPassed;
        result.error_message = "ROM load/save test completed successfully";
      } else {
        result.status = TestStatus::kFailed;
        result.error_message =
            "ROM load/save test failed: " + test_status.ToString();
      }

    } catch (const std::exception& e) {
      result.status = TestStatus::kFailed;
      result.error_message =
          "ROM load/save test exception: " + std::string(e.what());
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
            // Test data integrity by comparing key areas
            std::vector<uint32_t> test_addresses = {0x7FC0, 0x8000, 0x10000,
                                                    0x20000};

            for (uint32_t addr : test_addresses) {
              auto original_byte = rom->ReadByte(addr);
              auto copy_byte = test_rom->ReadByte(addr);

              if (!original_byte.ok() || !copy_byte.ok()) {
                return absl::InternalError(
                    "Failed to read ROM data for comparison");
              }

              if (*original_byte != *copy_byte) {
                return absl::InternalError(
                    absl::StrFormat("Data integrity check failed at address "
                                    "0x%X: original=0x%02X, copy=0x%02X",
                                    addr, *original_byte, *copy_byte));
              }
            }

            return absl::OkStatus();
          });

      if (test_status.ok()) {
        result.status = TestStatus::kPassed;
        result.error_message =
            "Data integrity test passed - all checked addresses match";
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

  void RunTransactionSystemTest(TestResults& results, Rom* rom) {
    auto start_time = std::chrono::steady_clock::now();

    TestResult result;
    result.name = "Transaction_System_Test";
    result.suite_name = GetName();
    result.category = GetCategory();
    result.timestamp = start_time;

    try {
      auto& test_manager = TestManager::Get();

      auto test_status =
          test_manager.TestRomWithCopy(rom, [&](Rom* test_rom) -> absl::Status {
            // Test transaction system
            Transaction transaction(*test_rom);

            // Store original values
            auto original_byte1 = test_rom->ReadByte(0x1000);
            auto original_byte2 = test_rom->ReadByte(0x2000);
            auto original_word = test_rom->ReadWord(0x3000);

            if (!original_byte1.ok() || !original_byte2.ok() ||
                !original_word.ok()) {
              return absl::InternalError("Failed to read original ROM data");
            }

            // Make changes in transaction
            transaction.WriteByte(0x1000, 0xAA)
                .WriteByte(0x2000, 0xBB)
                .WriteWord(0x3000, 0xCCDD);

            // Commit transaction
            RETURN_IF_ERROR(transaction.Commit());

            // Verify changes
            auto new_byte1 = test_rom->ReadByte(0x1000);
            auto new_byte2 = test_rom->ReadByte(0x2000);
            auto new_word = test_rom->ReadWord(0x3000);

            if (!new_byte1.ok() || !new_byte2.ok() || !new_word.ok()) {
              return absl::InternalError("Failed to read modified ROM data");
            }

            if (*new_byte1 != 0xAA || *new_byte2 != 0xBB ||
                *new_word != 0xCCDD) {
              return absl::InternalError(
                  "Transaction changes not applied correctly");
            }

            return absl::OkStatus();
          });

      if (test_status.ok()) {
        result.status = TestStatus::kPassed;
        result.error_message = "Transaction system test completed successfully";
      } else {
        result.status = TestStatus::kFailed;
        result.error_message =
            "Transaction system test failed: " + test_status.ToString();
      }

    } catch (const std::exception& e) {
      result.status = TestStatus::kFailed;
      result.error_message =
          "Transaction system test exception: " + std::string(e.what());
    }

    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    results.AddResult(result);
  }

  void RunLargeScaleEditingTest(TestResults& results, Rom* rom) {
    auto start_time = std::chrono::steady_clock::now();

    TestResult result;
    result.name = "Large_Scale_Editing_Test";
    result.suite_name = GetName();
    result.category = GetCategory();
    result.timestamp = start_time;

    try {
      auto& test_manager = TestManager::Get();

      auto test_status =
          test_manager.TestRomWithCopy(rom, [&](Rom* test_rom) -> absl::Status {
            // Test large-scale editing
            for (int i = 0; i < num_edits_; i++) {
              uint32_t addr = 0x1000 + (i * 4);
              uint8_t value = i % 256;

              RETURN_IF_ERROR(test_rom->WriteByte(addr, value));
            }

            // Verify all changes
            for (int i = 0; i < num_edits_; i++) {
              uint32_t addr = 0x1000 + (i * 4);
              uint8_t expected_value = i % 256;

              auto actual_value = test_rom->ReadByte(addr);
              if (!actual_value.ok()) {
                return absl::InternalError(
                    absl::StrFormat("Failed to read address 0x%X", addr));
              }

              if (*actual_value != expected_value) {
                return absl::InternalError(absl::StrFormat(
                    "Value mismatch at 0x%X: expected=0x%02X, actual=0x%02X",
                    addr, expected_value, *actual_value));
              }
            }

            return absl::OkStatus();
          });

      if (test_status.ok()) {
        result.status = TestStatus::kPassed;
        result.error_message = absl::StrFormat(
            "Large-scale editing test passed: %d edits", num_edits_);
      } else {
        result.status = TestStatus::kFailed;
        result.error_message =
            "Large-scale editing test failed: " + test_status.ToString();
      }

    } catch (const std::exception& e) {
      result.status = TestStatus::kFailed;
      result.error_message =
          "Large-scale editing test exception: " + std::string(e.what());
    }

    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    results.AddResult(result);
  }

  void RunCorruptionDetectionTest(TestResults& results, Rom* rom) {
    auto start_time = std::chrono::steady_clock::now();

    TestResult result;
    result.name = "Corruption_Detection_Test";
    result.suite_name = GetName();
    result.category = GetCategory();
    result.timestamp = start_time;

    try {
      auto& test_manager = TestManager::Get();

      auto test_status =
          test_manager.TestRomWithCopy(rom, [&](Rom* test_rom) -> absl::Status {
            // Intentionally corrupt some data
            RETURN_IF_ERROR(test_rom->WriteByte(0x1000, 0xFF));
            RETURN_IF_ERROR(test_rom->WriteByte(0x2000, 0xAA));

            // Verify corruption is detected
            auto corrupted_byte1 = test_rom->ReadByte(0x1000);
            auto corrupted_byte2 = test_rom->ReadByte(0x2000);

            if (!corrupted_byte1.ok() || !corrupted_byte2.ok()) {
              return absl::InternalError("Failed to read corrupted data");
            }

            if (*corrupted_byte1 != 0xFF || *corrupted_byte2 != 0xAA) {
              return absl::InternalError("Corruption not applied correctly");
            }

            // Verify original data is different
            auto original_byte1 = rom->ReadByte(0x1000);
            auto original_byte2 = rom->ReadByte(0x2000);

            if (!original_byte1.ok() || !original_byte2.ok()) {
              return absl::InternalError(
                  "Failed to read original data for comparison");
            }

            if (*corrupted_byte1 == *original_byte1 ||
                *corrupted_byte2 == *original_byte2) {
              return absl::InternalError(
                  "Corruption detection test failed - data not actually "
                  "corrupted");
            }

            return absl::OkStatus();
          });

      if (test_status.ok()) {
        result.status = TestStatus::kPassed;
        result.error_message =
            "Corruption detection test passed - corruption successfully "
            "applied and detected";
      } else {
        result.status = TestStatus::kFailed;
        result.error_message =
            "Corruption detection test failed: " + test_status.ToString();
      }

    } catch (const std::exception& e) {
      result.status = TestStatus::kFailed;
      result.error_message =
          "Corruption detection test exception: " + std::string(e.what());
    }

    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    results.AddResult(result);
  }

  // Configuration
  bool test_rom_load_save_ = true;
  bool test_data_integrity_ = true;
  bool test_transaction_system_ = true;
  bool test_large_scale_editing_ = true;
  bool test_corruption_detection_ = true;
  int num_edits_ = 10;
};

}  // namespace test
}  // namespace yaze

#endif  // YAZE_APP_TEST_E2E_TEST_SUITE_H
