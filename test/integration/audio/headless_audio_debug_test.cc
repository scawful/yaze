// Headless Audio Debug Tests
//
// Comprehensive audio debugging tests for diagnosing timing issues.
// Collects timing metrics and verifies audio pipeline correctness.

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include <gtest/gtest.h>

#include <chrono>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <vector>

#include "app/emu/audio/apu.h"
#include "app/emu/audio/dsp.h"
#include "app/emu/snes.h"
#include "rom/rom.h"
#include "test_utils.h"
#include "util/log.h"

namespace yaze {
namespace test {

// =============================================================================
// Timing Metrics Structure
// =============================================================================

struct AudioTimingMetrics {
  // Cycle counts
  uint64_t total_master_cycles = 0;
  uint64_t total_apu_cycles = 0;
  uint64_t total_dsp_samples = 0;

  // Rates (calculated)
  double apu_cycles_per_second = 0.0;
  double dsp_samples_per_second = 0.0;
  double apu_to_master_ratio = 0.0;

  // Per-frame statistics
  double samples_per_frame_avg = 0.0;
  int samples_per_frame_min = INT_MAX;
  int samples_per_frame_max = 0;

  // Drift detection
  std::vector<double> per_second_apu_rates;
  std::vector<double> per_second_sample_rates;
  double max_drift_percent = 0.0;

  // Expected values for comparison
  static constexpr uint64_t kExpectedApuCyclesPerSecond = 1025280;
  static constexpr int kExpectedSamplesPerSecond = 32040;
  static constexpr int kExpectedSamplesPerFrame = 533;
  static constexpr double kExpectedApuMasterRatio = 0.0478;

  std::string ToString() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4);
    oss << "=== Audio Timing Metrics ===\n";
    oss << "Master cycles: " << total_master_cycles << "\n";
    oss << "APU cycles: " << total_apu_cycles
        << " (expected/sec: " << kExpectedApuCyclesPerSecond << ")\n";
    oss << "DSP samples: " << total_dsp_samples
        << " (expected/sec: " << kExpectedSamplesPerSecond << ")\n";
    oss << "\n";
    oss << "APU cycles/sec: " << apu_cycles_per_second
        << " (ratio to expected: "
        << (apu_cycles_per_second / kExpectedApuCyclesPerSecond) << ")\n";
    oss << "DSP samples/sec: " << dsp_samples_per_second
        << " (ratio to expected: "
        << (dsp_samples_per_second / kExpectedSamplesPerSecond) << ")\n";
    oss << "APU/Master ratio: " << apu_to_master_ratio
        << " (expected: " << kExpectedApuMasterRatio << ")\n";
    oss << "\n";
    oss << "Samples/frame: avg=" << samples_per_frame_avg
        << ", min=" << samples_per_frame_min << ", max=" << samples_per_frame_max
        << " (expected: " << kExpectedSamplesPerFrame << ")\n";
    oss << "Max drift: " << (max_drift_percent * 100.0) << "%\n";
    return oss.str();
  }
};

// =============================================================================
// Headless Audio Debug Test Fixture
// =============================================================================

class HeadlessAudioDebugTest : public TestRomManager::BoundRomTest {
 protected:
  void SetUp() override {
    BoundRomTest::SetUp();
    if (!rom_available()) {
      return;
    }

    snes_ = std::make_unique<emu::Snes>();
    snes_->Init(rom()->vector());
    apu_ = &snes_->apu();

    // Reset APU cycle tracking for fresh start
    // Snes::Init() runs bootstrap cycles which advances the APU's
    // last_master_cycles_, so we need to reset for accurate timing tests.
    apu_->Reset();
  }

  void TearDown() override {
    apu_ = nullptr;
    snes_.reset();
    BoundRomTest::TearDown();
  }

