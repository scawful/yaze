#include "snes.h"

#include <cstdint>
#include <memory>
#include <string>
#include <thread>

#include "app/emu/apu.h"
#include "app/emu/cpu.h"
#include "app/emu/mem.h"
#include "app/emu/ppu.h"

namespace yaze {
namespace app {
namespace emu {

namespace {
uint8_t GetHeaderOffset(const Memory& memory) {
  uint8_t mapMode = memory[(0x00 << 16) + 0xFFD5];
  uint8_t offset;

  switch (mapMode & 0x07) {
    case 0:  // LoROM
      offset = 0x7F;
      break;
    case 1:  // HiROM
      offset = 0xFF;
      break;
    case 5:  // ExHiROM
      offset = 0x40;
      break;
    default:
      throw std::runtime_error("Unsupported map mode");
  }

  return offset;
}

}  // namespace

void DMA::StartDMATransfer(uint8_t channelMask) {
  for (int i = 0; i < 8; ++i) {
    if ((channelMask & (1 << i)) != 0) {
      Channel& ch = channels[i];

      // Validate channel parameters (e.g., DMAPn, BBADn, A1Tn, DASn)
      // ...

      // Determine the transfer direction based on the DMAPn register
      bool fromMemory = (ch.DMAPn & 0x80) != 0;

      // Determine the transfer size based on the DMAPn register
      bool transferTwoBytes = (ch.DMAPn & 0x40) != 0;

      // Perform the DMA transfer based on the channel parameters
      std::cout << "Starting DMA transfer for channel " << i << std::endl;

      for (uint16_t j = 0; j < ch.DASn; ++j) {
        // Read a byte or two bytes from memory based on the transfer size
        // ...

        // Write the data to the B-bus address (BBADn) if transferring from
        // memory
        // ...

        // Update the A1Tn register based on the transfer direction
        if (fromMemory) {
          ch.A1Tn += transferTwoBytes ? 2 : 1;
        } else {
          ch.A1Tn -= transferTwoBytes ? 2 : 1;
        }
      }

      // Update the channel registers after the transfer (e.g., A1Tn, DASn)
      // ...
    }
  }
  MDMAEN = channelMask;  // Set the MDMAEN register to the channel mask
}

void DMA::EnableHDMATransfers(uint8_t channelMask) {
  for (int i = 0; i < 8; ++i) {
    if ((channelMask & (1 << i)) != 0) {
      Channel& ch = channels[i];

      // Validate channel parameters (e.g., DMAPn, BBADn, A1Tn, A2An, NLTRn)
      // ...

      // Perform the HDMA setup based on the channel parameters
      std::cout << "Enabling HDMA transfer for channel " << i << std::endl;

      // Read the HDMA table from memory starting at A1Tn
      // ...

      // Update the A2An register based on the HDMA table
      // ...

      // Update the NLTRn register based on the HDMA table
      // ...
    }
  }
  HDMAEN = channelMask;  // Set the HDMAEN register to the channel mask
}

ROMInfo SNES::ReadRomHeader(uint32_t offset) {
  ROMInfo romInfo;

  // Read cartridge title
  char title[22];
  for (int i = 0; i < 21; ++i) {
    title[i] = cpu.ReadByte(offset + i);
  }
  title[21] = '\0';  // Null-terminate the string
  romInfo.title = std::string(title);

  // Read ROM speed and memory map mode
  uint8_t romSpeedAndMapMode = cpu.ReadByte(offset + 0x15);
  romInfo.romSpeed = (ROMSpeed)(romSpeedAndMapMode & 0x07);
  romInfo.bankSize = (BankSize)((romSpeedAndMapMode >> 5) & 0x01);

  // Read ROM type
  romInfo.romType = (ROMType)cpu.ReadByte(offset + 0x16);

  // Read ROM size
  romInfo.romSize = (ROMSize)cpu.ReadByte(offset + 0x17);

  // Read RAM size
  romInfo.sramSize = (SRAMSize)cpu.ReadByte(offset + 0x18);

  // Read country code
  romInfo.countryCode = (CountryCode)cpu.ReadByte(offset + 0x19);

  // Read license
  romInfo.license = (License)cpu.ReadByte(offset + 0x1A);

  // Read ROM version
  romInfo.version = cpu.ReadByte(offset + 0x1B);

  // Read checksum complement
  romInfo.checksumComplement = cpu.ReadWord(offset + 0x1E);

  // Read checksum
  romInfo.checksum = cpu.ReadWord(offset + 0x1C);

  // Read NMI VBL vector
  romInfo.nmiVblVector = cpu.ReadWord(offset + 0x3E);

  // Read reset vector
  romInfo.resetVector = cpu.ReadWord(offset + 0x3C);

  return romInfo;
}

void SNES::Init(ROM& rom) {
  // Load the ROM into memory and set up the memory mapping
  memory_.Initialize(rom.vector());

  // Read the ROM header
  auto header_offset = GetHeaderOffset(memory_);
  rom_info_ = ReadRomHeader(header_offset);

  // Perform a long jump into a FastROM bank (if the ROM speed is FastROM)
  // Disable the emulation flag (switch to 65816 native mode)s
  cpu.Init();

  // Initialize PPU
  ppu.Init();

  // Initialize APU
  apu.Init();

  // Disable interrupts and rendering
  memory_.WriteByte(0x4200, 0x00);  // NMITIMEN
  memory_.WriteByte(0x420C, 0x00);  // HDMAEN

  // Disable screen
  memory_.WriteByte(0x2100, 0x8F);  // INIDISP

  // Fill Work-RAM with zeros using two 64KiB fixed address DMA transfers to
  // WMDATA
  // TODO: Make this load from work ram, potentially in Memory class
  std::memset((void*)memory_.ram_.data(), 0, sizeof(memory_.ram_));

  // Reset PPU registers to a known good state
  memory_.WriteByte(0x4201, 0xFF);  // WRIO

  // Objects
  memory_.WriteByte(0x2101, 0x00);  // OBSEL
  memory_.WriteByte(0x2102, 0x00);  // OAMADDL
  memory_.WriteByte(0x2103, 0x00);  // OAMADDH

  // Backgrounds
  memory_.WriteByte(0x2105, 0x00);  // BGMODE
  memory_.WriteByte(0x2106, 0x00);  // MOSAIC

  memory_.WriteByte(0x2107, 0x00);  // BG1SC
  memory_.WriteByte(0x2108, 0x00);  // BG2SC
  memory_.WriteByte(0x2109, 0x00);  // BG3SC
  memory_.WriteByte(0x210A, 0x00);  // BG4SC

  memory_.WriteByte(0x210B, 0x00);  // BG12NBA
  memory_.WriteByte(0x210C, 0x00);  // BG34NBA

  // Scroll Registers
  memory_.WriteByte(0x210D, 0x00);  // BG1HOFS
  memory_.WriteByte(0x210E, 0xFF);  // BG1VOFS

  memory_.WriteByte(0x210F, 0x00);  // BG2HOFS
  memory_.WriteByte(0x2110, 0xFF);  // BG2VOFS

  memory_.WriteByte(0x2111, 0x00);  // BG3HOFS
  memory_.WriteByte(0x2112, 0xFF);  // BG3VOFS

  memory_.WriteByte(0x2113, 0x00);  // BG4HOFS
  memory_.WriteByte(0x2114, 0xFF);  // BG4VOFS

  // VRAM Registers
  memory_.WriteByte(0x2115, 0x80);  // VMAIN

  // Mode 7
  memory_.WriteByte(0x211A, 0x00);  // M7SEL
  memory_.WriteByte(0x211B, 0x01);  // M7A
  memory_.WriteByte(0x211C, 0x00);  // M7B
  memory_.WriteByte(0x211D, 0x00);  // M7C
  memory_.WriteByte(0x211E, 0x01);  // M7D
  memory_.WriteByte(0x211F, 0x00);  // M7X
  memory_.WriteByte(0x2120, 0x00);  // M7Y

  // Windows
  memory_.WriteByte(0x2123, 0x00);  // W12SEL
  memory_.WriteByte(0x2124, 0x00);  // W34SEL
  memory_.WriteByte(0x2125, 0x00);  // WOBJSEL
  memory_.WriteByte(0x2126, 0x00);  // WH0
  memory_.WriteByte(0x2127, 0x00);  // WH1
  memory_.WriteByte(0x2128, 0x00);  // WH2
  memory_.WriteByte(0x2129, 0x00);  // WH3
  memory_.WriteByte(0x212A, 0x00);  // WBGLOG
  memory_.WriteByte(0x212B, 0x00);  // WOBJLOG

  // Layer Enable
  memory_.WriteByte(0x212C, 0x00);  // TM
  memory_.WriteByte(0x212D, 0x00);  // TS
  memory_.WriteByte(0x212E, 0x00);  // TMW
  memory_.WriteByte(0x212F, 0x00);  // TSW

  // Color Math
  memory_.WriteByte(0x2130, 0x30);  // CGWSEL
  memory_.WriteByte(0x2131, 0x00);  // CGADSUB
  memory_.WriteByte(0x2132, 0xE0);  // COLDATA

  // Misc
  memory_.WriteByte(0x2133, 0x00);  // SETINI

  running_ = true;
  scanline = 0;
}

void SNES::Run() {
  running_ = true;

  const int cpuClockSpeed = 21477272;  // 21.477272 MHz
  const int ppuClockSpeed = 5369318;   // 5.369318 MHz
  const int apuClockSpeed = 32000;     // 32 KHz
  const double targetFPS = 60.0;       // 60 frames per second

  const double cpuCycleTime = 1.0 / cpuClockSpeed;
  const double ppuCycleTime = 1.0 / ppuClockSpeed;
  const double apuCycleTime = 1.0 / apuClockSpeed;
  const double frameTime = 1.0 / targetFPS;

  double cpuAccumulatedTime = 0.0;
  double ppuAccumulatedTime = 0.0;
  double apuAccumulatedTime = 0.0;
  double frameAccumulatedTime = 0.0;

  auto lastTime = std::chrono::high_resolution_clock::now();

  while (running_) {
    auto currentTime = std::chrono::high_resolution_clock::now();
    double deltaTime =
        std::chrono::duration<double>(currentTime - lastTime).count();
    lastTime = currentTime;

    cpuAccumulatedTime += deltaTime;
    ppuAccumulatedTime += deltaTime;
    apuAccumulatedTime += deltaTime;
    frameAccumulatedTime += deltaTime;

    while (cpuAccumulatedTime >= cpuCycleTime) {
      cpu.ExecuteInstruction(cpu.ReadByte(cpu.PC));
      cpuAccumulatedTime -= cpuCycleTime;
    }

    while (ppuAccumulatedTime >= ppuCycleTime) {
      RenderScanline();
      ppuAccumulatedTime -= ppuCycleTime;
    }

    while (apuAccumulatedTime >= apuCycleTime) {
      // apu.Update();
      apuAccumulatedTime -= apuCycleTime;
    }

    if (frameAccumulatedTime >= frameTime) {
      // renderer.Render();
      frameAccumulatedTime -= frameTime;
    }

    HandleInput();
  }
}

// Enable NMI Interrupts
void SNES::EnableVBlankInterrupts() {
  vBlankFlag = false;

  // Clear the RDNMI VBlank flag
  memory_.ReadByte(0x4210);  // RDNMI

  // Enable vblank NMI interrupts and Joypad auto-read
  memory_.WriteByte(0x4200, 0x81);  // NMITIMEN
}

// Wait until the VBlank routine has been processed
void SNES::WaitForVBlank() {
  vBlankFlag = true;

  // Loop until `vBlankFlag` is clear
  while (vBlankFlag) {
    std::this_thread::yield();
  }
}

// NMI Interrupt Service Routine
void SNES::NmiIsr() {
  // Switch to a FastROM bank (assuming NmiIsr is in bank 0x80)
  // ...

  // Push CPU registers to stack
  cpu.PHP();

  // Reset DB and DP registers
  cpu.DB = 0x80;  // Assuming bank 0x80, can be changed to 0x00
  cpu.D = 0;

  if (vBlankFlag) {
    VBlankRoutine();

    // Clear `vBlankFlag`
    vBlankFlag = false;
  }

  // Increment 32-bit frameCounter
  frameCounter++;

  // Restore CPU registers
  cpu.PHB();
}

// VBlank routine
void SNES::VBlankRoutine() {
  // Execute code that needs to run during VBlank, such as transferring data to
  // the PPU
  // ...
}

void SNES::RenderScanline() {
  // Render background layers
  for (int layer = 0; layer < 4; layer++) {
    DrawBackgroundLayer(layer);
  }

  // Render sprites
  DrawSprites();
}

void SNES::DrawBackgroundLayer(int layer) {
  // ...
}

void SNES::DrawSprites() {
  // ...
}

void SNES::HandleInput() {
  // ...
}

void SNES::SaveState(const std::string& path) {
  // ...
}

void SNES::LoadState(const std::string& path) {
  // ...
}

void SNES::Debug() {
  // ...
}

void SNES::Breakpoint(uint16_t address) {
  // ...
}

}  // namespace emu
}  // namespace app
}  // namespace yaze