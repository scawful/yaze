#include "object_drawer.h"

#include <cstdio>
#include <filesystem>

#include "absl/strings/str_format.h"
#include "app/gfx/types/snes_tile.h"
#include "core/features.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "util/log.h"
#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/dungeon/draw_routines/draw_routine_types.h"
#include "zelda3/dungeon/draw_routines/special_routines.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"

namespace yaze {
namespace zelda3 {

ObjectDrawer::ObjectDrawer(Rom* rom, int room_id,
                           const uint8_t* room_gfx_buffer)
    : rom_(rom), room_id_(room_id), room_gfx_buffer_(room_gfx_buffer) {
  InitializeDrawRoutines();
}

void ObjectDrawer::SetTraceCollector(std::vector<TileTrace>* collector,
                                     bool trace_only) {
  trace_collector_ = collector;
  trace_only_ = trace_only;
}

void ObjectDrawer::ClearTraceCollector() {
  trace_collector_ = nullptr;
  trace_only_ = false;
}

void ObjectDrawer::SetTraceContext(const RoomObject& object,
                                   RoomObject::LayerType layer) {
  trace_context_.object_id = static_cast<uint16_t>(object.id_);
  trace_context_.size = object.size_;
  trace_context_.layer = static_cast<uint8_t>(layer);
}

void ObjectDrawer::PushTrace(int tile_x, int tile_y,
                             const gfx::TileInfo& tile_info) {
  if (!trace_collector_) {
    return;
  }
  uint8_t flags = 0;
  if (tile_info.horizontal_mirror_)
    flags |= 0x1;
  if (tile_info.vertical_mirror_)
    flags |= 0x2;
  if (tile_info.over_)
    flags |= 0x4;
  flags |= static_cast<uint8_t>((tile_info.palette_ & 0x7) << 3);

  TileTrace trace{};
  trace.object_id = trace_context_.object_id;
  trace.size = trace_context_.size;
  trace.layer = trace_context_.layer;
  trace.x_tile = static_cast<int16_t>(tile_x);
  trace.y_tile = static_cast<int16_t>(tile_y);
  trace.tile_id = tile_info.id_;
  trace.flags = flags;
  trace_collector_->push_back(trace);
}

void ObjectDrawer::TraceHookThunk(int tile_x, int tile_y,
                                  const gfx::TileInfo& tile_info,
                                  void* user_data) {
  auto* drawer = static_cast<ObjectDrawer*>(user_data);
  if (!drawer) {
    return;
  }
  drawer->PushTrace(tile_x, tile_y, tile_info);
}

void ObjectDrawer::DrawUsingRegistryRoutine(
    int routine_id, const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
  // Many DrawRoutineRegistry routines are implemented as pure functions that
  // call DrawRoutineUtils::WriteTile8(), which only writes to BackgroundBuffer's
  // tile buffer (not the bitmap). Runtime rendering/compositing uses the
  // bitmap-backed buffers, so we capture tile writes from the pure routine and
  // replay them via ObjectDrawer::WriteTile8().
  const auto* info = DrawRoutineRegistry::Get().GetRoutineInfo(routine_id);
  if (info == nullptr) {
    LOG_DEBUG("ObjectDrawer", "DrawUsingRegistryRoutine: unknown routine %d",
              routine_id);
    return;
  }

  struct CapturedWrite {
    int x = 0;
    int y = 0;
    gfx::TileInfo tile{};
  };

  std::vector<CapturedWrite> writes;
  writes.reserve(256);

  DrawRoutineUtils::SetTraceHook(
      [](int tile_x, int tile_y, const gfx::TileInfo& tile_info,
         void* user_data) {
        auto* out = static_cast<std::vector<CapturedWrite>*>(user_data);
        if (!out) {
          return;
        }
        out->push_back(CapturedWrite{tile_x, tile_y, tile_info});
      },
      &writes,
      /*trace_only=*/true);

  DrawContext ctx{
      .target_bg = bg,
      .object = obj,
      .tiles = tiles,
      .state = state,
      .rom = rom_,
      .room_id = room_id_,
      .room_gfx_buffer = room_gfx_buffer_,
      .secondary_bg = nullptr,
  };
  info->function(ctx);

  DrawRoutineUtils::ClearTraceHook();

  for (const auto& w : writes) {
    WriteTile8(bg, w.x, w.y, w.tile);
  }
}

absl::Status ObjectDrawer::DrawObject(
    const RoomObject& object, gfx::BackgroundBuffer& bg1,
    gfx::BackgroundBuffer& bg2, const gfx::PaletteGroup& palette_group,
    [[maybe_unused]] const DungeonState* state,
    gfx::BackgroundBuffer* layout_bg1) {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  if (!routines_initialized_) {
    return absl::FailedPreconditionError("Draw routines not initialized");
  }

  // Ensure object has tiles loaded
  auto mutable_obj = const_cast<RoomObject&>(object);
  mutable_obj.SetRom(rom_);
  mutable_obj.EnsureTilesLoaded();

  // Select buffer based on layer
  // Layer 0 (BG1): Main objects - drawn to BG1_Objects (on top of layout)
  // Layer 1 (BG2): Overlay objects - drawn to BG2_Objects (behind layout)
  // Layer 2 (BG3): Priority objects (torches) - drawn to BG1_Objects (on top)
  bool use_bg2 = (object.layer_ == RoomObject::LayerType::BG2);
  auto& target_bg = use_bg2 ? bg2 : bg1;

  // Log buffer selection for debugging layer routing
  LOG_DEBUG("ObjectDrawer", "Object 0x%03X layer=%d -> drawing to %s buffer",
            object.id_, static_cast<int>(object.layer_),
            use_bg2 ? "BG2 (behind layout)" : "BG1 (on top of layout)");

  // Check for custom object override first (guarded by feature flag).
  // We check this BEFORE routine lookup to allow overriding vanilla objects.
  int subtype = object.size_ & 0x1F;
  bool is_custom_object = false;
  const bool is_track_corner_alias = object.id_ >= 0x100 && object.id_ <= 0x103;
  const bool allow_custom_override =
      !is_track_corner_alias || this->allow_track_corner_aliases_;
  if (core::FeatureFlags::get().kEnableCustomObjects && allow_custom_override &&
      CustomObjectManager::Get().GetObjectInternal(object.id_, subtype).ok()) {
    is_custom_object = true;
    // Custom objects default to drawing on the target layer only, unless all_bgs_ is set
    // Mask propagation is difficult without dimensions, so we rely on explicit transparency in the custom object tiles if needed

    // Draw to target layer
    SetTraceContext(object, use_bg2 ? RoomObject::LayerType::BG2
                                    : RoomObject::LayerType::BG1);
    DrawCustomObject(object, target_bg, mutable_obj.tiles(), state);

    // If marked for both BGs, draw to the other layer too
    if (object.all_bgs_) {
      auto& other_bg = (object.layer_ == RoomObject::LayerType::BG2 ||
                        object.layer_ == RoomObject::LayerType::BG3)
                           ? bg1
                           : bg2;
      SetTraceContext(object, (&other_bg == &bg1) ? RoomObject::LayerType::BG1
                                                  : RoomObject::LayerType::BG2);
      DrawCustomObject(object, other_bg, mutable_obj.tiles(), state);
    }
    return absl::OkStatus();
  }

  // Skip objects that don't have tiles loaded
  if (!is_custom_object && mutable_obj.tiles().empty()) {
    LOG_DEBUG("ObjectDrawer",
              "Object 0x%03X at (%d,%d) has NO TILES - skipping", object.id_,
              object.x_, object.y_);
    return absl::OkStatus();
  }

  // Look up draw routine for this object
  int routine_id = GetDrawRoutineId(object.id_);

  // Log draw routine lookup with tile info
  LOG_DEBUG("ObjectDrawer",
            "Object 0x%03X at (%d,%d) size=%d -> routine=%d tiles=%zu",
            object.id_, object.x_, object.y_, object.size_, routine_id,
            mutable_obj.tiles().size());

  if (routine_id < 0 || routine_id >= static_cast<int>(draw_routines_.size())) {
    LOG_DEBUG("ObjectDrawer",
              "Object 0x%03X: NO ROUTINE (id=%d, max=%zu) - using fallback 1x1",
              object.id_, routine_id, draw_routines_.size());
    // Fallback to simple 1x1 drawing using first 8x8 tile
    if (!mutable_obj.tiles().empty()) {
      const auto& tile_info = mutable_obj.tiles()[0];
      SetTraceContext(object, use_bg2 ? RoomObject::LayerType::BG2
                                      : RoomObject::LayerType::BG1);
      WriteTile8(target_bg, object.x_, object.y_, tile_info);
    }
    return absl::OkStatus();
  }

  // Null-tile guard: skip routines whose tile payload is too small.
  // Hack ROMs with abbreviated tile tables would otherwise cause
  // out-of-bounds access in fixed-size draw patterns.
  const DrawRoutineInfo* routine_info =
      DrawRoutineRegistry::Get().GetRoutineInfo(routine_id);
  if (routine_info && routine_info->min_tiles > 0 &&
      static_cast<int>(mutable_obj.tiles().size()) < routine_info->min_tiles) {
    LOG_WARN("ObjectDrawer",
             "Object 0x%03X at (%d,%d): tile payload too small "
             "(%zu < %d required by routine '%s') - skipping",
             object.id_, object.x_, object.y_, mutable_obj.tiles().size(),
             routine_info->min_tiles, routine_info->name.c_str());
    // Fall through to 1x1 fallback if any tiles are present
    if (!mutable_obj.tiles().empty()) {
      const auto& tile_info = mutable_obj.tiles()[0];
      SetTraceContext(object, use_bg2 ? RoomObject::LayerType::BG2
                                      : RoomObject::LayerType::BG1);
      WriteTile8(target_bg, object.x_, object.y_, tile_info);
    }
    return absl::OkStatus();
  }

  bool trace_hook_active = false;
  if (trace_collector_) {
    DrawRoutineUtils::SetTraceHook(&ObjectDrawer::TraceHookThunk, this,
                                   trace_only_);
    trace_hook_active = true;
  }

  // Check if this should draw to both BG layers.
  // In the original engine, BothBG routines explicitly write to both tilemaps
  // regardless of which object list or pass they are executed from.
  bool is_both_bg = (object.all_bgs_ || RoutineDrawsToBothBGs(routine_id));

  if (is_both_bg) {
    // Draw to both background layers
    SetTraceContext(object, RoomObject::LayerType::BG1);
    draw_routines_[routine_id](this, object, bg1, mutable_obj.tiles(), state);
    SetTraceContext(object, RoomObject::LayerType::BG2);
    draw_routines_[routine_id](this, object, bg2, mutable_obj.tiles(), state);
  } else {
    // Execute the appropriate draw routine on target buffer only
    SetTraceContext(object, use_bg2 ? RoomObject::LayerType::BG2
                                    : RoomObject::LayerType::BG1);
    draw_routines_[routine_id](this, object, target_bg, mutable_obj.tiles(),
                               state);
  }

  if (trace_hook_active) {
    DrawRoutineUtils::ClearTraceHook();
  }

  // BG2 Mask Propagation: ONLY layer-2 mask/pit objects should mark BG1 as
  // transparent.
  //
  // Regular BG2 objects (walls, statues, ceilings, platforms) rely on priority
  // bits for correct Z-order and should NOT automatically clear BG1.
  //
  // FIX: Previously this marked ALL Layer 1 objects as creating BG1 transparency,
  // which caused walls/statues to incorrectly show "above" the layout.
  // Now we only call MarkBG1Transparent for known mask objects.
  bool is_pit_or_mask =
      (object.id_ == 0xA4) ||                        // Pit
      (object.id_ >= 0xA5 && object.id_ <= 0xAC) ||  // Diagonal layer 2 masks
      (object.id_ == 0xC0) ||                        // Large ceiling overlay
      (object.id_ == 0xC2) ||                        // Layer 2 pit mask (large)
      (object.id_ == 0xC3) ||   // Layer 2 pit mask (medium)
      (object.id_ == 0xC8) ||   // Water floor overlay
      (object.id_ == 0xC6) ||   // Layer 2 mask (large)
      (object.id_ == 0xD7) ||   // Layer 2 mask (medium)
      (object.id_ == 0xD8) ||   // Flood water overlay
      (object.id_ == 0xD9) ||   // Layer 2 swim mask
      (object.id_ == 0xDA) ||   // Flood water overlay B
      (object.id_ == 0xFE6) ||  // Type 3 pit
      (object.id_ == 0xFF3);    // Type 3 layer 2 mask (full)

  if (!trace_only_ && is_pit_or_mask &&
      object.layer_ == RoomObject::LayerType::BG2 && !is_both_bg) {
    auto [pixel_width, pixel_height] = CalculateObjectDimensions(object);

    // Log pit/mask transparency propagation
    LOG_DEBUG(
        "ObjectDrawer",
        "Pit mask 0x%03X at (%d,%d) -> marking %dx%d pixels transparent in BG1",
        object.id_, object.x_, object.y_, pixel_width, pixel_height);

    // Mark the object buffer BG1 as transparent
    MarkBG1Transparent(bg1, object.x_, object.y_, pixel_width, pixel_height);
    // Also mark the layout buffer (floor tiles) as transparent if provided
    if (layout_bg1 != nullptr) {
      MarkBG1Transparent(*layout_bg1, object.x_, object.y_, pixel_width,
                         pixel_height);
    }
  }

  return absl::OkStatus();
}

absl::Status ObjectDrawer::DrawObjectList(
    const std::vector<RoomObject>& objects, gfx::BackgroundBuffer& bg1,
    gfx::BackgroundBuffer& bg2, const gfx::PaletteGroup& palette_group,
    [[maybe_unused]] const DungeonState* state,
    gfx::BackgroundBuffer* layout_bg1, bool reset_chest_index) {
  if (reset_chest_index) {
    ResetChestIndex();
  }
  absl::Status status = absl::OkStatus();

  // DEBUG: Count objects routed to each buffer
  int to_bg1 = 0, to_bg2 = 0, both_bgs = 0;

  for (const auto& object : objects) {
    // Track buffer routing for summary
    bool use_bg2 = (object.layer_ == RoomObject::LayerType::BG2);
    int routine_id = GetDrawRoutineId(object.id_);
    bool is_both_bg = (object.all_bgs_ || RoutineDrawsToBothBGs(routine_id));

    if (is_both_bg) {
      both_bgs++;
    } else if (use_bg2) {
      to_bg2++;
    } else {
      to_bg1++;
    }

    auto s = DrawObject(object, bg1, bg2, palette_group, state, layout_bg1);
    if (!s.ok() && status.ok()) {
      status = s;
    }
  }

  LOG_DEBUG("ObjectDrawer", "Buffer routing: to_BG1=%d, to_BG2=%d, BothBGs=%d",
            to_bg1, to_bg2, both_bgs);

  // NOTE: Palette is already set in Room::RenderRoomGraphics() before calling
  // this function. We just need to sync the pixel data to the SDL surface.
  auto& bg1_bmp = bg1.bitmap();
  auto& bg2_bmp = bg2.bitmap();

  // Sync bitmap data to SDL surfaces (palette already applied)
  if (bg1_bmp.modified() && bg1_bmp.surface() &&
      bg1_bmp.mutable_data().size() > 0) {
    SDL_LockSurface(bg1_bmp.surface());
    // Safety check: ensure surface can hold the data
    // Note: This assumes 8bpp surface where pitch >= width
    size_t surface_size = bg1_bmp.surface()->h * bg1_bmp.surface()->pitch;
    size_t buffer_size = bg1_bmp.mutable_data().size();

    if (surface_size >= buffer_size) {
      // TODO: Handle pitch mismatch properly (copy row by row)
      // For now, just ensure we don't overflow
      memcpy(bg1_bmp.surface()->pixels, bg1_bmp.mutable_data().data(),
             buffer_size);
    } else {
      LOG_DEBUG("ObjectDrawer", "BG1 Surface too small: surf=%zu buf=%zu",
                surface_size, buffer_size);
    }
    SDL_UnlockSurface(bg1_bmp.surface());
  }

  if (bg2_bmp.modified() && bg2_bmp.surface() &&
      bg2_bmp.mutable_data().size() > 0) {
    SDL_LockSurface(bg2_bmp.surface());
    size_t surface_size = bg2_bmp.surface()->h * bg2_bmp.surface()->pitch;
    size_t buffer_size = bg2_bmp.mutable_data().size();

    if (surface_size >= buffer_size) {
      memcpy(bg2_bmp.surface()->pixels, bg2_bmp.mutable_data().data(),
             buffer_size);
    } else {
      LOG_DEBUG("ObjectDrawer", "BG2 Surface too small: surf=%zu buf=%zu",
                surface_size, buffer_size);
    }
    SDL_UnlockSurface(bg2_bmp.surface());
  }

  return status;
}

// ============================================================================
// Metadata-based BothBG Detection
// ============================================================================

bool ObjectDrawer::RoutineDrawsToBothBGs(int routine_id) {
  // Use DrawRoutineRegistry as the single source of truth for BothBG metadata.
  return DrawRoutineRegistry::Get().RoutineDrawsToBothBGs(routine_id);
}

// ============================================================================
// Draw Routine Registry Initialization
// ============================================================================

void ObjectDrawer::InitializeDrawRoutines() {
  // This function maps object IDs to their corresponding draw routines.
  // The mapping is based on ZScream's DungeonObjectData.cs and the game's
  // assembly code. The order of functions in draw_routines_ MUST match the
  // indices used here.
  //
  // ASM Reference (Bank 01):
  // Subtype 1 Data Offset: $018000 (DrawObjects.type1_subtype_1_data_offset)
  // Subtype 1 Routine Ptr: $018200 (DrawObjects.type1_subtype_1_routine)
  // Subtype 2 Data Offset: $0183F0 (DrawObjects.type1_subtype_2_data_offset)
  // Subtype 2 Routine Ptr: $018470 (DrawObjects.type1_subtype_2_routine)
  // Subtype 3 Data Offset: $0184F0 (DrawObjects.type1_subtype_3_data_offset)
  // Subtype 3 Routine Ptr: $0185F0 (DrawObjects.type1_subtype_3_routine)

  draw_routines_.clear();

  // Object-to-routine mapping now lives in DrawRoutineRegistry::BuildObjectMapping().
  // ObjectDrawer::GetDrawRoutineId() delegates to the registry singleton.
  // Initialize draw routine function array in the correct order
  // Routines 0-82 (existing), 80-98 (new special routines for stairs, locks, etc.)
  draw_routines_.reserve(100);

  // Routine 0
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(0, obj, bg, tiles, state);
      });
  // Routine 1
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(1, obj, bg, tiles, state);
      });
  // Routine 2 - 2x4 tiles with adjacent spacing (s * 2), count = size + 1
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(2, obj, bg, tiles, state);
      });
  // Routine 3 - Same as routine 2 but draws to both BG1 and BG2
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(3, obj, bg, tiles, state);
      });
  // Routine 4
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(4, obj, bg, tiles, state);
      });
  // Routine 5
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(5, obj, bg, tiles, state);
      });
  // Routine 6
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(6, obj, bg, tiles, state);
      });
  // Routine 7
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(7, obj, bg, tiles, state);
      });
  // Routine 8
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(8, obj, bg, tiles, state);
      });
  // Routine 9
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(9, obj, bg, tiles, state);
      });
  // Routine 10
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(10, obj, bg, tiles, state);
      });
  // Routine 11
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(11, obj, bg, tiles, state);
      });
  // Routine 12
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(12, obj, bg, tiles, state);
      });
  // Routine 13
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(13, obj, bg, tiles, state);
      });
  // Routine 14
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(14, obj, bg, tiles, state);
      });
  // Routine 15
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(15, obj, bg, tiles, state);
      });
  // Routine 16
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(16, obj, bg, tiles, state);
      });
  // Routine 17 - Diagonal Acute BothBG
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(17, obj, bg, tiles, state);
      });
  // Routine 18 - Diagonal Grave BothBG
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(18, obj, bg, tiles, state);
      });
  // Routine 19 - 4x4 Corner (Type 2 corners)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(19, obj, bg, tiles, state);
      });

  // Routine 20 - Edge objects 1x2 +2
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(20, obj, bg, tiles, state);
      });
  // Routine 21 - Edge with perimeter 1x1 +3
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(21, obj, bg, tiles, state);
      });
  // Routine 22 - Edge variant 1x1 +2
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(22, obj, bg, tiles, state);
      });
  // Routine 23 - Top corners 1x2 +13
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(23, obj, bg, tiles, state);
      });
  // Routine 24 - Bottom corners 1x2 +13
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(24, obj, bg, tiles, state);
      });
  // Routine 25 - Solid fill 1x1 +3 (floor patterns)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(25, obj, bg, tiles, state);
      });
  // Routine 26 - Door switcherer
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(26, obj, bg, tiles, state);
      });
  // Routine 27 - Decorations 4x4 spaced 2
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(27, obj, bg, tiles, state);
      });
  // Routine 28 - Statues 2x3 spaced 2
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(28, obj, bg, tiles, state);
      });
  // Routine 29 - Pillars 2x4 spaced 4
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(29, obj, bg, tiles, state);
      });
  // Routine 30 - Decorations 4x3 spaced 4
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(30, obj, bg, tiles, state);
      });
  // Routine 31 - Doubled 2x2 spaced 2
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(31, obj, bg, tiles, state);
      });
  // Routine 32 - Decorations 2x2 spaced 12
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(32, obj, bg, tiles, state);
      });
  // Routine 33 - Somaria Line
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(33, obj, bg, tiles, state);
      });
  // Routine 34 - Water Face
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(34, obj, bg, tiles, state);
      });
  // Routine 35 - 4x4 Corner BothBG
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(35, obj, bg, tiles, state);
      });
  // Routine 36 - Weird Corner Bottom BothBG
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(36, obj, bg, tiles, state);
      });
  // Routine 37 - Weird Corner Top BothBG
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(37, obj, bg, tiles, state);
      });
  // Routine 38 - Nothing
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(38, obj, bg, tiles, state);
      });
  // Routine 39 - Chest rendering (small + big)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawChest(obj, bg, tiles, state);
      });
  // Routine 40 - Rightwards 4x2 (Floor Tile)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(40, obj, bg, tiles, state);
      });
  // Routine 41 - Rightwards Decor 4x2 spaced 8 (12-column spacing)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(41, obj, bg, tiles, state);
      });
  // Routine 42 - Rightwards Cannon Hole 4x3
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(42, obj, bg, tiles, state);
      });
  // Routine 43 - Downwards Floor 4x4 (object 0x70)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(43, obj, bg, tiles, state);
      });
  // Routine 44 - Downwards 1x1 Solid +3 (object 0x71)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(44, obj, bg, tiles, state);
      });
  // Routine 45 - Downwards Decor 4x4 spaced 2 (objects 0x73-0x74)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(45, obj, bg, tiles, state);
      });
  // Routine 46 - Downwards Pillar 2x4 spaced 2 (objects 0x75, 0x87)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(46, obj, bg, tiles, state);
      });
  // Routine 47 - Downwards Decor 3x4 spaced 4 (objects 0x76-0x77)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(47, obj, bg, tiles, state);
      });
  // Routine 48 - Downwards Decor 2x2 spaced 12 (objects 0x78, 0x7B)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(48, obj, bg, tiles, state);
      });
  // Routine 49 - Downwards Line 1x1 +1 (object 0x7C)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(49, obj, bg, tiles, state);
      });
  // Routine 50 - Downwards Decor 2x4 spaced 8 (objects 0x7F, 0x80)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(50, obj, bg, tiles, state);
      });
  // Routine 51 - Rightwards Line 1x1 +1 (object 0x50)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(51, obj, bg, tiles, state);
      });
  // Routine 52 - Rightwards Bar 4x3 (object 0x4C)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(52, obj, bg, tiles, state);
      });
  // Routine 53 - Rightwards Shelf 4x4 (objects 0x4D-0x4F)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(53, obj, bg, tiles, state);
      });
  // Routine 54 - Rightwards Big Rail 1x3 +5 (object 0x5D)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(54, obj, bg, tiles, state);
      });
  // Routine 55 - Rightwards Block 2x2 spaced 2 (object 0x5E)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(55, obj, bg, tiles, state);
      });

  // ============================================================================
  // Phase 4: SuperSquare Routines (routines 56-64)
  // ============================================================================

  // Routine 56 - 4x4 Blocks in 4x4 SuperSquare (objects 0xC0, 0xC2)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(56, obj, bg, tiles, state);
      });

  // Routine 57 - 3x3 Floor in 4x4 SuperSquare (objects 0xC3, 0xD7)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(57, obj, bg, tiles, state);
      });

  // Routine 58 - 4x4 Floor in 4x4 SuperSquare (objects 0xC5-0xCA, 0xD1-0xD2,
  // 0xD9, 0xDF-0xE8)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(58, obj, bg, tiles, state);
      });

  // Routine 59 - 4x4 Floor One in 4x4 SuperSquare (object 0xC4)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(59, obj, bg, tiles, state);
      });

  // Routine 60 - 4x4 Floor Two in 4x4 SuperSquare (object 0xDB)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(60, obj, bg, tiles, state);
      });

  // Routine 61 - Big Hole 4x4 (object 0xA4)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(61, obj, bg, tiles, state);
      });

  // Routine 62 - Spike 2x2 in 4x4 SuperSquare (object 0xDE)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(62, obj, bg, tiles, state);
      });

  // Routine 63 - Table Rock 4x4 (object 0xDD)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(63, obj, bg, tiles, state);
      });

  // Routine 64 - Water Overlay 8x8 (objects 0xD8, 0xDA)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(64, obj, bg, tiles, state);
      });

  // ============================================================================
  // Phase 4 Step 2: Simple Variant Routines (routines 65-74)
  // ============================================================================

  // Routine 65 - Downwards Decor 3x4 spaced 2 (objects 0x81-0x84)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(65, obj, bg, tiles, state);
      });

  // Routine 66 - Downwards Big Rail 3x1 plus 5 (object 0x88)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(66, obj, bg, tiles, state);
      });

  // Routine 67 - Downwards Block 2x2 spaced 2 (object 0x89)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(67, obj, bg, tiles, state);
      });

  // Routine 68 - Downwards Cannon Hole 3x6 (objects 0x85-0x86)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(68, obj, bg, tiles, state);
      });

  // Routine 69 - Downwards Bar 2x3 (object 0x8F)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(69, obj, bg, tiles, state);
      });

  // Routine 70 - Downwards Pots 2x2 (object 0x95)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(70, obj, bg, tiles, state);
      });

  // Routine 71 - Downwards Hammer Pegs 2x2 (object 0x96)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(71, obj, bg, tiles, state);
      });

  // Routine 72 - Rightwards Edge 1x1 plus 7 (objects 0xB0-0xB1)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(72, obj, bg, tiles, state);
      });

  // Routine 73 - Rightwards Pots 2x2 (object 0xBC)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(73, obj, bg, tiles, state);
      });

  // Routine 74 - Rightwards Hammer Pegs 2x2 (object 0xBD)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(74, obj, bg, tiles, state);
      });

  // ============================================================================
  // Phase 4 Step 3: Diagonal Ceiling Routines (routines 75-78)
  // ============================================================================

  // Routine 75 - Diagonal Ceiling Top Left (objects 0xA0, 0xA5, 0xA9)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(75, obj, bg, tiles, state);
      });

  // Routine 76 - Diagonal Ceiling Bottom Left (objects 0xA1, 0xA6, 0xAA)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(76, obj, bg, tiles, state);
      });

  // Routine 77 - Diagonal Ceiling Top Right (objects 0xA2, 0xA7, 0xAB)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(77, obj, bg, tiles, state);
      });

  // Routine 78 - Diagonal Ceiling Bottom Right (objects 0xA3, 0xA8, 0xAC)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(78, obj, bg, tiles, state);
      });

  // ============================================================================
  // Phase 4 Step 5: Special Routines (routines 79-82)
  // ============================================================================

  // Routine 79 - Closed Chest Platform (object 0xC1, 68 tiles)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(79, obj, bg, tiles, state);
      });

  // Routine 80 - Moving Wall West (object 0xCD, 28 tiles)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(80, obj, bg, tiles, state);
      });

  // Routine 81 - Moving Wall East (object 0xCE, 28 tiles)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(81, obj, bg, tiles, state);
      });

  // Routine 82 - Open Chest Platform (object 0xDC, 21 tiles)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(82, obj, bg, tiles, state);
      });

  // ============================================================================
  // New Special Routines (Phase 5) - Stairs, Locks, Interactive Objects
  // ============================================================================

  // Routine 83 - InterRoom Fat Stairs Up (object 0x12D)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(83, obj, bg, tiles, state);
      });

  // Routine 84 - InterRoom Fat Stairs Down A (object 0x12E)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(84, obj, bg, tiles, state);
      });

  // Routine 85 - InterRoom Fat Stairs Down B (object 0x12F)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(85, obj, bg, tiles, state);
      });

  // Routine 86 - Auto Stairs (objects 0x130-0x133)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(86, obj, bg, tiles, state);
      });

  // Routine 87 - Straight InterRoom Stairs (Type 3 objects 0x21E-0x229)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(87, obj, bg, tiles, state);
      });

  // Routine 88 - Spiral Stairs Going Up Upper (object 0x138)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(88, obj, bg, tiles, state);
      });

  // Routine 89 - Spiral Stairs Going Down Upper (object 0x139)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(89, obj, bg, tiles, state);
      });

  // Routine 90 - Spiral Stairs Going Up Lower (object 0x13A)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(90, obj, bg, tiles, state);
      });

  // Routine 91 - Spiral Stairs Going Down Lower (object 0x13B)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(91, obj, bg, tiles, state);
      });

  // Routine 92 - Big Key Lock (Type 3 object 0x218)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(92, obj, bg, tiles, state);
      });

  // Routine 93 - Bombable Floor (Type 3 object 0x247)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(93, obj, bg, tiles, state);
      });

  // Routine 94 - Empty Water Face (Type 3 object 0x200)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(94, obj, bg, tiles, state);
      });

  // Routine 95 - Spitting Water Face (Type 3 object 0x201)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(95, obj, bg, tiles, state);
      });

  // Routine 96 - Drenching Water Face (Type 3 object 0x202)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(96, obj, bg, tiles, state);
      });

  // Routine 97 - Prison Cell (Type 3 objects 0x20D, 0x217)
  // This routine draws to both BG1 and BG2 with horizontal flip symmetry
  // Note: secondary_bg is set in DrawObject() for dual-BG objects
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(97, obj, bg, tiles, state);
      });

  // Routine 98 - Bed 4x5 (Type 2 objects 0x122, 0x128)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(DrawRoutineIds::kBed4x5, obj, bg, tiles,
                                       state);
      });

  // Routine 99 - Rightwards 3x6 (Type 2 object 0x12C, Type 3 0x236-0x237)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(DrawRoutineIds::kRightwards3x6, obj, bg,
                                       tiles, state);
      });

  // Routine 100 - Utility 6x3 (Type 2 object 0x13E, Type 3 0x24D, 0x25D)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(DrawRoutineIds::kUtility6x3, obj, bg,
                                       tiles, state);
      });

  // Routine 101 - Utility 3x5 (Type 3 objects 0x255, 0x25B)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(DrawRoutineIds::kUtility3x5, obj, bg,
                                       tiles, state);
      });

  // Routine 102 - Vertical Turtle Rock Pipe (Type 3 objects 0x23A, 0x23B)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(DrawRoutineIds::kVerticalTurtleRockPipe,
                                       obj, bg, tiles, state);
      });

  // Routine 103 - Horizontal Turtle Rock Pipe (Type 3 objects 0x23C, 0x23D,
  // 0x25C)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(
            DrawRoutineIds::kHorizontalTurtleRockPipe, obj, bg, tiles, state);
      });

  // Routine 104 - Light Beam on Floor (Type 3 object 0x270)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(DrawRoutineIds::kLightBeam, obj, bg,
                                       tiles, state);
      });

  // Routine 105 - Big Light Beam on Floor (Type 3 object 0x271)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(DrawRoutineIds::kBigLightBeam, obj, bg,
                                       tiles, state);
      });

  // Routine 106 - Boss Shell 4x4 (Type 3 objects 0x272, 0x27B, 0xF95)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(DrawRoutineIds::kBossShell4x4, obj, bg,
                                       tiles, state);
      });

  // Routine 107 - Solid Wall Decor 3x4 (Type 3 objects 0x269-0x26A, 0x26E-0x26F)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(DrawRoutineIds::kSolidWallDecor3x4, obj,
                                       bg, tiles, state);
      });

  // Routine 108 - Archery Game Target Door (Type 3 objects 0x260-0x261)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(DrawRoutineIds::kArcheryGameTargetDoor,
                                       obj, bg, tiles, state);
      });

  // Routine 109 - Ganon Triforce Floor Decor (Type 3 object 0x278)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(DrawRoutineIds::kGanonTriforceFloorDecor,
                                       obj, bg, tiles, state);
      });

  // Routine 110 - Single 2x2 (pots, statues, single-instance 2x2 objects)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(DrawRoutineIds::kSingle2x2, obj, bg,
                                       tiles, state);
      });

  // Routine 111 - Waterfall47 (object 0x47)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(DrawRoutineIds::kWaterfall47, obj, bg,
                                       tiles, state);
      });

  // Routine 112 - Waterfall48 (object 0x48)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(DrawRoutineIds::kWaterfall48, obj, bg,
                                       tiles, state);
      });

  // Routine 113 - Single 4x4 (NO repetition)
  // ASM: RoomDraw_4x4 - draws a single 4x4 pattern (16 tiles)
  // Used for: 0xFEB (large decor), and other single 4x4 objects
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(DrawRoutineIds::kSingle4x4, obj, bg,
                                       tiles, state);
      });

  // Routine 114 - Single 4x3 (NO repetition)
  // ASM: RoomDraw_TableRock4x3 - draws a single 4x3 pattern (12 tiles)
  // Used for: 0xFED (water grate), 0xFB1 (big chest), etc.
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(DrawRoutineIds::kSingle4x3, obj, bg,
                                       tiles, state);
      });

  // Routine 115 - RupeeFloor (special pattern for 0xF92)
  // ASM: RoomDraw_RupeeFloor - draws 3 columns of 2-tile pairs at 3 Y positions
  // Pattern: 6 tiles wide, 8 rows tall with gaps (rows 2 and 5 are empty)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(DrawRoutineIds::kRupeeFloor, obj, bg,
                                       tiles, state);
      });

  // Routine 116 - Actual 4x4 tile8 pattern (32x32 pixels, NO repetition)
  // ASM: RoomDraw_4x4 - draws exactly 4 columns x 4 rows = 16 tiles
  // Used for: 0xFE6 (pit)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(DrawRoutineIds::kActual4x4, obj, bg,
                                       tiles, state);
      });

  auto ensure_index = [this](size_t index) {
    while (draw_routines_.size() <= index) {
      draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                                  gfx::BackgroundBuffer& bg,
                                  std::span<const gfx::TileInfo> tiles,
                                  [[maybe_unused]] const DungeonState* state) {
        self->DrawNothing(obj, bg, tiles, state);
      });
    }
  };

  // Routine 117 - Vertical rails with CORNER+MIDDLE+END pattern (0x8A-0x8C)
  // ASM: RoomDraw_DownwardsHasEdge1x1_1to16_plus23 - matches horizontal 0x22
  ensure_index(117);
  draw_routines_[117] =
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(117, obj, bg, tiles, state);
      };

  // Routine 118 - Horizontal long rails with CORNER+MIDDLE+END pattern (0x5F)
  ensure_index(118);
  draw_routines_[118] =
      [](ObjectDrawer* self, const RoomObject& obj, gfx::BackgroundBuffer& bg,
         std::span<const gfx::TileInfo> tiles, const DungeonState* state) {
        self->DrawUsingRegistryRoutine(118, obj, bg, tiles, state);
      };

  // Routine 130 - Custom Object (Oracle of Secrets 0x31, 0x32)
  // Uses external binary files instead of ROM tile data.
  // Requires CustomObjectManager initialization and enable_custom_objects flag.
  ensure_index(130);
  draw_routines_[130] = [](ObjectDrawer* self, const RoomObject& obj,
                           gfx::BackgroundBuffer& bg,
                           std::span<const gfx::TileInfo> tiles,
                           [[maybe_unused]] const DungeonState* state) {
    self->DrawCustomObject(obj, bg, tiles, state);
  };

  routines_initialized_ = true;
}

