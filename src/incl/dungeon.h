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
  uint8_t target = 0;
  uint8_t target_layer = 0;
};

struct z3_staircase {
  uint8_t id;
  uint8_t room;
  const char *label;
};

struct z3_chest {
  uint8_t x = 0;
  uint8_t y = 0;
  uint8_t item = 0;
  bool picker = false;
  bool big_chest = false;
};

struct z3_chest_data {
  uchar id;
  bool size;
};

#ifdef __cplusplus
}
#endif

#endif  // YAZE_BASE_DUNGEON_H_
