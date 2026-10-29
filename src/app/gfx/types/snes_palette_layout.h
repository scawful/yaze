#ifndef YAZE_APP_GFX_TYPES_SNES_PALETTE_LAYOUT_H_
#define YAZE_APP_GFX_TYPES_SNES_PALETTE_LAYOUT_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace yaze::gfx {

struct PaletteGroup;

namespace palette_group_internal {

enum class OverworldPaletteKind { kMain, kAuxiliary };

// A validated native span in normalized (copier-header-free) ROM coordinates.
// Span/shape validation does not grant storage ownership.
class ResolvedPaletteRecord {
 public:
  uint32_t pc_address() const { return pc_address_; }
  uint32_t snes_address() const { return snes_address_; }
  int color_count() const { return color_count_; }
  size_t byte_size() const { return static_cast<size_t>(color_count_) * 2; }
  absl::StatusOr<uint32_t> ColorAddress(int color_index) const;
  absl::StatusOr<std::pair<uint32_t, uint32_t>> ColorWriteRange(
      int color_index) const;

 private:
  friend absl::StatusOr<ResolvedPaletteRecord> ResolvePaletteRecord(
      std::optional<uint32_t>, uint64_t, size_t);
  ResolvedPaletteRecord(uint32_t pc, uint32_t snes, int colors)
      : pc_address_(pc), snes_address_(snes), color_count_(colors) {}
  uint32_t pc_address_;
  uint32_t snes_address_;
  int color_count_;
};

absl::StatusOr<ResolvedPaletteRecord> ResolvePaletteRecord(
    std::optional<uint32_t> snes_address, uint64_t color_count,
    size_t normalized_rom_size);

// Proposed capacity describes unbound slots, never an active native directory.
class OverworldPaletteLayout {
 public:
  static OverworldPaletteLayout Legacy();
  static OverworldPaletteLayout ProposedUnbound();
  // Includes unbound declared slots; Resolve is required before any use.
  int RecordCount(OverworldPaletteKind kind) const;
  absl::StatusOr<ResolvedPaletteRecord> Resolve(
      OverworldPaletteKind kind, int selector,
      size_t normalized_rom_size) const;

 private:
  explicit OverworldPaletteLayout(bool proposed) : proposed_(proposed) {}
  bool proposed_;
};

absl::Status LoadOverworldPaletteRecords(
    OverworldPaletteKind kind, const OverworldPaletteLayout& layout,
    const std::vector<uint8_t>& normalized_rom_data, PaletteGroup& group);

}  // namespace palette_group_internal

}  // namespace yaze::gfx

#endif  // YAZE_APP_GFX_TYPES_SNES_PALETTE_LAYOUT_H_
