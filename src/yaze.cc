#include "yaze.h"

#include <iostream>
#include <memory>
#include <sstream>
#include <cstring>
#include <stdexcept>

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

// Static variables for library state
static bool g_library_initialized = false;

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

// Version and initialization functions
yaze_status yaze_library_init() {
  if (g_library_initialized) {
    return YAZE_OK;
  }
  
  // Initialize SDL and other subsystems if needed
  g_library_initialized = true;
  return YAZE_OK;
}

void yaze_library_shutdown() {
  if (!g_library_initialized) {
    return;
  }
  
  // Cleanup subsystems
  g_library_initialized = false;

  return;
}

const char* yaze_status_to_string(yaze_status status) {
  switch (status) {
    case YAZE_OK:
      return "Success";
    case YAZE_ERROR_UNKNOWN:
      return "Unknown error";
    case YAZE_ERROR_INVALID_ARG:
      return "Invalid argument";
    case YAZE_ERROR_FILE_NOT_FOUND:
      return "File not found";
    case YAZE_ERROR_MEMORY:
      return "Memory allocation failed";
    case YAZE_ERROR_IO:
      return "I/O operation failed";
    case YAZE_ERROR_CORRUPTION:
      return "Data corruption detected";
    case YAZE_ERROR_NOT_INITIALIZED:
      return "Component not initialized";
    default:
      return "Unknown status code";
  }
}

const char* yaze_get_version_string() {
  return YAZE_VERSION_STRING;
}

int yaze_get_version_number() {
  return YAZE_VERSION_NUMBER;
}

bool yaze_check_version_compatibility(const char* expected_version) {
  if (expected_version == nullptr) {
    return false;
  }
  return strcmp(expected_version, YAZE_VERSION_STRING) == 0;
}

yaze_status yaze_init(yaze_editor_context* context, const char* rom_filename) {
  if (context == nullptr) {
    return YAZE_ERROR_INVALID_ARG;
  }
  
  if (!g_library_initialized) {
    yaze_status init_status = yaze_library_init();
    if (init_status != YAZE_OK) {
      return init_status;
    }
  }
  
  context->rom = nullptr;
  context->error_message = nullptr;
  
  if (rom_filename != nullptr && strlen(rom_filename) > 0) {
    context->rom = yaze_load_rom(rom_filename);
    if (context->rom == nullptr) {
      context->error_message = "Failed to load ROM file";
      return YAZE_ERROR_FILE_NOT_FOUND;
    }
  }

  return YAZE_OK;
}

yaze_status yaze_shutdown(yaze_editor_context* context) {
  if (context == nullptr) {
    return YAZE_ERROR_INVALID_ARG;
  }
  
  if (context->rom != nullptr) {
    yaze_unload_rom(context->rom);
    context->rom = nullptr;
  }
  
  context->error_message = nullptr;
  return YAZE_OK;
}

zelda3_rom* yaze_load_rom(const char* filename) {
  if (filename == nullptr || strlen(filename) == 0) {
    return nullptr;
  }
  
  auto internal_rom = std::make_unique<yaze::Rom>();
  if (!internal_rom->LoadFromFile(filename).ok()) {
    return nullptr;
  }

  auto* rom = new zelda3_rom();
  rom->filename = filename;
  rom->impl = internal_rom.release();  // Transfer ownership
  rom->data = const_cast<uint8_t*>(static_cast<yaze::Rom*>(rom->impl)->data());
  rom->size = static_cast<yaze::Rom*>(rom->impl)->size();
  rom->version = ZELDA3_VERSION_US;  // Default, should be detected
  rom->is_modified = false;
  return rom;
}

void yaze_unload_rom(zelda3_rom* rom) {
  if (rom == nullptr) {
    return;
  }
  
  if (rom->impl != nullptr) {
    delete static_cast<yaze::Rom*>(rom->impl);
    rom->impl = nullptr;
  }

  delete rom;
}

