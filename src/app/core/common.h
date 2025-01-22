#ifndef YAZE_CORE_COMMON_H
#define YAZE_CORE_COMMON_H

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"

namespace yaze {

/**
 * @namespace yaze::core
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
    // Log instructions to the GUI debugger.
    bool kLogInstructions = true;

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

  static Flags &get() {
    static Flags instance;
    return instance;
  }

  std::string Serialize() const {
    std::string result;
    result +=
        "kLogInstructions: " + std::to_string(get().kLogInstructions) + "\n";
    result +=
        "kSaveAllPalettes: " + std::to_string(get().kSaveAllPalettes) + "\n";
    result += "kSaveGfxGroups: " + std::to_string(get().kSaveGfxGroups) + "\n";
    result +=
        "kSaveWithChangeQueue: " + std::to_string(get().kSaveWithChangeQueue) +
        "\n";
    result += "kDrawDungeonRoomGraphics: " +
              std::to_string(get().kDrawDungeonRoomGraphics) + "\n";
    result +=
        "kSaveDungeonMaps: " + std::to_string(get().kSaveDungeonMaps) + "\n";
    result += "kLogToConsole: " + std::to_string(get().kLogToConsole) + "\n";
    result += "kDrawOverworldSprites: " +
              std::to_string(get().overworld.kDrawOverworldSprites) + "\n";
    result += "kSaveOverworldMaps: " +
              std::to_string(get().overworld.kSaveOverworldMaps) + "\n";
    result += "kSaveOverworldEntrances: " +
              std::to_string(get().overworld.kSaveOverworldEntrances) + "\n";
    result += "kSaveOverworldExits: " +
              std::to_string(get().overworld.kSaveOverworldExits) + "\n";
    result += "kSaveOverworldItems: " +
              std::to_string(get().overworld.kSaveOverworldItems) + "\n";
    result += "kSaveOverworldProperties: " +
              std::to_string(get().overworld.kSaveOverworldProperties) + "\n";
    return result;
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

}  // namespace core
}  // namespace yaze

#endif
