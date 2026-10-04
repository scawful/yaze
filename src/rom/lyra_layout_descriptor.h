#ifndef YAZE_ROM_LYRA_LAYOUT_DESCRIPTOR_H_
#define YAZE_ROM_LYRA_LAYOUT_DESCRIPTOR_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace yaze::rom {

// This is the proposed overworld ROM format, not the separate SRAM event format.
// A valid revision-1 image is readable for inspection but has no Yaze writer.
enum class LyraLayoutPresence { kAbsent, kValid, kInvalid };

struct LyraLayoutEntry {
  uint16_t id = 0;
  uint16_t kind = 0;
  uint32_t snes_address = 0;
  uint32_t pc_address = 0;
  uint16_t count = 0;
  uint16_t stride = 0;
  uint32_t crc32 = 0;
};

struct LyraLayoutInspection {
  LyraLayoutPresence presence = LyraLayoutPresence::kAbsent;
  uint16_t revision = 0;
  uint16_t capacity = 0;
  uint16_t active_count = 0;
  bool write_supported = true;
  std::string error;
  std::vector<LyraLayoutEntry> entries;
};

// The input is a headerless ROM image. The caller owns session attribution:
// an attributed image remains protected even if a later in-memory edit removes
// its magic bytes. Unrelated ROM data at the locator is not attribution.
LyraLayoutInspection InspectLyraLayout(const uint8_t* image, size_t size);

}  // namespace yaze::rom

#endif  // YAZE_ROM_LYRA_LAYOUT_DESCRIPTOR_H_
