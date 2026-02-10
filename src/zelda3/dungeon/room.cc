#include "room.h"

#include <yaze.h>

#include <algorithm>
#include <cstdint>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "app/platform/sdl_compat.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "util/log.h"
#include "zelda3/dungeon/editor_dungeon_state.h"
#include "zelda3/dungeon/object_drawer.h"
#include "zelda3/dungeon/palette_debug.h"
#include "zelda3/dungeon/room_layer_manager.h"
#include "zelda3/dungeon/room_object.h"
#include "zelda3/dungeon/track_collision_generator.h"
#include "zelda3/sprite/sprite.h"

namespace yaze {
namespace zelda3 {

// Define room effect names in a single translation unit to avoid SIOF
const std::string RoomEffect[8] = {"Nothing",
                                   "Nothing",
                                   "Moving Floor",
                                   "Moving Water",
                                   "Trinexx Shell",
                                   "Red Flashes",
                                   "Light Torch to See Floor",
                                   "Ganon's Darkness"};

// Define room tag names in a single translation unit to avoid SIOF
const std::string RoomTag[65] = {"Nothing",
                                 "NW Kill Enemy to Open",
                                 "NE Kill Enemy to Open",
                                 "SW Kill Enemy to Open",
                                 "SE Kill Enemy to Open",
                                 "W Kill Enemy to Open",
                                 "E Kill Enemy to Open",
                                 "N Kill Enemy to Open",
                                 "S Kill Enemy to Open",
                                 "Clear Quadrant to Open",
                                 "Clear Full Tile to Open",
                                 "NW Push Block to Open",
                                 "NE Push Block to Open",
                                 "SW Push Block to Open",
                                 "SE Push Block to Open",
                                 "W Push Block to Open",
                                 "E Push Block to Open",
                                 "N Push Block to Open",
                                 "S Push Block to Open",
                                 "Push Block to Open",
                                 "Pull Lever to Open",
                                 "Collect Prize to Open",
                                 "Hold Switch Open Door",
                                 "Toggle Switch to Open Door",
                                 "Turn off Water",
                                 "Turn on Water",
                                 "Water Gate",
                                 "Water Twin",
                                 "Moving Wall Right",
                                 "Moving Wall Left",
                                 "Crash",
                                 "Crash",
                                 "Push Switch Exploding Wall",
                                 "Holes 0",
                                 "Open Chest (Holes 0)",
                                 "Holes 1",
                                 "Holes 2",
                                 "Defeat Boss for Dungeon Prize",
                                 "SE Kill Enemy to Push Block",
                                 "Trigger Switch Chest",
                                 "Pull Lever Exploding Wall",
                                 "NW Kill Enemy for Chest",
                                 "NE Kill Enemy for Chest",
                                 "SW Kill Enemy for Chest",
                                 "SE Kill Enemy for Chest",
                                 "W Kill Enemy for Chest",
                                 "E Kill Enemy for Chest",
                                 "N Kill Enemy for Chest",
                                 "S Kill Enemy for Chest",
                                 "Clear Quadrant for Chest",
                                 "Clear Full Tile for Chest",
                                 "Light Torches to Open",
                                 "Holes 3",
                                 "Holes 4",
                                 "Holes 5",
                                 "Holes 6",
                                 "Agahnim Room",
                                 "Holes 7",
                                 "Holes 8",
                                 "Open Chest for Holes 8",
                                 "Push Block for Chest",
                                 "Clear Room for Triforce Door",
                                 "Light Torches for Chest",
                                 "Kill Boss Again"};

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

  if (!rom || !rom->is_loaded() || rom->size() == 0) {
    return room_size;
  }

  auto room_size_address = 0xF8000 + (room_id * 3);

  // Bounds check
  if (room_size_address < 0 ||
      room_size_address + 2 >= static_cast<int>(rom->size())) {
    return room_size;
  }

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

