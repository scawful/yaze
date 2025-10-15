#ifndef YAZE_CORE_ASAR_WRAPPER_H
#define YAZE_CORE_ASAR_WRAPPER_H

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace yaze {
namespace core {

/**
 * @brief Symbol information extracted from Asar assembly
 */
struct AsarSymbol {
  std::string name;           // Symbol name
  uint32_t address;           // Memory address
  std::string opcode;         // Associated opcode if available
  std::string file;           // Source file
  int line;                   // Line number in source
  std::string comment;        // Optional comment
};

/**
 * @brief Asar patch result information
 */
struct AsarPatchResult {
  bool success;                         // Whether patch was successful
  std::vector<std::string> errors;      // Error messages if any
  std::vector<std::string> warnings;    // Warning messages
  std::vector<AsarSymbol> symbols;      // Extracted symbols
  uint32_t rom_size;                    // Final ROM size after patching
  uint32_t crc32;                       // CRC32 checksum of patched ROM
};

/**
 * @brief Modern C++ wrapper for Asar 65816 assembler integration.
 *
 * Provides a high-level interface for:
 * - Patching ROMs with assembly code
 * - Extracting symbol names and opcodes
 * - Cross-platform compatibility (Windows, macOS, Linux)
 */
class AsarWrapper {
 public:
  AsarWrapper();
  ~AsarWrapper();

  AsarWrapper(const AsarWrapper&) = delete;
  AsarWrapper& operator=(const AsarWrapper&) = delete;

  AsarWrapper(AsarWrapper&&) = default;
  AsarWrapper& operator=(AsarWrapper&&) = default;

  absl::Status Initialize();
  void Shutdown();
  bool IsInitialized() const { return initialized_; }

  std::string GetVersion() const;
  int GetApiVersion() const;

  absl::StatusOr<AsarPatchResult> ApplyPatch(
      const std::string& patch_path, std::vector<uint8_t>& rom_data,
      const std::vector<std::string>& include_paths = {});

  absl::StatusOr<AsarPatchResult> ApplyPatchFromString(
      const std::string& patch_content, std::vector<uint8_t>& rom_data,
      const std::string& base_path = "");

  absl::StatusOr<std::vector<AsarSymbol>> ExtractSymbols(
      const std::string& asm_path,
      const std::vector<std::string>& include_paths = {});

  std::map<std::string, AsarSymbol> GetSymbolTable() const;
  std::optional<AsarSymbol> FindSymbol(const std::string& name) const;
  std::vector<AsarSymbol> GetSymbolsAtAddress(uint32_t address) const;

  void Reset();

  std::vector<std::string> GetLastErrors() const { return last_errors_; }
  std::vector<std::string> GetLastWarnings() const { return last_warnings_; }

  absl::Status CreatePatch(const std::vector<uint8_t>& original_rom,
                           const std::vector<uint8_t>& modified_rom,
                           const std::string& patch_path);

  absl::Status ValidateAssembly(const std::string& asm_path);

 private:
  bool initialized_;
  std::map<std::string, AsarSymbol> symbol_table_;
  std::vector<std::string> last_errors_;
  std::vector<std::string> last_warnings_;

  void ProcessErrors();
  void ProcessWarnings();
  void ExtractSymbolsFromLastOperation();
  AsarSymbol ConvertAsarSymbol(const void* asar_symbol_data) const;
};

}  // namespace core
}  // namespace yaze

#endif  // YAZE_CORE_ASAR_WRAPPER_H
