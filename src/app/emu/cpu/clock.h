#ifndef YAZE_APP_EMU_CLOCK_H_
#define YAZE_APP_EMU_CLOCK_H_

#include <cstdint>

namespace yaze {
namespace app {
namespace emu {

class Clock {
 public:
  virtual ~Clock() = default;
  virtual void UpdateClock(double delta) = 0;
  virtual unsigned long long GetCycleCount() const = 0;
  virtual void ResetAccumulatedTime() = 0;
  virtual void SetFrequency(float new_frequency) = 0;
  virtual float GetFrequency() const = 0;
};

class ClockImpl : public Clock {
 public:
  ClockImpl() = default;
  virtual ~ClockImpl() = default;

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

  void UpdateClock(double delta) override {
    UpdateCycleCount(delta);
    ResetAccumulatedTime();
  }

  void ResetAccumulatedTime() override { accumulatedTime = 0.0; }
  unsigned long long GetCycleCount() const override { return cycleCount; }
  float GetFrequency() const override { return frequency; }
  void SetFrequency(float new_frequency) override {
    this->frequency = new_frequency;
  }

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