#ifndef YAZE_CLI_AUTOMATION_ROM_AUTOMATION_API_H
#define YAZE_CLI_AUTOMATION_ROM_AUTOMATION_API_H

#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "rom/rom.h"
#include "nlohmann/json.hpp"

namespace yaze {
namespace cli {
namespace automation {

/**
 * @brief High-level API for ROM manipulation and automation
 *
 * Provides a clean interface for AI agents and automation scripts to
 * interact with ROM data without direct memory manipulation.
 */
class RomAutomationAPI {
 public:
  explicit RomAutomationAPI(Rom* rom) : rom_(rom) {}

  // ============================================================================
  // Direct ROM Operations
  // ============================================================================

  /**
   * @brief Read bytes from ROM at specified address
   * @param address Starting address
   * @param length Number of bytes to read
   * @return Vector of bytes or error
   */
  absl::StatusOr<std::vector<uint8_t>> ReadBytes(uint32_t address,
                                                  size_t length) const;

  /**
   * @brief Write bytes to ROM at specified address
   * @param address Starting address
   * @param data Bytes to write
   * @param verify If true, read back and verify write succeeded
   * @return Status of write operation
   */
  absl::Status WriteBytes(uint32_t address, const std::vector<uint8_t>& data,
                         bool verify = true);

  /**
   * @brief Find pattern in ROM
   * @param pattern Bytes to search for
   * @param start_address Optional starting address
   * @param max_results Maximum number of results to return
   * @return Vector of addresses where pattern was found
   */
  absl::StatusOr<std::vector<uint32_t>> FindPattern(
      const std::vector<uint8_t>& pattern,
      uint32_t start_address = 0,
      size_t max_results = 100) const;

  // ============================================================================
  // ROM State Management
  // ============================================================================

  /**
   * @brief Snapshot of ROM state at a point in time
   */
  struct RomSnapshot {
    std::string name;
    std::string timestamp;
    std::vector<uint8_t> data;
    nlohmann::json metadata;
    bool compressed = false;
  };

  /**
   * @brief Create a snapshot of current ROM state
   * @param name Snapshot identifier
   * @param compress If true, compress the snapshot data
   * @return Created snapshot or error
   */
  absl::StatusOr<RomSnapshot> CreateSnapshot(const std::string& name,
                                              bool compress = true);

  /**
   * @brief Restore ROM to a previous snapshot
   * @param snapshot Snapshot to restore
   * @param verify If true, verify restoration succeeded
   * @return Status of restoration
   */
  absl::Status RestoreSnapshot(const RomSnapshot& snapshot,
                               bool verify = true);

  /**
   * @brief List all available snapshots
   * @return Vector of snapshot metadata
   */
  std::vector<nlohmann::json> ListSnapshots() const;

  /**
   * @brief Compare current ROM with snapshot
   * @param snapshot Snapshot to compare against
   * @return Difference report as JSON
   */
  absl::StatusOr<nlohmann::json> CompareWithSnapshot(
      const RomSnapshot& snapshot) const;

  // ============================================================================
  // ROM Validation
  // ============================================================================

  /**
   * @brief Validation result for ROM integrity checks
   */
  struct ValidationResult {
    bool is_valid;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    nlohmann::json details;
  };

  /**
   * @brief Validate ROM headers
   * @return Validation result
   */
  ValidationResult ValidateHeaders() const;

  /**
   * @brief Validate ROM checksums
   * @return Validation result
   */
  ValidationResult ValidateChecksums() const;

  /**
   * @brief Validate specific ROM regions
   * @param regions List of region names to validate
   * @return Validation result
   */
  ValidationResult ValidateRegions(
      const std::vector<std::string>& regions) const;

  /**
   * @brief Comprehensive ROM validation
   * @return Combined validation result
   */
  ValidationResult ValidateFull() const;

  // ============================================================================
  // ROM Patching
  // ============================================================================

