#include "app/emu/debug/semantic_introspection.h"

#include <nlohmann/json.hpp>
#include <sstream>

#include "absl/status/status.h"

namespace yaze {
namespace emu {
namespace debug {

using json = nlohmann::json;

SemanticIntrospectionEngine::SemanticIntrospectionEngine(Memory* memory)
    : memory_(memory) {
  if (!memory_) {
    // Handle null pointer gracefully - this should be caught at construction
  }
}

absl::StatusOr<SemanticGameState> SemanticIntrospectionEngine::GetSemanticState() {
  if (!memory_) {
    return absl::InvalidArgumentError("Memory pointer is null");
  }

  SemanticGameState state;

  // Get game mode state
  auto game_mode = GetGameModeState();
  if (!game_mode.ok()) {
    return game_mode.status();
  }
  state.game_mode = *game_mode;

  // Get player state
  auto player = GetPlayerState();
  if (!player.ok()) {
    return player.status();
  }
  state.player = *player;

  // Get location context
  auto location = GetLocationContext();
  if (!location.ok()) {
    return location.status();
  }
  state.location = *location;

  // Get sprite states
  auto sprites = GetSpriteStates();
  if (!sprites.ok()) {
    return sprites.status();
  }
  state.sprites = *sprites;

  // Get frame info
  state.frame.frame_counter = memory_->ReadByte(alttp::kFrameCounter);
  state.frame.is_lag_frame = false;  // TODO: Implement lag frame detection

  return state;
}

absl::StatusOr<std::string> SemanticIntrospectionEngine::GetStateAsJson() {
  auto state = GetSemanticState();
  if (!state.ok()) {
    return state.status();
  }

  json j;

  // Game mode
  j["game_mode"]["main_mode"] = state->game_mode.main_mode;
  j["game_mode"]["submodule"] = state->game_mode.submodule;
  j["game_mode"]["mode_name"] = state->game_mode.mode_name;
  j["game_mode"]["in_game"] = state->game_mode.in_game;
  j["game_mode"]["in_transition"] = state->game_mode.in_transition;

  // Player
  j["player"]["x"] = state->player.x;
  j["player"]["y"] = state->player.y;
  j["player"]["state"] = state->player.state_name;
  j["player"]["direction"] = state->player.direction_name;
  j["player"]["layer"] = state->player.layer;
  j["player"]["health"] = state->player.health;
  j["player"]["max_health"] = state->player.max_health;

  // Location
  j["location"]["indoors"] = state->location.indoors;
  if (state->location.indoors) {
    j["location"]["dungeon_room"] = state->location.dungeon_room;
    j["location"]["room_name"] = state->location.room_name;
  } else {
    j["location"]["overworld_area"] = state->location.overworld_area;
    j["location"]["area_name"] = state->location.area_name;
  }

  // Sprites
  j["sprites"] = json::array();
  for (const auto& sprite : state->sprites) {
    json sprite_json;
    sprite_json["id"] = sprite.id;
    sprite_json["type"] = sprite.type_name;
    sprite_json["x"] = sprite.x;
    sprite_json["y"] = sprite.y;
    sprite_json["state"] = sprite.state_name;
    j["sprites"].push_back(sprite_json);
  }

  // Frame
  j["frame"]["counter"] = state->frame.frame_counter;
  j["frame"]["is_lag"] = state->frame.is_lag_frame;

  return j.dump(2);  // Pretty print with 2-space indentation
}

absl::StatusOr<PlayerState> SemanticIntrospectionEngine::GetPlayerState() {
  if (!memory_) {
    return absl::InvalidArgumentError("Memory pointer is null");
  }

  PlayerState player;

  // Read player coordinates
  uint8_t x_low = memory_->ReadByte(alttp::kLinkXLow);
  uint8_t x_high = memory_->ReadByte(alttp::kLinkXHigh);
  player.x = (x_high << 8) | x_low;

  uint8_t y_low = memory_->ReadByte(alttp::kLinkYLow);
  uint8_t y_high = memory_->ReadByte(alttp::kLinkYHigh);
  player.y = (y_high << 8) | y_low;

  // Read player state
  player.state = memory_->ReadByte(alttp::kLinkState);
  player.state_name = GetPlayerStateName(player.state);

  // Read direction
  player.direction = memory_->ReadByte(alttp::kLinkDirection);
  player.direction_name = GetPlayerDirectionName(player.direction);

  // Read layer
  player.layer = memory_->ReadByte(alttp::kLinkLayer);

  // Read health
  player.health = memory_->ReadByte(alttp::kLinkHealth);
  player.max_health = memory_->ReadByte(alttp::kLinkMaxHealth);

  return player;
}

absl::StatusOr<std::vector<SpriteState>> SemanticIntrospectionEngine::GetSpriteStates() {
  if (!memory_) {
    return absl::InvalidArgumentError("Memory pointer is null");
  }

  std::vector<SpriteState> sprites;

  // Check up to 16 sprite slots
  for (uint8_t i = 0; i < 16; ++i) {
    uint8_t state = memory_->ReadByte(alttp::kSpriteState + i);

    // Skip inactive sprites (state 0 typically means inactive)
    if (state == 0) {
      continue;
    }

    SpriteState sprite;
    sprite.id = i;

    // Read sprite coordinates
    uint8_t x_low = memory_->ReadByte(alttp::kSpriteXLow + i);
    uint8_t x_high = memory_->ReadByte(alttp::kSpriteXHigh + i);
    sprite.x = (x_high << 8) | x_low;

    uint8_t y_low = memory_->ReadByte(alttp::kSpriteYLow + i);
    uint8_t y_high = memory_->ReadByte(alttp::kSpriteYHigh + i);
    sprite.y = (y_high << 8) | y_low;

    // Read sprite type and state
    sprite.type = memory_->ReadByte(alttp::kSpriteType + i);
    sprite.type_name = GetSpriteTypeName(sprite.type);
    sprite.state = state;
    sprite.state_name = GetSpriteStateName(state);

    sprites.push_back(sprite);
  }

  return sprites;
}

absl::StatusOr<LocationContext> SemanticIntrospectionEngine::GetLocationContext() {
  if (!memory_) {
    return absl::InvalidArgumentError("Memory pointer is null");
  }

  LocationContext location;

  // Check if indoors
  location.indoors = memory_->ReadByte(alttp::kIndoorFlag) != 0;

  if (location.indoors) {
    // Read dungeon room (16-bit)
    uint8_t room_low = memory_->ReadByte(alttp::kDungeonRoomLow);
    uint8_t room_high = memory_->ReadByte(alttp::kDungeonRoomHigh);
    location.dungeon_room = (room_high << 8) | room_low;
    location.room_name = GetDungeonRoomName(location.dungeon_room);
    location.area_name = "";  // Not applicable for dungeons
  } else {
    // Read overworld area
    location.overworld_area = memory_->ReadByte(alttp::kOverworldArea);
    location.area_name = GetOverworldAreaName(location.overworld_area);
    location.room_name = "";  // Not applicable for overworld
  }

  return location;
}

absl::StatusOr<GameModeState> SemanticIntrospectionEngine::GetGameModeState() {
  if (!memory_) {
    return absl::InvalidArgumentError("Memory pointer is null");
  }

  GameModeState mode;

  mode.main_mode = memory_->ReadByte(alttp::kGameMode);
  mode.submodule = memory_->ReadByte(alttp::kSubmodule);
  mode.mode_name = GetGameModeName(mode.main_mode, mode.submodule);

  // Determine if in-game (modes 0x07-0x18 are generally gameplay)
  mode.in_game = (mode.main_mode >= 0x07 && mode.main_mode <= 0x18);

  // Check for transition states (modes that involve screen transitions)
  mode.in_transition = (mode.main_mode == 0x0F || mode.main_mode == 0x10 ||
                       mode.main_mode == 0x11 || mode.main_mode == 0x12);

  return mode;
}

// Helper method implementations

std::string SemanticIntrospectionEngine::GetGameModeName(uint8_t mode, uint8_t submodule) {
  switch (mode) {
    case 0x00: return "Startup/Initial";
    case 0x01: return "Title Screen";
    case 0x02: return "File Select";
    case 0x03: return "Name Entry";
    case 0x04: return "Delete Save";
    case 0x05: return "Load Game";
    case 0x06: return "Pre-Dungeon";
    case 0x07: return "Dungeon";
    case 0x08: return "Pre-Overworld";
    case 0x09: return "Overworld";
    case 0x0A: return "Pre-Overworld (Special)";
    case 0x0B: return "Overworld (Special)";
    case 0x0C: return "Unknown Mode";
    case 0x0D: return "Blank Screen";
    case 0x0E: return "Text/Dialog";
    case 0x0F: return "Screen Transition";
    case 0x10: return "Room Transition";
    case 0x11: return "Overworld Transition";
    case 0x12: return "Message";
    case 0x13: return "Death Sequence";
    case 0x14: return "Attract Mode";
    case 0x15: return "Mirror Warp";
    case 0x16: return "Refill Stats";
    case 0x17: return "Game Over";
    case 0x18: return "Triforce Room";
    case 0x19: return "Victory";
    case 0x1A: return "Ending Sequence";
    case 0x1B: return "Credits";
    default: return "Unknown (" + std::to_string(mode) + ")";
  }
}

std::string SemanticIntrospectionEngine::GetPlayerStateName(uint8_t state) {
  switch (state) {
    case 0x00: return "Standing";
    case 0x01: return "Walking";
    case 0x02: return "Turning";
    case 0x03: return "Pushing";
    case 0x04: return "Swimming";
    case 0x05: return "Attacking";
    case 0x06: return "Spin Attack";
    case 0x07: return "Item Use";
    case 0x08: return "Lifting";
    case 0x09: return "Throwing";
    case 0x0A: return "Stunned";
    case 0x0B: return "Jumping";
    case 0x0C: return "Falling";
    case 0x0D: return "Dashing";
    case 0x0E: return "Hookshot";
    case 0x0F: return "Carrying";
    case 0x10: return "Sitting";
    case 0x11: return "Telepathy";
    case 0x12: return "Bunny";
    case 0x13: return "Sleep";
    case 0x14: return "Cape";
    case 0x15: return "Dying";
    case 0x16: return "Tree Pull";
    case 0x17: return "Spin Jump";
    default: return "Unknown (" + std::to_string(state) + ")";
  }
}

std::string SemanticIntrospectionEngine::GetPlayerDirectionName(uint8_t direction) {
  switch (direction) {
    case 0: return "North";
    case 2: return "South";
    case 4: return "West";
    case 6: return "East";
    default: return "Unknown (" + std::to_string(direction) + ")";
  }
}

std::string SemanticIntrospectionEngine::GetSpriteTypeName(uint8_t type) {
  // Common ALTTP sprite types (subset for demonstration)
  switch (type) {
    case 0x00: return "Raven";
    case 0x01: return "Vulture";
    case 0x02: return "Flying Stalfos Head";
    case 0x03: return "Empty";
    case 0x04: return "Pull Switch";
    case 0x05: return "Pull Switch (unused)";
    case 0x06: return "Pull Switch (wrong)";
    case 0x07: return "Pull Switch (unused)";
    case 0x08: return "Octorok (one way)";
    case 0x09: return "Moldorm (boss)";
    case 0x0A: return "Octorok (four way)";
    case 0x0B: return "Chicken";
    case 0x0C: return "Octorok (stone)";
    case 0x0D: return "Buzzblob";
    case 0x0E: return "Snapdragon";
    case 0x0F: return "Octoballoon";
    case 0x10: return "Octoballoon Hatchlings";
    case 0x11: return "Hinox";
    case 0x12: return "Moblin";
    case 0x13: return "Mini Helmasaur";
    case 0x14: return "Thieves' Town Grate";
    case 0x15: return "Antifairy";
    case 0x16: return "Sahasrahla";
    case 0x17: return "Bush Hoarder";
    case 0x18: return "Mini Moldorm";
    case 0x19: return "Poe";
    case 0x1A: return "Smithy";
    case 0x1B: return "Arrow";
    case 0x1C: return "Statue";
    case 0x1D: return "Flutequest";
    case 0x1E: return "Crystal Switch";
    case 0x1F: return "Sick Kid";
    case 0x20: return "Sluggula";
    case 0x21: return "Water Switch";
    case 0x22: return "Ropa";
    case 0x23: return "Red Bari";
    case 0x24: return "Blue Bari";
    case 0x25: return "Talking Tree";
    case 0x26: return "Hardhat Beetle";
    case 0x27: return "Deadrock";
    case 0x28: return "Dark World Hint NPC";
    case 0x29: return "Adult";
    case 0x2A: return "Sweeping Lady";
    case 0x2B: return "Hobo";
    case 0x2C: return "Lumberjacks";
    case 0x2D: return "Neckless Man";
    case 0x2E: return "Flute Kid";
    case 0x2F: return "Race Game Lady";
    case 0x30: return "Race Game Guy";
    case 0x31: return "Fortune Teller";
    case 0x32: return "Angry Brothers";
    case 0x33: return "Pull For Rupees";
    case 0x34: return "Young Snitch";
    case 0x35: return "Innkeeper";
    case 0x36: return "Witch";
    case 0x37: return "Waterfall";
    case 0x38: return "Eye Statue";
    case 0x39: return "Locksmith";
    case 0x3A: return "Magic Bat";
    case 0x3B: return "Bonk Item";
    case 0x3C: return "Kid In KakTree";
    case 0x3D: return "Old Snitch Lady";
    case 0x3E: return "Hoarder";
    case 0x3F: return "Tutorial Guard";
    case 0x40: return "Lightning Lock";
    case 0x41: return "Blue Guard";
    case 0x42: return "Green Guard";
    case 0x43: return "Red Spear Guard";
    case 0x44: return "Bluesain Bolt";
    case 0x45: return "Usain Bolt";
    case 0x46: return "Blue Archer";
    case 0x47: return "Green Bush Guard";
    case 0x48: return "Red Javelin Guard";
    case 0x49: return "Red Bush Guard";
    case 0x4A: return "Bomb Guard";
    case 0x4B: return "Green Knife Guard";
    case 0x4C: return "Geldman";
    case 0x4D: return "Toppo";
    case 0x4E: return "Popo";
    case 0x4F: return "Popo2";
    case 0x50: return "Cannonball";
    case 0x51: return "Armos";
    case 0x52: return "King Zora";
    case 0x53: return "Armos Knight (boss)";
    case 0x54: return "Lanmolas (boss)";
    case 0x55: return "Fireball Zora";
    case 0x56: return "Walking Zora";
    case 0x57: return "Desert Statue";
    case 0x58: return "Crab";
    case 0x59: return "Lost Woods Bird";
    case 0x5A: return "Lost Woods Squirrel";
    case 0x5B: return "Spark (Left to Right)";
    case 0x5C: return "Spark (Right to Left)";
    case 0x5D: return "Roller (vertical moving)";
    case 0x5E: return "Roller (vertical moving)";
    case 0x5F: return "Roller";
    case 0x60: return "Roller (horizontal moving)";
    case 0x61: return "Beamos";
    case 0x62: return "Master Sword";
    case 0x63: return "Debirando Pit";
    case 0x64: return "Debirando";
    case 0x65: return "Archery Guy";
    case 0x66: return "Wall Cannon (vertical left)";
    case 0x67: return "Wall Cannon (vertical right)";
    case 0x68: return "Wall Cannon (horizontal top)";
    case 0x69: return "Wall Cannon (horizontal bottom)";
    case 0x6A: return "Ball N' Chain";
    case 0x6B: return "Cannon Soldier";
    case 0x6C: return "Cannon Soldier";
    case 0x6D: return "Mirror Portal";
    case 0x6E: return "Rat";
    case 0x6F: return "Rope";
    case 0x70: return "Keese";
    case 0x71: return "Helmasaur King Fireball";
    case 0x72: return "Leever";
    case 0x73: return "Pond Trigger";
    case 0x74: return "Uncle Priest";
    case 0x75: return "Running Man";
    case 0x76: return "Bottle Salesman";
    case 0x77: return "Princess Zelda";
    case 0x78: return "Antifairy (alternate)";
    case 0x79: return "Village Elder";
    case 0x7A: return "Bee";
    case 0x7B: return "Agahnim";
    case 0x7C: return "Agahnim Ball";
    case 0x7D: return "Green Stalfos";
    case 0x7E: return "Big Spike";
    case 0x7F: return "Firebar (clockwise)";
    case 0x80: return "Firebar (counterclockwise)";
    case 0x81: return "Firesnake";
    case 0x82: return "Hover";
    case 0x83: return "Green Eyegore";
    case 0x84: return "Red Eyegore";
    case 0x85: return "Yellow Stalfos";
    case 0x86: return "Kodongo";
    case 0x87: return "Flames";
    case 0x88: return "Mothula (boss)";
    case 0x89: return "Mothula Beam";
    case 0x8A: return "Spike Block";
    case 0x8B: return "Gibdo";
    case 0x8C: return "Arrghus (boss)";
    case 0x8D: return "Arrghus spawn";
    case 0x8E: return "Terrorpin";
    case 0x8F: return "Slime";
    case 0x90: return "Wallmaster";
    case 0x91: return "Stalfos Knight";
    case 0x92: return "Helmasaur King";
    case 0x93: return "Bumper";
    case 0x94: return "Pirogusu";
    case 0x95: return "Laser Eye (left)";
    case 0x96: return "Laser Eye (right)";
    case 0x97: return "Laser Eye (top)";
    case 0x98: return "Laser Eye (bottom)";
    case 0x99: return "Pengator";
    case 0x9A: return "Kyameron";
    case 0x9B: return "Wizzrobe";
    case 0x9C: return "Zoro";
    case 0x9D: return "Babasu";
    case 0x9E: return "Haunted Grove Ostritch";
    case 0x9F: return "Haunted Grove Rabbit";
    case 0xA0: return "Haunted Grove Bird";
    case 0xA1: return "Freezor";
    case 0xA2: return "Kholdstare";
    case 0xA3: return "Kholdstare Shell";
    case 0xA4: return "Falling Ice";
    case 0xA5: return "Zazak (blue)";
    case 0xA6: return "Zazak (red)";
    case 0xA7: return "Stalfos";
    case 0xA8: return "Bomber Flying Creatures from Darkworld";
    case 0xA9: return "Bomber Flying Creatures from Darkworld";
    case 0xAA: return "Pikit";
    case 0xAB: return "Maiden";
    case 0xAC: return "Apple";
    case 0xAD: return "Lost Old Man";
    case 0xAE: return "Down Pipe";
    case 0xAF: return "Up Pipe";
    case 0xB0: return "Right Pip";
    case 0xB1: return "Left Pipe";
    case 0xB2: return "Good Bee Again";
    case 0xB3: return "Hylian Inscription";
    case 0xB4: return "Thief's chest";
    case 0xB5: return "Bomb Salesman";
    case 0xB6: return "Kiki";
    case 0xB7: return "Blind Maiden";
    case 0xB8: return "Dialogue Tester";
    case 0xB9: return "Bully / Pink Ball";
    case 0xBA: return "Whirlpool";
    case 0xBB: return "Shopkeeper";
    case 0xBC: return "Drunk in the Inn";
    case 0xBD: return "Vitreous (boss)";
    case 0xBE: return "Vitreous small eye";
    case 0xBF: return "Vitreous' lightning";
    case 0xC0: return "Monster in Lake of Ill Omen";
    case 0xC1: return "Quicksand";
    case 0xC2: return "Gibo";
    case 0xC3: return "Thief";
    case 0xC4: return "Medusa";
    case 0xC5: return "4-Way Shooter";
    case 0xC6: return "Pokey";
    case 0xC7: return "Big Fairy";
    case 0xC8: return "Tektite";
    case 0xC9: return "Chain Chomp";
    case 0xCA: return "Trinexx Rock Head";
    case 0xCB: return "Trinexx Fire Head";
    case 0xCC: return "Trinexx Ice Head";
    case 0xCD: return "Blind (boss)";
    case 0xCE: return "Blind Laser";
    case 0xCF: return "Running Stalfos Head";
    case 0xD0: return "Lynel";
    case 0xD1: return "Bunny Beam";
    case 0xD2: return "Flopping Fish";
    case 0xD3: return "Stal";
    case 0xD4: return "Landmine";
    case 0xD5: return "Digging Game Guy";
    case 0xD6: return "Ganon";
    case 0xD7: return "Ganon Fire";
    case 0xD8: return "Heart";
    case 0xD9: return "Green Rupee";
    case 0xDA: return "Blue Rupee";
    case 0xDB: return "Red Rupee";
    case 0xDC: return "Bomb Refill (1)";
    case 0xDD: return "Bomb Refill (4)";
    case 0xDE: return "Bomb Refill (8)";
    case 0xDF: return "Small Magic Refill";
    case 0xE0: return "Full Magic Refill";
    case 0xE1: return "Arrow Refill (5)";
    case 0xE2: return "Arrow Refill (10)";
    case 0xE3: return "Fairy";
    case 0xE4: return "Small Key";
    case 0xE5: return "Big Key";
    case 0xE6: return "Shield";
    case 0xE7: return "Mushroom";
    case 0xE8: return "Fake Master Sword";
    case 0xE9: return "Magic Shop Assistant";
    case 0xEA: return "Heart Container";
    case 0xEB: return "Heart Piece";
    case 0xEC: return "Thrown Item";
    case 0xED: return "Somaria Platform";
    case 0xEE: return "Castle Mantle";
    case 0xEF: return "Somaria Platform (unused)";
    case 0xF0: return "Somaria Platform (unused)";
    case 0xF1: return "Somaria Platform (unused)";
    case 0xF2: return "Medallion Tablet";
    default: return "Unknown Sprite (" + std::to_string(type) + ")";
  }
}

std::string SemanticIntrospectionEngine::GetSpriteStateName(uint8_t state) {
  // Generic sprite state names (actual states vary by sprite type)
  switch (state) {
    case 0x00: return "Inactive";
    case 0x01: return "Spawning";
    case 0x02: return "Normal";
    case 0x03: return "Held";
    case 0x04: return "Stunned";
    case 0x05: return "Falling";
    case 0x06: return "Dead";
    case 0x07: return "Unused1";
    case 0x08: return "Active";
    case 0x09: return "Recoil";
    case 0x0A: return "Carried";
    case 0x0B: return "Frozen";
    default: return "State " + std::to_string(state);
  }
}

std::string SemanticIntrospectionEngine::GetOverworldAreaName(uint8_t area) {
  // ALTTP overworld areas
  switch (area) {
    case 0x00: return "Lost Woods";
    case 0x02: return "Lumberjack Tree";
    case 0x03: case 0x04: case 0x05: case 0x06:
      return "West Death Mountain";
    case 0x07: return "East Death Mountain";
    case 0x0A: return "Mountain Entry";
    case 0x0F: return "Waterfall of Wishing";
    case 0x10: return "Lost Woods Alcove";
    case 0x11: return "North of Kakariko";
    case 0x12: case 0x13: case 0x14: return "Northwest Pond";
    case 0x15: return "Desert Area";
    case 0x16: case 0x17: return "Desert Palace Entrance";
    case 0x18: return "Kakariko Village";
    case 0x1A: return "Pond of Happiness";
    case 0x1B: case 0x1C: return "West Hyrule";
    case 0x1D: return "Link's House";
    case 0x1E: return "East Hyrule";
    case 0x22: return "Smithy House";
    case 0x25: return "Zora's Domain";
    case 0x28: return "Haunted Grove Entrance";
    case 0x29: case 0x2A: return "West Hyrule";
    case 0x2B: return "Hyrule Castle";
    case 0x2C: return "East Hyrule";
    case 0x2D: case 0x2E: return "Eastern Palace";
    case 0x2F: return "Marsh";
    case 0x30: return "Desert of Mystery";
    case 0x32: return "Haunted Grove";
    case 0x33: case 0x34: return "West Hyrule";
    case 0x35: return "Graveyard";
    case 0x37: return "Waterfall Lake";
    case 0x39: case 0x3A: return "South Hyrule";
    case 0x3B: return "Pyramid";
    case 0x3C: return "East Dark World";
    case 0x3F: return "Marsh";
    case 0x40: return "Skull Woods";
    case 0x42: return "Dark Lumberjack Tree";
    case 0x43: case 0x44: case 0x45: return "West Death Mountain";
    case 0x47: return "Turtle Rock";
    case 0x4A: return "Bumper Cave Entry";
    case 0x4F: return "Dark Waterfall";
    case 0x50: return "Skull Woods Alcove";
    case 0x51: return "North of Outcasts";
    case 0x52: case 0x53: case 0x54: return "Northwest Dark World";
    case 0x55: return "Dark Desert";
    case 0x56: case 0x57: return "Misery Mire";
    case 0x58: return "Village of Outcasts";
    case 0x5A: return "Dark Pond of Happiness";
    case 0x5B: return "West Dark World";
    case 0x5D: return "Dark Link's House";
    case 0x5E: return "East Dark World";
    case 0x62: return "Haunted Grove";
    case 0x65: return "Dig Game";
    case 0x68: return "Dark Haunted Grove Entrance";
    case 0x69: case 0x6A: return "West Dark World";
    case 0x6B: return "Pyramid of Power";
    case 0x6C: return "East Dark World";
    case 0x6D: case 0x6E: return "Shield Shop";
    case 0x6F: return "Dark Marsh";
    case 0x70: return "Misery Mire";
    case 0x72: return "Dark Haunted Grove";
    case 0x73: case 0x74: return "West Dark World";
    case 0x75: return "Dark Graveyard";
    case 0x77: return "Palace of Darkness";
    case 0x7A: return "South Dark World";
    case 0x7B: return "Pyramid of Power";
    case 0x7C: return "East Dark World";
    case 0x7F: return "Swamp Palace";
    case 0x80: return "Master Sword Grove";
    case 0x81: return "Zora's Domain";
    default: return "Area " + std::to_string(area);
  }
}

std::string SemanticIntrospectionEngine::GetDungeonRoomName(uint16_t room) {
  // Simplified dungeon room naming - actual names depend on extensive lookup
  // This is a small subset for demonstration
  if (room < 0x100) {
    // Light World dungeons
    if (room >= 0x00 && room <= 0x0F) {
      return "Sewer/Escape Room " + std::to_string(room);
    } else if (room >= 0x20 && room <= 0x3F) {
      return "Hyrule Castle Room " + std::to_string(room - 0x20);
    } else if (room >= 0x50 && room <= 0x5F) {
      return "Castle Tower Room " + std::to_string(room - 0x50);
    } else if (room >= 0x60 && room <= 0x6F) {
      return "Agahnim Tower Room " + std::to_string(room - 0x60);
    } else if (room >= 0x70 && room <= 0x7F) {
      return "Swamp Palace Room " + std::to_string(room - 0x70);
    } else if (room >= 0x80 && room <= 0x8F) {
      return "Skull Woods Room " + std::to_string(room - 0x80);
    } else if (room >= 0x90 && room <= 0x9F) {
      return "Thieves' Town Room " + std::to_string(room - 0x90);
    } else if (room >= 0xA0 && room <= 0xAF) {
      return "Ice Palace Room " + std::to_string(room - 0xA0);
    } else if (room >= 0xB0 && room <= 0xBF) {
      return "Misery Mire Room " + std::to_string(room - 0xB0);
    } else if (room >= 0xC0 && room <= 0xCF) {
      return "Turtle Rock Room " + std::to_string(room - 0xC0);
    } else if (room >= 0xD0 && room <= 0xDF) {
      return "Palace of Darkness Room " + std::to_string(room - 0xD0);
    } else if (room >= 0xE0 && room <= 0xEF) {
      return "Desert Palace Room " + std::to_string(room - 0xE0);
    } else if (room >= 0xF0 && room <= 0xFF) {
      return "Eastern Palace Room " + std::to_string(room - 0xF0);
    }
  }

  // Special rooms
  switch (room) {
    case 0x00: return "Sewer Entrance";
    case 0x01: return "Hyrule Castle North Corridor";
    case 0x02: return "Switch Room (Escape)";
    case 0x10: return "Ganon Tower Entrance";
    case 0x11: return "Ganon Tower Stairs";
    case 0x20: return "Ganon Tower Big Chest";
    case 0x30: return "Ganon Tower Final Approach";
    case 0x40: return "Ganon Tower Top";
    case 0x41: return "Ganon Arena";
    default: return "Room " + std::to_string(room);
  }
}

}  // namespace debug
}  // namespace emu
}  // namespace yaze