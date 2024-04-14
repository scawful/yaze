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
  Sprite(Bytes src, uchar mapid, uchar id, uchar x, uchar y, int map_x,
         int map_y);
  void InitSprite(const Bytes& src, uchar mapid, uchar id, uchar x, uchar y,
                  int map_x, int map_y);
  void updateBBox();

  void Draw();
  void DrawSpriteTile(int x, int y, int srcx, int srcy, int pal,
                      bool mirror_x = false, bool mirror_y = false,
                      int sizex = 2, int sizey = 2);

  void UpdateMapProperties(short map_id) override;

  // New methods
  void updateCoordinates(int map_x, int map_y);

  auto PreviewGraphics() const { return preview_gfx_; }
  auto id() const { return id_; }
  auto set_id(uchar id) { id_ = id; }
  auto x() const { return x_; }
  auto y() const { return y_; }
  auto nx() const { return nx_; }
  auto ny() const { return ny_; }
  auto map_id() const { return map_id_; }
  auto map_x() const { return map_x_; }
  auto map_y() const { return map_y_; }

  auto layer() const { return layer_; }
  auto subtype() const { return subtype_; }
  auto& keyDrop() const { return key_drop_; }

  auto Width() const { return bounding_box_.w; }
  auto Height() const { return bounding_box_.h; }
  std::string& Name() { return name_; }
  auto deleted() const { return deleted_; }
  auto set_deleted(bool deleted) { deleted_ = deleted; }

 private:
  Bytes current_gfx_;
  bool overworld_;

  uchar map_id_;
  uchar id_;
  // uchar x_;
  // uchar y_;
  uchar nx_;
  uchar ny_;
  uchar overlord_ = 0;
  std::string name_;

  int subtype_;
  int layer_;

  int map_x_;
  int map_y_;
  Bytes preview_gfx_;
  uchar lowerX_;
  uchar lowerY_;
  uchar higherX_;
  uchar higherY_;
  SDL_Rect bounding_box_;

  int width_ = 16;
  int height_ = 16;

  int key_drop_;

  bool deleted_ = false;
};

}  // namespace zelda3
}  // namespace app
}  // namespace yaze

#endif