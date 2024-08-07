#include "yaze.h"

#include "app/rom.h"

// TODO: Implement yaze_initialize
void yaze_initialize(void) {}

// TODO: Implement yaze_cleanup
void yaze_cleanup(void) {}

Rom load_rom(const char* filename) {
  yaze::app::Rom* internal_rom;
  internal_rom = new yaze::app::Rom();
  if (!internal_rom->LoadFromFile(filename).ok()) {
    delete internal_rom;
    Rom rom;
    rom.impl = nullptr;
    rom.filename = filename;
    rom.data = nullptr;
    rom.size = 0;
    return rom;
  }

  Rom rom;
  rom.impl = internal_rom;
  rom.filename = filename;
  rom.data = internal_rom->data();
  rom.size = internal_rom->size();
  return rom;
}

void unload_rom(Rom rom) {
  if (rom.impl) {
    delete static_cast<yaze::app::Rom*>(rom.impl);
  }
}