#include "zelda3/graphics_sheet_store.h"

#include <algorithm>
#include <atomic>

#include "absl/strings/str_format.h"

namespace yaze {
namespace zelda3 {

namespace {

uint64_t NextRevision() {
  static std::atomic<uint64_t> counter{0};
  return ++counter;
}

}  // namespace

bool GraphicsSheetStore::HasSheet(uint16_t sheet) const {
  return sheet < kSheetCount &&
         (static_cast<size_t>(sheet) + 1) * kSheetBytes <= pixels_.size();
}

absl::Span<const uint8_t> GraphicsSheetStore::Sheet(uint16_t sheet) const {
  if (!HasSheet(sheet)) {
    return {};
  }
  return absl::MakeConstSpan(pixels_.data() + sheet * kSheetBytes, kSheetBytes);
}

uint64_t GraphicsSheetStore::Revision(uint16_t sheet) const {
  return sheet < kSheetCount ? revisions_[sheet] : 0;
}

absl::Status GraphicsSheetStore::WriteSheet(uint16_t sheet,
                                            absl::Span<const uint8_t> pixels,
                                            uint8_t max_index) {
  if (!HasSheet(sheet)) {
    return absl::OutOfRangeError(absl::StrFormat(
        "Sheet 0x%02X is not in the graphics store (%zu bytes held)", sheet,
        pixels_.size()));
  }
  if (pixels.size() != kSheetBytes) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Sheet 0x%02X needs %zu pixels, got %zu", sheet,
                        kSheetBytes, pixels.size()));
  }
  const auto high =
      std::find_if(pixels.begin(), pixels.end(),
                   [max_index](uint8_t v) { return v > max_index; });
  if (high != pixels.end()) {
    const size_t at = static_cast<size_t>(high - pixels.begin());
    return absl::InvalidArgumentError(absl::StrFormat(
        "Sheet 0x%02X pixel (%zu, %zu) uses color %d; the limit is %d", sheet,
        at % kSheetWidth, at / kSheetWidth, *high, max_index));
  }
  auto first = pixels_.begin() + sheet * kSheetBytes;
  if (std::equal(pixels.begin(), pixels.end(), first)) {
    return absl::OkStatus();
  }
  std::copy(pixels.begin(), pixels.end(), first);
  revisions_[sheet] = NextRevision();
  return absl::OkStatus();
}

void GraphicsSheetStore::MarkAllSheetsChanged() {
  for (auto& revision : revisions_) {
    revision = NextRevision();
  }
}

void GraphicsSheetStore::Clear() {
  pixels_.clear();
  MarkAllSheetsChanged();
}

GraphicsSheetStore::Edit::Edit(GraphicsSheetStore& store, uint16_t sheet)
    : sheet_(sheet) {
  if (!store.HasSheet(sheet)) {
    return;
  }
  store_ = &store;
  const auto current = store.Sheet(sheet);
  before_.assign(current.begin(), current.end());
}

GraphicsSheetStore::Edit::~Edit() {
  Commit();
}

uint8_t GraphicsSheetStore::Edit::Get(int x, int y) const {
  if (!ok() || x < 0 || y < 0 || x >= width() || y >= height()) {
    return 0;
  }
  return store_->pixels_[sheet_ * kSheetBytes + y * kSheetWidth + x];
}

bool GraphicsSheetStore::Edit::Set(int x, int y, uint8_t index) {
  if (!ok() || x < 0 || y < 0 || x >= width() || y >= height()) {
    return false;
  }
  uint8_t& pixel = store_->pixels_[sheet_ * kSheetBytes + y * kSheetWidth + x];
  if (pixel == index) {
    return false;
  }
  pixel = index;
  uncommitted_ = true;
  return true;
}

absl::Span<const uint8_t> GraphicsSheetStore::Edit::pixels() const {
  return ok() ? store_->Sheet(sheet_) : absl::Span<const uint8_t>();
}

void GraphicsSheetStore::Edit::Commit() {
  if (!ok() || !uncommitted_) {
    return;
  }
  store_->revisions_[sheet_] = NextRevision();
  uncommitted_ = false;
}

}  // namespace zelda3
}  // namespace yaze
