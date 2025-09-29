#ifndef ZELDA_H
#define ZELDA_H

/**
 * @file zelda.h
 * @brief The Legend of Zelda: A Link to the Past - Data Structures and Constants
 * 
 * This header defines data structures and constants specific to
 * The Legend of Zelda: A Link to the Past ROM format and game data.
 * 
 * @version 0.3.2
 * @author YAZE Team
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/**
 * @defgroup rom_types ROM Types and Versions
 * @{
 */

/**
 * @brief Different versions of the game supported by YAZE
 * 
 * YAZE supports multiple regional versions and ROM hacks of
 * The Legend of Zelda: A Link to the Past.
 */
typedef enum zelda3_version {
  ZELDA3_VERSION_UNKNOWN = 0, /**< Unknown or unsupported version */
  ZELDA3_VERSION_US = 1,      /**< US/North American version */
  ZELDA3_VERSION_JP = 2,      /**< Japanese version */
  ZELDA3_VERSION_EU = 3,      /**< European version */
  ZELDA3_VERSION_PROTO = 4,   /**< Prototype/development version */
  ZELDA3_VERSION_RANDOMIZER = 5, /**< Randomizer ROM (experimental) */
  
  // Legacy aliases for backward compatibility
  US = ZELDA3_VERSION_US,     /**< @deprecated Use ZELDA3_VERSION_US */
  JP = ZELDA3_VERSION_JP,     /**< @deprecated Use ZELDA3_VERSION_JP */
  SD = ZELDA3_VERSION_PROTO,  /**< @deprecated Use ZELDA3_VERSION_PROTO */
  RANDO = ZELDA3_VERSION_RANDOMIZER, /**< @deprecated Use ZELDA3_VERSION_RANDOMIZER */
} zelda3_version;

/**
 * @brief Detect ROM version from header data
 * 
 * @param rom_data Pointer to ROM data
 * @param size Size of ROM data in bytes
 * @return Detected version, or ZELDA3_VERSION_UNKNOWN if not recognized
 */
zelda3_version zelda3_detect_version(const uint8_t* rom_data, size_t size);

/**
 * @brief Get version name as string
 * 
 * @param version Version enum value
 * @return Human-readable version name
 */
const char* zelda3_version_to_string(zelda3_version version);

/**
 * @brief ROM data pointers for different game versions
 * 
 * Contains memory addresses where specific data structures are located
 * within the ROM. These addresses vary between different regional versions.
 */
