# E4 - Emulator Development Guide

**Last Updated**: October 6, 2025
**Status**: ✅ **FUNCTIONAL & STABLE**

This document provides a comprehensive overview of the YAZE SNES emulator subsystem, consolidating all development notes, bug fixes, and architectural decisions. It serves as the single source of truth for understanding and developing the emulator.

## 1. Current Status

The YAZE SNES emulator is **fully functional and stable**. All critical timing, synchronization, and instruction execution bugs in the APU/SPC700 and CPU have been resolved. The emulator can successfully boot and run "The Legend of Zelda: A Link to the Past" and other SNES games.

- ✅ **CPU-APU Synchronization**: Cycle-accurate.
- ✅ **SPC700 Emulation**: All instructions and timing are correct.
- ✅ **IPL ROM Protocol**: Handshake and data transfers work as expected.
- ✅ **Memory System**: Stable and consolidated.
- ✅ **UI Integration**: Runs smoothly within the YAZE GUI.

The main remaining work involves UI/UX enhancements and adding advanced debugging features, rather than fixing core emulation bugs.

## 2. How to Use the Emulator

### Method 1: Main Yaze Application (GUI)

1.  **Build YAZE**:
    ```bash
    cmake --build build --target yaze -j8
    ```
2.  **Run YAZE**:
    ```bash
    ./build/bin/yaze.app/Contents/MacOS/yaze
    ```
3.  **Open a ROM**: Use `File > Open ROM` or drag and drop a ROM file onto the window.
4.  **Start Emulation**:
    -   Navigate to `View > Emulator` from the menu.
    -   Click the **Play (▶)** button in the emulator toolbar.

### Method 2: Standalone Emulator (`yaze_emu`)

For headless testing and debugging, use the standalone `yaze_emu` executable.

```bash
# Run for a specific number of frames and then exit
./build/bin/yaze_emu.app/Contents/MacOS/yaze_emu --emu_max_frames=600

# Run with a specific ROM
./build/bin/yaze_emu.app/Contents/MacOS/yaze_emu --emu_rom=path/to/rom.sfc

# Enable APU and CPU debug logging
./build/bin/yaze_emu.app/Contents/MacOS/yaze_emu --emu_debug_apu=true --emu_debug_cpu=true
```

## 3. Architecture

### Memory System

The emulator's memory architecture was consolidated to resolve critical bugs and improve clarity.

-   **`rom_`**: A `std::vector<uint8_t>` that holds the cartridge ROM data. This is the source of truth for the emulator core's read path (`cart_read()`).
-   **`ram_`**: A `std::vector<uint8_t>` for SRAM.
-   **`memory_`**: A 16MB flat address space used *only* by the editor interface for direct memory inspection, not by the emulator core during execution.

This separation fixed a critical bug where the editor and emulator were reading from different, inconsistent memory sources.

### CPU-APU-SPC700 Interaction

The SNES audio subsystem is complex and requires precise timing.

1.  **Initialization**: The SNES CPU boots and initializes the APU.
2.  **IPL ROM Boot**: The SPC700 (the APU's CPU) executes its 64-byte internal IPL ROM.
3.  **Handshake**: The SPC700 writes `$AA` and `$BB` to its output ports. The CPU reads these values and writes back `$CC` to initiate a data transfer.
4.  **Data Transfer**: The CPU uploads the audio driver and data to the SPC700's RAM in blocks. This involves a counter-based acknowledgment protocol.
5.  **Execution**: Once the audio driver is uploaded, the SPC700 jumps to the new code and begins handling audio processing independently.

## 4. The Debugging Journey: A Summary of Critical Fixes

The path to a functional emulator involved fixing a cascade of 9 critical, interconnected bugs.

1.  **APU Cycle Synchronization**: The APU was not advancing its cycles in sync with the master clock, causing an immediate deadlock.
    -   **Fix**: Implemented a delta-based calculation in `Apu::RunCycles()` using `g_last_master_cycles`.

2.  **SPC700 `read_word` Address Truncation**: 16-bit addresses were being truncated to 8 bits, causing the SPC700 to read its reset vector from the wrong location ($00C0 instead of $FFC0).
    -   **Fix**: Changed function parameters in `spc700.h` from `uint8_t` to `uint16_t`.

3.  **Multi-Step Instruction `bstep` Increment**: Instructions like `MOVS` were only executing their first step because the internal step counter (`bstep`) was never incremented.
    -   **Fix**: Added `bstep++` to the first step of all multi-step instructions.

4.  **Step Reset Logic**: The main instruction loop was resetting the step counter unconditionally, breaking multi-step instructions.
    -   **Fix**: Guarded the step reset with `if (bstep == 0)`.

5.  **Opcode Re-Read**: A new opcode was being fetched before the previous multi-step instruction had completed.
    -   **Fix**: Guarded the opcode read with `if (bstep == 0)`.

6.  **Address Re-Calculation**: Address mode functions were being called on each step of a multi-step instruction, advancing the PC incorrectly.
    -   **Fix**: Cached the calculated address in `this->adr` on the first step and reused it.

7.  **CMP Z-Flag Calculation**: `CMP` instructions were checking the 16-bit result for zero, causing incorrect flag calculations for 8-bit operations.
    -   **Fix**: Changed all `CMP` functions to check `(result & 0xFF) == 0`.

8.  **IPL ROM Counter Write**: The IPL ROM was missing a key instruction to echo the transfer counter back to the CPU.
    -   **Fix**: Corrected the IPL ROM byte array in `apu.cc` to include `CB F4` (`MOV ($F4),Y`).

9.  **SDL Event Loop Blocking**: The main application loop used `SDL_WaitEvent`, which blocked rendering unless the user moved the mouse.
    -   **Fix**: Switched to `SDL_PollEvent` to enable continuous rendering at 60 FPS.

## 5. Logging System

A structured logging system (`util/log.h`) was integrated to replace all `printf` statements.

-   **Categories**: `APU`, `SNES`, `CPU`, `Memory`, `SPC700`.
-   **Levels**: `DEBUG`, `INFO`, `WARN`, `ERROR`.
-   **Usage**: `LOG_INFO("APU", "Reset complete");`

### How to Enable

```bash
# Run with debug logging for specific categories
./build/bin/yaze --log-level=DEBUG --log-categories=APU,SNES

# Log to a file
./build/bin/yaze --log-level=DEBUG --log-file=emulator.log
```

## 6. Testing

The emulator subsystem has a growing suite of tests.

-   **Unit Tests**: Located in `test/unit/emu/`, these verify specific components like the APU handshake (`apu_ipl_handshake_test.cc`).
-   **Standalone Emulator**: `yaze_emu` provides a headless way to run the emulator for a fixed number of frames, perfect for regression testing.

### Running Tests

```bash
# Build the test runner
cmake --build build --target yaze_test

# Run all emulator-related tests
./build/bin/yaze_test --gtest_filter="*Apu*":"*Spc700*"
```

## 7. Future Work & Enhancements

While the core is stable, future work can focus on:

-   **UI Enhancements**: Create a dedicated debugger panel in the UI with register views, memory editors, and breakpoints.
-   **`z3ed` Integration**: Expose emulator controls (start, stop, step, save state) to the `z3ed` CLI for automated testing and AI-driven debugging.
-   **Audio Output**: Connect the S-DSP's output buffer to SDL audio to enable sound.
-   **Performance Optimization**: Profile and optimize hot paths, potentially with a JIT compiler for the CPU.
-   **Expanded Test Coverage**: Add comprehensive tests for all CPU and PPU instructions.
