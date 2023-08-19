#ifndef YAZE_APP_EMU_PPU_H
#define YAZE_APP_EMU_PPU_H

#include <cstdint>
#include <vector>

#include "app/emu/mem.h"

namespace yaze {
namespace app {
namespace emu {

class PPU {
 public:
  // Initializes the PPU with the necessary resources and dependencies
  PPU(Memory &memory);

  void Init();

  // Resets the PPU to its initial state
  void Reset();

  // Runs the PPU for a specified number of clock cycles
  void Run(int cycles);

  // Reads a byte from the specified PPU register
  uint8_t ReadRegister(uint16_t address);

  // Writes a byte to the specified PPU register
  void WriteRegister(uint16_t address, uint8_t value);

  // Renders a scanline of the screen
  void RenderScanline();

  // Returns the pixel data for the current frame
  const std::vector<uint8_t> &GetFrameBuffer() const;

 private:
  // Internal methods to handle PPU rendering and operations

  // Updates internal state based on PPU register settings
  void UpdateModeSettings();

  // Renders a background layer
  void RenderBackground(int layer);

  // Renders sprites (also known as objects)
  void RenderSprites();

  // Retrieves a pixel color from the palette
  uint32_t GetPaletteColor(uint8_t colorIndex);

  // Handles VRAM (Video RAM) reads and writes
  uint8_t ReadVRAM(uint16_t address);
  void WriteVRAM(uint16_t address, uint8_t value);

  // Handles OAM (Object Attribute Memory) reads and writes
  uint8_t ReadOAM(uint16_t address);
  void WriteOAM(uint16_t address, uint8_t value);

  // Other internal methods for handling PPU functionality

  // Member variables to store internal PPU state and resources
  Memory &memory_;
  std::vector<uint8_t> frameBuffer_;
  // Other state variables (registers, counters, mode settings, etc.)
};

}  // namespace emu
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EMU_PPU_H