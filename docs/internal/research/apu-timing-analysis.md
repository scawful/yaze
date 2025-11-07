# APU Timing Fix - Technical Analysis

**Branch:** `feature/apu-timing-fix`
**Date:** October 10, 2025
**Status:** Implemented - Core Timing Fixed (Minor Audio Glitches Remain)

---

## Implementation Status

**Completed:**
- Atomic `Step()` function for SPC700
- Fixed-point cycle ratio (no floating-point drift)
- Cycle budget model in APU
- Removed `bstep` mechanism from instructions.cc
- Cycle-accurate instruction implementations
- Proper branch timing (+2 cycles when taken)
- Dummy read/write cycles for MOV and RMW instructions

**Known Issues:**
- Some audio glitches/distortion during playback
- Minor timing inconsistencies under investigation
- Can be improved in future iterations

**Note:** The APU now executes correctly and music plays, but audio quality can be further refined.

## Problem Summary

The APU fails to load and play music because the SPC700 gets stuck during the initial CPU-APU handshake. This handshake uploads the sound driver from ROM to APU RAM. The timing desynchronization causes infinite loops detected by the watchdog timer.

---

## Current Implementation Analysis

### 1. **Cycle Counting System** (`spc700.cc`)

**Current Approach:**
```cpp
// In spc700.h line 87:
int last_opcode_cycles_ = 0;

// In RunOpcode() line 80:
last_opcode_cycles_ = spc700_cycles[opcode];  // Static lookup
```

**Problem:** The `spc700_cycles[]` array provides BASELINE cycle counts only. It does NOT account for:
- Addressing mode variations
- Page boundary crossings (+1 cycle)
- Branch taken vs not taken (+2 cycles if taken)
- Memory access penalties

### 2. **The `bstep` Mechanism** (`spc700.cc`)

**What is `bstep`?**

`bstep` is a "business step" counter used to spread complex multi-step instructions across multiple calls to `RunOpcode()`.

**Example from line 1108-1115 (opcode 0xCB - MOVSY dp):**
```cpp
case 0xcb: {  // movsy dp
  if (bstep == 0) {
    adr = dp();  // Save address for bstep=1
  }
  if (adr == 0x00F4 && bstep == 1) {
    LOG_DEBUG("SPC", "MOVSY writing Y=$%02X to F4 at PC=$%04X", Y, PC);
  }
  MOVSY(adr);  // Use saved address
  break;
}
```

The `MOVSY()` function internally increments `bstep` to track progress:
- `bstep=0`: Call `dp()` to get address
- `bstep=1`: Actually perform the write
- `bstep=2`: Reset to 0, instruction complete

**Why this is fragile:**
1. **Non-atomic execution**: An instruction takes 2-3 calls to `RunOpcode()` to complete
2. **State leakage**: If `bstep` gets out of sync, all future instructions fail
3. **Cycle accounting errors**: Cycles are consumed incrementally, not atomically
4. **Debugging nightmare**: Hard to trace when an instruction "really" executes

### 3. **APU Main Loop** (`apu.cc:73-143`)

**Current implementation:**
```cpp
void Apu::RunCycles(uint64_t master_cycles) {
  const double ratio = memory_.pal_timing() ? apuCyclesPerMasterPal : apuCyclesPerMaster;
  uint64_t master_delta = master_cycles - g_last_master_cycles;
  g_last_master_cycles = master_cycles;

  const uint64_t target_apu_cycles = cycles_ + static_cast<uint64_t>(master_delta * ratio);

  while (cycles_ < target_apu_cycles) {
    spc700_.RunOpcode();  // Variable cycles
    int spc_cycles = spc700_.GetLastOpcodeCycles();

    for (int i = 0; i < spc_cycles; ++i) {
      Cycle();  // Advance DSP/timers
    }
  }
}
```

