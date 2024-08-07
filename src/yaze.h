#ifndef YAZE_H
#define YAZE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include "base/snes_color.h"
#include "base/sprite.h"

void yaze_initialize(void);

void yaze_cleanup(void);

typedef struct rom rom;

struct rom {
  const char* filename;
  const uint8_t* data;
  size_t size;
  void* impl;  // yaze::app::Rom*
};

rom load_rom(const char* filename);
void unload_rom(rom rom);

snes_color get_color_from_paletteset(const rom* rom, int palette_set,
                                     int palette, int color);

#ifdef __cplusplus
}
#endif

#endif  // YAZE_H
