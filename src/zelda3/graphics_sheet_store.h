#ifndef YAZE_ZELDA3_GRAPHICS_SHEET_STORE_H
#define YAZE_ZELDA3_GRAPHICS_SHEET_STORE_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/types/span.h"

namespace yaze {
namespace zelda3 {

/**
 * @file graphics_sheet_store.h
 * @brief One ROM session's graphics sheet pixels, with a revision per sheet.
 *
 * The pixels use the legacy `GameData::graphics_buffer` layout: 223 sheets of
 * 128x32 8bpp indices, sheet `i` at byte `i * 4096`. `graphics_buffer` is an
 * alias of this buffer, so Room, OverworldMap and the title screen read it
 * unchanged.
 *
 * A sheet's revision changes whenever the store replaces its pixels, so a
 * consumer that caches pixels can tell when to rebuild. Revisions come from
 * one process-wide counter, so two stores never hand out the same number
 * for different pixels (a copied store keeps the numbers it copied). Writes through the
 * `graphics_buffer` alias bypass the revisions; see
 * docs/internal/agents/graphics-sheet-store-proposal-2026-09-26.md for the
 * migration that moves writers onto the store.
 */
class GraphicsSheetStore {
 public:
  static constexpr uint16_t kSheetCount = 223;
  static constexpr size_t kSheetWidth = 128;
  static constexpr size_t kSheetHeight = 32;
  static constexpr size_t kSheetBytes = kSheetWidth * kSheetHeight;

  /// The whole buffer, `graphics_buffer` layout. May hold fewer than 223
  /// sheets while a load is in progress or after Clear().
  const std::vector<uint8_t>& pixels() const { return pixels_; }
  /// For the `graphics_buffer` alias and the loader only.
  std::vector<uint8_t>& mutable_pixels() { return pixels_; }

  /// True when the buffer holds all 4096 bytes of `sheet`.
  bool HasSheet(uint16_t sheet) const;
  /// The sheet's 4096 pixels, or an empty span when it is not held.
  absl::Span<const uint8_t> Sheet(uint16_t sheet) const;
  /// Changes each time the store replaces this sheet's pixels; 0 for a sheet
  /// out of range.
  uint64_t Revision(uint16_t sheet) const;

  /// Replaces one sheet's pixels and bumps its revision. Refuses a sheet
  /// outside the buffer, the wrong size, or indices above `max_index`.
  /// Writing identical pixels changes nothing, including the revision.
  absl::Status WriteSheet(uint16_t sheet, absl::Span<const uint8_t> pixels,
                          uint8_t max_index = 0x0F);

  /// Call after the loader refills the buffer: bumps every revision.
  void MarkAllSheetsChanged();
  /// Empties the buffer and bumps every revision.
  void Clear();

 private:
  std::vector<uint8_t> pixels_;
  std::array<uint64_t, kSheetCount> revisions_{};
};

/// Base of GameData that owns the store and the legacy `graphics_buffer`
/// alias. Copying or moving rebinds the alias to the new object's own store;
/// the default copy would leave it pointing at the source.
struct GraphicsSheetStoreHolder {
  GraphicsSheetStoreHolder() = default;
  GraphicsSheetStoreHolder(const GraphicsSheetStoreHolder& other)
      : sheet_store(other.sheet_store) {}
  GraphicsSheetStoreHolder(GraphicsSheetStoreHolder&& other) noexcept
      : sheet_store(std::move(other.sheet_store)) {}
  GraphicsSheetStoreHolder& operator=(const GraphicsSheetStoreHolder& other) {
    sheet_store = other.sheet_store;
    return *this;
  }
  GraphicsSheetStoreHolder& operator=(
      GraphicsSheetStoreHolder&& other) noexcept {
    sheet_store = std::move(other.sheet_store);
    return *this;
  }

  GraphicsSheetStore sheet_store;
  /// Legacy name for `sheet_store.mutable_pixels()`.
  std::vector<uint8_t>& graphics_buffer = sheet_store.mutable_pixels();
};

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_GRAPHICS_SHEET_STORE_H
