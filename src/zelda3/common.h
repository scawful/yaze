#ifndef YAZE_APP_ZELDA3_COMMON_H
#define YAZE_APP_ZELDA3_COMMON_H

#include <cstdint>
#include <string>

/**
 * @namespace yaze::zelda3
 * @brief Zelda 3 specific classes and functions.
 */
namespace yaze::zelda3 {

/**
 * @class GameEntity
 * @brief Base class for all overworld and dungeon entities.
 *
 * Coordinate System (matches ZScream naming conventions):
 * - x_, y_: World coordinates in pixels (0-4095 for overworld)
 *   ZScream equivalent: PlayerX/PlayerY (ExitOW.cs), GlobalX/GlobalY
 * (EntranceOW.cs)
 *
 * - game_x_, game_y_: Map-local tile coordinates (0-63 for normal, 0-31 for
 * small areas) ZScream equivalent: AreaX/AreaY (ExitOW.cs), GameX/GameY
 * (items/sprites)
 *
 * - map_id_: Parent map ID accounting for large/wide/tall multi-area maps
 *   ZScream equivalent: MapID property
 *
 * - entity_id_: Index in entity array (for display/debugging)
 */
class GameEntity {
 public:
  enum EntityType {
    kEntrance = 0,
    kExit = 1,
    kItem = 2,
    kSprite = 3,
    kTransport = 4,
    kMusic = 5,
    kTilemap = 6,
    kProperties = 7,
    kDungeonSprite = 8,
  } entity_type_;

  // World coordinates (0-4095 for overworld)
  // ZScream: PlayerX/PlayerY (exits), GlobalX/GlobalY (entrances)
  int x_ = 0;
  int y_ = 0;

  // Map-local game coordinates (0-63 tiles, or 0-31 for small areas)
  // ZScream: AreaX/AreaY (exits), GameX/GameY (items/sprites)
  int game_x_ = 0;
  int game_y_ = 0;

  // Entity index in array (for display/debugging)
  int entity_id_ = 0;

  // Parent map ID (accounting for large/wide/tall areas)
  // ZScream: MapID property
  uint16_t map_id_ = 0;

  auto set_x(int x) { x_ = x; }
  auto set_y(int y) { y_ = y; }

  GameEntity() = default;
  virtual ~GameEntity() {}

