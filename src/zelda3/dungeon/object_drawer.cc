#include "object_drawer.h"

#include <cstdio>

#include "absl/strings/str_format.h"
#include "app/gfx/types/snes_tile.h"
#include "util/log.h"
#include "zelda3/dungeon/draw_routines/draw_routine_types.h"
#include "zelda3/dungeon/draw_routines/special_routines.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"

namespace yaze {
namespace zelda3 {

ObjectDrawer::ObjectDrawer(Rom* rom, int room_id, const uint8_t* room_gfx_buffer)
    : rom_(rom), room_id_(room_id), room_gfx_buffer_(room_gfx_buffer) {
  InitializeDrawRoutines();
}

absl::Status ObjectDrawer::DrawObject(const RoomObject& object,
                                      gfx::BackgroundBuffer& bg1,
                                      gfx::BackgroundBuffer& bg2,
                                      const gfx::PaletteGroup& palette_group,
                                      [[maybe_unused]] const DungeonState* state) {
  LOG_DEBUG("ObjectDrawer",
            "Drawing object 0x%02X at (%d,%d) size=%d tiles=%zu", object.id_,
            object.x_, object.y_, object.size_, object.tiles().size());

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
  auto& target_bg = (object.layer_ == RoomObject::LayerType::BG2) ? bg2 : bg1;

  // Skip objects that don't have tiles loaded
  if (mutable_obj.tiles().empty()) {
    return absl::OkStatus();
  }

  // Look up draw routine for this object
  int routine_id = GetDrawRoutineId(object.id_);

  LOG_DEBUG("ObjectDrawer", "Object %04X -> routine_id=%d", object.id_,
            routine_id);

  if (routine_id < 0 || routine_id >= static_cast<int>(draw_routines_.size())) {
    LOG_DEBUG("ObjectDrawer", "Using fallback 1x1 drawing for object %04X",
              object.id_);
    // Fallback to simple 1x1 drawing using first 8x8 tile
    if (!mutable_obj.tiles().empty()) {
      const auto& tile_info = mutable_obj.tiles()[0];
      WriteTile8(target_bg, object.x_, object.y_, tile_info);
    }
    return absl::OkStatus();
  }

  LOG_DEBUG("ObjectDrawer", "Executing draw routine %d for object %04X",
            routine_id, object.id_);

  // Check if this is a BothBG routine
  // These routines should draw to both BG1 and BG2:
  // - 3, 9: diagonal walls (BothBG variants)
  // - 17, 18: diagonal walls acute/grave (BothBG)
  // - 97: prison cell (dual-layer bars)
  // We now use the all_bgs flag from the object itself, which is set during decoding
  bool is_both_bg = object.all_bgs_ || (routine_id == 3 || routine_id == 9 ||
                     routine_id == 17 || routine_id == 18 || routine_id == 97);

  if (is_both_bg) {
    // Draw to both background layers
    draw_routines_[routine_id](this, object, bg1, mutable_obj.tiles(), state);
    draw_routines_[routine_id](this, object, bg2, mutable_obj.tiles(), state);
  } else {
    // Execute the appropriate draw routine on target buffer only
    draw_routines_[routine_id](this, object, target_bg, mutable_obj.tiles(), state);
  }

  return absl::OkStatus();
}

absl::Status ObjectDrawer::DrawObjectList(
    const std::vector<RoomObject>& objects, gfx::BackgroundBuffer& bg1,
    gfx::BackgroundBuffer& bg2, const gfx::PaletteGroup& palette_group,
    [[maybe_unused]] const DungeonState* state) {
  ResetChestIndex();
  absl::Status status = absl::OkStatus();
  for (const auto& object : objects) {
    auto s = DrawObject(object, bg1, bg2, palette_group, state);
    if (!s.ok() && status.ok()) {
      status = s;
    }
  }

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
  // Objects 0x01-0x04 all use the 2x4 wall pattern (routine 1)
  // ASM: These are similar wall variations that draw adjacent 2x4 tiles
  for (int id = 0x01; id <= 0x04; id++) {
    object_to_routine_map_[id] = 1;
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

  // Custom/Special Objects (0x31-0x3E)
  object_to_routine_map_[0x31] = 38; // Nothing (Logic: RoomDraw_Nothing_A)
  object_to_routine_map_[0x32] = 38; // Nothing (Logic: RoomDraw_Nothing_A)
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
  // 0x47-0x48 Waterfalls - map to 1x1 for now or custom if implemented
  object_to_routine_map_[0x47] = 25;
  object_to_routine_map_[0x48] = 25;
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
  object_to_routine_map_[0x5F] = 22;  // edge 1x1

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
  // 0x8A-0x8C: 3 tiles each
  object_to_routine_map_[0x8A] = 22;  // edge 1x1
  object_to_routine_map_[0x8B] = 22;
  object_to_routine_map_[0x8C] = 22;
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

  // Subtype 2 Object Mappings (0x100-0x1FF)
  for (int id = 0x100; id <= 0x107; id++) {
    object_to_routine_map_[id] = 16; // 4x4
  }
  for (int id = 0x108; id <= 0x10F; id++) {
    object_to_routine_map_[id] = 35; // 4x4 Corner BothBG
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
  // 0x122 Bed 4x5 -> Map to 4x4 (16)
  object_to_routine_map_[0x122] = 16;
  // 0x123 Table 4x3 -> Map to 4x3 (30)
  object_to_routine_map_[0x123] = 30;
  // 0x124-0x125 4x4
  object_to_routine_map_[0x124] = 16;
  object_to_routine_map_[0x125] = 16;
  // 0x126 Single 2x3 -> Map to 2x3 (28)
  object_to_routine_map_[0x126] = 28;
  // 0x127 Rightwards 2x2 -> Map to 2x2 (4)
  object_to_routine_map_[0x127] = 4;
  // 0x128 Bed 4x5 -> Map to 4x4 (16)
  object_to_routine_map_[0x128] = 16;
  // 0x129 4x4
  object_to_routine_map_[0x129] = 16;
  // 0x12A Mario -> Map to 2x2 (4)
  object_to_routine_map_[0x12A] = 4;
  // 0x12B Rightwards 2x2 -> Map to 2x2 (4)
  object_to_routine_map_[0x12B] = 4;
  // 0x12C 3x6 -> Map to 4x4 (16)
  object_to_routine_map_[0x12C] = 16;
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
  // 0x13E Utility 6x3 -> Map to 4x3 (30)
  object_to_routine_map_[0x13E] = 30;
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
  for (int id = 0xF90; id <= 0xF93; id++) {
    object_to_routine_map_[id] = 16; // 4x4
  }
  object_to_routine_map_[0xF94] = 33; // Somaria Line
  object_to_routine_map_[0xF95] = 16;
  object_to_routine_map_[0xF96] = 16;
  // 0xF97 (0x217): PrisonCell variant - dual-BG drawing
  object_to_routine_map_[0xF97] = 97;  // Prison cell routine
  for (int id = 0xF98; id <= 0xF9A; id++) {
    object_to_routine_map_[id] = 39; // DrawChest
  }
  for (int id = 0xF9B; id <= 0xF9D; id++) {
    object_to_routine_map_[id] = 35; // 4x4 Corner BothBG
  }
  for (int id = 0xF9E; id <= 0xFA1; id++) {
    object_to_routine_map_[id] = 36; // Weird Corner Bottom
  }
  for (int id = 0xFA2; id <= 0xFA5; id++) {
    object_to_routine_map_[id] = 37; // Weird Corner Top
  }
  for (int id = 0xFA6; id <= 0xFA9; id++) {
    object_to_routine_map_[id] = 4; // Rightwards 2x2
  }
  // 0xFAA: Nothing
  object_to_routine_map_[0xFAA] = 38;
  object_to_routine_map_[0xFAB] = 16;
  object_to_routine_map_[0xFAC] = 16;
  // 0xFAD-0xFAE: Nothing
  object_to_routine_map_[0xFAD] = 38;
  object_to_routine_map_[0xFAE] = 38;
  object_to_routine_map_[0xFAF] = 16;
  object_to_routine_map_[0xFB0] = 16;
  object_to_routine_map_[0xFB1] = 16;
  object_to_routine_map_[0xFB2] = 16;
  object_to_routine_map_[0xFB3] = 35; // 4x4 Corner BothBG
  for (int id = 0xFB4; id <= 0xFB9; id++) {
    object_to_routine_map_[id] = 34; // Water Face
  }
  // 0xFF3: Nothing (was 0x273)
  object_to_routine_map_[0xFF3] = 38;
  // 0xFFF: Nothing (was 0x27F)
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
  // Routine 2
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawRightwards2x4spaced4_1to16(obj, bg, tiles);
  });
  // Routine 3
  draw_routines_.push_back([](ObjectDrawer* self, const RoomObject& obj,
                              gfx::BackgroundBuffer& bg,
                              std::span<const gfx::TileInfo> tiles,
                              [[maybe_unused]] const DungeonState* state) {
    self->DrawRightwards2x4spaced4_1to16_BothBG(obj, bg, tiles);
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
  // Door rendering based on ZELDA3_DUNGEON_SPEC.md Section 5
  // Uses DoorType and DoorDirection enums for type safety
  // Position calculations via DoorPositionManager

  if (!rom_ || !rom_->is_loaded()) return;

  auto& bitmap = bg1.bitmap();
  if (!bitmap.is_active() || bitmap.width() == 0) return;

  // Get door position from DoorPositionManager
  auto [tile_x, tile_y] = door.GetTileCoords();
  auto dims = door.GetDimensions();
  int door_width = dims.width_tiles;
  int door_height = dims.height_tiles;

  // TODO: Door graphics loading needs proper SNES-to-PC address conversion
  // The kDoorGfxUp/Down/Left/Right addresses are SNES addresses that need
  // LoROM mapping to file offsets. For now, use colored indicators.
  //
  // Future implementation would:
  // 1. Convert SNES address to PC offset: pc_addr = ((snes_addr & 0x7F0000) >> 1) | (snes_addr & 0x7FFF)
  // 2. Read door tilemap from ROM
  // 3. Use tilemap entries with room_gfx_buffer_ to draw actual graphics

  // Draw door indicator (colored rectangle with type-specific color)
  DrawDoorIndicator(bitmap, tile_x, tile_y, door_width, door_height,
                    door.type, door.direction);

  LOG_DEBUG("ObjectDrawer", "DrawDoor: type=%s dir=%s pos=%d at tile(%d,%d) size=%dx%d",
            std::string(GetDoorTypeName(door.type)).c_str(),
            std::string(GetDoorDirectionName(door.direction)).c_str(),
            door.position, tile_x, tile_y, door_width, door_height);
}

void ObjectDrawer::DrawDoorIndicator(gfx::Bitmap& bitmap, int tile_x, int tile_y,
                                      int width, int height, DoorType type,
                                      DoorDirection direction) {
  // Draw a simple colored rectangle as door indicator when graphics unavailable
  // Different colors for different door types using DoorType enum

  uint8_t color_idx;
  switch (type) {
    case DoorType::Open:
    case DoorType::NormalDoor:
      color_idx = 45;  // Standard door color (brown)
      break;

    case DoorType::SmallKeyDoor:
    case DoorType::SmallKeyBlock:
      color_idx = 60;  // Key door - yellowish
      break;

    case DoorType::BigKeyDoor:
    case DoorType::BigKeyBlock:
      color_idx = 58;  // Big key - golden
      break;

    case DoorType::Bombable:
    case DoorType::ExplodingWall:
      color_idx = 15;  // Bombable - brownish/cracked
      break;

    case DoorType::ShutterDoor:
    case DoorType::DungeonShutter:
    case DoorType::TrapDoor:
      color_idx = 30;  // Shutter - greenish
      break;

    case DoorType::SwingingDoor:
      color_idx = 42;  // Swinging - lighter brown
      break;

    case DoorType::InvisibleDoor:
      color_idx = 5;  // Invisible - very faint
      break;

    case DoorType::SanctuaryDoor:
      color_idx = 35;  // Sanctuary - special
      break;

    case DoorType::CaveExitNorth:
    case DoorType::CaveExitSouth:
      color_idx = 25;  // Cave exit - dark
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
  // Determine if chest is open
  bool is_open = false;
  if (state) {
    is_open = state->IsChestOpen(room_id_, current_chest_index_);
  }
  
  // Increment index for next chest
  current_chest_index_++;

  // Draw logic
  // Heuristic: If we have extra tiles loaded, assume they are for the open state.
  // Standard chests are 2x2 (4 tiles). Big chests are 4x4 (16 tiles).
  // If we have double the tiles, use the second half for open state.
  
  if (is_open) {
    if (tiles.size() >= 32) {
      // Big chest open tiles (indices 16-31)
      DrawRightwards4x4_1to16(obj, bg, tiles.subspan(16));
      return;
    }
    if (tiles.size() >= 8 && tiles.size() < 16) {
      // Small chest open tiles (indices 4-7)
      DrawRightwards2x2_1to16(obj, bg, tiles.subspan(4));
      return;
    }
    // If no extra tiles, fall through to draw closed chest (better than nothing)
  }

  // Fallback to standard 4x4 or 2x2 drawing based on tile count
  if (tiles.size() >= 16) {
    DrawRightwards4x4_1to16(obj, bg, tiles);
  } else if (tiles.size() >= 4) {
    DrawRightwards2x2_1to16(obj, bg, tiles);
  } else {
    // Fallback for incomplete data
    DrawRightwards4x4_1to16(obj, bg, tiles);
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
  // Pattern: Draws 2x2 tiles rightward (object 0x00)
  // Size byte determines how many times to repeat (1-15 or 32)
  // ROM tile order is COLUMN-MAJOR: [col0_row0, col0_row1, col1_row0, col1_row1]
  int size = obj.size_;
  if (size == 0)
    size = 32;  // Special case for object 0x00

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

void ObjectDrawer::DrawRightwards2x4spaced4_1to16_BothBG(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Same as above but draws to both BG1 and BG2 (objects 0x05-0x06)
  DrawRightwards2x4spaced4_1to16(obj, bg, tiles);
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
  // Pattern: 4x4 grid corner (Type 2 corners 0x40-0x4F, 0x108-0x10F)
  // Type 2 objects only have 8 tiles, so we need to handle both 16 and 8 tile cases

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
  // Pattern: 1x2 tiles rightward with +2 offset (object 0x21)
  int size = obj.size_ & 0x0F;

  // Assembly: (size << 1) + 1 = (size * 2) + 1
  int count = (size * 2) + 1;

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 2) {
      // Use first tile span for 1x2 pattern
      WriteTile8(bg, obj.x_ + s + 2, obj.y_, tiles[0]);
      WriteTile8(bg, obj.x_ + s + 2, obj.y_ + 1, tiles[1]);
    }
  }
}

void ObjectDrawer::DrawRightwardsHasEdge1x1_1to16_plus3(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: 1x1 tiles with edge detection +3 offset (object 0x22)
  int size = obj.size_ & 0x0F;

  // Assembly: GetSize_1to16_timesA(2), so count = size + 2
  int count = size + 2;

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 1) {
      // Use first 8x8 tile from span
      WriteTile8(bg, obj.x_ + s + 3, obj.y_, tiles[0]);
    }
  }
}

void ObjectDrawer::DrawRightwardsHasEdge1x1_1to16_plus2(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: 1x1 tiles with edge detection +2 offset (objects 0x23-0x2E, 0x3F-0x46)
  int size = obj.size_ & 0x0F;

  // Assembly: GetSize_1to16, so count = size + 1
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 1) {
      // Use first 8x8 tile from span
      WriteTile8(bg, obj.x_ + s + 2, obj.y_, tiles[0]);
    }
  }
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
  // Pattern: 4x4 block rightward (object 0x33)
  int size = obj.size_ & 0x0F;

