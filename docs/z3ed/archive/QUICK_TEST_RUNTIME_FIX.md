# Quick Test: Runtime Fix Validation

**Created**: October 2, 2025, 10:00 PM  
**Purpose**: Quick validation that the runtime fix works  
**Time Required**: 15-20 minutes

## Prerequisites

Ensure both targets are built:
```bash
cmake --build build-grpc-test --target z3ed -j8
cmake --build build-grpc-test --target yaze -j8
```

## Test Sequence

### Test 1: Server Startup (2 minutes)

**Objective**: Verify YAZE starts with test harness enabled

```bash
# Terminal 1: Start YAZE
./build-grpc-test/bin/yaze.app/Contents/MacOS/yaze \
  --enable_test_harness \
  --test_harness_port=50052 \
  --rom_file=assets/zelda3.sfc &

# Wait for startup
sleep 3

# Verify server is listening
lsof -i :50052
```

**Expected Output**:
```
COMMAND   PID      USER   FD   TYPE DEVICE SIZE/OFF NODE NAME
yaze    12345   scawful   15u  IPv4  ...      0t0  TCP *:50052 (LISTEN)
```

**Success**: ✅ Server is listening on port 50052  
**Failure**: ❌ No output → check logs for errors

---

### Test 2: Ping RPC (1 minute)

**Objective**: Verify basic gRPC connectivity

```bash
# Terminal 2: Test ping
grpcurl -plaintext \
  -import-path src/app/core/proto \
  -proto imgui_test_harness.proto \
  -d '{"message":"test"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Ping
```

**Expected Output**:
```json
{
  "message": "Pong: test",
  "timestampMs": "1696287654321",
  "yazeVersion": "0.3.2"
}
```

**Success**: ✅ JSON response received  
**Failure**: ❌ Connection error → check server still running

---

### Test 3: Click RPC - No Assertion Failure (5 minutes)

**Objective**: Verify the runtime fix - no ImGuiTestEngine assertion

```bash
# Click Overworld button
grpcurl -plaintext \
  -import-path src/app/core/proto \
  -proto imgui_test_harness.proto \
  -d '{"target":"button:Overworld","type":"LEFT"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Click
```

**Watch YAZE Window**:
- Overworld Editor window should open
- No crash or assertion dialog

**Watch Terminal 1 (YAZE logs)**:
- Should NOT see: `Assertion failed: (engine->TestContext->Test != test)`
- Should see: Test execution logs (if verbose enabled)

**Expected gRPC Response**:
```json
{
  "success": true,
  "message": "Clicked button 'Overworld'",
  "executionTimeMs": 234
}
```

**Critical Success Criteria**:
- ✅ No assertion failure
- ✅ YAZE still running after RPC
- ✅ Overworld Editor opened
- ✅ gRPC response indicates success

**If Assertion Occurs**:
❌ The fix didn't work - check:
1. Was the correct file compiled? (`imgui_test_harness_service.cc`)
2. Are you running the newly built binary?
3. Check git diff to verify changes applied

---

### Test 4: Multiple Clicks (3 minutes)

**Objective**: Verify test accumulation doesn't cause issues

```bash
# Click Overworld (already open - should be idempotent)
grpcurl -plaintext \
  -import-path src/app/core/proto \
  -proto imgui_test_harness.proto \
  -d '{"target":"button:Overworld","type":"LEFT"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Click

# Click Dungeon
grpcurl -plaintext \
  -import-path src/app/core/proto \
  -proto imgui_test_harness.proto \
  -d '{"target":"button:Dungeon","type":"LEFT"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Click

# Click Graphics (if exists)
grpcurl -plaintext \
  -import-path src/app/core/proto \
  -proto imgui_test_harness.proto \
  -d '{"target":"button:Graphics","type":"LEFT"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Click
```

**Success Criteria**:
- ✅ All 3 RPCs complete successfully
- ✅ No assertions or crashes
- ✅ YAZE remains responsive

**Note**: Multiple windows may open - this is expected

---

### Test 5: CLI Agent Test Command (5 minutes)

**Objective**: Verify end-to-end natural language automation

```bash
# Terminal 2: Run CLI agent test
./build-grpc-test/bin/z3ed agent test \
  --prompt "Open Overworld editor"
```