  /**
   * @brief Apply IPS/BPS patch to ROM
   * @param patch_data Patch file contents
   * @param patch_format Format of patch (IPS, BPS, etc.)
   * @return Status of patch application
   */
  absl::Status ApplyPatch(const std::vector<uint8_t>& patch_data,
                         const std::string& patch_format);

  /**
   * @brief Generate patch between current ROM and target
   * @param target_rom Target ROM to diff against
   * @param patch_format Format to generate (IPS, BPS)
   * @return Generated patch data or error
   */
  absl::StatusOr<std::vector<uint8_t>> GeneratePatch(
      const Rom& target_rom,
      const std::string& patch_format) const;

  // ============================================================================
  // Region Management
  // ============================================================================

  /**
   * @brief Export a region of ROM to file
   * @param region_name Named region or custom range
   * @param start_address Start of region (if custom)
   * @param end_address End of region (if custom)
   * @return Exported data or error
   */
  absl::StatusOr<std::vector<uint8_t>> ExportRegion(
      const std::string& region_name,
      uint32_t start_address = 0,
      uint32_t end_address = 0) const;

  /**
   * @brief Import data to a ROM region
   * @param region_name Named region or custom range
   * @param data Data to import
   * @param address Starting address for import
   * @return Status of import operation
   */
  absl::Status ImportRegion(const std::string& region_name,
                           const std::vector<uint8_t>& data,
                           uint32_t address);

  // ============================================================================
  // Batch Operations
  // ============================================================================

  /**
   * @brief Batch operation for multiple ROM modifications
   */
  struct BatchOperation {
    enum Type { READ, WRITE, VERIFY, PATCH };
    Type type;
    uint32_t address;
    std::vector<uint8_t> data;
    nlohmann::json params;
  };

  /**
   * @brief Execute multiple ROM operations atomically
   * @param operations List of operations to execute
   * @param stop_on_error If true, abort on first error
   * @return Results of each operation or error
   */
  absl::StatusOr<std::vector<nlohmann::json>> ExecuteBatch(
      const std::vector<BatchOperation>& operations,
      bool stop_on_error = true);

  // ============================================================================
  // Transaction Support
  // ============================================================================

  /**
   * @brief Begin a ROM modification transaction
   * @return Transaction ID
   */
  std::string BeginTransaction();

  /**
   * @brief Commit a ROM modification transaction
   * @param transaction_id Transaction to commit
   * @return Status of commit
   */
  absl::Status CommitTransaction(const std::string& transaction_id);

  /**
   * @brief Rollback a ROM modification transaction
   * @param transaction_id Transaction to rollback
   * @return Status of rollback
   */
  absl::Status RollbackTransaction(const std::string& transaction_id);

  // ============================================================================
  // Statistics and Analysis
  // ============================================================================

  /**
   * @brief Get ROM statistics
   * @return JSON object with ROM statistics
   */
  nlohmann::json GetStatistics() const;

  /**
   * @brief Analyze ROM for common patterns
   * @return Analysis results as JSON
   */
  nlohmann::json AnalyzePatterns() const;

  /**
   * @brief Find unused space in ROM
   * @param min_size Minimum size of free space to report
   * @return List of free regions
   */
  std::vector<std::pair<uint32_t, size_t>> FindFreeSpace(
      size_t min_size = 16) const;

 private:
  Rom* rom_;
  std::map<std::string, RomSnapshot> snapshots_;
  std::map<std::string, std::unique_ptr<class Transaction>> transactions_;

  // Helper methods
  std::vector<uint8_t> CompressData(const std::vector<uint8_t>& data) const;
  std::vector<uint8_t> DecompressData(const std::vector<uint8_t>& data) const;
  bool VerifyWrite(uint32_t address, const std::vector<uint8_t>& expected) const;
};

}  // namespace automation
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_AUTOMATION_ROM_AUTOMATION_API_H