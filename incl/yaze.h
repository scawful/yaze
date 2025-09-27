#ifndef YAZE_H
#define YAZE_H

/**
 * @file yaze.h
 * @brief Yet Another Zelda3 Editor (YAZE) - Public C API
 * 
 * This header provides the main C API for YAZE, a modern ROM editor for
 * The Legend of Zelda: A Link to the Past. This API allows external
 * applications to interact with YAZE's functionality.
 * 
 * @version 0.3.1
 * @author YAZE Team
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "zelda.h"

/** 
 * @defgroup version Version Information
 * @{
 */

/** Major version number */
#define YAZE_VERSION_MAJOR 0
/** Minor version number */
#define YAZE_VERSION_MINOR 3
/** Patch version number */
#define YAZE_VERSION_PATCH 1

/** Combined version as a string */
#define YAZE_VERSION_STRING "0.3.1"

/** Combined version as a number (major * 10000 + minor * 100 + patch) */
#define YAZE_VERSION_NUMBER 301

/** @} */

typedef struct yaze_editor_context {
  zelda3_rom* rom;
  const char* error_message;
} yaze_editor_context;

/**
 * @defgroup core Core API
 * @{
 */

/**
 * @brief Status codes returned by YAZE functions
 * 
 * All YAZE functions that can fail return a status code to indicate
 * success or the type of error that occurred.
 */
typedef enum yaze_status {
  YAZE_OK = 0,                /**< Operation completed successfully */
  YAZE_ERROR_UNKNOWN = -1,    /**< Unknown error occurred */
  YAZE_ERROR_INVALID_ARG = 1, /**< Invalid argument provided */
  YAZE_ERROR_FILE_NOT_FOUND = 2, /**< File not found */
  YAZE_ERROR_MEMORY = 3,      /**< Memory allocation failed */
  YAZE_ERROR_IO = 4,          /**< I/O operation failed */
  YAZE_ERROR_CORRUPTION = 5,  /**< Data corruption detected */
  YAZE_ERROR_NOT_INITIALIZED = 6, /**< Component not initialized */
} yaze_status;

/**
 * @brief Convert a status code to a human-readable string
 * 
 * @param status The status code to convert
 * @return A null-terminated string describing the status
 */
const char* yaze_status_to_string(yaze_status status);

/**
 * @brief Initialize the YAZE library
 * 
 * This function must be called before using any other YAZE functions.
 * It initializes internal subsystems and prepares the library for use.
 * 
 * @return YAZE_OK on success, error code on failure
 */
yaze_status yaze_library_init(void);

/**
 * @brief Shutdown the YAZE library
 * 
 * This function cleans up resources allocated by yaze_library_init().
 * After calling this function, no other YAZE functions should be called
 * until yaze_library_init() is called again.
 */
void yaze_library_shutdown(void);

/**
 * @brief Main entry point for the YAZE application
 * 
 * @param argc Number of command line arguments
 * @param argv Array of command line argument strings
 * @return Exit code (0 for success, non-zero for error)
 */
int yaze_app_main(int argc, char** argv);

/**
 * @brief Check if the current YAZE version is compatible with the expected version
 * 
 * @param expected_version Expected version string (e.g., "0.3.1")
 * @return true if compatible, false otherwise
 */
bool yaze_check_version_compatibility(const char* expected_version);

/**
 * @brief Get the current YAZE version string
 * 
 * @return A null-terminated string containing the version
 */
const char* yaze_get_version_string(void);

/**
 * @brief Get the current YAZE version number
 * 
 * @return Version number (major * 10000 + minor * 100 + patch)
 */
int yaze_get_version_number(void);

/**
 * @brief Initialize a YAZE editor context
 * 
 * Creates and initializes an editor context for working with ROM files.
 * The context manages the ROM data and provides access to editing functions.
 * 
 * @param context Pointer to context structure to initialize
 * @param rom_filename Path to the ROM file to load (can be NULL to create empty context)
 * @return YAZE_OK on success, error code on failure
 * 
 * @note The caller is responsible for calling yaze_shutdown() to clean up the context
 */
yaze_status yaze_init(yaze_editor_context* context, const char* rom_filename);

/**
 * @brief Shutdown and clean up a YAZE editor context
 * 
 * Releases all resources associated with the context, including ROM data.
 * After calling this function, the context should not be used.
 * 
 * @param context Pointer to context to shutdown
 * @return YAZE_OK on success, error code on failure
 */
