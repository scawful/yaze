// test_sdl3_audio_compile.cc - Standalone compile test for SDL3 audio backend
// This verifies the SDL3 audio backend compiles without errors
// Compile with: g++ -std=c++17 -DYAZE_USE_SDL3 -I../../src -c test_sdl3_audio_compile.cc

// Mock the dependencies to test compilation
namespace yaze {
namespace log {
inline void Log(const char* level, const char* tag, const char* fmt, ...) {}
}  // namespace log
}  // namespace yaze

#define LOG_INFO(tag, ...) yaze::log::Log("INFO", tag, __VA_ARGS__)
#define LOG_WARN(tag, ...) yaze::log::Log("WARN", tag, __VA_ARGS__)
#define LOG_ERROR(tag, ...) yaze::log::Log("ERROR", tag, __VA_ARGS__)

// Define YAZE_USE_SDL3 to enable SDL3 backend
#define YAZE_USE_SDL3

// Include the actual implementation files to test compilation
#include "../../src/app/emu/audio/audio_backend.h"

// Mock SDL3 functions for compile test only
#ifdef YAZE_USE_SDL3

struct SDL_AudioSpec {
  int format;
  int channels;
  int freq;
};

struct SDL_AudioStream;
typedef unsigned int SDL_AudioDeviceID;
typedef int SDL_AudioFormat;

#define SDL_AUDIO_S16 0x8010
#define SDL_AUDIO_F32 0x8120
#define SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK 0

// Mock SDL3 functions
SDL_AudioStream* SDL_OpenAudioDeviceStream(SDL_AudioDeviceID id,
                                           const SDL_AudioSpec* spec,
                                           void* callback, void* userdata) {
  return nullptr;
}

SDL_AudioDeviceID SDL_GetAudioStreamDevice(SDL_AudioStream* stream) {
  return 0;
}

int SDL_GetAudioDeviceFormat(SDL_AudioDeviceID device, SDL_AudioSpec* spec,
                             int* sample_frames) {
  return 0;
}

int SDL_ResumeAudioDevice(SDL_AudioDeviceID device) {
  return 0;
}

int SDL_PauseAudioDevice(SDL_AudioDeviceID device) {
  return 0;
}

bool SDL_IsAudioDevicePaused(SDL_AudioDeviceID device) {
  return false;
}

void SDL_DestroyAudioStream(SDL_AudioStream* stream) {}

int SDL_PutAudioStreamData(SDL_AudioStream* stream, const void* buf, int len) {
  return 0;
}

int SDL_GetAudioStreamQueued(SDL_AudioStream* stream) {
  return 0;
}

int SDL_GetAudioStreamAvailable(SDL_AudioStream* stream) {
  return 0;
}

int SDL_GetAudioStreamData(SDL_AudioStream* stream, void* buf, int len) {
  return 0;
}

void SDL_ClearAudioStream(SDL_AudioStream* stream) {}

SDL_AudioStream* SDL_CreateAudioStream(const SDL_AudioSpec* src_spec,
                                       const SDL_AudioSpec* dst_spec) {
  return nullptr;
}

const char* SDL_GetError() {
  return "Mock error";
}

// Now include the SDL3 implementation
#include "../../src/app/emu/audio/sdl3_audio_backend.cc"

#endif  // YAZE_USE_SDL3

// Simple test to verify it compiles
int main() {
  #ifdef YAZE_USE_SDL3
  yaze::emu::audio::SDL3AudioBackend backend;

  yaze::emu::audio::AudioConfig config;
  config.sample_rate = 48000;
  config.channels = 2;
  config.buffer_frames = 1024;
  config.format = yaze::emu::audio::SampleFormat::INT16;

  // Just verify the interface is correct
  bool initialized = backend.IsInitialized();
  std::string name = backend.GetBackendName();
  float volume = backend.GetVolume();
  backend.SetVolume(0.5f);

  return (name == "SDL3") ? 0 : 1;
  #else
  return 0;
  #endif
}