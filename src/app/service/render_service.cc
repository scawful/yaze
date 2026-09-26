#include "app/service/render_service.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "app/gfx/core/bitmap.h"
#include "app/platform/sdl_compat.h"
#include "app/service/headless_overlay_renderer.h"
#include "util/macro.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_layer_manager.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_map.h"
#include "zelda3/overworld/overworld_version_helper.h"

#include <SDL.h>
#ifdef YAZE_CLI_HAS_PNG
#include <png.h>
#endif

namespace yaze {
namespace app {
namespace service {

namespace {

absl::Status ValidateRenderScale(float scale) {
  if (!std::isfinite(scale) || scale < 0.25f || scale > 8.0f) {
    return absl::InvalidArgumentError(
        "Render scale must be finite and between 0.25 and 8.0");
  }
  return absl::OkStatus();
}

#ifdef YAZE_CLI_HAS_PNG
// PNG write helpers (mirrored from visual_diff_engine.cc).
struct PngCtx {
  std::vector<uint8_t>* buf;
};

void PngWrite(png_structp png, png_bytep data, png_size_t len) {
  auto* ctx = static_cast<PngCtx*>(png_get_io_ptr(png));
  ctx->buf->insert(ctx->buf->end(), data, data + len);
}

void PngFlush(png_structp /*png*/) {}
#endif

// Returns the overlay RGBA color for a custom-collision tile value.
// Returns alpha=0 for tiles we don't want to highlight.
struct TileColor {
  uint8_t r, g, b, a;
};

TileColor CollisionColor(uint8_t tile) {
  if (tile == 0)
    return {0, 0, 0, 0};
  if (tile == 0xB0 || tile == 0xB1)
    return {0, 210, 170, 150};  // straight — teal
  if (tile >= 0xB2 && tile <= 0xB5)
    return {0, 190, 255, 150};  // corner — cyan
  if (tile == 0xB6)
    return {255, 215, 0, 170};  // intersection — gold
  if (tile >= 0xB7 && tile <= 0xBA)
    return {255, 80, 0, 200};  // stop — orange-red
  if (tile >= 0xBB && tile <= 0xBE)
    return {80, 150, 255, 150};  // T-junction — blue
  if (tile >= 0xD0 && tile <= 0xD3)
    return {210, 0, 255, 170};  // switch corner — magenta
  return {140, 140, 140, 120};  // other non-zero — grey
}

// 3x5 glyphs for overlay labels; each row is 3 bits, MSB = left column.
const uint8_t* GlyphFor(char c) {
  static const uint8_t kDigits[16][5] = {
      {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7},
      {5, 5, 7, 1, 1}, {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 1, 2, 2},
      {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7}, {2, 5, 7, 5, 5}, {6, 5, 6, 5, 6},
      {3, 4, 4, 4, 3}, {6, 5, 5, 5, 6}, {7, 4, 6, 4, 7}, {7, 4, 6, 4, 4}};
  static const uint8_t kX[5] = {5, 5, 2, 5, 5};
  static const uint8_t kH[5] = {5, 5, 7, 5, 5};
  static const uint8_t kI[5] = {7, 2, 2, 2, 7};
  static const uint8_t kColon[5] = {0, 2, 0, 2, 0};
  if (c >= '0' && c <= '9')
    return kDigits[c - '0'];
  if (c >= 'A' && c <= 'F')
    return kDigits[10 + (c - 'A')];
  if (c == 'X')
    return kX;
  if (c == 'H')
    return kH;
  if (c == 'I')
    return kI;
  if (c == ':')
    return kColon;
  return nullptr;
}

// Draw a label with a dark backing box; font pixels are 2x2 area pixels.
void DrawLabel(HeadlessOverlayRenderer& draw, float x, float y,
               const std::string& text, uint8_t r, uint8_t g, uint8_t b) {
  constexpr float kPx = 2.0f;
  const float w = static_cast<float>(text.size()) * 4 * kPx + kPx;
  const float h = 7 * kPx;
  draw.DrawFilledRect(x, y, w, h, 0, 0, 0, 200);
  for (size_t i = 0; i < text.size(); ++i) {
    const uint8_t* glyph = GlyphFor(text[i]);
    if (!glyph)
      continue;
    const float gx = x + kPx + static_cast<float>(i) * 4 * kPx;
    for (int row = 0; row < 5; ++row) {
      for (int col = 0; col < 3; ++col) {
        if (glyph[row] & (4 >> col)) {
          draw.DrawFilledRect(gx + col * kPx, y + kPx + row * kPx, kPx, kPx, r,
                              g, b, 255);
        }
      }
    }
  }
}

struct PhaseColor {
  uint8_t r, g, b;
};

PhaseColor SpritePhaseColor(int phase) {
  switch (phase) {
    case 0:
      return {80, 255, 120};  // beginning: green
    case 1:
      return {255, 80, 80};  // first part: red
    default:
      return {90, 170, 255};  // second part: blue
  }
}

}  // namespace

absl::StatusOr<float> ParseRenderScale(absl::string_view value) {
  float scale = 0;
  if (!absl::SimpleAtof(value, &scale)) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid scale value: %s", value));
  }
  if (const auto status = ValidateRenderScale(scale); !status.ok()) {
    return status;
  }
  return scale;
}

RenderService::RenderService(Rom* rom, zelda3::GameData* game_data)
    : rom_(rom), game_data_(game_data) {}

absl::StatusOr<RenderResult> RenderService::RenderDungeonRoom(
    const RenderRequest& req) {
  if (const auto status = ValidateRenderScale(req.scale); !status.ok()) {
    return status;
  }
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  if (!game_data_) {
    return absl::FailedPreconditionError("GameData not available");
  }
  if (req.room_id < 0 || req.room_id >= zelda3::kNumberOfRooms) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid room_id 0x%02X", req.room_id));
  }

#ifndef YAZE_CLI_HAS_PNG
  return absl::UnimplementedError("PNG encoding unavailable (libpng missing)");
#endif

