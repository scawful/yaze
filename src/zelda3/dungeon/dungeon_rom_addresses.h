#ifndef YAZE_APP_ZELDA3_DUNGEON_ROM_ADDRESSES_H
#define YAZE_APP_ZELDA3_DUNGEON_ROM_ADDRESSES_H

#include <array>  // Added for std::array
#include <cstdint>

namespace yaze {
namespace zelda3 {

// ============================================================================
// Dungeon ROM Address Constants
// ============================================================================
// This file contains all ROM addresses for dungeon-related data in ALTTP.
// Organized by category for readability and maintainability.

// === Room Data Pointers ===
constexpr int kRoomObjectLayoutPointer = 0x882D;  // Layout pointer table
constexpr int kRoomObjectPointer = 0x874C;        // Object data pointer (Long)
constexpr int kRoomHeaderPointer = 0xB5DD;        // Room header pointer (LONG)
constexpr int kRoomHeaderPointerBank = 0xB5E7;    // Room header bank byte

// === Palette Data ===
constexpr int kDungeonsMainBgPalettePointers = 0xDEC4B;  // JP Same
constexpr int kDungeonsPalettes = 0xDD734;               // Dungeon palette data

// === Item & Sprite Data ===
constexpr int kRoomItemsPointers = 0xDB69;      // JP 0xDB67
constexpr int kRoomsSpritePointer = 0x4C298;    // JP Same (2-byte bank 09D62E)
constexpr int kSpriteBlocksetPointer = 0x5B57;  // Sprite graphics pointer

// === Graphics Data ===
constexpr int kGfxGroupsPointer = 0x6237;    // Graphics group table
constexpr int kTileAddress = 0x001B52;       // Main tile graphics
constexpr int kTileAddressFloor = 0x001B5A;  // Floor tile graphics

// === Block Data ===
constexpr int kBlocksLength = 0x8896;     // Word value
constexpr int kBlocksPointer1 = 0x15AFA;  // Block data pointer 1
constexpr int kBlocksPointer2 = 0x15B01;  // Block data pointer 2
constexpr int kBlocksPointer3 = 0x15B08;  // Block data pointer 3
constexpr int kBlocksPointer4 = 0x15B0F;  // Block data pointer 4

// === Chests ===
constexpr int kChestsLengthPointer = 0xEBF6;  // Chest count pointer
constexpr int kChestsDataPointer1 = 0xEBFB;   // Chest data start

// === Torches ===
constexpr int kTorchData = 0x2736A;            // JP 0x2704A
constexpr int kTorchesLengthPointer = 0x88C1;  // Torch count pointer

// === Pits & Warps ===
constexpr int kPitPointer = 0x394AB;  // Pit/hole data
constexpr int kPitCount = 0x394A6;    // Number of pits

// === Doors ===
constexpr int kDoorPointers = 0xF83C0;        // Door data table
constexpr int kDoorGfxUp = 0x4D9E;            // Door graphics (up)
constexpr int kDoorGfxDown = 0x4E06;          // Door graphics (down)
constexpr int kDoorGfxCaveExitDown = 0x4E06;  // Cave exit door
constexpr int kDoorGfxLeft = 0x4E66;          // Door graphics (left)
constexpr int kDoorGfxRight = 0x4EC6;         // Door graphics (right)
constexpr int kDoorPosUp = 0x197E;            // Door position (up)
constexpr int kDoorPosDown = 0x1996;          // Door position (down)
constexpr int kDoorPosLeft = 0x19AE;          // Door position (left)
constexpr int kDoorPosRight = 0x19C6;         // Door position (right)

// === Sprites ===
constexpr int kSpritesData = 0x4D8B0;           // Sprite data start
constexpr int kSpritesDataEmptyRoom = 0x4D8AE;  // Empty room sprite marker
constexpr int kSpritesEndData = 0x4EC9E;        // Sprite data end
constexpr int kDungeonSpritePointers =
    0x090000;  // Dungeon sprite pointer table

// === Messages ===
constexpr int kMessagesIdDungeon = 0x3F61D;  // Dungeon message IDs

// === Custom Collision (ZScream expanded region) ===
constexpr int kCustomCollisionRoomPointers = 0x128090;  // 296 rooms × 3 bytes
constexpr int kCustomCollisionDataPosition = 0x128450;
constexpr int kCustomCollisionDataEnd = 0x130000;

// === Room Metadata ===
constexpr int kNumberOfRooms = 296;  // Total dungeon rooms (0x00-0x127)

// Stair objects (special handling)
constexpr uint16_t kStairsObjects[] = {0x139, 0x138, 0x13B, 0x12E, 0x12D};

// === Layout Pointers (referenced in comments) ===
// Layout00 ptr: 0x47EF04
// Layout01 ptr: 0xAFEF04
// Layout02 ptr: 0xF0EF04
// Layout03 ptr: 0x4CF004
// Layout04 ptr: 0xA8F004
// Layout05 ptr: 0xECF004
// Layout06 ptr: 0x48F104
// Layout07 ptr: 0xA4F104

// === Notes ===
// - Layout arrays are NOT exactly the same as rooms
// - Object array is terminated by 0xFFFF (no layers)
// - In normal room, 0xFFFF goes to next layer (layers 0, 1, 2)

// Static pointers for the 8 predefined room layouts
static const std::array<int, 8> kRoomLayoutPointers = {
    0x04EF47, 0x04EFAF, 0x04EFF0, 0x04F04C,
    0x04F0A8, 0x04F0EC, 0x04F148, 0x04F1A4,
};

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_DUNGEON_ROM_ADDRESSES_H
