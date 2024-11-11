#ifndef YAZE_BASE_DUNGEON_H_
#define YAZE_BASE_DUNGEON_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct z3_object_door {
  short id;
  uint8_t x;
  uint8_t y;
  uint8_t size;
  uint8_t type;
  uint8_t layer;
};

struct z3_dungeon_destination {
  uint8_t index;
  uint8_t target;
  uint8_t target_layer;
};

struct z3_staircase {
  uint8_t id;
  uint8_t room;
  const char *label;
};

struct z3_chest {
  uint8_t x;
  uint8_t y;
  uint8_t item;
  bool picker;
  bool big_chest;
};

struct z3_chest_data {
  uint8_t id;
  bool size;
};

enum z3_dungeon_background2 {
  Off,
  Parallax,
  Dark,
  OnTop,
  Translucent,
  Addition,
  Normal,
  Transparent,
  DarkRoom
};

#ifdef __cplusplus
}
#endif

#endif  // YAZE_BASE_DUNGEON_H_
