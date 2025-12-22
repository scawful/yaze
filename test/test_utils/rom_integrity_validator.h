#ifndef YAZE_TEST_UTILS_ROM_INTEGRITY_VALIDATOR_H
#define YAZE_TEST_UTILS_ROM_INTEGRITY_VALIDATOR_H

#include <string>
#include <vector>

#include "rom/rom.h"

namespace yaze {
namespace test {

/**
 * @struct ValidationResult
 * @brief Results of ROM integrity validation
 */
struct ValidationResult {
  bool valid = true;
  std::vector<std::string> errors;
  std::vector<std::string> warnings;

  // Add error message
  void AddError(const std::string& error) {
    errors.push_back(error);
    valid = false;
  }

  // Add warning message (doesn't invalidate)
  void AddWarning(const std::string& warning) { warnings.push_back(warning); }

  // Get summary string
  std::string GetSummary() const {
    if (valid) {
      if (warnings.empty()) {
        return "ROM validation passed with no issues";
      } else {
        return absl::StrFormat(
            "ROM validation passed with %zu warnings", warnings.size());
      }
    } else {
      return absl::StrFormat("ROM validation failed with %zu errors",
                             errors.size());
    }
  }
};

/**
 * @class RomIntegrityValidator
 * @brief Validates ROM integrity after edits
 *
 * Performs comprehensive validation to ensure that ROM edits
 * maintain structural integrity and don't corrupt data.
 *
 * Validation checks:
 * - SNES header checksum correctness
 * - Pointer validity (all pointers within ROM bounds)
 * - Compression integrity (compressed data can be decompressed)
 * - Data structure consistency (overworld, dungeons, graphics)
 * - Boundary checks (no data overflow)
 *
 * Example usage:
 * @code
 * Rom rom;
 * rom.LoadFromFile("zelda3.sfc");
 *
 * // Make edits...
 * rom.WriteByte(0x1234, 0x42);
 *
 * RomIntegrityValidator validator;
 * ValidationResult result = validator.ValidateRomIntegrity(&rom);
 *
 * if (!result.valid) {
 *   for (const auto& error : result.errors) {
 *     std::cerr << "Error: " << error << std::endl;
 *   }
 * }
 * @endcode
 */
class RomIntegrityValidator {
 public:
  RomIntegrityValidator() = default;

  // Perform full ROM integrity validation
  ValidationResult ValidateRomIntegrity(Rom* rom);

  // Individual validation methods (can be used separately)
  bool ValidateChecksum(Rom* rom, ValidationResult* result = nullptr);
  bool ValidateGraphicsPointers(Rom* rom, ValidationResult* result = nullptr);
  bool ValidateCompression(Rom* rom, ValidationResult* result = nullptr);
  bool ValidateOverworldStructure(Rom* rom,
                                  ValidationResult* result = nullptr);
  bool ValidateDungeonStructure(Rom* rom, ValidationResult* result = nullptr);
  bool ValidateSpriteData(Rom* rom, ValidationResult* result = nullptr);

 private:
  // Helper methods
  bool ValidatePointerRange(uint32_t pointer, uint32_t rom_size,
                            const std::string& pointer_name,
                            ValidationResult* result);

  bool ValidateDataRegion(Rom* rom, uint32_t start, uint32_t end,
                          const std::string& region_name,
                          ValidationResult* result);

  bool ValidateCompressedBlock(Rom* rom, uint32_t address,
                                const std::string& block_name,
                                ValidationResult* result);
};

}  // namespace test
}  // namespace yaze

#endif  // YAZE_TEST_UTILS_ROM_INTEGRITY_VALIDATOR_H
