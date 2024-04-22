#ifndef YAZE_APP_EMU_MEMORY_DMA_H
#define YAZE_APP_EMU_MEMORY_DMA_H

#include <cstdint>

#include "app/emu/memory/memory.h"
#include "app/emu/snes.h"

namespace yaze {
namespace app {
namespace emu {
namespace memory {
namespace dma {

void HandleDma(SNES* snes, MemoryImpl* memory, int cpu_cycles);

void WaitCycle(SNES* snes, MemoryImpl* memory);

void InitHdma(SNES* snes, MemoryImpl* memory, bool do_sync, int cycles);
void DoHdma(SNES* snes, MemoryImpl* memory, bool do_sync, int cycles);

void TransferByte(SNES* snes, MemoryImpl* memory, uint16_t aAdr, uint8_t aBank,
                  uint8_t bAdr, bool fromB);

uint8_t Read(MemoryImpl* memory, uint16_t address);
void Write(MemoryImpl* memory, uint16_t address, uint8_t data);
void StartDma(MemoryImpl* memory, uint8_t val, bool hdma);
void DoDma(MemoryImpl* memory, int cycles);

}  // namespace dma
}  // namespace memory
}  // namespace emu
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EMU_MEMORY_DMA_H