  // Collect timing metrics over specified duration (in simulated seconds)
  AudioTimingMetrics CollectMetrics(int duration_seconds) {
    AudioTimingMetrics metrics;

    constexpr int kFramesPerSecond = 60;
    constexpr uint64_t kMasterCyclesPerFrame = 357366;

    uint64_t start_apu = apu_->GetCycles();
    uint32_t start_samples = apu_->dsp().GetSampleOffset();

    // Track cumulative master cycles (APU expects monotonically increasing values)
    uint64_t cumulative_master_cycles = 0;

    for (int sec = 0; sec < duration_seconds; ++sec) {
      uint64_t sec_start_apu = apu_->GetCycles();
      uint32_t sec_start_samples = apu_->dsp().GetSampleOffset();

      int sec_samples_min = INT_MAX;
      int sec_samples_max = 0;
      int sec_total_samples = 0;

      for (int frame = 0; frame < kFramesPerSecond; ++frame) {
        uint32_t frame_start = apu_->dsp().GetSampleOffset();

        // APU expects cumulative master cycles, not per-frame delta
        cumulative_master_cycles += kMasterCyclesPerFrame;
        apu_->RunCycles(cumulative_master_cycles);
        metrics.total_master_cycles += kMasterCyclesPerFrame;

        uint32_t frame_end = apu_->dsp().GetSampleOffset();
        int frame_samples = (frame_end >= frame_start)
                                ? (frame_end - frame_start)
                                : (2048 - frame_start + frame_end);

        sec_total_samples += frame_samples;
        sec_samples_min = std::min(sec_samples_min, frame_samples);
        sec_samples_max = std::max(sec_samples_max, frame_samples);
        metrics.samples_per_frame_min =
            std::min(metrics.samples_per_frame_min, frame_samples);
        metrics.samples_per_frame_max =
            std::max(metrics.samples_per_frame_max, frame_samples);
      }

      uint64_t sec_end_apu = apu_->GetCycles();
      uint64_t sec_apu_delta = sec_end_apu - sec_start_apu;

      double sec_apu_rate = static_cast<double>(sec_apu_delta);
      double sec_sample_rate = static_cast<double>(sec_total_samples);

      metrics.per_second_apu_rates.push_back(sec_apu_rate);
      metrics.per_second_sample_rates.push_back(sec_sample_rate);

      // Track max drift from expected
      double apu_drift =
          std::abs(sec_apu_rate - AudioTimingMetrics::kExpectedApuCyclesPerSecond) /
          AudioTimingMetrics::kExpectedApuCyclesPerSecond;
      double sample_drift =
          std::abs(sec_sample_rate - AudioTimingMetrics::kExpectedSamplesPerSecond) /
          AudioTimingMetrics::kExpectedSamplesPerSecond;
      metrics.max_drift_percent =
          std::max(metrics.max_drift_percent, std::max(apu_drift, sample_drift));
    }

    uint64_t end_apu = apu_->GetCycles();
    uint32_t end_samples = apu_->dsp().GetSampleOffset();

    metrics.total_apu_cycles = end_apu - start_apu;
    metrics.total_dsp_samples = (end_samples >= start_samples)
                                     ? (end_samples - start_samples)
                                     : (2048 - start_samples + end_samples);

    // For long tests, we need to track cumulative samples differently
    // since the ring buffer wraps. Use per-second totals instead.
    if (duration_seconds > 0) {
      double total_samples_from_rates = 0;
      for (double rate : metrics.per_second_sample_rates) {
        total_samples_from_rates += rate;
      }
      metrics.total_dsp_samples = static_cast<uint64_t>(total_samples_from_rates);
    }

    // Calculate rates
    metrics.apu_cycles_per_second =
        static_cast<double>(metrics.total_apu_cycles) / duration_seconds;
    metrics.dsp_samples_per_second =
        static_cast<double>(metrics.total_dsp_samples) / duration_seconds;
    metrics.apu_to_master_ratio =
        static_cast<double>(metrics.total_apu_cycles) / metrics.total_master_cycles;

    // Calculate per-frame average
    int total_frames = duration_seconds * kFramesPerSecond;
    metrics.samples_per_frame_avg =
        static_cast<double>(metrics.total_dsp_samples) / total_frames;

    return metrics;
  }

