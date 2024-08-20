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
    name_ = core::kSpriteDefaultNames[id];
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
    name_ = core::kSpriteDefaultNames[id];
    map_x_ = map_x;
    map_y_ = map_y;
    preview_gfx_.resize(64 * 64, 0xFF);
  }
  void updateBBox();

  void Draw();
  void DrawSpriteTile(int x, int y, int srcx, int srcy, int pal,
                      bool mirror_x = false, bool mirror_y = false,
                      int sizex = 2, int sizey = 2);

  void UpdateMapProperties(short map_id) override;

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