#include "app/emu/snes.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/emu/audio/apu.h"
#include "app/emu/memory/dma.h"
#include "app/emu/memory/memory.h"
#include "app/emu/render/render_context.h"
#include "app/emu/video/ppu.h"
#include "util/log.h"

#define RETURN_IF_ERROR(expr)      \
  do {                             \
    absl::Status _status = (expr); \
    if (!_status.ok()) {           \
      return _status;              \
    }                              \
  } while (0)

namespace yaze {
namespace emu {

namespace {
void input_latch(Input* input, bool value) {
  input->latch_line_ = value;
  if (input->latch_line_) {
    input->latched_state_ = input->current_state_;
  }
}

uint8_t input_read(Input* input) {
  if (input->latch_line_)
    input->latched_state_ = input->current_state_;

  // Invert state for serial line: 1 (Pressed) -> 0, 0 (Released) -> 1
  // This matches SNES hardware Active Low logic.
  // Also ensures shifting in 0s results in 1s (Released) for bits 17+.
  uint8_t ret = (~input->latched_state_) & 1;

  input->latched_state_ >>= 1;
  return ret;
}

bool IsLittleEndianHost() {
  uint16_t test = 1;
  return *reinterpret_cast<uint8_t*>(&test) == 1;
}

constexpr uint32_t kStateMagic = 0x59415A45;  // 'YAZE'
constexpr uint32_t kStateFormatVersion = 2;
constexpr uint32_t kMaxChunkSize = 16 * 1024 * 1024;  // 16MB safety cap

constexpr uint32_t MakeTag(char a, char b, char c, char d) {
  return static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8) |
         (static_cast<uint32_t>(c) << 16) | (static_cast<uint32_t>(d) << 24);
}

absl::Status WriteBytes(std::ostream& out, const void* data, size_t size) {
  out.write(reinterpret_cast<const char*>(data), size);
  if (!out) {
    return absl::InternalError("Failed to write bytes to state stream");
  }
  return absl::OkStatus();
}

absl::Status ReadBytes(std::istream& in, void* data, size_t size) {
  in.read(reinterpret_cast<char*>(data), size);
  if (!in) {
    return absl::InternalError("Failed to read bytes from state stream");
  }
  return absl::OkStatus();
}

absl::Status WriteUint32LE(std::ostream& out, uint32_t value) {
  std::array<uint8_t, 4> bytes = {static_cast<uint8_t>(value & 0xFF),
                                  static_cast<uint8_t>((value >> 8) & 0xFF),
                                  static_cast<uint8_t>((value >> 16) & 0xFF),
                                  static_cast<uint8_t>(value >> 24)};
  return WriteBytes(out, bytes.data(), bytes.size());
}

absl::Status ReadUint32LE(std::istream& in, uint32_t* value) {
  std::array<uint8_t, 4> bytes{};
  auto status = ReadBytes(in, bytes.data(), bytes.size());
  if (!status.ok()) {
    return status;
  }
  *value = static_cast<uint32_t>(bytes[0]) |
           (static_cast<uint32_t>(bytes[1]) << 8) |
           (static_cast<uint32_t>(bytes[2]) << 16) |
           (static_cast<uint32_t>(bytes[3]) << 24);
  return absl::OkStatus();
}

template <typename T>
absl::Status WriteScalar(std::ostream& out, T value) {
  static_assert(std::is_trivially_copyable<T>::value,
                "Only trivial scalars supported");
  std::array<uint8_t, sizeof(T)> buffer{};
  std::memcpy(buffer.data(), &value, sizeof(T));
  return WriteBytes(out, buffer.data(), buffer.size());
}

template <typename T>
absl::Status ReadScalar(std::istream& in, T* value) {
  static_assert(std::is_trivially_copyable<T>::value,
                "Only trivial scalars supported");
  std::array<uint8_t, sizeof(T)> buffer{};
  auto status = ReadBytes(in, buffer.data(), buffer.size());
  if (!status.ok()) {
    return status;
  }
  std::memcpy(value, buffer.data(), sizeof(T));
  return absl::OkStatus();
}

struct ChunkHeader {
  uint32_t tag;
  uint32_t version;
  uint32_t size;
  uint32_t crc32;
};

absl::Status WriteChunk(std::ostream& out, uint32_t tag, uint32_t version,
                        const std::string& payload) {
  if (payload.size() > kMaxChunkSize) {
    return absl::FailedPreconditionError("Serialized chunk too large");
  }
  ChunkHeader header{
      tag, version, static_cast<uint32_t>(payload.size()),
      render::CalculateCRC32(reinterpret_cast<const uint8_t*>(payload.data()),
                             payload.size())};

  RETURN_IF_ERROR(WriteUint32LE(out, header.tag));
  RETURN_IF_ERROR(WriteUint32LE(out, header.version));
  RETURN_IF_ERROR(WriteUint32LE(out, header.size));
  RETURN_IF_ERROR(WriteUint32LE(out, header.crc32));
  RETURN_IF_ERROR(WriteBytes(out, payload.data(), payload.size()));
  return absl::OkStatus();
}

absl::Status ReadChunkHeader(std::istream& in, ChunkHeader* header) {
  RETURN_IF_ERROR(ReadUint32LE(in, &header->tag));
  RETURN_IF_ERROR(ReadUint32LE(in, &header->version));
  RETURN_IF_ERROR(ReadUint32LE(in, &header->size));
  RETURN_IF_ERROR(ReadUint32LE(in, &header->crc32));
  return absl::OkStatus();
}

}  // namespace

void Snes::Init(const std::vector<uint8_t>& rom_data) {
  LOG_DEBUG("SNES", "Initializing emulator with ROM size %zu bytes",
            rom_data.size());

  // Initialize the CPU, PPU, and APU
  ppu_.Init();
  apu_.Init();

  // Connect handshake tracker to APU for debugging
  apu_.set_handshake_tracker(&apu_handshake_tracker_);

  // Load the ROM into memory and set up the memory mapping
  memory_.Initialize(rom_data);
  Reset(true);

  running_ = true;
  LOG_DEBUG("SNES", "Emulator initialization complete");
}

void Snes::Reset(bool hard) {
  LOG_DEBUG("SNES", "Reset called (hard=%d)", hard);
  cpu_.Reset(hard);
  apu_.Reset();
  ppu_.Reset();
  ResetDma(&memory_);
  input1.latch_line_ = false;
  input2.latch_line_ = false;
  input1.current_state_ = 0;  // Clear current button states
  input2.current_state_ = 0;  // Clear current button states
  input1.latched_state_ = 0;
  input2.latched_state_ = 0;
  input1.previous_state_ = 0;
  input2.previous_state_ = 0;
  if (hard)
    memset(ram, 0, sizeof(ram));
  ram_adr_ = 0;
  memory_.set_h_pos(0);
  memory_.set_v_pos(0);
  frames_ = 0;
  cycles_ = 0;
  sync_cycle_ = 0;
  apu_catchup_cycles_ = 0.0;
  h_irq_enabled_ = false;
  v_irq_enabled_ = false;
  nmi_enabled_ = false;
  h_timer_ = 0x1ff * 4;
  v_timer_ = 0x1ff;
  in_nmi_ = false;
  irq_condition_ = false;
  in_irq_ = false;
  in_vblank_ = false;
  memset(port_auto_read_, 0, sizeof(port_auto_read_));
  auto_joy_read_ = false;
  auto_joy_timer_ = 0;
  ppu_latch_ = false;
  multiply_a_ = 0xff;
  multiply_result_ = 0xFE01;
  divide_a_ = 0xffFF;
  divide_result_ = 0x101;
  fast_mem_ = false;
  memory_.set_open_bus(0);
  next_horiz_event = 16;
  InitAccessTime(false);
  LOG_DEBUG("SNES", "Reset complete - CPU will start at $%02X:%04X", cpu_.PB,
            cpu_.PC);
}

void Snes::RunFrame() {
  while (in_vblank_) {
    cpu_.RunOpcode();
  }

  uint32_t frame = frames_;

  while (!in_vblank_ && frame == frames_) {
    cpu_.RunOpcode();
  }
}

void Snes::RunAudioFrame() {
  // Audio-focused frame execution: runs CPU+APU but skips PPU rendering
  // This maintains CPU-APU communication timing while reducing overhead
  // Used by MusicEditor for authentic audio playback
  // Note: PPU registers are still writable, but rendering logic (StartLine/RunLine)
  // in RunCycle() is skipped when audio_only_mode_ is true.

  audio_only_mode_ = true;

  // Run through vblank if we're in it
  while (in_vblank_) {
    cpu_.RunOpcode();
  }

  uint32_t frame = frames_;

  // Run until next vblank
  while (!in_vblank_ && frame == frames_) {
    cpu_.RunOpcode();
  }

  audio_only_mode_ = false;
}

void Snes::CatchUpApu() {
  // Bring APU up to the same master cycle count since last catch-up.
  // cycles_ is monotonically increasing in RunCycle().
  apu_.RunCycles(cycles_);
}

void Snes::HandleInput() {
  // IMPORTANT: Clear and repopulate auto-read data
  // This data persists until the next call, allowing NMI to read it
  memset(port_auto_read_, 0, sizeof(port_auto_read_));

  // Populate port_auto_read_ non-destructively by reading current_state_ directly.
  // The SNES shifts out 16 bits: B, Y, Select, Start, Up, Down, Left, Right, A, X, L, R, 0, 0, 0, 0
  // This corresponds to bits 0-11 of our input state.
  // Note: This assumes current_state_ matches the SNES controller bit order:
  // Bit 0: B, 1: Y, 2: Select, 3: Start, 4: Up, 5: Down, 6: Left, 7: Right, 8: A, 9: X, 10: L, 11: R
  uint16_t latched1 = input1.current_state_;
  uint16_t latched2 = input2.current_state_;
  // if (input1.previous_state_ != input1.current_state_) {
  //   LOG_DEBUG("HandleInput: P1 state 0x%04X -> 0x%04X", input1.previous_state_,
  //             input1.current_state_);
  // }
  // if (input2.previous_state_ != input2.current_state_) {
  //   LOG_DEBUG("HandleInput: P2 state 0x%04X -> 0x%04X", input2.previous_state_,
  //             input2.current_state_);
  // }
  for (int i = 0; i < 16; i++) {
    // Read bit i from current state (0 for bits >= 12)
    uint8_t val1 = (latched1 >> i) & 1;
    uint8_t val2 = (latched2 >> i) & 1;

    // Store in port_auto_read_ (Big Endian format for registers $4218-$421F)
    // port_auto_read_[0/1] gets bits 0-15 shifted into position
    port_auto_read_[0] |= (val1 << (15 - i));
    port_auto_read_[1] |= (val2 << (15 - i));
    // port_auto_read_[2/3] remain 0 as standard controllers don't use them
  }

  // Debug: Log input state when buttons are pressed
  static int input_log_count = 0;
  if (latched1 != 0 && input_log_count++ < 50) {
    LOG_INFO("Input", "current_state=0x%04X -> $4218=0x%02X $4219=0x%02X",
             latched1, port_auto_read_[0] & 0xFF, port_auto_read_[0] >> 8);
  }

  // Store previous state for edge detection
  // Do this here instead of after a destructive latch sequence
  input1.previous_state_ = input1.current_state_;
  input2.previous_state_ = input2.current_state_;

  // Make auto-joypad data immediately available; real hardware has a delay,
  // but our current timing can leave it busy when NMI reads $4218/4219.
  auto_joy_timer_ = 0;
}

void Snes::RunCycle() {
  cycles_ += 2;

  // check for h/v timer irq's
  bool condition = ((v_irq_enabled_ || h_irq_enabled_) &&
                    (memory_.v_pos() == v_timer_ || !v_irq_enabled_) &&
                    (memory_.h_pos() == h_timer_ || !h_irq_enabled_));

  if (!irq_condition_ && condition) {
    in_irq_ = true;
    cpu_.SetIrq(true);
  }
  irq_condition_ = condition;

  // increment position; must come after irq checks! (hagane, cybernator)
  memory_.set_h_pos(memory_.h_pos() + 2);

  // handle positional stuff
  if (memory_.h_pos() == next_horiz_event) {
    switch (memory_.h_pos()) {
      case 16: {
        next_horiz_event = 512;
        if (memory_.v_pos() == 0)
          memory_.init_hdma_request();

        // Start PPU line rendering (setup for JIT rendering)
        // Skip in audio-only mode for performance
        if (!audio_only_mode_ && !in_vblank_ && memory_.v_pos() > 0)
          ppu_.StartLine(memory_.v_pos());
      } break;
      case 512: {
        next_horiz_event = 1104;
        // Render the line halfway of the screen for better compatibility
        // Using CatchUp instead of RunLine for progressive rendering
        // Skip in audio-only mode for performance
        if (!audio_only_mode_ && !in_vblank_ && memory_.v_pos() > 0)
          ppu_.CatchUp(512);
      } break;
      case 1104: {
        // Finish rendering the visible line
        // Skip in audio-only mode for performance
        if (!audio_only_mode_ && !in_vblank_ && memory_.v_pos() > 0)
          ppu_.CatchUp(1104);

        if (!in_vblank_)
          memory_.run_hdma_request();
        if (!memory_.pal_timing()) {
          // line 240 of odd frame with no interlace is 4 cycles shorter
          next_horiz_event = (memory_.v_pos() == 240 && !ppu_.even_frame &&
                              !ppu_.frame_interlace)
                                 ? 1360
                                 : 1364;
        } else {
          // line 311 of odd frame with interlace is 4 cycles longer
          next_horiz_event = (memory_.v_pos() != 311 || ppu_.even_frame ||
                              !ppu_.frame_interlace)
                                 ? 1364
                                 : 1368;
        }
      } break;
      case 1360:
      case 1364:
      case 1368: {  // this is the end (of the h-line)
        next_horiz_event = 16;

        memory_.set_h_pos(0);
        memory_.set_v_pos(memory_.v_pos() + 1);
        if (!memory_.pal_timing()) {
          // even interlace frame is 263 lines
          if ((memory_.v_pos() == 262 &&
               (!ppu_.frame_interlace || !ppu_.even_frame)) ||
              memory_.v_pos() == 263) {
            memory_.set_v_pos(0);
            frames_++;
            static int frame_log = 0;
            if (++frame_log % 60 == 0)
              LOG_INFO("SNES", "Frames incremented 60 times");
          }
        } else {
          // even interlace frame is 313 lines
          if ((memory_.v_pos() == 312 &&
               (!ppu_.frame_interlace || !ppu_.even_frame)) ||
              memory_.v_pos() == 313) {
            memory_.set_v_pos(0);
            frames_++;
            static int frame_log_pal = 0;
            if (++frame_log_pal % 60 == 0)
              LOG_INFO("SNES", "Frames (PAL) incremented 60 times");
          }
        }

        // end of hblank, do most memory_.v_pos()-tests
        bool starting_vblank = false;
        if (memory_.v_pos() == 0) {
          // end of vblank
          static int vblank_end_count = 0;
          if (vblank_end_count++ < 10) {
            LOG_DEBUG(
                "SNES",
                "VBlank END - v_pos=0, setting in_vblank_=false at frame %d",
                frames_);
          }
          in_vblank_ = false;
          in_nmi_ = false;
          ppu_.HandleFrameStart();
        } else if (memory_.v_pos() == 225) {
          // ask the ppu if we start vblank now or at memory_.v_pos() 240
          // (overscan)
          starting_vblank = !ppu_.CheckOverscan();
        } else if (memory_.v_pos() == 240) {
          // if we are not yet in vblank, we had an overscan frame, set
          // starting_vblank
          if (!in_vblank_)
            starting_vblank = true;
        }
        if (starting_vblank) {
          // catch up the apu at end of emulated frame (we end frame @ start of
          // vblank)
          CatchUpApu();
          // IMPORTANT: This is the ONLY location where NewFrame() should be called
          // during frame execution. It marks the DSP sample boundary at vblank start,
          // after CatchUpApu() has synced the APU. Do NOT call NewFrame() from
          // Emulator::RunAudioFrame() - that causes incorrect frame boundary timing
          // and results in audio playing at wrong speed (1.5x due to 48000/32040 ratio).
          // This also handles games where DMA extends past vblank (Megaman X2 titlescreen,
          // Tales of Phantasia demo, Actraiser 2 fade-in).
          apu_.dsp().NewFrame();
          // we are starting vblank
          ppu_.HandleVblank();

          static int vblank_start_count = 0;
          if (vblank_start_count++ < 10) {
            LOG_DEBUG(
                "SNES",
                "VBlank START - v_pos=%d, setting in_vblank_=true at frame %d",
                memory_.v_pos(), frames_);
          }

          in_vblank_ = true;
          in_nmi_ = true;
          if (auto_joy_read_) {
            // TODO: this starts a little after start of vblank
            auto_joy_timer_ = 4224;
            HandleInput();
          }

          if (nmi_enabled_) {
            cpu_.Nmi();
          }
        }
      } break;
    }
  }
  // handle auto_joy_read_-timer
  if (auto_joy_timer_ > 0)
    auto_joy_timer_ -= 2;
}

void Snes::RunCycles(int cycles) {
  if (memory_.h_pos() + cycles >= 536 && memory_.h_pos() < 536) {
    // if we go past 536, add 40 cycles for dram refersh
    cycles += 40;
  }
  for (int i = 0; i < cycles; i += 2) {
    RunCycle();
  }
}

void Snes::SyncCycles(bool start, int sync_cycles) {
  int count = 0;
  if (start) {
    sync_cycle_ = cycles_;
    count = sync_cycles - (cycles_ % sync_cycles);
  } else {
    count = sync_cycles - ((cycles_ - sync_cycle_) % sync_cycles);
  }
  RunCycles(count);
}

uint8_t Snes::ReadBBus(uint8_t adr) {
  if (adr < 0x40) {
    return ppu_.Read(adr, ppu_latch_);
  }
  if (adr < 0x80) {
    CatchUpApu();  // catch up the apu before reading
    uint8_t val = apu_.out_ports_[adr & 0x3];
    // Log port reads when value changes or during critical phase
    static int cpu_port_read_count = 0;
    static uint8_t last_f4 = 0xFF, last_f5 = 0xFF;
    bool value_changed = ((adr & 0x3) == 0 && val != last_f4) ||
                         ((adr & 0x3) == 1 && val != last_f5);
    if (value_changed || cpu_port_read_count++ < 5) {
      LOG_DEBUG("SNES",
                "CPU read APU port $21%02X (F%d) = $%02X at PC=$%02X:%04X "
                "[AFTER CatchUp: APU_cycles=%llu CPU_cycles=%llu]",
                0x40 + (adr & 0x3), (adr & 0x3) + 4, val, cpu_.PB, cpu_.PC,
                apu_.GetCycles(), cycles_);
      if ((adr & 0x3) == 0)
        last_f4 = val;
      if ((adr & 0x3) == 1)
        last_f5 = val;
    }
    return val;
  }
  if (adr == 0x80) {
    uint8_t ret = ram[ram_adr_++];
    ram_adr_ &= 0x1ffff;
    return ret;
  }
  return memory_.open_bus();
}

uint8_t Snes::ReadReg(uint16_t adr) {
  switch (adr) {
    case 0x4210: {
      uint8_t val = 0x2;  // CPU version (4 bit)
      val |= in_nmi_ << 7;
      in_nmi_ = false;
      return val | (memory_.open_bus() & 0x70);
    }
    case 0x4211: {
      uint8_t val = in_irq_ << 7;
      in_irq_ = false;
      cpu_.SetIrq(false);
      return val | (memory_.open_bus() & 0x7f);
    }
    case 0x4212: {
      uint8_t val = (auto_joy_timer_ > 0);
      val |= (memory_.h_pos() < 4 || memory_.h_pos() >= 1096) << 6;
      val |= in_vblank_ << 7;
      return val | (memory_.open_bus() & 0x3e);
    }
    case 0x4213: {
      return ppu_latch_ << 7;  // IO-port
    }
    case 0x4214: {
      return divide_result_ & 0xff;
    }
    case 0x4215: {
      return divide_result_ >> 8;
    }
    case 0x4216: {
      return multiply_result_ & 0xff;
    }
    case 0x4217: {
      return multiply_result_ >> 8;
    }
    case 0x4218:
    case 0x421a:
    case 0x421c:
    case 0x421e: {
      // If transfer is still in progress, data is not yet valid
      if (auto_joy_timer_ > 0) {
        static int zero_return_count = 0;
        if (zero_return_count++ < 50) {
          LOG_WARN("SNES", "Reading $%04X while auto_joy_timer_=%d, returning 0!",
                   adr, auto_joy_timer_);
        }
        return 0;
      }
      uint8_t result = port_auto_read_[(adr - 0x4218) / 2] & 0xff;
      return result;
    }
    case 0x4219:
    case 0x421b:
    case 0x421d:
    case 0x421f: {
      // If transfer is still in progress, data is not yet valid
      if (auto_joy_timer_ > 0)
        return 0;
      uint8_t result = port_auto_read_[(adr - 0x4219) / 2] >> 8;

      return result;
    }
    default: {
      return memory_.open_bus();
    }
  }
}

uint8_t Snes::Rread(uint32_t adr) {
  uint8_t bank = adr >> 16;
  adr &= 0xffff;
  if (bank == 0x7e || bank == 0x7f) {
    return ram[((bank & 1) << 16) | adr];  // ram
  }
  if (bank < 0x40 || (bank >= 0x80 && bank < 0xc0)) {
    if (adr < 0x2000) {
      return ram[adr];  // ram mirror
    }
    if (adr >= 0x2100 && adr < 0x2200) {
      return ReadBBus(adr & 0xff);  // B-bus
    }
    if (adr == 0x4016) {
      uint8_t result = input_read(&input1) | (memory_.open_bus() & 0xfc);

      return result;
    }
    if (adr == 0x4017) {
      return input_read(&input2) | (memory_.open_bus() & 0xe0) | 0x1c;
    }
    if (adr >= 0x4200 && adr < 0x4220) {

      return ReadReg(adr);  // internal registers
    }
    if (adr >= 0x4300 && adr < 0x4380) {
      return ReadDma(&memory_, adr);  // dma registers
    }
  }
  // read from cart
  return memory_.cart_read(bank, adr);
}

uint8_t Snes::Read(uint32_t adr) {
  uint8_t val = Rread(adr);
  memory_.set_open_bus(val);
  return val;
}

void Snes::WriteBBus(uint8_t adr, uint8_t val) {
  if (adr < 0x40) {
    // PPU Register write - catch up rendering first to ensure mid-scanline effects work
    // Only needed if we are in the visible portion of a visible scanline
    // Skip in audio-only mode for performance (no video output needed)
    if (!audio_only_mode_ && !in_vblank_ && memory_.v_pos() > 0 &&
        memory_.h_pos() < 1100) {
      ppu_.CatchUp(memory_.h_pos());
    }
    ppu_.Write(adr, val);
    return;
  }
  if (adr < 0x80) {
    CatchUpApu();  // catch up the apu before writing
    apu_.in_ports_[adr & 0x3] = val;

    // Track CPU port writes for handshake debugging
    uint32_t full_pc = (static_cast<uint32_t>(cpu_.PB) << 16) | cpu_.PC;
    apu_handshake_tracker_.OnCpuPortWrite(adr & 0x3, val, full_pc);

    // NOTE: Auto-reset disabled - relying on complete IPL ROM with counter
    // protocol The IPL ROM will handle multi-upload sequences via its transfer
    // loop

    return;
  }
  switch (adr) {
    case 0x80: {
      ram[ram_adr_++] = val;
      ram_adr_ &= 0x1ffff;
      break;
    }
    case 0x81: {
      ram_adr_ = (ram_adr_ & 0x1ff00) | val;
      break;
    }
    case 0x82: {
      ram_adr_ = (ram_adr_ & 0x100ff) | (val << 8);
      break;
    }
    case 0x83: {
      ram_adr_ = (ram_adr_ & 0x0ffff) | ((val & 1) << 16);
      break;
    }
  }
}

void Snes::WriteReg(uint16_t adr, uint8_t val) {
  switch (adr) {
    case 0x4200: {
      // Log ALL writes to $4200 unconditionally

      auto_joy_read_ = val & 0x1;
      if (!auto_joy_read_)
        auto_joy_timer_ = 0;

      // Debug: Log when auto-joy-read is enabled/disabled

      h_irq_enabled_ = val & 0x10;
      v_irq_enabled_ = val & 0x20;
      if (!h_irq_enabled_ && !v_irq_enabled_) {
        in_irq_ = false;
        cpu_.SetIrq(false);
      }
      // if nmi is enabled while in_nmi_ is still set, immediately generate nmi
      if (!nmi_enabled_ && (val & 0x80) && in_nmi_) {
        cpu_.Nmi();
      }
      bool old_nmi = nmi_enabled_;
      nmi_enabled_ = val & 0x80;

      cpu_.set_int_delay(true);
      break;
    }
    case 0x4201: {
      if (!(val & 0x80) && ppu_latch_) {
        // latch the ppu
        ppu_.LatchHV();
      }
      ppu_latch_ = val & 0x80;
      break;
    }
    case 0x4202: {
      multiply_a_ = val;
      break;
    }
    case 0x4203: {
      multiply_result_ = multiply_a_ * val;
      break;
    }
    case 0x4204: {
      divide_a_ = (divide_a_ & 0xff00) | val;
      break;
    }
    case 0x4205: {
      divide_a_ = (divide_a_ & 0x00ff) | (val << 8);
      break;
    }
    case 0x4206: {
      if (val == 0) {
        divide_result_ = 0xffff;
        multiply_result_ = divide_a_;
      } else {
        divide_result_ = divide_a_ / val;
        multiply_result_ = divide_a_ % val;
      }
      break;
    }
    case 0x4207: {
      h_timer_ = (h_timer_ & 0x100) | val;
      break;
    }
    case 0x4208: {
      h_timer_ = (h_timer_ & 0x0ff) | ((val & 1) << 8);
      break;
    }
    case 0x4209: {
      v_timer_ = (v_timer_ & 0x100) | val;
      break;
    }
    case 0x420a: {
      v_timer_ = (v_timer_ & 0x0ff) | ((val & 1) << 8);
      break;
    }
    case 0x420b: {
      StartDma(&memory_, val, false);
      break;
    }
    case 0x420c: {
      StartDma(&memory_, val, true);
      break;
    }
    case 0x420d: {
      fast_mem_ = val & 0x1;
      break;
    }
    default: {
      break;
    }
  }
}

void Snes::Write(uint32_t adr, uint8_t val) {
  memory_.set_open_bus(val);

  uint8_t bank = adr >> 16;
  adr &= 0xffff;
  if (bank == 0x7e || bank == 0x7f) {

    ram[((bank & 1) << 16) | adr] = val;  // ram
  }
  if (bank < 0x40 || (bank >= 0x80 && bank < 0xc0)) {
    if (adr < 0x2000) {

      ram[adr] = val;  // ram mirror
    }
    if (adr >= 0x2100 && adr < 0x2200) {
      WriteBBus(adr & 0xff, val);  // B-bus
    }
    if (adr == 0x4016) {

      input_latch(&input1, val & 1);  // input latch
      input_latch(&input2, val & 1);
    }
    if (adr >= 0x4200 && adr < 0x4220) {
      WriteReg(adr, val);  // internal registers
    }
    if (adr >= 0x4300 && adr < 0x4380) {
      WriteDma(&memory_, adr, val);  // dma registers
    }
  }

  // write to cart
  memory_.cart_write(bank, adr, val);
}

int Snes::GetAccessTime(uint32_t adr) {
  uint8_t bank = adr >> 16;
  adr &= 0xffff;
  if ((bank < 0x40 || (bank >= 0x80 && bank < 0xc0)) && adr < 0x8000) {
    // 00-3f,80-bf:0-7fff
    if (adr < 0x2000 || adr >= 0x6000)
      return 8;  // 0-1fff, 6000-7fff
    if (adr < 0x4000 || adr >= 0x4200)
      return 6;  // 2000-3fff, 4200-5fff
    return 12;   // 4000-41ff
  }
  // 40-7f,co-ff:0000-ffff, 00-3f,80-bf:8000-ffff
  return (fast_mem_ && bank >= 0x80) ? 6
                                     : 8;  // depends on setting in banks 80+
}

uint8_t Snes::CpuRead(uint32_t adr) {
  cpu_.set_int_delay(false);
  const int cycles = access_time[adr] - 4;
  HandleDma(this, &memory_, cycles);
  RunCycles(cycles);
  uint8_t rv = Read(adr);
  HandleDma(this, &memory_, 4);
  RunCycles(4);
  return rv;
}

void Snes::CpuWrite(uint32_t adr, uint8_t val) {
  cpu_.set_int_delay(false);
  const int cycles = access_time[adr];
  HandleDma(this, &memory_, cycles);
  RunCycles(cycles);
  Write(adr, val);
}

void Snes::CpuIdle(bool waiting) {
  cpu_.set_int_delay(false);
  HandleDma(this, &memory_, 6);
  RunCycles(6);
}

void Snes::SetSamples(int16_t* sample_data, int wanted_samples) {
  apu_.dsp().GetSamples(sample_data, wanted_samples, memory_.pal_timing());
}

void Snes::SetPixels(uint8_t* pixel_data) {
  ppu_.PutPixels(pixel_data);
}

void Snes::SetButtonState(int player, int button, bool pressed) {
  // Select the appropriate input based on player number
  // Select the appropriate input based on player number
  // Player 0 = Controller 1, Player 1 = Controller 2
  Input* input = (player == 0) ? &input1 : &input2;

  // SNES controller button mapping (standard layout)
  // Bit 0: B, Bit 1: Y, Bit 2: Select, Bit 3: Start
  // Bit 4: Up, Bit 5: Down, Bit 6: Left, Bit 7: Right
  // Bit 8: A, Bit 9: X, Bit 10: L, Bit 11: R

  if (button < 0 || button > 11)
    return;  // Validate button range

  uint16_t old_state = input->current_state_;

  if (pressed) {
    // Set the button bit
    input->current_state_ |= (1 << button);
  } else {
    // Clear the button bit
    input->current_state_ &= ~(1 << button);
  }
}

absl::Status Snes::LoadLegacyState(std::istream& file) {
  uint32_t version = 0;
  RETURN_IF_ERROR(ReadUint32LE(file, &version));
  if (version != 1) {
    return absl::FailedPreconditionError("Unsupported legacy state version");
  }

  RETURN_IF_ERROR(ReadBytes(file, ram, sizeof(ram)));
  RETURN_IF_ERROR(ReadScalar(file, &ram_adr_));
  RETURN_IF_ERROR(ReadScalar(file, &cycles_));
  RETURN_IF_ERROR(ReadScalar(file, &sync_cycle_));
  RETURN_IF_ERROR(ReadScalar(file, &apu_catchup_cycles_));
  RETURN_IF_ERROR(ReadScalar(file, &h_irq_enabled_));
  RETURN_IF_ERROR(ReadScalar(file, &v_irq_enabled_));
  RETURN_IF_ERROR(ReadScalar(file, &nmi_enabled_));
  RETURN_IF_ERROR(ReadScalar(file, &h_timer_));
  RETURN_IF_ERROR(ReadScalar(file, &v_timer_));
  RETURN_IF_ERROR(ReadScalar(file, &in_nmi_));
  RETURN_IF_ERROR(ReadScalar(file, &irq_condition_));
  RETURN_IF_ERROR(ReadScalar(file, &in_irq_));
  RETURN_IF_ERROR(ReadScalar(file, &in_vblank_));
  RETURN_IF_ERROR(ReadBytes(file, port_auto_read_, sizeof(port_auto_read_)));
  RETURN_IF_ERROR(ReadScalar(file, &auto_joy_read_));
  RETURN_IF_ERROR(ReadScalar(file, &auto_joy_timer_));
  RETURN_IF_ERROR(ReadScalar(file, &ppu_latch_));
  RETURN_IF_ERROR(ReadScalar(file, &multiply_a_));
  RETURN_IF_ERROR(ReadScalar(file, &multiply_result_));
  RETURN_IF_ERROR(ReadScalar(file, &divide_a_));
  RETURN_IF_ERROR(ReadScalar(file, &divide_result_));
  RETURN_IF_ERROR(ReadScalar(file, &fast_mem_));
  RETURN_IF_ERROR(ReadScalar(file, &next_horiz_event));

  cpu_.LoadState(file);
  ppu_.LoadState(file);
  apu_.LoadState(file);

  if (!file) {
    return absl::InternalError("Failed while reading legacy state");
  }
  return absl::OkStatus();
}

absl::Status Snes::saveState(const std::string& path) {
  std::ofstream file(path, std::ios::binary);
  if (!file) {
    return absl::InternalError("Failed to open state file for writing");
  }
  if (!IsLittleEndianHost()) {
    return absl::FailedPreconditionError(
        "State serialization requires a little-endian host");
  }

  RETURN_IF_ERROR(WriteUint32LE(file, kStateMagic));
  RETURN_IF_ERROR(WriteUint32LE(file, kStateFormatVersion));

  auto write_core_chunk = [&]() -> absl::Status {
    std::ostringstream chunk(std::ios::binary);
    RETURN_IF_ERROR(WriteBytes(chunk, ram, sizeof(ram)));
    RETURN_IF_ERROR(WriteScalar(chunk, ram_adr_));
    RETURN_IF_ERROR(WriteScalar(chunk, cycles_));
    RETURN_IF_ERROR(WriteScalar(chunk, sync_cycle_));
    RETURN_IF_ERROR(WriteScalar(chunk, apu_catchup_cycles_));
    RETURN_IF_ERROR(WriteScalar(chunk, h_irq_enabled_));
    RETURN_IF_ERROR(WriteScalar(chunk, v_irq_enabled_));
    RETURN_IF_ERROR(WriteScalar(chunk, nmi_enabled_));
    RETURN_IF_ERROR(WriteScalar(chunk, h_timer_));
    RETURN_IF_ERROR(WriteScalar(chunk, v_timer_));
    RETURN_IF_ERROR(WriteScalar(chunk, in_nmi_));
    RETURN_IF_ERROR(WriteScalar(chunk, irq_condition_));
    RETURN_IF_ERROR(WriteScalar(chunk, in_irq_));
    RETURN_IF_ERROR(WriteScalar(chunk, in_vblank_));
    for (const auto val : port_auto_read_) {
      RETURN_IF_ERROR(WriteScalar(chunk, val));
    }
    RETURN_IF_ERROR(WriteScalar(chunk, auto_joy_read_));
    RETURN_IF_ERROR(WriteScalar(chunk, auto_joy_timer_));
    RETURN_IF_ERROR(WriteScalar(chunk, ppu_latch_));
    RETURN_IF_ERROR(WriteScalar(chunk, multiply_a_));
    RETURN_IF_ERROR(WriteScalar(chunk, multiply_result_));
    RETURN_IF_ERROR(WriteScalar(chunk, divide_a_));
    RETURN_IF_ERROR(WriteScalar(chunk, divide_result_));
    RETURN_IF_ERROR(WriteScalar(chunk, fast_mem_));
    RETURN_IF_ERROR(WriteScalar(chunk, next_horiz_event));

    if (!chunk) {
      return absl::InternalError("Failed to buffer core state");
    }
    return WriteChunk(file, MakeTag('S', 'N', 'E', 'S'), 1, chunk.str());
  };

  RETURN_IF_ERROR(write_core_chunk());

  auto write_component = [&](uint32_t tag, uint32_t version,
                             auto&& writer) -> absl::Status {
    std::ostringstream chunk(std::ios::binary);
    writer(chunk);
    if (!chunk) {
      return absl::InternalError(
          absl::StrFormat("Failed to serialize chunk %08x", tag));
    }
    auto payload = chunk.str();
    if (payload.size() > kMaxChunkSize) {
      return absl::FailedPreconditionError(
          "Serialized chunk exceeded maximum allowed size");
    }
    return WriteChunk(file, tag, version, payload);
  };

  RETURN_IF_ERROR(
      write_component(MakeTag('C', 'P', 'U', ' '), 1,
                      [&](std::ostream& out) { cpu_.SaveState(out); }));
  RETURN_IF_ERROR(
      write_component(MakeTag('P', 'P', 'U', ' '), 1,
                      [&](std::ostream& out) { ppu_.SaveState(out); }));
  RETURN_IF_ERROR(
      write_component(MakeTag('A', 'P', 'U', ' '), 1,
                      [&](std::ostream& out) { apu_.SaveState(out); }));

  return absl::OkStatus();
}

absl::Status Snes::loadState(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return absl::InternalError("Failed to open state file for reading");
  }
  if (!IsLittleEndianHost()) {
    return absl::FailedPreconditionError(
        "State serialization requires a little-endian host");
  }

