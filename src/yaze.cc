#include "yaze.h"

#include "app/rom.h"
#include "app/zelda3/overworld/overworld.h"

void yaze_init(yaze_flags* flags) {
  if (flags == nullptr) {
    return;
  }

  if (flags->rom_filename == nullptr) {
    return;
  }

  flags->rom = yaze_load_rom(flags->rom_filename);
}

void yaze_cleanup(yaze_flags* flags) {
  if (flags->rom) {
    yaze_unload_rom(flags->rom);
  }
}

yaze_project* yaze_load_project(const char* filename) {
  yaze_project* project = new yaze_project();
  project->filename = filename;
  project->rom = yaze_load_rom(filename);
  project->overworld = yaze_load_overworld(project->rom);
  return project;
}

z3_rom* yaze_load_rom(const char* filename) {
  yaze::app::Rom* internal_rom;
  internal_rom = new yaze::app::Rom();
  if (!internal_rom->LoadFromFile(filename).ok()) {
    delete internal_rom;
    return nullptr;
  }

  z3_rom* rom = new z3_rom();
  rom->filename = filename;
  rom->impl = internal_rom;
  rom->data = internal_rom->data();
  rom->size = internal_rom->size();
  return rom;
}

void yaze_unload_rom(z3_rom* rom) {
  if (rom->impl) {
    delete static_cast<yaze::app::Rom*>(rom->impl);
  }

  if (rom) {
    delete rom;
  }
}

snes_color yaze_get_color_from_paletteset(const z3_rom* rom, int palette_set,
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

z3_overworld* yaze_load_overworld(const z3_rom* rom) {
  if (rom->impl == nullptr) {
    return nullptr;
  }

  yaze::app::Rom* internal_rom = static_cast<yaze::app::Rom*>(rom->impl);

  yaze::app::zelda3::overworld::Overworld* internal_overworld =
      new yaze::app::zelda3::overworld::Overworld();
  auto load_ow = internal_overworld->Load(*internal_rom);
  if (!load_ow.ok()) {
    return nullptr;
  }

  z3_overworld* overworld = new z3_overworld();
  overworld->impl = internal_overworld;
  int map_id = 0;
  for (const auto& ow_map : internal_overworld->overworld_maps()) {
    overworld->maps[map_id] = new z3_overworld_map();
    overworld->maps[map_id]->id = map_id;
    map_id++;
  }
  return overworld;
}