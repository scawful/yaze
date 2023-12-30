#ifndef YAZE_APP_EMU_AUDIO_S_DSP_H
#define YAZE_APP_EMU_AUDIO_S_DSP_H

#include <cstdint>
#include <functional>
#include <vector>

#include "app/emu/memory/memory.h"

namespace yaze {
namespace app {
namespace emu {

using SampleFetcher = std::function<uint8_t(uint16_t)>;
using SamplePusher = std::function<void(int16_t)>;

/**
 *
 * The S-DSP is a digital signal processor generating the sound data.
 *
 * A DSP register can be selected with $F2, after which it can be read or
 * written at $F3. Often it is useful to load the register address into A, and
 * the value to send in Y, so that MOV $F2, YA can be used to do both in one
 * 16-bit instruction.
 *
 * The DSP register address space only has 7 bits. The high bit of $F2, if set,
 * will make the selected register read-only via $F3.
 *
 * When initializing the DSP registers for the first time, take care not to
 * accidentally enable echo writeback via FLG, because it will immediately begin
 * overwriting values in RAM.
 *
 * Voices
 * There are 8 voices, numbered 0 to 7.
 * Each voice X has 10 registers in the range $X0-$X9.
 *
 * Name 	  Address 	Bits 	Notes
 * VOL (L) 	$X0 	SVVV VVVV 	Left channel volume, signed.
 * VOL (R) 	$X1 	SVVV VVVV 	Right channel volume, signed.
 * P (L) 	  $X2 	LLLL LLLL 	Low 8 bits of sample pitch.
 * P (H) 	  $X3 	--HH HHHH 	High 6 bits of sample pitch.
 * SCRN 	  $X4 	SSSS SSSS 	Selects a sample source entry from the
 * directory ADSR (1) $X5 	EDDD AAAA 	ADSR enable (E), decay rate (D),
 * attack rate (A).
 * ADSR (2) $X6 	SSSR RRRR 	Sustain level (S), release rate (R).
 * GAIN 	  $X7 	0VVV VVVV   1MMV VVVV 	Mode (M), value (V).
 * ENVX 	  $X8 	0VVV VVVV 	Reads current 7-bit value of ADSR/GAIN
 * envelope.
 * OUTX 	  $X9 	SVVV VVVV 	Reads signed 8-bit value of current
 * sample wave multiplied by ENVX, before applying VOL.
 *
 */

class DigitalSignalProcessor {
 private:
  static const size_t kNumVoices = 8;
  static const size_t kNumVoiceRegs = 10;
  static const size_t kNumGlobalRegs = 15;

  enum class VoiceState { OFF, ATTACK, DECAY, SUSTAIN, RELEASE };

  struct Voice {
    int8_t vol_left;        // x0
    int8_t vol_right;       // x1
    uint8_t pitch_low;      // x2
    uint8_t pitch_high;     // x3
    uint8_t source_number;  // x4
    uint8_t adsr1;          // x5
    uint8_t adsr2;          // x6
    uint8_t gain;           // x7
    uint8_t envx;           // x8 (read-only)
    int8_t outx;            // x9 (read-only)

    VoiceState state = VoiceState::OFF;
    uint16_t current_amplitude = 0;  // Current amplitude value used for ADSR
    uint16_t decay_level;  // Calculated decay level based on ADSR settings
  };
  Voice voices_[8];

  // Global DSP registers
  uint8_t mvol_left;   // 0C
  uint8_t mvol_right;  // 0D
  uint8_t evol_left;   // 0E
  uint8_t evol_right;  // 0F
  uint8_t kon;         // 10
  uint8_t koff;        // 11
  uint8_t flags;       // 12
  uint8_t endx;        // 13 (read-only)

  // Global registers
  std::vector<uint8_t> globalRegs = std::vector<uint8_t>(kNumGlobalRegs, 0x00);

  static const uint16_t ENVELOPE_MAX = 2047;  // $7FF

  // Attack times in ms
  const std::vector<uint32_t> attackTimes = {
      4100, 2600, 1500, 1000, 640, 380, 260, 160, 96, 64, 40, 24, 16, 10, 6, 0};

