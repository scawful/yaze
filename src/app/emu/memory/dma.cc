#include "app/emu/memory/dma.h"

namespace yaze {
namespace emu {

static const int bAdrOffsets[8][4] = {{0, 0, 0, 0}, {0, 1, 0, 1}, {0, 0, 0, 0},
                                      {0, 0, 1, 1}, {0, 1, 2, 3}, {0, 1, 0, 1},
                                      {0, 0, 0, 0}, {0, 0, 1, 1}};

static const int transferLength[8] = {1, 2, 2, 4, 4, 4, 2, 4};

void ResetDma(MemoryImpl* memory) {
  auto channel = memory->dma_channels();
  for (int i = 0; i < 8; i++) {
    channel[i].b_addr = 0xff;
    channel[i].a_addr = 0xffff;
    channel[i].a_bank = 0xff;
    channel[i].size = 0xffff;
    channel[i].ind_bank = 0xff;
    channel[i].table_addr = 0xffff;
    channel[i].rep_count = 0xff;
    channel[i].unusedByte = 0xff;
    channel[i].dma_active = false;
    channel[i].hdma_active = false;
    channel[i].mode = 7;
    channel[i].fixed = true;
    channel[i].decrement = true;
    channel[i].indirect = true;
    channel[i].from_b = true;
    channel[i].unusedBit = true;
    channel[i].do_transfer = false;
    channel[i].terminated = false;
  }
  memory->set_dma_state(0);
  memory->set_hdma_init_requested(false);
  memory->set_hdma_run_requested(false);
}

uint8_t ReadDma(MemoryImpl* memory, uint16_t adr) {
  auto channel = memory->dma_channels();
  uint8_t c = (adr & 0x70) >> 4;
  switch (adr & 0xf) {
    case 0x0: {
      uint8_t val = channel[c].mode;
      val |= channel[c].fixed << 3;
      val |= channel[c].decrement << 4;
      val |= channel[c].unusedBit << 5;
      val |= channel[c].indirect << 6;
      val |= channel[c].from_b << 7;
      return val;
    }
    case 0x1: {
      return channel[c].b_addr;
    }
    case 0x2: {
      return channel[c].a_addr & 0xff;
    }
    case 0x3: {
      return channel[c].a_addr >> 8;
    }
    case 0x4: {
      return channel[c].a_bank;
    }
    case 0x5: {
      return channel[c].size & 0xff;
    }
    case 0x6: {
      return channel[c].size >> 8;
    }
    case 0x7: {
      return channel[c].ind_bank;
    }
    case 0x8: {
      return channel[c].table_addr & 0xff;
    }
    case 0x9: {
      return channel[c].table_addr >> 8;
    }
    case 0xa: {
      return channel[c].rep_count;
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

void WriteDma(MemoryImpl* memory, uint16_t adr, uint8_t val) {
  auto channel = memory->dma_channels();
  uint8_t c = (adr & 0x70) >> 4;
  switch (adr & 0xf) {
    case 0x0: {
      channel[c].mode = val & 0x7;
      channel[c].fixed = val & 0x8;
      channel[c].decrement = val & 0x10;
      channel[c].unusedBit = val & 0x20;
      channel[c].indirect = val & 0x40;
      channel[c].from_b = val & 0x80;
      break;
    }
    case 0x1: {
      channel[c].b_addr = val;
      break;
    }
    case 0x2: {
      channel[c].a_addr = (channel[c].a_addr & 0xff00) | val;
      break;
    }
    case 0x3: {
      channel[c].a_addr = (channel[c].a_addr & 0xff) | (val << 8);
      break;
    }
    case 0x4: {
      channel[c].a_bank = val;
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
      channel[c].ind_bank = val;
      break;
    }
    case 0x8: {
      channel[c].table_addr = (channel[c].table_addr & 0xff00) | val;
      break;
    }
    case 0x9: {
      channel[c].table_addr = (channel[c].table_addr & 0xff) | (val << 8);
      break;
    }
    case 0xa: {
      channel[c].rep_count = val;
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

void DoDma(Snes* snes, MemoryImpl* memory, int cpuCycles) {
  auto channel = memory->dma_channels();
  snes->cpu().set_int_delay(true);

  // align to multiple of 8
  snes->SyncCycles(true, 8);

  // full transfer overhead
  WaitCycle(snes, memory);
  for (int i = 0; i < 8; i++) {
    if (!channel[i].dma_active)
      continue;

    // do channel i
    WaitCycle(snes, memory);  // overhead per channel
    int offIndex = 0;
    while (channel[i].dma_active) {
      WaitCycle(snes, memory);
      TransferByte(snes, memory, channel[i].a_addr, channel[i].a_bank,
                   channel[i].b_addr + bAdrOffsets[channel[i].mode][offIndex++],
                   channel[i].from_b);
      offIndex &= 3;
      if (!channel[i].fixed) {
        channel[i].a_addr += channel[i].decrement ? -1 : 1;
      }
      channel[i].size--;
      if (channel[i].size == 0) {
        channel[i].dma_active = false;
      }
    }
  }

  // re-align to cpu cycles
  snes->SyncCycles(false, cpuCycles);
}

void HandleDma(Snes* snes, MemoryImpl* memory, int cpu_cycles) {
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

void WaitCycle(Snes* snes, MemoryImpl* memory) {
  // run hdma if requested, no sync (already sycned due to dma)
  if (memory->hdma_init_requested())
    InitHdma(snes, memory, false, 0);
  if (memory->hdma_run_requested())
    DoHdma(snes, memory, false, 0);

  snes->RunCycles(8);
}

void InitHdma(Snes* snes, MemoryImpl* memory, bool do_sync, int cpu_cycles) {
  auto channel = memory->dma_channels();
  memory->set_hdma_init_requested(false);
  bool hdmaEnabled = false;
  // check if a channel is enabled, and do reset
  for (int i = 0; i < 8; i++) {
    if (channel[i].hdma_active)
      hdmaEnabled = true;
    channel[i].do_transfer = false;
    channel[i].terminated = false;
  }
  if (!hdmaEnabled)
    return;
  snes->cpu().set_int_delay(true);
  if (do_sync)
    snes->SyncCycles(true, 8);

  // full transfer overhead
  snes->RunCycles(8);
  for (int i = 0; i < 8; i++) {
    if (channel[i].hdma_active) {
      // terminate any dma
      channel[i].dma_active = false;
      // load address, repCount, and indirect address if needed
      snes->RunCycles(8);
      channel[i].table_addr = channel[i].a_addr;
      channel[i].rep_count =
          snes->Read((channel[i].a_bank << 16) | channel[i].table_addr++);
      if (channel[i].rep_count == 0)
        channel[i].terminated = true;
      if (channel[i].indirect) {
        snes->RunCycles(8);
        channel[i].size =
            snes->Read((channel[i].a_bank << 16) | channel[i].table_addr++);
        snes->RunCycles(8);
        channel[i].size |=
            snes->Read((channel[i].a_bank << 16) | channel[i].table_addr++)
            << 8;
      }
      channel[i].do_transfer = true;
    }
  }
  if (do_sync)
    snes->SyncCycles(false, cpu_cycles);
}

void DoHdma(Snes* snes, MemoryImpl* memory, bool do_sync, int cycles) {
  auto channel = memory->dma_channels();
  memory->set_hdma_run_requested(false);
  bool hdmaActive = false;
  int lastActive = 0;
  for (int i = 0; i < 8; i++) {
    if (channel[i].hdma_active) {
      hdmaActive = true;
      if (!channel[i].terminated)
        lastActive = i;
    }
  }

  if (!hdmaActive)
    return;
  snes->cpu().set_int_delay(true);

  if (do_sync)
    snes->SyncCycles(true, 8);

  // full transfer overhead
  snes->RunCycles(8);
  // do all copies
  for (int i = 0; i < 8; i++) {
    // terminate any dma
    if (channel[i].hdma_active)
      channel[i].dma_active = false;
    if (channel[i].hdma_active && !channel[i].terminated) {
      // do the hdma
      if (channel[i].do_transfer) {
        for (int j = 0; j < transferLength[channel[i].mode]; j++) {
          snes->RunCycles(8);
          if (channel[i].indirect) {
            TransferByte(snes, memory, channel[i].size++, channel[i].ind_bank,
                         channel[i].b_addr + bAdrOffsets[channel[i].mode][j],
                         channel[i].from_b);
          } else {
            TransferByte(snes, memory, channel[i].table_addr++,
                         channel[i].a_bank,
                         channel[i].b_addr + bAdrOffsets[channel[i].mode][j],
                         channel[i].from_b);
          }
        }
      }
    }
  }
  // do all updates
  for (int i = 0; i < 8; i++) {
    if (channel[i].hdma_active && !channel[i].terminated) {
      channel[i].rep_count--;
      channel[i].do_transfer = channel[i].rep_count & 0x80;
      snes->RunCycles(8);
      uint8_t newRepCount =
          snes->Read((channel[i].a_bank << 16) | channel[i].table_addr);
      if ((channel[i].rep_count & 0x7f) == 0) {
        channel[i].rep_count = newRepCount;
        channel[i].table_addr++;
        if (channel[i].indirect) {
          if (channel[i].rep_count == 0 && i == lastActive) {
            // if this is the last active channel, only fetch high, and use 0
            // for low
            channel[i].size = 0;
          } else {
            snes->RunCycles(8);
            channel[i].size =
                snes->Read((channel[i].a_bank << 16) | channel[i].table_addr++);
          }
          snes->RunCycles(8);
          channel[i].size |=
              snes->Read((channel[i].a_bank << 16) | channel[i].table_addr++)
              << 8;
        }
        if (channel[i].rep_count == 0)
          channel[i].terminated = true;
        channel[i].do_transfer = true;
      }
    }
  }

  if (do_sync)
    snes->SyncCycles(false, cycles);
}

void TransferByte(Snes* snes, MemoryImpl* memory, uint16_t aAdr, uint8_t aBank,
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
  auto record_vram = [&](uint8_t b_addr) {
    switch (b_addr) {
      case 0x18:  // VRAM data write low
      case 0x19:  // VRAM data write high
      case 0x04:  // OAM data write
      case 0x22:  // CGRAM data write
        snes->AccumulateVramBytes(1);
        break;
      default:
        break;
    }
  };
  if (fromB) {
    uint8_t val = validB ? snes->ReadBBus(bAdr) : memory->open_bus();
    if (validA) {
      snes->AccumulateDmaBytes(1);
      record_vram(bAdr);
      snes->Write((aBank << 16) | aAdr, val);
    }
  } else {
    uint8_t val =
        validA ? snes->Read((aBank << 16) | aAdr) : memory->open_bus();
    if (validB) {
      snes->AccumulateDmaBytes(1);
      record_vram(bAdr);
      snes->WriteBBus(bAdr, val);
    }
  }
}

void StartDma(MemoryImpl* memory, uint8_t val, bool hdma) {
  auto channel = memory->dma_channels();
  for (int i = 0; i < 8; i++) {
    if (hdma) {
      channel[i].hdma_active = val & (1 << i);
    } else {
      channel[i].dma_active = val & (1 << i);
    }
  }
  if (!hdma) {
    memory->set_dma_state(val != 0 ? 1 : 0);
  }
}

}  // namespace emu
}  // namespace yaze
