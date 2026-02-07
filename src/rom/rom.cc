#include "rom.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <new>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "app/gfx/types/snes_color.h"
#include "app/gfx/types/snes_tile.h"
#include "util/hex.h"
#include "util/log.h"
#include "util/macro.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include "app/platform/wasm/wasm_collaboration.h"
#endif

#if !defined(__EMSCRIPTEN__)
#if defined(_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif
#endif

namespace yaze {

namespace {

// ============================================================================
// ROM Structure Constants
// ============================================================================

/// Standard SNES ROM size for The Legend of Zelda: A Link to the Past (1MB)
constexpr size_t kBaseRomSize = 1048576;

/// Size of the optional SMC/SFC copier header that some ROM dumps include.
constexpr size_t kHeaderSize = 0x200;  // 512 bytes

// ============================================================================
// SMC Header Detection and Removal
// ============================================================================

void MaybeStripSmcHeader(std::vector<uint8_t>& rom_data, unsigned long& size) {
  if (size % kBaseRomSize == kHeaderSize && size >= kHeaderSize &&
      rom_data.size() >= kHeaderSize) {
    rom_data.erase(rom_data.begin(), rom_data.begin() + kHeaderSize);
    size -= kHeaderSize;
    LOG_INFO("Rom", "Stripped SMC header from ROM (new size: %lu)", size);
  }
}

#ifdef __EMSCRIPTEN__
inline void MaybeBroadcastChange(uint32_t offset,
                                 const std::vector<uint8_t>& old_bytes,
                                 const std::vector<uint8_t>& new_bytes) {
  if (new_bytes.empty())
    return;
  auto& collab = app::platform::GetWasmCollaborationInstance();
  if (!collab.IsConnected() || collab.IsApplyingRemoteChange()) {
    return;
  }
  (void)collab.BroadcastChange(offset, old_bytes, new_bytes);
}
#endif

#if !defined(__EMSCRIPTEN__)
void BestEffortFsyncFile(const std::filesystem::path& path) {
#if defined(_WIN32)
  // FlushFileBuffers requires GENERIC_WRITE access.
  HANDLE handle = CreateFileW(path.wstring().c_str(), GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE |
                                  FILE_SHARE_DELETE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                              nullptr);
  if (handle == INVALID_HANDLE_VALUE) {
    return;
  }
  (void)FlushFileBuffers(handle);
  (void)CloseHandle(handle);
#else
  int fd = open(path.c_str(), O_RDONLY);
  if (fd < 0) {
    return;
  }
  (void)fsync(fd);
  (void)close(fd);
#endif
}

void BestEffortFsyncParentDir(const std::filesystem::path& file_path) {
#if defined(_WIN32)
  (void)file_path;
  // Best-effort only; Windows directory fsync is not portable here.
#else
  std::filesystem::path dir_path = file_path.parent_path();
  if (dir_path.empty()) {
    dir_path = ".";
  }
  int fd = open(dir_path.c_str(), O_RDONLY);
  if (fd < 0) {
    return;
  }
  (void)fsync(fd);
  (void)close(fd);
#endif
}
#endif  // !defined(__EMSCRIPTEN__)

}  // namespace

absl::Status Rom::LoadFromFile(const std::string& filename,
                               const LoadOptions& options) {
  if (filename.empty()) {
    return absl::InvalidArgumentError(
        "Could not load ROM: parameter `filename` is empty.");
  }

#ifdef __EMSCRIPTEN__
  filename_ = filename;
  std::ifstream test_file(filename_, std::ios::binary);
  if (!test_file.is_open()) {
    return absl::NotFoundError(absl::StrCat(
        "ROM file does not exist or cannot be opened: ", filename_));
  }
  test_file.seekg(0, std::ios::end);
  size_ = test_file.tellg();
  test_file.close();

  if (size_ < 32768) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "ROM file too small (%zu bytes), minimum is 32KB", size_));
  }
#else
  if (!std::filesystem::exists(filename)) {
    return absl::NotFoundError(
        absl::StrCat("ROM file does not exist: ", filename));
  }
  filename_ = std::filesystem::absolute(filename).string();
#endif
  short_name_ = filename_.substr(filename_.find_last_of("/\\") + 1);

  std::ifstream file(filename_, std::ios::binary);
  if (!file.is_open()) {
    return absl::NotFoundError(
        absl::StrCat("Could not open ROM file: ", filename_));
  }

#ifndef __EMSCRIPTEN__
  try {
    size_ = std::filesystem::file_size(filename_);
    if (size_ < 32768) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "ROM file too small (%zu bytes), minimum is 32KB", size_));
    }
  } catch (...) {
    file.seekg(0, std::ios::end);
    size_ = file.tellg();
  }
