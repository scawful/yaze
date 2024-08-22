#ifndef YAZE_APP_ZELDA3_SPRITE_H
#define YAZE_APP_ZELDA3_SPRITE_H

#include <SDL.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "app/core/constants.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_tile.h"
#include "app/rom.h"
#include "app/zelda3/common.h"

namespace yaze {
namespace app {
namespace zelda3 {

static const std::string kSpriteDefaultNames[]{
    "00 Raven",
    "01 Vulture",
    "02 Flying Stalfos Head",
    "03 No Pointer (Empty",
    "04 Pull Switch (good",
    "05 Pull Switch (unused",
    "06 Pull Switch (bad",
    "07 Pull Switch (unused",
    "08 Octorock (one way",
    "09 Moldorm (Boss",
    "0A Octorock (four way",
    "0B Chicken",
    "0C Octorock (?",
    "0D Buzzblock",
    "0E Snapdragon",
    "0F Octoballoon",
    "10 Octoballon Hatchlings",
    "11 Hinox",
    "12 Moblin",
    "13 Mini Helmasaure",
    "14 Gargoyle's Domain Gate",
    "15 Antifairy",
    "16 Sahasrahla / Aginah",
    "17 Bush Hoarder",
    "18 Mini Moldorm",
    "19 Poe",
    "1A Dwarves",
    "1B Arrow in wall",
    "1C Statue",
    "1D Weathervane",
    "1E Crystal Switch",
    "1F Bug-Catching Kid",
    "20 Sluggula",
    "21 Push Switch",
    "22 Ropa",
    "23 Red Bari",
    "24 Blue Bari",
    "25 Talking Tree",
    "26 Hardhat Beetle",
    "27 Deadrock",
    "28 Storytellers",
    "29 Blind Hideout attendant",
    "2A Sweeping Lady",
    "2B Storytellers",
    "2C Lumberjacks",
    "2D Telepathic Stones",
    "2E Multipurpose Sprite",
    "2F Race Npc",
    "30 Person?",
    "31 Fortune Teller",
    "32 Angry Brothers",
    "33 Pull for items",
    "34 Scared Girl",
    "35 Innkeeper",
    "36 Witch",
    "37 Waterfall",
    "38 Arrow Target",
    "39 Average Middle",
    "3A Half Magic Bat",
    "3B Dash Item",
    "3C Village Kid",
    "3D Signs? Chicken lady also showed up / Scared ladies outside houses.",
    "3E Rock Hoarder",
    "3F Tutorial Soldier",
    "40 Lightning Lock",
    "41 Blue Sword Soldier / Used by guards to detect player",
    "42 Green Sword Soldier",
    "43 Red Spear Soldier",
    "44 Assault Sword Soldier",
    "45 Green Spear Soldier",
    "46 Blue Archer",
    "47 Green Archer",
    "48 Red Javelin Soldier",
    "49 Red Javelin Soldier 2",
    "4A Red Bomb Soldiers",
    "4B Green Soldier Recruits",
    "4C Geldman",
    "4D Rabbit",
    "4E Popo",
    "4F Popo 2",
    "50 Cannon Balls",
    "51 Armos",
    "52 Giant Zora",
    "53 Armos Knights (Boss",
    "54 Lanmolas (Boss",
    "55 Fireball Zora",
    "56 Walking Zora",
    "57 Desert Palace Barriers",
    "58 Crab",
    "59 Bird",
    "5A Squirrel",
    "5B Spark (Left to Right",
    "5C Spark (Right to Left",
    "5D Roller (vertical moving",
    "5E Roller (vertical moving",
    "5F Roller",
    "60 Roller (horizontal moving",
    "61 Beamos",
    "62 Master Sword",
    "63 Devalant (Non",
    "64 Devalant (Shooter",
    "65 Shooting Gallery Proprietor",
    "66 Moving Cannon Ball Shooters (Right",
    "67 Moving Cannon Ball Shooters (Left",
    "68 Moving Cannon Ball Shooters (Down",
    "69 Moving Cannon Ball Shooters (Up",
    "6A Ball N' Chain Trooper",
    "6B Cannon Soldier",
    "6C Mirror Portal",
    "6D Rat",
    "6E Rope",
    "6F Keese",
    "70 Helmasaur King Fireball",
    "71 Leever",
    "72 Activator for the ponds (where you throw in items",
    "73 Uncle / Priest",
    "74 Running Man",
    "75 Bottle Salesman",
    "76 Princess Zelda",
    "77 Antifairy (Alternate",
    "78 Village Elder",
    "79 Bee",
    "7A Agahnim",
    "7B Agahnim Energy Ball",
    "7C Hyu",
    "7D Big Spike Trap",
    "7E Guruguru Bar (Clockwise",
    "7F Guruguru Bar (Counter Clockwise",
    "80 Winder",
    "81 Water Tektite",
    "82 Antifairy Circle",
    "83 Green Eyegore",
    "84 Red Eyegore",
    "85 Yellow Stalfos",
    "86 Kodongos",
    "87 Flames",
    "88 Mothula (Boss",
    "89 Mothula's Beam",
    "8A Spike Trap",
    "8B Gibdo",
    "8C Arrghus (Boss",
    "8D Arrghus spawn",
    "8E Terrorpin",
    "8F Slime",
    "90 Wallmaster",
    "91 Stalfos Knight",
    "92 Helmasaur King",
    "93 Bumper",
    "94 Swimmers",
    "95 Eye Laser (Right",
    "96 Eye Laser (Left",
    "97 Eye Laser (Down",
    "98 Eye Laser (Up",
    "99 Pengator",
    "9A Kyameron",
    "9B Wizzrobe",
    "9C Tadpoles",
    "9D Tadpoles",
    "9E Ostrich (Haunted Grove",
    "9F Flute",
    "A0 Birds (Haunted Grove",
    "A1 Freezor",
    "A2 Kholdstare (Boss",
    "A3 Kholdstare's Shell",
    "A4 Falling Ice",
    "A5 Zazak Fireball",
    "A6 Red Zazak",
    "A7 Stalfos",
    "A8 Bomber Flying Creatures from Darkworld",
    "A9 Bomber Flying Creatures from Darkworld",
    "AA Pikit",
    "AB Maiden",
    "AC Apple",
    "AD Lost Old Man",
    "AE Down Pipe",
    "AF Up Pipe",
    "B0 Right Pip",
    "B1 Left Pipe",
    "B2 Good bee again?",
    "B3 Hylian Inscription",
    "B4 Thief?s chest (not the one that follows you",
    "B5 Bomb Salesman",
    "B6 Kiki",
    "B7 Maiden following you in Blind Dungeon",
    "B8 Monologue Testing Sprite",
    "B9 Feuding Friends on Death Mountain",
    "BA Whirlpool",
    "BB Salesman / chestgame guy / 300 rupee giver guy / Chest game thief",
    "BC Drunk in the inn",
    "BD Vitreous (Large Eyeball",
    "BE Vitreous (Small Eyeball",
    "BF Vitreous' Lightning",
    "C0 Monster in Lake of Ill Omen / Quake Medallion",
    "C1 Agahnim teleporting Zelda to dark world",
    "C2 Boulders",
    "C3 Gibo",
    "C4 Thief",
    "C5 Medusa",
    "C6 Four Way Fireball Spitters (spit when you use your sword",
    "C7 Hokku",
    "C8 Big Fairy who heals you",
    "C9 Tektite",
    "CA Chain Chomp",
    "CB Trinexx",
    "CC Another part of trinexx",
    "CD Yet another part of trinexx",
    "CE Blind The Thief (Boss)",
    "CF Swamola",
    "D0 Lynel",
    "D1 Bunny Beam",
    "D2 Flopping fish",
    "D3 Stal",
    "D4 Landmine",
    "D5 Digging Game Proprietor",
    "D6 Ganon",
    "D7 Copy of Ganon",
    "D8 Heart",
    "D9 Green Rupee",
    "DA Blue Rupee",
    "DB Red Rupee",
    "DC Bomb Refill (1)",
    "DD Bomb Refill (4)",
    "DE Bomb Refill (8)",
    "DF Small Magic Refill",
    "E0 Full Magic Refill",
    "E1 Arrow Refill (5)",
    "E2 Arrow Refill (10)",
    "E3 Fairy",
    "E4 Key",
    "E5 Big Key",
    "E6 Shield",
    "E7 Mushroom",
    "E8 Fake Master Sword",
    "E9 Magic Shop dude / His items",
    "EA Heart Container",
    "EB Heart Piece",
    "EC Bushes",
    "ED Cane Of Somaria Platform",
    "EE Mantle",
    "EF Cane of Somaria Platform (Unused)",
    "F0 Cane of Somaria Platform (Unused)",
    "F1 Cane of Somaria Platform (Unused)",
    "F2 Medallion Tablet",
    "F3",
    "F4 Falling Rocks",
    "F5",
    "F6",
    "F7",
    "F8",
    "F9",
    "FA",
    "FB",
    "FC",
    "FD",
    "FE",
    "FF",
};

/**
 * @class Sprite
 * @brief A class for managing sprites in the overworld and underworld.
 */
class Sprite : public OverworldEntity {
 public:
  Sprite() = default;
  Sprite(std::vector<uint8_t> src, uint8_t mapid, uint8_t id, uint8_t x,
         uint8_t y, int map_x, int map_y)
      : current_gfx_(src),
        map_id_(static_cast<int>(mapid)),
        id_(id),
        nx_(x),
        ny_(y),
        map_x_(map_x),
        map_y_(map_y) {
    type_ = zelda3::OverworldEntity::EntityType::kSprite;
    entity_id_ = id;
    x_ = map_x_;
    y_ = map_y_;
    current_gfx_ = src;
    overworld_ = true;
    name_ = kSpriteDefaultNames[id];
    preview_gfx_.resize(64 * 64, 0xFF);
  }

