#include "object_drawer.h"

#include <cstdio>
#include <filesystem>

#include "absl/strings/str_format.h"
#include "app/gfx/types/snes_tile.h"
#include "rom/rom.h"
#include "core/features.h"
#include "rom/snes.h"
#include "util/log.h"
#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/dungeon/draw_routines/draw_routine_types.h"
#include "zelda3/dungeon/draw_routines/special_routines.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"

namespace yaze {
namespace zelda3 {

ObjectDrawer::ObjectDrawer(Rom* rom, int room_id, const uint8_t* room_gfx_buffer)
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
  if (tile_info.horizontal_mirror_) flags |= 0x1;
  if (tile_info.vertical_mirror_) flags |= 0x2;
  if (tile_info.over_) flags |= 0x4;
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

absl::Status ObjectDrawer::DrawObject(const RoomObject& object,
                                      gfx::BackgroundBuffer& bg1,
                                      gfx::BackgroundBuffer& bg2,
                                      const gfx::PaletteGroup& palette_group,
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

  // Skip objects that don't have tiles loaded
  if (mutable_obj.tiles().empty()) {
    LOG_DEBUG("ObjectDrawer", "Object 0x%03X at (%d,%d) has NO TILES - skipping",
              object.id_, object.x_, object.y_);
    return absl::OkStatus();
  }

  // Check for custom object override first (guarded by feature flag).
  // We check this BEFORE routine lookup to allow overriding vanilla objects.
  int subtype = object.size_ & 0x1F;
  if (core::FeatureFlags::get().kEnableCustomObjects &&
      CustomObjectManager::Get().GetObjectInternal(object.id_, subtype).ok()) {
    // Custom objects default to drawing on the target layer only, unless all_bgs_ is set
    // Mask propagation is difficult without dimensions, so we rely on explicit transparency in the custom object tiles if needed
    
    // Draw to target layer
    SetTraceContext(object, use_bg2 ? RoomObject::LayerType::BG2
                                    : RoomObject::LayerType::BG1);
    DrawCustomObject(object, target_bg, mutable_obj.tiles(), state);

    // If marked for both BGs, draw to the other layer too
    if (object.all_bgs_) {
      auto& other_bg = (object.layer_ == RoomObject::LayerType::BG2 ||
                        object.layer_ == RoomObject::LayerType::BG3) ? bg1 : bg2;
      SetTraceContext(object, (&other_bg == &bg1) ? RoomObject::LayerType::BG1
                                                  : RoomObject::LayerType::BG2);
      DrawCustomObject(object, other_bg, mutable_obj.tiles(), state);
    }
    // return absl::OkStatus();
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

  bool trace_hook_active = false;
  if (trace_collector_) {
    DrawRoutineUtils::SetTraceHook(&ObjectDrawer::TraceHookThunk, this,
                                   trace_only_);
    trace_hook_active = true;
  }

  // Check if this should draw to both BG layers
  // IMPORTANT: BothBG only applies to Layer 0 (main) objects - structural elements.
  // Layer 1 (BG2 overlay) objects should ONLY draw to BG2 so they appear BEHIND
  // Layer 0 content. Layer 2 (priority) objects should ONLY draw to BG1 (on top).
  bool is_both_bg = (object.layer_ == RoomObject::LayerType::BG1) &&
                    (object.all_bgs_ || RoutineDrawsToBothBGs(routine_id));

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
    draw_routines_[routine_id](this, object, target_bg, mutable_obj.tiles(), state);
  }

  if (trace_hook_active) {
    DrawRoutineUtils::ClearTraceHook();
  }

  // BG2 Mask Propagation: ONLY pit/mask objects should mark BG1 as transparent.
  // Regular Layer 1 objects (walls, statues, stairs) should just draw to BG2
  // and let normal compositing handle the layering (BG1 draws on top of BG2).
  //
  // FIX: Previously this marked ALL Layer 1 objects as creating BG1 transparency,
  // which caused walls/statues to incorrectly show "above" the layout.
  // Now we only call MarkBG1Transparent for actual pit/mask objects.
  bool is_pit_or_mask = (object.id_ == 0xA4) ||  // BigHole4x4
                        (object.id_ >= 0x9B && object.id_ <= 0xA6) ||  // Pit edges
                        (object.id_ == 0xFE6) ||  // Type 3 pit
                        (object.id_ == 0xFBE || object.id_ == 0xFBF);  // Layer 2 pit masks

  if (!trace_only_ && is_pit_or_mask &&
      object.layer_ == RoomObject::LayerType::BG2 && !is_both_bg) {
    auto [pixel_width, pixel_height] = CalculateObjectDimensions(object);

    // Log pit/mask transparency propagation
    LOG_DEBUG("ObjectDrawer",
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
    gfx::BackgroundBuffer* layout_bg1) {
  ResetChestIndex();
  absl::Status status = absl::OkStatus();

  // DEBUG: Count objects routed to each buffer
  int to_bg1 = 0, to_bg2 = 0, both_bgs = 0;

  for (const auto& object : objects) {
    // Track buffer routing for summary
    bool use_bg2 = (object.layer_ == RoomObject::LayerType::BG2);
    bool is_layer0 = (object.layer_ == RoomObject::LayerType::BG1);
    int routine_id = GetDrawRoutineId(object.id_);
    bool is_both_bg = is_layer0 && (object.all_bgs_ || RoutineDrawsToBothBGs(routine_id));

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
      LOG_DEBUG("ObjectDrawer", "BG1 Surface too small: surf=%zu buf=%zu", surface_size, buffer_size);
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
      LOG_DEBUG("ObjectDrawer", "BG2 Surface too small: surf=%zu buf=%zu", surface_size, buffer_size);
    }
    SDL_UnlockSurface(bg2_bmp.surface());
  }

  return status;
}

// ============================================================================
// Metadata-based BothBG Detection
// ============================================================================

bool ObjectDrawer::RoutineDrawsToBothBGs(int routine_id) {
  // Use the unified DrawRoutineRegistry for BothBG metadata.
  // This ensures consistency between ObjectDrawer and ObjectGeometry.
  //
  // Fallback to hardcoded list for routines not yet in the registry,
  // particularly layout structural objects that MUST draw to both BG layers.
  
  // First check the unified registry
  if (DrawRoutineRegistry::Get().RoutineDrawsToBothBGs(routine_id)) {
    return true;
  }
  
  // Fallback: Layout structural objects that must draw to both BG layers
  // even if not explicitly marked in registry metadata.
  // These form the room structure (walls, corners, ceilings).
  static constexpr int kBothBGRoutines[] = {
      DrawRoutineIds::kRightwards2x2_1to15or32,     // 0: ceiling
      DrawRoutineIds::kRightwards2x4_1to15or26,     // 1: layout walls
      DrawRoutineIds::kDownwards4x2_1to15or26,      // 8: layout walls
      DrawRoutineIds::kCorner4x4,                   // 19: layout corners
      DrawRoutineIds::kRightwards2x4_1to16_BothBG,  // 3: diagonal walls
      DrawRoutineIds::kDownwards4x2_1to16_BothBG,   // 9: diagonal walls
      DrawRoutineIds::kDiagonalAcute_1to16_BothBG,  // 17: upper-left diagonal
      DrawRoutineIds::kDiagonalGrave_1to16_BothBG,  // 18: upper-right diagonal
      DrawRoutineIds::kCorner4x4_BothBG,            // 35: 4x4 corners
      DrawRoutineIds::kWeirdCornerBottom_BothBG,    // 36: weird corners
      DrawRoutineIds::kWeirdCornerTop_BothBG,       // 37: weird corners
  };

  for (int id : kBothBGRoutines) {
    if (routine_id == id) return true;
  }
  return false;
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

  object_to_routine_map_.clear();
  draw_routines_.clear();

  // Subtype 1 Object Mappings (Horizontal)
  // ASM: Routines $00-$0B from table at $018200
  object_to_routine_map_[0x00] = 0;
  // Objects 0x01-0x02 use RoomDraw_Rightwards2x4_1to15or26 (routine 1)
  // ASM: GetSize_1to15or26 - size defaults to 26 when 0
  for (int id = 0x01; id <= 0x02; id++) {
    object_to_routine_map_[id] = 1;
  }
  // Objects 0x03-0x04 use RoomDraw_Rightwards2x4spaced4_1to16 (routine 2)
  // ASM: GetSize_1to16 - count = size + 1
  for (int id = 0x03; id <= 0x04; id++) {
    object_to_routine_map_[id] = 2;
  }
  for (int id = 0x05; id <= 0x06; id++) {
    object_to_routine_map_[id] = 3;
  }
  for (int id = 0x07; id <= 0x08; id++) {
    object_to_routine_map_[id] = 4;
  }
  object_to_routine_map_[0x09] = 5;
  for (int id = 0x0A; id <= 0x0B; id++) {
    object_to_routine_map_[id] = 6;
  }

  // Diagonal walls (0x0C-0x20) - Verified against bank_01.asm
  // Non-BothBG diagonals: 0x0C-0x14 (matching assembly lines 280-289)
  // Acute Diagonals (/) - NON-BothBG
  for (int id : {0x0C, 0x0D, 0x10, 0x11, 0x14}) {
    object_to_routine_map_[id] = 5;  // DiagonalAcute_1to16 (non-BothBG)
  }
  // Grave Diagonals (\) - NON-BothBG
  for (int id : {0x0E, 0x0F, 0x12, 0x13}) {
    object_to_routine_map_[id] = 6;  // DiagonalGrave_1to16 (non-BothBG)
  }
  // BothBG diagonals start at 0x15 (matching assembly lines 289-300)
  // Acute Diagonals (/) - BothBG
  for (int id : {0x15, 0x18, 0x19, 0x1C, 0x1D, 0x20}) {
    object_to_routine_map_[id] = 17;  // DiagonalAcute_1to16_BothBG
  }
  // Grave Diagonals (\) - BothBG
  for (int id : {0x16, 0x17, 0x1A, 0x1B, 0x1E, 0x1F}) {
    object_to_routine_map_[id] = 18;  // DiagonalGrave_1to16_BothBG
  }

  // Edge and Corner Objects (0x21-0x30) - Verified against bank_01.asm lines 302-317
  object_to_routine_map_[0x21] = 20;  // RoomDraw_Rightwards1x2_1to16_plus2
  object_to_routine_map_[0x22] = 21;  // RoomDraw_RightwardsHasEdge1x1_1to16_plus3
  // 0x23-0x2E all use RoomDraw_RightwardsHasEdge1x1_1to16_plus2
  for (int id = 0x23; id <= 0x2E; id++) {
    object_to_routine_map_[id] = 22;
  }
  object_to_routine_map_[0x2F] = 23;  // RoomDraw_RightwardsTopCorners1x2_1to16_plus13
  object_to_routine_map_[0x30] = 24;  // RoomDraw_RightwardsBottomCorners1x2_1to16_plus13

  // Custom Objects (0x31-0x32) - Oracle of Secrets minecart tracks and furniture
  // USDASM marks these as RoomDraw_Nothing; only map to custom routines when enabled.
  if (core::FeatureFlags::get().kEnableCustomObjects) {
    // These use external binary files instead of ROM tile data.
    object_to_routine_map_[0x31] = DrawRoutineIds::kCustomObject;  // Custom tracks
    object_to_routine_map_[0x32] = DrawRoutineIds::kCustomObject;  // Custom furniture
  } else {
    object_to_routine_map_[0x31] = DrawRoutineIds::kNothing;
    object_to_routine_map_[0x32] = DrawRoutineIds::kNothing;
  }
  object_to_routine_map_[0x33] = 16; // 4x4 Block
  object_to_routine_map_[0x34] = 25; // Solid 1x1
  object_to_routine_map_[0x35] = 26; // Door Switcher
  object_to_routine_map_[0x36] = 27; // Decor 4x4
  object_to_routine_map_[0x37] = 27; // Decor 4x4
  object_to_routine_map_[0x38] = 28; // Statue 2x3
  object_to_routine_map_[0x39] = 29; // Pillar 2x4
  object_to_routine_map_[0x3A] = 30; // Decor 4x3
  object_to_routine_map_[0x3B] = 30; // Decor 4x3
  object_to_routine_map_[0x3C] = 31; // Doubled 2x2
  object_to_routine_map_[0x3D] = 29; // Pillar 2x4
  object_to_routine_map_[0x3E] = 32; // Decor 2x2

  // 0x3F-0x46 all use RoomDraw_RightwardsHasEdge1x1_1to16_plus2 (verified bank_01.asm lines 332-338)
  for (int id = 0x3F; id <= 0x46; id++) {
    object_to_routine_map_[id] = 22;
  }
  // 0x47-0x48 Waterfalls - use dedicated waterfall routines
  // ASM: RoomDraw_Waterfall47 and RoomDraw_Waterfall48
  object_to_routine_map_[0x47] = 111;  // Waterfall47 (1x5 columns)
  object_to_routine_map_[0x48] = 112;  // Waterfall48 (1x3 columns)
  // 0x49-0x4A Floor Tile 4x2 - Verified against bank_01.asm RoomDraw_RightwardsFloorTile4x2_1to16
  object_to_routine_map_[0x49] = 40;  // Rightwards4x2 (4 cols × 2 rows, horizontal spacing)
  object_to_routine_map_[0x4A] = 40;
  // 0x4B Decor 2x2 spaced 12
  object_to_routine_map_[0x4B] = 32;
  // 0x4C: RoomDraw_RightwardsBar4x3_1to16 (bank_01.asm line 345)
  object_to_routine_map_[0x4C] = 52;  // Bar4x3
  // 0x4D-0x4F: RoomDraw_RightwardsShelf4x4_1to16 (bank_01.asm lines 346-348)
  object_to_routine_map_[0x4D] = 53;  // Shelf4x4
  object_to_routine_map_[0x4E] = 53;
  object_to_routine_map_[0x4F] = 53;

  // 0x50-0x5F Objects (verified against bank_01.asm routine table at $018200)
  // 0x50: RoomDraw_RightwardsLine1x1_1to16plus1 (bank_01.asm line 349)
  object_to_routine_map_[0x50] = 51;  // Line1x1_plus1
  // 0x51-0x52: RoomDraw_RightwardsCannonHole4x3_1to16 (18 tiles = 4×3 pattern + edges)
  object_to_routine_map_[0x51] = 42;  // CannonHole4x3
  object_to_routine_map_[0x52] = 42;
  // 0x53: RoomDraw_Rightwards2x2_1to16
  object_to_routine_map_[0x53] = 4;   // 2x2
  // 0x54: RoomDraw_Nothing_B
  object_to_routine_map_[0x54] = 38;  // Nothing
  // 0x55-0x56: RoomDraw_RightwardsDecor4x2spaced8_1to16 (8 tiles, 12-column spacing)
  object_to_routine_map_[0x55] = 41;  // Decor4x2spaced8
  object_to_routine_map_[0x56] = 41;
  // 0x57-0x5A: RoomDraw_Nothing_C
  object_to_routine_map_[0x57] = 38;  // Nothing
  object_to_routine_map_[0x58] = 38;
  object_to_routine_map_[0x59] = 38;
  object_to_routine_map_[0x5A] = 38;
  // 0x5B-0x5C: RoomDraw_RightwardsCannonHole4x3_1to16
  object_to_routine_map_[0x5B] = 42;  // CannonHole4x3
  object_to_routine_map_[0x5C] = 42;
  // 0x5D: RoomDraw_RightwardsBigRail1x3_1to16plus5 (bank_01.asm line 362)
  object_to_routine_map_[0x5D] = 54;  // BigRail1x3_plus5
  // 0x5E: RoomDraw_RightwardsBlock2x2spaced2_1to16 (bank_01.asm line 363)
  object_to_routine_map_[0x5E] = 55;  // Block2x2spaced2
  // 0x5F: RoomDraw_RightwardsHasEdge1x1_1to16_plus23
  object_to_routine_map_[0x5F] = DrawRoutineIds::kRightwardsHasEdge1x1_1to16_plus23;

  // Subtype 1 Object Mappings (Vertical 0x60-0x6F)
  object_to_routine_map_[0x60] = 7;
  for (int id = 0x61; id <= 0x62; id++) {
    object_to_routine_map_[id] = 8;
  }
  for (int id = 0x63; id <= 0x64; id++) {
    object_to_routine_map_[id] = 9;
  }
  for (int id = 0x65; id <= 0x66; id++) {
    object_to_routine_map_[id] = 10;
  }
  for (int id = 0x67; id <= 0x68; id++) {
    object_to_routine_map_[id] = 11;
  }
  object_to_routine_map_[0x69] = 12;
  for (int id = 0x6A; id <= 0x6B; id++) {
    object_to_routine_map_[id] = 13;
  }
  object_to_routine_map_[0x6C] = 14;
  object_to_routine_map_[0x6D] = 15;
  // 0x6E-0x6F: 1 tile each
  object_to_routine_map_[0x6E] = 38;  // Nothing (Logic: RoomDraw_Nothing_A)
  object_to_routine_map_[0x6F] = 38;

  // 0x70-0x7F Objects (verified against bank_01.asm lines 381-396)
  // 0x70: RoomDraw_DownwardsFloor4x4_1to16
  object_to_routine_map_[0x70] = 43;  // DownwardsFloor4x4
  // 0x71: RoomDraw_Downwards1x1Solid_1to16_plus3
  object_to_routine_map_[0x71] = 44;  // Downwards1x1Solid_plus3
  // 0x72: RoomDraw_Nothing_B
  object_to_routine_map_[0x72] = 38;  // Nothing
  // 0x73-0x74: RoomDraw_DownwardsDecor4x4spaced2_1to16
  object_to_routine_map_[0x73] = 45;  // DownwardsDecor4x4spaced2
  object_to_routine_map_[0x74] = 45;
  // 0x75: RoomDraw_DownwardsPillar2x4spaced2_1to16
  object_to_routine_map_[0x75] = 46;  // DownwardsPillar2x4spaced2
  // 0x76-0x77: RoomDraw_DownwardsDecor3x4spaced4_1to16
  object_to_routine_map_[0x76] = 47;  // DownwardsDecor3x4spaced4
  object_to_routine_map_[0x77] = 47;
  // 0x78: RoomDraw_DownwardsDecor2x2spaced12_1to16
  object_to_routine_map_[0x78] = 48;  // DownwardsDecor2x2spaced12
  // 0x79-0x7A: RoomDraw_DownwardsEdge1x1_1to16 (bank_01.asm lines 390-391)
  object_to_routine_map_[0x79] = 13;  // DownwardsEdge1x1
  object_to_routine_map_[0x7A] = 13;
  // 0x7B: RoomDraw_DownwardsDecor2x2spaced12_1to16 (same as 0x78)
  object_to_routine_map_[0x7B] = 48;  // DownwardsDecor2x2spaced12
  // 0x7C: RoomDraw_DownwardsLine1x1_1to16plus1 (bank_01.asm line 393)
  object_to_routine_map_[0x7C] = 49;  // DownwardsLine1x1_plus1
  // 0x7D: RoomDraw_Downwards2x2_1to16 (bank_01.asm line 394)
  object_to_routine_map_[0x7D] = 11;  // Downwards2x2_1to16
  // 0x7E: RoomDraw_Nothing_B
  object_to_routine_map_[0x7E] = 38;  // Nothing
  // 0x7F: RoomDraw_DownwardsDecor2x4spaced8_1to16
  object_to_routine_map_[0x7F] = 50;  // DownwardsDecor2x4spaced8

  // 0x80-0x8F Objects (verified against bank_01.asm lines 397-412)
  // 0x80: RoomDraw_DownwardsDecor2x4spaced8_1to16 (same as 0x7F)
  object_to_routine_map_[0x80] = 50;  // DownwardsDecor2x4spaced8
  // 0x81-0x84: RoomDraw_DownwardsDecor3x4spaced2_1to16 (12 tiles = 3x4)
  object_to_routine_map_[0x81] = 65;  // DownwardsDecor3x4spaced2_1to16
  object_to_routine_map_[0x82] = 65;
  object_to_routine_map_[0x83] = 65;
  object_to_routine_map_[0x84] = 65;
  // 0x85-0x86: RoomDraw_DownwardsCannonHole3x6_1to16 (18 tiles = 3x6)
  object_to_routine_map_[0x85] = 68;  // DownwardsCannonHole3x6_1to16
  object_to_routine_map_[0x86] = 68;
  // 0x87: RoomDraw_DownwardsPillar2x4spaced2_1to16 (same as 0x75)
  object_to_routine_map_[0x87] = 46;  // DownwardsPillar2x4spaced2
  // 0x88: RoomDraw_DownwardsBigRail3x1_1to16plus5 (12 tiles)
  object_to_routine_map_[0x88] = 66;  // DownwardsBigRail3x1_1to16plus5
  // 0x89: RoomDraw_DownwardsBlock2x2spaced2_1to16 (4 tiles)
  object_to_routine_map_[0x89] = 67;  // DownwardsBlock2x2spaced2_1to16
  // 0x8A-0x8C: Vertical rails with CORNER+MIDDLE+END pattern (3 tiles each)
  // ASM: RoomDraw_DownwardsHasEdge1x1_1to16_plus23 - matches horizontal 0x22
  object_to_routine_map_[0x8A] = DrawRoutineIds::kDownwardsHasEdge1x1_1to16_plus23;
  object_to_routine_map_[0x8B] = DrawRoutineIds::kDownwardsHasEdge1x1_1to16_plus23;
  object_to_routine_map_[0x8C] = DrawRoutineIds::kDownwardsHasEdge1x1_1to16_plus23;
  // 0x8D-0x8E: RoomDraw_DownwardsEdge1x1_1to16
  object_to_routine_map_[0x8D] = 13;  // DownwardsEdge1x1_1to16
  object_to_routine_map_[0x8E] = 13;
  // 0x8F: RoomDraw_DownwardsBar2x3_1to16 (6 tiles = 2x3)
  object_to_routine_map_[0x8F] = 69;  // DownwardsBar2x3_1to16

  // 0x90-0x9F Objects (based on tile counts from kSubtype1TileLengths)
  // 0x90-0x91: 8 tiles each
  object_to_routine_map_[0x90] = 8;   // 4x2
  object_to_routine_map_[0x91] = 8;
  // 0x92-0x93: RoomDraw_Downwards2x2_1to15or32
  object_to_routine_map_[0x92] = 7;   // Downwards2x2_1to15or32
  object_to_routine_map_[0x93] = 7;
  // 0x94: RoomDraw_DownwardsFloor4x4_1to16
  object_to_routine_map_[0x94] = 43;  // DownwardsFloor4x4_1to16
  // 0x95: RoomDraw_DownwardsPots2x2_1to16 (4 tiles = 2x2 interactive)
  object_to_routine_map_[0x95] = 70;  // DownwardsPots2x2_1to16
  // 0x96: RoomDraw_DownwardsHammerPegs2x2_1to16 (4 tiles = 2x2 interactive)
  object_to_routine_map_[0x96] = 71;  // DownwardsHammerPegs2x2_1to16
  // 0x97-0x9F: 1 tile each
  for (int id = 0x97; id <= 0x9F; id++) {
    object_to_routine_map_[id] = 38;  // Nothing (Logic: RoomDraw_Nothing_B)
  }

  // 0xA0-0xAF Objects (diagonal ceilings and big hole)
  // Phase 4 Step 3: Diagonal ceiling routines implemented
  // DiagonalCeilingTopLeft: 0xA0, 0xA5, 0xA9
  object_to_routine_map_[0xA0] = 75;  // DiagonalCeilingTopLeft (A variant)
  object_to_routine_map_[0xA5] = 75;  // DiagonalCeilingTopLeft (B variant)
  object_to_routine_map_[0xA9] = 75;  // DiagonalCeilingTopLeft (B variant)
  // DiagonalCeilingBottomLeft: 0xA1, 0xA6, 0xAA
  object_to_routine_map_[0xA1] = 76;  // DiagonalCeilingBottomLeft (A variant)
  object_to_routine_map_[0xA6] = 76;  // DiagonalCeilingBottomLeft (B variant)
  object_to_routine_map_[0xAA] = 76;  // DiagonalCeilingBottomLeft (B variant)
  // DiagonalCeilingTopRight: 0xA2, 0xA7, 0xAB
  object_to_routine_map_[0xA2] = 77;  // DiagonalCeilingTopRight (A variant)
  object_to_routine_map_[0xA7] = 77;  // DiagonalCeilingTopRight (B variant)
  object_to_routine_map_[0xAB] = 77;  // DiagonalCeilingTopRight (B variant)
  // DiagonalCeilingBottomRight: 0xA3, 0xA8, 0xAC
  object_to_routine_map_[0xA3] = 78;  // DiagonalCeilingBottomRight (A variant)
  object_to_routine_map_[0xA8] = 78;  // DiagonalCeilingBottomRight (B variant)
  object_to_routine_map_[0xAC] = 78;  // DiagonalCeilingBottomRight (B variant)
  // BigHole4x4: 0xA4
  object_to_routine_map_[0xA4] = 61;  // BigHole4x4_1to16
  // Nothing: 0xAD-0xAF
  object_to_routine_map_[0xAD] = 38;  // Nothing (RoomDraw_Nothing_B)
  object_to_routine_map_[0xAE] = 38;  // Nothing (RoomDraw_Nothing_B)
  object_to_routine_map_[0xAF] = 38;  // Nothing (RoomDraw_Nothing_B)

  // 0xB0-0xBF Objects (based on tile counts from kSubtype1TileLengths)
  // 0xB0-0xB1: RoomDraw_RightwardsEdge1x1_1to16plus7 (1 tile with +7 offset)
  object_to_routine_map_[0xB0] = 72;  // RightwardsEdge1x1_1to16plus7
  object_to_routine_map_[0xB1] = 72;
  // 0xB2: 16 tiles
  object_to_routine_map_[0xB2] = 16;  // 4x4
  // 0xB3-0xB4: 3 tiles each
  object_to_routine_map_[0xB3] = 22;  // edge 1x1
  object_to_routine_map_[0xB4] = 22;
  // 0xB5: Weird2x4_1to16
  object_to_routine_map_[0xB5] = 8;   // 4x2
  // 0xB6-0xB7: RoomDraw_Rightwards2x4_1to15or26
  object_to_routine_map_[0xB6] = 1;   // Rightwards2x4_1to15or26
  object_to_routine_map_[0xB7] = 1;
  // 0xB8-0xB9: RoomDraw_Rightwards2x2_1to15or32
  object_to_routine_map_[0xB8] = 0;   // Rightwards2x2_1to15or32
  object_to_routine_map_[0xB9] = 0;
  // 0xBA: 16 tiles
  object_to_routine_map_[0xBA] = 16;  // 4x4
  // 0xBB: RoomDraw_RightwardsBlock2x2spaced2_1to16
  object_to_routine_map_[0xBB] = 55;  // RightwardsBlock2x2spaced2_1to16
  // 0xBC: RoomDraw_RightwardsPots2x2_1to16 (4 tiles = 2x2 interactive)
  object_to_routine_map_[0xBC] = 73;  // RightwardsPots2x2_1to16
  // 0xBD: RoomDraw_RightwardsHammerPegs2x2_1to16 (4 tiles = 2x2 interactive)
  object_to_routine_map_[0xBD] = 74;  // RightwardsHammerPegs2x2_1to16
  // 0xBE-0xBF: 1 tile each
  object_to_routine_map_[0xBE] = 38;  // Nothing (Logic: RoomDraw_Nothing_B)
  object_to_routine_map_[0xBF] = 38;

  // 0xC0-0xCF Objects (Phase 4: SuperSquare mappings)
  // 0xC0: RoomDraw_4x4BlocksIn4x4SuperSquare
  object_to_routine_map_[0xC0] = 56;  // 4x4BlocksIn4x4SuperSquare
  // 0xC1: ClosedChestPlatform (68 tiles)
  object_to_routine_map_[0xC1] = 79;  // ClosedChestPlatform
  // 0xC2: RoomDraw_4x4BlocksIn4x4SuperSquare
  object_to_routine_map_[0xC2] = 56;  // 4x4BlocksIn4x4SuperSquare
  // 0xC3: RoomDraw_3x3FloorIn4x4SuperSquare
  object_to_routine_map_[0xC3] = 57;  // 3x3FloorIn4x4SuperSquare
  // 0xC4: RoomDraw_4x4FloorOneIn4x4SuperSquare
  object_to_routine_map_[0xC4] = 59;  // 4x4FloorOneIn4x4SuperSquare
  // 0xC5-0xCA: RoomDraw_4x4FloorIn4x4SuperSquare
  for (int id = 0xC5; id <= 0xCA; id++) {
    object_to_routine_map_[id] = 58;  // 4x4FloorIn4x4SuperSquare
  }
  // 0xCB-0xCC: Nothing (RoomDraw_Nothing_E)
  object_to_routine_map_[0xCB] = 38;
  object_to_routine_map_[0xCC] = 38;
  // 0xCD-0xCE: Moving Walls
  object_to_routine_map_[0xCD] = 80;  // MovingWallWest
  object_to_routine_map_[0xCE] = 81;  // MovingWallEast
  // 0xCF: Nothing (RoomDraw_Nothing_D)
  object_to_routine_map_[0xCF] = 38;

  // 0xD0-0xDF Objects (Phase 4: SuperSquare mappings)
  // 0xD0: Nothing (RoomDraw_Nothing_D)
  object_to_routine_map_[0xD0] = 38;
  // 0xD1-0xD2: RoomDraw_4x4FloorIn4x4SuperSquare
  object_to_routine_map_[0xD1] = 58;  // 4x4FloorIn4x4SuperSquare
  object_to_routine_map_[0xD2] = 58;
  // 0xD3-0xD6: CheckIfWallIsMoved (Logic-only, no tiles)
  object_to_routine_map_[0xD3] = 38;  // Nothing
  object_to_routine_map_[0xD4] = 38;
  object_to_routine_map_[0xD5] = 38;
  object_to_routine_map_[0xD6] = 38;
  // 0xD7: RoomDraw_3x3FloorIn4x4SuperSquare
  object_to_routine_map_[0xD7] = 57;  // 3x3FloorIn4x4SuperSquare
  // 0xD8: RoomDraw_WaterOverlayA8x8_1to16
  object_to_routine_map_[0xD8] = 64;  // WaterOverlay8x8_1to16
  // 0xD9: RoomDraw_4x4FloorIn4x4SuperSquare
  object_to_routine_map_[0xD9] = 58;  // 4x4FloorIn4x4SuperSquare
  // 0xDA: RoomDraw_WaterOverlayB8x8_1to16
  object_to_routine_map_[0xDA] = 64;  // WaterOverlay8x8_1to16
  // 0xDB: RoomDraw_4x4FloorTwoIn4x4SuperSquare
  object_to_routine_map_[0xDB] = 60;  // 4x4FloorTwoIn4x4SuperSquare
  // 0xDC: OpenChestPlatform (21 tiles)
  object_to_routine_map_[0xDC] = 82;  // OpenChestPlatform
  // 0xDD: RoomDraw_TableRock4x4_1to16
  object_to_routine_map_[0xDD] = 63;  // TableRock4x4_1to16
  // 0xDE: RoomDraw_Spike2x2In4x4SuperSquare
  object_to_routine_map_[0xDE] = 62;  // Spike2x2In4x4SuperSquare
  // 0xDF: RoomDraw_4x4FloorIn4x4SuperSquare
  object_to_routine_map_[0xDF] = 58;  // 4x4FloorIn4x4SuperSquare

  // 0xE0-0xEF Objects (Phase 4: SuperSquare mappings)
  // 0xE0-0xE8: RoomDraw_4x4FloorIn4x4SuperSquare
  for (int id = 0xE0; id <= 0xE8; id++) {
    object_to_routine_map_[id] = 58;  // 4x4FloorIn4x4SuperSquare
  }
  // 0xE9-0xEF: 1 tile each - Nothing (RoomDraw_Nothing_B)
  for (int id = 0xE9; id <= 0xEF; id++) {
    object_to_routine_map_[id] = 38;
  }

  // 0xF0-0xF7 Objects (all 1 tile) - Nothing (RoomDraw_Nothing_B)
  for (int id = 0xF0; id <= 0xF7; id++) {
    object_to_routine_map_[id] = 38;
  }
  // 0xF9-0xFD: Type 1 chests (small/big/map/compass)
  for (int id = 0xF9; id <= 0xFD; id++) {
    object_to_routine_map_[id] = 39;  // Chest draw routine
  }

  // Subtype 2 Object Mappings (0x100-0x13F)
  // ASM Reference: bank_01.asm .type1_subtype_2_routine ($018470)
  // 0x100-0x107: RoomDraw_4x4
  for (int id = 0x100; id <= 0x107; id++) {
    object_to_routine_map_[id] = 16;  // Rightwards 4x4
  }
  for (int id = 0x108; id <= 0x10F; id++) {
    object_to_routine_map_[id] = 35;  // 4x4 Corner BothBG
  }
  for (int id = 0x110; id <= 0x113; id++) {
    object_to_routine_map_[id] = 36; // Weird Corner Bottom
  }
  for (int id = 0x114; id <= 0x117; id++) {
    object_to_routine_map_[id] = 37; // Weird Corner Top
  }
  // 0x118-0x11B Rightwards 2x2
  for (int id = 0x118; id <= 0x11B; id++) {
    object_to_routine_map_[id] = 4; // Rightwards 2x2
  }
  // 0x11C 4x4
  object_to_routine_map_[0x11C] = 16;
  // 0x11D Single 2x3 Pillar -> Map to 2x3 Statue (28)
  object_to_routine_map_[0x11D] = 28;
  // 0x11E Single 2x2 -> Map to 2x2 (4)
  object_to_routine_map_[0x11E] = 4;
  // 0x11F Star Switch -> Map to 1x1 (25)
  object_to_routine_map_[0x11F] = 25;
  // 0x120 Torch -> Map to 1x1 (25)
  object_to_routine_map_[0x120] = 25;
  // 0x121 Single 2x3 Pillar -> Map to 2x3 Statue (28)
  object_to_routine_map_[0x121] = 28;
  // 0x122 Bed 4x5 -> Use dedicated Bed4x5 routine
  object_to_routine_map_[0x122] = 98;
  // 0x123 Table 4x3 -> Map to 4x3 (30)
  object_to_routine_map_[0x123] = 30;
  // 0x124-0x125 4x4
  object_to_routine_map_[0x124] = 16;
  object_to_routine_map_[0x125] = 16;
  // 0x126 Single 2x3 -> Map to 2x3 (28)
  object_to_routine_map_[0x126] = 28;
  // 0x127 Rightwards 2x2 -> Map to 2x2 (4)
  object_to_routine_map_[0x127] = 4;
  // 0x128 Bed 4x5 -> Use dedicated Bed4x5 routine
  object_to_routine_map_[0x128] = 98;
  // 0x129 4x4
  object_to_routine_map_[0x129] = 16;
  // 0x12A Mario -> Map to 2x2 (4)
  object_to_routine_map_[0x12A] = 4;
  // 0x12B Rightwards 2x2 -> Map to 2x2 (4)
  object_to_routine_map_[0x12B] = 4;
  // 0x12C DrawRightwards3x6 -> Use dedicated 3x6 routine
  object_to_routine_map_[0x12C] = 99;
  // 0x12D-0x12F InterRoom Fat Stairs (ASM: $01A41B, $01A458, $01A486)
  object_to_routine_map_[0x12D] = 83;  // InterRoomFatStairsUp
  object_to_routine_map_[0x12E] = 84;  // InterRoomFatStairsDownA
  object_to_routine_map_[0x12F] = 85;  // InterRoomFatStairsDownB
  // 0x130-0x133 Auto Stairs (ASM: RoomDraw_AutoStairs*)
  for (int id = 0x130; id <= 0x133; id++) {
    object_to_routine_map_[id] = 86;  // AutoStairs
  }
  // 0x134 Rightwards 2x2 -> Map to 2x2 (4)
  object_to_routine_map_[0x134] = 4;
  // 0x135-0x136 Water Hop Stairs -> Map to 4x4 (16)
  object_to_routine_map_[0x135] = 16;
  object_to_routine_map_[0x136] = 16;
  // 0x137 Dam Flood Gate -> Map to 4x4 (16)
  object_to_routine_map_[0x137] = 16;
  // 0x138-0x13B Spiral Stairs (ASM: RoomDraw_SpiralStairs*)
  object_to_routine_map_[0x138] = 88;  // SpiralStairsGoingUpUpper
  object_to_routine_map_[0x139] = 89;  // SpiralStairsGoingDownUpper
  object_to_routine_map_[0x13A] = 90;  // SpiralStairsGoingUpLower
  object_to_routine_map_[0x13B] = 91;  // SpiralStairsGoingDownLower
  // 0x13C Sanctuary Wall -> Map to 4x4 (16)
  object_to_routine_map_[0x13C] = 16;
  // 0x13D Table 4x3 -> Map to 4x3 (30)
  object_to_routine_map_[0x13D] = 30;
  // 0x13E Utility 6x3 -> Use dedicated 6x3 routine
  object_to_routine_map_[0x13E] = 100;
  // 0x13F Magic Bat Altar -> Map to 4x4 (16)
  object_to_routine_map_[0x13F] = 16;

  // NOTE: Type 2 objects only range from 0x100-0x13F (64 objects)
  // Objects 0x140-0x1FF do NOT exist in ROM - removed fake mappings

  // Subtype 3 Object Mappings (0xF80-0xFFF)
  // Type 3 object IDs are 0xF80-0xFFF (128 objects)
  // Index = (object_id - 0xF80) & 0x7F maps to table indices 0-127
  // Water Face variants (ASM: 0x200-0x202)
  object_to_routine_map_[0xF80] = 94;  // EmptyWaterFace
  object_to_routine_map_[0xF81] = 95;  // SpittingWaterFace
  object_to_routine_map_[0xF82] = 96;  // DrenchingWaterFace
  // Somaria Line (0x203-0x20C)
  for (int id = 0xF83; id <= 0xF89; id++) {
    object_to_routine_map_[id] = 33;  // Somaria Line
  }
  for (int id = 0xF8A; id <= 0xF8C; id++) {
    object_to_routine_map_[id] = 33;
  }
  // 0xF8D (0x20D): PrisonCell - dual-BG drawing with horizontal flip symmetry
  object_to_routine_map_[0xF8D] = 97;  // Prison cell routine
  object_to_routine_map_[0xF8E] = 33;
  object_to_routine_map_[0xF8F] = 33;
  // 0xF90-0xF93 (ASM 210-213): SINGLE 2x2 per bank_01.asm
  // Note: 0xF92 (ASM 212) is RoomDraw_RupeeFloor - special 6x8 pattern
  object_to_routine_map_[0xF90] = 110;  // Single2x2
  object_to_routine_map_[0xF91] = 110;  // Single2x2
  object_to_routine_map_[0xF92] = 115;  // RupeeFloor (special 6x8 pattern)
  object_to_routine_map_[0xF93] = 110;  // Single2x2
  // 0xF94 (ASM 214): RoomDraw_TableRock4x3
  object_to_routine_map_[0xF94] = 30; // TableRock4x3 (Decor 4x3)
  // 0xF95 (ASM 215): RoomDraw_KholdstareShell - boss shell routine
  object_to_routine_map_[0xF95] = 106;
  // 0xF96 (ASM 216): RoomDraw_HammerPegSingle - single peg, use 1x1
  object_to_routine_map_[0xF96] = 25;
  // 0xF97 (0x217): PrisonCell variant - dual-BG drawing
  object_to_routine_map_[0xF97] = 97;  // Prison cell routine
  // 0xF98 (ASM 218): RoomDraw_BigKeyLock - uses dedicated routine
  object_to_routine_map_[0xF98] = 92;  // BigKeyLock
  // 0xF99 (ASM 219): RoomDraw_Chest
  object_to_routine_map_[0xF99] = 39;  // DrawChest
  // 0xF9A (ASM 21A): RoomDraw_OpenChest
  object_to_routine_map_[0xF9A] = 39;  // DrawChest (OpenChest variant)
  // 0xF9B-0xF9D (ASM 21B-21D): AutoStairsSouthMultiLayer A/B/C
  // These are stair objects that use 4x4 base patterns
  for (int id = 0xF9B; id <= 0xF9D; id++) {
    object_to_routine_map_[id] = 86;  // AutoStairs (shared routine)
  }
  // 0xF9E-0xFA1 (ASM 21E-221): StraightInterroomStairs Upper variants
  // These are stair objects that use the straight interroom stairs routine
  for (int id = 0xF9E; id <= 0xFA1; id++) {
    object_to_routine_map_[id] = 87;  // StraightInterroomStairs
  }
  // 0xFA2-0xFA5 (ASM 222-225): Rightwards2x2 - SINGLE 2x2 (no repetition)
  for (int id = 0xFA2; id <= 0xFA5; id++) {
    object_to_routine_map_[id] = 110;  // Single2x2
  }
  // 0xFA6-0xFA9 (ASM 226-229): StraightInterroomStairs Lower variants
  for (int id = 0xFA6; id <= 0xFA9; id++) {
    object_to_routine_map_[id] = 87;  // StraightInterroomStairs
  }
  // 0xFAA (22A): RoomDraw_LampCones - use 4x4 base
  object_to_routine_map_[0xFAA] = 16;
  // 0xFAB (22B): RoomDraw_WeirdGloveRequiredPot - SINGLE 2x2 pot (no repetition)
  object_to_routine_map_[0xFAB] = 110;
  // 0xFAC (22C): RoomDraw_BigGrayRock - SINGLE 2x2 rock (no repetition)
  object_to_routine_map_[0xFAC] = 110;
  // 0xFAD (22D): RoomDraw_AgahnimsAltar - 4x4
  object_to_routine_map_[0xFAD] = 16;
  // 0xFAE (22E): RoomDraw_AgahnimsWindows - 4x4
  object_to_routine_map_[0xFAE] = 16;
  // 0xFAF (22F): RoomDraw_SinglePot - SINGLE 2x2 (no repetition)
  object_to_routine_map_[0xFAF] = 110;
  // 0xFB0 (230): RoomDraw_WeirdUglyPot - SINGLE 2x2 (no repetition)
  object_to_routine_map_[0xFB0] = 110;
  // 0xFB1 (231): RoomDraw_BigChest - SINGLE 4x3 (no repetition)
  object_to_routine_map_[0xFB1] = 114;  // Single4x3
  // 0xFB2 (232): RoomDraw_OpenBigChest - SINGLE 4x3 (no repetition)
  object_to_routine_map_[0xFB2] = 114;  // Single4x3
  // 0xFB3 (233): RoomDraw_AutoStairsSouthMergedLayer - shared AutoStairs routine
  object_to_routine_map_[0xFB3] = 86;
  // 0xFB4-0xFB9 (234-239): ChestPlatformVerticalWall and DrawRightwards3x6 variants
  for (int id = 0xFB4; id <= 0xFB9; id++) {
    object_to_routine_map_[id] = 16;  // Use 4x4 as fallback
  }
  // 0xFBA-0xFBB (23A-23B): VerticalTurtleRockPipe
  object_to_routine_map_[0xFBA] = 102;  // Vertical Turtle Rock pipe
  object_to_routine_map_[0xFBB] = 102;
  // 0xFBC-0xFBD (23C-23D): HorizontalTurtleRockPipe
  object_to_routine_map_[0xFBC] = 103;  // Horizontal Turtle Rock pipe
  object_to_routine_map_[0xFBD] = 103;
  // 0xFBE-0xFC6 (23E-246): Various SINGLE 2x2 objects (no repetition)
  for (int id = 0xFBE; id <= 0xFC6; id++) {
    object_to_routine_map_[id] = 110;  // Single2x2
  }
  // 0xFC7 (247): RoomDraw_BombableFloor
  object_to_routine_map_[0xFC7] = 93;  // BombableFloor
  // 0xFC8 (248): RoomDraw_4x4
  object_to_routine_map_[0xFC8] = 16;
  // 0xFC9-0xFCA (249-24A): SINGLE 2x2 (no repetition)
  object_to_routine_map_[0xFC9] = 110;
  object_to_routine_map_[0xFCA] = 110;
  // 0xFCB (24B): RoomDraw_BigWallDecor - 4x4
  object_to_routine_map_[0xFCB] = 16;
  // 0xFCC (24C): RoomDraw_SmithyFurnace - 4x4
  object_to_routine_map_[0xFCC] = 16;
  // 0xFCD (24D): RoomDraw_Utility6x3 - use dedicated 6x3 routine
  object_to_routine_map_[0xFCD] = 100;
  // 0xFCE (24E): RoomDraw_TableRock4x3
  object_to_routine_map_[0xFCE] = 30;
  // 0xFCF-0xFD3 (24F-253): SINGLE 2x2 (no repetition)
  for (int id = 0xFCF; id <= 0xFD3; id++) {
    object_to_routine_map_[id] = 110;
  }
  // 0xFD4 (254): RoomDraw_FortuneTellerRoom - complex, use 4x4
  object_to_routine_map_[0xFD4] = 16;
  // 0xFD5 (255): RoomDraw_Utility3x5 - use dedicated 3x5 routine
  object_to_routine_map_[0xFD5] = 101;
  // 0xFD6-0xFDA (256-25A): Various SINGLE 2x2 (no repetition)
  for (int id = 0xFD6; id <= 0xFDA; id++) {
    object_to_routine_map_[id] = 110;
  }
  // 0xFDB (25B): RoomDraw_Utility3x5 - use dedicated 3x5 routine
  object_to_routine_map_[0xFDB] = 101;
  // 0xFDC (25C): HorizontalTurtleRockPipe
  object_to_routine_map_[0xFDC] = 103;
  // 0xFDD (25D): RoomDraw_Utility6x3 - use dedicated 6x3 routine
  object_to_routine_map_[0xFDD] = 100;
  // 0xFDE-0xFDF (25E-25F): SINGLE 2x2 (no repetition)
  object_to_routine_map_[0xFDE] = 110;
  object_to_routine_map_[0xFDF] = 110;
  // 0xFE0-0xFE1 (260-261): ArcheryGameTargetDoor - 3x6 door pattern
  object_to_routine_map_[0xFE0] = 108;
  object_to_routine_map_[0xFE1] = 108;
  // 0xFE2 (262): VitreousGooGraphics - 4x4
  object_to_routine_map_[0xFE2] = 16;
  // 0xFE3-0xFE5 (263-265): SINGLE 2x2 (no repetition)
  for (int id = 0xFE3; id <= 0xFE5; id++) {
    object_to_routine_map_[id] = 110;
  }
  // 0xFE6 (266): RoomDraw_4x4 - actual 4x4 tile8 pattern (32x32 pixels)
  object_to_routine_map_[0xFE6] = 116;  // DrawActual4x4
  // 0xFE7-0xFE8 (267-268): TableRock4x3
  object_to_routine_map_[0xFE7] = 30;
  object_to_routine_map_[0xFE8] = 30;
  // 0xFE9-0xFEA (269-26A): SolidWallDecor3x4
  object_to_routine_map_[0xFE9] = 107;
  object_to_routine_map_[0xFEA] = 107;
  // 0xFEB (26B): RoomDraw_4x4 - SINGLE 4x4 (no repetition)
  object_to_routine_map_[0xFEB] = 113;  // Single4x4
  // 0xFEC-0xFED (26C-26D): RoomDraw_TableRock4x3 - SINGLE 4x3 (no repetition)
  object_to_routine_map_[0xFEC] = 114;  // Single4x3
  object_to_routine_map_[0xFED] = 114;  // Single4x3
  // 0xFEE-0xFEF (26E-26F): SolidWallDecor3x4
  object_to_routine_map_[0xFEE] = 107;
  object_to_routine_map_[0xFEF] = 107;
  // 0xFF0 (270): LightBeamOnFloor - dedicated light beam routine
  object_to_routine_map_[0xFF0] = 104;
  // 0xFF1 (271): BigLightBeamOnFloor - dedicated big light beam routine
  object_to_routine_map_[0xFF1] = 105;
  // 0xFF2 (272): TrinexxShell - boss shell routine
  object_to_routine_map_[0xFF2] = 106;
  // 0xFF3 (273): BG2MaskFull - Nothing
  object_to_routine_map_[0xFF3] = 38;
  // 0xFF4 (274): FloorLight - 4x4
  object_to_routine_map_[0xFF4] = 16;
  // 0xFF5 (275): SINGLE 2x2 (no repetition)
  object_to_routine_map_[0xFF5] = 110;
  // 0xFF6-0xFF7 (276-277): BigWallDecor - 4x4
  object_to_routine_map_[0xFF6] = 16;
  object_to_routine_map_[0xFF7] = 16;
  // 0xFF8 (278): GanonTriforceFloorDecor - 4x8 Triforce floor pattern
  object_to_routine_map_[0xFF8] = 109;
  // 0xFF9 (279): TableRock4x3
  object_to_routine_map_[0xFF9] = 30;
  // 0xFFA (27A): RoomDraw_4x4
  object_to_routine_map_[0xFFA] = 16;
  // 0xFFB (27B): VitreousGooDamage - boss shell routine
  object_to_routine_map_[0xFFB] = 106;
  // 0xFFC-0xFFE (27C-27E): SINGLE 2x2 (no repetition)
  for (int id = 0xFFC; id <= 0xFFE; id++) {
    object_to_routine_map_[id] = 110;
  }
  // 0xFFF (27F): Nothing
  object_to_routine_map_[0xFFF] = 38;

  // Initialize draw routine function array in the correct order
  // Routines 0-82 (existing), 80-98 (new special routines for stairs, locks, etc.)
  draw_routines_.reserve(100);

  // Routine 0
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawRightwards2x2_1to15or32(obj, bg, tiles);
  });
  // Routine 1
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawRightwards2x4_1to15or26(obj, bg, tiles);
  });
  // Routine 2 - 2x4 tiles with adjacent spacing (s * 2), count = size + 1
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawRightwards2x4_1to16(obj, bg, tiles);
  });
  // Routine 3 - Same as routine 2 but draws to both BG1 and BG2
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawRightwards2x4_1to16_BothBG(obj, bg, tiles);
  });
  // Routine 4
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawRightwards2x2_1to16(obj, bg, tiles);
  });
  // Routine 5
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawDiagonalAcute_1to16(obj, bg, tiles);
  });
  // Routine 6
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawDiagonalGrave_1to16(obj, bg, tiles);
  });
  // Routine 7
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawDownwards2x2_1to15or32(obj, bg, tiles);
  });
  // Routine 8
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawDownwards4x2_1to15or26(obj, bg, tiles);
  });
  // Routine 9
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawDownwards4x2_1to16_BothBG(obj, bg, tiles);
  });
  // Routine 10
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawDownwardsDecor4x2spaced4_1to16(obj, bg, tiles);
  });
  // Routine 11
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawDownwards2x2_1to16(obj, bg, tiles);
  });
  // Routine 12
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawDownwardsHasEdge1x1_1to16_plus3(obj, bg, tiles);
  });
  // Routine 13
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawDownwardsEdge1x1_1to16(obj, bg, tiles);
  });
  // Routine 14
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawDownwardsLeftCorners2x1_1to16_plus12(obj, bg, tiles);
  });
  // Routine 15
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawDownwardsRightCorners2x1_1to16_plus12(obj, bg, tiles);
  });
  // Routine 16
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawRightwards4x4_1to16(obj, bg, tiles);
  });
  // Routine 17 - Diagonal Acute BothBG
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawDiagonalAcute_1to16_BothBG(obj, bg, tiles);
  });
  // Routine 18 - Diagonal Grave BothBG
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawDiagonalGrave_1to16_BothBG(obj, bg, tiles);
  });
  // Routine 19 - 4x4 Corner (Type 2 corners)
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawCorner4x4(obj, bg, tiles);
  });

  // Routine 20 - Edge objects 1x2 +2
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawRightwards1x2_1to16_plus2(obj, bg, tiles);
  });
  // Routine 21 - Edge with perimeter 1x1 +3
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawRightwardsHasEdge1x1_1to16_plus3(obj, bg, tiles);
  });
  // Routine 22 - Edge variant 1x1 +2
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawRightwardsHasEdge1x1_1to16_plus2(obj, bg, tiles);
  });
  // Routine 23 - Top corners 1x2 +13
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawRightwardsTopCorners1x2_1to16_plus13(obj, bg, tiles);
  });
  // Routine 24 - Bottom corners 1x2 +13
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawRightwardsBottomCorners1x2_1to16_plus13(obj, bg, tiles);
  });
  // Routine 25 - Solid fill 1x1 +3 (floor patterns)
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawRightwards1x1Solid_1to16_plus3(obj, bg, tiles);
  });
  // Routine 26 - Door switcherer
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawDoorSwitcherer(obj, bg, tiles);
  });
  // Routine 27 - Decorations 4x4 spaced 2
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawRightwardsDecor4x4spaced2_1to16(obj, bg, tiles);
  });
  // Routine 28 - Statues 2x3 spaced 2
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawRightwardsStatue2x3spaced2_1to16(obj, bg, tiles);
  });
  // Routine 29 - Pillars 2x4 spaced 4
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawRightwardsPillar2x4spaced4_1to16(obj, bg, tiles);
  });
  // Routine 30 - Decorations 4x3 spaced 4
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawRightwardsDecor4x3spaced4_1to16(obj, bg, tiles);
  });
  // Routine 31 - Doubled 2x2 spaced 2
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawRightwardsDoubled2x2spaced2_1to16(obj, bg, tiles);
  });
  // Routine 32 - Decorations 2x2 spaced 12
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawRightwardsDecor2x2spaced12_1to16(obj, bg, tiles);
  });
  // Routine 33 - Somaria Line
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawSomariaLine(obj, bg, tiles);
  });
  // Routine 34 - Water Face
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawWaterFace(obj, bg, tiles);
  });
  // Routine 35 - 4x4 Corner BothBG
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->Draw4x4Corner_BothBG(obj, bg, tiles);
  });
  // Routine 36 - Weird Corner Bottom BothBG
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawWeirdCornerBottom_BothBG(obj, bg, tiles);
  });
  // Routine 37 - Weird Corner Top BothBG
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawWeirdCornerTop_BothBG(obj, bg, tiles);
  });
  // Routine 38 - Nothing
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawNothing(obj, bg, tiles);
  });
  // Routine 39 - Chest rendering (small + big)
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              const DungeonState* state) {
    self->DrawChest(obj, bg, tiles, state);
  });
  // Routine 40 - Rightwards 4x2 (Floor Tile)
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawRightwards4x2_1to16(obj, bg, tiles);
  });
  // Routine 41 - Rightwards Decor 4x2 spaced 8 (12-column spacing)
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawRightwardsDecor4x2spaced8_1to16(obj, bg, tiles);
  });
  // Routine 42 - Rightwards Cannon Hole 4x3
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawRightwardsCannonHole4x3_1to16(obj, bg, tiles);
  });
  // Routine 43 - Downwards Floor 4x4 (object 0x70)
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawDownwardsFloor4x4_1to16(obj, bg, tiles);
  });
  // Routine 44 - Downwards 1x1 Solid +3 (object 0x71)
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawDownwards1x1Solid_1to16_plus3(obj, bg, tiles);
  });
  // Routine 45 - Downwards Decor 4x4 spaced 2 (objects 0x73-0x74)
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawDownwardsDecor4x4spaced2_1to16(obj, bg, tiles);
  });
  // Routine 46 - Downwards Pillar 2x4 spaced 2 (objects 0x75, 0x87)
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawDownwardsPillar2x4spaced2_1to16(obj, bg, tiles);
  });
  // Routine 47 - Downwards Decor 3x4 spaced 4 (objects 0x76-0x77)
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawDownwardsDecor3x4spaced4_1to16(obj, bg, tiles);
  });
  // Routine 48 - Downwards Decor 2x2 spaced 12 (objects 0x78, 0x7B)
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawDownwardsDecor2x2spaced12_1to16(obj, bg, tiles);
  });
  // Routine 49 - Downwards Line 1x1 +1 (object 0x7C)
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawDownwardsLine1x1_1to16plus1(obj, bg, tiles);
  });
  // Routine 50 - Downwards Decor 2x4 spaced 8 (objects 0x7F, 0x80)
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawDownwardsDecor2x4spaced8_1to16(obj, bg, tiles);
  });
  // Routine 51 - Rightwards Line 1x1 +1 (object 0x50)
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawRightwardsLine1x1_1to16plus1(obj, bg, tiles);
  });
  // Routine 52 - Rightwards Bar 4x3 (object 0x4C)
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawRightwardsBar4x3_1to16(obj, bg, tiles);
  });
  // Routine 53 - Rightwards Shelf 4x4 (objects 0x4D-0x4F)
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawRightwardsShelf4x4_1to16(obj, bg, tiles);
  });
  // Routine 54 - Rightwards Big Rail 1x3 +5 (object 0x5D)
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawRightwardsBigRail1x3_1to16plus5(obj, bg, tiles);
  });
  // Routine 55 - Rightwards Block 2x2 spaced 2 (object 0x5E)
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawRightwardsBlock2x2spaced2_1to16(obj, bg, tiles);
  });

  // ============================================================================
  // Phase 4: SuperSquare Routines (routines 56-64)
  // ============================================================================

  // Routine 56 - 4x4 Blocks in 4x4 SuperSquare (objects 0xC0, 0xC2)
  draw_routines_.push_back(
      []([[maybe_unused]] ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         [[maybe_unused]] const DungeonState* state) {
        DrawContext ctx{bg, obj, tiles, state, nullptr, 0, nullptr};
        draw_routines::Draw4x4BlocksIn4x4SuperSquare(ctx);
      });

  // Routine 57 - 3x3 Floor in 4x4 SuperSquare (objects 0xC3, 0xD7)
  draw_routines_.push_back(
      []([[maybe_unused]] ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         [[maybe_unused]] const DungeonState* state) {
        DrawContext ctx{bg, obj, tiles, state, nullptr, 0, nullptr};
        draw_routines::Draw3x3FloorIn4x4SuperSquare(ctx);
      });

  // Routine 58 - 4x4 Floor in 4x4 SuperSquare (objects 0xC5-0xCA, 0xD1-0xD2,
  // 0xD9, 0xDF-0xE8)
  draw_routines_.push_back(
      []([[maybe_unused]] ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         [[maybe_unused]] const DungeonState* state) {
        DrawContext ctx{bg, obj, tiles, state, nullptr, 0, nullptr};
        draw_routines::Draw4x4FloorIn4x4SuperSquare(ctx);
      });

  // Routine 59 - 4x4 Floor One in 4x4 SuperSquare (object 0xC4)
  draw_routines_.push_back(
      []([[maybe_unused]] ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         [[maybe_unused]] const DungeonState* state) {
        DrawContext ctx{bg, obj, tiles, state, nullptr, 0, nullptr};
        draw_routines::Draw4x4FloorOneIn4x4SuperSquare(ctx);
      });

  // Routine 60 - 4x4 Floor Two in 4x4 SuperSquare (object 0xDB)
  draw_routines_.push_back(
      []([[maybe_unused]] ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         [[maybe_unused]] const DungeonState* state) {
        DrawContext ctx{bg, obj, tiles, state, nullptr, 0, nullptr};
        draw_routines::Draw4x4FloorTwoIn4x4SuperSquare(ctx);
      });

  // Routine 61 - Big Hole 4x4 (object 0xA4)
  draw_routines_.push_back(
      []([[maybe_unused]] ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         [[maybe_unused]] const DungeonState* state) {
        DrawContext ctx{bg, obj, tiles, state, nullptr, 0, nullptr};
        draw_routines::DrawBigHole4x4_1to16(ctx);
      });

  // Routine 62 - Spike 2x2 in 4x4 SuperSquare (object 0xDE)
  draw_routines_.push_back(
      []([[maybe_unused]] ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         [[maybe_unused]] const DungeonState* state) {
        DrawContext ctx{bg, obj, tiles, state, nullptr, 0, nullptr};
        draw_routines::DrawSpike2x2In4x4SuperSquare(ctx);
      });

  // Routine 63 - Table Rock 4x4 (object 0xDD)
  draw_routines_.push_back(
      []([[maybe_unused]] ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         [[maybe_unused]] const DungeonState* state) {
        DrawContext ctx{bg, obj, tiles, state, nullptr, 0, nullptr};
        draw_routines::DrawTableRock4x4_1to16(ctx);
      });

  // Routine 64 - Water Overlay 8x8 (objects 0xD8, 0xDA)
  draw_routines_.push_back(
      []([[maybe_unused]] ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         [[maybe_unused]] const DungeonState* state) {
        DrawContext ctx{bg, obj, tiles, state, nullptr, 0, nullptr};
        draw_routines::DrawWaterOverlay8x8_1to16(ctx);
      });

  // ============================================================================
  // Phase 4 Step 2: Simple Variant Routines (routines 65-74)
  // ============================================================================

  // Routine 65 - Downwards Decor 3x4 spaced 2 (objects 0x81-0x84)
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawDownwardsDecor3x4spaced2_1to16(obj, bg, tiles);
  });

  // Routine 66 - Downwards Big Rail 3x1 plus 5 (object 0x88)
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawDownwardsBigRail3x1_1to16plus5(obj, bg, tiles);
  });

  // Routine 67 - Downwards Block 2x2 spaced 2 (object 0x89)
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawDownwardsBlock2x2spaced2_1to16(obj, bg, tiles);
  });

  // Routine 68 - Downwards Cannon Hole 3x6 (objects 0x85-0x86)
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawDownwardsCannonHole3x6_1to16(obj, bg, tiles);
  });

  // Routine 69 - Downwards Bar 2x3 (object 0x8F)
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawDownwardsBar2x3_1to16(obj, bg, tiles);
  });

  // Routine 70 - Downwards Pots 2x2 (object 0x95)
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawDownwardsPots2x2_1to16(obj, bg, tiles);
  });

  // Routine 71 - Downwards Hammer Pegs 2x2 (object 0x96)
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawDownwardsHammerPegs2x2_1to16(obj, bg, tiles);
  });

  // Routine 72 - Rightwards Edge 1x1 plus 7 (objects 0xB0-0xB1)
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawRightwardsEdge1x1_1to16plus7(obj, bg, tiles);
  });

  // Routine 73 - Rightwards Pots 2x2 (object 0xBC)
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawRightwardsPots2x2_1to16(obj, bg, tiles);
  });

  // Routine 74 - Rightwards Hammer Pegs 2x2 (object 0xBD)
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawRightwardsHammerPegs2x2_1to16(obj, bg, tiles);
  });

  // ============================================================================
  // Phase 4 Step 3: Diagonal Ceiling Routines (routines 75-78)
  // ============================================================================

  // Routine 75 - Diagonal Ceiling Top Left (objects 0xA0, 0xA5, 0xA9)
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawDiagonalCeilingTopLeft(obj, bg, tiles);
  });

  // Routine 76 - Diagonal Ceiling Bottom Left (objects 0xA1, 0xA6, 0xAA)
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawDiagonalCeilingBottomLeft(obj, bg, tiles);
  });

  // Routine 77 - Diagonal Ceiling Top Right (objects 0xA2, 0xA7, 0xAB)
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawDiagonalCeilingTopRight(obj, bg, tiles);
  });

  // Routine 78 - Diagonal Ceiling Bottom Right (objects 0xA3, 0xA8, 0xAC)
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawDiagonalCeilingBottomRight(obj, bg, tiles);
  });

  // ============================================================================
  // Phase 4 Step 5: Special Routines (routines 79-82)
  // ============================================================================

  // Routine 79 - Closed Chest Platform (object 0xC1, 68 tiles)
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawClosedChestPlatform(obj, bg, tiles);
  });

  // Routine 80 - Moving Wall West (object 0xCD, 28 tiles)
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawMovingWallWest(obj, bg, tiles);
  });

  // Routine 81 - Moving Wall East (object 0xCE, 28 tiles)
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawMovingWallEast(obj, bg, tiles);
  });

  // Routine 82 - Open Chest Platform (object 0xDC, 21 tiles)
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawOpenChestPlatform(obj, bg, tiles);
  });

  // ============================================================================
  // New Special Routines (Phase 5) - Stairs, Locks, Interactive Objects
  // ============================================================================

  // Routine 83 - InterRoom Fat Stairs Up (object 0x12D)
  draw_routines_.push_back(
      []([[maybe_unused]] ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         [[maybe_unused]] const DungeonState* state) {
        DrawContext ctx{bg, obj, tiles, state, nullptr, 0, nullptr};
        draw_routines::DrawInterRoomFatStairsUp(ctx);
      });

  // Routine 84 - InterRoom Fat Stairs Down A (object 0x12E)
  draw_routines_.push_back(
      []([[maybe_unused]] ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         [[maybe_unused]] const DungeonState* state) {
        DrawContext ctx{bg, obj, tiles, state, nullptr, 0, nullptr};
        draw_routines::DrawInterRoomFatStairsDownA(ctx);
      });

  // Routine 85 - InterRoom Fat Stairs Down B (object 0x12F)
  draw_routines_.push_back(
      []([[maybe_unused]] ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         [[maybe_unused]] const DungeonState* state) {
        DrawContext ctx{bg, obj, tiles, state, nullptr, 0, nullptr};
        draw_routines::DrawInterRoomFatStairsDownB(ctx);
      });

  // Routine 86 - Auto Stairs (objects 0x130-0x133)
  draw_routines_.push_back(
      []([[maybe_unused]] ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         [[maybe_unused]] const DungeonState* state) {
        DrawContext ctx{bg, obj, tiles, state, nullptr, 0, nullptr};
        draw_routines::DrawAutoStairs(ctx);
      });

  // Routine 87 - Straight InterRoom Stairs (Type 3 objects 0x21E-0x229)
  draw_routines_.push_back(
      []([[maybe_unused]] ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         [[maybe_unused]] const DungeonState* state) {
        DrawContext ctx{bg, obj, tiles, state, nullptr, 0, nullptr};
        draw_routines::DrawStraightInterRoomStairs(ctx);
      });

  // Routine 88 - Spiral Stairs Going Up Upper (object 0x138)
  draw_routines_.push_back(
      []([[maybe_unused]] ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         [[maybe_unused]] const DungeonState* state) {
        DrawContext ctx{bg, obj, tiles, state, nullptr, 0, nullptr};
        draw_routines::DrawSpiralStairs(ctx, true, true);
      });

  // Routine 89 - Spiral Stairs Going Down Upper (object 0x139)
  draw_routines_.push_back(
      []([[maybe_unused]] ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         [[maybe_unused]] const DungeonState* state) {
        DrawContext ctx{bg, obj, tiles, state, nullptr, 0, nullptr};
        draw_routines::DrawSpiralStairs(ctx, false, true);
      });

  // Routine 90 - Spiral Stairs Going Up Lower (object 0x13A)
  draw_routines_.push_back(
      []([[maybe_unused]] ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         [[maybe_unused]] const DungeonState* state) {
        DrawContext ctx{bg, obj, tiles, state, nullptr, 0, nullptr};
        draw_routines::DrawSpiralStairs(ctx, true, false);
      });

  // Routine 91 - Spiral Stairs Going Down Lower (object 0x13B)
  draw_routines_.push_back(
      []([[maybe_unused]] ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         [[maybe_unused]] const DungeonState* state) {
        DrawContext ctx{bg, obj, tiles, state, nullptr, 0, nullptr};
        draw_routines::DrawSpiralStairs(ctx, false, false);
      });

  // Routine 92 - Big Key Lock (Type 3 object 0x218)
  draw_routines_.push_back(
      []([[maybe_unused]] ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         [[maybe_unused]] const DungeonState* state) {
        DrawContext ctx{bg, obj, tiles, state, nullptr, 0, nullptr};
        draw_routines::DrawBigKeyLock(ctx);
      });

  // Routine 93 - Bombable Floor (Type 3 object 0x247)
  draw_routines_.push_back(
      []([[maybe_unused]] ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         [[maybe_unused]] const DungeonState* state) {
        DrawContext ctx{bg, obj, tiles, state, nullptr, 0, nullptr};
        draw_routines::DrawBombableFloor(ctx);
      });

  // Routine 94 - Empty Water Face (Type 3 object 0x200)
  draw_routines_.push_back(
      []([[maybe_unused]] ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         [[maybe_unused]] const DungeonState* state) {
        DrawContext ctx{bg, obj, tiles, state, nullptr, 0, nullptr};
        draw_routines::DrawEmptyWaterFace(ctx);
      });

  // Routine 95 - Spitting Water Face (Type 3 object 0x201)
  draw_routines_.push_back(
      []([[maybe_unused]] ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         [[maybe_unused]] const DungeonState* state) {
        DrawContext ctx{bg, obj, tiles, state, nullptr, 0, nullptr};
        draw_routines::DrawSpittingWaterFace(ctx);
      });

  // Routine 96 - Drenching Water Face (Type 3 object 0x202)
  draw_routines_.push_back(
      []([[maybe_unused]] ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         [[maybe_unused]] const DungeonState* state) {
        DrawContext ctx{bg, obj, tiles, state, nullptr, 0, nullptr, nullptr};
        draw_routines::DrawDrenchingWaterFace(ctx);
      });

  // Routine 97 - Prison Cell (Type 3 objects 0x20D, 0x217)
  // This routine draws to both BG1 and BG2 with horizontal flip symmetry
  // Note: secondary_bg is set in DrawObject() for dual-BG objects
  draw_routines_.push_back(
      []([[maybe_unused]] ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         [[maybe_unused]] const DungeonState* state) {
        DrawContext ctx{bg, obj, tiles, state, nullptr, 0, nullptr, nullptr};
        draw_routines::DrawPrisonCell(ctx);
      });

  // Routine 98 - Bed 4x5 (Type 2 objects 0x122, 0x128)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         const DungeonState* state) {
        self->DrawBed4x5(obj, bg, tiles, state);
      });

  // Routine 99 - Rightwards 3x6 (Type 2 object 0x12C, Type 3 0x236-0x237)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         const DungeonState* state) {
        self->DrawRightwards3x6(obj, bg, tiles, state);
      });

  // Routine 100 - Utility 6x3 (Type 2 object 0x13E, Type 3 0x24D, 0x25D)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         const DungeonState* state) {
        self->DrawUtility6x3(obj, bg, tiles, state);
      });

  // Routine 101 - Utility 3x5 (Type 3 objects 0x255, 0x25B)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         const DungeonState* state) {
        self->DrawUtility3x5(obj, bg, tiles, state);
      });

  // Routine 102 - Vertical Turtle Rock Pipe (Type 3 objects 0x23A, 0x23B)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         const DungeonState* state) {
        self->DrawVerticalTurtleRockPipe(obj, bg, tiles, state);
      });

  // Routine 103 - Horizontal Turtle Rock Pipe (Type 3 objects 0x23C, 0x23D, 0x25C)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         const DungeonState* state) {
        self->DrawHorizontalTurtleRockPipe(obj, bg, tiles, state);
      });

  // Routine 104 - Light Beam on Floor (Type 3 object 0x270)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         const DungeonState* state) {
        self->DrawLightBeam(obj, bg, tiles, state);
      });

  // Routine 105 - Big Light Beam on Floor (Type 3 object 0x271)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         const DungeonState* state) {
        self->DrawBigLightBeam(obj, bg, tiles, state);
      });

  // Routine 106 - Boss Shell 4x4 (Type 3 objects 0x272, 0x27B, 0xF95)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         const DungeonState* state) {
        self->DrawBossShell4x4(obj, bg, tiles, state);
      });

  // Routine 107 - Solid Wall Decor 3x4 (Type 3 objects 0x269-0x26A, 0x26E-0x26F)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         const DungeonState* state) {
        self->DrawSolidWallDecor3x4(obj, bg, tiles, state);
      });

  // Routine 108 - Archery Game Target Door (Type 3 objects 0x260-0x261)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         const DungeonState* state) {
        self->DrawArcheryGameTargetDoor(obj, bg, tiles, state);
      });

  // Routine 109 - Ganon Triforce Floor Decor (Type 3 object 0x278)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         const DungeonState* state) {
        self->DrawGanonTriforceFloorDecor(obj, bg, tiles, state);
      });

  // Routine 110 - Single 2x2 (pots, statues, single-instance 2x2 objects)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         const DungeonState* state) {
        self->DrawSingle2x2(obj, bg, tiles, state);
      });

  // Routine 111 - Waterfall47 (object 0x47)
  // ASM: RoomDraw_Waterfall47 - draws 1x5 columns, count = (size+1)*2
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         [[maybe_unused]] const DungeonState* state) {
        int size = obj.size_ & 0x0F;
        int count = (size + 1) * 2;  // ASM: ASL $B2

        if (tiles.size() < 5) return;

        // Draw first 1x5 column
        for (int row = 0; row < 5; row++) {
          self->WriteTile8(bg, obj.x_, obj.y_ + row, tiles[row]);
        }

        // Draw middle columns
        for (int s = 0; s < count; s++) {
          int col_x = obj.x_ + 1 + s;
          for (int row = 0; row < 5; row++) {
            // Use tiles at offset 10 for middle columns
            size_t tile_idx = std::min(size_t(5 + row), tiles.size() - 1);
            self->WriteTile8(bg, col_x, obj.y_ + row, tiles[tile_idx]);
          }
        }

        // Draw last 1x5 column
        int last_x = obj.x_ + 1 + count;
        for (int row = 0; row < 5; row++) {
          size_t tile_idx = std::min(size_t(10 + row), tiles.size() - 1);
          self->WriteTile8(bg, last_x, obj.y_ + row, tiles[tile_idx]);
        }
      });

  // Routine 112 - Waterfall48 (object 0x48)
  // ASM: RoomDraw_Waterfall48 - draws 1x3 columns, count = (size+1)*2
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         [[maybe_unused]] const DungeonState* state) {
        int size = obj.size_ & 0x0F;
        int count = (size + 1) * 2;  // ASM: ASL $B2

        if (tiles.size() < 3) return;

        // Draw first 1x3 column
        for (int row = 0; row < 3; row++) {
          self->WriteTile8(bg, obj.x_, obj.y_ + row, tiles[row]);
        }

        // Draw middle columns
        for (int s = 0; s < count; s++) {
          int col_x = obj.x_ + 1 + s;
          for (int row = 0; row < 3; row++) {
            self->WriteTile8(bg, col_x, obj.y_ + row, tiles[row]);
          }
        }

        // Draw last 1x3 column using different tiles
        int last_x = obj.x_ + 1 + count;
        for (int row = 0; row < 3; row++) {
          size_t tile_idx = std::min(size_t(3 + row), tiles.size() - 1);
          self->WriteTile8(bg, last_x, obj.y_ + row, tiles[tile_idx]);
        }
      });

  // Routine 113 - Single 4x4 (NO repetition)
  // ASM: RoomDraw_4x4 - draws a single 4x4 pattern (16 tiles)
  // Used for: 0xFEB (large decor), and other single 4x4 objects
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         [[maybe_unused]] const DungeonState* state) {
        self->DrawSingle4x4(obj, bg, tiles, state);
      });

  // Routine 114 - Single 4x3 (NO repetition)
  // ASM: RoomDraw_TableRock4x3 - draws a single 4x3 pattern (12 tiles)
  // Used for: 0xFED (water grate), 0xFB1 (big chest), etc.
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         [[maybe_unused]] const DungeonState* state) {
        self->DrawSingle4x3(obj, bg, tiles, state);
      });

  // Routine 115 - RupeeFloor (special pattern for 0xF92)
  // ASM: RoomDraw_RupeeFloor - draws 3 columns of 2-tile pairs at 3 Y positions
  // Pattern: 6 tiles wide, 8 rows tall with gaps (rows 2 and 5 are empty)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         [[maybe_unused]] const DungeonState* state) {
        self->DrawRupeeFloor(obj, bg, tiles, state);
      });

  // Routine 116 - Actual 4x4 tile8 pattern (32x32 pixels, NO repetition)
  // ASM: RoomDraw_4x4 - draws exactly 4 columns x 4 rows = 16 tiles
  // Used for: 0xFE6 (pit)
  draw_routines_.push_back(
      [](ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         [[maybe_unused]] const DungeonState* state) {
        self->DrawActual4x4(obj, bg, tiles, state);
      });

  auto ensure_index = [this](size_t index) {
    while (draw_routines_.size() <= index) {
      draw_routines_.push_back(
          [](ObjectDrawer* self, const RoomObject& obj,
             gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
             [[maybe_unused]] const DungeonState* state) {
            self->DrawNothing(obj, bg, tiles, state);
          });
    }
  };

  // Routine 117 - Vertical rails with CORNER+MIDDLE+END pattern (0x8A-0x8C)
  // ASM: RoomDraw_DownwardsHasEdge1x1_1to16_plus23 - matches horizontal 0x22
  ensure_index(117);
  draw_routines_[117] =
      [](ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         [[maybe_unused]] const DungeonState* state) {
        self->DrawDownwardsHasEdge1x1_1to16_plus23(obj, bg, tiles, state);
      };

  // Routine 118 - Horizontal long rails with CORNER+MIDDLE+END pattern (0x5F)
  ensure_index(118);
  draw_routines_[118] =
      [](ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         [[maybe_unused]] const DungeonState* state) {
        self->DrawRightwardsHasEdge1x1_1to16_plus23(obj, bg, tiles, state);
      };

  // Routine 130 - Custom Object (Oracle of Secrets 0x31, 0x32)
  // Uses external binary files instead of ROM tile data.
  // Requires CustomObjectManager initialization and enable_custom_objects flag.
  ensure_index(130);
  draw_routines_[130] =
      [](ObjectDrawer* self, const RoomObject& obj,
         gfx::BackgroundBuffer& bg, std::span<const gfx::TileInfo> tiles,
         [[maybe_unused]] const DungeonState* state) {
        self->DrawCustomObject(obj, bg, tiles, state);
      };

  routines_initialized_ = true;
}

