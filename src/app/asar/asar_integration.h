#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "asar.h"

// Macro for error handling
#define RETURN_IF_ERROR(expr) \
  do { \
    auto _status = (expr); \
    if (!_status.ok()) return _status; \
  } while (0)

namespace yaze {
namespace app {
namespace asar {

// Symbol information extracted from Asar
struct SymbolInfo {
  std::string name;
  int location;
  std::string type;  // "label", "define", etc.
  std::string value; // For defines, the actual value
};

// Opcode information for 65816 assembly
struct OpcodeInfo {
  std::string mnemonic;
  std::string addressing_mode;
  int size;
  std::string description;
};

// ROM patching result
struct PatchResult {
  bool success;
  std::vector<std::string> errors;
  std::vector<std::string> warnings;
  std::vector<SymbolInfo> symbols;
  std::vector<writtenblockdata> written_blocks;
  int rom_size_before;
  int rom_size_after;
};

// Asar integration class for ROM patching and symbol extraction
class AsarIntegration {
 public:
  AsarIntegration();
  ~AsarIntegration();

  // Initialize Asar library
  absl::Status Initialize();

  // Patch ROM with assembly code
  absl::StatusOr<PatchResult> PatchRom(
      const std::string& patch_file,
      const std::vector<uint8_t>& rom_data,
      const std::vector<std::string>& include_paths = {},
      const std::unordered_map<std::string, std::string>& defines = {});

  // Extract symbols from a patch file without applying it
  absl::StatusOr<std::vector<SymbolInfo>> ExtractSymbols(
      const std::string& patch_file,
      const std::vector<std::string>& include_paths = {});

  // Get all available 65816 opcodes
  std::vector<OpcodeInfo> Get65816Opcodes();

  // Get symbol value by name
  absl::StatusOr<int> GetSymbolValue(const std::string& symbol_name);

  // Get all labels
  absl::StatusOr<std::vector<SymbolInfo>> GetAllLabels();

  // Get all defines
  absl::StatusOr<std::vector<SymbolInfo>> GetAllDefines();

  // Get written blocks from last patch operation
  absl::StatusOr<std::vector<writtenblockdata>> GetWrittenBlocks();

  // Generate symbols file in various formats
  absl::StatusOr<std::string> GenerateSymbolsFile(const std::string& format = "wla");

  // Check if Asar is properly initialized
  bool IsInitialized() const { return initialized_; }

  // Get Asar version
  std::string GetVersion();

  // Get API version
  std::string GetApiVersion();

 private:
  bool initialized_;
  std::vector<uint8_t> current_rom_data_;
  
  // Helper methods
  absl::Status ResetAsar();
  std::vector<SymbolInfo> ConvertLabelsToSymbols();
  std::vector<SymbolInfo> ConvertDefinesToSymbols();
  std::vector<std::string> GetErrorMessages();
  std::vector<std::string> GetWarningMessages();
};

}  // namespace asar
}  // namespace app
}  // namespace yaze