#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

#include "app/rom.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_map.h"

using namespace yaze::zelda3;
using namespace yaze;

class ROMPatchUtility {
 public:
  static absl::Status CreateV3PatchedROM(const std::string& input_rom_path,
                                         const std::string& output_rom_path) {
    // Load the vanilla ROM
    Rom rom;
    RETURN_IF_ERROR(rom.LoadFromFile(input_rom_path));

    // Apply ZSCustomOverworld v3 settings
    RETURN_IF_ERROR(ApplyV3Patch(rom));

    // Save the patched ROM
    return rom.SaveToFile(Rom::SaveSettings{.filename = output_rom_path});
  }

 private:
  static absl::Status ApplyV3Patch(Rom& rom) {
    // Set ASM version to v3
    rom.WriteByte(OverworldCustomASMHasBeenApplied, 0x03);

    // Enable v3 features
    rom.WriteByte(OverworldCustomAreaSpecificBGEnabled, 0x01);
    rom.WriteByte(OverworldCustomSubscreenOverlayEnabled, 0x01);
    rom.WriteByte(OverworldCustomAnimatedGFXEnabled, 0x01);
    rom.WriteByte(OverworldCustomTileGFXGroupEnabled, 0x01);
    rom.WriteByte(OverworldCustomMosaicEnabled, 0x01);
    rom.WriteByte(OverworldCustomMainPaletteEnabled, 0x01);

    // Apply v3 settings to first 10 maps for testing
    for (int i = 0; i < 10; i++) {
      // Set area sizes (mix of different sizes)
      AreaSizeEnum area_size = static_cast<AreaSizeEnum>(i % 4);
      rom.WriteByte(kOverworldScreenSize + i, static_cast<uint8_t>(area_size));

      // Set main palettes
      rom.WriteByte(OverworldCustomMainPaletteArray + i, i % 8);

      // Set area-specific background colors
      uint16_t bg_color = 0x0000 + (i * 0x1000);
      rom.WriteByte(OverworldCustomAreaSpecificBGPalette + (i * 2),
                    bg_color & 0xFF);
      rom.WriteByte(OverworldCustomAreaSpecificBGPalette + (i * 2) + 1,
                    (bg_color >> 8) & 0xFF);

      // Set subscreen overlays
      uint16_t overlay = 0x0090 + i;
      rom.WriteByte(OverworldCustomSubscreenOverlayArray + (i * 2),
                    overlay & 0xFF);
      rom.WriteByte(OverworldCustomSubscreenOverlayArray + (i * 2) + 1,
                    (overlay >> 8) & 0xFF);

      // Set animated GFX
      rom.WriteByte(OverworldCustomAnimatedGFXArray + i, 0x50 + i);

      // Set custom tile GFX groups (8 bytes per map)
      for (int j = 0; j < 8; j++) {
        rom.WriteByte(OverworldCustomTileGFXGroupArray + (i * 8) + j,
                      0x20 + j + i);
      }

      // Set mosaic settings
      rom.WriteByte(OverworldCustomMosaicArray + i, i % 16);

      // Set expanded message IDs
      uint16_t message_id = 0x1000 + i;
      rom.WriteByte(kOverworldMessagesExpanded + (i * 2), message_id & 0xFF);
      rom.WriteByte(kOverworldMessagesExpanded + (i * 2) + 1,
                    (message_id >> 8) & 0xFF);
    }

    return absl::OkStatus();
  }
};

int main(int argc, char* argv[]) {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " <input_rom> <output_rom>"
              << std::endl;
    return 1;
  }

  std::string input_rom = argv[1];
  std::string output_rom = argv[2];

  auto status = ROMPatchUtility::CreateV3PatchedROM(input_rom, output_rom);
  if (!status.ok()) {
    std::cerr << "Failed to create patched ROM: " << status.message()
              << std::endl;
    return 1;
  }

  std::cout << "Successfully created v3 patched ROM: " << output_rom
            << std::endl;
  return 0;
}