yaze_status yaze_shutdown(yaze_editor_context* context);

/** @} */

/**
 * @defgroup graphics Graphics and Bitmap Functions
 * @{
 */

/**
 * @brief Bitmap data structure
 * 
 * Represents a bitmap image with pixel data and metadata.
 */
typedef struct yaze_bitmap {
  int width;      /**< Width in pixels */
  int height;     /**< Height in pixels */
  uint8_t bpp;    /**< Bits per pixel (1, 2, 4, 8) */
  uint8_t* data;  /**< Pixel data (caller owns memory) */
} yaze_bitmap;

/**
 * @brief Load a bitmap from file
 * 
 * Loads a bitmap image from the specified file. Supports common
 * image formats and SNES-specific formats.
 * 
 * @param filename Path to the image file
 * @return Bitmap structure with loaded data, or empty bitmap on error
 * 
 * @note The caller is responsible for freeing the data pointer
 */
yaze_bitmap yaze_load_bitmap(const char* filename);

/**
 * @brief Free bitmap data
 * 
 * Releases memory allocated for bitmap pixel data.
 * 
 * @param bitmap Pointer to bitmap structure to free
 */
void yaze_free_bitmap(yaze_bitmap* bitmap);

/**
 * @brief Create an empty bitmap
 * 
 * Allocates a new bitmap with the specified dimensions.
 * 
 * @param width Width in pixels
 * @param height Height in pixels
 * @param bpp Bits per pixel
 * @return Initialized bitmap structure, or empty bitmap on error
 */
yaze_bitmap yaze_create_bitmap(int width, int height, uint8_t bpp);

/**
 * @brief SNES color in 15-bit RGB format (BGR555)
 * 
 * Represents a color in the SNES native format. Colors are stored
 * as 8-bit values but only the lower 5 bits are used by the SNES.
 */
typedef struct snes_color {
  uint16_t red;   /**< Red component (0-255, but SNES uses 0-31) */
  uint16_t green; /**< Green component (0-255, but SNES uses 0-31) */
  uint16_t blue;  /**< Blue component (0-255, but SNES uses 0-31) */
} snes_color;

/**
 * @brief Convert RGB888 color to SNES color
 * 
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @return SNES color structure
 */
