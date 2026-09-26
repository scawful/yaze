// C API version helpers declared in inc/zelda.h. They live in yaze_zelda3
// rather than src/yaze.cc (which no CMake target builds) so the app, the CLI
// and the tests can link them.

#include <string>

#include "absl/strings/match.h"
#include "zelda.h"

zelda3_version zelda3_detect_version(const uint8_t* rom_data, size_t size) {
  if (rom_data == nullptr || size < 0x8000) {
    return ZELDA3_VERSION_UNKNOWN;
  }

  // SNES LoROM header titles at 0x7FC0 (PC offset)
  // Titles are 21 bytes long
  std::string title;
  for (int i = 0; i < 21; ++i) {
    char c = static_cast<char>(rom_data[0x7FC0 + i]);
    if (c >= 0x20 && c < 0x7F) {
      title += c;
    }
  }

  if (absl::StrContains(title, "THE LEGEND OF ZELDA")) {
    // US or EU version often share this title
    // Check destination code at 0x7FD9: 0x01 = USA, 0x02+ = PAL regions
    uint8_t region = rom_data[0x7FD9];
    if (region == 0x00)
      return ZELDA3_VERSION_JP;
    if (region == 0x01)
      return ZELDA3_VERSION_US;
    return ZELDA3_VERSION_EU;
  }

  if (absl::StrContains(title, "ZELDA NO DENSETSU")) {
    return ZELDA3_VERSION_JP;
  }

  // Fallback: Check for common randomizer indicators
  if (absl::StrContains(title, "VT RANDO") ||
      absl::StrContains(title, "Z3 RANDOMIZER")) {
    return ZELDA3_VERSION_RANDOMIZER;
  }

  return ZELDA3_VERSION_US;  // Default assumption for ALttP clones/hacks
}

const char* zelda3_version_to_string(zelda3_version version) {
  switch (version) {
    case ZELDA3_VERSION_US:
      return "US/North American";
    case ZELDA3_VERSION_JP:
      return "Japanese";
    case ZELDA3_VERSION_EU:
      return "European";
    case ZELDA3_VERSION_PROTO:
      return "Prototype";
    case ZELDA3_VERSION_RANDOMIZER:
      return "Randomizer";
    default:
      return "Unknown";
  }
}