int ObjectDrawer::GetDrawRoutineId(int16_t object_id) const {
  // Delegate to the unified registry for the canonical mapping
  return DrawRoutineRegistry::Get().GetRoutineIdForObject(object_id);
}

// ============================================================================
// Draw Routine Implementations (Based on ZScream patterns)
// ============================================================================

void ObjectDrawer::DrawDoor(const DoorDef& door, int door_index,
                            gfx::BackgroundBuffer& bg1,
                            gfx::BackgroundBuffer& bg2,
                            const DungeonState* state) {
  // Door rendering based on ZELDA3_DUNGEON_SPEC.md Section 5 and disassembly
  // Uses DoorType and DoorDirection enums for type safety
  // Position calculations via DoorPositionManager

  LOG_DEBUG("ObjectDrawer", "DrawDoor: idx=%d type=%d dir=%d pos=%d",
            door_index, static_cast<int>(door.type),
            static_cast<int>(door.direction), door.position);

  if (!rom_ || !rom_->is_loaded() || !room_gfx_buffer_) {
    LOG_DEBUG("ObjectDrawer", "DrawDoor: SKIPPED - rom=%p loaded=%d gfx=%p",
              (void*)rom_, rom_ ? rom_->is_loaded() : 0,
              (void*)room_gfx_buffer_);
    return;
  }

  auto& bitmap = bg1.bitmap();
  if (!bitmap.is_active() || bitmap.width() == 0) {
    LOG_DEBUG("ObjectDrawer",
              "DrawDoor: SKIPPED - bitmap not active or zero width");
    return;
  }

  const bool is_door_open =
      state && state->IsDoorOpen(room_id_, door_index);

  // Get door position from DoorPositionManager
  auto [tile_x, tile_y] = door.GetTileCoords();
  auto dims = door.GetDimensions();
  int door_width = dims.width_tiles;
  int door_height = dims.height_tiles;

  LOG_DEBUG("ObjectDrawer", "DrawDoor: tile_pos=(%d,%d) dims=%dx%d", tile_x,
            tile_y, door_width, door_height);

  constexpr int kRoomDrawObjectDataBase = 0x1B52;
  constexpr int kNorthCurtainClosedOffset = 0x078A;

  auto draw_from_object_data =
      [&](gfx::BackgroundBuffer& target, int width, int height,
          int tile_data_addr) {
        auto& bitmap = target.bitmap();
        auto& priority_buffer = target.mutable_priority_data();
        auto& coverage_buffer = target.mutable_coverage_data();
        const int bitmap_width = bitmap.width();
        const auto& rom_data = rom_->data();
        int tile_idx = 0;

        for (int dx = 0; dx < width; dx++) {
          for (int dy = 0; dy < height; dy++) {
            const int addr = tile_data_addr + (tile_idx * 2);
            const uint16_t tile_word =
                rom_data[addr] | (rom_data[addr + 1] << 8);
            const auto tile_info = gfx::WordToTileInfo(tile_word);
            const int pixel_x = (tile_x + dx) * 8;
            const int pixel_y = (tile_y + dy) * 8;

            DrawTileToBitmap(bitmap, tile_info, pixel_x, pixel_y,
                             room_gfx_buffer_);

            const uint8_t priority = tile_info.over_ ? 1 : 0;
            const auto& bitmap_data = bitmap.vector();
            for (int py = 0; py < 8; py++) {
              const int dest_y = pixel_y + py;
              if (dest_y < 0 || dest_y >= bitmap.height()) {
                continue;
              }
              for (int px = 0; px < 8; px++) {
                const int dest_x = pixel_x + px;
                if (dest_x < 0 || dest_x >= bitmap_width) {
                  continue;
                }
                const int dest_index = dest_y * bitmap_width + dest_x;
                if (dest_index >= 0 &&
                    dest_index < static_cast<int>(coverage_buffer.size())) {
                  coverage_buffer[dest_index] = 1;
                }
                if (dest_index < static_cast<int>(bitmap_data.size()) &&
                    bitmap_data[dest_index] != 255) {
                  priority_buffer[dest_index] = priority;
                }
              }
            }

            tile_idx++;
          }
        }
      };

  // USDASM has special north-door branches that do not follow the generic 4x3
  // ranged-door path.
  if (door.direction == DoorDirection::North &&
      door.type == DoorType::ExplodingWall && !is_door_open) {
    LOG_DEBUG("ObjectDrawer",
              "DrawDoor: closed exploding wall intentionally draws nothing");
    (void)bg2;
    return;
  }

  if (door.direction == DoorDirection::North &&
      door.type == DoorType::CurtainDoor && !is_door_open) {
    const int tile_data_addr =
        kRoomDrawObjectDataBase + kNorthCurtainClosedOffset;
    const int data_size = 16 * 2;  // RoomDraw_4x4 closed curtain path.
    if (tile_data_addr < 0 ||
        tile_data_addr + data_size > static_cast<int>(rom_->size())) {
      DrawDoorIndicator(bg1, tile_x, tile_y, /*width=*/4, /*height=*/4,
                        door.type, door.direction);
      return;
    }
    draw_from_object_data(bg1, /*width=*/4, /*height=*/4, tile_data_addr);
    return;
  }

  // Door graphics use an indirect addressing scheme:
  // 1. kDoorGfxUp/Down/Left/Right point to offset tables (DoorGFXDataOffset_*)
  // 2. Each table entry is a 16-bit offset into RoomDrawObjectData
  // 3. RoomDrawObjectData base is at PC 0x1B52 (SNES $00:9B52)
  // 4. Actual tile data = 0x1B52 + offset_from_table

  // Select offset table based on direction
  int offset_table_addr = 0;
  switch (door.direction) {
    case DoorDirection::North:
      offset_table_addr = kDoorGfxUp;
      break;  // 0x4D9E
    case DoorDirection::South:
      offset_table_addr = kDoorGfxDown;
      break;  // 0x4E06
    case DoorDirection::West:
      offset_table_addr = kDoorGfxLeft;
      break;  // 0x4E66
    case DoorDirection::East:
      offset_table_addr = kDoorGfxRight;
      break;  // 0x4EC6
  }

  // Calculate door type index (door types step by 2: 0x00, 0x02, 0x04, ...)
  int type_value = static_cast<int>(door.type);
  int type_index = type_value / 2;

  // Read offset from table (each entry is 2 bytes)
  int table_entry_addr = offset_table_addr + (type_index * 2);
  if (table_entry_addr + 1 >= static_cast<int>(rom_->size())) {
    DrawDoorIndicator(bg1, tile_x, tile_y, door_width, door_height, door.type,
                      door.direction);
    return;
  }

  const auto& rom_data = rom_->data();
  uint16_t tile_offset =
      rom_data[table_entry_addr] | (rom_data[table_entry_addr + 1] << 8);

  // RoomDrawObjectData base address (PC offset)
  int tile_data_addr = kRoomDrawObjectDataBase + tile_offset;

  LOG_DEBUG(
      "ObjectDrawer",
      "DrawDoor: offset_table=0x%X type_idx=%d tile_offset=0x%X tile_addr=0x%X",
      offset_table_addr, type_index, tile_offset, tile_data_addr);

  // Validate address range (12 tiles * 2 bytes = 24 bytes)
  int tiles_per_door = door_width * door_height;  // 12 tiles (4x3 or 3x4)
  int data_size = tiles_per_door * 2;
  if (tile_data_addr < 0 ||
      tile_data_addr + data_size > static_cast<int>(rom_->size())) {
    LOG_DEBUG("ObjectDrawer",
              "DrawDoor: INVALID ADDRESS - falling back to indicator");
    DrawDoorIndicator(bg1, tile_x, tile_y, door_width, door_height, door.type,
                      door.direction);
    return;
  }

  // Read and render door tiles
  // All directions use column-major tile order (matching ASM draw routines)
  // The ROM stores tiles in column-major order for all door directions.
  LOG_DEBUG("ObjectDrawer", "DrawDoor: Reading %d tiles from 0x%X",
            tiles_per_door, tile_data_addr);
  draw_from_object_data(bg1, door_width, door_height, tile_data_addr);

  LOG_DEBUG("ObjectDrawer",
            "DrawDoor: type=%s dir=%s pos=%d at tile(%d,%d) size=%dx%d "
            "offset_table=0x%X tile_offset=0x%X tile_addr=0x%X",
            std::string(GetDoorTypeName(door.type)).c_str(),
            std::string(GetDoorDirectionName(door.direction)).c_str(),
            door.position, tile_x, tile_y, door_width, door_height,
            offset_table_addr, tile_offset, tile_data_addr);
}