int ObjectDrawer::GetDrawRoutineId(int16_t object_id) const {
  auto it = object_to_routine_map_.find(object_id);
  if (it != object_to_routine_map_.end()) {
    return it->second;
  }

  // Default to simple 1x1 solid for unmapped objects
  return -1;
}

// ============================================================================
// Draw Routine Implementations (Based on ZScream patterns)
// ============================================================================

void ObjectDrawer::DrawDoor(const DoorDef& door, int door_index,
                              gfx::BackgroundBuffer& bg1,
                              gfx::BackgroundBuffer& bg2,
                              [[maybe_unused]] const DungeonState* state) {
  // Door rendering based on ZELDA3_DUNGEON_SPEC.md Section 5 and disassembly
  // Uses DoorType and DoorDirection enums for type safety
  // Position calculations via DoorPositionManager

  LOG_DEBUG("ObjectDrawer", "DrawDoor: idx=%d type=%d dir=%d pos=%d",
            door_index, static_cast<int>(door.type),
            static_cast<int>(door.direction), door.position);

  if (!rom_ || !rom_->is_loaded() || !room_gfx_buffer_) {
    LOG_DEBUG("ObjectDrawer", "DrawDoor: SKIPPED - rom=%p loaded=%d gfx=%p",
              (void*)rom_, rom_ ? rom_->is_loaded() : 0, (void*)room_gfx_buffer_);
    return;
  }

  auto& bitmap = bg1.bitmap();
  if (!bitmap.is_active() || bitmap.width() == 0) {
    LOG_DEBUG("ObjectDrawer", "DrawDoor: SKIPPED - bitmap not active or zero width");
    return;
  }

  // Get door position from DoorPositionManager
  auto [tile_x, tile_y] = door.GetTileCoords();
  auto dims = door.GetDimensions();
  int door_width = dims.width_tiles;
  int door_height = dims.height_tiles;
  
  LOG_DEBUG("ObjectDrawer", "DrawDoor: tile_pos=(%d,%d) dims=%dx%d",
            tile_x, tile_y, door_width, door_height);

  // Door graphics use an indirect addressing scheme:
  // 1. kDoorGfxUp/Down/Left/Right point to offset tables (DoorGFXDataOffset_*)
  // 2. Each table entry is a 16-bit offset into RoomDrawObjectData
  // 3. RoomDrawObjectData base is at PC 0x1B52 (SNES $00:9B52)
  // 4. Actual tile data = 0x1B52 + offset_from_table

  // Select offset table based on direction
  int offset_table_addr = 0;
  switch (door.direction) {
    case DoorDirection::North: offset_table_addr = kDoorGfxUp; break;    // 0x4D9E
    case DoorDirection::South: offset_table_addr = kDoorGfxDown; break;  // 0x4E06
    case DoorDirection::West:  offset_table_addr = kDoorGfxLeft; break;  // 0x4E66
    case DoorDirection::East:  offset_table_addr = kDoorGfxRight; break; // 0x4EC6
  }

  // Calculate door type index (door types step by 2: 0x00, 0x02, 0x04, ...)
  int type_value = static_cast<int>(door.type);
  int type_index = type_value / 2;

  // Read offset from table (each entry is 2 bytes)
  int table_entry_addr = offset_table_addr + (type_index * 2);
  if (table_entry_addr + 1 >= static_cast<int>(rom_->size())) {
    DrawDoorIndicator(bitmap, tile_x, tile_y, door_width, door_height,
                      door.type, door.direction);
    return;
  }

  const auto& rom_data = rom_->data();
  uint16_t tile_offset = rom_data[table_entry_addr] | 
                         (rom_data[table_entry_addr + 1] << 8);

  // RoomDrawObjectData base address (PC offset)
  constexpr int kRoomDrawObjectDataBase = 0x1B52;
  int tile_data_addr = kRoomDrawObjectDataBase + tile_offset;

  LOG_DEBUG("ObjectDrawer",
            "DrawDoor: offset_table=0x%X type_idx=%d tile_offset=0x%X tile_addr=0x%X",
            offset_table_addr, type_index, tile_offset, tile_data_addr);

  // Validate address range (12 tiles * 2 bytes = 24 bytes)
  int tiles_per_door = door_width * door_height;  // 12 tiles (4x3 or 3x4)
  int data_size = tiles_per_door * 2;
  if (tile_data_addr < 0 || tile_data_addr + data_size > static_cast<int>(rom_->size())) {
    LOG_DEBUG("ObjectDrawer", "DrawDoor: INVALID ADDRESS - falling back to indicator");
    DrawDoorIndicator(bitmap, tile_x, tile_y, door_width, door_height,
                      door.type, door.direction);
    return;
  }

  // Read and render door tiles
  // All directions use column-major tile order (matching ASM draw routines)
  // The ROM stores tiles in column-major order for all door directions
  LOG_DEBUG("ObjectDrawer", "DrawDoor: Reading %d tiles from 0x%X",
            tiles_per_door, tile_data_addr);
  int tile_idx = 0;
  auto& priority_buffer = bg1.mutable_priority_data();
  int bitmap_width = bitmap.width();
  
  for (int dx = 0; dx < door_width; dx++) {
    for (int dy = 0; dy < door_height; dy++) {
      int addr = tile_data_addr + (tile_idx * 2);
      uint16_t tile_word = rom_data[addr] | (rom_data[addr + 1] << 8);

      auto tile_info = gfx::WordToTileInfo(tile_word);
      int pixel_x = (tile_x + dx) * 8;
      int pixel_y = (tile_y + dy) * 8;

      if (tile_idx < 4) {
        LOG_DEBUG("ObjectDrawer", "DrawDoor: tile[%d] word=0x%04X id=%d pal=%d pixel=(%d,%d)",
                  tile_idx, tile_word, tile_info.id_, tile_info.palette_, pixel_x, pixel_y);
      }

      DrawTileToBitmap(bitmap, tile_info, pixel_x, pixel_y, room_gfx_buffer_);
      
      // Update priority buffer for this tile
      uint8_t priority = tile_info.over_ ? 1 : 0;
      const auto& bitmap_data = bitmap.vector();
      for (int py = 0; py < 8; py++) {
        int dest_y = pixel_y + py;
        if (dest_y < 0 || dest_y >= bitmap.height()) continue;
        for (int px = 0; px < 8; px++) {
          int dest_x = pixel_x + px;
          if (dest_x < 0 || dest_x >= bitmap_width) continue;
          int dest_index = dest_y * bitmap_width + dest_x;
          if (dest_index < static_cast<int>(bitmap_data.size()) &&
              bitmap_data[dest_index] != 255) {
            priority_buffer[dest_index] = priority;
          }
        }
      }
      
      tile_idx++;
    }
  }

  LOG_DEBUG("ObjectDrawer", 
            "DrawDoor: type=%s dir=%s pos=%d at tile(%d,%d) size=%dx%d "
            "offset_table=0x%X tile_offset=0x%X tile_addr=0x%X",
            std::string(GetDoorTypeName(door.type)).c_str(),
            std::string(GetDoorDirectionName(door.direction)).c_str(),
            door.position, tile_x, tile_y, door_width, door_height,
            offset_table_addr, tile_offset, tile_data_addr);
}

