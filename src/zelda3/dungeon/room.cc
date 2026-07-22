#include "room.h"

#include <yaze.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "app/platform/sdl_compat.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "rom/write_fence.h"
#include "util/log.h"
#include "zelda3/dungeon/dungeon_block_codec.h"
#include "zelda3/dungeon/dungeon_stream_allocator.h"
#include "zelda3/dungeon/dungeon_torch_codec.h"
#include "zelda3/dungeon/editor_dungeon_state.h"
#include "zelda3/dungeon/object_drawer.h"
#include "zelda3/dungeon/palette_debug.h"
#include "zelda3/dungeon/pit_damage_table.h"
#include "zelda3/dungeon/room_layer_manager.h"
#include "zelda3/dungeon/room_object.h"
#include "zelda3/dungeon/track_collision_generator.h"
#include "zelda3/sprite/sprite.h"

namespace yaze {
namespace zelda3 {

namespace {

bool RoomUsesTrackCornerAliases(const std::vector<RoomObject>& objects) {
  return std::any_of(objects.begin(), objects.end(),
                     [](const RoomObject& obj) { return obj.id_ == 0x31; });
}

uint8_t Layer2ModeFromHeaderByte(uint8_t byte0) {
  return static_cast<uint8_t>((byte0 >> 5) & 0x07);
}

bool IsDarkRoomHeaderByte(uint8_t byte0) {
  return (byte0 & 0x01) != 0;
}

const LayerMergeType& LayerMergeFromHeaderByte(uint8_t byte0) {
  return kLayerMergeTypeList[IsDarkRoomHeaderByte(byte0)
                                 ? 8
                                 : Layer2ModeFromHeaderByte(byte0)];
}

background2 Background2FromHeaderByte(uint8_t byte0) {
  if (IsDarkRoomHeaderByte(byte0)) {
    return background2::DarkRoom;
  }
  return static_cast<background2>(Layer2ModeFromHeaderByte(byte0));
}

template <typename WriteColor>
void PopulateDungeonRenderPaletteRows(const gfx::SnesPalette& dungeon_palette,
                                      const gfx::SnesPalette* hud_palette,
                                      WriteColor write_color) {
  if (hud_palette != nullptr) {
    const size_t hud_count = std::min<size_t>(hud_palette->size(), 32);
    for (size_t i = 0; i < hud_count; ++i) {
      write_color(static_cast<int>(i), (*hud_palette)[i]);
    }
  }

  constexpr int kColorsPerRomBank = 15;
  constexpr int kIndicesPerSdlBank = 16;
  constexpr int kNumRomBanks = 6;
  constexpr int kDungeonBankStart = 2;
  for (int rom_bank = 0; rom_bank < kNumRomBanks; ++rom_bank) {
    const int sdl_bank = rom_bank + kDungeonBankStart;
    for (int color = 0; color < kColorsPerRomBank; ++color) {
      const size_t rom_index =
          static_cast<size_t>(rom_bank * kColorsPerRomBank + color);
      if (rom_index >= dungeon_palette.size()) {
        return;
      }
      const int dst_index = sdl_bank * kIndicesPerSdlBank + color + 1;
      write_color(dst_index, dungeon_palette[rom_index]);
    }
  }
}

}  // namespace

std::vector<SDL_Color> BuildDungeonRenderPalette(
    const gfx::SnesPalette& dungeon_palette,
    const gfx::SnesPalette* hud_palette) {
  std::vector<SDL_Color> colors(256, {0, 0, 0, 0});
  PopulateDungeonRenderPaletteRows(
      dungeon_palette, hud_palette,
      [&](int dst_index, const gfx::SnesColor& color) {
        if (dst_index < 0 || dst_index >= static_cast<int>(colors.size())) {
          return;
        }
        const ImVec4 rgb = color.rgb();
        colors[dst_index] = {static_cast<Uint8>(rgb.x),
                             static_cast<Uint8>(rgb.y),
                             static_cast<Uint8>(rgb.z), 255};
      });
  colors[255] = {0, 0, 0, 0};
  return colors;
}

void LoadDungeonRenderPaletteToCgram(std::span<uint16_t> cgram,
                                     const gfx::SnesPalette& dungeon_palette,
                                     const gfx::SnesPalette* hud_palette) {
  PopulateDungeonRenderPaletteRows(
      dungeon_palette, hud_palette,
      [&](int dst_index, const gfx::SnesColor& color) {
        if (dst_index < 0 || dst_index >= static_cast<int>(cgram.size())) {
          return;
        }
        cgram[dst_index] = color.snes();
      });
}

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

namespace {

struct PhysicalStreamInfo {
  int address = -1;
  int physical_end = -1;
  bool shared = false;

  int capacity() const {
    return physical_end > address ? physical_end - address : 0;
  }
};

PhysicalStreamInfo AnalyzePhysicalStream(const std::vector<int>& room_addresses,
                                         int room_id,
                                         int known_region_end = -1) {
  PhysicalStreamInfo info;
  if (room_id < 0 || room_id >= static_cast<int>(room_addresses.size())) {
    return info;
  }

  info.address = room_addresses[room_id];
  if (info.address < 0) {
    return info;
  }

  int next_address = std::numeric_limits<int>::max();
  for (int other_room_id = 0;
       other_room_id < static_cast<int>(room_addresses.size());
       ++other_room_id) {
    if (other_room_id == room_id || room_addresses[other_room_id] < 0) {
      continue;
    }
    const int other_address = room_addresses[other_room_id];
    if (other_address == info.address) {
      info.shared = true;
    } else if (other_address > info.address) {
      next_address = std::min(next_address, other_address);
    }
  }

  if (known_region_end > info.address) {
    next_address = std::min(next_address, known_region_end);
  }
  if (next_address == std::numeric_limits<int>::max()) {
    return info;
  }

  // A stream cannot safely grow across a LoROM bank boundary even when the
  // next pointer happens to live in the following physical bank. The bank end
  // alone is not a physical data boundary, so fail closed unless an actual
  // pointer or a supplied region end bounds this bank.
  constexpr int kLoRomBankSize = 0x8000;
  const int bank_end = ((info.address / kLoRomBankSize) + 1) * kLoRomBankSize;
  if (next_address > bank_end) {
    return info;
  }
  info.physical_end = next_address;
  return info;
}

absl::Status GetObjectPointerTablePc(const std::vector<uint8_t>& rom_data,
                                     int* table_pc) {
  if (table_pc == nullptr) {
    return absl::InvalidArgumentError("table_pc pointer is null");
  }
  if (kRoomObjectPointer + 2 >= static_cast<int>(rom_data.size())) {
    return absl::OutOfRangeError(
        "Object pointer table address is out of range");
  }

  const uint32_t table_snes =
      (static_cast<uint32_t>(rom_data[kRoomObjectPointer + 2]) << 16) |
      (static_cast<uint32_t>(rom_data[kRoomObjectPointer + 1]) << 8) |
      rom_data[kRoomObjectPointer];
  const int pc = static_cast<int>(SnesToPc(table_snes));
  if (pc < 0 || pc + (kNumberOfRooms * 3) > static_cast<int>(rom_data.size())) {
    return absl::OutOfRangeError("Object pointer table is out of range");
  }

  *table_pc = pc;
  return absl::OkStatus();
}

uint32_t ReadRoomObjectAddressSnes(const std::vector<uint8_t>& rom_data,
                                   int table_pc, int room_id) {
  if (room_id < 0 || room_id >= kNumberOfRooms) {
    return 0;
  }
  const int ptr_off = table_pc + (room_id * 3);
  if (ptr_off < 0 || ptr_off + 2 >= static_cast<int>(rom_data.size())) {
    return 0;
  }
  return (static_cast<uint32_t>(rom_data[ptr_off + 2]) << 16) |
         (static_cast<uint32_t>(rom_data[ptr_off + 1]) << 8) |
         rom_data[ptr_off];
}

int ReadRoomObjectAddressPc(const std::vector<uint8_t>& rom_data, int table_pc,
                            int room_id) {
  const uint32_t snes = ReadRoomObjectAddressSnes(rom_data, table_pc, room_id);
  if ((snes & 0xFFFF) < 0x8000) {
    return -1;
  }
  const int pc = static_cast<int>(SnesToPc(snes));
  return pc >= 0 && pc < static_cast<int>(rom_data.size()) ? pc : -1;
}

absl::StatusOr<PhysicalStreamInfo> GetObjectStreamInfo(
    const std::vector<uint8_t>& rom_data, int room_id) {
  if (room_id < 0 || room_id >= kNumberOfRooms) {
    return absl::OutOfRangeError("Room ID out of range");
  }
  int table_pc = 0;
  RETURN_IF_ERROR(GetObjectPointerTablePc(rom_data, &table_pc));

  std::vector<int> addresses(kNumberOfRooms, -1);
  for (int id = 0; id < kNumberOfRooms; ++id) {
    addresses[id] = ReadRoomObjectAddressPc(rom_data, table_pc, id);
  }
  const int hard_end = GetDungeonObjectDataRegionEnd(addresses[room_id]);
  PhysicalStreamInfo info = AnalyzePhysicalStream(addresses, room_id, hard_end);
  if (info.address < 0) {
    return absl::OutOfRangeError("Object stream pointer is out of range");
  }
  return info;
}

absl::Status GetSpritePointerTablePc(const std::vector<uint8_t>& rom_data,
                                     int* table_pc) {
  if (table_pc == nullptr) {
    return absl::InvalidArgumentError("table_pc pointer is null");
  }
  if (kRoomsSpritePointer + 1 >= static_cast<int>(rom_data.size())) {
    return absl::OutOfRangeError(
        "Sprite pointer table address is out of range");
  }

  int table_snes = (0x09 << 16) | (rom_data[kRoomsSpritePointer + 1] << 8) |
                   rom_data[kRoomsSpritePointer];
  int pc = SnesToPc(table_snes);
  if (pc < 0 || pc + (kNumberOfRooms * 2) > static_cast<int>(rom_data.size())) {
    return absl::OutOfRangeError("Sprite pointer table is out of range");
  }

  *table_pc = pc;
  return absl::OkStatus();
}

int ReadRoomSpriteAddressPc(const std::vector<uint8_t>& rom_data, int table_pc,
                            int room_id) {
  if (room_id < 0 || room_id >= kNumberOfRooms) {
    return -1;
  }
  const int ptr_off = table_pc + (room_id * 2);
  if (ptr_off < 0 || ptr_off + 1 >= static_cast<int>(rom_data.size())) {
    return -1;
  }

  const uint16_t pointer =
      (static_cast<uint16_t>(rom_data[ptr_off + 1]) << 8) | rom_data[ptr_off];
  if (pointer < 0x8000) {
    return -1;
  }
  const int sprite_address = static_cast<int>(SnesToPc((0x09 << 16) | pointer));
  return sprite_address >= 0 &&
                 sprite_address < static_cast<int>(rom_data.size())
             ? sprite_address
             : -1;
}

absl::StatusOr<PhysicalStreamInfo> GetSpriteStreamInfo(
    const std::vector<uint8_t>& rom_data, int room_id) {
  if (room_id < 0 || room_id >= kNumberOfRooms) {
    return absl::OutOfRangeError("Room ID out of range");
  }
  int table_pc = 0;
  RETURN_IF_ERROR(GetSpritePointerTablePc(rom_data, &table_pc));

  std::vector<int> addresses(kNumberOfRooms, -1);
  for (int id = 0; id < kNumberOfRooms; ++id) {
    addresses[id] = ReadRoomSpriteAddressPc(rom_data, table_pc, id);
  }
  const int hard_end =
      std::min(static_cast<int>(rom_data.size()), kSpritesDataEndExclusive);
  PhysicalStreamInfo info = AnalyzePhysicalStream(addresses, room_id, hard_end);
  if (info.address < 0 || info.address >= hard_end) {
    return absl::OutOfRangeError("Sprite stream pointer is out of range");
  }
  return info;
}

int MeasureSpriteStreamSize(const std::vector<uint8_t>& rom_data,
                            int sprite_address, int hard_end) {
  if (sprite_address < 0 || sprite_address >= hard_end ||
      sprite_address >= static_cast<int>(rom_data.size())) {
    return 0;
  }

  int cursor = sprite_address + 1;  // Skip SortSprites mode byte.
  while (cursor < hard_end) {
    if (rom_data[cursor] == 0xFF) {
      ++cursor;  // Include terminator.
      break;
    }
    if (cursor + 2 >= hard_end) {
      cursor = hard_end;
      break;
    }
    cursor += 3;
  }

  return std::max(0, cursor - sprite_address);
}

absl::Status RelocateDungeonStream(Rom* rom, int room_id,
                                   DungeonStreamKind expected_kind,
                                   const DungeonStreamLayout& layout,
                                   std::vector<uint8_t> encoded_stream) {
  if (layout.kind != expected_kind) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Room %d relocation layout has the wrong dungeon stream kind",
        room_id));
  }

  ASSIGN_OR_RETURN(const DungeonStreamInventory inventory,
                   InventoryDungeonStreams(*rom, layout));
  ASSIGN_OR_RETURN(
      const DungeonStreamWritePlan plan,
      PlanDungeonStreamWrites(inventory, {{static_cast<uint32_t>(room_id),
                                           std::move(encoded_stream)}}));
  return ApplyDungeonStreamWritePlan(rom, plan);
}

absl::StatusOr<bool> DungeonStreamRequiresCopyOnWrite(
    const Rom& rom, int room_id, DungeonStreamKind expected_kind,
    const DungeonStreamLayout& layout, size_t replacement_size) {
  if (layout.kind != expected_kind) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Room %d save layout has the wrong dungeon stream kind", room_id));
  }
  if (room_id < 0 || static_cast<uint32_t>(room_id) >= layout.pointer_count) {
    return absl::OutOfRangeError(
        "Room ID is outside the dungeon stream layout");
  }

  ASSIGN_OR_RETURN(const DungeonStreamInventory inventory,
                   InventoryDungeonStreams(rom, layout));
  if (!inventory.ok()) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Dungeon stream inventory has %zu issue(s); refusing an in-place "
        "save",
        inventory.issues.size()));
  }

  const auto contains_room = [room_id](const std::vector<uint32_t>& owners) {
    return std::find(owners.begin(), owners.end(),
                     static_cast<uint32_t>(room_id)) != owners.end();
  };
  for (const auto& alias : inventory.aliases) {
    if (contains_room(alias.room_ids)) {
      return true;
    }
  }
  for (const auto& overlap : inventory.overlaps) {
    if (contains_room(overlap.first_room_ids) ||
        contains_room(overlap.second_room_ids)) {
      return true;
    }
  }

  const auto& record = inventory.streams[room_id];
  const uint64_t replacement_end =
      static_cast<uint64_t>(record.data_pc) + replacement_size;
  const uint32_t bank_end = ((record.data_pc / 0x8000u) + 1u) * 0x8000u;
  const bool stays_in_declared_data =
      replacement_end <= bank_end &&
      std::any_of(inventory.layout.data_ranges.begin(),
                  inventory.layout.data_ranges.end(), [&](const auto& range) {
                    return range.begin <= record.data_pc &&
                           replacement_end <= range.end;
                  });
  return !stays_in_declared_data;
}

}  // namespace

