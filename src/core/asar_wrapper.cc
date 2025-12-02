#include "core/asar_wrapper.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
#include <sstream>

#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"

#ifdef YAZE_ENABLE_ASAR
#include "asar-dll-bindings/c/asar.h"
#endif

namespace yaze {
namespace core {

namespace fs = std::filesystem;

AsarWrapper::AsarWrapper() : initialized_(false), library_enabled_(false) {}

AsarWrapper::~AsarWrapper() {
  if (initialized_) {
    Shutdown();
  }
}

absl::Status AsarWrapper::Initialize() {
  if (initialized_) {
    return absl::OkStatus();
  }

#ifdef YAZE_ENABLE_ASAR
  // Verify API version compatibility
  int api_version = asar_apiversion();
  if (api_version < 300) {  // Require at least API version 3.0
    return absl::InternalError(absl::StrFormat(
        "Asar API version %d is too old (required: 300+)", api_version));
  }

  library_enabled_ = true;
  initialized_ = true;
  return absl::OkStatus();
#else
  // Library not available; allow initialization so we can fall back to the
  // bundled/system CLI.
  library_enabled_ = false;
  initialized_ = true;
  return absl::OkStatus();
#endif
}

void AsarWrapper::Shutdown() {
  if (initialized_) {
    // Note: Static library doesn't have asar_close()
    initialized_ = false;
    library_enabled_ = false;
  }
}

std::string AsarWrapper::GetVersion() const {
#ifdef YAZE_ENABLE_ASAR
  if (!initialized_ || !library_enabled_) {
    return "Not initialized";
  }

  int version = asar_version();
  int major = version / 10000;
  int minor = (version / 100) % 100;
  int patch = version % 100;

  return absl::StrFormat("%d.%d.%d", major, minor, patch);
#endif

#ifdef YAZE_ASAR_STANDALONE_PATH
  return "Asar CLI (bundled)";
#else
  return "Asar CLI (external)";
#endif
}

int AsarWrapper::GetApiVersion() const {
#ifdef YAZE_ENABLE_ASAR
  if (!initialized_ || !library_enabled_) {
    return 0;
  }
  return asar_apiversion();
#endif
  return 0;
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

#ifdef YAZE_ENABLE_ASAR
  if (library_enabled_) {
    auto lib_result =
        ApplyPatchWithLibrary(patch_path, rom_data, include_paths);
    if (lib_result.ok()) {
      return lib_result;
    }
    // Fall through to CLI fallback
    last_errors_.push_back(lib_result.status().ToString());
  }
#endif

  return ApplyPatchWithBinary(patch_path, rom_data, include_paths);
}

absl::StatusOr<AsarPatchResult> AsarWrapper::ApplyPatchFromString(
    const std::string& patch_content,
    std::vector<uint8_t>& rom_data,
    const std::string& base_path) {
  if (!initialized_) {
    return absl::FailedPreconditionError("Asar not initialized");
  }

  // Reset previous state
  Reset();

  // Write patch content to temporary file
  fs::path temp_patch_path =
      fs::temp_directory_path() / "yaze_asar_temp.asm";

  std::ofstream temp_patch_file(temp_patch_path);
  if (!temp_patch_file) {
    return absl::InternalError("Failed to create temporary patch file");
  }

  temp_patch_file << patch_content;
  temp_patch_file.close();

  // Apply patch using temporary file
  auto patch_result = ApplyPatch(temp_patch_path.string(), rom_data, {base_path});

  // Clean up temporary file
  std::error_code ec;
  fs::remove(temp_patch_path, ec);

  return patch_result;
}

absl::StatusOr<std::vector<AsarSymbol>> AsarWrapper::ExtractSymbols(
    const std::string& asm_path,
    const std::vector<std::string>& include_paths) {
#ifdef YAZE_ENABLE_ASAR
  if (!initialized_ || !library_enabled_) {
    return absl::FailedPreconditionError("Asar not initialized");
  }

  // Reset state before extraction
  Reset();

  // Create a dummy ROM for symbol extraction
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);  // 1MB dummy ROM

  auto result = ApplyPatch(asm_path, dummy_rom, include_paths);
  if (!result.ok()) {
    return result.status();
  }