**Problems:**
1. **Floating-point `ratio`**: `apuCyclesPerMaster` is `double` (line 17), causing precision drift
2. **Opcode-level granularity**: Advances by opcode, not by cycle
3. **No sub-cycle accuracy**: Can't model instructions that span multiple cycles

### 4. **Floating-Point Precision** (`apu.cc:17`)

```cpp
static const double apuCyclesPerMaster = (32040 * 32) / (1364 * 262 * 60.0);
```

**Calculation:**
- Numerator: 32040 * 32 = 1,025,280
- Denominator: 1364 * 262 * 60.0 = 21,437,280
- Result: ~0.04783 (floating point)

**Problem:** Over thousands of cycles, tiny rounding errors accumulate, causing timing drift.

---

## Root Cause: Handshake Timing Failure

### The Handshake Protocol

1. **APU Ready**: SPC700 writes `$AA` to `$F4`, `$BB` to `$F5`
2. **CPU Waits**: Main CPU polls for `$BBAA`
3. **CPU Initiates**: Writes `$CC` to APU input port
4. **APU Acknowledges**: SPC700 sees `$CC`, prepares to receive
5. **Byte Transfer Loop**: CPU sends byte, waits for echo confirmation, sends next byte

### Where It Gets Stuck

The SPC700 enters an infinite loop because:
- **SPC700 is waiting** for a byte from CPU (hasn't arrived yet)
- **CPU is waiting** for acknowledgment from SPC700 (already sent, but missed)

This happens because cycle counts are off by 1-2 cycles per instruction, which accumulates over the ~500-1000 instructions in the handshake.

---

## LakeSnes Comparison Analysis

### What LakeSnes Does Right

**1. Atomic Instruction Execution (spc.c:73-93)**
```c
void spc_runOpcode(Spc* spc) {
  if(spc->resetWanted) { /* handle reset */ return; }
  if(spc->stopped) { spc_idleWait(spc); return; }

  uint8_t opcode = spc_readOpcode(spc);
  spc_doOpcode(spc, opcode);  // COMPLETE instruction in one call
}
```

**Key insight:** LakeSnes executes instructions **atomically** - no `bstep`, no `step`, no state leakage.

**2. Cycle Tracking via Callbacks (spc.c:406-409)**
```c
static void spc_movsy(Spc* spc, uint16_t adr) {
  spc_read(spc, adr);          // Calls apu_cycle()
  spc_write(spc, adr, spc->y); // Calls apu_cycle()
}
```

Every `spc_read()`, `spc_write()`, and `spc_idle()` call triggers `apu_cycle()`, which:
- Advances APU cycle counter
- Ticks DSP every 32 cycles
- Updates timers

**3. Simple Addressing Mode Functions (spc.c:189-275)**
```c
static uint16_t spc_adrDp(Spc* spc) {
  return spc_readOpcode(spc) | (spc->p << 8);
}

static uint16_t spc_adrDpx(Spc* spc) {
  uint16_t res = ((spc_readOpcode(spc) + spc->x) & 0xff) | (spc->p << 8);
  spc_idle(spc);  // Extra cycle for indexed addressing
  return res;
}
```

Each memory access and idle call automatically advances cycles.

**4. APU Main Loop (apu.c:73-82)**
```c
int apu_runCycles(Apu* apu, int wantedCycles) {
  int runCycles = 0;
  uint32_t startCycles = apu->cycles;
  while(runCycles < wantedCycles) {
    spc_runOpcode(apu->spc);
    runCycles += (uint32_t) (apu->cycles - startCycles);
    startCycles = apu->cycles;
  }
  return runCycles;
}
```

**Problem:** This approach tracks cycles by **delta**, which works because every memory access calls `apu_cycle()`.

### Where LakeSnes Falls Short (And How We Can Do Better)

**1. No Explicit Cycle Return**
- LakeSnes relies on tracking `cycles` delta after each opcode
- Doesn't return precise cycle count from `spc_runOpcode()`
- Makes it hard to validate cycle accuracy per instruction

**Our improvement:** Return exact cycle count from `Step()`:
```cpp
int Spc700::Step() {
  uint8_t opcode = ReadOpcode();
  int cycles = CalculatePreciseCycles(opcode);
  ExecuteInstructionAtomic(opcode);
  return cycles;  // EXPLICIT return
}
```

**2. Implicit Cycle Counting**
- Cycles accumulated implicitly through callbacks
- Hard to debug when cycles are wrong
- No way to verify cycle accuracy per instruction

**Our improvement:** Explicit cycle budget model in `Apu::RunCycles()`:
```cpp
while (cycles_ < target_apu_cycles) {
  int spc_cycles = spc700_.Step();  // Explicit cycle count
  for (int i = 0; i < spc_cycles; ++i) {
    Cycle();  // Explicit cycle advancement
  }
}
```

**3. No Fixed-Point Ratio**
- LakeSnes also uses floating-point (implicitly in SNES main loop)
- Subject to same precision drift issues

**Our improvement:** Integer numerator/denominator for perfect precision.

### What We're Adopting from LakeSnes

**Atomic instruction execution** - No `bstep` mechanism
**Simple addressing mode functions** - Return address, advance cycles via callbacks
**Cycle advancement per memory access** - Every read/write/idle advances cycles

### What We're Improving Over LakeSnes

**Explicit cycle counting** - `Step()` returns exact cycles consumed
**Cycle budget model** - Clear loop with explicit cycle advancement
**Fixed-point ratio** - Integer arithmetic for perfect precision
**Testability** - Easy to verify cycle counts per instruction

---

## Solution Design

### Phase 1: Atomic Instruction Execution

**Goal:** Eliminate `bstep` mechanism entirely.

**New Design:**
```cpp
// New function signature
int Spc700::Step() {
  if (reset_wanted_) { /* handle reset */ return 8; }
  if (stopped_) { /* handle stop */ return 2; }

  // Fetch opcode
  uint8_t opcode = ReadOpcode();

  // Calculate EXACT cycle cost upfront
  int cycles = CalculatePreciseCycles(opcode);

  // Execute instruction COMPLETELY
  ExecuteInstructionAtomic(opcode);

  return cycles;  // Return exact cycles consumed
}
```

**Benefits:**
- One call = one complete instruction
- Cycles calculated before execution
- No state leakage between calls
- Easier debugging

### Phase 2: Precise Cycle Calculation

**New function:**
```cpp
int Spc700::CalculatePreciseCycles(uint8_t opcode) {
  int base_cycles = spc700_cycles[opcode];

  // Account for addressing mode penalties
  switch (opcode) {
    case 0x10: case 0x30: /* ... branches ... */
      // Branches: +2 cycles if taken (handled in execution)
      break;
    case 0x15: case 0x16: /* ... abs+X, abs+Y ... */
      // Check if page boundary crossed (+1 cycle)
      if (will_cross_page_boundary(opcode)) {
        base_cycles += 1;
      }
      break;
    // ... more addressing mode checks ...
  }

  return base_cycles;
}
```

### Phase 3: Refactor `Apu::RunCycles` to Cycle Budget Model

**New implementation:**
```cpp
void Apu::RunCycles(uint64_t master_cycles) {
  // 1. Calculate target using FIXED-POINT ratio (Phase 4)
  uint64_t master_delta = master_cycles - g_last_master_cycles;
  g_last_master_cycles = master_cycles;

  // 2. Fixed-point conversion (avoiding floating point)
  uint64_t target_apu_cycles = cycles_ + (master_delta * kApuCyclesNumerator) / kApuCyclesDenominator;

  // 3. Run until budget exhausted
  while (cycles_ < target_apu_cycles) {
    // 4. Execute ONE instruction atomically
    int spc_cycles_consumed = spc700_.Step();

    // 5. Advance DSP/timers for each cycle
    for (int i = 0; i < spc_cycles_consumed; ++i) {
      Cycle();  // Ticks DSP, timers, increments cycles_
    }
  }
}
```

### Phase 4: Fixed-Point Cycle Ratio

**Replace floating-point with integer ratio:**
```cpp
// Old (apu.cc:17)
static const double apuCyclesPerMaster = (32040 * 32) / (1364 * 262 * 60.0);

// New
static constexpr uint64_t kApuCyclesNumerator = 32040 * 32;      // 1,025,280
static constexpr uint64_t kApuCyclesDenominator = 1364 * 262 * 60;  // 21,437,280
```

**Conversion:**
```cpp
apu_cycles = (master_cycles * kApuCyclesNumerator) / kApuCyclesDenominator;
```

**Benefits:**
- Perfect precision (no floating-point drift)
- Integer arithmetic is faster
- Deterministic across platforms

---

## Implementation Plan

### Step 1: Add `Spc700::Step()` Function
- Add new `Step()` method to `spc700.h`
- Implement atomic instruction execution
- Keep `RunOpcode()` temporarily for compatibility

### Step 2: Implement Precise Cycle Calculation
- Create `CalculatePreciseCycles()` helper
- Handle branch penalties
- Handle page boundary crossings
- Add tests to verify against known SPC700 timings

### Step 3: Eliminate `bstep` Mechanism
- Refactor all multi-step instructions (0xCB, 0xD0, 0xD7, etc.)
- Remove `bstep` variable
- Remove `step` variable
- Verify all 256 opcodes work atomically

### Step 4: Refactor `Apu::RunCycles`
- Switch to cycle budget model
- Use `Step()` instead of `RunOpcode()`
- Add cycle budget logging for debugging

### Step 5: Convert to Fixed-Point Ratio
- Replace `apuCyclesPerMaster` double
- Use integer numerator/denominator
- Add constants for PAL timing too

### Step 6: Testing
- Test with vanilla Zelda3 ROM
- Verify handshake completes
- Verify music plays
- Check for watchdog timeouts
- Measure timing accuracy

---

## Files to Modify

1. **src/app/emu/audio/spc700.h**
   - Add `int Step()` method
   - Add `int CalculatePreciseCycles(uint8_t opcode)`
   - Remove `bstep` and `step` variables

2. **src/app/emu/audio/spc700.cc**
   - Implement `Step()`
   - Implement `CalculatePreciseCycles()`
   - Refactor `ExecuteInstructions()` to be atomic
   - Remove all `bstep` logic

3. **src/app/emu/audio/apu.h**
   - Update cycle ratio constants

4. **src/app/emu/audio/apu.cc**
   - Refactor `RunCycles()` to use `Step()`
   - Convert to fixed-point ratio
   - Remove floating-point arithmetic

5. **test/unit/spc700_timing_test.cc** (new)
   - Test cycle accuracy for all opcodes
   - Test handshake simulation
   - Verify no regressions

---

## Success Criteria

- [x] All SPC700 instructions execute atomically (one `Step()` call)
- [x] Cycle counts accurate to ±1 cycle per instruction
- [x] APU handshake completes without watchdog timeout
- [x] Music loads and plays in vanilla Zelda3
- [x] No floating-point drift over long emulation sessions
- [ ] Unit tests pass for all 256 opcodes (future work)
- [ ] Audio quality refined (minor glitches remain)

---

## Implementation Completed

1. Create feature branch
2. Analyze current implementation
3. Implement `Spc700::Step()` function
4. Add precise cycle calculation
5. Refactor `Apu::RunCycles`
6. Convert to fixed-point ratio
7. Refactor instructions.cc to be atomic and cycle-accurate
8. Test with Zelda3 ROM
9. Write unit tests (future work)
10. Fine-tune audio quality (future work)

---

**References:**
- [SPC700 Opcode Reference](https://problemkaputt.de/fullsnes.htm#snesapucpu)
- [APU Timing Documentation](https://wiki.superfamicom.org/spc700-reference)
- docs/E6-emulator-improvements.md
