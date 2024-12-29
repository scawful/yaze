#ifndef YAZE_BASE_DUNGEON_H_
#define YAZE_BASE_DUNGEON_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef struct z3_object_door {
  short id;
  uint8_t x;
  uint8_t y;
  uint8_t size;
  uint8_t type;
  uint8_t layer;
} z3_object_door;

typedef struct z3_dungeon_destination {
  uint8_t index;
  uint8_t target;
  uint8_t target_layer;
} z3_dungeon_destination;

typedef struct z3_staircase {
  uint8_t id;
  uint8_t room;
  const char *label;
} z3_staircase;

typedef struct z3_chest {
  uint8_t x;
  uint8_t y;
  uint8_t item;
  bool picker;
  bool big_chest;
} z3_chest;

typedef struct z3_chest_data {
  uint8_t id;
  bool size;
} z3_chest_data;

typedef enum z3_dungeon_background2 {
  Off,
  Parallax,
  Dark,
  OnTop,
  Translucent,
  Addition,
  Normal,
  Transparent,
  DarkRoom
} z3_dungeon_background2;

typedef struct z3_dungeon_room {
  z3_dungeon_background2 bg2;
  z3_dungeon_destination pits;
  z3_dungeon_destination stairs[4];
} z3_dungeon_room;

#ifdef __cplusplus
}
#endif

#endif // YAZE_BASE_DUNGEON_H_