#endif

  try {
    rom_data_.resize(size_);
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(rom_data_.data()), size_);
  } catch (const std::bad_alloc& e) {
    return absl::ResourceExhaustedError(absl::StrFormat(
        "Failed to allocate memory for ROM (%zu bytes)", size_));
  }

  file.close();

  if (options.strip_header) {
    MaybeStripSmcHeader(rom_data_, size_);
  }
  size_ = rom_data_.size();

  if (options.load_resource_labels) {
    resource_label_manager_.LoadLabels(absl::StrFormat("%s.labels", filename));
  }

  // Parse SNES Header for Title
  if (rom_data_.size() >= 0x8000) {
    // Check LoROM (0x7FC0) vs HiROM (0xFFC0)
    // Simple heuristic: Z3 is LoROM
    size_t header_offset = 0x7FC0;
    if (rom_data_.size() >= 0x10000) {
        // Compute checksums to verify?
        // For now default to LoROM
    }
    
    if (header_offset + 21 <= rom_data_.size()) {
        char buffer[22] = {0};
        for (int i = 0; i < 21; ++i) {
            uint8_t c = rom_data_[header_offset + i];
            buffer[i] = (c >= 32 && c <= 126) ? c : ' ';
        }
        title_ = std::string(buffer);
        // Trim trailing spaces safely
        auto last_non_space = title_.find_last_not_of(' ');
        if (last_non_space == std::string::npos) {
          title_.clear();
        } else {
          title_.erase(last_non_space + 1);
        }
    }
  }

  return absl::OkStatus();
}

absl::Status Rom::LoadFromData(const std::vector<uint8_t>& data,
                               const LoadOptions& options) {
  if (data.empty()) {
    return absl::InvalidArgumentError(
        "Could not load ROM: parameter `data` is empty.");
  }
  rom_data_ = data;
  size_ = data.size();

  if (options.strip_header) {
    MaybeStripSmcHeader(rom_data_, size_);
  }
  size_ = rom_data_.size();

  // Parse SNES Header for Title
  if (rom_data_.size() >= 0x8000) {
    size_t header_offset = 0x7FC0;
    if (header_offset + 21 <= rom_data_.size()) {
        char buffer[22] = {0};
        for (int i = 0; i < 21; ++i) {
            uint8_t c = rom_data_[header_offset + i];
            buffer[i] = (c >= 32 && c <= 126) ? c : ' ';
        }
        title_ = std::string(buffer);
        auto last_non_space = title_.find_last_not_of(' ');
        if (last_non_space == std::string::npos) {
          title_.clear();
        } else {
          title_.erase(last_non_space + 1);
        }
    }
  }

  return absl::OkStatus();
}

absl::Status Rom::SaveToFile(const SaveSettings& settings) {
  if (rom_data_.empty()) {
    return absl::InternalError("ROM data is empty.");
  }

  std::string filename = settings.filename;
  if (filename.empty()) {
    filename = filename_;
  }

  if (settings.backup) {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::string backup_filename =
        absl::StrCat(filename, "_backup_", std::ctime(&now_c));

    backup_filename.erase(
        std::remove(backup_filename.begin(), backup_filename.end(), '\n'),
        backup_filename.end());
    std::replace(backup_filename.begin(), backup_filename.end(), ' ', '_');

    try {
      std::filesystem::copy(filename_, backup_filename,
                            std::filesystem::copy_options::overwrite_existing);
    } catch (const std::filesystem::filesystem_error& e) {
      LOG_WARN("Rom", "Could not create backup: %s", e.what());
    }
  }

  if (settings.save_new) {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    auto filename_no_ext = filename.substr(0, filename.find_last_of("."));
    filename = absl::StrCat(filename_no_ext, "_", std::ctime(&now_c));
    filename.erase(std::remove(filename.begin(), filename.end(), ' '),
                   filename.end());
    filename.erase(std::remove(filename.begin(), filename.end(), '\n'),
                   filename.end());
    filename = filename + ".sfc";
  }

  // Save stability: write to a temp file in the same directory and rename into
  // place. If we crash mid-write, the original ROM stays intact.
  const std::filesystem::path target_path(filename);
  std::filesystem::path temp_path = target_path;
  temp_path += ".tmp";

  std::ofstream file(temp_path, std::ios::binary | std::ios::trunc);
  if (!file) {
    return absl::InternalError(absl::StrCat(
        "Could not open temp ROM file for writing: ", temp_path.string()));
  }

  file.write(reinterpret_cast<const char*>(rom_data_.data()), rom_data_.size());
  file.flush();
  if (!file) {
    file.close();
    std::error_code rm_ec;
    std::filesystem::remove(temp_path, rm_ec);
    return absl::InternalError(absl::StrCat("Error while writing ROM file: ",
                                            temp_path.string()));
  }

  file.close();

#if !defined(__EMSCRIPTEN__)
  // Best-effort fsync so temp file contents are durable before rename.
  BestEffortFsyncFile(temp_path);
#endif

  std::error_code rename_ec;
  std::filesystem::rename(temp_path, target_path, rename_ec);
#if defined(_WIN32)
  // Windows may fail rename when the destination exists; fall back to remove +
  // rename (non-atomic but still avoids partial/truncated writes).
  if (rename_ec && std::filesystem::exists(target_path)) {
    std::error_code rm_target_ec;
    std::filesystem::remove(target_path, rm_target_ec);
    rename_ec.clear();
    std::filesystem::rename(temp_path, target_path, rename_ec);
  }
#endif
  if (rename_ec) {
    std::error_code rm_ec;
    std::filesystem::remove(temp_path, rm_ec);
    return absl::InternalError(absl::StrCat(
        "Failed to move temp ROM into place: ", rename_ec.message()));
  }

#if !defined(__EMSCRIPTEN__)
  // Best-effort fsync the parent dir so the rename is durable.
  BestEffortFsyncParentDir(target_path);
#endif

  dirty_ = false;
  return absl::OkStatus();
}

