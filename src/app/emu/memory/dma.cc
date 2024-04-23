#include "app/emu/memory/dma.h"

#include <iostream>

namespace yaze {
namespace app {
namespace emu {
namespace memory {
namespace dma {

static const int bAdrOffsets[8][4] = {{0, 0, 0, 0}, {0, 1, 0, 1}, {0, 0, 0, 0},
                                      {0, 0, 1, 1}, {0, 1, 2, 3}, {0, 1, 0, 1},
                                      {0, 0, 0, 0}, {0, 0, 1, 1}};

static const int transferLength[8] = {1, 2, 2, 4, 4, 4, 2, 4};

void Reset(MemoryImpl* memory) {
  auto channel = memory->dma_channels();
  for (int i = 0; i < 8; i++) {
    channel[i].bAdr = 0xff;
    channel[i].aAdr = 0xffff;
    channel[i].aBank = 0xff;
    channel[i].size = 0xffff;
    channel[i].indBank = 0xff;
    channel[i].tableAdr = 0xffff;
    channel[i].repCount = 0xff;
    channel[i].unusedByte = 0xff;
    channel[i].dmaActive = false;
    channel[i].hdmaActive = false;
    channel[i].mode = 7;
    channel[i].fixed = true;
    channel[i].decrement = true;
    channel[i].indirect = true;
    channel[i].fromB = true;
    channel[i].unusedBit = true;
    channel[i].doTransfer = false;
    channel[i].terminated = false;
  }
  memory->set_dma_state(0);
  memory->set_hdma_init_requested(false);
  memory->set_hdma_run_requested(false);
}

uint8_t Read(MemoryImpl* memory, uint16_t adr) {
  auto channel = memory->dma_channels();
  uint8_t c = (adr & 0x70) >> 4;
  switch (adr & 0xf) {
    case 0x0: {
      uint8_t val = channel[c].mode;
      val |= channel[c].fixed << 3;
      val |= channel[c].decrement << 4;
      val |= channel[c].unusedBit << 5;
      val |= channel[c].indirect << 6;
      val |= channel[c].fromB << 7;
      return val;
    }
    case 0x1: {
      return channel[c].bAdr;
    }
    case 0x2: {
      return channel[c].aAdr & 0xff;
    }
    case 0x3: {
      return channel[c].aAdr >> 8;
    }
    case 0x4: {
      return channel[c].aBank;
    }
    case 0x5: {
      return channel[c].size & 0xff;
    }
    case 0x6: {
      return channel[c].size >> 8;
    }
    case 0x7: {
      return channel[c].indBank;
    }
    case 0x8: {
      return channel[c].tableAdr & 0xff;
    }
    case 0x9: {
      return channel[c].tableAdr >> 8;
    }
    case 0xa: {
      return channel[c].repCount;
    }
    case 0xb:
    case 0xf: {
      return channel[c].unusedByte;
    }
    default: {
      return memory->open_bus();
    }
  }
}

void Write(MemoryImpl* memory, uint16_t adr, uint8_t val) {
  auto channel = memory->dma_channels();
  uint8_t c = (adr & 0x70) >> 4;
  switch (adr & 0xf) {
    case 0x0: {
      channel[c].mode = val & 0x7;
      channel[c].fixed = val & 0x8;
      channel[c].decrement = val & 0x10;
      channel[c].unusedBit = val & 0x20;
      channel[c].indirect = val & 0x40;
      channel[c].fromB = val & 0x80;
      break;
    }
    case 0x1: {
      channel[c].bAdr = val;
      break;
    }
    case 0x2: {
      channel[c].aAdr = (channel[c].aAdr & 0xff00) | val;
      break;
    }
    case 0x3: {
      channel[c].aAdr = (channel[c].aAdr & 0xff) | (val << 8);
      break;
    }
    case 0x4: {
      channel[c].aBank = val;
      break;
    }
    case 0x5: {
      channel[c].size = (channel[c].size & 0xff00) | val;
      break;
    }
    case 0x6: {
      channel[c].size = (channel[c].size & 0xff) | (val << 8);
      break;
    }
    case 0x7: {
      channel[c].indBank = val;
      break;
    }
    case 0x8: {
      channel[c].tableAdr = (channel[c].tableAdr & 0xff00) | val;
      break;
    }
    case 0x9: {
      channel[c].tableAdr = (channel[c].tableAdr & 0xff) | (val << 8);
      break;
    }
    case 0xa: {
      channel[c].repCount = val;
      break;
    }
    case 0xb:
    case 0xf: {
      channel[c].unusedByte = val;
      break;
    }
    default: {
      break;
    }
  }
}

void DoDma(SNES* snes, MemoryImpl* memory, int cpuCycles) {
  auto channel = memory->dma_channels();

  // align to multiple of 8
  snes->SyncCycles(true, 8);

  // full transfer overhead
  WaitCycle(snes, memory);
  for (int i = 0; i < 8; i++) {
    if (!channel[i].dmaActive) continue;
    // do channel i
    WaitCycle(snes, memory);  // overhead per channel
    int offIndex = 0;
    while (channel[i].dmaActive) {
      WaitCycle(snes, memory);
      TransferByte(snes, memory, channel[i].aAdr, channel[i].aBank,
                   channel[i].bAdr + bAdrOffsets[channel[i].mode][offIndex++],
                   channel[i].fromB);
      offIndex &= 3;
      if (!channel[i].fixed) {
        channel[i].aAdr += channel[i].decrement ? -1 : 1;
      }
      channel[i].size--;
      if (channel[i].size == 0) {
        channel[i].dmaActive = false;
      }
    }
  }

  // re-align to cpu cycles
  snes->SyncCycles(false, cpuCycles);
}

void HandleDma(SNES* snes, MemoryImpl* memory, int cpu_cycles) {
  // if hdma triggered, do it, except if dmastate indicates dma will be done now
  // (it will be done as part of the dma in that case)
  if (memory->hdma_init_requested() && memory->dma_state() != 2)
    InitHdma(snes, memory, true, cpu_cycles);
  if (memory->hdma_run_requested() && memory->dma_state() != 2)
    DoHdma(snes, memory, true, cpu_cycles);
  if (memory->dma_state() == 1) {
    memory->set_dma_state(2);
    return;
  }
  if (memory->dma_state() == 2) {
    // do dma
    DoDma(snes, memory, cpu_cycles);
    memory->set_dma_state(0);
  }
}

void WaitCycle(SNES* snes, MemoryImpl* memory) {
  // run hdma if requested, no sync (already sycned due to dma)
  if (memory->hdma_init_requested()) InitHdma(snes, memory, false, 0);
  if (memory->hdma_run_requested()) DoHdma(snes, memory, false, 0);

  snes->RunCycles(8);
}

void InitHdma(SNES* snes, MemoryImpl* memory, bool do_sync, int cpu_cycles) {
  auto channel = memory->dma_channels();
  memory->set_hdma_init_requested(false);
  bool hdmaEnabled = false;
  // check if a channel is enabled, and do reset
  for (int i = 0; i < 8; i++) {
    if (channel[i].hdmaActive) hdmaEnabled = true;
    channel[i].doTransfer = false;
    channel[i].terminated = false;
  }
  if (!hdmaEnabled) return;
  if (do_sync) snes->SyncCycles(true, 8);

  // full transfer overhead
  snes->RunCycles(8);
  for (int i = 0; i < 8; i++) {
    if (channel[i].hdmaActive) {
      // terminate any dma
      channel[i].dmaActive = false;
      // load address, repCount, and indirect address if needed
      snes->RunCycles(8);
      channel[i].tableAdr = channel[i].aAdr;
      channel[i].repCount =
          snes->Read((channel[i].aBank << 16) | channel[i].tableAdr++);
      if (channel[i].repCount == 0) channel[i].terminated = true;
      if (channel[i].indirect) {
        snes->RunCycles(8);
        channel[i].size =
            snes->Read((channel[i].aBank << 16) | channel[i].tableAdr++);
        snes->RunCycles(8);
        channel[i].size |=
            snes->Read((channel[i].aBank << 16) | channel[i].tableAdr++) << 8;
      }
      channel[i].doTransfer = true;
    }
  }
  if (do_sync) snes->SyncCycles(false, cpu_cycles);
}

void DoHdma(SNES* snes, MemoryImpl* memory, bool do_sync, int cycles) {
  auto channel = memory->dma_channels();
  memory->set_hdma_run_requested(false);
  bool hdmaActive = false;
  int lastActive = 0;
  for (int i = 0; i < 8; i++) {
    if (channel[i].hdmaActive) {
      hdmaActive = true;
      if (!channel[i].terminated) lastActive = i;
    }
  }

  if (!hdmaActive) return;

  if (do_sync) snes->SyncCycles(true, 8);

  // full transfer overhead
  snes->RunCycles(8);
  // do all copies
  for (int i = 0; i < 8; i++) {
    // terminate any dma
    if (channel[i].hdmaActive) channel[i].dmaActive = false;
    if (channel[i].hdmaActive && !channel[i].terminated) {
      // do the hdma
      if (channel[i].doTransfer) {
        for (int j = 0; j < transferLength[channel[i].mode]; j++) {
          snes->RunCycles(8);
          if (channel[i].indirect) {
            TransferByte(snes, memory, channel[i].size++, channel[i].indBank,
                         channel[i].bAdr + bAdrOffsets[channel[i].mode][j],
                         channel[i].fromB);
          } else {
            TransferByte(snes, memory, channel[i].tableAdr++, channel[i].aBank,
                         channel[i].bAdr + bAdrOffsets[channel[i].mode][j],
                         channel[i].fromB);
          }
        }
      }
    }
  }
  // do all updates
  for (int i = 0; i < 8; i++) {
    if (channel[i].hdmaActive && !channel[i].terminated) {
      channel[i].repCount--;
      channel[i].doTransfer = channel[i].repCount & 0x80;
      snes->RunCycles(8);
      uint8_t newRepCount =
          snes->Read((channel[i].aBank << 16) | channel[i].tableAdr);
      if ((channel[i].repCount & 0x7f) == 0) {
        channel[i].repCount = newRepCount;
        channel[i].tableAdr++;
        if (channel[i].indirect) {
          if (channel[i].repCount == 0 && i == lastActive) {
            // if this is the last active channel, only fetch high, and use 0
            // for low
            channel[i].size = 0;
          } else {
            snes->RunCycles(8);
            channel[i].size =
                snes->Read((channel[i].aBank << 16) | channel[i].tableAdr++);
          }
          snes->RunCycles(8);
          channel[i].size |=
              snes->Read((channel[i].aBank << 16) | channel[i].tableAdr++) << 8;
        }
        if (channel[i].repCount == 0) channel[i].terminated = true;
        channel[i].doTransfer = true;
      }
    }
  }

  if (do_sync) snes->SyncCycles(false, cycles);
}

void TransferByte(SNES* snes, MemoryImpl* memory, uint16_t aAdr, uint8_t aBank,
                  uint8_t bAdr, bool fromB) {
  // accessing 0x2180 via b-bus while a-bus accesses ram gives open bus
  bool validB =
      !(bAdr == 0x80 &&
        (aBank == 0x7e || aBank == 0x7f ||
         ((aBank < 0x40 || (aBank >= 0x80 && aBank < 0xc0)) && aAdr < 0x2000)));
  // accesing b-bus, or dma regs via a-bus gives open bus
  bool validA = !((aBank < 0x40 || (aBank >= 0x80 && aBank < 0xc0)) &&
                  (aAdr == 0x420b || aAdr == 0x420c ||
                   (aAdr >= 0x4300 && aAdr < 0x4380) ||
                   (aAdr >= 0x2100 && aAdr < 0x2200)));
  if (fromB) {
    uint8_t val = validB ? snes->ReadBBus(bAdr) : memory->open_bus();
    if (validA) snes->Write((aBank << 16) | aAdr, val);
  } else {
    uint8_t val =
        validA ? snes->Read((aBank << 16) | aAdr) : memory->open_bus();
    if (validB) snes->WriteBBus(bAdr, val);
  }
}

void StartDma(MemoryImpl* memory, uint8_t val, bool hdma) {
  auto channel = memory->dma_channels();
  for (int i = 0; i < 8; i++) {
    if (hdma) {
      channel[i].hdmaActive = val & (1 << i);
    } else {
      channel[i].dmaActive = val & (1 << i);
    }
  }
  if (!hdma) {
    memory->set_dma_state(val != 0 ? 1 : 0);
  }
}

}  // namespace dma
}  // namespace memory
}  // namespace emu
}  // namespace app
}  // namespace yaze