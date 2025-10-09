#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

#include "app/emu/snes.h"
#include "app/rom.h"

using namespace yaze;

/**
 * @brief Dumps the state of WRAM and CPU/PPU registers after initialization.
 *
 * This tool runs the SNES emulator from the Reset vector until it hits the
 * main game loop. It then dumps the entire state of WRAM and relevant registers
 * to a header file. This captured state can be used to initialize the emulator
 * for test harnesses, providing a realistic environment for executing native
 * game code.
 */
class DungeonTestHarness {
 public:
  explicit DungeonTestHarness(const std::string& rom_path)
      : rom_path_(rom_path) {}

  absl::Status GenerateHarnessState(const std::string& output_path) {
    // Load ROM
    Rom rom;
    RETURN_IF_ERROR(rom.LoadFromFile(rom_path_));
    auto rom_data = rom.vector();

    // Initialize SNES
    emu::Snes snes;
    snes.Init(rom_data);
    snes.Reset(false);

    auto& cpu = snes.cpu();
    auto& ppu = snes.ppu();

    // Run emulator until the main game loop is reached
    int max_cycles = 5000000;  // 5 million cycles should be plenty
    int cycles = 0;
    while (cycles < max_cycles) {
      snes.RunCycle();
      cycles++;
      if (cpu.PB == 0x00 && cpu.PC == 0x8034) {
        break;  // Reached MainGameLoop
      }
    }

    if (cycles >= max_cycles) {
      return absl::InternalError("Emulator timed out; did not reach main game loop.");
    }

    std::ofstream out_file(output_path);
    if (!out_file.is_open()) {
      return absl::InternalError("Failed to open output file: " + output_path);
    }

    // Write header
    out_file << "// =============================================================================" << std::endl;
    out_file << "// YAZE Dungeon Test Harness State - Generated from: " << rom_path_ << std::endl;
    out_file << "// Generated on: " << __DATE__ << " " << __TIME__ << std::endl;
    out_file << "// =============================================================================" << std::endl;
    out_file << std::endl;
    out_file << "#pragma once" << std::endl;
    out_file << std::endl;
    out_file << "#include <cstdint>" << std::endl;
    out_file << "#include <array>" << std::endl;
    out_file << std::endl;
    out_file << "namespace yaze {" << std::endl;
    out_file << "namespace emu {" << std::endl;
    out_file << std::endl;

    // Write WRAM state
    out_file << "constexpr std::array<uint8_t, 0x20000> kInitialWRAMState = {{" << std::endl;
    for (int i = 0; i < 0x20000; ++i) {
      if (i % 16 == 0) out_file << "    ";
      out_file << "0x" << std::hex << std::setw(2) << std::setfill('0')
               << static_cast<int>(snes.Read(0x7E0000 + i));
      if (i < 0x1FFFF) out_file << ", ";
      if (i % 16 == 15) out_file << std::endl;
    }
    out_file << "}};" << std::endl << std::endl;

    // Write CPU/PPU register state
    out_file << "// =============================================================================" << std::endl;
    out_file << "// Initial Register States" << std::endl;
    out_file << "// =============================================================================" << std::endl;
    out_file << std::endl;
    
    out_file << "struct InitialPpuState {" << std::endl;
    out_file << "    uint8_t inidisp = 0x" << std::hex << ppu.Read(0x2100, false) << ";" << std::endl;
    out_file << "    uint8_t objsel = 0x" << std::hex << ppu.Read(0x2101, false) << ";" << std::endl;
    out_file << "    uint8_t bgmode = 0x" << std::hex << ppu.Read(0x2105, false) << ";" << std::endl;
    out_file << "    uint8_t mosaic = 0x" << std::hex << ppu.Read(0x2106, false) << ";" << std::endl;
    out_file << "    uint8_t tm = 0x" << std::hex << ppu.Read(0x212C, false) << ";" << std::endl;
    out_file << "    uint8_t ts = 0x" << std::hex << ppu.Read(0x212D, false) << ";" << std::endl;
    out_file << "    uint8_t cgwsel = 0x" << std::hex << ppu.Read(0x2130, false) << ";" << std::endl;
    out_file << "    uint8_t cgadsub = 0x" << std::hex << ppu.Read(0x2131, false) << ";" << std::endl;
    out_file << "    uint8_t setini = 0x" << std::hex << ppu.Read(0x2133, false) << ";" << std::endl;
    out_file << "};" << std::endl << std::endl;

    out_file << "}  // namespace emu" << std::endl;
    out_file << "}  // namespace yaze" << std::endl;

    return absl::OkStatus();
  }

 private:
  std::string rom_path_;
};

int main(int argc, char* argv[]) {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " <rom_path> <output_path>" << std::endl;
    return 1;
  }

  std::string rom_path = argv[1];
  std::string output_path = argv[2];

  if (!std::filesystem::exists(rom_path)) {
    std::cerr << "Error: ROM file not found: " << rom_path << std::endl;
    return 1;
  }

  DungeonTestHarness harness(rom_path);
  auto status = harness.GenerateHarnessState(output_path);

  if (status.ok()) {
    std::cout << "Successfully generated dungeon harness state from " << rom_path
              << " to " << output_path << std::endl;
    return 0;
  } else {
    std::cerr << "Error generating harness state: " << status.message() << std::endl;
    return 1;
  }
}