  std::lock_guard<std::mutex> lock(mu_);

  // Load room data from ROM (header, objects, pots, torches, blocks, pits).
  zelda3::Room room = zelda3::LoadRoomFromRom(rom_, req.room_id);
  room.SetGameData(game_data_);

  // Load sprites (requires ROM, game_data not needed here).
  room.LoadSprites();

  // Load graphics sheets and render tiles to BackgroundBuffers (CPU only).
  room.LoadRoomGraphics();
  room.RenderRoomGraphics();

  // Composite all layers to a single Bitmap (CPU, SDL surface with palette).
  zelda3::RoomLayerManager layer_mgr;
  layer_mgr.ApplyLayerMerging(room.layer_merging());
  layer_mgr.ApplyRoomEffect(room.effect());
  layer_mgr.ApplyGameLayerRegisters(room.GameLayerRegisters());
  auto& composite = room.GetCompositeBitmap(layer_mgr);

  if (!composite.is_active() || composite.width() <= 0 ||
      composite.height() <= 0) {
    return absl::InternalError("Composite bitmap is empty after render");
  }

  const int out_w = static_cast<int>(composite.width() * req.scale);
  const int out_h = static_cast<int>(composite.height() * req.scale);

  // Convert indexed+palette bitmap to RGBA bytes.
  auto rgba_or = BitmapToRgba(composite, out_w, out_h);
  if (!rgba_or.ok())
    return rgba_or.status();
  auto rgba = std::move(rgba_or).value();

  // Paint overlays directly into the RGBA buffer.
  if (req.overlay_flags != RenderOverlay::kNone) {
    ApplyOverlays(rgba, out_w, out_h, room, req.overlay_flags, req.scale);
  }

  // Encode to PNG.
  auto png_or = EncodePng(rgba, out_w, out_h);
  if (!png_or.ok())
    return png_or.status();

  RenderResult result;
  result.png_data = std::move(png_or).value();
  result.width = out_w;
  result.height = out_h;
  return result;
}

absl::StatusOr<OverworldRenderResult> RenderService::RenderOverworldArea(
    const OverworldRenderRequest& req) {
  if (const auto status = ValidateRenderScale(req.scale); !status.ok()) {
    return status;
  }
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  if (!game_data_) {
    return absl::FailedPreconditionError("GameData not available");
  }
  if (req.screen_id < 0 || req.screen_id >= zelda3::kNumOverworldMaps) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid overworld screen 0x%02X", req.screen_id));
  }
  if (req.phase < -1 || req.phase > 2) {
    return absl::InvalidArgumentError("Sprite phase must be 0, 1, or 2");
  }

#ifndef YAZE_CLI_HAS_PNG
  return absl::UnimplementedError("PNG encoding unavailable (libpng missing)");
#endif

  std::lock_guard<std::mutex> lock(mu_);

  zelda3::Overworld overworld(rom_, game_data_);
  RETURN_IF_ERROR(overworld.Load(rom_));

