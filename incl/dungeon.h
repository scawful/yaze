#ifndef YAZE_BASE_DUNGEON_H_
#define YAZE_BASE_DUNGEON_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef struct z3_dungeon_room {
  typedef struct object_door {
    short id;
    uint8_t x;
    uint8_t y;
    uint8_t size;
    uint8_t type;
    uint8_t layer;
  } object_door;
  object_door* doors;

  typedef struct staircase {
    uint8_t id;
    uint8_t room;
    const char* label;
  } staircase;
  staircase* staircases;

  typedef struct chest {
    uint8_t x;
    uint8_t y;
    uint8_t item;
    bool picker;
    bool big_chest;
  } chest;
  chest* chests;

  typedef struct chest_data {
    uint8_t id;
    bool size;
  } chest_data;
  chest_data* chests;

  typedef enum background2 {
    Off,
    Parallax,
    Dark,
    OnTop,
    Translucent,
    Addition,
    Normal,
    Transparent,
    DarkRoom
  } background2;
  background2 bg2;

  typedef struct destination {
    uint8_t index;
    uint8_t target;
    uint8_t target_layer;
  } destination;

  destination pits;
  destination stairs[4];
} z3_dungeon_room;

#ifdef __cplusplus
}
#endif

#endif  // YAZE_BASE_DUNGEON_H_
