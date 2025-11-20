#include "room.h"

#include <yaze.h>

#include <cstdint>
#include <vector>

#include "absl/strings/str_cat.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "app/rom.h"
#include "app/snes.h"
#include "util/log.h"
#include "zelda3/dungeon/object_drawer.h"
#include "zelda3/dungeon/room_object.h"
#include "zelda3/sprite/sprite.h"

namespace yaze {
namespace zelda3 {

RoomSize CalculateRoomSize(Rom* rom, int room_id) {
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

  // Reading bytes for long address construction
  uint8_t low = rom->data()[room_size_address];
  uint8_t high = rom->data()[room_size_address + 1];
  uint8_t bank = rom->data()[room_size_address + 2];

  // Constructing the long address
  int long_address = (bank << 16) | (high << 8) | low;
  room_size.room_size_pointer = long_address;

  if (long_address == 0x0A8000) {
    // Blank room disregard in size calculation
    room_size.room_size = 0;
  } else {
    // use the long address to calculate the size of the room
    // we will use the room_id_ to calculate the next room's address
    // and subtract the two to get the size of the room

    int next_room_address = 0xF8000 + ((room_id + 1) * 3);

    // Reading bytes for long address construction
    uint8_t next_low = rom->data()[next_room_address];
    uint8_t next_high = rom->data()[next_room_address + 1];
    uint8_t next_bank = rom->data()[next_room_address + 2];

    // Constructing the long address
    int next_long_address = (next_bank << 16) | (next_high << 8) | next_low;

    // Calculate the size of the room
    int actual_room_size = next_long_address - long_address;
    room_size.room_size = actual_room_size;
  }

  return room_size;
}

Room LoadRoomFromRom(Rom* rom, int room_id) {
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

  // Load additional room features
  room.LoadDoors();
  room.LoadTorches();
  room.LoadBlocks();
  room.LoadPits();

  room.SetLoaded(true);
  return room;
}

void Room::LoadRoomGraphics(uint8_t entrance_blockset) {
  const auto& room_gfx = rom()->room_blockset_ids;
  const auto& sprite_gfx = rom()->spriteset_ids;

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
    printf("[CopyRoomGraphicsToBuffer] ROM not loaded\n");
    return;
  }

  auto gfx_buffer_data = rom()->mutable_graphics_buffer();
  if (!gfx_buffer_data || gfx_buffer_data->empty()) {
    printf("[CopyRoomGraphicsToBuffer] Graphics buffer is null or empty\n");
    return;
  }

  printf("[CopyRoomGraphicsToBuffer] Room %d: Copying graphics from blocks\n",
         room_id_);

  // Copy room graphics to buffer
  int sheet_pos = 0;
  int bytes_copied = 0;
  for (int i = 0; i < 16; i++) {
    // Validate block index
    if (blocks_[i] < 0 || blocks_[i] > 255) {
      sheet_pos += kGfxBufferRoomOffset;
      continue;
    }

    int data = 0;
    int block_offset = blocks_[i] * kGfxBufferRoomOffset;

    // Validate block_offset bounds
    if (block_offset < 0 ||
        block_offset >= static_cast<int>(gfx_buffer_data->size())) {
      sheet_pos += kGfxBufferRoomOffset;
      continue;
    }

    while (data < kGfxBufferRoomOffset) {
      int buffer_index = data + block_offset;
      if (buffer_index >= 0 &&
          buffer_index < static_cast<int>(gfx_buffer_data->size())) {
        uint8_t map_byte = (*gfx_buffer_data)[buffer_index];
        // NOTE: DO NOT apply sprite offset here!
        // current_gfx16_ holds pixel data (palette indices 0-7), not tile IDs.
        // The 0x88 offset is for tile IDs in tilemaps, not raw pixel data.
        // if (i < 4) {
        //   map_byte += kGfxBufferRoomSpriteLastLineOffset;
        // }

        // Validate current_gfx16_ access
        int gfx_index = data + sheet_pos;
        if (gfx_index >= 0 &&
            gfx_index < static_cast<int>(sizeof(current_gfx16_))) {
          current_gfx16_[gfx_index] = map_byte;
          if (map_byte != 0)
            bytes_copied++;
        }
      }
      data++;
    }

    sheet_pos += kGfxBufferRoomOffset;
  }

