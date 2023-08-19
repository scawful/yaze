#include "snes.h"

#include <cstdint>
#include <string>

#include "app/emu/apu.h"
#include "app/emu/cpu.h"
#include "app/emu/mem.h"
#include "app/emu/ppu.h"

namespace yaze {
namespace app {
namespace emu {

void SNES::Init(ROM& rom) {
  // Initialize CPU
  cpu.Init();

  // Initialize PPU
  ppu.Init();

  // Initialize APU
  apu.Init();

  // Load ROM
  memory_.SetMemory(rom.vector());
}

void SNES::Run() {
  running_ = true;

  const int cpuClockSpeed = 21477272;  // 21.477272 MHz
  const int ppuClockSpeed = 5369318;   // 5.369318 MHz
  const int apuClockSpeed = 32000;     // 32 KHz
  const double targetFPS = 60.0;       // 60 frames per second

  const double cpuCycleTime = 1.0 / cpuClockSpeed;
  const double ppuCycleTime = 1.0 / ppuClockSpeed;
  const double apuCycleTime = 1.0 / apuClockSpeed;
  const double frameTime = 1.0 / targetFPS;

  double cpuAccumulatedTime = 0.0;
  double ppuAccumulatedTime = 0.0;
  double apuAccumulatedTime = 0.0;
  double frameAccumulatedTime = 0.0;

  auto lastTime = std::chrono::high_resolution_clock::now();

  while (running_) {
    auto currentTime = std::chrono::high_resolution_clock::now();
    double deltaTime =
        std::chrono::duration<double>(currentTime - lastTime).count();
    lastTime = currentTime;

    cpuAccumulatedTime += deltaTime;
    ppuAccumulatedTime += deltaTime;
    apuAccumulatedTime += deltaTime;
    frameAccumulatedTime += deltaTime;

    while (cpuAccumulatedTime >= cpuCycleTime) {
      cpu.ExecuteInstruction(cpu.ReadByte(cpu.PC));
      cpuAccumulatedTime -= cpuCycleTime;
    }

    while (ppuAccumulatedTime >= ppuCycleTime) {
      RenderScanline();
      ppuAccumulatedTime -= ppuCycleTime;
    }

    while (apuAccumulatedTime >= apuCycleTime) {
      // apu.Update();
      apuAccumulatedTime -= apuCycleTime;
    }

    if (frameAccumulatedTime >= frameTime) {
      // renderer.Render();
      frameAccumulatedTime -= frameTime;
    }

    HandleInput();
  }
}

void SNES::RenderScanline() {
  // Render background layers
  for (int layer = 0; layer < 4; layer++) {
    DrawBackgroundLayer(layer);
  }

  // Render sprites
  DrawSprites();
}

void SNES::DrawBackgroundLayer(int layer) {
  // ...
}

void SNES::DrawSprites() {
  // ...
}

void SNES::HandleInput() {
  // ...
}

void SNES::SaveState(const std::string& path) {
  // ...
}

void SNES::LoadState(const std::string& path) {
  // ...
}

void SNES::Debug() {
  // ...
}

void SNES::Breakpoint(uint16_t address) {
  // ...
}

}  // namespace emu
}  // namespace app
}  // namespace yaze