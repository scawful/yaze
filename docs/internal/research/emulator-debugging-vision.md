# YAZE Emulator Enhancement Roadmap

**Version**: 1.0  
**Date**: October 8, 2025  
**Status**: Planning Phase  
**Target**: Mesen2-Level Debugging + AI Integration

---

## Executive Summary

This document outlines the roadmap for evolving YAZE's SNES emulator from a basic runtime into a **world-class debugging platform** with AI agent integration. The goal is to achieve feature parity with Mesen2's advanced debugging capabilities while adding unique AI-powered features through the z3ed CLI system.

### Core Objectives
1. **Advanced Debugger** - Breakpoints, watchpoints, memory inspection, trace logging
2. **Performance Optimization** - Cycle-accurate timing, dynarec, frame pacing
3. **Audio System Fix** - SDL2 audio output currently broken, needs investigation
4. **AI Integration** - z3ed agent can read/write emulator state, automate testing
5. **SPC700 Debugger** - Full audio CPU debugging with APU state inspection

---

##  Current State Analysis

### What Works 
- **CPU Emulation**: 65816 core functional, runs games
- **PPU Rendering**: Display works, texture updates to SDL2
- **ROM Loading**: Can load and execute SNES ROMs
- **Basic Controls**: Start/stop/pause/reset functionality
- **Memory Access**: Read/write to CPU memory space
- **Renderer Integration**: Now using `IRenderer` interface (SDL3-ready!)
- **Stability**: Emulator pauses during window resize (macOS protection)

### What's Broken ❌
- **Audio Output**: SDL2 audio device initialized but no sound plays
- **SPC700 Debugging**: No inspection tools for audio CPU
- **Performance**: Not cycle-accurate, timing issues
- **Debugging Tools**: Minimal breakpoint support, no watchpoints
- **Memory Viewer**: Basic hex view, no structured inspection
- **Trace Logging**: No execution tracing capability

### What's Missing Pending:
- **Advanced Breakpoints**: Conditional, access-based, CPU/SPC700
- **Memory Watchpoints**: Track reads/writes to specific addresses
- **Disassembly View**: Real-time code annotation
- **Performance Profiling**: Hotspot analysis, cycle counting
- **Event Viewer**: Track NMI, IRQ, DMA events
- **PPU Inspector**: VRAM, OAM, palette debugging
- **APU Inspector**: DSP state, sample buffer, channel visualization
- **AI Integration**: z3ed agent can't access emulator yet

---

## Tool Phase 1: Audio System Fix (Priority: CRITICAL)

### Problem Analysis
**Current State**:
```cpp
// controller.cc:31-33
editor_manager_.emulator().set_audio_buffer(window_.audio_buffer_.get());
editor_manager_.emulator().set_audio_device_id(window_.audio_device_);

// window.cc:114-130
window.audio_device_ = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
SDL_PauseAudioDevice(window.audio_device_, 0);  // Unpause
```

**The Issue**: Audio device is initialized and unpaused, but `SDL_QueueAudio()` in emulator isn't producing sound.

### Investigation Steps

1. **Verify Audio Device State**
```cpp
// Add to Emulator::Run()
if (frame_count_ % 60 == 0) {  // Every second
  uint32_t queued = SDL_GetQueuedAudioSize(audio_device_);
  SDL_AudioStatus status = SDL_GetAudioDeviceStatus(audio_device_);
  printf("[AUDIO] Queued: %u bytes, Status: %d (1=playing, 2=paused)\n", 
         queued, status);
}
```

2. **Check SPC700 Sample Generation**
```cpp
// Verify snes_.SetSamples() is producing valid data
snes_.SetSamples(audio_buffer_, wanted_samples_);

// Debug output
int16_t* samples = audio_buffer_;
bool has_audio = false;
for (int i = 0; i < wanted_samples_ * 2; i++) {
  if (samples[i] != 0) {
    has_audio = true;
    break;
  }
}
if (!has_audio && frame_count_ % 60 == 0) {
  printf("[AUDIO] Warning: All samples are zero!\n");
}
```

3. **Validate Audio Format Compatibility**
```cpp
// window.cc - Check if requested format matches obtained format
SDL_AudioSpec want, have;
// ... (existing code)
window.audio_device_ = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);

printf("[AUDIO] Requested: %dHz, %d channels, format=%d\n", 
       want.freq, want.channels, want.format);
printf("[AUDIO] Obtained:  %dHz, %d channels, format=%d\n", 
       have.freq, have.channels, have.format);

if (have.freq != want.freq || have.channels != want.channels) {
  LOG_ERROR("Audio", "Audio spec mismatch - may need resampling");
}
```

### Likely Fixes

**Fix 1: Audio Device Paused State**
```cpp
// The audio device might be re-pausing itself
// Try forcing unpause in the emulator loop
if (frame_count_ % 60 == 0) {
  SDL_PauseAudioDevice(audio_device_, 0);  // Ensure unpaused
}
```

**Fix 2: Sample Format Conversion**
```cpp
// SPC700 might be outputting wrong format
// Ensure AUDIO_S16 (signed 16-bit) matches emulator output
void Snes::SetSamples(int16_t* buffer, int count) {
  // Verify apu_.GetSamples() returns int16_t, not float or uint16_t
  apu_.GetSamples(buffer, count);
  
  // Debug: Check for clipping or DC offset
  for (int i = 0; i < count * 2; i++) {
    if (buffer[i] < -32768 || buffer[i] > 32767) {
      printf("[AUDIO] Sample %d out of range: %d\n", i, buffer[i]);
    }
  }
}
```

**Fix 3: Buffer Size Mismatch**
```cpp
// Ensure buffer allocation matches usage
// window.cc:128
window.audio_buffer_ = std::make_shared<int16_t>(audio_frequency / 50 * 4);
// This allocates: 48000 / 50 * 4 = 3840 int16_t samples
// Emulator uses: wanted_samples_ * 4 bytes = (48000/60) * 4 = 3200 bytes
// = 1600 int16_t samples (800 per channel)
// MISMATCH! Should be:
window.audio_buffer_ = std::make_shared<int16_t>(audio_frequency / 50 * 2);  // Stereo
```

### Quick Win Actions
**File**: `window.cc`
**Line**: 128
**Change**: 
```cpp
// Before:
window.audio_buffer_ = std::make_shared<int16_t>(audio_frequency / 50 * 4);

// After:
// Allocate for stereo at 60Hz (worst case)
// 48000Hz / 60 FPS = 800 samples/frame * 2 channels = 1600 int16_t
window.audio_buffer_ = std::make_shared<int16_t>((audio_frequency / 50) * 2);
```

**File**: `emulator.cc`  
**After Line**: 216
**Add**:
```cpp
// Debug audio output
if (frame_count_ % 300 == 0) {  // Every 5 seconds
  uint32_t queued = SDL_GetQueuedAudioSize(audio_device_);
  SDL_AudioStatus status = SDL_GetAudioDeviceStatus(audio_device_);
  printf("[AUDIO] Status=%d, Queued=%u, WantedSamples=%d\n", 
         status, queued, wanted_samples_);
}
```

**Estimated Fix Time**: 2-4 hours

---

## 🐛 Phase 2: Advanced Debugger (Mesen2 Feature Parity)

### Feature Comparison: YAZE vs Mesen2

