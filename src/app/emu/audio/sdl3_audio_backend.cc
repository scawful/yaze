// sdl3_audio_backend.cc - SDL3 Audio Backend Implementation

#ifdef YAZE_USE_SDL3

#include "app/emu/audio/sdl3_audio_backend.h"

#include <algorithm>
#include <cstring>

#include "util/log.h"

namespace yaze {
namespace emu {
namespace audio {

// ============================================================================
// SDL3AudioBackend Implementation
// ============================================================================

SDL3AudioBackend::~SDL3AudioBackend() {
  Shutdown();
}

bool SDL3AudioBackend::Initialize(const AudioConfig& config) {
  if (initialized_) {
    LOG_WARN("AudioBackend", "SDL3 backend already initialized, shutting down first");
    Shutdown();
  }

  config_ = config;

  // Set up the audio specification for SDL3
  SDL_AudioSpec spec;
  spec.format = (config.format == SampleFormat::INT16) ? SDL_AUDIO_S16 : SDL_AUDIO_F32;
  spec.channels = config.channels;
  spec.freq = config.sample_rate;

  // SDL3 uses stream-based API - open audio device stream
  audio_stream_ = SDL_OpenAudioDeviceStream(
      SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,  // Use default playback device
      &spec,                               // Desired spec
      nullptr,                             // Callback (nullptr for stream mode)
      nullptr                              // User data
  );

  if (!audio_stream_) {
    LOG_ERROR("AudioBackend", "SDL3: Failed to open audio stream: %s", SDL_GetError());
    return false;
  }

  // Get the actual device ID from the stream
  device_id_ = SDL_GetAudioStreamDevice(audio_stream_);
  if (!device_id_) {
    LOG_ERROR("AudioBackend", "SDL3: Failed to get audio device from stream");
    SDL_DestroyAudioStream(audio_stream_);
    audio_stream_ = nullptr;
    return false;
  }

  // Get actual device format information
  SDL_AudioSpec obtained_spec;
  if (SDL_GetAudioDeviceFormat(device_id_, &obtained_spec, nullptr) < 0) {
    LOG_WARN("AudioBackend", "SDL3: Could not query device format: %s", SDL_GetError());
    // Use requested values as fallback
    device_format_ = spec.format;
    device_channels_ = spec.channels;
    device_freq_ = spec.freq;
  } else {
    device_format_ = obtained_spec.format;
    device_channels_ = obtained_spec.channels;
    device_freq_ = obtained_spec.freq;

    // Update config if we got different values
    if (device_freq_ != config_.sample_rate || device_channels_ != config_.channels) {
      LOG_WARN("AudioBackend",
               "SDL3: Audio spec mismatch - wanted %dHz %dch, got %dHz %dch",
               config_.sample_rate, config_.channels, device_freq_, device_channels_);
      config_.sample_rate = device_freq_;
      config_.channels = device_channels_;
    }
  }

  LOG_INFO("AudioBackend",
           "SDL3 audio initialized: %dHz, %d channels, format=%d",
           device_freq_, device_channels_, device_format_);

  initialized_ = true;
  resampling_enabled_ = false;
  native_rate_ = 0;
  native_channels_ = 0;
  resample_buffer_.clear();

  // Start playback immediately
  if (SDL_ResumeAudioDevice(device_id_) < 0) {
    LOG_ERROR("AudioBackend", "SDL3: Failed to resume audio device: %s", SDL_GetError());
    Shutdown();
    return false;
  }

  return true;
}

void SDL3AudioBackend::Shutdown() {
  if (!initialized_) {
    return;
  }

  // Clean up resampling stream
  if (resampling_stream_) {
    SDL_DestroyAudioStream(resampling_stream_);
    resampling_stream_ = nullptr;
  }
  resampling_enabled_ = false;
  native_rate_ = 0;
  native_channels_ = 0;
  resample_buffer_.clear();

  // Pause device before cleanup
  if (device_id_) {
    SDL_PauseAudioDevice(device_id_);
  }

  // Destroy main audio stream
  if (audio_stream_) {
    SDL_DestroyAudioStream(audio_stream_);
    audio_stream_ = nullptr;
  }

  device_id_ = 0;
  initialized_ = false;

  LOG_INFO("AudioBackend", "SDL3 audio shut down");
}

void SDL3AudioBackend::Play() {
  if (!initialized_ || !device_id_) {
    return;
  }
  SDL_ResumeAudioDevice(device_id_);
}

void SDL3AudioBackend::Pause() {
  if (!initialized_ || !device_id_) {
    return;
  }
  SDL_PauseAudioDevice(device_id_);
}

void SDL3AudioBackend::Stop() {
  if (!initialized_) {
    return;
  }
  Clear();
  if (device_id_) {
    SDL_PauseAudioDevice(device_id_);
  }
}

void SDL3AudioBackend::Clear() {
  if (!initialized_) {
    return;
  }

  if (audio_stream_) {
    SDL_ClearAudioStream(audio_stream_);
  }

  if (resampling_stream_) {
    SDL_ClearAudioStream(resampling_stream_);
  }
}

bool SDL3AudioBackend::QueueSamples(const int16_t* samples, int num_samples) {
  if (!initialized_ || !audio_stream_ || !samples) {
    return false;
  }

  // Fast path: No volume adjustment needed
  if (volume_ == 1.0f) {
    int result = SDL_PutAudioStreamData(audio_stream_, samples,
                                        num_samples * sizeof(int16_t));
    if (result < 0) {
      LOG_ERROR("AudioBackend", "SDL3: SDL_PutAudioStreamData failed: %s", SDL_GetError());
      return false;
    }
    return true;
  }

  // Slow path: Volume scaling required
  thread_local std::vector<int16_t> scaled_samples;

  if (scaled_samples.size() < static_cast<size_t>(num_samples)) {
    scaled_samples.resize(num_samples);
  }

  // Apply volume scaling
  float vol = volume_.load();
  for (int i = 0; i < num_samples; ++i) {
    int32_t scaled = static_cast<int32_t>(samples[i] * vol);
    scaled_samples[i] = static_cast<int16_t>(std::clamp(scaled, -32768, 32767));
  }

  int result = SDL_PutAudioStreamData(audio_stream_, scaled_samples.data(),
                                      num_samples * sizeof(int16_t));
  if (result < 0) {
    LOG_ERROR("AudioBackend", "SDL3: SDL_PutAudioStreamData failed: %s", SDL_GetError());
    return false;
  }

  return true;
}

bool SDL3AudioBackend::QueueSamples(const float* samples, int num_samples) {
  if (!initialized_ || !audio_stream_ || !samples) {
    return false;
  }

  // Convert float to int16 with volume scaling
  thread_local std::vector<int16_t> int_samples;
  if (int_samples.size() < static_cast<size_t>(num_samples)) {
    int_samples.resize(num_samples);
  }

  float vol = volume_.load();
  for (int i = 0; i < num_samples; ++i) {
    float scaled = std::clamp(samples[i] * vol, -1.0f, 1.0f);
    int_samples[i] = static_cast<int16_t>(scaled * 32767.0f);
  }

  return QueueSamples(int_samples.data(), num_samples);
}

bool SDL3AudioBackend::QueueSamplesNative(const int16_t* samples,
                                          int frames_per_channel, int channels,
                                          int native_rate) {
  if (!initialized_ || !samples) {
    return false;
  }

  // Check if we need to set up resampling
  if (!resampling_enabled_ || !resampling_stream_) {
    LOG_WARN("AudioBackend", "SDL3: Native rate resampling not enabled");
    return false;
  }

  // Verify the resampling configuration matches
  if (native_rate != native_rate_ || channels != native_channels_) {
    SetAudioStreamResampling(true, native_rate, channels);
    if (!resampling_stream_) {
      return false;
    }
  }

  const int bytes_in = frames_per_channel * channels * static_cast<int>(sizeof(int16_t));

  // Put data into resampling stream
  if (SDL_PutAudioStreamData(resampling_stream_, samples, bytes_in) < 0) {
    LOG_ERROR("AudioBackend", "SDL3: Failed to put data in resampling stream: %s",
              SDL_GetError());
    return false;
  }

  // Get available resampled data
  int available_bytes = SDL_GetAudioStreamAvailable(resampling_stream_);
  if (available_bytes < 0) {
    LOG_ERROR("AudioBackend", "SDL3: Failed to get available stream data: %s",
              SDL_GetError());
    return false;
  }

  if (available_bytes == 0) {
    return true;  // No data ready yet
  }

  // Resize buffer if needed
  int available_samples = available_bytes / static_cast<int>(sizeof(int16_t));
  if (static_cast<int>(resample_buffer_.size()) < available_samples) {
    resample_buffer_.resize(available_samples);
  }

  // Get resampled data
  int bytes_read = SDL_GetAudioStreamData(resampling_stream_,
                                          resample_buffer_.data(),
                                          available_bytes);
  if (bytes_read < 0) {
    LOG_ERROR("AudioBackend", "SDL3: Failed to get resampled data: %s",
              SDL_GetError());
    return false;
  }

  // Queue the resampled data
  int samples_read = bytes_read / static_cast<int>(sizeof(int16_t));
  return QueueSamples(resample_buffer_.data(), samples_read);
}

AudioStatus SDL3AudioBackend::GetStatus() const {
  AudioStatus status;

  if (!initialized_) {
    return status;
  }

  // Check if device is playing
  status.is_playing = device_id_ && !SDL_IsAudioDevicePaused(device_id_);

  // Get queued audio size from stream
  if (audio_stream_) {
    int queued_bytes = SDL_GetAudioStreamQueued(audio_stream_);
    if (queued_bytes >= 0) {
      status.queued_bytes = static_cast<uint32_t>(queued_bytes);
    }
  }

  // Calculate queued frames
  int bytes_per_frame = config_.channels *
                       (config_.format == SampleFormat::INT16 ? 2 : 4);
  if (bytes_per_frame > 0) {
    status.queued_frames = status.queued_bytes / bytes_per_frame;
  }

  // Check for underrun (queue too low while playing)
  if (status.is_playing && status.queued_frames < 100) {
    status.has_underrun = true;
  }

  return status;
}

bool SDL3AudioBackend::IsInitialized() const {
  return initialized_;
}

AudioConfig SDL3AudioBackend::GetConfig() const {
  return config_;
}

void SDL3AudioBackend::SetVolume(float volume) {
  volume_ = std::clamp(volume, 0.0f, 1.0f);
}

float SDL3AudioBackend::GetVolume() const {
  return volume_;
}

void SDL3AudioBackend::SetAudioStreamResampling(bool enable, int native_rate,
                                                int channels) {
  if (!initialized_) {
    return;
  }

  if (!enable) {
    // Disable resampling
    if (resampling_stream_) {
      SDL_DestroyAudioStream(resampling_stream_);
      resampling_stream_ = nullptr;
    }
    resampling_enabled_ = false;
    native_rate_ = 0;
    native_channels_ = 0;
    resample_buffer_.clear();
    return;
  }

  // Check if we need to recreate the resampling stream
  const bool needs_recreate = (resampling_stream_ == nullptr) ||
                              (native_rate_ != native_rate) ||
                              (native_channels_ != channels);

  if (!needs_recreate) {
    resampling_enabled_ = true;
    return;
  }

  // Clean up existing stream
  if (resampling_stream_) {
    SDL_DestroyAudioStream(resampling_stream_);
    resampling_stream_ = nullptr;
  }

  // Create new resampling stream
  // Source spec (native rate)
  SDL_AudioSpec src_spec;
  src_spec.format = SDL_AUDIO_S16;
  src_spec.channels = channels;
  src_spec.freq = native_rate;

  // Destination spec (device rate)
  SDL_AudioSpec dst_spec;
  dst_spec.format = device_format_;
  dst_spec.channels = device_channels_;
  dst_spec.freq = device_freq_;

  // Create audio stream for resampling
  resampling_stream_ = SDL_CreateAudioStream(&src_spec, &dst_spec);
  if (!resampling_stream_) {
    LOG_ERROR("AudioBackend", "SDL3: Failed to create resampling stream: %s",
              SDL_GetError());
    resampling_enabled_ = false;
    native_rate_ = 0;
    native_channels_ = 0;
    return;
  }

  // Clear any existing data
  SDL_ClearAudioStream(resampling_stream_);

  // Update state
  resampling_enabled_ = true;
  native_rate_ = native_rate;
  native_channels_ = channels;
  resample_buffer_.clear();

  LOG_INFO("AudioBackend",
           "SDL3: Resampling enabled: %dHz %dch -> %dHz %dch",
           native_rate, channels, device_freq_, device_channels_);
}

// Helper functions for volume application
bool SDL3AudioBackend::ApplyVolume(int16_t* samples, int num_samples) const {
  if (!samples) {
    return false;
  }

  float vol = volume_.load();
  if (vol == 1.0f) {
    return true;  // No change needed
  }

  for (int i = 0; i < num_samples; ++i) {
    int32_t scaled = static_cast<int32_t>(samples[i] * vol);
    samples[i] = static_cast<int16_t>(std::clamp(scaled, -32768, 32767));
  }

  return true;
}

bool SDL3AudioBackend::ApplyVolume(float* samples, int num_samples) const {
  if (!samples) {
    return false;
  }

  float vol = volume_.load();
  if (vol == 1.0f) {
    return true;  // No change needed
  }

  for (int i = 0; i < num_samples; ++i) {
    samples[i] = std::clamp(samples[i] * vol, -1.0f, 1.0f);
  }

  return true;
}

}  // namespace audio
}  // namespace emu
}  // namespace yaze

#endif  // YAZE_USE_SDL3