// sdl3_audio_backend_test.cc - Unit tests for SDL3 audio backend
// Tests the SDL3 audio backend implementation without requiring SDL3 runtime

#include <gtest/gtest.h>

#ifdef YAZE_USE_SDL3

#include <cmath>
#include <vector>

#include "app/emu/audio/sdl3_audio_backend.h"

namespace yaze {
namespace emu {
namespace audio {
namespace {

// Test fixture for SDL3 audio backend tests
class SDL3AudioBackendTest : public ::testing::Test {
 protected:
  void SetUp() override {
    backend_ = std::make_unique<SDL3AudioBackend>();
  }

  void TearDown() override {
    if (backend_ && backend_->IsInitialized()) {
      backend_->Shutdown();
    }
  }

  // Generate a simple sine wave for testing
  std::vector<int16_t> GenerateSineWave(int sample_rate, float frequency,
                                        float duration_seconds) {
    int num_samples = static_cast<int>(sample_rate * duration_seconds);
    std::vector<int16_t> samples(num_samples);

    for (int i = 0; i < num_samples; ++i) {
      float t = static_cast<float>(i) / sample_rate;
      float value = std::sin(2.0f * M_PI * frequency * t);
      samples[i] = static_cast<int16_t>(value * 32767.0f);
    }

    return samples;
  }