void ObjectDrawer::DrawDoorIndicator(gfx::BackgroundBuffer& bg, int tile_x,
                                     int tile_y, int width, int height,
                                     DoorType type, DoorDirection direction) {
  // Draw a simple colored rectangle as door indicator when graphics unavailable
  // Different colors for different door types using DoorType enum

  auto& bitmap = bg.bitmap();
  auto& coverage_buffer = bg.mutable_coverage_data();

  uint8_t color_idx;
  switch (type) {
    case DoorType::NormalDoor:
    case DoorType::NormalDoorLower:
      color_idx = 45;  // Standard door color (brown)
      break;

    case DoorType::SmallKeyDoor:
    case DoorType::SmallKeyStairsUp:
    case DoorType::SmallKeyStairsDown:
    case DoorType::SmallKeyStairsUpLower:
    case DoorType::SmallKeyStairsDownLower:
      color_idx = 60;  // Key door - yellowish
      break;

    case DoorType::BigKeyDoor:
    case DoorType::UnopenableBigKeyDoor:
      color_idx = 58;  // Big key - golden
      break;

    case DoorType::BombableDoor:
    case DoorType::BombableCaveExit:
    case DoorType::ExplodingWall:
    case DoorType::DashWall:
      color_idx = 15;  // Bombable/destructible - brownish/cracked
      break;

    case DoorType::DoubleSidedShutter:
    case DoorType::DoubleSidedShutterLower:
    case DoorType::BottomSidedShutter:
    case DoorType::TopSidedShutter:
    case DoorType::BottomShutterLower:
    case DoorType::TopShutterLower:
      color_idx = 30;  // Shutter - greenish
      break;

    case DoorType::EyeWatchDoor:
      color_idx = 42;  // Eye watch - lighter brown
      break;

    case DoorType::CurtainDoor:
      color_idx = 35;  // Curtain - special
      break;

    case DoorType::CaveExit:
    case DoorType::FancyDungeonExit:
    case DoorType::FancyDungeonExitLower:
    case DoorType::WaterfallDoor:
    case DoorType::LitCaveExitLower:
      color_idx = 25;  // Cave/dungeon exit - dark
      break;

    case DoorType::ExitMarker:
    case DoorType::DungeonSwapMarker:
    case DoorType::LayerSwapMarker:
      color_idx = 5;  // Markers - very faint
      break;

    default:
      color_idx = 50;  // Default door color
      break;
  }

  int pixel_x = tile_x * 8;
  int pixel_y = tile_y * 8;
  int pixel_width = width * 8;
  int pixel_height = height * 8;

  int bitmap_width = bitmap.width();
  int bitmap_height = bitmap.height();

  // Draw filled rectangle with border
  for (int py = 0; py < pixel_height; py++) {
    for (int px = 0; px < pixel_width; px++) {
      int dest_x = pixel_x + px;
      int dest_y = pixel_y + py;

      if (dest_x >= 0 && dest_x < bitmap_width && dest_y >= 0 &&
          dest_y < bitmap_height) {
        // Draw border (2 pixel thick) or fill
        bool is_border = (px < 2 || px >= pixel_width - 2 || py < 2 ||
                          py >= pixel_height - 2);
        uint8_t final_color = is_border ? (color_idx + 5) : color_idx;

        int offset = (dest_y * bitmap_width) + dest_x;
        bitmap.WriteToPixel(offset, final_color);

        if (offset >= 0 && offset < static_cast<int>(coverage_buffer.size())) {
          coverage_buffer[offset] = 1;
        }
      }
    }
  }
}

