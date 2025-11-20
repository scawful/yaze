# Real-Time State Synchronization Protocol (SSP)

**Status**: DRAFT  
**Owner**: GEMINI_3_GENIUS  
**Context**: [Real-Time Emulator Research](../agents/REALTIME_EMULATOR_RESEARCH.md)

## The Problem: Asynchronous Chaos
The Editor runs on the UI thread (or separate process). The Emulator runs on a tight loop (PPU/CPU/APU).
Directly writing to `memory_.rom_[offset]` creates race conditions:
1.  **Instruction Tear**: CPU executes an opcode while we overwrite its arguments.
2.  **Visual Artifacts**: PPU renders a tile while we patch its bitplanes.
3.  **State Desync**: Editor thinks ROM is `0xAB`, Emulator has `0xCD`.

## The Solution: Atomic Patch Transactions

### 1. The `LivePatch` Struct
We define a strict contract for all mutations.

```cpp
struct LivePatch {
    enum Type { CODE, GRAPHICS, DATA, PALETTE };
    uint32_t address;      // SNES LoROM/HiROM address (decoded)
    std::vector<uint8_t> data;
    uint64_t frame_id;     // Target frame (0 = immediate/next safe)
    bool force_vblank;     // If true, MUST wait for VBlank
};
```

### 2. The Ring Buffer (Safe Queue)
A lock-free (or spin-locked) ring buffer connects the Editor Producer to the Emulator Consumer.

*   **Editor**: Pushes `LivePatch` to `PatchQueue`.
*   **Emulator**: Checks `PatchQueue` **ONLY** at the start of VBlank (Line 225).

### 3. The Consumption Loop
```cpp
// In Emulator::RunFrame()
void OnVBlankStart() {
    while (!patch_queue_.empty()) {
        LivePatch patch = patch_queue_.pop();
        
        // 1. Apply Patch
        memory_.WriteBlock(patch.address, patch.data);
        
        // 2. Invalidate Caches (The "Smart" Part)
        if (patch.type == LivePatch::GRAPHICS) {
            ppu_.InvalidateTile(patch.address);
        }
        
        // 3. Telemetry
        state_monitor_.LogPatchApplied(patch);
    }
}
```

## Visual Safety Window
We will expose `IsVBlank()` to the UI.
- **Green Light**: VBlank active. Safe to patch.
- **Red Light**: Scanning active. Patch will be queued.

## Testing Strategy
Generative tests must not just assert `ReadByte() == val`. They must:
1.  Send Patch.
2.  Step Emulator > 1 Frame.
3.  Assert `ReadByte() == val`.
4.  Assert `EmulatorState != CRASHED`.

## Open Tasks
- [ ] Implement `PatchQueue` ring buffer and plumbing in `Emulator`
- [ ] Add LoROM/HiROM helper to convert editor offsets to emulator addresses
- [ ] UI indicator for safe patch windows (status bar + tooltip)
- [ ] Telemetry logging (frame IDs, patch history) for SSP diagnostics

---
*Architected by GEMINI_3_GENIUS*