  std::unique_ptr<SDL3AudioBackend> backend_;
};

// Test basic initialization and shutdown
TEST_F(SDL3AudioBackendTest, InitializeAndShutdown) {
  AudioConfig config;
  config.sample_rate = 48000;
  config.channels = 2;
  config.buffer_frames = 1024;
  config.format = SampleFormat::INT16;

  EXPECT_FALSE(backend_->IsInitialized());

  // Note: This test will fail if SDL3 is not available at runtime
  // We'll mark it as optional/skippable
  if (!SDL_WasInit(SDL_INIT_AUDIO)) {
    GTEST_SKIP() << "SDL3 audio not available, skipping test";
  }

  EXPECT_TRUE(backend_->Initialize(config));
  EXPECT_TRUE(backend_->IsInitialized());
  EXPECT_EQ(backend_->GetBackendName(), "SDL3");

  backend_->Shutdown();
  EXPECT_FALSE(backend_->IsInitialized());
}

// Test configuration retrieval
TEST_F(SDL3AudioBackendTest, GetConfiguration) {
  AudioConfig config;
  config.sample_rate = 44100;
  config.channels = 2;
  config.buffer_frames = 512;
  config.format = SampleFormat::INT16;

  if (!SDL_WasInit(SDL_INIT_AUDIO)) {
    GTEST_SKIP() << "SDL3 audio not available, skipping test";
  }

  ASSERT_TRUE(backend_->Initialize(config));

  AudioConfig retrieved = backend_->GetConfig();
  // Note: Actual values might differ from requested
  EXPECT_GT(retrieved.sample_rate, 0);
  EXPECT_GT(retrieved.channels, 0);
  EXPECT_GT(retrieved.buffer_frames, 0);
}

// Test volume control
TEST_F(SDL3AudioBackendTest, VolumeControl) {
  EXPECT_EQ(backend_->GetVolume(), 1.0f);

  backend_->SetVolume(0.5f);
  EXPECT_EQ(backend_->GetVolume(), 0.5f);

  backend_->SetVolume(-0.1f);  // Should clamp to 0
  EXPECT_EQ(backend_->GetVolume(), 0.0f);

  backend_->SetVolume(1.5f);  // Should clamp to 1
  EXPECT_EQ(backend_->GetVolume(), 1.0f);
}

// Test audio queueing (INT16)
TEST_F(SDL3AudioBackendTest, QueueSamplesInt16) {
  AudioConfig config;
  config.sample_rate = 48000;
  config.channels = 2;
  config.buffer_frames = 1024;
  config.format = SampleFormat::INT16;

  if (!SDL_WasInit(SDL_INIT_AUDIO)) {
    GTEST_SKIP() << "SDL3 audio not available, skipping test";
  }

  ASSERT_TRUE(backend_->Initialize(config));

  // Generate test audio
  auto samples = GenerateSineWave(48000, 440.0f, 0.1f);  // 440Hz for 0.1s

  // Queue the samples
  EXPECT_TRUE(backend_->QueueSamples(samples.data(), samples.size()));

  // Check status
  AudioStatus status = backend_->GetStatus();
  EXPECT_GT(status.queued_bytes, 0);
}

// Test audio queueing (float)
TEST_F(SDL3AudioBackendTest, QueueSamplesFloat) {
  AudioConfig config;
  config.sample_rate = 48000;
  config.channels = 2;
  config.buffer_frames = 1024;
  config.format = SampleFormat::FLOAT32;

  if (!SDL_WasInit(SDL_INIT_AUDIO)) {
    GTEST_SKIP() << "SDL3 audio not available, skipping test";
  }

  ASSERT_TRUE(backend_->Initialize(config));

  // Generate float samples
  std::vector<float> samples(4800);  // 0.1 second at 48kHz
  for (size_t i = 0; i < samples.size(); ++i) {
    float t = static_cast<float>(i) / 48000.0f;
    samples[i] = std::sin(2.0f * M_PI * 440.0f * t);  // 440Hz sine wave
  }

  // Queue the samples
  EXPECT_TRUE(backend_->QueueSamples(samples.data(), samples.size()));

  // Check status
  AudioStatus status = backend_->GetStatus();
  EXPECT_GT(status.queued_bytes, 0);
}

// Test playback control
TEST_F(SDL3AudioBackendTest, PlaybackControl) {
  AudioConfig config;
  config.sample_rate = 48000;
  config.channels = 2;
  config.buffer_frames = 1024;
  config.format = SampleFormat::INT16;

  if (!SDL_WasInit(SDL_INIT_AUDIO)) {
    GTEST_SKIP() << "SDL3 audio not available, skipping test";
  }

  ASSERT_TRUE(backend_->Initialize(config));

  // Initially should be playing (auto-started)
  AudioStatus status = backend_->GetStatus();
  EXPECT_TRUE(status.is_playing);

  // Test pause
  backend_->Pause();
  status = backend_->GetStatus();
  EXPECT_FALSE(status.is_playing);

  // Test resume
  backend_->Play();
  status = backend_->GetStatus();
  EXPECT_TRUE(status.is_playing);

  // Test stop (should clear and pause)
  backend_->Stop();
  status = backend_->GetStatus();
  EXPECT_FALSE(status.is_playing);
  EXPECT_EQ(status.queued_bytes, 0);
}

// Test clear functionality
TEST_F(SDL3AudioBackendTest, ClearQueue) {
  AudioConfig config;
  config.sample_rate = 48000;
  config.channels = 2;
  config.buffer_frames = 1024;
  config.format = SampleFormat::INT16;

  if (!SDL_WasInit(SDL_INIT_AUDIO)) {
    GTEST_SKIP() << "SDL3 audio not available, skipping test";
  }

  ASSERT_TRUE(backend_->Initialize(config));

  // Queue some samples
  auto samples = GenerateSineWave(48000, 440.0f, 0.1f);
  ASSERT_TRUE(backend_->QueueSamples(samples.data(), samples.size()));

  // Verify samples were queued
  AudioStatus status = backend_->GetStatus();
  EXPECT_GT(status.queued_bytes, 0);

  // Clear the queue
  backend_->Clear();

  // Verify queue is empty
  status = backend_->GetStatus();
  EXPECT_EQ(status.queued_bytes, 0);
}

// Test resampling support
TEST_F(SDL3AudioBackendTest, ResamplingSupport) {
  EXPECT_TRUE(backend_->SupportsAudioStream());

  AudioConfig config;
  config.sample_rate = 48000;
  config.channels = 2;
  config.buffer_frames = 1024;
  config.format = SampleFormat::INT16;

  if (!SDL_WasInit(SDL_INIT_AUDIO)) {
    GTEST_SKIP() << "SDL3 audio not available, skipping test";
  }

  ASSERT_TRUE(backend_->Initialize(config));

  // Enable resampling for 32kHz native rate
  backend_->SetAudioStreamResampling(true, 32000, 2);

  // Generate samples at native rate
  auto samples = GenerateSineWave(32000, 440.0f, 0.1f);

  // Queue native rate samples
  EXPECT_TRUE(backend_->QueueSamplesNative(samples.data(),
                                           samples.size() / 2, 2, 32000));
}

// Test double initialization
TEST_F(SDL3AudioBackendTest, DoubleInitialization) {
  AudioConfig config;
  config.sample_rate = 48000;
  config.channels = 2;
  config.buffer_frames = 1024;
  config.format = SampleFormat::INT16;

  if (!SDL_WasInit(SDL_INIT_AUDIO)) {
    GTEST_SKIP() << "SDL3 audio not available, skipping test";
  }

  ASSERT_TRUE(backend_->Initialize(config));
  EXPECT_TRUE(backend_->IsInitialized());

  // Second initialization should reinitialize
  config.sample_rate = 44100;  // Different rate
  EXPECT_TRUE(backend_->Initialize(config));
  EXPECT_TRUE(backend_->IsInitialized());

  AudioConfig retrieved = backend_->GetConfig();
  // Should have the new configuration (or device's actual rate)
  EXPECT_GT(retrieved.sample_rate, 0);
}

}  // namespace
}  // namespace audio
}  // namespace emu
}  // namespace yaze

#endif  // YAZE_USE_SDL3