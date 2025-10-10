# APU Timing and Handshake Bug Analysis & Refactoring Plan

## 1. Problem Statement

The emulator's Audio Processing Unit (APU) currently fails to load and play music. Analysis of the execution flow indicates that the SPC700 processor gets "stuck" during the initial handshake sequence with the main CPU. This handshake is responsible for uploading the sound driver from the ROM to the APU's RAM. The failure of this timing-sensitive process prevents the sound driver from ever running, thus no music is played.

This document outlines the cause of this timing failure and proposes a refactoring plan to achieve cycle-accurate emulation required for a stable APU.

## 2. Analysis of the CPU-APU Handshake

The process of starting the APU and loading a sound bank is not a simple data transfer; it is a tightly synchronized conversation between the main CPU (65816) and the APU's CPU (SPC700).

### 2.1. The Conversation Protocol

The main CPU, executing the `LoadSongBank` routine (`#_008888` in `bank_00.asm`), and the SPC700, executing its internal IPL ROM (`bootRom` in `apu.cc`), follow a strict protocol:

1.  **APU Ready**: The SPC700 boots, initializes itself, and signals it is ready by writing `$AA` to its port `$F4` and `$BB` to port `$F5`.
2.  **CPU Waits**: The main CPU waits in a tight loop, reading the combined 16-bit value from its I/O ports until it sees `$BBAA`. This confirms the APU is alive.
3.  **CPU Initiates**: The CPU writes the command code `$CC` to the APU's input port.
4.  **APU Acknowledges**: The SPC700, which was waiting for this command, sees `$CC` and prepares to receive a block of data.
5.  **Synchronized Byte Transfer**: The CPU and APU then enter a lock-step loop to transfer the sound driver byte-by-byte. For each byte:
    *   The CPU sends the data.
    *   The CPU waits for the APU to read the data and echo back a confirmation value.
    *   Only upon receiving the confirmation does the CPU send the next byte.

### 2.2. Point of Failure

The "stuck" behavior occurs because one side of this conversation fails to meet the other's expectation. Due to timing desynchronization, either:
*   The SPC700 is waiting for a byte that the CPU has not yet sent (or sent too early).
*   The CPU is waiting for an acknowledgment that the SPC700 has already sent (or has not yet sent).

The result is an infinite loop on the SPC700, which is what the watchdog timer in `Apu::RunCycles` detects.

## 3. Root Cause: Cycle Inaccuracy in SPC700 Emulation

The handshake's reliance on precise timing exposes inaccuracies in the current SPC700 emulation model. The core issue is that the emulator does not calculate the *exact* number of cycles each SPC700 instruction consumes.

### 3.1. Incomplete Opcode Timing

The emulator uses a static lookup table (`spc700_cycles.h`) for instruction cycle counts. This provides a *base* value but fails to account for critical variations:
*   **Addressing Modes**: Different addressing modes have different cycle costs.
*   **Page Boundaries**: Memory accesses that cross a 256-byte page boundary take an extra cycle.
*   **Branching**: Conditional branches take a different number of cycles depending on whether the branch is taken or not.

While some of this is handled (e.g., `DoBranch`), it is not applied universally, leading to small, cumulative errors.

### 3.2. Fragile Multi-Step Execution Model

The `step`/`bstep` mechanism in `Spc700::RunOpcode` is a significant source of fragility. It attempts to model complex instructions by spreading their execution across multiple calls. This means the full cycle cost of an instruction is not consumed atomically. An off-by-one error in any step, or an incorrect transition, can corrupt the timing of the entire APU, causing the handshake to fail.

### 3.3. Floating-Point Precision

The use of a `double` for the `apuCyclesPerMaster` ratio can introduce minute floating-point precision errors. Over the thousands of cycles required for the handshake, these small errors can accumulate and contribute to the overall timing drift between the CPU and APU.

## 4. Proposed Refactoring Plan

To resolve this, the APU emulation must be refactored from its current instruction-based timing model to a more robust **cycle-accurate model**. This is the standard approach for emulating timing-sensitive hardware.

### Step 1: Implement Cycle-Accurate Instruction Execution

The `Spc700::RunOpcode` function must be refactored to calculate and consume the *exact* cycle count for each instruction *before* execution.

*   **Calculate Exact Cost**: Before running an opcode, determine its precise cycle cost by analyzing its opcode, addressing mode, and potential page-boundary penalties. The `spc700_cycles.h` table should be used as a base, with additional cycles added as needed.
*   **Atomic Execution**: The `bstep` mechanism should be removed. An instruction, no matter how complex, should be fully executed within a single call to a new `Spc700::Step()` function. This function will be responsible for consuming the exact number of cycles it calculated.

### Step 2: Centralize the APU Execution Loop

The main `Apu::RunCycles` loop should be the sole driver of APU time.

*   **Cycle Budget**: At the start of a frame, `Apu::RunCycles` will calculate the total "budget" of APU cycles it needs to execute.
*   **Cycle-by-Cycle Stepping**: It will then loop, calling `Spc700::Step()` and `Dsp::Cycle()` and decrementing its cycle budget, until the budget is exhausted. This ensures the SPC700 and DSP are always perfectly synchronized.

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

### Step 3: Use Integer-Based Cycle Ratios

To eliminate floating-point errors, the `apuCyclesPerMaster` ratio should be converted to a fixed-point integer ratio. This provides perfect, drift-free conversion between main CPU and APU cycles over long periods.

## 5. Conclusion

The APU handshake failure is a classic and challenging emulation problem that stems directly from timing inaccuracies. By refactoring the SPC700 execution loop to be cycle-accurate, we can ensure the emulated CPU and APU remain perfectly synchronized, allowing the handshake to complete successfully and enabling music playback.