  // Assembly: GetSize_1to16, so count = size + 1
  int count = size + 1;

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
  int size = obj.size_ & 0x0F;

  // Assembly: GetSize_1to16, so count = size + 1
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 8) {
      // Draw 2x4 pattern in COLUMN-MAJOR order (matching assembly)
      for (int x = 0; x < 2; ++x) {
        for (int y = 0; y < 4; ++y) {
          WriteTile8(bg, obj.x_ + (s * 6) + x, obj.y_ + y, tiles[x * 4 + y]);
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
  int size = obj.size_ & 0x0F;

  // Assembly: GetSize_1to16, so count = size + 1
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 12) {
      // Draw 4x3 pattern in COLUMN-MAJOR order (matching assembly)
      for (int x = 0; x < 4; ++x) {
        for (int y = 0; y < 3; ++y) {
          WriteTile8(bg, obj.x_ + (s * 6) + x, obj.y_ + y, tiles[x * 3 + y]);
        }
      }
    }
  }
}

void ObjectDrawer::DrawRightwardsDoubled2x2spaced2_1to16(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Doubled 2x2 with spacing (object 0x3C)
  // 4 columns × 2 rows = 8 tiles in COLUMN-MAJOR order
  int size = obj.size_ & 0x0F;

  // Assembly: GetSize_1to16, so count = size + 1
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 8) {
      // Draw doubled 2x2 pattern in COLUMN-MAJOR order (matching assembly)
      for (int x = 0; x < 4; ++x) {
        for (int y = 0; y < 2; ++y) {
          WriteTile8(bg, obj.x_ + (s * 6) + x, obj.y_ + y, tiles[x * 2 + y]);
        }
      }
    }
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
  // Pattern: Draws 4x2 tiles rightward with spacing (objects 0x55-0x56)
  // Assembly: RoomDraw_RightwardsDecor4x2spaced8_1to16 -> spacing $0018 = 12 tile columns
  // This is 4 columns × 2 rows = 8 tiles in ROW-MAJOR order with 12-column spacing
  int size = obj.size_ & 0x0F;
  int count = size + 1;  // GetSize_1to16

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 8) {
      // Draw 4x2 pattern in ROW-MAJOR order with 12-tile horizontal spacing
      int base_x = obj.x_ + (s * 12);  // spacing of 12 columns
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
  // Pattern: 4x3 bar rightward with 6-tile X spacing (object 0x4C)
  // Assembly: RoomDraw_RightwardsBar4x3_1to16 - adds $0006 to X per iteration
  int size = obj.size_ & 0x0F;
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 12) {
      int base_x = obj.x_ + (s * 6);  // 6-tile X spacing
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
    }
  }
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
  // Pattern: 1x3 big rail rightward with +5 modifier (object 0x5D)
  // Assembly: RoomDraw_RightwardsBigRail1x3_1to16plus5
  // Draws 3 rows, extending rightward
  int size = obj.size_ & 0x0F;
  int count = size + 6;  // +5 gives +6 iterations total

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 3) {
      WriteTile8(bg, obj.x_ + s, obj.y_, tiles[0]);
      WriteTile8(bg, obj.x_ + s, obj.y_ + 1, tiles[1]);
      WriteTile8(bg, obj.x_ + s, obj.y_ + 2, tiles[2]);
    }
  }
}