| Feature | Mesen2 | YAZE Current | YAZE Target |
|---------|--------|--------------|-------------|
| CPU Breakpoints |  Execute/Read/Write | Warning: Basic Execute |  Full Support |
| Memory Watchpoints |  Conditional | ❌ None |  Conditional |
| Disassembly View |  Live Annotated | ❌ Static |  Live + Labels |
| Memory Viewer |  Multi-region | Warning: Basic Hex |  Structured |
| Trace Logger |  CPU/SPC/DMA | ❌ None |  All Channels |
| Event Viewer |  IRQ/NMI/DMA | ❌ None |  Full Timeline |
| Performance |  Cycle Accurate | ❌ Approximate |  Cycle Accurate |
| Save States | Warning: Limited | Warning: Basic |  Full State |
| PPU Debugger |  Layer Viewer | ❌ None |  VRAM Inspector |
| APU Debugger |  DSP Viewer | ❌ None |  Channel Mixer |
| Scripting |  Lua | ❌ None |  z3ed + AI! |

### 2.1 Breakpoint System

**Architecture**:
```cpp
// src/app/emu/debug/breakpoint_manager.h
class BreakpointManager {
public:
  enum class Type {
    EXECUTE,     // Break when PC reaches address
    READ,        // Break when address is read
    WRITE,       // Break when address is written
    ACCESS,      // Break on read OR write
    CONDITIONAL  // Break when condition is true
  };
  
  struct Breakpoint {
    uint32_t address;
    Type type;
    bool enabled;
    std::string condition;  // Lua expression or simple comparison
    uint32_t hit_count;
    std::function<bool()> callback;  // Optional custom logic
  };
  
  uint32_t AddBreakpoint(uint32_t address, Type type, 
                         const std::string& condition = "");
  void RemoveBreakpoint(uint32_t id);
  bool ShouldBreak(uint32_t address, Type access_type);
  std::vector<Breakpoint> ListBreakpoints();
  
private:
  std::unordered_map<uint32_t, Breakpoint> breakpoints_;
  uint32_t next_id_ = 1;
};
```

**CPU Integration**:
```cpp
// src/app/emu/cpu/cpu.cc
void CPU::RunOpcode() {
  // Check execute breakpoint BEFORE running
  if (breakpoint_mgr_->ShouldBreak(PC, BreakpointType::EXECUTE)) {
    emulator_->OnBreakpointHit(PC, BreakpointType::EXECUTE);
    return;  // Pause execution
  }
  
  // Run instruction...
  ExecuteInstruction();
  
  // Memory access breakpoints handled in Read()/Write()
}

uint8_t CPU::Read(uint32_t address) {
  if (breakpoint_mgr_->ShouldBreak(address, BreakpointType::READ)) {
    emulator_->OnBreakpointHit(address, BreakpointType::READ);
  }
  return memory_->Read(address);
}
```

**z3ed Integration**:
```bash
# CLI commands
z3ed emu breakpoint add --address 0x00FFD9 --type execute
z3ed emu breakpoint add --address 0x7E0010 --type write --condition "value > 100"
z3ed emu breakpoint list
z3ed emu breakpoint remove --id 1

# AI agent can use these
"Set a breakpoint at the Link damage handler"
→ Agent finds damage code address → z3ed emu breakpoint add
```

**Estimated Effort**: 8-12 hours

---

### 2.2 Memory Watchpoints

**Features**:
- Track specific memory regions
- Log all accesses with stack traces
- Detect buffer overflows
- Find data corruption sources

**Implementation**:
```cpp
// src/app/emu/debug/watchpoint_manager.h
class WatchpointManager {
public:
  struct Watchpoint {
    uint32_t start_address;
    uint32_t end_address;
    bool track_reads;
    bool track_writes;
    std::vector<AccessLog> history;
  };
  
  struct AccessLog {
    uint32_t pc;           // Where the access happened
    uint32_t address;      // What address was accessed
    uint8_t old_value;     // Value before write
    uint8_t new_value;     // Value after write
    bool is_write;
    uint64_t cycle_count;
  };
  
  void AddWatchpoint(uint32_t start, uint32_t end, bool reads, bool writes);
  void OnMemoryAccess(uint32_t pc, uint32_t address, bool is_write, 
                      uint8_t old_val, uint8_t new_val);
  std::vector<AccessLog> GetHistory(uint32_t address, int max_entries = 100);
};
```

**Use Cases**:
- Find where Link's HP is being modified
- Track item collection bugs
- Debug event flag corruption
- Detect unintended memory writes

**z3ed Commands**:
```bash
z3ed emu watch add --start 0x7E0000 --end 0x7E1FFF --reads --writes
z3ed emu watch history --address 0x7E0010
z3ed emu watch export --format csv
```

**Estimated Effort**: 6-8 hours

---

### 2.3 Live Disassembly Viewer

**Mesen2 Inspiration**:
- Scrollable code view with current PC highlighted
- Labels from ROM labels file
- Inline comments
- Jump target visualization
- Hot code highlighting (most-executed instructions)

**Architecture**:
```cpp
// src/app/emu/debug/disassembly_viewer.h (already exists!)
// Enhance existing viewer

class DisassemblyViewer {
public:
  struct DisassembledInstruction {
    uint32_t address;
    std::string mnemonic;
    std::string operands;
    std::string comment;
    uint32_t execution_count;  // NEW: Hotspot tracking
    bool is_breakpoint;
    bool is_current_pc;
  };
  
  void Update(CPU& cpu);
  void RenderWindow();
  void JumpToAddress(uint32_t address);
  void ToggleBreakpoint(uint32_t address);
  
  // NEW: Hotspot profiling
  void EnableProfiling(bool enable);
  std::vector<uint32_t> GetHotspots(int top_n = 10);
  
private:
  std::unordered_map<uint32_t, uint32_t> execution_counts_;
  std::shared_ptr<ResourceLabel> labels_;  // From ROM
};
```

**ImGui Integration**:
```cpp
void Emulator::RenderDisassemblyWindow() {
  if (ImGui::Begin("Disassembly", &show_disassembly_)) {
    // Scrollable list
    ImGui::BeginChild("DisasmScroll");
    
    // Show ±50 instructions around PC
    uint32_t pc = snes_.cpu().PC;
    for (int offset = -50; offset <= 50; offset++) {
      auto instr = disassembly_viewer_.GetInstruction(pc + offset);
      
      // Highlight current PC
      if (offset == 0) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1,1,0,1));
      }
      
      // Show execution count for hotspots
      if (instr.execution_count > 1000) {
        ImGui::TextColored(ImVec4(1,0.5,0,1), "");  // Hot code
      }
      
      ImGui::Text("%06X  %s  %s  ; %s", 
                  instr.address, instr.mnemonic.c_str(), 
                  instr.operands.c_str(), instr.comment.c_str());
      
      if (offset == 0) ImGui::PopStyleColor();
      
      // Click to toggle breakpoint
      if (ImGui::IsItemClicked()) {
        disassembly_viewer_.ToggleBreakpoint(instr.address);
      }
    }
    
    ImGui::EndChild();
  }
  ImGui::End();
}
```

**Estimated Effort**: 10-12 hours

---

### 2.4 Enhanced Memory Viewer

**Multi-Region Support**:
```cpp
enum class MemoryRegion {
  WRAM,        // 0x7E0000-0x7FFFFF (128KB)
  SRAM,        // Cartridge RAM
  VRAM,        // PPU video RAM
  CGRAM,       // PPU palette RAM
  OAM,         // PPU sprite RAM
  ARAM,        // SPC700 audio RAM
  ROM,         // Cartridge ROM
  REGISTERS    // Hardware registers
};
```