RoomSize CalculateRoomSize(Rom* rom, int room_id) {
  RoomSize room_size{};
  if (!rom || !rom->is_loaded() || rom->size() == 0 || room_id < 0 ||
      room_id >= kNumberOfRooms) {
    return room_size;
  }

  const auto& rom_data = rom->vector();
  int table_pc = 0;
  if (!GetObjectPointerTablePc(rom_data, &table_pc).ok()) {
    return room_size;
  }
  room_size.room_size_pointer =
      ReadRoomObjectAddressSnes(rom_data, table_pc, room_id);

  auto stream_info = GetObjectStreamInfo(rom_data, room_id);
  if (!stream_info.ok() || stream_info->shared) {
    return room_size;
  }
  room_size.room_size = stream_info->capacity();
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
  room.LoadChests();
  room.LoadPotItems();
  room.LoadTorches();
  room.LoadBlocks();
  room.LoadPits();

  room.SetLoaded(true);
  room.ClearSaveDirtyState();
  room.ClearCustomCollisionDirty();
  room.ClearWaterFillDirty();
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

  const uint8_t header_byte0 = rom->data()[header_location];
  room.SetLayer2Mode(Layer2ModeFromHeaderByte(header_byte0));
  room.SetLayerMerging(LayerMergeFromHeaderByte(header_byte0));
  room.SetBg2(Background2FromHeaderByte(header_byte0));
  room.SetCollision((CollisionKey)((header_byte0 >> 2) & 0x07));
  room.SetIsLight(IsDarkRoomHeaderByte(header_byte0));
  room.SetIsDark(IsDarkRoomHeaderByte(header_byte0));

  // USDASM grounding (bank_01.asm LoadRoomHeader, e.g. $01:B61B):
  // The room header stores an 8-bit "palette set ID" (0-71 in vanilla), which
  // is later multiplied by 4 to index UnderworldPaletteSets. Do NOT truncate to
  // 6 bits: IDs 0x40-0x47 are valid and were previously corrupted by & 0x3F.
  room.SetPalette(rom->data()[header_location + 1]);
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

  int msg_addr = kMessagesIdDungeon + (room_id * 2);
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

  uint8_t b = rom->data()[hpos];

  room.SetLayer2Mode(Layer2ModeFromHeaderByte(b));
  room.SetLayerMerging(LayerMergeFromHeaderByte(b));
  room.SetIsDark(IsDarkRoomHeaderByte(b));
  hpos++;
  // Skip palette byte here - already set by SetPalette() from the primary
  // header table above (line ~329). The old SetPaletteDirect wrote to a
  // separate dead-code member; now palette_ is unified.
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

  room.ClearSaveDirtyState();
  room.ClearCustomCollisionDirty();
  room.ClearWaterFillDirty();
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

int Room::ResolveDungeonPaletteId() const {
  if (!game_data_ || !rom_)
    return 0;
  const auto& group = game_data_->palette_groups.dungeon_main;
  const int num_palettes = static_cast<int>(group.size());
  if (num_palettes == 0)
    return 0;

  int id = palette_;
  if (palette_ < game_data_->paletteset_ids.size() &&
      !game_data_->paletteset_ids[palette_].empty()) {
    const auto offset = game_data_->paletteset_ids[palette_][0];
    const auto word = rom_->ReadWord(kDungeonPalettePointerTable + offset);
    if (word.ok()) {
      id = word.value() / kDungeonPaletteBytes;
    }
  }
  if (id < 0 || id >= num_palettes)
    id = 0;
  return id;
}

void Room::LoadRoomGraphics(std::optional<uint8_t> entrance_blockset) {
  if (!game_data_) {
    LOG_DEBUG("Room", "GameData not set for room %d", room_id_);
    return;
  }

  const auto& room_gfx = game_data_->room_blockset_ids;
  const auto& sprite_gfx = game_data_->spriteset_ids;
  const uint8_t requested_main_blockset =
      entrance_blockset.value_or(render_entrance_blockset_);
  uint8_t main_blockset = 0;
  if (requested_main_blockset != 0xFF &&
      requested_main_blockset < game_data_->main_blockset_ids.size()) {
    main_blockset = requested_main_blockset;
  } else if (blockset_ < game_data_->main_blockset_ids.size()) {
    main_blockset = blockset_;
  } else {
    LOG_WARN("Room",
             "Room %d: invalid main fallback blockset %d; using main group 0",
             room_id_, blockset_);
  }
  if (requested_main_blockset != 0xFF &&
      requested_main_blockset >= game_data_->main_blockset_ids.size()) {
    LOG_WARN("Room",
             "Room %d: entrance main blockset %d out of range; using %d",
             room_id_, requested_main_blockset, main_blockset);
  }
  resolved_main_blockset_ = main_blockset;

  LOG_DEBUG("Room",
            "Room %d: room_blockset=%d, main_blockset=%d, spriteset=%d, "
            "palette=%d",
            room_id_, blockset_, main_blockset, spriteset_, palette_);

  for (int i = 0; i < 8; i++) {
    blocks_[i] = game_data_->main_blockset_ids[main_blockset][i];
    if (i >= 3 && i <= 6 && blockset_ < room_gfx.size()) {
      const uint8_t room_sheet = room_gfx[blockset_][i - 3];
      if (room_sheet != 0) {
        blocks_[i] = room_sheet;
      }
    }
  }
  if (blockset_ >= room_gfx.size()) {
    LOG_WARN("Room", "Room %d: room blockset %d out of range; skipped $0AA2",
             room_id_, blockset_);
  }

  blocks_[8] = 115 + 0;  // Static Sprites Blocksets (fairy,pot,ect...)
  blocks_[9] = 115 + 10;
  blocks_[10] = 115 + 6;
  blocks_[11] = 115 + 7;
  const size_t sprite_gfx_index = static_cast<size_t>(spriteset_) + 64;
  if (sprite_gfx_index < sprite_gfx.size()) {
    for (int i = 0; i < 4; i++) {
      blocks_[12 + i] =
          static_cast<uint8_t>(sprite_gfx[sprite_gfx_index][i] + 115);
    }
  } else {
    LOG_WARN("Room",
             "Room %d: spriteset %d out of range; clearing sprite sheets",
             room_id_, spriteset_);
    for (int i = 0; i < 4; i++) {
      blocks_[12 + i] = 0;
    }
  }  // 12-15 sprites

  LOG_DEBUG("Room", "Sheet IDs BG[0-7]: %d %d %d %d %d %d %d %d", blocks_[0],
            blocks_[1], blocks_[2], blocks_[3], blocks_[4], blocks_[5],
            blocks_[6], blocks_[7]);
}

void Room::EnsureObjectsLoaded() {
  if (objects_loaded_) {
    return;
  }
  LoadObjects();
}

void Room::EnsureSpritesLoaded() {
  if (sprites_loaded_) {
    return;
  }
  LoadSprites();
}

void Room::EnsurePotItemsLoaded() {
  if (pot_items_loaded_) {
    return;
  }
  LoadPotItems();
}

void Room::ReloadGraphics(std::optional<uint8_t> entrance_blockset) {
  if (entrance_blockset.has_value()) {
    SetRenderEntranceBlockset(*entrance_blockset);
  }
  EnsureObjectsLoaded();
  MarkGraphicsDirty();
  MarkLayoutDirty();
  MarkObjectsDirty();
  RenderRoomGraphics();
}

void Room::PrepareForRender(std::optional<uint8_t> entrance_blockset) {
  if (entrance_blockset.has_value()) {
    SetRenderEntranceBlockset(*entrance_blockset);
  }
  EnsureObjectsLoaded();

  auto& bg1_bmp = bg1_buffer_.bitmap();
  auto& bg2_bmp = bg2_buffer_.bitmap();
  if (dirty_state_.graphics || dirty_state_.objects || dirty_state_.layout ||
      dirty_state_.textures || !bg1_bmp.is_active() || bg1_bmp.width() == 0 ||
      !bg2_bmp.is_active() || bg2_bmp.width() == 0) {
    RenderRoomGraphics();
  }
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
  // active main graphics group ($0AA1) and the slot index ($0F).
  // InitializeTilesets starts $0F at 7 for destination block 0 and decrements
  // it through destination block 7, so the runtime slot is 7 - block. For UW
  // groups (< $20), runtime slots 4-7 use Right; for OW groups (>= $20), the
  // Right runtime slots are {2,3,4,7}.
  const uint8_t active_main_blockset =
      resolved_main_blockset_ != 0xFF
          ? resolved_main_blockset_
          : (render_entrance_blockset_ != 0xFF ? render_entrance_blockset_
                                               : blockset_);
  auto is_right_palette_background_slot = [&](int block) -> bool {
    if (block < 0 || block >= 8) {
      return false;
    }
    const int runtime_slot = 7 - block;
    if (active_main_blockset < 0x20) {
      return runtime_slot >= 4;
    }
    return (runtime_slot == 2 || runtime_slot == 3 || runtime_slot == 4 ||
            runtime_slot == 7);
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
  const uint64_t requested_signature = layer_mgr.CompositeStateSignature();
  if (dirty_state_.composite || !has_composite_signature_ ||
      composite_signature_ != requested_signature) {
    layer_mgr.CompositeToOutput(*this, composite_bitmap_);
    dirty_state_.composite = false;
    composite_signature_ = requested_signature;
    has_composite_signature_ = true;
  }
  PaletteDebugger::Get().SetCurrentBitmap(&composite_bitmap_);
  return composite_bitmap_;
}

void Room::RenderRoomGraphics() {
  // PERFORMANCE OPTIMIZATION: Check if room properties have changed
  bool properties_changed = false;

  // Check if graphics properties changed
  if (cached_blockset_ != blockset_ || cached_spriteset_ != spriteset_ ||
      cached_palette_ != palette_ || cached_layout_ != layout_id_ ||
      cached_floor1_graphics_ != floor1_graphics_ ||
      cached_floor2_graphics_ != floor2_graphics_) {
    cached_blockset_ = blockset_;
    cached_spriteset_ = spriteset_;
    cached_palette_ = palette_;
    cached_layout_ = layout_id_;
    cached_floor1_graphics_ = floor1_graphics_;
    cached_floor2_graphics_ = floor2_graphics_;
    dirty_state_.graphics = true;
    properties_changed = true;
  }

  // Check if effect/tags changed
  if (cached_effect_ != static_cast<uint8_t>(effect_) ||
      cached_tag1_ != tag1_ || cached_tag2_ != tag2_) {
    cached_effect_ = static_cast<uint8_t>(effect_);
    cached_tag1_ = tag1_;
    cached_tag2_ = tag2_;
    dirty_state_.objects = true;
    properties_changed = true;
  }

  // If nothing changed and textures exist, skip rendering
  if (!properties_changed && !dirty_state_.graphics && !dirty_state_.objects &&
      !dirty_state_.layout && !dirty_state_.textures) {
    auto& bg1_bmp = bg1_buffer_.bitmap();
    auto& bg2_bmp = bg2_buffer_.bitmap();
    if (bg1_bmp.is_active() && bg1_bmp.width() > 0 && bg2_bmp.is_active() &&
        bg2_bmp.width() > 0) {
      LOG_DEBUG("[RenderRoomGraphics]",
                "Room %d: No changes detected, skipping render", room_id_);
      return;
    }
  }

  LOG_DEBUG("[RenderRoomGraphics]",
            "Room %d: Rendering graphics (dirty_flags: g=%d o=%d l=%d t=%d)",
            room_id_, dirty_state_.graphics, dirty_state_.objects,
            dirty_state_.layout, dirty_state_.textures);

  // Capture dirty state BEFORE clearing flags (needed for floor/bg draw logic)
  bool was_graphics_dirty = dirty_state_.graphics;
  bool was_layout_dirty = dirty_state_.layout;

  // STEP 0: Load graphics if needed
  if (dirty_state_.graphics) {
    // Ensure blocks_[] array is properly initialized before copying graphics
    // LoadRoomGraphics sets up which sheets go into which blocks
    LoadRoomGraphics();
    CopyRoomGraphicsToBuffer();
    dirty_state_.graphics = false;
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
    bg1_buffer_.DrawFloor(rom()->vector(), kTileAddress, kTileAddressFloor,
                          floor1_graphics_);
    bg2_buffer_.DrawFloor(rom()->vector(), kTileAddress, kTileAddressFloor,
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
  // Layout objects (walls, corners) are drawn after floor so they appear over it.
  // USDASM order (bank_01.asm LoadAndBuildRoom): floors, layout, primary object
  // stream, BG2 overlay stream (post-0xFFFF), BG1 overlay stream, then blocks/
  // torches. `RenderObjectsToBackground` runs three object-stream passes; layout
  // is emitted here before object buffers. See dungeon-object-rendering-spec.md.
  if (was_layout_dirty || need_floor_draw) {
    LoadLayoutTilesToBuffer();
    dirty_state_.layout = false;
  }

  // Get and apply palette BEFORE rendering objects (so objects use correct colors)
  if (!game_data_)
    return;
  auto& dungeon_pal_group = game_data_->palette_groups.dungeon_main;
  if (dungeon_pal_group.empty())
    return;

  const int palette_id = ResolveDungeonPaletteId();
  auto bg1_palette = dungeon_pal_group[palette_id];

  object_bg1_buffer_.EnsureBitmapInitialized();
  object_bg2_buffer_.EnsureBitmapInitialized();

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

  if (bg1_palette.size() > 0) {
    std::optional<gfx::SnesPalette> hud_palette_storage;
    const gfx::SnesPalette* hud_palette = nullptr;
    if (!game_data_->palette_groups.hud.empty()) {
      hud_palette_storage = game_data_->palette_groups.hud.palette_ref(0);
      hud_palette = &*hud_palette_storage;
    }

    // Apply dungeon palette in a layout that mirrors SNES CGRAM directly.
    //
    // SNES CGRAM layout for dungeons:
    //   Rows 0-1 : HUD palette
    //   Rows 2-7 : Dungeon main, 6 banks × 15 colors = 90 colors
    //              (`PaletteLoad_UnderworldSet` copies starting at color $21)
    //
    // SDL palette (256 indices) mirrors CGRAM rows 1:1:
    //   SDL indices [bank*16 .. bank*16+15] for bank = CGRAM row 0-7.
    //   Slot 0 of each bank is still transparent to the tile renderer because
    //   source pixel value 0 is skipped, but rows 0-1 must still be populated
    //   with the HUD palette because vanilla floor and ceiling tilewords do use
    //   palette rows 0 and 1.
    //
    // Drawing formula (see ObjectDrawer): final_color = pixel + (pal * 16).
    // Where pal is the 3-bit tile palette field (0-7) and pixel is 1-15.
    const auto render_palette =
        BuildDungeonRenderPalette(bg1_palette, hud_palette);

    // Store current palette state for pixel inspector / issue report debugging.
    PaletteDebugger::Get().SetCurrentPalette(bg1_palette);
    PaletteDebugger::Get().SetCurrentRenderPalette(render_palette);
    PaletteDebugger::Get().SetCurrentBitmap(&bg1_bmp);

    auto set_dungeon_palette = [&](gfx::Bitmap& bmp) {
      bmp.SetPalette(render_palette);
      if (bmp.surface()) {
        // Set color key to 255 for proper alpha blending (undrawn areas)
        SDL_SetColorKey(bmp.surface(), SDL_TRUE, 255);
        SDL_SetSurfaceBlendMode(bmp.surface(), SDL_BLENDMODE_BLEND);
      }
    };

    set_dungeon_palette(bg1_bmp);
    set_dungeon_palette(bg2_bmp);
    set_dungeon_palette(object_bg1_buffer_.bitmap());
    set_dungeon_palette(object_bg2_buffer_.bitmap());

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
    // NOTE: RoomLayerManager::CompositeToOutput() now handles translucent
    // blending with proper SNES color math. These SDL alpha settings are a
    // legacy fallback for direct SDL rendering paths. Consolidation would
    // remove this in favor of RoomLayerManager exclusively.
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

  auto release_texture = [](gfx::Bitmap* bitmap) {
    if (bitmap->texture()) {
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::DESTROY, bitmap);
    }
  };

  release_texture(&bg1_bmp);
  release_texture(&bg2_bmp);
  release_texture(&object_bg1_buffer_.bitmap());
  release_texture(&object_bg2_buffer_.bitmap());

  dirty_state_.textures = false;

  // IMPORTANT: Mark composite as dirty after any render work
  // This ensures GetCompositeBitmap() regenerates the merged output
  dirty_state_.composite = true;

  // REMOVED: Don't process texture queue here - let it be batched!
  // Processing happens once per frame in DrawDungeonCanvas()
  // This dramatically improves performance when multiple rooms are open
  // gfx::Arena::Get().ProcessTextureQueue(nullptr);  // OLD: Caused slowdown!
  LOG_DEBUG("[RenderRoomGraphics]",
            "Texture commands queued for batch processing");
}

void Room::LoadLayoutTilesToBuffer() {
  LOG_DEBUG("Room", "LoadLayoutTilesToBuffer for room %d, layout=%d", room_id_,
            layout_id_);

  if (!rom_ || !rom_->is_loaded()) {
    LOG_DEBUG("Room", "ROM not loaded, aborting");
    return;
  }

  // Rebuild only layout-owned reveal requests. Room-object masks share this
  // raw BG1 target and remain valid when just the layout is rerendered.
  bg1_buffer_.ClearBG1RevealMask(gfx::BG1RevealMaskSource::kBG2Layout);

  // Load layout tiles from ROM if not already loaded
  layout_.SetRom(rom_);
  auto layout_status = layout_.LoadLayout(layout_id_);
  if (!layout_status.ok()) {
    LOG_DEBUG("Room", "Failed to load layout %d: %s", layout_id_,
              layout_status.message().data());
    return;
  }

  const auto& layout_objects = layout_.GetObjects();
  LOG_DEBUG("Room", "Layout %d has %zu objects", layout_id_,
            layout_objects.size());
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
  auto& dungeon_pal_group = game_data_->palette_groups.dungeon_main;
  if (dungeon_pal_group.empty())
    return;

  const int palette_id = ResolveDungeonPaletteId();
  auto room_palette = dungeon_pal_group[palette_id];
  gfx::PaletteGroup palette_group;
  palette_group.AddPalette(room_palette);
  // Palette chunking follows direct CGRAM row mirroring: tile palette bits
  // select SDL bank rows 0-7, and dungeon colors live in rows 2-7 with index 0
  // transparent within each bank. See the completed palette-fix plan in
  // docs/internal/archive/completed_features/dungeon-palette-fix-plan-2025-12.md.

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

  if (!dirty_state_.objects && !dirty_state_.graphics && bitmaps_exist) {
    LOG_DEBUG("[RenderObjectsToBackground]",
              "Room %d: Objects not dirty, skipping render", room_id_);
    return;
  }

  // Handle rendering based on mode (currently using emulator-based rendering)
  // Emulator or Hybrid mode (use ObjectDrawer)
  LOG_DEBUG("[RenderObjectsToBackground]",
            "Room %d: Emulator rendering objects", room_id_);
  // Get palette group for object rendering (same lookup as other render paths).
  if (!game_data_)
    return;
  auto& dungeon_pal_group = game_data_->palette_groups.dungeon_main;
  if (dungeon_pal_group.empty())
    return;

  const int palette_id = ResolveDungeonPaletteId();
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
  drawer.SetAllowTrackCornerAliases(RoomUsesTrackCornerAliases(tile_objects_));
  drawer.SetBG1RevealMaskSource(gfx::BG1RevealMaskSource::kBG2Objects);
  // NOTE: BothBG routines (ceiling corners, merged stairs, prison cells) are
  // handled by DrawRoutineRegistry's draws_to_both_bgs flag. The room object
  // stream is split here as primary -> BG2 overlay -> BG1 overlay, while the
  // layout pass is rendered separately by RoomLayout::Draw.

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

  // IMPORTANT: Clear coverage buffers when clearing object buffers.
  // Coverage distinguishes "no draw" vs "drew transparent", so stale values
  // can cause objects to incorrectly clear the layout.
  object_bg1_buffer_.ClearCoverageBuffer();
  object_bg2_buffer_.ClearCoverageBuffer();

  // Room-object masks target both raw BG1 stacks. Clear only their source bit
  // so layout-owned reveals survive an object-only rerender.
  object_bg1_buffer_.ClearBG1RevealMask(gfx::BG1RevealMaskSource::kBG2Objects);
  bg1_buffer_.ClearBG1RevealMask(gfx::BG1RevealMaskSource::kBG2Objects);

  // Log stream distribution for this room.
  // USDASM order is: main list -> BG2 overlay list -> BG1 overlay list.
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
      "Room %03X Object Stream Summary: Main=%d, BG2Overlay=%d, BG1Overlay=%d",
      room_id_, layer0_count, layer1_count, layer2_count);

  // Render room-object streams in USDASM order.
  // - List index 0: primary object list -> BG1 object buffer (upper tilemap)
  // - List index 1: BG2 overlay list -> BG2 object buffer
  // - List index 2: BG1 overlay list -> BG1 object buffer (BG3 enum; same draw
  //   path as BG1 in ObjectDrawer for non-BothBG objects)
  // `tile_objects_[].layer_` holds the list index (0/1/2) for save/load, not
  // the buffer name. Map with MapRoomObjectListIndexToDrawLayer before drawing.
  // BothBG routines still fan out to both buffers via DrawRoutineRegistry.
  // Pass bg1_buffer_ as the second raw BG1 target. BG2 room objects record
  // deferred reveal bits on both layout and object targets without mutating
  // either bitmap.
  //
  // Three DrawObjectList passes match USDASM list order; the shared chest/
  // big-key-lock event index continues across passes (reset only on the first
  // non-empty pass).
  std::vector<std::vector<RoomObject>> by_list(3);
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

    uint8_t list_index = obj.GetLayerValue();
    if (list_index > 2) {
      list_index = 2;
    }
    RoomObject render_obj = obj;
    render_obj.layer_ = MapRoomObjectListIndexToDrawLayer(list_index);
    by_list[list_index].push_back(std::move(render_obj));
  }

  absl::Status status = absl::OkStatus();
  bool reset_room_events_for_next_chunk = true;
  for (int pass = 0; pass < 3; ++pass) {
    if (by_list[pass].empty()) {
      continue;
    }
    auto chunk_status = drawer.DrawObjectList(
        by_list[pass], object_bg1_buffer_, object_bg2_buffer_, palette_group,
        dungeon_state_.get(), &bg1_buffer_, reset_room_events_for_next_chunk);
    reset_room_events_for_next_chunk = false;
    if (!chunk_status.ok() && status.ok()) {
      status = chunk_status;
    }
  }

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
      // SpecialUnderworldObjects bit 13 chooses the draw tilemap. Bit 14 is an
      // independent behavior/pit selector retained in block metadata and must
      // not affect rendering.
      (void)drawer.DrawRoomDrawObjectData2x2(
          static_cast<uint16_t>(obj.id_), obj.x_, obj.y_, obj.layer_,
          kRoomDrawObj_PushableBlock, object_bg1_buffer_, object_bg2_buffer_);
      continue;
    }
    if ((obj.options() & ObjectOption::Torch) != ObjectOption::Nothing) {
      const uint16_t off =
          obj.lit_ ? kRoomDrawObj_TorchLit : kRoomDrawObj_TorchUnlit;
      // RoomDraw_LightableTorch retains bit 13 in its masked tilemap offset,
      // so the stored draw layer selects upper/BG1 or lower/BG2. Reserved bit
      // 14 and the lit bit do not affect the draw target.
      (void)drawer.DrawRoomDrawObjectData2x2(
          static_cast<uint16_t>(obj.id_), obj.x_, obj.y_, obj.layer_, off,
          object_bg1_buffer_, object_bg2_buffer_);
      continue;
    }
  }

  if (!status.ok()) {
    LOG_WARN(
        "[RenderObjectsToBackground]",
        "Room %03X: ObjectDrawer failed: %s (objects left dirty for retry)",
        room_id_,
        std::string(status.message().data(), status.message().size()).c_str());
    // Do not scribble placeholder rectangles into layout buffers; fix the
    // underlying draw path or ROM state instead.
    dirty_state_.objects = true;
  } else {
    // Mark objects as clean after successful render
    dirty_state_.objects = false;
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
  int object_pointer = (rom_data[kRoomObjectPointer + 2] << 16) +
                       (rom_data[kRoomObjectPointer + 1] << 8) +
                       (rom_data[kRoomObjectPointer]);
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

    layout_id_ =
        static_cast<uint8_t>((rom_data[objects_location + 1] >> 2) & 0x07);
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
  objects_loaded_ = true;
  MarkGraphicsDirty();
  MarkLayoutDirty();
  MarkObjectsDirty();
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

    // ASM Marker: 0xFF 0xFF - End of object list (next list in USDASM order).
    // Stored in RoomObject::layer_ as list index for EncodeObjects():
    //   0 = primary list (drawn to BG1/upper object buffer by default)
    //   1 = BG2 overlay list
    //   2 = BG1 overlay list (ObjectDrawer uses BG3 enum; still BG1 object path)
    if (b1 == 0xFF && b2 == 0xFF) {
      pos += 2;  // Jump to next layer
      layer++;
      LOG_DEBUG(
          "Room", "Room %03X: Object list transition to index %d (%s)",
          room_id_, layer,
          layer == 1 ? "BG2 overlay" : (layer == 2 ? "BG1 overlay" : "END"));
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

      LOG_DEBUG("Room", "Room %03X: Object 0x%03X at (%d,%d) stream=%d (%s)",
                room_id_, r.id_, r.x_, r.y_, layer,
                layer == 0 ? "Primary"
                           : (layer == 1 ? "BG2 overlay" : "BG1 overlay"));

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

  // Organize objects by ROM object-stream index (0=primary, 1=BG2 overlay,
  // 2=BG1 overlay), stored in RoomObject::layer_ / GetLayerValue().
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

  // Object stream format (USDASM bank_01.asm LoadAndBuildRoom / RoomDraw_DrawAllObjects):
  // - List index 0 (primary) terminated by $FFFF
  // - List index 1 (BG2 overlay) terminated by $FFFF
  // - List index 2 (BG1 overlay) ends with door marker $FFF0 (bytes F0 FF), then
  //   2-byte door entries, and finally $FFFF which terminates both the door
  //   list and the third object list.
  //
  // NOTE: We always emit the door marker and a terminator, even if there are
  // zero doors, because vanilla room data does so as well.

  // Encode list index 0 (primary)
  for (const auto& obj : layer0_objects) {
    auto encoded = obj.EncodeObjectToBytes();
    bytes.push_back(encoded.b1);
    bytes.push_back(encoded.b2);
    bytes.push_back(encoded.b3);
  }
  bytes.push_back(0xFF);
  bytes.push_back(0xFF);

  // Encode list index 1 (BG2 overlay)
  for (const auto& obj : layer1_objects) {
    auto encoded = obj.EncodeObjectToBytes();
    bytes.push_back(encoded.b1);
    bytes.push_back(encoded.b2);
    bytes.push_back(encoded.b3);
  }
  bytes.push_back(0xFF);
  bytes.push_back(0xFF);

  // Encode list index 2 (BG1 overlay)
  for (const auto& obj : layer2_objects) {
    auto encoded = obj.EncodeObjectToBytes();
    bytes.push_back(encoded.b1);
    bytes.push_back(encoded.b2);
    bytes.push_back(encoded.b3);
  }

  // ASM marker 0xF0 0xFF - start of door list (RoomDraw_DrawAllObjects checks
  // for word $FFF0).
  bytes.push_back(0xF0);
  bytes.push_back(0xFF);
  for (const auto& door : doors_) {
    auto [b1, b2] = door.EncodeBytes();
    bytes.push_back(b1);
    bytes.push_back(b2);
  }

  // Door list terminator (word $FFFF). This is also the list-2 terminator.
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

    // Key drops are stored as hidden marker sprites immediately after the
    // sprite that owns the drop. Keep these bytes in sync with LoadSprites().
    if (sprite.key_drop() == 1) {
      bytes.insert(bytes.end(), {0xFE, 0x00, 0xE4});
    } else if (sprite.key_drop() == 2) {
      bytes.insert(bytes.end(), {0xFD, 0x00, 0xE4});
    }
  }

  // Terminator
  bytes.push_back(0xFF);

  return bytes;
}

int FindMaxUsedSpriteAddress(Rom* rom) {
  if (!rom || !rom->is_loaded()) {
    return kSpritesDataEndExclusive;
  }

  const auto& rom_data = rom->vector();
  int sprite_pointer = 0;
  if (!GetSpritePointerTablePc(rom_data, &sprite_pointer).ok()) {
    return kSpritesDataEndExclusive;
  }

  const int hard_end =
      std::min(static_cast<int>(rom_data.size()), kSpritesDataEndExclusive);
  if (hard_end <= 0) {
    return kSpritesDataEndExclusive;
  }

  int max_used = std::min(hard_end, kSpritesData);
  std::unordered_set<int> visited_addresses;
  for (int room_id = 0; room_id < kNumberOfRooms; ++room_id) {
    int sprite_address =
        ReadRoomSpriteAddressPc(rom_data, sprite_pointer, room_id);
    if (sprite_address < kSpritesData || sprite_address >= hard_end) {
      continue;
    }
    if (!visited_addresses.insert(sprite_address).second) {
      continue;
    }

    int stream_size =
        MeasureSpriteStreamSize(rom_data, sprite_address, hard_end);
    int stream_end = sprite_address + stream_size;
    if (stream_end > max_used) {
      max_used = stream_end;
    }
  }

  return max_used;
}

absl::Status RelocateSpriteData(Rom* rom, int room_id,
                                const std::vector<uint8_t>& encoded_bytes) {
  if (!rom || !rom->is_loaded()) {
    return absl::InvalidArgumentError("ROM not loaded");
  }
  if (room_id < 0 || room_id >= kNumberOfRooms) {
    return absl::OutOfRangeError("Room ID out of range");
  }
  if (encoded_bytes.empty() || encoded_bytes.back() != 0xFF ||
      (encoded_bytes.size() % 3) != 1) {
    return absl::InvalidArgumentError(
        "Encoded sprite payload must be N*3 bytes plus 0xFF terminator");
  }

  const auto& rom_data = rom->vector();
  int sprite_pointer = 0;
  RETURN_IF_ERROR(GetSpritePointerTablePc(rom_data, &sprite_pointer));

  int old_sprite_address =
      ReadRoomSpriteAddressPc(rom_data, sprite_pointer, room_id);
  if (old_sprite_address < 0 ||
      old_sprite_address >= static_cast<int>(rom_data.size())) {
    return absl::OutOfRangeError("Sprite address out of range");
  }

  const uint8_t sort_mode = rom_data[old_sprite_address];

  const int write_pos = FindMaxUsedSpriteAddress(rom);
  const size_t required_size = 1u + encoded_bytes.size();
  if (write_pos < kSpritesData ||
      static_cast<size_t>(write_pos) + required_size >
          static_cast<size_t>(kSpritesDataEndExclusive)) {
    return absl::ResourceExhaustedError(absl::StrFormat(
        "Not enough sprite data space. Need %d bytes at 0x%06X, "
        "region ends at 0x%06X",
        static_cast<int>(required_size), write_pos, kSpritesDataEndExclusive));
  }
  if (static_cast<size_t>(write_pos) + required_size > rom_data.size()) {
    const int required_end = write_pos + static_cast<int>(required_size);
    return absl::OutOfRangeError(
        absl::StrFormat("ROM too small for sprite relocation write (need "
                        "end=0x%06X, size=0x%06X)",
                        required_end, static_cast<int>(rom_data.size())));
  }

  std::vector<uint8_t> relocated;
  relocated.reserve(required_size);
  relocated.push_back(sort_mode);
  relocated.insert(relocated.end(), encoded_bytes.begin(), encoded_bytes.end());
  RETURN_IF_ERROR(rom->WriteVector(write_pos, std::move(relocated)));

  const uint32_t snes_addr = PcToSnes(write_pos);
  const int ptr_off = sprite_pointer + (room_id * 2);
  RETURN_IF_ERROR(rom->WriteByte(ptr_off, snes_addr & 0xFF));
  RETURN_IF_ERROR(rom->WriteByte(ptr_off + 1, (snes_addr >> 8) & 0xFF));

  return absl::OkStatus();
}

absl::Status Room::SaveObjects(const DungeonStreamLayout* layout) {
  if (rom_ == nullptr) {
    return absl::InvalidArgumentError("ROM pointer is null");
  }
  if (!object_stream_dirty()) {
    return absl::OkStatus();
  }

  const auto& rom_data = rom()->vector();
  ASSIGN_OR_RETURN(const PhysicalStreamInfo stream_info,
                   GetObjectStreamInfo(rom_data, room_id_));
  const auto encoded_bytes = EncodeObjects();
  bool requires_copy_on_write = false;
  if (layout != nullptr) {
    ASSIGN_OR_RETURN(requires_copy_on_write,
                     DungeonStreamRequiresCopyOnWrite(
                         *rom_, room_id_, DungeonStreamKind::kObject, *layout,
                         encoded_bytes.size() + 2u));
  }
  const auto relocate = [&]() -> absl::Status {
    if (stream_info.address + 2 > static_cast<int>(rom_data.size())) {
      return absl::OutOfRangeError("Object stream header is out of range");
    }
    std::vector<uint8_t> replacement = {rom_data[stream_info.address],
                                        rom_data[stream_info.address + 1]};
    replacement.insert(replacement.end(), encoded_bytes.begin(),
                       encoded_bytes.end());
    RETURN_IF_ERROR(RelocateDungeonStream(rom_, room_id_,
                                          DungeonStreamKind::kObject, *layout,
                                          std::move(replacement)));
    ClearObjectStreamDirty();
    return absl::OkStatus();
  };
  if (stream_info.shared || requires_copy_on_write) {
    if (layout != nullptr) {
      return relocate();
    }
    return absl::FailedPreconditionError(absl::StrFormat(
        "Room %d object stream at PC 0x%06X is shared; repacking is required",
        room_id_, stream_info.address));
  }
  if (stream_info.capacity() <= 2) {
    if (layout != nullptr) {
      return relocate();
    }
    return absl::FailedPreconditionError(absl::StrFormat(
        "Room %d object stream has no safe physical boundary", room_id_));
  }

  // Skip graphics/layout header (2 bytes)
  const int write_pos = stream_info.address + 2;

  // Encode all objects
  const int available_payload_size = stream_info.capacity() - 2;

  // Validate against the nearest greater physical pointer, not the next room
  // ID. Pointer tables are not ordered by room ID in vanilla or expanded ROMs.
  if (encoded_bytes.size() > static_cast<size_t>(available_payload_size)) {
    if (layout != nullptr) {
      return relocate();
    }
    return absl::ResourceExhaustedError(absl::StrFormat(
        "Room %d object data too large! Size: %d, Available: %d", room_id_,
        static_cast<int>(encoded_bytes.size()), available_payload_size));
  }

  const int door_list_offset = static_cast<int>(encoded_bytes.size()) -
                               static_cast<int>(doors_.size()) * 2 - 2;
  if (door_list_offset < 0) {
    return absl::FailedPreconditionError("Invalid encoded door list offset");
  }
  const int door_pointer_slot = kDoorPointers + (room_id_ * 3);
  if (door_pointer_slot < 0 ||
      door_pointer_slot + 2 >= static_cast<int>(rom_data.size())) {
    return absl::OutOfRangeError("Door pointer slot is out of range");
  }
  const int door_pointer_pc = write_pos + door_list_offset;

  // Write encoded bytes to ROM (includes 0xF0 0xFF + door list)
  RETURN_IF_ERROR(rom_->WriteVector(write_pos, encoded_bytes));

  // Write door pointer: first byte after 0xF0 0xFF (per ZScreamDungeon Save.cs)
  RETURN_IF_ERROR(rom_->WriteLong(
      door_pointer_slot, static_cast<uint32_t>(PcToSnes(door_pointer_pc))));

  ClearObjectStreamDirty();

  return absl::OkStatus();
}

absl::Status Room::SaveObjectStreamHeader(const DungeonStreamLayout* layout) {
  if (rom_ == nullptr) {
    return absl::InvalidArgumentError("ROM pointer is null");
  }
  if (!object_stream_header_dirty()) {
    return absl::OkStatus();
  }
  if (floor1_graphics_ > 0x0F || floor2_graphics_ > 0x0F) {
    return absl::InvalidArgumentError(
        "Dungeon floor graphics values must be in range 0..15");
  }
  if (layout_id_ > 0x07) {
    return absl::InvalidArgumentError(
        "Dungeon layout ID must be in range 0..7");
  }

  const auto& rom_data = rom_->vector();
  ASSIGN_OR_RETURN(const PhysicalStreamInfo stream_info,
                   GetObjectStreamInfo(rom_data, room_id_));
  if (stream_info.address < 0 ||
      stream_info.address + 1 >= static_cast<int>(rom_data.size())) {
    return absl::OutOfRangeError("Object stream header is out of range");
  }

  const uint8_t dirty_mask = save_dirty_state_.object_stream_header;
  auto patch_header = [&](std::vector<uint8_t>* stream) -> absl::Status {
    if (stream == nullptr || stream->size() < 2) {
      return absl::DataLossError(
          "Object stream is missing its two-byte header");
    }
    if ((dirty_mask & kObjectHeaderFloor1Dirty) != 0) {
      (*stream)[0] = static_cast<uint8_t>(((*stream)[0] & 0xF0) |
                                          (floor1_graphics_ & 0x0F));
    }
    if ((dirty_mask & kObjectHeaderFloor2Dirty) != 0) {
      (*stream)[0] =
          static_cast<uint8_t>(((*stream)[0] & 0x0F) | (floor2_graphics_ << 4));
    }
    if ((dirty_mask & kObjectHeaderLayoutDirty) != 0) {
      (*stream)[1] = static_cast<uint8_t>(((*stream)[1] & 0xE3) |
                                          ((layout_id_ & 0x07) << 2));
    }
    return absl::OkStatus();
  };

  bool requires_copy_on_write = stream_info.shared;
  std::vector<uint8_t> replacement;
  if (layout != nullptr) {
    if (layout->kind != DungeonStreamKind::kObject) {
      return absl::InvalidArgumentError(
          "Object-stream header save requires an object stream layout");
    }
    ASSIGN_OR_RETURN(const DungeonStreamInventory inventory,
                     InventoryDungeonStreams(*rom_, *layout));
    if (!inventory.ok()) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Dungeon stream inventory has %zu issue(s); refusing object "
          "header save",
          inventory.issues.size()));
    }
    if (room_id_ < 0 ||
        static_cast<size_t>(room_id_) >= inventory.streams.size()) {
      return absl::OutOfRangeError(
          "Room ID is outside the dungeon stream layout");
    }
    replacement = inventory.streams[room_id_].encoded_stream;
    bool layout_requires_copy_on_write = false;
    ASSIGN_OR_RETURN(layout_requires_copy_on_write,
                     DungeonStreamRequiresCopyOnWrite(
                         *rom_, room_id_, DungeonStreamKind::kObject, *layout,
                         replacement.size()));
    requires_copy_on_write =
        requires_copy_on_write || layout_requires_copy_on_write;
  }

  if (requires_copy_on_write) {
    if (layout == nullptr) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Room %d object stream at PC 0x%06X is shared; a copy-on-write "
          "manifest is required to save its header",
          room_id_, stream_info.address));
    }
    RETURN_IF_ERROR(patch_header(&replacement));
    RETURN_IF_ERROR(RelocateDungeonStream(rom_, room_id_,
                                          DungeonStreamKind::kObject, *layout,
                                          std::move(replacement)));
    ClearObjectStreamHeaderDirty();
    return absl::OkStatus();
  }

  std::vector<uint8_t> header = {rom_data[stream_info.address],
                                 rom_data[stream_info.address + 1]};
  RETURN_IF_ERROR(patch_header(&header));
  RETURN_IF_ERROR(rom_->WriteVector(stream_info.address, std::move(header)));
  ClearObjectStreamHeaderDirty();
  return absl::OkStatus();
}