absl::StatusOr<uint8_t> Rom::ReadByte(int offset) const {
  if (offset < 0 || offset >= static_cast<int>(rom_data_.size())) {
    return absl::OutOfRangeError(absl::StrFormat(
        "Offset %d out of range (size: %d)", offset, rom_data_.size()));
  }
  return rom_data_[offset];
}

absl::StatusOr<uint16_t> Rom::ReadWord(int offset) const {
  if (offset < 0 || offset + 1 >= static_cast<int>(rom_data_.size())) {
    return absl::OutOfRangeError("Offset out of range");
  }
  return (uint16_t)(rom_data_[offset] | (rom_data_[offset + 1] << 8));
}

absl::StatusOr<uint32_t> Rom::ReadLong(int offset) const {
  if (offset < 0 || offset + 2 >= static_cast<int>(rom_data_.size())) {
    return absl::OutOfRangeError("Offset out of range");
  }
  return (uint32_t)(rom_data_[offset] | (rom_data_[offset + 1] << 8) |
                    (rom_data_[offset + 2] << 16));
}

absl::StatusOr<std::vector<uint8_t>> Rom::ReadByteVector(
    uint32_t offset, uint32_t length) const {
  if (offset + length > static_cast<uint32_t>(rom_data_.size())) {
    return absl::OutOfRangeError("Offset and length out of range");
  }
  std::vector<uint8_t> result;
  result.reserve(length);
  for (uint32_t i = offset; i < offset + length; i++) {
    result.push_back(rom_data_[i]);
  }
  return result;
}

absl::StatusOr<gfx::Tile16> Rom::ReadTile16(uint32_t tile16_id,
                                            uint32_t tile16_ptr) {
  // Skip 8 bytes per tile.
  auto tpos = tile16_ptr + (tile16_id * 0x08);
  gfx::Tile16 tile16 = {};
  ASSIGN_OR_RETURN(auto new_tile0, ReadWord(tpos));
  tile16.tile0_ = gfx::WordToTileInfo(new_tile0);
  tpos += 2;
  ASSIGN_OR_RETURN(auto new_tile1, ReadWord(tpos));
  tile16.tile1_ = gfx::WordToTileInfo(new_tile1);
  tpos += 2;
  ASSIGN_OR_RETURN(auto new_tile2, ReadWord(tpos));
  tile16.tile2_ = gfx::WordToTileInfo(new_tile2);
  tpos += 2;
  ASSIGN_OR_RETURN(auto new_tile3, ReadWord(tpos));
  tile16.tile3_ = gfx::WordToTileInfo(new_tile3);
  return tile16;
}

absl::Status Rom::WriteTile16(int tile16_id, uint32_t tile16_ptr,
                              const gfx::Tile16& tile) {
  auto tpos = tile16_ptr + (tile16_id * 0x08);
  RETURN_IF_ERROR(WriteShort(tpos, gfx::TileInfoToWord(tile.tile0_)));
  tpos += 2;
  RETURN_IF_ERROR(WriteShort(tpos, gfx::TileInfoToWord(tile.tile1_)));
  tpos += 2;
  RETURN_IF_ERROR(WriteShort(tpos, gfx::TileInfoToWord(tile.tile2_)));
  tpos += 2;
  RETURN_IF_ERROR(WriteShort(tpos, gfx::TileInfoToWord(tile.tile3_)));
  return absl::OkStatus();
}

