#include "zelda3/zelda3_labels.h"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "zelda3/common.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/sprite/overlord.h"
#include "zelda3/sprite/sprite.h"

namespace yaze::zelda3 {

// Room names - reuse existing kRoomNames array
const std::vector<std::string>& Zelda3Labels::GetRoomNames() {
  static std::vector<std::string> room_names;
  if (room_names.empty()) {
    for (const auto& name : kRoomNames) {
      room_names.emplace_back(name);
    }
  }
  return room_names;
}

// Entrance names - reuse existing kEntranceNames array
const std::vector<std::string>& Zelda3Labels::GetEntranceNames() {
  static std::vector<std::string> entrance_names;
  if (entrance_names.empty()) {
    for (const auto& name : kEntranceNames) {
      entrance_names.emplace_back(name);
    }
  }
  return entrance_names;
}

// Sprite names - reuse existing kSpriteDefaultNames array
const std::vector<std::string>& Zelda3Labels::GetSpriteNames() {
  static std::vector<std::string> sprite_names;
  if (sprite_names.empty()) {
    for (const auto& name : kSpriteDefaultNames) {
      sprite_names.push_back(name);
    }
  }
  return sprite_names;
}

// Overlord names - reuse existing kOverlordNames array
const std::vector<std::string>& Zelda3Labels::GetOverlordNames() {
  static std::vector<std::string> overlord_names;
  if (overlord_names.empty()) {
    for (const auto& name : kOverlordNames) {
      overlord_names.push_back(name);
    }
  }
  return overlord_names;
}

// Overworld map names (64 Light World + 64 Dark World + 32 Special Areas)
const std::vector<std::string>& Zelda3Labels::GetOverworldMapNames() {
  static const std::vector<std::string> map_names = {
      // Light World (0x00-0x3F)
      "Lost Woods", "Master Sword Pedestal", "Castle Courtyard", "Link's House",
      "Eastern Palace", "Desert Palace", "Hyrule Castle", "Witch's Hut",
      "Kakariko Village", "Death Mountain", "Tower of Hera", "Spectacle Rock",
      "Graveyard", "Sanctuary", "Lake Hylia", "Desert of Mystery",
      "Eastern Ruins", "Zora's Domain", "Catfish", "Dam", "Potion Shop",
      "Kakariko Well", "Blacksmith", "Sick Kid", "Library", "Mushroom",
      "Magic Bat", "Fairy Fountain", "Fortune Teller", "Lake Shop", "Bomb Shop",
      "Cave 45", "Checkerboard Cave", "Mini Moldorm Cave", "Ice Rod Cave",
      "Bonk Rocks", "Bottle Merchant", "Sahasrahla's Hut", "Chicken House",
      "Aginah's Cave", "Dam Exterior", "Mimic Cave Exterior", "Waterfall Fairy",
      "Pyramid", "Fat Fairy", "Spike Cave", "Hookshot Cave", "Graveyard Ledge",
      "Dark Lumberjacks", "Bumper Cave", "Skull Woods 1", "Skull Woods 2",
      "Skull Woods 3", "Skull Woods 4", "Skull Woods 5", "Skull Woods 6",
      "Skull Woods 7", "Skull Woods 8", "Ice Palace Exterior",
      "Misery Mire Exterior", "Palace of Darkness Exterior",
      "Swamp Palace Exterior", "Turtle Rock Exterior", "Thieves' Town Exterior",

      // Dark World (0x40-0x7F)
      "Dark Woods", "Dark Chapel", "Dark Castle", "Dark Shields", "Dark Palace",
      "Dark Desert", "Dark Castle Gate", "Dark Witch", "Dark Village",
      "Dark Mountain", "Dark Tower", "Dark Rocks", "Dark Graveyard",
      "Dark Sanctuary", "Dark Lake", "Dark Desert South", "Dark Eastern",
      "Dark Zora", "Dark Catfish", "Dark Dam", "Dark Shop", "Dark Well",
      "Dark Blacksmith", "Dark Sick Kid", "Dark Library", "Dark Mushroom",
      "Dark Bat", "Dark Fountain", "Dark Fortune", "Dark Lake Shop",
      "Dark Bomb Shop", "Dark Cave 45", "Dark Checker", "Dark Mini Moldorm",
      "Dark Ice Rod", "Dark Bonk", "Dark Bottle", "Dark Sahasrahla",
      "Dark Chicken", "Dark Aginah", "Dark Dam Exit", "Dark Mimic Exit",
      "Dark Waterfall", "Pyramid Top", "Dark Fat Fairy", "Dark Spike Cave",
      "Dark Hookshot", "Dark Graveyard Ledge", "Lumberjack House",
      "Dark Bumper", "Skull Woods A", "Skull Woods B", "Skull Woods C",
      "Skull Woods D", "Skull Woods E", "Skull Woods F", "Skull Woods G",
      "Skull Woods H", "Ice Palace Entry", "Misery Mire Entry",
      "Palace of Darkness Entry", "Swamp Palace Entry", "Turtle Rock Entry",
      "Thieves' Town Entry",

      // Special Areas (0x80-0x9F)
      "Special Area 1", "Special Area 2", "Special Area 3", "Special Area 4",
      "Special Area 5", "Special Area 6", "Special Area 7", "Special Area 8",
      "Special Area 9", "Special Area 10", "Special Area 11", "Special Area 12",
      "Special Area 13", "Special Area 14", "Special Area 15",
      "Special Area 16", "Special Area 17", "Special Area 18",
      "Special Area 19", "Special Area 20", "Special Area 21",
      "Special Area 22", "Special Area 23", "Special Area 24",
      "Special Area 25", "Special Area 26", "Special Area 27",
      "Special Area 28", "Special Area 29", "Special Area 30",
      "Special Area 31", "Special Area 32"};
  return map_names;
}

// Item names (complete item list)
const std::vector<std::string>& Zelda3Labels::GetItemNames() {
  static const std::vector<std::string> item_names = {
      "None",
      "Fighter Sword",
      "Master Sword",
      "Tempered Sword",
      "Golden Sword",
      "Fighter Shield",
      "Fire Shield",
      "Mirror Shield",
      "Fire Rod",
      "Ice Rod",
      "Hammer",
      "Hookshot",
      "Bow",
      "Boomerang",
      "Powder",
      "Bee Badge",
      "Bombos Medallion",
      "Ether Medallion",
      "Quake Medallion",
      "Lamp",
      "Shovel",
      "Flute",
      "Somaria Cane",
      "Bottle",
      "Heart Piece",
      "Byrna Cane",
      "Cape",
      "Mirror",
      "Power Glove",
      "Titan Mitt",
      "Book of Mudora",
      "Zora Flippers",
      "Moon Pearl",
      "Crystal",
      "Bug Net",
      "Blue Mail",
      "Red Mail",
      "Key",
      "Compass",
      "Heart Container",
      "Bomb",
      "3 Bombs",
      "Mushroom",
      "Red Boomerang",
      "Red Potion",
      "Green Potion",
      "Blue Potion",
      "Red Potion (Refill)",
      "Green Potion (Refill)",
      "Blue Potion (Refill)",
      "10 Bombs",
      "Big Key",
      "Map",
      "1 Rupee",
      "5 Rupees",
      "20 Rupees",
      "Pendant of Courage",
      "Pendant of Wisdom",
      "Pendant of Power",
      "Bow and Arrows",
      "Silver Arrows Upgrade",
      "Bee",
      "Fairy",
      "Heart Container (Boss)",
      "Heart",
      "1 Arrow",
      "10 Arrows",
      "Magic",
      "Small Magic",
      "300 Rupees",
      "20 Rupees (Green)",
      "100 Rupees",
      "50 Rupees",
      "Heart Container (Sanctuary)",
      "Arrow Refill (5)",
      "Arrow Refill (10)",
      "Bomb Refill (1)",
      "Bomb Refill (4)",
      "Bomb Refill (8)",
      "Blue Shield (Refill)",
      "Magic Upgrade (1/2)",
      "Magic Upgrade (1/4)",
      "Programmable Item 1",
      "Programmable Item 2",
      "Programmable Item 3",
      "Silvers",
      "Rupoor",
      "Null Item",
      "Red Clock",
      "Blue Clock",
      "Green Clock",
      "Progressive Sword",
      "Progressive Shield",
      "Progressive Armor",
      "Progressive Lifting Glove",
      "RNG Item (Single)",
      "RNG Item (Multi)",
      "Progressive Bow",
      "Progressive Bow (Alt)",
      "Aga Pendant",
      "Pendant (Green)",
      "Blue Pendant",
      "Good Bee",
      "Tossed Key"};
  return item_names;
}

// Music track names
const std::vector<std::string>& Zelda3Labels::GetMusicTrackNames() {
  static const std::vector<std::string> music_names = {
      "Nothing",
      "Light World",
      "Beginning",
      "Rabbit",
      "Forest",
      "Intro",
      "Town",
      "Warp",
      "Dark World",
      "Master Sword",
      "File Select",
      "Soldier",
      "Boss",
      "Dark World Death Mountain",
      "Minigame",
      "Skull Woods",
      "Indoor",
      "Cave 1",
      "Zelda's Rescue",
      "Crystal",
      "Shop",
      "Cave 2",
      "Game Over",
      "Boss Victory",
      "Sanctuary",
      "Boss Victory (Short)",
      "Dark World Woods",
      "Pendant",
      "Ganon's Message",
      "Hyrule Castle",
      "Light World Death Mountain",
      "Eastern Palace",
      "Desert Palace",
      "Agahnim's Theme",
      "Damp Dungeon",
      "Ganon Reveals",
      "Confrontation",
      "Ganon's Theme",
      "Triforce",
      "Credits",
      "Unused",
      "Unused",
      "Unused",
      "Unused",
      "Unused",
      "Unused",
      "Unused",
      "Unused"};
  return music_names;
}

// Graphics sheet names
const std::vector<std::string>& Zelda3Labels::GetGraphicsSheetNames() {
  static const std::vector<std::string> gfx_names = {"Sprite Sheet 0",
                                                     "Sprite Sheet 1",
                                                     "Sprite Sheet 2",
                                                     "Sprite Sheet 3",
                                                     "Sprite Sheet 4",
                                                     "Sprite Sheet 5",
                                                     "Sprite Sheet 6",
                                                     "Sprite Sheet 7",
                                                     "Sprite Sheet 8",
                                                     "Sprite Sheet 9",
                                                     "Sprite Sheet A",
                                                     "Sprite Sheet B",
                                                     "Sprite Sheet C",
                                                     "Sprite Sheet D",
                                                     "Sprite Sheet E",
                                                     "Sprite Sheet F",
                                                     "Link's Sprites",
                                                     "Sword Sprites",
                                                     "Shield Sprites",
                                                     "Common Sprites",
                                                     "Boss Sprites",
                                                     "NPC Sprites",
                                                     "Enemy Sprites 1",
                                                     "Enemy Sprites 2",
                                                     "Item Sprites",
                                                     "Dungeon Objects",
                                                     "Overworld Objects",
                                                     "Interface",
                                                     "Font",
                                                     "Credits",
                                                     "Unused",
                                                     "Unused"};
  return gfx_names;
}

// Room object names - these are large, so we'll delegate to a helper
namespace {
std::vector<std::string> ConvertArrayToVector(const char** array, size_t size) {
  std::vector<std::string> result;
  result.reserve(size);
  for (size_t i = 0; i < size; ++i) {
    result.emplace_back(array[i]);
  }
  return result;
}
}  // namespace

const std::vector<std::string>& Zelda3Labels::GetType1RoomObjectNames() {
  static const std::vector<std::string> names = []() {
    std::vector<std::string> result;
    // Note: Type1RoomObjectNames is constexpr, we need to count its size
    // For now, we'll add known objects. In full implementation,
    // we'd import from room_object.h
    result = {
        "Ceiling ↔",
        "Wall (top, north) ↔",
        "Wall (top, south) ↔",
        "Wall (bottom, north) ↔",
        "Wall (bottom, south) ↔",
        "Wall columns (north) ↔",
        "Wall columns (south) ↔",
        "Deep wall (north) ↔",
        "Deep wall (south) ↔",
        "Diagonal wall A ◤ (top) ↔",
        "Diagonal wall A ◣ (top) ↔",
        "Diagonal wall A ◥ (top) ↔",
        "Diagonal wall A ◢ (top) ↔",
        // ... Add all Type1 objects here
    };
    return result;
  }();
  return names;
}

const std::vector<std::string>& Zelda3Labels::GetType2RoomObjectNames() {
  static const std::vector<std::string> names = []() {
    std::vector<std::string> result;
    // Add Type2 room objects
    result = {"Type2 Object 1", "Type2 Object 2" /* ... */};
    return result;
  }();
  return names;
}

const std::vector<std::string>& Zelda3Labels::GetType3RoomObjectNames() {
  static const std::vector<std::string> names = []() {
    std::vector<std::string> result;
    // Add Type3 room objects
    result = {"Type3 Object 1", "Type3 Object 2" /* ... */};
    return result;
  }();
  return names;
}

// Room effect names - reuse existing RoomEffect array
const std::vector<std::string>& Zelda3Labels::GetRoomEffectNames() {
  static std::vector<std::string> effect_names;
  if (effect_names.empty()) {
    for (const auto& name : RoomEffect) {
      effect_names.push_back(name);
    }
  }
  return effect_names;
}

// Room tag names
const std::vector<std::string>& Zelda3Labels::GetRoomTagNames() {
  static const std::vector<std::string> tag_names = {
      "No Tag", "NW",    "NE",       "SW",       "SE",   "West", "East",
      "North",  "South", "Entrance", "Treasure", "Boss", "Dark"};
  return tag_names;
}

// Tile type names
const std::vector<std::string>& Zelda3Labels::GetTileTypeNames() {
  static const std::vector<std::string> tile_names = {
      "Nothing (standard floor)",
      "Nothing (unused?)",
      "Collision",
      "Collision (unknown types)",
      "Collision",
      "Collision (unused?)",
      "Collision",
      "Collision",
      "Deep water",
      "Shallow water",
      "Unknown (near water/pit edges)",
      "Collision (water/pit edges)",
      "Overlay mask",
      "Spike floor",
      "GT ice",
      "Ice palace ice",
      "Slope ◤",
      "Slope ◥",
      "Slope ◣",
      "Slope ◢",
      "Nothing (unused?)",
      "Nothing (unused?)",
      "Nothing (unused?)",
      "Slope ◤",
      "Slope ◥",
      "Slope ◣",
      "Slope ◢",
      "Layer swap",
      "Pit",
      "Manual stairs",
      "Pot switch",
      "Pressure switch",
      "Blocks switch (chest, PoD, walls)",
      "Layer toggle",
      "Layer 2 overlay",
      "North single-layer auto stairs",
      "North layer swap auto stairs",
      "South single-layer auto stairs",
      "South layer swap auto stairs",
      "North/south layer swap auto stairs",
      "North/south single-layer auto stairs",
      "West single-layer auto stairs",
      "West layer swap auto stairs",
      "East single-layer auto stairs",
      "East layer swap auto stairs",
      "East/west layer swap auto stairs",
      "East/west single-layer auto stairs",
      "Nothing (stairs edge)",
      "Straight inter-room stairs south/up",
      "Straight inter-room stairs north/down",
      "Straight inter-room stairs south/down 2",
      "Straight inter-room stairs north/up 2",
      "Star tile (inactive on GBA)",
      "Collision (near stairs)",
      "Warp tile",
      "Square corners ⌜⌝⌞⌟",
      "Thick corner ⌜",
      "Thick corner ⌝",
      "Thick corner ⌞",
      "Thick corner ⌟",
      "Roof/grass tiles?",
      "Spike floor"};
  return tile_names;
}

// Convert all labels to structured map for project embedding
std::unordered_map<std::string, std::unordered_map<std::string, std::string>>
Zelda3Labels::ToResourceLabels() {
  std::unordered_map<std::string, std::unordered_map<std::string, std::string>>
      labels;

  // Rooms
  const auto& rooms = GetRoomNames();
  for (size_t i = 0; i < rooms.size(); ++i) {
    labels["room"][std::to_string(i)] = rooms[i];
  }

  // Entrances
  const auto& entrances = GetEntranceNames();
  for (size_t i = 0; i < entrances.size(); ++i) {
    labels["entrance"][std::to_string(i)] = entrances[i];
  }

  // Sprites
  const auto& sprites = GetSpriteNames();
  for (size_t i = 0; i < sprites.size(); ++i) {
    labels["sprite"][std::to_string(i)] = sprites[i];
  }

  // Overlords
  const auto& overlords = GetOverlordNames();
  for (size_t i = 0; i < overlords.size(); ++i) {
    labels["overlord"][std::to_string(i)] = overlords[i];
  }

  // Overworld maps
  const auto& maps = GetOverworldMapNames();
  for (size_t i = 0; i < maps.size(); ++i) {
    labels["overworld_map"][std::to_string(i)] = maps[i];
  }

  // Items
  const auto& items = GetItemNames();
  for (size_t i = 0; i < items.size(); ++i) {
    labels["item"][std::to_string(i)] = items[i];
  }

  // Music tracks
  const auto& music = GetMusicTrackNames();
  for (size_t i = 0; i < music.size(); ++i) {
    labels["music"][std::to_string(i)] = music[i];
  }

  // Graphics sheets
  const auto& gfx = GetGraphicsSheetNames();
  for (size_t i = 0; i < gfx.size(); ++i) {
    labels["graphics"][std::to_string(i)] = gfx[i];
  }

  // Room effects
  const auto& effects = GetRoomEffectNames();
  for (size_t i = 0; i < effects.size(); ++i) {
    labels["room_effect"][std::to_string(i)] = effects[i];
  }

  // Room tags
  const auto& tags = GetRoomTagNames();
  for (size_t i = 0; i < tags.size(); ++i) {
    labels["room_tag"][std::to_string(i)] = tags[i];
  }

  // Tile types
  const auto& tiles = GetTileTypeNames();
  for (size_t i = 0; i < tiles.size(); ++i) {
    labels["tile_type"][std::to_string(i)] = tiles[i];
  }

  return labels;
}

// Get a label by resource type and ID
std::string Zelda3Labels::GetLabel(const std::string& resource_type, int id,
                                   const std::string& default_value) {
  static auto labels = ToResourceLabels();

  auto type_it = labels.find(resource_type);
  if (type_it == labels.end()) {
    return default_value.empty() ? resource_type + "_" + std::to_string(id)
                                 : default_value;
  }

  auto label_it = type_it->second.find(std::to_string(id));
  if (label_it == type_it->second.end()) {
    return default_value.empty() ? resource_type + "_" + std::to_string(id)
                                 : default_value;
  }

  return label_it->second;
}

}  // namespace yaze::zelda3
