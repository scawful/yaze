#ifndef YAZE_APP_EMU_I_EMULATOR_H_
#define YAZE_APP_EMU_I_EMULATOR_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/emu/emulator_types.h"

namespace yaze {
namespace emu {

/**
 * @brief Abstract interface for emulator backends (Internal vs Mesen2)
 *
 * This interface uses native C++ types from emulator_types.h rather than
 * proto types, so it compiles and is testable without gRPC/protobuf.
 * The gRPC service layer converts between these types and proto types
 * via proto_converter.h.
 */
class IEmulator {
 public:
  virtual ~IEmulator() = default;

  // --- Core Lifecycle ---
  virtual bool IsConnected() const = 0;
  virtual bool IsRunning() const = 0;
  virtual void Pause() = 0;
  virtual void Resume() = 0;
  virtual void Reset() = 0;
  virtual absl::Status Step(int count) = 0;
  virtual absl::Status StepOver() = 0;
  virtual absl::Status StepOut() = 0;

  // --- ROM ---
  virtual absl::Status LoadRom(const std::string& path) = 0;
  virtual std::string GetLoadedRomPath() const = 0;

  // --- Memory ---
  virtual absl::StatusOr<uint8_t> ReadByte(uint32_t addr) = 0;
  virtual absl::StatusOr<std::vector<uint8_t>> ReadBlock(uint32_t addr,
                                                         size_t len) = 0;
  virtual absl::Status WriteByte(uint32_t addr, uint8_t val) = 0;
  virtual absl::Status WriteBlock(uint32_t addr,
                                  const std::vector<uint8_t>& data) = 0;

  // --- CPU State ---
  virtual absl::Status GetCpuState(CpuStateSnapshot* out_state) = 0;

  // --- Game State (ALTTP specific) ---
  virtual absl::Status GetGameState(GameSnapshot* out_state) = 0;

  // --- Breakpoints ---
  virtual absl::Status RunToBreakpoint(BreakpointHitResult* response) = 0;

  virtual absl::StatusOr<uint32_t> AddBreakpoint(
      uint32_t addr, BreakpointKind type, CpuKind cpu,
      const std::string& condition, const std::string& description) = 0;
  virtual absl::Status RemoveBreakpoint(uint32_t breakpoint_id) = 0;
  virtual absl::Status ToggleBreakpoint(uint32_t breakpoint_id,
                                        bool enabled) = 0;
  virtual std::vector<BreakpointSnapshot> ListBreakpoints() = 0;

  // --- Input ---
  virtual absl::Status PressButton(InputButton button) = 0;
  virtual absl::Status ReleaseButton(InputButton button) = 0;

  // --- Save State ---
  virtual absl::Status SaveState(int slot) {
    return absl::UnimplementedError("SaveState not supported by this backend");
  }
  virtual absl::Status LoadState(int slot) {
    return absl::UnimplementedError("LoadState not supported by this backend");
  }

  // --- Feature Query ---
  virtual bool SupportsFeature(EmulatorFeature feature) const {
    (void)feature;
    return false;
  }

  // --- Optional Backend-Specific Features ---
  virtual absl::Status SetCollisionOverlay(bool enable) {
    return absl::UnimplementedError(
        "Collision overlay not supported by this backend");
  }
};

}  // namespace emu
}  // namespace yaze

#endif  // YAZE_APP_EMU_I_EMULATOR_H_