void ObjectDrawer::DrawRightwardsBlock2x2spaced2_1to16(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: 2x2 block rightward with 4-tile X spacing (object 0x5E)
  // Assembly: RoomDraw_RightwardsBlock2x2spaced2_1to16 - adds $0004 to X per iteration
  int size = obj.size_ & 0x0F;
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 4) {
      int base_x = obj.x_ + (s * 4);  // 4-tile X spacing
      // Draw 2x2 pattern
      WriteTile8(bg, base_x, obj.y_, tiles[0]);
      WriteTile8(bg, base_x + 1, obj.y_, tiles[1]);
      WriteTile8(bg, base_x, obj.y_ + 1, tiles[2]);
      WriteTile8(bg, base_x + 1, obj.y_ + 1, tiles[3]);
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
  // Pattern: 1x1 tiles with edge detection +3 offset downward (object 0x69)
  int size = obj.size_ & 0x0F;

  // Assembly: GetSize_1to16_timesA(2), so count = size + 2
  int count = size + 2;

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 1) {
      // Use first 8x8 tile from span
      WriteTile8(bg, obj.x_ + 3, obj.y_ + s, tiles[0]);
    }
  }
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
  // Pattern: Big rail 3x1 downward with +5 modifier (object 0x88)
  // Vertical version of RightwardsBigRail1x3_1to16plus5
  // Assembly: RoomDraw_DownwardsBigRail3x1_1to16plus5
  int size = obj.size_ & 0x0F;
  int count = size + 6;  // +5 gives +6 iterations total

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 3) {
      // Draw 3x1 column pattern
      WriteTile8(bg, obj.x_, obj.y_ + s, tiles[0]);
      WriteTile8(bg, obj.x_ + 1, obj.y_ + s, tiles[1]);
      WriteTile8(bg, obj.x_ + 2, obj.y_ + s, tiles[2]);
    }
  }
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
  // Pattern: Diagonal ceiling top-left (objects 0xA0, 0xA5, 0xA9)
  // Assembly: RoomDraw_DiagonalCeilingTopLeftA/B
  // Draws 1x1 tiles going DOWN (Y+1 each iteration)
  // Count formula: LDA #$0004; JSR GetSize_1to16_timesA -> count = size + 4
  int size = obj.size_ & 0x0F;
  int count = size + 4;

  if (tiles.empty()) return;

  for (int s = 0; s < count; s++) {
    // Assembly: ADC #$0080 = move down 1 row
    WriteTile8(bg, obj.x_, obj.y_ + s, tiles[0]);
  }
}