  void LogMetricsToFile(const AudioTimingMetrics& metrics,
                        const std::string& filename) {
    std::ofstream file(filename);
    if (!file) {
      LOG_ERROR("AudioDebug", "Failed to open metrics file: %s",
                filename.c_str());
      return;
    }

    file << metrics.ToString();

    // CSV data for analysis
    file << "\n=== Per-Second Data (CSV) ===\n";
    file << "second,apu_cycles,dsp_samples,apu_ratio,sample_ratio\n";
    for (size_t i = 0; i < metrics.per_second_apu_rates.size(); ++i) {
      file << i << "," << metrics.per_second_apu_rates[i] << ","
           << metrics.per_second_sample_rates[i] << ","
           << (metrics.per_second_apu_rates[i] /
               AudioTimingMetrics::kExpectedApuCyclesPerSecond)
           << ","
           << (metrics.per_second_sample_rates[i] /
               AudioTimingMetrics::kExpectedSamplesPerSecond)
           << "\n";
    }

    file.close();
    LOG_INFO("AudioDebug", "Metrics written to %s", filename.c_str());
  }

  std::unique_ptr<emu::Snes> snes_;
  emu::Apu* apu_ = nullptr;
};

// =============================================================================
// Comprehensive Diagnostic Tests
// =============================================================================

TEST_F(HeadlessAudioDebugTest, FullTimingDiagnostic) {
  // Run 10 seconds of simulated playback and collect all metrics
  constexpr int kTestDurationSeconds = 10;

  LOG_INFO("AudioDebug", "Starting %d-second timing diagnostic...",
           kTestDurationSeconds);

  AudioTimingMetrics metrics = CollectMetrics(kTestDurationSeconds);

  // Log full metrics
  LOG_INFO("AudioDebug", "\n%s", metrics.ToString().c_str());

  // Verify APU cycle rate
  double apu_ratio =
      metrics.apu_cycles_per_second / AudioTimingMetrics::kExpectedApuCyclesPerSecond;
  EXPECT_NEAR(apu_ratio, 1.0, 0.01)
      << "APU cycle rate should be within 1% of expected. "
      << "Got " << metrics.apu_cycles_per_second << " cycles/sec";

  // Verify DSP sample rate
  double sample_ratio =
      metrics.dsp_samples_per_second / AudioTimingMetrics::kExpectedSamplesPerSecond;
  EXPECT_NEAR(sample_ratio, 1.0, 0.01)
      << "DSP sample rate should be within 1% of expected. "
      << "Got " << metrics.dsp_samples_per_second << " samples/sec";

  // Verify samples per frame
  EXPECT_NEAR(metrics.samples_per_frame_avg,
              AudioTimingMetrics::kExpectedSamplesPerFrame, 2.0)
      << "Samples per frame should be ~533";

  // Verify no significant drift
  EXPECT_LT(metrics.max_drift_percent, 0.02)
      << "Max drift should be < 2%";
}

TEST_F(HeadlessAudioDebugTest, CycleRateDriftOverTime) {
  // Run extended simulation to detect timing drift
  constexpr int kTestDurationSeconds = 60;

  LOG_INFO("AudioDebug", "Starting %d-second drift detection test...",
           kTestDurationSeconds);

  AudioTimingMetrics metrics = CollectMetrics(kTestDurationSeconds);

  // Log to file for detailed analysis
  LogMetricsToFile(metrics, "/tmp/audio_timing_drift.txt");

  // Check for drift: compare first half to second half
  if (metrics.per_second_apu_rates.size() >= 2) {
    size_t half = metrics.per_second_apu_rates.size() / 2;

    double first_half_avg = 0;
    double second_half_avg = 0;

    for (size_t i = 0; i < half; ++i) {
      first_half_avg += metrics.per_second_apu_rates[i];
    }
    first_half_avg /= half;

    for (size_t i = half; i < metrics.per_second_apu_rates.size(); ++i) {
      second_half_avg += metrics.per_second_apu_rates[i];
    }
    second_half_avg /= (metrics.per_second_apu_rates.size() - half);

    double drift = std::abs(second_half_avg - first_half_avg) / first_half_avg;

    LOG_INFO("AudioDebug",
             "Drift analysis: first_half=%.0f, second_half=%.0f, drift=%.4f%%",
             first_half_avg, second_half_avg, drift * 100);

    EXPECT_LT(drift, 0.001)
        << "APU cycle rate should not drift over time. "
        << "First half avg: " << first_half_avg
        << ", Second half avg: " << second_half_avg;
  }

  // Overall timing should still be accurate
  double overall_ratio =
      metrics.apu_cycles_per_second / AudioTimingMetrics::kExpectedApuCyclesPerSecond;
  EXPECT_NEAR(overall_ratio, 1.0, 0.005)
      << "After 60 seconds, timing should be within 0.5% of expected";
}

