// Audio Timing Tests for yaze MusicEditor
//
// These tests verify the APU and DSP timing accuracy to diagnose
// and prevent audio playback speed issues (e.g., 1.5x speed bug).
//
// All tests are ROM-dependent to ensure realistic audio driver behavior.

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <memory>

#include "app/emu/audio/apu.h"
#include "app/emu/audio/dsp.h"
#include "app/emu/memory/memory.h"
#include "app/emu/snes.h"
#include "rom/rom.h"
#include "test_utils.h"
#include "util/log.h"

namespace yaze {
namespace test {

// =============================================================================
// Audio Timing Constants
// =============================================================================
namespace audio_constants {

// SNES master clock frequency (NTSC)
constexpr uint64_t kMasterClock = 21477272;

// APU clock frequency (~1.024 MHz)
// Derived from: (32040 * 32) = 1,025,280 Hz
constexpr uint64_t kApuClock = 1025280;

// DSP native sample rate
constexpr int kNativeSampleRate = 32040;

// NTSC frame rate
constexpr double kNtscFrameRate = 60.0988;

// Master cycles per NTSC frame
constexpr uint64_t kMasterCyclesPerFrame = 357366;  // 21477272 / 60.0988

// Expected samples per NTSC frame
constexpr int kSamplesPerFrame = 533;  // 32040 / 60.0988

// APU/Master clock ratio numerator and denominator (from apu.cc)
constexpr uint64_t kApuCyclesNumerator = 32040 * 32;      // 1,025,280
constexpr uint64_t kApuCyclesDenominator = 1364 * 262 * 60;  // 21,437,280

// Tolerance percentages for timing tests
constexpr double kApuCycleRateTolerance = 0.01;    // 1%
constexpr double kDspSampleRateTolerance = 0.005;  // 0.5%
constexpr int kSamplesPerFrameTolerance = 2;       // +/- 2 samples

}  // namespace audio_constants

// =============================================================================
// Audio Timing Test Fixture
// =============================================================================

class AudioTimingTest : public TestRomManager::BoundRomTest {
 protected:
  void SetUp() override {
    BoundRomTest::SetUp();
    if (!rom_available()) {
      return;
    }

    // Reset cumulative cycle counter for each test
    cumulative_master_cycles_ = 0;

    // Initialize SNES with ROM
    snes_ = std::make_unique<emu::Snes>();
    snes_->Init(rom()->vector());

    // Get reference to APU
    apu_ = &snes_->apu();

    // Reset APU cycle tracking to ensure fresh start for timing tests
    // Snes::Init() runs bootstrap cycles which advances the APU's
    // last_master_cycles_, so we need to reset it for our tests.
    apu_->Reset();
  }

  void TearDown() override {
    apu_ = nullptr;
    snes_.reset();
    BoundRomTest::TearDown();
  }

  // Run APU for a specified number of master clock cycles
  // Returns the number of APU cycles actually executed
  uint64_t RunApuForMasterCycles(uint64_t master_cycles) {
    uint64_t apu_before = apu_->GetCycles();
    // APU expects cumulative master cycles
    cumulative_master_cycles_ += master_cycles;
    apu_->RunCycles(cumulative_master_cycles_);
    return apu_->GetCycles() - apu_before;
  }

  // Get current DSP sample offset (for counting samples)
  uint32_t GetDspSampleOffset() const {
    return apu_->dsp().GetSampleOffset();
  }

  // Count samples generated over a number of frames
  int CountSamplesOverFrames(int frame_count) {
    uint32_t start_offset = GetDspSampleOffset();

    for (int i = 0; i < frame_count; ++i) {
      // APU expects cumulative master cycles, not per-frame delta
      cumulative_master_cycles_ += audio_constants::kMasterCyclesPerFrame;
      apu_->RunCycles(cumulative_master_cycles_);
    }

    uint32_t end_offset = GetDspSampleOffset();

    // Handle wrap-around (DSP buffer is 2048 samples with 0x7ff mask)
    constexpr uint32_t kBufferSize = 2048;
    if (end_offset >= start_offset) {
      return end_offset - start_offset;
    } else {
      return (kBufferSize - start_offset) + end_offset;
    }
  }

  // Track cumulative master cycles for APU calls
  uint64_t cumulative_master_cycles_ = 0;