void ObjectDrawer::DrawDiagonalCeilingBottomLeft(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Diagonal ceiling bottom-left (objects 0xA1, 0xA6, 0xAA)
  // Assembly: RoomDraw_DiagonalCeilingBottomLeftA/B
  // Draws increasing number of 1x1 tiles per row, going DOWN
  // Count formula: LDA #$0004; JSR GetSize_1to16_timesA -> count = size + 4
  int size = obj.size_ & 0x0F;
  int count = size + 4;

  if (tiles.empty()) return;

  // Assembly increments B4 counter before each iteration
  // This creates a widening diagonal pattern
  int tile_count = 1;
  for (int s = 0; s < count; s++) {
    for (int t = 0; t < tile_count; t++) {
      WriteTile8(bg, obj.x_ - t, obj.y_ + s, tiles[0]);
    }
    tile_count++;
  }
}

void ObjectDrawer::DrawDiagonalCeilingTopRight(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Diagonal ceiling top-right (objects 0xA2, 0xA7, 0xAB)
  // Assembly: RoomDraw_DiagonalCeilingTopRightA/B
  // Draws 1x1 tiles going DOWN-RIGHT diagonal (Y+1, X+1 each iteration)
  // Count formula: LDA #$0004; JSR GetSize_1to16_timesA -> count = size + 4
  int size = obj.size_ & 0x0F;
  int count = size + 4;

  if (tiles.empty()) return;

  for (int s = 0; s < count; s++) {
    // Assembly: ADC #$0082 = move down 1 row + right 1 tile
    WriteTile8(bg, obj.x_ + s, obj.y_ + s, tiles[0]);
  }
}