**Structured Views**:
```cpp
class MemoryViewer {
public:
  void RenderHexView(MemoryRegion region);
  void RenderStructView(uint32_t address, const std::string& struct_name);
  void RenderDiffView(uint32_t address, const uint8_t* reference);
  
  // NEW: ROM label integration
  void SetLabels(std::shared_ptr<ResourceLabel> labels);
  std::string GetLabelForAddress(uint32_t address);
  
  // NEW: Goto functionality
  void GotoAddress(uint32_t address);
  void GotoLabel(const std::string& label);
};
```

**ImGui Layout**:
```
┌────────────────────────────────────────────────┐
│ Memory Viewer                                  │
├────────────────────────────────────────────────┤
│ Region: [WRAM ▼] | Goto: [0x7E0010] [Find]   │
├────────────────────────────────────────────────┤
│ Addr    +0 +1 +2 +3 +4 +5 +6 +7  ASCII        │
│ 7E0000  00 05 3C 00 00 00 00 00  ..<.....      │ ← Link's HP
│ 7E0008  1F 00 00 00 00 00 00 00  ........      │
│ ...                                            │
├────────────────────────────────────────────────┤
│ Watchpoints: [Add] [Clear All]                │
│ • 0x7E0000-0x7E0010 (R/W) - Link stats        │
└────────────────────────────────────────────────┘
```

**Estimated Effort**: 6-8 hours

---

##  Phase 3: Performance Optimizations

### 3.1 Cycle-Accurate Timing

**Current Issue**: Emulator runs on frame timing, not cycle timing

**Mesen2 Approach**:
```cpp
class CPU {
  void RunCycle() {
    if (cycles_until_next_instruction_ == 0) {
      ExecuteInstruction();  // Sets cycles_until_next_instruction_
    } else {
      cycles_until_next_instruction_--;
    }
    
    // Other components run per-cycle
    ppu_.RunCycle();
    apu_.RunCycle();
    dma_.RunCycle();
  }
};

// Emulator loop
void Emulator::RunFrame() {
  const int cycles_per_frame = snes_.memory().pal_timing() ? 1538400 : 1789773;
  for (int i = 0; i < cycles_per_frame; i++) {
    snes_.RunCycle();  // ONE cycle at a time
  }
}
```

**Benefits**:
- Accurate mid-scanline effects
- Proper DMA timing
- Correct PPU rendering edge cases
- Deterministic emulation

**Estimated Effort**: 20-30 hours (major refactor)

---

### 3.2 Dynamic Recompilation (Dynarec)

**Why**: Cycle-accurate interpretation is slow (~30 FPS). Dynarec can hit 60 FPS.

**Strategy**:
```cpp
class Dynarec {
  // Compile frequently-executed code blocks to native ARM/x64
  void* CompileBlock(uint32_t start_pc);
  void InvalidateBlock(uint32_t address);  // When code changes
  
  // Cache compiled blocks
  std::unordered_map<uint32_t, void*> code_cache_;
};

void CPU::RunOpcode() {
  if (dynarec_enabled_) {
    // Check if block is compiled
    if (auto* block = dynarec_.GetBlock(PC)) {
      return ((BlockFunc)block)();  // Execute native code
    }
  }
  
  // Fallback to interpreter
  ExecuteInstruction();
}
```

**Complexity**: Very high - requires assembly code generation

**Alternative**: Use existing dynarec library like `bsnes-jit`

**Estimated Effort**: 40-60 hours (or use library: 10 hours)

---

### 3.3 Frame Pacing Improvements

**Current Issue**: SDL_Delay(1) is too coarse

**Better Approach**:
```cpp
void Emulator::RunFrame() {
  auto frame_start = std::chrono::high_resolution_clock::now();
  
  // Run SNES frame
  snes_.RunFrame();
  
  // Calculate how long to wait
  auto frame_end = std::chrono::high_resolution_clock::now();
  auto frame_duration = std::chrono::duration_cast<std::chrono::microseconds>(
      frame_end - frame_start);
  
  auto target_duration = std::chrono::microseconds(
      static_cast<int64_t>(wanted_frames_ * 1'000'000));
  
  if (frame_duration < target_duration) {
    auto sleep_time = target_duration - frame_duration;
    std::this_thread::sleep_for(sleep_time);  // Precise sleep
  }
}
```

**Estimated Effort**: 2-3 hours

---

## Game Phase 4: SPC700 Audio CPU Debugger

### 4.1 APU Inspector Window

**Layout**:
```
┌─────────────────────────────────────────────────────────┐
│ APU Debugger                                            │
├─────────────────────────────────────────────────────────┤
│ SPC700 CPU State:                                       │
│   PC: 0x1234  A: 0x00  X: 0x05  Y: 0xFF  PSW: 0x02     │
│   SP: 0xEF    Cycles: 12,345,678                        │
├─────────────────────────────────────────────────────────┤
│ DSP Registers:  [Channel 0▼]                           │
│   VOL_L: 127    VOL_R: 127    PITCH: 2048              │
│   ADSR: 0xBE7F  GAIN: 0x7F    ENVX: 45  OUTX: 78       │
│                                                         │
│ Waveform: [████▓▓▓▓░░░░▒▒▒▒████▓▓▓▓░░░░] (live)      │
├─────────────────────────────────────────────────────────┤
│ Audio RAM (64KB):                                       │
│ 0000  BRR BRR BRR BRR ...  (sample data)               │
│ ...                                                     │
├─────────────────────────────────────────────────────────┤
│ Channel Mixer:                                          │
│ 0: ████████████░░░░ (75%)  1: ░░░░░░░░░░░░░░ (0%)     │
│ 2: ██████░░░░░░░░░░ (45%)  3: ░░░░░░░░░░░░░░ (0%)     │
│ ...                                                     │
└─────────────────────────────────────────────────────────┘
```

**Implementation**:
```cpp
// src/app/emu/debug/apu_inspector.h
class ApuInspector {
public:
  void RenderWindow(Snes& snes);
  void RenderChannelMixer();
  void RenderWaveform(int channel);
  void RenderDspRegisters();
  void RenderAudioRam();
  
  // Sample buffer visualization
  void UpdateWaveformData(const int16_t* samples, int count);
  
private:
  std::array<std::vector<float>, 8> channel_waveforms_;
  int selected_channel_ = 0;
};
```

**z3ed Commands**:
```bash
z3ed emu apu status
z3ed emu apu channel --id 0
z3ed emu apu dump-ram --output audio_ram.bin
z3ed emu apu export-samples --channel 0 --format wav
```

**Estimated Effort**: 12-15 hours

---

### 4.2 Audio Sample Export

**Feature**: Export audio samples to WAV for analysis

```cpp
class AudioExporter {
public:
  void StartRecording(int channel = -1);  // -1 = all channels
  void StopRecording();
  void ExportToWav(const std::string& filename);
  
private:
  std::vector<int16_t> recorded_samples_;
  bool recording_ = false;
};
```

**Use Cases**:
- Debug why sound effects aren't playing
- Export game music for analysis
- Compare with real SNES hardware recordings

**Estimated Effort**: 4-6 hours

---

## AI Phase 5: z3ed AI Agent Integration

### 5.1 Emulator State Access