  const auto* requested = overworld.overworld_map(req.screen_id);
  if (requested == nullptr) {
    return absl::InternalError("Overworld map missing after load");
  }
  const int parent = requested->parent();
  const auto* parent_map = overworld.overworld_map(parent);
  if (parent_map == nullptr) {
    return absl::InternalError("Overworld parent map missing after load");
  }

  int cols = 1;
  int rows = 1;
  std::string area_name = "small";
  switch (parent_map->area_size()) {
    case zelda3::AreaSizeEnum::LargeArea:
      cols = rows = 2;
      area_name = "large";
      break;
    case zelda3::AreaSizeEnum::WideArea:
      cols = 2;
      area_name = "wide";
      break;
    case zelda3::AreaSizeEnum::TallArea:
      rows = 2;
      area_name = "tall";
      break;
    case zelda3::AreaSizeEnum::SmallArea:
      break;
  }
  if (area_name == "small" && parent_map->is_large_map()) {
    cols = rows = 2;
    area_name = "large";
  }

  constexpr int kScreen = 512;
  const int width = cols * kScreen;
  const int height = rows * kScreen;
  std::vector<uint8_t> native(static_cast<size_t>(width) * height * 4, 0);

  OverworldRenderResult result;
  result.requested_screen = req.screen_id;
  result.parent_screen = parent;
  result.area_size = area_name;

  auto palette_rgb = [](const gfx::SnesPalette& palette, uint8_t idx,
                        uint8_t& r, uint8_t& g, uint8_t& b) {
    r = g = b = 0;
    if (idx < palette.size()) {
      const auto rgb = palette[idx].rgb();
      r = static_cast<uint8_t>(rgb.x);
      g = static_cast<uint8_t>(rgb.y);
      b = static_cast<uint8_t>(rgb.z);
    }
  };

  for (int dy = 0; dy < rows; ++dy) {
    for (int dx = 0; dx < cols; ++dx) {
      const int screen = parent + dx + dy * 8;
      // Subscreen overlay layer (sky, fog, lava, canopy, rain): drawn with
      // this screen's graphics and palette, like the game's BG1.
      std::vector<uint8_t> overlay_pixels;
      bool overlay_is_background = false;
      if (req.area_overlay) {
        ASSIGN_OR_RETURN(auto layer,
                         overworld.BuildSubscreenOverlayLayer(screen));
        if (layer.overlay_screen >= 0) {
          overlay_pixels = std::move(layer.pixels);
          overlay_is_background = layer.background;
          result.subscreen_overlay = layer.overlay_screen;
        }
      }
      RETURN_IF_ERROR(overworld.EnsureMapBuilt(screen));
      const auto* map = overworld.overworld_map(screen);
      if (map == nullptr) {
        return absl::InternalError("Overworld child map missing");
      }
      const auto& pixels = map->bitmap_data();
      const auto& palette = map->current_palette();
      if (pixels.size() < static_cast<size_t>(kScreen) * kScreen) {
        return absl::InternalError(
            absl::StrFormat("Map 0x%02X has no bitmap after build", screen));
      }
      result.screens.push_back(screen);
      for (int y = 0; y < kScreen; ++y) {
        for (int x = 0; x < kScreen; ++x) {
          const uint8_t idx = pixels[y * kScreen + x];
          uint8_t r = 0, g = 0, b = 0;
          palette_rgb(palette, idx, r, g, b);
          if (!overlay_pixels.empty()) {
            const uint8_t oidx = overlay_pixels[y * kScreen + x];
            if (oidx != 0) {
              uint8_t orr = 0, og = 0, ob = 0;
              palette_rgb(palette, oidx, orr, og, ob);
              if (overlay_is_background) {
                // The layer only covers backdrop pixels.
                r = orr;
                g = og;
                b = ob;
              } else {
                // Front overlays (fog, rain, canopy) blend at half strength.
                r = static_cast<uint8_t>((r + orr) / 2);
                g = static_cast<uint8_t>((g + og) / 2);
                b = static_cast<uint8_t>((b + ob) / 2);
              }
            }
          }
          const size_t base = (static_cast<size_t>(dy * kScreen + y) * width +
                               dx * kScreen + x) *
                              4;
          native[base + 0] = r;
          native[base + 1] = g;
          native[base + 2] = b;
          native[base + 3] = 255;
        }
      }
    }
  }

