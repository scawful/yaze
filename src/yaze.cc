// C API implementation - no heavy GUI/editor dependencies
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "app/editor/message/message_data.h"
#include "rom/rom.h"
#include "yaze.h"
#include "yaze_config.h"
#include "zelda3/game_data.h"
#include "zelda3/overworld/overworld.h"

extern "C" {

// Static variables for library state
static bool g_library_initialized = false;

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

const char* yaze_get_version_string() { return YAZE_VERSION_STRING; }

int yaze_get_version_number() { return YAZE_VERSION_NUMBER; }

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

  // Create and load GameData
  auto internal_game_data = std::make_unique<yaze::zelda3::GameData>();
  auto load_status = yaze::zelda3::LoadGameData(*internal_rom, *internal_game_data);
  if (!load_status.ok()) {
    return nullptr;
  }

  auto* rom = new zelda3_rom();
  rom->filename = filename;
  rom->impl = internal_rom.release();  // Transfer ownership
  rom->game_data = internal_game_data.release();  // Transfer ownership
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

  if (rom->game_data != nullptr) {
    delete static_cast<yaze::zelda3::GameData*>(rom->game_data);
    rom->game_data = nullptr;
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
      .backup = true, .save_new = false, .filename = filename});

  if (!status.ok()) {
    return YAZE_ERROR_IO;
  }

  rom->is_modified = false;
  return YAZE_OK;
}

yaze_bitmap yaze_load_bitmap(const char* filename) {
  yaze_bitmap bitmap;
  bitmap.width = 0;
  bitmap.height = 0;
  bitmap.bpp = 0;
  bitmap.data = nullptr;
  return bitmap;
}

snes_color yaze_get_color_from_paletteset(const zelda3_rom* rom,
                                          int palette_set, int palette,
                                          int color) {
  snes_color color_struct;
  color_struct.red = 0;
  color_struct.green = 0;
  color_struct.blue = 0;

  if (rom->game_data) {
    auto* game_data = static_cast<yaze::zelda3::GameData*>(rom->game_data);
    auto get_color =
        game_data->palette_groups
            .get_group(yaze::gfx::kPaletteGroupAddressesKeys[palette_set])
            ->palette(palette)[color];
    color_struct = get_color.rom_color();

    return color_struct;
  }

  return color_struct;
}

zelda3_overworld* yaze_load_overworld(const zelda3_rom* rom) {
  if (rom->impl == nullptr) {
    return nullptr;
  }

  yaze::Rom* internal_rom = static_cast<yaze::Rom*>(rom->impl);
  auto* game_data = static_cast<yaze::zelda3::GameData*>(rom->game_data);
  auto internal_overworld = new yaze::zelda3::Overworld(internal_rom, game_data);
  if (!internal_overworld->Load(internal_rom).ok()) {
    return nullptr;
  }

  zelda3_overworld* overworld = new zelda3_overworld();
  overworld->impl = internal_overworld;
  int map_id = 0;
  for (const auto& ow_map : internal_overworld->overworld_maps()) {
    overworld->maps[map_id] = new zelda3_overworld_map();
    overworld->maps[map_id]->id = map_id;
    map_id++;
  }
  return overworld;
}

zelda3_dungeon_room* yaze_load_all_rooms(const zelda3_rom* rom) {
  if (rom->impl == nullptr) {
    return nullptr;
  }
  yaze::Rom* internal_rom = static_cast<yaze::Rom*>(rom->impl);
  zelda3_dungeon_room* rooms = new zelda3_dungeon_room[256];
  return rooms;
}

yaze_status yaze_load_messages(const zelda3_rom* rom, zelda3_message** messages,
                               int* message_count) {
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
      // Safe string copy with bounds checking
      std::memcpy((*messages)[i].parsed_text, msg.ContentsParsed.c_str(),
                  msg.ContentsParsed.length());
      (*messages)[i].parsed_text[msg.ContentsParsed.length()] = '\0';

      (*messages)[i].is_compressed = msg.Data.size() != msg.DataParsed.size();
      (*messages)[i].encoding_type = 1;  // ALttP standard encoding
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

  if (width <= 0 || height <= 0 ||
      (bpp != 1 && bpp != 2 && bpp != 4 && bpp != 8)) {
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
  color.red = r;  // Store full 8-bit values (existing code expects this)
  color.green = g;
  color.blue = b;
  return color;
}

void yaze_snes_color_to_rgb(snes_color color, uint8_t* r, uint8_t* g,
                            uint8_t* b) {
  if (r != nullptr) *r = static_cast<uint8_t>(color.red);
  if (g != nullptr) *g = static_cast<uint8_t>(color.green);
  if (b != nullptr) *b = static_cast<uint8_t>(color.blue);
}

// Version detection functions
zelda3_version zelda3_detect_version(const uint8_t* rom_data, size_t size) {
  if (rom_data == nullptr || size < 0x8000) {
    return ZELDA3_VERSION_UNKNOWN;
  }

  // SNES LoROM header titles at 0x7FC0 (PC offset)
  // Titles are 21 bytes long
  std::string title;
  for (int i = 0; i < 21; ++i) {
    char c = static_cast<char>(rom_data[0x7FC0 + i]);
    if (c >= 0x20 && c < 0x7F) {
      title += c;
    }
  }

  if (absl::StrContains(title, "THE LEGEND OF ZELDA")) {
    // US or EU version often share this title
    // Check destination code at 0x7FD9: 0x01 = USA, 0x02+ = PAL regions
    uint8_t region = rom_data[0x7FD9];
    if (region == 0x00) return ZELDA3_VERSION_JP;
    if (region == 0x01) return ZELDA3_VERSION_US;
    return ZELDA3_VERSION_EU;
  }

  if (absl::StrContains(title, "ZELDA NO DENSETSU")) {
    return ZELDA3_VERSION_JP;
  }

  // Fallback: Check for common randomizer indicators
  if (absl::StrContains(title, "VT RANDO") ||
      absl::StrContains(title, "Z3 RANDOMIZER")) {
    return ZELDA3_VERSION_RANDOMIZER;
  }

  return ZELDA3_VERSION_US;  // Default assumption for ALttP clones/hacks
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

const zelda3_version_pointers* zelda3_get_version_pointers(
    zelda3_version version) {
  switch (version) {
    case ZELDA3_VERSION_US:
      return &zelda3_us_pointers;
    case ZELDA3_VERSION_JP:
      return &zelda3_jp_pointers;
    default:
      return &zelda3_us_pointers;  // Default fallback
  }
}
}  // extern "C"