  printf(
      "[CopyRoomGraphicsToBuffer] Room %d: Copied %d non-zero bytes to "
      "current_gfx16_\n",
      room_id_, bytes_copied);
  LoadAnimatedGraphics();
}

void Room::RenderRoomGraphics() {
  // PERFORMANCE OPTIMIZATION: Check if room properties have changed
  bool properties_changed = false;

  // Check if graphics properties changed
  if (cached_blockset_ != blockset || cached_spriteset_ != spriteset ||
      cached_palette_ != palette || cached_layout_ != layout ||
      cached_floor1_graphics_ != floor1_graphics_ ||
      cached_floor2_graphics_ != floor2_graphics_) {
    cached_blockset_ = blockset;
    cached_spriteset_ = spriteset;
    cached_palette_ = palette;
    cached_layout_ = layout;
    cached_floor1_graphics_ = floor1_graphics_;
    cached_floor2_graphics_ = floor2_graphics_;
    graphics_dirty_ = true;
    properties_changed = true;
  }

  // Check if effect/tags changed
  if (cached_effect_ != static_cast<uint8_t>(effect_) ||
      cached_tag1_ != tag1_ || cached_tag2_ != tag2_) {
    cached_effect_ = static_cast<uint8_t>(effect_);
    cached_tag1_ = tag1_;
    cached_tag2_ = tag2_;
    objects_dirty_ = true;
    properties_changed = true;
  }

  // If nothing changed and textures exist, skip rendering
  if (!properties_changed && !graphics_dirty_ && !objects_dirty_ &&
      !layout_dirty_ && !textures_dirty_) {
    auto& bg1_bmp = bg1_buffer_.bitmap();
    auto& bg2_bmp = bg2_buffer_.bitmap();
    if (bg1_bmp.texture() && bg2_bmp.texture()) {
      LOG_DEBUG("[RenderRoomGraphics]",
                "Room %d: No changes detected, skipping render", room_id_);
      return;
    }
  }

  LOG_DEBUG("[RenderRoomGraphics]",
            "Room %d: Rendering graphics (dirty_flags: g=%d o=%d l=%d t=%d)",
            room_id_, graphics_dirty_, objects_dirty_, layout_dirty_,
            textures_dirty_);

  // STEP 0: Load graphics if needed
  if (graphics_dirty_) {
    CopyRoomGraphicsToBuffer();
    graphics_dirty_ = false;
  }

  // STEP 1: Load layout tiles if needed
  if (layout_dirty_) {
    LoadLayoutTilesToBuffer();
    layout_dirty_ = false;
  }

  // Debug: Log floor graphics values
  LOG_DEBUG("[RenderRoomGraphics]",
            "Room %d: floor1=%d, floor2=%d, blocks_size=%zu", room_id_,
            floor1_graphics_, floor2_graphics_, blocks_.size());

  // LoadGraphicsSheetsIntoArena() removed - using per-room graphics instead
  // Arena sheets are optional and not needed for room rendering

  // STEP 2: Draw floor tiles to bitmaps (base layer) - if graphics changed OR bitmaps not created yet
  bool need_floor_draw = graphics_dirty_;
  auto& bg1_bmp = bg1_buffer_.bitmap();
  auto& bg2_bmp = bg2_buffer_.bitmap();

  // Always draw floor if bitmaps don't exist yet (first time rendering)
  if (!bg1_bmp.is_active() || bg1_bmp.width() == 0 || !bg2_bmp.is_active() ||
      bg2_bmp.width() == 0) {
    need_floor_draw = true;
    LOG_DEBUG("[RenderRoomGraphics]",
              "Room %d: Bitmaps not created yet, forcing floor draw", room_id_);
  }

  if (need_floor_draw) {
    bg1_buffer_.DrawFloor(rom()->vector(), tile_address, tile_address_floor,
                          floor1_graphics_);
    bg2_buffer_.DrawFloor(rom()->vector(), tile_address, tile_address_floor,
                          floor2_graphics_);
  }

  // STEP 3: Draw background tiles (walls/structure) to buffers - if graphics changed OR bitmaps just created
  bool need_bg_draw = graphics_dirty_ || need_floor_draw;
  if (need_bg_draw) {
    bg1_buffer_.DrawBackground(std::span<uint8_t>(current_gfx16_));
    bg2_buffer_.DrawBackground(std::span<uint8_t>(current_gfx16_));
  }

  // Get and apply palette BEFORE rendering objects (so objects use correct colors)
  auto& dungeon_pal_group = rom()->mutable_palette_group()->dungeon_main;
  int num_palettes = dungeon_pal_group.size();

  // Use palette indirection table lookup (same as dungeon_canvas_viewer.cc line 854)
  int palette_id = palette;  // Default fallback
  if (palette < rom()->paletteset_ids.size() &&
      !rom()->paletteset_ids[palette].empty()) {
    auto dungeon_palette_ptr = rom()->paletteset_ids[palette][0];
    auto palette_word = rom()->ReadWord(0xDEC4B + dungeon_palette_ptr);
    if (palette_word.ok()) {
      palette_id =
          palette_word.value() / 180;  // Divide by 180 to get group index
      LOG_DEBUG("[RenderRoomGraphics]",
                "Palette lookup: byte=0x%02X → group_id=%d", palette,
                palette_id);
    }
  }

  // Clamp to valid range
  if (palette_id < 0 || palette_id >= num_palettes) {
    palette_id = palette_id % num_palettes;
  }

  auto bg1_palette = dungeon_pal_group[palette_id];

  if (bg1_palette.size() > 0) {
    // Apply FULL 90-color dungeon palette
    bg1_bmp.SetPalette(bg1_palette);
    bg2_bmp.SetPalette(bg1_palette);
  }

  // Render objects ON TOP of background tiles (AFTER palette is set)
  // ObjectDrawer will write indexed pixel data that uses the palette we just set
  RenderObjectsToBackground();

  // PERFORMANCE OPTIMIZATION: Queue texture commands but DON'T process immediately
  // This allows multiple rooms to batch their texture updates together
  // The dungeon_canvas_viewer.cc:552 will process all queued textures once per frame
  if (bg1_bmp.texture()) {
    // Texture exists - UPDATE it with new object data
    LOG_DEBUG("[RenderRoomGraphics]",
              "Queueing UPDATE for existing textures (deferred)");
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::UPDATE, &bg1_bmp);
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::UPDATE, &bg2_bmp);
  } else {
    // No texture yet - CREATE it
    LOG_DEBUG("[RenderRoomGraphics]",
              "Queueing CREATE for new textures (deferred)");
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::CREATE, &bg1_bmp);
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::CREATE, &bg2_bmp);
  }

  // Mark textures as clean after successful queuing
  textures_dirty_ = false;

  // REMOVED: Don't process texture queue here - let it be batched!
  // Processing happens once per frame in DrawDungeonCanvas()
  // This dramatically improves performance when multiple rooms are open
  // gfx::Arena::Get().ProcessTextureQueue(nullptr);  // OLD: Caused slowdown!
  LOG_DEBUG("[RenderRoomGraphics]",
            "Texture commands queued for batch processing");
}