absl::Status Room::SaveSprites(const DungeonStreamLayout* layout) {
  if (rom_ == nullptr) {
    return absl::InvalidArgumentError("ROM pointer is null");
  }
  if (!sprites_dirty()) {
    return absl::OkStatus();
  }

  const auto& rom_data = rom()->vector();
  if (room_id_ < 0 || room_id_ >= kNumberOfRooms) {
    return absl::OutOfRangeError("Room ID out of range");
  }

  ASSIGN_OR_RETURN(const PhysicalStreamInfo stream_info,
                   GetSpriteStreamInfo(rom_data, room_id_));
  const auto encoded_bytes = EncodeSprites();
  bool requires_copy_on_write = false;
  if (layout != nullptr) {
    ASSIGN_OR_RETURN(requires_copy_on_write,
                     DungeonStreamRequiresCopyOnWrite(
                         *rom_, room_id_, DungeonStreamKind::kSprite, *layout,
                         encoded_bytes.size() + 1u));
  }
  const auto relocate = [&]() -> absl::Status {
    std::vector<uint8_t> replacement = {rom_data[stream_info.address]};
    replacement.insert(replacement.end(), encoded_bytes.begin(),
                       encoded_bytes.end());
    RETURN_IF_ERROR(RelocateDungeonStream(rom_, room_id_,
                                          DungeonStreamKind::kSprite, *layout,
                                          std::move(replacement)));
    ClearSpritesDirty();
    return absl::OkStatus();
  };
  if (stream_info.shared || requires_copy_on_write) {
    if (layout != nullptr) {
      return relocate();
    }
    return absl::FailedPreconditionError(absl::StrFormat(
        "Room %d sprite stream at PC 0x%06X is shared; repacking is required",
        room_id_, stream_info.address));
  }
  if (stream_info.capacity() <= 1) {
    if (layout != nullptr) {
      return relocate();
    }
    return absl::FailedPreconditionError(absl::StrFormat(
        "Room %d sprite stream has no safe physical boundary", room_id_));
  }

  const int available_payload_size = stream_info.capacity() - 1;
  const int payload_address = stream_info.address + 1;
  if (payload_address < 0 ||
      payload_address >= static_cast<int>(rom_->size())) {
    return absl::OutOfRangeError(absl::StrFormat(
        "Room %d has invalid sprite payload address", room_id_));
  }

  if (static_cast<int>(encoded_bytes.size()) > available_payload_size) {
    if (layout != nullptr) {
      return relocate();
    }
    return absl::ResourceExhaustedError(absl::StrFormat(
        "Room %d sprite data too large! Size: %d, Available: %d; repacking "
        "is required",
        room_id_, static_cast<int>(encoded_bytes.size()),
        available_payload_size));
  }

  RETURN_IF_ERROR(rom_->WriteVector(payload_address, encoded_bytes));
  ClearSpritesDirty();
  return absl::OkStatus();
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

  // Build 14-byte header to match LoadRoomHeaderFromRom layout. The high
  // three bits are the BG2/layer mode; bit 0 is the dark-room flag. DarkRoom
  // is an editor enum value, not a raw high-bit value.
  uint8_t layer2_mode_for_save = layer2_mode_ & 0x07;
  if (bg2() != background2::DarkRoom) {
    layer2_mode_for_save = static_cast<uint8_t>(bg2()) & 0x07;
  }
  const bool dark_room =
      IsLight() || is_dark_ || bg2() == background2::DarkRoom;
  uint8_t byte0 = static_cast<uint8_t>(
      (layer2_mode_for_save << 5) |
      ((static_cast<uint8_t>(collision()) & 0x07) << 2) |
      (rom_data[header_location] & 0x02) | (dark_room ? 1 : 0));
  // Preserve the full palette set ID byte (USDASM LoadRoomHeader uses 8-bit).
  uint8_t byte1 = palette_;
  // Byte 7 stores the pit target layer in bits 0-1 followed by the first
  // three staircase target layers in consecutive two-bit fields.
  uint8_t byte7 =
      (pits_.target_layer & 0x03) | ((staircase_plane(0) & 0x03) << 2) |
      ((staircase_plane(1) & 0x03) << 4) | ((staircase_plane(2) & 0x03) << 6);
  const uint8_t byte8 = static_cast<uint8_t>(
      (rom_data[header_location + 8] & 0xFC) | (staircase_plane(3) & 0x03));

  RETURN_IF_ERROR(rom_->WriteByte(header_location + 0, byte0));
  RETURN_IF_ERROR(rom_->WriteByte(header_location + 1, byte1));
  RETURN_IF_ERROR(rom_->WriteByte(header_location + 2, blockset_));
  RETURN_IF_ERROR(rom_->WriteByte(header_location + 3, spriteset_));
  RETURN_IF_ERROR(
      rom_->WriteByte(header_location + 4, static_cast<uint8_t>(effect())));
  RETURN_IF_ERROR(
      rom_->WriteByte(header_location + 5, static_cast<uint8_t>(tag1())));
  RETURN_IF_ERROR(
      rom_->WriteByte(header_location + 6, static_cast<uint8_t>(tag2())));
  RETURN_IF_ERROR(rom_->WriteByte(header_location + 7, byte7));
  RETURN_IF_ERROR(rom_->WriteByte(header_location + 8, byte8));
  RETURN_IF_ERROR(rom_->WriteByte(header_location + 9, holewarp_));
  RETURN_IF_ERROR(rom_->WriteByte(header_location + 10, staircase_room(0)));
  RETURN_IF_ERROR(rom_->WriteByte(header_location + 11, staircase_room(1)));
  RETURN_IF_ERROR(rom_->WriteByte(header_location + 12, staircase_room(2)));
  RETURN_IF_ERROR(rom_->WriteByte(header_location + 13, staircase_room(3)));

  int msg_addr = kMessagesIdDungeon + (room_id_ * 2);
  if (msg_addr < 0 || msg_addr + 1 >= static_cast<int>(rom_data.size())) {
    return absl::OutOfRangeError("Message ID address out of range");
  }
  RETURN_IF_ERROR(rom_->WriteWord(msg_addr, message_id_));

  ClearHeaderDirty();

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
  objects_loaded_ = true;
  MarkSaveDirtyForTileObject(object);

  return absl::OkStatus();
}