void ObjectDrawer::DrawDoorIndicator(gfx::Bitmap& bitmap, int tile_x, int tile_y,
                                      int width, int height, DoorType type,
                                      DoorDirection direction) {
  // Draw a simple colored rectangle as door indicator when graphics unavailable
  // Different colors for different door types using DoorType enum

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

      if (dest_x >= 0 && dest_x < bitmap_width &&
          dest_y >= 0 && dest_y < bitmap_height) {
        // Draw border (2 pixel thick) or fill
        bool is_border = (px < 2 || px >= pixel_width - 2 ||
                          py < 2 || py >= pixel_height - 2);
        uint8_t final_color = is_border ? (color_idx + 5) : color_idx;

        int offset = (dest_y * bitmap_width) + dest_x;
        bitmap.WriteToPixel(offset, final_color);
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
      WriteTile8(bg, obj.x_, obj.y_, tiles[4]);       // top-left
      WriteTile8(bg, obj.x_, obj.y_ + 1, tiles[5]);   // bottom-left
      WriteTile8(bg, obj.x_ + 1, obj.y_, tiles[6]);   // top-right
      WriteTile8(bg, obj.x_ + 1, obj.y_ + 1, tiles[7]); // bottom-right
    }
      return;
  }

  // Draw closed chest - SINGLE 2x2 pattern (column-major order)
  if (tiles.size() >= 4) {
    WriteTile8(bg, obj.x_, obj.y_, tiles[0]);       // top-left
    WriteTile8(bg, obj.x_, obj.y_ + 1, tiles[1]);   // bottom-left
    WriteTile8(bg, obj.x_ + 1, obj.y_, tiles[2]);   // top-right
    WriteTile8(bg, obj.x_ + 1, obj.y_ + 1, tiles[3]); // bottom-right
  }
}

