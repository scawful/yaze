#include "zelda3/formats/offsets.h"

#include <algorithm>

namespace yaze::zelda3::formats {

namespace {

constexpr RegionOffsets kUsOffsets = {
    // Dungeon
    DungeonOffsets{/*torches=*/0x2736A, /*torch_count=*/0x88C1},
    // Overworld (unused placeholder)
    OverworldOffsets{/*dummy=*/0x33333},
    // Text
    TextOffsets{
        /*bank=*/0x0E,
        /*dictionary=*/0x74703,
        /*dictionary_bound=*/0x748D9,
        /*param_counts=*/0x7536B,
        /*region1=*/0xE0000,
        /*region1_bound=*/0xE8000,
        /*region2=*/0x75F40,
        /*region2_bound=*/0x77400,
        /*max_message_length=*/0xE00,
        /*codes=*/
        TextCodes{
            /*zchar_base=*/0x00,
            /*zchar_bound=*/0x67,
            /*command_base=*/0x67,
            /*command_bound=*/0x80,
            /*msg_terminator=*/0x7F,
            /*region_switch=*/0x80,
            /*dict_base=*/0x88,
            /*dict_bound=*/0xEA,
            /*abs_terminator=*/0xFF,
        }},
};

}  // namespace

std::optional<RegionOffsets> GetRegionOffsets(Region region) {
  switch (region) {
    case Region::kUs:
      return kUsOffsets;
    default:
      return std::nullopt;
  }
}

bool ValidateOffsets(const RegionOffsets& offsets, size_t rom_size_bytes) {
  const auto within = [rom_size_bytes](uint32_t addr) {
    return addr < rom_size_bytes;
  };

  // Text bounds
  const auto& t = offsets.text;
  if (!(within(t.dictionary) && within(t.dictionary_bound) &&
        within(t.param_counts) && within(t.region1) &&
        within(t.region1_bound) && within(t.region2) &&
        within(t.region2_bound))) {
    return false;
  }
  if (!(t.dictionary < t.dictionary_bound &&
        t.region1 < t.region1_bound && t.region2 < t.region2_bound)) {
    return false;
  }

  // Codes sanity: base < bound
  const auto& c = t.codes;
  if (!(c.zchar_base <= c.zchar_bound && c.command_base <= c.command_bound &&
        c.dict_base <= c.dict_bound)) {
    return false;
  }

  // Dungeon torch offsets should also be in range.
  const auto& d = offsets.dungeon;
  if (!(within(d.torches) && within(d.torch_count))) {
    return false;
  }

  return true;
}

}  // namespace yaze::zelda3::formats