absl::Status Room::RemoveObject(size_t index) {
  if (index >= tile_objects_.size()) {
    return absl::OutOfRangeError("Object index out of range");
  }

  MarkSaveDirtyForTileObject(tile_objects_[index]);
  tile_objects_.erase(tile_objects_.begin() + index);
  objects_loaded_ = true;
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

  MarkSaveDirtyForTileObject(tile_objects_[index]);
  tile_objects_[index] = object;
  objects_loaded_ = true;
  MarkSaveDirtyForTileObject(object);

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
  for (short stair : kStairsObjects) {
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
  const auto& rom_data = rom()->vector();
  // Avoid duplicate entries if callers reload sprite data on the same room.
  sprites_.clear();
  sprites_loaded_ = false;
  if (room_id_ < 0 || room_id_ >= kNumberOfRooms) {
    return;
  }

  int sprite_pointer = 0;
  if (!GetSpritePointerTablePc(rom_data, &sprite_pointer).ok()) {
    return;
  }

  int sprite_address =
      ReadRoomSpriteAddressPc(rom_data, sprite_pointer, room_id_);
  if (sprite_address < 0 ||
      sprite_address + 1 >= static_cast<int>(rom_data.size())) {
    return;
  }

  // First byte is the SortSprites mode (0 or 1), not sprite data.
  sprite_address += 1;

  while (sprite_address + 2 < static_cast<int>(rom_data.size())) {
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

  sprites_loaded_ = true;
}

void Room::LoadChests() {
  chests_in_room_.clear();
  chests_loaded_ = false;
  if (!rom_ || !rom_->is_loaded()) {
    return;
  }
  const auto& rom_data = rom()->vector();
  if (kChestsDataPointer1 + 2 >= static_cast<int>(rom_data.size()) ||
      kChestsLengthPointer + 1 >= static_cast<int>(rom_data.size())) {
    return;
  }

  const int cpos = static_cast<int>(SnesToPc(
      (static_cast<uint32_t>(rom_data[kChestsDataPointer1 + 2]) << 16) |
      (static_cast<uint32_t>(rom_data[kChestsDataPointer1 + 1]) << 8) |
      rom_data[kChestsDataPointer1]));
  const size_t byte_length =
      (static_cast<size_t>(rom_data[kChestsLengthPointer + 1]) << 8) |
      rom_data[kChestsLengthPointer];
  const size_t bounded_byte_length = std::min<size_t>(
      byte_length, cpos >= 0 && cpos < static_cast<int>(rom_data.size())
                       ? rom_data.size() - static_cast<size_t>(cpos)
                       : 0);
  const size_t record_count = std::min<size_t>(
      bounded_byte_length / kChestTableRecordSize, kChestTableCapacityRecords);

  for (size_t i = 0; i < record_count; ++i) {
    const size_t offset =
        static_cast<size_t>(cpos) + (i * kChestTableRecordSize);
    if ((((rom_data[offset + 1] << 8) + rom_data[offset]) & 0x7FFF) ==
        room_id_) {
      // There's a chest in that room !
      bool big = false;
      if ((((rom_data[offset + 1] << 8) + rom_data[offset]) & 0x8000) ==
          0x8000) {
        big = true;
      }

      chests_in_room_.emplace_back(chest_data{rom_data[offset + 2], big});
    }
  }
  chests_loaded_ = true;
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
  int bytes_count = (rom_data[kTorchesLengthPointer + 1] << 8) |
                    rom_data[kTorchesLengthPointer];

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

    uint8_t b1 = rom_data[kTorchData + i];
    uint8_t b2 = rom_data[kTorchData + i + 1];

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

        b1 = rom_data[kTorchData + i];
        b2 = rom_data[kTorchData + i + 1];

        // End of torch list for this room
        if (b1 == 0xFF && b2 == 0xFF) {
          break;
        }

        const LightableTorchEntry entry = DecodeLightableTorchEntry({b1, b2});

        // Create torch object (ID 0x150)
        RoomObject torch_obj(0x150, entry.px, entry.py, 0, entry.draw_layer);
        torch_obj.SetRom(rom_);
        torch_obj.set_options(ObjectOption::Torch);
        torch_obj.set_torch_reserved_bit(entry.reserved);
        torch_obj.lit_ = entry.lit;

        tile_objects_.push_back(torch_obj);

        LOG_DEBUG(
            "Room", "Loaded torch at (%d,%d) draw_layer=%d reserved=%d lit=%d",
            entry.px, entry.py, entry.draw_layer, entry.reserved, entry.lit);

        i += 2;
      }
      break;  // Found and processed our room's torches
    } else {
      // Skip to next room's torches
      i += 2;
      while (i < bytes_count) {
        if (i + 1 >= bytes_count)
          break;
        b1 = rom_data[kTorchData + i];
        b2 = rom_data[kTorchData + i + 1];
        if (b1 == 0xFF && b2 == 0xFF) {
          break;
        }
        i += 2;
      }
    }
  }
  torches_loaded_ = true;
}

