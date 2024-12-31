#ifndef YAZE_CORE_COMMON_H
#define YAZE_CORE_COMMON_H

#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/container/flat_hash_map.h"

namespace yaze {

/**
 * @namespace yaze::core
 * @brief Core application logic and utilities.
 */
namespace core {


struct HexStringParams {
  enum class Prefix { kNone, kDollar, kHash, k0x } prefix = Prefix::kDollar;
  bool uppercase = true;
};

std::string HexByte(uint8_t byte, HexStringParams params = {});
std::string HexWord(uint16_t word, HexStringParams params = {});
std::string HexLong(uint32_t dword, HexStringParams params = {});
std::string HexLongLong(uint64_t qword, HexStringParams params = {});

bool StringReplace(std::string &str, const std::string &from,
                   const std::string &to);

/**
 * @class ExperimentFlags
 * @brief A class to manage experimental feature flags.
 */
class ExperimentFlags {
public:
  struct Flags {
    // Log instructions to the GUI debugger.
    bool kLogInstructions = true;

    // Flag to enable ImGui input config flags. Currently is
    // handled manually by controller class but should be
    // ported away from that eventually.
    bool kUseNewImGuiInput = false;

    // Flag to enable the saving of all palettes to the Rom.
    bool kSaveAllPalettes = false;

    // Flag to enable the saving of gfx groups to the rom.
    bool kSaveGfxGroups = false;

    // Flag to enable the change queue, which could have any anonymous
    // save routine for the Rom. In practice, just the overworld tilemap
    // and tile32 save.
    bool kSaveWithChangeQueue = false;

    // Attempt to run the dungeon room draw routine when opening a room.
    bool kDrawDungeonRoomGraphics = true;

    // Use the new platform specific file dialog wrappers.
    bool kNewFileDialogWrapper = true;

    // Uses texture streaming from SDL for my dynamic updates.
    bool kLoadTexturesAsStreaming = true;

    // Save dungeon map edits to the Rom.
    bool kSaveDungeonMaps = false;

    // Save graphics sheet to the Rom.
    bool kSaveGraphicsSheet = false;

    // Log to the console.
    bool kLogToConsole = false;

    // Overworld flags
    struct Overworld {
      // Load and render overworld sprites to the screen. Unstable.
      bool kDrawOverworldSprites = false;

      // Save overworld map edits to the Rom.
      bool kSaveOverworldMaps = true;

      // Save overworld entrances to the Rom.
      bool kSaveOverworldEntrances = true;

      // Save overworld exits to the Rom.
      bool kSaveOverworldExits = true;

      // Save overworld items to the Rom.
      bool kSaveOverworldItems = true;

      // Save overworld properties to the Rom.
      bool kSaveOverworldProperties = true;

      // Load custom overworld data from the ROM and enable UI.
      bool kLoadCustomOverworld = false;
    } overworld;
  };