  const int out_w = static_cast<int>(width * req.scale);
  const int out_h = static_cast<int>(height * req.scale);
  std::vector<uint8_t> rgba(static_cast<size_t>(out_w) * out_h * 4, 255);
  for (int oy = 0; oy < out_h; ++oy) {
    const int sy = oy * height / out_h;
    for (int ox = 0; ox < out_w; ++ox) {
      const int sx = ox * width / out_w;
      std::copy_n(native.begin() + (static_cast<size_t>(sy) * width + sx) * 4,
                  4, rgba.begin() + (static_cast<size_t>(oy) * out_w + ox) * 4);
    }
  }

  HeadlessOverlayRenderer draw(rgba, out_w, out_h, req.scale);
  const uint32_t flags = req.overlay_flags;
  const int origin_x = (parent % 8) * kScreen;
  const int origin_y = ((parent % 64) / 8) * kScreen;
  auto owned = [&](int map_id) {
    const auto* map = overworld.overworld_map(map_id);
    return map != nullptr && map->parent() == parent;
  };
  auto in_area = [&](int lx, int ly) {
    return lx >= 0 && ly >= 0 && lx < width && ly < height;
  };

  if (flags & OverworldOverlay::kGrid) {
    for (int x = 0; x <= width; x += 16) {
      const bool edge = x % kScreen == 0;
      draw.DrawLine(static_cast<float>(x), 0, static_cast<float>(x),
                    static_cast<float>(height - 1), 255, 255, 255,
                    edge ? 180 : 40);
    }
    for (int y = 0; y <= height; y += 16) {
      const bool edge = y % kScreen == 0;
      draw.DrawLine(0, static_cast<float>(y), static_cast<float>(width - 1),
                    static_cast<float>(y), 255, 255, 255, edge ? 180 : 40);
    }
  }

  auto mark = [&](const std::string& kind, int id, int lx, int ly,
                  const std::string& label, uint8_t r, uint8_t g, uint8_t b,
                  int phase = -1, int list_index = -1) {
    draw.DrawFilledRect(static_cast<float>(lx), static_cast<float>(ly), 16, 16,
                        r, g, b, 90);
    draw.DrawRect(static_cast<float>(lx), static_cast<float>(ly), 16, 16, r, g,
                  b, 255);
    DrawLabel(draw, static_cast<float>(lx), static_cast<float>(ly + 17), label,
              r, g, b);
    result.markers.push_back({kind, id, phase, list_index, lx, ly});
  };

  if (flags & OverworldOverlay::kItems) {
    for (const auto& item : overworld.all_items()) {
      if (item.deleted || !owned(item.room_map_id_))
        continue;
      const int lx = item.x_ - origin_x;
      const int ly = item.y_ - origin_y;
      if (!in_area(lx, ly))
        continue;
      mark("item", item.id_, lx, ly, absl::StrFormat("I%02X", item.id_), 255,
           230, 60);
    }
  }
  if (flags & OverworldOverlay::kEntrances) {
    for (const auto& entrance : overworld.entrances()) {
      if (entrance.deleted || !owned(entrance.map_id_))
        continue;
      const int lx = entrance.x_ - origin_x;
      const int ly = entrance.y_ - origin_y;
      if (!in_area(lx, ly))
        continue;
      mark("entrance", entrance.entrance_id_, lx, ly,
           absl::StrFormat("E%02X", entrance.entrance_id_), 255, 255, 255);
    }
  }
  if (flags & OverworldOverlay::kHoles) {
    for (const auto& hole : overworld.holes()) {
      if (hole.deleted || !owned(hole.map_id_))
        continue;
      const int lx = hole.x_ - origin_x;
      const int ly = hole.y_ - origin_y;
      if (!in_area(lx, ly))
        continue;
      mark("hole", hole.entrance_id_, lx, ly,
           absl::StrFormat("H%02X", hole.entrance_id_), 255, 150, 40);
    }
  }
  if (flags & OverworldOverlay::kExits) {
    for (const auto& exit : *overworld.exits()) {
      if (exit.deleted_ || !owned(exit.map_id_))
        continue;
      const int lx = exit.x_ - origin_x;
      const int ly = exit.y_ - origin_y;
      if (!in_area(lx, ly))
        continue;
      mark("exit", exit.room_id_, lx, ly,
           absl::StrFormat("X%02X", exit.room_id_), 200, 120, 255);
    }
  }
  if (flags & OverworldOverlay::kSprites) {
    for (int phase = 0; phase < 3; ++phase) {
      if (req.phase >= 0 && req.phase != phase)
        continue;
      const auto color = SpritePhaseColor(phase);
      int list_index = 0;
      for (const auto& sprite : overworld.sprites(phase)) {
        if (sprite.map_id() != parent)
          continue;
        const int index = list_index++;
        if (sprite.deleted())
          continue;
        // Offset phases slightly so identical entries in two lists stay
        // visible when every phase is drawn.
        const int nudge = req.phase < 0 ? phase * 2 : 0;
        const int lx = sprite.x() - origin_x + nudge;
        const int ly = sprite.y() - origin_y + nudge;
        if (!in_area(lx, ly))
          continue;
        const std::string label =
            req.phase < 0 ? absl::StrFormat("%d:%02X", phase, sprite.id())
                          : absl::StrFormat("%02X", sprite.id());
        mark("sprite", sprite.id(), lx, ly, label, color.r, color.g, color.b,
             phase, index);
      }
    }
  }

