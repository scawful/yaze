#include <cstdint>
#include <string>

#include "app/emu/apu.h"
#include "app/emu/clock.h"
#include "app/emu/cpu.h"
#include "app/emu/dbg.h"
#include "app/emu/ppu.h"
#include "app/emu/spc700.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace emu {

// Direct Memory Address
class DMA {
 public:
  DMA() {
    // Initialize DMA and HDMA channels
    for (int i = 0; i < 8; ++i) {
      channels[i].DMAPn = 0;
      channels[i].BBADn = 0;
      channels[i].UNUSEDn = 0;
      channels[i].A1Tn = 0xFFFFFF;
      channels[i].DASn = 0xFFFF;
      channels[i].A2An = 0xFFFF;
      channels[i].NLTRn = 0xFF;
    }
  }

  // DMA Transfer Modes
  enum class DMA_TRANSFER_TYPE {
    OAM,
    PPUDATA,
    CGDATA,
    FILL_VRAM,
    CLEAR_VRAM,
    RESET_VRAM
  };

  // Functions for handling DMA and HDMA transfers
  void StartDMATransfer(uint8_t channels);
  void EnableHDMATransfers(uint8_t channels);

  // Structure for DMA and HDMA channel registers
  struct Channel {
    uint8_t DMAPn;    // DMA/HDMA parameters
    uint8_t BBADn;    // B-bus address
    uint8_t UNUSEDn;  // Unused byte
    uint32_t A1Tn;    // DMA Current Address / HDMA Table Start Address
    uint16_t DASn;    // DMA Byte-Counter / HDMA indirect table address
    uint16_t A2An;    // HDMA Table Current Address
    uint8_t NLTRn;    // HDMA Line-Counter
  };
  Channel channels[8];

  uint8_t MDMAEN = 0;  // Start DMA transfer
  uint8_t HDMAEN = 0;  // Enable HDMA transfers
};

class SNES : public DMA {
 public:
  SNES() = default;
  ~SNES() = default;

  ROMInfo ReadRomHeader(uint32_t offset);

  // Initialization
  void Init(ROM& rom);

  // Main emulation loop
  void Run();

  // Enable NMI Interrupts
  void EnableVBlankInterrupts();

  // Wait until the VBlank routine has been processed
  void WaitForVBlank();

  // NMI Interrupt Service Routine
  void NmiIsr();

  // VBlank routine
  void VBlankRoutine();

  // Controller input handling
  void HandleInput();

  // Save/Load game state
  void SaveState(const std::string& path);
  void LoadState(const std::string& path);

  // Debugger
  void Debug();
  void Breakpoint(uint16_t address);

  bool running() const { return running_; }

 private:
  void WriteToRegister(uint16_t address, uint8_t value) {
    memory_.WriteByte(address, value);
  }

  // Components of the SNES
  MemoryImpl memory_;
  Clock clock_;
  AudioRAM audio_ram_;

  CPU cpu{memory_, clock_};
  PPU ppu{memory_, clock_};
  APU apu{memory_, audio_ram_, clock_};

  // Helper classes
  ROMInfo rom_info_;
  Debugger debugger;

  std::vector<uint8_t> rom_data;

  // Byte flag to indicate if the VBlank routine should be executed or not
  std::atomic<bool> v_blank_flag_;

  // 32-bit counter to track the number of NMI interrupts (useful for clocks and
  // timers)
  std::atomic<uint32_t> frame_counter_;

  // Other private member variables
  bool running_;
  int scanline;
};

}  // namespace emu
}  // namespace app
}  // namespace yaze