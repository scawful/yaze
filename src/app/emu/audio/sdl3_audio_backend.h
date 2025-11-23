// sdl3_audio_backend.h - SDL3 Audio Backend Implementation
// Stream-based audio implementation for SDL3

#ifndef YAZE_APP_EMU_AUDIO_SDL3_AUDIO_BACKEND_H
#define YAZE_APP_EMU_AUDIO_SDL3_AUDIO_BACKEND_H

#ifdef YAZE_USE_SDL3

#include <SDL3/SDL.h>
#include <SDL3/SDL_audio.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "app/emu/audio/audio_backend.h"

namespace yaze {
namespace emu {
namespace audio {

/**
 * @brief SDL3 audio backend implementation using SDL_AudioStream API
 *
 * SDL3 introduces a stream-based audio API replacing the queue-based approach.
 * This implementation provides compatibility with the IAudioBackend interface
 * while leveraging SDL3's improved audio pipeline.
 */
class SDL3AudioBackend : public IAudioBackend {
 public:
  SDL3AudioBackend() = default;
  ~SDL3AudioBackend() override;

  // Initialization
  bool Initialize(const AudioConfig& config) override;
  void Shutdown() override;

  // Playback control
  void Play() override;
  void Pause() override;
  void Stop() override;
  void Clear() override;

  // Audio data
  bool QueueSamples(const int16_t* samples, int num_samples) override;
  bool QueueSamples(const float* samples, int num_samples) override;
  bool QueueSamplesNative(const int16_t* samples, int frames_per_channel,
                          int channels, int native_rate) override;

  // Status queries
  AudioStatus GetStatus() const override;
  bool IsInitialized() const override;
  AudioConfig GetConfig() const override;

  // Volume control (0.0 to 1.0)
  void SetVolume(float volume) override;
  float GetVolume() const override;

  // SDL3 supports audio stream resampling natively
  void SetAudioStreamResampling(bool enable, int native_rate,
                                int channels) override;
  bool SupportsAudioStream() const override { return true; }

  // Backend identification
  std::string GetBackendName() const override { return "SDL3"; }

 private:
  // Helper functions
  bool ApplyVolume(int16_t* samples, int num_samples) const;
  bool ApplyVolume(float* samples, int num_samples) const;

  // SDL3 audio stream - primary interface for audio output
  SDL_AudioStream* audio_stream_ = nullptr;

  // Resampling stream for native rate support
  SDL_AudioStream* resampling_stream_ = nullptr;

  // Audio device ID
  SDL_AudioDeviceID device_id_ = 0;

  // Configuration
  AudioConfig config_;

  // State
  std::atomic<bool> initialized_{false};
  std::atomic<float> volume_{1.0f};

  // Resampling configuration
  bool resampling_enabled_ = false;
  int native_rate_ = 0;
  int native_channels_ = 0;

  // Buffer for resampling operations
  std::vector<int16_t> resample_buffer_;

  // Format information
  SDL_AudioFormat device_format_ = SDL_AUDIO_S16;
  int device_channels_ = 2;
  int device_freq_ = 48000;
};

}  // namespace audio
}  // namespace emu
}  // namespace yaze

#endif  // YAZE_USE_SDL3

#endif  // YAZE_APP_EMU_AUDIO_SDL3_AUDIO_BACKEND_H