namespace {

constexpr int kTorchesMaxSize = 0x120;  // ZScream Constants.TorchesMaxSize

struct TorchSegment {
  uint16_t room_id = 0;
  std::vector<uint8_t> bytes;
};

// Parse current ROM torch blob in authoring order for preserve-merge.
std::vector<TorchSegment> ParseRomTorchSegments(
    const std::vector<uint8_t>& rom_data, int bytes_count) {
  std::vector<TorchSegment> segments;
  int i = 0;
  while (i + 1 < bytes_count && i < kTorchesMaxSize) {
    uint8_t b1 = rom_data[kTorchData + i];
    uint8_t b2 = rom_data[kTorchData + i + 1];
    if (b1 == 0xFF && b2 == 0xFF) {
      // Vanilla contains standalone $FFFF padding between two authored room
      // segments. Keep it as an unowned pass-through segment so a no-op save
      // remains byte-identical instead of compacting the table.
      TorchSegment padding;
      padding.room_id = 0xFFFF;
      padding.bytes = {0xFF, 0xFF};
      segments.push_back(std::move(padding));
      i += 2;
      continue;
    }
    uint16_t room_id = (b2 << 8) | b1;
    if (room_id >= kNumberOfRooms) {
      i += 2;
      continue;
    }
    TorchSegment seg;
    seg.room_id = room_id;
    seg.bytes.push_back(b1);
    seg.bytes.push_back(b2);
    i += 2;
    while (i + 1 < bytes_count && i < kTorchesMaxSize) {
      b1 = rom_data[kTorchData + i];
      b2 = rom_data[kTorchData + i + 1];
      if (b1 == 0xFF && b2 == 0xFF) {
        seg.bytes.push_back(0xFF);
        seg.bytes.push_back(0xFF);
        i += 2;
        break;
      }
      seg.bytes.push_back(b1);
      seg.bytes.push_back(b2);
      i += 2;
    }
    segments.push_back(std::move(seg));
  }
  return segments;
}

std::vector<uint8_t> EncodeTorchSegmentForRoom(int room_id, const Room& room) {
  std::vector<uint8_t> bytes;
  for (const auto& obj : room.GetTileObjects()) {
    if ((obj.options() & ObjectOption::Torch) == ObjectOption::Nothing) {
      continue;
    }
    if (bytes.empty()) {
      bytes.push_back(room_id & 0xFF);
      bytes.push_back((room_id >> 8) & 0xFF);
    }
    const LightableTorchBytes encoded = EncodeLightableTorchEntry({
        .px = static_cast<uint8_t>(obj.x()),
        .py = static_cast<uint8_t>(obj.y()),
        .draw_layer = static_cast<uint8_t>(obj.GetLayerValue() & 1),
        .reserved = obj.torch_reserved_bit(),
        .lit = obj.lit_,
    });
    bytes.push_back(encoded.low);
    bytes.push_back(encoded.high);
  }
  if (!bytes.empty()) {
    bytes.push_back(0xFF);
    bytes.push_back(0xFF);
  }
  return bytes;
}

absl::Status ValidateSpecialObjectDrawLayerSelector(const RoomObject& object,
                                                    int room_id,
                                                    const char* object_type) {
  const uint8_t selector = object.GetLayerValue();
  if (selector <= 1) {
    return absl::OkStatus();
  }
  return absl::InvalidArgumentError(absl::StrFormat(
      "%s in room 0x%03X has invalid special draw-layer selector %d; "
      "expected 0 "
      "(upper/BG1) or 1 (lower/BG2)",
      object_type, room_id, selector));
}

absl::Status ValidateLightableTorchForSave(const RoomObject& object,
                                           int room_id) {
  RETURN_IF_ERROR(
      ValidateSpecialObjectDrawLayerSelector(object, room_id, "Torch"));
  if (object.x() <= 0x3E && object.y() <= 0x3E) {
    return absl::OkStatus();
  }
  return absl::InvalidArgumentError(absl::StrFormat(
      "Torch in room 0x%03X has invalid position (%d,%d); expected x/y in "
      "range 0..62",
      room_id, object.x(), object.y()));
}

}  // namespace

template <typename RoomLookup>
absl::Status SaveAllTorchesImpl(Rom* rom, int room_count,
                                RoomLookup&& room_lookup) {
  if (!rom || !rom->is_loaded()) {
    return absl::InvalidArgumentError("ROM not loaded");
  }

  const auto& rom_data = rom->vector();
  int existing_count = (rom_data[kTorchesLengthPointer + 1] << 8) |
                       rom_data[kTorchesLengthPointer];
  if (existing_count > kTorchesMaxSize) {
    existing_count = kTorchesMaxSize;
  }
  auto rom_segments = ParseRomTorchSegments(rom_data, existing_count);

  std::vector<uint8_t> bytes;
  const int room_limit = std::min(room_count, kNumberOfRooms);
  std::vector<bool> owned_rooms(room_limit, false);
  std::vector<bool> seen_original_room(room_limit, false);
  std::vector<bool> emitted_owned_room(room_limit, false);
  std::vector<std::vector<uint8_t>> replacements(room_limit);
  bool any_owned_room = false;
  for (int room_id = 0; room_id < room_limit; ++room_id) {
    const Room* room = room_lookup(room_id);
    const bool room_owned =
        room != nullptr && (room->AreTorchesLoaded() || room->torches_dirty());
    if (!room_owned) {
      continue;
    }
    for (const auto& object : room->GetTileObjects()) {
      if ((object.options() & ObjectOption::Torch) != ObjectOption::Nothing) {
        RETURN_IF_ERROR(ValidateLightableTorchForSave(object, room_id));
      }
    }
    owned_rooms[room_id] = true;
    any_owned_room = true;
    replacements[room_id] = EncodeTorchSegmentForRoom(room_id, *room);
  }

  if (!any_owned_room) {
    return absl::OkStatus();
  }

  for (const auto& segment : rom_segments) {
    if (segment.room_id < room_limit) {
      seen_original_room[segment.room_id] = true;
      if (owned_rooms[segment.room_id]) {
        if (!emitted_owned_room[segment.room_id]) {
          bytes.insert(bytes.end(), replacements[segment.room_id].begin(),
                       replacements[segment.room_id].end());
          emitted_owned_room[segment.room_id] = true;
        }
        continue;
      }
    }
    bytes.insert(bytes.end(), segment.bytes.begin(), segment.bytes.end());
  }

  for (int room_id = 0; room_id < room_limit; ++room_id) {
    if (owned_rooms[room_id] && !seen_original_room[room_id] &&
        !replacements[room_id].empty()) {
      bytes.insert(bytes.end(), replacements[room_id].begin(),
                   replacements[room_id].end());
    }
  }

  if (bytes.size() > kTorchesMaxSize) {
    return absl::ResourceExhaustedError(
        absl::StrFormat("Torch data too large: %d bytes (max %d)", bytes.size(),
                        kTorchesMaxSize));
  }

  const uint16_t current_len =
      static_cast<uint16_t>(rom_data[kTorchesLengthPointer]) |
      (static_cast<uint16_t>(rom_data[kTorchesLengthPointer + 1]) << 8);
  if (current_len == bytes.size() &&
      kTorchData + static_cast<int>(bytes.size()) <=
          static_cast<int>(rom_data.size()) &&
      std::equal(bytes.begin(), bytes.end(), rom_data.begin() + kTorchData)) {
    for (int room_id = 0; room_id < room_limit; ++room_id) {
      if (const Room* room = room_lookup(room_id);
          room != nullptr && room->torches_dirty()) {
        const_cast<Room*>(room)->ClearTorchesDirty();
      }
    }
    return absl::OkStatus();
  }

  RETURN_IF_ERROR(rom->WriteWord(kTorchesLengthPointer,
                                 static_cast<uint16_t>(bytes.size())));
  RETURN_IF_ERROR(rom->WriteVector(kTorchData, bytes));
  for (int room_id = 0; room_id < room_limit; ++room_id) {
    if (const Room* room = room_lookup(room_id);
        room != nullptr && room->torches_dirty()) {
      const_cast<Room*>(room)->ClearTorchesDirty();
    }
  }
  return absl::OkStatus();
}

absl::Status SaveAllTorches(Rom* rom, absl::Span<const Room> rooms) {
  return SaveAllTorchesImpl(rom, static_cast<int>(rooms.size()),
                            [&rooms](int room_id) { return &rooms[room_id]; });
}

absl::Status SaveAllTorches(
    Rom* rom, int room_count,
    const std::function<const Room*(int)>& room_lookup) {
  return SaveAllTorchesImpl(rom, room_count, room_lookup);
}

// Region preservation for `RoomsWithPitDamage` when no edited table is supplied.
// When `pit_damage_table` is non-null and dirty, encode the in-memory membership
// list through `PitDamageTable::SaveToRom` instead of blind preservation.
absl::Status SaveAllPits(Rom* rom) {
  return SaveAllPits(rom, nullptr);
}

