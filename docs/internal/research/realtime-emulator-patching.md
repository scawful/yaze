# Real-Time Emulator Integration - Research Report
**Investigator:** CLAUDE_AIINF  
**Date:** 2025-11-20  
**Status:** IN PROGRESS  
**Coolness Factor:** 9/10 ⚡

## The Vision
Edit your ROM in yaze's editors → Changes appear INSTANTLY in the running emulator → No reload, no restart. Just MAGIC.

## Current Architecture

### How It Works Now:
1. User loads ROM → `Rom::LoadFromFile()` reads file
2. Emulator gets ROM data → `Emulator::Initialize(vector<uint8_t> rom_data)`  
3. SNES memory initializes → `MemoryImpl::Initialize()` **copies ROM into internal buffer** (memory.cc:24)
4. Emulation runs from that internal copy

### Key Discovery 🔑:
```cpp
// memory.cc:24
std::copy(rom_data.begin(), rom_data.begin() + copy_size, rom_.begin());
```

**The ROM is in a MUTABLE internal buffer (`rom_`)** - we can patch it!

## Technical Approach

### Phase 1: Direct Memory Patching (MVP)
**Concept:** When editor changes ROM, also patch emulator's ROM buffer

```cpp
// Proposed API
class Emulator {
  // NEW: Hot-patch ROM without reset
  void PatchROMByte(uint32_t offset, uint8_t value) {
    if (offset < memory_.rom_.size()) {
      memory_.rom_[offset] = value;
    }
  }
  
  void PatchROMRegion(uint32_t offset, const std::vector<uint8_t>& data) {
    // Batch update for graphics/maps
    std::copy(data.begin(), data.end(), memory_.rom_.begin() + offset);
  }
};
```

**Integration Points:**
- `OverworldEditor::SaveMap()` → call `emulator->PatchROMRegion()`
- `GraphicsEditor::UpdateTile()` → call `emulator->PatchROMByte()`
- `DungeonEditor::PlaceObject()` → call `emulator->PatchROMRegion()`

**Challenges:**
- Need public accessor to `MemoryImpl::rom_`  
- Must handle bank mapping (SNES uses bank:address not flat offsets)
- Some changes need PPU/APU refresh (graphics caching)

### Phase 2: Smart Invalidation
**Problem:** Emulator might cache decoded graphics/maps  
**Solution:** Invalidation callbacks

```cpp
// When ROM region changes, notify relevant subsystems
void NotifyROMChange(uint32_t offset, size_t length) {
  // If graphics data changed, invalidate PPU caches
  if (IsGraphicsRegion(offset)) {
    ppu_.InvalidateTileCache();
  }
  // If map data changed, refresh map decoder
  if (IsMapRegion(offset)) {
    // Map data is usually decoded on-demand, might just work!
  }
}
```

### Phase 3: Visual Feedback Loop
**The Dream:** See your changes frame-by-frame as you edit

```cpp
// Editor → Emulator bridge
class LiveEditBridge {
  void OnOverworldTileChanged(uint16_t x, uint16_t y, uint16_t tile) {
    // Calculate ROM offset for this tile
    uint32_t offset = CalcOverworldTileOffset(x, y, current_map_);
    // Write to ROM
    rom_->WriteByte(offset, tile & 0xFF);
    rom_->WriteByte(offset + 1, tile >> 8);
    // Patch emulator
    emulator_->PatchROMRegion(offset, {tile & 0xFF, tile >> 8});
  }
};
```

## Required Dependencies
- ✅ Already have: Emulator with accessible memory system
- ✅ Already have: Rom class with transaction API
- ⚠️ Need: Public accessor for `MemoryImpl::rom_`
- ⚠️ Need: Bank→offset mapping utility
- ⚠️ Nice to have: PPU/APU cache invalidation hooks

## Estimated Complexity
**MVP (Phase 1):** 🟢 EASY - 2-3 hours
- Add `mutable_rom_buffer()` accessor to MemoryImpl
- Add `PatchROM()` methods to Emulator
- Wire up one editor (e.g., Overworld) as proof-of-concept

**Full Integration (Phase 2+3):** 🟡 MEDIUM - 1-2 days
- All editors integrated
- Cache invalidation system
- Performance tuning (batch updates)
- Edge case handling (mid-frame patches, etc.)

## Performance Concerns
- **Memory writes:** Negligible (just array writes)
- **Cache invalidation:** Could cause frame drops if too aggressive
- **Mitigation:** Batch updates, only invalidate what changed

## Proof of Concept Plan
1. ✅ Research current architecture (DONE)
2. Add `Emulator::PatchROMByte()` API
3. Test with OverworldEditor changing one tile
4. Verify emulator shows the change without reset
5. Benchmark frame time impact
6. If successful → expand to all editors!

## Why This Is AWESOME
- **Instant feedback** = faster iteration for ROM hackers
- **No context switching** = Stay in flow state
- **Perfect for AI agents** = They can see results of their changes immediately
- **Unique feature** = No other ROM editor does this!

## Next Steps
- [ ] Check MemoryImpl implementation details
- [ ] Design bank→offset mapping function  
- [ ] Create minimal POC branch
- [ ] Test with simple overworld tile change

**Status:** Feasibility confirmed! Moving to implementation planning phase.

---
*Research by CLAUDE_AIINF - Making the impossible, probable since 2025* 😎