  // Decay times in ms
  const std::vector<uint32_t> decayTimes = {1200, 740, 440, 290,
                                            180,  110, 74,  37};

  // Release times in ms
  const std::vector<uint32_t> releaseTimes = {
      // "Infinite" is represented by a large value, e.g., UINT32_MAX
      UINT32_MAX, 38000, 28000, 24000, 19000, 14000, 12000, 9400,
      7100,       5900,  4700,  3500,  2900,  2400,  1800,  1500,
      1200,       880,   740,   590,   440,   370,   290,   220,
      180,        150,   110,   92,    74,    55,    37,    18};

  // Gain timings for decrease linear, decrease exponential, etc.
  // Organized by mode: [Linear Increase, Bentline Increase, Linear Decrease,
  // Exponential Decrease]
  const std::vector<std::vector<uint32_t>> gainTimings = {
      {UINT32_MAX, 3100, 2600, 2000, 1500, 1300, 1000, 770, 640, 510, 380,
       320,        260,  190,  160,  130,  96,   80,   64,  48,  40,  32,
       24,         20,   16,   12,   10,   8,    6,    4,   2},
      {UINT32_MAX, 5400, 4600, 3500, 2600, 2300, 1800, 1300, 1100, 900,
       670,        560,  450,  340,  280,  220,  170,  140,  110,  84,
       70,         56,   42,   35,   28,   21,   18,   14,   11,   7,
       /*3.5=*/3},
      // Repeating the Linear Increase timings for Linear Decrease, since they
      // are the same.
      {UINT32_MAX, 3100, 2600, 2000, 1500, 1300, 1000, 770, 640, 510, 380,
       320,        260,  190,  160,  130,  96,   80,   64,  48,  40,  32,
       24,         20,   16,   12,   10,   8,    6,    4,   2},
      {UINT32_MAX, 38000, 28000, 24000, 19000, 14000, 12000, 9400,
       7100,       5900,  4700,  3500,  2900,  2400,  1800,  1500,
       1200,       880,   740,   590,   440,   370,   290,   220,
       180,        150,   110,   92,    55,    37,    18}};

  // DSP Period Table
  const std::vector<std::vector<uint16_t>> DigitalSignalProcessorPeriodTable = {
      // ... Your DSP period table here ...
  };

  // DSP Period Offset
  const std::vector<uint16_t> DigitalSignalProcessorPeriodOffset = {
      // ... Your DSP period offsets here ...
  };

  uint8_t calculate_envelope_value(uint16_t amplitude) const {
    // Convert the 16-bit amplitude to an 8-bit envelope value
    return amplitude >> 8;
  }

  void apply_envelope_to_output(uint8_t voice_num) {
    Voice& voice = voices_[voice_num];

    // Scale the OUTX by the envelope value
    // This might be a linear scaling, or more complex operations can be used
    voice.outx = (voice.outx * voice.envx) / 255;
  }

  SampleFetcher sample_fetcher_;
  SamplePusher sample_pusher_;

 public:
  DigitalSignalProcessor() = default;

  void Reset();

  void SetSampleFetcher(std::function<uint8_t(uint16_t)> fetcher);
  void SetSamplePusher(std::function<void(int16_t)> pusher);

  // Read a byte from a voice register
  uint8_t ReadVoiceReg(uint8_t voice, uint8_t reg) const;

  // Write a byte to a voice register
  void WriteVoiceReg(uint8_t voice, uint8_t reg, uint8_t value);

  // Read a byte from a global register
  uint8_t ReadGlobalReg(uint8_t reg) const {
    return globalRegs[reg % kNumGlobalRegs];
  }

  // Write a byte to a global register
  void WriteGlobalReg(uint8_t reg, uint8_t value) {
    globalRegs[reg % kNumGlobalRegs] = value;
  }

  int16_t DecodeSample(uint8_t voice_num);
  int16_t ProcessSample(uint8_t voice_num, int16_t sample);
  void MixSamples();

  // Trigger a voice to start playing
  void trigger_voice(uint8_t voice_num) {
    if (voice_num >= kNumVoices) return;

    Voice& voice = voices_[voice_num];
    voice.state = VoiceState::ATTACK;
    // Initialize other state management variables if needed
  }

