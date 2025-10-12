# Emulator Core Improvements Roadmap

**Last Updated:** October 10, 2025  
**Status:** Active Planning

## Overview

This document outlines improvements, refactors, and optimizations for the yaze emulator core. These changes aim to enhance accuracy, performance, and code maintainability.

Items are presented in order of descending priority, from critical accuracy fixes to quality-of-life improvements.

---

## Critical Priority: APU Timing Fix

### Problem Statement

The emulator's Audio Processing Unit (APU) currently fails to load and play music. Analysis shows that the SPC700 processor gets "stuck" during the initial handshake sequence with the main CPU. This handshake is responsible for uploading the sound driver from ROM to APU RAM. The failure of this timing-sensitive process prevents the sound driver from running.

### Root Cause: CPU-APU Handshake Timing

The process of starting the APU and loading a sound bank requires tightly synchronized communication between the main CPU (65816) and the APU's CPU (SPC700).

#### The Handshake Protocol

1.  **APU Ready**: SPC700 boots, initializes, signals ready by writing `$AA` to port `$F4` and `$BB` to port `$F5`
2.  **CPU Waits**: Main CPU waits in tight loop, reading combined 16-bit value from I/O ports until it sees `$BBAA`
3.  **CPU Initiates**: CPU writes command code `$CC` to APU's input port
4.  **APU Acknowledges**: SPC700 sees `$CC` and prepares to receive data block
5.  **Synchronized Byte Transfer**: CPU and APU enter lock-step loop to transfer sound driver byte-by-byte:
    *   CPU sends data
    *   CPU waits for APU to read data and echo back confirmation
    *   Only upon receiving confirmation does CPU send next byte

#### Point of Failure

The "stuck" behavior occurs because one side fails to meet the other's expectation. Due to timing desynchronization:
*   The SPC700 is waiting for a byte that the CPU has not yet sent (or sent too early), OR
*   The CPU is waiting for an acknowledgment that the SPC700 has already sent (or has not yet sent)

The result is an infinite loop on the SPC700, detected by the watchdog timer in `Apu::RunCycles`.

### Technical Analysis

The handshake's reliance on precise timing exposes inaccuracies in the current SPC700 emulation model.

#### Issue 1: Incomplete Opcode Timing

The emulator uses a static lookup table (`spc700_cycles.h`) for instruction cycle counts. This provides a *base* value but fails to account for:
*   **Addressing Modes**: Different addressing modes have different cycle costs
*   **Page Boundaries**: Memory accesses crossing 256-byte page boundaries take an extra cycle
*   **Branching**: Conditional branches take different cycle counts depending on whether branch is taken

While some of this is handled (e.g., `DoBranch`), it is not applied universally, leading to small, cumulative errors.

#### Issue 2: Fragile Multi-Step Execution Model

The `step`/`bstep` mechanism in `Spc700::RunOpcode` is a significant source of fragility. It attempts to model complex instructions by spreading execution across multiple calls. This means the full cycle cost of an instruction is not consumed atomically. An off-by-one error in any step corrupts the timing of the entire APU.

#### Issue 3: Floating-Point Precision

The use of `double` for the `apuCyclesPerMaster` ratio can introduce minute floating-point precision errors. Over thousands of cycles required for the handshake, these small errors accumulate and contribute to timing drift between CPU and APU.

### Proposed Solution: Cycle-Accurate Refactoring

#### Step 1: Implement Cycle-Accurate Instruction Execution

The `Spc700::RunOpcode` function must be refactored to calculate and consume the *exact* cycle count for each instruction *before* execution.

*   **Calculate Exact Cost**: Before running an opcode, determine its precise cycle cost by analyzing opcode, addressing mode, and potential page-boundary penalties
*   **Atomic Execution**: Remove the `bstep` mechanism. An instruction, no matter how complex, should be fully executed within a single call to a new `Spc700::Step()` function

#### Step 2: Centralize the APU Execution Loop

The main `Apu::RunCycles` loop should be the sole driver of APU time.