absl::Status Rom::WriteByte(int addr, uint8_t value) {
  if (addr >= static_cast<int>(rom_data_.size())) {
    return absl::OutOfRangeError("Address out of range");
  }
  const uint8_t old_val = rom_data_[addr];
  rom_data_[addr] = value;
  dirty_ = true;
#ifdef __EMSCRIPTEN__
  MaybeBroadcastChange(addr, {old_val}, {value});
#endif
  return absl::OkStatus();
}

absl::Status Rom::WriteWord(int addr, uint16_t value) {
  if (addr + 1 >= static_cast<int>(rom_data_.size())) {
    return absl::OutOfRangeError("Address out of range");
  }
  const uint8_t old0 = rom_data_[addr];
  const uint8_t old1 = rom_data_[addr + 1];
  rom_data_[addr] = (uint8_t)(value & 0xFF);
  rom_data_[addr + 1] = (uint8_t)((value >> 8) & 0xFF);
  dirty_ = true;
#ifdef __EMSCRIPTEN__
  MaybeBroadcastChange(addr, {old0, old1},
                       {static_cast<uint8_t>(value & 0xFF),
                        static_cast<uint8_t>((value >> 8) & 0xFF)});
#endif
  return absl::OkStatus();
}

absl::Status Rom::WriteShort(int addr, uint16_t value) {
  return WriteWord(addr, value);
}

absl::Status Rom::WriteLong(uint32_t addr, uint32_t value) {
  if (addr + 2 >= static_cast<uint32_t>(rom_data_.size())) {
    return absl::OutOfRangeError("Address out of range");
  }
  const uint8_t old0 = rom_data_[addr];
  const uint8_t old1 = rom_data_[addr + 1];
  const uint8_t old2 = rom_data_[addr + 2];
  rom_data_[addr] = (uint8_t)(value & 0xFF);
  rom_data_[addr + 1] = (uint8_t)((value >> 8) & 0xFF);
  rom_data_[addr + 2] = (uint8_t)((value >> 16) & 0xFF);
  dirty_ = true;
#ifdef __EMSCRIPTEN__
  MaybeBroadcastChange(addr, {old0, old1, old2},
                       {static_cast<uint8_t>(value & 0xFF),
                        static_cast<uint8_t>((value >> 8) & 0xFF),
                        static_cast<uint8_t>((value >> 16) & 0xFF)});
#endif
  return absl::OkStatus();
}

absl::Status Rom::WriteVector(int addr, std::vector<uint8_t> data) {
  if (addr + static_cast<int>(data.size()) >
      static_cast<int>(rom_data_.size())) {
    return absl::OutOfRangeError("Address out of range");
  }
  std::vector<uint8_t> old_data;
  old_data.reserve(data.size());
  for (int i = 0; i < static_cast<int>(data.size()); i++) {
    old_data.push_back(rom_data_[addr + i]);
    rom_data_[addr + i] = data[i];
  }
  dirty_ = true;
#ifdef __EMSCRIPTEN__
  MaybeBroadcastChange(addr, old_data, data);
#endif
  return absl::OkStatus();
}

absl::Status Rom::WriteColor(uint32_t address, const gfx::SnesColor& color) {
  uint16_t bgr = ((color.snes() >> 10) & 0x1F) | ((color.snes() & 0x1F) << 10) |
                 (color.snes() & 0x7C00);
  return WriteWord(address, bgr);
}

absl::Status Rom::WriteHelper(const WriteAction& action) {
  if (std::holds_alternative<uint8_t>(action.value)) {
    return WriteByte(action.address, std::get<uint8_t>(action.value));
  } else if (std::holds_alternative<uint16_t>(action.value) ||
             std::holds_alternative<short>(action.value)) {
    return WriteShort(action.address, std::get<uint16_t>(action.value));
  } else if (std::holds_alternative<std::vector<uint8_t>>(action.value)) {
    return WriteVector(action.address,
                       std::get<std::vector<uint8_t>>(action.value));
  } else if (std::holds_alternative<gfx::SnesColor>(action.value)) {
    return WriteColor(action.address, std::get<gfx::SnesColor>(action.value));
  }
  return absl::InvalidArgumentError("Invalid write argument type");
}

}  // namespace yaze
