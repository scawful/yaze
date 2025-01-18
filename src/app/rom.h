#ifndef YAZE_APP_ROM_H
#define YAZE_APP_ROM_H

#include <SDL.h>
#include <zelda.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "app/core/common.h"
#include "app/core/constants.h"
#include "app/core/project.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"

namespace yaze {

constexpr uint32_t kNumGfxSheets = 223;
constexpr uint32_t kNumLinkSheets = 14;
constexpr uint32_t kTile16Ptr = 0x78000;
constexpr uint32_t kNormalGfxSpaceStart = 0x87000;
constexpr uint32_t kNormalGfxSpaceEnd = 0xC4200;
constexpr uint32_t kFontSpriteLocation = 0x70000;
constexpr uint32_t kGfxGroupsPointer = 0x6237;
constexpr uint32_t kUncompressedSheetSize = 0x0800;
constexpr uint32_t kNumMainBlocksets = 37;
constexpr uint32_t kNumRoomBlocksets = 82;
constexpr uint32_t kNumSpritesets = 144;
constexpr uint32_t kNumPalettesets = 72;
constexpr uint32_t kEntranceGfxGroup = 0x5D97;

// TODO: Verify what this was used for in ZS
constexpr uint32_t kMaxGraphics = 0xC3FB5;

/**
 * @brief A map of version constants for each version of the game.
 */
static const std::map<zelda3_version, zelda3_version_pointers>
    kVersionConstantsMap = {
        {zelda3_version::US, zelda3_us_pointers},
        {zelda3_version::JP, zelda3_jp_pointers},
        {zelda3_version::SD, {}},
        {zelda3_version::RANDO, {}},
};

/**
 * @brief The Rom class is used to load, save, and modify Rom data.
 */
class Rom {
 public:
  /**
   * Load Rom data from a file.
   *
   * @param filename The name of the file to load.
   * @param z3_load Whether to load data specific to Zelda 3.
   *
   */
  absl::Status LoadFromFile(const std::string& filename, bool z3_load = true);
  absl::Status LoadFromPointer(uchar* data, size_t length, bool z3_load = true);
  absl::Status LoadFromBytes(const std::vector<uint8_t>& data);

  /**
   * @brief Saves the Rom data to a file
   *
   * @param backup If true, creates a backup file with timestamp in its name
   * @param filename The name of the file to save the Rom data to
   *
   * @return absl::Status Returns an OK status if the save was successful,
   * otherwise returns an error status
   */
  absl::Status SaveToFile(bool backup, bool save_new = false,
                          std::string filename = "");

  /**
   * Saves the given palette to the Rom if any of its colors have been modified.
   *
   * @param index The index of the palette to save.
   * @param group_name The name of the group containing the palette.
   * @param palette The palette to save.
   */
  absl::Status SavePalette(int index, const std::string& group_name,
                           gfx::SnesPalette& palette);

  /**
   * @brief Saves all palettes in the Rom.
   *
   * This function iterates through all palette groups and all palettes in each
   * group, and saves each palette using the SavePalette() function.
   */
  absl::Status SaveAllPalettes();

  /**
   * @brief Expand the Rom data to a specified size.
   */
  void Expand(int size) {
    rom_data_.resize(size);
    size_ = size;
  }

  /**
   *  @brief Close the Rom file.
   */
  absl::Status Close() {
    rom_data_.clear();
    size_ = 0;
    is_loaded_ = false;
    return absl::OkStatus();
  }

  /**
   * @brief Precondition check for reading and writing to the Rom.
   */
  absl::Status ReadWritePreconditions() const {
    if (!is_loaded_) {
      return absl::FailedPreconditionError("ROM file not loaded");
    }
    if (rom_data_.empty() || size_ == 0) {
      return absl::FailedPreconditionError(
          "File was loaded, but ROM data was empty.");
    }
    return absl::OkStatus();
  }

  absl::StatusOr<uint8_t> ReadByte(int offset) {
    RETURN_IF_ERROR(ReadWritePreconditions());
    if (offset >= static_cast<int>(rom_data_.size())) {
      return absl::FailedPreconditionError("Offset out of range");
    }
    return rom_data_[offset];
  }

  absl::StatusOr<uint16_t> ReadWord(int offset) {
    RETURN_IF_ERROR(ReadWritePreconditions());
    if (offset + 1 >= static_cast<int>(rom_data_.size())) {
      return absl::FailedPreconditionError("Offset out of range");
    }
    auto result = (uint16_t)(rom_data_[offset] | (rom_data_[offset + 1] << 8));
    return result;
  }