**Expected Output**:
```
=== GUI Automation Test ===
Prompt: Open Overworld editor
Server: localhost:50052

Generated workflow:
Workflow: Open Overworld Editor
  1. Click(button:Overworld)
  2. Wait(window_visible:Overworld Editor, 5000ms)

✓ Connected to test harness

[1/2] Click(button:Overworld) ... ✓ (125ms)
[2/2] Wait(window_visible:Overworld Editor, 5000ms) ... ✓ (1250ms)

✅ Test passed in 1375ms
```

**Success Criteria**:
- ✅ Workflow generation succeeds
- ✅ Connection to test harness succeeds
- ✅ Both steps execute successfully
- ✅ No errors or crashes
- ✅ Exit code 0

---

### Test 6: Graceful Shutdown (1 minute)

**Objective**: Verify test cleanup happens correctly

```bash
# Terminal 1: Stop YAZE (Ctrl+C or)
killall yaze

# Wait a moment
sleep 2

# Verify process stopped
ps aux | grep yaze
```

**Expected**:
- No hanging yaze processes
- No error messages about test cleanup
- Clean shutdown

**Success**: ✅ Process stopped cleanly  
**Failure**: ❌ Hanging process → may need `killall -9 yaze`

---

## Overall Success Criteria

✅ **PASS** if ALL of the following are true:
1. Server starts without errors
2. Ping RPC responds correctly
3. Click RPC executes without assertion failure
4. Multiple clicks work without issues
5. CLI agent test command works end-to-end
6. YAZE shuts down cleanly

❌ **FAIL** if ANY of the following occur:
- Assertion failure: `(engine->TestContext->Test != test)`
- Crash during RPC execution
- Hanging process on shutdown
- CLI command unable to connect
- Timeout on valid widget clicks

## Troubleshooting

### Issue: "Address already in use" on port 50052

**Solution**:
```bash
# Kill any existing YAZE processes
killall yaze

# Wait a moment for port to be released
sleep 2

# Try again
```

### Issue: grpcurl command not found

**Solution**:
```bash
# Install grpcurl on macOS
brew install grpcurl
```

### Issue: Widget not found (timeout)

**Possible Causes**:
1. YAZE not fully started when RPC sent → wait 5s after launch
2. Widget name incorrect → check YAZE source for button labels
3. Widget disabled or hidden → verify in YAZE GUI

**Solution**:
- Increase wait time before sending RPCs
- Verify widget exists by clicking manually first
- Check widget naming in YAZE source code

### Issue: Build failed

**Solution**:
```bash
# Clean build directory
rm -rf build-grpc-test

# Reconfigure and rebuild
cmake -B build-grpc-test -DYAZE_WITH_GRPC=ON
cmake --build build-grpc-test --target yaze -j8
cmake --build build-grpc-test --target z3ed -j8
```

## Next Steps After Passing

If all tests pass:

1. **Update Status**:
   - Mark IT-02 runtime fix as validated
   - Update IMPLEMENTATION_STATUS_OCT2_PM.md
   - Update NEXT_PRIORITIES_OCT2.md

2. **Run Full E2E Validation**:
   - Follow [E2E_VALIDATION_GUIDE.md](E2E_VALIDATION_GUIDE.md)
   - Test all 6 RPCs thoroughly
   - Test proposal workflow
   - Document edge cases

3. **Move to Priority 2**:
   - Begin Policy Framework implementation (AW-04)
   - 6-8 hours of work remaining

## Recording Results

Document your test results:

```markdown
## Test Results - [Date/Time]

**Tester**: [Name]
**Environment**: macOS [version], YAZE build [hash]

### Results:
- [ ] Test 1: Server Startup
- [ ] Test 2: Ping RPC
- [ ] Test 3: Click RPC (no assertion)
- [ ] Test 4: Multiple Clicks
- [ ] Test 5: CLI Agent Test
- [ ] Test 6: Graceful Shutdown

**Overall Result**: PASS / FAIL

**Notes**:
- [Any observations or issues]

**Next Action**:
- [What to do next based on results]
```

---

**Last Updated**: October 2, 2025, 10:00 PM  
**Status**: Ready for validation testing
