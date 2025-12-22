// clang-format off
// wasm_audio.cc - WebAudio Backend Implementation for WASM/Emscripten
// Implements audio output using browser's WebAudio API via Emscripten

#ifdef __EMSCRIPTEN__

#include "app/emu/platform/wasm/wasm_audio.h"

#include <emscripten.h>
#include <emscripten/html5.h>
#include <algorithm>
#include <cstring>
#include <iostream>

namespace yaze {
namespace emu {
namespace audio {

// JavaScript functions for WebAudio API interaction
// These are implemented using EM_JS to directly embed JavaScript code

EM_JS(void*, wasm_audio_create_context, (int sample_rate), {
  try {
    // Create AudioContext with specified sample rate
    const AudioContext = window.AudioContext || window.webkitAudioContext;
    if (!AudioContext) {
      console.error("WebAudio API not supported in this browser");
      return 0;
    }

    const ctx = new AudioContext({
      sampleRate: sample_rate,
      latencyHint: 'interactive'
    });

    // Store context in global object for access
    if (!window.yazeAudio) {
      window.yazeAudio = {};
    }

    // Generate unique ID for this context
    const contextId = Date.now();
    window.yazeAudio[contextId] = {
      context: ctx,
      processor: null,
      bufferQueue: [],
      isPlaying: false,
      volume: 1.0
    };

    console.log('Created WebAudio context with sample rate:', sample_rate);
    return contextId;
  } catch (e) {
    console.error('Failed to create WebAudio context:', e);
    return 0;
  }
});

EM_JS(void*, wasm_audio_create_processor, (void* context_handle, int buffer_size, int channels), {
  try {
    const audio = window.yazeAudio[context_handle];
    if (!audio || !audio.context) {
      console.error('Invalid audio context handle');
      return 0;
    }

    const ctx = audio.context;

    // Create gain node for volume control
    const gainNode = ctx.createGain();
    gainNode.gain.value = audio.volume;
    audio.gainNode = gainNode;

    // Try AudioWorklet first (modern, better performance)
    // Fall back to ScriptProcessorNode if not available
    const tryAudioWorklet = async () => {
      try {
        // Check if AudioWorklet is supported
        if (typeof AudioWorkletNode === 'undefined' || !ctx.audioWorklet) {
          throw new Error('AudioWorklet not supported');
        }

        // Load the AudioWorklet processor module
        await ctx.audioWorklet.addModule('core/audio_worklet_processor.js');

        // Create the worklet node
        const workletNode = new AudioWorkletNode(ctx, 'snes-audio-processor', {
          numberOfInputs: 0,
          numberOfOutputs: 1,
          outputChannelCount: [channels],
          processorOptions: {
            bufferSize: buffer_size * 4,  // Larger ring buffer
            channels: channels
          }
        });

        // Connect worklet -> gain -> destination
        workletNode.connect(gainNode);
        gainNode.connect(ctx.destination);

        // Store worklet reference
        audio.workletNode = workletNode;
        audio.useWorklet = true;

        // Handle messages from worklet
        workletNode.port.onmessage = (event) => {
          if (event.data.type === 'status') {
            audio.workletStatus = event.data;
          }
        };

        console.log('[AudioWorklet] Created SNES audio processor with buffer size:', buffer_size);
        return true;
      } catch (e) {
        console.warn('[AudioWorklet] Failed to initialize, falling back to ScriptProcessorNode:', e.message);
        return false;
      }
    };

    // Try AudioWorklet, fall back to ScriptProcessorNode
    tryAudioWorklet().then(success => {
      if (!success) {
        // Fallback: Create ScriptProcessorNode (deprecated but widely supported)
        const processor = ctx.createScriptProcessor(buffer_size, 0, channels);

        // Connect processor -> gain -> destination
        processor.connect(gainNode);
        gainNode.connect(ctx.destination);

        // Store nodes
        audio.processor = processor;
        audio.useWorklet = false;

        // Setup audio processing callback
        processor.onaudioprocess = function(e) {
          const outputBuffer = e.outputBuffer;
          const numChannels = outputBuffer.numberOfChannels;
          const frameCount = outputBuffer.length;

          if (!audio.isPlaying || audio.bufferQueue.length === 0) {
            // Output silence
            for (let ch = 0; ch < numChannels; ch++) {
              const channel = outputBuffer.getChannelData(ch);
              channel.fill(0);
            }
            return;
          }

          // Process queued buffers
          let framesWritten = 0;
          while (framesWritten < frameCount && audio.bufferQueue.length > 0) {
            const buffer = audio.bufferQueue[0];
            const remainingInBuffer = buffer.length - buffer.position;
            const framesToCopy = Math.min(frameCount - framesWritten, remainingInBuffer);

            // Copy samples to output channels
            for (let ch = 0; ch < numChannels; ch++) {
              const outputChannel = outputBuffer.getChannelData(ch);
              for (let i = 0; i < framesToCopy; i++) {
                const sampleIndex = (buffer.position + i) * numChannels + ch;
                outputChannel[framesWritten + i] = buffer.samples[sampleIndex] || 0;
              }
            }

            buffer.position += framesToCopy;
            framesWritten += framesToCopy;

            // Remove buffer if fully consumed
            if (buffer.position >= buffer.length) {
              audio.bufferQueue.shift();
            }
          }

          // Fill remaining with silence if needed
          if (framesWritten < frameCount) {
            for (let ch = 0; ch < numChannels; ch++) {
              const channel = outputBuffer.getChannelData(ch);
              for (let i = framesWritten; i < frameCount; i++) {
                channel[i] = 0;
              }
            }
          }
        };

        console.log('[ScriptProcessor] Created audio processor with buffer size:', buffer_size);
      }
    });

    return context_handle; // Return same handle since processor is stored in audio object
  } catch (e) {
    console.error('Failed to create audio processor:', e);
    return 0;
  }
});

EM_JS(void, wasm_audio_queue_samples, (void* context_handle, float* samples, int frame_count, int channels), {
  const audio = window.yazeAudio[context_handle];
  if (!audio) return;

  // Copy samples from WASM memory to JavaScript array
  const totalSamples = frame_count * channels;
  const sampleArray = new Float32Array(totalSamples);
  for (let i = 0; i < totalSamples; i++) {
    sampleArray[i] = HEAPF32[(samples >> 2) + i];
  }

  // Route samples to appropriate backend
  if (audio.useWorklet && audio.workletNode) {
    // AudioWorklet: Send samples via MessagePort (more efficient)
    audio.workletNode.port.postMessage({
      type: 'samples',
      samples: sampleArray,
      frameCount: frame_count
    });
  } else {
    // ScriptProcessorNode: Add to buffer queue
    audio.bufferQueue.push({
      samples: sampleArray,
      length: frame_count,
      position: 0
    });

    // Limit queue size to prevent excessive memory usage
    const maxQueueSize = 32;
    while (audio.bufferQueue.length > maxQueueSize) {
      audio.bufferQueue.shift();
    }
  }
});

EM_JS(void, wasm_audio_play, (void* context_handle), {
  const audio = window.yazeAudio[context_handle];
  if (!audio || !audio.context) return;

  audio.isPlaying = true;

  // Resume context if suspended (due to autoplay policy)
  if (audio.context.state === 'suspended') {
    audio.context.resume().then(() => {
      console.log('Audio context resumed');
    }).catch(e => {
      console.error('Failed to resume audio context:', e);
    });
  }
});

EM_JS(void, wasm_audio_pause, (void* context_handle), {
  const audio = window.yazeAudio[context_handle];
  if (!audio) return;

  audio.isPlaying = false;
});

EM_JS(void, wasm_audio_stop, (void* context_handle), {
  const audio = window.yazeAudio[context_handle];
  if (!audio) return;

  audio.isPlaying = false;
  audio.bufferQueue = [];
});

EM_JS(void, wasm_audio_clear, (void* context_handle), {
  const audio = window.yazeAudio[context_handle];
  if (!audio) return;

  audio.bufferQueue = [];
});

EM_JS(void, wasm_audio_set_volume, (void* context_handle, float volume), {
  const audio = window.yazeAudio[context_handle];
  if (!audio || !audio.gainNode) return;

  audio.volume = Math.max(0, Math.min(1, volume));
  audio.gainNode.gain.value = audio.volume;
});

EM_JS(float, wasm_audio_get_volume, (void* context_handle), {
  const audio = window.yazeAudio[context_handle];
  if (!audio) return 1.0;

  return audio.volume;
});

EM_JS(int, wasm_audio_get_queued_frames, (void* context_handle), {
  const audio = window.yazeAudio[context_handle];
  if (!audio) return 0;

  let total = 0;
  for (const buffer of audio.bufferQueue) {
    total += buffer.length - buffer.position;
  }
  return total;
});

EM_JS(bool, wasm_audio_is_playing, (void* context_handle), {
  const audio = window.yazeAudio[context_handle];
  if (!audio) return false;

  return audio.isPlaying;
});

EM_JS(bool, wasm_audio_is_suspended, (void* context_handle), {
  const audio = window.yazeAudio[context_handle];
  if (!audio || !audio.context) return true;

  return audio.context.state === 'suspended';
});

EM_JS(void, wasm_audio_resume_context, (void* context_handle), {
  const audio = window.yazeAudio[context_handle];
  if (!audio || !audio.context) return;

  if (audio.context.state === 'suspended') {
    audio.context.resume().then(() => {
      console.log('Audio context resumed via user interaction');
    }).catch(e => {
      console.error('Failed to resume audio context:', e);
    });
  }
});

EM_JS(void, wasm_audio_shutdown, (void* context_handle), {
  const audio = window.yazeAudio[context_handle];
  if (!audio) return;

  // Clean up AudioWorklet if used
  if (audio.workletNode) {
    audio.workletNode.port.postMessage({ type: 'clear' });
    audio.workletNode.disconnect();
    audio.workletNode = null;
    audio.useWorklet = false;
    console.log('[AudioWorklet] Processor disconnected');
  }

  // Clean up ScriptProcessorNode if used
  if (audio.processor) {
    audio.processor.disconnect();
    audio.processor = null;
    console.log('[ScriptProcessor] Processor disconnected');
  }

  if (audio.gainNode) {
    audio.gainNode.disconnect();
    audio.gainNode = null;
  }

  if (audio.context) {
    audio.context.close().then(() => {
      console.log('Audio context closed');
    }).catch(e => {
      console.error('Failed to close audio context:', e);
    });
  }

  delete window.yazeAudio[context_handle];
});

// C++ Implementation

WasmAudioBackend::WasmAudioBackend() {
  conversion_buffer_.reserve(kDefaultBufferSize * 2);  // Stereo
  resampling_buffer_.reserve(kDefaultBufferSize * 2);
}

WasmAudioBackend::~WasmAudioBackend() {
  Shutdown();
}

bool WasmAudioBackend::Initialize(const AudioConfig& config) {
  if (initialized_) {
    return true;
  }

  config_ = config;

  // Create WebAudio context
  audio_context_ = reinterpret_cast<void*>(wasm_audio_create_context(config.sample_rate));
  if (!audio_context_) {
    std::cerr << "Failed to create WebAudio context" << std::endl;
    return false;
  }

  // Create script processor for audio output
  script_processor_ = reinterpret_cast<void*>(
      wasm_audio_create_processor(audio_context_, config.buffer_frames, config.channels));
  if (!script_processor_) {
    std::cerr << "Failed to create WebAudio processor" << std::endl;
    wasm_audio_shutdown(audio_context_);
    audio_context_ = nullptr;
    return false;
  }

  initialized_ = true;
  context_suspended_ = wasm_audio_is_suspended(audio_context_);

  std::cout << "WasmAudioBackend initialized - Sample rate: " << config.sample_rate
            << " Hz, Channels: " << config.channels
            << ", Buffer: " << config.buffer_frames << " frames" << std::endl;

  return true;
}

void WasmAudioBackend::Shutdown() {
  if (!initialized_) {
    return;
  }

  Stop();

  if (audio_context_) {
    wasm_audio_shutdown(audio_context_);
    audio_context_ = nullptr;
  }

  script_processor_ = nullptr;
  initialized_ = false;

  // Clear buffers
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    while (!sample_queue_.empty()) {
      sample_queue_.pop();
    }
  }

