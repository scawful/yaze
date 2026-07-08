#include "app/editor/overworld/map_texture_coordinator.h"

#include <algorithm>
#include <new>

#include "app/editor/overworld/ui_constants.h"
#include "app/gfx/backend/irenderer.h"
#include "app/gfx/resource/arena.h"
#include "util/log.h"
#include "zelda3/overworld/overworld_map.h"

namespace yaze::editor {

namespace {

bool HasTextureContext(const MapTextureContext& ctx) {
  return ctx.overworld && ctx.maps_bmp;
}

}  // namespace

void OverworldMapTextureCoordinator::ResetMapBitmaps() {
  if (!ctx_.maps_bmp) {
    return;
  }
  for (auto& bitmap : *ctx_.maps_bmp) {
    bitmap = gfx::Bitmap();
  }
}

void OverworldMapTextureCoordinator::ProcessDeferredTextures() {
  if (ctx_.renderer) {
    gfx::Arena::Get().ProcessTextureQueue(ctx_.renderer);
  }
  if (!HasTextureContext(ctx_) || !ctx_.current_map || !ctx_.current_world ||
      !ctx_.refresh_map_on_demand) {
    return;
  }

  int refresh_count = 0;
  constexpr int kMaxRefreshesPerFrame = 2;

  const auto try_refresh_map = [&](int map_index) {
    if (map_index < 0 || map_index >= zelda3::kNumOverworldMaps ||
        refresh_count >= kMaxRefreshesPerFrame) {
      return;
    }
    auto& bitmap = (*ctx_.maps_bmp)[map_index];
    if (bitmap.modified() && bitmap.is_active()) {
      ctx_.refresh_map_on_demand(map_index);
      ++refresh_count;
    }
  };

  try_refresh_map(*ctx_.current_map);

  const int current_world = std::clamp(*ctx_.current_world, 0, 2);
  const int world_start = current_world * 0x40;
  const int world_end = std::min(world_start + 0x40, zelda3::kNumOverworldMaps);
  for (int map_index = world_start;
       map_index < world_end && refresh_count < kMaxRefreshesPerFrame;
       ++map_index) {
    if (map_index == *ctx_.current_map) {
      continue;
    }
    try_refresh_map(map_index);
  }
}

void OverworldMapTextureCoordinator::EnsureMapTexture(int map_index) {
  if (!HasTextureContext(ctx_) || map_index < 0 ||
      map_index >= zelda3::kNumOverworldMaps) {
    return;
  }

  const auto* existing_map = ctx_.overworld->overworld_map(map_index);
  const bool was_built = existing_map && existing_map->is_built();

  auto status = ctx_.overworld->EnsureMapBuilt(map_index);
  if (!status.ok()) {
    LOG_ERROR("OverworldMapTextureCoordinator", "Failed to build map %d: %s",
              map_index, status.message());
    return;
  }

  auto& bitmap = (*ctx_.maps_bmp)[map_index];
  const auto* map = ctx_.overworld->overworld_map(map_index);
  if (!map) {
    return;
  }
  const bool needs_bitmap_sync = !was_built || bitmap.modified();

  if (!bitmap.is_active()) {
    const auto& palette = map->current_palette();
    try {
      bitmap.Create(kOverworldMapSize, kOverworldMapSize, 0x80,
                    map->bitmap_data());
      bitmap.SetPalette(palette);
    } catch (const std::bad_alloc& e) {
      LOG_ERROR("OverworldMapTextureCoordinator",
                "Error allocating bitmap for map %d: %s", map_index, e.what());
      return;
    }
  } else if (needs_bitmap_sync) {
    bitmap.set_data(map->bitmap_data());
    bitmap.SetPalette(map->current_palette());
    bitmap.set_modified(false);
  }

  if (!bitmap.texture() && bitmap.is_active()) {
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::CREATE, &bitmap);
  } else if (needs_bitmap_sync && bitmap.texture() && bitmap.is_active()) {
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::UPDATE, &bitmap);
  }
}

void OverworldMapTextureCoordinator::PrimeWorldMaps(
    int world, bool process_texture_queue) {
  if (!HasTextureContext(ctx_)) {
    return;
  }

  const int clamped_world = std::clamp(world, 0, 2);
  const int world_start = clamped_world * 0x40;
  const int world_end = std::min(world_start + 0x40, zelda3::kNumOverworldMaps);
  if (world_start >= zelda3::kNumOverworldMaps) {
    return;
  }

  const int current_world = ctx_.current_world ? *ctx_.current_world : 0;
  const int current_map = ctx_.current_map ? *ctx_.current_map : world_start;
  const int current_local_map =
      (current_world == clamped_world) ? (current_map & 0x3F) : 0;
  const int first_map = world_start + std::clamp(current_local_map, 0,
                                                 world_end - world_start - 1);
  constexpr int kMaxMapsToPrime = 8;
  const int prime_end = std::min(first_map + kMaxMapsToPrime, world_end);
  for (int map_index = first_map; map_index < prime_end; ++map_index) {
    EnsureMapTexture(map_index);
  }

  if (process_texture_queue) {
    ProcessDeferredTextures();
  }
}

}  // namespace yaze::editor
