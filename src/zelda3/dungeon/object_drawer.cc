#include "object_drawer.h"

#include <cstdio>

#include "absl/strings/str_format.h"
#include "app/gfx/types/snes_tile.h"
#include "util/log.h"
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

  // Check if this is a BothBG routine (routines 3, 9, 17, 18)
  // These routines should draw to both BG1 and BG2
  // We now use the all_bgs flag from the object itself, which is set during decoding
  bool is_both_bg = object.all_bgs_ || (routine_id == 3 || routine_id == 9 ||
                     routine_id == 17 || routine_id == 18);

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
  for (int id = 0x01; id <= 0x02; id++) {
    object_to_routine_map_[id] = 1;
  }
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
  // 0x49-0x4A Floor Tile 4x2
  object_to_routine_map_[0x49] = 1; // 2x4 (close enough)
  object_to_routine_map_[0x4A] = 1;
  // 0x4B Decor 2x2 spaced 12
  object_to_routine_map_[0x4B] = 32;
  // 0x4C Bar 4x3
  object_to_routine_map_[0x4C] = 30; // Decor 4x3
  // 0x4D-0x4F Shelf 4x4
  object_to_routine_map_[0x4D] = 16; // 4x4
  object_to_routine_map_[0x4E] = 16;
  object_to_routine_map_[0x4F] = 16;

  // 0x50-0x5F Objects (based on tile counts from kSubtype1TileLengths)
  // 0x50: 1 tile
  object_to_routine_map_[0x50] = 25;  // 1x1
  // 0x51-0x52: 18 tiles (2x9 or similar)
  object_to_routine_map_[0x51] = 1;   // 2x4 (best fit for elongated)
  object_to_routine_map_[0x52] = 1;
  // 0x53: 4 tiles
  object_to_routine_map_[0x53] = 4;   // 2x2
  // 0x54: 1 tile
  object_to_routine_map_[0x54] = 38;  // Nothing (Logic: RoomDraw_Nothing_B)
  // 0x55-0x56: 8 tiles
  object_to_routine_map_[0x55] = 1;   // 2x4
  object_to_routine_map_[0x56] = 1;
  // 0x57-0x5A: 1 tile each
  object_to_routine_map_[0x57] = 38;  // Nothing (Logic: RoomDraw_Nothing_C)
  object_to_routine_map_[0x58] = 38;
  object_to_routine_map_[0x59] = 38;
  object_to_routine_map_[0x5A] = 38;
  // 0x5B-0x5C: 18 tiles
  object_to_routine_map_[0x5B] = 1;   // 2x4
  object_to_routine_map_[0x5C] = 1;
  // 0x5D: 15 tiles
  object_to_routine_map_[0x5D] = 30;  // 4x3 (closest fit)
  // 0x5E: 4 tiles
  object_to_routine_map_[0x5E] = 4;   // 2x2
  // 0x5F: 3 tiles
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

  // 0x70-0x7F Objects (based on tile counts from kSubtype1TileLengths)
  // 0x70: 16 tiles
  object_to_routine_map_[0x70] = 16;  // 4x4
  // 0x71-0x72: 1 tile each
  object_to_routine_map_[0x71] = 25;  // 1x1
  object_to_routine_map_[0x72] = 38;  // Nothing (Logic: RoomDraw_Nothing_B)
  // 0x73-0x74: 16 tiles each
  object_to_routine_map_[0x73] = 16;  // 4x4
  object_to_routine_map_[0x74] = 16;
  // 0x75: 8 tiles
  object_to_routine_map_[0x75] = 1;   // 2x4
  // 0x76-0x77: 16 tiles each
  object_to_routine_map_[0x76] = 16;  // 4x4
  object_to_routine_map_[0x77] = 16;
  // 0x78: 4 tiles
  object_to_routine_map_[0x78] = 4;   // 2x2
  // 0x79-0x7A: 1 tile each
  object_to_routine_map_[0x79] = 25;  // 1x1
  object_to_routine_map_[0x7A] = 25;
  // 0x7B: 4 tiles
  object_to_routine_map_[0x7B] = 4;   // 2x2
  // 0x7C: 1 tile
  object_to_routine_map_[0x7C] = 25;  // 1x1
  // 0x7D: 4 tiles
  object_to_routine_map_[0x7D] = 4;   // 2x2
  // 0x7E: 1 tile
  object_to_routine_map_[0x7E] = 38;  // Nothing (Logic: RoomDraw_Nothing_B)
  // 0x7F: 8 tiles
  object_to_routine_map_[0x7F] = 1;   // 2x4

  // 0x80-0x8F Objects (based on tile counts from kSubtype1TileLengths)
  // 0x80: 8 tiles
  object_to_routine_map_[0x80] = 8;   // 4x2 (vertical)
  // 0x81-0x84: 12 tiles each
  object_to_routine_map_[0x81] = 30;  // 4x3
  object_to_routine_map_[0x82] = 30;
  object_to_routine_map_[0x83] = 30;
  object_to_routine_map_[0x84] = 30;
  // 0x85-0x86: 18 tiles each
  object_to_routine_map_[0x85] = 8;   // 4x2 (best fit for vertical elongated)
  object_to_routine_map_[0x86] = 8;
  // 0x87: 8 tiles
  object_to_routine_map_[0x87] = 8;   // 4x2
  // 0x88: 12 tiles
  object_to_routine_map_[0x88] = 30;  // 4x3
  // 0x89: 4 tiles
  object_to_routine_map_[0x89] = 11;  // 2x2 downwards
  // 0x8A-0x8C: 3 tiles each
  object_to_routine_map_[0x8A] = 22;  // edge 1x1
  object_to_routine_map_[0x8B] = 22;
  object_to_routine_map_[0x8C] = 22;
  // 0x8D-0x8E: 1 tile each
  object_to_routine_map_[0x8D] = 25;  // 1x1
  object_to_routine_map_[0x8E] = 25;
  // 0x8F: 6 tiles
  object_to_routine_map_[0x8F] = 28;  // 2x3

  // 0x90-0x9F Objects (based on tile counts from kSubtype1TileLengths)
  // 0x90-0x91: 8 tiles each
  object_to_routine_map_[0x90] = 8;   // 4x2
  object_to_routine_map_[0x91] = 8;
  // 0x92-0x93: 4 tiles each
  object_to_routine_map_[0x92] = 11;  // 2x2 downwards
  object_to_routine_map_[0x93] = 11;
  // 0x94: 16 tiles
  object_to_routine_map_[0x94] = 16;  // 4x4
  // 0x95-0x96: 4 tiles each
  object_to_routine_map_[0x95] = 11;  // 2x2
  object_to_routine_map_[0x96] = 11;
  // 0x97-0x9F: 1 tile each
  for (int id = 0x97; id <= 0x9F; id++) {
    object_to_routine_map_[id] = 38;  // Nothing (Logic: RoomDraw_Nothing_B)
  }

  // 0xA0-0xAF Objects (mostly 1 tile each, except 0xA4 with 24 tiles)
  for (int id = 0xA0; id <= 0xAF; id++) {
    if (id == 0xA4) {
      object_to_routine_map_[id] = 16;  // 4x4 (best fit for 24 tiles)
    } else if (id >= 0xAD && id <= 0xAF) {
      object_to_routine_map_[id] = 38;  // Nothing (Logic: RoomDraw_Nothing_B)
    } else {
      object_to_routine_map_[id] = 25;  // 1x1
    }
  }

  // 0xB0-0xBF Objects (based on tile counts from kSubtype1TileLengths)
  // 0xB0-0xB1: 1 tile each
  object_to_routine_map_[0xB0] = 25;  // 1x1
  object_to_routine_map_[0xB1] = 25;
  // 0xB2: 16 tiles
  object_to_routine_map_[0xB2] = 16;  // 4x4
  // 0xB3-0xB4: 3 tiles each
  object_to_routine_map_[0xB3] = 22;  // edge 1x1
  object_to_routine_map_[0xB4] = 22;
  // 0xB5-0xB7: 8 tiles each
  object_to_routine_map_[0xB5] = 8;   // 4x2
  object_to_routine_map_[0xB6] = 8;
  object_to_routine_map_[0xB7] = 8;
  // 0xB8-0xB9: 4 tiles each
  object_to_routine_map_[0xB8] = 11;  // 2x2
  object_to_routine_map_[0xB9] = 11;
  // 0xBA: 16 tiles
  object_to_routine_map_[0xBA] = 16;  // 4x4
  // 0xBB-0xBD: 4 tiles each
  object_to_routine_map_[0xBB] = 11;  // 2x2
  object_to_routine_map_[0xBC] = 11;
  object_to_routine_map_[0xBD] = 11;
  // 0xBE-0xBF: 1 tile each
  object_to_routine_map_[0xBE] = 38;  // Nothing (Logic: RoomDraw_Nothing_B)
  object_to_routine_map_[0xBF] = 38;

  // 0xC0-0xCF Objects (based on tile counts from kSubtype1TileLengths)
  // 0xC0: 1 tile
  object_to_routine_map_[0xC0] = 25;  // 1x1
  // 0xC1: 68 tiles (special large object)
  object_to_routine_map_[0xC1] = 16;  // 4x4 fallback
  // 0xC2-0xC3: 1 tile each
  object_to_routine_map_[0xC2] = 25;  // 1x1
  object_to_routine_map_[0xC3] = 25;
  // 0xC4: Nothing (RoomDraw_Nothing_B)
  object_to_routine_map_[0xC4] = 38;
  // 0xC5-0xCA: 8 tiles each
  for (int id = 0xC5; id <= 0xCA; id++) {
    object_to_routine_map_[id] = 8;   // 4x2
  }
  // 0xCB-0xCC: 1 tile each
  object_to_routine_map_[0xCB] = 38;  // Nothing (RoomDraw_Nothing_E)
  object_to_routine_map_[0xCC] = 38;
  // 0xCD-0xCE: 28 tiles each (large objects) - Moving Walls
  object_to_routine_map_[0xCD] = 16;  // 4x4 fallback (TODO: Logic check)
  object_to_routine_map_[0xCE] = 16;
  // 0xCF: 1 tile
  object_to_routine_map_[0xCF] = 38;  // Nothing (RoomDraw_Nothing_D)

  // 0xD0-0xDF Objects (based on tile counts from kSubtype1TileLengths)
  // 0xD0: 1 tile
  object_to_routine_map_[0xD0] = 38;  // Nothing (RoomDraw_Nothing_D)
  // 0xD1-0xD2: 8 tiles each
  object_to_routine_map_[0xD1] = 8;   // 4x2
  object_to_routine_map_[0xD2] = 8;
  // 0xD3-0xD6: 0 tiles (special/empty)
  object_to_routine_map_[0xD3] = 38;  // CheckIfWallIsMoved (Logic)
  object_to_routine_map_[0xD4] = 38;
  object_to_routine_map_[0xD5] = 38;
  object_to_routine_map_[0xD6] = 38;
  // 0xD7: 1 tile
  object_to_routine_map_[0xD7] = 25;  // 1x1
  // 0xD8-0xDB: 8 tiles each
  object_to_routine_map_[0xD8] = 8;   // 4x2
  object_to_routine_map_[0xD9] = 8;
  object_to_routine_map_[0xDA] = 8;
  object_to_routine_map_[0xDB] = 8;
  // 0xDC: 21 tiles
  object_to_routine_map_[0xDC] = 16;  // 4x4 fallback
  // 0xDD: 16 tiles
  object_to_routine_map_[0xDD] = 16;  // 4x4
  // 0xDE: 4 tiles
  object_to_routine_map_[0xDE] = 11;  // 2x2
  // 0xDF: 8 tiles
  object_to_routine_map_[0xDF] = 8;   // 4x2

  // 0xE0-0xEF Objects (based on tile counts from kSubtype1TileLengths)
  // 0xE0-0xE8: 8 tiles each
  for (int id = 0xE0; id <= 0xE8; id++) {
    object_to_routine_map_[id] = 8;   // 4x2
  }
  // 0xE9-0xEF: 1 tile each - Nothing (RoomDraw_Nothing_B)
  for (int id = 0xE9; id <= 0xEF; id++) {
    object_to_routine_map_[id] = 38;
  }

  // 0xF0-0xF7 Objects (all 1 tile) - Nothing (RoomDraw_Nothing_B)
  for (int id = 0xF0; id <= 0xF7; id++) {
    object_to_routine_map_[id] = 38;
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
  // 0x12D-0x12F Stairs -> Map to 4x4 (16)
  object_to_routine_map_[0x12D] = 16;
  object_to_routine_map_[0x12E] = 16;
  object_to_routine_map_[0x12F] = 16;
  // 0x130-0x133 Stairs -> Map to 4x4 (16)
  for (int id = 0x130; id <= 0x133; id++) {
    object_to_routine_map_[id] = 16;
  }
  // 0x134 Rightwards 2x2 -> Map to 2x2 (4)
  object_to_routine_map_[0x134] = 4;
  // 0x135-0x136 Water Stairs -> Map to 4x4 (16)
  object_to_routine_map_[0x135] = 16;
  object_to_routine_map_[0x136] = 16;
  // 0x137 Dam -> Map to 4x4 (16)
  object_to_routine_map_[0x137] = 16;
  // 0x138-0x13B Spiral Stairs -> Map to 2x2 (4)
  for (int id = 0x138; id <= 0x13B; id++) {
    object_to_routine_map_[id] = 4;
  }
  // 0x13C Sanctuary Wall -> Map to 4x4 (16)
  object_to_routine_map_[0x13C] = 16;
  // 0x13D Table 4x3 -> Map to 4x3 (30)
  object_to_routine_map_[0x13D] = 30;
  // 0x13E Utility 6x3 -> Map to 4x3 (30)
  object_to_routine_map_[0x13E] = 30;
  // 0x13F Magic Bat Altar -> Map to 4x4 (16)
  object_to_routine_map_[0x13F] = 16;

  // 0x140-0x14F Big Chests -> Map to 4x4 (16)
  for (int id = 0x140; id <= 0x14F; id++) {
    object_to_routine_map_[id] = 39; // DrawChest
  }
  // 0x150-0x15F Small Chests -> Map to 2x2 (4)
  for (int id = 0x150; id <= 0x15F; id++) {
    object_to_routine_map_[id] = 39; // DrawChest
  }
  // 0x160-0x1FF Misc Objects (Pots, etc.)
  // Pots (0x160-0x16F) -> Map to 1x1 (25)
  for (int id = 0x160; id <= 0x16F; id++) {
    object_to_routine_map_[id] = 25;
  }
  // 0x170-0x17F ? -> Map to 4x4 (16) as fallback
  for (int id = 0x170; id <= 0x17F; id++) {
    object_to_routine_map_[id] = 16;
  }
  // 0x180-0x1FF ? -> Map to 4x4 (16) as fallback
  for (int id = 0x180; id <= 0x1FF; id++) {
    object_to_routine_map_[id] = 16;
  }

  // Subtype 3 Object Mappings (0xF80-0xFFF)
  // Type 3 object IDs are 0xF80-0xFFF (128 objects)
  // Index = (object_id - 0xF80) & 0x7F maps to table indices 0-127
  object_to_routine_map_[0xF80] = 34; // Water Face (index 0)
  object_to_routine_map_[0xF81] = 34;
  object_to_routine_map_[0xF82] = 34;
  for (int id = 0xF83; id <= 0xF89; id++) {
    object_to_routine_map_[id] = 33; // Somaria Line
  }
  for (int id = 0xF8A; id <= 0xF8C; id++) {
    object_to_routine_map_[id] = 33;
  }
  // 0xF8D: PrisonCell -> 1x1 placeholder
  object_to_routine_map_[0xF8D] = 25;
  object_to_routine_map_[0xF8E] = 33;
  object_to_routine_map_[0xF8F] = 33;
  for (int id = 0xF90; id <= 0xF93; id++) {
    object_to_routine_map_[id] = 16; // 4x4
  }
  object_to_routine_map_[0xF94] = 33; // Somaria Line
  object_to_routine_map_[0xF95] = 16;
  object_to_routine_map_[0xF96] = 16;
  for (int id = 0xF97; id <= 0xF9A; id++) {
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
  draw_routines_.reserve(40);

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
  // Direction: 0=North, 1=South, 2=West, 3=East
  // Dimensions: North/South = 4x3 tiles, East/West = 3x4 tiles

  if (!rom_ || !rom_->is_loaded()) return;

  auto& bitmap = bg1.bitmap();
  if (!bitmap.is_active() || bitmap.width() == 0) return;

  // Door position encoding:
  // The position byte encodes where on the wall edge the door is placed.
  // For horizontal walls (N/S): position determines X coordinate
  // For vertical walls (E/W): position determines Y coordinate
  // Position is in tile units (8 pixels each)

  int tile_x = 0;
  int tile_y = 0;
  int door_width = 0;   // in tiles
  int door_height = 0;  // in tiles

  // Calculate door position based on direction
  // Position byte encodes door location along the wall
  switch (door.direction & 0x03) {
    case 0:  // North (top wall)
      tile_x = (door.position & 0x1F) * 2;  // X position along top
      tile_y = 0;                            // At top edge
      door_width = 4;
      door_height = 3;
      break;

    case 1:  // South (bottom wall)
      tile_x = (door.position & 0x1F) * 2;  // X position along bottom
      tile_y = 61;                           // Near bottom edge (64-3)
      door_width = 4;
      door_height = 3;
      break;

    case 2:  // West (left wall)
      tile_x = 0;                            // At left edge
      tile_y = (door.position & 0x1F) * 2;  // Y position along left
      door_width = 3;
      door_height = 4;
      break;

    case 3:  // East (right wall)
      tile_x = 61;                           // Near right edge (64-3)
      tile_y = (door.position & 0x1F) * 2;  // Y position along right
      door_width = 3;
      door_height = 4;
      break;
  }

  // Get door graphics address based on direction
  int gfx_base_addr = 0;
  switch (door.direction & 0x03) {
    case 0: gfx_base_addr = kDoorGfxUp; break;
    case 1: gfx_base_addr = kDoorGfxDown; break;
    case 2: gfx_base_addr = kDoorGfxLeft; break;
    case 3: gfx_base_addr = kDoorGfxRight; break;
  }

  // Each door type has a different offset into the graphics table
  // Door types are stored in upper nibble of byte 2
  // Graphics are 2 bytes per tile (tile ID word)
  int type_offset = door.type * (door_width * door_height * 2);
  int gfx_addr = gfx_base_addr + type_offset;

  // Clamp graphics address to valid ROM range
  if (gfx_addr < 0 || gfx_addr + (door_width * door_height * 2) >
      static_cast<int>(rom_->size())) {
    // Fall back to drawing a simple door indicator
    DrawDoorIndicator(bitmap, tile_x, tile_y, door_width, door_height,
                      door.type, door.direction);
    return;
  }

  // Draw door tiles from ROM graphics data
  const auto& rom_data = rom_->data();
  int tile_idx = 0;

  for (int dy = 0; dy < door_height; dy++) {
    for (int dx = 0; dx < door_width; dx++) {
      int addr = gfx_addr + (tile_idx * 2);
      if (addr + 1 < static_cast<int>(rom_->size())) {
        uint16_t tile_word = rom_data[addr] | (rom_data[addr + 1] << 8);
        auto tile_info = gfx::WordToTileInfo(tile_word);

        // Draw tile to bitmap
        int pixel_x = (tile_x + dx) * 8;
        int pixel_y = (tile_y + dy) * 8;
        DrawTileToBitmap(bitmap, tile_info, pixel_x, pixel_y, room_gfx_buffer_);
      }
      tile_idx++;
    }
  }

  LOG_DEBUG("ObjectDrawer", "DrawDoor: type=%d dir=%d pos=%d at tile(%d,%d) size=%dx%d",
            door.type, door.direction, door.position, tile_x, tile_y,
            door_width, door_height);
}

void ObjectDrawer::DrawDoorIndicator(gfx::Bitmap& bitmap, int tile_x, int tile_y,
                                      int width, int height, uint8_t type,
                                      uint8_t direction) {
  // Draw a simple colored rectangle as door indicator when graphics unavailable
  // Different colors for different door types

  uint8_t color_idx;
  switch (type) {
    case 0x00:  // Regular
    case 0x02:  // Regular2
    case 0x40:  // RegularDoor33
      color_idx = 45;  // Standard door color
      break;

    case 0x1C:  // SmallKeyDoor
      color_idx = 60;  // Key door - yellowish
      break;

    case 0x28:  // BreakableWall
    case 0x30:  // LgExplosion
      color_idx = 15;  // Bombable - brownish
      break;

    case 0x44:  // Shutter
    case 0x18:  // ShuttersTwoWay
    case 0x48:  // ShutterTrap
      color_idx = 30;  // Shutter - greenish
      break;

    case 0x1A:  // InvisibleDoor
      color_idx = 5;  // Invisible - faint
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

void ObjectDrawer::DrawRightwards2x4spaced4_1to16(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles, [[maybe_unused]] const DungeonState* state) {
  // Pattern: Draws 2x4 tiles rightward with spacing (objects 0x03-0x04)
  // Uses RoomDraw_Nx4 with N=2, tiles are COLUMN-MAJOR, spacing of 4 tiles
  int size = obj.size_ & 0x0F;

  // Assembly: JSR RoomDraw_GetSize_1to16
  // GetSize_1to16: count = size + 1
  int count = size + 1;

  for (int s = 0; s < count; s++) {
    if (tiles.size() >= 8) {
      // Draw 2x4 pattern in COLUMN-MAJOR order with 4-tile spacing
      // Column 0 (tiles 0-3)
      WriteTile8(bg, obj.x_ + (s * 6), obj.y_, tiles[0]);      // col 0, row 0
      WriteTile8(bg, obj.x_ + (s * 6), obj.y_ + 1, tiles[1]);  // col 0, row 1
      WriteTile8(bg, obj.x_ + (s * 6), obj.y_ + 2, tiles[2]);  // col 0, row 2
      WriteTile8(bg, obj.x_ + (s * 6), obj.y_ + 3, tiles[3]);  // col 0, row 3
      // Column 1 (tiles 4-7)
      WriteTile8(bg, obj.x_ + (s * 6) + 1, obj.y_, tiles[4]);      // col 1, row 0
      WriteTile8(bg, obj.x_ + (s * 6) + 1, obj.y_ + 1, tiles[5]);  // col 1, row 1
      WriteTile8(bg, obj.x_ + (s * 6) + 1, obj.y_ + 2, tiles[6]);  // col 1, row 2
      WriteTile8(bg, obj.x_ + (s * 6) + 1, obj.y_ + 3, tiles[7]);  // col 1, row 3
    } else if (tiles.size() >= 4) {
      // Fallback: with 4 tiles we can only draw 1 column (1x4 pattern)
      WriteTile8(bg, obj.x_ + (s * 6), obj.y_, tiles[0]);
      WriteTile8(bg, obj.x_ + (s * 6), obj.y_ + 1, tiles[1]);
      WriteTile8(bg, obj.x_ + (s * 6), obj.y_ + 2, tiles[2]);
      WriteTile8(bg, obj.x_ + (s * 6), obj.y_ + 3, tiles[3]);
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

  // Palette offset: Dungeon palette uses 15 colors per group (90 total = 6 sub-palettes)
  // Pixel 0 is transparent and skipped. Pixel 1 maps to index 0.
  // SNES tilemap allows palette 0-7, but we only have 6 sub-palettes (0-5).
  // Clamp palette index to valid range to prevent out-of-bounds color access.
  uint8_t pal = tile_info.palette_ & 0x07;
  if (pal > 5) {
    pal = pal % 6;  // Wrap palettes 6,7 to 0,1
  }
  uint8_t palette_offset = pal * 15;

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
      // Logic: iterates s from 0 to size + 6
      // Each step advances X by 1 tile.
      // Y changes by 1 tile per step (up or down).
      // Total width/height approx (size + 6) * 8
      size = size & 0x0F;
      width = (size + 6) * 8;
      height = (size + 6) * 8;
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

    case 39: // DrawChest
      width = 16;
      height = 16;
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