#include "core/asar_wrapper.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#else
#include <process.h>
#endif

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
    const std::string& patch_path, std::vector<uint8_t>& rom_data,
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
    const std::string& patch_content, std::vector<uint8_t>& rom_data,
    const std::string& base_path) {
  if (!initialized_) {
    return absl::FailedPreconditionError("Asar not initialized");
  }

  // Reset previous state
  Reset();

  // Write patch content to temporary file
  // NOTE: Must be unique across processes. Tests run `ctest -jN` which launches
  // many AsarWrapper instances concurrently.
  static std::atomic<uint64_t> seq{0};
  const auto timestamp =
      std::chrono::steady_clock::now().time_since_epoch().count();
  const uint64_t nonce = seq.fetch_add(1, std::memory_order_relaxed);
#ifdef _WIN32
  const int pid = _getpid();
#else
  const int pid = getpid();
#endif
  fs::path temp_patch_path =
      fs::temp_directory_path() /
      absl::StrFormat("yaze_asar_temp_%d_%lld_%llu.asm", pid,
                      static_cast<long long>(timestamp),
                      static_cast<unsigned long long>(nonce));

  std::ofstream temp_patch_file(temp_patch_path);
  if (!temp_patch_file) {
    return absl::InternalError("Failed to create temporary patch file");
  }

  temp_patch_file << patch_content;
  temp_patch_file.close();

  // Apply patch using temporary file
  auto patch_result =
      ApplyPatch(temp_patch_path.string(), rom_data, {base_path});

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

std::optional<AsarSymbol> AsarWrapper::FindSymbol(
    const std::string& name) const {
  auto it = symbol_table_.find(name);
  if (it != symbol_table_.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::vector<AsarSymbol> AsarWrapper::GetSymbolsAtAddress(
    uint32_t address) const {
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

absl::Status AsarWrapper::CreatePatch(const std::vector<uint8_t>& original_rom,
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
        "Assembly validation failed: %s", absl::StrJoin(result->errors, "; ")));
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
    const std::string& patch_path, std::vector<uint8_t>& rom_data,
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
  bool patch_success =
      asar_patch(patch_path.c_str(), reinterpret_cast<char*>(rom_data.data()),
                 buffer_size, &rom_size);

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

  return absl::InternalError(
      absl::StrFormat("Patch failed: %s", absl::StrJoin(last_errors_, "; ")));
#else
  (void)patch_path;
  (void)rom_data;
  (void)include_paths;
  return absl::UnimplementedError(
      "ASAR library not enabled - build with YAZE_ENABLE_ASAR=1");
#endif
}

absl::StatusOr<AsarPatchResult> AsarWrapper::ApplyPatchWithBinary(
    const std::string& patch_path, std::vector<uint8_t>& rom_data,
    const std::vector<std::string>& include_paths) {
  last_errors_.clear();
  last_warnings_.clear();

  std::error_code ec;
  fs::path patch_fs = fs::absolute(patch_path, ec);
  if (ec) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Failed to resolve patch path: %s", ec.message()));
  }
  if (!fs::exists(patch_fs)) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Patch file not found: %s", patch_fs.string()));
  }

  auto asar_path_opt = ResolveAsarBinary();
  if (!asar_path_opt) {
    return absl::FailedPreconditionError("Asar CLI path could not be resolved");
  }

  fs::path patch_dir = patch_fs.parent_path();
  fs::path patch_name = patch_fs.filename();

  // Write ROM to temporary file
  auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
  fs::path temp_rom = fs::temp_directory_path() /
                      ("yaze_asar_cli_" + std::to_string(timestamp) + ".sfc");
  fs::path temp_symbols =
      fs::temp_directory_path() /
      ("yaze_asar_symbols_" + std::to_string(timestamp) + ".sym");
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
  cmd << "\"" << *asar_path_opt << "\" --symbols=wla"
      << " --symbols-path=\"" << temp_symbols.string() << "\"";
  for (const auto& include_path : include_paths) {
    cmd << " -I\"" << include_path << "\"";
  }
  // Capture stderr
  cmd << " \"" << patch_name.string() << "\" \"" << temp_rom.string()
      << "\" 2>&1";

  // Run in patch directory so relative incsrc paths resolve naturally
  fs::path original_cwd = fs::current_path(ec);
  if (!patch_dir.empty()) {
    fs::current_path(patch_dir, ec);
  }

  // Execute using popen to capture output
  std::array<char, 128> buffer;
  std::string output;
#ifdef _WIN32
  FILE* pipe = _popen(cmd.str().c_str(), "r");
#else
  FILE* pipe = popen(cmd.str().c_str(), "r");
#endif
  if (!pipe) {
    fs::remove(temp_rom, ec);
    fs::remove(temp_symbols, ec);
    if (!patch_dir.empty())
      fs::current_path(original_cwd, ec);
    return absl::InternalError("popen() failed for Asar CLI");
  }
  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
    output += buffer.data();
  }