void ObjectDrawer::DrawChest(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                             std::span<const gfx::TileInfo> tiles,
                             [[maybe_unused]] const DungeonState* state) {
  // ASM: RoomDraw_Chest - draws a SINGLE chest (2x2) without size-based repetition
  // 0xF99 = closed chest, 0xF9A = open chest
  // The size byte is NOT used for repetition

  // Determine if chest is open
  bool is_open = false;
  if (state) {
    is_open = state->IsChestOpen(room_id_, current_chest_index_);
  }

  // Increment index for next chest
  current_chest_index_++;

  // Draw SINGLE chest - no repetition based on size
  // Standard chests are 2x2 (4 tiles)
  // If we have extra tiles loaded, the second 4 are for open state

  if (is_open && tiles.size() >= 8) {
    // Small chest open tiles (indices 4-7) - SINGLE 2x2 draw
    if (tiles.size() >= 8) {
      WriteTile8(bg, obj.x_, obj.y_, tiles[4]);          // top-left
      WriteTile8(bg, obj.x_, obj.y_ + 1, tiles[5]);      // bottom-left
      WriteTile8(bg, obj.x_ + 1, obj.y_, tiles[6]);      // top-right
      WriteTile8(bg, obj.x_ + 1, obj.y_ + 1, tiles[7]);  // bottom-right
    }
    return;
  }

  // Draw closed chest - SINGLE 2x2 pattern (column-major order)
  if (tiles.size() >= 4) {
    WriteTile8(bg, obj.x_, obj.y_, tiles[0]);          // top-left
    WriteTile8(bg, obj.x_, obj.y_ + 1, tiles[1]);      // bottom-left
    WriteTile8(bg, obj.x_ + 1, obj.y_, tiles[2]);      // top-right
    WriteTile8(bg, obj.x_ + 1, obj.y_ + 1, tiles[3]);  // bottom-right
  }
}