  ASSIGN_OR_RETURN(result.png_data, EncodePng(rgba, out_w, out_h));
  result.width = out_w;
  result.height = out_h;
  return result;
}

absl::StatusOr<RoomMetadata> RenderService::GetDungeonRoomMetadata(
    int room_id) {
  if (room_id < 0 || room_id >= zelda3::kNumberOfRooms) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid room_id 0x%02X", room_id));
  }

  std::lock_guard<std::mutex> lock(mu_);

  zelda3::Room room = zelda3::LoadRoomFromRom(rom_, room_id);
  room.SetGameData(game_data_);
  room.LoadSprites();

  RoomMetadata meta;
  meta.room_id = room_id;
  meta.blockset = room.blockset();
  meta.spriteset = room.spriteset();
  meta.palette = room.palette();
  meta.layout_id = room.layout_id();
  meta.effect = static_cast<int>(room.effect());
  meta.collision = static_cast<int>(room.collision());
  meta.tag1 = static_cast<int>(room.tag1());
  meta.tag2 = static_cast<int>(room.tag2());
  meta.message_id = room.message_id();
  meta.has_custom_collision = room.has_custom_collision();
  meta.object_count = static_cast<int>(room.GetTileObjects().size());
  meta.sprite_count = static_cast<int>(room.GetSprites().size());
  return meta;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

absl::StatusOr<std::vector<uint8_t>> RenderService::BitmapToRgba(
    const gfx::Bitmap& bitmap, int out_w, int out_h) {
  SDL_Surface* surface = bitmap.surface();
  if (!surface) {
    return absl::InternalError("Bitmap has no SDL surface");
  }

  const int src_w = bitmap.width();
  const int src_h = bitmap.height();

  // Get the palette from the surface.
  SDL_Palette* pal = platform::GetSurfacePalette(surface);

  const uint8_t* indexed = bitmap.data();
  if (!indexed) {
    return absl::InternalError("Bitmap has no pixel data");
  }

  // Allocate RGBA output at target size.
  std::vector<uint8_t> rgba(static_cast<size_t>(out_w) * out_h * 4, 0xFF);

  // Nearest-neighbour scale: for each output pixel, sample the source.
  for (int oy = 0; oy < out_h; ++oy) {
    const int sy = oy * src_h / out_h;
    for (int ox = 0; ox < out_w; ++ox) {
      const int sx = ox * src_w / out_w;
      if (sx >= src_w || sy >= src_h)
        continue;
      const uint8_t idx = indexed[sy * src_w + sx];

      uint8_t r = 0, g = 0, b = 0;
      if (pal && static_cast<int>(idx) < pal->ncolors) {
        r = pal->colors[idx].r;
        g = pal->colors[idx].g;
        b = pal->colors[idx].b;
      }

      const size_t base = static_cast<size_t>((oy * out_w + ox) * 4);
      rgba[base + 0] = r;
      rgba[base + 1] = g;
      rgba[base + 2] = b;
      rgba[base + 3] = 255;
    }
  }

  return rgba;
}

