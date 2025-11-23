// wasm_audio.h - WebAudio Backend Implementation for WASM/Emscripten
// Provides audio output for SNES emulator using browser's WebAudio API

#ifndef YAZE_APP_EMU_PLATFORM_WASM_WASM_AUDIO_H
#define YAZE_APP_EMU_PLATFORM_WASM_WASM_AUDIO_H

#ifdef __EMSCRIPTEN__

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

#include "app/emu/audio/audio_backend.h"

namespace yaze {
namespace emu {
namespace audio {

/**
 * @brief WebAudio backend implementation for WASM/browser environments
 *
 * This backend uses the browser's WebAudio API to play SNES audio samples.
 * It handles:
 * - AudioContext creation and management
 * - Sample buffering and queueing
 * - Browser autoplay policy compliance
 * - Volume control
 * - Format conversion (16-bit PCM to Float32 for WebAudio)
 *
 * The SNES outputs 16-bit stereo audio at approximately 32kHz, which
 * this backend resamples to the browser's native sample rate if needed.
 */
class WasmAudioBackend : public IAudioBackend {
 public:
  WasmAudioBackend();
  ~WasmAudioBackend() override;

  // Initialization
  bool Initialize(const AudioConfig& config) override;
  void Shutdown() override;

  // Playback control
  void Play() override;
  void Pause() override;
  void Stop() override;
  void Clear() override;

  // Audio data submission
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

  // Audio stream resampling support
  void SetAudioStreamResampling(bool enable, int native_rate,
                                int channels) override;
  bool SupportsAudioStream() const override { return true; }

  // Backend identification
  std::string GetBackendName() const override { return "WebAudio"; }

  // WASM-specific: Handle user interaction for autoplay policy
  void HandleUserInteraction();

  // WASM-specific: Check if audio context is suspended due to autoplay
  bool IsContextSuspended() const;

 private:
  // Internal buffer management
  struct AudioBuffer {
    std::vector<float> samples;
    int sample_count;
    int channels;
  };

  // Helper functions
  void ProcessAudioQueue();
  bool ConvertToFloat32(const int16_t* input, float* output, int num_samples);
  void ApplyVolumeToBuffer(float* buffer, int num_samples);

  // JavaScript audio context handle (opaque pointer)
  void* audio_context_ = nullptr;

  // JavaScript script processor node handle
  void* script_processor_ = nullptr;

  // Configuration
  AudioConfig config_;

  // State management
  std::atomic<bool> initialized_{false};
  std::atomic<bool> playing_{false};
  std::atomic<bool> context_suspended_{true};
  std::atomic<float> volume_{1.0f};

  // Sample queue for buffering
  std::mutex queue_mutex_;
  std::queue<AudioBuffer> sample_queue_;
  std::atomic<size_t> queued_samples_{0};

  // Resampling configuration
  bool resampling_enabled_ = false;
  int native_rate_ = 32000;  // SNES native rate
  int native_channels_ = 2;

  // Working buffers
  std::vector<float> conversion_buffer_;
  std::vector<float> resampling_buffer_;

  // Performance metrics
  mutable std::atomic<bool> has_underrun_{false};
  std::atomic<size_t> total_samples_played_{0};

  // Buffer size configuration
  static constexpr int kDefaultBufferSize = 2048;
  static constexpr int kMaxQueuedBuffers = 16;
  static constexpr float kInt16ToFloat = 1.0f / 32768.0f;
};

}  // namespace audio
}  // namespace emu
}  // namespace yaze

#endif  // __EMSCRIPTEN__

#endif  // YAZE_APP_EMU_PLATFORM_WASM_WASM_AUDIO_H