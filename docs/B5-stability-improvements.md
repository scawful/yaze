# Stability, Testability & Release Workflow Improvements

This document outlines recent improvements to YAZE's stability, testability, and release workflows, along with recommendations for future optimizations.

## Recent Improvements (v0.3.2)

### Windows Platform Stability

#### Stack Size Optimization
**Problem:** Windows default stack size (1MB) was insufficient for `EditorManager::LoadAssets()` which loads 223 graphics sheets and initializes multiple editors.

**Solution:** Increased stack size to 8MB to match Unix-like systems.

**Implementation:**
```cmake
# src/app/app.cmake
if(MSVC)
  target_link_options(yaze PRIVATE /STACK:8388608)  # 8MB stack
elseif(MINGW)
  target_link_options(yaze PRIVATE -Wl,--stack,8388608)
endif()
```

**Impact:**
- ✅ Eliminated stack overflow crashes during ROM loading
- ✅ Consistent behavior across all platforms
- ✅ Handles deep call chains from editor initialization

#### Development Utility Isolation
**Problem:** Development-only utilities (`extract_vanilla_values`, `rom_patch_utility`) were being built in CI/release workflows, causing linker errors.

**Solution:** Isolated development utilities from CI/release builds using environment detection.

**Implementation:**
```cmake
# test/CMakeLists.txt
if(NOT YAZE_MINIMAL_BUILD AND YAZE_ENABLE_ROM_TESTS AND NOT DEFINED ENV{GITHUB_ACTIONS})
  add_executable(extract_vanilla_values ...)
  target_link_libraries(extract_vanilla_values yaze_core ...)
endif()
```

**Impact:**
- ✅ Clean release builds without development artifacts
- ✅ Proper library linkage for development tools
- ✅ Faster CI/CD pipelines

### Graphics System Stability

#### Segmentation Fault Resolution
**Problem:** Tile cache system using `std::move()` operations invalidated Bitmap surface pointers, causing crashes.

**Solution:** Disabled move semantics in tile cache and implemented direct SDL texture updates.

**Technical Details:**
- Root cause: `std::move()` operations on Bitmap objects invalidated internal SDL_Surface pointers
- Fix: Disabled tile cache and use direct texture updates
- Optimization: Maintained surface/texture pooling while ensuring pointer stability

**Impact:**
- ✅ Eliminated all segmentation faults in tile16 editor
- ✅ Stable graphics rendering pipeline
- ✅ Reliable texture management

#### Comprehensive Bounds Checking
**Problem:** Out-of-bounds access to tile and palette data caused crashes and corruption.

**Solution:** Added extensive bounds checking throughout graphics pipeline.

**Areas Covered:**
- Tile16 canvas access
- Palette group selection
- Graphics sheet indexing
- Texture coordinate calculations

**Impact:**
- ✅ Prevents crashes from invalid data
- ✅ Better error reporting
- ✅ Safer memory access patterns

### Build System Improvements

#### Modern Windows Workflow
**Previous Approach:** Generate Visual Studio solution files using Python script.

**New Approach:** Use Visual Studio's native CMake support.

**Benefits:**
- No project generation step required
- CMakeLists.txt is the single source of truth
- Changes reflect immediately without regeneration
- Better IntelliSense and debugging experience
- Cross-platform consistency

**Migration:**
```powershell
# Old workflow
python scripts/generate-vs-projects.py
# Open YAZE.sln

# New workflow  
# File → Open → Folder → yaze
# Visual Studio detects CMakeLists.txt automatically
```

#### Enhanced CI/CD Reliability
**Improvements:**
- Automatic vcpkg fallback mechanisms
- Proper development utility isolation
- Consistent test execution across platforms
- Better error reporting and debugging

## Recommended Optimizations

### High Priority

#### 1. Lazy Graphics Loading
**Current:** All 223 graphics sheets loaded on ROM open.

**Proposed:** Load graphics sheets on-demand when editors access them.

**Benefits:**
- Faster ROM loading (3-5x improvement expected)
- Reduced memory footprint
- Better startup performance
- Eliminates stack pressure

**Implementation Strategy:**
```cpp
class LazyGraphicsLoader {
  std::array<std::optional<gfx::Bitmap>, kNumGfxSheets> sheets_;
  
  gfx::Bitmap& GetSheet(int index) {
    if (!sheets_[index]) {
      sheets_[index] = LoadGraphicsSheet(rom_, index);
    }
    return *sheets_[index];
  }
};
```

**Impact:**
- 🔵 Complexity: Medium
- 🟢 Performance Gain: High  
- 🟢 Risk: Low (backward compatible)

#### 2. Heap-Based Large Allocations
**Current:** Large arrays and vectors allocated on stack during asset loading.

**Proposed:** Move large data structures to heap allocation.

**Benefits:**
- Reduces stack pressure
- More flexible memory management
- Better for Windows default stack constraints
- Safer for deep call chains

**Areas to Convert:**
- Graphics sheet arrays in LoadAllGraphicsData()
- Editor initialization data structures
- Temporary buffers in compression/decompression

**Impact:**
- 🟢 Complexity: Low
- 🟢 Performance Gain: Medium
- 🟢 Risk: Very Low

#### 3. Streaming ROM Assets
**Current:** Load entire ROM and all assets into memory.

**Proposed:** Stream assets from ROM file as needed.

**Benefits:**
- Minimal memory footprint
- Instant ROM opening
- Better for large ROM hacks
- More scalable architecture

**Challenges:**
- Requires architecture refactoring
- Need efficient caching strategy
- Must maintain edit performance

**Impact:**
- 🔴 Complexity: High
- 🟢 Performance Gain: Very High
- 🟡 Risk: Medium (requires testing)