**Add to ConversationalAgentService**:
```cpp
// src/cli/service/agent/conversational_agent_service.h
class ConversationalAgentService {
public:
  // NEW: Emulator access
  void SetEmulator(emu::Emulator* emulator);
  
  // Tool: Get emulator state
  std::string HandleEmulatorState();
  std::string HandleCpuState();
  std::string HandleMemoryRead(uint32_t address, int count);
  std::string HandleMemoryWrite(uint32_t address, const std::vector<uint8_t>& data);
  
private:
  emu::Emulator* emulator_ = nullptr;
};
```

**z3ed Tool Schema**:
```json
{
  "name": "emulator-read-memory",
  "description": "Read emulator memory at a specific address",
  "parameters": {
    "address": {"type": "integer", "description": "Memory address (e.g., 0x7E0010)"},
    "count": {"type": "integer", "description": "Number of bytes to read"},
    "region": {"type": "string", "enum": ["wram", "sram", "rom", "aram"]}
  }
}
```

**Example AI Queries**:
```
User: "What is Link's current HP?"
Agent: [calls emulator-read-memory address=0x7E0000 count=1]
       → Response: "Link has 6 hearts (0x60 = 96 health points)"

User: "Set Link to full health"
Agent: [calls emulator-write-memory address=0x7E0000 data=[0xA0]]
       → Response: "Link's HP set to 160 (full health)"

User: "Where is the game stuck?"
Agent: [calls emulator-cpu-state]
       → Response: "PC=$00:8234 - Infinite loop in NMI handler"
```

---

### 5.2 Automated Test Generation

**Use Case**: AI generates emulator tests from natural language

**Example Flow**:
```bash
z3ed agent test-scenario --prompt "Test that Link takes damage from enemies"

# AI generates:
{
  "steps": [
    {"action": "load-state", "file": "link_at_full_hp.sfc"},
    {"action": "run-frames", "count": 60},
    {"action": "assert-memory", "address": "0x7E0000", "value": "0xA0"},
    {"action": "move-link", "direction": "right", "frames": 30},
    {"action": "assert-memory-decreased", "address": "0x7E0000"},
    {"action": "screenshot", "name": "link_damaged.png"}
  ]
}
```

**Implementation**:
```cpp
// src/cli/commands/agent/test_scenario_runner.h
class TestScenarioRunner {
public:
  struct TestStep {
    std::string action;
    absl::flat_hash_map<std::string, std::string> params;
  };
  
  absl::Status RunScenario(const std::vector<TestStep>& steps);
  
private:
  void ExecuteLoadState(const std::string& file);
  void ExecuteRunFrames(int count);
  void ExecuteAssertMemory(uint32_t address, uint8_t expected);
  void ExecuteMoveLink(const std::string& direction, int frames);
  void ExecuteScreenshot(const std::string& filename);
};
```

**Estimated Effort**: 8-10 hours

---

### 5.3 Memory Map Learning

**Feature**: AI learns ROM's memory layout from debugging sessions

**Architecture**:
```cpp
// Extends existing learn command
z3ed agent learn --memory-map "0x7E0000" --label "link_hp" --type "uint8"
z3ed agent learn --memory-map "0x7E0010" --label "link_x_pos" --type "uint16"

// AI can then use this knowledge
User: "What is Link's position?"
Agent: [checks learned memory map]
       [calls emulator-read-memory address=0x7E0010 count=2 type=uint16]
```

**Storage**:
```json
// ~/.yaze/agent/memory_maps/zelda3.json
{
  "rom_hash": "abc123...",
  "symbols": {
    "0x7E0000": {"name": "link_hp", "type": "uint8", "description": "Link's health"},
    "0x7E0010": {"name": "link_x_pos", "type": "uint16", "description": "Link X coordinate"},
    "0x7E0012": {"name": "link_y_pos", "type": "uint16", "description": "Link Y coordinate"}
  }
}
```

**Estimated Effort**: 6-8 hours

---

## 📊 Phase 6: Performance Profiling

### 6.1 Cycle Counter & Profiler

**Mesen2 Feature**: Shows which code is hot, helps optimize hacks

**Implementation**:
```cpp
class PerformanceProfiler {
public:
  struct FunctionProfile {
    uint32_t start_address;
    uint32_t end_address;
    std::string name;
    uint64_t total_cycles;
    uint32_t call_count;
    float percentage;
  };
  
  void StartProfiling();
  void StopProfiling();
  std::vector<FunctionProfile> GetHotFunctions(int top_n = 20);
  void ExportFlameGraph(const std::string& filename);
  
private:
  std::unordered_map<uint32_t, uint64_t> address_cycle_counts_;
};
```

**ImGui Visualization**:
```
┌────────────────────────────────────────────────┐
│ Performance Profiler                           │
├────────────────────────────────────────────────┤
│ Function           Cycles      Calls    %      │
│ NMI Handler        2,456,789   1,234   15.2%  │
│ Link Update        1,987,654   3,600   12.3%  │
│ PPU Transfer       1,234,567   890     7.6%   │
│ ...                                            │
├────────────────────────────────────────────────┤
│ [Start Profiling] [Stop] [Export Flame Graph] │
└────────────────────────────────────────────────┘
```

**z3ed Commands**:
```bash
z3ed emu profile start
# ... run game for a bit ...
z3ed emu profile stop
z3ed emu profile report --top 20
z3ed emu profile export --format flamegraph --output profile.svg
```

**Estimated Effort**: 10-12 hours

---

### 6.2 Frame Time Analysis

**Track frame timing issues**:
```cpp
class FrameTimeAnalyzer {
  struct FrameStats {
    float cpu_time_ms;
    float ppu_time_ms;
    float apu_time_ms;
    float total_time_ms;
    int dropped_frames;
  };
  
  void RecordFrame();
  FrameStats GetAverageStats(int last_n_frames = 60);
  std::vector<float> GetFrameTimeGraph(int frames = 300);  // 5 seconds
};
```

**Visualization**: Real-time graph showing frame time spikes

**Estimated Effort**: 4-6 hours

---

##  Phase 7: Event System & Timeline

### 7.1 Event Logger

**Mesen2 Feature**: Timeline view of all hardware events

**Events to Track**:
- NMI (V-Blank)
- IRQ (H-Blank, Timer)
- DMA transfers
- HDMA activations
- PPU mode changes
- Audio sample playback starts

**Implementation**:
```cpp
class EventLogger {
public:
  enum class EventType {
    NMI, IRQ, DMA, HDMA, PPU_MODE_CHANGE, APU_SAMPLE_START
  };
  
  struct Event {
    EventType type;
    uint64_t cycle;
    uint32_t pc;  // Where CPU was when event occurred
    std::string details;
  };
  
  void LogEvent(EventType type, const std::string& details);
  std::vector<Event> GetEvents(uint64_t start_cycle, uint64_t end_cycle);
  void Clear();
  
private:
  std::deque<Event> event_history_;  // Keep last 10,000 events
};
```

**Visualization**:
```
Timeline (last 5 frames):
Frame 1: [NMI]────[DMA]──[HDMA]─────────────────
Frame 2: [NMI]────[DMA]──────────[IRQ]──────────
Frame 3: [NMI]────[DMA]──[HDMA]─────────────────
         ^        ^      ^
         16.67ms  18ms   20ms (timing shown)
```

**Estimated Effort**: 8-10 hours

---

## 🧠 Phase 8: AI-Powered Debugging

### 8.1 Intelligent Crash Analysis

**Feature**: AI analyzes emulator state when game crashes/freezes

