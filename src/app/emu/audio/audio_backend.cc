// audio_backend.cc - Audio Backend Implementation

#include "app/emu/audio/audio_backend.h"

#include <SDL.h>

#include <algorithm>
#include <vector>

#include "util/log.h"

namespace yaze {
namespace emu {
namespace audio {

// ============================================================================
// SDL2AudioBackend Implementation
// ============================================================================

SDL2AudioBackend::~SDL2AudioBackend() { Shutdown(); }

bool SDL2AudioBackend::Initialize(const AudioConfig& config) {
  if (initialized_) {
    LOG_WARN("AudioBackend", "Already initialized, shutting down first");
    Shutdown();
  }

  config_ = config;

  SDL_AudioSpec want, have;
  SDL_memset(&want, 0, sizeof(want));

  want.freq = config.sample_rate;
  want.format = (config.format == SampleFormat::INT16) ? AUDIO_S16 : AUDIO_F32;
  want.channels = config.channels;
  want.samples = config.buffer_frames;
  want.callback = nullptr;  // Use queue-based audio

  device_id_ = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);

  if (device_id_ == 0) {
    LOG_ERROR("AudioBackend", "Failed to open SDL audio device: %s",
              SDL_GetError());
    return false;
  }

  device_format_ = have.format;
  device_channels_ = have.channels;
  device_freq_ = have.freq;

  // Verify we got what we asked for
  if (have.freq != want.freq || have.channels != want.channels) {
    LOG_WARN("AudioBackend",
             "Audio spec mismatch - wanted %dHz %dch, got %dHz %dch", want.freq,
             want.channels, have.freq, have.channels);
    // Update config with actual values
    config_.sample_rate = have.freq;
    config_.channels = have.channels;
  }

  LOG_INFO("AudioBackend",
           "SDL2 audio initialized: %dHz, %d channels, %d samples buffer",
           have.freq, have.channels, have.samples);

  initialized_ = true;
  audio_stream_enabled_ = false;
  stream_native_rate_ = 0;
  if (audio_stream_) {
    SDL_FreeAudioStream(audio_stream_);
    audio_stream_ = nullptr;
  }
  stream_buffer_.clear();

  // Start playback immediately (unpause)
  SDL_PauseAudioDevice(device_id_, 0);

