# Debugging WASM Memory Access Errors

## Quick Methods to Find Out-of-Bounds Accesses

### Method 1: Enable Emscripten SAFE_HEAP (Easiest)

Add `-s SAFE_HEAP=1` to your Emscripten flags. This adds bounds checking to all memory accesses and will give you a precise error location.

**In CMakePresets.json**, add to `CMAKE_CXX_FLAGS`:
```json
"CMAKE_CXX_FLAGS": "... -s SAFE_HEAP=1 -s ASSERTIONS=2"
```

**Pros**: Catches all out-of-bounds accesses automatically
**Cons**: Slower execution (debugging only)

### Method 2: Map WASM Function Number to Source

The error shows `wasm-function[3704]`. You can map this to source:

1. Build with source maps: Add `-g4 -s SOURCE_MAP_BASE='http://localhost:8080/'` to linker flags
2. Use `wasm-objdump` to list functions:
   ```bash
   wasm-objdump -x build-wasm/bin/yaze.wasm | grep -A 5 "func\[3704\]"
   ```
3. Or use browser DevTools: The stack trace should show function names if source maps are enabled

### Method 3: Add Logging Wrapper (Recommended for Production)

Create a ROM access wrapper that logs all accesses:

```cpp
// In rom.h or a new debug_rom.h
#ifdef __EMSCRIPTEN__
class DebugRomAccess {
public:
  static bool CheckAccess(const uint8_t* data, size_t offset, size_t size, size_t rom_size, const char* func_name) {
    if (offset + size > rom_size) {
      emscripten_log(EM_LOG_ERROR, 
        "OUT OF BOUNDS: %s accessing offset %zu + %zu (ROM size: %zu)",
        func_name, offset, size, rom_size);
      return false;
    }
    return true;
  }
};
#endif
```

Then wrap ROM accesses:
```cpp
#ifdef __EMSCRIPTEN__
if (!DebugRomAccess::CheckAccess(rom.data(), offset, size, rom.size(), __FUNCTION__)) {
  return absl::OutOfRangeError("Bounds check failed");
}
#endif
```

### Method 4: Use Browser DevTools Memory Profiler

1. Open Chrome DevTools → Memory tab
2. Take heap snapshot before loading ROM
3. Load ROM
4. Take another snapshot
5. Compare to see what memory was allocated
6. The error location in the stack trace should point to the problematic function

### Method 5: Add Function Entry Logging

Add logging at the start of functions that access ROM data:

```cpp
#ifdef __EMSCRIPTEN__
#define LOG_ROM_ACCESS(func_name, offset, size) \
  emscripten_log(EM_LOG_CONSOLE, "[ROM] %s: offset=%zu, size=%zu", func_name, offset, size)
#else
#define LOG_ROM_ACCESS(func_name, offset, size)
#endif
```

Then add to functions like `DecompressV2`, `GetGraphicsAddress`, etc.

## Quick Fix: Enable SAFE_HEAP Now

The fastest way to get a precise error location is to temporarily enable SAFE_HEAP:

1. Edit `CMakePresets.json` → `wasm-release` preset
2. Add to `CMAKE_CXX_FLAGS`: `-s SAFE_HEAP=1 -s ASSERTIONS=2`
3. Rebuild: `cmake --build build-wasm`
4. The error will now show the exact line number and function

**Note**: Remove SAFE_HEAP for production builds (it's slow), but it's perfect for debugging.

