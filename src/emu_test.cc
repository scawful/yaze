// Headless Emulator Test Harness
// Minimal SDL initialization for testing APU without GUI overhead

#include "app/platform/sdl_compat.h"

#include <cstdint>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/emu/snes.h"
#include "util/log.h"

ABSL_FLAG(std::string, emu_test_rom, "", "Path to ROM file to test");
ABSL_FLAG(int, max_frames, 60, "Maximum frames to run (0 = infinite)");
ABSL_FLAG(int, log_interval, 10, "Log APU state every N frames");
ABSL_FLAG(bool, dump_audio, false, "Dump audio output to WAV file");
ABSL_FLAG(std::string, audio_file, "apu_test.wav", "Audio dump filename");
ABSL_FLAG(bool, verbose, false, "Enable verbose logging");
ABSL_FLAG(bool, trace_apu, false, "Enable detailed APU instruction tracing");

namespace yaze {
namespace emu {
namespace test {

class HeadlessEmulator {
 public:
  HeadlessEmulator() = default;
  ~HeadlessEmulator() { Cleanup(); }

  absl::Status Init(const std::string& rom_path) {
    // Initialize minimal SDL (audio + events only)
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_EVENTS) < 0) {
      return absl::InternalError(
          absl::StrCat("SDL_Init failed: ", SDL_GetError()));
    }
    sdl_initialized_ = true;

    // Load ROM file
    FILE* file = fopen(rom_path.c_str(), "rb");
    if (!file) {
      return absl::NotFoundError(
          absl::StrCat("Failed to open ROM: ", rom_path));
    }

    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);

    rom_data_.resize(size);
    if (fread(rom_data_.data(), 1, size, file) != size) {
      fclose(file);
      return absl::InternalError("Failed to read ROM data");
    }
    fclose(file);

    printf("Loaded ROM: %zu bytes\n", size);

    // Initialize SNES emulator
    snes_ = std::make_unique<Snes>();
    snes_->Init(rom_data_);
    snes_->Reset(true);

    printf("SNES initialized and reset\n");
    printf("APU PC after reset: $%04X\n", snes_->apu().spc700().PC);
    printf("APU cycles: %llu\n", snes_->apu().GetCycles());

    return absl::OkStatus();
  }

  absl::Status RunFrames(int max_frames, int log_interval) {
    int frame = 0;
    bool infinite = (max_frames == 0);

    printf("Starting emulation (max_frames=%d, log_interval=%d)\n", max_frames,
           log_interval);

    while (infinite || frame < max_frames) {
      // Run one frame
      snes_->RunFrame();
      frame++;

      // Periodic APU state logging
      if (log_interval > 0 && frame % log_interval == 0) {
        LogApuState(frame);
      }

      // Check for exit events
      SDL_Event event;
      while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
          printf("SDL_QUIT received, stopping emulation\n");
          return absl::OkStatus();
        }
      }

