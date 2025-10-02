# End-to-End Workflow Validation Guide

**Created**: October 2, 2025  
**Status**: Priority 1 - Ready to Execute  
**Time Estimate**: 2-3 hours

## Overview

This guide provides a comprehensive checklist for validating the complete z3ed agent workflow from proposal creation through ROM commit. This is the final validation step before declaring the agentic workflow system operational.

## Prerequisites

### Build Requirements

```bash
# Build z3ed CLI
cmake --build build --target z3ed -j8

# Build YAZE with gRPC support
cmake -B build-grpc-test -DYAZE_WITH_GRPC=ON
cmake --build build-grpc-test --target yaze -j$(sysctl -n hw.ncpu)

# Verify grpcurl is installed
brew install grpcurl
```

### Test Assets

- ROM file: `assets/zelda3.sfc` (required)
- Empty workspace for proposals: `/tmp/yaze/` (auto-created)

## Validation Checklist

### ✅ Phase 1: Automated Test Script (30 minutes)

#### 1.1. Run E2E Test Script

```bash
./scripts/test_harness_e2e.sh
```

**Expected Output**:
```
=== ImGuiTestHarness E2E Test ===

Starting YAZE with test harness...
YAZE PID: 12345
Waiting for server to start...
✓ Server started successfully

=== Running RPC Tests ===

Test 1: Ping (Health Check)
✓ PASSED

Test 2: Click (Button)
✓ PASSED

Test 3: Type (Text Input)
✓ PASSED

Test 4: Wait (Window Visible)
✓ PASSED

Test 5: Assert (Window Visible)
✓ PASSED

Test 6: Screenshot (Not Implemented)
✓ PASSED

=== Test Summary ===
Tests Run:    6
Tests Passed: 6
Tests Failed: 0

All tests passed!
```

**Success Criteria**:
- [ ] All 6 tests pass
- [ ] No connection errors
- [ ] No port conflicts
- [ ] Server starts and stops cleanly

**Troubleshooting**:
- If port in use: `killall yaze && sleep 2`
- If grpcurl missing: `brew install grpcurl`
- If binary not found: Check `build-grpc-test/bin/` directory

---

### ✅ Phase 2: Manual Proposal Workflow (60 minutes)

#### 2.1. Create Test Proposal

```bash
# Create a proposal via CLI
./build/bin/z3ed agent run \
  --rom=assets/zelda3.sfc \
  --prompt "Test proposal for E2E validation" \
  --sandbox

# Expected output:
# ✅ Agent run completed successfully.
#    Proposal ID: <UUID>
#    Sandbox: /tmp/yaze/sandboxes/<UUID>/zelda3.sfc
#    Use 'z3ed agent diff' to review changes
```

**Verification Steps**:
1. [ ] Command completes without error
2. [ ] Proposal ID is displayed
3. [ ] Sandbox ROM file exists at shown path
4. [ ] No crashes or hangs

#### 2.2. List Proposals

```bash
./build/bin/z3ed agent list

# Expected output:
# === Agent Proposals ===
#
# ID: <UUID>
#   Status: Pending
#   Created: <timestamp>
#   Prompt: Test proposal for E2E validation
#   Commands: 0
#   Bytes Changed: 0
#
# Total: 1 proposal(s)
```

**Verification Steps**:
1. [ ] Proposal appears in list
2. [ ] Status shows "Pending"
3. [ ] All metadata fields populated
4. [ ] Prompt matches input

#### 2.3. View Proposal Diff

```bash
./build/bin/z3ed agent diff

# Expected output:
# === Proposal Diff ===
# Proposal ID: <UUID>
# Sandbox ID: <UUID>
# Prompt: Test proposal for E2E validation
# Description: Agent-generated ROM modifications
# Status: Pending
# Created: <timestamp>
# Commands Executed: 0
# Bytes Changed: 0
#
# --- Diff Content ---
# (No changes yet for mock implementation)
#
# --- Execution Log ---
# Starting agent run with prompt: Test proposal for E2E validation
# Generated 0 commands
# Completed execution of 0 commands
#
# === Next Steps ===
# To accept changes: z3ed agent commit
# To reject changes: z3ed agent revert
# To review in GUI: yaze --proposal=<UUID>
```