void Room::LoadLayoutTilesToBuffer() {
  LOG_DEBUG("RenderRoomGraphics", "LoadLayoutTilesToBuffer START for room %d",
            room_id_);

  if (!rom_ || !rom_->is_loaded()) {
    LOG_DEBUG("RenderRoomGraphics", "ROM not loaded, aborting");
    return;
  }

  const auto& layout_objects = layout_.GetObjects();
  LOG_DEBUG("RenderRoomGraphics", "Layout has %zu objects",
            layout_objects.size());
  if (layout_objects.empty()) {
    return;
  }

  int tiles_written_bg1 = 0;
  int tiles_written_bg2 = 0;
  int tiles_skipped = 0;

  for (const auto& layout_obj : layout_objects) {
    uint8_t x = layout_obj.x();
    uint8_t y = layout_obj.y();

    auto tile_result = layout_obj.GetTile(0);
    if (!tile_result.ok()) {
      tiles_skipped++;
      continue;
    }
    const auto* tile_info = tile_result.value();
    uint16_t tile_word = gfx::TileInfoToWord(*tile_info);

    if (layout_obj.GetLayerValue() == 1) {
      bg2_buffer_.SetTileAt(x, y, tile_word);
      tiles_written_bg2++;
    } else {
      bg1_buffer_.SetTileAt(x, y, tile_word);
      tiles_written_bg1++;
    }
  }

  LOG_DEBUG("RenderRoomGraphics", "Layout tiles: BG1=%d BG2=%d skipped=%d",
            tiles_written_bg1, tiles_written_bg2, tiles_skipped);
}