void ObjectDrawer::DrawNothing(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                               std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Intentionally empty - represents invisible logic objects or placeholders
  // ASM: RoomDraw_Nothing_A ($0190F2), RoomDraw_Nothing_B ($01932E), etc.
  // These routines typically just RTS.
  LOG_DEBUG("ObjectDrawer", "DrawNothing for object 0x%02X (logic/invisible)", obj.id_);
}

void ObjectDrawer::DrawRightwards2x2_1to15or32(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Draws 2x2 tiles rightward (object 0x00 = ceiling, 0xB8-0xB9)
  // Size byte determines how many times to repeat (1-15 or 32)
  // ROM tile order is COLUMN-MAJOR: [col0_row0, col0_row1, col1_row0, col1_row1]
  int size = obj.size_;
  if (size == 0)
    size = 32;  // Special case for object 0x00

  // Debug: Log ceiling objects (0x00, 0xB8, 0xB9)
  bool is_ceiling = (obj.id_ == 0x00 || obj.id_ == 0xB8 || obj.id_ == 0xB9);
  if (is_ceiling && tiles.size() >= 4) {
    LOG_DEBUG("ObjectDrawer", "Ceiling Draw: obj=0x%02X pos=(%d,%d) size=%d tiles=%zu",
              obj.id_, obj.x_, obj.y_, size, tiles.size());
    LOG_DEBUG("ObjectDrawer", "  Tile IDs: [%d, %d, %d, %d]",
              tiles[0].id_, tiles[1].id_, tiles[2].id_, tiles[3].id_);
    LOG_DEBUG("ObjectDrawer", "  Palettes: [%d, %d, %d, %d]",
              tiles[0].palette_, tiles[1].palette_, tiles[2].palette_, tiles[3].palette_);
  }

  LOG_DEBUG("ObjectDrawer",
            "DrawRightwards2x2: obj=%04X pos=(%d,%d) size=%d tiles=%zu",
            obj.id_, obj.x_, obj.y_, size, tiles.size());

  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 4) {
      // Draw 2x2 pattern in COLUMN-MAJOR order (matching assembly)
      // tiles[0] → $BF → (col 0, row 0) = top-left
      // tiles[1] → $CB → (col 0, row 1) = bottom-left
      // tiles[2] → $C2 → (col 1, row 0) = top-right
      // tiles[3] → $CE → (col 1, row 1) = bottom-right
      WriteTile8(bg, obj.x_ + (s * 2), obj.y_, tiles[0]);      // col 0, row 0
      WriteTile8(bg, obj.x_ + (s * 2), obj.y_ + 1, tiles[1]);  // col 0, row 1
      WriteTile8(bg, obj.x_ + (s * 2) + 1, obj.y_, tiles[2]);  // col 1, row 0
      WriteTile8(bg, obj.x_ + (s * 2) + 1, obj.y_ + 1,
                 tiles[3]);  // col 1, row 1
    } else {
      LOG_DEBUG("ObjectDrawer",
                "DrawRightwards2x2: SKIPPING - tiles.size()=%zu < 4",
                tiles.size());
    }
  }
}

void ObjectDrawer::DrawRightwards2x4_1to15or26(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Draws 2x4 tiles rightward (objects 0x01-0x02)
  // Uses RoomDraw_Nx4 with N=2, tiles are COLUMN-MAJOR:
  // [col0_row0, col0_row1, col0_row2, col0_row3, col1_row0, col1_row1, col1_row2, col1_row3]
  int size = obj.size_;
  if (size == 0)
    size = 26;  // Special case

  LOG_DEBUG("ObjectDrawer", "Wall Draw 2x4: obj=0x%03X pos=(%d,%d) size=%d tiles=%zu",
            obj.id_, obj.x_, obj.y_, size, tiles.size());
  LOG_DEBUG("ObjectDrawer",
            "DrawRightwards2x4: obj=%04X pos=(%d,%d) size=%d tiles=%zu",
            obj.id_, obj.x_, obj.y_, size, tiles.size());

  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 8) {
      // Draw 2x4 pattern in COLUMN-MAJOR order (matching RoomDraw_Nx4)
      // Column 0 (tiles 0-3)
      WriteTile8(bg, obj.x_ + (s * 2), obj.y_, tiles[0]);      // col 0, row 0
      WriteTile8(bg, obj.x_ + (s * 2), obj.y_ + 1, tiles[1]);  // col 0, row 1
      WriteTile8(bg, obj.x_ + (s * 2), obj.y_ + 2, tiles[2]);  // col 0, row 2
      WriteTile8(bg, obj.x_ + (s * 2), obj.y_ + 3, tiles[3]);  // col 0, row 3
      // Column 1 (tiles 4-7)
      WriteTile8(bg, obj.x_ + (s * 2) + 1, obj.y_, tiles[4]);      // col 1, row 0
      WriteTile8(bg, obj.x_ + (s * 2) + 1, obj.y_ + 1, tiles[5]);  // col 1, row 1
      WriteTile8(bg, obj.x_ + (s * 2) + 1, obj.y_ + 2, tiles[6]);  // col 1, row 2
      WriteTile8(bg, obj.x_ + (s * 2) + 1, obj.y_ + 3, tiles[7]);  // col 1, row 3
    } else if (tiles.size() >= 4) {
      // Fallback: with 4 tiles we can only draw 1 column (1x4 pattern)
      WriteTile8(bg, obj.x_ + (s * 2), obj.y_, tiles[0]);
      WriteTile8(bg, obj.x_ + (s * 2), obj.y_ + 1, tiles[1]);
      WriteTile8(bg, obj.x_ + (s * 2), obj.y_ + 2, tiles[2]);
      WriteTile8(bg, obj.x_ + (s * 2), obj.y_ + 3, tiles[3]);
    }
  }
}

void ObjectDrawer::DrawRightwards2x4_1to16(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Draws 2x4 tiles rightward with adjacent spacing (objects 0x03-0x04)
  // Uses RoomDraw_Nx4 with N=2, tiles are COLUMN-MAJOR
  // ASM: GetSize_1to16 means count = size + 1
  int size = obj.size_ & 0x0F;
  int count = size + 1;

  LOG_DEBUG("ObjectDrawer",
            "DrawRightwards2x4_1to16: obj=%04X pos=(%d,%d) size=%d count=%d tiles=%zu",
            obj.id_, obj.x_, obj.y_, size, count, tiles.size());

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 8) {
      // Draw 2x4 pattern in COLUMN-MAJOR order with adjacent spacing (s * 2)
      // Column 0 (tiles 0-3)
      WriteTile8(bg, obj.x_ + (s * 2), obj.y_, tiles[0]);      // col 0, row 0
      WriteTile8(bg, obj.x_ + (s * 2), obj.y_ + 1, tiles[1]);  // col 0, row 1
      WriteTile8(bg, obj.x_ + (s * 2), obj.y_ + 2, tiles[2]);  // col 0, row 2
      WriteTile8(bg, obj.x_ + (s * 2), obj.y_ + 3, tiles[3]);  // col 0, row 3
      // Column 1 (tiles 4-7)
      WriteTile8(bg, obj.x_ + (s * 2) + 1, obj.y_, tiles[4]);      // col 1, row 0
      WriteTile8(bg, obj.x_ + (s * 2) + 1, obj.y_ + 1, tiles[5]);  // col 1, row 1
      WriteTile8(bg, obj.x_ + (s * 2) + 1, obj.y_ + 2, tiles[6]);  // col 1, row 2
      WriteTile8(bg, obj.x_ + (s * 2) + 1, obj.y_ + 3, tiles[7]);  // col 1, row 3
    } else if (tiles.size() >= 4) {
      // Fallback: with 4 tiles we can only draw 1 column (1x4 pattern)
      WriteTile8(bg, obj.x_ + (s * 2), obj.y_, tiles[0]);
      WriteTile8(bg, obj.x_ + (s * 2), obj.y_ + 1, tiles[1]);
      WriteTile8(bg, obj.x_ + (s * 2), obj.y_ + 2, tiles[2]);
      WriteTile8(bg, obj.x_ + (s * 2), obj.y_ + 3, tiles[3]);
    }
  }
}

void ObjectDrawer::DrawRightwards2x4_1to16_BothBG(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Same as DrawRightwards2x4_1to16 but draws to both BG1 and BG2 (objects 0x05-0x06)
  DrawRightwards2x4_1to16(obj, bg, tiles);
  // Note: BothBG would require access to both buffers - simplified for now
}

void ObjectDrawer::DrawRightwards2x2_1to16(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Draws 2x2 tiles rightward (objects 0x07-0x08)
  // ROM tile order is COLUMN-MAJOR: [col0_row0, col0_row1, col1_row0, col1_row1]
  int size = obj.size_ & 0x0F;

  // Assembly: JSR RoomDraw_GetSize_1to16
  // GetSize_1to16: count = size + 1
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 4) {
      // Draw 2x2 pattern in COLUMN-MAJOR order (matching assembly)
      WriteTile8(bg, obj.x_ + (s * 2), obj.y_, tiles[0]);      // col 0, row 0
      WriteTile8(bg, obj.x_ + (s * 2), obj.y_ + 1, tiles[1]);  // col 0, row 1
      WriteTile8(bg, obj.x_ + (s * 2) + 1, obj.y_, tiles[2]);  // col 1, row 0
      WriteTile8(bg, obj.x_ + (s * 2) + 1, obj.y_ + 1,
                 tiles[3]);  // col 1, row 1
    }
  }
}

void ObjectDrawer::DrawDiagonalAcute_1to16(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Diagonal acute (/) - draws 5 tiles vertically, moves up-right
  // Based on bank_01.asm RoomDraw_DiagonalAcute_1to16 at $018C58
  // Uses RoomDraw_2x2and1 to draw 5 tiles at rows 0-4, then moves Y -= $7E
  int size = obj.size_ & 0x0F;

  // Assembly: LDA #$0007; JSR RoomDraw_GetSize_1to16_timesA
  // GetSize_1to16_timesA formula: ((B2 << 2) | B4) + A
  // Since size_ already contains ((B2 << 2) | B4), count = size + 7
  int count = size + 7;

  if (tiles.size() < 5) return;

  for (int s = 0; s < count; s++) {
    // Draw 5 tiles in a vertical column (RoomDraw_2x2and1 pattern)
    // Assembly stores to [$BF],Y, [$CB],Y, [$D7],Y, [$DA],Y, [$DD],Y
    // These are rows 0, 1, 2, 3, 4 at the same X position
    int tile_x = obj.x_ + s;      // Move right each iteration
    int tile_y = obj.y_ - s;      // Move up each iteration (acute = /)

    WriteTile8(bg, tile_x, tile_y + 0, tiles[0]);
    WriteTile8(bg, tile_x, tile_y + 1, tiles[1]);
    WriteTile8(bg, tile_x, tile_y + 2, tiles[2]);
    WriteTile8(bg, tile_x, tile_y + 3, tiles[3]);
    WriteTile8(bg, tile_x, tile_y + 4, tiles[4]);
  }
}

void ObjectDrawer::DrawDiagonalGrave_1to16(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Diagonal grave (\) - draws 5 tiles vertically, moves down-right
  // Based on bank_01.asm RoomDraw_DiagonalGrave_1to16 at $018C61
  // Uses RoomDraw_2x2and1 to draw 5 tiles at rows 0-4, then moves Y += $82
  int size = obj.size_ & 0x0F;

  // Assembly: LDA #$0007; JSR RoomDraw_GetSize_1to16_timesA
  // GetSize_1to16_timesA formula: ((B2 << 2) | B4) + A
  // Since size_ already contains ((B2 << 2) | B4), count = size + 7
  int count = size + 7;

  if (tiles.size() < 5) return;

  for (int s = 0; s < count; s++) {
    // Draw 5 tiles in a vertical column (RoomDraw_2x2and1 pattern)
    // Assembly stores to [$BF],Y, [$CB],Y, [$D7],Y, [$DA],Y, [$DD],Y
    // These are rows 0, 1, 2, 3, 4 at the same X position
    int tile_x = obj.x_ + s;      // Move right each iteration
    int tile_y = obj.y_ + s;      // Move down each iteration (grave = \)

    WriteTile8(bg, tile_x, tile_y + 0, tiles[0]);
    WriteTile8(bg, tile_x, tile_y + 1, tiles[1]);
    WriteTile8(bg, tile_x, tile_y + 2, tiles[2]);
    WriteTile8(bg, tile_x, tile_y + 3, tiles[3]);
    WriteTile8(bg, tile_x, tile_y + 4, tiles[4]);
  }
}

void ObjectDrawer::DrawDiagonalAcute_1to16_BothBG(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Diagonal acute (/) for both BG layers (objects 0x15, 0x18-0x1D, 0x20)
  // Based on bank_01.asm RoomDraw_DiagonalAcute_1to16_BothBG at $018C6A
  // Assembly: LDA #$0006; JSR RoomDraw_GetSize_1to16_timesA
  // GetSize_1to16_timesA formula: ((B2 << 2) | B4) + A
  // Since size_ already contains ((B2 << 2) | B4), count = size + 6
  int size = obj.size_ & 0x0F;
  int count = size + 6;

  if (tiles.size() < 5) return;

  for (int s = 0; s < count; s++) {
    int tile_x = obj.x_ + s;
    int tile_y = obj.y_ - s;

    WriteTile8(bg, tile_x, tile_y + 0, tiles[0]);
    WriteTile8(bg, tile_x, tile_y + 1, tiles[1]);
    WriteTile8(bg, tile_x, tile_y + 2, tiles[2]);
    WriteTile8(bg, tile_x, tile_y + 3, tiles[3]);
    WriteTile8(bg, tile_x, tile_y + 4, tiles[4]);
  }
  // Note: BothBG should write to both BG1 and BG2 - handled by DrawObject caller
}

void ObjectDrawer::DrawDiagonalGrave_1to16_BothBG(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Diagonal grave (\) for both BG layers (objects 0x16-0x17, 0x1A-0x1B, 0x1E-0x1F)
  // Based on bank_01.asm RoomDraw_DiagonalGrave_1to16_BothBG at $018CB9
  // Assembly: LDA #$0006; JSR RoomDraw_GetSize_1to16_timesA
  // GetSize_1to16_timesA formula: ((B2 << 2) | B4) + A
  // Since size_ already contains ((B2 << 2) | B4), count = size + 6
  int size = obj.size_ & 0x0F;
  int count = size + 6;

  if (tiles.size() < 5) return;

  for (int s = 0; s < count; s++) {
    int tile_x = obj.x_ + s;
    int tile_y = obj.y_ + s;

    WriteTile8(bg, tile_x, tile_y + 0, tiles[0]);
    WriteTile8(bg, tile_x, tile_y + 1, tiles[1]);
    WriteTile8(bg, tile_x, tile_y + 2, tiles[2]);
    WriteTile8(bg, tile_x, tile_y + 3, tiles[3]);
    WriteTile8(bg, tile_x, tile_y + 4, tiles[4]);
  }
  // Note: BothBG should write to both BG1 and BG2 - handled by DrawObject caller
}

void ObjectDrawer::DrawCorner4x4(const RoomObject& obj,
                                 gfx::BackgroundBuffer& bg,
                                 std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: 4x4 grid corner for Type 2 objects
  // LAYOUT CORNERS: 0x100-0x103 (upper-left, lower-left, upper-right, lower-right)
  //   - These are the concave corners used in room layouts
  //   - GetSubtype2TileCount returns 16 tiles for these (32 bytes of tile data)
  // OTHER CORNERS: 0x108-0x10F (BothBG variants handled by routine 35)
  // Type 2 objects may have 8 or 16 tiles depending on the specific object

  if (tiles.size() >= 16) {
    // Full 4x4 pattern - Column-major ordering per ZScream
    int tid = 0;
    for (int xx = 0; xx < 4; xx++) {
      for (int yy = 0; yy < 4; yy++) {
        WriteTile8(bg, obj.x_ + xx, obj.y_ + yy, tiles[tid++]);
      }
    }
  } else if (tiles.size() >= 8) {
    // Type 2 objects: 8 tiles arranged in 2x4 column-major pattern
    // This is the standard Type 2 tile layout
    int tid = 0;
    for (int xx = 0; xx < 2; xx++) {
      for (int yy = 0; yy < 4; yy++) {
        WriteTile8(bg, obj.x_ + xx, obj.y_ + yy, tiles[tid++]);
      }
    }
  } else if (tiles.size() >= 4) {
    // Fallback: 2x2 pattern
    int tid = 0;
    for (int xx = 0; xx < 2; xx++) {
      for (int yy = 0; yy < 2; yy++) {
        WriteTile8(bg, obj.x_ + xx, obj.y_ + yy, tiles[tid++]);
      }
    }
  }
}

void ObjectDrawer::DrawRightwards1x2_1to16_plus2(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: 1x3 tiles rightward with caps (object 0x21)
  // ZScream: left cap (tiles 0-2), middle columns (tiles 3-5), right cap (tiles 6-8)
  int size = obj.size_ & 0x0F;

  if (tiles.size() >= 9) {
    auto draw_column = [&](int x, int base) {
      WriteTile8(bg, x, obj.y_, tiles[base + 0]);
      WriteTile8(bg, x, obj.y_ + 1, tiles[base + 1]);
      WriteTile8(bg, x, obj.y_ + 2, tiles[base + 2]);
    };

    // Left cap at origin
    draw_column(obj.x_, 0);

    // Middle columns: two per size step (size + 1 iterations)
    int mid_cols = (size + 1) * 2;
    for (int s = 0; s < mid_cols; s++) {
      draw_column(obj.x_ + 1 + s, 3);
    }

    // Right cap
    draw_column(obj.x_ + 1 + mid_cols, 6);
    return;
  }

  // Fallback: simple 1x2 pattern
  int count = (size * 2) + 1;
  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 2) {
      WriteTile8(bg, obj.x_ + s + 2, obj.y_, tiles[0]);
      WriteTile8(bg, obj.x_ + s + 2, obj.y_ + 1, tiles[1]);
    }
  }
}

void ObjectDrawer::DrawRightwardsHasEdge1x1_1to16_plus3(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Rail with corner check (object 0x22 small rails)
  // ASM: RoomDraw_RightwardsHasEdge1x1_1to16_plus3
  // ZScream: count = size + 2
  // Structure: [CORNER] -> [MIDDLE * count] -> [END]
  int size = obj.size_ & 0x0F;
  int count = size + 2;

  if (tiles.size() < 3) return;

  int x = obj.x_;
  
  // Draw corner tile (tile 0) - in editor we always draw it
  WriteTile8(bg, x, obj.y_, tiles[0]);
  x++;
  
  // Draw middle tiles (tile 1) repeated
  for (int s = 0; s < count; s++) {
    WriteTile8(bg, x, obj.y_, tiles[1]);
    x++;
  }
  
  // Draw end tile (tile 2)
  WriteTile8(bg, x, obj.y_, tiles[2]);
}

void ObjectDrawer::DrawRightwardsHasEdge1x1_1to16_plus23(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Long rail with corner/middle/end (object 0x5F)
  // ZScream: count = size + 21
  int size = obj.size_ & 0x0F;
  int count = size + 21;

  if (tiles.size() < 3) return;

  int x = obj.x_;
  WriteTile8(bg, x, obj.y_, tiles[0]);
  x++;

  for (int s = 0; s < count; s++) {
    WriteTile8(bg, x, obj.y_, tiles[1]);
    x++;
  }

  WriteTile8(bg, x, obj.y_, tiles[2]);
}

