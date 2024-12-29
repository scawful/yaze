#include "yaze.h"

#include <cstdio>

#include "app/rom.h"
#include "app/zelda3/overworld/overworld.h"

void yaze_check_version(const char* version) {
  printf("Yaze version: %s\n", version);
  auto version_check = yaze::core::CheckVersion(version);
  if (!version_check.ok()) {
    printf("%s\n", version_check.status().message().data());
    exit(1);
  }
  return;
}

int yaze_init(yaze_editor_context* yaze_ctx) {
  if (yaze_ctx->project->rom_filename == nullptr) {
    return -1;
  }

  yaze_ctx->rom = yaze_load_rom(yaze_ctx->project->rom_filename);
  if (yaze_ctx->rom == nullptr) {
    return -1;
  }

  return 0;
}

void yaze_cleanup(yaze_editor_context* yaze_ctx) {
  if (yaze_ctx->rom) {
    yaze_unload_rom(yaze_ctx->rom);
  }
}

yaze_project yaze_load_project(const char* filename) {
  yaze_project project;
  project.filepath = filename;
  return project;
}

z3_rom* yaze_load_rom(const char* filename) {
  yaze::Rom* internal_rom;
  internal_rom = new yaze::Rom();
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
    delete static_cast<yaze::Rom*>(rom->impl);
  }

  if (rom) {
    delete rom;
  }
}

yaze_bitmap yaze_load_bitmap(const char* filename) {
  yaze_bitmap bitmap;
  bitmap.width = 0;
  bitmap.height = 0;
  bitmap.bpp = 0;
  bitmap.data = nullptr;
  return bitmap;
}

snes_color yaze_get_color_from_paletteset(const z3_rom* rom, int palette_set,
                                          int palette, int color) {
  snes_color color_struct;
  color_struct.red = 0;
  color_struct.green = 0;
  color_struct.blue = 0;

  if (rom->impl) {
    yaze::Rom* internal_rom = static_cast<yaze::Rom*>(rom->impl);
    auto get_color =
        internal_rom->palette_group()
            .get_group(yaze::gfx::kPaletteGroupAddressesKeys[palette_set])
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

  yaze::Rom* internal_rom = static_cast<yaze::Rom*>(rom->impl);
  auto internal_overworld = new yaze::zelda3::overworld::Overworld();
  if (!internal_overworld->Load(*internal_rom).ok()) {
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
