#include "app/emu/audio/apu.h"

#include <SDL.h>

#include <cstdint>
#include <functional>
#include <iostream>
#include <vector>

#include "app/emu/audio/dsp.h"
#include "app/emu/audio/spc700.h"
#include "app/emu/cpu/clock.h"
#include "app/emu/memory/memory.h"

namespace yaze {
namespace app {
namespace emu {
namespace audio {

void Apu::Init() {
  // Set the clock frequency
  clock_.SetFrequency(kApuClockSpeed);
}

void Apu::Reset() {
  clock_.ResetAccumulatedTime();
  spc700_.Reset();
  dsp_.Reset();
}

void Apu::Update() {
  auto cycles_to_run = clock_.GetCycleCount();

  for (auto i = 0; i < cycles_to_run; ++i) {
    // Update the SPC700
    uint8_t opcode = spc700_.read(spc700_.PC);
    spc700_.ExecuteInstructions(opcode);
    spc700_.PC++;
  }
}

void Apu::Notify(uint32_t address, uint16_t data) {
  if (address < 0x2140 || address > 0x2143) {
    return;
  }
  auto offset = address - 0x2140;
  spc700_.write(offset, data);

  // HACK - This is a temporary solution to get the Apu to play audio
  ports_[address - 0x2140] = data;
  switch (address) {
    case 0x2140:
      SignalReady();
      break;
    case 0x2141:
      // TODO: Handle data byte transfer here
      break;
    case 0x2142:
      // TODO: Handle the setup of destination address
      break;
    case 0x2143:
      // TODO: Handle additional communication/commands
      break;
  }
}

}  // namespace audio
}  // namespace emu
}  // namespace app
}  // namespace yaze