  // Peek to determine format
  uint32_t magic = 0;
  auto magic_status = ReadUint32LE(file, &magic);
  if (!magic_status.ok() || magic != kStateMagic) {
    file.clear();
    file.seekg(0);
    return LoadLegacyState(file);
  }

  uint32_t format_version = 0;
  RETURN_IF_ERROR(ReadUint32LE(file, &format_version));
  if (format_version != kStateFormatVersion) {
    return absl::FailedPreconditionError("Unsupported state file format");
  }

  auto load_core_chunk = [&](std::istream& stream) -> absl::Status {
    RETURN_IF_ERROR(ReadBytes(stream, ram, sizeof(ram)));
    RETURN_IF_ERROR(ReadScalar(stream, &ram_adr_));
    RETURN_IF_ERROR(ReadScalar(stream, &cycles_));
    RETURN_IF_ERROR(ReadScalar(stream, &sync_cycle_));
    RETURN_IF_ERROR(ReadScalar(stream, &apu_catchup_cycles_));
    RETURN_IF_ERROR(ReadScalar(stream, &h_irq_enabled_));
    RETURN_IF_ERROR(ReadScalar(stream, &v_irq_enabled_));
    RETURN_IF_ERROR(ReadScalar(stream, &nmi_enabled_));
    RETURN_IF_ERROR(ReadScalar(stream, &h_timer_));
    RETURN_IF_ERROR(ReadScalar(stream, &v_timer_));
    RETURN_IF_ERROR(ReadScalar(stream, &in_nmi_));
    RETURN_IF_ERROR(ReadScalar(stream, &irq_condition_));
    RETURN_IF_ERROR(ReadScalar(stream, &in_irq_));
    RETURN_IF_ERROR(ReadScalar(stream, &in_vblank_));
    for (auto& val : port_auto_read_) {
      RETURN_IF_ERROR(ReadScalar(stream, &val));
    }
    RETURN_IF_ERROR(ReadScalar(stream, &auto_joy_read_));
    RETURN_IF_ERROR(ReadScalar(stream, &auto_joy_timer_));
    RETURN_IF_ERROR(ReadScalar(stream, &ppu_latch_));
    RETURN_IF_ERROR(ReadScalar(stream, &multiply_a_));
    RETURN_IF_ERROR(ReadScalar(stream, &multiply_result_));
    RETURN_IF_ERROR(ReadScalar(stream, &divide_a_));
    RETURN_IF_ERROR(ReadScalar(stream, &divide_result_));
    RETURN_IF_ERROR(ReadScalar(stream, &fast_mem_));
    RETURN_IF_ERROR(ReadScalar(stream, &next_horiz_event));
    return absl::OkStatus();
  };