  ExperimentFlags() = default;
  virtual ~ExperimentFlags() = default;
  auto flags() const {
    if (!flags_) {
      flags_ = std::make_shared<Flags>();
    }
    Flags *flags = flags_.get();
    return flags;
  }
  Flags *mutable_flags() {
    if (!flags_) {
      flags_ = std::make_shared<Flags>();
    }
    return flags_.get();
  }
  std::string Serialize() const {
    std::string result;
    result +=
        "kLogInstructions: " + std::to_string(flags_->kLogInstructions) + "\n";
    result +=
        "kUseNewImGuiInput: " + std::to_string(flags_->kUseNewImGuiInput) +
        "\n";
    result +=
        "kSaveAllPalettes: " + std::to_string(flags_->kSaveAllPalettes) + "\n";
    result +=
        "kSaveGfxGroups: " + std::to_string(flags_->kSaveGfxGroups) + "\n";
    result += "kSaveWithChangeQueue: " +
              std::to_string(flags_->kSaveWithChangeQueue) + "\n";
    result += "kDrawDungeonRoomGraphics: " +
              std::to_string(flags_->kDrawDungeonRoomGraphics) + "\n";
    result += "kNewFileDialogWrapper: " +
              std::to_string(flags_->kNewFileDialogWrapper) + "\n";
    result += "kLoadTexturesAsStreaming: " +
              std::to_string(flags_->kLoadTexturesAsStreaming) + "\n";
    result +=
        "kSaveDungeonMaps: " + std::to_string(flags_->kSaveDungeonMaps) + "\n";
    result += "kLogToConsole: " + std::to_string(flags_->kLogToConsole) + "\n";
    result += "kDrawOverworldSprites: " +
              std::to_string(flags_->overworld.kDrawOverworldSprites) + "\n";
    result += "kSaveOverworldMaps: " +
              std::to_string(flags_->overworld.kSaveOverworldMaps) + "\n";
    result += "kSaveOverworldEntrances: " +
              std::to_string(flags_->overworld.kSaveOverworldEntrances) + "\n";
    result += "kSaveOverworldExits: " +
              std::to_string(flags_->overworld.kSaveOverworldExits) + "\n";
    result += "kSaveOverworldItems: " +
              std::to_string(flags_->overworld.kSaveOverworldItems) + "\n";
    result += "kSaveOverworldProperties: " +
              std::to_string(flags_->overworld.kSaveOverworldProperties) + "\n";
    return result;
  }

private:
  static std::shared_ptr<Flags> flags_;
};

/**
 * @class NotifyValue
 * @brief A class to manage a value that can be modified and notify when it
 * changes.
 */
template <typename T> class NotifyValue {
public:
  NotifyValue() : value_(), modified_(false), temp_value_() {}
  NotifyValue(const T &value)
      : value_(value), modified_(false), temp_value_() {}

  void set(const T &value) {
    value_ = value;
    modified_ = true;
  }

  const T &get() {
    modified_ = false;
    return value_;
  }

  T &mutable_get() {
    modified_ = false;
    temp_value_ = value_;
    return temp_value_;
  }

  void apply_changes() {
    if (temp_value_ != value_) {
      value_ = temp_value_;
      modified_ = true;
    }
  }

  void operator=(const T &value) { set(value); }
  operator T() { return get(); }

  bool modified() const { return modified_; }

private:
  T value_;
  bool modified_;
  T temp_value_;
};

static bool log_to_console = false;
static std::string log_file_out = "log.txt";

template <typename... Args>
static void logf(const absl::FormatSpec<Args...> &format, const Args &...args) {
  std::string message = absl::StrFormat(format, args...);
  if (log_to_console) {
    std::cout << message << std::endl;
  }
  static std::ofstream fout(log_file_out, std::ios::out | std::ios::app);
  fout << message << std::endl;
}

struct StructuredLog {
  std::string raw_message;
  std::string category;
};

static absl::flat_hash_map<std::string, std::vector<std::string>> log_categories;

template <typename... Args>
static void logm(const std::string &category,
                 const absl::FormatSpec<Args...> &format, const Args &...args) {
  std::string message = absl::StrFormat(format, args...);
  std::cout << category << ": " << message << std::endl;
  if (log_categories.contains(category)) {
    log_categories[category].push_back(message);
  } else {
    log_categories[category] = {message};
  }
}

class Logger {
public:
  static void log(std::string message) {
    static std::ofstream fout(log_file_out, std::ios::out | std::ios::app);
    fout << message << std::endl;
  }
};

constexpr uint32_t kFastRomRegion = 0x808000;

inline uint32_t SnesToPc(uint32_t addr) noexcept {
  if (addr >= kFastRomRegion) {
    addr -= kFastRomRegion;
  }
  uint32_t temp = (addr & 0x7FFF) + ((addr / 2) & 0xFF8000);
  return (temp + 0x0);
}

inline uint32_t PcToSnes(uint32_t addr) {
  uint8_t *b = reinterpret_cast<uint8_t *>(&addr);
  b[2] = static_cast<uint8_t>(b[2] * 2);

  if (b[1] >= 0x80) {
    b[2] += 1;
  } else {
    b[1] += 0x80;
  }

  return addr;
}

inline int AddressFromBytes(uint8_t bank, uint8_t high, uint8_t low) noexcept {
  return (bank << 16) | (high << 8) | low;
}

inline uint32_t MapBankToWordAddress(uint8_t bank, uint16_t addr) noexcept {
  uint32_t result = 0;
  result = (bank << 16) | addr;
  return result;
}

uint32_t Get24LocalFromPC(uint8_t *data, int addr, bool pc = true);

/**
 * @brief Store little endian 16-bit value using a byte pointer, offset by an
 * index before dereferencing
 */
void stle16b_i(uint8_t *const p_arr, size_t const p_index,
               uint16_t const p_val);

void stle16b(uint8_t *const p_arr, uint16_t const p_val);

/**
 * @brief Load little endian halfword (16-bit) dereferenced from an arrays of
 * bytes. This version provides an index that will be multiplied by 2 and added
 * to the base address.
 */
uint16_t ldle16b_i(uint8_t const *const p_arr, size_t const p_index);

// Load little endian halfword (16-bit) dereferenced from
uint16_t ldle16b(uint8_t const *const p_arr);

struct FolderItem {
  std::string name;
  std::vector<FolderItem> subfolders;
  std::vector<std::string> files;
};

typedef struct FolderItem FolderItem;

void CreateBpsPatch(const std::vector<uint8_t> &source,
                    const std::vector<uint8_t> &target,
                    std::vector<uint8_t> &patch);

void ApplyBpsPatch(const std::vector<uint8_t> &source,
                   const std::vector<uint8_t> &patch,
                   std::vector<uint8_t> &target);

}  // namespace core
}  // namespace yaze

#endif