void ObjectDrawer::DrawRightwardsHasEdge1x1_1to16_plus2(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Rail with corner check (objects 0x23-0x2E carpet trim, 0x3F-0x46)
  // ASM: RoomDraw_RightwardsHasEdge1x1_1to16_plus2
  // Uses GetSize_1to16, count = size + 1
  // Structure: [CORNER if needed] -> [MIDDLE * count] -> [END]
  int size = obj.size_ & 0x0F;
  int count = size + 1;

  if (tiles.size() < 3) return;

  int x = obj.x_;
  
  // Draw corner tile (tile 0) - in editor we always draw it
  WriteTile8(bg, x, obj.y_, tiles[0]);
  x++;
  
  // Draw middle tiles (tile 1) repeated
  for (int s = 0; s < count; s++) {
    WriteTile8(bg, x, obj.y_, tiles[1]);
    x++;
  }
  
  // Draw end tile (tile 2)
  WriteTile8(bg, x, obj.y_, tiles[2]);
}

void ObjectDrawer::DrawRightwardsTopCorners1x2_1to16_plus13(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Top corner 1x2 tiles with +13 offset (object 0x2F)
  int size = obj.size_ & 0x0F;

  // Assembly: GetSize_1to16_timesA(0x0A), so count = size + 10
  int count = size + 10;

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 2) {
      // Use first tile span for 1x2 pattern
      WriteTile8(bg, obj.x_ + s + 13, obj.y_, tiles[0]);
      WriteTile8(bg, obj.x_ + s + 13, obj.y_ + 1, tiles[1]);
    }
  }
}

void ObjectDrawer::DrawRightwardsBottomCorners1x2_1to16_plus13(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    const std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Bottom corner 1x2 tiles with +13 offset (object 0x30)
  int size = obj.size_ & 0x0F;

  // Assembly: GetSize_1to16_timesA(0x0A), so count = size + 10
  int count = size + 10;

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 2) {
      // Use first tile span for 1x2 pattern
      WriteTile8(bg, obj.x_ + s + 13, obj.y_ + 1, tiles[0]);
      WriteTile8(bg, obj.x_ + s + 13, obj.y_ + 2, tiles[1]);
    }
  }
}

void ObjectDrawer::CustomDraw(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Custom draw routine (objects 0x31-0x32)
  // For now, fall back to simple 1x1
  if (tiles.size() >= 1) {
    // Use first 8x8 tile from span
    WriteTile8(bg, obj.x_, obj.y_, tiles[0]);
  }
}

void ObjectDrawer::DrawRightwards4x4_1to16(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: 4x4 block rightward (objects 0x33, 0xBA = large ceiling, etc.)
  int size = obj.size_ & 0x0F;

  // Assembly: GetSize_1to16, so count = size + 1
  int count = size + 1;

  // Debug: Log large ceiling objects (0xBA)
  if (obj.id_ == 0xBA && tiles.size() >= 16) {
    LOG_DEBUG("ObjectDrawer", "Large Ceiling Draw: obj=0x%02X pos=(%d,%d) size=%d tiles=%zu",
              obj.id_, obj.x_, obj.y_, size, tiles.size());
    LOG_DEBUG("ObjectDrawer", "  First 4 Tile IDs: [%d, %d, %d, %d]",
              tiles[0].id_, tiles[1].id_, tiles[2].id_, tiles[3].id_);
    LOG_DEBUG("ObjectDrawer", "  First 4 Palettes: [%d, %d, %d, %d]",
              tiles[0].palette_, tiles[1].palette_, tiles[2].palette_, tiles[3].palette_);
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

void ObjectDrawer::DrawRightwards1x1Solid_1to16_plus3(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: 1x1 solid tiles +3 offset (object 0x34)
  int size = obj.size_ & 0x0F;

  // Assembly: GetSize_1to16_timesA(4), so count = size + 4
  int count = size + 4;

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 1) {
      // Use first 8x8 tile from span
      WriteTile8(bg, obj.x_ + s + 3, obj.y_, tiles[0]);
    }
  }
}

void ObjectDrawer::DrawDoorSwitcherer(const RoomObject& obj,
                                      gfx::BackgroundBuffer& bg,
                                      std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Door switcher (object 0x35)
  // Special door logic
  // Check state to decide graphic
  int tile_index = 0;
  if (state && state->IsDoorSwitchActive(room_id_)) {
      // Use active tile if available (assuming 2nd tile is active state)
      if (tiles.size() >= 2) {
          tile_index = 1;
      }
  }

  if (tiles.size() > tile_index) {
    WriteTile8(bg, obj.x_, obj.y_, tiles[tile_index]);
  }
}

void ObjectDrawer::DrawRightwardsDecor4x4spaced2_1to16(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: 4x4 decoration with spacing (objects 0x36-0x37)
  int size = obj.size_ & 0x0F;

  // Assembly: GetSize_1to16, so count = size + 1
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 16) {
      // Draw 4x4 pattern with spacing in COLUMN-MAJOR order (matching assembly)
      for (int x = 0; x < 4; ++x) {
        for (int y = 0; y < 4; ++y) {
          WriteTile8(bg, obj.x_ + (s * 6) + x, obj.y_ + y, tiles[x * 4 + y]);
        }
      }
    }
  }
}

void ObjectDrawer::DrawRightwardsStatue2x3spaced2_1to16(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: 2x3 statue with spacing (object 0x38)
  // 2 columns × 3 rows = 6 tiles in COLUMN-MAJOR order
  int size = obj.size_ & 0x0F;

  // Assembly: GetSize_1to16, so count = size + 1
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 6) {
      // Draw 2x3 pattern in COLUMN-MAJOR order (matching assembly)
      for (int x = 0; x < 2; ++x) {
        for (int y = 0; y < 3; ++y) {
          WriteTile8(bg, obj.x_ + (s * 4) + x, obj.y_ + y, tiles[x * 3 + y]);
        }
      }
    }
  }
}

void ObjectDrawer::DrawRightwardsPillar2x4spaced4_1to16(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: 2x4 pillar with spacing (objects 0x39, 0x3D)
  // 2 columns × 4 rows = 8 tiles in COLUMN-MAJOR order
  // ASM: ADC #$0008 = 8 bytes = 4 tiles spacing between starts
  // Object is 2 tiles wide, so gap is 2 tiles
  int size = obj.size_ & 0x0F;

  // Assembly: GetSize_1to16, so count = size + 1
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 8) {
      // Draw 2x4 pattern in COLUMN-MAJOR order (matching assembly)
      // Spacing: 4 tiles between starts (object width 2 + gap 2) per ASM ADC #$0008
      for (int x = 0; x < 2; ++x) {
        for (int y = 0; y < 4; ++y) {
          WriteTile8(bg, obj.x_ + (s * 4) + x, obj.y_ + y, tiles[x * 4 + y]);
        }
      }
    }
  }
}

void ObjectDrawer::DrawRightwardsDecor4x3spaced4_1to16(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
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

void ObjectDrawer::DrawRightwardsDoubled2x2spaced2_1to16(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: 2x2 block repeated with a vertical gap (object 0x3C)
  int size = obj.size_ & 0x0F;
  int count = size + 1;

  if (tiles.size() < 4) return;

  for (int s = 0; s < count; s++) {
    int base_x = obj.x_ + (s * 4);

    // Top 2x2 block
    WriteTile8(bg, base_x, obj.y_, tiles[0]);
    WriteTile8(bg, base_x + 1, obj.y_, tiles[2]);
    WriteTile8(bg, base_x, obj.y_ + 1, tiles[1]);
    WriteTile8(bg, base_x + 1, obj.y_ + 1, tiles[3]);

    // Bottom 2x2 block (offset by 6 tiles)
    WriteTile8(bg, base_x, obj.y_ + 6, tiles[0]);
    WriteTile8(bg, base_x + 1, obj.y_ + 6, tiles[2]);
    WriteTile8(bg, base_x, obj.y_ + 7, tiles[1]);
    WriteTile8(bg, base_x + 1, obj.y_ + 7, tiles[3]);
  }
}

void ObjectDrawer::DrawRightwardsDecor2x2spaced12_1to16(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: 2x2 decoration with large spacing (object 0x3E)
  int size = obj.size_ & 0x0F;

  // Assembly: GetSize_1to16, so count = size + 1
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 4) {
      // Draw 2x2 pattern in COLUMN-MAJOR order (matching assembly)
      // tiles[0] → col 0, row 0 = top-left
      // tiles[1] → col 0, row 1 = bottom-left
      // tiles[2] → col 1, row 0 = top-right
      // tiles[3] → col 1, row 1 = bottom-right
      WriteTile8(bg, obj.x_ + (s * 14), obj.y_, tiles[0]);      // col 0, row 0
      WriteTile8(bg, obj.x_ + (s * 14), obj.y_ + 1, tiles[1]);  // col 0, row 1
      WriteTile8(bg, obj.x_ + (s * 14) + 1, obj.y_, tiles[2]);      // col 1, row 0
      WriteTile8(bg, obj.x_ + (s * 14) + 1, obj.y_ + 1, tiles[3]);  // col 1, row 1
    }
  }
}

void ObjectDrawer::DrawRightwards4x2_1to16(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Draws 4x2 tiles rightward (objects 0x49-0x4A: Floor Tile 4x2)
  // Assembly: RoomDraw_RightwardsFloorTile4x2_1to16 -> RoomDraw_Downwards4x2VariableSpacing
  // This is 4 columns × 2 rows = 8 tiles in ROW-MAJOR order with horizontal spacing
  // Spacing of $0008 = 4 tile columns = 32 pixels
  int size = obj.size_ & 0x0F;
  int count = size + 1;  // GetSize_1to16

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 8) {
      // Draw 4x2 pattern in ROW-MAJOR order (matching assembly RoomDraw_Downwards4x2VariableSpacing)
      // Row 0: tiles 0, 1, 2, 3 at x+0, x+1, x+2, x+3
      WriteTile8(bg, obj.x_ + (s * 4), obj.y_, tiles[0]);
      WriteTile8(bg, obj.x_ + (s * 4) + 1, obj.y_, tiles[1]);
      WriteTile8(bg, obj.x_ + (s * 4) + 2, obj.y_, tiles[2]);
      WriteTile8(bg, obj.x_ + (s * 4) + 3, obj.y_, tiles[3]);
      // Row 1: tiles 4, 5, 6, 7 at x+0, x+1, x+2, x+3
      WriteTile8(bg, obj.x_ + (s * 4), obj.y_ + 1, tiles[4]);
      WriteTile8(bg, obj.x_ + (s * 4) + 1, obj.y_ + 1, tiles[5]);
      WriteTile8(bg, obj.x_ + (s * 4) + 2, obj.y_ + 1, tiles[6]);
      WriteTile8(bg, obj.x_ + (s * 4) + 3, obj.y_ + 1, tiles[7]);
    }
  }
}

void ObjectDrawer::DrawRightwardsDecor4x2spaced8_1to16(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Draws 1x8 column tiles rightward with spacing (objects 0x55-0x56 wall torches)
  // Assembly: RoomDraw_RightwardsDecor4x2spaced8_1to16 -> RoomDraw_Downwards4x2VariableSpacing
  // ASM writes to 8 consecutive row buffers ([$BF] through [$D4]) = 1 column × 8 rows
  // Spacing = 0x18 = 24 bytes = 12 tile columns between repetitions
  int size = obj.size_ & 0x0F;
  int count = size + 1;  // GetSize_1to16

  if (tiles.size() < 8) return;

  for (int s = 0; s < count; s++) {
    // Draw 1x8 column pattern with 12-tile horizontal spacing
    int base_x = obj.x_ + (s * 12);  // spacing of 12 columns
    for (int row = 0; row < 8; row++) {
      WriteTile8(bg, base_x, obj.y_ + row, tiles[row]);
    }
  }
}

void ObjectDrawer::DrawRightwardsCannonHole4x3_1to16(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Draws 4x3 tiles (objects 0x51-0x52, 0x5B-0x5C: Cannon Hole)
  // Assembly: RoomDraw_RightwardsCannonHole4x3_1to16
  // This is 4 columns × 3 rows = 12 tiles in ROW-MAJOR order
  int size = obj.size_ & 0x0F;
  int count = size + 1;  // GetSize_1to16

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 12) {
      int base_x = obj.x_ + (s * 4);
      // Row 0
      WriteTile8(bg, base_x, obj.y_, tiles[0]);
      WriteTile8(bg, base_x + 1, obj.y_, tiles[1]);
      WriteTile8(bg, base_x + 2, obj.y_, tiles[2]);
      WriteTile8(bg, base_x + 3, obj.y_, tiles[3]);
      // Row 1
      WriteTile8(bg, base_x, obj.y_ + 1, tiles[4]);
      WriteTile8(bg, base_x + 1, obj.y_ + 1, tiles[5]);
      WriteTile8(bg, base_x + 2, obj.y_ + 1, tiles[6]);
      WriteTile8(bg, base_x + 3, obj.y_ + 1, tiles[7]);
      // Row 2
      WriteTile8(bg, base_x, obj.y_ + 2, tiles[8]);
      WriteTile8(bg, base_x + 1, obj.y_ + 2, tiles[9]);
      WriteTile8(bg, base_x + 2, obj.y_ + 2, tiles[10]);
      WriteTile8(bg, base_x + 3, obj.y_ + 2, tiles[11]);
    } else if (tiles.size() >= 8) {
      // Fallback: use 4x2 if we don't have enough tiles
      int base_x = obj.x_ + (s * 4);
      WriteTile8(bg, base_x, obj.y_, tiles[0]);
      WriteTile8(bg, base_x + 1, obj.y_, tiles[1]);
      WriteTile8(bg, base_x + 2, obj.y_, tiles[2]);
      WriteTile8(bg, base_x + 3, obj.y_, tiles[3]);
      WriteTile8(bg, base_x, obj.y_ + 1, tiles[4]);
      WriteTile8(bg, base_x + 1, obj.y_ + 1, tiles[5]);
      WriteTile8(bg, base_x + 2, obj.y_ + 1, tiles[6]);
      WriteTile8(bg, base_x + 3, obj.y_ + 1, tiles[7]);
    }
  }
}

// Additional Rightwards draw routines (0x47-0x5E range)

void ObjectDrawer::DrawRightwardsLine1x1_1to16plus1(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: 1x1 line rightward with +1 modifier (object 0x50)
  // Assembly: RoomDraw_RightwardsLine1x1_1to16plus1
  int size = obj.size_ & 0x0F;
  int count = size + 2;  // +1 gives +2 iterations total

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 1) {
      WriteTile8(bg, obj.x_ + s, obj.y_, tiles[0]);
    }
  }
}

void ObjectDrawer::DrawRightwardsBar4x3_1to16(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: 4x3 bar with left/right caps and repeating middle columns
  int size = obj.size_ & 0x0F;
  int count = size + 1;

  if (tiles.size() < 9) return;

  // Middle columns
  for (int s = 0; s < count; s++) {
    int base_x = obj.x_ + (s * 2);
    WriteTile8(bg, base_x + 1, obj.y_, tiles[3]);
    WriteTile8(bg, base_x + 2, obj.y_, tiles[3]);
    WriteTile8(bg, base_x + 1, obj.y_ + 1, tiles[4]);
    WriteTile8(bg, base_x + 2, obj.y_ + 1, tiles[4]);
    WriteTile8(bg, base_x + 1, obj.y_ + 2, tiles[5]);
    WriteTile8(bg, base_x + 2, obj.y_ + 2, tiles[5]);
  }

  // Left cap
  WriteTile8(bg, obj.x_, obj.y_, tiles[0]);
  WriteTile8(bg, obj.x_, obj.y_ + 1, tiles[1]);
  WriteTile8(bg, obj.x_, obj.y_ + 2, tiles[2]);

  // Right cap
  int right_x = obj.x_ + (size * 2) + 3;
  WriteTile8(bg, right_x, obj.y_, tiles[6]);
  WriteTile8(bg, right_x, obj.y_ + 1, tiles[7]);
  WriteTile8(bg, right_x, obj.y_ + 2, tiles[8]);
}

void ObjectDrawer::DrawRightwardsShelf4x4_1to16(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: 4x4 shelf rightward with complex column pattern (objects 0x4D-0x4F)
  // Assembly: RoomDraw_RightwardsShelf4x4_1to16
  // This draws 4x4 patterns with 6-tile X spacing
  int size = obj.size_ & 0x0F;
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 16) {
      int base_x = obj.x_ + (s * 6);  // 6-tile X spacing
      // Draw 4x4 pattern in COLUMN-MAJOR order
      for (int col = 0; col < 4; col++) {
        for (int row = 0; row < 4; row++) {
          int tile_idx = col * 4 + row;
          WriteTile8(bg, base_x + col, obj.y_ + row, tiles[tile_idx]);
        }
      }
    }
  }
}

void ObjectDrawer::DrawRightwardsBigRail1x3_1to16plus5(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Big rail with LEFT CAP (2x3) + MIDDLE (1x3 repeated) + RIGHT CAP (2x3)
  // Assembly: RoomDraw_RightwardsBigRail1x3_1to16plus5
  // Tile layout: [6 tiles for left cap] [3 tiles for middle] [6 tiles for right cap]
  // Left cap: 2 columns x 3 rows in COLUMN-MAJOR order
  // Middle: 1 column x 3 rows, repeated (size + 2) times
  // Right cap: 2 columns x 3 rows in COLUMN-MAJOR order
  
  int size = obj.size_ & 0x0F;
  int middle_count = size + 2;  // ASM: INC B2 after GetSize_1to16 = (size+1)+1 = size+2
  
  if (tiles.size() < 15) return;  // Need 6+3+6=15 tiles minimum
  
  int x = obj.x_;
  
  // Draw LEFT CAP: 2 columns x 3 rows (tiles 0-5 in column-major order)
  // Column 0
  WriteTile8(bg, x, obj.y_, tiles[0]);
  WriteTile8(bg, x, obj.y_ + 1, tiles[1]);
  WriteTile8(bg, x, obj.y_ + 2, tiles[2]);
  // Column 1
  WriteTile8(bg, x + 1, obj.y_, tiles[3]);
  WriteTile8(bg, x + 1, obj.y_ + 1, tiles[4]);
  WriteTile8(bg, x + 1, obj.y_ + 2, tiles[5]);
  x += 2;
  
  // Draw MIDDLE: 1 column x 3 rows (tiles 6-8), repeated middle_count times
  for (int s = 0; s < middle_count; s++) {
    WriteTile8(bg, x + s, obj.y_, tiles[6]);
    WriteTile8(bg, x + s, obj.y_ + 1, tiles[7]);
    WriteTile8(bg, x + s, obj.y_ + 2, tiles[8]);
  }
  x += middle_count;
  
  // Draw RIGHT CAP: 2 columns x 3 rows (tiles 9-14 in column-major order)
  // Column 0
  WriteTile8(bg, x, obj.y_, tiles[9]);
  WriteTile8(bg, x, obj.y_ + 1, tiles[10]);
  WriteTile8(bg, x, obj.y_ + 2, tiles[11]);
  // Column 1
  WriteTile8(bg, x + 1, obj.y_, tiles[12]);
  WriteTile8(bg, x + 1, obj.y_ + 1, tiles[13]);
  WriteTile8(bg, x + 1, obj.y_ + 2, tiles[14]);
}

void ObjectDrawer::DrawRightwardsBlock2x2spaced2_1to16(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: 2x2 block rightward with 4-tile X spacing (object 0x5E)
  // Assembly: RoomDraw_RightwardsBlock2x2spaced2_1to16 - uses RoomDraw_Rightwards2x2
  // Tiles are in COLUMN-MAJOR order: [top-left, bottom-left, top-right, bottom-right]
  int size = obj.size_ & 0x0F;
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 4) {
      int base_x = obj.x_ + (s * 4);  // 4-tile X spacing (INY x4 = 2 tiles)
      // Draw 2x2 pattern in COLUMN-MAJOR order (matching ASM)
      WriteTile8(bg, base_x, obj.y_, tiles[0]);       // top-left
      WriteTile8(bg, base_x, obj.y_ + 1, tiles[1]);   // bottom-left
      WriteTile8(bg, base_x + 1, obj.y_, tiles[2]);   // top-right
      WriteTile8(bg, base_x + 1, obj.y_ + 1, tiles[3]); // bottom-right
    }
  }
}

// ============================================================================
// Downwards Draw Routines
// ============================================================================

void ObjectDrawer::DrawDownwards2x2_1to15or32(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Draws 2x2 tiles downward (object 0x60)
  // Size byte determines how many times to repeat (1-15 or 32)
  int size = obj.size_;
  if (size == 0)
    size = 32;  // Special case for object 0x60

  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 4) {
      // Draw 2x2 pattern in COLUMN-MAJOR order (matching assembly)
      // Assembly uses indirect pointers: $BF, $CB, $C2, $CE
      // tiles[0] → $BF → (col 0, row 0) = top-left
      // tiles[1] → $CB → (col 0, row 1) = bottom-left
      // tiles[2] → $C2 → (col 1, row 0) = top-right
      // tiles[3] → $CE → (col 1, row 1) = bottom-right
      WriteTile8(bg, obj.x_, obj.y_ + (s * 2), tiles[0]);      // col 0, row 0
      WriteTile8(bg, obj.x_, obj.y_ + (s * 2) + 1, tiles[1]);  // col 0, row 1
      WriteTile8(bg, obj.x_ + 1, obj.y_ + (s * 2), tiles[2]);  // col 1, row 0
      WriteTile8(bg, obj.x_ + 1, obj.y_ + (s * 2) + 1,
                 tiles[3]);  // col 1, row 1
    }
  }
}

void ObjectDrawer::DrawDownwards4x2_1to15or26(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Draws 4x2 tiles downward (objects 0x61-0x62)
  // This is 4 columns × 2 rows = 8 tiles in ROW-MAJOR order (per ZScream)
  int size = obj.size_;
  if (size == 0)
    size = 26;  // Special case

  LOG_DEBUG("ObjectDrawer", "Wall Draw 4x2 Vertical: obj=0x%03X pos=(%d,%d) size=%d tiles=%zu",
            obj.id_, obj.x_, obj.y_, size, tiles.size());

  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 8) {
      // Draw 4x2 pattern in ROW-MAJOR order (matching ZScream)
      // Row 0: tiles 0, 1, 2, 3 at x+0, x+1, x+2, x+3
      WriteTile8(bg, obj.x_, obj.y_ + (s * 2), tiles[0]);
      WriteTile8(bg, obj.x_ + 1, obj.y_ + (s * 2), tiles[1]);
      WriteTile8(bg, obj.x_ + 2, obj.y_ + (s * 2), tiles[2]);
      WriteTile8(bg, obj.x_ + 3, obj.y_ + (s * 2), tiles[3]);
      // Row 1: tiles 4, 5, 6, 7 at x+0, x+1, x+2, x+3
      WriteTile8(bg, obj.x_, obj.y_ + (s * 2) + 1, tiles[4]);
      WriteTile8(bg, obj.x_ + 1, obj.y_ + (s * 2) + 1, tiles[5]);
      WriteTile8(bg, obj.x_ + 2, obj.y_ + (s * 2) + 1, tiles[6]);
      WriteTile8(bg, obj.x_ + 3, obj.y_ + (s * 2) + 1, tiles[7]);
    } else if (tiles.size() >= 4) {
      // Fallback: with 4 tiles draw 4x1 row pattern
      WriteTile8(bg, obj.x_, obj.y_ + (s * 2), tiles[0]);
      WriteTile8(bg, obj.x_ + 1, obj.y_ + (s * 2), tiles[1]);
      WriteTile8(bg, obj.x_ + 2, obj.y_ + (s * 2), tiles[2]);
      WriteTile8(bg, obj.x_ + 3, obj.y_ + (s * 2), tiles[3]);
    }
  }
}

void ObjectDrawer::DrawDownwards4x2_1to16_BothBG(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Same as above but draws to both BG1 and BG2 (objects 0x63-0x64)
  DrawDownwards4x2_1to15or26(obj, bg, tiles);
  // Note: BothBG would require access to both buffers - simplified for now
}

void ObjectDrawer::DrawDownwardsDecor4x2spaced4_1to16(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Draws 4x2 decoration downward with spacing (objects 0x65-0x66)
  // This is 4 columns × 2 rows = 8 tiles in COLUMN-MAJOR order with 6-tile Y spacing
  int size = obj.size_ & 0x0F;

  // Assembly: GetSize_1to16, so count = size + 1
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 8) {
      // Draw 4x2 pattern in COLUMN-MAJOR order
      // Column 0 (tiles 0-1)
      WriteTile8(bg, obj.x_, obj.y_ + (s * 6), tiles[0]);      // col 0, row 0
      WriteTile8(bg, obj.x_, obj.y_ + (s * 6) + 1, tiles[1]);  // col 0, row 1
      // Column 1 (tiles 2-3)
      WriteTile8(bg, obj.x_ + 1, obj.y_ + (s * 6), tiles[2]);      // col 1, row 0
      WriteTile8(bg, obj.x_ + 1, obj.y_ + (s * 6) + 1, tiles[3]);  // col 1, row 1
      // Column 2 (tiles 4-5)
      WriteTile8(bg, obj.x_ + 2, obj.y_ + (s * 6), tiles[4]);      // col 2, row 0
      WriteTile8(bg, obj.x_ + 2, obj.y_ + (s * 6) + 1, tiles[5]);  // col 2, row 1
      // Column 3 (tiles 6-7)
      WriteTile8(bg, obj.x_ + 3, obj.y_ + (s * 6), tiles[6]);      // col 3, row 0
      WriteTile8(bg, obj.x_ + 3, obj.y_ + (s * 6) + 1, tiles[7]);  // col 3, row 1
    }
  }
}

void ObjectDrawer::DrawDownwards2x2_1to16(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Draws 2x2 tiles downward (objects 0x67-0x68)
  int size = obj.size_ & 0x0F;

  // Assembly: GetSize_1to16, so count = size + 1
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 4) {
      // Draw 2x2 pattern in COLUMN-MAJOR order (matching assembly)
      // tiles[0] → col 0, row 0 = top-left
      // tiles[1] → col 0, row 1 = bottom-left
      // tiles[2] → col 1, row 0 = top-right
      // tiles[3] → col 1, row 1 = bottom-right
      WriteTile8(bg, obj.x_, obj.y_ + (s * 2), tiles[0]);      // col 0, row 0
      WriteTile8(bg, obj.x_, obj.y_ + (s * 2) + 1, tiles[1]);  // col 0, row 1
      WriteTile8(bg, obj.x_ + 1, obj.y_ + (s * 2), tiles[2]);      // col 1, row 0
      WriteTile8(bg, obj.x_ + 1, obj.y_ + (s * 2) + 1, tiles[3]);  // col 1, row 1
    }
  }
}

