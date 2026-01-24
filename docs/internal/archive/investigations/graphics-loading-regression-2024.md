# Graphics Loading Regression Analysis (2024)

## Overview

This document records the root cause analysis and fix for a critical graphics loading regression where all overworld maps appeared green and graphics sheets appeared "brownish purple" (solid 0xFF fill).

## Symptoms

- Overworld maps rendered as solid green tiles
- Graphics sheets in the Graphics Editor appeared as solid purple/brown color
- All 223 graphics sheets were filled with 0xFF bytes
- Issue appeared after WASM-related changes to `src/app/rom.cc`

## Root Cause

**Two bugs combined to cause complete graphics loading failure:**

### Bug 1: DecompressV2 Size Parameter = 0

The most critical bug was in the `DecompressV2()` calls in `LoadAllGraphicsData()` and `Load2BppGraphics()`:

```cpp
// BROKEN - size parameter is 0, causes immediate empty return
gfx::lc_lz2::DecompressV2(rom.data(), offset, 0, 1, rom.size())

// CORRECT - size must be 0x800 (2048 bytes)
gfx::lc_lz2::DecompressV2(rom.data(), offset, 0x800, 1, rom.size())
```

In `compression.cc`, the `DecompressV2` function has this early-exit check:

```cpp
if (size == 0) {
  return std::vector<uint8_t>();  // Returns empty immediately!
}
```

When `size=0` was passed, every single graphics sheet decompression returned an empty vector, triggering the fallback path that fills the graphics buffer with 0xFF bytes.

### Bug 2: Header Stripping Logic Change (Secondary)

The SMC header detection was also modified from:

```cpp
// ORIGINAL (working) - modulo 1MB
size % kBaseRomSize == kHeaderSize  // kBaseRomSize = 1,048,576

// CHANGED TO (problematic) - modulo 32KB
size % 0x8000 == kHeaderSize
```

The 32KB modulo check could cause false positives on ROMs that happened to have sizes matching the pattern, potentially stripping data that wasn't actually an SMC header.

## Investigation Process

### Initial Hypothesis

1. **Header/Footer Mismatch** - Suspected incorrect ROM alignment causing 512-byte offset in all pointer lookups
2. **Pointer Table Corruption** - Suspected `GetGraphicsAddress` reading garbage due to misalignment
3. **Decompression Failure** - Suspected `DecompressV2` failing silently

### Discovery Method

1. **Agent-based parallel investigation** - Spawned three agents to analyze:
   - ROM alignment (header stripping logic)
   - Graphics pipeline (pointer tables and decompression)
   - WASM integration (data transfer integrity)

2. **Git archaeology** - Compared working commit (`43dfd65b2c`) with broken code:
   ```bash
   git show 43dfd65b2c:src/app/rom.cc | grep "DecompressV2"
   # Output: gfx::lc_lz2::DecompressV2(rom.data(), offset)  # 2 args!
   ```

3. **Function signature analysis** - Found `DecompressV2` signature:
   ```cpp
   DecompressV2(data, offset, size=0x800, mode=1, rom_size=-1)
   ```

4. **Root cause identified** - The broken code passed explicit `0` for the size parameter, overriding the default of `0x800`.

## Fix Applied

### File: `src/app/rom.cc`

1. **Restored header stripping logic** to use 1MB modulo:
   ```cpp
   if (size % kBaseRomSize == kHeaderSize && size >= kHeaderSize &&
       rom_data.size() >= kHeaderSize)
   ```

2. **Fixed DecompressV2 calls** (2 locations):
   - Line ~126 (Load2BppGraphics)
   - Line ~332 (LoadAllGraphicsData)

   Changed from `DecompressV2(..., 0, 1, rom.size())` to `DecompressV2(..., 0x800, 1, rom.size())`

3. **Added diagnostic logging** to help future debugging:
   - ROM alignment verification after header stripping
   - SNES checksum validation logging
   - Graphics pointer table probe (first 5 sheets)

### File: `src/app/gfx/util/compression.h`

Added comprehensive documentation to `DecompressV2()` with explicit warning about size=0.

## Prevention Measures

### Code Comments Added

1. **MaybeStripSmcHeader** - Warning not to change modulo base from 1MB to 32KB
2. **DecompressV2 calls** - Comments explaining the 0x800 size requirement
3. **LoadAllGraphicsData** - Function header documenting the regression

### Documentation Added

1. Updated `compression.h` with full parameter documentation
2. Added `@warning` tags about size=0 behavior
3. Documented sheet categories and compression formats in `rom.cc`

## Key Learnings

1. **Default parameters can be overridden accidentally** - When adding new parameters to a function call, be careful not to override defaults with wrong values.

2. **Early-exit conditions can cause silent failures** - The `if (size == 0) return empty` was valid behavior, but calling code must respect it.

3. **Diagnostic logging is valuable** - The added probe logging for the first 5 graphics sheets helps quickly identify alignment issues.

4. **Git archaeology is essential** - Comparing with known-working commits reveals exactly what changed.

## Related Files

- `src/app/rom.cc` - Main ROM handling and graphics loading
- `src/app/gfx/util/compression.cc` - LC-LZ2 decompression implementation
- `src/app/gfx/util/compression.h` - Decompression function declarations
- `inc/zelda.h` - Version-specific pointer table offsets

## Commits

- **Breaking commit**: Changes to WASM memory safety in `rom.cc`
- **Fix commit**: Restored header stripping, fixed DecompressV2 size parameter