*   **Cycle Budget**: At the start of a frame, calculate the total "budget" of APU cycles needed
*   **Cycle-by-Cycle Stepping**: Loop, calling `Spc700::Step()` and `Dsp::Cycle()`, decrementing cycle budget until exhausted

**Example of the new loop in `Apu::RunCycles`:**
```cpp
void Apu::RunCycles(uint64_t master_cycles) {
  // 1. Calculate cycle budget for this frame
  const uint64_t target_apu_cycles = ...; 

  // 2. Run the APU until the budget is met
  while (cycles_ < target_apu_cycles) {
    // 3. Execute one SPC700 cycle/instruction and get its true cost
    int spc_cycles_consumed = spc700_.Step();
    
    // 4. Advance DSP and Timers for each cycle consumed
    for (int i = 0; i < spc_cycles_consumed; ++i) {
      Cycle(); // This ticks the DSP and timers
    }
  }
}
```

#### Step 3: Use Integer-Based Cycle Ratios

To eliminate floating-point errors, convert the `apuCyclesPerMaster` ratio to a fixed-point integer ratio. This provides perfect, drift-free conversion between main CPU and APU cycles over long periods.

---

## High Priority: Core Architecture & Timing Model

### CPU Cycle Counting

*   **Issue:** The main CPU loop in `Snes::RunCycle()` advances the master cycle counter by a fixed amount (`+= 2`). Real 65816 instructions have variable cycle counts. The current workaround of scattering `callbacks_.idle()` calls is error-prone and difficult to maintain.
*   **Recommendation:** Refactor `Cpu::ExecuteInstruction` to calculate and return the *precise* cycle cost of each instruction, including penalties for addressing modes and memory access speeds. The main `Snes` loop should then consume this exact value, centralizing timing logic and dramatically improving accuracy.

### Main Synchronization Loop

*   **Issue:** The main loop in `Snes::RunFrame()` is state-driven based on the `in_vblank_` flag. This can be fragile and makes it difficult to reason about component state at any given cycle.
*   **Recommendation:** Transition to a unified main loop driven by a single master cycle counter. In this model, each component (CPU, PPU, APU, DMA) is "ticked" forward based on the master clock. This is a more robust and modular architecture that simplifies component synchronization.

---

## Medium Priority: PPU Performance

### Rendering Approach Optimization

*   **Issue:** The PPU currently uses a "pixel-based" renderer (`Ppu::RunLine` calls `HandlePixel` for every pixel). This is highly accurate but can be slow due to high function call overhead and poor cache locality.
*   **Optimization:** Refactor the PPU to use a **scanline-based renderer**. Instead of processing one pixel at a time, process all active layers for an entire horizontal scanline, compose them into a temporary buffer, and then write the completed scanline to the framebuffer. This is a major architectural change but is a standard and highly effective optimization technique in SNES emulation.

**Benefits:**
- Reduced function call overhead
- Better cache locality
- Easier to vectorize/SIMD
- Standard approach in accurate SNES emulators

---

## Low Priority: Code Quality & Refinements

### APU Code Modernization

*   **Issue:** The code in `dsp.cc` and `spc700.cc`, inherited from other projects, is written in a very C-like style, using raw pointers, `memset`, and numerous "magic numbers."
*   **Refactor:** Gradually refactor this code to use modern C++ idioms:
    - Replace raw arrays with `std::array`
    - Use constructors with member initializers instead of `memset`
    - Define `constexpr` variables or `enum class` types for hardware registers and flags
    - Improve type safety, readability, and long-term maintainability

### Audio Subsystem & Buffering

*   **Issue:** The current implementation in `Emulator::Run` queues audio samples directly to the SDL audio device. If the emulator lags for even a few frames, the audio buffer can underrun, causing audible pops and stutters.
*   **Improvement:** Implement a **lock-free ring buffer (or circular buffer)** to act as an intermediary. The emulator thread would continuously write generated samples into this buffer, while the audio device (in its own thread) would continuously read from it. This decouples the emulation speed from the audio hardware, smoothing out performance fluctuations and preventing stutter.