void Room::RenderObjectsToBackground() {
  LOG_DEBUG("[RenderObjectsToBackground]",
            "Starting object rendering for room %d", room_id_);

  if (!rom_ || !rom_->is_loaded()) {
    LOG_DEBUG("[RenderObjectsToBackground]", "ROM not loaded, aborting");
    return;
  }

  // PERFORMANCE OPTIMIZATION: Only render objects if they have changed or if graphics changed
  // Also render if bitmaps were just created (need_floor_draw was true in RenderRoomGraphics)
  auto& bg1_bmp = bg1_buffer_.bitmap();
  auto& bg2_bmp = bg2_buffer_.bitmap();
  bool bitmaps_exist = bg1_bmp.is_active() && bg1_bmp.width() > 0 &&
                       bg2_bmp.is_active() && bg2_bmp.width() > 0;

  if (!objects_dirty_ && !graphics_dirty_ && bitmaps_exist) {
    LOG_DEBUG("[RenderObjectsToBackground]",
              "Room %d: Objects not dirty, skipping render", room_id_);
    return;
  }

  // Get palette group for object rendering (use SAME lookup as RenderRoomGraphics)
  auto& dungeon_pal_group = rom()->mutable_palette_group()->dungeon_main;
  int num_palettes = dungeon_pal_group.size();

  // Use palette indirection table lookup
  int palette_id = palette;
  if (palette < rom()->paletteset_ids.size() &&
      !rom()->paletteset_ids[palette].empty()) {
    auto dungeon_palette_ptr = rom()->paletteset_ids[palette][0];
    auto palette_word = rom()->ReadWord(0xDEC4B + dungeon_palette_ptr);
    if (palette_word.ok()) {
      palette_id = palette_word.value() / 180;
    }
  }

  if (palette_id < 0 || palette_id >= num_palettes) {
    palette_id = 0;
  }

  auto room_palette = dungeon_pal_group[palette_id];
  // Dungeon palettes are 16-color sub-palettes. Split the 90-color palette into 16-color groups.
  auto palette_group_result =
      gfx::CreatePaletteGroupFromLargePalette(room_palette, 16);
  if (!palette_group_result.ok()) {
    // Fallback to empty palette group
    gfx::PaletteGroup empty_group;
    ObjectDrawer drawer(rom_, current_gfx16_.data());
    drawer.DrawObjectList(tile_objects_, bg1_buffer_, bg2_buffer_, empty_group);
    return;
  }
  auto palette_group = palette_group_result.value();

  // Use ObjectDrawer for pattern-based object rendering
  // This provides proper wall/object drawing patterns
  // Pass the room-specific graphics buffer (current_gfx16_) so objects use correct tiles
  ObjectDrawer drawer(rom_, current_gfx16_.data());
  auto status = drawer.DrawObjectList(tile_objects_, bg1_buffer_, bg2_buffer_,
                                      palette_group);

  // Log only failures, not successes
  if (!status.ok()) {
    LOG_DEBUG("[RenderObjectsToBackground]", "ObjectDrawer failed: %s",
              std::string(status.message()).c_str());
  } else {
    // Mark objects as clean after successful render
    objects_dirty_ = false;
    LOG_DEBUG("[RenderObjectsToBackground]",
              "Room %d: Objects rendered successfully", room_id_);
  }
}

