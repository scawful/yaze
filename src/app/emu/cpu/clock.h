#ifndef YAZE_APP_EMU_CLOCK_H_
#define YAZE_APP_EMU_CLOCK_H_

#include <cstdint>

namespace yaze {
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

  void UpdateCycleCount(double delta_time) {
    accumulated_time += delta_time;
    double cycle_time = 1.0 / frequency;

    while (accumulated_time >= cycle_time) {
      Cycle();
      accumulated_time -= cycle_time;
    }
  }

  void Cycle() {
    cycle++;
    cycle_count++;
  }

  void UpdateClock(double delta) override {
    UpdateCycleCount(delta);
    ResetAccumulatedTime();
  }

  void ResetAccumulatedTime() override { accumulated_time = 0.0; }
  unsigned long long GetCycleCount() const override { return cycle_count; }
  float GetFrequency() const override { return frequency; }
  void SetFrequency(float new_frequency) override {
    this->frequency = new_frequency;
  }

private:
  uint64_t cycle = 0;                 // Current cycle
  float frequency = 0.0;              // Frequency of the clock in Hz
  unsigned long long cycle_count = 0; // Total number of cycles executed
  double accumulated_time = 0.0; // Accumulated time since the last cycle update
};

} // namespace emu
} // namespace yaze

#endif // YAZE_APP_EMU_CLOCK_H_