void ObjectDrawer::DrawDownwardsHasEdge1x1_1to16_plus3(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Vertical rail with corner/middle/end (object 0x69)
  int size = obj.size_ & 0x0F;
  int count = size + 1;

  if (tiles.size() < 3) return;

  int y = obj.y_;
  WriteTile8(bg, obj.x_, y, tiles[0]);
  y++;

  for (int s = 0; s < count; s++) {
    WriteTile8(bg, obj.x_, y, tiles[1]);
    y++;
  }

  WriteTile8(bg, obj.x_, y, tiles[2]);
}

void ObjectDrawer::DrawDownwardsHasEdge1x1_1to16_plus23(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Vertical rail with CORNER+MIDDLE+END pattern (objects 0x8A-0x8C)
  // ASM: RoomDraw_DownwardsHasEdge1x1_1to16_plus23 ($019314)
  // ZScream: count = size + 21
  // Structure: [CORNER] -> [MIDDLE * count] -> [END]
  int size = obj.size_ & 0x0F;
  int count = size + 21;

  if (tiles.size() < 3) return;

  int tile_y = obj.y_;
  
  // Draw corner tile (tile 0) - always draw in editor
  WriteTile8(bg, obj.x_, tile_y, tiles[0]);
  tile_y++;
  
  // Draw middle tiles (tile 1) repeated
  for (int s = 0; s < count; s++) {
    WriteTile8(bg, obj.x_, tile_y, tiles[1]);
    tile_y++;
  }
  
  // Draw end tile (tile 2)
  WriteTile8(bg, obj.x_, tile_y, tiles[2]);
}

void ObjectDrawer::DrawDownwardsEdge1x1_1to16(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: 1x1 edge tiles downward (objects 0x6A-0x6B)
  int size = obj.size_ & 0x0F;

  // Assembly: GetSize_1to16, so count = size + 1
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 1) {
      // Use first 8x8 tile from span
      WriteTile8(bg, obj.x_, obj.y_ + s, tiles[0]);
    }
  }
}

void ObjectDrawer::DrawDownwardsLeftCorners2x1_1to16_plus12(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Left corner 2x1 tiles with +12 offset downward (object 0x6C)
  int size = obj.size_ & 0x0F;

  // Assembly: GetSize_1to16_timesA(0x0A), so count = size + 10
  int count = size + 10;

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 2) {
      // Use first tile span for 2x1 pattern
      WriteTile8(bg, obj.x_ + 12, obj.y_ + s, tiles[0]);
      WriteTile8(bg, obj.x_ + 12 + 1, obj.y_ + s, tiles[1]);
    }
  }
}

void ObjectDrawer::DrawDownwardsRightCorners2x1_1to16_plus12(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Right corner 2x1 tiles with +12 offset downward (object 0x6D)
  int size = obj.size_ & 0x0F;

  // Assembly: GetSize_1to16_timesA(0x0A), so count = size + 10
  int count = size + 10;

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 2) {
      // Use first tile span for 2x1 pattern
      WriteTile8(bg, obj.x_ + 12, obj.y_ + s, tiles[0]);
      WriteTile8(bg, obj.x_ + 12 + 1, obj.y_ + s, tiles[1]);
    }
  }
}

// Additional Downwards draw routines (0x70-0x7F range)

void ObjectDrawer::DrawDownwardsFloor4x4_1to16(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Draws 4x4 floor tiles downward (object 0x70)
  // Assembly: RoomDraw_DownwardsFloor4x4_1to16 - adds $0200 to Y per iteration
  // $0200 = 512 = 4 tile rows (tilemap is 64 tiles wide = 128 bytes/row)
  int size = obj.size_ & 0x0F;
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 16) {
      // Draw 4x4 pattern in COLUMN-MAJOR order
      for (int col = 0; col < 4; col++) {
        for (int row = 0; row < 4; row++) {
          int tile_idx = col * 4 + row;
          WriteTile8(bg, obj.x_ + col, obj.y_ + (s * 4) + row, tiles[tile_idx]);
        }
      }
    }
  }
}

void ObjectDrawer::DrawDownwards1x1Solid_1to16_plus3(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: 1x1 solid tiles downward with +3 modifier (object 0x71)
  // Assembly: RoomDraw_Downwards1x1Solid_1to16_plus3 - adds $0080 to Y per iteration
  int size = obj.size_ & 0x0F;
  int count = size + 4;  // +3 gives +4 iterations total (1 + size + 3)

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 1) {
      WriteTile8(bg, obj.x_, obj.y_ + s, tiles[0]);
    }
  }
}

void ObjectDrawer::DrawDownwardsDecor4x4spaced2_1to16(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: 4x4 decoration with 2-tile spacing downward (objects 0x73-0x74)
  // Assembly: RoomDraw_DownwardsDecor4x4spaced2_1to16 - adds $0300 to Y per iteration
  // $0300 = 6 tile rows spacing
  int size = obj.size_ & 0x0F;
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 16) {
      // Draw 4x4 pattern in COLUMN-MAJOR order with 6-tile Y spacing
      for (int col = 0; col < 4; col++) {
        for (int row = 0; row < 4; row++) {
          int tile_idx = col * 4 + row;
          WriteTile8(bg, obj.x_ + col, obj.y_ + (s * 6) + row, tiles[tile_idx]);
        }
      }
    }
  }
}

void ObjectDrawer::DrawDownwardsPillar2x4spaced2_1to16(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: 2x4 pillar with 2-tile spacing downward (object 0x75, 0x87)
  // Assembly: RoomDraw_DownwardsPillar2x4spaced2_1to16 - adds $0300 to Y per iteration
  // $0300 = 6 tile rows spacing
  int size = obj.size_ & 0x0F;
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 8) {
      // Draw 2x4 pattern in COLUMN-MAJOR order with 6-tile Y spacing
      for (int col = 0; col < 2; col++) {
        for (int row = 0; row < 4; row++) {
          int tile_idx = col * 4 + row;
          WriteTile8(bg, obj.x_ + col, obj.y_ + (s * 6) + row, tiles[tile_idx]);
        }
      }
    }
  }
}

void ObjectDrawer::DrawDownwardsDecor3x4spaced4_1to16(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: 3x4 decoration with 4-tile spacing downward (objects 0x76-0x77)
  // Assembly: RoomDraw_DownwardsDecor3x4spaced4_1to16 - adds $0400 to Y per iteration
  // $0400 = 8 tile rows spacing
  int size = obj.size_ & 0x0F;
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 12) {
      // Draw 3x4 pattern in COLUMN-MAJOR order with 8-tile Y spacing
      for (int col = 0; col < 3; col++) {
        for (int row = 0; row < 4; row++) {
          int tile_idx = col * 4 + row;
          WriteTile8(bg, obj.x_ + col, obj.y_ + (s * 8) + row, tiles[tile_idx]);
        }
      }
    }
  }
}

void ObjectDrawer::DrawDownwardsDecor2x2spaced12_1to16(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: 2x2 decoration with 12-tile spacing downward (objects 0x78, 0x7B)
  // Assembly: RoomDraw_DownwardsDecor2x2spaced12_1to16 - adds $0600 to Y per iteration
  // $0600 = 14 tile rows spacing
  int size = obj.size_ & 0x0F;
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 4) {
      // Draw 2x2 pattern in COLUMN-MAJOR order with 14-tile Y spacing
      for (int col = 0; col < 2; col++) {
        for (int row = 0; row < 2; row++) {
          int tile_idx = col * 2 + row;
          WriteTile8(bg, obj.x_ + col, obj.y_ + (s * 14) + row, tiles[tile_idx]);
        }
      }
    }
  }
}

void ObjectDrawer::DrawDownwardsLine1x1_1to16plus1(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: 1x1 line downward with +1 modifier (object 0x7C)
  // Assembly: RoomDraw_DownwardsLine1x1_1to16plus1
  int size = obj.size_ & 0x0F;
  int count = size + 2;  // +1 gives +2 iterations total

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 1) {
      WriteTile8(bg, obj.x_, obj.y_ + s, tiles[0]);
    }
  }
}

void ObjectDrawer::DrawDownwardsDecor2x4spaced8_1to16(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: 2x4 decoration with 8-tile spacing downward (objects 0x7F, 0x80)
  // Assembly: RoomDraw_DownwardsDecor2x4spaced8_1to16 - adds $0500 to Y per iteration
  // $0500 = 10 tile rows spacing
  int size = obj.size_ & 0x0F;
  int count = size + 1;

  // Debug: Log vertical ceiling objects (0x80)
  if (obj.id_ == 0x80 && tiles.size() >= 8) {
    LOG_DEBUG("ObjectDrawer", "Vertical Ceiling Draw: obj=0x%02X pos=(%d,%d) size=%d tiles=%zu",
              obj.id_, obj.x_, obj.y_, size, tiles.size());
    LOG_DEBUG("ObjectDrawer", "  Tile IDs: [%d, %d, %d, %d, %d, %d, %d, %d]",
              tiles[0].id_, tiles[1].id_, tiles[2].id_, tiles[3].id_,
              tiles[4].id_, tiles[5].id_, tiles[6].id_, tiles[7].id_);
    LOG_DEBUG("ObjectDrawer", "  Palettes: [%d, %d, %d, %d, %d, %d, %d, %d]",
              tiles[0].palette_, tiles[1].palette_, tiles[2].palette_, tiles[3].palette_,
              tiles[4].palette_, tiles[5].palette_, tiles[6].palette_, tiles[7].palette_);
  }

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 8) {
      // Draw 2x4 pattern in COLUMN-MAJOR order with 10-tile Y spacing
      for (int col = 0; col < 2; col++) {
        for (int row = 0; row < 4; row++) {
          int tile_idx = col * 4 + row;
          WriteTile8(bg, obj.x_ + col, obj.y_ + (s * 10) + row, tiles[tile_idx]);
        }
      }
    }
  }
}

// ============================================================================
// Phase 4 Step 2: Simple Variant Routines (0x80-0x96, 0xB0-0xBD range)
// ============================================================================

void ObjectDrawer::DrawDownwardsDecor3x4spaced2_1to16(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: 3x4 decoration with 2-tile spacing downward (objects 0x81-0x84)
  // 3 cols × 4 rows = 12 tiles, with 6-tile Y spacing (4 tile height + 2 spacing)
  int size = obj.size_ & 0x0F;
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 12) {
      // Draw 3x4 pattern in COLUMN-MAJOR order with 6-tile Y spacing
      for (int col = 0; col < 3; col++) {
        for (int row = 0; row < 4; row++) {
          int tile_idx = col * 4 + row;
          WriteTile8(bg, obj.x_ + col, obj.y_ + (s * 6) + row, tiles[tile_idx]);
        }
      }
    }
  }
}

void ObjectDrawer::DrawDownwardsBigRail3x1_1to16plus5(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Big rail with TOP CAP (2x2) + MIDDLE (2x1 repeated) + BOTTOM CAP (2x3)
  // Assembly: RoomDraw_DownwardsBigRail3x1_1to16plus5
  // Tile layout: [4 tiles for top cap 2x2] [2 tiles for middle] [6 tiles for bottom cap 2x3]
  // Top cap: 2x2 in COLUMN-MAJOR order
  // Middle: 2 tiles wide (1 row), repeated (size + 1) times
  // Bottom cap: 2 columns x 3 rows in COLUMN-MAJOR order
  
  int size = obj.size_ & 0x0F;
  int middle_count = size + 1;  // GetSize_1to16 = size + 1
  
  if (tiles.size() < 12) return;  // Need 4+2+6=12 tiles minimum
  
  int y = obj.y_;
  
  // Draw TOP CAP: 2x2 block (tiles 0-3 in column-major order)
  // Column 0
  WriteTile8(bg, obj.x_, y, tiles[0]);
  WriteTile8(bg, obj.x_, y + 1, tiles[1]);
  // Column 1
  WriteTile8(bg, obj.x_ + 1, y, tiles[2]);
  WriteTile8(bg, obj.x_ + 1, y + 1, tiles[3]);
  y += 2;
  
  // Draw MIDDLE: 2 tiles wide x 1 row (tiles 4-5), repeated middle_count times
  for (int s = 0; s < middle_count; s++) {
    WriteTile8(bg, obj.x_, y + s, tiles[4]);
    WriteTile8(bg, obj.x_ + 1, y + s, tiles[5]);
  }
  y += middle_count;
  
  // Draw BOTTOM CAP: 2 columns x 3 rows (tiles 6-11 in column-major order)
  // Column 0
  WriteTile8(bg, obj.x_, y, tiles[6]);
  WriteTile8(bg, obj.x_, y + 1, tiles[7]);
  WriteTile8(bg, obj.x_, y + 2, tiles[8]);
  // Column 1
  WriteTile8(bg, obj.x_ + 1, y, tiles[9]);
  WriteTile8(bg, obj.x_ + 1, y + 1, tiles[10]);
  WriteTile8(bg, obj.x_ + 1, y + 2, tiles[11]);
}

void ObjectDrawer::DrawDownwardsBlock2x2spaced2_1to16(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: 2x2 block downward with 4-tile Y spacing (object 0x89)
  // Vertical version of RightwardsBlock2x2spaced2_1to16
  int size = obj.size_ & 0x0F;
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 4) {
      int base_y = obj.y_ + (s * 4);  // 4-tile Y spacing
      // Draw 2x2 pattern in COLUMN-MAJOR order
      WriteTile8(bg, obj.x_, base_y, tiles[0]);
      WriteTile8(bg, obj.x_, base_y + 1, tiles[1]);
      WriteTile8(bg, obj.x_ + 1, base_y, tiles[2]);
      WriteTile8(bg, obj.x_ + 1, base_y + 1, tiles[3]);
    }
  }
}

void ObjectDrawer::DrawDownwardsCannonHole3x6_1to16(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: 3x6 cannon hole downward (objects 0x85-0x86)
  // 3 cols × 6 rows = 18 tiles
  // Vertical version of RightwardsCannonHole4x3_1to16
  int size = obj.size_ & 0x0F;
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 18) {
      // Draw 3x6 pattern in COLUMN-MAJOR order
      for (int col = 0; col < 3; col++) {
        for (int row = 0; row < 6; row++) {
          int tile_idx = col * 6 + row;
          WriteTile8(bg, obj.x_ + col, obj.y_ + (s * 6) + row, tiles[tile_idx]);
        }
      }
    }
  }
}

void ObjectDrawer::DrawDownwardsBar2x3_1to16(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: 2x3 bar downward (object 0x8F)
  // 2 cols × 3 rows = 6 tiles
  int size = obj.size_ & 0x0F;
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 6) {
      // Draw 2x3 pattern in COLUMN-MAJOR order
      for (int col = 0; col < 2; col++) {
        for (int row = 0; row < 3; row++) {
          int tile_idx = col * 3 + row;
          WriteTile8(bg, obj.x_ + col, obj.y_ + (s * 3) + row, tiles[tile_idx]);
        }
      }
    }
  }
}

void ObjectDrawer::DrawDownwardsPots2x2_1to16(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: 2x2 pots downward (object 0x95)
  // Interactive object - draws 2x2 pattern vertically repeating
  int size = obj.size_ & 0x0F;
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 4) {
      int base_y = obj.y_ + (s * 2);
      // Draw 2x2 pattern in COLUMN-MAJOR order
      WriteTile8(bg, obj.x_, base_y, tiles[0]);
      WriteTile8(bg, obj.x_, base_y + 1, tiles[1]);
      WriteTile8(bg, obj.x_ + 1, base_y, tiles[2]);
      WriteTile8(bg, obj.x_ + 1, base_y + 1, tiles[3]);
    }
  }
}

void ObjectDrawer::DrawDownwardsHammerPegs2x2_1to16(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: 2x2 hammer pegs downward (object 0x96)
  // Interactive object - draws 2x2 pattern vertically repeating
  int size = obj.size_ & 0x0F;
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 4) {
      int base_y = obj.y_ + (s * 2);
      // Draw 2x2 pattern in COLUMN-MAJOR order
      WriteTile8(bg, obj.x_, base_y, tiles[0]);
      WriteTile8(bg, obj.x_, base_y + 1, tiles[1]);
      WriteTile8(bg, obj.x_ + 1, base_y, tiles[2]);
      WriteTile8(bg, obj.x_ + 1, base_y + 1, tiles[3]);
    }
  }
}

void ObjectDrawer::DrawRightwardsEdge1x1_1to16plus7(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: 1x1 edge tiles rightward with +7 offset (objects 0xB0-0xB1)
  // Assembly: RoomDraw_RightwardsEdge1x1_1to16plus7
  int size = obj.size_ & 0x0F;
  int count = size + 8;  // +7 gives +8 iterations total

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 1) {
      WriteTile8(bg, obj.x_ + s, obj.y_, tiles[0]);
    }
  }
}

void ObjectDrawer::DrawRightwardsPots2x2_1to16(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: 2x2 pots rightward (object 0xBC)
  // Interactive object - draws 2x2 pattern horizontally repeating
  int size = obj.size_ & 0x0F;
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 4) {
      int base_x = obj.x_ + (s * 2);
      // Draw 2x2 pattern in COLUMN-MAJOR order
      WriteTile8(bg, base_x, obj.y_, tiles[0]);
      WriteTile8(bg, base_x, obj.y_ + 1, tiles[1]);
      WriteTile8(bg, base_x + 1, obj.y_, tiles[2]);
      WriteTile8(bg, base_x + 1, obj.y_ + 1, tiles[3]);
    }
  }
}

void ObjectDrawer::DrawRightwardsHammerPegs2x2_1to16(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: 2x2 hammer pegs rightward (object 0xBD)
  // Interactive object - draws 2x2 pattern horizontally repeating
  int size = obj.size_ & 0x0F;
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 4) {
      int base_x = obj.x_ + (s * 2);
      // Draw 2x2 pattern in COLUMN-MAJOR order
      WriteTile8(bg, base_x, obj.y_, tiles[0]);
      WriteTile8(bg, base_x, obj.y_ + 1, tiles[1]);
      WriteTile8(bg, base_x + 1, obj.y_, tiles[2]);
      WriteTile8(bg, base_x + 1, obj.y_ + 1, tiles[3]);
    }
  }
}

// ============================================================================
// Phase 4 Step 3: Diagonal Ceiling Routines
// ============================================================================

void ObjectDrawer::DrawDiagonalCeilingTopLeft(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Diagonal ceiling top-left - TRIANGLE shape (objects 0xA0, 0xA5, 0xA9)
  // Assembly: RoomDraw_DiagonalCeilingTopLeftA/B with GetSize_1to16_timesA(4)
  // count = (size & 0x0F) + 4 per ASM
  int count = (obj.size_ & 0x0F) + 4;

  if (tiles.empty()) {
    LOG_DEBUG("ObjectDrawer", "DiagonalCeilingTopLeft: No tiles for obj 0x%02X", obj.id_);
    return;
  }

  LOG_DEBUG("ObjectDrawer", 
            "DiagonalCeilingTopLeft: obj=0x%02X pos=(%d,%d) size=%d count=%d",
            obj.id_, obj.x_, obj.y_, obj.size_, count);

  // Each row: starts with 'count' tiles, then count-1, count-2, etc.
  int tiles_in_row = count;
  for (int row = 0; row < count && tiles_in_row > 0; row++) {
    for (int col = 0; col < tiles_in_row; col++) {
      WriteTile8(bg, obj.x_ + col, obj.y_ + row, tiles[0]);
    }
    tiles_in_row--;  // One fewer tile each row (DEC $B2)
  }
}

void ObjectDrawer::DrawDiagonalCeilingBottomLeft(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Diagonal ceiling bottom-left - TRIANGLE shape (objects 0xA1, 0xA6, 0xAA)
  // Assembly: RoomDraw_DiagonalCeilingBottomLeftA/B with GetSize_1to16_timesA(4)
  // count = (size & 0x0F) + 4 per ASM
  int count = (obj.size_ & 0x0F) + 4;

  if (tiles.empty()) return;

  // Row 0: 1 tile, Row 1: 2 tiles, ..., Row count-1: count tiles
  for (int row = 0; row < count; row++) {
    int tiles_in_row = row + 1;
    for (int col = 0; col < tiles_in_row; col++) {
      WriteTile8(bg, obj.x_ + col, obj.y_ + row, tiles[0]);
    }
  }
}

void ObjectDrawer::DrawDiagonalCeilingTopRight(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Diagonal ceiling top-right - TRIANGLE shape (objects 0xA2, 0xA7, 0xAB)
  // Assembly: RoomDraw_DiagonalCeilingTopRightA/B with GetSize_1to16_timesA(4)
  // count = (size & 0x0F) + 4 per ASM
  int count = (obj.size_ & 0x0F) + 4;

  if (tiles.empty()) return;

  // Row 0 at (x,y): count tiles, Row 1 at (x+1,y+1): count-1 tiles, etc.
  int tiles_in_row = count;
  for (int row = 0; row < count && tiles_in_row > 0; row++) {
    for (int col = 0; col < tiles_in_row; col++) {
      WriteTile8(bg, obj.x_ + row + col, obj.y_ + row, tiles[0]);
    }
    tiles_in_row--;
  }
}

void ObjectDrawer::DrawDiagonalCeilingBottomRight(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Diagonal ceiling bottom-right - TRIANGLE shape (objects 0xA3, 0xA8, 0xAC)
  // Assembly: RoomDraw_DiagonalCeilingBottomRightA/B with GetSize_1to16_timesA(4)
  // count = (size & 0x0F) + 4 per ASM
  int count = (obj.size_ & 0x0F) + 4;

  if (tiles.empty()) return;

  // Row 0 at (x, y): count tiles, Row 1 at (x+1, y-1): count-1 tiles, etc.
  int tiles_in_row = count;
  for (int row = 0; row < count && tiles_in_row > 0; row++) {
    for (int col = 0; col < tiles_in_row; col++) {
      WriteTile8(bg, obj.x_ + row + col, obj.y_ - row, tiles[0]);
    }
    tiles_in_row--;
  }
}

// ============================================================================
// Phase 4 Step 5: Special Routines (0xC1, 0xCD, 0xCE, 0xDC)
// ============================================================================

void ObjectDrawer::DrawClosedChestPlatform(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles) {
  // ASM: RoomDraw_ClosedChestPlatform ($018CC7)
  //
  // Structure:
  //   LDA.b $B2         ; Width size
  //   CLC
  //   ADC.w #$0004      ; width = B2 + 4
  //   STA.b $B2
  //   STA.b $0A
  //   INC.b $B4         ; height = B4 + 1
  //   JSR RoomDraw_ChestPlatformHorizontalWallWithCorners
  //   ... vertical walls on sides ...
  //
  // The platform has 68 tiles forming a frame + carpet pattern.
  // Layer handling uses same $BF check as OpenChestPlatform.
  
  if (tiles.size() < 16) return;

  // width = B2 + 4, height = B4 + 1
  int width = (obj.size_ & 0x0F) + 4;
  int height = ((obj.size_ >> 4) & 0x0F) + 1;

  LOG_DEBUG("ObjectDrawer",
            "DrawClosedChestPlatform: obj=0x%03X pos=(%d,%d) size=0x%02X width=%d height=%d",
            obj.id_, obj.x_, obj.y_, obj.size_, width, height);

  // Draw outer frame + interior carpet pattern
  // The routine builds a rectangular platform with walls and floor
  size_t tile_idx = 0;
  for (int row = 0; row < height * 3; ++row) {
    for (int col = 0; col < width * 2; ++col) {
      WriteTile8(bg, obj.x_ + col, obj.y_ + row, tiles[tile_idx % tiles.size()]);
      tile_idx++;
    }
  }
}

void ObjectDrawer::DrawMovingWallWest(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles) {
  // ASM: RoomDraw_MovingWallWest ($019190)
  // Draw vertical wall structure (28 tiles) - always draw in editor
  if (tiles.size() < 6) return;

  int count = ((obj.size_ >> 4) & 0x0F) + 4;

  // Draw wall columns
  for (int row = 0; row < count; ++row) {
    for (int col = 0; col < 3; ++col) {
      size_t tile_idx = (row * 3 + col) % tiles.size();
      WriteTile8(bg, obj.x_ + col, obj.y_ + row, tiles[tile_idx]);
    }
  }
}

void ObjectDrawer::DrawMovingWallEast(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles) {
  // ASM: RoomDraw_MovingWallEast ($01921C)
  // Mirror of West - draws from right-to-left
  if (tiles.size() < 6) return;

  int count = ((obj.size_ >> 4) & 0x0F) + 4;

  for (int row = 0; row < count; ++row) {
    for (int col = 0; col < 3; ++col) {
      size_t tile_idx = (row * 3 + col) % tiles.size();
      WriteTile8(bg, obj.x_ - col, obj.y_ + row, tiles[tile_idx]);
    }
  }
}

void ObjectDrawer::DrawOpenChestPlatform(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles) {
  // ASM: RoomDraw_OpenChestPlatform ($019733)
  // 
  // Layer handling from ASM:
  //   LDA.b $BF         ; Load current layer pointer
  //   CMP.w #$4000      ; Compare with lower_layer ($4000)
  //   BNE .upper_layer  ; Skip if upper layer
  //   TYA
  //   ORA.w #$2000      ; Set BG2 priority bit for lower layer
  //   TAY
  //
  // In the editor, layer handling is done via the object's layer_ field
  // and the DrawObject() method routes to the appropriate buffer.
  // The BG2 priority bit is not directly applicable here since we use
  // separate bitmaps for compositing.

  if (tiles.size() < 8) return;

  // width = B2 + 1 (ASM: INC.b $B2)
  int width = (obj.size_ & 0x0F) + 1;
  
  // segments = (B4 * 2) + 5 (ASM: LDA.b $B4; ASL A; CLC; ADC.w #$0005)
  int segments = ((obj.size_ >> 4) & 0x0F) * 2 + 5;

  LOG_DEBUG("ObjectDrawer",
            "DrawOpenChestPlatform: obj=0x%03X pos=(%d,%d) size=0x%02X width=%d segments=%d",
            obj.id_, obj.x_, obj.y_, obj.size_, width, segments);

  // ASM: obj0AB4 is the tile data source (21 tiles total)
  // The routine draws segments, each segment is a row of 'width' tiles
  // Then draws two additional segments at the end (INY INY; JSR .draw_segment; INY INY)
  
  // Draw the main segments
  int tile_offset = 0;
  for (int seg = 0; seg < segments; ++seg) {
    for (int col = 0; col < width; ++col) {
      size_t tile_idx = tile_offset % tiles.size();
      WriteTile8(bg, obj.x_ + col, obj.y_ + seg, tiles[tile_idx]);
      tile_offset++;
    }
  }
  
  // Draw final two segments (ASM: INY INY; JSR .draw_segment; INY INY; JSR .draw_segment)
  // These use tiles at offset +2 and +4 from the current position
  tile_offset += 2;
  for (int col = 0; col < width; ++col) {
    size_t tile_idx = tile_offset % tiles.size();
    WriteTile8(bg, obj.x_ + col, obj.y_ + segments, tiles[tile_idx]);
    tile_offset++;
  }
  tile_offset += 2;
  for (int col = 0; col < width; ++col) {
    size_t tile_idx = tile_offset % tiles.size();
    WriteTile8(bg, obj.x_ + col, obj.y_ + segments + 1, tiles[tile_idx]);
    tile_offset++;
  }
}

