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

typedef struct z3_rom z3_rom;
struct z3_rom {
  const char* filename;
  const uint8_t* data;
  size_t size;
  void* impl;  // yaze::app::Rom*
};

struct yaze_flags {
  int debug;
  const char* rom_filename;
  z3_rom* rom;
};

z3_rom* yaze_load_rom(const char* filename);
void yaze_unload_rom(z3_rom* rom);

snes_color yaze_get_color_from_paletteset(const z3_rom* rom, int palette_set,
                                          int palette, int color);

#ifdef __cplusplus
}
#endif

#endif  // YAZE_H