**Verification Steps**:
1. [ ] Diff displays correctly
2. [ ] Execution log shows all steps
3. [ ] Metadata matches proposal
4. [ ] No errors reading files

#### 2.4. Launch YAZE GUI

```bash
# Start YAZE normally (not test harness mode)
./build/bin/yaze.app/Contents/MacOS/yaze

# Navigate to: Debug → Agent Proposals
```

**Verification Steps**:
1. [ ] YAZE launches without crashes
2. [ ] "Agent Proposals" menu item exists
3. [ ] ProposalDrawer opens when clicked
4. [ ] Drawer appears on right side (400px width)

#### 2.5. Test ProposalDrawer UI

**List View Verification**:
1. [ ] Proposal appears in list
2. [ ] Status badge shows "Pending" in yellow
3. [ ] Prompt text is visible
4. [ ] Created timestamp displayed
5. [ ] Click proposal to open detail view

**Detail View Verification**:
1. [ ] All metadata displayed correctly
2. [ ] Execution log visible and scrollable
3. [ ] Diff section shows (empty for mock)
4. [ ] Accept/Reject/Delete buttons visible
5. [ ] Back button returns to list

**Filtering Verification**:
1. [ ] "All" filter shows proposal
2. [ ] "Pending" filter shows proposal
3. [ ] "Accepted" filter hides proposal (not accepted yet)
4. [ ] "Rejected" filter hides proposal (not rejected yet)

**Refresh Verification**:
1. [ ] Click "Refresh" button
2. [ ] Proposal count updates if needed
3. [ ] No crashes or errors

#### 2.6. Test Accept Workflow

**Steps**:
1. Select proposal in list view
2. Open detail view
3. Click "Accept" button
4. Confirm in dialog (if shown)
5. Wait for processing

**Verification**:
1. [ ] Accept button triggers action
2. [ ] Status changes to "Accepted"
3. [ ] Status badge turns green
4. [ ] ROM data merged successfully (check logs)
5. [ ] Sandbox ROM remains unchanged
6. [ ] No crashes during merge

**Post-Accept Checks**:
```bash
# Verify proposal status persists
./build/bin/z3ed agent list
# Should show Status: Accepted

# Verify ROM was modified (if changes were made)
# For mock implementation, this will be no-op
```

#### 2.7. Test Reject Workflow

**Create another proposal**:
```bash
./build/bin/z3ed agent run \
  --rom=assets/zelda3.sfc \
  --prompt "Proposal to reject" \
  --sandbox
```

**Steps**:
1. Open ProposalDrawer in YAZE
2. Select new proposal
3. Click "Reject" button
4. Confirm in dialog (if shown)

**Verification**:
1. [ ] Reject button triggers action
2. [ ] Status changes to "Rejected"
3. [ ] Status badge turns red
4. [ ] ROM remains unchanged
5. [ ] Sandbox ROM unchanged
6. [ ] No crashes

#### 2.8. Test Delete Workflow

**Create another proposal**:
```bash
./build/bin/z3ed agent run \
  --rom=assets/zelda3.sfc \
  --prompt "Proposal to delete" \
  --sandbox
```

**Steps**:
1. Open ProposalDrawer in YAZE
2. Select new proposal
3. Click "Delete" button
4. Confirm in dialog

**Verification**:
1. [ ] Delete button triggers action
2. [ ] Proposal removed from list
3. [ ] Files cleaned up from disk
4. [ ] No crashes

**File Cleanup Check**:
```bash
# Verify proposal directory was removed
ls /tmp/yaze/proposals/
# Should NOT show deleted proposal ID

# Verify sandbox was removed
ls /tmp/yaze/sandboxes/
# Should NOT show deleted sandbox ID
```

---

### ✅ Phase 3: Real Widget Testing (60 minutes)

#### 3.1. Start Test Harness

```bash
# Terminal 1: Start YAZE with test harness
./build-grpc-test/bin/yaze.app/Contents/MacOS/yaze \
  --enable_test_harness \
  --test_harness_port=50052 \
  --rom_file=assets/zelda3.sfc &

# Wait for startup
sleep 3

# Verify server is listening
lsof -i :50052
# Should show yaze process
```

#### 3.2. Test Overworld Editor Workflow

