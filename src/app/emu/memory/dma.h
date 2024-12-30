#ifndef YAZE_APP_EMU_MEMORY_DMA_H
#define YAZE_APP_EMU_MEMORY_DMA_H

#include <cstdint>

#include "app/emu/memory/memory.h"
#include "app/emu/snes.h"

namespace yaze {
namespace emu {

void ResetDma(MemoryImpl* memory);
void HandleDma(Snes* snes, MemoryImpl* memory, int cpu_cycles);

void WaitCycle(Snes* snes, MemoryImpl* memory);

void InitHdma(Snes* snes, MemoryImpl* memory, bool do_sync, int cycles);
void DoHdma(Snes* snes, MemoryImpl* memory, bool do_sync, int cycles);

void TransferByte(Snes* snes, MemoryImpl* memory, uint16_t aAdr, uint8_t aBank,
                  uint8_t bAdr, bool fromB);

uint8_t ReadDma(MemoryImpl* memory, uint16_t address);
void WriteDma(MemoryImpl* memory, uint16_t address, uint8_t data);
void StartDma(MemoryImpl* memory, uint8_t val, bool hdma);
void DoDma(Snes* snes, MemoryImpl* memory, int cycles);

}  // namespace emu
}  // namespace yaze

#endif  // YAZE_APP_EMU_MEMORY_DMA_H