#ifndef _WIN32
  int exit_status = pclose(pipe);
  int exit_code = exit_status;
  if (exit_status != -1) {
    if (WIFEXITED(exit_status)) {
      exit_code = WEXITSTATUS(exit_status);
    } else if (WIFSIGNALED(exit_status)) {
      exit_code = 128 + WTERMSIG(exit_status);
    }
  }
#else
  int exit_code = _pclose(pipe);
#endif

  if (!patch_dir.empty()) {
    fs::current_path(original_cwd, ec);
  }

  // Parse output for errors/warnings
  // Asar format: "file.asm:line: error: message"
  std::istringstream output_stream(output);
  std::string line;
  while (std::getline(output_stream, line)) {
    if (line.find("error: ") != std::string::npos ||
        line.find(": error:") != std::string::npos) {
      last_errors_.push_back(line);
    } else if (line.find("warning: ") != std::string::npos) {
      last_warnings_.push_back(line);
    }
  }

  if (exit_code != 0 && last_errors_.empty()) {
    last_errors_.push_back(
        absl::StrFormat("Asar CLI exited with status %d", exit_code));
  }

  if (!last_errors_.empty()) {
    fs::remove(temp_rom, ec);
    fs::remove(temp_symbols, ec);
    return absl::InternalError(
        absl::StrFormat("Patch failed: %s", absl::StrJoin(last_errors_, "; ")));
  }

  // Read patched ROM back into memory
  std::ifstream patched_rom(temp_rom, std::ios::binary);
  if (!patched_rom) {
    last_errors_.push_back("Failed to read patched ROM from Asar CLI");
    fs::remove(temp_rom, ec);
    fs::remove(temp_symbols, ec);
    return absl::InternalError(last_errors_.back());
  }

  std::vector<uint8_t> new_data((std::istreambuf_iterator<char>(patched_rom)),
                                std::istreambuf_iterator<char>());
  rom_data.swap(new_data);
  fs::remove(temp_rom, ec);

  if (fs::exists(temp_symbols, ec)) {
    auto symbol_status = LoadSymbolsFromFile(temp_symbols.string());
    if (!symbol_status.ok()) {
      last_warnings_.push_back(std::string(symbol_status.message()));
    }
    fs::remove(temp_symbols, ec);
  }

  AsarPatchResult result;
  result.success = true;
  result.errors = last_errors_;
  result.warnings = last_warnings_;
  if (!symbol_table_.empty()) {
    result.symbols.reserve(symbol_table_.size());
    for (const auto& [name, symbol] : symbol_table_) {
      result.symbols.push_back(symbol);
    }
  }
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

absl::Status AsarWrapper::LoadSymbolsFromFile(const std::string& symbol_path) {
  std::ifstream file(symbol_path);
  if (!file.is_open()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Cannot open symbol file: %s", symbol_path));
  }

  symbol_table_.clear();
  std::string line;
  while (std::getline(file, line)) {
    // WLA format: bank:addr label
    // Example: 00:8000 Reset

    // Skip sections or comments
    if (line.empty() || line[0] == '[' || line[0] == ';')
      continue;

    size_t colon_pos = line.find(':');
    if (colon_pos == std::string::npos || colon_pos != 2)
      continue;

    size_t space_pos = line.find(' ', colon_pos);
    if (space_pos == std::string::npos)
      continue;

    try {
      std::string bank_str = line.substr(0, colon_pos);
      std::string addr_str =
          line.substr(colon_pos + 1, space_pos - (colon_pos + 1));
      std::string name = line.substr(space_pos + 1);

      // Trim name
      name.erase(0, name.find_first_not_of(" \t\r\n"));
      name.erase(name.find_last_not_of(" \t\r\n") + 1);

      int bank = std::stoi(bank_str, nullptr, 16);
      int addr = std::stoi(addr_str, nullptr, 16);
      uint32_t full_addr = (bank << 16) | addr;

      AsarSymbol symbol;
      symbol.name = name;
      symbol.address = full_addr;

      symbol_table_[name] = symbol;
    } catch (...) {
      // Parse error, skip line
      continue;
    }
  }

  return absl::OkStatus();
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
    symbol.file = "";    // Not available in basic API
    symbol.line = 0;     // Not available in basic API
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
