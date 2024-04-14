#include "app/emu/audio/apu.h"

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

void Apu::Init() {
  // Set the clock frequency
  clock_.SetFrequency(kApuClockSpeed);

  // Initialize Digital Signal Processor Callbacks
  dsp_.SetSampleFetcher([this](uint16_t address) -> uint8_t {
    return this->FetchSampleFromRam(address);
  });

  dsp_.SetSamplePusher(
      [this](int16_t sample) { this->PushToAudioBuffer(sample); });
}

void Apu::Reset() {
  clock_.ResetAccumulatedTime();
  spc700_.Reset();
  dsp_.Reset();
}

void Apu::Update() {
  auto cycles_to_run = clock_.GetCycleCount();

  for (auto i = 0; i < cycles_to_run; ++i) {
    // Update the Apu
    UpdateChannelSettings();

    // Update the SPC700
    uint8_t opcode = spc700_.read(spc700_.PC);
    spc700_.ExecuteInstructions(opcode);
    spc700_.PC++;
  }

  ProcessSamples();
}

void Apu::Notify(uint32_t address, uint8_t data) {
  if (address < 0x2140 || address > 0x2143) {
    return;
  }
  auto offset = address - 0x2140;
  spc700_.write(offset, data);

  // HACK - This is a temporary solution to get the Apu to play audio
  ports_[address - 0x2140] = data;
  switch (address) {
    case 0x2140:
      if (data == BEGIN_SIGNAL) {
        SignalReady();
      }
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

void Apu::ProcessSamples() {
  // Fetch sample data from AudioRam
  // Iterate over all voices
  for (uint8_t voice_num = 0; voice_num < 8; voice_num++) {
    // Fetch the sample data for the current voice from AudioRam
    uint8_t sample = FetchSampleForVoice(voice_num);

    // Process the sample through DSP
    int16_t processed_sample = dsp_.ProcessSample(voice_num, sample);

    // Add the processed sample to the audio buffer
    audio_samples_.push_back(processed_sample);
  }
}

uint8_t Apu::FetchSampleForVoice(uint8_t voice_num) {
  uint16_t address = CalculateAddressForVoice(voice_num);
  return aram_.read(address);
}

uint16_t Apu::CalculateAddressForVoice(uint8_t voice_num) {
  // TODO: Calculate the address for the specified voice
  return voice_num;
}

int16_t Apu::GetNextSample() {
  if (!audio_samples_.empty()) {
    int16_t sample = audio_samples_.front();
    audio_samples_.erase(audio_samples_.begin());
    return sample;
  }
  return 0;  // TODO: Return the last sample instead of 0.
}

const std::vector<int16_t>& Apu::GetAudioSamples() const {
  return audio_samples_;
}

void Apu::UpdateChannelSettings() {
  // TODO: Implement this method to update the channel settings.
}

int16_t Apu::GenerateSample(int channel) {
  // TODO: Implement this method to generate a sample for the specified channel.
}

void Apu::ApplyEnvelope(int channel) {
  // TODO: Implement this method to apply an envelope to the specified channel.
}

uint8_t Apu::ReadDspMemory(uint16_t address) {
  return dsp_.ReadGlobalReg(address);
}

void Apu::WriteDspMemory(uint16_t address, uint8_t value) {
  dsp_.WriteGlobalReg(address, value);
}

}  // namespace emu
}  // namespace app
}  // namespace yaze