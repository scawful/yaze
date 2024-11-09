#ifndef YAZE_BASE_DUNGEON_H_
#define YAZE_BASE_DUNGEON_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct object_door {
  object_door() = default;
  object_door(short id, uint8_t x, uint8_t y, uint8_t size, uint8_t layer)
      : id_(id), x_(x), y_(y), size_(size), layer_(layer) {}

  short id_;
  uint8_t x_;
  uint8_t y_;
  uint8_t size_;
  uint8_t type_;
  uint8_t layer_;
};

#ifdef __cplusplus
}
#endif

#endif  // YAZE_BASE_DUNGEON_H_
