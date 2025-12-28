// audio_backend.cc - Audio Backend Implementation

#include "app/emu/audio/audio_backend.h"

#include "app/platform/sdl_compat.h"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#include <algorithm>
#include <vector>

#include "util/log.h"

#ifdef YAZE_USE_SDL3
#include "app/emu/audio/sdl3_audio_backend.h"
#endif

#ifdef __EMSCRIPTEN__
#include "app/emu/platform/wasm/wasm_audio.h"
#endif

namespace yaze {
namespace emu {
namespace audio {

// ============================================================================
// SDL2AudioBackend Implementation
// ============================================================================

#ifdef YAZE_USE_SDL3

// SDL2 backend is not available in SDL3 builds; provide stubs to satisfy
// legacy interfaces while deferring to SDL3AudioBackend in the factory.
SDL2AudioBackend::~SDL2AudioBackend() = default;

bool SDL2AudioBackend::Initialize(const AudioConfig& /*config*/) {
  LOG_ERROR("AudioBackend",
            "SDL2AudioBackend is unavailable when building with SDL3");
  return false;
}

void SDL2AudioBackend::Shutdown() {}
void SDL2AudioBackend::Play() {}
void SDL2AudioBackend::Pause() {}
void SDL2AudioBackend::Stop() {}
void SDL2AudioBackend::Clear() {}

bool SDL2AudioBackend::QueueSamples(const int16_t* /*samples*/,
                                    int /*num_samples*/) {
  return false;
}

bool SDL2AudioBackend::QueueSamples(const float* /*samples*/,
                                    int /*num_samples*/) {
  return false;
}

bool SDL2AudioBackend::QueueSamplesNative(const int16_t* /*samples*/,
                                          int /*frames_per_channel*/,
                                          int /*channels*/,
                                          int /*native_rate*/) {
  return false;
}

AudioStatus SDL2AudioBackend::GetStatus() const { return {}; }
bool SDL2AudioBackend::IsInitialized() const { return false; }
AudioConfig SDL2AudioBackend::GetConfig() const { return config_; }

void SDL2AudioBackend::SetVolume(float /*volume*/) {}
float SDL2AudioBackend::GetVolume() const { return 0.0f; }

void SDL2AudioBackend::SetAudioStreamResampling(bool /*enable*/,
                                                int /*native_rate*/,
                                                int /*channels*/) {}

bool SDL2AudioBackend::IsAudioStreamEnabled() const { return false; }

#else

SDL2AudioBackend::~SDL2AudioBackend() {
  Shutdown();
}

bool SDL2AudioBackend::Initialize(const AudioConfig& config) {
  if (initialized_) {
    LOG_WARN("AudioBackend", "Already initialized, shutting down first");
    Shutdown();
  }

  config_ = config;

  SDL_AudioSpec want, have;
  SDL_memset(&want, 0, sizeof(want));

  // Force 48000Hz request to ensure we get a standard rate that SDL/CoreAudio
  // handles reliably. We will handle 32kHz -> 48kHz resampling ourselves
  // using SDL_AudioStream.
  want.freq = 48000;
  want.format = (config.format == SampleFormat::INT16) ? AUDIO_S16 : AUDIO_F32;
  want.channels = config.channels;
  want.samples = config.buffer_frames;
  want.callback = nullptr;  // Use queue-based audio

  // Allow SDL to change any parameter to match the hardware.
  // This is CRITICAL: If we force 32040Hz on a 48000Hz device without allowing changes,
  // SDL might claim success but playback will be at the wrong speed (chipmunk effect).
  // By allowing changes, 'have' will contain the REAL hardware spec (e.g. 48000Hz),
  // which allows us to detect the mismatch and enable our software resampler.
  // Allow format and channel changes, but FORCE frequency to 48000Hz.
  // This prevents issues where SDL reports a weird frequency (e.g. 32040Hz)
  // but the hardware actually runs at 48000Hz, causing pitch/speed issues.
  // SDL will handle internal resampling if the hardware doesn't support 48000Hz.
  int allowed_changes = SDL_AUDIO_ALLOW_FORMAT_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE;
#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
  SDL_SetHint(SDL_HINT_AUDIO_CATEGORY, "ambient");
#endif
  device_id_ = SDL_OpenAudioDevice(nullptr, 0, &want, &have, allowed_changes);

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
           "SDL2 audio initialized: %dHz, %d channels, buffer: want=%d, have=%d",
           have.freq, have.channels, want.samples, have.samples);
  LOG_INFO("AudioBackend",
           "Device actual: freq=%d, format=0x%04X, channels=%d (device_id=%u)",
           device_freq_, device_format_, device_channels_, device_id_);

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
  if (!initialized_)
    return;

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
  if (!initialized_) {
    LOG_WARN("AudioBackend", "Play() called but not initialized!");
    return;
  }
  SDL_AudioStatus status_before = SDL_GetAudioDeviceStatus(device_id_);
  SDL_PauseAudioDevice(device_id_, 0);
  SDL_AudioStatus status_after = SDL_GetAudioDeviceStatus(device_id_);
  LOG_INFO("AudioBackend", "Play() device=%u: status %d -> %d (0=stopped,1=playing,2=paused)",
           device_id_, status_before, status_after);
}

void SDL2AudioBackend::Pause() {
  if (!initialized_)
    return;
  SDL_PauseAudioDevice(device_id_, 1);
}

void SDL2AudioBackend::Stop() {
  if (!initialized_)
    return;
  Clear();
  SDL_PauseAudioDevice(device_id_, 1);
}

void SDL2AudioBackend::Clear() {
  if (!initialized_)
    return;
  uint32_t before = SDL_GetQueuedAudioSize(device_id_);
  SDL_ClearQueuedAudio(device_id_);
  if (audio_stream_) {
    SDL_AudioStreamClear(audio_stream_);
  }
  uint32_t after = SDL_GetQueuedAudioSize(device_id_);
  LOG_INFO("AudioBackend", "Clear() device=%u: queue %u -> %u bytes", device_id_, before, after);
}

bool SDL2AudioBackend::QueueSamples(const int16_t* samples, int num_samples) {
  if (!initialized_ || !samples)
    return false;

  // Periodic logging (debug only, very infrequent)
  static int queue_log = 0;
  if (++queue_log % 2000 == 0) {
    LOG_DEBUG("AudioBackend", "QueueSamples: %d samples to device %u", num_samples, device_id_);
  }

  if (volume_ == 1.0f) {
    // Fast path: No volume adjustment needed
    int result =
        SDL_QueueAudio(device_id_, samples, num_samples * sizeof(int16_t));
    if (result < 0) {
      static int error_log = 0;
      if (++error_log % 60 == 0) {
        LOG_ERROR("AudioBackend", "SDL_QueueAudio failed: %s", SDL_GetError());
      }
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
    static int error_log = 0;
    if (++error_log % 60 == 0) {
      LOG_ERROR("AudioBackend", "SDL_QueueAudio failed: %s", SDL_GetError());
    }
    return false;
  }

  return true;
}

bool SDL2AudioBackend::QueueSamples(const float* samples, int num_samples) {
  if (!initialized_ || !samples)
    return false;

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
  // DIAGNOSTIC: Track which backend instance is calling (per-instance, not static)
  call_count_++;
  if (call_count_ <= 2 || call_count_ % 1000 == 0) {
    LOG_DEBUG("AudioBackend",
              "QueueSamplesNative [device=%u]: frames=%d, ch=%d, native=%dHz, "
              "stream_enabled=%d, stream=%p, device_freq=%dHz, calls=%d",
              device_id_, frames_per_channel, channels, native_rate,
              audio_stream_enabled_, static_cast<void*>(audio_stream_), device_freq_,
              call_count_);
  }

  if (!initialized_ || samples == nullptr) {
    static int init_fail_log = 0;
    if (++init_fail_log % 300 == 0) {
      LOG_WARN("AudioBackend", "QueueSamplesNative: FAILED (init=%d, samples=%p)",
               initialized_, samples);
    }
    return false;
  }

  if (!audio_stream_enabled_ || audio_stream_ == nullptr) {
    static int stream_fail_log = 0;
    if (++stream_fail_log % 600 == 0) {
      LOG_WARN("AudioBackend", "QueueSamplesNative: STREAM DISABLED (enabled=%d, stream=%p) - Audio will play at WRONG SPEED!",
               audio_stream_enabled_, static_cast<void*>(audio_stream_));
    }
    return false;
  }

  static int native_log = 0;
  if (++native_log % 300 == 0) {
      LOG_DEBUG("AudioBackend", "QueueSamplesNative: %d frames (Native: %d, Stream: %d)", frames_per_channel, native_rate, stream_native_rate_);
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
    static int put_log = 0;
    if (++put_log % 60 == 0) {
      LOG_ERROR("AudioBackend", "SDL_AudioStreamPut failed: %s", SDL_GetError());
    }
    return false;
  }

  const int available_bytes = SDL_AudioStreamAvailable(audio_stream_);
  if (available_bytes < 0) {
    static int avail_log = 0;
    if (++avail_log % 60 == 0) {
      LOG_ERROR("AudioBackend", "SDL_AudioStreamAvailable failed: %s",
                SDL_GetError());
    }
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

  int bytes_read = SDL_AudioStreamGet(audio_stream_, stream_buffer_.data(),
                                      available_bytes);
  if (bytes_read < 0) {
    static int get_log = 0;
    if (++get_log % 60 == 0) {
      LOG_ERROR("AudioBackend", "SDL_AudioStreamGet failed: %s", SDL_GetError());
    }
    return false;
  }

  // Debug resampling ratio occasionally
  static int log_counter = 0;
  if (++log_counter % 600 == 0) {
    LOG_DEBUG("AudioBackend",
              "Resample trace: In=%d bytes (%dHz), Out=%d bytes (%dHz)", bytes_in,
              stream_native_rate_, bytes_read, device_freq_);
  }

  return QueueSamples(stream_buffer_.data(), available_samples);
}

AudioStatus SDL2AudioBackend::GetStatus() const {
  AudioStatus status;

  if (!initialized_)
    return status;

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
    static int underrun_log = 0;
    if (++underrun_log % 300 == 0) {
      LOG_WARN("AudioBackend", "Audio underrun risk: queued_frames=%u (device=%u)",
               status.queued_frames, device_id_);
    }
  }

  return status;
}

bool SDL2AudioBackend::IsInitialized() const {
  return initialized_;
}

AudioConfig SDL2AudioBackend::GetConfig() const {
  return config_;
}

void SDL2AudioBackend::SetAudioStreamResampling(bool enable, int native_rate,
                                                int channels) {
  if (!initialized_)
    return;

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

  LOG_INFO("AudioBackend", "SDL_AudioStream created: %dHz %dch -> %dHz %dch",
           native_rate, channels, device_freq_, device_channels_);

  SDL_AudioStreamClear(audio_stream_);
  audio_stream_enabled_ = true;
  stream_native_rate_ = native_rate;
  stream_buffer_.clear();
}

void SDL2AudioBackend::SetVolume(float volume) {
  volume_ = std::clamp(volume, 0.0f, 1.0f);
}

float SDL2AudioBackend::GetVolume() const {
  return volume_;
}

bool SDL2AudioBackend::IsAudioStreamEnabled() const {
  return audio_stream_enabled_ && audio_stream_ != nullptr;
}

#endif  // YAZE_USE_SDL3

// ============================================================================
// NullAudioBackend Implementation (for testing/headless)
// ============================================================================

bool NullAudioBackend::Initialize(const AudioConfig& config) {
  config_ = config;
  initialized_ = true;
  playing_ = false;
  total_queued_samples_ = 0;
  total_queued_frames_ = 0;
  current_queued_bytes_ = 0;
  LOG_INFO("AudioBackend", "Null audio backend initialized (%dHz, %d channels)",
           config.sample_rate, config.channels);
  return true;
}

void NullAudioBackend::Shutdown() {
  initialized_ = false;
  playing_ = false;
  LOG_INFO("AudioBackend", "Null audio backend shut down");
}

void NullAudioBackend::Play() {
  if (initialized_) playing_ = true;
}

void NullAudioBackend::Pause() {
  if (initialized_) playing_ = false;
}

void NullAudioBackend::Stop() {
  if (initialized_) {
    playing_ = false;
    current_queued_bytes_ = 0;
  }
}

void NullAudioBackend::Clear() {
  current_queued_bytes_ = 0;
}

bool NullAudioBackend::QueueSamples(const int16_t* samples, int num_samples) {
  if (!initialized_ || !samples) return false;

  total_queued_samples_ += num_samples;
  current_queued_bytes_ += num_samples * sizeof(int16_t);
  return true;
}

bool NullAudioBackend::QueueSamples(const float* samples, int num_samples) {
  if (!initialized_ || !samples) return false;

  total_queued_samples_ += num_samples;
  current_queued_bytes_ += num_samples * sizeof(float);
  return true;
}

bool NullAudioBackend::QueueSamplesNative(const int16_t* samples,
                                          int frames_per_channel, int channels,
                                          int native_rate) {
  if (!initialized_ || !samples) return false;

  // Track frames queued (for timing verification)
  total_queued_frames_ += frames_per_channel;
  total_queued_samples_ += frames_per_channel * channels;
  current_queued_bytes_ += frames_per_channel * channels * sizeof(int16_t);

  return true;
}

AudioStatus NullAudioBackend::GetStatus() const {
  AudioStatus status;
  status.is_playing = playing_;
  status.queued_bytes = static_cast<uint32_t>(current_queued_bytes_);
  status.queued_frames = static_cast<uint32_t>(current_queued_bytes_ /
                         (config_.channels * sizeof(int16_t)));
  status.has_underrun = false;
  return status;
}

bool NullAudioBackend::IsInitialized() const {
  return initialized_;
}

AudioConfig NullAudioBackend::GetConfig() const {
  return config_;
}

void NullAudioBackend::SetVolume(float volume) {
  volume_ = std::clamp(volume, 0.0f, 1.0f);
}

float NullAudioBackend::GetVolume() const {
  return volume_;
}

void NullAudioBackend::SetAudioStreamResampling(bool enable, int native_rate,
                                                 int channels) {
  audio_stream_enabled_ = enable;
  stream_native_rate_ = native_rate;
  stream_channels_ = channels;
  LOG_INFO("AudioBackend", "Null backend: resampling %s (%dHz -> %dHz)",
           enable ? "enabled" : "disabled", native_rate, config_.sample_rate);
}

bool NullAudioBackend::IsAudioStreamEnabled() const {
  return audio_stream_enabled_;
}

void NullAudioBackend::ResetCounters() {
  total_queued_samples_ = 0;
  total_queued_frames_ = 0;
  current_queued_bytes_ = 0;
}

// ============================================================================
// AudioBackendFactory Implementation
// ============================================================================

std::unique_ptr<IAudioBackend> AudioBackendFactory::Create(BackendType type) {
  switch (type) {
    case BackendType::SDL2:
#ifdef YAZE_USE_SDL3
      // Prefer SDL3 backend when SDL3 is in use.
      return std::make_unique<SDL3AudioBackend>();
#else
      return std::make_unique<SDL2AudioBackend>();
#endif

    case BackendType::SDL3:
#ifdef YAZE_USE_SDL3
      return std::make_unique<SDL3AudioBackend>();
#else
      LOG_ERROR("AudioBackend", "SDL3 backend requested but not compiled with SDL3 support");
      return std::make_unique<SDL2AudioBackend>();
#endif

    case BackendType::WASM:
#ifdef __EMSCRIPTEN__
      return std::make_unique<WasmAudioBackend>();
#else
      LOG_ERROR("AudioBackend", "WASM backend requested but not compiled for Emscripten");
      return std::make_unique<SDL2AudioBackend>();
#endif

    case BackendType::NULL_BACKEND:
      return std::make_unique<NullAudioBackend>();

    default:
      LOG_ERROR("AudioBackend", "Unknown backend type, using default backend");
#ifdef YAZE_USE_SDL3
      return std::make_unique<SDL3AudioBackend>();
#else
      return std::make_unique<SDL2AudioBackend>();
#endif
  }
}

}  // namespace audio
}  // namespace emu
}  // namespace yaze