void ObjectDrawer::DrawNothing(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                               std::span<const gfx::TileInfo> tiles,
                               [[maybe_unused]] const DungeonState* state) {
  // Intentionally empty - represents invisible logic objects or placeholders
  // ASM: RoomDraw_Nothing_A ($0190F2), RoomDraw_Nothing_B ($01932E), etc.
  // These routines typically just RTS.
  LOG_DEBUG("ObjectDrawer", "DrawNothing for object 0x%02X (logic/invisible)",
            obj.id_);
}

void ObjectDrawer::CustomDraw(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
  // Pattern: Custom draw routine (objects 0x31-0x32)
  // For now, fall back to simple 1x1
  if (tiles.size() >= 1) {
    // Use first 8x8 tile from span
    WriteTile8(bg, obj.x_, obj.y_, tiles[0]);
  }
}

void ObjectDrawer::DrawRightwards4x4_1to16(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles,
    [[maybe_unused]] const DungeonState* state) {
  // Pattern: 4x4 block rightward (objects 0x33, 0xBA = large ceiling, etc.)
  int size = obj.size_ & 0x0F;

  // Assembly: GetSize_1to16, so count = size + 1
  int count = size + 1;

  // Debug: Log large ceiling objects (0xBA)
  if (obj.id_ == 0xBA && tiles.size() >= 16) {
    LOG_DEBUG("ObjectDrawer",
              "Large Ceiling Draw: obj=0x%02X pos=(%d,%d) size=%d tiles=%zu",
              obj.id_, obj.x_, obj.y_, size, tiles.size());
    LOG_DEBUG("ObjectDrawer", "  First 4 Tile IDs: [%d, %d, %d, %d]",
              tiles[0].id_, tiles[1].id_, tiles[2].id_, tiles[3].id_);
    LOG_DEBUG("ObjectDrawer", "  First 4 Palettes: [%d, %d, %d, %d]",
              tiles[0].palette_, tiles[1].palette_, tiles[2].palette_,
              tiles[3].palette_);
  }

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 16) {
      // Draw 4x4 pattern in COLUMN-MAJOR order (matching assembly)
      // Iterate columns (x) first, then rows (y) within each column
      for (int x = 0; x < 4; ++x) {
        for (int y = 0; y < 4; ++y) {
          WriteTile8(bg, obj.x_ + (s * 4) + x, obj.y_ + y, tiles[x * 4 + y]);
        }
      }
    }
  }
}

void ObjectDrawer::DrawRightwardsDecor4x3spaced4_1to16(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles,
    [[maybe_unused]] const DungeonState* state) {
  // Pattern: 4x3 decoration with spacing (objects 0x3A-0x3B)
  // 4 columns × 3 rows = 12 tiles in COLUMN-MAJOR order
  // ASM: ADC #$0008 to Y = 8-byte advance = 4 tiles per iteration
  // Total spacing: 4 (object width) + 4 (gap) = 8 tiles between starts
  int size = obj.size_ & 0x0F;

  // Assembly: GetSize_1to16, so count = size + 1
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 12) {
      // Draw 4x3 pattern in COLUMN-MAJOR order (matching assembly)
      // Spacing: 8 tiles (4 object + 4 gap) per ASM ADC #$0008
      for (int x = 0; x < 4; ++x) {
        for (int y = 0; y < 3; ++y) {
          WriteTile8(bg, obj.x_ + (s * 8) + x, obj.y_ + y, tiles[x * 3 + y]);
        }
      }
    }
  }
}

// ============================================================================
// Utility Methods
// ============================================================================

void ObjectDrawer::MarkBg1RectTransparent(gfx::BackgroundBuffer& bg1,
                                          int start_px, int start_py,
                                          int pixel_width, int pixel_height) {
  auto& bitmap = bg1.bitmap();
  if (!bitmap.is_active() || bitmap.width() == 0) {
    return;
  }

  int canvas_width = bitmap.width();
  int canvas_height = bitmap.height();
  auto& data = bitmap.mutable_data();

  for (int py = start_py; py < start_py + pixel_height && py < canvas_height;
       py++) {
    if (py < 0)
      continue;
    for (int px = start_px; px < start_px + pixel_width && px < canvas_width;
         px++) {
      if (px < 0)
        continue;
      int idx = py * canvas_width + px;
      if (idx >= 0 && idx < static_cast<int>(data.size())) {
        data[idx] = 255;
      }
    }
  }
  bitmap.set_modified(true);
}

void ObjectDrawer::MarkBG1Transparent(gfx::BackgroundBuffer& bg1, int tile_x,
                                      int tile_y, int pixel_width,
                                      int pixel_height) {
  MarkBg1RectTransparent(bg1, tile_x * 8, tile_y * 8, pixel_width,
                         pixel_height);
}

void ObjectDrawer::WriteTile8(gfx::BackgroundBuffer& bg, int tile_x, int tile_y,
                              const gfx::TileInfo& tile_info) {
  if (!IsValidTilePosition(tile_x, tile_y)) {
    return;
  }
  PushTrace(tile_x, tile_y, tile_info);
  if (trace_only_) {
    return;
  }
  // Draw directly to bitmap instead of tile buffer to avoid being overwritten
  auto& bitmap = bg.bitmap();
  if (!bitmap.is_active() || bitmap.width() == 0) {
    return;  // Bitmap not ready
  }

  // The room-specific graphics buffer (current_gfx16_) contains the assembled
  // tile graphics for the current room. Object tile IDs are relative to this
  // buffer.
  const uint8_t* gfx_data = room_gfx_buffer_;

  if (!gfx_data) {
    LOG_DEBUG("ObjectDrawer", "ERROR: No graphics data available");
    return;
  }

  // Draw single 8x8 tile directly to bitmap
  DrawTileToBitmap(bitmap, tile_info, tile_x * 8, tile_y * 8, gfx_data);

  // Mark coverage for the full 8x8 tile region (even if pixels are transparent).
  //
  // This distinguishes "tilemap entry written but transparent" from "no write",
  // which is required to emulate SNES behavior where a transparent tile still
  // overwrites the previous tilemap entry (clearing BG1 and revealing BG2/backdrop).
  auto& coverage_buffer = bg.mutable_coverage_data();

  // Also update priority buffer with tile's priority bit.
  // Priority (over_) affects Z-ordering in SNES Mode 1 compositing.
  uint8_t priority = tile_info.over_ ? 1 : 0;
  int pixel_x = tile_x * 8;
  int pixel_y = tile_y * 8;
  auto& priority_buffer = bg.mutable_priority_data();
  int width = bitmap.width();

  // Update priority for each pixel in the 8x8 tile
  const auto& bitmap_data = bitmap.vector();
  for (int py = 0; py < 8; py++) {
    int dest_y = pixel_y + py;
    if (dest_y < 0 || dest_y >= bitmap.height())
      continue;

    for (int px = 0; px < 8; px++) {
      int dest_x = pixel_x + px;
      if (dest_x < 0 || dest_x >= width)
        continue;

      int dest_index = dest_y * width + dest_x;

      // Coverage is set for all pixels in the tile footprint.
      if (dest_index >= 0 &&
          dest_index < static_cast<int>(coverage_buffer.size())) {
        coverage_buffer[dest_index] = 1;
      }

      // Store priority only for opaque pixels; transparent writes clear stale
      // priority at this location.
      if (dest_index < static_cast<int>(bitmap_data.size()) &&
          bitmap_data[dest_index] != 255) {
        priority_buffer[dest_index] = priority;
      } else {
        priority_buffer[dest_index] = 0xFF;
      }
    }
  }
}

bool ObjectDrawer::IsValidTilePosition(int tile_x, int tile_y) const {
  return tile_x >= 0 && tile_x < kMaxTilesX && tile_y >= 0 &&
         tile_y < kMaxTilesY;
}