// ============================================================================
// Utility Methods
// ============================================================================

void ObjectDrawer::MarkBG1Transparent(gfx::BackgroundBuffer& bg1, int tile_x,
                                       int tile_y, int pixel_width,
                                       int pixel_height) {
  auto& bitmap = bg1.bitmap();
  if (!bitmap.is_active() || bitmap.width() == 0) {
    LOG_DEBUG("ObjectDrawer", "MarkBG1Transparent: Bitmap not ready, skipping");
    return;  // Bitmap not ready
  }

  int start_px = tile_x * 8;
  int start_py = tile_y * 8;
  int canvas_width = bitmap.width();
  int canvas_height = bitmap.height();
  auto& data = bitmap.mutable_data();

  int pixels_marked = 0;
  
  // Mark pixels as transparent (255) where BG2 overlay objects are drawn
  // This creates "holes" in BG1 that allow BG2 content to show through
  for (int py = start_py; py < start_py + pixel_height && py < canvas_height;
       py++) {
    if (py < 0) continue;
    for (int px = start_px; px < start_px + pixel_width && px < canvas_width;
         px++) {
      if (px < 0) continue;
      int idx = py * canvas_width + px;
      if (idx >= 0 && idx < static_cast<int>(data.size())) {
        data[idx] = 255;  // 255 = transparent in our compositing system
        pixels_marked++;
      }
    }
  }
  bitmap.set_modified(true);

  LOG_DEBUG("ObjectDrawer",
            "MarkBG1Transparent: Marked %d pixels at tile(%d,%d) pixel(%d,%d) size(%d,%d)",
            pixels_marked, tile_x, tile_y, start_px, start_py, pixel_width, pixel_height);
}

void ObjectDrawer::WriteTile8(gfx::BackgroundBuffer& bg, int tile_x, int tile_y,
                              const gfx::TileInfo& tile_info) {
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
  
  // Also update priority buffer with tile's priority bit
  // Priority (over_) affects Z-ordering in SNES Mode 1 compositing
  uint8_t priority = tile_info.over_ ? 1 : 0;
  int pixel_x = tile_x * 8;
  int pixel_y = tile_y * 8;
  auto& priority_buffer = bg.mutable_priority_data();
  int width = bitmap.width();
  
  // Update priority for each pixel in the 8x8 tile
  const auto& bitmap_data = bitmap.vector();
  for (int py = 0; py < 8; py++) {
    int dest_y = pixel_y + py;
    if (dest_y < 0 || dest_y >= bitmap.height()) continue;
    
    for (int px = 0; px < 8; px++) {
      int dest_x = pixel_x + px;
      if (dest_x < 0 || dest_x >= width) continue;
      
      int dest_index = dest_y * width + dest_x;
      // Only set priority for non-transparent pixels
      // Check if this pixel was actually drawn (not transparent)
      if (dest_index < static_cast<int>(bitmap_data.size()) &&
          bitmap_data[dest_index] != 255) {
        priority_buffer[dest_index] = priority;
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
  if (!tiledata) return;

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

  int tile_base_x = tile_col * 8;    // 8 bytes per tile horizontally
  int tile_base_y = tile_row * 1024; // 1024 bytes per tile row (8 rows * 128 bytes)

  // DEBUG: Log first few tiles being drawn with their graphics data
  static int draw_debug_count = 0;
  if (draw_debug_count < 5) {
    int sample_index = tile_base_y + tile_base_x;
    LOG_DEBUG("ObjectDrawer", "DrawTile: id=%d (col=%d,row=%d) gfx_offset=%d (0x%04X)",
              tile_info.id_, tile_col, tile_row, sample_index, sample_index);
    draw_debug_count++;
  }

  // Palette offset calculation using 16-color bank chunking (matches SNES CGRAM)
  //
  // SNES CGRAM layout:
  // - Each CGRAM row has 16 colors, with index 0 being transparent
  // - Dungeon tiles use palette bits 2-7, mapping to CGRAM rows 2-7
  // - We map palette bits 2-7 to SDL banks 0-5
  //
  // Drawing formula: final_color = pixel + (bank * 16)
  // Where pixel 0 = transparent (not written), pixel 1-15 = colors within bank
  uint8_t pal = tile_info.palette_ & 0x07;
  uint8_t palette_offset;
  if (pal >= 2 && pal <= 7) {
    // Map palette bits 2-7 to SDL banks 0-5 using 16-color stride
    palette_offset = (pal - 2) * 16;
  } else {
    // Palette 0-1 are for HUD/other - fallback to first bank
    palette_offset = 0;
  }

  // Draw 8x8 pixels
  bool any_pixels_written = false;

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

      if (pixel != 0) {
        // Pixel 0 is transparent (not written). Pixels 1-15 map to bank indices 1-15.
        // With 16-color bank chunking: final_color = pixel + (bank * 16)
        uint8_t final_color = pixel + palette_offset;
        int dest_x = pixel_x + px;
        int dest_y = pixel_y + py;

        if (dest_x >= 0 && dest_x < bitmap.width() &&
            dest_y >= 0 && dest_y < bitmap.height()) {
          int dest_index = dest_y * bitmap.width() + dest_x;
          if (dest_index >= 0 &&
              dest_index < static_cast<int>(bitmap.mutable_data().size())) {
            bitmap.mutable_data()[dest_index] = final_color;
            any_pixels_written = true;
          }
        }
      }
    }
  }

  // Mark bitmap as modified if we wrote any pixels
  if (any_pixels_written) {
    bitmap.set_modified(true);
  }
}

// ============================================================================
// Type 3 / Special Routine Implementations
// ============================================================================

void ObjectDrawer::DrawSomariaLine(const RoomObject& obj,
                                   gfx::BackgroundBuffer& bg,
                                   std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Somaria Line (objects 0x203-0x20F, 0x214)
  // Draws a line of tiles based on direction encoded in object ID
  // Direction mapping based on ZScream reference:
  //   0x203: Horizontal right
  //   0x204: Vertical down
  //   0x205: Diagonal down-right
  //   0x206: Diagonal down-left
  //   0x207-0x209: Variations
  //   0x20A-0x20C: More variations
  //   0x20E-0x20F: Additional patterns
  //   0x214: Another line type

  if (tiles.empty()) return;

  int length = (obj.size_ & 0x0F) + 1;
  int obj_subid = obj.id_ & 0x0F;  // Low nibble determines direction

  // Determine direction based on object sub-ID
  int dx = 1, dy = 0;  // Default: horizontal right
  switch (obj_subid) {
    case 0x03: dx = 1; dy = 0; break;   // Horizontal right
    case 0x04: dx = 0; dy = 1; break;   // Vertical down
    case 0x05: dx = 1; dy = 1; break;   // Diagonal down-right
    case 0x06: dx = -1; dy = 1; break;  // Diagonal down-left
    case 0x07: dx = 1; dy = 0; break;   // Horizontal (variant)
    case 0x08: dx = 0; dy = 1; break;   // Vertical (variant)
    case 0x09: dx = 1; dy = 1; break;   // Diagonal (variant)
    case 0x0A: dx = 1; dy = 0; break;   // Horizontal
    case 0x0B: dx = 0; dy = 1; break;   // Vertical
    case 0x0C: dx = 1; dy = 1; break;   // Diagonal
    case 0x0E: dx = 1; dy = 0; break;   // Horizontal
    case 0x0F: dx = 0; dy = 1; break;   // Vertical
    default: dx = 1; dy = 0; break;     // Default horizontal
  }

  // Draw tiles along the path using first tile (Somaria uses single tile)
  for (int i = 0; i < length; ++i) {
    int tile_idx = i % tiles.size();  // Cycle through tiles if multiple
    WriteTile8(bg, obj.x_ + (i * dx), obj.y_ + (i * dy), tiles[tile_idx]);
  }
}

void ObjectDrawer::DrawWaterFace(const RoomObject& obj,
                                 gfx::BackgroundBuffer& bg,
                                 std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Water Face (Type 3 objects 0xF80-0xF82)
  // Draws a 2x2 face in COLUMN-MAJOR order
  // TODO: Implement state check from RoomDraw_EmptyWaterFace ($019D29)
  // Checks Room ID ($AF), Room State ($7EF000), Door Flags ($0403) to switch graphic
  if (tiles.size() >= 4) {
    WriteTile8(bg, obj.x_, obj.y_, tiles[0]);      // col 0, row 0
    WriteTile8(bg, obj.x_, obj.y_ + 1, tiles[1]);  // col 0, row 1
    WriteTile8(bg, obj.x_ + 1, obj.y_, tiles[2]);      // col 1, row 0
    WriteTile8(bg, obj.x_ + 1, obj.y_ + 1, tiles[3]);  // col 1, row 1
  }
}

void ObjectDrawer::Draw4x4Corner_BothBG(const RoomObject& obj,
                                        gfx::BackgroundBuffer& bg,
                                        std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: 4x4 Corner for Both BG (objects 0x108-0x10F for Type 2)
  // Type 3 objects (0xF9B-0xF9D) only have 8 tiles, draw 2x4 pattern
  if (tiles.size() >= 16) {
    DrawCorner4x4(obj, bg, tiles);
  } else if (tiles.size() >= 8) {
    // Draw 2x4 corner pattern (column-major)
    int tid = 0;
    for (int xx = 0; xx < 2; xx++) {
      for (int yy = 0; yy < 4; yy++) {
        WriteTile8(bg, obj.x_ + xx, obj.y_ + yy, tiles[tid++]);
      }
    }
  } else if (tiles.size() >= 4) {
    // Fallback: 2x2 pattern
    DrawWaterFace(obj, bg, tiles);
  }
}

void ObjectDrawer::DrawWeirdCornerBottom_BothBG(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Weird Corner Bottom (objects 0x110-0x113 for Type 2)
  // ASM: RoomDraw_WeirdCornerBottom_BothBG sets count=3, uses 4x4Corner pattern
  // Pattern: 3 columns × 4 rows = 12 tiles, column-major order
  if (tiles.size() >= 16) {
    DrawCorner4x4(obj, bg, tiles);
  } else if (tiles.size() >= 12) {
    // Draw 3x4 corner pattern (column-major, 3 columns × 4 rows)
    int tid = 0;
    for (int xx = 0; xx < 3; xx++) {
      for (int yy = 0; yy < 4; yy++) {
        WriteTile8(bg, obj.x_ + xx, obj.y_ + yy, tiles[tid++]);
      }
    }
  } else if (tiles.size() >= 8) {
    // Fallback: 2x4 column-major pattern
    int tid = 0;
    for (int xx = 0; xx < 2; xx++) {
      for (int yy = 0; yy < 4; yy++) {
        WriteTile8(bg, obj.x_ + xx, obj.y_ + yy, tiles[tid++]);
      }
    }
  } else if (tiles.size() >= 4) {
    DrawWaterFace(obj, bg, tiles);
  }
}

void ObjectDrawer::DrawWeirdCornerTop_BothBG(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Weird Corner Top (objects 0x114-0x117 for Type 2)
  // ASM: RoomDraw_WeirdCornerTop_BothBG draws 4 columns of 3 tiles each
  // Pattern: 4 columns × 3 rows = 12 tiles, column-major order
  if (tiles.size() >= 16) {
    DrawCorner4x4(obj, bg, tiles);
  } else if (tiles.size() >= 12) {
    // Draw 4x3 corner pattern (column-major, 4 columns × 3 rows)
    int tid = 0;
    for (int xx = 0; xx < 4; xx++) {
      for (int yy = 0; yy < 3; yy++) {
        WriteTile8(bg, obj.x_ + xx, obj.y_ + yy, tiles[tid++]);
      }
    }
  } else if (tiles.size() >= 8) {
    // Fallback: 2x4 column-major pattern
    int tid = 0;
    for (int xx = 0; xx < 2; xx++) {
      for (int yy = 0; yy < 4; yy++) {
        WriteTile8(bg, obj.x_ + xx, obj.y_ + yy, tiles[tid++]);
      }
    }
  } else if (tiles.size() >= 4) {
    DrawWaterFace(obj, bg, tiles);
  }
}

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

// ============================================================================
// Type 2 Special Object Routines
// ============================================================================

void ObjectDrawer::DrawBed4x5(const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
  // Pattern: 4 wide x 5 tall (20 tiles) in column-major order
  // Objects: 0x122, 0x128
  constexpr int kWidth = 4;
  constexpr int kHeight = 5;
  
  if (tiles.size() >= kWidth * kHeight) {
    // Column-major layout (tiles go down first, then across)
    int tid = 0;
    for (int x = 0; x < kWidth && tid < static_cast<int>(tiles.size()); ++x) {
      for (int y = 0; y < kHeight && tid < static_cast<int>(tiles.size()); ++y) {
        WriteTile8(bg, obj.x_ + x, obj.y_ + y, tiles[tid++]);
      }
    }
  } else {
    // Fallback: use 4x4 pattern
    DrawRightwards4x4_1to16(obj, bg, tiles);
  }
}

void ObjectDrawer::DrawRightwards3x6(const RoomObject& obj,
                                     gfx::BackgroundBuffer& bg,
                                     std::span<const gfx::TileInfo> tiles,
                                     [[maybe_unused]] const DungeonState* state) {
  // Pattern: 3 wide x 6 tall (18 tiles) in column-major order
  // Objects: 0x12C, 0x236, 0x237
  constexpr int kWidth = 3;
  constexpr int kHeight = 6;
  
  if (tiles.size() >= kWidth * kHeight) {
    int tid = 0;
    for (int x = 0; x < kWidth && tid < static_cast<int>(tiles.size()); ++x) {
      for (int y = 0; y < kHeight && tid < static_cast<int>(tiles.size()); ++y) {
        WriteTile8(bg, obj.x_ + x, obj.y_ + y, tiles[tid++]);
      }
    }
  } else {
    // Fallback: use available tiles in 4x4 pattern
    DrawRightwards4x4_1to16(obj, bg, tiles);
  }
}

void ObjectDrawer::DrawUtility6x3(const RoomObject& obj,
                                  gfx::BackgroundBuffer& bg,
                                  std::span<const gfx::TileInfo> tiles,
                                  [[maybe_unused]] const DungeonState* state) {
  // Pattern: 6 wide x 3 tall (18 tiles) in row-major order
  // Objects: 0x13E, 0x24D, 0x25D
  constexpr int kWidth = 6;
  constexpr int kHeight = 3;
  
  if (tiles.size() >= kWidth * kHeight) {
    int tid = 0;
    for (int y = 0; y < kHeight && tid < static_cast<int>(tiles.size()); ++y) {
      for (int x = 0; x < kWidth && tid < static_cast<int>(tiles.size()); ++x) {
        WriteTile8(bg, obj.x_ + x, obj.y_ + y, tiles[tid++]);
      }
    }
  } else {
    // Fallback: use 4x3 pattern
    DrawRightwardsDecor4x3spaced4_1to16(obj, bg, tiles);
  }
}

void ObjectDrawer::DrawUtility3x5(const RoomObject& obj,
                                  gfx::BackgroundBuffer& bg,
                                  std::span<const gfx::TileInfo> tiles,
                                  [[maybe_unused]] const DungeonState* state) {
  // Pattern: 3 wide x 5 tall (15 tiles) in column-major order
  // Objects: 0x255, 0x25B
  constexpr int kWidth = 3;
  constexpr int kHeight = 5;
  
  if (tiles.size() >= kWidth * kHeight) {
    int tid = 0;
    for (int x = 0; x < kWidth && tid < static_cast<int>(tiles.size()); ++x) {
      for (int y = 0; y < kHeight && tid < static_cast<int>(tiles.size()); ++y) {
        WriteTile8(bg, obj.x_ + x, obj.y_ + y, tiles[tid++]);
      }
    }
  } else {
    // Fallback: use 4x4 pattern
    DrawRightwards4x4_1to16(obj, bg, tiles);
  }
}

// ============================================================================
// Type 3 Special Object Routines
// ============================================================================

void ObjectDrawer::DrawVerticalTurtleRockPipe(const RoomObject& obj,
                                              gfx::BackgroundBuffer& bg,
                                              std::span<const gfx::TileInfo> tiles,
                                              [[maybe_unused]] const DungeonState* state) {
  // Pattern: Vertical pipe - 2 wide x 6 tall (12 tiles) in column-major order
  // Objects: 0xFBA, 0xFBB (ASM 23A, 23B)
  constexpr int kWidth = 2;
  constexpr int kHeight = 6;

  if (tiles.empty()) return;

  int tid = 0;
  int num_tiles = static_cast<int>(tiles.size());
  for (int x = 0; x < kWidth; ++x) {
    for (int y = 0; y < kHeight; ++y) {
      WriteTile8(bg, obj.x_ + x, obj.y_ + y, tiles[tid % num_tiles]);
      tid++;
    }
  }
}

void ObjectDrawer::DrawHorizontalTurtleRockPipe(const RoomObject& obj,
                                                gfx::BackgroundBuffer& bg,
                                                std::span<const gfx::TileInfo> tiles,
                                                [[maybe_unused]] const DungeonState* state) {
  // Pattern: Horizontal pipe - 6 wide x 2 tall (12 tiles) in row-major order
  // Objects: 0xFBC, 0xFBD, 0xFDC (ASM 23C, 23D, 25C)
  constexpr int kWidth = 6;
  constexpr int kHeight = 2;

  if (tiles.empty()) return;

  int tid = 0;
  int num_tiles = static_cast<int>(tiles.size());
  for (int y = 0; y < kHeight; ++y) {
    for (int x = 0; x < kWidth; ++x) {
      WriteTile8(bg, obj.x_ + x, obj.y_ + y, tiles[tid % num_tiles]);
      tid++;
    }
  }
}

void ObjectDrawer::DrawLightBeam(const RoomObject& obj,
                                 gfx::BackgroundBuffer& bg,
                                 std::span<const gfx::TileInfo> tiles,
                                 [[maybe_unused]] const DungeonState* state) {
  // Pattern: Light beam on floor - 4x4 grid
  // Objects: 0xFF0 (ASM 270)
  // Standard 4x4 pattern, but tiles may have special transparency
  constexpr int kWidth = 4;
  constexpr int kHeight = 4;
  
  if (tiles.size() >= kWidth * kHeight) {
    int tid = 0;
    for (int y = 0; y < kHeight && tid < static_cast<int>(tiles.size()); ++y) {
      for (int x = 0; x < kWidth && tid < static_cast<int>(tiles.size()); ++x) {
        WriteTile8(bg, obj.x_ + x, obj.y_ + y, tiles[tid++]);
      }
    }
  } else {
    // Fallback
    DrawRightwards4x4_1to16(obj, bg, tiles);
  }
}

void ObjectDrawer::DrawBigLightBeam(const RoomObject& obj,
                                    gfx::BackgroundBuffer& bg,
                                    std::span<const gfx::TileInfo> tiles,
                                    [[maybe_unused]] const DungeonState* state) {
  // Pattern: Big light beam - 6x6 grid
  // Objects: 0xFF1 (ASM 271)
  constexpr int kWidth = 6;
  constexpr int kHeight = 6;
  
  if (tiles.size() >= kWidth * kHeight) {
    int tid = 0;
    for (int y = 0; y < kHeight && tid < static_cast<int>(tiles.size()); ++y) {
      for (int x = 0; x < kWidth && tid < static_cast<int>(tiles.size()); ++x) {
        WriteTile8(bg, obj.x_ + x, obj.y_ + y, tiles[tid++]);
      }
    }
  } else {
    // Fallback: use 4x4 pattern
    DrawRightwards4x4_1to16(obj, bg, tiles);
  }
}

void ObjectDrawer::DrawBossShell4x4(const RoomObject& obj,
                                    gfx::BackgroundBuffer& bg,
                                    std::span<const gfx::TileInfo> tiles,
                                    [[maybe_unused]] const DungeonState* state) {
  // Pattern: Boss shell (Trinexx, Vitreous, Kholdstare) - 4x4 grid
  // Objects: 0xFF2 (Trinexx 272), 0xF95 (Kholdstare 215), 0xFFB (Vitreous 27B)
  // Uses standard 4x4 pattern but may have state-dependent tile selection
  DrawRightwards4x4_1to16(obj, bg, tiles);
}

void ObjectDrawer::DrawSolidWallDecor3x4(const RoomObject& obj,
                                         gfx::BackgroundBuffer& bg,
                                         std::span<const gfx::TileInfo> tiles,
                                         [[maybe_unused]] const DungeonState* state) {
  // Pattern: Solid wall decoration - 3 wide x 4 tall (12 tiles) in column-major order
  // Objects: 0xFE9-0xFEA, 0xFEE-0xFEF (ASM 269-26A, 26E-26F)
  constexpr int kWidth = 3;
  constexpr int kHeight = 4;
  
  if (tiles.size() >= kWidth * kHeight) {
    int tid = 0;
    for (int x = 0; x < kWidth && tid < static_cast<int>(tiles.size()); ++x) {
      for (int y = 0; y < kHeight && tid < static_cast<int>(tiles.size()); ++y) {
        WriteTile8(bg, obj.x_ + x, obj.y_ + y, tiles[tid++]);
      }
    }
  } else {
    // Fallback: use 4x4 pattern
    DrawRightwards4x4_1to16(obj, bg, tiles);
  }
}

void ObjectDrawer::DrawArcheryGameTargetDoor(const RoomObject& obj,
                                             gfx::BackgroundBuffer& bg,
                                             std::span<const gfx::TileInfo> tiles,
                                             [[maybe_unused]] const DungeonState* state) {
  // Pattern: Archery Game Target Door - two 3x3 sections stacked vertically
  // ASM: RoomDraw_ArcheryGameTargetDoor at $01A7A3:
  //   1. LDA #$0003, JSR RoomDraw_1x3N_rightwards -> 3 columns of 3 tiles (9 tiles)
  //   2. Y += 0x180 (0x180 / 0x80 = 3 rows down)
  //   3. LDA #$0003, JMP RoomDraw_1x3N_rightwards -> another 3 columns of 3 tiles
  //
  // RoomDraw_1x3N_rightwards draws COLUMN-MAJOR: each column is 3 tiles tall
  // Tile order: col0[row0,row1,row2], col1[row0,row1,row2], col2[row0,row1,row2]
  // Objects: 0xFE0-0xFE1 (ASM 260-261)
  // Total: 3 wide x 6 tall (18 tiles) in column-major order per section
  constexpr int kWidth = 3;
  constexpr int kSectionHeight = 3;
  
  if (tiles.size() >= 18) {
    int tid = 0;
    // Section 1: Top 3x3 in COLUMN-MAJOR order
    for (int x = 0; x < kWidth; ++x) {
      for (int y = 0; y < kSectionHeight; ++y) {
        WriteTile8(bg, obj.x_ + x, obj.y_ + y, tiles[tid++]);
      }
    }
    
    // Section 2: Bottom 3x3 in COLUMN-MAJOR order (continues tile data)
    for (int x = 0; x < kWidth; ++x) {
      for (int y = 0; y < kSectionHeight; ++y) {
        WriteTile8(bg, obj.x_ + x, obj.y_ + kSectionHeight + y, tiles[tid++]);
      }
    }
  } else if (tiles.size() >= 9) {
    // Draw just the first section if not enough tiles
    int tid = 0;
    for (int x = 0; x < kWidth; ++x) {
      for (int y = 0; y < kSectionHeight; ++y) {
        WriteTile8(bg, obj.x_ + x, obj.y_ + y, tiles[tid++]);
      }
    }
  } else {
    // Fallback: use 4x4 pattern
    DrawRightwards4x4_1to16(obj, bg, tiles);
  }
}

void ObjectDrawer::DrawSingle2x2(const RoomObject& obj,
                                 gfx::BackgroundBuffer& bg,
                                 std::span<const gfx::TileInfo> tiles,
                                 [[maybe_unused]] const DungeonState* state) {
  // Pattern: Single 2x2 block (NO repetition based on size)
  // Used for pots, statues, and other single-instance 2x2 objects
  // ASM: RoomDraw_Rightwards2x2 at $019895 draws 4 tiles once
  // Tile order is COLUMN-MAJOR: [col0_row0, col0_row1, col1_row0, col1_row1]
  if (tiles.size() >= 4) {
    WriteTile8(bg, obj.x_, obj.y_, tiles[0]);      // col 0, row 0
    WriteTile8(bg, obj.x_, obj.y_ + 1, tiles[1]);  // col 0, row 1
    WriteTile8(bg, obj.x_ + 1, obj.y_, tiles[2]);  // col 1, row 0
    WriteTile8(bg, obj.x_ + 1, obj.y_ + 1, tiles[3]);  // col 1, row 1
  }
}

void ObjectDrawer::DrawSingle4x4(const RoomObject& obj,
                                 gfx::BackgroundBuffer& bg,
                                 std::span<const gfx::TileInfo> tiles,
                                 [[maybe_unused]] const DungeonState* state) {
  // Pattern: Single 4x4 block (NO repetition based on size)
  // Used for: 0xFEB (large decor) and other single 4x4 objects
  // Tile order is COLUMN-MAJOR: tiles advance down each column, then right
  if (tiles.size() < 16) return;

  int tid = 0;
  for (int x = 0; x < 4; ++x) {
    for (int y = 0; y < 4; ++y) {
      WriteTile8(bg, obj.x_ + x, obj.y_ + y, tiles[tid++]);
    }
  }
}

void ObjectDrawer::DrawSingle4x3(const RoomObject& obj,
                                 gfx::BackgroundBuffer& bg,
                                 std::span<const gfx::TileInfo> tiles,
                                 [[maybe_unused]] const DungeonState* state) {
  // Pattern: Single 4x3 block (NO repetition based on size)
  // ASM: RoomDraw_TableRock4x3 at $0199E6 - draws 4 columns of 3 rows (12 tiles)
  // Calls RoomDraw_1x3N_rightwards with A=4, which draws 4 columns of 3 rows
  // Tile order is COLUMN-MAJOR: tiles advance down each column, then right
  if (tiles.size() >= 12) {
    int tid = 0;
    for (int x = 0; x < 4; ++x) {
      for (int y = 0; y < 3; ++y) {
        WriteTile8(bg, obj.x_ + x, obj.y_ + y, tiles[tid++]);
      }
    }
  }
}

void ObjectDrawer::DrawRupeeFloor(const RoomObject& obj,
                                  gfx::BackgroundBuffer& bg,
                                  std::span<const gfx::TileInfo> tiles,
                                  [[maybe_unused]] const DungeonState* state) {
  // ASM: RoomDraw_RupeeFloor at $019AA9
  // Draws 3 columns of tile pairs, each pair drawn at 3 Y positions:
  //   Row 0-1: First pair
  //   Row 3-4: Second pair (row 2 is skipped)
  //   Row 6-7: Third pair (row 5 is skipped)
  // Uses 2 tiles per column (tile 0 and tile 1), drawn 3 times each
  // Total visual: 6 tiles wide x 8 tiles tall (with gaps at rows 2 and 5)
  
  // Note: Original ASM checks room flags to hide if rupees collected
  // For editor preview, we always draw the pattern
  
  if (tiles.size() < 2) return;
  
  // Draw 3 columns (INX x4 = 2 tiles per iteration, 3 iterations)
  for (int col = 0; col < 3; ++col) {
    int x = obj.x_ + (col * 2);  // 2 tiles per column (left and right of pair)
    
    // Draw the tile pair at 3 Y positions
    // Rows 0-1
    WriteTile8(bg, x, obj.y_, tiles[0]);
    WriteTile8(bg, x + 1, obj.y_, tiles[0]);
    WriteTile8(bg, x, obj.y_ + 1, tiles[1]);
    WriteTile8(bg, x + 1, obj.y_ + 1, tiles[1]);
    
    // Rows 3-4 (skip row 2)
    WriteTile8(bg, x, obj.y_ + 3, tiles[0]);
    WriteTile8(bg, x + 1, obj.y_ + 3, tiles[0]);
    WriteTile8(bg, x, obj.y_ + 4, tiles[1]);
    WriteTile8(bg, x + 1, obj.y_ + 4, tiles[1]);
    
    // Rows 6-7 (skip row 5)
    WriteTile8(bg, x, obj.y_ + 6, tiles[0]);
    WriteTile8(bg, x + 1, obj.y_ + 6, tiles[0]);
    WriteTile8(bg, x, obj.y_ + 7, tiles[1]);
    WriteTile8(bg, x + 1, obj.y_ + 7, tiles[1]);
  }
}

void ObjectDrawer::DrawActual4x4(const RoomObject& obj,
                                 gfx::BackgroundBuffer& bg,
                                 std::span<const gfx::TileInfo> tiles,
                                 [[maybe_unused]] const DungeonState* state) {
  // ASM: RoomDraw_4x4 at $0197ED - draws exactly 4 columns x 4 rows = 16 tiles
  // This is an actual 4x4 tile8 pattern (32x32 pixels), NO repetition
  // Used for: 0xFE6 (pit) and other single 4x4 objects
  // Tile order is COLUMN-MAJOR: tiles advance down each column, then right
  
  if (tiles.size() < 16) return;
  
  int tid = 0;
  for (int x = 0; x < 4; ++x) {
    for (int y = 0; y < 4; ++y) {
      WriteTile8(bg, obj.x_ + x, obj.y_ + y, tiles[tid++]);
    }
  }
}

void ObjectDrawer::DrawGanonTriforceFloorDecor(const RoomObject& obj,
                                               gfx::BackgroundBuffer& bg,
                                               std::span<const gfx::TileInfo> tiles,
                                               [[maybe_unused]] const DungeonState* state) {
  // Pattern: Ganon's Triforce Floor Decoration - THREE 4x4 blocks forming Triforce
  // ASM: RoomDraw_GanonTriforceFloorDecor at $01A7F0:
  //   1. JSR RoomDraw_4x4           -> Block 1 at Y, X advances to tiles 16
  //   2. PHX                        -> Save X (now at tiles 16)
  //   3. Y += 0x1FC, JSR RoomDraw_4x4 -> Block 2 with tiles 16-31
  //   4. PLX                        -> Restore X back to tiles 16
  //   5. Y += 0x204, JMP RoomDraw_4x4 -> Block 3 with tiles 16-31 (SAME as block 2!)
  //
  // VRAM layout: 0x80 bytes per row (64 tiles @ 2 bytes each = 128 bytes)
  // 0x1FC = 508 -> ~4 rows down, -2 cols (124 mod 128 = wraps left)
  // 0x204 = 516 -> ~4 rows down, +2 cols
  //
  // Visual pattern (Triforce):
  //       [Block 1]          <- Top center (tiles 0-15)
  //   [Block 2][Block 3]     <- Bottom row (BOTH use tiles 16-31!)
  constexpr int kBlockSize = 4;
  
  if (tiles.size() >= 32) {
    // Block 1: Top center (tiles 0-15)
    int tid = 0;
    for (int y = 0; y < kBlockSize; ++y) {
      for (int x = 0; x < kBlockSize; ++x) {
        WriteTile8(bg, obj.x_ + x + 2, obj.y_ + y, tiles[tid++]);
      }
    }
    
    // Block 2: Bottom-left (tiles 16-31)
    tid = 16;
    for (int y = 0; y < kBlockSize; ++y) {
      for (int x = 0; x < kBlockSize; ++x) {
        WriteTile8(bg, obj.x_ + x, obj.y_ + kBlockSize + y, tiles[tid++]);
      }
    }
    
    // Block 3: Bottom-right (SAME tiles 16-31, per PLX restoring X)
    tid = 16;
    for (int y = 0; y < kBlockSize; ++y) {
      for (int x = 0; x < kBlockSize; ++x) {
        WriteTile8(bg, obj.x_ + x + 4, obj.y_ + kBlockSize + y, tiles[tid++]);
      }
    }
  } else if (tiles.size() >= 16) {
    // Draw just one 4x4 block if not enough tiles
    int tid = 0;
    for (int y = 0; y < kBlockSize; ++y) {
      for (int x = 0; x < kBlockSize; ++x) {
        WriteTile8(bg, obj.x_ + x, obj.y_ + y, tiles[tid++]);
      }
    }
  } else {
    // Fallback
    DrawRightwards4x4_1to16(obj, bg, tiles);
  }
}

}  // namespace zelda3
}  // namespace yaze