int yaze_save_rom(zelda3_rom* rom, const char* filename) {
  if (rom == nullptr || filename == nullptr) {
    return YAZE_ERROR_INVALID_ARG;
  }
  
  if (rom->impl == nullptr) {
    return YAZE_ERROR_NOT_INITIALIZED;
  }
  
  auto* internal_rom = static_cast<yaze::Rom*>(rom->impl);
  auto status = internal_rom->SaveToFile(yaze::Rom::SaveSettings{
      .backup = true, 
      .save_new = false, 
      .filename = filename
  });
  
  if (!status.ok()) {
    return YAZE_ERROR_IO;
  }
  
  rom->is_modified = false;
  return YAZE_OK;
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

yaze_status yaze_load_messages(const zelda3_rom* rom, zelda3_message** messages, int* message_count) {
  if (rom == nullptr || messages == nullptr || message_count == nullptr) {
    return YAZE_ERROR_INVALID_ARG;
  }
  
  if (rom->impl == nullptr) {
    return YAZE_ERROR_NOT_INITIALIZED;
  }

  try {
    // Use LoadAllTextData from message_data.h
    std::vector<yaze::editor::MessageData> message_data =
        yaze::editor::ReadAllTextData(rom->data, 0);
    
    *message_count = static_cast<int>(message_data.size());
    *messages = new zelda3_message[*message_count];
    
    for (size_t i = 0; i < message_data.size(); ++i) {
      const auto& msg = message_data[i];
      (*messages)[i].id = msg.ID;
      (*messages)[i].rom_address = msg.Address;
      (*messages)[i].length = static_cast<uint16_t>(msg.RawString.length());
      
      // Allocate and copy string data
      (*messages)[i].raw_data = new uint8_t[msg.Data.size()];
      std::memcpy((*messages)[i].raw_data, msg.Data.data(), msg.Data.size());
      
      (*messages)[i].parsed_text = new char[msg.ContentsParsed.length() + 1];
      std::strcpy((*messages)[i].parsed_text, msg.ContentsParsed.c_str());
      
      (*messages)[i].is_compressed = false;  // TODO: Detect compression
      (*messages)[i].encoding_type = 0;      // TODO: Detect encoding
    }
  } catch (const std::exception& e) {
    return YAZE_ERROR_MEMORY;
  }

  return YAZE_OK;
}

// Additional API functions implementation

// Graphics functions
void yaze_free_bitmap(yaze_bitmap* bitmap) {
  if (bitmap != nullptr && bitmap->data != nullptr) {
    delete[] bitmap->data;
    bitmap->data = nullptr;
    bitmap->width = 0;
    bitmap->height = 0;
    bitmap->bpp = 0;
  }
}

yaze_bitmap yaze_create_bitmap(int width, int height, uint8_t bpp) {
  yaze_bitmap bitmap = {};
  
  if (width <= 0 || height <= 0 || (bpp != 1 && bpp != 2 && bpp != 4 && bpp != 8)) {
    return bitmap;  // Return empty bitmap on invalid args
  }
  
  bitmap.width = width;
  bitmap.height = height;
  bitmap.bpp = bpp;
  bitmap.data = new uint8_t[width * height]();
  
  return bitmap;
}

snes_color yaze_rgb_to_snes_color(uint8_t r, uint8_t g, uint8_t b) {
  snes_color color = {};
  color.red = r;    // Store full 8-bit values (existing code expects this)
  color.green = g;
  color.blue = b;
  return color;
}

void yaze_snes_color_to_rgb(snes_color color, uint8_t* r, uint8_t* g, uint8_t* b) {
  if (r != nullptr) *r = static_cast<uint8_t>(color.red);
  if (g != nullptr) *g = static_cast<uint8_t>(color.green);
  if (b != nullptr) *b = static_cast<uint8_t>(color.blue);
}

// Version detection functions
zelda3_version zelda3_detect_version(const uint8_t* rom_data, size_t size) {
  if (rom_data == nullptr || size < 0x100000) {
    return ZELDA3_VERSION_UNKNOWN;
  }
  
  // TODO: Implement proper version detection based on ROM header
  return ZELDA3_VERSION_US;  // Default assumption
}

const char* zelda3_version_to_string(zelda3_version version) {
  switch (version) {
    case ZELDA3_VERSION_US:
      return "US/North American";
    case ZELDA3_VERSION_JP:
      return "Japanese";
    case ZELDA3_VERSION_EU:
      return "European";
    case ZELDA3_VERSION_PROTO:
      return "Prototype";
    case ZELDA3_VERSION_RANDOMIZER:
      return "Randomizer";
    default:
      return "Unknown";
  }
}

const zelda3_version_pointers* zelda3_get_version_pointers(zelda3_version version) {
  switch (version) {
    case ZELDA3_VERSION_US:
      return &zelda3_us_pointers;
    case ZELDA3_VERSION_JP:
      return &zelda3_jp_pointers;
    default:
      return &zelda3_us_pointers;  // Default fallback
  }
}