  return result->symbols;
#else
  (void)asm_path;
  (void)include_paths;
  return absl::UnimplementedError(
      "ASAR library not enabled - build with YAZE_ENABLE_ASAR=1");
#endif
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
#ifdef YAZE_ENABLE_ASAR
  if (initialized_ && library_enabled_) {
    asar_reset();
  }
#endif
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
  return absl::UnimplementedError(
      "Patch creation from ROM differences not yet implemented");
}

absl::Status AsarWrapper::ValidateAssembly(const std::string& asm_path) {
#ifdef YAZE_ENABLE_ASAR
  if (!library_enabled_) {
    return absl::UnimplementedError(
        "ASAR library not enabled - build with YAZE_ENABLE_ASAR=1");
  }

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
#else
  (void)asm_path;
  return absl::UnimplementedError(
      "ASAR library not enabled - build with YAZE_ENABLE_ASAR=1");
#endif
}

void AsarWrapper::ProcessErrors() {
#ifdef YAZE_ENABLE_ASAR
  last_errors_.clear();

  int error_count = 0;
  const errordata* errors = asar_geterrors(&error_count);

  for (int i = 0; i < error_count; ++i) {
    last_errors_.push_back(std::string(errors[i].fullerrdata));
  }
#endif
}

void AsarWrapper::ProcessWarnings() {
#ifdef YAZE_ENABLE_ASAR
  last_warnings_.clear();

  int warning_count = 0;
  const errordata* warnings = asar_getwarnings(&warning_count);

  for (int i = 0; i < warning_count; ++i) {
    last_warnings_.push_back(std::string(warnings[i].fullerrdata));
  }
#endif
}

absl::StatusOr<AsarPatchResult> AsarWrapper::ApplyPatchWithLibrary(
    const std::string& patch_path,
    std::vector<uint8_t>& rom_data,
    const std::vector<std::string>& /*include_paths*/) {
#ifdef YAZE_ENABLE_ASAR
  if (!initialized_ || !library_enabled_) {
    return absl::FailedPreconditionError("Asar library not initialized");
  }

  AsarPatchResult result;
  result.success = false;

  // Prepare ROM data
  int rom_size = static_cast<int>(rom_data.size());
  int buffer_size =
      std::max(rom_size, 16 * 1024 * 1024);  // At least 16MB buffer

  // Resize ROM data if needed
  if (rom_data.size() < static_cast<size_t>(buffer_size)) {
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

    result.crc32 = 0;  // TODO: CRC32 calculation for library path
    return result;
  }

  return absl::InternalError(absl::StrFormat(
      "Patch failed: %s", absl::StrJoin(last_errors_, "; ")));
#else
  (void)patch_path;
  (void)rom_data;
  (void)include_paths;
  return absl::UnimplementedError(
      "ASAR library not enabled - build with YAZE_ENABLE_ASAR=1");
#endif
}

absl::StatusOr<AsarPatchResult> AsarWrapper::ApplyPatchWithBinary(
    const std::string& patch_path,
    std::vector<uint8_t>& rom_data,
    const std::vector<std::string>& include_paths) {
  last_errors_.clear();
  last_warnings_.clear();

  auto asar_path_opt = ResolveAsarBinary();
  if (!asar_path_opt) {
    return absl::FailedPreconditionError("Asar CLI path could not be resolved");
  }

  fs::path patch_fs = fs::absolute(patch_path);
  fs::path patch_dir = patch_fs.parent_path();
  fs::path patch_name = patch_fs.filename();

  // Write ROM to temporary file
  auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
  fs::path temp_rom = fs::temp_directory_path() /
                      ("yaze_asar_cli_" + std::to_string(timestamp) + ".sfc");
  {
    std::ofstream temp_rom_file(temp_rom, std::ios::binary);
    if (!temp_rom_file) {
      return absl::InternalError(
          "Failed to create temporary ROM file for Asar CLI");
    }
    temp_rom_file.write(reinterpret_cast<const char*>(rom_data.data()),
                        static_cast<std::streamsize>(rom_data.size()));
    if (!temp_rom_file) {
      return absl::InternalError("Failed to write temporary ROM file");
    }
  }

  // Build command
  std::ostringstream cmd;
  cmd << "\"" << *asar_path_opt << "\" --no-progress";
  for (const auto& include_path : include_paths) {
    cmd << " -I\"" << include_path << "\"";
  }
  cmd << " \"" << patch_name.string() << "\" \"" << temp_rom.string() << "\"";

  // Run in patch directory so relative incsrc paths resolve naturally
  std::error_code ec;
  fs::path original_cwd = fs::current_path(ec);
  if (!patch_dir.empty()) {
    fs::current_path(patch_dir, ec);
  }

  int rc = std::system(cmd.str().c_str());

  if (!patch_dir.empty()) {
    fs::current_path(original_cwd, ec);
  }

  if (rc != 0) {
    last_errors_.push_back(
        absl::StrFormat("Asar CLI failed with exit code %d", rc));
    fs::remove(temp_rom, ec);
    return absl::InternalError(last_errors_.back());
  }

  // Read patched ROM back into memory
  std::ifstream patched_rom(temp_rom, std::ios::binary);
  if (!patched_rom) {
    last_errors_.push_back("Failed to read patched ROM from Asar CLI");
    fs::remove(temp_rom, ec);
    return absl::InternalError(last_errors_.back());
  }

  std::vector<uint8_t> new_data(
      (std::istreambuf_iterator<char>(patched_rom)),
      std::istreambuf_iterator<char>());
  rom_data.swap(new_data);
  fs::remove(temp_rom, ec);

  AsarPatchResult result;
  result.success = true;
  result.rom_size = static_cast<uint32_t>(rom_data.size());
  result.crc32 = 0;  // TODO: CRC32 for CLI path
  return result;
}

std::optional<std::string> AsarWrapper::ResolveAsarBinary() const {
  const char* env_path = std::getenv("YAZE_ASAR_PATH");
  if (env_path != nullptr) {
    fs::path env(env_path);
    std::error_code ec;
    if (fs::exists(env, ec)) {
      return env.string();
    }
  }

#ifdef YAZE_ASAR_STANDALONE_PATH
  {
    fs::path bundled(YAZE_ASAR_STANDALONE_PATH);
    std::error_code ec;
    if (!bundled.empty() && fs::exists(bundled, ec)) {
      return bundled.string();
    }
  }
#endif

  // Fallback to system asar in PATH
  return std::string("asar");
}

void AsarWrapper::ExtractSymbolsFromLastOperation() {
#ifdef YAZE_ENABLE_ASAR
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
#endif
}

AsarSymbol AsarWrapper::ConvertAsarSymbol(const void* asar_symbol_data) const {
  // This would convert from Asar's internal symbol representation
  // to our AsarSymbol struct. Implementation depends on Asar's API.
  AsarSymbol symbol;
  return symbol;
}

}  // namespace core
}  // namespace yaze
