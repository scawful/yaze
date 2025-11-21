#include "texture_atlas.h"

#include "util/log.h"

namespace yaze {
namespace gfx {

TextureAtlas::TextureAtlas(int width, int height)
    : width_(width), height_(height) {
  // Create atlas bitmap with initial empty data
  std::vector<uint8_t> empty_data(width * height, 0);
  atlas_bitmap_ = Bitmap(width, height, 8, empty_data);
  LOG_DEBUG("[TextureAtlas]", "Created %dx%d atlas", width, height);
}

TextureAtlas::AtlasRegion* TextureAtlas::AllocateRegion(int source_id,
                                                        int width, int height) {
  // Simple linear packing algorithm
  // TODO: Implement more efficient rect packing (shelf, guillotine, etc.)

  int pack_x, pack_y;
  if (!TryPackRect(width, height, pack_x, pack_y)) {
    LOG_DEBUG("[TextureAtlas]",
              "Failed to allocate %dx%d region for source %d (atlas full)",
              width, height, source_id);
    return nullptr;
  }

  AtlasRegion region;
  region.x = pack_x;
  region.y = pack_y;
  region.width = width;
  region.height = height;
  region.source_id = source_id;
  region.in_use = true;

  regions_[source_id] = region;

  LOG_DEBUG("[TextureAtlas]", "Allocated region (%d,%d,%dx%d) for source %d",
            pack_x, pack_y, width, height, source_id);

  return &regions_[source_id];
}

absl::Status TextureAtlas::PackBitmap(const Bitmap& src,
                                      const AtlasRegion& region) {
  if (!region.in_use) {
    return absl::FailedPreconditionError("Region not allocated");
  }

  if (!src.is_active() || src.width() == 0 || src.height() == 0) {
    return absl::InvalidArgumentError("Source bitmap not active");
  }

  if (region.width < src.width() || region.height < src.height()) {
    return absl::InvalidArgumentError("Region too small for bitmap");
  }

  // TODO: Implement pixel copying from src to atlas_bitmap_ at region
  // coordinates For now, just return OK (stub implementation)

  LOG_DEBUG("[TextureAtlas]",
            "Packed %dx%d bitmap into region at (%d,%d) for source %d",
            src.width(), src.height(), region.x, region.y, region.source_id);

  return absl::OkStatus();
}

absl::Status TextureAtlas::DrawRegion(int source_id, int /*dest_x*/,
                                      int /*dest_y*/) {
  auto it = regions_.find(source_id);
  if (it == regions_.end() || !it->second.in_use) {
    return absl::NotFoundError("Region not found or not in use");
  }

  // TODO: Integrate with renderer to draw atlas region at (dest_x, dest_y)
  // For now, just return OK (stub implementation)

  return absl::OkStatus();
}

void TextureAtlas::FreeRegion(int source_id) {
  auto it = regions_.find(source_id);
  if (it != regions_.end()) {
    it->second.in_use = false;
    LOG_DEBUG("[TextureAtlas]", "Freed region for source %d", source_id);
  }
}

void TextureAtlas::Clear() {
  regions_.clear();
  next_x_ = 0;
  next_y_ = 0;
  row_height_ = 0;
  LOG_DEBUG("[TextureAtlas]", "Cleared all regions");
}

const TextureAtlas::AtlasRegion* TextureAtlas::GetRegion(int source_id) const {
  auto it = regions_.find(source_id);
  if (it != regions_.end() && it->second.in_use) {
    return &it->second;
  }
  return nullptr;
}

TextureAtlas::AtlasStats TextureAtlas::GetStats() const {
  AtlasStats stats;
  stats.total_pixels = width_ * height_;
  stats.total_regions = regions_.size();

  for (const auto& [id, region] : regions_) {
    if (region.in_use) {
      stats.used_regions++;
      stats.used_pixels += region.width * region.height;
    }
  }

  if (stats.total_pixels > 0) {
    stats.utilization =
        static_cast<float>(stats.used_pixels) / stats.total_pixels * 100.0f;
  }

  return stats;
}

bool TextureAtlas::TryPackRect(int width, int height, int& out_x, int& out_y) {
  // Simple shelf packing algorithm
  // Try to pack in current row
  if (next_x_ + width <= width_) {
    // Fits in current row
    out_x = next_x_;
    out_y = next_y_;
    next_x_ += width;
    row_height_ = std::max(row_height_, height);
    return true;
  }

  // Move to next row
  next_x_ = 0;
  next_y_ += row_height_;
  row_height_ = 0;

  // Check if fits in new row
  if (next_y_ + height <= height_ && width <= width_) {
    out_x = next_x_;
    out_y = next_y_;
    next_x_ += width;
    row_height_ = height;
    return true;
  }

  // Atlas is full
  return false;
}

}  // namespace gfx
}  // namespace yaze
