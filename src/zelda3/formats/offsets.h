#ifndef YAZE_APP_ZELDA3_FORMATS_OFFSETS_H
#define YAZE_APP_ZELDA3_FORMATS_OFFSETS_H

#include <cstddef>
#include <cstdint>
#include <optional>

// Zelda 3 offsets extracted from hmagic (US ROM) with added safety checks.
// Intended for parsing text/dungeon structures in yaze without pulling in
// hmagic's unsafe global state.
namespace yaze::zelda3::formats {

enum class Region {
  kUs = 0,
  kEu,
  kJp,
};

struct DungeonOffsets {
  uint32_t torches = 0;
  uint32_t torch_count = 0;
};

struct TextCodes {
  uint8_t zchar_base = 0;
  uint8_t zchar_bound = 0;
  uint8_t command_base = 0;
  uint8_t command_bound = 0;
  uint8_t msg_terminator = 0;
  uint8_t region_switch = 0;
  uint8_t dict_base = 0;
  uint8_t dict_bound = 0;
  uint8_t abs_terminator = 0;
};

struct TextOffsets {
  uint8_t bank = 0;
  uint32_t dictionary = 0;
  uint32_t dictionary_bound = 0;
  uint32_t param_counts = 0;
  uint32_t region1 = 0;
  uint32_t region1_bound = 0;
  uint32_t region2 = 0;
  uint32_t region2_bound = 0;
  uint32_t max_message_length = 0;
  TextCodes codes{};
};

struct OverworldOffsets {
  // Placeholder for future additions; kept for parity with hmagic structs.
  uint32_t dummy = 0;
};

struct RegionOffsets {
  DungeonOffsets dungeon{};
  OverworldOffsets overworld{};
  TextOffsets text{};
};

// Returns offsets for a region if known; currently populated for US only.
std::optional<RegionOffsets> GetRegionOffsets(Region region);

// Simple bounds validation to avoid reading outside the ROM buffer.
bool ValidateOffsets(const RegionOffsets& offsets, size_t rom_size_bytes);

}  // namespace yaze::zelda3::formats

#endif  // YAZE_APP_ZELDA3_FORMATS_OFFSETS_H
