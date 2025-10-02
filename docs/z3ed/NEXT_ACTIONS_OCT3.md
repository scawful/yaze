# Next Actions - October 3, 2025

**Created**: October 2, 2025, 10:00 PM  
**Target Completion**: October 3, 2025 (Tomorrow)  
**Total Time**: 2-3 hours

## Immediate Priority: Complete E2E Validation

### Context
The E2E test harness is operational but window detection fails after menu clicks. Menu items are successfully clicked (verified by logs showing "Clicked menuitem"), but subsequent window visibility checks timeout.

### Root Cause
When a menu item is clicked in YAZE, it calls a callback that sets a flag (`editor.set_active(true)`). The actual ImGui window is not created until the next frame's `Update()` call. ImGuiTestEngine's window detection runs immediately after the click, before the window exists.

### Solution Strategy

#### Option 1: Add Frame Yield (Recommended)
**Implementation**: Modify Click RPC to yield control after successful click

```cpp
// In imgui_test_harness_service.cc, Click RPC handler
absl::StatusOr<ClickResponse> ImGuiTestHarnessServiceImpl::Click(...) {
  // ... existing click logic ...
  
  // After successful click, yield to let ImGui process frames
  ImGuiTestEngine_Yield(engine);
  
  // Or sleep briefly to allow window creation
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  
  return response;
}
```

**Pros**: Simple, reliable, matches ImGui's event loop model  
**Cons**: Adds 500ms latency per click

#### Option 2: Partial Name Matching
**Implementation**: Make window name matching more forgiving

```cpp
// In Wait/Assert RPC handlers
bool FindWindowByPartialName(const std::string& target) {
  ImGuiContext* ctx = ImGui::GetCurrentContext();
  std::string target_lower = absl::AsciiStrToLower(target);
  
  for (ImGuiWindow* window : ctx->Windows) {
    if (!window) continue;
    
    std::string window_name = absl::AsciiStrToLower(window->Name);
    
    // Strip icon prefixes (they're non-ASCII characters)
    if (absl::StrContains(window_name, target_lower)) {
      return window->Active && window->WasActive;
    }
  }
  return false;
}
```

**Pros**: More robust, handles icon prefixes  
**Cons**: May match wrong window if names are similar

#### Option 3: Increase Timeouts + Better Polling
**Implementation**: Update test script with longer timeouts

```bash
# Wait longer for window creation after menu click
run_test "Wait (Overworld Editor)" "Wait" \
  '{"condition":"window_visible:Overworld Editor","timeout_ms":10000,"poll_interval_ms":200}'
```

**Pros**: No code changes needed  
**Cons**: Slower tests, doesn't fix underlying issue

### Recommended Approach

**Implement all three**:
1. Add 500ms sleep after menu item clicks (Option 1)
2. Implement partial name matching for window detection (Option 2)  
3. Update test script with 10s timeouts (Option 3)

**Why**: Defense in depth - each layer handles a different edge case:
- Sleep handles timing issues
- Partial matching handles name variations
- Longer timeouts handle slow systems

### Implementation Steps (2-3 hours)

#### Step 1: Fix Click RPC (30 minutes)
**File**: `src/app/core/imgui_test_harness_service.cc`

```cpp
// After successful test execution in Click RPC:
if (success) {
  // Yield control to ImGui to process frames
  // This allows menu callbacks to create windows before we check visibility
  for (int i = 0; i < 3; ++i) {  // Yield 3 frames
    ImGuiTestEngine_Yield(engine);
  }
  // Also add a brief sleep for safety
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
}
```

**Test**:
```bash
# Rebuild
cmake --build build-grpc-test --target yaze -j8

# Test manually
./build-grpc-test/bin/yaze.app/Contents/MacOS/yaze \
  --enable_test_harness --test_harness_port=50052 \
  --rom_file=assets/zelda3.sfc &

sleep 3

# Click menu
grpcurl -plaintext -import-path src/app/core/proto \
  -proto imgui_test_harness.proto \
  -d '{"target":"menuitem: Overworld Editor","type":"LEFT"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Click

# Check window (should work now)
grpcurl -plaintext -import-path src/app/core/proto \
  -proto imgui_test_harness.proto \
  -d '{"condition":"window_visible:Overworld Editor","timeout_ms":5000}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Wait
```

#### Step 2: Improve Window Detection (1 hour)
**File**: `src/app/core/imgui_test_harness_service.cc`

Add helper function:
```cpp
// Add to ImGuiTestHarnessServiceImpl class
private:
  // Helper: Find window by partial name match (case-insensitive)
  ImGuiWindow* FindWindowByName(const std::string& target) {
    ImGuiContext* ctx = ImGui::GetCurrentContext();
    if (!ctx) return nullptr;
    
    std::string target_clean = absl::AsciiStrToLower(
        absl::StripAsciiWhitespace(target));
    
    for (ImGuiWindow* window : ctx->Windows) {
      if (!window || !window->WasActive) continue;
      
      std::string window_name = window->Name;
      
      // Strip leading icon (they're typically 1-4 bytes of non-ASCII)
      size_t first_ascii = 0;
      while (first_ascii < window_name.size() && 
             !std::isalnum(window_name[first_ascii]) &&
             window_name[first_ascii] != '_') {
        ++first_ascii;
      }
      window_name = window_name.substr(first_ascii);
      
      window_name = absl::AsciiStrToLower(
          absl::StripAsciiWhitespace(window_name));
      
      // Check if window name contains target
      if (absl::StrContains(window_name, target_clean)) {
        return window;
      }
    }
    return nullptr;
  }
```