typedef struct zelda3_version_pointers {
  // New Google C++ style names
  uint32_t gfx_animated_pointer;           /**< Animated graphics pointer */
  uint32_t overworld_gfx_groups1;          /**< Overworld graphics group 1 */
  uint32_t overworld_gfx_groups2;          /**< Overworld graphics group 2 */
  uint32_t compressed_map32_pointers_high; /**< Map32 high pointers */
  uint32_t compressed_map32_pointers_low;  /**< Map32 low pointers */
  uint32_t overworld_map_palette_group;    /**< Map palette groups */
  uint32_t overlay_pointers;               /**< Overlay data pointers */
  uint32_t overlay_pointers_bank;          /**< Overlay bank number */
  uint32_t overworld_tiles_type;           /**< Tile type definitions */
  uint32_t overworld_gfx_ptr1;            /**< Graphics pointer 1 */
  uint32_t overworld_gfx_ptr2;            /**< Graphics pointer 2 */
  uint32_t overworld_gfx_ptr3;            /**< Graphics pointer 3 */
  uint32_t map32_tile_tl;                 /**< 32x32 tile top-left */
  uint32_t map32_tile_tr;                 /**< 32x32 tile top-right */
  uint32_t map32_tile_bl;                 /**< 32x32 tile bottom-left */
  uint32_t map32_tile_br;                 /**< 32x32 tile bottom-right */
  uint32_t sprite_blockset_pointer;        /**< Sprite graphics pointer */
  uint32_t dungeon_palettes_groups;       /**< Dungeon palette groups */
  
  // Legacy aliases for backward compatibility (deprecated)
  uint32_t kGfxAnimatedPointer;           /**< @deprecated Use gfx_animated_pointer */
  uint32_t kOverworldGfxGroups1;          /**< @deprecated Use overworld_gfx_groups1 */
  uint32_t kOverworldGfxGroups2;          /**< @deprecated Use overworld_gfx_groups2 */
  uint32_t kCompressedAllMap32PointersHigh; /**< @deprecated Use compressed_map32_pointers_high */
  uint32_t kCompressedAllMap32PointersLow;  /**< @deprecated Use compressed_map32_pointers_low */
  uint32_t kOverworldMapPaletteGroup;    /**< @deprecated Use overworld_map_palette_group */
  uint32_t kOverlayPointers;             /**< @deprecated Use overlay_pointers */
  uint32_t kOverlayPointersBank;         /**< @deprecated Use overlay_pointers_bank */
  uint32_t kOverworldTilesType;          /**< @deprecated Use overworld_tiles_type */
  uint32_t kOverworldGfxPtr1;            /**< @deprecated Use overworld_gfx_ptr1 */
  uint32_t kOverworldGfxPtr2;            /**< @deprecated Use overworld_gfx_ptr2 */
  uint32_t kOverworldGfxPtr3;            /**< @deprecated Use overworld_gfx_ptr3 */
  uint32_t kMap32TileTL;                 /**< @deprecated Use map32_tile_tl */
  uint32_t kMap32TileTR;                 /**< @deprecated Use map32_tile_tr */
  uint32_t kMap32TileBL;                 /**< @deprecated Use map32_tile_bl */
  uint32_t kMap32TileBR;                 /**< @deprecated Use map32_tile_br */
  uint32_t kSpriteBlocksetPointer;       /**< @deprecated Use sprite_blockset_pointer */
  uint32_t kDungeonPalettesGroups;       /**< @deprecated Use dungeon_palettes_groups */
} zelda3_version_pointers;

/**
 * @brief Get version-specific pointers
 * 
 * @param version ROM version
 * @return Pointer to version-specific address structure
 */
const zelda3_version_pointers* zelda3_get_version_pointers(zelda3_version version);

const static zelda3_version_pointers zelda3_us_pointers = {
    // New style names
    0x10275,  // gfx_animated_pointer
    0x5D97,   // overworld_gfx_groups1
    0x6073,   // overworld_gfx_groups2
    0x1794D,  // compressed_map32_pointers_high
    0x17B2D,  // compressed_map32_pointers_low
    0x75504,  // overworld_map_palette_group
    0x77664,  // overlay_pointers
    0x0E,     // overlay_pointers_bank
    0x71459,  // overworld_tiles_type
    0x4F80,   // overworld_gfx_ptr1
    0x505F,   // overworld_gfx_ptr2
    0x513E,   // overworld_gfx_ptr3
    0x18000,  // map32_tile_tl
    0x1B400,  // map32_tile_tr
    0x20000,  // map32_tile_bl
    0x23400,  // map32_tile_br
    0x5B57,   // sprite_blockset_pointer
    0x75460,  // dungeon_palettes_groups
    
    // Legacy k-prefixed names (same values for backward compatibility)
    0x10275,  // kGfxAnimatedPointer
    0x5D97,   // kOverworldGfxGroups1
    0x6073,   // kOverworldGfxGroups2
    0x1794D,  // kCompressedAllMap32PointersHigh
    0x17B2D,  // kCompressedAllMap32PointersLow
    0x75504,  // kOverworldMapPaletteGroup
    0x77664,  // kOverlayPointers
    0x0E,     // kOverlayPointersBank
    0x71459,  // kOverworldTilesType
    0x4F80,   // kOverworldGfxPtr1
    0x505F,   // kOverworldGfxPtr2
    0x513E,   // kOverworldGfxPtr3
    0x18000,  // kMap32TileTL
    0x1B400,  // kMap32TileTR
    0x20000,  // kMap32TileBL
    0x23400,  // kMap32TileBR
    0x5B57,   // kSpriteBlocksetPointer
    0x75460,  // kDungeonPalettesGroups
};

