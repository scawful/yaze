#ifndef YAZE_APP_CORE_ASAR_WRAPPER_H
#define YAZE_APP_CORE_ASAR_WRAPPER_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace yaze {
namespace app {
namespace core {

/**
 * @brief Symbol information extracted from Asar assembly
 */
struct AsarSymbol {
  std::string name;           // Symbol name
  uint32_t address;          // Memory address
  std::string opcode;        // Associated opcode if available
  std::string file;          // Source file
  int line;                  // Line number in source
  std::string comment;       // Optional comment
};

/**
 * @brief Asar patch result information
 */
struct AsarPatchResult {
  bool success;              // Whether patch was successful
  std::vector<std::string> errors;     // Error messages if any
  std::vector<std::string> warnings;   // Warning messages
  std::vector<AsarSymbol> symbols;     // Extracted symbols
  uint32_t rom_size;         // Final ROM size after patching
  uint32_t crc32;            // CRC32 checksum of patched ROM
};

/**
 * @brief Modern C++ wrapper for Asar 65816 assembler integration
 * 
 * This class provides a high-level interface for:
 * - Patching ROMs with assembly code
 * - Extracting symbol names and opcodes
 * - Cross-platform compatibility (Windows, macOS, Linux)
 */
class AsarWrapper {
 public:
  AsarWrapper();
  ~AsarWrapper();

  // Disable copy constructor and assignment
  AsarWrapper(const AsarWrapper&) = delete;
  AsarWrapper& operator=(const AsarWrapper&) = delete;

  // Enable move constructor and assignment
  AsarWrapper(AsarWrapper&&) = default;
  AsarWrapper& operator=(AsarWrapper&&) = default;

  /**
   * @brief Initialize the Asar library
   * @return Status indicating success or failure
   */
  absl::Status Initialize();

  /**
   * @brief Clean up and close the Asar library
   */
  void Shutdown();

  /**
   * @brief Check if Asar is initialized and ready
   * @return True if initialized, false otherwise
   */
  bool IsInitialized() const { return initialized_; }

  /**
   * @brief Get Asar version information
   * @return Version string
   */
  std::string GetVersion() const;

  /**
   * @brief Get Asar API version
   * @return API version number
   */
  int GetApiVersion() const;

  /**
   * @brief Apply an assembly patch to a ROM
   * @param patch_path Path to the .asm patch file
   * @param rom_data ROM data to patch (will be modified)
   * @param include_paths Additional include paths for assembly files
   * @return Patch result with status and extracted information
   */
  absl::StatusOr<AsarPatchResult> ApplyPatch(
      const std::string& patch_path,
      std::vector<uint8_t>& rom_data,
      const std::vector<std::string>& include_paths = {});

  /**
   * @brief Apply an assembly patch from string content
   * @param patch_content Assembly source code as string
   * @param rom_data ROM data to patch (will be modified)
   * @param base_path Base path for resolving includes
   * @return Patch result with status and extracted information
   */
  absl::StatusOr<AsarPatchResult> ApplyPatchFromString(
      const std::string& patch_content,
      std::vector<uint8_t>& rom_data,
      const std::string& base_path = "");

  /**
   * @brief Extract symbols from an assembly file without patching
   * @param asm_path Path to the assembly file
   * @param include_paths Additional include paths
   * @return Vector of extracted symbols
   */
  absl::StatusOr<std::vector<AsarSymbol>> ExtractSymbols(
      const std::string& asm_path,
      const std::vector<std::string>& include_paths = {});

  /**
   * @brief Get all available symbols from the last patch operation
   * @return Map of symbol names to symbol information
   */
  std::map<std::string, AsarSymbol> GetSymbolTable() const;

  /**
   * @brief Find a symbol by name
   * @param name Symbol name to search for
   * @return Symbol information if found
   */
  std::optional<AsarSymbol> FindSymbol(const std::string& name) const;

  /**
   * @brief Get symbols at a specific address
   * @param address Memory address to search
   * @return Vector of symbols at that address
   */
  std::vector<AsarSymbol> GetSymbolsAtAddress(uint32_t address) const;

  /**
   * @brief Reset the Asar state (clear errors, warnings, symbols)
   */
  void Reset();

  /**
   * @brief Get the last error messages
   * @return Vector of error strings
   */
  std::vector<std::string> GetLastErrors() const { return last_errors_; }

  /**
   * @brief Get the last warning messages
   * @return Vector of warning strings
   */
  std::vector<std::string> GetLastWarnings() const { return last_warnings_; }

  /**
   * @brief Create a patch that can be applied to transform one ROM to another
   * @param original_rom Original ROM data
   * @param modified_rom Modified ROM data
   * @param patch_path Output path for the generated patch
   * @return Status indicating success or failure
   */
  absl::Status CreatePatch(
      const std::vector<uint8_t>& original_rom,
      const std::vector<uint8_t>& modified_rom,
      const std::string& patch_path);

  /**
   * @brief Validate an assembly file for syntax errors
   * @param asm_path Path to the assembly file
   * @return Status indicating validation result
   */
  absl::Status ValidateAssembly(const std::string& asm_path);

 private:
  bool initialized_;
  std::map<std::string, AsarSymbol> symbol_table_;
  std::vector<std::string> last_errors_;
  std::vector<std::string> last_warnings_;

  /**
   * @brief Process errors from Asar and store them
   */
  void ProcessErrors();

  /**
   * @brief Process warnings from Asar and store them
   */
  void ProcessWarnings();

  /**
   * @brief Extract symbols from the last Asar operation
   */
  void ExtractSymbolsFromLastOperation();

  /**
   * @brief Convert Asar symbol data to AsarSymbol struct
   */
  AsarSymbol ConvertAsarSymbol(const void* asar_symbol_data) const;
};

}  // namespace core
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_CORE_ASAR_WRAPPER_H