  std::unique_ptr<emu::Snes> snes_;
  emu::Apu* apu_ = nullptr;
};

// =============================================================================
// Core APU Timing Tests
// =============================================================================

TEST_F(AudioTimingTest, ApuCycleRateMatchesExpected) {
  // Run APU for 1 second worth of master clock cycles
  constexpr uint64_t kOneSecondMasterCycles = audio_constants::kMasterClock;

  uint64_t apu_cycles = RunApuForMasterCycles(kOneSecondMasterCycles);

  // Expected APU cycles: ~1,024,000
  constexpr uint64_t kExpectedApuCycles = audio_constants::kApuClock;
  const double ratio =
      static_cast<double>(apu_cycles) / static_cast<double>(kExpectedApuCycles);

  // Log results for debugging
  LOG_INFO("AudioTiming",
           "APU cycles in 1 second: %llu (expected: %llu, ratio: %.4f)",
           apu_cycles, kExpectedApuCycles, ratio);

  // Verify within 1% tolerance
  EXPECT_NEAR(ratio, 1.0, audio_constants::kApuCycleRateTolerance)
      << "APU cycle rate mismatch! Got " << apu_cycles << " cycles, expected ~"
      << kExpectedApuCycles << " (ratio: " << ratio << ")";
}

TEST_F(AudioTimingTest, DspSampleRateMatchesNative) {
  // Run APU for 1 second and count DSP samples
  constexpr int kTestFrames = 60;  // ~1 second at 60fps

  int total_samples = CountSamplesOverFrames(kTestFrames);

  // Expected: ~32,040 samples
  constexpr int kExpectedSamples = audio_constants::kNativeSampleRate;
  const double ratio =
      static_cast<double>(total_samples) / static_cast<double>(kExpectedSamples);

  LOG_INFO("AudioTiming",
           "DSP samples in %d frames: %d (expected: %d, ratio: %.4f)",
           kTestFrames, total_samples, kExpectedSamples, ratio);

  EXPECT_NEAR(ratio, 1.0, audio_constants::kDspSampleRateTolerance)
      << "DSP sample rate mismatch! Got " << total_samples
      << " samples, expected ~" << kExpectedSamples << " (ratio: " << ratio
      << ")";
}

TEST_F(AudioTimingTest, FrameProducesCorrectSampleCount) {
  // Run exactly one NTSC frame
  uint32_t start_offset = GetDspSampleOffset();
  apu_->RunCycles(audio_constants::kMasterCyclesPerFrame);
  uint32_t end_offset = GetDspSampleOffset();

  int samples = (end_offset >= start_offset)
                    ? (end_offset - start_offset)
                    : (2048 - start_offset + end_offset);

  LOG_INFO("AudioTiming", "Samples per frame: %d (expected: %d +/- %d)", samples,
           audio_constants::kSamplesPerFrame,
           audio_constants::kSamplesPerFrameTolerance);

  EXPECT_NEAR(samples, audio_constants::kSamplesPerFrame,
              audio_constants::kSamplesPerFrameTolerance)
      << "Frame sample count mismatch! Got " << samples << " samples";
}

TEST_F(AudioTimingTest, MultipleFramesAccumulateSamplesCorrectly) {
  constexpr int kTestFrames = 60;
  constexpr int kExpectedTotal =
      audio_constants::kSamplesPerFrame * kTestFrames;

  int total_samples = CountSamplesOverFrames(kTestFrames);

  LOG_INFO("AudioTiming", "Total samples in %d frames: %d (expected: ~%d)",
           kTestFrames, total_samples, kExpectedTotal);

  // Allow 1% tolerance for accumulated drift
  const double ratio =
      static_cast<double>(total_samples) / static_cast<double>(kExpectedTotal);
  EXPECT_NEAR(ratio, 1.0, 0.01)
      << "Accumulated sample count mismatch over " << kTestFrames << " frames";
}

TEST_F(AudioTimingTest, ApuMasterClockRatioIsCorrect) {
  // Verify the fixed-point ratio used in APU::RunCycles
  constexpr double kExpectedRatio =
      static_cast<double>(audio_constants::kApuCyclesNumerator) /
      static_cast<double>(audio_constants::kApuCyclesDenominator);

  LOG_INFO("AudioTiming", "APU/Master ratio: %.6f (num=%llu, den=%llu)",
           kExpectedRatio, audio_constants::kApuCyclesNumerator,
           audio_constants::kApuCyclesDenominator);

  // Run a small test to verify actual ratio matches expected
  constexpr uint64_t kTestMasterCycles = 1000000;  // 1M master cycles
  uint64_t apu_cycles = RunApuForMasterCycles(kTestMasterCycles);

  double actual_ratio =
      static_cast<double>(apu_cycles) / static_cast<double>(kTestMasterCycles);

  EXPECT_NEAR(actual_ratio, kExpectedRatio, 0.0001)
      << "APU/Master ratio mismatch! Actual: " << actual_ratio
      << ", Expected: " << kExpectedRatio;
}

TEST_F(AudioTimingTest, DspCyclesEvery32ApuCycles) {
  // The DSP should cycle once every 32 APU cycles (from apu.cc:246)
  // This is verified by checking sample generation rate

  // Run 32000 APU cycles (should produce 1000 DSP cycles = 1000 samples)
  uint64_t start_apu = apu_->GetCycles();
  uint32_t start_samples = GetDspSampleOffset();

  // We need to run enough master cycles to get 32000 APU cycles
  // APU cycles = master * (1025280 / 21437280) ≈ master * 0.0478
  // So master = 32000 / 0.0478 ≈ 669456
  constexpr uint64_t kTargetApuCycles = 32000;
  constexpr uint64_t kMasterCycles =
      (kTargetApuCycles * audio_constants::kApuCyclesDenominator) /
      audio_constants::kApuCyclesNumerator;

  apu_->RunCycles(kMasterCycles);

  uint64_t end_apu = apu_->GetCycles();
  uint32_t end_samples = GetDspSampleOffset();

  uint64_t apu_delta = end_apu - start_apu;
  int sample_delta = (end_samples >= start_samples)
                         ? (end_samples - start_samples)
                         : (2048 - start_samples + end_samples);

  // Expected: 1 sample per 32 APU cycles
  double cycles_per_sample = static_cast<double>(apu_delta) / sample_delta;

  LOG_INFO("AudioTiming",
           "APU cycles per DSP sample: %.2f (expected: 32.0), samples=%d, "
           "apu_cycles=%llu",
           cycles_per_sample, sample_delta, apu_delta);

  EXPECT_NEAR(cycles_per_sample, 32.0, 0.5)
      << "DSP not cycling every 32 APU cycles!";
}

// =============================================================================
// Regression Tests for 1.5x Speed Bug
// =============================================================================

TEST_F(AudioTimingTest, PlaybackSpeedRegression_NotTooFast) {
  // This test verifies that audio doesn't play at 1.5x speed
  // If the bug is present, we'd see ~47,700 samples instead of ~32,040

  constexpr int kTestFrames = 60;  // 1 second
  int total_samples = CountSamplesOverFrames(kTestFrames);

  // At 1.5x speed, we'd get ~48,060 samples
  constexpr int kBuggySpeed15x = 48060;

  // Verify we're NOT close to the 1.5x buggy value
  double speed_ratio =
      static_cast<double>(total_samples) / audio_constants::kNativeSampleRate;

  LOG_INFO("AudioTiming",
           "Speed check: %d samples in 1 second (ratio: %.2fx, 1.0x expected)",
           total_samples, speed_ratio);

  // If speed is >= 1.3x, something is wrong
  EXPECT_LT(speed_ratio, 1.3)
      << "Audio playback is too fast! Speed ratio: " << speed_ratio
      << "x (samples: " << total_samples << ", expected: ~32040)";

  // Speed should be close to 1.0x
  EXPECT_GT(speed_ratio, 0.9) << "Audio playback is too slow!";
}

// =============================================================================
// Extended Timing Stability Tests
// =============================================================================

TEST_F(AudioTimingTest, NoCycleDriftOver60Seconds) {
  // Run for 60 seconds of simulated time and check for drift
  constexpr int kTestSeconds = 60;
  constexpr int kFramesPerSecond = 60;

  uint64_t cumulative_apu_cycles = 0;
  int cumulative_samples = 0;

  for (int sec = 0; sec < kTestSeconds; ++sec) {
    uint64_t apu_before = apu_->GetCycles();
    int samples_before = GetDspSampleOffset();

    // Run one second of frames
    // APU expects cumulative master cycles, not per-frame delta
    for (int frame = 0; frame < kFramesPerSecond; ++frame) {
      cumulative_master_cycles_ += audio_constants::kMasterCyclesPerFrame;
      apu_->RunCycles(cumulative_master_cycles_);
    }

    uint64_t apu_after = apu_->GetCycles();
    int samples_after = GetDspSampleOffset();

    cumulative_apu_cycles += (apu_after - apu_before);
    int sample_delta = (samples_after >= samples_before)
                           ? (samples_after - samples_before)
                           : (2048 - samples_before + samples_after);
    cumulative_samples += sample_delta;
  }

  // After 60 seconds, we should have very close to expected values
  constexpr uint64_t kExpectedApuCycles =
      audio_constants::kApuClock * kTestSeconds;
  constexpr int kExpectedSamples =
      audio_constants::kNativeSampleRate * kTestSeconds;

  double apu_ratio = static_cast<double>(cumulative_apu_cycles) / kExpectedApuCycles;
  double sample_ratio = static_cast<double>(cumulative_samples) / kExpectedSamples;

  LOG_INFO("AudioTiming",
           "60-second drift test: APU ratio=%.6f, Sample ratio=%.6f",
           apu_ratio, sample_ratio);

  // Very tight tolerance for extended test - no drift should accumulate
  EXPECT_NEAR(apu_ratio, 1.0, 0.001)
      << "APU cycle drift detected over 60 seconds!";
  EXPECT_NEAR(sample_ratio, 1.0, 0.005)
      << "Sample count drift detected over 60 seconds!";
}

}  // namespace test
}  // namespace yaze
