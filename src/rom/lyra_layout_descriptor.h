#ifndef YAZE_ROM_LYRA_LAYOUT_DESCRIPTOR_H_
#define YAZE_ROM_LYRA_LAYOUT_DESCRIPTOR_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "app/gfx/types/snes_palette_layout.h"

#include "rom/native_overworld_authoring_metadata.h"

namespace yaze::rom {

struct NativeOverworldEndpointEdits;

// Source-only prototype. No runtime producer/reader is admitted by revision 5.
// Entry 413 holds sorted (page, local Tile16 ID, TL/TR/BL/BR words) rows;
// unused rows are twelve FF bytes. Entry 400 remains the global fallback.
inline constexpr uint16_t kNativePageTile16TableId = 413;
inline constexpr size_t kNativePageTile16Stride = 12;
struct NativePageTile16Definition {
  uint16_t map_id = 0;
  uint16_t tile_id = 0;
  std::array<uint16_t, 4> words{};
  bool operator==(const NativePageTile16Definition&) const = default;
};
struct NativePageTile16Edits {
  std::vector<NativePageTile16Definition> expected;
  std::vector<NativePageTile16Definition> value;
};
absl::StatusOr<std::vector<NativePageTile16Definition>> DecodeNativePageTile16(
    std::span<const uint8_t> bytes, uint16_t map_count);
absl::StatusOr<std::vector<uint8_t>> EncodeNativePageTile16(
    std::span<const NativePageTile16Definition> definitions, uint16_t map_count,
    size_t row_capacity);

// This is the proposed overworld ROM format, not the separate SRAM event format.
// Validation describes the stored layout; runtime and writer admission are
// separate. Revisions 1/2 retain their original inspection/pilot contracts.
// Revisions 3 and 4 share the native special-overworld descriptor at 0x147000;
// revision 4 adds appearance records 410/411 (producer-defined addresses),
// optionally record 412 with the explicit authoring-metadata flag 0x20.
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
  uint8_t tail_actor_bank = 0;
  bool write_supported = true;
  // A revision-2 capability permits only the dedicated A0 terrain Save As.
  // Generic ROM writes remain refused until the corresponding normal writer
  // is integrated, including for a structurally valid revision-3 directory.
  bool a0_terrain_write_supported = false;
  bool offline_only = true;
  std::string error;
  std::vector<LyraLayoutEntry> entries;
  // Structural codec validation only; actual sprite-list binding and metadata
  // mutation are separate capabilities, not admitted by this inspection.
  bool has_authoring_metadata = false;
};

inline bool IsNativeSpecialOverworldLayout(const LyraLayoutInspection& layout) {
  return layout.presence == LyraLayoutPresence::kValid &&
         (layout.revision == 3 || layout.revision == 4 || layout.revision == 5);
}

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

// Seal a terrain-only candidate. Its directory identity, code, source pools
// and every unrelated byte must still match baseline. Pointer mirrors and
// decoded pages are validated after updating the affected CRC fields.
// Geometry uses complete batches of tables 7..15 and 30 for each edited map.
// Partial parent/size/navigation rows are refused by the native sealer.
struct NativeOverworldPropertyEdit {
  uint16_t table_id = 0;
  uint16_t map_id = 0;
  std::vector<uint8_t> value;
};

// Background palette edits are resolved by the same captured session layout
// as GameData. No caller-supplied byte address grants write authority.
enum class NativeOverworldPaletteKind { kMain, kAuxiliary, kAnimated };
struct NativeOverworldPaletteColorEdit {
  NativeOverworldPaletteKind kind;
  uint16_t palette_index;
  uint16_t color_index;
  uint16_t expected;
  uint16_t value;
};
struct NativeOverworldPaletteSelectorEdit {
  uint16_t group_index;
  std::array<uint8_t, 4> expected;
  std::array<uint8_t, 4> value;
};
struct NativeOverworldPaletteEdits {
  gfx::palette_group_internal::OverworldPaletteLayout layout =
      gfx::palette_group_internal::OverworldPaletteLayout::Legacy();
  std::vector<NativeOverworldPaletteSelectorEdit> selectors;
  std::vector<NativeOverworldPaletteColorEdit> colors;
};

// Ordered packed sprite stream: y, x, sprite code, with a final FF. Placement
// IDs, when present in optional record 412, are not runtime sprite codes.
struct NativeOverworldSpriteEdit {
  uint16_t phase = 0;
  uint16_t map_id = 0;
  std::vector<uint8_t> value;
};
struct NativeOverworldSpriteWrite {
  int address = 0;
  std::vector<uint8_t> bytes;
};
// Shared by normal native Save and the existing overworld sprite planner.
// Inventory both banks and preserve unloaded lists; no ROM mutation occurs.
absl::StatusOr<std::vector<NativeOverworldSpriteWrite>>
PlanNativeOverworldSpriteSave(
    const std::vector<uint8_t>& baseline,
    std::span<const NativeOverworldSpriteEdit> edits,
    const NativeOverworldAuthoringMetadata* placement_witness = nullptr);

// Explicit property edits are applied only after the terrain candidate passes
// its ownership check. Existing native map slots are mirrored atomically;
// Geometry requires a complete parent/size/navigation row for every changed
// map; this low-level ownership check does not derive navigation values.
// Model editors supply the shared geometry planner's output. Roles and entity
// pointers are not property-write capabilities.
// Optional endpoint edits use selected runtime rows with full preimages and
// final-geometry validation; no raw endpoint bytes in candidate are admitted.
absl::StatusOr<std::vector<uint8_t>> SealNativeOverworldTerrain(
    const std::vector<uint8_t>& baseline, std::vector<uint8_t> candidate,
    std::span<const NativeOverworldPropertyEdit> properties = {},
    std::span<const NativeOverworldSpriteEdit> sprites = {},
    const NativeOverworldAuthoringMetadata* baseline_metadata = nullptr,
    const NativeOverworldAuthoringMetadata* publish_metadata = nullptr,
    const NativeOverworldPaletteEdits* palettes = nullptr,
    const NativeOverworldEndpointEdits* endpoints = nullptr,
    const NativePageTile16Edits* page_tile16 = nullptr,
    bool allow_experimental_page_tile16 = false);

// Byte ranges [begin, end) that native special-overworld Save owns: the
// directory, every directory table, the original-engine prefix copies kept in
// step with them, and original sprite storage up to the room sprite table.
// Writers outside the overworld must leave all of them unchanged.
absl::StatusOr<std::vector<std::pair<uint32_t, uint32_t>>>
NativeOverworldOwnedRanges(const LyraLayoutInspection& layout,
                           const uint8_t* image, size_t size);

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