absl::Status SaveAllPits(Rom* rom, PitDamageTable* pit_damage_table) {
  if (pit_damage_table != nullptr && pit_damage_table->dirty()) {
    RETURN_IF_ERROR(pit_damage_table->SaveToRom(rom));
    pit_damage_table->ClearDirty();
    return absl::OkStatus();
  }
  if (!rom || !rom->is_loaded()) {
    return absl::InvalidArgumentError("ROM not loaded");
  }
  const auto& rom_data = rom->vector();
  if (kPitCount < 0 || kPitCount >= static_cast<int>(rom_data.size()) ||
      kPitPointer + 2 >= static_cast<int>(rom_data.size())) {
    return absl::OutOfRangeError("Pit count/pointer out of range");
  }
  int max_offset = rom_data[kPitCount];
  // Total bytes = max_offset + 2 (covers offsets 0..max_offset
  // inclusive, with each entry being a 2-byte word). When max_offset
  // is 0, there's still 1 word to preserve (the entry at offset 0).
  int data_len = max_offset + 2;
  int pit_ptr_snes = (rom_data[kPitPointer + 2] << 16) |
                     (rom_data[kPitPointer + 1] << 8) | rom_data[kPitPointer];
  int pit_data_pc = SnesToPc(pit_ptr_snes);
  if (pit_data_pc < 0 ||
      pit_data_pc + data_len > static_cast<int>(rom_data.size())) {
    return absl::OutOfRangeError("Pit data region out of range");
  }
  std::vector<uint8_t> data(rom_data.begin() + pit_data_pc,
                            rom_data.begin() + pit_data_pc + data_len);
  RETURN_IF_ERROR(rom->WriteByte(kPitCount, max_offset));
  RETURN_IF_ERROR(rom->WriteByte(kPitPointer, pit_ptr_snes & 0xFF));
  RETURN_IF_ERROR(rom->WriteByte(kPitPointer + 1, (pit_ptr_snes >> 8) & 0xFF));
  RETURN_IF_ERROR(rom->WriteByte(kPitPointer + 2, (pit_ptr_snes >> 16) & 0xFF));
  return rom->WriteVector(pit_data_pc, data);
}

namespace {

constexpr int kBlocksRegionSize = 0x80;
constexpr std::array<int, 4> kBlocksPointerSlots = {
    kBlocksPointer1, kBlocksPointer2, kBlocksPointer3, kBlocksPointer4};

bool HalfOpenRangesOverlap(int first_begin, int first_end, int second_begin,
                           int second_end) {
  return first_begin < second_end && second_begin < first_end;
}

absl::Status ValidateBlocksLoaderPointerOperand(
    const std::vector<uint8_t>& rom_data, int operand_pc) {
  if (operand_pc <= 0 || operand_pc + 5 >= static_cast<int>(rom_data.size())) {
    return absl::OutOfRangeError("Blocks pointer operand out of range");
  }
  // The block table pointers are the 3-byte operands in the US USDASM
  // bank_02 loader shape (#_02DAF9..#_02DB12):
  //   BF ll hh bb  LDA.l table+N*0x80,X
  //   9D ll hh     STA.w $7EF940+N*0x80,X
  // The data table starts at bank_04's
  // SpecialUnderworldObjects_pushable_block (#_04F1DE). Pinned against a real
  // vanilla ROM by
  // DungeonSaveRegionTest.BlocksLoaderPointerOperandsMatchUsdasmShape.
  //
  // Guard both sides before dereferencing or future repointing so a bad
  // constant or already-patched ROM cannot make the saver treat unrelated
  // instruction bytes as data pointers.
  if (rom_data[operand_pc - 1] != 0xBF || rom_data[operand_pc + 3] != 0x9D) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Blocks pointer operand at PC 0x%05X is not in the expected "
        "LDA.l ...,X / STA.w loader sequence",
        operand_pc));
  }
  return absl::OkStatus();
}

absl::Status PreflightBlocksLoaderDestinations(
    const std::vector<uint8_t>& rom_data, std::array<int, 4>* destination_pcs) {
  if (kBlocksLength < 0 ||
      kBlocksLength + 1 >= static_cast<int>(rom_data.size())) {
    return absl::OutOfRangeError("Blocks length out of range");
  }

  for (size_t page = 0; page < kBlocksPointerSlots.size(); ++page) {
    const int operand_pc = kBlocksPointerSlots[page];
    RETURN_IF_ERROR(ValidateBlocksLoaderPointerOperand(rom_data, operand_pc));
    const int snes = (rom_data[operand_pc + 2] << 16) |
                     (rom_data[operand_pc + 1] << 8) | rom_data[operand_pc];
    const int data_pc = SnesToPc(snes);
    if (data_pc < 0 ||
        data_pc + kBlocksRegionSize > static_cast<int>(rom_data.size())) {
      return absl::OutOfRangeError(absl::StrFormat(
          "Blocks data region out of range for loader page %d", page + 1));
    }
    (*destination_pcs)[page] = data_pc;
  }

  constexpr int kLengthMetadataEnd = kBlocksLength + 2;
  for (size_t page = 0; page < destination_pcs->size(); ++page) {
    const int page_begin = (*destination_pcs)[page];
    const int page_end = page_begin + kBlocksRegionSize;
    if (HalfOpenRangesOverlap(page_begin, page_end, kBlocksLength,
                              kLengthMetadataEnd)) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Blocks data page %d at PC [0x%05X, 0x%05X) overlaps block-table "
          "length metadata [0x%05X, 0x%05X)",
          page + 1, page_begin, page_end, kBlocksLength, kLengthMetadataEnd));
    }

    for (size_t loader = 0; loader < kBlocksPointerSlots.size(); ++loader) {
      // Each destination operand is embedded in a seven-byte loader
      // instruction: BF ll hh bb 9D ll hh. Treat the complete instruction as
      // metadata so a table write cannot corrupt either opcode or operand.
      const int loader_begin = kBlocksPointerSlots[loader] - 1;
      const int loader_end = kBlocksPointerSlots[loader] + 6;
      if (HalfOpenRangesOverlap(page_begin, page_end, loader_begin,
                                loader_end)) {
        return absl::FailedPreconditionError(absl::StrFormat(
            "Blocks data page %d at PC [0x%05X, 0x%05X) overlaps loader %d "
            "opcode/operand metadata [0x%05X, 0x%05X)",
            page + 1, page_begin, page_end, loader + 1, loader_begin,
            loader_end));
      }
    }

    for (size_t previous = 0; previous < page; ++previous) {
      const int previous_begin = (*destination_pcs)[previous];
      const int previous_end = previous_begin + kBlocksRegionSize;
      if (HalfOpenRangesOverlap(page_begin, page_end, previous_begin,
                                previous_end)) {
        return absl::FailedPreconditionError(absl::StrFormat(
            "Blocks data pages %d and %d overlap at PC ranges [0x%05X, "
            "0x%05X) and [0x%05X, 0x%05X)",
            previous + 1, page + 1, previous_begin, previous_end, page_begin,
            page_end));
      }
    }
  }

  return absl::OkStatus();
}

}  // namespace

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
  std::array<int, 4> destination_pcs{};
  RETURN_IF_ERROR(
      PreflightBlocksLoaderDestinations(rom_data, &destination_pcs));
  if (blocks_count <= 0) {
    return absl::OkStatus();
  }
  for (int r = 0; r < 4; ++r) {
    const int pc = destination_pcs[r];
    int off = r * kBlocksRegionSize;
    int len = std::min(kBlocksRegionSize, blocks_count - off);
    if (len <= 0)
      break;
    std::vector<uint8_t> chunk(rom_data.begin() + pc,
                               rom_data.begin() + pc + len);
    RETURN_IF_ERROR(rom->WriteVector(pc, chunk));
  }
  RETURN_IF_ERROR(
      rom->WriteWord(kBlocksLength, static_cast<uint16_t>(blocks_count)));
  return absl::OkStatus();
}

absl::Status SaveAllBlocks(Rom* rom, int room_count,
                           const std::function<const Room*(int)>& room_lookup) {
  if (!rom || !rom->is_loaded()) {
    return absl::InvalidArgumentError("ROM not loaded");
  }
  const auto& rom_data = rom->vector();
  if (kBlocksLength + 1 >= static_cast<int>(rom_data.size())) {
    return absl::OutOfRangeError("Blocks length out of range");
  }

  std::array<int, 4> destination_pcs{};
  RETURN_IF_ERROR(
      PreflightBlocksLoaderDestinations(rom_data, &destination_pcs));

  // Read the original block buffer by dereferencing the four pointer
  // slots. We need this so unmaterialized / header-only rooms can have
  // their entries preserved verbatim — only rooms whose blocks were
  // actually loaded into memory get re-encoded from `tile_objects_`.
  // This prevents the editor migration from silently dropping vanilla
  // blocks for any room the user hasn't materialized yet.
  const int original_count_word =
      (rom_data[kBlocksLength + 1] << 8) | rom_data[kBlocksLength];
  const int original_byte_len = std::max(0, original_count_word);
  std::vector<uint8_t> original_buffer(original_byte_len, 0);
  for (int r = 0; r < 4; ++r) {
    const int pc = destination_pcs[r];
    const int off = r * kBlocksRegionSize;
    const int len = std::min(kBlocksRegionSize, original_byte_len - off);
    if (len <= 0)
      break;
    std::copy_n(rom_data.begin() + pc, len, original_buffer.begin() + off);
  }
  const int original_slot_count = original_byte_len / 4;

  // Build:
  //   - `slot_replacements`: for each existing slot whose room_id is
  //     "owned" by an editor-loaded room, the re-encoded bytes (or
  //     absent if the block was deleted in memory).
  //   - `owned_room_ids`: the set of room_ids whose blocks were
  //     materialized (so unmaterialized / header-only rooms can be
  //     preserved verbatim from `original_buffer`).
  //   - `appended`: blocks with `load_order == kBlockLoadOrderNew`,
  //     appended to the end in creation order.
  struct EncodedBlock {
    PushableBlockBytes bytes;
    const RoomObject* source_object;
  };
  std::unordered_set<uint16_t> owned_room_ids;
  std::unordered_set<int> claimed_load_orders;
  std::unordered_map<int, EncodedBlock> slot_replacements;
  std::vector<EncodedBlock> appended;
  for (int rid = 0; rid < room_count; ++rid) {
    const Room* room = room_lookup(rid);
    if (room == nullptr)
      continue;
    if (!room->AreBlocksLoaded()) {
      if (room->blocks_dirty()) {
        return absl::FailedPreconditionError(absl::StrFormat(
            "Room 0x%03X has unsaved pushable-block edits, but its block "
            "table is not loaded. Load the room's blocks before saving.",
            rid));
      }
      continue;  // Header-only — preserve its slots verbatim from ROM.
    }
    owned_room_ids.insert(static_cast<uint16_t>(rid));
    for (const auto& obj : room->GetTileObjects()) {
      if ((obj.options() & ObjectOption::Block) != ObjectOption::Block)
        continue;
      RETURN_IF_ERROR(
          ValidateSpecialObjectDrawLayerSelector(obj, rid, "Pushable block"));
      PushableBlockEntry encoded_entry;
      encoded_entry.room_id = static_cast<uint16_t>(rid);
      encoded_entry.px = obj.x();
      encoded_entry.py = obj.y();
      encoded_entry.draw_layer = obj.GetLayerValue();
      encoded_entry.behavior_layer = obj.block_behavior_layer();
      const PushableBlockBytes encoded =
          EncodePushableBlockEntry(encoded_entry);
      const EncodedBlock encoded_block{encoded, &obj};
      const int load_order = obj.block_load_order();
      if (load_order >= 0 && !claimed_load_orders.insert(load_order).second) {
        return absl::FailedPreconditionError(absl::StrFormat(
            "Room 0x%03X has multiple pushable blocks claiming non-new "
            "load-order slot %d",
            rid, load_order));
      }
      if (load_order == RoomObject::kBlockLoadOrderNew) {
        appended.push_back(encoded_block);
      } else if (load_order >= 0 && load_order < original_slot_count) {
        const int original_offset = load_order * 4;
        const uint16_t original_room_id =
            static_cast<uint16_t>(original_buffer[original_offset] |
                                  (original_buffer[original_offset + 1] << 8));
        if (original_room_id != static_cast<uint16_t>(rid)) {
          // Undo/redo snapshots can restore the load order that was valid
          // before a prior save compacted the global table. Never let that
          // stale identity replace a different room's entry; preserve the
          // object by appending it as a newly reconciled entry instead.
          appended.push_back(encoded_block);
          continue;
        }
        slot_replacements.emplace(load_order, encoded_block);
      } else {
        // load_order points outside the original buffer (e.g. ROM
        // changed under us). Treat as new.
        appended.push_back(encoded_block);
      }
    }
  }

  // Walk the original buffer slot-by-slot, replacing entries owned by
  // materialized rooms and preserving the rest verbatim.
  std::vector<uint8_t> output;
  output.reserve(original_byte_len + appended.size() * 4);
  std::vector<std::pair<const RoomObject*, int>> load_order_updates;
  load_order_updates.reserve(slot_replacements.size() + appended.size());
  const auto append_encoded_block =
      [&output, &load_order_updates](const EncodedBlock& block) {
        const int output_slot = static_cast<int>(output.size() / 4);
        output.push_back(block.bytes.b1);
        output.push_back(block.bytes.b2);
        output.push_back(block.bytes.b3);
        output.push_back(block.bytes.b4);
        load_order_updates.emplace_back(block.source_object, output_slot);
      };
  for (int slot = 0; slot < original_slot_count; ++slot) {
    const uint8_t b1 = original_buffer[slot * 4 + 0];
    const uint8_t b2 = original_buffer[slot * 4 + 1];
    const uint16_t slot_room_id = static_cast<uint16_t>(b1 | (b2 << 8));
    if (owned_room_ids.contains(slot_room_id)) {
      const auto it = slot_replacements.find(slot);
      if (it == slot_replacements.end()) {
        // The block at this slot was deleted in memory. Skip it,
        // shrinking the output.
        continue;
      }
      append_encoded_block(it->second);
    } else {
      // Unmaterialized / header-only room: keep the original bytes.
      output.push_back(b1);
      output.push_back(b2);
      output.push_back(original_buffer[slot * 4 + 2]);
      output.push_back(original_buffer[slot * 4 + 3]);
    }
  }
  // Append newly-added blocks (load_order == kBlockLoadOrderNew) at the
  // tail in creation order. Anything in slot_replacements that didn't
  // match a slot was already routed to `appended` above.
  for (const auto& block : appended) {
    append_encoded_block(block);
  }

  // LoadAndBuildRoom's block scan is a do-while loop: it always reads the
  // entry at $7EF940 before adding four and comparing against this byte
  // length. A zero limit can therefore never terminate at the first boundary;
  // the 16-bit index walks beyond the 0x200-byte WRAM table until it wraps.
  // Fail before the first ROM write and keep edited rooms dirty rather than
  // emitting a runtime-unsafe empty table.
  if (output.empty()) {
    return absl::FailedPreconditionError(
        "Pushable-block table cannot be empty: ALTTP's runtime scan reads one "
        "entry before comparing the byte-length limit. Keep at least one "
        "pushable block, or patch the runtime loop before removing the last "
        "entry.");
  }

  // Capacity check against the vanilla 128-entry cap.
  const int kMaxEntries = (4 * kBlocksRegionSize) / 4;
  if (static_cast<int>(output.size() / 4) > kMaxEntries) {
    return absl::FailedPreconditionError(absl::StrCat(
        "Pushable-block table overflow: ", output.size() / 4,
        " entries exceeds the vanilla cap of ", kMaxEntries,
        " (expand layout requires repointing all 4 LDA.l operand slots; "
        "out of scope for this encoder)."));
  }

  // Build the write plan from the four destinations preflighted above. Doing
  // the topology check before encoding means direct callers that do not wrap
  // this public API in a transaction cannot discover a bad later page only
  // after an earlier page has already been written.
  const int total_bytes = static_cast<int>(output.size());
  struct BlockWriteDestination {
    int pc;
    int output_offset;
    int length;
  };
  std::vector<BlockWriteDestination> write_destinations;
  write_destinations.reserve(4);
  for (int r = 0; r < 4; ++r) {
    const int off = r * kBlocksRegionSize;
    const int len = std::min(kBlocksRegionSize, total_bytes - off);
    if (len <= 0)
      break;
    write_destinations.push_back({destination_pcs[r], off, len});
  }

  // Write each prevalidated region. We do not relocate the data — the four
  // operand slots keep pointing at their existing SNES addresses.
  for (const auto& destination : write_destinations) {
    std::vector<uint8_t> chunk(
        output.begin() + destination.output_offset,
        output.begin() + destination.output_offset + destination.length);
    RETURN_IF_ERROR(rom->WriteVector(destination.pc, chunk));
  }

  RETURN_IF_ERROR(
      rom->WriteWord(kBlocksLength, static_cast<uint16_t>(total_bytes)));

  // Deleting an entry compacts every following slot. Rebase each loaded
  // object's identity to its committed output slot so a later no-op save does
  // not try to replace the stale pre-compaction slot and silently drop it.
  // This metadata stays untouched until every ROM write succeeds, matching
  // the dirty-state failure contract below.
  for (const auto& [object, load_order] : load_order_updates) {
    const_cast<RoomObject*>(object)->set_block_load_order(load_order);
  }
  for (int room_id = 0; room_id < room_count; ++room_id) {
    if (const Room* room = room_lookup(room_id);
        room != nullptr && room->AreBlocksLoaded() && room->blocks_dirty()) {
      const_cast<Room*>(room)->ClearBlocksDirty();
    }
  }
  return absl::OkStatus();
}

