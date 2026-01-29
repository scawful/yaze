#ifndef YAZE_APP_EMU_I_EMULATOR_H_
#define YAZE_APP_EMU_I_EMULATOR_H_

#include <string>
#include <vector>
#include <cstdint>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

// Forward declare proto types to avoid heavy includes if possible,
// or include them if necessary for usage.
// Since this is for the service, we can likely include the service protos.
#ifdef YAZE_WITH_GRPC
#include "protos/emulator_service.pb.h"
#endif

namespace yaze {
namespace emu {

#ifdef YAZE_WITH_GRPC
/**
 * @brief Abstract interface for emulator backends (Internal vs Mesen2)
 * 
 * This interface decouples the gRPC EmulatorService from the specific
 * emulator implementation.
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
  virtual absl::StatusOr<std::vector<uint8_t>> ReadBlock(uint32_t addr, size_t len) = 0;
  virtual absl::Status WriteByte(uint32_t addr, uint8_t val) = 0;
  virtual absl::Status WriteBlock(uint32_t addr, const std::vector<uint8_t>& data) = 0;

  // --- CPU State ---
  // Using proto type for convenience in service
  virtual absl::Status GetCpuState(yaze::agent::CPUState* out_state) = 0;

  // --- Game State (ALTTP specific) ---
  virtual absl::Status GetGameState(yaze::agent::GameStateResponse* out_state) = 0;

  // --- Breakpoints ---
  // Run loop until breakpoint hit or timeout
  virtual absl::Status RunToBreakpoint(yaze::agent::BreakpointHitResponse* response) = 0;

  virtual absl::StatusOr<uint32_t> AddBreakpoint(uint32_t addr, 
                                                 yaze::agent::BreakpointType type,
                                                 yaze::agent::CpuType cpu,
                                                 const std::string& condition,
                                                 const std::string& description) = 0;
  virtual absl::Status RemoveBreakpoint(uint32_t id) = 0;
  virtual absl::Status ToggleBreakpoint(uint32_t id, bool enabled) = 0;
  virtual std::vector<yaze::agent::BreakpointInfo> ListBreakpoints() = 0;

  // --- Input ---
  virtual absl::Status PressButton(yaze::agent::Button button) = 0;
  virtual absl::Status ReleaseButton(yaze::agent::Button button) = 0;
  
  // --- Analysis ---
//   virtual absl::StatusOr<std::string> Disassemble(uint32_t addr, int count) = 0;
};
#endif

}  // namespace emu
}  // namespace yaze

#endif  // YAZE_APP_EMU_I_EMULATOR_H_