void ObjectDrawer::DrawTileToBitmap(gfx::Bitmap& bitmap,
                                    const gfx::TileInfo& tile_info, int pixel_x,
                                    int pixel_y, const uint8_t* tiledata) {
  // Draw an 8x8 tile directly to bitmap at pixel coordinates
  // Graphics data is in 8BPP linear format (1 pixel per byte)
  if (!tiledata)
    return;

  // DEBUG: Check if bitmap is valid
  if (!bitmap.is_active() || bitmap.width() == 0 || bitmap.height() == 0) {
    LOG_DEBUG("ObjectDrawer", "ERROR: Invalid bitmap - active=%d, size=%dx%d",
              bitmap.is_active(), bitmap.width(), bitmap.height());
    return;
  }

  // Calculate tile position in 8BPP graphics buffer
  // Layout: 16 tiles per row, each tile is 8 pixels wide (8 bytes)
  // Row stride: 128 bytes (16 tiles * 8 bytes)
  // Buffer size: 0x10000 (65536 bytes) = 64 tile rows max
  constexpr int kGfxBufferSize = 0x10000;
  constexpr int kMaxTileRow = 63;  // 64 rows (0-63), each 1024 bytes

  int tile_col = tile_info.id_ % 16;
  int tile_row = tile_info.id_ / 16;

  // CRITICAL: Validate tile_row to prevent index out of bounds
  if (tile_row > kMaxTileRow) {
    LOG_DEBUG("ObjectDrawer", "Tile ID 0x%03X out of bounds (row %d > %d)",
              tile_info.id_, tile_row, kMaxTileRow);
    return;
  }

  int tile_base_x = tile_col * 8;  // 8 bytes per tile horizontally
  int tile_base_y =
      tile_row * 1024;  // 1024 bytes per tile row (8 rows * 128 bytes)

  // DEBUG: Log first few tiles being drawn with their graphics data
  static int draw_debug_count = 0;
  if (draw_debug_count < 5) {
    int sample_index = tile_base_y + tile_base_x;
    LOG_DEBUG("ObjectDrawer",
              "DrawTile: id=%d (col=%d,row=%d) gfx_offset=%d (0x%04X)",
              tile_info.id_, tile_col, tile_row, sample_index, sample_index);
    draw_debug_count++;
  }

  // Palette offset calculation using direct CGRAM row mirroring.
  //
  // Room::RenderRoomGraphics loads dungeon main palettes into SDL bank rows 2-7,
  // leaving rows 0-1 as transparent HUD placeholders. The tile palette bits are
  // therefore already the correct SDL bank row index.
  //
  // Drawing formula: final_color = pixel + (pal * 16)
  // Where pixel 0 = transparent (not written), pixel 1-15 = colors within bank.
  uint8_t pal = tile_info.palette_ & 0x07;
  const uint8_t palette_offset = static_cast<uint8_t>(pal * 16);

  // Draw 8x8 pixels with overwrite semantics.
  //
  // Important SNES behavior: writing a tilemap entry replaces the previous
  // contents for the full 8x8 footprint. Source pixel 0 is transparent, but it
  // still clears what was there before. We model that by writing 255
  // (transparent key) for zero pixels.
  bool any_pixels_changed = false;

  for (int py = 0; py < 8; py++) {
    // Source row with vertical mirroring
    int src_row = tile_info.vertical_mirror_ ? (7 - py) : py;

    for (int px = 0; px < 8; px++) {
      // Source column with horizontal mirroring
      int src_col = tile_info.horizontal_mirror_ ? (7 - px) : px;

      // Calculate source index in 8BPP buffer
      // Stride is 128 bytes (sheet width)
      int src_index = (src_row * 128) + src_col + tile_base_x + tile_base_y;
      uint8_t pixel = tiledata[src_index];
      uint8_t out_pixel = 255;  // transparent/clear
      if (pixel != 0) {
        // Pixels 1-15 map into a 16-color bank chunk.
        out_pixel = static_cast<uint8_t>(pixel + palette_offset);
      }

      int dest_x = pixel_x + px;
      int dest_y = pixel_y + py;
      if (dest_x < 0 || dest_x >= bitmap.width() || dest_y < 0 ||
          dest_y >= bitmap.height()) {
        continue;
      }

      int dest_index = dest_y * bitmap.width() + dest_x;
      if (dest_index < 0 ||
          dest_index >= static_cast<int>(bitmap.mutable_data().size())) {
        continue;
      }

      auto& dst = bitmap.mutable_data()[dest_index];
      if (dst != out_pixel) {
        dst = out_pixel;
        any_pixels_changed = true;
      }
    }
  }

  if (any_pixels_changed) {
    bitmap.set_modified(true);
  }
}

absl::Status ObjectDrawer::DrawRoomDrawObjectData2x2(
    uint16_t object_id, int tile_x, int tile_y, RoomObject::LayerType layer,
    uint16_t room_draw_object_data_offset, gfx::BackgroundBuffer& bg1,
    gfx::BackgroundBuffer& bg2) {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  const auto& rom_data = rom_->vector();
  const int base =
      kRoomObjectTileAddress + static_cast<int>(room_draw_object_data_offset);
  if (base < 0 || base + 7 >= static_cast<int>(rom_data.size())) {
    return absl::OutOfRangeError(absl::StrFormat(
        "RoomDrawObjectData 2x2 out of range: base=0x%X", base));
  }

  auto read_word = [&](int off) -> uint16_t {
    return static_cast<uint16_t>(rom_data[off]) |
           (static_cast<uint16_t>(rom_data[off + 1]) << 8);
  };

  const uint16_t w0 = read_word(base + 0);
  const uint16_t w1 = read_word(base + 2);
  const uint16_t w2 = read_word(base + 4);
  const uint16_t w3 = read_word(base + 6);

  const gfx::TileInfo t0 = gfx::WordToTileInfo(w0);
  const gfx::TileInfo t1 = gfx::WordToTileInfo(w1);
  const gfx::TileInfo t2 = gfx::WordToTileInfo(w2);
  const gfx::TileInfo t3 = gfx::WordToTileInfo(w3);

  // Set trace context once; WriteTile8 will emit per-tile traces.
  RoomObject trace_obj(
      static_cast<int16_t>(object_id), static_cast<uint8_t>(tile_x),
      static_cast<uint8_t>(tile_y), 0, static_cast<uint8_t>(layer));
  SetTraceContext(trace_obj, layer);

  gfx::BackgroundBuffer& target_bg =
      (layer == RoomObject::LayerType::BG2) ? bg2 : bg1;

  // Column-major order (matches USDASM $BF/$CB/$C2/$CE writes).
  WriteTile8(target_bg, tile_x + 0, tile_y + 0, t0);  // top-left
  WriteTile8(target_bg, tile_x + 0, tile_y + 1, t1);  // bottom-left
  WriteTile8(target_bg, tile_x + 1, tile_y + 0, t2);  // top-right
  WriteTile8(target_bg, tile_x + 1, tile_y + 1, t3);  // bottom-right

  return absl::OkStatus();
}

// ============================================================================
// Type 3 / Special Routine Implementations
// ============================================================================

void ObjectDrawer::DrawLargeCanvasObject(const RoomObject& obj,
                                         gfx::BackgroundBuffer& bg,
                                         std::span<const gfx::TileInfo> tiles,
                                         int width, int height) {
  // Generic large object drawer
  if (tiles.size() >= static_cast<size_t>(width * height)) {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        WriteTile8(bg, obj.x_ + x, obj.y_ + y, tiles[y * width + x]);
      }
    }
  }
}

}  // namespace zelda3
}  // namespace yaze

