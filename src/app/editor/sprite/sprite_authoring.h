#ifndef YAZE_APP_EDITOR_SPRITE_SPRITE_AUTHORING_H_
#define YAZE_APP_EDITOR_SPRITE_SPRITE_AUTHORING_H_

#include <algorithm>
#include <cmath>
#include <sstream>

#include "absl/status/statusor.h"

#include "app/editor/sprite/zsprite.h"
#include "app/gfx/types/snes_palette.h"
#include "core/sprite_asset.h"
#include "zelda3/sprite/sprite_oam_tables.h"

namespace yaze::editor::sprite_authoring {
inline absl::StatusOr<gfx::PaletteGroup> BindPaletteRows(
    const project::SpriteAssetBinding& binding,
    const gfx::PaletteGroup& defaults, const gfx::PaletteGroup& global,
    const gfx::PaletteGroup& aux1, const gfx::PaletteGroup& aux2,
    const gfx::PaletteGroup& aux3) {
  gfx::PaletteGroup result;
  for (size_t i = 0; i < 8; ++i) {
    const auto& row = binding.palette_rows[i];
    const gfx::PaletteGroup* group = nullptr;
    int index = row.index;
    if (row.group == "auto") {
      group = &defaults;
      index = i;
    } else if (row.group == "global_sprites")
      group = &global;
    else if (row.group == "sprites_aux1")
      group = &aux1;
    else if (row.group == "sprites_aux2")
      group = &aux2;
    else if (row.group == "sprites_aux3")
      group = &aux3;
    if (!group || index < 0 || static_cast<size_t>(index) >= group->size())
      return absl::OutOfRangeError(
          "Sprite palette binding unavailable in loaded ROM");
    result.AddPalette(group->palette(index));
  }
  return result;
}

inline absl::Status ValidateDrawAdapter(const zsprite::ZSprite& sprite,
                                        const std::string& adapter) {
  if (adapter == "literal_v1")
    return absl::OkStatus();
  if (adapter != "oracle_maple_v1")
    return absl::InvalidArgumentError("Unsupported draw adapter");
  for (const auto& frame : sprite.editor.Frames)
    for (const auto& tile : frame.Tiles)
      if (tile.x != 128 || !tile.size)
        return absl::FailedPreconditionError(
            "Maple's current driver requires X offset 0 and 16x16 tiles");
  return absl::OkStatus();
}

// ZSpriteMaker GetASM subtracts these canvas coordinates from stored bytes.
constexpr int kOriginX = 128;
constexpr int kOriginY = 112;
constexpr size_t kMaxFrames = 256;  // Animation endpoints are bytes.

inline int OffsetX(const zsprite::OamTile& tile) {
  return int(tile.x) - kOriginX;
}
inline int OffsetY(const zsprite::OamTile& tile) {
  return int(tile.y) - kOriginY;
}

inline zsprite::Frame CopyVanillaLayout(const zelda3::SpriteOamLayout& layout) {
  zsprite::Frame frame;
  // Registry entries have paint order; ZSM renders index zero last.
  for (auto it = layout.tiles.rbegin(); it != layout.tiles.rend(); ++it) {
    const auto& tile = *it;
    frame.Tiles.emplace_back(tile.x_offset + kOriginX, tile.y_offset + kOriginY,
                             tile.flip_x, tile.flip_y, tile.tile_id,
                             tile.palette, tile.size_16x16, 3);
  }
  return frame;
}

inline bool AppendFrame(zsprite::ZSprite& sprite, int copy_from = -1) {
  auto& frames = sprite.editor.Frames;
  if (frames.size() >= kMaxFrames || copy_from < -1 ||
      copy_from >= static_cast<int>(frames.size()))
    return false;
  // Copy before vector growth to avoid a reference into reallocated storage.
  auto frame = copy_from < 0 ? zsprite::Frame{} : frames[copy_from];
  frames.push_back(std::move(frame));
  return true;
}

inline bool DeleteFrame(zsprite::ZSprite& sprite, int index) {
  auto& frames = sprite.editor.Frames;
  if (frames.size() <= 1 || index < 0 ||
      index >= static_cast<int>(frames.size()))
    return false;
  frames.erase(frames.begin() + index);
  const int last = std::min(255, static_cast<int>(frames.size()) - 1);
  for (auto& animation : sprite.animations) {
    auto remap = [=](int endpoint) {
      return std::clamp(endpoint - (endpoint > index ? 1 : 0), 0, last);
    };
    animation.frame_start = remap(animation.frame_start);
    animation.frame_end =
        std::max<int>(animation.frame_start, remap(animation.frame_end));
  }
  return true;
}

// The inherited draw loop uses byte indices. Export only table data; dispatch,
// bank placement, OAM allocation and behavior remain owned by the ASM family.
inline absl::StatusOr<std::string> ExportDrawTables(
    const zsprite::ZSprite& sprite) {
  if (sprite.editor.Frames.empty() || sprite.editor.Frames.size() > kMaxFrames)
    return absl::InvalidArgumentError("Draw export needs 1 to 256 frames");
  std::ostringstream starts, counts, xs, ys, chars, properties, sizes;
  size_t total = 0;
  for (size_t i = 0; i < sprite.editor.Frames.size(); ++i) {
    const auto& frame = sprite.editor.Frames[i];
    if (frame.Tiles.empty() || total + frame.Tiles.size() > 128)
      return absl::InvalidArgumentError(
          "Draw export needs nonempty frames and at most 128 total tiles (word "
          "offset indexing)");
    if (i) {
      starts << ", ";
      counts << ", ";
    }
    starts << total;
    counts << frame.Tiles.size() - 1;
    for (const auto& tile : frame.Tiles) {
      if (tile.id > 511 || tile.palette > 7 || tile.priority > 3 || tile.z != 0)
        return absl::InvalidArgumentError(
            "Draw export requires tile 0..511, palette 0..7, priority 0..3 and "
            "Z=0");
      if (total++) {
        xs << ", ";
        ys << ", ";
        chars << ", ";
        properties << ", ";
        sizes << ", ";
      }
      xs << OffsetX(tile);
      ys << OffsetY(tile);
      chars << (tile.id & 255);
      properties << ((tile.mirror_y ? 128 : 0) | (tile.mirror_x ? 64 : 0) |
                     (tile.priority << 4) | (tile.palette << 1) |
                     (tile.id >> 8));
      sizes << (tile.size ? 2 : 0);
    }
  }
  return std::string(
             "; Yaze draw-table candidate. Review driver, bank and OAM "
             "allocation.\n") +
         ".start_index\n  db " + starts.str() + "\n.nbr_of_tiles\n  db " +
         counts.str() + "\n.x_offsets\n  dw " + xs.str() +
         "\n.y_offsets\n  dw " + ys.str() + "\n.chr\n  db " + chars.str() +
         "\n.properties\n  db " + properties.str() + "\n.sizes\n  db " +
         sizes.str() + "\n";
}

// Keep elapsed remainder and advance multiple frames when rendering is slow.
// Invalid imported ranges are bounded for playback without rewriting the file.
inline bool Advance(const zsprite::AnimationGroup& animation,
                    size_t frame_count, float delta, int& frame,
                    float& remainder) {
  if (!frame_count || !std::isfinite(delta) || delta < 0)
    return false;
  const int last = std::min<size_t>(255, frame_count - 1);
  const int start = std::min<int>(animation.frame_start, last);
  const int end = std::clamp<int>(animation.frame_end, start, last);
  const int before = frame;
  frame = std::clamp(frame, start, end);
  const double duration = std::max<int>(1, animation.frame_speed) / 60.0;
  const double elapsed =
      delta +
      (std::isfinite(remainder) && remainder > 0 ? double(remainder) : 0.0);
  const double steps = std::floor(elapsed / duration);
  const int length = end - start + 1;
  frame = start +
          (frame - start + static_cast<int>(std::fmod(steps, length))) % length;
  remainder = static_cast<float>(std::fmod(elapsed, duration));
  return frame != before;
}
}  // namespace yaze::editor::sprite_authoring
#endif
