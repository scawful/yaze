#include "app/core/asar_wrapper.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"

// Include Asar C bindings  
#include "asar-dll-bindings/c/asar.h"

namespace yaze {
namespace core {

AsarWrapper::AsarWrapper() : initialized_(false) {}

AsarWrapper::~AsarWrapper() {
  if (initialized_) {
    Shutdown();
  }
}

absl::Status AsarWrapper::Initialize() {
  if (initialized_) {
    return absl::OkStatus();
  }

  // Verify API version compatibility
  int api_version = asar_apiversion();
  if (api_version < 300) {  // Require at least API version 3.0
    return absl::InternalError(absl::StrFormat(
        "Asar API version %d is too old (required: 300+)", api_version));
  }

  initialized_ = true;
  return absl::OkStatus();
}

void AsarWrapper::Shutdown() {
  if (initialized_) {
    // Note: Static library doesn't have asar_close()
    initialized_ = false;
  }
}

std::string AsarWrapper::GetVersion() const {
  if (!initialized_) {
    return "Not initialized";
  }
  
  int version = asar_version();
  int major = version / 10000;
  int minor = (version / 100) % 100;
  int patch = version % 100;
  
  return absl::StrFormat("%d.%d.%d", major, minor, patch);
}

int AsarWrapper::GetApiVersion() const {
  if (!initialized_) {
    return 0;
  }
  return asar_apiversion();
}

absl::StatusOr<AsarPatchResult> AsarWrapper::ApplyPatch(
    const std::string& patch_path,
    std::vector<uint8_t>& rom_data,
    const std::vector<std::string>& include_paths) {
  
  if (!initialized_) {
    return absl::FailedPreconditionError("Asar not initialized");
  }

  // Reset previous state
  Reset();

  AsarPatchResult result;
  result.success = false;

  // Prepare ROM data
  int rom_size = static_cast<int>(rom_data.size());
  int buffer_size = std::max(rom_size, 16 * 1024 * 1024);  // At least 16MB buffer
  
  // Resize ROM data if needed
  if (rom_data.size() < buffer_size) {
    rom_data.resize(buffer_size, 0);
  }

  // Apply the patch
  bool patch_success = asar_patch(
      patch_path.c_str(),
      reinterpret_cast<char*>(rom_data.data()),
      buffer_size,
      &rom_size);

  // Process results
  ProcessErrors();
  ProcessWarnings();
  
  result.errors = last_errors_;
  result.warnings = last_warnings_;
  result.success = patch_success && last_errors_.empty();

  if (result.success) {
    // Resize ROM data to actual size
    rom_data.resize(rom_size);
    result.rom_size = rom_size;
    
    // Extract symbols
    ExtractSymbolsFromLastOperation();
    result.symbols.reserve(symbol_table_.size());
    for (const auto& [name, symbol] : symbol_table_) {
      result.symbols.push_back(symbol);
    }
    
    // Calculate CRC32 if available
    // Note: Asar might provide this, check if function exists
    result.crc32 = 0;  // TODO: Implement CRC32 calculation
  } else {
    return absl::InternalError(absl::StrFormat(
        "Patch failed: %s", absl::StrJoin(last_errors_, "; ")));
  }

  return result;
}

absl::StatusOr<AsarPatchResult> AsarWrapper::ApplyPatchFromString(
    const std::string& patch_content,
    std::vector<uint8_t>& rom_data,
    const std::string& base_path) {
  
  // Create temporary file for patch content
  std::string temp_path = "/tmp/yaze_temp_patch.asm";
  if (!base_path.empty()) {
    // Ensure directory exists
    std::filesystem::create_directories(base_path);
    temp_path = base_path + "/temp_patch.asm";
  }

  std::ofstream temp_file(temp_path);
  if (!temp_file) {
    return absl::InternalError(absl::StrFormat(
        "Failed to create temporary patch file at: %s", temp_path));
  }
  
  temp_file << patch_content;
  temp_file.close();

  auto result = ApplyPatch(temp_path, rom_data);
  
  // Clean up temporary file
  std::remove(temp_path.c_str());
  
  return result;
}

absl::StatusOr<std::vector<AsarSymbol>> AsarWrapper::ExtractSymbols(
    const std::string& asm_path,
    const std::vector<std::string>& include_paths) {
  
  if (!initialized_) {
    return absl::FailedPreconditionError("Asar not initialized");
  }

  // Create a dummy ROM for symbol extraction
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);  // 1MB dummy ROM
  