  void InitSprite(const std::vector<uint8_t>& src, uint8_t mapid, uint8_t id,
                  uint8_t x, uint8_t y, int map_x, int map_y) {
    current_gfx_ = src;
    overworld_ = true;
    map_id_ = static_cast<int>(mapid);
    id_ = id;
    type_ = zelda3::OverworldEntity::EntityType::kSprite;
    entity_id_ = id;
    x_ = map_x_;
    y_ = map_y_;
    nx_ = x;
    ny_ = y;
    name_ = kSpriteDefaultNames[id];
    map_x_ = map_x;
    map_y_ = map_y;
    preview_gfx_.resize(64 * 64, 0xFF);
  }
  void updateBBox();

  void Draw();
  void DrawSpriteTile(int x, int y, int srcx, int srcy, int pal,
                      bool mirror_x = false, bool mirror_y = false,
                      int sizex = 2, int sizey = 2);

  void UpdateMapProperties(uint16_t map_id) override;

  // New methods
  void UpdateCoordinates(int map_x, int map_y);

  auto PreviewGraphics() const { return preview_gfx_; }
  auto id() const { return id_; }
  auto set_id(uint8_t id) { id_ = id; }
  auto x() const { return x_; }
  auto y() const { return y_; }
  auto nx() const { return nx_; }
  auto ny() const { return ny_; }
  auto map_id() const { return map_id_; }
  auto map_x() const { return map_x_; }
  auto map_y() const { return map_y_; }
  auto game_state() const { return game_state_; }

  auto layer() const { return layer_; }
  auto subtype() const { return subtype_; }
  auto& keyDrop() const { return key_drop_; }

  auto Width() const { return bounding_box_.w; }
  auto Height() const { return bounding_box_.h; }
  auto name() { return name_; }
  auto deleted() const { return deleted_; }
  auto set_deleted(bool deleted) { deleted_ = deleted; }

 private:
  uint8_t map_id_;
  uint8_t game_state_;
  uint8_t id_;
  uint8_t nx_;
  uint8_t ny_;
  uint8_t overlord_ = 0;
  uint8_t lowerX_;
  uint8_t lowerY_;
  uint8_t higherX_;
  uint8_t higherY_;

  int width_ = 16;
  int height_ = 16;
  int map_x_;
  int map_y_;
  int layer_;
  int subtype_;
  int key_drop_;

  bool deleted_ = false;
  bool overworld_;

  std::string name_;
  std::vector<uint8_t> preview_gfx_;
  std::vector<uint8_t> current_gfx_;

  SDL_Rect bounding_box_;
};

}  // namespace zelda3
}  // namespace app
}  // namespace yaze

#endif