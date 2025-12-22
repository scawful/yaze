# Gemini 3 Pro Prompt: Overworld Regression Fix and Improvements

## Context

You are working on **yaze** (Yet Another Zelda3 Editor), a C++23 ROM editor for The Legend of Zelda: A Link to the Past. A regression has been introduced that breaks loading of custom ROMs like "Oracle of Secrets" ROM hack.

## Primary Bug: ASM Version Check Inconsistency

### Root Cause Analysis

The recent refactoring introduced `OverworldVersionHelper` for centralized ROM version detection, but **not all code paths were updated to use it consistently**. Specifically:

**In `src/zelda3/overworld/overworld.cc:71`:**
```cpp
uint8_t asm_version = (*rom_)[OverworldCustomASMHasBeenApplied];
if (asm_version >= 3) {  // BUG: 0xFF (255) is >= 3!
  AssignMapSizes(overworld_maps_);
} else {
  FetchLargeMaps();  // Vanilla ROMs need this path
}
```

**The bug**: `asm_version >= 3` evaluates to `true` for vanilla ROMs where `asm_version == 0xFF` (255), causing vanilla ROMs and custom ROMs without ZScream ASM patches to incorrectly call `AssignMapSizes()` instead of `FetchLargeMaps()`.

**Other places correctly check**:
```cpp
if (asm_version >= 3 && asm_version != 0xFF) { ... }  // Correct
```

### Inconsistent Locations Found

Search results showing mixed patterns:
- `overworld.cc:71` - **BUG**: `if (asm_version >= 3)` - missing `&& asm_version != 0xFF`
- `overworld.cc:449` - **BUG**: `if (expanded_flag != 0x04 || asm_version >= 3)` - missing check
- `overworld.cc:506` - **BUG**: similar pattern
- `overworld.cc:281` - **CORRECT**: `(asm_version < 3 || asm_version == 0xFF)`
- `overworld.cc:373` - **CORRECT**: `if (asm_version >= 3 && asm_version != 0xFF)`
- Other files also have inconsistencies

## Your Task

### Phase 1: Fix the Regression (CRITICAL)

1. **Update all ASM version checks** in overworld code to either:
   - Use `OverworldVersionHelper::GetVersion()` and semantic checks like `SupportsAreaEnum()`, OR
   - Consistently use `asm_version >= 3 && asm_version != 0xFF` pattern

2. **Key files to fix**:
   - `src/zelda3/overworld/overworld.cc`
   - `src/zelda3/overworld/overworld_map.cc`
   - `src/zelda3/overworld/overworld_item.cc`

3. **Priority fixes in `overworld.cc`**:
   - Line 71: Change to `if (asm_version >= 3 && asm_version != 0xFF)`
   - Line 449: Add `&& asm_version != 0xFF` check
   - Line 506: Add `&& asm_version != 0xFF` check
   - Review all other locations from the grep results

### Phase 2: Standardize Version Checking (Recommended)

Replace all raw `asm_version` checks with `OverworldVersionHelper`:

**Instead of:**
```cpp
uint8_t asm_version = (*rom_)[OverworldCustomASMHasBeenApplied];
if (asm_version >= 3 && asm_version != 0xFF) {
```

**Use:**
```cpp
auto version = OverworldVersionHelper::GetVersion(*rom_);
if (OverworldVersionHelper::SupportsAreaEnum(version)) {
```

This centralizes the logic and prevents future inconsistencies.

### Phase 3: Add Unit Tests

Create tests in `test/unit/zelda3/overworld_test.cc` to verify:
1. Vanilla ROM (0xFF) uses `FetchLargeMaps()` path
2. ZScream v3 ROM (0x03) uses `AssignMapSizes()` path
3. Custom ROMs with other values behave correctly

## Key Files Reference

```
src/zelda3/overworld/
├── overworld.cc              # Main loading logic
├── overworld.h
├── overworld_map.cc          # Individual map handling
├── overworld_map.h
├── overworld_item.cc         # Item loading
├── overworld_item.h
├── overworld_entrance.h      # Entrance/Exit data
├── overworld_exit.cc
├── overworld_exit.h
├── overworld_version_helper.h # Version detection helper
```

## OverworldVersionHelper API

```cpp
enum class OverworldVersion {
  kVanilla = 0,     // 0xFF in ROM - no ZScream ASM
  kZSCustomV1 = 1,
  kZSCustomV2 = 2,
  kZSCustomV3 = 3   // Area enum system
};

class OverworldVersionHelper {
  static OverworldVersion GetVersion(const Rom& rom);
  static bool SupportsAreaEnum(OverworldVersion v);     // v3 only
  static bool SupportsExpandedSpace(OverworldVersion v); // v1+
  static bool SupportsCustomBGColors(OverworldVersion v); // v2+
  // ...
};
```

## Commits That Introduced the Regression

1. `1e39df88a3` - "refactor: enhance overworld entity properties and version handling"
   - Introduced `OverworldVersionHelper`
   - 15 files changed, +546 -282 lines

2. `5894809aaf` - "refactor: improve overworld map version handling and code organization"
   - Updated `OverworldMap` to use version helper
   - 4 files changed, +145 -115 lines

## Build & Test Commands

```bash
# Configure
cmake --preset mac-dbg

# Build
cmake --build build --target yaze -j8

# Run unit tests
ctest --test-dir build -L stable -R overworld

# Run the app to test loading
./build/bin/Debug/yaze.app/Contents/MacOS/yaze --rom_file=/path/to/oracle_of_secrets.sfc
```

## Success Criteria

1. Oracle of Secrets ROM loads correctly in the Overworld Editor
2. Vanilla ALTTP ROMs continue to work
3. ZScream v3 patched ROMs continue to work
4. All existing unit tests pass
5. No new compiler warnings

## Additional Context

- The editor supports multiple ROM types: Vanilla, ZScream v1/v2/v3 patched ROMs, and custom hacks
- `OverworldCustomASMHasBeenApplied` address (0x130000) stores the version byte
- 0xFF = vanilla (no patches), 1-2 = legacy ZScream, 3 = current ZScream
- "Oracle of Secrets" is a popular ROM hack that may use 0xFF or a custom value

## Code Quality Requirements

- Follow Google C++ Style Guide
- Use `absl::Status` for error handling
- Run clang-format before committing
- Update CLAUDE.md coordination board when done