  auto result = ApplyPatch(asm_path, dummy_rom, include_paths);
  if (!result.ok()) {
    return result.status();
  }

  return result->symbols;
}

std::map<std::string, AsarSymbol> AsarWrapper::GetSymbolTable() const {
  return symbol_table_;
}

std::optional<AsarSymbol> AsarWrapper::FindSymbol(const std::string& name) const {
  auto it = symbol_table_.find(name);
  if (it != symbol_table_.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::vector<AsarSymbol> AsarWrapper::GetSymbolsAtAddress(uint32_t address) const {
  std::vector<AsarSymbol> symbols;
  for (const auto& [name, symbol] : symbol_table_) {
    if (symbol.address == address) {
      symbols.push_back(symbol);
    }
  }
  return symbols;
}

void AsarWrapper::Reset() {
  if (initialized_) {
    asar_reset();
  }
  symbol_table_.clear();
  last_errors_.clear();
  last_warnings_.clear();
}

absl::Status AsarWrapper::CreatePatch(
    const std::vector<uint8_t>& original_rom,
    const std::vector<uint8_t>& modified_rom,
    const std::string& patch_path) {
  
  // This is a complex operation that would require:
  // 1. Analyzing differences between ROMs
  // 2. Generating appropriate assembly code
  // 3. Writing the patch file
  
  // For now, return not implemented
  return absl::UnimplementedError(
      "Patch creation from ROM differences not yet implemented");
}

absl::Status AsarWrapper::ValidateAssembly(const std::string& asm_path) {
  // Create a dummy ROM for validation
  std::vector<uint8_t> dummy_rom(1024, 0);
  
  auto result = ApplyPatch(asm_path, dummy_rom);
  if (!result.ok()) {
    return result.status();
  }
  
  if (!result->success) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Assembly validation failed: %s", 
        absl::StrJoin(result->errors, "; ")));
  }
  
  return absl::OkStatus();
}

void AsarWrapper::ProcessErrors() {
  last_errors_.clear();
  
  int error_count = 0;
  const errordata* errors = asar_geterrors(&error_count);
  
  for (int i = 0; i < error_count; ++i) {
    last_errors_.push_back(std::string(errors[i].fullerrdata));
  }
}

void AsarWrapper::ProcessWarnings() {
  last_warnings_.clear();
  
  int warning_count = 0;
  const errordata* warnings = asar_getwarnings(&warning_count);
  
  for (int i = 0; i < warning_count; ++i) {
    last_warnings_.push_back(std::string(warnings[i].fullerrdata));
  }
}

void AsarWrapper::ExtractSymbolsFromLastOperation() {
  symbol_table_.clear();
  
  // Extract labels using the correct API function
  int symbol_count = 0;
  const labeldata* labels = asar_getalllabels(&symbol_count);
  
  for (int i = 0; i < symbol_count; ++i) {
    AsarSymbol symbol;
    symbol.name = std::string(labels[i].name);
    symbol.address = labels[i].location;
    symbol.file = "";  // Not available in basic API
    symbol.line = 0;   // Not available in basic API
    symbol.opcode = "";  // Would need additional processing
    symbol.comment = "";
    
    symbol_table_[symbol.name] = symbol;
  }
}

AsarSymbol AsarWrapper::ConvertAsarSymbol(const void* asar_symbol_data) const {
  // This would convert from Asar's internal symbol representation
  // to our AsarSymbol struct. Implementation depends on Asar's API.
  
  AsarSymbol symbol;
  // Placeholder implementation
  return symbol;
}

}  // namespace core
}  // namespace yaze