std::pair<int, int> yaze::zelda3::ObjectDrawer::CalculateObjectDimensions(const RoomObject& object) {
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
    case 0: // DrawRightwards2x2_1to15or32
    case 4: // DrawRightwards2x2_1to16
    case 7: // DrawDownwards2x2_1to15or32
    case 11: // DrawDownwards2x2_1to16
      // 2x2 tiles repeated
      if (routine_id == 0 || routine_id == 7) {
        if (size == 0) size = 32;
      } else {
        size = size & 0x0F;
        if (size == 0) size = 16; // 0 usually means 16 for 1to16 routines
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
      height = 32;  // 4 tiles tall
      break;
    }
    case 2:  // RoomDraw_Rightwards2x4spaced4_1to16 (objects 0x03-0x04)
    case 3:  // RoomDraw_Rightwards2x4spaced4_1to16_BothBG (objects 0x05-0x06)
    {
      // ASM: GetSize_1to16, draws 2x4 tiles with 4-tile adjacent spacing
      size = size & 0x0F;
      int count = size + 1;
      width = count * 16;  // 2 tiles wide per repetition (adjacent)
      height = 32;  // 4 tiles tall
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
    case 17: // DrawDiagonalAcute_1to16_BothBG
    case 18: // DrawDiagonalGrave_1to16_BothBG
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
      width = 32;  // 4 tiles wide
      height = effective_size * 16;  // 2 tiles tall per repetition
      break;
    }
    case 9:  // RoomDraw_Downwards4x2_1to16_BothBG (objects 0x63-0x64)
    case 10: // RoomDraw_DownwardsDecor4x2spaced4_1to16 (objects 0x65-0x66)
    {
      // ASM: GetSize_1to16, draws 4x2 tiles with spacing
      size = size & 0x0F;
      int count = size + 1;
      width = 32;  // 4 tiles wide
      height = count * 16;  // 2 tiles tall per repetition (adjacent)
      break;
    }

    case 12: // RoomDraw_DownwardsHasEdge1x1_1to16_plus3
      size = size & 0x0F;
      width = 8;
      height = (size + 3) * 8;
      break;
    case 13: // RoomDraw_DownwardsEdge1x1_1to16
      size = size & 0x0F;
      width = 8;
      height = (size + 1) * 8;
      break;
    case 14: // RoomDraw_DownwardsLeftCorners2x1_1to16_plus12
    case 15: // RoomDraw_DownwardsRightCorners2x1_1to16_plus12
      size = size & 0x0F;
      width = 16;
      height = (size + 10) * 8;
      break;

    case 16: // DrawRightwards4x4_1to16 (Routine 16)
    {
      // 4x4 block repeated horizontally based on size
      // ASM: GetSize_1to16, count = (size & 0x0F) + 1
      int count = (size & 0x0F) + 1;
      width = 32 * count;  // 4 tiles * 8 pixels * count
      height = 32;         // 4 tiles * 8 pixels
      break;
    }
    case 19: // DrawCorner4x4 (Type 2 corners 0x100-0x103)
    case 34: // Water Face (4x4)
    case 35: // 4x4 Corner BothBG
    case 36: // Weird Corner Bottom
    case 37: // Weird Corner Top
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

    case 20: // Edge 1x2 (RoomDraw_Rightwards1x2_1to16_plus2)
    {
      // ZScream: width = size * 2 + 4, height = 3 tiles
      size = size & 0x0F;
      width = (size * 2 + 4) * 8;
      height = 24;
      break;
    }

    case 21: // RoomDraw_RightwardsHasEdge1x1_1to16_plus3 (small rails 0x22)
    {
      // ZScream: count = size + 2 (corner + middle*count + end)
      size = size & 0x0F;
      width = (size + 4) * 8;
      height = 8;
      break;
    }
    case 22: // RoomDraw_RightwardsHasEdge1x1_1to16_plus2 (carpet trim 0x23-0x2E)
    {
      // ASM: GetSize_1to16, count = size + 1
      // Plus corner (1) + end (1) = count + 2 total width
      size = size & 0x0F;
      int count = size + 1;
      width = (count + 2) * 8;  // corner + middle*count + end
      height = 8;
      break;
    }
    case 118: // RoomDraw_RightwardsHasEdge1x1_1to16_plus23 (long rails 0x5F)
    {
      size = size & 0x0F;
      width = (size + 23) * 8;
      height = 8;
      break;
    }
    case 25: // RoomDraw_Rightwards1x1Solid_1to16_plus3
    {
      // ASM: GetSize_1to16_timesA(4), so count = size + 4
      size = size & 0x0F;
      width = (size + 4) * 8;
      height = 8;
      break;
    }

    case 23: // RightwardsTopCorners1x2_1to16_plus13
    case 24: // RightwardsBottomCorners1x2_1to16_plus13
      size = size & 0x0F;
      width = 8 + size * 8;
      height = 16;
      break;

    case 26: // Door Switcher
      width = 32;
      height = 32;
      break;

    case 27: // RoomDraw_RightwardsDecor4x4spaced2_1to16
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

    case 28: // RoomDraw_RightwardsStatue2x3spaced2_1to16
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

    case 29: // RoomDraw_RightwardsPillar2x4spaced4_1to16
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

    case 30: // RoomDraw_RightwardsDecor4x3spaced4_1to16
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

    case 31: // RoomDraw_RightwardsDoubled2x2spaced2_1to16
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
    case 32: // RoomDraw_RightwardsDecor2x2spaced12_1to16
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

    case 33: // Somaria Line
      // Variable length, estimate from size
      size = size & 0x0F;
      width = 8 + size * 8;
      height = 8;
      break;

    case 38: // Nothing (RoomDraw_Nothing)
      width = 8;
      height = 8;
      break;

    case 40: // Rightwards 4x2 (FloorTile)
    {
      // 4 cols x 2 rows, GetSize_1to16
      size = size & 0x0F;
      int count = size + 1;
      width = count * 4 * 8;  // 4 tiles per repetition
      height = 16;  // 2 tiles
      break;
    }

    case 41: // Rightwards Decor 1x8 spaced 12 (wall torches 0x55-0x56)
    {
      // ASM: 1 column x 8 rows with 12-tile horizontal spacing
      size = size & 0x0F;
      int count = size + 1;
      width = ((count - 1) * 12 + 1) * 8;  // 1 tile wide per block
      height = 64;  // 8 tiles tall
      break;
    }

    case 42: // Rightwards Cannon Hole 4x3
    {
      // 4x3 tiles, GetSize_1to16
      size = size & 0x0F;
      int count = size + 1;
      width = count * 4 * 8;
      height = 24;
      break;
    }

    case 43: // Downwards Floor 4x4
    {
      // 4x4 tiles, GetSize_1to16
      size = size & 0x0F;
      int count = size + 1;
      width = 32;
      height = count * 4 * 8;
      break;
    }

    case 44: // Downwards 1x1 Solid +3
    {
      size = size & 0x0F;
      width = 8;
      height = (size + 4) * 8;
      break;
    }

    case 45: // Downwards Decor 4x4 spaced 2
    {
      size = size & 0x0F;
      int count = size + 1;
      width = 32;
      height = ((count - 1) * 6 + 4) * 8;
      break;
    }

    case 46: // Downwards Pillar 2x4 spaced 2
    {
      size = size & 0x0F;
      int count = size + 1;
      width = 16;
      height = ((count - 1) * 6 + 4) * 8;
      break;
    }

    case 47: // Downwards Decor 3x4 spaced 4
    {
      size = size & 0x0F;
      int count = size + 1;
      width = 24;
      height = ((count - 1) * 6 + 4) * 8;
      break;
    }

    case 48: // Downwards Decor 2x2 spaced 12
    {
      size = size & 0x0F;
      int count = size + 1;
      width = 16;
      height = ((count - 1) * 14 + 2) * 8;
      break;
    }

    case 49: // Downwards Line 1x1 +1
    {
      size = size & 0x0F;
      width = 8;
      height = (size + 2) * 8;
      break;
    }

    case 50: // Downwards Decor 2x4 spaced 8
    {
      size = size & 0x0F;
      int count = size + 1;
      width = 16;
      height = ((count - 1) * 12 + 4) * 8;
      break;
    }

    case 51: // Rightwards Line 1x1 +1
    {
      size = size & 0x0F;
      width = (size + 2) * 8;
      height = 8;
      break;
    }

    case 52: // Rightwards Bar 4x3
    {
      size = size & 0x0F;
      int count = size + 1;
      width = ((count - 1) * 6 + 4) * 8;
      height = 24;
      break;
    }

    case 53: // Rightwards Shelf 4x4
    {
      size = size & 0x0F;
      int count = size + 1;
      width = ((count - 1) * 6 + 4) * 8;
      height = 32;
      break;
    }

    case 54: // Rightwards Big Rail 1x3 +5
    {
      size = size & 0x0F;
      width = (size + 6) * 8;
      height = 24;
      break;
    }

    case 55: // Rightwards Block 2x2 spaced 2
    {
      size = size & 0x0F;
      int count = size + 1;
      width = ((count - 1) * 4 + 2) * 8;
      height = 16;
      break;
    }

    // Routines 56-64: SuperSquare patterns
    // ASM: These routines use size_x = (size & 0x0F) + 1, size_y = ((size >> 4) & 0x0F) + 1
    // Each super square unit is 4 tiles (32 pixels) in each dimension
    case 56: // 4x4BlocksIn4x4SuperSquare
    case 57: // 3x3FloorIn4x4SuperSquare
    case 58: // 4x4FloorIn4x4SuperSquare
    case 59: // 4x4FloorOneIn4x4SuperSquare
    case 60: // 4x4FloorTwoIn4x4SuperSquare
    case 62: // Spike2x2In4x4SuperSquare
    {
      int size_x = (size & 0x0F) + 1;
      int size_y = ((size >> 4) & 0x0F) + 1;
      width = size_x * 32;   // 4 tiles per super square
      height = size_y * 32;  // 4 tiles per super square
      break;
    }
    case 61: // BigHole4x4
    case 63: // TableRock4x4
    case 64: // WaterOverlay8x8
      width = 32;
      height = 32;
      break;

    // Routines 65-74: Various downwards/rightwards patterns
    case 65: // DownwardsDecor3x4spaced2
    {
      size = size & 0x0F;
      int count = size + 1;
      width = 24;
      height = ((count - 1) * 5 + 4) * 8;
      break;
    }

    case 66: // DownwardsBigRail3x1 +5
    {
      // Top cap (2x2) + Middle (2x1 x count) + Bottom cap (2x3)
      // Total: 2 tiles wide, 2 + (size+1) + 3 = size + 6 tiles tall
      size = size & 0x0F;
      width = 16;  // 2 tiles wide
      height = (size + 6) * 8;
      break;
    }

    case 67: // DownwardsBlock2x2spaced2
    {
      size = size & 0x0F;
      int count = size + 1;
      width = 16;
      height = ((count - 1) * 4 + 2) * 8;
      break;
    }

    case 68: // DownwardsCannonHole3x6
    {
      size = size & 0x0F;
      int count = size + 1;
      width = 24;
      height = count * 6 * 8;
      break;
    }

    case 69: // DownwardsBar2x3
    {
      size = size & 0x0F;
      int count = size + 1;
      width = 16;
      height = ((count - 1) * 3 + 3) * 8;
      break;
    }

    case 70: // DownwardsPots2x2
    case 71: // DownwardsHammerPegs2x2
    {
      size = size & 0x0F;
      int count = size + 1;
      width = 16;
      height = count * 2 * 8;
      break;
    }

    case 72: // RightwardsEdge1x1 +7
    {
      size = size & 0x0F;
      width = (size + 8) * 8;
      height = 8;
      break;
    }

    case 73: // RightwardsPots2x2
    case 74: // RightwardsHammerPegs2x2
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
    case 75: // DiagonalCeilingTopLeft - triangle at origin
    case 76: // DiagonalCeilingBottomLeft - triangle at origin
    {
      // Smaller outline for triangle - use half the drawn area
      int count = (size & 0x0F) + 2;
      width = count * 8;
      height = count * 8;
      break;
    }
    case 77: // DiagonalCeilingTopRight - triangle shifts diagonally
    case 78: // DiagonalCeilingBottomRight - triangle shifts diagonally
    {
      // Smaller outline for diagonal triangles
      int count = (size & 0x0F) + 2;
      width = count * 8;
      height = count * 8;
      break;
    }

    // Special platform routines (79-82)
    case 79: // ClosedChestPlatform
    case 80: // MovingWallWest
    case 81: // MovingWallEast
    case 82: // OpenChestPlatform
      width = 64;  // 8 tiles wide
      height = 64;  // 8 tiles tall
      break;

    // Stair routines - different sizes for different types
    
    // 4x4 stair patterns (32x32 pixels)
    case 83: // InterRoomFatStairsUp (0x12D)
    case 84: // InterRoomFatStairsDownA (0x12E)
    case 85: // InterRoomFatStairsDownB (0x12F)
    case 86: // AutoStairs (0x130-0x133)
    case 87: // StraightInterroomStairs (0xF9E-0xFA9)
      width = 32;   // 4 tiles
      height = 32;  // 4 tiles (4x4 pattern)
      break;
    
    // 4x3 stair patterns (32x24 pixels)
    case 88: // SpiralStairsGoingUpUpper (0x138)
    case 89: // SpiralStairsGoingDownUpper (0x139)
    case 90: // SpiralStairsGoingUpLower (0x13A)
    case 91: // SpiralStairsGoingDownLower (0x13B)
      // ASM: RoomDraw_1x3N_rightwards with A=4 -> 4 columns x 3 rows
      width = 32;   // 4 tiles
      height = 24;  // 3 tiles
      break;

    case 92: // BigKeyLock
      width = 16;
      height = 24;
      break;

    case 93: // BombableFloor
      width = 32;
      height = 32;
      break;

    case 94: // EmptyWaterFace
    case 95: // SpittingWaterFace
    case 96: // DrenchingWaterFace
      width = 32;
      height = 32;
      break;

    case 97: // PrisonCell
      width = 32;
      height = 48;
      break;

    case 98: // Bed4x5
      width = 32;
      height = 40;
      break;

    case 99: // Rightwards3x6
      width = 24;
      height = 48;
      break;

    case 100: // Utility6x3
      width = 48;
      height = 24;
      break;

    case 101: // Utility3x5
      width = 24;
      height = 40;
      break;

    case 102: // VerticalTurtleRockPipe
      width = 16;
      height = 48;
      break;

    case 103: // HorizontalTurtleRockPipe
      width = 48;
      height = 16;
      break;

    case 104: // LightBeam
      width = 16;
      height = 32;
      break;

    case 105: // BigLightBeam
      width = 32;
      height = 64;
      break;

    case 106: // BossShell4x4
      width = 32;
      height = 32;
      break;

    case 107: // SolidWallDecor3x4
      width = 24;
      height = 32;
      break;

    case 108: // ArcheryGameTargetDoor
      width = 24;
      height = 48;
      break;

    case 109: // GanonTriforceFloorDecor
      width = 32;
      height = 64;
      break;

    case 110: // Single2x2
      width = 16;
      height = 16;
      break;

    case 111: // Waterfall47 (object 0x47)
    {
      // ASM: count = (size+1)*2, draws 1x5 columns
      // Width = first column + middle columns + last column = 2 + count tiles
      size = size & 0x0F;
      int count = (size + 1) * 2;
      width = (2 + count) * 8;
      height = 40;  // 5 tiles
      break;
    }
    case 112: // Waterfall48 (object 0x48)
    {
      // ASM: count = (size+1)*2, draws 1x3 columns
      // Width = first column + middle columns + last column = 2 + count tiles
      size = size & 0x0F;
      int count = (size + 1) * 2;
      width = (2 + count) * 8;
      height = 24;  // 3 tiles
      break;
    }

    case 113: // Single4x4 (no repetition) - 4x4 TILE16 = 8x8 TILE8
      // 4 tile16 wide x 4 tile16 tall = 8 tile8 x 8 tile8 = 64x64 pixels
      width = 64;
      height = 64;
      break;

    case 114: // Single4x3 (no repetition)
      // 4 tiles wide x 3 tiles tall = 32x24 pixels
      width = 32;
      height = 24;
      break;

    case 115: // RupeeFloor (special pattern)
      // 6 tiles wide (3 columns x 2 tiles) x 8 tiles tall = 48x64 pixels
      width = 48;
      height = 64;
      break;

    case 116: // Actual4x4 (true 4x4 tile8 pattern, no repetition)
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

void yaze::zelda3::ObjectDrawer::DrawCustomObject(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                                    [[maybe_unused]] std::span<const gfx::TileInfo> tiles,
                                    [[maybe_unused]] const DungeonState* state) {
  // CustomObjectManager should be initialized by DungeonEditorV2 with the 
  // project's custom_objects_folder path before any objects are drawn
  auto& manager = CustomObjectManager::Get();

  int subtype = obj.size_ & 0x1F;
  auto result = manager.GetObjectInternal(obj.id_, subtype);
  if (!result.ok()) {
    LOG_DEBUG("ObjectDrawer", "Custom object 0x%03X subtype %d not found: %s",
              obj.id_, subtype, result.status().message().data());
    return;
  }

  auto custom_obj = result.value();
  if (!custom_obj || custom_obj->IsEmpty()) return;

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

void yaze::zelda3::ObjectDrawer::DrawPotItem(uint8_t item_id, int x, int y,
                                             gfx::BackgroundBuffer& bg) {
  // Draw a small colored indicator for pot items
  // Item types from ZELDA3_DUNGEON_SPEC.md Section 7.2
  // Uses palette indices that map to recognizable colors

  if (item_id == 0) return;  // Nothing - skip

  auto& bitmap = bg.bitmap();
  if (!bitmap.is_active() || bitmap.width() == 0) return;

  // Convert tile coordinates to pixel coordinates
  // Items are drawn offset from pot position (centered on pot)
  int pixel_x = (x * 8) + 2;  // Offset 2 pixels into the pot tile
  int pixel_y = (y * 8) + 2;

  // Choose color based on item category
  // Using palette indices that should be visible in dungeon palettes
  uint8_t color_idx;
  switch (item_id) {
    // Rupees (green/blue/red tones)
    case 1:   // Green rupee
    case 7:   // Blue rupee
    case 12:  // Blue rupee variant
      color_idx = 30;  // Greenish (palette 2, index 0)
      break;

    // Hearts (red tones)
    case 6:   // Heart
    case 11:  // Heart
    case 13:  // Heart variant
      color_idx = 0;  // Should be reddish in dungeon palette
      break;

    // Keys (yellow/gold)
    case 8:   // Key*8
    case 19:  // Key
      color_idx = 45;  // Yellowish (palette 3)
      break;

    // Bombs (dark/black)
    case 5:   // Bomb
    case 10:  // 1 bomb
    case 16:  // Bomb refill
      color_idx = 60;  // Darker color (palette 4)
      break;

    // Arrows (brown/wood)
    case 9:   // Arrow
    case 17:  // Arrow refill
      color_idx = 15;  // Brownish (palette 1)
      break;

    // Magic (blue/purple)
    case 14:  // Small magic
    case 15:  // Big magic
      color_idx = 75;  // Bluish (palette 5)
      break;

    // Fairy (pink/light)
    case 18:  // Fairy
    case 20:  // Fairy*8
      color_idx = 5;  // Pinkish
      break;

    // Special/Traps (distinct colors)
    case 2:   // Rock crab
    case 3:   // Bee
      color_idx = 20;  // Enemy indicator
      break;

    case 23:  // Hole
    case 24:  // Warp
    case 25:  // Staircase
      color_idx = 10;  // Transport indicator
      break;

    case 26:  // Bombable
    case 27:  // Switch
      color_idx = 35;  // Interactive indicator
      break;

    case 4:   // Random
    default:
      color_idx = 50;  // Default/random indicator
      break;
  }

  // Draw a 4x4 colored square as item indicator
  int bitmap_width = bitmap.width();
  int bitmap_height = bitmap.height();

  for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
      int dest_x = pixel_x + px;
      int dest_y = pixel_y + py;

      // Bounds check
      if (dest_x >= 0 && dest_x < bitmap_width &&
          dest_y >= 0 && dest_y < bitmap_height) {
        int offset = (dest_y * bitmap_width) + dest_x;
        bitmap.WriteToPixel(offset, color_idx);
      }
    }
  }
}
