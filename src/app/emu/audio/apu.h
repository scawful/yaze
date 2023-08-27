#ifndef YAZE_APP_EMU_APU_H_
#define YAZE_APP_EMU_APU_H_

#include <cstdint>
#include <iostream>
#include <vector>

#include "app/emu/audio/dsp.h"
#include "app/emu/audio/spc700.h"
#include "app/emu/clock.h"
#include "app/emu/memory/memory.h"

namespace yaze {
namespace app {
namespace emu {

/**
 *
 * 64 kilobytes of RAM are mapped across the 16-bit memory space of the SPC-700.
 * Some regions of this space are overlaid with special hardware functions.
 *
 * Range 	      Note
 * $0000-00EF 	Zero Page RAM
 * $00F0-00FF 	Sound CPU Registers
 * $0100-01FF 	Stack Page RAM
 * $0200-FFBF 	RAM
 * $FFC0-FFFF 	IPL ROM or RAM
 *
 * The region at $FFC0-FFFF will normally read from the 64-byte IPL ROM, but the
 * underlying RAM can always be written to, and the high bit of the Control
 * register $F1 can be cleared to unmap the IPL ROM and allow read access to
 * this RAM.
 *
 */

const int kApuClockSpeed = 1024000;  // 1.024 MHz
const int apuSampleRate = 32000;     // 32 KHz
const int apuClocksPerSample = 64;   // 64 clocks per sample

class APU : public Observer {
 public:
  // Initializes the APU with the necessary resources and dependencies
  APU(MemoryImpl &memory, AudioRam &aram, Clock &clock)
      : aram_(aram), clock_(clock), memory_(memory) {}

  void Init();

  // Resets the APU to its initial state
  void Reset();

  // Runs the APU for one frame
  void Update();

  void ProcessSamples();

  uint8_t FetchSampleForVoice(uint8_t voice_num);

  uint16_t CalculateAddressForVoice(uint8_t voice_num);

  int16_t GetNextSample();

  void Notify(uint32_t address, uint8_t data) override {
    if (address >= 0x2140 && address <= 0x2143) {
      spc700_.Notify(address, data);
    }
  }

  void UpdateClock(int delta_time) { clock_.UpdateClock(delta_time); }

  // Method to fetch a sample from AudioRam
  uint8_t FetchSampleFromRam(uint16_t address) const {
    return aram_.read(address);
  }

  // Method to push a processed sample to the audio buffer
  void PushToAudioBuffer(int16_t sample) { audioSamples_.push_back(sample); }

  // Reads a byte from the specified APU register
  uint8_t ReadRegister(uint16_t address);

  // Writes a byte to the specified APU register
  void WriteRegister(uint16_t address, uint8_t value);

  // Returns the audio samples for the current frame
  const std::vector<int16_t> &GetAudioSamples() const;

  // Called upon a reset
  void Initialize() {
    spc700_.Reset();
    dsp_.Reset();
    // Set stack pointer, zero-page values, etc. for the SPC700
    SignalReady();
  }

  void SignalReady() {
    // Set Port 0 = $AA and Port 1 = $BB
    ports_[0] = READY_SIGNAL_0;
    ports_[1] = READY_SIGNAL_1;
    memory_.WriteByte(0x2140, READY_SIGNAL_0);
    memory_.WriteByte(0x2141, READY_SIGNAL_1);
  }

  bool IsReadySignalReceived() const {
    return ports_[0] == READY_SIGNAL_0 && ports_[1] == READY_SIGNAL_1;
  }

  void WaitForSignal() const {
    // This might be an active wait or a passive state where APU does nothing
    // until it's externally triggered by the main CPU writing to its ports.
    while (ports_[0] != BEGIN_SIGNAL)
      ;
  }

  uint16_t ReadAddressFromPorts() const {
    // Read 2 byte address from port 2 (low) and 3 (high)
    return static_cast<uint16_t>(ports_[2]) |
           (static_cast<uint16_t>(ports_[3]) << 8);
  }

  void AcknowledgeSignal() {
    // Read value from Port 0 and write it back to Port 0
    ports_[0] = ports_[0];
  }

  void BeginTransfer() {
    uint16_t destAddr = ReadAddressFromPorts();
    uint8_t counter = 0;

    // Port 1 determines whether to execute or transfer
    while (ports_[1] != 0) {
      uint8_t data = ports_[1];
      aram_.write(destAddr, data);
      AcknowledgeSignal();

      destAddr++;
      counter++;

      // Synchronize with the counter from the main CPU
      while (ports_[0] != counter)
        ;
    }
  }

  void ExecuteProgram() {
    // For now, this is a placeholder. Actual execution would involve running
    // the SPC700's instruction at the specified address.
    spc700_.ExecuteInstructions(ReadAddressFromPorts());
  }

  // This method will be called by the main CPU to write to the APU's ports.
  void WriteToPort(uint8_t portNum, uint8_t value) {
    if (portNum < 4) {
      ports_[portNum] = value;
      if (portNum == 0 && value == BEGIN_SIGNAL) {
        BeginTransfer();
      }
    }
  }

 private:
  // Constants for communication
  static const uint8_t READY_SIGNAL_0 = 0xAA;
  static const uint8_t READY_SIGNAL_1 = 0xBB;
  static const uint8_t BEGIN_SIGNAL = 0xCC;

  // Port buffers (equivalent to $2140 to $2143 for the main CPU)
  uint8_t ports_[4] = {0};

  // Updates internal state based on APU register settings
  void UpdateChannelSettings();

  // Generates a sample for an audio channel
  int16_t GenerateSample(int channel);

  // Applies an envelope to an audio channel
  void ApplyEnvelope(int channel);

  // Handles DSP (Digital Signal Processor) memory reads and writes
  uint8_t ReadDSPMemory(uint16_t address);
  void WriteDSPMemory(uint16_t address, uint8_t value);

  // Member variables to store internal APU state and resources
  AudioRam &aram_;
  Clock &clock_;
  MemoryImpl &memory_;

  DigitalSignalProcessor dsp_;
  SPC700 spc700_{aram_};
  std::vector<int16_t> audioSamples_;
};

}  // namespace emu
}  // namespace app
}  // namespace yaze

#endif