### Debugger & Tooling Optimizations

#### DisassemblyViewer Data Structure
*   **Issue:** `DisassemblyViewer` uses a `std::map` to store instruction traces. For a tool that handles frequent insertions and lookups, this can be suboptimal.
*   **Optimization:** Replace `std::map` with `std::unordered_map` for faster average-case performance.

#### BreakpointManager Lookups
*   **Issue:** The `ShouldBreakOn...` functions perform a linear scan over a `std::vector` of all breakpoints. This is O(n) and could become a minor bottleneck if a very large number of breakpoints are set.
*   **Optimization:** For execution breakpoints, use a `std::unordered_set<uint32_t>` for O(1) average lookup time. This would make breakpoint checking near-instantaneous, regardless of how many are active.

---

## Completed Improvements

### Audio System Fixes (v0.4.0) ✅

#### Problem Statement
The SNES emulator experienced audio glitchiness and skips, particularly during the ALTTP title screen, with audible pops, crackling, and sample skipping during music playback.

#### Root Causes Fixed
1. **Aggressive Sample Dropping**: Audio buffering logic was dropping up to 50% of generated samples, creating discontinuities
2. **Incorrect Resampling**: Duplicate calculations in linear interpolation wasted CPU cycles
3. **Missing Frame Synchronization**: DSP's `NewFrame()` method was never called, causing timing drift
4. **Missing Hermite Interpolation**: Only Linear/Cosine/Cubic were available (Hermite is the industry standard)

#### Solutions Implemented
1. **Never Drop Samples**: Always queue all generated samples unless buffer critically full (>4 frames)
2. **Fixed Resampling Code**: Removed duplicate calculations and added bounds checking
3. **Frame Boundary Synchronization**: Added `dsp.NewFrame()` call before sample generation
4. **Hermite Interpolation**: New interpolation type matching bsnes/Snes9x standard

**Performance Comparison**:
| Interpolation | Quality | Speed | Use Case |
|--------------|---------|-------|----------|
| Linear | ⭐⭐ | ⭐⭐⭐⭐⭐ | Low-end hardware only |
| **Hermite** | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ | **Recommended default** |
| Cosine | ⭐⭐⭐ | ⭐⭐⭐ | Smooth but slow |
| Cubic | ⭐⭐⭐⭐⭐ | ⭐⭐ | Maximum accuracy |

**Result**: Smooth, glitch-free audio matching real SNES hardware quality.

**Testing**: Validated on ALTTP title screen, overworld theme, dungeon ambience, and menu sounds.

**Status**: ✅ Production Ready

---

## Implementation Priority

1. **Critical (v0.4.0):** APU timing fix - Required for music playback
2. **High (v0.5.0):** CPU cycle counting accuracy - Required for game compatibility
3. **High (v0.5.0):** Main synchronization loop refactor - Foundation for accuracy
4. **Medium (v0.6.0):** PPU scanline renderer - Performance optimization
5. **Low (ongoing):** Code quality improvements - Technical debt reduction

---

## Success Metrics

### APU Timing Fix Success
- [ ] Music plays in all tested games
- [ ] Sound effects work correctly
- [ ] No audio glitches or stuttering
- [ ] Handshake completes within expected cycle count

### Overall Emulation Accuracy
- [ ] CPU cycle accuracy within ±1 cycle per instruction
- [ ] APU synchronized within ±1 cycle with CPU
- [ ] PPU timing accurate to scanline level
- [ ] All test ROMs pass

### Performance Targets
- [ ] 60 FPS on modest hardware (2015+ laptops)
- [ ] PPU optimizations provide 20%+ speedup
- [ ] Audio buffer never underruns in normal operation

---

## Related Documentation

- `docs/E4-Emulator-Development-Guide.md` - Implementation details
- `docs/E1-emulator-enhancement-roadmap.md` - Feature roadmap
- `docs/E5-debugging-guide.md` - Debugging techniques

---

**Status:** Active Planning  
**Next Steps:** Begin APU timing refactoring for v0.4.0

