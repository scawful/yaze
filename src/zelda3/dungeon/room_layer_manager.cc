#include "zelda3/dungeon/room_layer_manager.h"

#include <algorithm>
#include <array>
#include <vector>

#include "SDL.h"
#include "app/gfx/types/snes_color_math.h"
#include "app/gfx/types/snes_palette.h"
#include "util/log.h"

namespace yaze {
namespace zelda3 {

namespace {

// Helper to copy SDL palette from source surface to destination bitmap
// Uses vector extraction + SetPalette for reliable palette application
void ApplySDLPaletteToBitmap(SDL_Surface* src_surface,
                             gfx::Bitmap& dst_bitmap) {
  if (!src_surface || !src_surface->format)
    return;

  SDL_Palette* src_pal = src_surface->format->palette;
  if (!src_pal || src_pal->ncolors == 0)
    return;

  // Extract palette colors into a vector
  std::vector<SDL_Color> colors(256);
  int colors_to_copy = std::min(src_pal->ncolors, 256);
  for (int i = 0; i < colors_to_copy; ++i) {
    colors[i] = src_pal->colors[i];
  }

  // Fill remaining with transparent black (prevents undefined colors)
  for (int i = colors_to_copy; i < 256; ++i) {
    colors[i] = {0, 0, 0, 0};
  }

  // Dungeon rendering uses palette index 255 as the "undrawn/transparent" fill.
  // Palette index 0 is not written by our tile renderer (pixel value 0 is skipped),
  // so we reserve it as an opaque backdrop color for the composited output.
  colors[0] = {0, 0, 0, 255};
  colors[255] = {0, 0, 0, 0};

  // Apply palette to destination bitmap using the reliable method
  dst_bitmap.SetPalette(colors);
}

}  // namespace

void RoomLayerManager::CompositeToOutput(Room& room,
                                         gfx::Bitmap& output) const {
  constexpr int kWidth = 512;
  constexpr int kHeight = 512;
  constexpr int kPixelCount = kWidth * kHeight;

  // Log layer visibility and blend modes (once per room)
  static int last_room_id = -1;
  if (room.id() != last_room_id) {
    last_room_id = room.id();
    LOG_DEBUG("LayerManager",
              "Room %03X: BG1_Layout(vis=%d,blend=%d) "
              "BG1_Objects(vis=%d,blend=%d) BG2_Layout(vis=%d,blend=%d) "
              "BG2_Objects(vis=%d,blend=%d) MergeType=%d",
              room.id(), IsLayerVisible(LayerType::BG1_Layout),
              static_cast<int>(GetLayerBlendMode(LayerType::BG1_Layout)),
              IsLayerVisible(LayerType::BG1_Objects),
              static_cast<int>(GetLayerBlendMode(LayerType::BG1_Objects)),
              IsLayerVisible(LayerType::BG2_Layout),
              static_cast<int>(GetLayerBlendMode(LayerType::BG2_Layout)),
              IsLayerVisible(LayerType::BG2_Objects),
              static_cast<int>(GetLayerBlendMode(LayerType::BG2_Objects)),
              current_merge_type_id_);
  }

  // Get all 4 layer buffers
  auto& bg1_layout = GetLayerBuffer(room, LayerType::BG1_Layout);
  auto& bg1_objects = GetLayerBuffer(room, LayerType::BG1_Objects);
  auto& bg2_layout = GetLayerBuffer(room, LayerType::BG2_Layout);
  auto& bg2_objects = GetLayerBuffer(room, LayerType::BG2_Objects);

  auto layer_enabled = [&](LayerType type,
                           const gfx::BackgroundBuffer& buffer) {
    if (!IsLayerVisible(type) || IsHiddenByGame(type) ||
        GetLayerBlendMode(type) == LayerBlendMode::Off) {
      return false;
    }
    const auto& bitmap = buffer.bitmap();
    return bitmap.is_active() && bitmap.width() > 0;
  };

  const bool bg1_layout_on = layer_enabled(LayerType::BG1_Layout, bg1_layout);
  const bool bg1_obj_on = layer_enabled(LayerType::BG1_Objects, bg1_objects);
  const bool bg2_layout_on = layer_enabled(LayerType::BG2_Layout, bg2_layout);
  const bool bg2_obj_on = layer_enabled(LayerType::BG2_Objects, bg2_objects);

  const bool game_color_math =
      game_registers_applied_ && upper_tilemap_blended_;
  const bool bg2_layout_translucent =
      bg2_layout_on &&
      GetLayerBlendMode(LayerType::BG2_Layout) == LayerBlendMode::Translucent;
  const bool bg2_objects_translucent =
      bg2_obj_on &&
      GetLayerBlendMode(LayerType::BG2_Objects) == LayerBlendMode::Translucent;
  const bool true_color = !UpperTilemapCoversLower(room.layer2_mode()) &&
                          (use_priority_compositing_ ||
                           current_merge_type_id_ == 7 || game_color_math) &&
                          (bg2_layout_translucent || bg2_objects_translucent);
  const int depth = true_color ? 32 : 8;
  if (output.width() != kWidth || output.height() != kHeight ||
      output.depth() != depth) {
    output.Create(kWidth, kHeight, depth,
                  std::vector<uint8_t>(kPixelCount * (true_color ? 4 : 1), 0));
  } else {
    output.Fill(0);
  }
  output.metadata().purpose = gfx::Bitmap::BitmapPurpose::kCompositeOutput;
  // Preserve source indices; only the presentation image becomes RGBA.
  bool palette_copied = false;
  std::array<SDL_Color, 256> pal_lut{};
  pal_lut.fill({0, 0, 0, 255});
  auto bg1_revealed_at = [&](const gfx::BackgroundBuffer& target, int index) {
    // Water objects' legacy reveal masks emulate opaque cross-layer drawing.
    // Main/subscreen color math needs both tilemaps intact. Actual transparent
    // writes still replace their own tilemap through the coverage buffers.
    if (game_color_math) {
      return false;
    }
    const auto& mask = target.bg1_reveal_mask_data();
    if (index < 0 || index >= static_cast<int>(mask.size())) {
      return false;
    }
    const uint8_t value = mask[index];
    const bool layout_reveal =
        bg2_layout_on &&
        (value & static_cast<uint8_t>(gfx::BG1RevealMaskSource::kBG2Layout)) !=
            0;
    const bool object_reveal =
        bg2_obj_on &&
        (value & static_cast<uint8_t>(gfx::BG1RevealMaskSource::kBG2Objects)) !=
            0;
    return layout_reveal || object_reveal;
  };

  // Copy palette from first available visible layer
  auto CopyPaletteIfNeeded = [&](const gfx::Bitmap& src_bitmap) {
    if (!palette_copied && src_bitmap.surface()) {
      if (const auto* palette = src_bitmap.surface()->format->palette) {
        std::copy_n(palette->colors, std::min(palette->ncolors, 256),
                    pal_lut.begin());
      }
      pal_lut[0] = {0, 0, 0, 255};
      pal_lut[255] = {0, 0, 0, 0};
      if (!true_color)
        ApplySDLPaletteToBitmap(src_bitmap.surface(), output);
      palette_copied = true;
    }
  };

  if (UpperTilemapCoversLower(room.layer2_mode())) {
    // Header layer mode 6 uses the upper dungeon tilemap on the SNES main
    // screen and the lower tilemap on the sub screen. Ignoring OBJ/BG3, an
    // opaque upper pixel wins regardless of either tile's priority bit; the
    // lower pixel is visible only where the upper tilemap is transparent.
    //
    // Yaze's historic BG1/BG2 names describe semantic editor layers here:
    // BG1_* is the upper tilemap ($7E2000 / hardware BG2), while BG2_* is the
    // lower tilemap ($7E4000 / hardware BG1).
    if (bg1_layout_on) {
      CopyPaletteIfNeeded(bg1_layout.bitmap());
    }
    if (bg1_obj_on) {
      CopyPaletteIfNeeded(bg1_objects.bitmap());
    }
    if (bg2_layout_on) {
      CopyPaletteIfNeeded(bg2_layout.bitmap());
    }
    if (bg2_obj_on) {
      CopyPaletteIfNeeded(bg2_objects.bitmap());
    }

    const auto& upper_layout_px = bg1_layout.bitmap().data();
    const auto& upper_object_px = bg1_objects.bitmap().data();
    const auto& lower_layout_px = bg2_layout.bitmap().data();
    const auto& lower_object_px = bg2_objects.bitmap().data();
    const auto& upper_object_coverage = bg1_objects.coverage_data();
    const auto& lower_object_coverage = bg2_objects.coverage_data();

    auto resolve_tilemap_pixel =
        [&](bool layout_on, bool objects_on, const uint8_t* layout_pixels,
            const uint8_t* object_pixels,
            const std::vector<uint8_t>& object_coverage, int index) -> uint8_t {
      if (objects_on) {
        const bool object_wrote =
            (index < static_cast<int>(object_coverage.size()) &&
             object_coverage[index] != 0) ||
            !IsTransparent(object_pixels[index]);
        if (object_wrote) {
          return object_pixels[index];
        }
      }
      return layout_on ? layout_pixels[index] : 255;
    };

    auto& dst_data = output.mutable_data();
    for (int idx = 0; idx < kPixelCount; ++idx) {
      const uint8_t upper_pixel =
          resolve_tilemap_pixel(bg1_layout_on, bg1_obj_on, upper_layout_px,
                                upper_object_px, upper_object_coverage, idx);
      const uint8_t lower_pixel =
          resolve_tilemap_pixel(bg2_layout_on, bg2_obj_on, lower_layout_px,
                                lower_object_px, lower_object_coverage, idx);

      if (!IsTransparent(upper_pixel)) {
        dst_data[idx] = upper_pixel;
      } else if (!IsTransparent(lower_pixel)) {
        dst_data[idx] = lower_pixel;
      }
    }
  } else if (use_priority_compositing_ || current_merge_type_id_ == 0x07 ||
             game_color_math) {
    // Priority compositing (SNES Mode 1):
    // - BG2 priority=0 is behind BG1 priority=0.
    // - BG2 priority=1 can appear above BG1 priority=0.
    // - BG1 priority=1 is above BG2 priority=1.
    //
    // We first combine Layout+Objects for each BG (objects overwrite layout),
    // then resolve BG1 vs BG2 per-pixel using the stored priority buffers.
    // Game color math and mode 7 still blend with tile priority turned off.
    // In that case rank_for below retains the simple upper-over-lower order.
    // CGADSUB arithmetic is in five-bit channels, before display expansion.
    const bool subtract = game_registers_applied_ && (cgadsub_ & 0x80);
    const bool halve = game_registers_applied_ ? (cgadsub_ & 0x40) != 0
                                               : current_merge_type_id_ != 0x07;
    // Ensure the output palette matches the room's SDL palette.
    if (layer_enabled(LayerType::BG1_Layout, bg1_layout)) {
      CopyPaletteIfNeeded(bg1_layout.bitmap());
    }
    if (layer_enabled(LayerType::BG1_Objects, bg1_objects)) {
      CopyPaletteIfNeeded(bg1_objects.bitmap());
    }
    if (layer_enabled(LayerType::BG2_Layout, bg2_layout)) {
      CopyPaletteIfNeeded(bg2_layout.bitmap());
    }
    if (layer_enabled(LayerType::BG2_Objects, bg2_objects)) {
      CopyPaletteIfNeeded(bg2_objects.bitmap());
    }

    auto write_color = [&](int pixel, SDL_Color color) {
      auto* dst = output.mutable_data().data() + pixel * 4;
      dst[0] = color.r;
      dst[1] = color.g;
      dst[2] = color.b;
      dst[3] = 255;
    };
    auto write_index = [&](int pixel, uint8_t index) {
      if (true_color) {
        write_color(pixel, pal_lut[index]);
      } else {
        output.mutable_data()[pixel] = index;
      }
    };
    auto blend_color = [&](uint8_t main_index, uint8_t sub_index) {
      const auto& main = pal_lut[main_index];
      const auto& sub = pal_lut[sub_index];
      const auto channel = [&](uint8_t m, uint8_t s) {
        return gfx::ExpandSnesChannel(
            gfx::SnesColorMathChannel(m >> 3, s >> 3, subtract, halve));
      };
      return SDL_Color{channel(main.r, sub.r), channel(main.g, sub.g),
                       channel(main.b, sub.b), 255};
    };

    auto normalize_pri = [](uint8_t pri) -> uint8_t {
      // 0xFF is our "unset" value. Treat unset as low priority.
      return (pri == 0xFF) ? 0 : (pri ? 1 : 0);
    };

    // Mode 1 order: hardware BG1 p1 > BG2 p1 > BG1 p0 > BG2 p0. When both
    // tilemaps are on the main screen the lower one is hardware BG1 and wins
    // ties; otherwise keep the historic upper-first order.
    const bool lower_wins_ties = LowerTilemapWinsTies();
    auto rank_for = [&](bool is_bg1, uint8_t pri) -> int {
      pri = use_priority_compositing_ ? normalize_pri(pri) : 0;
      const bool first = lower_wins_ties ? !is_bg1 : is_bg1;
      if (first) {
        return pri ? 3 : 1;
      }
      return pri ? 2 : 0;
    };

    const auto& bg1_layout_px = bg1_layout.bitmap().data();
    const auto& bg1_obj_px = bg1_objects.bitmap().data();
    const auto& bg2_layout_px = bg2_layout.bitmap().data();
    const auto& bg2_obj_px = bg2_objects.bitmap().data();
    const auto& bg1_layout_pri = bg1_layout.priority_data();
    const auto& bg1_obj_pri = bg1_objects.priority_data();
    const auto& bg2_layout_pri = bg2_layout.priority_data();
    const auto& bg2_obj_pri = bg2_objects.priority_data();
    const auto& bg1_obj_cov = bg1_objects.coverage_data();
    const auto& bg2_obj_cov = bg2_objects.coverage_data();

    for (int idx = 0; idx < kPixelCount; ++idx) {
      write_index(idx, 0);
      uint8_t bg1_pixel = 255;
      uint8_t bg1_pri = 0;
      const bool bg1_obj_wrote =
          bg1_obj_on && ((idx < static_cast<int>(bg1_obj_cov.size()) &&
                          bg1_obj_cov[idx] != 0) ||
                         !IsTransparent(bg1_obj_px[idx]));
      if (bg1_obj_wrote) {
        if (!bg1_revealed_at(bg1_objects, idx)) {
          bg1_pixel = bg1_obj_px[idx];
          bg1_pri = bg1_obj_pri[idx];
        }
      } else if (bg1_layout_on && !IsTransparent(bg1_layout_px[idx])) {
        if (!bg1_revealed_at(bg1_layout, idx)) {
          bg1_pixel = bg1_layout_px[idx];
          bg1_pri = bg1_layout_pri[idx];
        }
      }

      uint8_t bg2_pixel = 255;
      uint8_t bg2_pri = 0;
      bool bg2_pixel_translucent = false;
      const bool bg2_obj_wrote =
          bg2_obj_on && ((idx < static_cast<int>(bg2_obj_cov.size()) &&
                          bg2_obj_cov[idx] != 0) ||
                         !IsTransparent(bg2_obj_px[idx]));
      if (bg2_obj_wrote) {
        bg2_pixel = bg2_obj_px[idx];
        bg2_pri = bg2_obj_pri[idx];
        bg2_pixel_translucent = bg2_objects_translucent;
      } else if (bg2_layout_on && !IsTransparent(bg2_layout_px[idx])) {
        bg2_pixel = bg2_layout_px[idx];
        bg2_pri = bg2_layout_pri[idx];
        bg2_pixel_translucent = bg2_layout_translucent;
      }

      if (IsTransparent(bg1_pixel)) {
        if (!IsTransparent(bg2_pixel)) {
          if (game_color_math && (cgadsub_ & 0x20) && bg2_pixel_translucent &&
              (bg1_layout_on || bg1_obj_on)) {
            // A transparent main tile exposes the backdrop, which CGADSUB
            // can also blend. Explicitly hiding the main layers is an editor
            // inspection view and continues to show the sub layer alone.
            write_color(idx, blend_color(0, bg2_pixel));
          } else {
            write_index(idx, bg2_pixel);
          }
        }
        continue;
      }
      if (IsTransparent(bg2_pixel)) {
        write_index(idx, bg1_pixel);
        continue;
      }

      // Both layers have opaque pixels. Resolve using priority + blend mode.
      const int r1 = rank_for(/*is_bg1=*/true, bg1_pri);
      const int r2 = rank_for(/*is_bg1=*/false, bg2_pri);

      // Upper tilemap is hardware BG2 on the main screen. Subtraction must
      // preserve that operand order even when inspecting tile priorities.
      // Hidden or overwritten BG2 sources never participate in color math.
      if (bg2_pixel_translucent && true_color) {
        write_color(idx, blend_color(bg1_pixel, bg2_pixel));
      } else {
        write_index(idx, (r1 >= r2) ? bg1_pixel : bg2_pixel);
      }
    }
  } else {
    // Fallback to simple back-to-front layer order: BG2 then BG1.
    //
    // Blend Modes (from LayerMergeType):
    // - Normal: Opaque pixels overwrite destination (standard)
    // - Translucent: 50% alpha blend with destination
    // - Addition: Additive color blending (SNES color math)
    // - Dark: Darkened blend (reduced brightness)
    // - Off: Layer is hidden
    //
    // Transparent BG1 pixels and active source-owned reveal bits expose BG2.
    // The deferred bits preserve raw BG1 when their owning BG2 layer is hidden.
    auto CompositeLayer = [&](gfx::BackgroundBuffer& buffer,
                              LayerType layer_type) {
      if (!layer_enabled(layer_type, buffer))
        return;

      const auto& src_bitmap = buffer.bitmap();
      LayerBlendMode blend_mode = GetLayerBlendMode(layer_type);

      CopyPaletteIfNeeded(src_bitmap);

      const auto& src_data = src_bitmap.data();
      auto& dst_data = output.mutable_data();

      // Get layer alpha for translucent/dark blending
      uint8_t layer_alpha = GetLayerAlpha(layer_type);

      for (int idx = 0; idx < kPixelCount; ++idx) {
        const bool is_bg1_layer = layer_type == LayerType::BG1_Layout ||
                                  layer_type == LayerType::BG1_Objects;
        if (is_bg1_layer && bg1_revealed_at(buffer, idx)) {
          continue;
        }
        uint8_t src_pixel = src_data[idx];

        // Skip transparent pixels (255 = fill color for undrawn areas).
        if (IsTransparent(src_pixel))
          continue;

        // Apply blend mode
        switch (blend_mode) {
          case LayerBlendMode::Normal:
            // Standard opaque overwrite
            dst_data[idx] = src_pixel;
            break;

          case LayerBlendMode::Translucent:
            // Fallback alpha approximation for paths without the palette-aware
            // priority compositor above. This is not a pixel-exact SNES model.
            if (IsTransparent(dst_data[idx]) || layer_alpha > 180) {
              dst_data[idx] = src_pixel;
            }
            // If layer_alpha <= 180, destination shows through (simplified blend)
            break;

          case LayerBlendMode::Addition:
            // Additive blending: in indexed mode, just use source if visible
            // True additive would need RGB values from palette
            if (IsTransparent(dst_data[idx])) {
              dst_data[idx] = src_pixel;
            } else {
              dst_data[idx] = src_pixel;
            }
            break;

          case LayerBlendMode::Dark:
            // Darkened blend: overwrite but surface will be color-modulated later
            dst_data[idx] = src_pixel;
            break;

          case LayerBlendMode::Off:
            // Layer hidden - should not reach here due to early return
            break;
        }
      }
    };

    // Process all layers in back-to-front order (matching SNES hardware)
    // BG2 is the lower/background layer, BG1 is the upper/foreground layer.
    CompositeLayer(bg2_layout, LayerType::BG2_Layout);
    CompositeLayer(bg2_objects, LayerType::BG2_Objects);
    CompositeLayer(bg1_layout, LayerType::BG1_Layout);
    CompositeLayer(bg1_objects, LayerType::BG1_Objects);
  }

  // If no palette was copied from layers, try to get it from bg1_buffer directly
  if (!palette_copied && !true_color) {
    const auto& bg1_bitmap = room.bg1_buffer().bitmap();
    if (bg1_bitmap.surface()) {
      ApplySDLPaletteToBitmap(bg1_bitmap.surface(), output);
    }
  }

  // Sync pixel data to SDL surface for texture creation
  output.UpdateSurfacePixels();

  // Set up transparency and effects for the composite output
  if (output.surface()) {
    // IMPORTANT: Use the same transparency setup as room.cc's set_dungeon_palette
    // Color key on index 255 (unused in 90-color dungeon palette)
    SDL_SetColorKey(output.surface(), true_color ? SDL_FALSE : SDL_TRUE, 255);
    SDL_SetSurfaceBlendMode(output.surface(), SDL_BLENDMODE_BLEND);

    // Apply DarkRoom effect if merge type is 0x08
    // This simulates the SNES master brightness reduction for unlit rooms
    if (current_merge_type_id_ == 0x08) {
      // Apply color modulation to darken the output (50% brightness)
      SDL_SetSurfaceColorMod(output.surface(), 128, 128, 128);
    } else {
      // Reset to full brightness for non-dark rooms
      SDL_SetSurfaceColorMod(output.surface(), 255, 255, 255);
    }
  }

  // Mark output as modified for texture update
  output.set_modified(true);
}

}  // namespace zelda3
}  // namespace yaze
