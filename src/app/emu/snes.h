#include <cstdint>
#include <string>

#include "app/emu/apu.h"
#include "app/emu/cpu.h"
#include "app/emu/dbg.h"
#include "app/emu/ppu.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace emu {

class SNES {
 public:
  SNES() = default;
  ~SNES() = default;

  // Initialization
  void Init(ROM& rom);

  // Main emulation loop
  void Run();

  // Functions for CPU-related operations
  void Fetch();
  void Decode();
  void Execute();

  // Functions for PPU-related operations
  void RenderScanline();
  void UpdateSprites();
  void DrawBackgroundLayer(int layer);
  void DrawSprites();

  // Memory-related functions
  uint8_t ReadMemory(uint16_t address);
  void WriteMemory(uint16_t address, uint8_t value);

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
  // Components of the SNES
  MemoryImpl memory_;
  CPU cpu{memory_};
  PPU ppu{memory_};
  APU apu{memory_};

  // Helper classes
  Debugger debugger;

  // Other private member variables
  std::vector<uint8_t> rom_data;
  bool running_;
  uint16_t pc;
  uint32_t cycle;
  int scanline;
};

}  // namespace emu
}  // namespace app
}  // namespace yaze