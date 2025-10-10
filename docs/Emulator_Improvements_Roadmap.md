# Emulator Core Improvements & Optimization Roadmap

## 1. Introduction

This document outlines potential long-term improvements, refactors, and optimizations for the `yaze` emulator core. These suggestions aim to enhance accuracy, performance, and code maintainability.

For a detailed analysis of the critical, high-priority bug affecting audio, please first refer to the separate document:

- **[APU Timing and Handshake Bug Analysis & Refactoring Plan](./APU_Timing_Fix_Plan.md)**

The items below are presented in order of descending priority, from critical accuracy fixes to quality-of-life code improvements.

## 2. Core Architecture & Timing Model (High Priority)

The most significant improvements relate to the fundamental emulation loop and cycle timing, which are the root cause of the current audio bug and affect overall system stability.

*   **CPU Cycle Counting:**
    *   **Issue:** The main CPU loop in `Snes::RunCycle()` advances the master cycle counter by a fixed amount (`+= 2`). Real 65816 instructions have variable cycle counts. The current workaround of scattering `callbacks_.idle()` calls is error-prone and difficult to maintain.
    *   **Recommendation:** Refactor `Cpu::ExecuteInstruction` to calculate and return the *precise* cycle cost of each instruction, including penalties for addressing modes and memory access speeds. The main `Snes` loop should then consume this exact value, centralizing timing logic and dramatically improving accuracy.

*   **Main Synchronization Loop:**
    *   **Issue:** The main loop in `Snes::RunFrame()` is state-driven based on the `in_vblank_` flag. This can be fragile and makes it difficult to reason about the state of all components at any given cycle.
    *   **Recommendation:** Transition to a unified main loop that is driven by a single master cycle counter. In this model, each component (CPU, PPU, APU, DMA) is "ticked" forward based on the master clock. This is a more robust and modular architecture that simplifies component synchronization.

## 3. PPU (Video Rendering) Performance

The Picture Processing Unit (PPU) is often a performance bottleneck. The following change could provide a significant speed boost.

*   **Rendering Approach:**
    *   **Issue:** The PPU currently uses a "pixel-based" renderer (`Ppu::RunLine` calls `HandlePixel` for every pixel). This is highly accurate but can be slow due to high function call overhead and poor cache locality.
    *   **Optimization:** Refactor the PPU to use a **scanline-based renderer**. Instead of processing one pixel at a time, this approach processes all active layers for an entire horizontal scanline, composes them into a temporary buffer, and then writes the completed scanline to the framebuffer. This is a major architectural change but is a standard and highly effective optimization technique in SNES emulation.

## 4. APU (Audio) Code Quality & Refinements

Beyond the critical timing fixes, the APU core would benefit from modernization.

*   **Code Style:**
    *   **Issue:** The code in `dsp.cc` and `spc700.cc`, inherited from other projects, is written in a very C-like style, using raw pointers, `memset`, and numerous "magic numbers."
    *   **Refactor:** Gradually refactor this code to use modern C++ idioms. Replace raw arrays with `std::array`, use constructors with member initializers instead of `memset`, and define `constexpr` variables or `enum class` types for hardware registers and flags. This will improve type safety, readability, and long-term maintainability.

## 5. Audio Subsystem & Buffering

To ensure smooth, stutter-free audio, the interface between the emulator and the host audio API can be improved.

*   **Audio Buffering Strategy:**
    *   **Issue:** The current implementation in `Emulator::Run` queues audio samples directly to the SDL audio device. If the emulator lags for even a few frames, the audio buffer can underrun, causing audible pops and stutters.
    *   **Improvement:** Implement a **lock-free ring buffer (or circular buffer)** to act as an intermediary. The emulator thread would continuously write generated samples into this buffer, while the audio device (in its own thread) would continuously read from it. This decouples the emulation speed from the audio hardware, smoothing out performance fluctuations and preventing stutter.

## 6. Debugger & Tooling Optimizations (Lower Priority)

These are minor optimizations that would improve the performance of the debugging tools, especially under heavy use.

*   **`DisassemblyViewer` Data Structure:**
    *   **Issue:** `DisassemblyViewer` uses a `std::map` to store instruction traces. For a tool that handles frequent insertions and lookups, this can be suboptimal.
    *   **Optimization:** Replace `std::map` with `std::unordered_map` for faster average-case performance.

*   **`BreakpointManager` Lookups:**
    *   **Issue:** The `ShouldBreakOn...` functions perform a linear scan over a `std::vector` of all breakpoints. This is O(n) and could become a minor bottleneck if a very large number of breakpoints are set.
    *   **Optimization:** For execution breakpoints, use a `std::unordered_set<uint32_t>` for O(1) average lookup time. This would make breakpoint checking near-instantaneous, regardless of how many are active.