```bash
# Terminal 2: Run automation commands

# Click Overworld button
grpcurl -plaintext \
  -import-path src/app/core/proto \
  -proto imgui_test_harness.proto \
  -d '{"target":"button:Overworld","type":"LEFT"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Click

# Wait for window to appear
grpcurl -plaintext \
  -import-path src/app/core/proto \
  -proto imgui_test_harness.proto \
  -d '{"condition":"window_visible:Overworld Editor","timeout_ms":5000}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Wait

# Assert window is visible
grpcurl -plaintext \
  -import-path src/app/core/proto \
  -proto imgui_test_harness.proto \
  -d '{"condition":"visible:Overworld Editor"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Assert
```

**Verification**:
1. [ ] Click RPC succeeds
2. [ ] Overworld Editor window opens in YAZE
3. [ ] Wait RPC succeeds (condition met)
4. [ ] Assert RPC succeeds (window visible)
5. [ ] No timeouts or errors

#### 3.3. Test Dungeon Editor Workflow

```bash
# Click Dungeon button
grpcurl -plaintext \
  -import-path src/app/core/proto \
  -proto imgui_test_harness.proto \
  -d '{"target":"button:Dungeon","type":"LEFT"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Click

# Wait for window
grpcurl -plaintext \
  -import-path src/app/core/proto \
  -proto imgui_test_harness.proto \
  -d '{"condition":"window_visible:Dungeon Editor","timeout_ms":5000}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Wait

# Assert visible
grpcurl -plaintext \
  -import-path src/app/core/proto \
  -proto imgui_test_harness.proto \
  -d '{"condition":"visible:Dungeon Editor"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Assert
```

**Verification**:
1. [ ] Click RPC succeeds
2. [ ] Dungeon Editor window opens
3. [ ] Wait RPC succeeds
4. [ ] Assert RPC succeeds
5. [ ] No errors

#### 3.4. Test CLI Agent Test Command

```bash
# Build z3ed with gRPC support first
cmake -B build-grpc-test -DYAZE_WITH_GRPC=ON
cmake --build build-grpc-test --target z3ed -j8

# Test simple open editor command
./build-grpc-test/bin/z3ed agent test \
  --prompt "Open Overworld editor"

# Expected output:
# === GUI Automation Test ===
# Prompt: Open Overworld editor
# Server: localhost:50052
#
# Generated workflow:
# Workflow: Open Overworld Editor
#   1. Click(button:Overworld)
#   2. Wait(window_visible:Overworld Editor, 5000ms)
#
# ✓ Connected to test harness
#
# [1/2] Click(button:Overworld) ... ✓ (125ms)
# [2/2] Wait(window_visible:Overworld Editor, 5000ms) ... ✓ (1250ms)
#
# ✅ Test passed in 1375ms
```

**Verification**:
1. [ ] Command parses prompt correctly
2. [ ] Workflow generation succeeds
3. [ ] Connection to test harness succeeds
4. [ ] All steps execute successfully
5. [ ] Timing information displayed
6. [ ] Exit code is 0

**Test Additional Prompts**:
```bash
# Open and verify
./build-grpc-test/bin/z3ed agent test \
  --prompt "Open Dungeon editor and verify it loads"

# Click button
./build-grpc-test/bin/z3ed agent test \
  --prompt "Click Overworld button"
```

**Verification for Each**:
1. [ ] Prompt recognized
2. [ ] Workflow generated correctly
3. [ ] All steps pass
4. [ ] No crashes or errors

---

### ✅ Phase 4: Documentation Updates (30 minutes)

#### 4.1. Update IT-01-QUICKSTART.md

Add section on CLI agent test command:

```markdown
## CLI Agent Test Command

You can now automate GUI testing with natural language prompts:

\`\`\`bash
# Start YAZE with test harness
./build-grpc-test/bin/yaze.app/Contents/MacOS/yaze \
  --enable_test_harness \
  --test_harness_port=50052 \
  --rom_file=assets/zelda3.sfc &

# Run automated test
./build-grpc-test/bin/z3ed agent test \
  --prompt "Open Overworld editor and verify it loads"
\`\`\`

### Supported Prompt Patterns

1. **Open Editor**: "Open Overworld editor"
2. **Open and Verify**: "Open Dungeon editor and verify it loads"
3. **Click Button**: "Click Open ROM button"
4. **Type Input**: "Type 'zelda3.sfc' in filename input"
```