void ObjectDrawer::DrawDiagonalCeilingBottomRight(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Diagonal ceiling bottom-right (objects 0xA3, 0xA8, 0xAC)
  // Assembly: RoomDraw_DiagonalCeilingBottomRightA/B
  // Draws 1x1 tiles going UP-RIGHT diagonal (Y-1, X+1 each iteration)
  // Count formula: LDA #$0004; JSR GetSize_1to16_timesA -> count = size + 4
  int size = obj.size_ & 0x0F;
  int count = size + 4;

  if (tiles.empty()) return;

  for (int s = 0; s < count; s++) {
    // Assembly: SEC; SBC #$007E = move up 1 row + right 1 tile
    WriteTile8(bg, obj.x_ + s, obj.y_ - s, tiles[0]);
  }
}

// ============================================================================
// Phase 4 Step 5: Special Routines (0xC1, 0xCD, 0xCE, 0xDC)
// ============================================================================

void ObjectDrawer::DrawClosedChestPlatform(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles) {
  // ASM: RoomDraw_ClosedChestPlatform ($018CC7)
  // 68 tiles: frame + carpet pattern
  // Width: B2 + 4, Height: B4 + 1
  if (tiles.size() < 16) return;

  int width = (obj.size_ & 0x0F) + 4;
  int height = ((obj.size_ >> 4) & 0x0F) + 1;

  // Draw outer frame + interior carpet pattern
  size_t tile_idx = 0;
  for (int y = 0; y < height * 3; ++y) {
    for (int x = 0; x < width * 2; ++x) {
      WriteTile8(bg, obj.x_ + x, obj.y_ + y, tiles[tile_idx % tiles.size()]);
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
  // 21 tiles, multi-segment pattern
  if (tiles.size() < 8) return;

  int width = (obj.size_ & 0x0F) + 1;
  int segments = ((obj.size_ >> 4) & 0x0F) * 2 + 5;

  // Draw segments
  for (int seg = 0; seg < segments; ++seg) {
    for (int x = 0; x < width + 1; ++x) {
      size_t tile_idx = (seg * width + x) % tiles.size();
      WriteTile8(bg, obj.x_ + x, obj.y_ + seg, tiles[tile_idx]);
    }
  }
}

// ============================================================================
// Utility Methods
// ============================================================================

void ObjectDrawer::WriteTile8(gfx::BackgroundBuffer& bg, int tile_x, int tile_y,
                              const gfx::TileInfo& tile_info) {
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
    printf("[DrawTile] id=%d (col=%d,row=%d) -> gfx offset=%d (0x%04X)\n",
           tile_info.id_, tile_col, tile_row, sample_index, sample_index);
    printf("  First 8 pixels at tile: ");
    for (int i = 0; i < 8; i++) {
      printf("%02X ", tiledata[sample_index + i]);
    }
    printf("\n");
    draw_debug_count++;
  }

  // Palette offset calculation based on SNES CGRAM layout from ASM analysis:
  // - PaletteLoad_UnderworldSet loads dungeon palette to CGRAM starting at 0x0042
  // - CGRAM 0x42 = color index 33 = row 2, position 1
  // - Dungeon tiles use palette bits 2-7, mapping to CGRAM rows 2-7
  // - Each CGRAM row has 16 colors; ROM stores 15 colors per row (index 0 = transparent)
  // - 6 rows × 15 colors = 90 colors in ROM, loaded to CGRAM indices 33-47, 49-63, etc.
  //
  // Tile palette bits → ROM palette offset:
  //   2 → 0-14, 3 → 15-29, 4 → 30-44, 5 → 45-59, 6 → 60-74, 7 → 75-89
  uint8_t pal = tile_info.palette_ & 0x07;
  uint8_t palette_offset;
  if (pal >= 2 && pal <= 7) {
    // Dungeon BG tiles use palette bits 2-7, mapping to ROM indices 0-89
    palette_offset = (pal - 2) * 15;
  } else {
    // Palette 0-1 are for HUD/other - fallback to first sub-palette
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
        // Pixel 0 is transparent. Pixel 1 maps to index 0 of the 15-color palette.
        uint8_t final_color = (pixel - 1) + palette_offset;
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
  // Type 3 objects (0xF9E-0xFA1) use 8 tiles in 4x2 bottom corner layout
  if (tiles.size() >= 16) {
    DrawCorner4x4(obj, bg, tiles);
  } else if (tiles.size() >= 8) {
    // Draw 4x2 bottom corner pattern (row-major for bottom corners)
    int tid = 0;
    for (int yy = 0; yy < 2; yy++) {
      for (int xx = 0; xx < 4; xx++) {
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
  // Type 3 objects (0xFA2-0xFA5) use 8 tiles in 4x2 top corner layout
  if (tiles.size() >= 16) {
    DrawCorner4x4(obj, bg, tiles);
  } else if (tiles.size() >= 8) {
    // Draw 4x2 top corner pattern (row-major for top corners)
    int tid = 0;
    for (int yy = 0; yy < 2; yy++) {
      for (int xx = 0; xx < 4; xx++) {
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

    case 1:  // 2x4 elongated horizontal
    case 2:
    case 3:
      width = 16;
      height = 32;
      break;

    case 5:  // DrawDiagonalAcute_1to16
    case 6:  // DrawDiagonalGrave_1to16
    case 17: // DrawDiagonalAcute_1to16_BothBG
    case 18: // DrawDiagonalGrave_1to16_BothBG
      // Diagonal patterns (Walls 0x10-0x1F map here)
      // Actual draw routines use count = size + 7 iterations
      // Each step advances X by 1 tile, Y changes by 1 tile (up or down)
      // Total width/height = (size + 7) * 8 pixels
      size = size & 0x0F;
      width = (size + 7) * 8;
      height = (size + 7) * 8;
      break;

    case 8:  // 4x2 (vertical)
    case 9:
    case 10:
      width = 32;
      height = 16;
      break;

    case 12: // 2x2 downwards extended
    case 13:
    case 14:
    case 15:
      // 2x2 base with size extension downward
      size = size & 0x0F;
      width = 16;
      height = 16 + size * 16;
      break;

    case 16: // DrawRightwards4x4_1to16 (Routine 16)
    case 34: // Water Face (4x4)
    case 35: // 4x4 Corner BothBG
    case 36: // Weird Corner Bottom
    case 37: // Weird Corner Top
      // 4x4 tiles (32x32 pixels)
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

    case 20: // Edge 1x2
      width = 8;
      height = 16;
      break;

    case 21: // Edge 1x1
    case 22: // Edge 1x1
    case 25: // Solid 1x1
      width = 8;
      height = 8;
      break;

    case 23: // RightwardsTopCorners1x2_1to16_plus13
    case 24: // RightwardsBottomCorners1x2_1to16_plus13
      size = size & 0x0F;
      width = 8 + size * 8;
      height = 16;
      break;

    case 26: // Door Switcher
    case 27: // Decor 4x4
      width = 32;
      height = 32;
      break;

    case 28: // Statue 2x3
      width = 16;
      height = 24;
      break;

    case 29: // Pillar 2x4
      width = 16;
      height = 32;
      break;

    case 30: // Decor 4x3
      width = 32;
      height = 24;
      break;

    case 31: // Doubled 2x2
    case 32: // Decor 2x2
      width = 16;
      height = 16;
      break;

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