  queued_samples_ = 0;
  total_samples_played_ = 0;
}

void WasmAudioBackend::Play() {
  if (!initialized_ || !audio_context_) {
    return;
  }

  playing_ = true;
  wasm_audio_play(audio_context_);

  // Check if context needs resuming (autoplay policy)
  if (context_suspended_) {
    context_suspended_ = wasm_audio_is_suspended(audio_context_);
  }
}

void WasmAudioBackend::Pause() {
  if (!initialized_ || !audio_context_) {
    return;
  }

  playing_ = false;
  wasm_audio_pause(audio_context_);
}

void WasmAudioBackend::Stop() {
  if (!initialized_ || !audio_context_) {
    return;
  }

  playing_ = false;
  wasm_audio_stop(audio_context_);

  // Clear internal queue
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    while (!sample_queue_.empty()) {
      sample_queue_.pop();
    }
  }

  queued_samples_ = 0;
  has_underrun_ = false;
}

void WasmAudioBackend::Clear() {
  if (!initialized_ || !audio_context_) {
    return;
  }

  wasm_audio_clear(audio_context_);

  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    while (!sample_queue_.empty()) {
      sample_queue_.pop();
    }
  }

  queued_samples_ = 0;
}

bool WasmAudioBackend::QueueSamples(const int16_t* samples, int num_samples) {
  if (!initialized_ || !audio_context_) {
    return false;
  }

  // Convert 16-bit PCM to float32 for WebAudio
  const int frame_count = num_samples / config_.channels;
  conversion_buffer_.resize(num_samples);

  if (!ConvertToFloat32(samples, conversion_buffer_.data(), num_samples)) {
    return false;
  }

  // Apply volume
  ApplyVolumeToBuffer(conversion_buffer_.data(), num_samples);

  // Queue samples to WebAudio
  wasm_audio_queue_samples(audio_context_, conversion_buffer_.data(),
                          frame_count, config_.channels);

  queued_samples_ += num_samples;
  total_samples_played_ += num_samples;

  return true;
}

