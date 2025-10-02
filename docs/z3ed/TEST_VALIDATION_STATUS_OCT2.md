# Test Validation Status - October 2, 2025

**Time**: 9:30 PM  
**Status**: E2E Tests Running | Menu Interaction Verified | Window Detection Issue Identified

## Current Test Results

### Working ✅
1. **Ping RPC** - Health check fully operational
2. **Menu Item Clicks** - Successfully clicking menu items via gRPC
   - Example: `menuitem: Overworld Editor` → clicked successfully
   - Example: `menuitem: Dungeon Editor` → clicked successfully

### Issues Identified 🔍

#### Issue 1: Window Detection After Menu Click
**Problem**: Menu items are clicked successfully, but subsequent window visibility checks fail

**Observed Behavior**:
```
Test 2: Click (Open Overworld Editor)
✓ Clicked menuitem ' Overworld Editor' (1873ms)

Test 3: Wait (Overworld Editor Window)
✗ Condition 'window_visible:Overworld Editor' not met after 5000ms timeout
```

**Root Cause Analysis**:
1. Menu items call `editor.set_active(true)` 
2. This sets a flag but doesn't immediately create ImGui window
3. Window creation happens in next frame's `Update()` call
4. ImGuiTestEngine's `WindowInfo()` API may not see newly created windows immediately
5. Window title may include ICON_MD prefix: `ICON_MD_LAYERS " Overworld Editor"`

**Potential Solutions**:
- A. Use longer wait time (current: 5s)
- B. Check for window with icon prefix: `window_visible: Overworld Editor`
- C. Use different condition type (element_visible vs window_visible)
- D. Add frame yield between menu click and window check

#### Issue 2: Screenshot RPC Proto Mismatch
**Problem**: Screenshot request proto schema doesn't match client usage

**Error Message**:
```
message type yaze.test.ScreenshotRequest has no known field named region
```

**Solution**: Update proto or skip for now (non-blocking for core functionality)

## Next Steps (Priority Order)

### 1. Debug Window Detection (30 min)
**Goal**: Understand why windows aren't detected after menu clicks

**Tasks**:
- [ ] Check actual window titles in YAZE (with icons)
- [ ] Test with exact window name including icon
- [ ] Add diagnostic logging to Wait RPC
- [ ] Try element_visible condition instead
- [ ] Increase wait timeout to 10s

**Test Command**:
```bash
# Terminal 1: Start YAZE
./build-grpc-test/bin/yaze.app/Contents/MacOS/yaze \
  --enable_test_harness \
  --test_harness_port=50052 \
  --rom_file=assets/zelda3.sfc &

# Terminal 2: Manual test sequence
sleep 5  # Let YAZE fully initialize

# Click menu item
grpcurl -plaintext -import-path src/app/core/proto -proto imgui_test_harness.proto \
  -d '{"target":"menuitem: Overworld Editor","type":"LEFT"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Click

# Wait a few frames
sleep 2

# Try different window name variations
grpcurl -plaintext -import-path src/app/core/proto -proto imgui_test_harness.proto \
  -d '{"condition":"window_visible:Overworld Editor","timeout_ms":10000}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Wait

# Or with icon
grpcurl -plaintext -import-path src/app/core/proto -proto imgui_test_harness.proto \
  -d '{"condition":"window_visible: Overworld Editor","timeout_ms":10000}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Wait
```

### 2. Fix Window Name Matching (1 hour)
**Options**:

**Option A: Strip Icons from Target Names**
```cpp
// In Wait RPC handler
std::string CleanWindowName(const std::string& name) {
  // Strip ICON_MD_ prefixes and leading spaces
  // " Overworld Editor" → "Overworld Editor"
  return absl::StripAsciiWhitespace(name);
}
```

**Option B: Use Partial Name Matching**
```cpp
// Check if window name contains target (case-insensitive)
bool window_found = false;
for (ImGuiWindow* window : ImGui::GetCurrentContext()->Windows) {
  if (absl::StrContains(absl::AsciiStrToLower(window->Name), 
                        absl::AsciiStrToLower(target))) {
    window_found = true;
    break;
  }
}
```

**Option C: Add Frame Yield**
```cpp
// In Click RPC, after successful click:
// Yield control back to ImGui to process one frame
ImGuiTestEngine_Yield(engine);
// Or sleep briefly
std::this_thread::sleep_for(std::chrono::milliseconds(500));
```

### 3. Update E2E Test Script (15 min)
Once window detection works, update test script:
```bash
# Use working window names
run_test "Wait (Overworld Editor)" "Wait" \
  '{"condition":"window_visible:Overworld Editor","timeout_ms":10000,"poll_interval_ms":100}'

# Add delay between click and wait
echo "Waiting for window to appear..."
sleep 2
```

### 4. Document Widget Naming Convention (30 min)
Create guide for test writers:

**Widget Naming Patterns**:
- Menu items: `menuitem:<Name>` (with or without icon prefix)
- Buttons: `button:<Label>`
- Windows: `window:<Title>` (may include icons)
- Input fields: `input:<Label>`
- Tabs: `tab:<Name>`

**Best Practices**:
- Use partial matching for windows (handles icon prefixes)
- Add 1-2s delay after menu clicks before checking windows
- Use 10s+ timeouts for initial window creation
- Use shorter timeouts (2-5s) for interactions within open windows

## Test Coverage Status

| RPC Method | Status | Notes |
|------------|--------|-------|
| Ping | ✅ Working | Health check operational |
| Click | ⚠️ Partial | Menu items work, window detection needs fix |
| Type | 📋 Pending | Depends on window detection fix |
| Wait | ⚠️ Partial | Polling works, condition matching needs fix |
| Assert | ⚠️ Partial | Same as Wait - condition matching issue |
| Screenshot | 🔧 Blocked | Proto mismatch - non-critical |

## Time Investment Today

- Build system fixes: 1h
- Type conversion debugging: 0.5h
- Test script updates: 0.5h
- Widget investigation: 1h
- **Total**: 3 hours

## Estimated Remaining Work

- Debug window detection: 0.5h
- Fix window matching logic: 1h
- Update tests and validate: 0.5h
- Document findings: 0.5h
- **Total**: 2.5 hours

**Target Completion**: October 3, 2025 (tomorrow morning)

## Success Criteria for Validation Complete

- [ ] All 5 main RPCs working (Ping, Click, Wait, Assert, Type)
- [ ] Can open editor windows via menu clicks
- [ ] Can detect window visibility after opening
- [ ] Can assert on window state
- [ ] E2E test script passes all tests except Screenshot
- [ ] Documentation updated with real examples
- [ ] Widget naming conventions documented

## Next Phase After Validation

Once E2E validation complete:
→ **Priority 3: Policy Evaluation Framework (AW-04)**
- 6-8 hours estimated
- Can work in parallel with validation improvements

---

**Last Updated**: October 2, 2025, 9:30 PM  
**Author**: GitHub Copilot (with @scawful)  
**Status**: In progress - window detection debugging needed
