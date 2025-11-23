# Codebase Investigation: Yaze vs Mesen2 SNES Emulation

## Executive Summary

This investigation compares the architecture of `yaze` (Yet Another Zelda Editor's emulator) with `Mesen2` (a high-accuracy multi-system emulator). The goal is to identify areas where `yaze` can be improved to approach `Mesen2`'s level of accuracy.

**Fundamental Difference:**
*   **Yaze** is an **instruction-level / scanline-based** emulator. It executes entire CPU instructions at once and catches up other subsystems (APU, PPU) at specific checkpoints (memory access, scanline end).
*   **Mesen2** is a **bus-level / cycle-based** emulator. It advances the system state (timers, DMA, interrupts) on every single CPU bus cycle (read/write/idle), allowing for sub-instruction synchronization.

## Detailed Comparison

### 1. CPU Timing & Bus Arbitration

| Feature | Yaze (`Snes::RunOpcode`, `Cpu::ExecuteInstruction`) | Mesen2 (`SnesCpu::Exec`, `Read/Write`) |
| :--- | :--- | :--- |
| **Granularity** | Executes full instruction, then adds cycles. Batches bus cycles around memory accesses. | Executes micro-ops. `Read/Write` calls `ProcessCpuCycle` to advance system state *per byte*. |
| **Timing** | `Snes::CpuRead` runs `access_time - 4` cycles, reads, then `4` cycles. | `SnesCpu::Read` determines speed (`GetCpuSpeed`), runs cycles, then reads. |
| **Interrupts** | Checked at instruction boundaries (`RunOpcode`). | Checked on every cycle (`ProcessCpuCycle` -> `DetectNmiSignalEdge`). |

**Improvement Opportunity:**
The current `yaze` approach of batching cycles in `CpuRead` (`RunCycles(access_time - 4)`) is a good approximation but fails for edge cases where an IRQ or DMA might trigger *during* an instruction's execution (e.g., between operand bytes).
*   **Recommendation:** Refactor `Cpu::ReadByte` / `Cpu::WriteByte` callbacks to advance the system clock *before* returning data. This moves `yaze` closer to a cycle-stepped architecture without rewriting the entire core state machine.

### 2. PPU Rendering & Raster Effects

| Feature | Yaze (`Ppu::RunLine`) | Mesen2 (`SnesPpu`) |
| :--- | :--- | :--- |
| **Rendering** | Scanline-based. Renders full line at H=512 (`next_horiz_event`). | Dot-based (effectively). Handles cycle-accurate register writes. |
| **Mid-Line Changes** | Register writes (`WriteBBus`) update internal state immediately, but rendering only happens later. **Raster effects (H-IRQ) will apply to the whole line or be missed.** | Register writes catch up the renderer to the current dot before applying changes. |

**Improvement Opportunity:**
This is the biggest accuracy gap. Games like *Tales of Phantasia* or *Star Ocean* that use raster effects (changing color/brightness/windowing mid-scanline) will not render correctly in `yaze`.
*   **Recommendation:** Implement a **"Just-In-Time" PPU Catch-up**.
    *   Add a `Ppu::CatchUp(uint16_t h_pos)` method.
    *   Call `ppu_.CatchUp(memory_.h_pos())` inside `Snes::WriteBBus` (PPU register writes).
    *   `CatchUp` should render pixels from `last_rendered_x` to `current_x`, then update `last_rendered_x`.

### 3. APU Synchronization

| Feature | Yaze (`Snes::CatchUpApu`) | Mesen2 (`Spc::IncCycleCount`) |
| :--- | :--- | :--- |
| **Sync Method** | Catch-up. Runs APU to match CPU master cycles on every port read/write (`ReadBBus`/`WriteBBus`). | Cycle interleaved. |
| **Ratio** | Fixed-point math (`kApuCyclesNumerator`...). | Floating point ratio derived from sample rates. |

**Assessment:**
`yaze`'s APU synchronization strategy is actually very robust. Calling `CatchUpApu` on every IO port access (`$2140-$2143`) ensures the SPC700 sees the correct data timing relative to the CPU. The handshake tracker (`ApuHandshakeTracker`) confirms this logic is working well for boot sequences.
*   **Recommendation:** No major architectural changes needed here. Focus on `Spc700` opcode accuracy and DSP mixing quality.

### 4. Input & Auto-Joypad Reading

| Feature | Yaze (`Snes::HandleInput`) | Mesen2 (`InternalRegisters::ProcessAutoJoypad`) |
| :--- | :--- | :--- |
| **Timing** | Runs once at VBlank start. Populates all registers immediately. | Runs continuously over ~4224 master clocks during VBlank. |
| **Accuracy** | Games reading `$4218` too early in VBlank will see finished data (correct values, wrong timing). | Games reading too early see 0 or partial data. |

**Improvement Opportunity:**
Some games rely on the *duration* of the auto-joypad read to time their VBlank routines.
*   **Recommendation:** Implement a state machine for auto-joypad reading in `Snes::RunCycle`. Instead of filling `port_auto_read_` instantly, fill it bit-by-bit over the correct number of cycles.

## 5. AI & Editor Integration Architecture

To support AI-driven debugging and dynamic editor integration (e.g., "Teleport & Test"), the emulator must evolve from a "black box" to an observable, controllable simulation.

### A. Dynamic State Injection (The "Test Sprite" Button)
Currently, testing requires a full reset or loading a binary save state. We need a **State Patching API** to programmatically set up game scenarios.

*   **Proposal:** `Emulator::InjectState(const GameStatePatch& patch)`
    *   **`GameStatePatch`**: A structure containing target WRAM values (e.g., Room ID, Coordinates, Inventory) and CPU state (PC location).
    *   **Workflow:**
        1.  **Reset & Fast-Boot:** Reset emulator and fast-forward past the boot sequence (e.g., until `GameMode` RAM indicates "Gameplay").
        2.  **Injection:** Pause execution and write the `patch` values directly to WRAM/SRAM.
        3.  **Resume:** Hand control to the user or AI agent.
    *   **Use Case:** "Test this sprite in Room 0x12." -> The editor builds a patch setting `ROOM_ID=0x12`, `LINK_X=StartPos`, and injects it.

### B. Semantic Inspection Layer (The "AI Eyes")
Multimodal models struggle with raw pixel streams for precise logic debugging. They need a "semantic overlay" that grounds visuals in game data.

*   **Proposal:** `SemanticIntrospectionEngine`
    *   **Symbol Mapping:** Uses `SymbolProvider` and `MemoryMap` (from `yaze` project) to decode raw RAM into meaningful concepts.
    *   **Structured Context:** Expose a method `GetSemanticState()` returning JSON/Struct:
        ```json
        {
          "mode": "Underworld",
          "room_id": 24,
          "link": { "x": 1200, "y": 800, "state": "SwordSlash", "hp": 16 },
          "sprites": [
            { "id": 0, "type": "Stalfos", "x": 1250, "y": 800, "state": "Active", "hp": 2 }
          ]
        }
        ```
    *   **Visual Grounding:** Provide an API to generate "debug frames" where hitboxes and interaction zones are drawn over the game feed. This allows Vision Models to correlate "Link is overlapping Stalfos" visually with `Link.x ~= Stalfos.x` logically.

### C. Headless & Fast-Forward Control
For automated verification (e.g., "Does entering this room crash?"), rendering overhead is unnecessary.

*   **Proposal:** Decoupled Rendering Pipeline
    *   Allow `Emulator` to run in **"Headless Mode"**:
        *   PPU renders to a simplified RAM buffer (or skips rendering if only logic is being tested).
        *   Audio backend is disabled or set to `NullBackend`.
        *   Execution speed is uncapped (limited only by CPU).
    *   **`RunUntil(Condition)` API:** Allow the agent to execute complex commands like:
        *   `RunUntil(PC == 0x8000)` (Breakpoint match)
        *   `RunUntil(Memory[0x10] == 0x01)` (Game mode change)
        *   `RunUntil(FrameCount == Target + 60)` (Time duration)

## Recent Improvements

### SDL3 Audio Backend (2025-11-23)

A new SDL3 audio backend has been implemented to modernize the emulator's audio subsystem:

**Implementation Details:**
- **Stream-based architecture**: Replaces SDL2's queue-based approach with SDL3's `SDL_AudioStream` API
- **Files added**:
  - `src/app/emu/audio/sdl3_audio_backend.h/cc` - Complete SDL3 backend implementation
  - `src/app/platform/sdl_compat.h` - Cross-version compatibility layer
- **Factory integration**: `AudioBackendFactory` now supports `BackendType::SDL3`
- **Resampling support**: Native handling of SPC700's 32kHz output to device rate
- **Volume control**: Optimized fast-path for unity gain (common case)

**Benefits:**
- Lower audio latency potential with stream-based processing
- Better synchronization between audio and video subsystems
- Native resampling reduces CPU overhead for rate conversion
- Future-proof architecture aligned with SDL3's design philosophy

**Testing:**
- Unit tests added in `test/unit/sdl3_audio_backend_test.cc`
- Conditional compilation via `YAZE_USE_SDL3` flag ensures backward compatibility
- Seamless fallback to SDL2 when SDL3 unavailable

## Action Plan

To upgrade `yaze` for both accuracy and AI integration, follow this implementation order:

1.  **PPU Catch-up (Accuracy - High Impact)**
    *   Modify `Ppu` to track `last_rendered_x`.
    *   Split `RunLine` into `RenderRange(start_x, end_x)`.
    *   Inject `ppu_.CatchUp()` calls in `Snes::WriteBBus`.

2.  **Semantic Inspection API (AI - High Impact)**
    *   Create `SemanticIntrospectionEngine` class.
    *   Connect it to `Memory` and `SymbolProvider`.
    *   Implement basic `GetPlayerState()` and `GetSpriteState()` using known ALTTP RAM offsets.

3.  **State Injection API (Integration - Medium Impact)**
    *   Implement `Emulator::InjectState`.
    *   Add specific "presets" for common ALTTP testing scenarios (e.g., "Dungeon Test", "Overworld Test").

4.  **Refined CPU Timing (Accuracy - Low Impact, High Effort)**
    *   Audit `Cpu::ExecuteInstruction` for missing `callbacks_.idle()` calls.
    *   Ensure "dummy read" cycles in RMW instructions trigger side effects.

5.  **Auto-Joypad Progressive Read (Accuracy - Low Impact)**
    *   Change `auto_joy_timer_` to drive bit-shifting in `port_auto_read_` registers.