  absl::StatusOr<uint32_t> ReadLong(int offset) {
    RETURN_IF_ERROR(ReadWritePreconditions());
    if (offset + 2 >= static_cast<int>(rom_data_.size())) {
      return absl::OutOfRangeError("Offset out of range");
    }
    auto result = (uint32_t)(rom_data_[offset] | (rom_data_[offset + 1] << 8) |
                             (rom_data_[offset + 2] << 16));
    return result;
  }

  absl::StatusOr<std::vector<uint8_t>> ReadByteVector(uint32_t offset,
                                                      uint32_t length) const {
    RETURN_IF_ERROR(ReadWritePreconditions());
    if (offset + length > static_cast<uint32_t>(rom_data_.size())) {
      return absl::OutOfRangeError("Offset and length out of range");
    }
    std::vector<uint8_t> result;
    for (uint32_t i = offset; i < offset + length; i++) {
      result.push_back(rom_data_[i]);
    }
    return result;
  }

  absl::StatusOr<gfx::Tile16> ReadTile16(uint32_t tile16_id) {
    // Skip 8 bytes per tile.
    auto tpos = kTile16Ptr + (tile16_id * 0x08);
    gfx::Tile16 tile16 = {};
    ASSIGN_OR_RETURN(auto new_tile0, ReadWord(tpos))
    tile16.tile0_ = gfx::WordToTileInfo(new_tile0);
    tpos += 2;
    ASSIGN_OR_RETURN(auto new_tile1, ReadWord(tpos))
    tile16.tile1_ = gfx::WordToTileInfo(new_tile1);
    tpos += 2;
    ASSIGN_OR_RETURN(auto new_tile2, ReadWord(tpos))
    tile16.tile2_ = gfx::WordToTileInfo(new_tile2);
    tpos += 2;
    ASSIGN_OR_RETURN(auto new_tile3, ReadWord(tpos))
    tile16.tile3_ = gfx::WordToTileInfo(new_tile3);
    return tile16;
  }

  absl::Status WriteTile16(int tile16_id, const gfx::Tile16& tile) {
    // Skip 8 bytes per tile.
    auto tpos = kTile16Ptr + (tile16_id * 0x08);
    RETURN_IF_ERROR(WriteShort(tpos, gfx::TileInfoToWord(tile.tile0_)));
    tpos += 2;
    RETURN_IF_ERROR(WriteShort(tpos, gfx::TileInfoToWord(tile.tile1_)));
    tpos += 2;
    RETURN_IF_ERROR(WriteShort(tpos, gfx::TileInfoToWord(tile.tile2_)));
    tpos += 2;
    RETURN_IF_ERROR(WriteShort(tpos, gfx::TileInfoToWord(tile.tile3_)));
    return absl::OkStatus();
  }

  absl::Status WriteByte(int addr, uint8_t value) {
    RETURN_IF_ERROR(ReadWritePreconditions());
    if (addr >= static_cast<int>(rom_data_.size())) {
      return absl::OutOfRangeError(absl::StrFormat(
          "Attempt to write byte %#02x value failed, address %d out of range",
          value, addr));
    }
    rom_data_[addr] = value;
    core::logf("WriteByte: %#06X: %s", addr, core::HexByte(value).data());
    return absl::OkStatus();
  }

  absl::Status WriteWord(int addr, uint16_t value) {
    RETURN_IF_ERROR(ReadWritePreconditions());
    if (addr + 1 >= static_cast<int>(rom_data_.size())) {
      return absl::OutOfRangeError(absl::StrFormat(
          "Attempt to write word %#04x value failed, address %d out of range",
          value, addr));
    }
    rom_data_[addr] = (uint8_t)(value & 0xFF);
    rom_data_[addr + 1] = (uint8_t)((value >> 8) & 0xFF);
    core::logf("WriteWord: %#06X: %s", addr, core::HexWord(value).data());
    return absl::OkStatus();
  }

  absl::Status WriteShort(int addr, uint16_t value) {
    RETURN_IF_ERROR(ReadWritePreconditions());
    if (addr + 1 >= static_cast<int>(rom_data_.size())) {
      return absl::OutOfRangeError(absl::StrFormat(
          "Attempt to write short %#04x value failed, address %d out of range",
          value, addr));
    }
    rom_data_[addr] = (uint8_t)(value & 0xFF);
    rom_data_[addr + 1] = (uint8_t)((value >> 8) & 0xFF);
    core::logf("WriteShort: %#06X: %s", addr, core::HexWord(value).data());
    return absl::OkStatus();
  }

