#ifndef YAZE_APP_EMU_MEMORY_DMA_H
#define YAZE_APP_EMU_MEMORY_DMA_H

#include <cstdint>

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
}  // namespace emu
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EMU_MEMORY_DMA_H