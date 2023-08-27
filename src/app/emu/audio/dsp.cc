#include "app/emu/audio/dsp.h"

#include "app/emu/memory/memory.h"

namespace yaze {
namespace app {
namespace emu {

void DigitalSignalProcessor::Reset() {}

uint8_t DigitalSignalProcessor::ReadVoiceReg(uint8_t voice, uint8_t reg) const {
  voice %= kNumVoices;
  switch (reg % kNumVoiceRegs) {
    case 0:
      return voices_[voice].vol_left;
    case 1:
      return voices_[voice].vol_right;
    case 2:
      return voices_[voice].pitch_low;
    case 3:
      return voices_[voice].pitch_high;
    case 4:
      return voices_[voice].source_number;
    case 5:
      return voices_[voice].adsr1;
    case 6:
      return voices_[voice].adsr2;
    case 7:
      return voices_[voice].gain;
    case 8:
      return voices_[voice].envx;
    case 9:
      return voices_[voice].outx;
    default:
      return 0;  // This shouldn't happen, but it's good to have a default
                 // case
  }
}

void DigitalSignalProcessor::WriteVoiceReg(uint8_t voice, uint8_t reg, uint8_t value) {
  voice %= kNumVoices;
  switch (reg % kNumVoiceRegs) {
    case 0:
      voices_[voice].vol_left = static_cast<int8_t>(value);
      break;
    case 1:
      voices_[voice].vol_right = static_cast<int8_t>(value);
      break;
    case 2:
      voices_[voice].pitch_low = value;
      break;
    case 3:
      voices_[voice].pitch_high = value;
      break;
    case 4:
      voices_[voice].source_number = value;
      break;
    case 5:
      voices_[voice].adsr1 = value;
      break;
    case 6:
      voices_[voice].adsr2 = value;
      break;
    case 7:
      voices_[voice].gain = value;
      break;
      // Note: envx and outx are read-only, so they don't have cases here
  }
}

// Set the callbacks
void DigitalSignalProcessor::SetSampleFetcher(SampleFetcher fetcher) { sample_fetcher_ = fetcher; }

void DigitalSignalProcessor::SetSamplePusher(SamplePusher pusher) { sample_pusher_ = pusher; }

int16_t DigitalSignalProcessor::DecodeSample(uint8_t voice_num) {
  Voice const& voice = voices_[voice_num];
  uint16_t sample_address = voice.source_number;

  // Use the callback to fetch the sample
  int16_t sample = static_cast<int16_t>(sample_fetcher_(sample_address) << 8);
  return sample;
}

int16_t DigitalSignalProcessor::ProcessSample(uint8_t voice_num, int16_t sample) {
  Voice const& voice = voices_[voice_num];

  // Adjust the pitch (for simplicity, we're just adjusting the sample value)
  sample += voice.pitch_low + (voice.pitch_high << 8);

  // Apply volume (separate for left and right for stereo sound)
  int16_t left_sample = (sample * voice.vol_left) / 255;
  int16_t right_sample = (sample * voice.vol_right) / 255;

  // Combine stereo samples into a single 16-bit value
  return (left_sample + right_sample) / 2;
}

void DigitalSignalProcessor::MixSamples() {
  int16_t mixed_sample = 0;

  for (uint8_t i = 0; i < kNumVoices; i++) {
    int16_t decoded_sample = DecodeSample(i);
    int16_t processed_sample = ProcessSample(i, decoded_sample);
    mixed_sample += processed_sample;
  }

  // Clamp the mixed sample to 16-bit range
  if (mixed_sample > 32767) {
    mixed_sample = 32767;
  } else if (mixed_sample < -32768) {
    mixed_sample = -32768;
  }

  // Use the callback to push the mixed sample
  sample_pusher_(mixed_sample);
}

void DigitalSignalProcessor::UpdateEnvelope(uint8_t voice) {
  uint8_t adsr1 = ReadVoiceReg(voice, 0x05);
  uint8_t adsr2 = ReadVoiceReg(voice, 0x06);
  uint8_t gain = ReadVoiceReg(voice, 0x07);

  uint8_t enableADSR = (adsr1 & 0x80) >> 7;

  if (enableADSR) {
    // Handle ADSR envelope
    Voice& voice_obj = voices_[voice];
    switch (voice_obj.state) {
      case VoiceState::ATTACK:
        // Update amplitude based on attack rate
        voice_obj.current_amplitude += AttackRate(adsr1);
        if (voice_obj.current_amplitude >= ENVELOPE_MAX) {
          voice_obj.current_amplitude = ENVELOPE_MAX;
          voice_obj.state = VoiceState::DECAY;
        }
        break;
      case VoiceState::DECAY:
        // Update amplitude based on decay rate
        voice_obj.current_amplitude -= DecayRate(adsr2);
        if (voice_obj.current_amplitude <= voice_obj.decay_level) {
          voice_obj.current_amplitude = voice_obj.decay_level;
          voice_obj.state = VoiceState::SUSTAIN;
        }
        break;
      case VoiceState::SUSTAIN:
        // Keep amplitude at the calculated decay level
        voice_obj.current_amplitude = voice_obj.decay_level;
        break;
      case VoiceState::RELEASE:
        // Update amplitude based on release rate
        voice_obj.current_amplitude -= ReleaseRate(adsr2);
        if (voice_obj.current_amplitude <= 0) {
          voice_obj.current_amplitude = 0;
          voice_obj.state = VoiceState::OFF;
        }
        break;
      default:
        break;
    }
  } else {
    // Handle Gain envelope
    // Extract mode from the gain byte
    uint8_t mode = (gain & 0xE0) >> 5;
    uint8_t rate = gain & 0x1F;

    Voice& voice_obj = voices_[voice];

    switch (mode) {
      case 0:  // Direct Designation
      case 1:
      case 2:
      case 3:
        voice_obj.current_amplitude =
            rate << 3;  // Multiplying by 8 to scale to 0-255
        break;

      case 6:  // Increase Mode (Linear)
        voice_obj.current_amplitude += gainTimings[0][rate];
        if (voice_obj.current_amplitude > ENVELOPE_MAX) {
          voice_obj.current_amplitude = ENVELOPE_MAX;
        }
        break;

      case 7:  // Increase Mode (Bent Line)
        // Hypothetical behavior: Increase linearly at first, then increase
        // more slowly You'll likely need to adjust this based on your
        // specific requirements
        if (voice_obj.current_amplitude < (ENVELOPE_MAX / 2)) {
          voice_obj.current_amplitude += gainTimings[1][rate];
        } else {
          voice_obj.current_amplitude += gainTimings[1][rate] / 2;
        }
        if (voice_obj.current_amplitude > ENVELOPE_MAX) {
          voice_obj.current_amplitude = ENVELOPE_MAX;
        }
        break;

      case 4:  // Decrease Mode (Linear)
        if (voice_obj.current_amplitude < gainTimings[2][rate]) {
          voice_obj.current_amplitude = 0;
        } else {
          voice_obj.current_amplitude -= gainTimings[2][rate];
        }
        break;

      case 5:  // Decrease Mode (Exponential)
        voice_obj.current_amplitude -=
            (voice_obj.current_amplitude * gainTimings[3][rate]) / ENVELOPE_MAX;
        break;

      default:
        // Default behavior can be handled here if necessary
        break;
    }
  }
}

void DigitalSignalProcessor::update_voice_state(uint8_t voice_num) {
  if (voice_num >= kNumVoices) return;

  Voice& voice = voices_[voice_num];
  switch (voice.state) {
    case VoiceState::OFF:
      // Reset current amplitude
      voice.current_amplitude = 0;
      break;

    case VoiceState::ATTACK:
      // Increase the current amplitude at a rate defined by the ATTACK
      // setting
      voice.current_amplitude += AttackRate(voice.adsr1);
      if (voice.current_amplitude >= ENVELOPE_MAX) {
        voice.current_amplitude = ENVELOPE_MAX;
        voice.state = VoiceState::DECAY;
        voice.decay_level = CalculateDecayLevel(voice.adsr2);
      }
      break;

    case VoiceState::DECAY:
      // Decrease the current amplitude at a rate defined by the DECAY setting
      voice.current_amplitude -= DecayRate(voice.adsr2);
      if (voice.current_amplitude <= voice.decay_level) {
        voice.current_amplitude = voice.decay_level;
        voice.state = VoiceState::SUSTAIN;
      }
      break;

    case VoiceState::SUSTAIN:
      // Keep the current amplitude at the decay level
      break;

    case VoiceState::RELEASE:
      // Decrease the current amplitude at a rate defined by the RELEASE
      // setting
      voice.current_amplitude -= ReleaseRate(voice.adsr2);
      if (voice.current_amplitude == 0) {
        voice.state = VoiceState::OFF;
      }
      break;
  }
}

void DigitalSignalProcessor::process_envelope(uint8_t voice_num) {
  if (voice_num >= kNumVoices) return;

  Voice& voice = voices_[voice_num];

  // Update the voice state first (based on keys, etc.)
  update_voice_state(voice_num);

  // Calculate the envelope value based on the current amplitude
  voice.envx = calculate_envelope_value(voice.current_amplitude);

  // Apply the envelope value to the audio output
  apply_envelope_to_output(voice_num);
}

}  // namespace emu
}  // namespace app
}  // namespace yaze