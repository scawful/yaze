#include "app/emu/apu.h"

#include <cstdint>
#include <iostream>
#include <vector>

#include "app/emu/mem.h"

namespace yaze {
namespace app {
namespace emu {

APU::APU(Memory& memory) : memory_(memory) {}

void APU::Init() {
  // Set the clock frequency
  SetFrequency(kApuClockSpeed);

  // Initialize registers
  // ...
}

void APU::Reset() {
  // Reset the clock
  ResetAccumulatedTime();

  // Reset the SPC700
  // ...
}

void APU::Update() {
  auto cycles_to_run = GetCycleCount();

  for (auto i = 0; i < cycles_to_run; ++i) {
    // Update the APU
    // ...

    // Update the SPC700
    // ...
  }
}

uint8_t APU::ReadRegister(uint16_t address) {
  // ...
}

void APU::WriteRegister(uint16_t address, uint8_t value) {
  // ...
}

const std::vector<int16_t>& APU::GetAudioSamples() const {
  // ...
}

void APU::UpdateChannelSettings() {
  // ...
}

int16_t APU::GenerateSample(int channel) {
  // ...
}

void APU::ApplyEnvelope(int channel) {
  // ...
}

uint8_t APU::ReadDSPMemory(uint16_t address) {
  // ...
}

void APU::WriteDSPMemory(uint16_t address, uint8_t value) {
  // ...
}

}  // namespace emu
}  // namespace app
}  // namespace yaze