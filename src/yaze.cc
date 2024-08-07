#include "yaze.h"

// TODO: Implement yaze_initialize
void yaze_initialize(void) {}

// TODO: Implement yaze_cleanup
void yaze_cleanup(void) {}

// TODO: Implement load_rom
Rom load_rom(const char* filename) {
  Rom rom;
  rom.filename = filename;
  rom.data = nullptr;
  return rom;
}