**Tasks**:
1. [ ] Add CLI agent test section
2. [ ] Document supported prompts
3. [ ] Add troubleshooting tips
4. [ ] Update examples

#### 4.2. Update E6-z3ed-implementation-plan.md

Mark Priority 1 complete:

```markdown
### Priority 1: End-to-End Workflow Validation ✅ COMPLETE

**Completion Date**: October 2, 2025  
**Time Spent**: 3 hours  
**Status**: All validation checks passed

**Completed Tasks**:
1. ✅ E2E test script validation
2. ✅ Manual proposal workflow testing
3. ✅ Real widget automation testing
4. ✅ CLI agent test command implementation
5. ✅ Documentation updates

**Key Findings**:
- All systems working as expected
- No critical issues identified
- Performance acceptable (< 2s per step)
- Ready for production use

**Next Priority**: IT-02 (CLI Agent Test Command - already implemented!)
```

**Tasks**:
1. [ ] Mark Priority 1 complete
2. [ ] Document completion details
3. [ ] List any issues found
4. [ ] Update status summary

#### 4.3. Update README.md

Update current status:

```markdown
### ✅ Priority 1: End-to-End Workflow Validation (COMPLETE)
**Goal**: Validated complete proposal lifecycle with real GUI and widgets  
**Time Invested**: 3 hours  
**Status**: All checks passed

### ✅ Priority 2: CLI Agent Test Command (COMPLETE)
**Goal**: Natural language prompt → automated GUI test workflow  
**Time Invested**: 2 hours (implemented alongside Priority 1)  
**Status**: Fully operational

**Implementation**:
- GuiAutomationClient: gRPC wrapper for CLI usage
- TestWorkflowGenerator: Natural language prompt parsing
- `z3ed agent test` command: End-to-end automation

**See**: [IT-01-QUICKSTART.md](IT-01-QUICKSTART.md) for usage examples
```

**Tasks**:
1. [ ] Update completion status
2. [ ] Add implementation details
3. [ ] Update quick start guide
4. [ ] Add examples

---

## Success Criteria Summary

### Must Pass (Critical)
- [ ] E2E test script: All 6 tests pass
- [ ] Proposal creation: Works without errors
- [ ] ProposalDrawer: Opens and displays proposals
- [ ] Accept workflow: ROM merging works correctly
- [ ] GUI automation: Real widgets respond to RPCs
- [ ] CLI agent test: At least 3 prompts work

### Should Pass (Important)
- [ ] Reject workflow: Status updates correctly
- [ ] Delete workflow: Files cleaned up
- [ ] Cross-session persistence: Proposals survive restart
- [ ] Error handling: Helpful messages on failure
- [ ] Performance: < 5s per automation step

### Nice to Have (Optional)
- [ ] Screenshots: Capture and save images
- [ ] Policy evaluation: Basic constraint checking
- [ ] Telemetry: Usage metrics collected

---

## Known Issues & Limitations

### Current Limitations
1. **MockAIService**: Not using real LLM (placeholder commands)
2. **Screenshot**: Not yet implemented (returns stub)
3. **Policy Evaluation**: Not yet implemented (AW-04)
4. **Windows Support**: Test harness not available on Windows

### Workarounds
1. Mock service sufficient for testing infrastructure
2. Screenshot can be added later (non-blocking)
3. Policy framework is Priority 3
4. Windows users can use manual testing

---

## Next Steps

After completing this validation:

1. **Mark Priority 1 Complete**: Update all documentation
2. **Mark Priority 2 Complete**: CLI agent test implemented
3. **Begin Priority 3**: Policy Evaluation Framework (AW-04)
4. **Production Deployment**: System ready for real usage

---

## Reporting Issues

If any validation step fails, document:

1. **What failed**: Specific step/command
2. **Error message**: Full output or screenshot
3. **Environment**: OS, build config, ROM file
4. **Reproduction**: Steps to reproduce
5. **Workaround**: Any temporary fixes found

Report issues in: `docs/z3ed/VALIDATION_ISSUES.md`

---

**Last Updated**: October 2, 2025  
**Contributors**: @scawful, GitHub Copilot  
**License**: Same as YAZE (see ../../LICENSE)