bool WasmAudioBackend::QueueSamples(const float* samples, int num_samples) {
  if (!initialized_ || !audio_context_) {
    return false;
  }

  // Copy and apply volume
  conversion_buffer_.resize(num_samples);
  std::memcpy(conversion_buffer_.data(), samples, num_samples * sizeof(float));
  ApplyVolumeToBuffer(conversion_buffer_.data(), num_samples);

  const int frame_count = num_samples / config_.channels;
  wasm_audio_queue_samples(audio_context_, conversion_buffer_.data(),
                          frame_count, config_.channels);

  queued_samples_ += num_samples;
  total_samples_played_ += num_samples;

  return true;
}

bool WasmAudioBackend::QueueSamplesNative(const int16_t* samples, int frames_per_channel,
                                          int channels, int native_rate) {
  if (!initialized_ || !audio_context_) {
    return false;
  }

  // For now, just use the regular queue function
  // TODO: Implement proper resampling if native_rate != config_.sample_rate
  return QueueSamples(samples, frames_per_channel * channels);
}

AudioStatus WasmAudioBackend::GetStatus() const {
  AudioStatus status;

  if (!initialized_ || !audio_context_) {
    return status;
  }

  status.is_playing = wasm_audio_is_playing(audio_context_);
  status.queued_frames = wasm_audio_get_queued_frames(audio_context_);
  status.queued_bytes = status.queued_frames * config_.channels *
                       (config_.format == SampleFormat::INT16 ? 2 : 4);
  status.has_underrun = has_underrun_;

  return status;
}