Update Wait/Assert RPCs to use this helper:
```cpp
// In Wait RPC, replace WindowInfo() call:
bool condition_met = false;
if (condition_type == "window_visible") {
  ImGuiWindow* window = FindWindowByName(condition_value);
  condition_met = (window != nullptr && window->Active);
}
// ... similar for Assert RPC ...
```

**Test**: Same as Step 1, should be more reliable

#### Step 3: Update Test Script (15 minutes)
**File**: `scripts/test_harness_e2e.sh`

```bash
# Update test sequence with proper waits:

# Click and wait for window
run_test "Click (Open Overworld Editor)" "Click" \
  '{"target":"menuitem: Overworld Editor","type":"LEFT"}'

# Window should appear after click (with yield fix)
run_test "Wait (Overworld Editor)" "Wait" \
  '{"condition":"window_visible:Overworld Editor","timeout_ms":10000,"poll_interval_ms":200}'

# Assert window visible
run_test "Assert (Overworld Editor Visible)" "Assert" \
  '{"condition":"visible:Overworld Editor"}'
```

**Test**: Run full E2E script
```bash
killall yaze 2>/dev/null || true
sleep 2
./scripts/test_harness_e2e.sh
```

**Expected**: All tests pass except Screenshot (proto issue)

#### Step 4: Document Widget Naming (30 minutes)
**File**: `docs/z3ed/WIDGET_NAMING_GUIDE.md` (new)

Create comprehensive guide:
- Widget types and naming patterns
- How icon prefixes work
- Best practices for test writers
- Timeout recommendations
- Common pitfalls and solutions

**File**: `docs/z3ed/IT-01-QUICKSTART.md` (update)

Add section on widget naming conventions with real examples

#### Step 5: Update Documentation (15 minutes)
**Files**: 
- `E6-z3ed-implementation-plan.md` - Mark E2E validation complete
- `TEST_VALIDATION_STATUS_OCT2.md` - Update with final results
- `NEXT_PRIORITIES_OCT2.md` - Mark Priority 0 complete, focus on Priority 1

### Success Criteria

- [ ] Click RPC yields frames after menu actions
- [ ] Window detection uses partial name matching
- [ ] E2E test script passes 5/6 tests (all except Screenshot)
- [ ] Can open Overworld Editor via gRPC and detect window
- [ ] Can open Dungeon Editor via gRPC and detect window
- [ ] Documentation updated with widget naming guide
- [ ] Ready to move to Policy Framework (AW-04)

### If This Doesn't Work

**Plan B**: Manual testing with ImGui Debug tools
1. Enable ImGui Demo window in YAZE
2. Use `ImGui::ShowMetricsWindow()` to inspect window names
3. Log exact window names after menu clicks
4. Update test script with exact names (including icons)

**Plan C**: Alternative testing approach
1. Skip window detection for now
2. Focus on button/input testing within already-open windows
3. Document limitation and move forward
4. Revisit window detection in later sprint

## After E2E Validation Complete

### Priority 1: Policy Evaluation Framework (6-8 hours)

**Goal**: YAML-based constraint system for gating proposal acceptance

**Key Files**:
- `src/cli/service/policy_evaluator.{h,cc}` - Core evaluation engine
- `.yaze/policies/agent.yaml` - Example policy configuration
- `src/app/editor/system/proposal_drawer.cc` - UI integration

**Deliverables**:
1. YAML policy parser
2. Policy evaluation engine (4 policy types)
3. ProposalDrawer integration with gate logic
4. Policy override workflow
5. Documentation and examples

**See**: [NEXT_PRIORITIES_OCT2.md](NEXT_PRIORITIES_OCT2.md) for detailed implementation guide

### Priority 2: Windows Cross-Platform Testing (4-6 hours)

**Goal**: Verify everything works on Windows

**Tasks**:
- Build on Windows with MSVC
- Test gRPC server startup
- Test all RPC methods
- Document Windows-specific setup
- Fix any platform-specific issues

### Priority 3: Production Readiness (6-8 hours)

**Goal**: Make system ready for real usage

**Tasks**:
- Add telemetry (opt-in)
- Implement Screenshot RPC
- Add more test coverage
- Performance profiling
- Error recovery improvements
- User-facing documentation

## Timeline

**October 3, 2025 (Tomorrow)**:
- Morning: E2E validation fixes (2-3 hours)
- Afternoon: Policy framework start (3-4 hours)

**October 4, 2025**:
- Complete policy framework (3-4 hours)
- Testing and documentation (2 hours)

**October 5-6, 2025**:
- Windows cross-platform testing
- Production readiness tasks

**Target v0.1 Release**: October 6, 2025

---

**Last Updated**: October 2, 2025, 10:00 PM  
**Author**: GitHub Copilot (with @scawful)  
**Status**: Ready for execution - all blockers removed