const static zelda3_version_pointers zelda3_jp_pointers = {
    // New style names
    0x10624,  // gfx_animated_pointer
    0x5DD7,   // overworld_gfx_groups1
    0x60B3,   // overworld_gfx_groups2
    0x176B1,  // compressed_map32_pointers_high
    0x17891,  // compressed_map32_pointers_low
    0x67E74,  // overworld_map_palette_group
    0x3FAF4,  // overlay_pointers
    0x07,     // overlay_pointers_bank
    0x7FD94,  // overworld_tiles_type
    0x4FC0,   // overworld_gfx_ptr1
    0x509F,   // overworld_gfx_ptr2
    0x517E,   // overworld_gfx_ptr3
    0x18000,  // map32_tile_tl
    0x1B3C0,  // map32_tile_tr
    0x20000,  // map32_tile_bl
    0x233C0,  // map32_tile_br
    0x5B97,   // sprite_blockset_pointer
    0x67DD0,  // dungeon_palettes_groups
    
    // Legacy k-prefixed names (same values for backward compatibility)
    0x10624,  // kGfxAnimatedPointer
    0x5DD7,   // kOverworldGfxGroups1
    0x60B3,   // kOverworldGfxGroups2
    0x176B1,  // kCompressedAllMap32PointersHigh
    0x17891,  // kCompressedAllMap32PointersLow
    0x67E74,  // kOverworldMapPaletteGroup
    0x3FAF4,  // kOverlayPointers
    0x07,     // kOverlayPointersBank
    0x7FD94,  // kOverworldTilesType
    0x4FC0,   // kOverworldGfxPtr1
    0x509F,   // kOverworldGfxPtr2
    0x517E,   // kOverworldGfxPtr3
    0x18000,  // kMap32TileTL
    0x1B3C0,  // kMap32TileTR
    0x20000,  // kMap32TileBL
    0x233C0,  // kMap32TileBR
    0x5B97,   // kSpriteBlocksetPointer
    0x67DD0,  // kDungeonPalettesGroups
};

/**
 * @brief ROM data structure
 * 
 * Represents a loaded Zelda 3 ROM with its data and metadata.
 */
typedef struct zelda3_rom {
  const char* filename;    /**< Original filename (can be NULL) */
  uint8_t* data;          /**< ROM data (read-only for external users) */
  uint64_t size;          /**< Size of ROM data in bytes */
  zelda3_version version; /**< Detected ROM version */
  bool is_modified;       /**< True if ROM has been modified */
  void* impl;            /**< Internal implementation pointer */
} zelda3_rom;

/** @} */

/**
 * @defgroup rom_functions ROM File Operations  
 * @{
 */

/**
 * @brief Load a ROM file
 * 
 * @param filename Path to ROM file
 * @return Loaded ROM structure, or NULL on error
 */
zelda3_rom* yaze_load_rom(const char* filename);

/**
 * @brief Unload and free ROM data
 * 
 * @param rom ROM to unload
 */
void yaze_unload_rom(zelda3_rom* rom);

/**
 * @brief Save ROM to file
 * 
 * @param rom ROM to save
 * @param filename Output filename
 * @return YAZE_OK on success, error code on failure
 */
int yaze_save_rom(zelda3_rom* rom, const char* filename);

/**
 * @brief Create a copy of ROM data
 * 
 * @param rom Source ROM
 * @return Copy of ROM, or NULL on error
 */
zelda3_rom* yaze_copy_rom(const zelda3_rom* rom);

/** @} */

/**
 * @defgroup messages Message Data Structures
 * @{
 */

/**
 * @brief In-game text message data
 * 
 * Represents a text message from the game, including both raw
 * ROM data and parsed/decoded text content.
 */
typedef struct zelda3_message {
  uint16_t id;                /**< Message ID (0-65535) */
  uint32_t rom_address;       /**< Address in ROM where message data starts */
  uint16_t length;            /**< Length of message data in bytes */
  uint8_t* raw_data;          /**< Raw message data from ROM */
  char* parsed_text;          /**< Decoded text content (UTF-8) */
  bool is_compressed;         /**< True if message uses compression */
  uint8_t encoding_type;      /**< Text encoding type used */
} zelda3_message;