```bash
z3ed agent debug-crash --state latest_crash.state

# AI examines:
# - CPU registers and flags
# - Stack contents
# - Recent code execution
# - Memory watchpoint history
# - Event timeline

# AI response:
"The game crashed because:
1. Infinite loop detected at $00:8234
2. This is the NMI handler
3. It's waiting for PPU register 0x2137 to change
4. The value hasn't changed in 10,000 cycles
5. Likely cause: PPU is in wrong mode (Mode 0 instead of Mode 1)

Suggested fix:
- Check PPU mode initialization at game start
- Verify NMI handler only runs when PPU is ready"
```

**Estimated Effort**: 15-20 hours

---

### 8.2 Automated Bug Reproduction

**Feature**: AI creates minimal test case from bug description

```bash
z3ed agent repro --prompt "Link takes damage when he shouldn't"

# AI generates reproduction steps:
{
  "steps": [
    "load_state: link_full_hp.sfc",
    "move: right, 50 frames",
    "assert: no damage taken",
    "expected: HP stays at 0xA0",
    "actual: HP decreased to 0x90",
    "analysis: Enemy collision box too large"
  ]
}
```

**Estimated Effort**: 12-15 hours

---

##  Implementation Roadmap

### Sprint 1: Audio Fix (Week 1)
**Priority**: CRITICAL  
**Time**: 4 hours  
**Deliverables**:
-  Investigate audio buffer size mismatch
-  Add audio debug logging
-  Fix SDL2 audio output
-  Verify audio plays correctly

### Sprint 2: Basic Debugger (Weeks 2-3)
**Priority**: HIGH  
**Time**: 20 hours  
**Deliverables**:
-  Breakpoint manager with execute/read/write
-  Enhanced disassembly viewer with hotspots
-  Improved memory viewer with regions
-  z3ed CLI commands for debugging

### Sprint 3: SPC700 Debugger (Week 4)
**Priority**: MEDIUM  
**Time**: 15 hours  
**Deliverables**:
-  APU inspector window
-  Channel waveform visualization
-  Audio RAM viewer
-  Sample export to WAV

### Sprint 4: AI Integration (Weeks 5-6)
**Priority**: MEDIUM  
**Time**: 25 hours  
**Deliverables**:
-  Emulator state tools for z3ed agent
-  Memory map learning system
-  Automated test scenario generation
-  Crash analysis AI

### Sprint 5: Performance (Weeks 7-8)
**Priority**: LOW (Future)  
**Time**: 40+ hours  
**Deliverables**:
-  Cycle-accurate timing
-  Dynamic recompilation
-  Performance profiler
-  Frame pacing improvements

---

## 🔬 Technical Deep Dives

### Audio System Architecture (SDL2)

**Current Flow**:
```
SPC700 → APU::GetSamples() → Snes::SetSamples() → audio_buffer_
         → SDL_QueueAudio() → SDL Audio Device → System Audio
```

**Debug Points**:
```cpp
// 1. Check SPC700 output
void APU::GetSamples(int16_t* buffer, int count) {
  // Are samples being generated?
  LOG_IF_ZERO_SAMPLES(buffer, count);
}

// 2. Check buffer handoff
void Snes::SetSamples(int16_t* buffer, int count) {
  apu_.GetSamples(buffer, count);
  // Are samples copied correctly?
  VERIFY_BUFFER_NOT_SILENT(buffer, count);
}

// 3. Check SDL queue
if (SDL_QueueAudio(device, buffer, size) < 0) {
  LOG_ERROR("SDL_QueueAudio failed: %s", SDL_GetError());
}

// 4. Check device status
if (SDL_GetAudioDeviceStatus(device) != SDL_AUDIO_PLAYING) {
  LOG_ERROR("Audio device not playing!");
}
```

**Common Issues**:
1. **Buffer size mismatch** - Fixed in Phase 1
2. **Format mismatch** - SPC700 outputs float, SDL wants int16
3. **Device paused** - SDL_PauseAudioDevice() called somewhere
4. **No APU timing** - SPC700 not running or too slow

---

### Memory Regions Reference

| Region | Address Range | Size | Description |
|--------|--------------|------|-------------|
| WRAM | 0x7E0000-0x7FFFFF | 128KB | Work RAM (game state) |
| SRAM | 0x700000-0x77FFFF | Variable | Save RAM (battery) |
| ROM | 0x000000-0x3FFFFF | Up to 6MB | Cartridge ROM |
| VRAM | PPU Internal | 64KB | Video RAM (tiles, maps) |
| CGRAM | PPU Internal | 512B | Palette RAM (colors) |
| OAM | PPU Internal | 544B | Sprite RAM (objects) |
| ARAM | SPC700 Internal | 64KB | Audio RAM (samples) |

**Access Patterns**:
```cpp
// CPU → WRAM (direct)
uint8_t value = cpu.Read(0x7E0010);

// CPU → VRAM (through PPU registers)
cpu.Write(0x2118, low_byte);   // VRAM write
cpu.Write(0x2119, high_byte);

// CPU → ARAM (through APU registers)
cpu.Write(0x2140, data);  // APU I/O port 0
```

---

## Game Phase 9: Advanced Features (Mesen2 Parity)

### 9.1 Rewind Feature

**User Experience**: Hold button to rewind gameplay

```cpp
class RewindManager {
  void RecordFrame();  // Save state every frame
  void Rewind(int frames);
  
  // Circular buffer (last 10 seconds = 600 frames)
  std::deque<SaveState> frame_history_;
  static constexpr int kMaxFrames = 600;
};
```

**Memory Impact**: ~600 * 100KB = 60MB (acceptable)

**Estimated Effort**: 6-8 hours

---

### 9.2 TAS (Tool-Assisted Speedrun) Input Recording

**Feature**: Record and replay input sequences

```cpp
class InputRecorder {
  struct InputFrame {
    uint16_t buttons;  // SNES controller state
    uint64_t frame_number;
  };
  
  void StartRecording();
  void StopRecording();
  void SaveMovie(const std::string& filename);
  void PlayMovie(const std::string& filename);
  
  std::vector<InputFrame> recorded_inputs_;
};
```

**File Format** (JSON):
```json
{
  "rom_hash": "abc123...",
  "frames": [
    {"frame": 0, "buttons": 0x0000},
    {"frame": 60, "buttons": 0x0080},  // A button pressed
    {"frame": 61, "buttons": 0x0000}
  ]
}
```

**z3ed Integration**:
```bash
z3ed emu record start
# ... play game ...
z3ed emu record stop --output my_gameplay.json
z3ed emu replay --input my_gameplay.json --verify

# AI can generate TAS inputs!
z3ed agent tas --prompt "Beat the first dungeon as fast as possible"
```

**Estimated Effort**: 8-10 hours

---

### 9.3 Comparison Mode

**Feature**: Run two emulator instances side-by-side

**Use Case**: Compare vanilla vs hacked ROM, or before/after AI changes

```cpp
class ComparisonEmulator {
  Emulator emu_a_;
  Emulator emu_b_;
  
  void RunBothFrames();
  void RenderSideBySide();
  void HighlightDifferences();
};
```

**Visualization**:
```
┌──────────────────┬──────────────────┐
│   Vanilla ROM    │   Hacked ROM     │
├──────────────────┼──────────────────┤
│ [Game Screen A]  │ [Game Screen B]  │
│                  │                  │
│ HP: 6 ❤❤❤       │ HP: 12 ❤❤❤❤❤❤   │ ← Difference
│ Rupees: 50       │ Rupees: 50       │
└──────────────────┴──────────────────┘
Memory Diff: 147 bytes different
```

**Estimated Effort**: 12-15 hours

