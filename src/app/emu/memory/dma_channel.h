#ifndef YAZE_APP_EMU_MEMORY_DMA_CHANNEL_H
#define YAZE_APP_EMU_MEMORY_DMA_CHANNEL_H

#include <cstdint>

namespace yaze {
namespace app {
namespace emu {
namespace memory {

typedef struct DmaChannel {
  uint8_t bAdr;
  uint16_t aAdr;
  uint8_t aBank;
  uint16_t size;      // also indirect hdma adr
  uint8_t indBank;    // hdma
  uint16_t tableAdr;  // hdma
  uint8_t repCount;   // hdma
  uint8_t unusedByte;
  bool dmaActive;
  bool hdmaActive;
  uint8_t mode;
  bool fixed;
  bool decrement;
  bool indirect;  // hdma
  bool fromB;
  bool unusedBit;
  bool doTransfer;  // hdma
  bool terminated;  // hdma
} DmaChannel;

}  // namespace memory
}  // namespace emu
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EMU_MEMORY_DMA_CHANNEL_H