/** @} */

/**
 * @defgroup overworld Overworld Data Structures
 * @{
 */

/**
 * @brief Overworld map data
 * 
 * Represents a single screen/area in the overworld, including
 * graphics, palette, music, and sprite information.
 */
typedef struct zelda3_overworld_map {
  uint16_t id;                    /**< Map ID (0-159 for most ROMs) */
  uint8_t parent_id;              /**< Parent map ID for sub-areas */
  uint8_t quadrant_id;            /**< Quadrant within parent (0-3) */
  uint8_t world_id;               /**< World number (Light/Dark) */
  uint8_t game_state;             /**< Game state requirements */
  
  /* Graphics and Visual Properties */
  uint8_t area_graphics;          /**< Area graphics set ID */
  uint8_t area_palette;           /**< Area palette set ID */
  uint8_t main_palette;           /**< Main palette ID */
  uint8_t animated_gfx;           /**< Animated graphics ID */
  
  /* Sprite Configuration */
  uint8_t sprite_graphics[3];     /**< Sprite graphics sets */
  uint8_t sprite_palette[3];      /**< Sprite palette sets */
  
  /* Audio Configuration */
  uint8_t area_music[4];          /**< Music tracks for different states */
  
  /* Extended Graphics (ZSCustomOverworld) */
  uint8_t static_graphics[16];    /**< Static graphics assignments */
  uint8_t custom_tileset[8];      /**< Custom tileset assignments */
  
  /* Screen Properties */
  uint16_t area_specific_bg_color; /**< Background color override */
  uint16_t subscreen_overlay;     /**< Subscreen overlay settings */
  
  /* Flags and Metadata */
  bool is_large_map;              /**< True for 32x32 maps */
  bool has_special_gfx;           /**< True if uses special graphics */
} zelda3_overworld_map;

/**
 * @brief Complete overworld data
 * 
 * Contains all overworld maps and related data for the entire game world.
 */
typedef struct zelda3_overworld {
  void* impl;                     /**< Internal implementation pointer */
  zelda3_overworld_map** maps;    /**< Array of overworld maps */
  int map_count;                  /**< Number of maps in array */
  zelda3_version rom_version;     /**< ROM version this data came from */
  bool has_zsco_features;         /**< True if ZSCustomOverworld features detected */
} zelda3_overworld;

/** @} */

/**
 * @defgroup dungeon Dungeon Data Structures
 * @{
 */

/**
 * @brief Dungeon sprite definition
 * 
 * Represents a sprite that can appear in dungeon rooms.
 */
typedef struct dungeon_sprite {
  const char* name;     /**< Sprite name (for debugging/display) */
  uint8_t id;          /**< Sprite type ID */
  uint8_t subtype;     /**< Sprite subtype/variant */
  uint8_t x;           /**< X position in room */
  uint8_t y;           /**< Y position in room */
  uint8_t layer;       /**< Layer (0=background, 1=foreground) */
  uint16_t properties; /**< Additional sprite properties */
} dungeon_sprite;

/**
 * @brief Background layer 2 effects
 * 
 * Defines the different visual effects that can be applied to
 * background layer 2 in dungeon rooms.
 */
typedef enum zelda3_bg2_effect {
  ZELDA3_BG2_OFF = 0,        /**< Background layer 2 disabled */
  ZELDA3_BG2_PARALLAX = 1,   /**< Parallax scrolling effect */
  ZELDA3_BG2_DARK = 2,       /**< Dark overlay effect */
  ZELDA3_BG2_ON_TOP = 3,     /**< Layer appears on top */
  ZELDA3_BG2_TRANSLUCENT = 4, /**< Semi-transparent overlay */
  ZELDA3_BG2_ADDITION = 5,   /**< Additive blending */
  ZELDA3_BG2_NORMAL = 6,     /**< Normal blending */
  ZELDA3_BG2_TRANSPARENT = 7, /**< Fully transparent */
  ZELDA3_BG2_DARK_ROOM = 8   /**< Dark room effect */
} zelda3_bg2_effect;

