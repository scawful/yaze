#ifndef YAZE_ROM_LYRA_LAYOUT_DESCRIPTOR_H_
#define YAZE_ROM_LYRA_LAYOUT_DESCRIPTOR_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "absl/status/statusor.h"

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
  // A revision-2 capability permits only the dedicated A0 terrain Save As.
  // Generic ROM writes remain refused for every Lyra revision.
  bool a0_terrain_write_supported = false;
  bool offline_only = true;
  std::string error;
  std::vector<LyraLayoutEntry> entries;
};

// The input is a headerless ROM image. The caller owns session attribution:
// an attributed image remains protected even if a later in-memory edit removes
// its magic bytes. Unrelated ROM data at the locator is not attribution.
LyraLayoutInspection InspectLyraLayout(const uint8_t* image, size_t size);

// Checked, session-local reads from an already validated descriptor. The
// descriptor owns table addresses; ordinary ROM settings are not consulted.
std::optional<std::span<const uint8_t>> LyraLayoutRecord(
    const LyraLayoutInspection& layout, const uint8_t* image, size_t size,
    uint16_t table_id, size_t index);

// Reuses the descriptor validator's stream decoder and allocation fences.
std::optional<std::array<uint16_t, 256>> LyraMapTile32(
    const LyraLayoutInspection& layout, const uint8_t* image, size_t size,
    size_t map_index);

inline constexpr uint32_t kLyraA0HighStreamPc = 0x132000;
inline constexpr uint32_t kLyraA0LowStreamPc = 0x132200;
inline constexpr size_t kLyraA0StreamSlotSize = 512;

// Stages a complete image without mutating the caller. Revision 1, malformed
// descriptors and every unowned byte remain protected. No checksum is changed.
absl::StatusOr<std::vector<uint8_t>> SerializeLyraA0Terrain(
    const std::vector<uint8_t>& baseline,
    const std::array<uint16_t, 256>& tiles);

}  // namespace yaze::rom

#endif  // YAZE_ROM_LYRA_LAYOUT_DESCRIPTOR_H_
