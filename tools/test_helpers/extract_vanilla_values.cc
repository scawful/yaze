#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>

#include "rom/rom.h"
#include "zelda3/overworld/overworld_map.h"
#include "zelda3/overworld/overworld.h"

using namespace yaze::zelda3;
using namespace yaze;

int main() {
  // Load the vanilla ROM
  Rom rom;
  if (!rom.LoadFromFile("zelda3.sfc").ok()) {
    std::cerr << "Failed to load ROM file" << std::endl;
    return 1;
  }

  std::cout << "// Vanilla ROM values extracted from zelda3.sfc" << std::endl;
  std::cout << "// Generated on " << __DATE__ << " " << __TIME__ << std::endl;
  std::cout << std::endl;

  // Extract ASM version
  uint8_t asm_version = rom[OverworldCustomASMHasBeenApplied];
  std::cout << "constexpr uint8_t kVanillaASMVersion = 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)asm_version << ";" << std::endl;
  std::cout << std::endl;

  // Extract area graphics for first 10 maps
  std::cout << "// Area graphics for first 10 maps" << std::endl;
  for (int i = 0; i < 10; i++) {
    uint8_t area_gfx = rom[kAreaGfxIdPtr + i];
    std::cout << "constexpr uint8_t kVanillaAreaGraphics" << i << " = 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)area_gfx << ";" << std::endl;
  }
  std::cout << std::endl;

  // Extract area palettes for first 10 maps
  std::cout << "// Area palettes for first 10 maps" << std::endl;
  for (int i = 0; i < 10; i++) {
    uint8_t area_pal = rom[kOverworldMapPaletteIds + i];
    std::cout << "constexpr uint8_t kVanillaAreaPalette" << i << " = 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)area_pal << ";" << std::endl;
  }
  std::cout << std::endl;

  // Extract message IDs for first 10 maps
  std::cout << "// Message IDs for first 10 maps" << std::endl;
  for (int i = 0; i < 10; i++) {
    uint16_t message_id = rom[kOverworldMessageIds + (i * 2)] | (rom[kOverworldMessageIds + (i * 2) + 1] << 8);
    std::cout << "constexpr uint16_t kVanillaMessageId" << i << " = 0x" << std::hex << std::setw(4) << std::setfill('0') << message_id << ";" << std::endl;
  }
  std::cout << std::endl;

  // Extract screen sizes for first 10 maps
  std::cout << "// Screen sizes for first 10 maps" << std::endl;
  for (int i = 0; i < 10; i++) {
    uint8_t screen_size = rom[kOverworldScreenSize + i];
    std::cout << "constexpr uint8_t kVanillaScreenSize" << i << " = 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)screen_size << ";" << std::endl;
  }
  std::cout << std::endl;

  // Extract sprite sets for first 10 maps
  std::cout << "// Sprite sets for first 10 maps" << std::endl;
  for (int i = 0; i < 10; i++) {
    uint8_t sprite_set = rom[kOverworldSpriteset + i];
    std::cout << "constexpr uint8_t kVanillaSpriteSet" << i << " = 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)sprite_set << ";" << std::endl;
  }
  std::cout << std::endl;

  // Extract sprite palettes for first 10 maps
  std::cout << "// Sprite palettes for first 10 maps" << std::endl;
  for (int i = 0; i < 10; i++) {
    uint8_t sprite_pal = rom[kOverworldSpritePaletteIds + i];
    std::cout << "constexpr uint8_t kVanillaSpritePalette" << i << " = 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)sprite_pal << ";" << std::endl;
  }
  std::cout << std::endl;

  // Extract music for first 10 maps
  std::cout << "// Music for first 10 maps" << std::endl;
  for (int i = 0; i < 10; i++) {
    uint8_t music = rom[kOverworldMusicBeginning + i];
    std::cout << "constexpr uint8_t kVanillaMusic" << i << " = 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)music << ";" << std::endl;
  }
  std::cout << std::endl;

  // Extract some special world values
  std::cout << "// Special world graphics and palettes" << std::endl;
  for (int i = 0; i < 5; i++) {
    uint8_t special_gfx = rom[kOverworldSpecialGfxGroup + i];
    uint8_t special_pal = rom[kOverworldSpecialPalGroup + i];
    std::cout << "constexpr uint8_t kVanillaSpecialGfx" << i << " = 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)special_gfx << ";" << std::endl;
    std::cout << "constexpr uint8_t kVanillaSpecialPal" << i << " = 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)special_pal << ";" << std::endl;
  }

  return 0;
}