// Legacy aliases for backward compatibility  
typedef zelda3_bg2_effect background2;
#define Off ZELDA3_BG2_OFF
#define Parallax ZELDA3_BG2_PARALLAX
#define Dark ZELDA3_BG2_DARK
#define OnTop ZELDA3_BG2_ON_TOP
#define Translucent ZELDA3_BG2_TRANSLUCENT
#define Addition ZELDA3_BG2_ADDITION
#define Normal ZELDA3_BG2_NORMAL
#define Transparent ZELDA3_BG2_TRANSPARENT
#define DarkRoom ZELDA3_BG2_DARK_ROOM

/**
 * @brief Dungeon door object
 * 
 * Represents a door or passage between rooms.
 */
typedef struct object_door {
  uint16_t id;        /**< Door ID for reference */
  uint8_t x;          /**< X position in room (0-63) */
  uint8_t y;          /**< Y position in room (0-63) */
  uint8_t size;       /**< Door size (width/height) */
  uint8_t type;       /**< Door type (normal, locked, etc.) */
  uint8_t layer;      /**< Layer (0=background, 1=foreground) */
  uint8_t key_type;   /**< Required key type (0=none) */
  bool is_locked;     /**< True if door requires key */
} object_door;

/**
 * @brief Staircase connection
 * 
 * Represents stairs or holes that connect different rooms or levels.
 */
typedef struct staircase {
  uint8_t id;               /**< Staircase ID */
  uint8_t room;             /**< Target room ID (for backward compatibility) */
  const char* label;        /**< Description (for debugging) */
} staircase;

/**
 * @brief Treasure chest
 * 
 * Represents a chest containing an item.
 */
typedef struct chest {
  uint8_t x;            /**< X position in room */
  uint8_t y;            /**< Y position in room */
  uint8_t item;         /**< Item ID (for backward compatibility) */
  bool picker;          /**< Legacy field */
  bool big_chest;       /**< True for large chests */
} chest;

/**
 * @brief Legacy chest data structure
 * 
 * @deprecated Use chest structure instead
 */
typedef struct chest_data {
  uint8_t id;   /**< Chest ID */
  bool size;    /**< True for big chest */
} chest_data;

/**
 * @brief Room transition destination
 * 
 * Defines where the player goes when using stairs, holes, or other transitions.
 */
typedef struct destination {
  uint8_t index;          /**< Entrance index */
  uint8_t target;         /**< Target room ID */
  uint8_t target_layer;   /**< Target layer */
} destination;

/** @} */

/**
 * @brief Complete dungeon room data
 * 
 * Contains all objects, sprites, and properties for a single dungeon room.
 */
typedef struct zelda3_dungeon_room {
  uint16_t id;                    /**< Room ID (0-295) */
  background2 bg2;                /**< Background layer 2 effect (legacy) */
  
  /* Room Contents */
  dungeon_sprite* sprites;        /**< Array of sprites in room */
  int sprite_count;               /**< Number of sprites */
  
  object_door* doors;             /**< Array of doors */
  int door_count;                 /**< Number of doors */
  
  staircase* staircases;          /**< Array of staircases */
  int staircase_count;            /**< Number of staircases */
  
  chest* chests;                  /**< Array of chests */
  int chest_count;                /**< Number of chests */
  
  /* Room Connections */
  destination pits;               /**< Pit fall destination */
  destination stairs[4];          /**< Stair destinations (up to 4) */
  
  /* Room Properties */
  uint8_t floor_type;             /**< Floor graphics type */
  uint8_t wall_type;              /**< Wall graphics type */
  uint8_t palette_id;             /**< Room palette ID */
  uint8_t music_track;            /**< Background music track */
  
  /* Flags */
  bool is_dark;                   /**< True if room requires lamp */
  bool has_water;                 /**< True if room contains water */
  bool blocks_items;              /**< True if room blocks certain items */
} zelda3_dungeon_room;

/** @} */

#ifdef __cplusplus
}
#endif

#endif  // ZELDA_H