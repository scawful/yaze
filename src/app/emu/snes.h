#ifndef YAZE_APP_EMU_SNES_H
#define YAZE_APP_EMU_SNES_H

#include <cstdint>
#include <functional>
#include <memory>

#include "absl/status/status.h"

#include "app/emu/audio/apu.h"
#include "app/emu/cpu/cpu.h"
#include "app/emu/debug/apu_debugger.h"
#include "app/emu/memory/memory.h"
#include "app/emu/video/ppu.h"

namespace yaze {
namespace emu {

struct Input {
  uint8_t type;
  bool latch_line_;
  // for controller
  uint16_t current_state_;   // actual state from this frame
  uint16_t latched_state_;   // state being shifted out for serial read
  uint16_t previous_state_;  // state from previous frame for edge detection
};

class Snes {
 public:
  Snes() {
    // Initialize input controllers to clean state
    input1 = {};
    input2 = {};

    cpu_.callbacks().read_byte = [this](uint32_t adr) {
      return CpuRead(adr);
    };
    cpu_.callbacks().write_byte = [this](uint32_t adr, uint8_t val) {
      CpuWrite(adr, val);
    };
    cpu_.callbacks().idle = [this](bool waiting) {
      CpuIdle(waiting);
    };
  }
  ~Snes() = default;

  void Init(const std::vector<uint8_t>& rom_data);
  void Reset(bool hard = false);
  void RunFrame();
  void RunAudioFrame();  // Audio-focused frame: runs CPU+APU, skips PPU rendering
  void CatchUpApu();
  void HandleInput();
  void RunCycle();
  void RunCycles(int cycles);
  void SyncCycles(bool start, int sync_cycles);

  uint8_t ReadBBus(uint8_t adr);
  uint8_t ReadReg(uint16_t adr);
  uint8_t Rread(uint32_t adr);
  uint8_t Read(uint32_t adr);

  void WriteBBus(uint8_t adr, uint8_t val);
  void WriteReg(uint16_t adr, uint8_t val);
  void Write(uint32_t adr, uint8_t val);

  int GetAccessTime(uint32_t adr);
  uint8_t CpuRead(uint32_t adr);
  void CpuWrite(uint32_t adr, uint8_t val);
  void CpuIdle(bool waiting);
  void InitAccessTime(bool recalc);
  std::vector<uint8_t> access_time;

  void SetSamples(int16_t* sample_data, int wanted_samples);
  void SetPixels(uint8_t* pixel_data);
  void SetButtonState(int player, int button, bool pressed);

  absl::Status loadState(const std::string& path);
  absl::Status saveState(const std::string& path);
  absl::Status LoadLegacyState(std::istream& file);

  bool running() const { return running_; }
  bool audio_only_mode() const { return audio_only_mode_; }
  void set_audio_only_mode(bool mode) { audio_only_mode_ = mode; }
  auto cpu() -> Cpu& { return cpu_; }
  auto ppu() -> Ppu& { return ppu_; }
  auto apu() -> Apu& { return apu_; }
  auto apu() const -> const Apu& { return apu_; }
  auto memory() -> MemoryImpl& { return memory_; }
  auto get_ram() -> uint8_t* { return ram; }
  auto mutable_cycles() -> uint64_t& { return cycles_; }

  // Input state accessors (for debugging UI)
  uint16_t GetInput1State() const { return input1.current_state_; }
  uint16_t GetInput2State() const { return input2.current_state_; }
  uint16_t GetPortAutoRead(int index) const {
    return (index >= 0 && index < 4) ? port_auto_read_[index] : 0;
  }
  bool IsAutoJoyReadEnabled() const { return auto_joy_read_; }

  // Frame metrics
  void ResetFrameMetrics() {
    dma_bytes_frame_ = 0;
    vram_bytes_frame_ = 0;
  }
  void AccumulateDmaBytes(uint32_t bytes) { dma_bytes_frame_ += bytes; }
  void AccumulateVramBytes(uint32_t bytes) { vram_bytes_frame_ += bytes; }
  uint64_t dma_bytes_frame() const { return dma_bytes_frame_; }
  uint64_t vram_bytes_frame() const { return vram_bytes_frame_; }

  // Audio debugging
  auto apu_handshake_tracker() -> debug::ApuHandshakeTracker& {
    return apu_handshake_tracker_;
  }

  bool fast_mem_ = false;

 private:
  MemoryImpl memory_;
  Cpu cpu_{memory_};
  Ppu ppu_{memory_};
  Apu apu_{memory_};

  std::vector<uint8_t> rom_data;

  bool running_ = false;
  bool audio_only_mode_ = false;  // Skip PPU rendering for audio-focused playback

  // ram
  uint8_t ram[0x20000];
  uint32_t ram_adr_;

  // Frame timing
  uint32_t frames_ = 0;
  uint64_t cycles_ = 0;
  uint64_t sync_cycle_ = 0;
  uint64_t dma_bytes_frame_ = 0;
  uint64_t vram_bytes_frame_ = 0;
  double apu_catchup_cycles_;
  uint32_t next_horiz_event;

  // Nmi / Irq
  bool h_irq_enabled_ = false;
  bool v_irq_enabled_ = false;
  bool nmi_enabled_ = false;
  uint16_t h_timer_ = 0;
  uint16_t v_timer_ = 0;
  bool in_nmi_ = false;
  bool irq_condition_ = false;
  bool in_irq_ = false;
  bool in_vblank_;

  // Multiplication / Division
  uint8_t multiply_a_;
  uint16_t multiply_result_;
  uint16_t divide_a_;
  uint16_t divide_result_;

  // Joypad State
  Input input1;
  Input input2;
  uint16_t port_auto_read_[4];  // as read by auto-joypad read
  bool auto_joy_read_ = false;
  uint16_t auto_joy_timer_ = 0;
  bool ppu_latch_;

  // Audio debugging
  debug::ApuHandshakeTracker apu_handshake_tracker_;
};

}  // namespace emu
}  // namespace yaze

#endif  // YAZE_APP_EMU_SNES_H
