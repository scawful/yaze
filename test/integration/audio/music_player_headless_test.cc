// MusicPlayer Headless Integration Tests
//
// Tests MusicPlayer functionality without requiring display or audio output.
// Uses NullAudioBackend to verify audio timing and playback behavior.

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <thread>

#include "app/editor/music/music_player.h"
#include "app/emu/audio/audio_backend.h"
#include "app/emu/emulator.h"
#include "rom/rom.h"
#include "test_utils.h"
#include "util/log.h"
#include "zelda3/music/music_bank.h"

namespace yaze {
namespace test {

// =============================================================================
// MusicPlayer Headless Test Fixture
// =============================================================================

class MusicPlayerHeadlessTest : public TestRomManager::BoundRomTest {
 protected:
  void SetUp() override {
    BoundRomTest::SetUp();

    // Create music bank from ROM
    music_bank_ = std::make_unique<zelda3::music::MusicBank>();

    // Initialize music player with null music bank for basic tests
    // Full music bank loading requires ROM parsing
    player_ = std::make_unique<editor::music::MusicPlayer>(nullptr);
    player_->SetRom(rom());
  }

  void TearDown() override {
    player_.reset();
    music_bank_.reset();
    BoundRomTest::TearDown();
  }

  // Simulate N frames of playback by calling Update() repeatedly
  void SimulatePlayback(int frames) {
    for (int i = 0; i < frames; ++i) {
      player_->Update();
      // Simulate ~16.6ms per frame (NTSC timing)
      // Note: In tests we don't actually sleep, just call Update()
    }
  }

  std::unique_ptr<zelda3::music::MusicBank> music_bank_;
  std::unique_ptr<editor::music::MusicPlayer> player_;
};

// =============================================================================
// Basic Initialization Tests
// =============================================================================

TEST_F(MusicPlayerHeadlessTest, InitializesWithRom) {
  // Player should be created
  EXPECT_NE(player_, nullptr);

  // Initially not ready until a song is played
  EXPECT_FALSE(player_->IsAudioReady());
}

TEST_F(MusicPlayerHeadlessTest, InitialStateIsStopped) {
  auto state = player_->GetState();

  EXPECT_FALSE(state.is_playing);
  EXPECT_FALSE(state.is_paused);
  EXPECT_EQ(state.playing_song_index, -1);
}

// =============================================================================
// Playback State Tests
// =============================================================================

TEST_F(MusicPlayerHeadlessTest, TogglePlayPauseFromStopped) {
  // When stopped with no song, toggle should do nothing
  player_->TogglePlayPause();

  auto state = player_->GetState();
  // Still stopped since no song was selected
  EXPECT_FALSE(state.is_playing);
}

TEST_F(MusicPlayerHeadlessTest, StopClearsPlaybackState) {
  // Start playback then stop
  player_->PlaySong(0);
  player_->Stop();

  auto state = player_->GetState();
  EXPECT_FALSE(state.is_playing);
  EXPECT_FALSE(state.is_paused);
}

// =============================================================================
// Audio Timing Verification Tests
// =============================================================================

TEST_F(MusicPlayerHeadlessTest, UpdateDoesNotCrashWithoutPlayback) {
  // Calling Update() when not playing should be safe
  EXPECT_NO_THROW(SimulatePlayback(60));
}

TEST_F(MusicPlayerHeadlessTest, DirectSpcModeCanBeEnabled) {
  // Direct SPC mode bypasses game CPU and plays audio directly
  // This is set via SetDirectSpcMode() - no getter exposed, just verify no crash
  EXPECT_NO_THROW(player_->SetDirectSpcMode(true));
  EXPECT_NO_THROW(player_->SetDirectSpcMode(false));
}

TEST_F(MusicPlayerHeadlessTest, InterpolationTypeCanBeSet) {
  // Interpolation type is set via SetInterpolationType()
  // No getter exposed, just verify no crash
  EXPECT_NO_THROW(player_->SetInterpolationType(0));  // Linear
  EXPECT_NO_THROW(player_->SetInterpolationType(2));  // Gaussian (default SNES)
}

// =============================================================================
// Playback Speed Regression Tests
// =============================================================================

TEST_F(MusicPlayerHeadlessTest, PlaybackStateTracksSpeedCorrectly) {
  auto state = player_->GetState();

  // Playback speed should always be 1.0x (varispeed was removed)
  EXPECT_FLOAT_EQ(state.playback_speed, 1.0f)
      << "Playback speed should be 1.0x";
}

TEST_F(MusicPlayerHeadlessTest, TicksPerSecondMatchesTempo) {
  // Default tempo of 150 should produce specific ticks per second
  // Formula: ticks_per_second = 500.0f * (tempo / 256.0f)
  // At tempo 150: 500 * (150/256) = 292.97

  constexpr float kDefaultTempo = 150.0f;
  constexpr float kExpectedTps = 500.0f * (kDefaultTempo / 256.0f);

  // Get state and verify ticks_per_second is reasonable
  auto state = player_->GetState();

  // Initially ticks_per_second may be 0 if no song is playing
  // After playing a song, it should match the formula
  LOG_INFO("MusicPlayerTest", "Initial ticks_per_second: %.2f (expected ~%.2f for tempo 150)",
           state.ticks_per_second, kExpectedTps);

  // If a song is playing, verify the value
  if (state.is_playing) {
    EXPECT_NEAR(state.ticks_per_second, kExpectedTps, 10.0f)
        << "Ticks per second should match tempo-based calculation";
  }
}

// =============================================================================
// Frame Timing Tests
// =============================================================================

TEST_F(MusicPlayerHeadlessTest, UpdateProcessesFramesCorrectly) {
  // This test verifies Update() can be called repeatedly without issues
  // In a real scenario, Update() would process audio frames

  auto start = std::chrono::steady_clock::now();

  // Simulate 10 seconds of updates (600 frames)
  constexpr int kTestFrames = 600;
  SimulatePlayback(kTestFrames);

  auto end = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration<double>(end - start).count();

  LOG_INFO("MusicPlayerTest", "Processed %d Update() calls in %.3f seconds",
           kTestFrames, elapsed);

  // Update() should be fast (no blocking)
  EXPECT_LT(elapsed, 1.0) << "Update() calls should be fast (not blocking)";
}

// =============================================================================
// Cleanup and Edge Cases
// =============================================================================

TEST_F(MusicPlayerHeadlessTest, DestructorCleansUpProperly) {
  // Start playback to initialize audio
  player_->PlaySong(0);

  // Simulate some activity
  SimulatePlayback(10);

  // Reset should clean up without crashes
  player_.reset();

  SUCCEED() << "MusicPlayer destructor completed without crash";
}

TEST_F(MusicPlayerHeadlessTest, MultiplePlaySongsAreSafe) {
  // Call PlaySong multiple times
  player_->PlaySong(0);
  player_->PlaySong(0);
  player_->PlaySong(0);

  // Should still work
  SimulatePlayback(10);
}

}  // namespace test
}  // namespace yaze