      // Check for stuck APU (PC not advancing)
      if (frame % 60 == 0) {
        uint16_t current_pc = snes_->apu().spc700().PC;
        if (current_pc == last_pc_ && frame > 60) {
          stuck_counter_++;
          if (stuck_counter_ > 5) {
            fprintf(stderr, "ERROR: APU stuck at PC=$%04X for %d frames\n",
                    current_pc, stuck_counter_ * 60);
            fprintf(stderr,
                    "ERROR: This likely indicates a hang or infinite loop\n");
            return absl::InternalError("APU stuck in infinite loop");
          }
        } else {
          stuck_counter_ = 0;
        }
        last_pc_ = current_pc;
      }
    }

    printf("Emulation complete: %d frames\n", frame);
    return absl::OkStatus();
  }

 private:
  void LogApuState(int frame) {
    auto& apu = snes_->apu();
    auto& spc = apu.spc700();
    auto& tracker = snes_->apu_handshake_tracker();

    printf("=== Frame %d APU State ===\n", frame);
    printf("  SPC700 PC: $%04X\n", spc.PC);
    printf("  SPC700 A:  $%02X\n", spc.A);
    printf("  SPC700 X:  $%02X\n", spc.X);
    printf("  SPC700 Y:  $%02X\n", spc.Y);
    printf("  SPC700 SP: $%02X\n", spc.SP);
    printf("  SPC700 PSW: N=%d V=%d P=%d B=%d H=%d I=%d Z=%d C=%d\n", spc.PSW.N,
           spc.PSW.V, spc.PSW.P, spc.PSW.B, spc.PSW.H, spc.PSW.I, spc.PSW.Z,
           spc.PSW.C);
    printf("  APU Cycles: %llu\n", apu.GetCycles());

    // Port status
    printf("  Input Ports:  F4=$%02X F5=$%02X F6=$%02X F7=$%02X\n",
           apu.in_ports_[0], apu.in_ports_[1], apu.in_ports_[2],
           apu.in_ports_[3]);
    printf("  Output Ports: F4=$%02X F5=$%02X F6=$%02X F7=$%02X\n",
           apu.out_ports_[0], apu.out_ports_[1], apu.out_ports_[2],
           apu.out_ports_[3]);

    // Handshake phase
    const char* handshake_phase = "UNKNOWN";
    switch (tracker.GetPhase()) {
      case debug::ApuHandshakeTracker::Phase::RESET:
        handshake_phase = "RESET";
        break;
      case debug::ApuHandshakeTracker::Phase::IPL_BOOT:
        handshake_phase = "IPL_BOOT";
        break;
      case debug::ApuHandshakeTracker::Phase::WAITING_BBAA:
        handshake_phase = "WAITING_BBAA";
        break;
      case debug::ApuHandshakeTracker::Phase::HANDSHAKE_CC:
        handshake_phase = "HANDSHAKE_CC";
        break;
      case debug::ApuHandshakeTracker::Phase::TRANSFER_ACTIVE:
        handshake_phase = "TRANSFER_ACTIVE";
        break;
      case debug::ApuHandshakeTracker::Phase::TRANSFER_DONE:
        handshake_phase = "TRANSFER_DONE";
        break;
      case debug::ApuHandshakeTracker::Phase::RUNNING:
        handshake_phase = "RUNNING";
        break;
    }
    printf("  Handshake: %s\n", handshake_phase);

    // Zero page (used by IPL ROM)
    auto& ram = apu.ram;
    printf("  Zero Page: $00=$%02X $01=$%02X $02=$%02X $03=$%02X\n", ram[0x00],
           ram[0x01], ram[0x02], ram[0x03]);

    // Check reset vector
    uint16_t reset_vector =
        static_cast<uint16_t>(ram[0xFFFE] | (ram[0xFFFF] << 8));
    printf("  Reset Vector ($FFFE-$FFFF): $%04X\n", reset_vector);
  }

  void Cleanup() {
    if (sdl_initialized_) {
      SDL_Quit();
      sdl_initialized_ = false;
    }
  }

  std::unique_ptr<Snes> snes_;
  std::vector<uint8_t> rom_data_;
  bool sdl_initialized_ = false;
  uint16_t last_pc_ = 0;
  int stuck_counter_ = 0;
};

}  // namespace test
}  // namespace emu
}  // namespace yaze

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);

  // Configure logging
  std::string rom_path = absl::GetFlag(FLAGS_emu_test_rom);
  int max_frames = absl::GetFlag(FLAGS_max_frames);
  int log_interval = absl::GetFlag(FLAGS_log_interval);
  bool verbose = absl::GetFlag(FLAGS_verbose);
  bool trace_apu = absl::GetFlag(FLAGS_trace_apu);

  if (rom_path.empty()) {
    std::cerr << "Error: --emu_test_rom flag is required\n";
    std::cerr << "Usage: yaze_emu_test --emu_test_rom=zelda3.sfc [options]\n";
    std::cerr << "\nOptions:\n";
    std::cerr << "  --emu_test_rom=PATH Path to ROM file (required)\n";
    std::cerr << "  --max_frames=N      Run for N frames (0=infinite, "
                 "default=60)\n";
    std::cerr << "  --log_interval=N    Log APU state every N frames "
                 "(default=10)\n";
    std::cerr << "  --verbose           Enable verbose logging\n";
    std::cerr << "  --trace_apu         Enable detailed APU instruction "
                 "tracing\n";
    return 1;
  }

  // Set log level
  if (verbose) {
    // Enable all logging
    std::cout << "Verbose logging enabled\n";
  }

  // Create and run headless emulator
  yaze::emu::test::HeadlessEmulator emulator;

  auto status = emulator.Init(rom_path);
  if (!status.ok()) {
    std::cerr << "Initialization failed: " << status.message() << "\n";
    return 1;
  }

  status = emulator.RunFrames(max_frames, log_interval);
  if (!status.ok()) {
    std::cerr << "Emulation failed: " << status.message() << "\n";
    return 1;
  }

  std::cout << "Test completed successfully\n";
  return 0;
}