std::pair<int, int> yaze::zelda3::ObjectDrawer::CalculateObjectDimensions(
    const RoomObject& object) {
  if (!routines_initialized_) {
    InitializeDrawRoutines();
  }

  // Default size 16x16 (2x2 tiles)
  int width = 16;
  int height = 16;

  int routine_id = GetDrawRoutineId(object.id_);
  int size = object.size_;

  // Based on routine ID, calculate dimensions
  // This logic must match the draw routines
  switch (routine_id) {
    case 0:   // DrawRightwards2x2_1to15or32
    case 4:   // DrawRightwards2x2_1to16
    case 7:   // DrawDownwards2x2_1to15or32
    case 11:  // DrawDownwards2x2_1to16
      // 2x2 tiles repeated
      if (routine_id == 0 || routine_id == 7) {
        if (size == 0)
          size = 32;
      } else {
        size = size & 0x0F;
        if (size == 0)
          size = 16;  // 0 usually means 16 for 1to16 routines
      }

      if (routine_id == 0 || routine_id == 4) {
        // Rightwards: size * 2 tiles width, 2 tiles height
        width = size * 16;
        height = 16;
      } else {
        // Downwards: 2 tiles width, size * 2 tiles height
        width = 16;
        height = size * 16;
      }
      break;

    case 1:  // RoomDraw_Rightwards2x4_1to15or26 (layout walls 0x01-0x02)
    {
      // ASM: GetSize_1to15or26 - defaults to 26 when size is 0
      int effective_size = (size == 0) ? 26 : (size & 0x0F);
      // Draws 2x4 tiles repeated 'effective_size' times horizontally
      width = effective_size * 16;  // 2 tiles wide per repetition
      height = 32;                  // 4 tiles tall
      break;
    }
    case 2:  // RoomDraw_Rightwards2x4spaced4_1to16 (objects 0x03-0x04)
    case 3:  // RoomDraw_Rightwards2x4spaced4_1to16_BothBG (objects 0x05-0x06)
    {
      // ASM: GetSize_1to16, draws 2x4 tiles with 4-tile adjacent spacing
      size = size & 0x0F;
      int count = size + 1;
      width = count * 16;  // 2 tiles wide per repetition (adjacent)
      height = 32;         // 4 tiles tall
      break;
    }

    case 5:  // DrawDiagonalAcute_1to16
    case 6:  // DrawDiagonalGrave_1to16
    {
      // ASM: RoomDraw_DiagonalAcute/Grave_1to16
      // Uses LDA #$0007; JSR RoomDraw_GetSize_1to16_timesA
      // count = size + 7
      // Each iteration draws 5 tiles vertically (RoomDraw_2x2and1 pattern)
      // Width = count tiles, Height = 5 tiles base + (count-1) diagonal offset
      size = size & 0x0F;
      int count = size + 7;
      width = count * 8;
      height = (count + 4) * 8;  // 5 tiles + (count-1) = count + 4
      break;
    }
    case 17:  // DrawDiagonalAcute_1to16_BothBG
    case 18:  // DrawDiagonalGrave_1to16_BothBG
    {
      // ASM: RoomDraw_DiagonalAcute/Grave_1to16_BothBG
      // Uses LDA #$0006; JSR RoomDraw_GetSize_1to16_timesA
      // count = size + 6 (one less than non-BothBG)
      size = size & 0x0F;
      int count = size + 6;
      width = count * 8;
      height = (count + 4) * 8;  // 5 tiles + (count-1) = count + 4
      break;
    }

    case 8:  // RoomDraw_Downwards4x2_1to15or26 (layout walls 0x61-0x62)
    {
      // ASM: GetSize_1to15or26 - defaults to 26 when size is 0
      int effective_size = (size == 0) ? 26 : (size & 0x0F);
      // Draws 4x2 tiles repeated 'effective_size' times vertically
      width = 32;                    // 4 tiles wide
      height = effective_size * 16;  // 2 tiles tall per repetition
      break;
    }
    case 9:   // RoomDraw_Downwards4x2_1to16_BothBG (objects 0x63-0x64)
    case 10:  // RoomDraw_DownwardsDecor4x2spaced4_1to16 (objects 0x65-0x66)
    {
      // ASM: GetSize_1to16, draws 4x2 tiles with spacing
      size = size & 0x0F;
      int count = size + 1;
      width = 32;           // 4 tiles wide
      height = count * 16;  // 2 tiles tall per repetition (adjacent)
      break;
    }

    case 12:  // RoomDraw_DownwardsHasEdge1x1_1to16_plus3
      size = size & 0x0F;
      width = 8;
      height = (size + 3) * 8;
      break;
    case 13:  // RoomDraw_DownwardsEdge1x1_1to16
      size = size & 0x0F;
      width = 8;
      height = (size + 1) * 8;
      break;
    case 14:  // RoomDraw_DownwardsLeftCorners2x1_1to16_plus12
    case 15:  // RoomDraw_DownwardsRightCorners2x1_1to16_plus12
      size = size & 0x0F;
      width = 16;
      height = (size + 10) * 8;
      break;

    case 16:  // DrawRightwards4x4_1to16 (Routine 16)
    {
      // 4x4 block repeated horizontally based on size
      // ASM: GetSize_1to16, count = (size & 0x0F) + 1
      int count = (size & 0x0F) + 1;
      width = 32 * count;  // 4 tiles * 8 pixels * count
      height = 32;         // 4 tiles * 8 pixels
      break;
    }
    case 19:  // DrawCorner4x4 (Type 2 corners 0x100-0x103)
    case 34:  // Water Face (4x4)
    case 35:  // 4x4 Corner BothBG
    case 36:  // Weird Corner Bottom
    case 37:  // Weird Corner Top
      // 4x4 tiles (32x32 pixels) - fixed size, no repetition
      width = 32;
      height = 32;
      break;
    case 39: {  // Chest routine (small or big)
      // Infer size from tile span: big chests provide >=16 tiles
      int tile_count = object.tiles().size();
      if (tile_count >= 16) {
        width = height = 32;  // Big chest 4x4
      } else {
        width = height = 16;  // Small chest 2x2
      }
      break;
    }

    case 20:  // Edge 1x2 (RoomDraw_Rightwards1x2_1to16_plus2)
    {
      // ZScream: width = size * 2 + 4, height = 3 tiles
      size = size & 0x0F;
      width = (size * 2 + 4) * 8;
      height = 24;
      break;
    }

    case 21:  // RoomDraw_RightwardsHasEdge1x1_1to16_plus3 (small rails 0x22)
    {
      // ZScream: count = size + 2 (corner + middle*count + end)
      size = size & 0x0F;
      width = (size + 4) * 8;
      height = 8;
      break;
    }
    case 22:  // RoomDraw_RightwardsHasEdge1x1_1to16_plus2 (carpet trim 0x23-0x2E)
    {
      // ASM: GetSize_1to16, count = size + 1
      // Plus corner (1) + end (1) = count + 2 total width
      size = size & 0x0F;
      int count = size + 1;
      width = (count + 2) * 8;  // corner + middle*count + end
      height = 8;
      break;
    }
    case 118:  // RoomDraw_RightwardsHasEdge1x1_1to16_plus23 (long rails 0x5F)
    {
      size = size & 0x0F;
      width = (size + 23) * 8;
      height = 8;
      break;
    }
    case 25:  // RoomDraw_Rightwards1x1Solid_1to16_plus3
    {
      // ASM: GetSize_1to16_timesA(4), so count = size + 4
      size = size & 0x0F;
      width = (size + 4) * 8;
      height = 8;
      break;
    }

    case 23:  // RightwardsTopCorners1x2_1to16_plus13
    case 24:  // RightwardsBottomCorners1x2_1to16_plus13
      size = size & 0x0F;
      width = 8 + size * 8;
      height = 16;
      break;

    case 26:  // Door Switcher
      width = 32;
      height = 32;
      break;

    case 27:  // RoomDraw_RightwardsDecor4x4spaced2_1to16
    {
      // 4x4 tiles with 6-tile X spacing per repetition
      // ASM: s * 6 spacing, count = size + 1
      size = size & 0x0F;
      int count = size + 1;
      // Total width = (count - 1) * 6 (spacing) + 4 (last block)
      width = ((count - 1) * 6 + 4) * 8;
      height = 32;  // 4 tiles
      break;
    }

    case 28:  // RoomDraw_RightwardsStatue2x3spaced2_1to16
    {
      // 2x3 tiles with 4-tile X spacing per repetition
      // ASM: s * 4 spacing, count = size + 1
      size = size & 0x0F;
      int count = size + 1;
      // Total width = (count - 1) * 4 (spacing) + 2 (last block)
      width = ((count - 1) * 4 + 2) * 8;
      height = 24;  // 3 tiles
      break;
    }

    case 29:  // RoomDraw_RightwardsPillar2x4spaced4_1to16
    {
      // 2x4 tiles with 4-tile X spacing per repetition
      // ASM: ADC #$0008 = 4 tiles between starts
      size = size & 0x0F;
      int count = size + 1;
      // Total width = (count - 1) * 4 (spacing) + 2 (last block)
      width = ((count - 1) * 4 + 2) * 8;
      height = 32;  // 4 tiles
      break;
    }

    case 30:  // RoomDraw_RightwardsDecor4x3spaced4_1to16
    {
      // 4x3 tiles with 8-tile X spacing per repetition
      // ASM: ADC #$0008 = 8-byte advance = 4 tiles gap between 4-tile objects
      size = size & 0x0F;
      int count = size + 1;
      // Total width = (count - 1) * 8 (spacing) + 4 (last block)
      width = ((count - 1) * 8 + 4) * 8;
      height = 24;  // 3 tiles
      break;
    }

    case 31:  // RoomDraw_RightwardsDoubled2x2spaced2_1to16
    {
      // 4x2 tiles (doubled 2x2) with 6-tile X spacing
      // ASM: s * 6 spacing, count = size + 1
      size = size & 0x0F;
      int count = size + 1;
      // Total width = (count - 1) * 6 (spacing) + 4 (last block)
      width = ((count - 1) * 6 + 4) * 8;
      height = 16;  // 2 tiles
      break;
    }
    case 32:  // RoomDraw_RightwardsDecor2x2spaced12_1to16
    {
      // 2x2 tiles with 14-tile X spacing per repetition
      // ASM: s * 14 spacing, count = size + 1
      size = size & 0x0F;
      int count = size + 1;
      // Total width = (count - 1) * 14 (spacing) + 2 (last block)
      width = ((count - 1) * 14 + 2) * 8;
      height = 16;  // 2 tiles
      break;
    }

    case 33:  // Somaria Line
      // Variable length, estimate from size
      size = size & 0x0F;
      width = 8 + size * 8;
      height = 8;
      break;

    case 38:  // Nothing (RoomDraw_Nothing)
      width = 8;
      height = 8;
      break;

    case 40:  // Rightwards 4x2 (FloorTile)
    {
      // 4 cols x 2 rows, GetSize_1to16
      size = size & 0x0F;
      int count = size + 1;
      width = count * 4 * 8;  // 4 tiles per repetition
      height = 16;            // 2 tiles
      break;
    }

    case 41:  // Rightwards Decor 1x8 spaced 12 (wall torches 0x55-0x56)
    {
      // ASM: 1 column x 8 rows with 12-tile horizontal spacing
      size = size & 0x0F;
      int count = size + 1;
      width = ((count - 1) * 12 + 1) * 8;  // 1 tile wide per block
      height = 64;                         // 8 tiles tall
      break;
    }

    case 42:  // Rightwards Cannon Hole 4x3
    {
      // 4x3 tiles, GetSize_1to16
      size = size & 0x0F;
      int count = size + 1;
      width = count * 4 * 8;
      height = 24;
      break;
    }

    case 43:  // Downwards Floor 4x4
    {
      // 4x4 tiles, GetSize_1to16
      size = size & 0x0F;
      int count = size + 1;
      width = 32;
      height = count * 4 * 8;
      break;
    }

    case 44:  // Downwards 1x1 Solid +3
    {
      size = size & 0x0F;
      width = 8;
      height = (size + 4) * 8;
      break;
    }

    case 45:  // Downwards Decor 4x4 spaced 2
    {
      size = size & 0x0F;
      int count = size + 1;
      width = 32;
      height = ((count - 1) * 6 + 4) * 8;
      break;
    }

    case 46:  // Downwards Pillar 2x4 spaced 2
    {
      size = size & 0x0F;
      int count = size + 1;
      width = 16;
      height = ((count - 1) * 6 + 4) * 8;
      break;
    }

    case 47:  // Downwards Decor 3x4 spaced 4
    {
      size = size & 0x0F;
      int count = size + 1;
      width = 24;
      height = ((count - 1) * 6 + 4) * 8;
      break;
    }

    case 48:  // Downwards Decor 2x2 spaced 12
    {
      size = size & 0x0F;
      int count = size + 1;
      width = 16;
      height = ((count - 1) * 14 + 2) * 8;
      break;
    }

    case 49:  // Downwards Line 1x1 +1
    {
      size = size & 0x0F;
      width = 8;
      height = (size + 2) * 8;
      break;
    }

    case 50:  // Downwards Decor 2x4 spaced 8
    {
      size = size & 0x0F;
      int count = size + 1;
      width = 16;
      height = ((count - 1) * 12 + 4) * 8;
      break;
    }

    case 51:  // Rightwards Line 1x1 +1
    {
      size = size & 0x0F;
      width = (size + 2) * 8;
      height = 8;
      break;
    }

    case 52:  // Rightwards Bar 4x3
    {
      size = size & 0x0F;
      int count = size + 1;
      width = ((count - 1) * 6 + 4) * 8;
      height = 24;
      break;
    }

    case 53:  // Rightwards Shelf 4x4
    {
      size = size & 0x0F;
      int count = size + 1;
      width = ((count - 1) * 6 + 4) * 8;
      height = 32;
      break;
    }

    case 54:  // Rightwards Big Rail 1x3 +5
    {
      size = size & 0x0F;
      width = (size + 6) * 8;
      height = 24;
      break;
    }

    case 55:  // Rightwards Block 2x2 spaced 2
    {
      size = size & 0x0F;
      int count = size + 1;
      width = ((count - 1) * 4 + 2) * 8;
      height = 16;
      break;
    }

    // Routines 56-64: SuperSquare patterns
    // ASM: Type1/Type3 objects pack 2-bit X/Y sizes into a 4-bit size:
    //   size = (x_size << 2) | y_size, where x_size/y_size are 0..3 (meaning 1..4).
    // Each super square unit is 4 tiles (32 pixels) in each dimension.
    case 56:  // 4x4BlocksIn4x4SuperSquare
    case 57:  // 3x3FloorIn4x4SuperSquare
    case 58:  // 4x4FloorIn4x4SuperSquare
    case 59:  // 4x4FloorOneIn4x4SuperSquare
    case 60:  // 4x4FloorTwoIn4x4SuperSquare
    case 62:  // Spike2x2In4x4SuperSquare
    {
      int size_x = ((size >> 2) & 0x03) + 1;
      int size_y = (size & 0x03) + 1;
      width = size_x * 32;   // 4 tiles per super square
      height = size_y * 32;  // 4 tiles per super square
      break;
    }
    case 61:  // BigHole4x4
    case 63:  // TableRock4x4
    case 64:  // WaterOverlay8x8
      width = 32;
      height = 32;
      break;

    // Routines 65-74: Various downwards/rightwards patterns
    case 65:  // DownwardsDecor3x4spaced2
    {
      size = size & 0x0F;
      int count = size + 1;
      width = 24;
      height = ((count - 1) * 5 + 4) * 8;
      break;
    }

    case 66:  // DownwardsBigRail3x1 +5
    {
      // Top cap (2x2) + Middle (2x1 x count) + Bottom cap (2x3)
      // Total: 2 tiles wide, 2 + (size+1) + 3 = size + 6 tiles tall
      size = size & 0x0F;
      width = 16;  // 2 tiles wide
      height = (size + 6) * 8;
      break;
    }

    case 67:  // DownwardsBlock2x2spaced2
    {
      size = size & 0x0F;
      int count = size + 1;
      width = 16;
      height = ((count - 1) * 4 + 2) * 8;
      break;
    }

    case 68:  // DownwardsCannonHole3x4
    {
      size = size & 0x0F;
      width = 24;
      // Height = repeated 3x2 segment (size+1) + final 3x2 edge segment.
      // => (2 * (size + 2)) tiles.
      height = (2 * (size + 2)) * 8;
      break;
    }

    case 69:  // DownwardsBar2x5
    {
      size = size & 0x0F;
      width = 16;
      // 1 top row + 2*(size+2) body rows.
      height = (2 * size + 5) * 8;
      break;
    }

    case 70:  // DownwardsPots2x2
    case 71:  // DownwardsHammerPegs2x2
    {
      size = size & 0x0F;
      int count = size + 1;
      width = 16;
      height = count * 2 * 8;
      break;
    }

    case 72:  // RightwardsEdge1x1 +7
    {
      size = size & 0x0F;
      width = (size + 8) * 8;
      height = 8;
      break;
    }

    case 73:  // RightwardsPots2x2
    case 74:  // RightwardsHammerPegs2x2
    {
      size = size & 0x0F;
      int count = size + 1;
      width = count * 2 * 8;
      height = 16;
      break;
    }

    // Diagonal ceilings (75-78) - TRIANGLE shapes
    // Draw uses count = (size & 0x0F) + 4
    // Outline uses smaller size since triangle only fills half the square area
    case 75:  // DiagonalCeilingTopLeft - triangle at origin
    case 76:  // DiagonalCeilingBottomLeft - triangle at origin
    {
      // Smaller outline for triangle - use half the drawn area
      int count = (size & 0x0F) + 2;
      width = count * 8;
      height = count * 8;
      break;
    }
    case 77:  // DiagonalCeilingTopRight - triangle shifts diagonally
    case 78:  // DiagonalCeilingBottomRight - triangle shifts diagonally
    {
      // Smaller outline for diagonal triangles
      int count = (size & 0x0F) + 2;
      width = count * 8;
      height = count * 8;
      break;
    }

    // Special platform routines (79-82)
    case 79:        // ClosedChestPlatform
    case 80:        // MovingWallWest
    case 81:        // MovingWallEast
    case 82:        // OpenChestPlatform
      width = 64;   // 8 tiles wide
      height = 64;  // 8 tiles tall
      break;

    // Stair routines - different sizes for different types

    // 4x4 stair patterns (32x32 pixels)
    case 83:        // InterRoomFatStairsUp (0x12D)
    case 84:        // InterRoomFatStairsDownA (0x12E)
    case 85:        // InterRoomFatStairsDownB (0x12F)
    case 86:        // AutoStairs (0x130-0x133)
    case 87:        // StraightInterroomStairs (0xF9E-0xFA9)
      width = 32;   // 4 tiles
      height = 32;  // 4 tiles (4x4 pattern)
      break;

    // 4x3 stair patterns (32x24 pixels)
    case 88:  // SpiralStairsGoingUpUpper (0x138)
    case 89:  // SpiralStairsGoingDownUpper (0x139)
    case 90:  // SpiralStairsGoingUpLower (0x13A)
    case 91:  // SpiralStairsGoingDownLower (0x13B)
      // ASM: RoomDraw_1x3N_rightwards with A=4 -> 4 columns x 3 rows
      width = 32;   // 4 tiles
      height = 24;  // 3 tiles
      break;

    case 92:  // BigKeyLock
      width = 16;
      height = 24;
      break;

    case 93:  // BombableFloor
      width = 32;
      height = 32;
      break;

    case 94:  // EmptyWaterFace
      width = 32;
      // Base empty-water variant is 4x3; stateful runtime branch can extend to
      // 4x5 when water is active.
      height = 24;
      break;

    case 95:  // SpittingWaterFace
      width = 32;
      height = 40;
      break;

    case 96:  // DrenchingWaterFace
      width = 32;
      height = 56;
      break;

    case 97:        // PrisonCell
      width = 80;   // 10 tiles
      height = 32;  // 4 tiles
      break;

    case 98:  // Bed4x5
      width = 32;
      height = 40;
      break;

    case 99:        // Rightwards3x6
      width = 48;   // 6 tiles
      height = 24;  // 3 tiles
      break;

    case 100:  // Utility6x3
      width = 48;
      height = 24;
      break;

    case 101:  // Utility3x5
      width = 24;
      height = 40;
      break;

    case 102:  // VerticalTurtleRockPipe
      width = 32;
      height = 48;
      break;

    case 103:  // HorizontalTurtleRockPipe
      width = 48;
      height = 32;
      break;

    case 104:      // LightBeam
      width = 32;  // 4 tiles
      height = 80;
      break;

    case 105:  // BigLightBeam
      width = 64;
      height = 64;
      break;

    case 106:  // BossShell4x4
      width = 32;
      height = 32;
      break;

    case 107:  // SolidWallDecor3x4
      width = 24;
      height = 32;
      break;

    case 108:  // ArcheryGameTargetDoor
      width = 24;
      height = 48;
      break;

    case 109:  // GanonTriforceFloorDecor
      width = 64;
      height = 64;
      break;

    case 110:  // Single2x2
      width = 16;
      height = 16;
      break;

    case 111:  // Waterfall47 (object 0x47)
    {
      // ASM: count = (size+1)*2, draws 1x5 columns
      // Width = first column + middle columns + last column = 2 + count tiles
      size = size & 0x0F;
      int count = (size + 1) * 2;
      width = (2 + count) * 8;
      height = 40;  // 5 tiles
      break;
    }
    case 112:  // Waterfall48 (object 0x48)
    {
      // ASM: count = (size+1)*2, draws 1x3 columns
      // Width = first column + middle columns + last column = 2 + count tiles
      size = size & 0x0F;
      int count = (size + 1) * 2;
      width = (2 + count) * 8;
      height = 24;  // 3 tiles
      break;
    }

    case 113:  // Single4x4 (no repetition) - 4x4 TILE16 = 8x8 TILE8
      // ASM RoomDraw_4x4 = 4x4 tile8.
      width = 32;
      height = 32;
      break;

    case 114:  // Single4x3 (no repetition)
      // 4 tiles wide x 3 tiles tall = 32x24 pixels
      width = 32;
      height = 24;
      break;

    case 115:  // RupeeFloor (special pattern)
      // 6 tiles wide (3 columns x 2 tiles) x 8 tiles tall = 48x64 pixels
      width = 48;
      height = 64;
      break;

    case 116:  // Actual4x4 (true 4x4 tile8 pattern, no repetition)
      // 4 tile8s x 4 tile8s = 32x32 pixels
      width = 32;
      height = 32;
      break;

    default:
      // Fallback to naive calculation if not handled
      // Matches DungeonCanvasViewer::DrawRoomObjects logic
      {
        int size_h = (object.size_ & 0x0F);
        int size_v = (object.size_ >> 4) & 0x0F;
        width = (size_h + 1) * 8;
        height = (size_v + 1) * 8;
      }
      break;
  }

  return {width, height};
}