  absl::Status WriteLong(uint32_t addr, uint32_t value) {
    RETURN_IF_ERROR(ReadWritePreconditions());
    if (addr + 2 >= static_cast<uint32_t>(rom_data_.size())) {
      return absl::OutOfRangeError(absl::StrFormat(
          "Attempt to write long %#06x value failed, address %d out of range",
          value, addr));
    }
    rom_data_[addr] = (uint8_t)(value & 0xFF);
    rom_data_[addr + 1] = (uint8_t)((value >> 8) & 0xFF);
    rom_data_[addr + 2] = (uint8_t)((value >> 16) & 0xFF);
    core::logf("WriteLong: %#06X: %s", addr, core::HexLong(value).data());
    return absl::OkStatus();
  }

  absl::Status WriteVector(int addr, std::vector<uint8_t> data) {
    if (addr + static_cast<int>(data.size()) >
        static_cast<int>(rom_data_.size())) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Attempt to write vector value failed, address %d out of range",
          addr));
    }
    for (int i = 0; i < static_cast<int>(data.size()); i++) {
      rom_data_[addr + i] = data[i];
    }
    core::logf("WriteVector: %#06X: %s", addr, core::HexByte(data[0]).data());
    return absl::OkStatus();
  }

  absl::Status WriteColor(uint32_t address, const gfx::SnesColor& color) {
    uint16_t bgr = ((color.snes() >> 10) & 0x1F) |
                   ((color.snes() & 0x1F) << 10) | (color.snes() & 0x7C00);

    // Write the 16-bit color value to the ROM at the specified address
    core::logf("WriteColor: %#06X: %s", address, core::HexWord(bgr).data());
    return WriteShort(address, bgr);
  }

  template <typename... Args>
  absl::Status WriteTransaction(Args... args) {
    absl::Status status;
    // Fold expression to apply the Write function on each argument
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

  uint8_t& operator[](unsigned long i) {
    if (i > size_) {
      throw std::out_of_range("Rom index out of range");
    }
    return rom_data_[i];
  }

  bool is_loaded() const {
    if (!absl::StrContains(filename_, ".sfc") &&
        !absl::StrContains(filename_, ".smc")) {
      return false;
    }
    return is_loaded_;
  }

  auto title() const { return title_; }
  auto size() const { return size_; }
  auto data() const { return rom_data_.data(); }
  auto mutable_data() { return rom_data_.data(); }
  auto begin() { return rom_data_.begin(); }
  auto end() { return rom_data_.end(); }

  auto vector() const { return rom_data_; }
  auto version() const { return version_; }
  auto filename() const { return filename_; }
  auto set_filename(std::string name) { filename_ = name; }

  std::vector<uint8_t> graphics_buffer() const { return graphics_buffer_; }
  auto mutable_graphics_buffer() { return &graphics_buffer_; }
  auto palette_group() const { return palette_groups_; }
  auto mutable_palette_group() { return &palette_groups_; }
  auto dungeon_palette(int i) { return palette_groups_.dungeon_main[i]; }
  auto mutable_dungeon_palette(int i) {
    return palette_groups_.dungeon_main.mutable_palette(i);
  }

  ResourceLabelManager* resource_label() { return &resource_label_manager_; }
  zelda3_version_pointers version_constants() const {
    return kVersionConstantsMap.at(version_);
  }

  std::array<std::array<uint8_t, 8>, kNumMainBlocksets> main_blockset_ids;
  std::array<std::array<uint8_t, 4>, kNumRoomBlocksets> room_blockset_ids;
  std::array<std::array<uint8_t, 4>, kNumSpritesets> spriteset_ids;
  std::array<std::array<uint8_t, 4>, kNumPalettesets> paletteset_ids;

  struct WriteAction {
    int address;
    std::variant<int, uint8_t, uint16_t, short, std::vector<uint8_t>,
                 gfx::SnesColor, std::vector<gfx::SnesColor>>
        value;
  };

 private:
  virtual absl::Status WriteHelper(const WriteAction& action) {
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
    } else if (std::holds_alternative<std::vector<gfx::SnesColor>>(
                   action.value)) {
      return absl::UnimplementedError(
          "WriteHelper: std::vector<gfx::SnesColor>");
    }
    auto error_message = absl::StrFormat("Invalid write argument type: %s",
                                         typeid(action.value).name());
    return absl::InvalidArgumentError(error_message);
  }

  template <typename T>
  absl::Status ReadHelper(T& var, int address) {
    if constexpr (std::is_same_v<T, uint8_t>) {
      ASSIGN_OR_RETURN(auto result, ReadByte(address));
      var = result;
    } else if constexpr (std::is_same_v<T, uint16_t>) {
      ASSIGN_OR_RETURN(auto result, ReadWord(address));
      var = result;
    } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
      ASSIGN_OR_RETURN(auto result, ReadByteVector(address, var.size()));
      var = result;
    }
    return absl::OkStatus();
  }

  absl::Status LoadZelda3();
  absl::Status LoadGfxGroups();
  absl::Status SaveGroupsToRom();

  // ROM file loaded flag
  bool is_loaded_ = false;

  // Size of the ROM data.
  unsigned long size_ = 0;

  // Title of the ROM loaded from the header
  std::string title_ = "ROM not loaded";

  // Filename of the ROM
  std::string filename_ = "";

  // Full contiguous rom space
  std::vector<uint8_t> rom_data_;

  // Full contiguous graphics space
  std::vector<uint8_t> graphics_buffer_;

  // Label manager for unique resource names.
  ResourceLabelManager resource_label_manager_;

  // All palette groups in the game
  gfx::PaletteGroupMap palette_groups_;

  // Version of the game
  zelda3_version version_ = zelda3_version::US;
};

