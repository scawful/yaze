#include "room.h"

#include <yaze.h>

#include <cmath>
#include <cstdint>
#include <vector>

#include "absl/strings/str_cat.h"
#include "app/core/window.h"
#include "app/gfx/arena.h"
#include "app/rom.h"
#include "app/snes.h"
#include "app/zelda3/dungeon/room_diagnostic.h"
#include "app/zelda3/dungeon/room_object.h"
#include "app/zelda3/sprite/sprite.h"

namespace yaze {
namespace zelda3 {

RoomSize CalculateRoomSize(Rom *rom, int room_id) {
  // Calculate the size of the room based on how many objects are used per room
  // Some notes from hacker Zarby89
  // vanilla rooms are using in average ~0x80 bytes
  // a "normal" person who wants more details than vanilla will use around 0x100
  // bytes per rooms you could fit 128 rooms like that in 1 bank
  // F8000 I don't remember if that's PC or snes tho
  // Check last rooms
  // F8000+(roomid*3)
  // So we want to search the rom() object at this addressed based on the room
  // ID since it's the roomid * 3 we will by pulling 3 bytes at a time We can do
  // this with the rom()->ReadByteVector(addr, size)
  // Existing room size address calculation...
  RoomSize room_size;
  room_size.room_size_pointer = 0;
  room_size.room_size = 0;

  auto room_size_address = 0xF8000 + (room_id * 3);
  // util::logf("Room #%#03X Addresss: %#06X", room_id, room_size_address);

  // Reading bytes for long address construction
  uint8_t low = rom->data()[room_size_address];
  uint8_t high = rom->data()[room_size_address + 1];
  uint8_t bank = rom->data()[room_size_address + 2];

  // Constructing the long address
  int long_address = (bank << 16) | (high << 8) | low;
  // util::logf("%#06X", long_address);
  room_size.room_size_pointer = long_address;

  if (long_address == 0x0A8000) {
    // Blank room disregard in size calculation
    // util::logf("Size of Room #%#03X: 0 bytes", room_id);
    room_size.room_size = 0;
  } else {
    // use the long address to calculate the size of the room
    // we will use the room_id_ to calculate the next room's address
    // and subtract the two to get the size of the room

    int next_room_address = 0xF8000 + ((room_id + 1) * 3);
    // util::logf("Next Room Address: %#06X", next_room_address);

    // Reading bytes for long address construction
    uint8_t next_low = rom->data()[next_room_address];
    uint8_t next_high = rom->data()[next_room_address + 1];
    uint8_t next_bank = rom->data()[next_room_address + 2];

    // Constructing the long address
    int next_long_address = (next_bank << 16) | (next_high << 8) | next_low;
    // util::logf("%#06X", next_long_address);

    // Calculate the size of the room
    int actual_room_size = next_long_address - long_address;
    room_size.room_size = actual_room_size;
    // util::logf("Size of Room #%#03X: %d bytes", room_id, actual_room_size);
  }

  return room_size;
}

Room LoadRoomFromRom(Rom *rom, int room_id) {
  Room room(room_id, rom);

  int header_pointer = (rom->data()[kRoomHeaderPointer + 2] << 16) +
                       (rom->data()[kRoomHeaderPointer + 1] << 8) +
                       (rom->data()[kRoomHeaderPointer]);
  header_pointer = SnesToPc(header_pointer);

  int address = (rom->data()[kRoomHeaderPointerBank] << 16) +
                (rom->data()[(header_pointer + 1) + (room_id * 2)] << 8) +
                rom->data()[(header_pointer) + (room_id * 2)];

  auto header_location = SnesToPc(address);

  room.SetBg2((background2)((rom->data()[header_location] >> 5) & 0x07));
  room.SetCollision((CollisionKey)((rom->data()[header_location] >> 2) & 0x07));
  room.SetIsLight(((rom->data()[header_location]) & 0x01) == 1);

  if (room.IsLight()) {
    room.SetBg2(background2::DarkRoom);
  }

  room.SetPalette(((rom->data()[header_location + 1] & 0x3F)));
  room.SetBlockset((rom->data()[header_location + 2]));
  room.SetSpriteset((rom->data()[header_location + 3]));
  room.SetEffect((EffectKey)((rom->data()[header_location + 4])));
  room.SetTag1((TagKey)((rom->data()[header_location + 5])));
  room.SetTag2((TagKey)((rom->data()[header_location + 6])));

  room.SetStaircasePlane(0, ((rom->data()[header_location + 7] >> 2) & 0x03));
  room.SetStaircasePlane(1, ((rom->data()[header_location + 7] >> 4) & 0x03));
  room.SetStaircasePlane(2, ((rom->data()[header_location + 7] >> 6) & 0x03));
  room.SetStaircasePlane(3, ((rom->data()[header_location + 8]) & 0x03));

  room.SetHolewarp((rom->data()[header_location + 9]));
  room.SetStaircaseRoom(0, (rom->data()[header_location + 10]));
  room.SetStaircaseRoom(1, (rom->data()[header_location + 11]));
  room.SetStaircaseRoom(2, (rom->data()[header_location + 12]));
  room.SetStaircaseRoom(3, (rom->data()[header_location + 13]));

  // =====

  int header_pointer_2 = (rom->data()[kRoomHeaderPointer + 2] << 16) +
                         (rom->data()[kRoomHeaderPointer + 1] << 8) +
                         (rom->data()[kRoomHeaderPointer]);
  header_pointer_2 = SnesToPc(header_pointer_2);

  int address_2 = (rom->data()[kRoomHeaderPointerBank] << 16) +
                  (rom->data()[(header_pointer_2 + 1) + (room_id * 2)] << 8) +
                  rom->data()[(header_pointer_2) + (room_id * 2)];

  room.SetMessageIdDirect(messages_id_dungeon + (room_id * 2));

  auto hpos = SnesToPc(address_2);
  hpos++;
  uint8_t b = rom->data()[hpos];

  room.SetLayer2Mode((b >> 5));
  room.SetLayerMerging(kLayerMergeTypeList[(b & 0x0C) >> 2]);

  room.SetIsDark((b & 0x01) == 0x01);
  hpos++;
  room.SetPaletteDirect(rom->data()[hpos]);
  hpos++;

  room.SetBackgroundTileset(rom->data()[hpos]);
  hpos++;

  room.SetSpriteTileset(rom->data()[hpos]);
  hpos++;

  room.SetLayer2Behavior(rom->data()[hpos]);
  hpos++;

  room.SetTag1Direct((TagKey)rom->data()[hpos]);
  hpos++;

  room.SetTag2Direct((TagKey)rom->data()[hpos]);
  hpos++;

  b = rom->data()[hpos];

  room.SetPitsTargetLayer((uint8_t)(b & 0x03));
  room.SetStair1TargetLayer((uint8_t)((b >> 2) & 0x03));
  room.SetStair2TargetLayer((uint8_t)((b >> 4) & 0x03));
  room.SetStair3TargetLayer((uint8_t)((b >> 6) & 0x03));
  hpos++;
  room.SetStair4TargetLayer((uint8_t)(rom->data()[hpos] & 0x03));
  hpos++;

  room.SetPitsTarget(rom->data()[hpos]);
  hpos++;
  room.SetStair1Target(rom->data()[hpos]);
  hpos++;
  room.SetStair2Target(rom->data()[hpos]);
  hpos++;
  room.SetStair3Target(rom->data()[hpos]);
  hpos++;
  room.SetStair4Target(rom->data()[hpos]);
  hpos++;

  // Load room objects
  int object_pointer = SnesToPc(room_object_pointer);
  int room_address = object_pointer + (room_id * 3);
  int objects_location = SnesToPc(room_address);

  // Load sprites
  int spr_ptr = 0x040000 | rooms_sprite_pointer;
  int sprite_address = SnesToPc(dungeon_spr_ptrs | spr_ptr + (room_id * 2));

  // Load room layout
  room.LoadRoomLayout();
  
  // Load additional room features
  room.LoadDoors();
  room.LoadTorches();
  room.LoadBlocks();
  room.LoadPits();

  room.SetLoaded(true);
  return room;
}

void Room::LoadRoomGraphics(uint8_t entrance_blockset) {
  const auto &room_gfx = rom()->room_blockset_ids;
  const auto &sprite_gfx = rom()->spriteset_ids;

  for (int i = 0; i < 8; i++) {
    blocks_[i] = rom()->main_blockset_ids[blockset][i];
    if (i >= 6 && i <= 6) {
      // 3-6
      if (entrance_blockset != 0xFF &&
          room_gfx[entrance_blockset][i - 3] != 0) {
        blocks_[i] = room_gfx[entrance_blockset][i - 3];
      }
    }
  }

  blocks_[8] = 115 + 0;  // Static Sprites Blocksets (fairy,pot,ect...)
  blocks_[9] = 115 + 10;
  blocks_[10] = 115 + 6;
  blocks_[11] = 115 + 7;
  for (int i = 0; i < 4; i++) {
    blocks_[12 + i] = (uint8_t)(sprite_gfx[spriteset + 64][i] + 115);
  }  // 12-16 sprites
}

constexpr int kGfxBufferOffset = 92 * 2048;
constexpr int kGfxBufferStride = 512;
constexpr int kGfxBufferAnimatedFrameOffset = 7 * 2048;
constexpr int kGfxBufferAnimatedFrameStride = 512;
constexpr int kGfxBufferRoomOffset = 2048;
constexpr int kGfxBufferRoomSpriteOffset = 512;
constexpr int kGfxBufferRoomSpriteStride = 2048;
constexpr int kGfxBufferRoomSpriteLastLineOffset = 0x88;

void Room::CopyRoomGraphicsToBuffer() {
  if (!rom_ || !rom_->is_loaded()) {
    return;
  }
  
  auto gfx_buffer_data = rom()->mutable_graphics_buffer();
  if (!gfx_buffer_data || gfx_buffer_data->empty()) {
    return;
  }

  // Copy room graphics to buffer
  int sheet_pos = 0;
  for (int i = 0; i < 16; i++) {
    // Validate block index
    if (blocks_[i] < 0 || blocks_[i] > 255) {
      sheet_pos += kGfxBufferRoomOffset;
      continue;
    }
    
    int data = 0;
    int block_offset = blocks_[i] * kGfxBufferRoomOffset;
    
    // Validate block_offset bounds
    if (block_offset < 0 || block_offset >= static_cast<int>(gfx_buffer_data->size())) {
      sheet_pos += kGfxBufferRoomOffset;
      continue;
    }
    
    while (data < kGfxBufferRoomOffset) {
      int buffer_index = data + block_offset;
      if (buffer_index >= 0 && buffer_index < static_cast<int>(gfx_buffer_data->size())) {
        uint8_t map_byte = (*gfx_buffer_data)[buffer_index];
        if (i < 4) {
          map_byte += kGfxBufferRoomSpriteLastLineOffset;
        }

        // Validate current_gfx16_ access
        int gfx_index = data + sheet_pos;
        if (gfx_index >= 0 && gfx_index < static_cast<int>(sizeof(current_gfx16_))) {
          current_gfx16_[gfx_index] = map_byte;
        }
      }
      data++;
    }

    sheet_pos += kGfxBufferRoomOffset;
  }

  LoadAnimatedGraphics();
}

void Room::RenderRoomGraphics() {
  std::printf("\n=== RenderRoomGraphics Room %d ===\n", room_id_);
  
  CopyRoomGraphicsToBuffer();
  std::printf("1. Graphics buffer copied\n");

  gfx::Arena::Get().bg1().DrawFloor(rom()->vector(), tile_address,
                                    tile_address_floor, floor1_graphics_);
  gfx::Arena::Get().bg2().DrawFloor(rom()->vector(), tile_address,
                                    tile_address_floor, floor2_graphics_);
  std::printf("2. Floor pattern drawn\n");

  // Render layout and object tiles to background buffers
  RenderObjectsToBackground();
  std::printf("3. Objects rendered to buffer\n");

  gfx::Arena::Get().bg1().DrawBackground(std::span<uint8_t>(current_gfx16_));
  gfx::Arena::Get().bg2().DrawBackground(std::span<uint8_t>(current_gfx16_));
  std::printf("4. Background drawn from buffer\n");

  auto& bg1_bmp = gfx::Arena::Get().bg1().bitmap();
  auto& bg2_bmp = gfx::Arena::Get().bg2().bitmap();
  std::printf("5. BG1 bitmap: active=%d, size=%dx%d, data_size=%zu\n",
             bg1_bmp.is_active(), bg1_bmp.width(), bg1_bmp.height(), bg1_bmp.vector().size());

  // Get the palette for this room - just use the 90-color palette as-is
  // The SNES will index into this palette correctly without needing expansion
  auto& dungeon_pal_group = rom()->mutable_palette_group()->dungeon_main;
  int num_palettes = dungeon_pal_group.size();
  int palette_id = palette;
  
  // Validate palette ID and fall back to palette 0 if invalid
  if (palette_id < 0 || palette_id >= num_palettes) {
    //palette_id = 0;
  }
  
  // Load the 90-color dungeon palette directly
  // The palette contains colors for BG layers - sprite colors are handled separately
  auto bg1_palette = dungeon_pal_group.palette(palette_id);
  
  std::printf("5a. Palette loaded: room palette_id=%d (requested=%d), size=%zu colors\n", 
              palette_id, palette, bg1_palette.size());

  // CRITICAL: Apply palette to bitmaps BEFORE creating/updating textures
  bg1_bmp.SetPaletteWithTransparent(bg1_palette, 0);
  bg2_bmp.SetPaletteWithTransparent(bg1_palette, 0);
  std::printf("5b. Palette applied to bitmaps\n");

  // ALWAYS recreate textures when palette changes (UpdateBitmap doesn't update palette!)
  std::printf("6. Recreating bitmap textures with new palette\n");
  core::Renderer::Get().CreateAndRenderBitmap(
      0x200, 0x200, 0x200, gfx::Arena::Get().bg1().bitmap().vector(),
      gfx::Arena::Get().bg1().bitmap(), bg1_palette);
  core::Renderer::Get().CreateAndRenderBitmap(
      0x200, 0x200, 0x200, gfx::Arena::Get().bg2().bitmap().vector(),
      gfx::Arena::Get().bg2().bitmap(), bg1_palette);
  
  std::printf("7. BG1 has texture: %d\n", bg1_bmp.texture() != nullptr);
  std::printf("=== RenderRoomGraphics Complete ===\n\n");
  
  // Run comprehensive diagnostic
  DiagnoseRoomRendering(*this, room_id_);
}

void Room::RenderObjectsToBackground() {
  if (!rom_ || !rom_->is_loaded()) {
    std::printf("RenderObjectsToBackground: ROM not loaded\n");
    return;
  }
  
  std::printf("RenderObjectsToBackground: Room %d has %zu objects\n", room_id_, tile_objects_.size());
  
  // Get references to the background buffers
  auto& bg1 = gfx::Arena::Get().bg1();
  auto& bg2 = gfx::Arena::Get().bg2();
  
  // Render tile objects to their respective layers
  int rendered_count = 0;
  for (const auto& obj : tile_objects_) {
    // Ensure object has tiles loaded
    auto mutable_obj = const_cast<RoomObject&>(obj);
    mutable_obj.EnsureTilesLoaded();
    
    // Get tiles with error handling
    auto tiles_result = obj.GetTiles();
    if (!tiles_result.ok()) {
      std::printf("  Object at (%d,%d) failed to load tiles: %s\n", 
                  obj.x_, obj.y_, tiles_result.status().ToString().c_str());
      continue;
    }
    if (tiles_result->empty()) {
      std::printf("  Object at (%d,%d) has no tiles\n", obj.x_, obj.y_);
      continue;
    }
    
    const auto& tiles = *tiles_result;
    std::printf("  Object at (%d,%d) has %zu tiles\n", obj.x_, obj.y_, tiles.size());
    
    // Calculate object position in tile coordinates (each position is an 8x8 tile)
    int obj_x = obj.x_;  // X position in 8x8 tile units
    int obj_y = obj.y_;  // Y position in 8x8 tile units
    
    // Determine which layer this object belongs to
    bool is_bg2 = (obj.layer_ == RoomObject::LayerType::BG2);
    auto& target_buffer = is_bg2 ? bg2 : bg1;
    
    // Calculate the width of the object in Tile16 units
    // Most objects are arranged in a grid, typically 1-8 tiles wide
    // We calculate width based on square root for square objects,
    // or use a more flexible approach for rectangular objects
    int tiles_wide = 1;
    if (tiles.size() > 1) {
      // Try to determine optimal layout based on tile count
      // Common patterns: 1x1, 2x2, 4x1, 2x4, 4x4, 8x1, etc.
      int sq = static_cast<int>(std::sqrt(tiles.size()));
      if (sq * sq == static_cast<int>(tiles.size())) {
        tiles_wide = sq;  // Perfect square (4, 9, 16, etc.)
      } else if (tiles.size() <= 4) {
        tiles_wide = tiles.size();  // Small objects laid out horizontally
      } else {
        // For larger objects, try common widths (4 or 8)
        tiles_wide = (tiles.size() >= 8) ? 8 : 4;
      }
    }
    
    // Draw each Tile16 from the object
    // Each Tile16 is a 16x16 tile made of 4 TileInfo (8x8) tiles
    for (size_t i = 0; i < tiles.size(); i++) {
      const auto& tile16 = tiles[i];
      
      // Calculate tile16 position based on calculated width (in 16x16 units, so multiply by 2 for 8x8 units)
      int base_x = obj_x + ((i % tiles_wide) * 2);
      int base_y = obj_y + ((i / tiles_wide) * 2);
      
      // Each Tile16 contains 4 TileInfo objects arranged as:
      // [0][1]  (top-left, top-right)
      // [2][3]  (bottom-left, bottom-right)
      const auto& tile_infos = tile16.tiles_info;
      
      // Draw the 4 sub-tiles of this Tile16
      for (int sub_tile = 0; sub_tile < 4; sub_tile++) {
        int tile_x = base_x + (sub_tile % 2);
        int tile_y = base_y + (sub_tile / 2);
        
        // Bounds check
        if (tile_x < 0 || tile_x >= 64 || tile_y < 0 || tile_y >= 64) {
          continue;
        }
        
        // Convert TileInfo to word format: (vflip<<15) | (hflip<<14) | (over<<13) | (palette<<10) | tile_id
        uint16_t tile_word = gfx::TileInfoToWord(tile_infos[sub_tile]);
        
        // Set the tile in the buffer
        target_buffer.SetTileAt(tile_x, tile_y, tile_word);
        rendered_count++;
      }
    }
  }
  
  std::printf("RenderObjectsToBackground: Rendered %d tiles total\n", rendered_count);
  
  // Note: Layout objects rendering would go here if needed
  // For now, focusing on regular tile objects which is what ZScream primarily renders
}

void Room::LoadAnimatedGraphics() {
  if (!rom_ || !rom_->is_loaded()) {
    return;
  }
  
  auto gfx_buffer_data = rom()->mutable_graphics_buffer();
  if (!gfx_buffer_data || gfx_buffer_data->empty()) {
    return;
  }
  
  auto rom_data = rom()->vector();
  if (rom_data.empty()) {
    return;
  }
  
  // Validate animated_frame_ bounds
  if (animated_frame_ < 0 || animated_frame_ > 10) {
    return;
  }
  
  // Validate background_tileset_ bounds
  if (background_tileset_ < 0 || background_tileset_ > 255) {
    return;
  }
  
  int gfx_ptr = SnesToPc(rom()->version_constants().kGfxAnimatedPointer);
  if (gfx_ptr < 0 || gfx_ptr >= static_cast<int>(rom_data.size())) {
    return;
  }
  
  int data = 0;
  while (data < 512) {
    // Validate buffer access for first operation
    int first_offset = data + (92 * 2048) + (512 * animated_frame_);
    if (first_offset >= 0 && first_offset < static_cast<int>(gfx_buffer_data->size())) {
      uint8_t map_byte = (*gfx_buffer_data)[first_offset];
      
      // Validate current_gfx16_ access
      int gfx_offset = data + (7 * 2048);
      if (gfx_offset >= 0 && gfx_offset < static_cast<int>(sizeof(current_gfx16_))) {
        current_gfx16_[gfx_offset] = map_byte;
      }
    }
    
    // Validate buffer access for second operation
    int tileset_index = rom_data[gfx_ptr + background_tileset_];
    int second_offset = data + (tileset_index * 2048) + (512 * animated_frame_);
    if (second_offset >= 0 && second_offset < static_cast<int>(gfx_buffer_data->size())) {
      uint8_t map_byte = (*gfx_buffer_data)[second_offset];
      
      // Validate current_gfx16_ access
      int gfx_offset = data + (7 * 2048) - 512;
      if (gfx_offset >= 0 && gfx_offset < static_cast<int>(sizeof(current_gfx16_))) {
        current_gfx16_[gfx_offset] = map_byte;
      }
    }
    
    data++;
  }
}

void Room::LoadObjects() {
  auto rom_data = rom()->vector();
  
  // Enhanced object loading with comprehensive validation
  int object_pointer = (rom_data[room_object_pointer + 2] << 16) +
                       (rom_data[room_object_pointer + 1] << 8) +
                       (rom_data[room_object_pointer]);
  object_pointer = SnesToPc(object_pointer);
  
  // Enhanced bounds checking for object pointer
  if (object_pointer < 0 || object_pointer >= (int)rom_->size()) {
    // util::logf("Object pointer out of range for room %d: %#06x", room_id_, object_pointer);
    return;
  }
  
  int room_address = object_pointer + (room_id_ * 3);
  
  // Enhanced bounds checking for room address
  if (room_address < 0 || room_address + 2 >= (int)rom_->size()) {
    // util::logf("Room address out of range for room %d: %#06x", room_id_, room_address);
    return;
  }

  int tile_address = (rom_data[room_address + 2] << 16) +
                     (rom_data[room_address + 1] << 8) + rom_data[room_address];

  int objects_location = SnesToPc(tile_address);
  
  // Enhanced bounds checking for objects location
  if (objects_location < 0 || objects_location >= (int)rom_->size()) {
    // util::logf("Objects location out of range for room %d: %#06x", room_id_, objects_location);
    return;
  }

  // Parse floor graphics and layout with validation
  if (objects_location + 1 < (int)rom_->size()) {
    if (is_floor_) {
      floor1_graphics_ = static_cast<uint8_t>(rom_data[objects_location] & 0x0F);
      floor2_graphics_ = static_cast<uint8_t>((rom_data[objects_location] >> 4) & 0x0F);
    }

    layout = static_cast<uint8_t>((rom_data[objects_location + 1] >> 2) & 0x07);
  }

  LoadChests();

  // Parse objects with enhanced error handling
  ParseObjectsFromLocation(objects_location + 2);
}

void Room::ParseObjectsFromLocation(int objects_location) {
  auto rom_data = rom()->vector();
  
  z3_staircases_.clear();
  int nbr_of_staircase = 0;

  int pos = objects_location;
  uint8_t b1 = 0;
  uint8_t b2 = 0;
  uint8_t b3 = 0;
  int layer = 0;
  bool door = false;
  bool end_read = false;
  
  // Enhanced parsing loop with bounds checking
  while (!end_read && pos < (int)rom_->size()) {
    // Check if we have enough bytes to read
    if (pos + 1 >= (int)rom_->size()) {
      break;
    }
    
    b1 = rom_data[pos];
    b2 = rom_data[pos + 1];

    if (b1 == 0xFF && b2 == 0xFF) {
      pos += 2;  // Jump to next layer
      layer++;
      door = false;
      if (layer == 3) {
        break;
      }
      continue;
    }

    if (b1 == 0xF0 && b2 == 0xFF) {
      pos += 2;  // Jump to door section
      door = true;
      continue;
    }

    // Check if we have enough bytes for object data
    if (pos + 2 >= (int)rom_->size()) {
      break;
    }
    
    b3 = rom_data[pos + 2];
    if (door) {
      pos += 2;
    } else {
      pos += 3;
    }

    if (!door) {
      // Use the refactored encoding/decoding functions (Phase 1, Task 1.2)
      RoomObject r = RoomObject::DecodeObjectFromBytes(b1, b2, b3, static_cast<uint8_t>(layer));
      
      // Validate object ID before adding to the room
      if (r.id_ >= 0 && r.id_ <= 0x3FF) {
        r.set_rom(rom_);
        tile_objects_.push_back(r);

        // Handle special object types (staircases, chests, etc.)
        HandleSpecialObjects(r.id_, r.x(), r.y(), nbr_of_staircase);
      }
    } else {
      // Handle door objects (placeholder for future implementation)
      // tile_objects_.push_back(z3_object_door(static_cast<short>((b2 << 8) + b1),
      //                                        0, 0, 0, static_cast<uint8_t>(layer)));
    }
  }
}

// ============================================================================
// Object Saving Implementation (Phase 1, Task 1.3)
// ============================================================================

std::vector<uint8_t> Room::EncodeObjects() const {
  std::vector<uint8_t> bytes;
  
  // Organize objects by layer
  std::vector<RoomObject> layer0_objects;
  std::vector<RoomObject> layer1_objects;
  std::vector<RoomObject> layer2_objects;
  
  for (const auto& obj : tile_objects_) {
    switch (obj.GetLayerValue()) {
      case 0: layer0_objects.push_back(obj); break;
      case 1: layer1_objects.push_back(obj); break;
      case 2: layer2_objects.push_back(obj); break;
    }
  }
  
  // Encode Layer 1 (BG2)
  for (const auto& obj : layer0_objects) {
    auto encoded = obj.EncodeObjectToBytes();
    bytes.push_back(encoded.b1);
    bytes.push_back(encoded.b2);
    bytes.push_back(encoded.b3);
  }
  bytes.push_back(0xFF);
  bytes.push_back(0xFF);
  
  // Encode Layer 2 (BG1)
  for (const auto& obj : layer1_objects) {
    auto encoded = obj.EncodeObjectToBytes();
    bytes.push_back(encoded.b1);
    bytes.push_back(encoded.b2);
    bytes.push_back(encoded.b3);
  }
  bytes.push_back(0xFF);
  bytes.push_back(0xFF);
  
  // Encode Layer 3
  for (const auto& obj : layer2_objects) {
    auto encoded = obj.EncodeObjectToBytes();
    bytes.push_back(encoded.b1);
    bytes.push_back(encoded.b2);
    bytes.push_back(encoded.b3);
  }
  
  // Final terminator
  bytes.push_back(0xFF);
  bytes.push_back(0xFF);
  
  return bytes;
}

absl::Status Room::SaveObjects() {
  if (rom_ == nullptr) {
    return absl::InvalidArgumentError("ROM pointer is null");
  }
  
  auto rom_data = rom()->vector();
  
  // Get object pointer
  int object_pointer = (rom_data[room_object_pointer + 2] << 16) +
                       (rom_data[room_object_pointer + 1] << 8) +
                       (rom_data[room_object_pointer]);
  object_pointer = SnesToPc(object_pointer);
  
  if (object_pointer < 0 || object_pointer >= (int)rom_->size()) {
    return absl::OutOfRangeError("Object pointer out of range");
  }
  
  int room_address = object_pointer + (room_id_ * 3);
  
  if (room_address < 0 || room_address + 2 >= (int)rom_->size()) {
    return absl::OutOfRangeError("Room address out of range");
  }

  int tile_address = (rom_data[room_address + 2] << 16) +
                     (rom_data[room_address + 1] << 8) + rom_data[room_address];

  int objects_location = SnesToPc(tile_address);
  
  if (objects_location < 0 || objects_location >= (int)rom_->size()) {
    return absl::OutOfRangeError("Objects location out of range");
  }
  
  // Skip graphics/layout header (2 bytes)
  int write_pos = objects_location + 2;
  
  // Encode all objects
  auto encoded_bytes = EncodeObjects();
  
  // Write encoded bytes to ROM using WriteVector
  return rom_->WriteVector(write_pos, encoded_bytes);
}

// ============================================================================
// Object Manipulation Methods (Phase 3)
// ============================================================================

absl::Status Room::AddObject(const RoomObject& object) {
  // Validate object
  if (!ValidateObject(object)) {
    return absl::InvalidArgumentError("Invalid object parameters");
  }
  
  // Add to internal list
  tile_objects_.push_back(object);
  
  return absl::OkStatus();
}

absl::Status Room::RemoveObject(size_t index) {
  if (index >= tile_objects_.size()) {
    return absl::OutOfRangeError("Object index out of range");
  }
  
  tile_objects_.erase(tile_objects_.begin() + index);
  
  return absl::OkStatus();
}

absl::Status Room::UpdateObject(size_t index, const RoomObject& object) {
  if (index >= tile_objects_.size()) {
    return absl::OutOfRangeError("Object index out of range");
  }
  
  if (!ValidateObject(object)) {
    return absl::InvalidArgumentError("Invalid object parameters");
  }
  
  tile_objects_[index] = object;
  
  return absl::OkStatus();
}

absl::StatusOr<size_t> Room::FindObjectAt(int x, int y, int layer) const {
  for (size_t i = 0; i < tile_objects_.size(); i++) {
    const auto& obj = tile_objects_[i];
    if (obj.x() == x && obj.y() == y && obj.GetLayerValue() == layer) {
      return i;
    }
  }
  return absl::NotFoundError("No object found at position");
}

bool Room::ValidateObject(const RoomObject& object) const {
  // Validate position (0-63 for both X and Y)
  if (object.x() < 0 || object.x() > 63) return false;
  if (object.y() < 0 || object.y() > 63) return false;
  
  // Validate layer (0-2)
  if (object.GetLayerValue() < 0 || object.GetLayerValue() > 2) return false;
  
  // Validate object ID range
  if (object.id_ < 0 || object.id_ > 0xFFF) return false;
  
  // Validate size for Type 1 objects
  if (object.id_ < 0x100 && object.size() > 15) return false;
  
  return true;
}

void Room::HandleSpecialObjects(short oid, uint8_t posX, uint8_t posY, int& nbr_of_staircase) {
  // Handle staircase objects
  for (short stair : stairsObjects) {
    if (stair == oid) {
      if (nbr_of_staircase < 4) {
        tile_objects_.back().set_options(ObjectOption::Stairs |
                                         tile_objects_.back().options());
        z3_staircases_.push_back({
            posX, posY,
            absl::StrCat("To ", staircase_rooms_[nbr_of_staircase])
                .data()});
        nbr_of_staircase++;
      } else {
        tile_objects_.back().set_options(ObjectOption::Stairs |
                                         tile_objects_.back().options());
        z3_staircases_.push_back({posX, posY, "To ???"});
      }
      break;
    }
  }

  // Handle chest objects
  if (oid == 0xF99) {
    if (chests_in_room_.size() > 0) {
      tile_objects_.back().set_options(ObjectOption::Chest |
                                       tile_objects_.back().options());
      chests_in_room_.erase(chests_in_room_.begin());
    }
  } else if (oid == 0xFB1) {
    if (chests_in_room_.size() > 0) {
      tile_objects_.back().set_options(ObjectOption::Chest |
                                       tile_objects_.back().options());
      chests_in_room_.erase(chests_in_room_.begin());
    }
  }
}

void Room::LoadSprites() {
  auto rom_data = rom()->vector();
  int sprite_pointer = (0x04 << 16) +
                       (rom_data[rooms_sprite_pointer + 1] << 8) +
                       (rom_data[rooms_sprite_pointer]);
  int sprite_address_snes =
      (0x09 << 16) + (rom_data[sprite_pointer + (room_id_ * 2) + 1] << 8) +
      rom_data[sprite_pointer + (room_id_ * 2)];

  int sprite_address = SnesToPc(sprite_address_snes);
  if (rom_data[sprite_address] == 1) {
    // sortsprites is unused
  }
  sprite_address += 1;

  while (true) {
    uint8_t b1 = rom_data[sprite_address];
    uint8_t b2 = rom_data[sprite_address + 1];
    uint8_t b3 = rom_data[sprite_address + 2];

    if (b1 == 0xFF) {
      break;
    }

    sprites_.emplace_back(b3, (b2 & 0x1F), (b1 & 0x1F),
                          ((b2 & 0xE0) >> 5) + ((b1 & 0x60) >> 2),
                          (b1 & 0x80) >> 7);

    if (sprites_.size() > 1) {
      Sprite &spr = sprites_.back();
      Sprite &prevSprite = sprites_[sprites_.size() - 2];

      if (spr.id() == 0xE4 && spr.x() == 0x00 && spr.y() == 0x1E &&
          spr.layer() == 1 && spr.subtype() == 0x18) {
        prevSprite.set_key_drop(1);
        sprites_.pop_back();
      }

      if (spr.id() == 0xE4 && spr.x() == 0x00 && spr.y() == 0x1D &&
          spr.layer() == 1 && spr.subtype() == 0x18) {
        prevSprite.set_key_drop(2);
        sprites_.pop_back();
      }
    }

    sprite_address += 3;
  }
}

void Room::LoadChests() {
  auto rom_data = rom()->vector();
  uint32_t cpos = SnesToPc((rom_data[chests_data_pointer1 + 2] << 16) +
                           (rom_data[chests_data_pointer1 + 1] << 8) +
                           (rom_data[chests_data_pointer1]));
  size_t clength = (rom_data[chests_length_pointer + 1] << 8) +
                   (rom_data[chests_length_pointer]);

  for (size_t i = 0; i < clength; i++) {
    if ((((rom_data[cpos + (i * 3) + 1] << 8) + (rom_data[cpos + (i * 3)])) &
         0x7FFF) == room_id_) {
      // There's a chest in that room !
      bool big = false;
      if ((((rom_data[cpos + (i * 3) + 1] << 8) + (rom_data[cpos + (i * 3)])) &
           0x8000) == 0x8000) {
        big = true;
      }

      chests_in_room_.emplace_back(
          chest_data{rom_data[cpos + (i * 3) + 2], big});
    }
  }
}

void Room::LoadRoomLayout() {
  // Use the new RoomLayout system to load walls, floors, and structural elements
  auto status = layout_.LoadLayout(room_id_);
  if (!status.ok()) {
    // Log error but don't fail - some rooms might not have layout data
    // util::logf("Failed to load room layout for room %d: %s", 
              //  room_id_, status.message().data());
    return;
  }
  
  // Store the layout ID for compatibility with existing code
  layout = static_cast<uint8_t>(room_id_ & 0xFF);
  
  // util::logf("Loaded room layout for room %d with %zu objects", 
            //  room_id_, layout_.GetObjects().size());
}

void Room::LoadDoors() {
  auto rom_data = rom()->vector();
  
  // Load door graphics and positions
  // Door graphics are stored at door_gfx_* addresses
  // Door positions are stored at door_pos_* addresses
  
  // For now, create placeholder door objects
  // TODO: Implement full door loading from ROM data
}

void Room::LoadTorches() {
  auto rom_data = rom()->vector();
  
  // Load torch data from torch_data address
  // int torch_count = rom_data[torches_length_pointer + 1] << 8 | rom_data[torches_length_pointer];
  
  // For now, create placeholder torch objects
  // TODO: Implement full torch loading from ROM data
}

void Room::LoadBlocks() {
  auto rom_data = rom()->vector();
  
  // Load block data from blocks_* addresses
  // int block_count = rom_data[blocks_length + 1] << 8 | rom_data[blocks_length];
  
  // For now, create placeholder block objects
  // TODO: Implement full block loading from ROM data
}

void Room::LoadPits() {
  auto rom_data = rom()->vector();
  
  // Load pit data from pit_pointer
  // int pit_count = rom_data[pit_count + 1] << 8 | rom_data[pit_count];
  
  // For now, create placeholder pit objects
  // TODO: Implement full pit loading from ROM data
}

}  // namespace zelda3
}  // namespace yaze
