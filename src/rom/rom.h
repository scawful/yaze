#ifndef YAZE_ROM_ROM_H
#define YAZE_ROM_ROM_H

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/gfx/types/snes_color.h"
#include "app/gfx/types/snes_tile.h"
#include "core/project.h"
#include "rom/rom_diagnostics.h"

namespace yaze {

/**
 * @brief The Rom class is used to load, save, and modify Rom data.
 * This is a generic SNES ROM container and does not contain game-specific logic.
 */
class Rom {
 public:
  struct SaveSettings {
    bool backup = false;
    bool save_new = false;
    std::string filename;
  };

  struct LoadOptions {
    bool strip_header = true;
    bool load_resource_labels = true;
    
    static LoadOptions Defaults() { return LoadOptions{}; }
  };

  Rom() = default;
  ~Rom() = default;

  absl::Status LoadFromFile(const std::string& filename,
                            const LoadOptions& options = LoadOptions::Defaults());
  absl::Status LoadFromData(const std::vector<uint8_t>& data,
                            const LoadOptions& options = LoadOptions::Defaults());

  absl::Status SaveToFile(const SaveSettings& settings);

  void Expand(int size) {
    rom_data_.resize(size);
    size_ = size;
  }

  void Close() {
    rom_data_.clear();
    size_ = 0;
  }

  // Raw access
  absl::StatusOr<uint8_t> ReadByte(int offset);
  absl::StatusOr<uint16_t> ReadWord(int offset);
  absl::StatusOr<uint32_t> ReadLong(int offset);
  absl::StatusOr<std::vector<uint8_t>> ReadByteVector(uint32_t offset,
                                                      uint32_t length) const;
  absl::StatusOr<gfx::Tile16> ReadTile16(uint32_t tile16_id, uint32_t tile16_ptr);

  absl::Status WriteTile16(int tile16_id, uint32_t tile16_ptr, const gfx::Tile16& tile);
  absl::Status WriteByte(int addr, uint8_t value);
  absl::Status WriteWord(int addr, uint16_t value);
  absl::Status WriteShort(int addr, uint16_t value);
  absl::Status WriteLong(uint32_t addr, uint32_t value);
  absl::Status WriteVector(int addr, std::vector<uint8_t> data);
  absl::Status WriteColor(uint32_t address, const gfx::SnesColor& color);

  template <typename... Args>
  absl::Status WriteTransaction(Args... args) {
    absl::Status status;
    ((status = WriteHelper(args)), ...);
    return status;
  }

  template <typename T, typename... Args>
  absl::Status ReadTransaction(T& var, int address, Args&&... args) {
    absl::Status status = ReadHelper<T>(var, address);
    if (!status.ok()) {
      return status;
    }
    if constexpr (sizeof...(args) > 0) {
      status = ReadTransaction(std::forward<Args>(args)...);
    }
    return status;
  }

  struct WriteAction {
    using ValueType =
        std::variant<int, uint8_t, uint16_t, short, std::vector<uint8_t>,
                     gfx::SnesColor, std::vector<gfx::SnesColor>>;
    int address;
    ValueType value;
  };

  virtual absl::Status WriteHelper(const WriteAction& action);

  template <typename T>
  absl::Status ReadHelper(T& var, int address) {
    if constexpr (std::is_same_v<T, uint8_t>) {
      auto result = ReadByte(address);
      if (!result.ok()) return result.status();
      var = *result;
    } else if constexpr (std::is_same_v<T, uint16_t>) {
      auto result = ReadWord(address);
      if (!result.ok()) return result.status();
      var = *result;
    } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
      auto result = ReadByteVector(address, var.size());
      if (!result.ok()) return result.status();
      var = *result;
    }
    return absl::OkStatus();
  }

  uint8_t& operator[](unsigned long i) {
    if (i >= size_)
      throw std::out_of_range("Rom index out of range");
    return rom_data_[i];
  }

  bool is_loaded() const { return !rom_data_.empty(); }
  bool dirty() const { return dirty_; }
  void set_dirty(bool dirty) { dirty_ = dirty; }
  void ClearDirty() { dirty_ = false; }
  
  auto title() const { return title_; }
  auto size() const { return size_; }
  auto data() const { return rom_data_.data(); }
  auto mutable_data() { return rom_data_.data(); }
  auto begin() { return rom_data_.begin(); }
  auto end() { return rom_data_.end(); }
  const auto& vector() const { return rom_data_; }
  auto& mutable_vector() { return rom_data_; }
  auto filename() const { return filename_; }
  auto set_filename(std::string_view name) { filename_ = name; }
  auto short_name() const { return short_name_; }

  // Resource labels are generic enough to keep here
  project::ResourceLabelManager* resource_label() {
    return &resource_label_manager_;
  }

 private:
  // Size of the ROM data.
  unsigned long size_ = 0;

  // Title of the ROM loaded from the header
  std::string title_ = "ROM not loaded";

  // Filename of the ROM
  std::string filename_;

  // Short name of the ROM
  std::string short_name_;

  // Full contiguous rom space
  std::vector<uint8_t> rom_data_;

  // Label manager for unique resource names.
  project::ResourceLabelManager resource_label_manager_;

  // True if there are unsaved changes
  bool dirty_ = false;
};

}  // namespace yaze

#endif  // YAZE_ROM_ROM_H