---

## Optimization Summary

### Quick Wins (< 1 week)
1. **Fix audio output** - 4 hours
2. **Add CPU breakpoints** - 6 hours
3. **Enhanced memory viewer** - 6 hours
4. **Frame pacing** - 3 hours

### Medium Term (1-2 months)
5. **Live disassembly** - 10 hours
6. **APU debugger** - 15 hours
7. **Event logger** - 8 hours
8. **AI emulator tools** - 25 hours

### Long Term (3-6 months)
9. **Cycle accuracy** - 30 hours
10. **Dynarec** - 60 hours
11. **TAS recording** - 10 hours
12. **Comparison mode** - 15 hours

---

## AI z3ed Agent Emulator Tools

### New Tool Categories

**Emulator Control**:
```json
{
  "name": "emulator-control",
  "actions": ["start", "stop", "pause", "reset", "step"],
  "description": "Control emulator execution"
}
```

**Memory Tools**:
```json
{
  "name": "emulator-read-memory",
  "parameters": {
    "address": "hex string (e.g., '0x7E0010')",
    "count": "number of bytes",
    "region": "wram|sram|rom|vram|aram"
  }
},
{
  "name": "emulator-write-memory",
  "parameters": {
    "address": "hex string",
    "data": "array of bytes"
  }
}
```

**State Tools**:
```json
{
  "name": "emulator-cpu-state",
  "returns": {
    "pc": "Program Counter",
    "a": "Accumulator",
    "x": "X Register",
    "y": "Y Register",
    "sp": "Stack Pointer",
    "flags": "Processor flags"
  }
},
{
  "name": "emulator-save-state",
  "parameters": {"filename": "string"}
},
{
  "name": "emulator-load-state",
  "parameters": {"filename": "string"}
}
```

**Debug Tools**:
```json
{
  "name": "emulator-add-breakpoint",
  "parameters": {
    "address": "hex string",
    "type": "execute|read|write|access",
    "condition": "optional expression"
  }
},
{
  "name": "emulator-get-hotspots",
  "parameters": {"top_n": "integer"},
  "returns": "array of most-executed code addresses"
}
```

### Example AI Conversations

**Debugging**:
```
User: "Why is Link's HP decreasing randomly?"

Agent: Let me investigate...
       [calls emulator-read-memory address=0x7E0000 count=1]
       → Current HP: 0x60 (6 hearts)
       
       [calls emulator-add-breakpoint address=0x7E0000 type=write]
       → Breakpoint set
       
       [waits for breakpoint hit]
       → Breakpoint hit! PC=$00:8234
       
       [calls emulator-read-memory address=0x8234 count=10]
       → Disassembly shows: Enemy collision handler
       
Agent: "Link's HP is being modified by the enemy collision code at $00:8234.
       The collision box is checking at the wrong Y offset (+8 instead of +16).
       This causes damage when Link is near but not touching enemies."
```

**Testing**:
```
User: "Test that the new sword does double damage"

Agent: [calls emulator-load-state file=enemy_encounter.sfc]
       [calls emulator-read-memory address=0x7E0010 count=2]  # Link's damage
       → Current damage: 0x0004
       
       [calls emulator-write-memory address=0x7E0010 data=[0x08]]  # Set 2x
       
       [simulates sword swing - could use input recording]
       
       [calls emulator-read-memory address=0x7EF280]  # Enemy HP
       → Enemy lost 8 HP (was 4 before)
       
Agent: "Confirmed! The new sword deals 8 damage instead of 4. 
       The 2x multiplier is working correctly."
```

---

## 📁 File Structure for New Features

```
src/app/emu/
├── debug/
│   ├── breakpoint_manager.{h,cc}          # NEW
│   ├── watchpoint_manager.{h,cc}          # NEW
│   ├── disassembly_viewer.{h,cc}          # EXISTS - enhance
│   ├── memory_viewer.{h,cc}               # NEW
│   ├── event_logger.{h,cc}                # NEW
│   ├── performance_profiler.{h,cc}        # NEW
│   └── apu_inspector.{h,cc}               # NEW
├── tas/
│   ├── input_recorder.{h,cc}              # NEW
│   ├── movie_file.{h,cc}                  # NEW
│   └── rewind_manager.{h,cc}              # NEW
└── emulator.{h,cc}                        # EXISTS - integrate above

src/cli/commands/agent/
├── emulator_tools.{h,cc}                  # NEW - z3ed agent emulator commands
└── test_scenario_runner.{h,cc}            # NEW - automated testing

src/cli/service/agent/
└── tools/
    ├── emulator_control_tool.cc           # NEW
    ├── emulator_memory_tool.cc            # NEW
    └── emulator_debug_tool.cc             # NEW
```

---

## 🎨 UI Mockups

### Debugger Layout (ImGui)

```
┌────────────────────────────────────────────────────────────────┐
│ YAZE Emulator - Debugging Mode                                │
├───────────────┬────────────────────────┬───────────────────────┤
│               │                        │                       │
│ Disassembly   │   Game Display         │   Registers          │
│               │                        │                       │
│ 00:8000 LDA   │  ┌──────────────────┐  │  A: 0x00  X: 0x05    │
│ 00:8002 STA   │  │                  │  │  Y: 0xFF  SP: 0xEF   │
│►00:8004 JMP   │  │    [Zelda 3]     │  │  PC: 0x8004          │
│ 00:8007 NOP   │  │                  │  │  PB: 0x00  DB: 0x00  │
│ 00:8008 RTL   │  └──────────────────┘  │  Flags: nv--dizc     │
│               │                        │                       │
├───────────────┼────────────────────────┼───────────────────────┤
│               │                        │                       │
│ Memory        │   Event Timeline       │   Stack              │
│               │                        │                       │
│ 7E0000  00 05 │  Frame 123:            │  0x1FF: 0x00         │
│ 7E0008  3C 00 │  [NMI]──[DMA]──[IRQ]   │  0x1FE: 0x80         │
│ 7E0010  1F 00 │                        │  0x1FD: 0x04 ← SP    │
│               │                        │                       │
└───────────────┴────────────────────────┴───────────────────────┘
```

### APU Debugger Layout

```
┌────────────────────────────────────────────────────────────────┐
│ SPC700 Audio Debugger                                          │
├────────────────────┬───────────────────────────────────────────┤
│ SPC700 Disassembly │ DSP State                                │
│                    │                                           │
│ 0100 MOV A,#$00    │ Master Volume: L=127 R=127               │
│►0102 MOV (X),A     │ Echo: OFF  FIR: Standard                 │
│ 0104 INCX          │                                           │
│                    │ Channel 0: ████████░░░░ (75%)            │
├────────────────────┤   VOL: L=100 R=100  PITCH=2048           │
│ Audio RAM (64KB)   │   ADSR: Attack=15 Decay=7 Sustain=7      │
│                    │   Sample: 0x0000-0x1234 (BRR)            │
│ 0000  00 00 00 00  │                                           │
│ 0010  BRR BRR ...  │ Channel 1: ░░░░░░░░░░░░ (0%)            │
│                    │   (Inactive)                              │
│ [Export Samples]   │                                           │
└────────────────────┴───────────────────────────────────────────┘
```

---

##  Performance Targets

### Current Performance
- **Emulation Speed**: ~60 FPS (with frame skipping)
- **CPU Usage**: 40-60% (one core)
- **Memory**: 80MB (emulator only)
- **Accuracy**: ~85% (some timing issues)

