#ifndef YAZE_APP_EMU_DEBUG_SEMANTIC_INTROSPECTION_H
#define YAZE_APP_EMU_DEBUG_SEMANTIC_INTROSPECTION_H

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "app/emu/memory/memory.h"

namespace yaze {
namespace emu {
namespace debug {

// ALTTP-specific RAM addresses
namespace alttp {
// Game Mode
constexpr uint32_t kGameMode = 0x7E0010;
constexpr uint32_t kSubmodule = 0x7E0011;
constexpr uint32_t kIndoorFlag = 0x7E001B;

// Player
constexpr uint32_t kLinkYLow = 0x7E0020;
constexpr uint32_t kLinkYHigh = 0x7E0021;
constexpr uint32_t kLinkXLow = 0x7E0022;
constexpr uint32_t kLinkXHigh = 0x7E0023;
constexpr uint32_t kLinkDirection = 0x7E002F;
constexpr uint32_t kLinkState = 0x7E005D;
constexpr uint32_t kLinkLayer = 0x7E00EE;
constexpr uint32_t kLinkHealth = 0x7E00F6;
constexpr uint32_t kLinkMaxHealth = 0x7E00F7;

// Location
constexpr uint32_t kOverworldArea = 0x7E008A;
constexpr uint32_t kDungeonRoom = 0x7E00A0;
constexpr uint32_t kDungeonRoomLow = 0x7E00A0;
constexpr uint32_t kDungeonRoomHigh = 0x7E00A1;

// Sprites (base addresses, add slot offset 0-15)
constexpr uint32_t kSpriteYLow = 0x7E0D00;
constexpr uint32_t kSpriteYHigh = 0x7E0D20;
constexpr uint32_t kSpriteXLow = 0x7E0D10;
constexpr uint32_t kSpriteXHigh = 0x7E0D30;
constexpr uint32_t kSpriteState = 0x7E0DD0;
constexpr uint32_t kSpriteType = 0x7E0E20;

// Frame timing
constexpr uint32_t kFrameCounter = 0x7E001A;
}  // namespace alttp

/**
 * @brief Semantic representation of a sprite entity
 */
struct SpriteState {
  uint8_t id;           // Sprite slot ID (0-15)
  uint16_t x;           // X coordinate
  uint16_t y;           // Y coordinate
  uint8_t type;         // Sprite type ID
  std::string type_name;  // Human-readable sprite type name
  uint8_t state;        // Sprite state
  std::string state_name; // Human-readable state (Active, Dead, etc.)
};

/**
 * @brief Semantic representation of the player state
 */
struct PlayerState {
  uint16_t x;              // X coordinate
  uint16_t y;              // Y coordinate
  uint8_t state;           // Action state
  std::string state_name;  // Human-readable state (Walking, Attacking, etc.)
  uint8_t direction;       // Facing direction (0=up, 2=down, 4=left, 6=right)
  std::string direction_name;  // Human-readable direction
  uint8_t layer;           // Z-layer (upper/lower)
  uint8_t health;          // Current health
  uint8_t max_health;      // Maximum health
};

/**
 * @brief Semantic representation of the current location
 */
struct LocationContext {
  bool indoors;            // True if in dungeon/house, false if overworld
  uint8_t overworld_area;  // Overworld area ID (if outdoors)
  uint16_t dungeon_room;   // Dungeon room ID (if indoors)
  std::string room_name;   // Human-readable location name
  std::string area_name;   // Human-readable area name
};

/**
 * @brief Semantic representation of the game mode
 */
struct GameModeState {
  uint8_t main_mode;       // Main game mode value
  uint8_t submodule;       // Submodule value
  std::string mode_name;   // Human-readable mode name
  bool in_game;            // True if actively playing (not in menu/title)
  bool in_transition;     // True if transitioning between screens
};

/**
 * @brief Frame timing information
 */
struct FrameInfo {
  uint8_t frame_counter;   // Current frame counter value
  bool is_lag_frame;       // True if this frame is lagging
};

/**
 * @brief Complete semantic game state
 */
struct SemanticGameState {
  GameModeState game_mode;
  PlayerState player;
  LocationContext location;
  std::vector<SpriteState> sprites;
  FrameInfo frame;
};

/**
 * @brief Engine for extracting semantic game state from SNES memory
 *
 * This class provides high-level semantic interpretations of raw SNES RAM
 * values, making it easier for AI agents to understand the game state without
 * needing to know the specific memory addresses or value encodings.
 */
class SemanticIntrospectionEngine {
 public:
  /**
   * @brief Construct a new Semantic Introspection Engine
   * @param memory Pointer to the SNES memory interface
   */
  explicit SemanticIntrospectionEngine(Memory* memory);
  ~SemanticIntrospectionEngine() = default;

  /**
   * @brief Get the complete semantic game state
   * @return Current semantic game state
   */
  absl::StatusOr<SemanticGameState> GetSemanticState();

  /**
   * @brief Get the semantic state as JSON string
   * @return JSON representation of the current game state
   */
  absl::StatusOr<std::string> GetStateAsJson();

  /**
   * @brief Get only the player state
   * @return Current player semantic state
   */
  absl::StatusOr<PlayerState> GetPlayerState();

  /**
   * @brief Get all active sprite states
   * @return Vector of active sprite states
   */
  absl::StatusOr<std::vector<SpriteState>> GetSpriteStates();

  /**
   * @brief Get the current location context
   * @return Current location semantic state
   */
  absl::StatusOr<LocationContext> GetLocationContext();

  /**
   * @brief Get the current game mode state
   * @return Current game mode semantic state
   */
  absl::StatusOr<GameModeState> GetGameModeState();

 private:
  Memory* memory_;  // Non-owning pointer to SNES memory

  // Helper methods for name lookups
  std::string GetGameModeName(uint8_t mode, uint8_t submodule);
  std::string GetPlayerStateName(uint8_t state);
  std::string GetPlayerDirectionName(uint8_t direction);
  std::string GetSpriteTypeName(uint8_t type);
  std::string GetSpriteStateName(uint8_t state);
  std::string GetOverworldAreaName(uint8_t area);
  std::string GetDungeonRoomName(uint16_t room);
};

}  // namespace debug
}  // namespace emu
}  // namespace yaze

#endif  // YAZE_APP_EMU_DEBUG_SEMANTIC_INTROSPECTION_H