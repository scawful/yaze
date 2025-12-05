#ifndef YAZE_APP_EMU_AUDIO_S_DSP_H
#define YAZE_APP_EMU_AUDIO_S_DSP_H

#include <cstdint>
#include <vector>

namespace yaze {
namespace emu {

enum class InterpolationType {
  Linear,
  Hermite,  // Used by bsnes/Snes9x - better quality than linear
  Gaussian, // SNES hardware accurate
  Cosine,
  Cubic,
};

typedef struct DspChannel {
  // pitch
  uint16_t pitch;
  uint16_t pitchCounter;
  bool pitchModulation;
  // brr decoding
  int16_t decodeBuffer[12];
  uint8_t bufferOffset;
  uint8_t srcn;
  uint16_t decodeOffset;
  uint8_t blockOffset;  // offset within brr block
  uint8_t brrHeader;
  bool useNoise;
  uint8_t startDelay;
  // adsr, envelope, gain
  uint8_t adsrRates[4];  // attack, decay, sustain, gain
  uint8_t adsrState;     // 0: attack, 1: decay, 2: sustain, 3: release
  uint8_t sustainLevel;
  uint8_t gainSustainLevel;
  bool useGain;
  uint8_t gainMode;
  bool directGain;
  uint16_t gainValue;     // for direct gain
  uint16_t preclampGain;  // for bent increase
  uint16_t gain;
  // keyon/off
  bool keyOn;
  bool keyOff;
  // output
  int16_t sampleOut;  // final sample, to be multiplied by channel volume
  int8_t volumeL;
  int8_t volumeR;
  bool echoEnable;
} DspChannel;

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
 * | Name    | Address | Bits      | Notes |
 * |---------|---------|-----------|--------------------------------------------------------|
 * | VOL (L) | $X0     | SVVV VVVV | Left channel volume, signed. | | VOL (R) |
 * $X1     | SVVV VVVV | Right channel volume, signed. | | P (L)   | $X2     |
 * LLLL LLLL | Low 8 bits of sample pitch.                            | | P (H)
 * | $X3     | --HH HHHH | High 6 bits of sample pitch. | | SCRN    | $X4     |
 * SSSS SSSS | Selects a sample source entry from the directory.      | | ADSR
 * (1)| $X5     | EDDD AAAA | ADSR enable (E), decay rate (D), attack rate (A).
 * | | ADSR (2)| $X6     | SSSR RRRR | Sustain level (S), release rate (R). | |
 * GAIN    | $X7     | 0VVV VVVV 1MMV VVVV | Mode (M), value (V). | | ENVX    |
 * $X8     | 0VVV VVVV | Reads current 7-bit value of ADSR/GAIN envelope. | |
 * OUTX    | $X9     | SVVV VVVV | Reads signed 8-bit value of current sample
 * wave        | |         |         |           | multiplied by ENVX, before
 * applying VOL.               |
 */

class Dsp {
 public:
  Dsp(std::vector<uint8_t>& aram) : aram_(aram) {}

  void NewFrame();

  void Reset();
  
  void SaveState(std::ostream& stream);
  void LoadState(std::istream& stream);

  void Cycle();

  void HandleEcho();
  void CycleChannel(int ch);

  void HandleNoise();
  void HandleGain(int ch);

  bool CheckCounter(int rate);

  void DecodeBrr(int ch);

  uint8_t Read(uint8_t adr);
  void Write(uint8_t adr, uint8_t val);

  int16_t GetSample(int ch);

  void GetSamples(int16_t* sample_data, int samples_per_frame, bool pal_timing);
  int CopyNativeFrame(int16_t* sample_data, bool pal_timing);

  void SetChannelMute(int ch, bool mute) {
    if (ch >= 0 && ch < 8) debug_mute_channels_[ch] = mute;
  }
  bool GetChannelMute(int ch) const {
    if (ch >= 0 && ch < 8) return debug_mute_channels_[ch];
    return false;
  }

  // Accessor for visualization
  const DspChannel& GetChannel(int ch) const {
    // Safety clamp
    if (ch < 0) ch = 0;
    if (ch > 7) ch = 7;
    return channel[ch];
  }

  // Accessor for master buffer (for oscilloscope)
  const int16_t* GetSampleBuffer() const { return sampleBuffer; }
  uint16_t GetSampleOffset() const { return sampleOffset; }

  // Reset sample buffer state for clean playback start
  // Clears the ring buffer and resets position tracking
  void ResetSampleBuffer();

  // Debug accessors for diagnostic UI
  uint32_t GetFrameBoundary() const { return lastFrameBoundary; }
  int8_t GetMasterVolumeL() const { return masterVolumeL; }
  int8_t GetMasterVolumeR() const { return masterVolumeR; }
  bool IsMuted() const { return mute; }
  bool IsReset() const { return reset; }
  bool IsEchoEnabled() const { return echoWrites; }
  uint16_t GetEchoDelay() const { return echoDelay; }

  // Default to Gaussian for authentic SNES sound
  InterpolationType interpolation_type = InterpolationType::Gaussian;

 private:
  // sample ring buffer (2048 samples, *2 for stereo)
  // Increased to 2048 to handle 2-frame updates (~1066 samples) without overflow
  int16_t sampleBuffer[0x800 * 2];
  uint16_t sampleOffset;  // current offset in samplebuffer

  bool debug_mute_channels_[8] = {false};

  std::vector<uint8_t>& aram_;

  // mirror ram
  uint8_t ram[0x80];
  // 8 channels
  DspChannel channel[8];
  // overarching
  uint16_t counter;
  uint16_t dirPage;
  bool evenCycle;
  bool mute;
  bool reset;
  int8_t masterVolumeL;
  int8_t masterVolumeR;
  // accumulation
  int16_t sampleOutL;
  int16_t sampleOutR;
  int16_t echoOutL;
  int16_t echoOutR;
  // noise
  int16_t noiseSample;
  uint8_t noiseRate;
  // echo
  bool echoWrites;
  int8_t echoVolumeL;
  int8_t echoVolumeR;
  int8_t feedbackVolume;
  uint16_t echoBufferAdr;
  uint16_t echoDelay;
  uint16_t echoLength;
  uint16_t echoBufferIndex;
  uint8_t firBufferIndex;
  int8_t firValues[8];
  int16_t firBufferL[8];
  int16_t firBufferR[8];
  uint32_t lastFrameBoundary;
};

}  // namespace emu
}  // namespace yaze

#endif  // YAZE_APP_EMU_AUDIO_S_DSP_H
