#ifndef YAZE_APP_ZELDA3_COMMON_H
#define YAZE_APP_ZELDA3_COMMON_H

namespace yaze {
namespace app {
/**
 * @namespace yaze::app::zelda3
 * @brief Zelda 3 specific classes and functions.
 */
namespace zelda3 {

/**
 * @brief Represents tile32 data for the overworld.
 */
using OWBlockset = std::vector<std::vector<uint16_t>>;

/**
 * @brief Overworld map tile32 data.
 */
struct OWMapTiles {
  OWBlockset light_world;    // 64 maps
  OWBlockset dark_world;     // 64 maps
  OWBlockset special_world;  // 32 maps
};
using OWMapTiles = struct OWMapTiles;

/**
 * @class OverworldEntity
 * @brief Base class for all overworld entities.
 */
class OverworldEntity {
 public:
  enum EntityType {
    kEntrance = 0,
    kExit = 1,
    kItem = 2,
    kSprite = 3,
    kTransport = 4,
    kMusic = 5,
    kTilemap = 6,
    kProperties = 7
  } type_;
  int x_;
  int y_;
  int game_x_;
  int game_y_;
  int entity_id_;
  int map_id_;

  auto set_x(int x) { x_ = x; }
  auto set_y(int y) { y_ = y; }

  OverworldEntity() = default;

  virtual void UpdateMapProperties(short map_id) = 0;
};
}  // namespace zelda3
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_COMMON_H