#ifndef YAZE_APP_EMU_MEMORY_DMA_CHANNEL_H
#define YAZE_APP_EMU_MEMORY_DMA_CHANNEL_H

#include <cstdint>

namespace yaze {
namespace emu {
namespace memory {

typedef struct DmaChannel {
  uint8_t b_addr;
  uint16_t a_addr;
  uint8_t a_bank;
  uint16_t size;        // also indirect hdma adr
  uint8_t ind_bank;     // hdma
  uint16_t table_addr;  // hdma
  uint8_t rep_count;    // hdma
  uint8_t unusedByte;
  bool dma_active;
  bool hdma_active;
  uint8_t mode;
  bool fixed;
  bool decrement;
  bool indirect;  // hdma
  bool from_b;
  bool unusedBit;
  bool do_transfer;  // hdma
  bool terminated;   // hdma
} DmaChannel;

}  // namespace memory
}  // namespace emu

}  // namespace yaze

#endif  // YAZE_APP_EMU_MEMORY_DMA_CHANNEL_H