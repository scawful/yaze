#ifndef YAZE_APP_EMU_CLOCK_H_
#define YAZE_APP_EMU_CLOCK_H_

#include <cstdint>

namespace yaze {
namespace app {
namespace emu {

class Clock {
 public:
  Clock() = default;
  virtual ~Clock() = default;

  void UpdateCycleCount(double deltaTime) {
    accumulatedTime += deltaTime;
    double cycleTime = 1.0 / frequency;

    while (accumulatedTime >= cycleTime) {
      Cycle();
      accumulatedTime -= cycleTime;
    }
  }

  void Cycle() {
    cycle++;
    cycleCount++;
  }

  void UpdateClock(double delta) {
    UpdateCycleCount(delta);
    ResetAccumulatedTime();
  }

  unsigned long long GetCycleCount() const { return cycleCount; }
  float GetFrequency() const { return frequency; }
  void SetFrequency(float new_frequency) { this->frequency = new_frequency; }
  void ResetAccumulatedTime() { accumulatedTime = 0.0; }

 private:
  uint64_t cycle = 0;                 // Current cycle
  float frequency = 0.0;              // Frequency of the clock in Hz
  unsigned long long cycleCount = 0;  // Total number of cycles executed
  double accumulatedTime = 0.0;  // Accumulated time since the last cycle update
};

}  // namespace emu
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EMU_CLOCK_H_