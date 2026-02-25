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

  /// Explicitly connect to a Mesen2 socket (replaces auto-connect).
  absl::Status Connect();
  absl::Status Connect(const std::string& socket_path);

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
  absl::StatusOr<std::vector<uint8_t>> ReadBlock(uint32_t addr,
                                                  size_t len) override;
  absl::Status WriteByte(uint32_t addr, uint8_t val) override;
  absl::Status WriteBlock(uint32_t addr,
                          const std::vector<uint8_t>& data) override;

  // --- CPU State ---
  absl::Status GetCpuState(CpuStateSnapshot* out_state) override;

  // --- Game State (ALTTP specific) ---
  absl::Status GetGameState(GameSnapshot* out_state) override;

  // --- Breakpoints ---
  absl::Status RunToBreakpoint(BreakpointHitResult* response) override;

  absl::StatusOr<uint32_t> AddBreakpoint(
      uint32_t addr, BreakpointKind type, CpuKind cpu,
      const std::string& condition,
      const std::string& description) override;
  absl::Status RemoveBreakpoint(uint32_t breakpoint_id) override;
  absl::Status ToggleBreakpoint(uint32_t breakpoint_id, bool enabled) override;
  std::vector<BreakpointSnapshot> ListBreakpoints() override;

  // --- Input ---
  absl::Status PressButton(InputButton button) override;
  absl::Status ReleaseButton(InputButton button) override;

  // --- Save State ---
  absl::Status SaveState(int slot) override;
  absl::Status LoadState(int slot) override;

  // --- Feature Query ---
  bool SupportsFeature(EmulatorFeature feature) const override;

  // --- Overlays ---
  absl::Status SetCollisionOverlay(bool enable) override;

 private:
  std::unique_ptr<MesenSocketClient> client_;
};

}  // namespace mesen
}  // namespace emu
}  // namespace yaze

#endif  // YAZE_APP_EMU_MESEN_MESEN_EMULATOR_ADAPTER_H_