### Target Performance (Post-Optimization)
- **Emulation Speed**: Solid 60 FPS (no skipping)
- **CPU Usage**: 20-30% (with dynarec)
- **Memory**: 100MB (with debug features)
- **Accuracy**: 98%+ (cycle-accurate)

### Optimization Strategy Priority
1. **Audio fix** - Enables testing with sound
2. **Frame pacing** - Eliminates jitter
3. **Hotspot profiling** - Identifies slow code
4. **Cycle accuracy** - Fixes timing bugs
5. **Dynarec** - 3x speed boost

---

## 🧪 Testing Integration

### Automated Emulator Tests (z3ed)

**Unit Tests**:
```bash
# Test CPU instructions
z3ed emu test cpu --test-suite 65816_opcodes.json

# Test PPU rendering
z3ed emu test ppu --test-rom ppu_test.sfc --frames 600

# Test APU audio
z3ed emu test apu --test-rom audio_test.sfc --export samples.wav
```

**Regression Tests**:
```bash
# Run all ROM compatibility tests
z3ed emu test regression --rom-dir test_roms/ --report report.html

# Compare with reference emulator
z3ed emu test compare --rom zelda3.sfc --reference bsnes --frames 1000
```

**AI-Generated Tests**:
```bash
z3ed agent test-scenario --prompt "Test that bombs damage enemies"

# AI generates emulator test script
{
  "setup": {
    "load_state": "link_with_bombs.sfc"
  },
  "steps": [
    {"action": "place-bomb", "x": 100, "y": 100},
    {"action": "wait-frames", "count": 60},
    {"action": "spawn-enemy", "type": "octorok", "x": 110, "y": 100},
    {"action": "wait-frames", "count": 60},
    {"action": "assert-memory", "address": "0x7EF280", "comparison": "less_than", "reference": "initial"}
  ]
}
```

---

## 🔌 z3ed Agent + Emulator Integration

### New Agent Tools

**Tool Registration** (automatic):
```cpp
// src/cli/service/agent/tool_dispatcher.cc
void ToolDispatcher::RegisterEmulatorTools(Emulator* emulator) {
  RegisterTool("emulator-start", [emulator]() { /* ... */ });
  RegisterTool("emulator-stop", [emulator]() { /* ... */ });
  RegisterTool("emulator-read-memory", [emulator](args) { /* ... */ });
  RegisterTool("emulator-cpu-state", [emulator]() { /* ... */ });
  RegisterTool("emulator-screenshot", [emulator]() { /* ... */ });
  // ... 15 more tools
}
```

**Example Conversations**:

**1. Live Debugging**:
```
User: "Why does Link keep getting stuck in walls?"

Agent: Let me debug this...
       [calls emulator-load-state file=link_stuck.sfc]
       [calls emulator-cpu-state]
       → PC=$00:9ABC (Link movement handler)
       
       [calls emulator-read-memory address=0x7E0022 count=2]
       → Link X: 0x0478, Y: 0x0234
       
       [calls emulator-read-memory address=0x7E0020 count=2]
       → Subpixel X: 0x0F, Y: 0x0F (stuck at boundary!)
       
       [calls emulator-add-breakpoint address=0x9ABC type=execute]
       [calls emulator-step]
       
Agent: "Found it! The collision detection at $00:9ABC is using
       integer division instead of proper subpixel handling.
       When Link's subpixel position is 0x0F, the boundary
       check fails and he gets stuck."
```

**2. Performance Analysis**:
```
User: "The overworld feels laggy after my changes"

Agent: [calls emulator-start]
       [calls emulator-profile-start]
       [waits 300 frames = 5 seconds]
       [calls emulator-profile-stop]
       [calls emulator-get-hotspots top_n=10]
       
       → Top hotspot: $00:8500 (45% of cycles)
       
       [calls emulator-disassemble address=0x8500 count=20]
       
Agent: "Your tile drawing code at $00:8500 is running every frame
       and taking 45% of CPU time! The original game only runs this
       when tiles change. Consider adding a dirty flag check."
```

**3. Automated ROM Testing**:
```
User: "Test my randomizer hack thoroughly"

Agent: I'll run comprehensive tests...
       [calls emulator-load-state file=randomizer_start.sfc]
       
       For each dungeon:
         [calls emulator-teleport-link dungeon=N]
         [calls emulator-run-frames count=3600]  # 60 seconds
         [calls emulator-screenshot name=dungeon_N.png]
         [calls emulator-check-crashes]
         [calls emulator-verify-items-reachable]
       
       [calls emulator-generate-report]
       
Agent: "Tested all 12 dungeons. Found issues:
       - Dungeon 3: Softlock at room 0x34 (missing key)
       - Dungeon 7: Crash at room 0x12 (invalid sprite ID)
       All other dungeons:  PASS"
```

---

## 🎓 Learning from Mesen2

### What Makes Mesen2 Great

1. **Comprehensive Debugging**
   - Every hardware event is logged
   - Full state inspection at any time
   - Breakpoints on everything (CPU, PPU, APU, memory)

2. **Performance**
   - Cycle-accurate yet fast (dynarec)
   - 60 FPS even with debugging enabled
   - Efficient state save/restore

3. **User Experience**
   - Integrated debugger in same window as game
   - Real-time visualization of state changes
   - Intuitive UI for complex operations

4. **Extensibility**
   - Lua scripting for automation
   - Event system for plugins
   - Export capabilities (traces, memory dumps)

### Our Unique Advantages

**YAZE + z3ed has features Mesen2 doesn't**:

1. **AI Integration**
   - Natural language debugging
   - Automated test generation
   - Intelligent crash analysis

2. **ROM Editor Integration**
   - Edit ROM while emulator runs
   - See changes immediately
   - Debugging informs editing

3. **Collaborative Debugging**
   - Share emulator state with team
   - Remote debugging via gRPC
   - AI agent can help multiple users

4. **Cross-Platform Testing**
   - Same emulator in CLI and GUI
   - Automated test scenarios
   - CI/CD integration

---

## 📊 Resource Requirements

### Development Time Estimates

| Phase | Hours | Weeks (Part-Time) |
|-------|-------|-------------------|
| Audio Fix | 4 | 0.5 |
| Basic Debugger | 20 | 2.5 |
| SPC700 Debugger | 15 | 2 |
| AI Integration | 25 | 3 |
| Performance Opts | 40 | 5 |
| **Total** | **104** | **13** |

### Memory Requirements

| Feature | RAM Usage |
|---------|-----------|
| Base Emulator | 80 MB |
| Breakpoint Manager | +2 MB |
| Event Logger | +5 MB (10K events) |
| Rewind Buffer | +60 MB (10 seconds) |
| Performance Profiler | +10 MB |
| **Total** | **~160 MB** |

### CPU Requirements

| Configuration | CPU % (Single Core) |
|--------------|---------------------|
| Interpreter Only | 40-60% |
| + Debugging | 50-70% |
| + Profiling | 60-80% |
| + Dynarec (future) | 20-30% |

---

## Recommended Implementation Order

### Month 1: Foundation
**Weeks 1-2**: Audio fix + Basic breakpoints  
**Weeks 3-4**: Memory viewer + Disassembly enhancements

### Month 2: Audio & Events
**Weeks 5-6**: SPC700 debugger + APU inspector  
**Weeks 7-8**: Event logger + Timeline view

### Month 3: AI Integration
**Weeks 9-10**: z3ed emulator tools + Agent integration  
**Weeks 11-12**: Automated testing + Scenario runner

