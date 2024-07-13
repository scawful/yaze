#ifndef YAZE_CORE_COMMON_H
#define YAZE_CORE_COMMON_H

#include <imgui/imgui.h>

#include <chrono>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <stack>
#include <string>

namespace yaze {
namespace app {

/**
 * @namespace yaze::app::core
 * @brief Core application logic and utilities.
 */
namespace core {

/**
 * @class ExperimentFlags
 * @brief A class to manage experimental feature flags.
 */
class ExperimentFlags {
 public:
  struct Flags {
    // Bitmap manager abstraction to manage graphics bin of Rom.
    bool kUseBitmapManager = true;

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

    // Platform specific loading of fonts from the system. Currently
    // only supports macOS.
    bool kLoadSystemFonts = true;

    // Uses texture streaming from SDL for my dynamic updates.
    bool kLoadTexturesAsStreaming = true;

    // Save dungeon map edits to the Rom.
    bool kSaveDungeonMaps = false;

    // Log to the console.
    bool kLogToConsole = false;

    // Load audio device for emulator
    bool kLoadAudioDevice = false;

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

 private:
  static std::shared_ptr<Flags> flags_;
};

/**
 * @class NotifyValue
 * @brief A class to manage a value that can be modified and notify when it
 * changes.
 */
template <typename T>
class NotifyValue {
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

class ImGuiIdIssuer {
 private:
  static std::stack<ImGuiID> idStack;

 public:
  // Generate and push a new ID onto the stack
  static ImGuiID GetNewID() {
    static int counter = 1;  // Start from 1 to ensure uniqueness
    ImGuiID child_id = ImGui::GetID((void *)(intptr_t)counter++);
    idStack.push(child_id);
    return child_id;
  }

  // Pop all IDs from the stack (can be called explicitly or upon program exit)
  static void Cleanup() {
    while (!idStack.empty()) {
      idStack.pop();
    }
  }
};

class Logger {
 public:
  static void log(std::string message) {
    static std::ofstream fout("log.txt", std::ios::out | std::ios::app);
    fout << message << std::endl;
  }
};

std::string UppercaseHexByte(uint8_t byte, bool leading = false);
std::string UppercaseHexWord(uint16_t word);
std::string UppercaseHexLong(uint32_t dword);

uint32_t SnesToPc(uint32_t addr);
uint32_t PcToSnes(uint32_t addr);

uint32_t MapBankToWordAddress(uint8_t bank, uint16_t addr);

int AddressFromBytes(uint8_t addr1, uint8_t addr2, uint8_t addr3);
int HexToDec(char *input, int length);

bool StringReplace(std::string &str, const std::string &from,
                   const std::string &to);

void stle16b_i(uint8_t *const p_arr, size_t const p_index,
               uint16_t const p_val);
uint16_t ldle16b_i(uint8_t const *const p_arr, size_t const p_index);

uint16_t ldle16b(uint8_t const *const p_arr);

void stle16b(uint8_t *const p_arr, uint16_t const p_val);

struct FolderItem {
  std::string name;
  std::vector<FolderItem> subfolders;
  std::vector<std::string> files;
};

typedef struct FolderItem FolderItem;

}  // namespace core
}  // namespace app
}  // namespace yaze

#endif