template <typename RoomLookup>
absl::Status SaveAllCollisionImpl(Rom* rom, int room_count,
                                  RoomLookup&& room_lookup) {
  if (!rom || !rom->is_loaded()) {
    return absl::InvalidArgumentError("ROM not loaded");
  }

  // If the custom collision region doesn't exist (vanilla ROM), treat as a noop
  // only when there are no pending custom collision edits. This avoids silently
  // dropping user-authored collision changes on ROMs that don't support the
  // expanded collision bank.
  const auto& rom_data = rom->vector();
  const int ptrs_size = kNumberOfRooms * 3;
  const bool has_ptr_table = HasCustomCollisionPointerTable(rom_data.size());
  const bool has_data_region = HasCustomCollisionDataRegion(rom_data.size());

  if (!has_ptr_table) {
    for (int room_id = 0; room_id < room_count; ++room_id) {
      const Room* room = room_lookup(room_id);
      if (room != nullptr && room->custom_collision_dirty()) {
        return absl::FailedPreconditionError(
            "Custom collision region not present in this ROM");
      }
    }
    return absl::OkStatus();
  }

  if (!has_data_region) {
    for (int room_id = 0; room_id < room_count; ++room_id) {
      const Room* room = room_lookup(room_id);
      if (room != nullptr && room->custom_collision_dirty()) {
        return absl::FailedPreconditionError(
            "Custom collision data region not present in this ROM");
      }
    }
    return absl::OkStatus();
  }

  // Save-time guardrails: custom collision writes must never clobber the
  // reserved WaterFill tail region (Oracle of Secrets).
  yaze::rom::WriteFence fence;
  RETURN_IF_ERROR(fence.Allow(
      static_cast<uint32_t>(kCustomCollisionRoomPointers),
      static_cast<uint32_t>(kCustomCollisionRoomPointers + ptrs_size),
      "CustomCollisionPointers"));
  RETURN_IF_ERROR(
      fence.Allow(static_cast<uint32_t>(kCustomCollisionDataPosition),
                  static_cast<uint32_t>(kCustomCollisionDataSoftEnd),
                  "CustomCollisionData"));
  yaze::rom::ScopedWriteFence scope(rom, &fence);

  const int room_limit = std::min(room_count, kNumberOfRooms);
  for (int room_id = 0; room_id < room_limit; ++room_id) {
    const Room* room = room_lookup(room_id);
    if (room == nullptr || !room->custom_collision_dirty()) {
      continue;
    }

    const int actual_room_id = room->id();
    const int ptr_offset = kCustomCollisionRoomPointers + (actual_room_id * 3);
    if (ptr_offset + 2 >= static_cast<int>(rom_data.size())) {
      return absl::OutOfRangeError("Custom collision pointer out of range");
    }

    if (!room->has_custom_collision()) {
      // Disable: clear the pointer entry.
      RETURN_IF_ERROR(rom->WriteByte(ptr_offset, 0));
      RETURN_IF_ERROR(rom->WriteByte(ptr_offset + 1, 0));
      RETURN_IF_ERROR(rom->WriteByte(ptr_offset + 2, 0));
      const_cast<Room*>(room)->ClearCustomCollisionDirty();
      continue;
    }

    // Treat an all-zero map as disabled to avoid wasting space.
    bool any = false;
    for (uint8_t v : room->custom_collision().tiles) {
      if (v != 0) {
        any = true;
        break;
      }
    }
    if (!any) {
      RETURN_IF_ERROR(rom->WriteByte(ptr_offset, 0));
      RETURN_IF_ERROR(rom->WriteByte(ptr_offset + 1, 0));
      RETURN_IF_ERROR(rom->WriteByte(ptr_offset + 2, 0));
      const_cast<Room*>(room)->ClearCustomCollisionDirty();
      continue;
    }

    RETURN_IF_ERROR(
        WriteTrackCollision(rom, actual_room_id, room->custom_collision()));
    const_cast<Room*>(room)->ClearCustomCollisionDirty();
  }

  return absl::OkStatus();
}

absl::Status SaveAllCollision(Rom* rom, absl::Span<Room> rooms) {
  return SaveAllCollisionImpl(
      rom, static_cast<int>(rooms.size()),
      [&rooms](int room_id) { return &rooms[room_id]; });
}

absl::Status SaveAllCollision(Rom* rom, int room_count,
                              const std::function<Room*(int)>& room_lookup) {
  return SaveAllCollisionImpl(rom, room_count, room_lookup);
}

absl::StatusOr<std::vector<std::pair<uint32_t, uint32_t>>>
GetChestTableWriteRanges(const Rom* rom) {
  if (rom == nullptr || !rom->is_loaded()) {
    return absl::InvalidArgumentError("ROM not loaded");
  }
  const auto& rom_data = rom->vector();
  if (kChestsLengthPointer + 1 >= static_cast<int>(rom_data.size()) ||
      kChestsDataPointer1 + 2 >= static_cast<int>(rom_data.size())) {
    return absl::OutOfRangeError("Chest pointers out of range");
  }

  const uint32_t data_pointer =
      (static_cast<uint32_t>(rom_data[kChestsDataPointer1 + 2]) << 16) |
      (static_cast<uint32_t>(rom_data[kChestsDataPointer1 + 1]) << 8) |
      rom_data[kChestsDataPointer1];
  const uint32_t data_pc = SnesToPc(data_pointer);
  if (data_pc > rom_data.size() ||
      static_cast<size_t>(kChestTableCapacityBytes) >
          rom_data.size() - static_cast<size_t>(data_pc)) {
    return absl::OutOfRangeError("Chest data region out of range");
  }
  const uint32_t data_end =
      data_pc + static_cast<uint32_t>(kChestTableCapacityBytes);
  const auto overlaps = [](uint32_t begin, uint32_t end, uint32_t other_begin,
                           uint32_t other_end) {
    return begin < other_end && other_begin < end;
  };
  if (overlaps(data_pc, data_end, kChestsLengthPointer,
               kChestsLengthPointer + 2) ||
      overlaps(data_pc, data_end, kChestsDataPointer1,
               kChestsDataPointer1 + 3)) {
    return absl::FailedPreconditionError(
        "Chest data region overlaps chest metadata operands");
  }

  return std::vector<std::pair<uint32_t, uint32_t>>{
      {static_cast<uint32_t>(kChestsLengthPointer),
       static_cast<uint32_t>(kChestsLengthPointer + 2)},
      {data_pc, data_end},
  };
}

namespace {

struct PhysicalChestRecord {
  uint16_t word = 0;
  uint8_t item = 0;

  uint16_t room_id() const { return word & 0x7FFF; }
};

// Parse current ROM chest data without grouping or normalizing records.
// `byte_length` is the runtime byte count at kChestsLengthPointer.
std::vector<PhysicalChestRecord> ParsePhysicalRomChests(
    const std::vector<uint8_t>& rom_data, int cpos, int byte_length) {
  std::vector<PhysicalChestRecord> records;
  const int record_count = byte_length / kChestTableRecordSize;
  records.reserve(record_count);
  for (int i = 0; i < record_count; ++i) {
    const int off = cpos + i * kChestTableRecordSize;
    if (off < 0 ||
        off + kChestTableRecordSize > static_cast<int>(rom_data.size())) {
      break;
    }
    const uint16_t word =
        (static_cast<uint16_t>(rom_data[off + 1]) << 8) | rom_data[off];
    records.push_back(PhysicalChestRecord{word, rom_data[off + 2]});
  }
  return records;
}

void AppendChestRecord(std::vector<uint8_t>* bytes, uint16_t word,
                       uint8_t item) {
  bytes->push_back(word & 0xFF);
  bytes->push_back((word >> 8) & 0xFF);
  bytes->push_back(item);
}

void AppendEditedChestRecord(std::vector<uint8_t>* bytes, int room_id,
                             const chest_data& chest) {
  const uint16_t word = static_cast<uint16_t>(room_id) |
                        (chest.size ? static_cast<uint16_t>(0x8000) : 0);
  AppendChestRecord(bytes, word, chest.id);
}

int ReadRoomPotItemAddressPc(const std::vector<uint8_t>& rom_data,
                             int room_id) {
  if (room_id < 0 || room_id >= kNumberOfRooms) {
    return -1;
  }
  const int ptr_off = kRoomItemsPointers + (room_id * 2);
  if (ptr_off < 0 || ptr_off + 1 >= static_cast<int>(rom_data.size())) {
    return -1;
  }
  const uint16_t item_ptr =
      (static_cast<uint16_t>(rom_data[ptr_off + 1]) << 8) | rom_data[ptr_off];
  if (item_ptr < 0x8000) {
    return -1;
  }
  const int item_addr = static_cast<int>(SnesToPc(0x010000 | item_ptr));
  return item_addr >= 0 && item_addr < static_cast<int>(rom_data.size())
             ? item_addr
             : -1;
}

absl::StatusOr<PhysicalStreamInfo> GetPotItemStreamInfo(
    const std::vector<uint8_t>& rom_data, int room_id) {
  if (kRoomItemsPointers + (kNumberOfRooms * 2) >
      static_cast<int>(rom_data.size())) {
    return absl::OutOfRangeError("Room items pointer table out of range");
  }
  if (room_id < 0 || room_id >= kNumberOfRooms) {
    return absl::OutOfRangeError("Room ID out of range");
  }

  std::vector<int> addresses(kNumberOfRooms, -1);
  for (int id = 0; id < kNumberOfRooms; ++id) {
    addresses[id] = ReadRoomPotItemAddressPc(rom_data, id);
  }
  const int hard_end =
      std::min(static_cast<int>(rom_data.size()), kRoomItemsDataEnd);
  PhysicalStreamInfo info = AnalyzePhysicalStream(addresses, room_id, hard_end);
  if (info.address < 0 || info.address >= hard_end) {
    return absl::FailedPreconditionError(
        "Room pot item pointer is null, invalid, or outside the item region");
  }
  return info;
}

}  // namespace

template <typename RoomLookup>
absl::Status SaveAllChestsImpl(Rom* rom, int room_count,
                               RoomLookup&& room_lookup) {
  if (rom == nullptr || !rom->is_loaded()) {
    return absl::InvalidArgumentError("ROM not loaded");
  }
  const auto& rom_data = rom->vector();
  if (kChestsLengthPointer + 1 >= static_cast<int>(rom_data.size()) ||
      kChestsDataPointer1 + 2 >= static_cast<int>(rom_data.size())) {
    return absl::OutOfRangeError("Chest pointers out of range");
  }
  const int room_limit = std::min(room_count, kNumberOfRooms);
  std::vector<const Room*> dirty_rooms(kNumberOfRooms, nullptr);
  bool any_dirty = false;
  for (int room_id = 0; room_id < room_limit; ++room_id) {
    const Room* room = room_lookup(room_id);
    if (room != nullptr && room->chests_dirty()) {
      dirty_rooms[room_id] = room;
      any_dirty = true;
    }
  }
  if (!any_dirty) {
    return absl::OkStatus();
  }

  ASSIGN_OR_RETURN(auto write_ranges, GetChestTableWriteRanges(rom));

  const int byte_length = (rom_data[kChestsLengthPointer + 1] << 8) |
                          rom_data[kChestsLengthPointer];
  if (byte_length < 0 || byte_length > kChestTableCapacityBytes ||
      (byte_length % kChestTableRecordSize) != 0) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Chest table byte length %d is invalid (capacity %d)",
                        byte_length, kChestTableCapacityBytes));
  }
  const int cpos = static_cast<int>(write_ranges[1].first);
  const auto physical_records =
      ParsePhysicalRomChests(rom_data, cpos, byte_length);
  if (physical_records.size() !=
      static_cast<size_t>(byte_length / kChestTableRecordSize)) {
    return absl::OutOfRangeError("Chest data region is truncated");
  }

  std::vector<size_t> old_counts(kNumberOfRooms, 0);
  for (const PhysicalChestRecord& record : physical_records) {
    if (record.room_id() < kNumberOfRooms) {
      ++old_counts[record.room_id()];
    }
  }

  size_t final_record_count = physical_records.size();
  for (int room_id = 0; room_id < room_limit; ++room_id) {
    if (dirty_rooms[room_id] == nullptr) {
      continue;
    }
    final_record_count -= old_counts[room_id];
    final_record_count += dirty_rooms[room_id]->GetChests().size();
  }
  if (final_record_count > static_cast<size_t>(kChestTableCapacityRecords)) {
    return absl::ResourceExhaustedError(absl::StrFormat(
        "Chest table has %d records; capacity is %d",
        static_cast<int>(final_record_count), kChestTableCapacityRecords));
  }

  std::vector<uint8_t> bytes;
  bytes.reserve(final_record_count * kChestTableRecordSize);
  std::vector<size_t> seen_counts(kNumberOfRooms, 0);
  for (const PhysicalChestRecord& record : physical_records) {
    const uint16_t room_id = record.room_id();
    const Room* dirty_room =
        room_id < kNumberOfRooms ? dirty_rooms[room_id] : nullptr;
    if (dirty_room == nullptr) {
      AppendChestRecord(&bytes, record.word, record.item);
      continue;
    }

    const size_t occurrence = seen_counts[room_id]++;
    const auto& replacements = dirty_room->GetChests();
    if (occurrence < replacements.size()) {
      AppendEditedChestRecord(&bytes, room_id, replacements[occurrence]);
    }
  }

  // Growth has no existing physical slot. Append extras in room-ID order so
  // repeated saves are deterministic while every pre-existing record keeps its
  // relative position.
  for (int room_id = 0; room_id < room_limit; ++room_id) {
    const Room* dirty_room = dirty_rooms[room_id];
    if (dirty_room == nullptr) {
      continue;
    }
    const auto& replacements = dirty_room->GetChests();
    for (size_t i = seen_counts[room_id]; i < replacements.size(); ++i) {
      AppendEditedChestRecord(&bytes, room_id, replacements[i]);
    }
  }

  if (bytes.size() != final_record_count * kChestTableRecordSize) {
    return absl::InternalError("Chest save plan size mismatch");
  }

  const bool length_changed = byte_length != static_cast<int>(bytes.size());
  const bool data_changed =
      !std::equal(bytes.begin(), bytes.end(), rom_data.begin() + cpos);
  yaze::rom::WriteFence fence;
  RETURN_IF_ERROR(fence.Allow(write_ranges[0].first, write_ranges[0].second,
                              "ChestTableLength"));
  RETURN_IF_ERROR(fence.Allow(write_ranges[1].first, write_ranges[1].second,
                              "ChestTableData"));
  yaze::rom::ScopedWriteFence scope(rom, &fence);

  if (data_changed) {
    RETURN_IF_ERROR(rom->WriteVector(cpos, bytes));
  }
  if (length_changed) {
    RETURN_IF_ERROR(rom->WriteWord(kChestsLengthPointer,
                                   static_cast<uint16_t>(bytes.size())));
  }
  for (int room_id = 0; room_id < room_limit; ++room_id) {
    if (dirty_rooms[room_id] != nullptr) {
      const_cast<Room*>(dirty_rooms[room_id])->ClearChestsDirty();
    }
  }
  return absl::OkStatus();
}