// LoadGraphicsSheetsIntoArena() removed - using per-room graphics instead
// Room rendering no longer depends on Arena graphics sheets

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
    if (first_offset >= 0 &&
        first_offset < static_cast<int>(gfx_buffer_data->size())) {
      uint8_t map_byte = (*gfx_buffer_data)[first_offset];

      // Validate current_gfx16_ access
      int gfx_offset = data + (7 * 2048);
      if (gfx_offset >= 0 &&
          gfx_offset < static_cast<int>(sizeof(current_gfx16_))) {
        current_gfx16_[gfx_offset] = map_byte;
      }
    }

    // Validate buffer access for second operation
    int tileset_index = rom_data[gfx_ptr + background_tileset_];
    int second_offset = data + (tileset_index * 2048) + (512 * animated_frame_);
    if (second_offset >= 0 &&
        second_offset < static_cast<int>(gfx_buffer_data->size())) {
      uint8_t map_byte = (*gfx_buffer_data)[second_offset];

      // Validate current_gfx16_ access
      int gfx_offset = data + (7 * 2048) - 512;
      if (gfx_offset >= 0 &&
          gfx_offset < static_cast<int>(sizeof(current_gfx16_))) {
        current_gfx16_[gfx_offset] = map_byte;
      }
    }

    data++;
  }
}