  bool core_loaded = false;
  bool cpu_loaded = false;
  bool ppu_loaded = false;
  bool apu_loaded = false;

  while (file && file.peek() != EOF) {
    ChunkHeader header{};
    auto header_status = ReadChunkHeader(file, &header);
    if (!header_status.ok()) {
      return header_status;
    }

    if (header.size > kMaxChunkSize) {
      return absl::FailedPreconditionError("State chunk too large");
    }

    std::string payload(header.size, '\0');
    auto read_status = ReadBytes(file, payload.data(), header.size);
    if (!read_status.ok()) {
      return read_status;
    }

    uint32_t crc = render::CalculateCRC32(
        reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
    if (crc != header.crc32) {
      return absl::FailedPreconditionError("State chunk CRC mismatch");
    }

    std::istringstream chunk_stream(payload, std::ios::binary);
    switch (header.tag) {
      case MakeTag('S', 'N', 'E', 'S'): {
        if (header.version != 1) {
          return absl::FailedPreconditionError(
              "Unsupported SNES chunk version");
        }
        auto status = load_core_chunk(chunk_stream);
        if (!status.ok())
          return status;
        core_loaded = true;
        break;
      }
      case MakeTag('C', 'P', 'U', ' '): {
        if (header.version != 1) {
          return absl::FailedPreconditionError("Unsupported CPU chunk version");
        }
        cpu_.LoadState(chunk_stream);
        if (!chunk_stream) {
          return absl::InternalError("Failed to load CPU chunk");
        }
        cpu_loaded = true;
        break;
      }
      case MakeTag('P', 'P', 'U', ' '): {
        if (header.version != 1) {
          return absl::FailedPreconditionError("Unsupported PPU chunk version");
        }
        ppu_.LoadState(chunk_stream);
        if (!chunk_stream) {
          return absl::InternalError("Failed to load PPU chunk");
        }
        ppu_loaded = true;
        break;
      }
      case MakeTag('A', 'P', 'U', ' '): {
        if (header.version != 1) {
          return absl::FailedPreconditionError("Unsupported APU chunk version");
        }
        apu_.LoadState(chunk_stream);
        if (!chunk_stream) {
          return absl::InternalError("Failed to load APU chunk");
        }
        apu_loaded = true;
        break;
      }
      default:
        // Skip unknown chunk types
        break;
    }
  }

  if (!core_loaded || !cpu_loaded || !ppu_loaded || !apu_loaded) {
    return absl::FailedPreconditionError("Missing required chunks in state");
  }
  return absl::OkStatus();
}

void Snes::InitAccessTime(bool recalc) {
  int start = (recalc) ? 0x800000 : 0;  // recalc only updates fast rom
  access_time.resize(0x1000000);
  for (int i = start; i < 0x1000000; i++) {
    access_time[i] = GetAccessTime(i);
  }
}

}  // namespace emu
}  // namespace yaze
