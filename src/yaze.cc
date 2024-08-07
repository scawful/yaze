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

snes_color get_color_from_paletteset(const Rom* rom, int palette_set,
                                     int palette, int color) {
  snes_color color_struct;
  color_struct.red = 0;
  color_struct.green = 0;
  color_struct.blue = 0;

  if (rom->impl) {
    yaze::app::Rom* internal_rom = static_cast<yaze::app::Rom*>(rom->impl);
    auto get_color =
        internal_rom->palette_group()
            .get_group(yaze::app::gfx::kPaletteGroupAddressesKeys[palette_set])
            ->palette(palette)
            .GetColor(color);
    if (!get_color.ok()) {
      return color_struct;
    }
    color_struct = get_color.value().rom_color();

    return color_struct;
  }

  return color_struct;
}