#ifndef YAZE_INCL_SNES_TILE_H
#define YAZE_INCL_SNES_TILE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

typedef struct snes_tile8 {
  uint32_t id;
  uint32_t palette_id;
  uint8_t data[64];
} snes_tile8;

typedef struct snes_tile_info {
  uint16_t id;
  uint8_t palette;
  bool priority;
  bool vertical_mirror;
  bool horizontal_mirror;
} snes_tile_info;

typedef struct snes_tile16 {
  snes_tile_info tiles[4];
} snes_tile16;

typedef struct snes_tile32 {
  uint16_t t0;
  uint16_t t1;
  uint16_t t2;
  uint16_t t3;
} snes_tile32;

#ifdef __cplusplus
}
#endif

#endif