  // Release a voice (e.g., note release in ADSR)
  void release_voice(uint8_t voice_num) {
    if (voice_num >= kNumVoices) return;

    Voice& voice = voices_[voice_num];
    if (voice.state != VoiceState::OFF) {
      voice.state = VoiceState::RELEASE;
    }
    // Update other state management variables if needed
  }

  // Calculate envelope for a given voice
  void UpdateEnvelope(uint8_t voice);

  // Voice-related functions (implementations)
  void set_voice_volume(int voice_num, int8_t left, int8_t right) {
    voices_[voice_num].vol_left = left;
    voices_[voice_num].vol_right = right;
  }

  void set_voice_pitch(int voice_num, uint16_t pitch) {
    voices_[voice_num].pitch_low = pitch & 0xFF;
    voices_[voice_num].pitch_high = (pitch >> 8) & 0xFF;
  }

  void set_voice_source_number(int voice_num, uint8_t srcn) {
    voices_[voice_num].source_number = srcn;
  }

  void set_voice_adsr(int voice_num, uint8_t adsr1, uint8_t adsr2) {
    voices_[voice_num].adsr1 = adsr1;
    voices_[voice_num].adsr2 = adsr2;
  }

  void set_voice_gain(int voice_num, uint8_t gain) {
    voices_[voice_num].gain = gain;
  }

  uint8_t read_voice_envx(int voice_num) { return voices_[voice_num].envx; }

  int8_t read_voice_outx(int voice_num) { return voices_[voice_num].outx; }

  // Global DSP functions
  void set_master_volume(int8_t left, int8_t right) {
    mvol_left = left;
    mvol_right = right;
  }

  void set_echo_volume(int8_t left, int8_t right) {
    evol_left = left;
    evol_right = right;
  }

  void update_voice_state(uint8_t voice_num);

  // Override the key_on and key_off methods to utilize the new state management
  void key_on(uint8_t value) {
    for (uint8_t i = 0; i < kNumVoices; i++) {
      if (value & (1 << i)) {
        trigger_voice(i);
      }
    }
  }

  void key_off(uint8_t value) {
    for (uint8_t i = 0; i < kNumVoices; i++) {
      if (value & (1 << i)) {
        release_voice(i);
      }
    }
  }

  void set_flags(uint8_t value) {
    flags = value;
    // More logic may be needed here depending on flag behaviors
  }

  uint8_t read_endx() { return endx; }

  uint16_t AttackRate(uint8_t adsr1) {
    // Convert the ATTACK portion of adsr1 into a rate of amplitude change
    // You might need to adjust this logic based on the exact ADSR
    // implementation details
    return (adsr1 & 0x0F) * 16;  // Just a hypothetical conversion
  }

  uint16_t DecayRate(uint8_t adsr2) {
    // Convert the DECAY portion of adsr2 into a rate of amplitude change
    return ((adsr2 >> 4) & 0x07) * 8;  // Hypothetical conversion
  }

  uint16_t ReleaseRate(uint8_t adsr2) {
    // Convert the RELEASE portion of adsr2 into a rate of amplitude change
    return (adsr2 & 0x0F) * 16;  // Hypothetical conversion
  }

  uint16_t CalculateDecayLevel(uint8_t adsr2) {
    // Calculate the decay level based on the SUSTAIN portion of adsr2
    // This is the level the amplitude will decay to before entering the SUSTAIN
    // phase Again, adjust based on your implementation details
    return ((adsr2 >> 4) & 0x07) * 256;  // Hypothetical conversion
  }

  // Envelope processing for all voices
  // Goes through each voice and processes its envelope.
  void process_envelopes() {
    for (size_t i = 0; i < kNumVoices; ++i) {
      process_envelope(i);
    }
  }

  // Envelope processing for a specific voice
  // For a given voice, update its state (ADSR), calculate the envelope value,
  // and apply the envelope to the audio output.
  void process_envelope(uint8_t voice_num);
};
}  // namespace emu
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EMU_AUDIO_S_DSP_H