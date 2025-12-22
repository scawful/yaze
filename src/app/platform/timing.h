#ifndef YAZE_APP_CORE_TIMING_H
#define YAZE_APP_CORE_TIMING_H

#include "app/platform/sdl_compat.h"

#include <cstdint>

namespace yaze {

/**
 * @class TimingManager
 * @brief Provides accurate timing for animations and frame pacing
 *
 * This class solves the issue where ImGui::GetIO().DeltaTime only updates
 * when events are processed (mouse movement, etc). It uses SDL's performance
 * counter to provide accurate timing regardless of input events.
 */
class TimingManager {
 public:
  static TimingManager& Get() {
    static TimingManager instance;
    return instance;
  }

  /**
   * @brief Update the timing manager (call once per frame)
   * @return The delta time since last update in seconds
   */
  float Update() {
    uint64_t current_time = SDL_GetPerformanceCounter();
    float delta_time = 0.0f;

    if (last_time_ > 0) {
      delta_time = (current_time - last_time_) / static_cast<float>(frequency_);

      // Clamp delta time to prevent huge jumps (e.g., when debugging)
      if (delta_time > 0.1f) {
        delta_time = 0.1f;
      }

      accumulated_time_ += delta_time;
      frame_count_++;

      // Update FPS counter once per second
      if (accumulated_time_ >= 1.0f) {
        fps_ = static_cast<float>(frame_count_) / accumulated_time_;
        frame_count_ = 0;
        accumulated_time_ = 0.0f;
      }
    }

    last_time_ = current_time;
    last_delta_time_ = delta_time;
    return delta_time;
  }

  /**
   * @brief Get the last frame's delta time in seconds
   */
  float GetDeltaTime() const { return last_delta_time_; }

  /**
   * @brief Get current FPS
   */
  float GetFPS() const { return fps_; }

  /**
   * @brief Get total elapsed time since first update
   */
  float GetElapsedTime() const {
    if (last_time_ == 0)
      return 0.0f;
    uint64_t current_time = SDL_GetPerformanceCounter();
    return (current_time - first_time_) / static_cast<float>(frequency_);
  }

  /**
   * @brief Reset the timing state
   */
  void Reset() {
    last_time_ = 0;
    first_time_ = SDL_GetPerformanceCounter();
    accumulated_time_ = 0.0f;
    frame_count_ = 0;
    fps_ = 0.0f;
    last_delta_time_ = 0.0f;
    frame_start_time_ = 0;
  }

  /**
   * @brief Mark the start of a new frame for budget tracking
   */
  void BeginFrame() {
    frame_start_time_ = SDL_GetPerformanceCounter();
  }

  /**
   * @brief Get elapsed time within the current frame in milliseconds
   * @return Milliseconds since BeginFrame() was called
   */
  float GetFrameElapsedMs() const {
    if (frame_start_time_ == 0) return 0.0f;
    uint64_t current = SDL_GetPerformanceCounter();
    return ((current - frame_start_time_) * 1000.0f) / static_cast<float>(frequency_);
  }

  /**
   * @brief Get remaining frame budget in milliseconds (targeting 60fps)
   * @return Milliseconds remaining before frame deadline
   */
  float GetFrameBudgetRemainingMs() const {
    constexpr float kTargetFrameTimeMs = 16.67f;  // 60fps target
    return kTargetFrameTimeMs - GetFrameElapsedMs();
  }

 private:
  TimingManager() {
    frequency_ = SDL_GetPerformanceFrequency();
    first_time_ = SDL_GetPerformanceCounter();
    last_time_ = 0;
    accumulated_time_ = 0.0f;
    frame_count_ = 0;
    fps_ = 0.0f;
    last_delta_time_ = 0.0f;
  }

  uint64_t frequency_;
  uint64_t first_time_;
  uint64_t last_time_;
  uint64_t frame_start_time_ = 0;
  float accumulated_time_;
  uint32_t frame_count_;
  float fps_;
  float last_delta_time_;

  TimingManager(const TimingManager&) = delete;
  TimingManager& operator=(const TimingManager&) = delete;
};

}  // namespace yaze

#endif  // YAZE_APP_CORE_TIMING_H
