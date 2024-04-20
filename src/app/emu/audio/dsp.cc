#include "app/emu/audio/dsp.h"

#include "app/emu/memory/memory.h"

namespace yaze {
namespace app {
namespace emu {
namespace audio {

void Dsp::Reset() {}

void Dsp::GetSamples(int16_t* sample_data, int samples_per_frame,
                     bool pal_timing) {
  // resample from 534 / 641 samples per frame to wanted value
  float wantedSamples = (pal_timing ? 641.0 : 534.0);
  double adder = wantedSamples / samples_per_frame;
  double location = sample_offset_ - wantedSamples;
  for (int i = 0; i < samples_per_frame; i++) {
    sample_data[i * 2] = sample_buffer_[(((int)location) & 0x3ff) * 2];
    sample_data[i * 2 + 1] = sample_buffer_[(((int)location) & 0x3ff) * 2 + 1];
    location += adder;
  }
}

}  // namespace audio
}  // namespace emu
}  // namespace app
}  // namespace yaze