  return true;
}

void SDL2AudioBackend::Shutdown() {
  if (!initialized_) return;

  if (audio_stream_) {
    SDL_FreeAudioStream(audio_stream_);
    audio_stream_ = nullptr;
  }
  audio_stream_enabled_ = false;
  stream_native_rate_ = 0;
  stream_buffer_.clear();

  if (device_id_ != 0) {
    SDL_PauseAudioDevice(device_id_, 1);
    SDL_CloseAudioDevice(device_id_);
    device_id_ = 0;
  }

  initialized_ = false;
  LOG_INFO("AudioBackend", "SDL2 audio shut down");
}

void SDL2AudioBackend::Play() {
  if (!initialized_) return;
  SDL_PauseAudioDevice(device_id_, 0);
}

void SDL2AudioBackend::Pause() {
  if (!initialized_) return;
  SDL_PauseAudioDevice(device_id_, 1);
}

void SDL2AudioBackend::Stop() {
  if (!initialized_) return;
  Clear();
  SDL_PauseAudioDevice(device_id_, 1);
}

void SDL2AudioBackend::Clear() {
  if (!initialized_) return;
  SDL_ClearQueuedAudio(device_id_);
  if (audio_stream_) {
    SDL_AudioStreamClear(audio_stream_);
  }
}

bool SDL2AudioBackend::QueueSamples(const int16_t* samples, int num_samples) {
  if (!initialized_ || !samples) return false;

  // OPTIMIZATION: Skip volume scaling if volume is 100% (common case)
  if (volume_ == 1.0f) {
    // Fast path: No volume adjustment needed
    int result =
        SDL_QueueAudio(device_id_, samples, num_samples * sizeof(int16_t));
    if (result < 0) {
      LOG_ERROR("AudioBackend", "SDL_QueueAudio failed: %s", SDL_GetError());
      return false;
    }
    return true;
  }

  // Slow path: Volume scaling required
  // Use thread-local buffer to avoid repeated allocations
  thread_local std::vector<int16_t> scaled_samples;

  // Resize only if needed (avoid reallocation on every call)
  if (scaled_samples.size() < static_cast<size_t>(num_samples)) {
    scaled_samples.resize(num_samples);
  }

  // Apply volume scaling with SIMD-friendly loop
  for (int i = 0; i < num_samples; ++i) {
    int32_t scaled = static_cast<int32_t>(samples[i] * volume_);
    // Clamp to prevent overflow
    if (scaled > 32767)
      scaled = 32767;
    else if (scaled < -32768)
      scaled = -32768;
    scaled_samples[i] = static_cast<int16_t>(scaled);
  }

  int result = SDL_QueueAudio(device_id_, scaled_samples.data(),
                              num_samples * sizeof(int16_t));
  if (result < 0) {
    LOG_ERROR("AudioBackend", "SDL_QueueAudio failed: %s", SDL_GetError());
    return false;
  }

  return true;
}

bool SDL2AudioBackend::QueueSamples(const float* samples, int num_samples) {
  if (!initialized_ || !samples) return false;

  // Convert float to int16
  std::vector<int16_t> int_samples(num_samples);
  for (int i = 0; i < num_samples; ++i) {
    float scaled = std::clamp(samples[i] * volume_, -1.0f, 1.0f);
    int_samples[i] = static_cast<int16_t>(scaled * 32767.0f);
  }

  return QueueSamples(int_samples.data(), num_samples);
}

bool SDL2AudioBackend::QueueSamplesNative(const int16_t* samples,
                                          int frames_per_channel, int channels,
                                          int native_rate) {
  if (!initialized_ || samples == nullptr) {
    return false;
  }

  if (!audio_stream_enabled_ || audio_stream_ == nullptr) {
    return false;
  }

  if (native_rate != stream_native_rate_ || channels != config_.channels) {
    SetAudioStreamResampling(true, native_rate, channels);
    if (audio_stream_ == nullptr) {
      return false;
    }
  }

  const int bytes_in =
      frames_per_channel * channels * static_cast<int>(sizeof(int16_t));

  if (SDL_AudioStreamPut(audio_stream_, samples, bytes_in) < 0) {
    LOG_ERROR("AudioBackend", "SDL_AudioStreamPut failed: %s", SDL_GetError());
    return false;
  }

  const int available_bytes = SDL_AudioStreamAvailable(audio_stream_);
  if (available_bytes < 0) {
    LOG_ERROR("AudioBackend", "SDL_AudioStreamAvailable failed: %s",
              SDL_GetError());
    return false;
  }

  if (available_bytes == 0) {
    return true;
  }

  const int available_samples =
      available_bytes / static_cast<int>(sizeof(int16_t));
  if (static_cast<int>(stream_buffer_.size()) < available_samples) {
    stream_buffer_.resize(available_samples);
  }

  if (SDL_AudioStreamGet(audio_stream_, stream_buffer_.data(),
                         available_bytes) < 0) {
    LOG_ERROR("AudioBackend", "SDL_AudioStreamGet failed: %s", SDL_GetError());
    return false;
  }

  return QueueSamples(stream_buffer_.data(), available_samples);
}

AudioStatus SDL2AudioBackend::GetStatus() const {
  AudioStatus status;

  if (!initialized_) return status;

  status.is_playing =
      (SDL_GetAudioDeviceStatus(device_id_) == SDL_AUDIO_PLAYING);
  status.queued_bytes = SDL_GetQueuedAudioSize(device_id_);

  // Calculate queued frames (each frame = channels * sample_size)
  int bytes_per_frame =
      config_.channels * (config_.format == SampleFormat::INT16 ? 2 : 4);
  status.queued_frames = status.queued_bytes / bytes_per_frame;

  // Check for underrun (queue too low while playing)
  if (status.is_playing && status.queued_frames < 100) {
    status.has_underrun = true;
  }

  return status;
}

bool SDL2AudioBackend::IsInitialized() const { return initialized_; }

AudioConfig SDL2AudioBackend::GetConfig() const { return config_; }

void SDL2AudioBackend::SetAudioStreamResampling(bool enable, int native_rate,
                                                int channels) {
  if (!initialized_) return;

  if (!enable) {
    if (audio_stream_) {
      SDL_FreeAudioStream(audio_stream_);
      audio_stream_ = nullptr;
    }
    audio_stream_enabled_ = false;
    stream_native_rate_ = 0;
    stream_buffer_.clear();
    return;
  }

  const bool needs_recreate = (audio_stream_ == nullptr) ||
                              (stream_native_rate_ != native_rate) ||
                              (channels != config_.channels);

  if (!needs_recreate) {
    audio_stream_enabled_ = true;
    return;
  }

  if (audio_stream_) {
    SDL_FreeAudioStream(audio_stream_);
    audio_stream_ = nullptr;
  }

  audio_stream_ =
      SDL_NewAudioStream(AUDIO_S16, channels, native_rate, device_format_,
                         device_channels_, device_freq_);
  if (!audio_stream_) {
    LOG_ERROR("AudioBackend", "SDL_NewAudioStream failed: %s", SDL_GetError());
    audio_stream_enabled_ = false;
    stream_native_rate_ = 0;
    return;
  }

  SDL_AudioStreamClear(audio_stream_);
  audio_stream_enabled_ = true;
  stream_native_rate_ = native_rate;
  stream_buffer_.clear();
}

void SDL2AudioBackend::SetVolume(float volume) {
  volume_ = std::clamp(volume, 0.0f, 1.0f);
}

float SDL2AudioBackend::GetVolume() const { return volume_; }

// ============================================================================
// AudioBackendFactory Implementation
// ============================================================================

std::unique_ptr<IAudioBackend> AudioBackendFactory::Create(BackendType type) {
  switch (type) {
    case BackendType::SDL2:
      return std::make_unique<SDL2AudioBackend>();

    case BackendType::NULL_BACKEND:
      // TODO: Implement null backend for testing
      LOG_WARN("AudioBackend", "NULL backend not yet implemented, using SDL2");
      return std::make_unique<SDL2AudioBackend>();

    default:
      LOG_ERROR("AudioBackend", "Unknown backend type, using SDL2");
      return std::make_unique<SDL2AudioBackend>();
  }
}

}  // namespace audio
}  // namespace emu
}  // namespace yaze