    // Bounds check for next room address
    if (next_room_address < 0 ||
        next_room_address + 2 >= static_cast<int>(rom->size())) {
      return room_size;
    }

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

// Loads a room from the ROM.
// ASM: Bank 01, Underworld_LoadRoom ($01873A)
Room LoadRoomFromRom(Rom* rom, int room_id) {
  // Use the header loader to get the base room with properties
  // ASM: JSR Underworld_LoadHeader ($01873A)
  Room room = LoadRoomHeaderFromRom(rom, room_id);

  // Load additional room features
  //
  // USDASM ground truth: LoadAndBuildRoom ($01:873A) draws the variable-length
  // room object stream first (RoomDraw_DrawAllObjects), then draws pushable
  // blocks ($7EF940) and torches ($7EFB40). These "special" objects are not
  // part of the room object stream and must not be saved into it.
  room.LoadObjects();
  room.LoadPotItems();
  room.LoadTorches();
  room.LoadBlocks();
  room.LoadPits();

  room.SetLoaded(true);
  return room;
}

Room LoadRoomHeaderFromRom(Rom* rom, int room_id) {
  Room room(room_id, rom);

  if (!rom || !rom->is_loaded() || rom->size() == 0) {
    return room;
  }

  // Validate kRoomHeaderPointer access
  if (kRoomHeaderPointer < 0 ||
      kRoomHeaderPointer + 2 >= static_cast<int>(rom->size())) {
    return room;
  }

  // ASM: RoomHeader_RoomToPointer table lookup
  int header_pointer = (rom->data()[kRoomHeaderPointer + 2] << 16) +
                       (rom->data()[kRoomHeaderPointer + 1] << 8) +
                       (rom->data()[kRoomHeaderPointer]);
  header_pointer = SnesToPc(header_pointer);

  // Validate kRoomHeaderPointerBank access
  if (kRoomHeaderPointerBank < 0 ||
      kRoomHeaderPointerBank >= static_cast<int>(rom->size())) {
    return room;
  }

  // Validate header_pointer table access
  int table_offset = (header_pointer) + (room_id * 2);
  if (table_offset < 0 || table_offset + 1 >= static_cast<int>(rom->size())) {
    return room;
  }

  int address = (rom->data()[kRoomHeaderPointerBank] << 16) +
                (rom->data()[table_offset + 1] << 8) +
                rom->data()[table_offset];

  auto header_location = SnesToPc(address);

  // Validate header_location access (we read up to +13 bytes)
  if (header_location < 0 ||
      header_location + 13 >= static_cast<int>(rom->size())) {
    return room;
  }

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

  // Validate kRoomHeaderPointer access (again, just in case)
  if (kRoomHeaderPointer < 0 ||
      kRoomHeaderPointer + 2 >= static_cast<int>(rom->size())) {
    return room;
  }

  int header_pointer_2 = (rom->data()[kRoomHeaderPointer + 2] << 16) +
                         (rom->data()[kRoomHeaderPointer + 1] << 8) +
                         (rom->data()[kRoomHeaderPointer]);
  header_pointer_2 = SnesToPc(header_pointer_2);

  // Validate kRoomHeaderPointerBank access
  if (kRoomHeaderPointerBank < 0 ||
      kRoomHeaderPointerBank >= static_cast<int>(rom->size())) {
    return room;
  }

  // Validate header_pointer_2 table access
  int table_offset_2 = (header_pointer_2) + (room_id * 2);
  if (table_offset_2 < 0 ||
      table_offset_2 + 1 >= static_cast<int>(rom->size())) {
    return room;
  }

  int address_2 = (rom->data()[kRoomHeaderPointerBank] << 16) +
                  (rom->data()[table_offset_2 + 1] << 8) +
                  rom->data()[table_offset_2];

  int msg_addr = messages_id_dungeon + (room_id * 2);
  if (msg_addr >= 0 && msg_addr + 1 < static_cast<int>(rom->size())) {
    uint16_t msg_val = (rom->data()[msg_addr + 1] << 8) | rom->data()[msg_addr];
    room.SetMessageId(msg_val);
  }

  auto hpos = SnesToPc(address_2);

  // Validate hpos access (we read sequentially)
  // We read about 14 bytes (hpos++ calls)
  if (hpos < 0 || hpos + 14 >= static_cast<int>(rom->size())) {
    return room;
  }

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

  // Note: We do NOT set is_loaded_ to true here, as this is just the header
  return room;
}

Room::Room(int room_id, Rom* rom, GameData* game_data)
    : room_id_(room_id),
      rom_(rom),
      game_data_(game_data),
      dungeon_state_(std::make_unique<EditorDungeonState>(rom, game_data)) {}

Room::Room() = default;
Room::~Room() = default;
Room::Room(Room&&) = default;
Room& Room::operator=(Room&&) = default;

void Room::LoadRoomGraphics(uint8_t entrance_blockset) {
  if (!game_data_) {
    LOG_DEBUG("Room", "GameData not set for room %d", room_id_);
    return;
  }

  const auto& room_gfx = game_data_->room_blockset_ids;
  const auto& sprite_gfx = game_data_->spriteset_ids;

  LOG_DEBUG("Room", "Room %d: blockset=%d, spriteset=%d, palette=%d", room_id_,
            blockset, spriteset, palette);

  for (int i = 0; i < 8; i++) {
    blocks_[i] = game_data_->main_blockset_ids[blockset][i];
    // Block 6 can be overridden by entrance-specific room graphics (index 3)
    // Note: The "3-6" comment was misleading - only block 6 uses room_gfx
    if (i == 6) {
      if (entrance_blockset != 0xFF && room_gfx[entrance_blockset][3] != 0) {
        blocks_[i] = room_gfx[entrance_blockset][3];
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

  LOG_DEBUG("Room", "Sheet IDs BG[0-7]: %d %d %d %d %d %d %d %d", blocks_[0],
            blocks_[1], blocks_[2], blocks_[3], blocks_[4], blocks_[5],
            blocks_[6], blocks_[7]);
}

constexpr int kGfxBufferOffset = 92 * 2048;
constexpr int kGfxBufferStride = 1024;
constexpr int kGfxBufferAnimatedFrameOffset = 7 * 4096;
constexpr int kGfxBufferAnimatedFrameStride = 1024;
constexpr int kGfxBufferRoomOffset = 4096;
constexpr int kGfxBufferRoomSpriteOffset = 1024;
constexpr int kGfxBufferRoomSpriteStride = 4096;
constexpr int kGfxBufferRoomSpriteLastLineOffset = 0x110;

void Room::CopyRoomGraphicsToBuffer() {
  if (!rom_ || !rom_->is_loaded()) {
    LOG_DEBUG("Room", "CopyRoomGraphicsToBuffer: ROM not loaded");
    return;
  }

  if (!game_data_) {
    LOG_DEBUG("Room", "CopyRoomGraphicsToBuffer: GameData not set");
    return;
  }
  auto* gfx_buffer_data = &game_data_->graphics_buffer;
  if (gfx_buffer_data->empty()) {
    LOG_DEBUG("Room", "CopyRoomGraphicsToBuffer: Graphics buffer is empty");
    return;
  }

  LOG_DEBUG("Room", "Room %d: Copying 8BPP graphics (buffer size: %zu)",
            room_id_, gfx_buffer_data->size());

  // Clear destination buffer
  std::fill(current_gfx16_.begin(), current_gfx16_.end(), 0);

  // USDASM grounding (bank_00.asm LoadBackgroundGraphics):
  // The engine expands 3BPP graphics to 4BPP in two modes:
  // - Left palette: plane3 = 0 (pixel values 0-7).
  // - Right palette: plane3 = OR(planes0..2), so non-zero pixels get bit3=1
  //   (pixel values 1-7 become 9-15; 0 remains 0/transparent).
  //
  // For background graphics sets, the game selects Left/Right based on the
  // "graphics group" ($0AA1, our Room::blockset) and the slot index ($0F).
  // For UW groups (< $20), slots 4-7 use Right; for OW groups (>= $20), the
  // Right slots are {2,3,4,7}. We mirror this by shifting non-zero pixels by
  // +8 when copying those background blocks into current_gfx16_.
  auto is_right_palette_background_slot = [&](int slot) -> bool {
    if (slot < 0 || slot >= 8) {
      return false;
    }
    if (blockset < 0x20) {
      return slot >= 4;
    }
    return (slot == 2 || slot == 3 || slot == 4 || slot == 7);
  };

  // Process each of the 16 graphics blocks
  for (int block = 0; block < 16; block++) {
    int sheet_id = blocks_[block];

    // Validate block index
    if (sheet_id >= 223) {  // kNumGfxSheets
      LOG_WARN("Room", "Invalid sheet index %d for block %d", sheet_id, block);
      continue;
    }

    // Source offset in ROM graphics buffer (now 8BPP format)
    // Each 8BPP sheet is 4096 bytes (128x32 pixels)
    int src_sheet_offset = sheet_id * 4096;

    // Validate source bounds
    if (src_sheet_offset + 4096 > gfx_buffer_data->size()) {
      LOG_ERROR("Room", "Graphics offset out of bounds: %d (size: %zu)",
                src_sheet_offset, gfx_buffer_data->size());
      continue;
    }

    // Copy 4096 bytes for the 8BPP sheet
    int dest_index_base = block * 4096;
    if (dest_index_base + 4096 <= current_gfx16_.size()) {
      const uint8_t* src = gfx_buffer_data->data() + src_sheet_offset;
      uint8_t* dst = current_gfx16_.data() + dest_index_base;

      // Only background blocks (0-7) participate in Left/Right palette
      // expansion. Sprite sheets are handled separately by the game.
      const bool right_pal = is_right_palette_background_slot(block);
      if (!right_pal) {
        memcpy(dst, src, 4096);
      } else {
        // Right palette expansion: set bit3 for non-zero pixels (1-7 -> 9-15).
        for (int i = 0; i < 4096; ++i) {
          uint8_t p = src[i];
          if (p != 0 && p < 8) {
            p |= 0x08;
          }
          dst[i] = p;
        }
      }
    }
  }

  LOG_DEBUG("Room", "Room %d: Graphics blocks copied successfully", room_id_);
  LoadAnimatedGraphics();
}

gfx::Bitmap& Room::GetCompositeBitmap(RoomLayerManager& layer_mgr) {
  if (composite_dirty_) {
    layer_mgr.CompositeToOutput(*this, composite_bitmap_);
    composite_dirty_ = false;
  }
  return composite_bitmap_;
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

  // Capture dirty state BEFORE clearing flags (needed for floor/bg draw logic)
  bool was_graphics_dirty = graphics_dirty_;
  bool was_layout_dirty = layout_dirty_;

  // STEP 0: Load graphics if needed
  if (graphics_dirty_) {
    // Ensure blocks_[] array is properly initialized before copying graphics
    // LoadRoomGraphics sets up which sheets go into which blocks
    LoadRoomGraphics(blockset);
    CopyRoomGraphicsToBuffer();
    graphics_dirty_ = false;
  }

  // Debug: Log floor graphics values
  LOG_DEBUG("[RenderRoomGraphics]",
            "Room %d: floor1=%d, floor2=%d, blocks_size=%zu", room_id_,
            floor1_graphics_, floor2_graphics_, blocks_.size());

  // STEP 1: Draw floor tiles to bitmaps (base layer) - if graphics changed OR
  // bitmaps not created yet
  bool need_floor_draw = was_graphics_dirty;
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

  // STEP 2: Draw background tiles (floor pattern) to bitmap
  // This converts the floor tile buffer to pixels
  bool need_bg_draw = was_graphics_dirty || need_floor_draw;
  if (need_bg_draw) {
    bg1_buffer_.DrawBackground(std::span<uint8_t>(current_gfx16_));
    bg2_buffer_.DrawBackground(std::span<uint8_t>(current_gfx16_));
  }

  // STEP 3: Draw layout objects ON TOP of floor
  // Layout objects (walls, corners) are drawn after floor so they appear over it
  // TODO(zelda3-hacking-expert): Mirror the SNES four-pass pipeline from
  // assets/asm/usdasm/bank_01.asm (documented in
  // docs/internal/agents/dungeon-object-rendering-spec.md): layout list,
  // main list, BG2 overlay list, BG1 overlay list, with BothBG routines
  // writing simultaneously. Today we only emit one layout pass + one object
  // list, so BG overlays and dual-layer draws can end up wrong (layout objects
  // rendering above later passes or BG merge treated as exclusive).
  if (was_layout_dirty || need_floor_draw) {
    LoadLayoutTilesToBuffer();
    layout_dirty_ = false;
  }

  // Get and apply palette BEFORE rendering objects (so objects use correct colors)
  if (!game_data_)
    return;
  auto& dungeon_pal_group = game_data_->palette_groups.dungeon_main;
  int num_palettes = dungeon_pal_group.size();
  if (num_palettes == 0)
    return;

  // Look up dungeon palette ID using the two-level paletteset_ids table.
  // paletteset_ids[palette][0] contains a BYTE OFFSET into the palette pointer
  // table at kDungeonPalettePointerTable. The word at that offset, divided by
  // 180 (bytes per palette), gives the actual palette index (0-19).
  int palette_id = palette;
  if (palette < game_data_->paletteset_ids.size() &&
      !game_data_->paletteset_ids[palette].empty()) {
    auto dungeon_palette_ptr = game_data_->paletteset_ids[palette][0];
    auto palette_word =
        rom()->ReadWord(kDungeonPalettePointerTable + dungeon_palette_ptr);
    if (palette_word.ok()) {
      palette_id = palette_word.value() / 180;
    }
  }
  if (palette_id < 0 || palette_id >= num_palettes) {
    palette_id = 0;
  }

  auto bg1_palette = dungeon_pal_group[palette_id];

  // DEBUG: Log palette loading
  PaletteDebugger::Get().LogPaletteLoad("Room::RenderRoomGraphics", palette_id,
                                        bg1_palette);

  LOG_DEBUG("Room", "RenderRoomGraphics: Palette ID=%d, Size=%zu", palette_id,
            bg1_palette.size());
  if (!bg1_palette.empty()) {
    LOG_DEBUG("Room", "RenderRoomGraphics: First color: R=%d G=%d B=%d",
              bg1_palette[0].rom_color().red, bg1_palette[0].rom_color().green,
              bg1_palette[0].rom_color().blue);
  }

  // Store current palette and bitmap for pixel inspector debugging
  PaletteDebugger::Get().SetCurrentPalette(bg1_palette);
  PaletteDebugger::Get().SetCurrentBitmap(&bg1_bmp);

  if (bg1_palette.size() > 0) {
    // Apply dungeon palette using 16-color bank chunking (matches SNES CGRAM)
    //
    // SNES CGRAM layout for dungeons:
    // - CGRAM has 16-color banks, each bank's index 0 is transparent
    // - Dungeon tiles use palette bits 2-7, mapping to CGRAM rows 2-7
    // - ROM stores 15 colors per bank (excluding transparent index 0)
    // - 6 banks × 15 colors = 90 colors in ROM
    //
    // SDL palette mapping (16-color chunks):
    // - Bank N (N=0-5): SDL indices [N*16 .. N*16+15]
    // - Index N*16 = transparent for that bank
    // - ROM colors [N*15 .. N*15+14] → SDL indices [N*16+1 .. N*16+15]
    //
    // Drawing formula: final_color = pixel + (bank * 16)
    // Where pixel 0 = transparent (not written), pixel 1-15 = colors 1-15 in bank
    auto set_dungeon_palette = [](gfx::Bitmap& bmp,
                                  const gfx::SnesPalette& pal) {
      std::vector<SDL_Color> colors(
          256, {0, 0, 0, 0});  // Initialize all transparent

      // Map ROM palette to 16-color banks
      // ROM: 90 colors (6 banks × 15 colors each)
      // SDL: 96 indices (6 banks × 16 indices each)
      constexpr int kColorsPerRomBank = 15;
      constexpr int kIndicesPerSdlBank = 16;
      constexpr int kNumBanks = 6;

      for (int bank = 0; bank < kNumBanks; bank++) {
        // Index 0 of each bank is transparent (already initialized to {0,0,0,0})
        // ROM colors map to SDL indices 1-15 within each bank
        for (int color = 0; color < kColorsPerRomBank; color++) {
          size_t rom_index = bank * kColorsPerRomBank + color;
          if (rom_index >= pal.size())
            break;

          int sdl_index =
              bank * kIndicesPerSdlBank + color + 1;  // +1 to skip transparent
          ImVec4 rgb = pal[rom_index].rgb();
          colors[sdl_index] = {
              static_cast<Uint8>(rgb.x), static_cast<Uint8>(rgb.y),
              static_cast<Uint8>(rgb.z),
              255  // Opaque
          };
        }
      }

      // Index 255 is also transparent (fill color for undrawn areas)
      colors[255] = {0, 0, 0, 0};

      bmp.SetPalette(colors);
      if (bmp.surface()) {
        // Set color key to 255 for proper alpha blending (undrawn areas)
        SDL_SetColorKey(bmp.surface(), SDL_TRUE, 255);
        SDL_SetSurfaceBlendMode(bmp.surface(), SDL_BLENDMODE_BLEND);
      }
    };

    set_dungeon_palette(bg1_bmp, bg1_palette);
    set_dungeon_palette(bg2_bmp, bg1_palette);
    set_dungeon_palette(object_bg1_buffer_.bitmap(), bg1_palette);
    set_dungeon_palette(object_bg2_buffer_.bitmap(), bg1_palette);

    // DEBUG: Verify palette was applied to SDL surface
    auto* surface = bg1_bmp.surface();
    if (surface) {
      SDL_Palette* palette = platform::GetSurfacePalette(surface);
      if (palette) {
        PaletteDebugger::Get().LogPaletteApplication(
            "Room::RenderRoomGraphics (BG1)", palette_id, true);

        // Log surface state for detailed debugging
        PaletteDebugger::Get().LogSurfaceState(
            "Room::RenderRoomGraphics (after SetPalette)", surface);
      } else {
        PaletteDebugger::Get().LogPaletteApplication(
            "Room::RenderRoomGraphics", palette_id, false,
            "SDL surface has no palette!");
      }
    }

    // Apply Layer Merge effects (Transparency/Blending) to BG2
    // NOTE: These SDL blend settings are for direct SDL rendering paths.
    // RoomLayerManager::CompositeToOutput uses manual pixel compositing and
    // handles blend modes separately via its layer_blend_mode_ array.
    // TODO(scawful): Consolidate blend handling - either implement proper
    // blending in CompositeLayer or ensure SDL path uses RoomLayerManager.
    if (layer_merging_.Layer2Translucent) {
      // Set alpha mod for translucency (50%)
      if (bg2_bmp.surface()) {
        SDL_SetSurfaceAlphaMod(bg2_bmp.surface(), 128);
      }
      if (object_bg2_buffer_.bitmap().surface()) {
        SDL_SetSurfaceAlphaMod(object_bg2_buffer_.bitmap().surface(), 128);
      }

      // Check for Addition mode (ID 0x05)
      if (layer_merging_.ID == 0x05) {
        if (bg2_bmp.surface()) {
          SDL_SetSurfaceBlendMode(bg2_bmp.surface(), SDL_BLENDMODE_ADD);
        }
        if (object_bg2_buffer_.bitmap().surface()) {
          SDL_SetSurfaceBlendMode(object_bg2_buffer_.bitmap().surface(),
                                  SDL_BLENDMODE_ADD);
        }
      }
    }
  }

  // Render objects ON TOP of background tiles (AFTER palette is set)
  // ObjectDrawer will write indexed pixel data that uses the palette we just
  // set
  RenderObjectsToBackground();

  // PERFORMANCE OPTIMIZATION: Queue texture commands but DON'T process
  // immediately. This allows multiple rooms to batch their texture updates
  // together. Processing happens in DrawDungeonCanvas() once per frame.
  //
  // IMPORTANT: Check each buffer INDIVIDUALLY for existing texture.
  // Layout and object buffers may have different states (e.g., layout rendered
  // but objects added later need CREATE, not UPDATE).
  auto queue_texture = [](gfx::Bitmap* bitmap, const char* name) {
    if (bitmap->texture()) {
      LOG_DEBUG("[RenderRoomGraphics]", "Queueing UPDATE for %s", name);
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::UPDATE, bitmap);
    } else {
      LOG_DEBUG("[RenderRoomGraphics]", "Queueing CREATE for %s", name);
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::CREATE, bitmap);
    }
  };

  queue_texture(&bg1_bmp, "bg1_buffer");
  queue_texture(&bg2_bmp, "bg2_buffer");
  queue_texture(&object_bg1_buffer_.bitmap(), "object_bg1_buffer");
  queue_texture(&object_bg2_buffer_.bitmap(), "object_bg2_buffer");

  // Mark textures as clean after successful queuing
  textures_dirty_ = false;

  // IMPORTANT: Mark composite as dirty after any render work
  // This ensures GetCompositeBitmap() regenerates the merged output
  composite_dirty_ = true;

  // REMOVED: Don't process texture queue here - let it be batched!
  // Processing happens once per frame in DrawDungeonCanvas()
  // This dramatically improves performance when multiple rooms are open
  // gfx::Arena::Get().ProcessTextureQueue(nullptr);  // OLD: Caused slowdown!
  LOG_DEBUG("[RenderRoomGraphics]",
            "Texture commands queued for batch processing");
}

void Room::LoadLayoutTilesToBuffer() {
  LOG_DEBUG("Room", "LoadLayoutTilesToBuffer for room %d, layout=%d", room_id_,
            layout);

  if (!rom_ || !rom_->is_loaded()) {
    LOG_DEBUG("Room", "ROM not loaded, aborting");
    return;
  }

  // Load layout tiles from ROM if not already loaded
  layout_.SetRom(rom_);
  auto layout_status = layout_.LoadLayout(layout);
  if (!layout_status.ok()) {
    LOG_DEBUG("Room", "Failed to load layout %d: %s", layout,
              layout_status.message().data());
    return;
  }

  const auto& layout_objects = layout_.GetObjects();
  LOG_DEBUG("Room", "Layout %d has %zu objects", layout, layout_objects.size());
  if (layout_objects.empty()) {
    return;
  }

  // Use ObjectDrawer to render layout objects properly
  // Layout objects are the same format as room objects and need draw routines
  // to render correctly (walls, corners, etc.)
  if (!game_data_) {
    LOG_DEBUG("RenderRoomGraphics", "GameData not set, cannot render layout");
    return;
  }

  // Get palette for layout rendering
  // Get palette for layout rendering using two-level lookup
  auto& dungeon_pal_group = game_data_->palette_groups.dungeon_main;
  int num_palettes = dungeon_pal_group.size();
  if (num_palettes == 0)
    return;
  int palette_id = palette;
  if (palette < game_data_->paletteset_ids.size() &&
      !game_data_->paletteset_ids[palette].empty()) {
    auto dungeon_palette_ptr = game_data_->paletteset_ids[palette][0];
    auto palette_word =
        rom()->ReadWord(kDungeonPalettePointerTable + dungeon_palette_ptr);
    if (palette_word.ok()) {
      palette_id = palette_word.value() / 180;
    }
  }
  if (palette_id < 0 || palette_id >= num_palettes) {
    palette_id = 0;
  }

  auto room_palette = dungeon_pal_group[palette_id];
  gfx::PaletteGroup palette_group;
  palette_group.AddPalette(room_palette);
  // TODO(zelda3-hacking-expert): Align palette chunking with 16-color banks
  // per docs/internal/agents/dungeon-palette-fix-plan.md. SDL palette should
  // map each subpalette to indices [n*16..n*16+15] with index 0 transparent,
  // using the same palette for bg1/bg2/object buffers. Add assertions/logging
  // for palette_id/pointer mismatch against usdasm ($0DEC4B pointers).

  // Draw layout objects using proper draw routines via RoomLayout
  auto status = layout_.Draw(room_id_, current_gfx16_.data(), bg1_buffer_,
                             bg2_buffer_, palette_group, dungeon_state_.get());

  if (!status.ok()) {
    LOG_DEBUG(
        "RenderRoomGraphics", "Layout Draw failed: %s",
        std::string(status.message().data(), status.message().size()).c_str());
  } else {
    LOG_DEBUG("RenderRoomGraphics", "Layout rendered with %zu objects",
              layout_objects.size());
  }
}

void Room::RenderObjectsToBackground() {
  LOG_DEBUG("[RenderObjectsToBackground]",
            "Starting object rendering for room %d", room_id_);

  if (!rom_ || !rom_->is_loaded()) {
    LOG_DEBUG("[RenderObjectsToBackground]", "ROM not loaded, aborting");
    return;
  }

  // PERFORMANCE OPTIMIZATION: Only render objects if they have changed or if
  // graphics changed Also render if bitmaps were just created (need_floor_draw
  // was true in RenderRoomGraphics)
  auto& bg1_bmp = bg1_buffer_.bitmap();
  auto& bg2_bmp = bg2_buffer_.bitmap();
  bool bitmaps_exist = bg1_bmp.is_active() && bg1_bmp.width() > 0 &&
                       bg2_bmp.is_active() && bg2_bmp.width() > 0;

  if (!objects_dirty_ && !graphics_dirty_ && bitmaps_exist) {
    LOG_DEBUG("[RenderObjectsToBackground]",
              "Room %d: Objects not dirty, skipping render", room_id_);
    return;
  }

  // Handle rendering based on mode (currently using emulator-based rendering)
  // Emulator or Hybrid mode (use ObjectDrawer)
  LOG_DEBUG("[RenderObjectsToBackground]",
            "Room %d: Emulator rendering objects", room_id_);
  // Get palette group for object rendering (use SAME lookup as
  // RenderRoomGraphics)
  if (!game_data_)
    return;
  auto& dungeon_pal_group = game_data_->palette_groups.dungeon_main;
  int num_palettes = dungeon_pal_group.size();

  // Look up dungeon palette ID using the two-level paletteset_ids table.
  // (same lookup as RenderRoomGraphics and LoadLayoutTilesToBuffer)
  int palette_id = palette;
  if (palette < game_data_->paletteset_ids.size() &&
      !game_data_->paletteset_ids[palette].empty()) {
    auto dungeon_palette_ptr = game_data_->paletteset_ids[palette][0];
    auto palette_word =
        rom()->ReadWord(kDungeonPalettePointerTable + dungeon_palette_ptr);
    if (palette_word.ok()) {
      palette_id = palette_word.value() / 180;
    }
  }
  if (palette_id < 0 || palette_id >= num_palettes) {
    palette_id = 0;
  }

  auto room_palette = dungeon_pal_group[palette_id];
  // Dungeon palettes are 90-color palettes for 3BPP graphics (8-color strides)
  // Pass the full palette to ObjectDrawer so it can handle all palette indices
  gfx::PaletteGroup palette_group;
  palette_group.AddPalette(room_palette);

  // Use ObjectDrawer for pattern-based object rendering
  // This provides proper wall/object drawing patterns
  // Pass the room-specific graphics buffer (current_gfx16_) so objects use
  // correct tiles
  ObjectDrawer drawer(rom_, room_id_, current_gfx16_.data());
  // TODO(zelda3-hacking-expert): When we split the object stream into the
  // four ASM layers, ensure DrawObjectList is invoked per stream with proper
  // target buffers so BothBG routines (ceiling corners, merged stairs, etc.)
  // land on both BG1/BG2 as in bank_01.asm. See
  // docs/internal/agents/dungeon-object-rendering-spec.md for the expected
  // order and dual-layer handling.

  // Clear object buffers before rendering
  // IMPORTANT: Fill with 255 (transparent color key) so objects overlay correctly
  // on the floor. We use index 255 as transparent since palette has 90 colors (0-89).
  object_bg1_buffer_.EnsureBitmapInitialized();
  object_bg2_buffer_.EnsureBitmapInitialized();
  object_bg1_buffer_.bitmap().Fill(255);
  object_bg2_buffer_.bitmap().Fill(255);

  // IMPORTANT: Clear priority buffers when clearing object buffers
  // Otherwise, old priority values persist and cause incorrect Z-ordering
  object_bg1_buffer_.ClearPriorityBuffer();
  object_bg2_buffer_.ClearPriorityBuffer();

  // Log layer distribution for this room
  int layer0_count = 0, layer1_count = 0, layer2_count = 0;
  for (const auto& obj : tile_objects_) {
    switch (obj.GetLayerValue()) {
      case 0:
        layer0_count++;
        break;
      case 1:
        layer1_count++;
        break;
      case 2:
        layer2_count++;
        break;
    }
  }
  LOG_DEBUG(
      "Room",
      "Room %03X Object Layer Summary: L0(BG1)=%d, L1(BG2)=%d, L2(BG3)=%d",
      room_id_, layer0_count, layer1_count, layer2_count);

  // Render objects to appropriate buffers
  // BG1 = Floor/Main (Layer 0, 2)
  // BG2 = Overlay (Layer 1)
  // Pass bg1_buffer_ for BG2 object masking - this creates "holes" in the floor
  // so BG2 overlay content (platforms, statues) shows through BG1 floor tiles
  std::vector<RoomObject> objects_to_draw;
  objects_to_draw.reserve(tile_objects_.size());
  for (const auto& obj : tile_objects_) {
    // Torches and pushable blocks are NOT part of the room object stream.
    // They come from the global tables and are drawn after the stream in
    // USDASM (LoadAndBuildRoom $01:873A). Draw them in a dedicated pass.
    if ((obj.options() & ObjectOption::Torch) != ObjectOption::Nothing) {
      continue;
    }
    if ((obj.options() & ObjectOption::Block) != ObjectOption::Nothing) {
      continue;
    }
    objects_to_draw.push_back(obj);
  }

  auto status = drawer.DrawObjectList(objects_to_draw, object_bg1_buffer_,
                                      object_bg2_buffer_, palette_group,
                                      dungeon_state_.get(), &bg1_buffer_);

  // Render doors using DoorDef struct with enum types
  // Doors are drawn to the OBJECT buffer for layer visibility control
  // This allows doors to remain visible when toggling BG1_Layout off
  for (int i = 0; i < static_cast<int>(doors_.size()); ++i) {
    const auto& door = doors_[i];
    ObjectDrawer::DoorDef door_def;
    door_def.type = door.type;
    door_def.direction = door.direction;
    door_def.position = door.position;
    // Draw doors to object buffers (not layout buffers) so they remain visible
    // when BG1_Layout is hidden. Doors are objects, not layout tiles.
    drawer.DrawDoor(door_def, i, object_bg1_buffer_, object_bg2_buffer_,
                    dungeon_state_.get());
  }
  // Mark object buffer as modified so texture gets updated
  if (!doors_.empty()) {
    object_bg1_buffer_.bitmap().set_modified(true);
  }

  // Render pot items
  // Pot items now have their own position from ROM data
  // No need to match to objects - each item has exact coordinates
  for (const auto& pot_item : pot_items_) {
    if (pot_item.item != 0) {  // Skip "Nothing" items
      // PotItem provides pixel coordinates, convert to tile coords
      int tile_x = pot_item.GetTileX();
      int tile_y = pot_item.GetTileY();
      drawer.DrawPotItem(pot_item.item, tile_x, tile_y, object_bg1_buffer_);
    }
  }

  // Render sprites (for key drops)
  // We don't have full sprite rendering yet, but we can visualize key drops
  for (const auto& sprite : sprites_) {
    if (sprite.key_drop() > 0) {
      // Draw key drop visualization
      // Use a special item ID or just draw a key icon
      // We can reuse DrawPotItem with a special ID for key
      // Or add DrawKeyDrop to ObjectDrawer
      // For now, let's use DrawPotItem with ID 0xFD (Small Key) or 0xFE (Big Key)
      uint8_t key_item = (sprite.key_drop() == 1) ? 0xFD : 0xFE;
      drawer.DrawPotItem(key_item, sprite.x(), sprite.y(), object_bg1_buffer_);
    }
  }

  // Special tables pass (USDASM-aligned):
  // - Pushable blocks: bank_01.asm RoomDraw_PushableBlock uses RoomDrawObjectData
  //   offset $0E52 (bank_00.asm #obj0E52).
  // - Lightable torches: bank_01.asm RoomDraw_LightableTorch chooses between
  //   offsets $0EC2 (unlit) and $0ECA (lit) (bank_00.asm #obj0EC2/#obj0ECA).
  constexpr uint16_t kRoomDrawObj_PushableBlock = 0x0E52;
  constexpr uint16_t kRoomDrawObj_TorchUnlit = 0x0EC2;
  constexpr uint16_t kRoomDrawObj_TorchLit = 0x0ECA;
  for (const auto& obj : tile_objects_) {
    if ((obj.options() & ObjectOption::Block) != ObjectOption::Nothing) {
      (void)drawer.DrawRoomDrawObjectData2x2(
          static_cast<uint16_t>(obj.id_), obj.x_, obj.y_, obj.layer_,
          kRoomDrawObj_PushableBlock, object_bg1_buffer_, object_bg2_buffer_);
      continue;
    }
    if ((obj.options() & ObjectOption::Torch) != ObjectOption::Nothing) {
      const uint16_t off = obj.lit_ ? kRoomDrawObj_TorchLit
                                    : kRoomDrawObj_TorchUnlit;
      (void)drawer.DrawRoomDrawObjectData2x2(
          static_cast<uint16_t>(obj.id_), obj.x_, obj.y_, obj.layer_, off,
          object_bg1_buffer_, object_bg2_buffer_);
      continue;
    }
  }

  // Log only failures, not successes
  if (!status.ok()) {
    LOG_DEBUG(
        "[RenderObjectsToBackground]",
        "ObjectDrawer failed: %s - FALLING BACK TO MANUAL",
        std::string(status.message().data(), status.message().size()).c_str());

    LOG_DEBUG("[RenderObjectsToBackground]",
              "Room %d: Manual rendering objects (fallback)", room_id_);
    auto& bg1_bmp = bg1_buffer_.bitmap();
    // Simple manual rendering: draw a colored rectangle for each object
    for (const auto& obj : tile_objects_) {
      int x = obj.x() * 8;
      int y = obj.y() * 8;
      int width = 16;  // Default size for manual draw
      int height = 16;

      // Basic layer-based coloring for manual mode
      uint8_t color_idx = 0;  // Default transparent
      if (obj.GetLayerValue() == 0) {
        color_idx = 5;  // Example: Reddish for Layer 0
      } else if (obj.GetLayerValue() == 1) {
        color_idx = 10;  // Example: Greenish for Layer 1
      } else {
        color_idx = 15;  // Example: Bluish for Layer 2
      }
      // Draw simple rectangle using WriteToBGRSurface
      for (int py = y; py < y + height && py < bg1_bmp.height(); ++py) {
        for (int px = x; px < x + width && px < bg1_bmp.width(); ++px) {
          int pixel_offset = (py * bg1_bmp.width()) + px;
          bg1_bmp.WriteToPixel(pixel_offset, color_idx);
        }
      }
    }
    objects_dirty_ = false;  // Mark as clean after manual draw
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

  if (!game_data_) {
    return;
  }
  auto* gfx_buffer_data = &game_data_->graphics_buffer;
  if (gfx_buffer_data->empty()) {
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

  int gfx_ptr = SnesToPc(version_constants().kGfxAnimatedPointer);
  if (gfx_ptr < 0 || gfx_ptr >= static_cast<int>(rom_data.size())) {
    return;
  }

  int data = 0;
  while (data < 1024) {
    // Validate buffer access for first operation
    // 92 * 4096 = 376832. 1024 * 10 = 10240. Total ~387KB.
    int first_offset = data + (92 * 4096) + (1024 * animated_frame_);
    if (first_offset >= 0 &&
        first_offset < static_cast<int>(gfx_buffer_data->size())) {
      uint8_t map_byte = (*gfx_buffer_data)[first_offset];

      // Validate current_gfx16_ access
      int gfx_offset = data + (7 * 4096);
      if (gfx_offset >= 0 &&
          gfx_offset < static_cast<int>(current_gfx16_.size())) {
        current_gfx16_[gfx_offset] = map_byte;
      }
    }

    // Validate buffer access for second operation
    int tileset_index = rom_data[gfx_ptr + background_tileset_];
    int second_offset =
        data + (tileset_index * 4096) + (1024 * animated_frame_);
    if (second_offset >= 0 &&
        second_offset < static_cast<int>(gfx_buffer_data->size())) {
      uint8_t map_byte = (*gfx_buffer_data)[second_offset];

      // Validate current_gfx16_ access
      int gfx_offset = data + (7 * 4096) - 1024;
      if (gfx_offset >= 0 &&
          gfx_offset < static_cast<int>(current_gfx16_.size())) {
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

  // Load custom collision map if present
  if (auto res = LoadCustomCollisionMap(rom_, room_id_); res.ok()) {
    custom_collision_ = std::move(res.value());
  }

  // Freshly loaded from ROM; not dirty until the editor mutates it.
  custom_collision_dirty_ = false;
}

void Room::ParseObjectsFromLocation(int objects_location) {
  auto rom_data = rom()->vector();

  // Clear existing objects before parsing to prevent accumulation on reload
  tile_objects_.clear();
  doors_.clear();
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
  // ASM: Main object loop logic (implicit in structure)
  while (!end_read && pos < (int)rom_->size()) {
    // Check if we have enough bytes to read
    if (pos + 1 >= (int)rom_->size()) {
      break;
    }

    b1 = rom_data[pos];
    b2 = rom_data[pos + 1];

    // ASM Marker: 0xFF 0xFF - End of Layer
    // Signals transition between object layers:
    //   Layer 0 -> BG1 buffer (main floor/walls)
    //   Layer 1 -> BG2 buffer (overlay layer)
    //   Layer 2 -> BG1 buffer (priority objects, still on BG1)
    if (b1 == 0xFF && b2 == 0xFF) {
      pos += 2;  // Jump to next layer
      layer++;
      LOG_DEBUG("Room", "Room %03X: Layer transition to layer %d (%s)",
                room_id_, layer,
                layer == 1 ? "BG2" : (layer == 2 ? "BG3" : "END"));
      door = false;
      if (layer == 3) {
        break;
      }
      continue;
    }

    // ASM Marker: 0xF0 0xFF - Start of Door List
    // See RoomDraw_DoorObject ($018916) logic
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
      // ASM: RoomDraw_RoomObject ($01893C)
      // Handles Subtype 1, 2, 3 parsing based on byte values
      RoomObject r = RoomObject::DecodeObjectFromBytes(
          b1, b2, b3, static_cast<uint8_t>(layer));

      LOG_DEBUG("Room", "Room %03X: Object 0x%03X at (%d,%d) layer=%d (%s)",
                room_id_, r.id_, r.x_, r.y_, layer,
                layer == 0 ? "BG1" : (layer == 1 ? "BG2" : "BG3"));

      // Validate object ID before adding to the room
      // Object IDs can be up to 12-bit (0xFFF) to support Type 3 objects
      if (r.id_ >= 0 && r.id_ <= 0xFFF) {
        r.SetRom(rom_);
        tile_objects_.push_back(r);

        // Handle special object types (staircases, chests, etc.)
        HandleSpecialObjects(r.id_, r.x(), r.y(), nbr_of_staircase);
      }
    } else {
      // Handle door objects
      // ASM format (from RoomDraw_DoorObject):
      //   b1: bits 4-7 = position index, bits 0-1 = direction
      //   b2: door type (full byte)
      auto door = Door::FromRomBytes(b1, b2);
      LOG_DEBUG("Room",
                "ParseDoor: room=%d b1=0x%02X b2=0x%02X pos=%d dir=%d type=%d",
                room_id_, b1, b2, door.position,
                static_cast<int>(door.direction), static_cast<int>(door.type));
      doors_.push_back(door);
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

  // IMPORTANT: Torches and pushable blocks are stored in global per-dungeon
  // tables (see USDASM: LoadAndBuildRoom $01:873A). They are drawn after the
  // room object stream passes, so they must never be encoded into the room
  // object stream.
  for (const auto& obj : tile_objects_) {
    if ((obj.options() & ObjectOption::Torch) != ObjectOption::Nothing) {
      continue;
    }
    if ((obj.options() & ObjectOption::Block) != ObjectOption::Nothing) {
      continue;
    }
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

  // ASM marker 0xF0 0xFF - start of door list (per ZScreamDungeon / RoomDraw_DoorObject)
  bytes.push_back(0xF0);
  bytes.push_back(0xFF);
  for (const auto& door : doors_) {
    auto [b1, b2] = door.EncodeBytes();
    bytes.push_back(b1);
    bytes.push_back(b2);
  }
  // Door list terminator (word $FFFF). In USDASM, RoomDraw_DrawAllObjects
  // enters door mode on word $FFF0 (bytes F0 FF) and stops when it reads
  // word $FFFF (bytes FF FF).
  bytes.push_back(0xFF);
  bytes.push_back(0xFF);

  return bytes;
}

std::vector<uint8_t> Room::EncodeSprites() const {
  std::vector<uint8_t> bytes;

  for (const auto& sprite : sprites_) {
    uint8_t b1, b2, b3;

    // b3 is simply the ID
    b3 = sprite.id();

    // b2 = (X & 0x1F) | ((Flags & 0x07) << 5)
    // Flags 0-2 come from b2 5-7
    b2 = (sprite.x() & 0x1F) | ((sprite.subtype() & 0x07) << 5);

    // b1 = (Y & 0x1F) | ((Flags & 0x18) << 2) | ((Layer & 1) << 7)
    // Flags 3-4 come from b1 5-6. (0x18 is 00011000)
    // Layer bit 0 comes from b1 7
    b1 = (sprite.y() & 0x1F) | ((sprite.subtype() & 0x18) << 2) |
         ((sprite.layer() & 0x01) << 7);

    bytes.push_back(b1);
    bytes.push_back(b2);
    bytes.push_back(b3);
  }

  // Terminator
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

  // Calculate available space
  RoomSize room_size_info = CalculateRoomSize(rom_, room_id_);
  int available_size = room_size_info.room_size;

  // Skip graphics/layout header (2 bytes)
  int write_pos = objects_location + 2;

  // Encode all objects
  auto encoded_bytes = EncodeObjects();

  // VALIDATION: Check if new data fits in available space
  // We subtract 2 bytes for the header which is not part of encoded_bytes
  if (encoded_bytes.size() > available_size - 2) {
    return absl::OutOfRangeError(absl::StrFormat(
        "Room %d object data too large! Size: %d, Available: %d", room_id_,
        encoded_bytes.size(), available_size - 2));
  }

  // Write encoded bytes to ROM (includes 0xF0 0xFF + door list)
  RETURN_IF_ERROR(rom_->WriteVector(write_pos, encoded_bytes));

  // Write door pointer: first byte after 0xF0 0xFF (per ZScreamDungeon Save.cs)
  const int door_list_offset = static_cast<int>(encoded_bytes.size()) -
                               static_cast<int>(doors_.size()) * 2 - 2;
  const int door_pointer_pc = write_pos + door_list_offset;
  RETURN_IF_ERROR(
      rom_->WriteLong(doorPointers + (room_id_ * 3),
                      static_cast<uint32_t>(PcToSnes(door_pointer_pc))));

  return absl::OkStatus();
}

absl::Status Room::SaveSprites() {
  if (rom_ == nullptr) {
    return absl::InvalidArgumentError("ROM pointer is null");
  }

  auto rom_data = rom()->vector();

  // Calculate sprite pointer table location
  // Bank 09 + rooms_sprite_pointer (was incorrectly 0x04)
  int sprite_pointer = (0x09 << 16) +
                       (rom_data[rooms_sprite_pointer + 1] << 8) +
                       (rom_data[rooms_sprite_pointer]);
  sprite_pointer = SnesToPc(sprite_pointer);

  if (sprite_pointer < 0 ||
      sprite_pointer + (room_id_ * 2) + 1 >= (int)rom_->size()) {
    return absl::OutOfRangeError("Sprite table pointer out of range");
  }

  // Read room sprite address from table
  int sprite_address_snes =
      (0x09 << 16) + (rom_data[sprite_pointer + (room_id_ * 2) + 1] << 8) +
      rom_data[sprite_pointer + (room_id_ * 2)];

  int sprite_address = SnesToPc(sprite_address_snes);

  if (sprite_address < 0 || sprite_address >= (int)rom_->size()) {
    return absl::OutOfRangeError("Sprite address out of range");
  }

  // Calculate available space for sprites
  // Check next room's sprite pointer
  int next_sprite_address_snes =
      (0x09 << 16) +
      (rom_data[sprite_pointer + ((room_id_ + 1) * 2) + 1] << 8) +
      rom_data[sprite_pointer + ((room_id_ + 1) * 2)];

  int next_sprite_address = SnesToPc(next_sprite_address_snes);

  // Handle wrap-around or end of bank if needed, but usually sequential
  int available_size = next_sprite_address - sprite_address;

  // If calculation seems wrong (negative or too large), fallback to a safe limit or error
  if (available_size <= 0 || available_size > 0x1000) {
    // Fallback: Assume standard max or just warn.
    // For now, let's be strict but allow a reasonable max if calculation fails (e.g. last room)
    if (room_id_ == NumberOfRooms - 1) {
      available_size = 0x100;  // Arbitrary safe limit for last room
    } else {
      // If negative, it means pointers are not sequential.
      // This happens in some ROMs. We can't easily validate size then without a free space map.
      // We'll log a warning and proceed with caution? No, prompt says "Free space validation".
      // Let's error out if we can't determine size.
      return absl::InternalError(absl::StrFormat(
          "Cannot determine available sprite space for room %d", room_id_));
    }
  }

  // Handle sortsprites byte (skip if present)
  bool has_sort_sprite = false;
  if (rom_data[sprite_address] == 1) {
    has_sort_sprite = true;
    sprite_address += 1;
    available_size -= 1;
  }

  auto encoded_bytes = EncodeSprites();

  // VALIDATION
  if (encoded_bytes.size() > available_size) {
    return absl::OutOfRangeError(absl::StrFormat(
        "Room %d sprite data too large! Size: %d, Available: %d", room_id_,
        encoded_bytes.size(), available_size));
  }

  return rom_->WriteVector(sprite_address, encoded_bytes);
}

absl::Status Room::SaveRoomHeader() {
  if (rom_ == nullptr) {
    return absl::InvalidArgumentError("ROM pointer is null");
  }

  const auto& rom_data = rom()->vector();
  if (kRoomHeaderPointer < 0 ||
      kRoomHeaderPointer + 2 >= static_cast<int>(rom_data.size())) {
    return absl::OutOfRangeError("Room header pointer out of range");
  }
  if (kRoomHeaderPointerBank < 0 ||
      kRoomHeaderPointerBank >= static_cast<int>(rom_data.size())) {
    return absl::OutOfRangeError("Room header pointer bank out of range");
  }

  int header_pointer = (rom_data[kRoomHeaderPointer + 2] << 16) +
                       (rom_data[kRoomHeaderPointer + 1] << 8) +
                       rom_data[kRoomHeaderPointer];
  header_pointer = SnesToPc(header_pointer);

  int table_offset = header_pointer + (room_id_ * 2);
  if (table_offset < 0 ||
      table_offset + 1 >= static_cast<int>(rom_data.size())) {
    return absl::OutOfRangeError("Room header table offset out of range");
  }

  int address = (rom_data[kRoomHeaderPointerBank] << 16) +
                (rom_data[table_offset + 1] << 8) + rom_data[table_offset];
  int header_location = SnesToPc(address);

  if (header_location < 0 ||
      header_location + 13 >= static_cast<int>(rom_data.size())) {
    return absl::OutOfRangeError("Room header location out of range");
  }

  // Build 14-byte header to match LoadRoomHeaderFromRom layout
  uint8_t byte0 = (static_cast<uint8_t>(bg2()) << 5) |
                  (static_cast<uint8_t>(collision()) << 2) |
                  (IsLight() ? 1 : 0);
  uint8_t byte1 = palette & 0x3F;
  uint8_t byte7 = (staircase_plane(0) << 2) | (staircase_plane(1) << 4) |
                  (staircase_plane(2) << 6);

  RETURN_IF_ERROR(rom_->WriteByte(header_location + 0, byte0));
  RETURN_IF_ERROR(rom_->WriteByte(header_location + 1, byte1));
  RETURN_IF_ERROR(rom_->WriteByte(header_location + 2, blockset));
  RETURN_IF_ERROR(rom_->WriteByte(header_location + 3, spriteset));
  RETURN_IF_ERROR(
      rom_->WriteByte(header_location + 4, static_cast<uint8_t>(effect())));
  RETURN_IF_ERROR(
      rom_->WriteByte(header_location + 5, static_cast<uint8_t>(tag1())));
  RETURN_IF_ERROR(
      rom_->WriteByte(header_location + 6, static_cast<uint8_t>(tag2())));
  RETURN_IF_ERROR(rom_->WriteByte(header_location + 7, byte7));
  RETURN_IF_ERROR(rom_->WriteByte(header_location + 8, staircase_plane(3)));
  RETURN_IF_ERROR(rom_->WriteByte(header_location + 9, holewarp));
  RETURN_IF_ERROR(rom_->WriteByte(header_location + 10, staircase_room(0)));
  RETURN_IF_ERROR(rom_->WriteByte(header_location + 11, staircase_room(1)));
  RETURN_IF_ERROR(rom_->WriteByte(header_location + 12, staircase_room(2)));
  RETURN_IF_ERROR(rom_->WriteByte(header_location + 13, staircase_room(3)));

  int msg_addr = kMessagesIdDungeon + (room_id_ * 2);
  if (msg_addr < 0 || msg_addr + 1 >= static_cast<int>(rom_data.size())) {
    return absl::OutOfRangeError("Message ID address out of range");
  }
  RETURN_IF_ERROR(rom_->WriteWord(msg_addr, message_id_));

  return absl::OkStatus();
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
  MarkObjectsDirty();

  return absl::OkStatus();
}

absl::Status Room::RemoveObject(size_t index) {
  if (index >= tile_objects_.size()) {
    return absl::OutOfRangeError("Object index out of range");
  }

  tile_objects_.erase(tile_objects_.begin() + index);
  MarkObjectsDirty();

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
  MarkObjectsDirty();

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
  // Door encoding: b1 = (door_pos << 4) | (door_dir & 0x03)
  //                     position in bits 4-7, direction in bits 0-1
  //                b2 = door_type (full byte, values 0x00, 0x02, 0x04, etc.)
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

  // Avoid duplication if LoadTorches is called multiple times.
  tile_objects_.erase(
      std::remove_if(tile_objects_.begin(), tile_objects_.end(),
                     [](const RoomObject& obj) {
                       return (obj.options() & ObjectOption::Torch) !=
                              ObjectOption::Nothing;
                     }),
      tile_objects_.end());

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
        torch_obj.SetRom(rom_);
        torch_obj.set_options(ObjectOption::Torch);
        torch_obj.lit_ = lit;

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

namespace {

constexpr int kTorchesMaxSize = 0x120;  // ZScream Constants.TorchesMaxSize

// Parse current ROM torch blob into per-room segments for preserve-merge.
std::vector<std::vector<uint8_t>> ParseRomTorchSegments(
    const std::vector<uint8_t>& rom_data, int bytes_count) {
  std::vector<std::vector<uint8_t>> segments(kNumberOfRooms);
  int i = 0;
  while (i + 1 < bytes_count && i < kTorchesMaxSize) {
    uint8_t b1 = rom_data[kTorchData + i];
    uint8_t b2 = rom_data[kTorchData + i + 1];
    if (b1 == 0xFF && b2 == 0xFF) {
      i += 2;
      continue;
    }
    uint16_t room_id = (b2 << 8) | b1;
    if (room_id >= kNumberOfRooms) {
      i += 2;
      continue;
    }
    std::vector<uint8_t> seg;
    seg.push_back(b1);
    seg.push_back(b2);
    i += 2;
    while (i + 1 < bytes_count && i < kTorchesMaxSize) {
      b1 = rom_data[kTorchData + i];
      b2 = rom_data[kTorchData + i + 1];
      if (b1 == 0xFF && b2 == 0xFF) {
        seg.push_back(0xFF);
        seg.push_back(0xFF);
        i += 2;
        break;
      }
      seg.push_back(b1);
      seg.push_back(b2);
      i += 2;
    }
    if (room_id < segments.size()) {
      segments[room_id] = std::move(seg);
    }
  }
  return segments;
}

}  // namespace

absl::Status SaveAllTorches(Rom* rom, absl::Span<const Room> rooms) {
  if (!rom || !rom->is_loaded()) {
    return absl::InvalidArgumentError("ROM not loaded");
  }

  bool any_torch_objects = false;
  for (const auto& room : rooms) {
    for (const auto& obj : room.GetTileObjects()) {
      if ((obj.options() & ObjectOption::Torch) != ObjectOption::Nothing) {
        any_torch_objects = true;
        break;
      }
    }
    if (any_torch_objects) {
      break;
    }
  }
  if (!any_torch_objects) {
    return absl::OkStatus();
  }

  const auto& rom_data = rom->vector();
  int existing_count = (rom_data[kTorchesLengthPointer + 1] << 8) |
                       rom_data[kTorchesLengthPointer];
  if (existing_count > kTorchesMaxSize) {
    existing_count = kTorchesMaxSize;
  }
  auto rom_segments = ParseRomTorchSegments(rom_data, existing_count);

  std::vector<uint8_t> bytes;
  for (size_t room_id = 0;
       room_id < rooms.size() && room_id < rom_segments.size(); ++room_id) {
    const auto& room = rooms[room_id];
    bool has_torch_objects = false;
    for (const auto& obj : room.GetTileObjects()) {
      if ((obj.options() & ObjectOption::Torch) != ObjectOption::Nothing) {
        has_torch_objects = true;
        break;
      }
    }
    if (has_torch_objects) {
      bytes.push_back(room_id & 0xFF);
      bytes.push_back((room_id >> 8) & 0xFF);
      for (const auto& obj : room.GetTileObjects()) {
        if ((obj.options() & ObjectOption::Torch) == ObjectOption::Nothing) {
          continue;
        }
        int address = obj.x() + (obj.y() * 64);
        int word = address << 1;
        uint8_t b1 = word & 0xFF;
        uint8_t b2 = ((word >> 8) & 0x1F) | ((obj.GetLayerValue() & 1) << 5);
        if (obj.lit_) {
          b2 |= 0x80;
        }
        bytes.push_back(b1);
        bytes.push_back(b2);
      }
      bytes.push_back(0xFF);
      bytes.push_back(0xFF);
    } else if (!rom_segments[room_id].empty()) {
      for (uint8_t b : rom_segments[room_id]) {
        bytes.push_back(b);
      }
    }
  }

  if (bytes.size() > kTorchesMaxSize) {
    return absl::ResourceExhaustedError(
        absl::StrFormat("Torch data too large: %d bytes (max %d)", bytes.size(),
                        kTorchesMaxSize));
  }

  // Avoid unnecessary writes: if the generated torch blob matches the ROM
  // exactly, keep the ROM clean. This prevents "save with no changes" from
  // dirtying the ROM and makes diffs stable.
  const uint16_t current_len =
      static_cast<uint16_t>(rom_data[kTorchesLengthPointer]) |
      (static_cast<uint16_t>(rom_data[kTorchesLengthPointer + 1]) << 8);
  if (current_len == bytes.size() &&
      kTorchData + static_cast<int>(bytes.size()) <=
          static_cast<int>(rom_data.size()) &&
      std::equal(bytes.begin(), bytes.end(), rom_data.begin() + kTorchData)) {
    return absl::OkStatus();
  }

  RETURN_IF_ERROR(rom->WriteWord(kTorchesLengthPointer,
                                 static_cast<uint16_t>(bytes.size())));
  return rom->WriteVector(kTorchData, bytes);
}

absl::Status SaveAllPits(Rom* rom) {
  if (!rom || !rom->is_loaded()) {
    return absl::InvalidArgumentError("ROM not loaded");
  }
  const auto& rom_data = rom->vector();
  if (kPitCount < 0 || kPitCount >= static_cast<int>(rom_data.size()) ||
      kPitPointer + 2 >= static_cast<int>(rom_data.size())) {
    return absl::OutOfRangeError("Pit count/pointer out of range");
  }
  int pit_count_byte = rom_data[kPitCount];
  int pit_entries = pit_count_byte / 2;
  if (pit_entries <= 0) {
    return absl::OkStatus();
  }
  int pit_ptr_snes = (rom_data[kPitPointer + 2] << 16) |
                     (rom_data[kPitPointer + 1] << 8) | rom_data[kPitPointer];
  int pit_data_pc = SnesToPc(pit_ptr_snes);
  int data_len = pit_entries * 2;
  if (pit_data_pc < 0 ||
      pit_data_pc + data_len > static_cast<int>(rom_data.size())) {
    return absl::OutOfRangeError("Pit data region out of range");
  }
  std::vector<uint8_t> data(rom_data.begin() + pit_data_pc,
                            rom_data.begin() + pit_data_pc + data_len);
  RETURN_IF_ERROR(rom->WriteByte(kPitCount, pit_count_byte));
  RETURN_IF_ERROR(rom->WriteByte(kPitPointer, pit_ptr_snes & 0xFF));
  RETURN_IF_ERROR(rom->WriteByte(kPitPointer + 1, (pit_ptr_snes >> 8) & 0xFF));
  RETURN_IF_ERROR(rom->WriteByte(kPitPointer + 2, (pit_ptr_snes >> 16) & 0xFF));
  return rom->WriteVector(pit_data_pc, data);
}

absl::Status SaveAllBlocks(Rom* rom) {
  if (!rom || !rom->is_loaded()) {
    return absl::InvalidArgumentError("ROM not loaded");
  }
  const auto& rom_data = rom->vector();
  if (kBlocksLength + 1 >= static_cast<int>(rom_data.size())) {
    return absl::OutOfRangeError("Blocks length out of range");
  }
  int blocks_count =
      (rom_data[kBlocksLength + 1] << 8) | rom_data[kBlocksLength];
  if (blocks_count <= 0) {
    return absl::OkStatus();
  }
  const int kRegionSize = 0x80;
  int ptrs[4] = {kBlocksPointer1, kBlocksPointer2, kBlocksPointer3,
                 kBlocksPointer4};
  for (int r = 0; r < 4; ++r) {
    if (ptrs[r] + 2 >= static_cast<int>(rom_data.size())) {
      return absl::OutOfRangeError("Blocks pointer out of range");
    }
    int snes = (rom_data[ptrs[r] + 2] << 16) | (rom_data[ptrs[r] + 1] << 8) |
               rom_data[ptrs[r]];
    int pc = SnesToPc(snes);
    int off = r * kRegionSize;
    int len = std::min(kRegionSize, blocks_count - off);
    if (len <= 0)
      break;
    if (pc < 0 || pc + len > static_cast<int>(rom_data.size())) {
      return absl::OutOfRangeError("Blocks data region out of range");
    }
    std::vector<uint8_t> chunk(rom_data.begin() + pc,
                               rom_data.begin() + pc + len);
    RETURN_IF_ERROR(rom->WriteVector(pc, chunk));
  }
  RETURN_IF_ERROR(
      rom->WriteWord(kBlocksLength, static_cast<uint16_t>(blocks_count)));
  return absl::OkStatus();
}

absl::Status SaveAllCollision(Rom* rom, absl::Span<Room> rooms) {
  if (!rom || !rom->is_loaded()) {
    return absl::InvalidArgumentError("ROM not loaded");
  }

  // If the custom collision region doesn't exist (vanilla ROM), treat as noop.
  const auto& rom_data = rom->vector();
  const int ptrs_size = kNumberOfRooms * 3;
  if (kCustomCollisionRoomPointers + ptrs_size >
      static_cast<int>(rom_data.size())) {
    return absl::OkStatus();
  }

  for (auto& room : rooms) {
    if (!room.IsLoaded()) {
      continue;
    }
    if (!room.custom_collision_dirty()) {
      continue;
    }

    const int room_id = room.id();
    const int ptr_offset = kCustomCollisionRoomPointers + (room_id * 3);
    if (ptr_offset + 2 >= static_cast<int>(rom_data.size())) {
      return absl::OutOfRangeError("Custom collision pointer out of range");
    }

    if (!room.has_custom_collision()) {
      // Disable: clear the pointer entry.
      RETURN_IF_ERROR(rom->WriteByte(ptr_offset, 0));
      RETURN_IF_ERROR(rom->WriteByte(ptr_offset + 1, 0));
      RETURN_IF_ERROR(rom->WriteByte(ptr_offset + 2, 0));
      room.ClearCustomCollisionDirty();
      continue;
    }

    // Treat an all-zero map as disabled to avoid wasting space.
    bool any = false;
    for (uint8_t v : room.custom_collision().tiles) {
      if (v != 0) {
        any = true;
        break;
      }
    }
    if (!any) {
      RETURN_IF_ERROR(rom->WriteByte(ptr_offset, 0));
      RETURN_IF_ERROR(rom->WriteByte(ptr_offset + 1, 0));
      RETURN_IF_ERROR(rom->WriteByte(ptr_offset + 2, 0));
      room.ClearCustomCollisionDirty();
      continue;
    }

    RETURN_IF_ERROR(WriteTrackCollision(rom, room_id, room.custom_collision()));
    room.ClearCustomCollisionDirty();
  }

  return absl::OkStatus();
}

namespace {

// Parse current ROM chest data into per-room lists (room_id, chest_data).
std::vector<std::vector<std::pair<uint8_t, bool>>> ParseRomChests(
    const std::vector<uint8_t>& rom_data, int cpos, int clength) {
  std::vector<std::vector<std::pair<uint8_t, bool>>> per_room(kNumberOfRooms);
  for (int i = 0;
       i < clength && cpos + i * 3 + 2 < static_cast<int>(rom_data.size());
       ++i) {
    int off = cpos + i * 3;
    uint16_t word = (rom_data[off + 1] << 8) | rom_data[off];
    uint16_t room_id = word & 0x7FFF;
    bool big = (word & 0x8000) != 0;
    uint8_t id = rom_data[off + 2];
    if (room_id < kNumberOfRooms) {
      per_room[room_id].emplace_back(id, big);
    }
  }
  return per_room;
}

std::vector<std::vector<uint8_t>> ParseRomPotItems(
    const std::vector<uint8_t>& rom_data) {
  std::vector<std::vector<uint8_t>> per_room(kNumberOfRooms);
  int table_addr = kRoomItemsPointers;
  if (table_addr + (kNumberOfRooms * 2) > static_cast<int>(rom_data.size())) {
    return per_room;
  }
  for (int room_id = 0; room_id < kNumberOfRooms; ++room_id) {
    int ptr_off = table_addr + (room_id * 2);
    uint16_t item_ptr = (rom_data[ptr_off + 1] << 8) | rom_data[ptr_off];
    int item_addr = SnesToPc(0x010000 | item_ptr);
    if (item_addr < 0 || item_addr >= static_cast<int>(rom_data.size())) {
      continue;
    }
    int next_ptr_off = table_addr + ((room_id + 1) * 2);
    int next_item_addr =
        (room_id + 1 < kNumberOfRooms)
            ? SnesToPc(0x010000 | ((rom_data[next_ptr_off + 1] << 8) |
                                   rom_data[next_ptr_off]))
            : item_addr + 0x100;
    int max_len = next_item_addr - item_addr;
    if (max_len <= 0)
      continue;

    std::vector<uint8_t> bytes;
    int cursor = item_addr;
    const int limit =
        std::min(item_addr + max_len, static_cast<int>(rom_data.size()));
    while (cursor + 1 < limit) {
      uint8_t b1 = rom_data[cursor++];
      uint8_t b2 = rom_data[cursor++];
      bytes.push_back(b1);
      bytes.push_back(b2);
      if (b1 == 0xFF && b2 == 0xFF) {
        break;
      }
      if (cursor >= limit) {
        break;
      }
      bytes.push_back(rom_data[cursor++]);
    }
    if (!bytes.empty()) {
      per_room[room_id] = std::move(bytes);
    }
  }
  return per_room;
}

}  // namespace

absl::Status SaveAllChests(Rom* rom, absl::Span<const Room> rooms) {
  if (!rom || !rom->is_loaded()) {
    return absl::InvalidArgumentError("ROM not loaded");
  }
  const auto& rom_data = rom->vector();
  if (kChestsLengthPointer + 1 >= static_cast<int>(rom_data.size()) ||
      kChestsDataPointer1 + 2 >= static_cast<int>(rom_data.size())) {
    return absl::OutOfRangeError("Chest pointers out of range");
  }
  int clength = (rom_data[kChestsLengthPointer + 1] << 8) |
                rom_data[kChestsLengthPointer];
  int cpos = SnesToPc((rom_data[kChestsDataPointer1 + 2] << 16) |
                      (rom_data[kChestsDataPointer1 + 1] << 8) |
                      rom_data[kChestsDataPointer1]);
  auto rom_chests = ParseRomChests(rom_data, cpos, clength);

  std::vector<uint8_t> bytes;
  for (size_t room_id = 0;
       room_id < rooms.size() && room_id < rom_chests.size(); ++room_id) {
    const auto& room = rooms[room_id];
    const auto& chests = room.GetChests();
    if (chests.empty()) {
      for (const auto& [id, big] : rom_chests[room_id]) {
        uint16_t word = room_id | (big ? 0x8000 : 0);
        bytes.push_back(word & 0xFF);
        bytes.push_back((word >> 8) & 0xFF);
        bytes.push_back(id);
      }
    } else {
      for (const auto& c : chests) {
        uint16_t word = room_id | (c.size ? 0x8000 : 0);
        bytes.push_back(word & 0xFF);
        bytes.push_back((word >> 8) & 0xFF);
        bytes.push_back(c.id);
      }
    }
  }

  if (cpos < 0 || cpos + static_cast<int>(bytes.size()) >
                      static_cast<int>(rom_data.size())) {
    return absl::OutOfRangeError("Chest data region out of range");
  }
  RETURN_IF_ERROR(rom->WriteWord(kChestsLengthPointer,
                                 static_cast<uint16_t>(bytes.size() / 3)));
  return rom->WriteVector(cpos, bytes);
}

absl::Status SaveAllPotItems(Rom* rom, absl::Span<const Room> rooms) {
  if (!rom || !rom->is_loaded()) {
    return absl::InvalidArgumentError("ROM not loaded");
  }
  const auto& rom_data = rom->vector();
  const auto rom_pot_items = ParseRomPotItems(rom_data);
  int table_addr = kRoomItemsPointers;
  if (table_addr + (kNumberOfRooms * 2) > static_cast<int>(rom_data.size())) {
    return absl::OutOfRangeError("Room items pointer table out of range");
  }
  for (size_t room_id = 0; room_id < rooms.size() && room_id < kNumberOfRooms;
       ++room_id) {
    const auto& room = rooms[room_id];
    const bool room_loaded = room.IsLoaded();
    const auto& pot_items = room.GetPotItems();
    int ptr_off = table_addr + (room_id * 2);
    uint16_t item_ptr = (rom_data[ptr_off + 1] << 8) | rom_data[ptr_off];
    int item_addr = SnesToPc(0x010000 | item_ptr);
    if (item_addr < 0 || item_addr + 2 >= static_cast<int>(rom_data.size())) {
      continue;
    }
    int next_ptr_off = table_addr + ((room_id + 1) * 2);
    int next_item_addr =
        (room_id + 1 < kNumberOfRooms)
            ? SnesToPc(0x010000 | ((rom_data[next_ptr_off + 1] << 8) |
                                   rom_data[next_ptr_off]))
            : item_addr + 0x100;
    int max_len = next_item_addr - item_addr;
    if (max_len <= 0)
      continue;
    std::vector<uint8_t> bytes;
    if (!room_loaded) {
      if (room_id < rom_pot_items.size()) {
        bytes = rom_pot_items[room_id];
      }
      if (bytes.empty()) {
        continue;  // Preserve ROM data if room not loaded and nothing parsed.
      }
    } else {
      for (const auto& pi : pot_items) {
        bytes.push_back(pi.position & 0xFF);
        bytes.push_back((pi.position >> 8) & 0xFF);
        bytes.push_back(pi.item);
      }
      bytes.push_back(0xFF);
      bytes.push_back(0xFF);
    }
    if (static_cast<int>(bytes.size()) > max_len) {
      continue;
    }
    RETURN_IF_ERROR(rom->WriteVector(item_addr, bytes));
  }
  return absl::OkStatus();
}

void Room::LoadBlocks() {
  auto rom_data = rom()->vector();

  // Read blocks length
  int blocks_count =
      (rom_data[blocks_length + 1] << 8) | rom_data[blocks_length];

  LOG_DEBUG("Room", "LoadBlocks: room_id=%d, blocks_count=%d", room_id_,
            blocks_count);

  // Avoid duplication if LoadBlocks is called multiple times.
  tile_objects_.erase(
      std::remove_if(tile_objects_.begin(), tile_objects_.end(),
                     [](const RoomObject& obj) {
                       return (obj.options() & ObjectOption::Block) !=
                              ObjectOption::Nothing;
                     }),
      tile_objects_.end());

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
      block_obj.SetRom(rom_);
      block_obj.set_options(ObjectOption::Block);

      tile_objects_.push_back(block_obj);

      LOG_DEBUG("Room", "Loaded block at (%d,%d) layer=%d", px, py, layer);
    }
  }
}

void Room::LoadPotItems() {
  if (!rom_ || !rom_->is_loaded())
    return;
  auto rom_data = rom()->vector();

  // Load pot items
  // Format per ASM analysis (bank_01.asm):
  //   - Pointer table at kRoomItemsPointers (0x01DB69)
  //   - Each room has a pointer to item data
  //   - Item data format: 3 bytes per item
  //     - 2 bytes: position word (Y_hi, X_lo encoding)
  //     - 1 byte: item type
  //   - Terminated by 0xFFFF position word

  int table_addr = kRoomItemsPointers;  // 0x01DB69

  // Read pointer for this room
  int ptr_addr = table_addr + (room_id_ * 2);
  if (ptr_addr + 1 >= static_cast<int>(rom_data.size()))
    return;

  uint16_t item_ptr = (rom_data[ptr_addr + 1] << 8) | rom_data[ptr_addr];

  // Convert to PC address (Bank 01 offset)
  int item_addr = SnesToPc(0x010000 | item_ptr);

  pot_items_.clear();

  // Read 3-byte entries until 0xFFFF terminator
  while (item_addr + 2 < static_cast<int>(rom_data.size())) {
    // Read position word (little endian)
    uint16_t position = (rom_data[item_addr + 1] << 8) | rom_data[item_addr];

    // Check for terminator
    if (position == 0xFFFF)
      break;

    // Read item type (3rd byte)
    uint8_t item_type = rom_data[item_addr + 2];

    PotItem pot_item;
    pot_item.position = position;
    pot_item.item = item_type;
    pot_items_.push_back(pot_item);

    item_addr += 3;  // Move to next entry
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
  // This data is already loaded in LoadRoomFromRom() into pits_ destination
  // struct The pit destination (where you go when you fall) is set via
  // SetPitsTarget()

  // Pits are typically represented in the layout/collision data, not as objects
  // The pits_ member already contains the target room and layer
  LOG_DEBUG("Room", "Pit destination - target=%d, target_layer=%d",
            pits_.target, pits_.target_layer);
}

// ============================================================================
// Object Limit Counting (ZScream Feature Parity)
// ============================================================================

std::map<DungeonLimit, int> Room::GetLimitedObjectCounts() const {
  auto counts = CreateLimitCounter();

  // Count sprites
  counts[DungeonLimit::Sprites] = static_cast<int>(sprites_.size());

  // Count overlords (sprites with ID > 0x40 are overlords in ALTTP)
  for (const auto& sprite : sprites_) {
    if (sprite.IsOverlord()) {
      counts[DungeonLimit::Overlords]++;
    }
  }

  // Count chests
  counts[DungeonLimit::Chests] = static_cast<int>(chests_in_room_.size());

  // Count doors (total and special)
  counts[DungeonLimit::Doors] = static_cast<int>(doors_.size());
  for (const auto& door : doors_) {
    // Special doors: shutters and key-locked doors.
    const bool is_special = [&]() -> bool {
      switch (door.type) {
        case DoorType::SmallKeyDoor:
        case DoorType::BigKeyDoor:
        case DoorType::UnopenableBigKeyDoor:
        case DoorType::DoubleSidedShutter:
        case DoorType::UnusedDoubleSidedShutter:
        case DoorType::DoubleSidedShutterLower:
        case DoorType::BottomSidedShutter:
        case DoorType::TopSidedShutter:
        case DoorType::BottomShutterLower:
        case DoorType::TopShutterLower:
        case DoorType::UnusableBottomShutter:
        case DoorType::NormalDoorOneSidedShutter:
        case DoorType::CurtainDoor:
        case DoorType::EyeWatchDoor:
          return true;
        default:
          return false;
      }
    }();
    if (is_special) {
      counts[DungeonLimit::SpecialDoors]++;
    }
  }

  // Count stairs
  counts[DungeonLimit::StairsTransition] = static_cast<int>(z3_staircases_.size());

  // Count objects with specific options
  for (const auto& obj : tile_objects_) {
    auto options = obj.options();

    // Count blocks
    if ((options & ObjectOption::Block) != ObjectOption::Nothing) {
      counts[DungeonLimit::Blocks]++;
    }

    // Count torches
    if ((options & ObjectOption::Torch) != ObjectOption::Nothing) {
      counts[DungeonLimit::Torches]++;
    }

    // Count star tiles (object IDs 0x11E and 0x11F)
    if (obj.id_ == 0x11E || obj.id_ == 0x11F) {
      counts[DungeonLimit::StarTiles]++;
    }

    // Count somaria paths (object IDs in 0xF83-0xF8F range)
    if (obj.id_ >= 0xF83 && obj.id_ <= 0xF8F) {
      counts[DungeonLimit::SomariaLine]++;
    }

    // Count staircase objects based on direction
    if ((options & ObjectOption::Stairs) != ObjectOption::Nothing) {
      // North-facing stairs: IDs 0x130-0x135
      if ((obj.id_ >= 0x130 && obj.id_ <= 0x135) ||
          obj.id_ == 0x139 || obj.id_ == 0x13A || obj.id_ == 0x13B) {
        counts[DungeonLimit::StairsNorth]++;
      }
      // South-facing stairs: IDs 0x13B-0x13D
      else if (obj.id_ >= 0x13C && obj.id_ <= 0x13F) {
        counts[DungeonLimit::StairsSouth]++;
      }
    }

    // Count general manipulable objects
    if ((options & ObjectOption::Block) != ObjectOption::Nothing ||
        (options & ObjectOption::Chest) != ObjectOption::Nothing ||
        (options & ObjectOption::Torch) != ObjectOption::Nothing) {
      counts[DungeonLimit::GeneralManipulable]++;
    }
  }

  return counts;
}

bool Room::HasExceededLimits() const {
  auto counts = GetLimitedObjectCounts();
  return yaze::zelda3::HasExceededLimits(counts);
}

std::vector<DungeonLimitInfo> Room::GetExceededLimitDetails() const {
  auto counts = GetLimitedObjectCounts();
  return GetExceededLimits(counts);
}

}  // namespace zelda3
}  // namespace yaze