void RenderService::ApplyOverlays(std::vector<uint8_t>& rgba, int width,
                                  int height, const zelda3::Room& room,
                                  uint32_t flags, float scale) {
  HeadlessOverlayRenderer draw(rgba, width, height, scale);

  // Tile grid — draw first so other overlays paint on top.
  if (flags & RenderOverlay::kGrid) {
    for (int tx = 0; tx < 64; ++tx) {
      draw.DrawLine(static_cast<float>(tx * 8), 0, static_cast<float>(tx * 8),
                    511, 60, 60, 60, 80);
    }
    for (int ty = 0; ty < 64; ++ty) {
      draw.DrawLine(0, static_cast<float>(ty * 8), 511,
                    static_cast<float>(ty * 8), 60, 60, 60, 80);
    }
  }

  // Custom collision overlay — one colored square per non-zero tile.
  if ((flags & RenderOverlay::kCollision) || (flags & RenderOverlay::kTrack)) {
    const auto& cc = room.custom_collision();
    if (cc.has_data) {
      for (int ty = 0; ty < 64; ++ty) {
        for (int tx = 0; tx < 64; ++tx) {
          const uint8_t val = cc.tiles[ty * 64 + tx];
          if (val == 0)
            continue;

          // kCollision shows all non-zero tiles; kTrack shows only track tiles.
          const bool is_track =
              (val >= 0xB0 && val <= 0xBE) || (val >= 0xD0 && val <= 0xD3);
          if (!(flags & RenderOverlay::kCollision) && !is_track)
            continue;

          const auto c = CollisionColor(val);
          if (c.a == 0)
            continue;
          draw.DrawFilledRect(static_cast<float>(tx * 8),
                              static_cast<float>(ty * 8), 8.0f, 8.0f, c.r, c.g,
                              c.b, c.a);
        }
      }
    }
  }

  // Objects — draw outline of each tile object's bounding rect.
  if (flags & RenderOverlay::kObjects) {
    for (const auto& obj : room.GetTileObjects()) {
      const float px = static_cast<float>(obj.x() * 8);
      const float py = static_cast<float>(obj.y() * 8);
      const float pw =
          static_cast<float>(std::max(1, static_cast<int>(obj.width_)) * 8);
      const float ph =
          static_cast<float>(std::max(1, static_cast<int>(obj.height_)) * 8);
      draw.DrawRect(px, py, pw, ph, 255, 200, 0, 200);
    }
  }

  // Sprites — filled square at sprite tile position.
  if (flags & RenderOverlay::kSprites) {
    for (const auto& spr : room.GetSprites()) {
      // Sprite nx/ny are in 16px units (2 tiles per unit).
      const float px = static_cast<float>(spr.nx() * 16);
      const float py = static_cast<float>(spr.ny() * 16);
      draw.DrawFilledRect(px, py, 16.0f, 16.0f, 255, 60, 60, 120);
      draw.DrawRect(px, py, 16.0f, 16.0f, 255, 60, 60, 230);
    }
  }

  // Camera quadrant boundaries — two lines bisecting the room.
  if (flags & RenderOverlay::kCameraQuads) {
    draw.DrawLine(256, 0, 256, 511, 200, 200, 255, 120);
    draw.DrawLine(0, 256, 511, 256, 200, 200, 255, 120);
  }
}

#ifdef YAZE_CLI_HAS_PNG
absl::StatusOr<std::vector<uint8_t>> RenderService::EncodePng(
    const std::vector<uint8_t>& rgba, int width, int height) {
  png_structp png =
      png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if (!png)
    return absl::InternalError("png_create_write_struct failed");

  png_infop info = png_create_info_struct(png);
  if (!info) {
    png_destroy_write_struct(&png, nullptr);
    return absl::InternalError("png_create_info_struct failed");
  }

  std::vector<uint8_t> output;
  PngCtx ctx{&output};

  if (setjmp(png_jmpbuf(png))) {
    png_destroy_write_struct(&png, &info);
    return absl::InternalError("PNG encoding error");
  }

  png_set_write_fn(png, &ctx, PngWrite, PngFlush);
  png_set_IHDR(png, info, static_cast<uint32_t>(width),
               static_cast<uint32_t>(height), 8, PNG_COLOR_TYPE_RGBA,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);
  png_write_info(png, info);

  std::vector<png_bytep> rows(static_cast<size_t>(height));
  for (int row = 0; row < height; ++row) {
    rows[static_cast<size_t>(row)] = const_cast<png_bytep>(
        rgba.data() + static_cast<size_t>(row) * width * 4);
  }
  png_write_image(png, rows.data());
  png_write_end(png, nullptr);
  png_destroy_write_struct(&png, &info);

  return output;
}
#else
absl::StatusOr<std::vector<uint8_t>> RenderService::EncodePng(
    const std::vector<uint8_t>& /*rgba*/, int /*width*/, int /*height*/) {
  return absl::UnimplementedError("PNG encoding unavailable (libpng missing)");
}
#endif

}  // namespace service
}  // namespace app
}  // namespace yaze
