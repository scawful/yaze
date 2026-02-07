#ifndef YAZE_APP_ZELDA3_SPRITE_H
#define YAZE_APP_ZELDA3_SPRITE_H

#include "app/platform/sdl_compat.h"

#include <cstdint>
#include <string>
#include <vector>

#include "zelda3/common.h"
#include "zelda3/sprite/overlord.h"

namespace yaze {
namespace zelda3 {

// Sprite names defined in sprite.cc to avoid static initialization order issues
extern const std::string kSpriteDefaultNames[256];
// Expanded names (from hmagic sprname.dat, 0x11c entries). Might differ in
// wording/coverage; consumers can choose which to use.
extern const char* const kSpriteNames[];
extern const size_t kSpriteNameCount;

// Global preference for using hmagic names when available.
void SetPreferHmagicSpriteNames(bool prefer);
bool PreferHmagicSpriteNames();

// Utility to resolve a sprite name; uses hmagic list when enabled and in range,
// otherwise falls back to the 256-entry defaults.
const char* ResolveSpriteName(uint16_t id);

/**
 * @class Sprite
 * @brief A class for managing sprites in the overworld and underworld.
 */
class Sprite : public GameEntity {
 public:
  Sprite() = default;
  Sprite(const std::vector<uint8_t>& src, uint8_t overworld_map_id, uint8_t id,
         uint8_t x, uint8_t y, int map_x, int map_y)
      : map_id_(static_cast<int>(overworld_map_id)),
        id_(id),
        nx_(x),
        ny_(y),
        map_x_(map_x),
        map_y_(map_y),
        current_gfx_(src) {
    entity_type_ = zelda3::GameEntity::EntityType::kSprite;
    entity_id_ = id;
    x_ = map_x_;
    y_ = map_y_;
    overworld_ = true;
    name_ = ResolveSpriteName(id);
    // Defer preview_gfx_ allocation until DrawSpriteTile() is called
  }

  Sprite(uint8_t id, uint8_t x, uint8_t y, uint8_t subtype, uint8_t layer)
      : id_(id), nx_(x), ny_(y), subtype_(subtype), layer_(layer) {
    x_ = x;
    y_ = y;
    name_ = ResolveSpriteName(id);
    if (((subtype & 0x07) == 0x07) && id > 0 && id <= 0x1A) {
      name_ = kOverlordNames[id - 1];
      overlord_ = 1;
    }
  }

  void InitSprite(const std::vector<uint8_t>& src, uint8_t overworld_map_id,
                  uint8_t id, uint8_t x, uint8_t y, int map_x, int map_y) {
    current_gfx_ = src;
    overworld_ = true;
    map_id_ = static_cast<int>(overworld_map_id);
    id_ = id;
    entity_type_ = zelda3::GameEntity::EntityType::kSprite;
    entity_id_ = id;
    x_ = map_x_;
    y_ = map_y_;
    nx_ = x;
    ny_ = y;
    name_ = ResolveSpriteName(id);
    map_x_ = map_x;
    map_y_ = map_y;
    // Defer preview_gfx_ allocation until DrawSpriteTile() is called
  }

  void Draw();
  void DrawSpriteTile(int x, int y, int srcx, int srcy, int pal,
                      bool mirror_x = false, bool mirror_y = false,
                      int sizex = 2, int sizey = 2);

  void UpdateMapProperties(uint16_t map_id,
                           const void* context = nullptr) override;
  void UpdateCoordinates(int map_x, int map_y);

  auto preview_graphics() const { return &preview_gfx_; }
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

  auto width() const { return width_; }
  auto height() const { return height_; }
  auto name() { return name_; }
  auto deleted() const { return deleted_; }
  auto set_deleted(bool deleted) { deleted_ = deleted; }
  auto set_key_drop(int key) { key_drop_ = key; }
  auto key_drop() const { return key_drop_; }

  // Check if this sprite is an overlord (special sprite type with different limits)
  bool IsOverlord() const { return overlord_ != 0; }

 private:
  uint8_t map_id_;
  uint8_t game_state_;
  uint8_t id_;
  uint8_t nx_;
  uint8_t ny_;
  uint8_t overlord_ = 0;
  uint8_t lower_x_ = 32;
  uint8_t lower_y_ = 32;
  uint8_t higher_x_ = 0;
  uint8_t higher_y_ = 0;

  int width_ = 16;
  int height_ = 16;
  int map_x_ = 0;
  int map_y_ = 0;
  int layer_ = 0;
  int subtype_ = 0;
  int key_drop_ = 0;

  bool deleted_ = false;
  bool overworld_;

  std::string name_;
  std::vector<uint8_t> preview_gfx_;
  std::vector<uint8_t> current_gfx_;

  SDL_Rect bounding_box_;
};

}  // namespace zelda3
}  // namespace yaze

#endif
