#include "yaze.h"

#include <iostream>
#include <memory>
#include <sstream>

#include "app/core/controller.h"
#include "app/core/platform/app_delegate.h"
#include "app/editor/message/message_data.h"
#include "app/rom.h"
#include "app/zelda3/overworld/overworld.h"
#include "util/flag.h"
#include "yaze_config.h"

DEFINE_FLAG(std::string, rom_file, "",
            "Path to the ROM file to load. "
            "If not specified, the app will run without a ROM.");

int yaze_app_main(int argc, char **argv) {
  yaze::util::FlagParser parser(yaze::util::global_flag_registry());
  RETURN_IF_EXCEPTION(parser.Parse(argc, argv));
  std::string rom_filename = "";
  if (!FLAGS_rom_file->Get().empty()) {
    rom_filename = FLAGS_rom_file->Get();
  }

#ifdef __APPLE__
  return yaze_run_cocoa_app_delegate(rom_filename.c_str());
#endif

  auto controller = std::make_unique<yaze::core::Controller>();
  EXIT_IF_ERROR(controller->OnEntry(rom_filename))
  while (controller->IsActive()) {
    controller->OnInput();
    if (auto status = controller->OnLoad(); !status.ok()) {
      std::cerr << status.message() << std::endl;
      break;
    }
    controller->DoRender();
  }
  controller->OnExit();
  return EXIT_SUCCESS;
}

void yaze_check_version(const char *version) {
  std::string current_version;
  std::stringstream ss;
  ss << YAZE_VERSION_MAJOR << "." << YAZE_VERSION_MINOR << "."
     << YAZE_VERSION_PATCH;
  ss >> current_version;

  if (version != current_version) {
    std::cout << "Yaze version mismatch: expected " << current_version
              << ", got " << version << std::endl;
    exit(1);
  }
}

yaze_status yaze_init(yaze_editor_context *yaze_ctx, char *rom_filename) {
  yaze_ctx->rom = yaze_load_rom(rom_filename);
  if (yaze_ctx->rom == nullptr) {
    yaze_ctx->error_message = "Failed to load ROM";
    return yaze_status::YAZE_ERROR;
  }

  return yaze_status::YAZE_OK;
}

yaze_status yaze_shutdown(yaze_editor_context *yaze_ctx) {
  if (yaze_ctx->rom) {
    yaze_unload_rom(yaze_ctx->rom);
  }
  return yaze_status::YAZE_OK;
}

zelda3_rom *yaze_load_rom(const char *filename) {
  yaze::Rom *internal_rom;
  internal_rom = new yaze::Rom();
  if (!internal_rom->LoadFromFile(filename).ok()) {
    delete internal_rom;
    return nullptr;
  }

  zelda3_rom *rom = new zelda3_rom();
  rom->filename = filename;
  rom->impl = internal_rom;
  rom->data = const_cast<uint8_t *>(internal_rom->data());
  rom->size = internal_rom->size();
  return rom;
}

void yaze_unload_rom(zelda3_rom *rom) {
  if (rom->impl) {
    delete static_cast<yaze::Rom *>(rom->impl);
  }

  if (rom) {
    delete rom;
  }
}

void yaze_save_rom(zelda3_rom *rom, const char *filename) {
  if (rom->impl) {
    yaze::Rom *internal_rom = static_cast<yaze::Rom *>(rom->impl);
    if (auto status = internal_rom->SaveToFile(yaze::Rom::SaveSettings{
            .backup = true, .save_new = false, .filename = filename});
        !status.ok()) {
      throw std::runtime_error(status.message().data());
    }
  }
}

yaze_bitmap yaze_load_bitmap(const char *filename) {
  yaze_bitmap bitmap;
  bitmap.width = 0;
  bitmap.height = 0;
  bitmap.bpp = 0;
  bitmap.data = nullptr;
  return bitmap;
}

snes_color yaze_get_color_from_paletteset(const zelda3_rom *rom,
                                          int palette_set, int palette,
                                          int color) {
  snes_color color_struct;
  color_struct.red = 0;
  color_struct.green = 0;
  color_struct.blue = 0;

  if (rom->impl) {
    yaze::Rom *internal_rom = static_cast<yaze::Rom *>(rom->impl);
    auto get_color =
        internal_rom->palette_group()
            .get_group(yaze::gfx::kPaletteGroupAddressesKeys[palette_set])
            ->palette(palette)[color];
    color_struct = get_color.rom_color();

    return color_struct;
  }

  return color_struct;
}

zelda3_overworld *yaze_load_overworld(const zelda3_rom *rom) {
  if (rom->impl == nullptr) {
    return nullptr;
  }

  yaze::Rom *internal_rom = static_cast<yaze::Rom *>(rom->impl);
  auto internal_overworld = new yaze::zelda3::Overworld(internal_rom);
  if (!internal_overworld->Load(internal_rom).ok()) {
    return nullptr;
  }

  zelda3_overworld *overworld = new zelda3_overworld();
  overworld->impl = internal_overworld;
  int map_id = 0;
  for (const auto &ow_map : internal_overworld->overworld_maps()) {
    overworld->maps[map_id] = new zelda3_overworld_map();
    overworld->maps[map_id]->id = map_id;
    map_id++;
  }
  return overworld;
}

zelda3_dungeon_room *yaze_load_all_rooms(const zelda3_rom *rom) {
  if (rom->impl == nullptr) {
    return nullptr;
  }
  yaze::Rom *internal_rom = static_cast<yaze::Rom *>(rom->impl);
  zelda3_dungeon_room *rooms = new zelda3_dungeon_room[256];
  return rooms;
}

yaze_status yaze_load_messages(zelda3_rom *rom, zelda3_message **messages) {
  if (rom->impl == nullptr) {
    return yaze_status::YAZE_ERROR;
  }

  // Use LoadAllTextData from message_data.h
  std::vector<yaze::editor::MessageData> message_data =
      yaze::editor::ReadAllTextData(rom->data, 0);
  for (const auto &message : message_data) {
    messages[message.ID] = new zelda3_message();
    messages[message.ID]->id = message.ID;
    messages[message.ID]->address = message.Address;
    messages[message.ID]->raw_string = reinterpret_cast<uint8_t *>(
        const_cast<char *>(message.RawString.data()));
    messages[message.ID]->contents_parsed = reinterpret_cast<uint8_t *>(
        const_cast<char *>(message.ContentsParsed.data()));
    messages[message.ID]->data =
        reinterpret_cast<uint8_t *>(const_cast<uint8_t *>(message.Data.data()));
    messages[message.ID]->data_parsed = reinterpret_cast<uint8_t *>(
        const_cast<uint8_t *>(message.DataParsed.data()));
  }

  return yaze_status::YAZE_OK;
}