class GraphicsSheetManager {
 public:
  static GraphicsSheetManager& GetInstance() {
    static GraphicsSheetManager instance;
    return instance;
  }
  GraphicsSheetManager() = default;
  virtual ~GraphicsSheetManager() = default;
  std::array<gfx::Bitmap, kNumGfxSheets>& gfx_sheets() { return gfx_sheets_; }
  auto gfx_sheet(int i) { return gfx_sheets_[i]; }
  auto mutable_gfx_sheet(int i) { return &gfx_sheets_[i]; }
  auto mutable_gfx_sheets() { return &gfx_sheets_; }

 private:
  std::array<gfx::Bitmap, kNumGfxSheets> gfx_sheets_;
};

/**
 * @brief This function iterates over all graphics sheets in the Rom and loads
 * them into memory. Depending on the sheet's index, it may be uncompressed or
 * compressed using the LC-LZ2 algorithm. The uncompressed sheets are 3 bits
 * per pixel (BPP), while the compressed sheets are 4 BPP. The loaded graphics
 * data is converted to 8 BPP and stored in a bitmap.
 *
 * The graphics sheets are divided into the following ranges:
 *
 * | Range   | Compression Type | Decompressed Size | Number of Chars |
 * |---------|------------------|------------------|-----------------|
 * | 0-112   | Compressed 3bpp BGR | 0x600 chars | Decompressed each |
 * | 113-114 | Compressed 2bpp | 0x800 chars | Decompressed each |
 * | 115-126 | Uncompressed 3bpp sprites | 0x600 chars | Each |
 * | 127-217 | Compressed 3bpp sprites | 0x600 chars | Decompressed each |
 * | 218-222 | Compressed 2bpp | 0x800 chars | Decompressed each |
 *
 */
absl::StatusOr<std::array<gfx::Bitmap, kNumGfxSheets>> LoadAllGraphicsData(
    Rom& rom, bool defer_render = false);

absl::Status SaveAllGraphicsData(
    Rom& rom, std::array<gfx::Bitmap, kNumGfxSheets>& gfx_sheets);

/**
 * @brief Loads 2bpp graphics from Rom data.
 *
 * This function loads 2bpp graphics from Rom data by iterating over a list of
 * sheet IDs, decompressing the sheet data, converting it to 8bpp format, and
 * appending the converted sheet data to a byte vector.
 *
 */
absl::StatusOr<std::vector<uint8_t>> Load2BppGraphics(const Rom& rom);

/**
 * @brief Loads the players 4bpp graphics sheet from Rom data.
 */
absl::StatusOr<std::array<gfx::Bitmap, kNumLinkSheets>> LoadLinkGraphics(
    const Rom& rom);

/**
 * @brief A class to hold a shared pointer to a Rom object.
 */
class SharedRom {
 public:
  SharedRom() = default;
  virtual ~SharedRom() = default;

  std::shared_ptr<Rom> shared_rom() {
    if (!shared_rom_) {
      shared_rom_ = std::make_shared<Rom>();
    }
    return shared_rom_;
  }

  auto rom() {
    if (!shared_rom_) {
      shared_rom_ = std::make_shared<Rom>();
    }
    Rom* rom = shared_rom_.get();
    return rom;
  }

  // private:
  static std::shared_ptr<Rom> shared_rom_;
};

}  // namespace yaze

#endif