void yaze::zelda3::ObjectDrawer::DrawCustomObject(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    [[maybe_unused]] std::span<const gfx::TileInfo> tiles,
    [[maybe_unused]] const DungeonState* state) {
  // CustomObjectManager should be initialized by DungeonEditorV2 with the
  // project's custom_objects_folder path before any objects are drawn
  auto& manager = CustomObjectManager::Get();

  int subtype = obj.size_ & 0x1F;
  const std::string filename = manager.ResolveFilename(obj.id_, subtype);
  auto result = manager.GetObjectInternal(obj.id_, subtype);
  if (!result.ok()) {
    DrawMissingCustomObjectPlaceholder(bg, obj.x_, obj.y_);
    LOG_DEBUG("ObjectDrawer",
              "Custom object 0x%03X subtype %d (%s) not found: %s", obj.id_,
              subtype, filename.empty() ? "<unmapped>" : filename.c_str(),
              result.status().message().data());
    return;
  }

  auto custom_obj = result.value();
  if (!custom_obj || custom_obj->IsEmpty())
    return;

  int tile_x = obj.x_;
  int tile_y = obj.y_;

  for (const auto& entry : custom_obj->tiles) {
    // entry.tile_data is vhopppcc cccccccc (SNES tilemap word format)
    // Convert to TileInfo and render using WriteTile8 (not SetTileAt which
    // only stores to buffer without rendering)
    gfx::TileInfo tile_info = gfx::WordToTileInfo(entry.tile_data);
    WriteTile8(bg, tile_x + entry.rel_x, tile_y + entry.rel_y, tile_info);
  }
}

void yaze::zelda3::ObjectDrawer::DrawMissingCustomObjectPlaceholder(
    gfx::BackgroundBuffer& bg, int tile_x, int tile_y) {
  if (trace_only_) {
    return;
  }

  auto& bitmap = bg.bitmap();
  if (!bitmap.is_active() || bitmap.width() <= 0 || bitmap.height() <= 0) {
    return;
  }

  auto& pixels = bitmap.mutable_data();
  auto& coverage = bg.mutable_coverage_data();
  auto& priority = bg.mutable_priority_data();

  constexpr int kPlaceholderSizePx = 16;
  constexpr uint8_t kFillColor = 33;
  constexpr uint8_t kAccentColor = 47;

  const int start_x = tile_x * 8;
  const int start_y = tile_y * 8;
  const int width = bitmap.width();
  const int height = bitmap.height();

  for (int py = 0; py < kPlaceholderSizePx; ++py) {
    const int dest_y = start_y + py;
    if (dest_y < 0 || dest_y >= height)
      continue;
    for (int px = 0; px < kPlaceholderSizePx; ++px) {
      const int dest_x = start_x + px;
      if (dest_x < 0 || dest_x >= width)
        continue;
      const bool border =
          (px == 0 || py == 0 || px == kPlaceholderSizePx - 1 ||
           py == kPlaceholderSizePx - 1);
      const bool diagonal =
          (px == py) || (px + py == kPlaceholderSizePx - 1);
      const int dest_index = dest_y * width + dest_x;
      pixels[dest_index] = (border || diagonal) ? kAccentColor : kFillColor;
      if (dest_index < static_cast<int>(coverage.size()))
        coverage[dest_index] = 1;
      if (dest_index < static_cast<int>(priority.size()))
        priority[dest_index] = 0;
    }
  }
  bitmap.set_modified(true);
}

void yaze::zelda3::ObjectDrawer::DrawPotItem(uint8_t item_id, int x, int y,
                                             gfx::BackgroundBuffer& bg) {
  // Draw a small colored indicator for pot items
  // Item types from ZELDA3_DUNGEON_SPEC.md Section 7.2
  // Uses palette indices that map to recognizable colors

  if (item_id == 0)
    return;  // Nothing - skip

  auto& bitmap = bg.bitmap();
  auto& coverage_buffer = bg.mutable_coverage_data();
  if (!bitmap.is_active() || bitmap.width() == 0)
    return;

  // Convert tile coordinates to pixel coordinates
  // Items are drawn offset from pot position (centered on pot)
  int pixel_x = (x * 8) + 2;  // Offset 2 pixels into the pot tile
  int pixel_y = (y * 8) + 2;

  // Choose color based on item category
  // Using palette indices that should be visible in dungeon palettes
  uint8_t color_idx;
  switch (item_id) {
    // Rupees (green/blue/red tones)
    case 1:            // Green rupee
    case 7:            // Blue rupee
    case 12:           // Blue rupee variant
      color_idx = 30;  // Greenish (palette 2, index 0)
      break;

    // Hearts (red tones)
    case 6:   // Heart
    case 11:  // Heart
    case 13:  // Heart variant
      // NOTE: Avoid palette indices 0/16/32/.. which are transparent in SNES
      // CGRAM rows. Using 5 gives a consistently visible indicator.
      color_idx = 5;
      break;

    // Keys (yellow/gold)
    case 8:            // Key*8
    case 19:           // Key
      color_idx = 45;  // Yellowish (palette 3)
      break;

    // Bombs (dark/black)
    case 5:            // Bomb
    case 10:           // 1 bomb
    case 16:           // Bomb refill
      color_idx = 60;  // Darker color (palette 4)
      break;

    // Arrows (brown/wood)
    case 9:            // Arrow
    case 17:           // Arrow refill
      color_idx = 15;  // Brownish (palette 1)
      break;

    // Magic (blue/purple)
    case 14:           // Small magic
    case 15:           // Big magic
      color_idx = 75;  // Bluish (palette 5)
      break;

    // Fairy (pink/light)
    case 18:          // Fairy
    case 20:          // Fairy*8
      color_idx = 5;  // Pinkish
      break;

    // Special/Traps (distinct colors)
    case 2:            // Rock crab
    case 3:            // Bee
      color_idx = 20;  // Enemy indicator
      break;

    case 23:           // Hole
    case 24:           // Warp
    case 25:           // Staircase
      color_idx = 10;  // Transport indicator
      break;

    case 26:           // Bombable
    case 27:           // Switch
      color_idx = 35;  // Interactive indicator
      break;

    case 4:  // Random
    default:
      color_idx = 50;  // Default/random indicator
      break;
  }

  // Safety: never use CGRAM transparent slots (0,16,32,...) for the indicator.
  // In the editor these would appear invisible or "missing" depending on
  // compositing.
  if (color_idx != 255 && (color_idx % 16) == 0) {
    color_idx++;
  }

  // Draw a 4x4 colored square as item indicator
  int bitmap_width = bitmap.width();
  int bitmap_height = bitmap.height();

  for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
      int dest_x = pixel_x + px;
      int dest_y = pixel_y + py;

      // Bounds check
      if (dest_x >= 0 && dest_x < bitmap_width && dest_y >= 0 &&
          dest_y < bitmap_height) {
        int offset = (dest_y * bitmap_width) + dest_x;
        bitmap.WriteToPixel(offset, color_idx);
        if (offset >= 0 && offset < static_cast<int>(coverage_buffer.size())) {
          coverage_buffer[offset] = 1;
        }
      }
    }
  }
}
