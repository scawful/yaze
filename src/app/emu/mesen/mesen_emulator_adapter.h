#ifndef YAZE_APP_EMU_MESEN_MESEN_EMULATOR_ADAPTER_H_
#define YAZE_APP_EMU_MESEN_MESEN_EMULATOR_ADAPTER_H_

#include <memory>

#include "app/emu/i_emulator.h"
#include "app/emu/mesen/mesen_socket_client.h"

namespace yaze {
namespace emu {
namespace mesen {

class MesenEmulatorAdapter : public IEmulator {
 public:
  MesenEmulatorAdapter();
  ~MesenEmulatorAdapter() override;

  // --- Core Lifecycle ---
  bool IsConnected() const override;
  bool IsRunning() const override;
  void Pause() override;
  void Resume() override;
  void Reset() override;
  absl::Status Step(int count) override;
  absl::Status StepOver() override;
  absl::Status StepOut() override;

  // --- ROM ---
  absl::Status LoadRom(const std::string& path) override;
  std::string GetLoadedRomPath() const override;

  // --- Memory ---
  absl::StatusOr<uint8_t> ReadByte(uint32_t addr) override;
  absl::StatusOr<std::vector<uint8_t>> ReadBlock(uint32_t addr, size_t len) override;
  absl::Status WriteByte(uint32_t addr, uint8_t val) override;
  absl::Status WriteBlock(uint32_t addr, const std::vector<uint8_t>& data) override;

  // --- CPU State ---
  absl::Status GetCpuState(yaze::agent::CPUState* out_state) override;

  // --- Game State (ALTTP specific) ---
  absl::Status GetGameState(yaze::agent::GameStateResponse* out_state) override;

  // --- Breakpoints ---
  absl::Status RunToBreakpoint(yaze::agent::BreakpointHitResponse* response) override;

  absl::StatusOr<uint32_t> AddBreakpoint(uint32_t addr, 
                                         yaze::agent::BreakpointType type,
                                         yaze::agent::CpuType cpu,
                                         const std::string& condition,
                                         const std::string& description) override;
  absl::Status RemoveBreakpoint(uint32_t id) override;
  absl::Status ToggleBreakpoint(uint32_t id, bool enabled) override;
  std::vector<yaze::agent::BreakpointInfo> ListBreakpoints() override;

  // --- Input ---
  absl::Status PressButton(yaze::agent::Button button) override;
  absl::Status ReleaseButton(yaze::agent::Button button) override;

 private:
  std::unique_ptr<MesenSocketClient> client_;
};

}  // namespace mesen
}  // namespace emu
}  // namespace yaze

#endif  // YAZE_APP_EMU_MESEN_MESEN_EMULATOR_ADAPTER_H_
