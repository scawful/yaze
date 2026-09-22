#include "zelda3/dungeon/game_tilemap_comparison.h"

#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "app/gfx/render/background_buffer.h"
#include "app/gfx/types/snes_palette.h"
#include "rom/rom.h"
#include "zelda3/dungeon/object_drawer.h"
#include "zelda3/dungeon/room.h"

namespace yaze::zelda3 {
namespace {

constexpr uint16_t kTileNumberBits = 0x03FF;
constexpr uint16_t kPaletteBits = 0x1C00;
constexpr uint16_t kPriorityBit = 0x2000;
constexpr uint16_t kHorizontalFlipBit = 0x4000;
constexpr uint16_t kVerticalFlipBit = 0x8000;

// Rebuilds the tile word ObjectDrawer::PushTrace recorded.
uint16_t TraceWord(const ObjectDrawer::TileTrace& trace) {
  uint16_t word = trace.tile_id & kTileNumberBits;
  word |= static_cast<uint16_t>((trace.flags >> 3) & 0x7) << 10;
  if (trace.flags & 0x4)
    word |= kPriorityBit;
  if (trace.flags & 0x1)
    word |= kHorizontalFlipBit;
  if (trace.flags & 0x2)
    word |= kVerticalFlipBit;
  return word;
}

bool IsSpecialTableObject(const RoomObject& object) {
  return (object.options() & ObjectOption::Torch) != ObjectOption::Nothing ||
         (object.options() & ObjectOption::Block) != ObjectOption::Nothing;
}

}  // namespace

absl::StatusOr<RoomTilemaps> ParseGameRoomTilemaps(
    const std::vector<uint8_t>& bytes) {
  if (bytes.size() != kGameRoomTilemapBytes) {
    return absl::InvalidArgumentError(
        absl::StrCat("Room tilemap capture must be ", kGameRoomTilemapBytes,
                     " bytes, got ", bytes.size()));
  }
  RoomTilemaps maps;
  maps.bg1.resize(kRoomTilemapWords);
  maps.bg2.resize(kRoomTilemapWords);
  for (size_t i = 0; i < kRoomTilemapWords; ++i) {
    maps.bg1[i] = static_cast<uint16_t>(bytes[i * 2] | (bytes[i * 2 + 1] << 8));
    const size_t j = kRoomTilemapWords * 2 + i * 2;
    maps.bg2[i] = static_cast<uint16_t>(bytes[j] | (bytes[j + 1] << 8));
  }
  return maps;
}

RoomTilemaps ComposeYazeRoomTilemaps(const Room& room) {
  auto compose = [](const gfx::BackgroundBuffer& layout,
                    const gfx::BackgroundBuffer& objects) {
    std::vector<uint16_t> out(kRoomTilemapWords, 0);
    const auto& layout_words = layout.buffer();
    const auto& object_words = objects.buffer();
    for (size_t i = 0; i < kRoomTilemapWords; ++i) {
      const uint16_t object_word =
          i < object_words.size() ? object_words[i] : 0;
      const uint16_t layout_word =
          i < layout_words.size() ? layout_words[i] : 0;
      out[i] = object_word != 0 ? object_word : layout_word;
    }
    return out;
  };
  RoomTilemaps maps;
  maps.bg1 = compose(room.bg1_buffer(), room.object_bg1_buffer());
  maps.bg2 = compose(room.bg2_buffer(), room.object_bg2_buffer());
  return maps;
}

bool GameHidesObjectOnRoomLoad(int tag1, int tag2, const RoomObject& object) {
  if (object.id_ != 0xF99) {
    return false;
  }
  auto hides = [](int tag) {
    return tag == 0x27 || tag == 0x3C || tag == 0x3E ||
           (tag >= 0x29 && tag <= 0x32);
  };
  return hides(tag1) || hides(tag2);
}

TileOwners ComputeObjectTileOwners(Rom* rom, int room_id,
                                   const std::vector<RoomObject>& objects,
                                   const RoomTilemaps& yaze,
                                   const std::vector<bool>& hidden) {
  TileOwners owners;
  owners.bg1.assign(kRoomTilemapWords, -1);
  owners.bg2.assign(kRoomTilemapWords, -1);
  if (yaze.bg1.size() != kRoomTilemapWords ||
      yaze.bg2.size() != kRoomTilemapWords) {
    return owners;
  }

  std::vector<ObjectDrawer::TileTrace> trace;
  ObjectDrawer drawer(rom, room_id, nullptr);
  drawer.SetTraceCollector(&trace, /*trace_only=*/true);
  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  gfx::PaletteGroup palette_group;
  std::vector<uint16_t> owner_words_bg1(kRoomTilemapWords, 0);
  std::vector<uint16_t> owner_words_bg2(kRoomTilemapWords, 0);

  // Same order as Room's object pass: the three room object lists in list
  // order. Torches and pushable blocks are drawn from their own tables and
  // are left without owners.
  for (int list = 0; list < 3; ++list) {
    for (size_t index = 0; index < objects.size(); ++index) {
      const RoomObject& source = objects[index];
      if (IsSpecialTableObject(source) ||
          (index < hidden.size() && hidden[index])) {
        continue;
      }
      int list_index = source.GetLayerValue();
      if (list_index > 2) {
        list_index = 2;
      }
      if (list_index != list) {
        continue;
      }
      RoomObject object = source;
      object.SetRom(rom);
      object.layer_ =
          MapRoomObjectListIndexToDrawLayer(static_cast<uint8_t>(list_index));
      trace.clear();
      if (!drawer.DrawObject(object, bg1, bg2, palette_group).ok()) {
        continue;
      }
      for (const auto& tile : trace) {
        if (tile.x_tile < 0 || tile.y_tile < 0 ||
            tile.x_tile >= kRoomTilemapSize ||
            tile.y_tile >= kRoomTilemapSize) {
          continue;
        }
        const size_t position =
            static_cast<size_t>(tile.y_tile) * kRoomTilemapSize + tile.x_tile;
        const bool bg2_tile = tile.layer == RoomObject::BG2;
        (bg2_tile ? owners.bg2 : owners.bg1)[position] =
            static_cast<int>(index);
        (bg2_tile ? owner_words_bg2 : owner_words_bg1)[position] =
            TraceWord(tile);
      }
    }
  }

  // Keep an owner only where its own word is what yaze shows; otherwise a
  // later draw (a door, another object) produced the final tile.
  for (size_t position = 0; position < kRoomTilemapWords; ++position) {
    if (owners.bg1[position] >= 0 &&
        owner_words_bg1[position] != yaze.bg1[position]) {
      owners.bg1[position] = -1;
    }
    if (owners.bg2[position] >= 0 &&
        owner_words_bg2[position] != yaze.bg2[position]) {
      owners.bg2[position] = -1;
    }
  }
  return owners;
}

RoomTilemapCheck CompareRoomTilemaps(int room_id, const RoomTilemaps& game,
                                     const RoomTilemaps& yaze,
                                     const TileOwners& owners,
                                     const std::vector<RoomObject>& objects) {
  RoomTilemapCheck check;
  check.room_id = room_id;
  check.placements.resize(objects.size());
  for (size_t i = 0; i < objects.size(); ++i) {
    check.placements[i].object_index = i;
    check.placements[i].object_id = objects[i].id_;
  }

  auto compare_layer = [&](int layer, const std::vector<uint16_t>& game_words,
                           const std::vector<uint16_t>& yaze_words,
                           const std::vector<int>& layer_owners,
                           int* matching) {
    for (size_t position = 0; position < kRoomTilemapWords; ++position) {
      const int owner =
          position < layer_owners.size() ? layer_owners[position] : -1;
      const bool same = game_words[position] == yaze_words[position];
      if (owner >= 0 && static_cast<size_t>(owner) < objects.size()) {
        auto& placement = check.placements[static_cast<size_t>(owner)];
        ++placement.tiles_owned;
        if (!same) {
          ++placement.tiles_different;
          placement.difference_bits |= static_cast<uint16_t>(
              game_words[position] ^ yaze_words[position]);
        }
      }
      if (same) {
        ++*matching;
        continue;
      }
      TileDifference difference;
      difference.layer = layer;
      difference.x = static_cast<int>(position % kRoomTilemapSize);
      difference.y = static_cast<int>(position / kRoomTilemapSize);
      difference.game = game_words[position];
      difference.yaze = yaze_words[position];
      difference.owner = owner;
      if (owner < 0) {
        ++check.unowned_differences;
      }
      check.differences.push_back(difference);
    }
  };
  if (game.bg1.size() == kRoomTilemapWords &&
      yaze.bg1.size() == kRoomTilemapWords) {
    compare_layer(1, game.bg1, yaze.bg1, owners.bg1, &check.bg1_matching);
  }
  if (game.bg2.size() == kRoomTilemapWords &&
      yaze.bg2.size() == kRoomTilemapWords) {
    compare_layer(2, game.bg2, yaze.bg2, owners.bg2, &check.bg2_matching);
  }
  return check;
}

std::string DescribeTileWordDifference(uint16_t xor_bits) {
  std::vector<std::string> parts;
  if (xor_bits & kTileNumberBits)
    parts.push_back("tile");
  if (xor_bits & kPaletteBits)
    parts.push_back("palette");
  if (xor_bits & kPriorityBit)
    parts.push_back("priority");
  if (xor_bits & (kHorizontalFlipBit | kVerticalFlipBit)) {
    parts.push_back("flip");
  }
  return parts.empty() ? "none" : absl::StrJoin(parts, ", ");
}

}  // namespace yaze::zelda3