snes_color yaze_rgb_to_snes_color(uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Convert SNES color to RGB888
 * 
 * @param color SNES color to convert
 * @param r Pointer to store red component (0-255)
 * @param g Pointer to store green component (0-255)
 * @param b Pointer to store blue component (0-255)
 */
void yaze_snes_color_to_rgb(snes_color color, uint8_t* r, uint8_t* g, uint8_t* b);

/**
 * @brief SNES color palette
 * 
 * Represents a color palette used by the SNES. Each palette contains
 * up to 256 colors, though most modes use fewer colors per palette.
 */
typedef struct snes_palette {
  uint16_t id;        /**< Palette ID (0-255) */
  uint16_t size;      /**< Number of colors in palette (1-256) */
  snes_color* colors; /**< Array of colors (caller owns memory) */
} snes_palette;

/**
 * @brief Create an empty palette
 * 
 * @param id Palette ID
 * @param size Number of colors to allocate
 * @return Initialized palette structure, or NULL on error
 */
snes_palette* yaze_create_palette(uint16_t id, uint16_t size);

/**
 * @brief Free palette memory
 * 
 * @param palette Pointer to palette to free
 */
void yaze_free_palette(snes_palette* palette);

/**
 * @brief Load palette from ROM
 * 
 * @param rom ROM to load palette from
 * @param palette_id ID of palette to load
 * @return Loaded palette, or NULL on error
 */
snes_palette* yaze_load_palette_from_rom(const zelda3_rom* rom, uint16_t palette_id);

/**
 * @brief 8x8 SNES tile data
 * 
 * Represents an 8x8 pixel tile with indexed color data.
 * Each pixel value is an index into a palette.
 */
typedef struct snes_tile8 {
  uint32_t id;        /**< Tile ID for reference */
  uint32_t palette_id; /**< Associated palette ID */
  uint8_t data[64];   /**< 64 pixels in row-major order (y*8+x) */
} snes_tile8;

/**
 * @brief Load tile data from ROM
 * 
 * @param rom ROM to load from
 * @param tile_id ID of tile to load
 * @param bpp Bits per pixel (1, 2, 4, 8)
 * @return Loaded tile data, or empty tile on error
 */
snes_tile8 yaze_load_tile_from_rom(const zelda3_rom* rom, uint32_t tile_id, uint8_t bpp);

/**
 * @brief Convert tile data between different bit depths
 * 
 * @param tile Source tile data
 * @param from_bpp Source bits per pixel
 * @param to_bpp Target bits per pixel
 * @return Converted tile data
 */
snes_tile8 yaze_convert_tile_bpp(const snes_tile8* tile, uint8_t from_bpp, uint8_t to_bpp);

typedef struct snes_tile_info {
  uint16_t id;
  uint8_t palette;
  bool priority;
  bool vertical_mirror;
  bool horizontal_mirror;
} snes_tile_info;

typedef struct snes_tile16 {
  snes_tile_info tiles[4];
} snes_tile16;

typedef struct snes_tile32 {
  uint16_t t0;
  uint16_t t1;
  uint16_t t2;
  uint16_t t3;
} snes_tile32;

/** @} */

/**
 * @defgroup rom ROM Manipulation
 * @{
 */

/**
 * @brief Load a ROM file
 * 
 * Loads a Zelda 3 ROM file and validates its format.
 * 
 * @param filename Path to ROM file (.sfc, .smc, etc.)
 * @return Pointer to ROM structure, or NULL on error
 * 
 * @note Caller must call yaze_unload_rom() to free memory
 */
zelda3_rom* yaze_load_rom_file(const char* filename);

/**
 * @brief Validate ROM integrity
 * 
 * Checks if the ROM data is valid and uncorrupted.
 * 
 * @param rom ROM to validate
 * @return YAZE_OK if valid, error code if corrupted
 */
yaze_status yaze_validate_rom(const zelda3_rom* rom);

/**
 * @brief Get ROM information
 * 
 * @param rom ROM to query
 * @param version Pointer to store detected ROM version
 * @param size Pointer to store ROM size in bytes
 * @return YAZE_OK on success, error code on failure
 */
yaze_status yaze_get_rom_info(const zelda3_rom* rom, zelda3_version* version, uint64_t* size);

/**
 * @brief Get a color from a palette set
 *
 * Retrieves a specific color from a palette set in the ROM.
 *
 * @param rom The ROM to get the color from
 * @param palette_set The palette set index (0-255)
 * @param palette The palette index within the set (0-15)
 * @param color The color index within the palette (0-15)
 * @return The color from the palette set
 */
snes_color yaze_get_color_from_paletteset(const zelda3_rom* rom,
                                          int palette_set, int palette,
                                          int color);

/** @} */

/**
 * @defgroup overworld Overworld Functions
 * @{
 */

/**
 * @brief Load the overworld from ROM
 *
 * Loads and parses the overworld data from the ROM, including all maps,
 * sprites, and related data structures.
 *
 * @param rom The ROM to load the overworld from
 * @return Pointer to overworld structure, or NULL on error
 * 
 * @note Caller must free the returned pointer when done
 */
zelda3_overworld* yaze_load_overworld(const zelda3_rom* rom);

/**
 * @brief Free overworld data
 * 
 * @param overworld Pointer to overworld to free
 */
void yaze_free_overworld(zelda3_overworld* overworld);

/**
 * @brief Get overworld map by index
 * 
 * @param overworld Overworld data
 * @param map_index Map index (0-159 for most ROMs)
 * @return Pointer to map data, or NULL if invalid index
 */
const zelda3_overworld_map* yaze_get_overworld_map(const zelda3_overworld* overworld, int map_index);

/**
 * @brief Get total number of overworld maps
 * 
 * @param overworld Overworld data
 * @return Number of maps available
 */
int yaze_get_overworld_map_count(const zelda3_overworld* overworld);

/** @} */

/**
 * @defgroup dungeon Dungeon Functions
 * @{
 */

/**
 * @brief Load all dungeon rooms from ROM
 *
 * Loads and parses all dungeon room data from the ROM.
 *
 * @param rom The ROM to load rooms from
 * @param room_count Pointer to store the number of rooms loaded
 * @return Array of room structures, or NULL on error
 * 
 * @note Caller must free the returned array when done
 */
zelda3_dungeon_room* yaze_load_all_rooms(const zelda3_rom* rom, int* room_count);

/**
 * @brief Load a specific dungeon room
 * 
 * @param rom ROM to load from
 * @param room_id Room ID to load (0-295 for most ROMs)
 * @return Pointer to room data, or NULL on error
 */
const zelda3_dungeon_room* yaze_load_room(const zelda3_rom* rom, int room_id);

/**
 * @brief Free dungeon room data
 * 
 * @param rooms Array of rooms to free
 * @param room_count Number of rooms in array
 */
void yaze_free_rooms(zelda3_dungeon_room* rooms, int room_count);

/** @} */

/**
 * @defgroup messages Message System
 * @{
 */

/**
 * @brief Load all text messages from ROM
 *
 * Loads and parses all in-game text messages from the ROM.
 *
 * @param rom The ROM to load messages from
 * @param messages Pointer to store array of messages
 * @param message_count Pointer to store number of messages loaded
 * @return YAZE_OK on success, error code on failure
 * 
 * @note Caller must free the messages array when done
 */
yaze_status yaze_load_messages(const zelda3_rom* rom, zelda3_message** messages, int* message_count);

/**
 * @brief Get a specific message by ID
 * 
 * @param rom ROM to load from
 * @param message_id Message ID to retrieve
 * @return Pointer to message data, or NULL if not found
 */
const zelda3_message* yaze_get_message(const zelda3_rom* rom, int message_id);

/**
 * @brief Free message data
 * 
 * @param messages Array of messages to free
 * @param message_count Number of messages in array
 */
void yaze_free_messages(zelda3_message* messages, int message_count);

/** @} */

/**
 * @brief Function pointer to initialize the extension.
 *
 * @param context The editor context.
 */
typedef void (*yaze_initialize_func)(yaze_editor_context* context);
typedef void (*yaze_cleanup_func)(void);

/**
 * @defgroup extensions Extension System
 * @{
 */

/**
 * @brief Extension interface for YAZE
 *
 * Defines the interface for YAZE extensions. Extensions can add new
 * functionality to YAZE and can be written in C or other languages.
 */
typedef struct yaze_extension {
  const char* name;           /**< Extension name (must not be NULL) */
  const char* version;        /**< Extension version string */
  const char* description;    /**< Brief description of functionality */
  const char* author;         /**< Extension author */
  int api_version;           /**< Required YAZE API version */

  /**
   * @brief Initialize the extension
   *
   * Called when the extension is loaded. Use this to set up
   * any resources or state needed by the extension.
   *
   * @param context Editor context provided by YAZE
   * @return YAZE_OK on success, error code on failure
   */
  yaze_status (*initialize)(yaze_editor_context* context);

  /**
   * @brief Clean up the extension
   *
   * Called when the extension is unloaded. Use this to clean up
   * any resources or state used by the extension.
   */
  void (*cleanup)(void);

  /**
   * @brief Get extension capabilities
   *
   * Returns a bitmask indicating what features this extension provides.
   *
   * @return Capability flags (see YAZE_EXT_CAP_* constants)
   */
  uint32_t (*get_capabilities)(void);
} yaze_extension;

/** Extension capability flags */
#define YAZE_EXT_CAP_ROM_EDITING    (1 << 0)  /**< Can edit ROM data */
#define YAZE_EXT_CAP_GRAPHICS       (1 << 1)  /**< Provides graphics functions */
#define YAZE_EXT_CAP_AUDIO          (1 << 2)  /**< Provides audio functions */
#define YAZE_EXT_CAP_SCRIPTING      (1 << 3)  /**< Provides scripting support */
#define YAZE_EXT_CAP_IMPORT_EXPORT  (1 << 4)  /**< Can import/export data */

/**
 * @brief Register an extension with YAZE
 * 
 * @param extension Extension to register
 * @return YAZE_OK on success, error code on failure
 */
yaze_status yaze_register_extension(const yaze_extension* extension);

/**
 * @brief Unregister an extension
 * 
 * @param name Name of extension to unregister
 * @return YAZE_OK on success, error code on failure
 */
yaze_status yaze_unregister_extension(const char* name);

/** @} */

#ifdef __cplusplus
}
#endif

#endif  // YAZE_H
