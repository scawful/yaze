#include "app/service/render_service.h"

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/gfx/core/bitmap.h"
#include "app/platform/sdl_compat.h"
#include "app/service/headless_overlay_renderer.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_layer_manager.h"

#include <SDL.h>
#ifdef YAZE_CLI_HAS_PNG
#include <png.h>
#endif

namespace yaze {
namespace app {
namespace service {

namespace {

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

}  // namespace

RenderService::RenderService(Rom* rom, zelda3::GameData* game_data)
    : rom_(rom), game_data_(game_data) {}

absl::StatusOr<RenderResult> RenderService::RenderDungeonRoom(
    const RenderRequest& req) {
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

  std::lock_guard<std::mutex> lock(mu_);

  // Load room data from ROM (header, objects, pots, torches, blocks, pits).
  zelda3::Room room = zelda3::LoadRoomFromRom(rom_, req.room_id);
  room.SetGameData(game_data_);

  // Load sprites (requires ROM, game_data not needed here).
  room.LoadSprites();

  // Load graphics sheets and render tiles to BackgroundBuffers (CPU only).
  room.LoadRoomGraphics(room.blockset());
  room.RenderRoomGraphics();

  // Composite all layers to a single Bitmap (CPU, SDL surface with palette).
  zelda3::RoomLayerManager layer_mgr;
  auto& composite = room.GetCompositeBitmap(layer_mgr);

  if (!composite.is_active() || composite.width() <= 0) {
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
#ifdef YAZE_CLI_HAS_PNG
  auto png_or = EncodePng(rgba, out_w, out_h);
  if (!png_or.ok())
    return png_or.status();

  RenderResult result;
  result.png_data = std::move(png_or).value();
#else
  RenderResult result;
  result.png_data = std::move(rgba);
#endif
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
