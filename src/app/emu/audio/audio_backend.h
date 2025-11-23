// audio_backend.h - Audio Backend Abstraction Layer
// Provides interface for swapping audio implementations (SDL2, SDL3, other
// libs)

#ifndef YAZE_APP_EMU_AUDIO_AUDIO_BACKEND_H
#define YAZE_APP_EMU_AUDIO_AUDIO_BACKEND_H

#include "app/platform/sdl_compat.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace yaze {
namespace emu {
namespace audio {

// Audio sample format
enum class SampleFormat {
  INT16,   // 16-bit signed PCM
  FLOAT32  // 32-bit float
};

// Audio configuration
struct AudioConfig {
  int sample_rate = 48000;
  int channels = 2;  // Stereo
  int buffer_frames = 1024;
  SampleFormat format = SampleFormat::INT16;
};

// Audio backend status
struct AudioStatus {
  bool is_playing = false;
  uint32_t queued_bytes = 0;
  uint32_t queued_frames = 0;
  bool has_underrun = false;
};

/**
 * @brief Abstract audio backend interface
 *
 * Allows swapping between SDL2, SDL3, or custom audio implementations
 * without changing emulator/music editor code.
 */
class IAudioBackend {
 public:
  virtual ~IAudioBackend() = default;

  // Initialization
  virtual bool Initialize(const AudioConfig& config) = 0;
  virtual void Shutdown() = 0;

  // Playback control
  virtual void Play() = 0;
  virtual void Pause() = 0;
  virtual void Stop() = 0;
  virtual void Clear() = 0;

  // Audio data
  virtual bool QueueSamples(const int16_t* samples, int num_samples) = 0;
  virtual bool QueueSamples(const float* samples, int num_samples) = 0;
  virtual bool QueueSamplesNative(const int16_t* samples,
                                  int frames_per_channel, int channels,
                                  int native_rate) {
    return false;
  }

  // Status queries
  virtual AudioStatus GetStatus() const = 0;
  virtual bool IsInitialized() const = 0;
  virtual AudioConfig GetConfig() const = 0;

  // Volume control (0.0 to 1.0)
  virtual void SetVolume(float volume) = 0;
  virtual float GetVolume() const = 0;

  // Optional: enable/disable SDL_AudioStream-based resampling
  virtual void SetAudioStreamResampling(bool enable, int native_rate,
                                        int channels) {}
  virtual bool SupportsAudioStream() const { return false; }

  // Get backend name for debugging
  virtual std::string GetBackendName() const = 0;
};

/**
 * @brief SDL2 audio backend implementation
 */
class SDL2AudioBackend : public IAudioBackend {
 public:
  SDL2AudioBackend() = default;
  ~SDL2AudioBackend() override;

  bool Initialize(const AudioConfig& config) override;
  void Shutdown() override;

  void Play() override;
  void Pause() override;
  void Stop() override;
  void Clear() override;

  bool QueueSamples(const int16_t* samples, int num_samples) override;
  bool QueueSamples(const float* samples, int num_samples) override;
  bool QueueSamplesNative(const int16_t* samples, int frames_per_channel,
                          int channels, int native_rate) override;

  AudioStatus GetStatus() const override;
  bool IsInitialized() const override;
  AudioConfig GetConfig() const override;

  void SetVolume(float volume) override;
  float GetVolume() const override;

  void SetAudioStreamResampling(bool enable, int native_rate,
                                int channels) override;
  bool SupportsAudioStream() const override { return true; }

  std::string GetBackendName() const override { return "SDL2"; }

 private:
  uint32_t device_id_ = 0;
  AudioConfig config_;
  bool initialized_ = false;
  float volume_ = 1.0f;
  SDL_AudioFormat device_format_ = AUDIO_S16;
  int device_channels_ = 2;
  int device_freq_ = 48000;
  bool audio_stream_enabled_ = false;
  int stream_native_rate_ = 0;
  SDL_AudioStream* audio_stream_ = nullptr;
  std::vector<int16_t> stream_buffer_;
};

/**
 * @brief Factory for creating audio backends
 */
class AudioBackendFactory {
 public:
  enum class BackendType {
    SDL2,
    SDL3,         // Future
    NULL_BACKEND  // For testing/headless
  };

  static std::unique_ptr<IAudioBackend> Create(BackendType type);
};

}  // namespace audio
}  // namespace emu
}  // namespace yaze

#endif  // YAZE_APP_EMU_AUDIO_AUDIO_BACKEND_H