### Month 4: Performance
**Weeks 13-14**: Cycle accuracy refactor  
**Weeks 15-16**: Dynarec or JIT library integration

### Month 5: Polish
**Weeks 17-18**: UI/UX improvements  
**Weeks 19-20**: Documentation + Examples

---

## 🔮 Future Vision: AI-Powered ROM Hacking

### The Ultimate Workflow

1. **AI Explores the Game**
```
z3ed agent explore --rom zelda3.sfc --goal "Find all heart piece locations"

# Agent:
# - Loads ROM in emulator
# - Runs around overworld automatically
# - Detects heart piece spawn events via memory watchpoints
# - Screenshots each location
# - Generates report with coordinates
```

2. **AI Debugs Your Hack**
```
z3ed agent debug --rom my_hack.sfc --issue "Boss doesn't take damage"

# Agent:
# - Loads hack in emulator
# - Adds breakpoints on damage handlers
# - Simulates boss fight
# - Identifies missing damage check
# - Suggests code fix with hex addresses
```

3. **AI Generates TAS**
```
z3ed agent speedrun --rom zelda3.sfc --category "any%"

# Agent:
# - Studies game mechanics via emulator
# - Discovers optimal movement patterns
# - Generates frame-perfect input sequence
# - Exports TAS movie file
```

4. **AI Validates Randomizers**
```
z3ed agent validate --rom randomizer.sfc --seed 12345

# Agent:
# - Generates logic graph from ROM
# - Simulates playthrough via emulator
# - Verifies all items are reachable
# - Checks for softlocks
# - Rates difficulty
```

---

## 🐛 Appendix A: Audio Debugging Checklist

**Run these checks to diagnose audio issues**:

### Check 1: Device Status
```cpp
SDL_AudioStatus status = SDL_GetAudioDeviceStatus(audio_device_);
printf("Audio Status: %d (1=playing, 2=paused, 3=stopped)\n", status);
// Expected: 1 (SDL_AUDIO_PLAYING)
```

### Check 2: Queue Size
```cpp
uint32_t queued = SDL_GetQueuedAudioSize(audio_device_);
printf("Queued Audio: %u bytes\n", queued);
// Expected: 1000-8000 bytes (1-2 frames worth)
// If 0: Not queueing
// If >50000: Overflowing, audio thread stalled
```

### Check 3: Sample Validation
```cpp
int16_t* samples = audio_buffer_;
bool all_zero = true;
for (int i = 0; i < wanted_samples_ * 2; i++) {
  if (samples[i] != 0) {
    all_zero = false;
    break;
  }
}
if (all_zero) {
  printf("ERROR: All audio samples are zero! SPC700 not outputting.\n");
}
```

### Check 4: Buffer Allocation
```cpp
// In window.cc:128, verify size calculation:
// For 48000Hz, 60 FPS, stereo:
// samples_per_frame = 48000 / 60 = 800
// stereo_samples = 800 * 2 = 1600 int16_t
// Size should be: 1600, NOT 3840

printf("Audio buffer size: %zu int16_t\n", audio_buffer_.size());
// Expected: 1600-1920 (for 60-50Hz)
```

### Check 5: SPC700 Execution
```cpp
// Verify SPC700 is actually running
uint64_t apu_cycles = snes_.apu().GetCycles();
// Should increase every frame
// If stuck: SPC700 deadlock or not running
```

### Quick Fixes to Try

**Fix A: Force Unpause**
```cpp
// emulator.cc, in RunFrame():
SDL_PauseAudioDevice(audio_device_, 0);  // Force play state
```

**Fix B: Larger Queue**
```cpp
// If buffer underruns, queue more:
SDL_QueueAudio(audio_device_, audio_buffer_, wanted_samples_ * 4 * 2);  // 2 frames
```

**Fix C: Clear Stale Queue**
```cpp
// If queue is stuck:
if (SDL_GetQueuedAudioSize(audio_device_) > 50000) {
  SDL_ClearQueuedAudio(audio_device_);  // Reset
}
```

---

## 📝 Appendix B: Mesen2 Feature Reference

### Debugger Windows (Inspiration)

1. **CPU Debugger**: Disassembly, registers, breakpoints
2. **Memory Tools**: Hex viewer, search, compare
3. **PPU Viewer**: Layer toggles, VRAM, OAM, palettes
4. **Event Viewer**: Timeline of all hardware events
5. **Trace Logger**: Full execution log with filters
6. **Performance Profiler**: Hotspot analysis
7. **Script Window**: Lua scripting for automation

### Event Types Tracked

- **CPU**: NMI, IRQ, BRK instruction, RESET
- **PPU**: V-Blank, H-Blank, Mode change, Sprite overflow
- **DMA**: General DMA, HDMA, channels used
- **APU**: Sample playback, DSP writes, Timer IRQ
- **Cart**: Save RAM write, special chip events

### Trace Logger Format

```
Cycle    PC      Opcode  A  X  Y  SP  Flags  Event
0        $00FFD9 SEI     00 00 00 1FF nvmxdiZc
1        $00FFDA CLI     00 00 00 1FF nvmxdizc
2        $00FFDB JMP     00 00 00 1FF nvmxdizc
...
16749    $008234 LDA     05 00 00 1EF Nvmxdizc [NMI]
```

---

##  Success Criteria

### Phase 1 Complete When:
-  Audio plays correctly from SDL2
-  Can hear game music and sound effects
-  No audio crackling or dropouts
-  Audio buffer diagnostics implemented

### Phase 2 Complete When:
-  Can set breakpoints on any address
-  Disassembly view shows live execution
-  Memory viewer has multi-region support
-  z3ed CLI can control debugger

### Phase 3 Complete When:
-  SPC700 debugger shows all APU state
-  Can visualize audio channels
-  Can export audio samples to WAV

### Phase 4 Complete When:
-  AI agent can read emulator memory
-  AI agent can control emulation
-  AI can generate test scenarios
-  Automated ROM testing works

### Phase 5 Complete When:
-  Emulator is cycle-accurate
-  60 FPS maintained with debugging
-  Dynarec provides 3x speedup
-  All Mesen2 features implemented

---

## 🎓 Learning Resources

### SNES Emulation
- [SNES Development Manual](https://www.romhacking.net/documents/226/)
- [Fullsnes by nocash](https://problemkaputt.de/fullsnes.htm)
- [bsnes source code](https://github.com/bsnes-emu/bsnes) - Reference implementation
- [Mesen2 source code](https://github.com/SourMesen/Mesen2) - Feature inspiration

### Audio Debugging
- [SPC700 Reference](https://wiki.superfamicom.org/spc700-reference)
- [DSP Register Guide](https://wiki.superfamicom.org/dsp-registers)
- [BRR Audio Format](https://wiki.superfamicom.org/bit-rate-reduction-brr)

### Performance Optimization
- [Fast SNES Emulation](https://github.com/arm9/snes9x-rpi) - ARM optimization
- [Dynarec Tutorial](https://github.com/rasky/r64emu/wiki/Dynamic-Recompilation) - N64 but applicable

---

##  Credits & Acknowledgments

**Emulator Core**: Based on LakeSnes by elzo-d  
**Debugger Inspiration**: Mesen2 by SourMesen  
**AI Integration**: z3ed agent system  
**Documentation**: With love (and Puerto Rican soup! 🍲)

---

*Document Version: 1.0*  
*Last Updated: October 8, 2025*  
*Next Review: After Audio Fix*  
*Sleep Well! 😴*