absl::Status SaveAllChests(Rom* rom, absl::Span<const Room> rooms) {
  return SaveAllChestsImpl(rom, static_cast<int>(rooms.size()),
                           [&rooms](int room_id) { return &rooms[room_id]; });
}

absl::Status SaveAllChests(Rom* rom, int room_count,
                           const std::function<const Room*(int)>& room_lookup) {
  return SaveAllChestsImpl(rom, room_count, room_lookup);
}

template <typename RoomLookup>
absl::Status SaveAllPotItemsImpl(
    Rom* rom, int room_count, RoomLookup&& room_lookup,
    const DungeonStreamLayout* repack_layout = nullptr) {
  if (!rom || !rom->is_loaded()) {
    return absl::InvalidArgumentError("ROM not loaded");
  }
  const auto& rom_data = rom->vector();
  if (kRoomItemsPointers + (kNumberOfRooms * 2) >
      static_cast<int>(rom_data.size())) {
    return absl::OutOfRangeError("Room items pointer table out of range");
  }

  const int room_limit = std::min(room_count, kNumberOfRooms);
  if (repack_layout != nullptr) {
    std::vector<DungeonStreamReplacement> replacements;
    for (int room_id = 0; room_id < room_limit; ++room_id) {
      const Room* room = room_lookup(room_id);
      if (room == nullptr || !room->pot_items_dirty()) {
        continue;
      }

      DungeonStreamReplacement replacement;
      replacement.room_id = static_cast<uint32_t>(room_id);
      replacement.encoded_stream.reserve(room->GetPotItems().size() * 3 + 2);
      for (const PotItem& item : room->GetPotItems()) {
        replacement.encoded_stream.push_back(item.position & 0xFF);
        replacement.encoded_stream.push_back((item.position >> 8) & 0xFF);
        replacement.encoded_stream.push_back(item.item);
      }
      replacement.encoded_stream.push_back(0xFF);
      replacement.encoded_stream.push_back(0xFF);
      replacements.push_back(std::move(replacement));
    }
    if (replacements.empty()) {
      return absl::OkStatus();
    }

    ASSIGN_OR_RETURN(const DungeonStreamInventory inventory,
                     InventoryDungeonStreams(*rom, *repack_layout));
    ASSIGN_OR_RETURN(const DungeonStreamWritePlan plan,
                     PlanDungeonStreamRepack(inventory, replacements));
    RETURN_IF_ERROR(ApplyDungeonStreamWritePlan(rom, plan));
    for (const DungeonStreamReplacement& replacement : replacements) {
      if (const Room* room = room_lookup(replacement.room_id);
          room != nullptr) {
        const_cast<Room*>(room)->ClearPotItemsDirty();
      }
    }
    return absl::OkStatus();
  }

  struct PendingPotItemWrite {
    int room_id = -1;
    int address = -1;
    std::vector<uint8_t> bytes;
  };

  // Build and validate every dirty write before touching the ROM. A later
  // shared/overfull stream must not leave earlier rooms partially written.
  std::vector<PendingPotItemWrite> pending_writes;
  for (int room_id = 0; room_id < room_limit; ++room_id) {
    const Room* room = room_lookup(room_id);
    if (room == nullptr || !room->pot_items_dirty()) {
      continue;
    }

    ASSIGN_OR_RETURN(const PhysicalStreamInfo stream_info,
                     GetPotItemStreamInfo(rom_data, room_id));
    if (stream_info.shared) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Room %d pot item stream at PC 0x%06X is shared; repacking is "
          "required",
          room_id, stream_info.address));
    }
    if (stream_info.capacity() <= 0) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Room %d pot item stream has no safe physical boundary", room_id));
    }

    PendingPotItemWrite pending;
    pending.room_id = room_id;
    pending.address = stream_info.address;
    for (const auto& pi : room->GetPotItems()) {
      pending.bytes.push_back(pi.position & 0xFF);
      pending.bytes.push_back((pi.position >> 8) & 0xFF);
      pending.bytes.push_back(pi.item);
    }
    pending.bytes.push_back(0xFF);
    pending.bytes.push_back(0xFF);
    if (static_cast<int>(pending.bytes.size()) > stream_info.capacity()) {
      return absl::ResourceExhaustedError(absl::StrFormat(
          "Room %d pot item data too large! Size: %d, Available: %d", room_id,
          static_cast<int>(pending.bytes.size()), stream_info.capacity()));
    }
    pending_writes.push_back(std::move(pending));
  }

  for (const auto& pending : pending_writes) {
    const bool data_changed =
        !std::equal(pending.bytes.begin(), pending.bytes.end(),
                    rom_data.begin() + pending.address);
    if (data_changed) {
      RETURN_IF_ERROR(rom->WriteVector(pending.address, pending.bytes));
    }
  }

  for (const auto& pending : pending_writes) {
    if (const Room* room = room_lookup(pending.room_id); room != nullptr) {
      const_cast<Room*>(room)->ClearPotItemsDirty();
    }
  }
  return absl::OkStatus();
}

absl::Status SaveAllPotItems(Rom* rom, absl::Span<const Room> rooms) {
  return SaveAllPotItemsImpl(rom, static_cast<int>(rooms.size()),
                             [&rooms](int room_id) { return &rooms[room_id]; });
}

absl::Status SaveAllPotItems(Rom* rom, absl::Span<const Room> rooms,
                             const DungeonStreamLayout* repack_layout) {
  return SaveAllPotItemsImpl(
      rom, static_cast<int>(rooms.size()),
      [&rooms](int room_id) { return &rooms[room_id]; }, repack_layout);
}

absl::Status SaveAllPotItems(
    Rom* rom, int room_count,
    const std::function<const Room*(int)>& room_lookup) {
  return SaveAllPotItemsImpl(rom, room_count, room_lookup);
}

absl::Status SaveAllPotItems(Rom* rom, int room_count,
                             const std::function<const Room*(int)>& room_lookup,
                             const DungeonStreamLayout* repack_layout) {
  return SaveAllPotItemsImpl(rom, room_count, room_lookup, repack_layout);
}

void Room::LoadBlocks() {
  auto rom_data = rom()->vector();

  // Read blocks length
  int blocks_count =
      (rom_data[kBlocksLength + 1] << 8) | rom_data[kBlocksLength];

  LOG_DEBUG("Room", "LoadBlocks: room_id=%d, blocks_count=%d", room_id_,
            blocks_count);

  // Load block data from the four data regions.
  //
  // `kBlocksPointer1..4` are 3-byte SNES long-address operand slots
  // embedded in bank_02's LDA.l instructions (`$02:DAF9..$02:DB2E`),
  // not inline data offsets. Each operand encodes
  // `data_base + region_offset` where data_base is the SNES address
  // of `SpecialUnderworldObjects_pushable_block` ($04:F1DE in vanilla)
  // and region_offset is `r * 0x80`. The previous code read directly
  // from the operand slots, so it was decoding the LDA.l opcode
  // operand bytes as block data — silently corrupting every block on
  // load. `SaveAllBlocks` has always dereferenced these correctly;
  // load now matches.
  const int kRegionSize = 0x80;
  const int kPointerSlots[4] = {kBlocksPointer1, kBlocksPointer2,
                                kBlocksPointer3, kBlocksPointer4};
  std::vector<uint8_t> blocks_data(blocks_count, 0);
  for (int r = 0; r < 4; ++r) {
    const int slot = kPointerSlots[r];
    if (slot + 2 >= static_cast<int>(rom_data.size())) {
      LOG_WARN("Room", "LoadBlocks: pointer slot %d out of range", r);
      return;
    }
    const absl::Status operand_status =
        ValidateBlocksLoaderPointerOperand(rom_data, slot);
    if (!operand_status.ok()) {
      LOG_WARN("Room", "LoadBlocks: %s",
               std::string(operand_status.message()).c_str());
      return;
    }
    const int snes =
        (rom_data[slot + 2] << 16) | (rom_data[slot + 1] << 8) | rom_data[slot];
    const int pc = SnesToPc(snes);
    const int off = r * kRegionSize;
    const int len = std::min(kRegionSize, blocks_count - off);
    if (len <= 0)
      break;
    if (pc < 0 || pc + len > static_cast<int>(rom_data.size())) {
      LOG_WARN("Room", "LoadBlocks: region %d data out of range", r);
      return;
    }
    std::copy_n(rom_data.begin() + pc, len, blocks_data.begin() + off);
  }

  // Avoid duplication if LoadBlocks is called multiple times. Do this only
  // after the ROM pointer operands and data regions are known-good so a guard
  // failure cannot make existing in-memory block objects vanish.
  tile_objects_.erase(
      std::remove_if(tile_objects_.begin(), tile_objects_.end(),
                     [](const RoomObject& obj) {
                       return (obj.options() & ObjectOption::Block) !=
                              ObjectOption::Nothing;
                     }),
      tile_objects_.end());

  // Parse blocks for this room (4 bytes per block entry).
  //
  // Vanilla scan (bank_01.asm:1162) walks the flat 396-byte table linearly
  // matching on room_id; there is no per-room 0xFFFF terminator. The
  // previous "break on b3==0xFF && b4==0xFF after room_id match" guard was
  // a phantom — it never fired in vanilla and would have prematurely
  // truncated a room's block list if a future tombstone happened to share
  // its room_id. Removed alongside the decoder fix.
  for (int i = 0; i + 3 < blocks_count; i += 4) {
    PushableBlockBytes bytes{blocks_data[i], blocks_data[i + 1],
                             blocks_data[i + 2], blocks_data[i + 3]};
    const PushableBlockEntry entry = DecodePushableBlockEntry(bytes);
    if (entry.room_id != room_id_)
      continue;

    RoomObject block_obj(0x0E00, entry.px, entry.py, 0, entry.draw_layer);
    block_obj.SetRom(rom_);
    block_obj.set_options(ObjectOption::Block);
    block_obj.set_block_behavior_layer(entry.behavior_layer);
    // Capture the entry's slot index in the global buffer so
    // SaveAllBlocks can emit entries in vanilla authoring order
    // (interleaved across rooms; sorting by room_id would reshuffle
    // bytes and break byte equality on no-op saves).
    block_obj.set_block_load_order(i / 4);
    tile_objects_.push_back(block_obj);

    LOG_DEBUG("Room", "Loaded block at (%d,%d) draw_layer=%d behavior_layer=%d",
              entry.px, entry.py, entry.draw_layer, entry.behavior_layer);
  }
  blocks_loaded_ = true;
}

void Room::LoadPotItems() {
  if (!rom_ || !rom_->is_loaded())
    return;
  auto rom_data = rom()->vector();
  pot_items_.clear();
  pot_items_loaded_ = false;

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

  pot_items_loaded_ = true;
}

void Room::LoadPits() {
  auto rom_data = rom()->vector();

  // The legacy symbol `kPitCount` is the LDX.w immediate at PC 0x394A6
  // — the **maximum X offset** in the runtime CMP loop, not an entry
  // count. Total entries = `(max_offset / 2) + 1`. This function does
  // not actually consume the table contents (yaze has no editable
  // surface for pit-damage gating); it just resolves the dereferenced
  // address for diagnostic logging. See
  // `test/integration/zelda3/dungeon_save_region_test.cc` for the
  // format-pinning tests and `memory/project_dungeon_pit_audit.md`
  // for the audit conclusion.
  const int max_offset = rom_data[kPitCount];
  const int pit_entries = max_offset / 2 + 1;

  const int pit_ptr = (rom_data[kPitPointer + 2] << 16) |
                      (rom_data[kPitPointer + 1] << 8) | rom_data[kPitPointer];

  LOG_DEBUG("Room",
            "LoadPits: room_id=%d, RoomsWithPitDamage entries=%d, "
            "table_snes=0x%06X",
            room_id_, pit_entries, pit_ptr);

  // The per-room pit DESTINATION (where Link goes when falling through
  // a non-damaging pit) is unrelated to the global RoomsWithPitDamage
  // table read above. It lives in the room header and was loaded into
  // `pits_` (target room + target_layer) by `LoadRoomFromRom`. The
  // round-trip for that state goes through the room header save path,
  // not `SaveAllPits`.
  LOG_DEBUG("Room", "Per-room pit destination: target=%d, target_layer=%d",
            pits_.target, pits_.target_layer);
}

// ============================================================================
// Object Limit Counting (ZScream Feature Parity)
// ============================================================================

std::map<DungeonLimit, int> Room::GetLimitedObjectCounts() const {
  auto counts = CreateLimitCounter();

  // Count sprites
  counts[DungeonLimit::kSprites] = static_cast<int>(sprites_.size());

  // Count overlords (sprites with ID > 0x40 are overlords in ALTTP)
  for (const auto& sprite : sprites_) {
    if (sprite.IsOverlord()) {
      counts[DungeonLimit::Overlords]++;
    }
  }

  // Count chests
  counts[DungeonLimit::kChests] = static_cast<int>(chests_in_room_.size());

  // Count doors (total and special)
  counts[DungeonLimit::kDoors] = static_cast<int>(doors_.size());
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
  counts[DungeonLimit::StairsTransition] =
      static_cast<int>(z3_staircases_.size());

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
      if ((obj.id_ >= 0x130 && obj.id_ <= 0x135) || obj.id_ == 0x139 ||
          obj.id_ == 0x13A || obj.id_ == 0x13B) {
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