bool WasmAudioBackend::IsInitialized() const {
  return initialized_;
}

AudioConfig WasmAudioBackend::GetConfig() const {
  return config_;
}

void WasmAudioBackend::SetVolume(float volume) {
  volume_ = std::max(0.0f, std::min(1.0f, volume));

  if (initialized_ && audio_context_) {
    wasm_audio_set_volume(audio_context_, volume_);
  }
}

float WasmAudioBackend::GetVolume() const {
  if (initialized_ && audio_context_) {
    return wasm_audio_get_volume(audio_context_);
  }
  return volume_;
}

void WasmAudioBackend::SetAudioStreamResampling(bool enable, int native_rate, int channels) {
  resampling_enabled_ = enable;
  native_rate_ = native_rate;
  native_channels_ = channels;
}

void WasmAudioBackend::HandleUserInteraction() {
  if (initialized_ && audio_context_) {
    wasm_audio_resume_context(audio_context_);
    context_suspended_ = false;
  }
}

bool WasmAudioBackend::IsContextSuspended() const {
  if (initialized_ && audio_context_) {
    return wasm_audio_is_suspended(audio_context_);
  }
  return true;
}

bool WasmAudioBackend::ConvertToFloat32(const int16_t* input, float* output, int num_samples) {
  if (!input || !output) {
    return false;
  }

  for (int i = 0; i < num_samples; ++i) {
    output[i] = static_cast<float>(input[i]) * kInt16ToFloat;
  }

  return true;
}

void WasmAudioBackend::ApplyVolumeToBuffer(float* buffer, int num_samples) {
  const float vol = volume_.load();
  if (vol == 1.0f) {
    return;  // No need to apply volume
  }

  for (int i = 0; i < num_samples; ++i) {
    buffer[i] *= vol;
  }
}

}  // namespace audio
}  // namespace emu
}  // namespace yaze

#endif  // __EMSCRIPTEN__