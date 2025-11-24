#ifndef YAZE_APP_PLATFORM_WASM_WASM_PATCH_EXPORT_H_
#define YAZE_APP_PLATFORM_WASM_WASM_PATCH_EXPORT_H_

#ifdef __EMSCRIPTEN__

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace yaze {
namespace platform {

/**
 * @struct PatchInfo
 * @brief Information about the differences between original and modified ROMs
 */
struct PatchInfo {
  size_t changed_bytes;                                // Total number of changed bytes
  size_t num_regions;                                  // Number of distinct changed regions
  std::vector<std::pair<size_t, size_t>> changed_regions;  // List of (offset, length) pairs
};

/**
 * @class WasmPatchExport
 * @brief Patch export functionality for WASM/browser environment
 *
 * This class provides functionality to generate and export BPS and IPS patches
 * from ROM modifications, allowing users to save their changes as patch files
 * that can be applied to clean ROMs.
 */
class WasmPatchExport {
 public:
  /**
   * @brief Export modifications as a BPS (Beat) patch file
   *
   * BPS is a modern patching format that supports:
   * - Variable-length encoding for efficient storage
   * - Delta encoding for changed regions
   * - CRC32 checksums for source, target, and patch validation
   *
   * @param original Original ROM data
   * @param modified Modified ROM data
   * @param filename Suggested filename for the download (e.g., "hack.bps")
   * @return Status indicating success or failure
   */
  static absl::Status ExportBPS(const std::vector<uint8_t>& original,
                                const std::vector<uint8_t>& modified,
                                const std::string& filename);

  /**
   * @brief Export modifications as an IPS patch file
   *
   * IPS is a classic patching format that supports:
   * - Simple record-based structure
   * - RLE encoding for repeated bytes
   * - Maximum file size of 16MB (24-bit addressing)
   *
   * @param original Original ROM data
   * @param modified Modified ROM data
   * @param filename Suggested filename for the download (e.g., "hack.ips")
   * @return Status indicating success or failure
   */
  static absl::Status ExportIPS(const std::vector<uint8_t>& original,
                                const std::vector<uint8_t>& modified,
                                const std::string& filename);

  /**
   * @brief Get a preview of changes between original and modified ROMs
   *
   * Analyzes the differences and returns summary information about
   * what would be included in a patch file.
   *
   * @param original Original ROM data
   * @param modified Modified ROM data
   * @return PatchInfo structure containing change statistics
   */
  static PatchInfo GetPatchPreview(const std::vector<uint8_t>& original,
                                   const std::vector<uint8_t>& modified);

 private:
  // CRC32 calculation
  static uint32_t CalculateCRC32(const std::vector<uint8_t>& data);
  static uint32_t CalculateCRC32(const uint8_t* data, size_t size);

  // BPS format helpers
  static void WriteVariableLength(std::vector<uint8_t>& output, uint64_t value);
  static std::vector<uint8_t> GenerateBPSPatch(const std::vector<uint8_t>& source,
                                               const std::vector<uint8_t>& target);

  // IPS format helpers
  static void WriteIPS24BitOffset(std::vector<uint8_t>& output, uint32_t offset);
  static void WriteIPS16BitSize(std::vector<uint8_t>& output, uint16_t size);
  static std::vector<uint8_t> GenerateIPSPatch(const std::vector<uint8_t>& source,
                                               const std::vector<uint8_t>& target);

  // Common helpers
  static std::vector<std::pair<size_t, size_t>> FindChangedRegions(
      const std::vector<uint8_t>& original,
      const std::vector<uint8_t>& modified);

  // Browser download functionality (implemented via EM_JS)
  static absl::Status DownloadPatchFile(const std::string& filename,
                                        const std::vector<uint8_t>& data,
                                        const std::string& mime_type);
};

}  // namespace platform
}  // namespace yaze

#else  // !__EMSCRIPTEN__

// Stub implementation for non-WASM builds
namespace yaze {
namespace platform {

struct PatchInfo {
  size_t changed_bytes = 0;
  size_t num_regions = 0;
  std::vector<std::pair<size_t, size_t>> changed_regions;
};

class WasmPatchExport {
 public:
  static absl::Status ExportBPS(const std::vector<uint8_t>&,
                                const std::vector<uint8_t>&,
                                const std::string&) {
    return absl::UnimplementedError("Patch export is only available in WASM builds");
  }

  static absl::Status ExportIPS(const std::vector<uint8_t>&,
                                const std::vector<uint8_t>&,
                                const std::string&) {
    return absl::UnimplementedError("Patch export is only available in WASM builds");
  }

  static PatchInfo GetPatchPreview(const std::vector<uint8_t>&,
                                   const std::vector<uint8_t>&) {
    return PatchInfo{};
  }
};

}  // namespace platform
}  // namespace yaze

#endif  // __EMSCRIPTEN__

#endif  // YAZE_APP_PLATFORM_WASM_WASM_PATCH_EXPORT_H_