TEST_F(HeadlessAudioDebugTest, SampleBufferDoesNotOverflow) {
  // Run continuous simulation and verify buffer wrapping works correctly
  constexpr int kTestFrames = 3600;  // 1 minute at 60fps

  uint32_t prev_offset = apu_->dsp().GetSampleOffset();
  int wrap_count = 0;

  for (int frame = 0; frame < kTestFrames; ++frame) {
    apu_->RunCycles(357366);  // One NTSC frame

    uint32_t curr_offset = apu_->dsp().GetSampleOffset();

    // Detect wrap-around
    if (curr_offset < prev_offset) {
      wrap_count++;
    }

    // Offset should always be within buffer bounds (0-2047)
    EXPECT_LT(curr_offset, 2048u)
        << "Sample offset exceeded buffer size at frame " << frame;

    prev_offset = curr_offset;
  }

  LOG_INFO("AudioDebug", "Buffer wrapped %d times in %d frames", wrap_count,
           kTestFrames);

  // With ~533 samples/frame and 2048 buffer size, we should wrap about
  // every 4 frames. In 3600 frames, expect ~900 wraps.
  EXPECT_GT(wrap_count, 800) << "Buffer should wrap regularly";
  EXPECT_LT(wrap_count, 1000) << "Buffer wrap count seems off";
}

// =============================================================================
// Speed Bug Regression Tests
// =============================================================================

TEST_F(HeadlessAudioDebugTest, NotPlayingAt15xSpeed) {
  // Specific test for the 1.5x speed bug
  constexpr int kTestDurationSeconds = 5;

  AudioTimingMetrics metrics = CollectMetrics(kTestDurationSeconds);

  // At 1.5x speed, we'd see ~48060 samples/sec instead of ~32040
  double speed_ratio =
      metrics.dsp_samples_per_second / AudioTimingMetrics::kExpectedSamplesPerSecond;

  LOG_INFO("AudioDebug", "Speed ratio: %.4fx (1.0x expected)", speed_ratio);

  // If bug is present, ratio would be ~1.5
  EXPECT_LT(speed_ratio, 1.3)
      << "Audio should not be playing at 1.5x speed! "
      << "Got " << metrics.dsp_samples_per_second << " samples/sec";

  EXPECT_GT(speed_ratio, 0.9)
      << "Audio should not be playing too slowly! "
      << "Got " << metrics.dsp_samples_per_second << " samples/sec";
}

TEST_F(HeadlessAudioDebugTest, ApuMasterRatioIsCorrect) {
  // Verify the fixed-point ratio calculation
  constexpr int kTestDurationSeconds = 5;

  AudioTimingMetrics metrics = CollectMetrics(kTestDurationSeconds);

  LOG_INFO("AudioDebug", "APU/Master ratio: %.6f (expected: ~0.0478)",
           metrics.apu_to_master_ratio);

  EXPECT_NEAR(metrics.apu_to_master_ratio, 0.0478, 0.001)
      << "APU/Master clock ratio is incorrect";
}

// =============================================================================
// Diagnostic Output Tests
// =============================================================================

TEST_F(HeadlessAudioDebugTest, GenerateTimingReport) {
  // Generate a comprehensive timing report for debugging
  constexpr int kTestDurationSeconds = 10;

  AudioTimingMetrics metrics = CollectMetrics(kTestDurationSeconds);

  std::string report = metrics.ToString();

  // Write to stdout for immediate visibility
  std::cout << "\n" << report << std::endl;

  // Also write to file
  LogMetricsToFile(metrics, "/tmp/audio_timing_report.txt");

  // This test always passes - it's for generating debug output
  SUCCEED() << "Timing report generated";
}

}  // namespace test
}  // namespace yaze