void Room::LoadObjects() {
  LOG_DEBUG("[LoadObjects]", "Starting LoadObjects for room %d", room_id_);
  auto rom_data = rom()->vector();

  // Enhanced object loading with comprehensive validation
  int object_pointer = (rom_data[room_object_pointer + 2] << 16) +
                       (rom_data[room_object_pointer + 1] << 8) +
                       (rom_data[room_object_pointer]);
  object_pointer = SnesToPc(object_pointer);

  // Enhanced bounds checking for object pointer
  if (object_pointer < 0 || object_pointer >= (int)rom_->size()) {
    return;
  }

  int room_address = object_pointer + (room_id_ * 3);

  // Enhanced bounds checking for room address
  if (room_address < 0 || room_address + 2 >= (int)rom_->size()) {
    return;
  }

  int tile_address = (rom_data[room_address + 2] << 16) +
                     (rom_data[room_address + 1] << 8) + rom_data[room_address];

  int objects_location = SnesToPc(tile_address);

  // Enhanced bounds checking for objects location
  if (objects_location < 0 || objects_location >= (int)rom_->size()) {
    return;
  }

  // Parse floor graphics and layout with validation
  if (objects_location + 1 < (int)rom_->size()) {
    if (is_floor_) {
      floor1_graphics_ =
          static_cast<uint8_t>(rom_data[objects_location] & 0x0F);
      floor2_graphics_ =
          static_cast<uint8_t>((rom_data[objects_location] >> 4) & 0x0F);
      LOG_DEBUG("[LoadObjects]",
                "Room %d: Set floor1_graphics_=%d, floor2_graphics_=%d",
                room_id_, floor1_graphics_, floor2_graphics_);
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
      RoomObject r = RoomObject::DecodeObjectFromBytes(
          b1, b2, b3, static_cast<uint8_t>(layer));

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
      case 0:
        layer0_objects.push_back(obj);
        break;
      case 1:
        layer1_objects.push_back(obj);
        break;
      case 2:
        layer2_objects.push_back(obj);
        break;
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
  if (object.x() < 0 || object.x() > 63)
    return false;
  if (object.y() < 0 || object.y() > 63)
    return false;

  // Validate layer (0-2)
  if (object.GetLayerValue() < 0 || object.GetLayerValue() > 2)
    return false;

  // Validate object ID range
  if (object.id_ < 0 || object.id_ > 0xFFF)
    return false;

  // Validate size for Type 1 objects
  if (object.id_ < 0x100 && object.size() > 15)
    return false;

  return true;
}

void Room::HandleSpecialObjects(short oid, uint8_t posX, uint8_t posY,
                                int& nbr_of_staircase) {
  // Handle staircase objects
  for (short stair : stairsObjects) {
    if (stair == oid) {
      if (nbr_of_staircase < 4) {
        tile_objects_.back().set_options(ObjectOption::Stairs |
                                         tile_objects_.back().options());
        z3_staircases_.push_back(
            {posX, posY,
             absl::StrCat("To ", staircase_rooms_[nbr_of_staircase]).data()});
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
      Sprite& spr = sprites_.back();
      Sprite& prevSprite = sprites_[sprites_.size() - 2];

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

void Room::LoadDoors() {
  auto rom_data = rom()->vector();

  // Doors are loaded as part of the object stream in LoadObjects()
  // When the parser encounters 0xF0 0xFF, it enters door mode
  // Door objects have format: b1 (position/direction), b2 (type)
  // Door encoding: b1 = (door_pos << 3) + door_dir
  //                b2 = door_type
  // This is already handled in ParseObjectsFromLocation()

  LOG_DEBUG("Room",
            "LoadDoors for room %d - doors are loaded via object stream",
            room_id_);
}

void Room::LoadTorches() {
  auto rom_data = rom()->vector();

  // Read torch data length
  int bytes_count = (rom_data[torches_length_pointer + 1] << 8) |
                    rom_data[torches_length_pointer];

  LOG_DEBUG("Room", "LoadTorches: room_id=%d, bytes_count=%d", room_id_,
            bytes_count);

  // Iterate through torch data to find torches for this room
  for (int i = 0; i < bytes_count; i += 2) {
    if (i + 1 >= bytes_count)
      break;

    uint8_t b1 = rom_data[torch_data + i];
    uint8_t b2 = rom_data[torch_data + i + 1];

    // Skip 0xFFFF markers
    if (b1 == 0xFF && b2 == 0xFF) {
      continue;
    }

    // Check if this entry is for our room
    uint16_t torch_room_id = (b2 << 8) | b1;
    if (torch_room_id == room_id_) {
      // Found torches for this room, read them
      i += 2;
      while (i < bytes_count) {
        if (i + 1 >= bytes_count)
          break;

        b1 = rom_data[torch_data + i];
        b2 = rom_data[torch_data + i + 1];

        // End of torch list for this room
        if (b1 == 0xFF && b2 == 0xFF) {
          break;
        }

        // Decode torch position and properties
        int address = ((b2 & 0x1F) << 8 | b1) >> 1;
        uint8_t px = address % 64;
        uint8_t py = address >> 6;
        uint8_t layer = (b2 & 0x20) >> 5;
        bool lit = (b2 & 0x80) == 0x80;

        // Create torch object (ID 0x150)
        RoomObject torch_obj(0x150, px, py, 0, layer);
        torch_obj.set_rom(rom_);
        torch_obj.set_options(ObjectOption::Torch);
        // Store lit state if needed (may require adding a field to RoomObject)

        tile_objects_.push_back(torch_obj);

        LOG_DEBUG("Room", "Loaded torch at (%d,%d) layer=%d lit=%d", px, py,
                  layer, lit);

        i += 2;
      }
      break;  // Found and processed our room's torches
    } else {
      // Skip to next room's torches
      i += 2;
      while (i < bytes_count) {
        if (i + 1 >= bytes_count)
          break;
        b1 = rom_data[torch_data + i];
        b2 = rom_data[torch_data + i + 1];
        if (b1 == 0xFF && b2 == 0xFF) {
          break;
        }
        i += 2;
      }
    }
  }
}

void Room::LoadBlocks() {
  auto rom_data = rom()->vector();

  // Read blocks length
  int blocks_count =
      (rom_data[blocks_length + 1] << 8) | rom_data[blocks_length];

  LOG_DEBUG("Room", "LoadBlocks: room_id=%d, blocks_count=%d", room_id_,
            blocks_count);

  // Load block data from multiple pointers
  std::vector<uint8_t> blocks_data(blocks_count);

  int pos1 = blocks_pointer1;
  int pos2 = blocks_pointer2;
  int pos3 = blocks_pointer3;
  int pos4 = blocks_pointer4;

  // Read block data from 4 different locations
  for (int i = 0; i < 0x80 && i < blocks_count; i++) {
    blocks_data[i] = rom_data[pos1 + i];

    if (i + 0x80 < blocks_count) {
      blocks_data[i + 0x80] = rom_data[pos2 + i];
    }
    if (i + 0x100 < blocks_count) {
      blocks_data[i + 0x100] = rom_data[pos3 + i];
    }
    if (i + 0x180 < blocks_count) {
      blocks_data[i + 0x180] = rom_data[pos4 + i];
    }
  }

  // Parse blocks for this room (4 bytes per block entry)
  for (int i = 0; i < blocks_count; i += 4) {
    if (i + 3 >= blocks_count)
      break;

    uint8_t b1 = blocks_data[i];
    uint8_t b2 = blocks_data[i + 1];
    uint8_t b3 = blocks_data[i + 2];
    uint8_t b4 = blocks_data[i + 3];

    // Check if this block belongs to our room
    uint16_t block_room_id = (b2 << 8) | b1;
    if (block_room_id == room_id_) {
      // End marker for this room's blocks
      if (b3 == 0xFF && b4 == 0xFF) {
        break;
      }

      // Decode block position
      int address = ((b4 & 0x1F) << 8 | b3) >> 1;
      uint8_t px = address % 64;
      uint8_t py = address >> 6;
      uint8_t layer = (b4 & 0x20) >> 5;

      // Create block object (ID 0x0E00)
      RoomObject block_obj(0x0E00, px, py, 0, layer);
      block_obj.set_rom(rom_);
      block_obj.set_options(ObjectOption::Block);

      tile_objects_.push_back(block_obj);

      LOG_DEBUG("Room", "Loaded block at (%d,%d) layer=%d", px, py, layer);
    }
  }
}

void Room::LoadPits() {
  auto rom_data = rom()->vector();

  // Read pit count
  int pit_entries = rom_data[pit_count] / 2;

  // Read pit pointer (long pointer)
  int pit_ptr = (rom_data[pit_pointer + 2] << 16) |
                (rom_data[pit_pointer + 1] << 8) | rom_data[pit_pointer];
  int pit_data_addr = SnesToPc(pit_ptr);

  LOG_DEBUG("Room", "LoadPits: room_id=%d, pit_entries=%d, pit_ptr=0x%06X",
            room_id_, pit_entries, pit_ptr);

  // Pit data is stored as: room_id (2 bytes), target info (2 bytes)
  // This data is already loaded in LoadRoomFromRom() into pits_ destination struct
  // The pit destination (where you go when you fall) is set via SetPitsTarget()

  // Pits are typically represented in the layout/collision data, not as objects
  // The pits_ member already contains the target room and layer
  LOG_DEBUG("Room", "Pit destination - target=%d, target_layer=%d",
            pits_.target, pits_.target_layer);
}

}  // namespace zelda3
}  // namespace yaze