  /**
   * @brief Update entity properties based on map position
   * @param map_id Parent map ID to update to
   * @param context Optional context (typically const Overworld* for coordinate
   * calculations)
   *
   * ZScream equivalent: UpdateMapStuff() / UpdateMapProperties()
   *
   * This method recalculates derived properties like:
   * - game_x_/game_y_ from world x_/y_ coordinates
   * - Scroll/camera values for exits (if is_automatic_ = true)
   * - Map position encoding for saving
   */
  virtual void UpdateMapProperties(uint16_t map_id,
                                   const void* context = nullptr) = 0;
};

constexpr int kNumOverworldMaps = 160;

// 1 byte, not 0 if enabled
// vanilla, v2, v3
constexpr int OverworldCustomASMHasBeenApplied = 0x140145;

constexpr const char* kEntranceNames[] = {
    "Link's House Intro",
    "Link's House Post-intro",
    "Sanctuary",
    "Hyrule Castle West",
    "Hyrule Castle Central",
    "Hyrule Castle East",
    "Death Mountain Express (Lower)",
    "Death Mountain Express (Upper)",
    "Eastern Palace",
    "Desert Palace Central",
    "Desert Palace East",
    "Desert Palace West",
    "Desert Palace Boss Lair",
    "Kakariko Elder's House West",
    "Kakariko Elder's House East",
    "Kakariko Angry Bros West",
    "Kakariko Angry Bros East",
    "Mad Batter Lair",
    "Under Lumberjacks' Weird Tree",
    "Death Mountain Maze 0000",
    "Death Mountain Maze 0001",
    "Turtle Rock Mountainface 1",
    "Death Mountain Cape Heart Piece Cave (Lower)",
    "Death Mountain Cape Heart Piece Cave (Upper)",
    "Turtle Rock Mountainface 2",
    "Turtle Rock Mountainface 3",
    "Death Mountain Maze 0002",
    "Death Mountain Maze 0003",
    "Death Mountain Maze 0004",
    "Death Mountain Maze 0005",
    "Death Mountain Maze 0006",
    "Death Mountain Maze 0007",
    "Death Mountain Maze 0008",
    "Spectacle Rock Maze 1",
    "Spectacle Rock Maze 2",
    "Spectacle Rock Maze 3",
    "Hyrule Castle Tower",
    "Swamp Palace",
    "Palace of Darkness",
    "Misery Mire",
    "Skull Woods 1",
    "Skull Woods 2",
    "Skull Woods Big Chest",
    "Skull Woods Boss Lair",
    "Lost Woods Thieves' Lair",
    "Ice Palace",
    "Death Mountain Escape West",
    "Death Mountain Escape East",
    "Death Mountain Elder's Cave (Lower)",
    "Death Mountain Elder's Cave (Upper)",
    "Hyrule Castle Secret Cellar",
    "Tower of Hera",
    "Thieves's Town",
    "Turtle Rock Main",
    "Ganon's Pyramid Sanctum (Lower)",
    "Ganon's Tower",
    "Fairy Cave 1",
    "Kakariko Western Well",
    "Death Mountain Maze 0009",
    "Death Mountain Maze 0010",
    "Treasure Shell Game 1",
    "Storyteller Cave 1",
    "Snitch House 1",
    "Snitch House 2",
    "SickBoy House",
    "Byrna Gauntlet",
    "Kakariko Pub South",
    "Kakariko Pub North",
    "Kakariko Inn",
    "Sahasrahlah's Disco Infernum",
    "Kakariko's Lame Shop",
    "Village of Outcasts Chest Game",
    "Village of Outcasts Orphanage",
    "Kakariko Library",
    "Kakariko Storage Shed",
    "Kakariko Sweeper Lady's House",
    "Potion Shop",
    "Aginah's Desert Cottage",
    "Watergate",
    "Death Mountain Maze 0011",
    "Fairy Cave 2",
    "Refill Cave 0001",
    "Refill Cave 0002",
    "The Bomb \"Shop\"",
    "Village of Outcasts Retirement Center",
    "Fairy Cave 3",
    "Good Bee Cave",
    "General Store 1",
    "General Store 2",
    "Archery Game",
    "Storyteller Cave 2",
    "Hall of the Invisibility Cape",
    "Pond of Wishing",
    "Pond of Happiness",
    "Fairy Cave 4",
    "Swamp of Evil Heart Piece Hall",
    "General Store 3",
    "Blind's Old Hideout",
    "Storyteller Cave 3",
    "Warped Pond of Wishing",
    "Chez Smithies",
    "Fortune Teller 1",
    "Fortune Teller 2",
    "Chest Shell Game 2",
    "Storyteller Cave 4",
    "Storyteller Cave 5",
    "Storyteller Cave 6",
    "Village House 1",
    "Thief Hideout 1",
    "Thief Hideout 2",
    "Heart Piece Cave 1",
    "Thief Hideout 3",
    "Refill Cave 3",
    "Fairy Cave 5",
    "Heart Piece Cave 2",
    "Hyrule Castle Prison",
    "Hyrule Castle Throne Room",
    "Hyrule Tower Agahnim's Sanctum",
    "Skull Woods 3 (Drop In)",
    "Skull Woods 4 (Drop In)",
    "Skull Woods 5 (Drop In)",
    "Skull Woods 6 (Drop In)",
    "Lost Woods Thieves' Hideout (Drop In)",
    "Ganon's Pyramid Sanctum (Upper)",
    "Fairy Cave 6 (Drop In)",
    "Hyrule Castle Secret Cellar (Drop In)",
    "Mad Batter Lair (Drop In)",
    "Under Lumberjacks' Weird Tree (Drop In)",
    "Kakariko Western Well (Drop In)",
    "Hyrule Sewers Goodies Room (Drop In)",
    "Chris Houlihan Room (Drop In)",
    "Heart Piece Cave 3 (Drop In)",
    "Ice Rod Cave"};

// TileTypeNames defined in common.cc to avoid static initialization order issues
extern const std::string TileTypeNames[256];

}  // namespace yaze::zelda3

#endif  // YAZE_APP_ZELDA3_COMMON_H
