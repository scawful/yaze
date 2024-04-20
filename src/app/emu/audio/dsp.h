#ifndef YAZE_APP_EMU_AUDIO_S_DSP_H
#define YAZE_APP_EMU_AUDIO_S_DSP_H

#include <cstdint>
#include <functional>
#include <vector>

#include "app/emu/memory/memory.h"

namespace yaze {
namespace app {
namespace emu {
namespace audio {

/**
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
 * | Name    | Address | Bits      | Notes                                                  |
 * |---------|---------|-----------|--------------------------------------------------------|
 * | VOL (L) | $X0     | SVVV VVVV | Left channel volume, signed.                           |
 * | VOL (R) | $X1     | SVVV VVVV | Right channel volume, signed.                          |
 * | P (L)   | $X2     | LLLL LLLL | Low 8 bits of sample pitch.                            |
 * | P (H)   | $X3     | --HH HHHH | High 6 bits of sample pitch.                           |
 * | SCRN    | $X4     | SSSS SSSS | Selects a sample source entry from the directory.      |
 * | ADSR (1)| $X5     | EDDD AAAA | ADSR enable (E), decay rate (D), attack rate (A).      |
 * | ADSR (2)| $X6     | SSSR RRRR | Sustain level (S), release rate (R).                   |
 * | GAIN    | $X7     | 0VVV VVVV 1MMV VVVV | Mode (M), value (V).                         |
 * | ENVX    | $X8     | 0VVV VVVV | Reads current 7-bit value of ADSR/GAIN envelope.       |
 * | OUTX    | $X9     | SVVV VVVV | Reads signed 8-bit value of current sample wave        |
 * |         |         |           | multiplied by ENVX, before applying VOL.               |
 */

class Dsp {
 public:
  void Reset();

  void GetSamples(int16_t* sample_data, int samples_per_frame, bool pal_timing);

 private:
  int16_t sample_buffer_[0x400 * 2];  // (1024 samples, *2 for stereo)
  int16_t sample_offset_;             // current offset in samplebuffer

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
};

}  // namespace audio
}  // namespace emu
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EMU_AUDIO_S_DSP_H