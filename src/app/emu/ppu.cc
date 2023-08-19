#include "app/emu/ppu.h"

#include <cstdint>
#include <iostream>
#include <vector>

#include "app/emu/mem.h"

namespace yaze {
namespace app {
namespace emu {

PPU::PPU(Memory& memory) : memory_(memory) {}

void PPU::Init() {
  // Initialize registers
  // ...
}

void PPU::RenderScanline() {
  // Render background layers
  // ...

  // Render sprites
  // ...
}

void PPU::Run(int cycles) {
  // ...
}

uint8_t PPU::ReadRegister(uint16_t address) {
  // ...
}

void PPU::WriteRegister(uint16_t address, uint8_t value) {
  // ...
}

const std::vector<uint8_t>& PPU::GetFrameBuffer() const {
  // ...
}

void PPU::UpdateModeSettings() {
  // ...
}

void PPU::RenderBackground(int layer) {
  // ...
}

void PPU::RenderSprites() {
  // ...
}

uint32_t PPU::GetPaletteColor(uint8_t colorIndex) {
  // ...
}

uint8_t PPU::ReadVRAM(uint16_t address) {
  // ...
}

void PPU::WriteVRAM(uint16_t address, uint8_t value) {
  // ...
}

uint8_t PPU::ReadOAM(uint16_t address) {
  // ...
}

void PPU::WriteOAM(uint16_t address, uint8_t value) {
  // ...
}

}  // namespace emu
}  // namespace app
}  // namespace yaze