### Medium Priority

#### 4. Enhanced Test Isolation
**Current:** Some tests share global state through Arena singleton.

**Proposed:** Better test isolation with mock singletons.

**Benefits:**
- More reliable test execution
- Parallel test execution possible
- Better test independence
- Easier debugging

**Implementation:**
```cpp
class TestArena : public Arena {
  // Test-specific implementation
};

TEST_F(GraphicsTest, TestCase) {
  TestArena arena;
  Arena::SetInstance(&arena);  // Override singleton
  // Run test
}
```

**Impact:**
- 🟡 Complexity: Medium
- 🟢 Performance Gain: Medium (parallel tests)
- 🟢 Risk: Low

#### 5. Dependency Caching Optimization
**Current:** CI builds re-download and build some dependencies.

**Proposed:** Enhanced caching strategies for vcpkg and build artifacts.

**Benefits:**
- Faster CI builds (2-3x improvement)
- Reduced CI costs
- More reliable builds (less network dependency)
- Better developer experience

**Implementation:**
```yaml
# GitHub Actions
- uses: actions/cache@v4
  with:
    path: |
      ~/.ccache
      ~/vcpkg_cache
      build/_deps
    key: ${{ runner.os }}-${{ hashFiles('**/CMakeLists.txt') }}
```

**Impact:**
- 🟢 Complexity: Low
- 🟢 Performance Gain: High (CI only)
- 🟢 Risk: Very Low

#### 6. Memory Pool for Graphics
**Current:** Individual allocation for each Bitmap and texture.

**Proposed:** Memory pool for graphics objects.

**Benefits:**
- Reduced allocation overhead
- Better cache locality
- Predictable memory usage
- Faster allocation/deallocation

**Areas to Apply:**
- Bitmap objects
- SDL surfaces and textures
- Tile data structures

**Impact:**
- 🟡 Complexity: Medium
- 🟡 Performance Gain: Medium
- 🟡 Risk: Medium (requires careful design)

### Low Priority

#### 7. Build Time Optimization
**Current:** Full rebuild takes 10-15 minutes.

**Proposed:** Optimize compilation units and dependencies.

**Strategies:**
- Use forward declarations more extensively
- Split large compilation units
- Optimize template instantiations
- Better use of precompiled headers

**Impact:**
- 🟡 Complexity: Medium
- 🟢 Performance Gain: Medium (developer experience)
- 🟢 Risk: Low

#### 8. Release Workflow Simplification
**Current:** Three separate release workflows (simplified, standard, complex).

**Proposed:** Single unified workflow with conditional features.

**Benefits:**
- Easier maintenance
- Consistent behavior
- Better documentation
- Clearer mental model

**Implementation:**
```yaml
jobs:
  release:
    strategy:
      matrix:
        include:
          - profile: minimal  # Quick releases
          - profile: standard  # Normal releases
          - profile: maximum   # Production releases
```

**Impact:**
- 🟢 Complexity: Low
- 🟢 Performance Gain: None (maintenance benefit)
- 🟢 Risk: Very Low

## Testing Improvements

### Current State
- ✅ Comprehensive unit test coverage (46+ tests)
- ✅ Integration tests for major components
- ✅ ROM-dependent tests properly isolated
- ✅ CI-safe test configuration
- ✅ Platform-specific test handling

### Recommendations

#### 1. Visual Regression Testing
**Goal:** Catch graphics rendering regressions automatically.

**Approach:**
- Capture screenshots of editor states
- Compare against baseline images
- Flag visual differences for review

**Tools:** ImGui Test Engine (already integrated)

#### 2. Performance Benchmarks
**Goal:** Track performance regressions in CI.

**Metrics:**
- ROM load time
- Graphics sheet decompression
- Editor initialization
- Memory usage

**Implementation:** Google Benchmark (already a dependency)

#### 3. Fuzz Testing
**Goal:** Find edge cases and crashes through random input.

**Areas:**
- ROM parsing
- Compression/decompression
- Palette handling
- Tile data processing

**Tools:** LibFuzzer or AFL

## Metrics & Monitoring

### Current Measurements
- Build time: ~10-15 minutes (full rebuild)
- ROM load time: ~2-3 seconds
- Memory usage: ~500MB-1GB typical
- Test execution: ~30 seconds (CI), ~2 minutes (full)

### Target Improvements
- Build time: <5 minutes (incremental), <10 minutes (full)
- ROM load time: <1 second (with lazy loading)
- Memory usage: <300MB (with streaming)
- Test execution: <15 seconds (CI), <1 minute (full)

## Action Items

### Immediate (v0.3.2)
- [x] Fix Windows stack overflow
- [x] Isolate development utilities
- [x] Fix graphics segfaults
- [x] Update build documentation
- [ ] Complete tile16 palette display fixes

### Short Term (v0.3.3)
- [ ] Implement lazy graphics loading
- [ ] Move large allocations to heap
- [ ] Enhanced CI caching
- [ ] Performance benchmarks

### Medium Term (v0.4.0)
- [ ] Streaming ROM assets
- [ ] Memory pool for graphics
- [ ] Visual regression tests
- [ ] Enhanced test isolation

### Long Term (v0.5.0+)
- [ ] Fuzz testing integration
- [ ] Build time optimization
- [ ] Release workflow unification
- [ ] Advanced memory profiling

## Conclusion

The v0.3.2 release focuses on stability and reliability improvements, particularly for the Windows platform. The recommended optimizations provide a clear roadmap for future performance and maintainability improvements while maintaining backward compatibility and code quality.

For questions or suggestions, please open an issue or discussion on the GitHub repository.
