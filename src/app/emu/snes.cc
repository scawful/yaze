#include "app/emu/snes.h"

#include <SDL_mixer.h>

#include <cstdint>
#include <memory>
#include <string>
#include <thread>

#include "app/emu/audio/apu.h"
#include "app/emu/audio/spc700.h"
#include "app/emu/clock.h"
#include "app/emu/cpu.h"
#include "app/emu/debug/debugger.h"
#include "app/emu/memory/memory.h"
#include "app/emu/video/ppu.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace emu {

namespace {

uint16_t GetHeaderOffset(const Memory& memory) {
  uint8_t mapMode = memory[(0x00 << 16) + 0xFFD5];
  uint16_t offset;

  switch (mapMode & 0x07) {
    case 0:  // LoROM
      // offset = 0x7F;
      offset = 0xFFC0;
      break;
    case 1:  // HiROM
      offset = 0xFFC0;
      break;
    case 5:  // ExHiROM
      offset = 0x40;
      break;
    default:
      throw std::invalid_argument(
          "Unable to locate supported ROM mapping mode in the provided ROM "
          "file. Please try another ROM file.");
  }

  return offset;
}

void audio_callback(void* userdata, uint8_t* stream, int len) {
  auto* apu = static_cast<APU*>(userdata);
  auto* buffer = reinterpret_cast<int16_t*>(stream);

  for (int i = 0; i < len / 2; i++) {  // Assuming 16-bit samples
    buffer[i] = apu->GetNextSample();  // This function should be implemented in
                                       // APU to fetch the next sample
  }
}

}  // namespace

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
  // Setup observers for the memory space
  memory_.AddObserver(&apu);
  memory_.AddObserver(&ppu);

  // Load the ROM into memory and set up the memory mapping
  memory_.Initialize(rom.vector());

  // Read the ROM header
  auto header_offset = GetHeaderOffset(memory_);
  rom_info_ = ReadRomHeader(header_offset);

  // Perform a long jump into a FastROM bank (if the ROM speed is FastROM)
  // Disable the emulation flag (switch to 65816 native mode)

  // Initialize CPU
  cpu.Init();
  cpu.PC = rom_info_.resetVector;

  // Initialize PPU
  ppu.Init();

  // Initialize APU
  apu.Init();

  // Initialize SDL_Mixer to play the audio samples
  // Mix_HookMusic(audio_callback, &apu);

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

  const double targetFPS = 60.0;  // 60 frames per second
  const double frame_time = 1.0 / targetFPS;
  double frame_accumulated_time = 0.0;

  auto last_time = std::chrono::high_resolution_clock::now();

  if (running_) {
    auto current_time = std::chrono::high_resolution_clock::now();
    double delta_time =
        std::chrono::duration<double>(current_time - last_time).count();
    last_time = current_time;

    frame_accumulated_time += delta_time;

    // Update the CPU
    cpu.UpdateClock(delta_time);
    cpu.Update();

    // Update the PPU
    ppu.UpdateClock(delta_time);
    ppu.Update();

    // Update the APU
    apu.UpdateClock(delta_time);
    apu.Update();

    if (frame_accumulated_time >= frame_time) {
      // renderer.Render();
      frame_accumulated_time -= frame_time;
    }

    HandleInput();
  }
}

// Enable NMI Interrupts
void SNES::EnableVBlankInterrupts() {
  v_blank_flag_ = false;

  // Clear the RDNMI VBlank flag
  memory_.ReadByte(0x4210);  // RDNMI

  // Enable vblank NMI interrupts and Joypad auto-read
  memory_.WriteByte(0x4200, 0x81);  // NMITIMEN
}

// Wait until the VBlank routine has been processed
void SNES::WaitForVBlank() {
  v_blank_flag_ = true;

  // Loop until `v_blank_flag_` is clear
  while (v_blank_flag_) {
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

  if (v_blank_flag_) {
    VBlankRoutine();

    // Clear `v_blank_flag_`
    v_blank_flag_ = false;
  }

  // Increment 32-bit frame_counter_
  frame_counter_++;

  // Restore CPU registers
  cpu.PHB();
}

// VBlank routine
void SNES::VBlankRoutine() {
  // Execute code that needs to run during VBlank, such as transferring data to
  // the PPU
}

void SNES::BootAPUWithIPL() {
  // 1. Waiting for the SPC700 to be ready
  while (!apu.IsReadySignalReceived()) {
    // Active waiting (this can be optimized)
  }

  // 2. Setting the starting address
  const uint16_t startAddress = 0x0200;
  memory_.WriteByte(0x2142, startAddress & 0xFF);  // Lower byte
  memory_.WriteByte(0x2143, startAddress >> 8);    // Upper byte
  memory_.WriteByte(0x2141, 0xCC);                 // Any non-zero value
  memory_.WriteByte(0x2140, 0xCC);                 // Signal to start

  const int DATA_SIZE = 0x1000;  // 4 KiB

  // 3. Sending data (simplified)
  // Assuming a buffer `audioData` containing the audio program/data
  uint8_t audioData[DATA_SIZE];  // Define DATA_SIZE and populate audioData as
                                 // needed
  for (int i = 0; i < DATA_SIZE; ++i) {
    memory_.WriteByte(0x2141, audioData[i]);
    memory_.WriteByte(0x2140, i & 0xFF);
    while (memory_.ReadByte(0x2140) != (i & 0xFF))
      ;  // Wait for acknowledgment
  }

  // 4. Running the SPC700 program
  memory_.WriteByte(0x2142, startAddress & 0xFF);  // Lower byte
  memory_.WriteByte(0x2143, startAddress >> 8);    // Upper byte
  memory_.WriteByte(0x2141, 0x00);                 // Zero to start the program
  memory_.WriteByte(0x2140, 0xCE);                 // Increment by 2
  while (memory_.ReadByte(0x2140) != 0xCE)
    ;  // Wait for acknowledgment
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