# z3ed Next Priorities - October 2, 2025 (Updated 10:15 PM)

**Current Status**: IT-02 Runtime Fix Complete ✅ | Ready for Quick Validation Testing

This document outlines the immediate next steps for the z3ed agent workflow system after completing the IT-02 runtime fix.

---

## Priority 0: Quick Validation Testing (IMMEDIATE - TONIGHT) 🔄

**Goal**: Validate that the runtime fix works correctly  
**Time Estimate**: 15-20 minutes  
**Status**: Ready to execute  
**Blocking**: None - all code changes complete and compiled

### Why This First?
- Fast feedback on whether the fix actually works
- Identifies any remaining issues early
- Minimal time investment for critical validation
- Enables moving forward with confidence

### Task: Run Quick Test Sequence

**Guide**: Follow [QUICK_TEST_RUNTIME_FIX.md](QUICK_TEST_RUNTIME_FIX.md)

**6 Tests to Execute**:

1. **Server Startup** (2 min)
   ```bash
   ./build-grpc-test/bin/yaze.app/Contents/MacOS/yaze \
     --enable_test_harness \
     --test_harness_port=50052 \
     --rom_file=assets/zelda3.sfc &
   ```
   - ✓ Server starts without crashes
   - ✓ Port 50052 listening

2. **Ping RPC** (1 min)
   ```bash
   grpcurl -plaintext -import-path src/app/core/proto -proto imgui_test_harness.proto \
     -d '{"message":"test"}' 127.0.0.1:50052 yaze.test.ImGuiTestHarness/Ping
   ```
   - ✓ JSON response received
   - ✓ Version and timestamp present

3. **Click RPC - Critical Test** (5 min)
   ```bash
   grpcurl -plaintext -import-path src/app/core/proto -proto imgui_test_harness.proto \
     -d '{"target":"button:Overworld","type":"LEFT"}' \
     127.0.0.1:50052 yaze.test.ImGuiTestHarness/Click
   ```
   - ✓ **NO ASSERTION FAILURE** (most important!)
   - ✓ Overworld Editor opens
   - ✓ Success response received

4. **Multiple Clicks** (3 min)
   - Click Overworld, Dungeon, Graphics buttons
   - ✓ All succeed without crashes
   - ✓ No memory issues

5. **CLI Agent Test** (5 min)
   ```bash
   ./build-grpc-test/bin/z3ed agent test \
     --prompt "Open Overworld editor"
   ```
   - ✓ Workflow generated
   - ✓ All steps execute
   - ✓ No errors

6. **Graceful Shutdown** (1 min)
   ```bash
   killall yaze
   ```
   - ✓ Clean shutdown
   - ✓ No hanging processes

**Success Criteria**:
- All 6 tests pass
- No assertion failures
- No crashes
- Clean shutdown

**If Tests Pass**:
→ Move to Priority 1 (Full E2E Validation)

**If Tests Fail**:
→ Debug issues, check build artifacts, review logs

---

## Priority 1: End-to-End Workflow Validation (NEXT - TOMORROW)

**Goal**: Validate the complete AI agent workflow from proposal creation through ROM commit  
**Time Estimate**: 2-3 hours  
**Status**: Ready to execute  
**Blocking**: None - all prerequisites complete

### Why This First?
- Validate all systems work together in production
- Identify any integration issues before building more features
- Establish baseline for acceptable UX and performance
- Document real-world usage patterns for future improvements

### Task Breakdown

#### 1.1. Automated Test Script Validation (30 min)
**Goal**: Verify E2E test script works correctly

```bash
# Run the automated test script
./scripts/test_harness_e2e.sh

# Expected: All 6 tests pass
# - Ping (health check)
# - Click (button interaction)
# - Type (text input)
# - Wait (condition polling)
# - Assert (state validation)
# - Screenshot (stub - not implemented message)
```

**Success Criteria**:
- Script runs without errors
- All RPCs return success responses
- Server starts and stops cleanly
- No port conflicts or hanging processes

**Troubleshooting**:
- If port 50052 in use: `killall yaze` or use different port
- If grpcurl missing: `brew install grpcurl`
- If binary not found: Build with `cmake --build build-grpc-test`

#### 1.2. Manual Workflow Testing (60 min)
**Goal**: Test complete proposal lifecycle with real GUI

**Steps**:
1. **Create Proposal via CLI**:
   ```bash
   # Build z3ed
   cmake --build build --target z3ed -j8
   
   # Create test proposal with sandbox
   ./build/bin/z3ed agent run "Test proposal for validation" --sandbox
   
   # Verify proposal created
   ./build/bin/z3ed agent list
   ./build/bin/z3ed agent diff --proposal-id <ID>
   ```

2. **Launch YAZE GUI**:
   ```bash
   ./build/bin/yaze.app/Contents/MacOS/yaze
   
   # Open ROM: File → Open ROM → assets/zelda3.sfc
   # Open drawer: Debug → Agent Proposals
   ```

3. **Test ProposalDrawer UI**:
   - ✅ Verify proposal appears in list
   - ✅ Click proposal to select
   - ✅ Review metadata (ID, timestamp, sandbox_id)
   - ✅ Review execution log content
   - ✅ Review diff content (if any)
   - ✅ Test filtering (All/Pending/Accepted/Rejected)
   - ✅ Test Refresh button

4. **Test Accept Workflow**:
   - ✅ Click "Accept" button
   - ✅ Confirm dialog appears
   - ✅ Verify ROM marked dirty (save prompt)
   - ✅ File → Save ROM
   - ✅ Verify proposal status changes to "Accepted"

5. **Test Reject Workflow**:
   - ✅ Create another test proposal
   - ✅ Click "Reject" button
   - ✅ Confirm dialog appears
   - ✅ Verify status changes to "Rejected"
   - ✅ Verify sandbox ROM unchanged

6. **Test Delete Workflow**:
   - ✅ Create another test proposal
   - ✅ Click "Delete" button
   - ✅ Confirm dialog appears
   - ✅ Verify proposal removed from list
   - ✅ Verify files cleaned up from disk

**Success Criteria**:
- All workflows complete without crashes
- ROM merging works correctly
- Status updates persist across sessions
- UI responsive and intuitive

**Known Issues to Document**:
- Any UX friction points
- Performance concerns with large diffs
- Edge cases that need handling

#### 1.3. Real Widget Testing (60 min)
**Goal**: Test GUI automation with actual YAZE widgets

**Workflow 1: Open Overworld Editor**:
```bash
# Start YAZE with test harness
./build-grpc-test/bin/yaze.app/Contents/MacOS/yaze \
  --enable_test_harness \
  --test_harness_port=50052 \
  --rom_file=assets/zelda3.sfc &

# Wait for startup
sleep 2

# Test workflow
grpcurl -plaintext -import-path src/app/core/proto -proto imgui_test_harness.proto \
  -d '{"target":"button:Overworld","type":"LEFT"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Click

grpcurl -plaintext -import-path src/app/core/proto -proto imgui_test_harness.proto \
  -d '{"condition":"window_visible:Overworld Editor","timeout_ms":5000}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Wait

grpcurl -plaintext -import-path src/app/core/proto -proto imgui_test_harness.proto \
  -d '{"condition":"visible:Overworld Editor"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Assert
```

**Workflow 2: Open Dungeon Editor**:
- Click "button:Dungeon"
- Wait "window_visible:Dungeon Editor"
- Assert "visible:Dungeon Editor"

**Workflow 3: Type in Input Field** (if applicable):
- Click "input:FieldName"
- Type text with clear_first
- Assert text_contains (partial implementation)

**Success Criteria**:
- All real widgets respond to automation
- Timeouts work correctly (5s default)
- Error messages helpful when widgets not found
- No crashes or hangs during automation

**Document**:
- Widget naming conventions (button:Name, window:Name, input:Name)
- Common timeout values needed
- Edge cases (disabled buttons, hidden windows, etc.)

#### 1.4. Documentation Updates (30 min)
**Goal**: Capture learnings and update guides

**Files to Update**:
1. **IT-01-QUICKSTART.md**:
   - Add real widget examples
   - Document common workflows
   - Add troubleshooting for real scenarios

2. **E6-z3ed-implementation-plan.md**:
   - Mark Priority 1 as complete
   - Add lessons learned section
   - Update known limitations

3. **STATE_SUMMARY_2025-10-02.md**:
   - Add E2E validation results
   - Update status metrics
   - Document performance characteristics

**Success Criteria**:
- New users can follow guides without getting stuck
- Common issues documented with solutions
- Real-world examples added

---

## Priority 2: CLI Agent Test Command (IT-02) 📋

**Goal**: Natural language prompt → automated GUI test workflow  
**Time Estimate**: 4-6 hours  
**Status**: Ready to start after Priority 1  
**Blocking Dependency**: Priority 1 completion

### Why This Next?
- Enables AI agents to drive YAZE GUI automatically
- Makes GUI automation accessible via simple CLI commands
- Provides foundation for complex multi-step workflows
- Demonstrates value of IT-01 infrastructure

### Design Overview

```
User Input:
  z3ed agent test --prompt "Open Overworld editor and verify it loads"

Workflow:
  1. Parse prompt → identify intent (open editor, verify visibility)
  2. Generate RPC sequence:
     - Click "button:Overworld"
     - Wait "window_visible:Overworld Editor" (5s timeout)
     - Assert "visible:Overworld Editor"
  3. Execute RPCs via gRPC client
  4. Capture results and report
  5. Optional: Screenshot for LLM feedback

Output:
  ✓ Clicked button:Overworld (85ms)
  ✓ Waited for window:Overworld Editor (1234ms)
  ✓ Asserted visible:Overworld Editor (12ms)
  
  Test passed in 1.331s
```

### Implementation Tasks

#### 2.1. Create gRPC Client Library (2 hours)
**Files**:
- `src/cli/service/gui_automation_client.h`
- `src/cli/service/gui_automation_client.cc`

**Interface**:
```cpp
class GuiAutomationClient {
 public:
  static GuiAutomationClient& Instance();
  
  absl::Status Connect(const std::string& host, int port);
  absl::StatusOr<PingResponse> Ping(const std::string& message);
  absl::StatusOr<ClickResponse> Click(const std::string& target, ClickType type);
  absl::StatusOr<TypeResponse> Type(const std::string& target, 
                                    const std::string& text,
                                    bool clear_first);
  absl::StatusOr<WaitResponse> Wait(const std::string& condition,
                                    int timeout_ms,
                                    int poll_interval_ms);
  absl::StatusOr<AssertResponse> Assert(const std::string& condition);
  absl::StatusOr<ScreenshotResponse> Screenshot(const std::string& region,
                                                 const std::string& format);
  
 private:
  std::unique_ptr<yaze::test::ImGuiTestHarness::Stub> stub_;
};
```

**Implementation Notes**:
- Use gRPC C++ client API
- Handle connection errors gracefully
- Support timeout configuration
- Return structured results (not raw proto messages)

#### 2.2. Create Test Workflow Generator (1.5 hours)
**Files**:
- `src/cli/service/test_workflow_generator.h`
- `src/cli/service/test_workflow_generator.cc`

**Interface**:
```cpp
struct TestStep {
  enum Type { kClick, kType, kWait, kAssert, kScreenshot };
  Type type;
  std::string target;
  std::string value;
  int timeout_ms = 5000;
};

struct TestWorkflow {
  std::string description;
  std::vector<TestStep> steps;
};

class TestWorkflowGenerator {
 public:
  static absl::StatusOr<TestWorkflow> GenerateFromPrompt(
      const std::string& prompt);
  
 private:
  static absl::StatusOr<TestWorkflow> ParseSimplePrompt(
      const std::string& prompt);
  static absl::StatusOr<TestWorkflow> ParseComplexPrompt(
      const std::string& prompt);
};
```

**Supported Prompt Patterns**:
1. **Simple Open**: "Open Overworld editor"
   - Click "button:Overworld"
   - Wait "window_visible:Overworld Editor"

2. **Open and Verify**: "Open Dungeon editor and verify it loads"
   - Click "button:Dungeon"
   - Wait "window_visible:Dungeon Editor"
   - Assert "visible:Dungeon Editor"

3. **Type and Validate**: "Type 'zelda3.sfc' in filename input"
   - Click "input:Filename"
   - Type "zelda3.sfc" with clear_first
   - Assert "text_contains:Filename:zelda3.sfc"

4. **Multi-Step**: "Open Overworld, click tile, verify properties panel"
   - Click "button:Overworld"
   - Wait "window_visible:Overworld Editor"
   - Click "canvas:Overworld" (x, y coordinates)
   - Wait "window_visible:Properties"

**Implementation Strategy**:
- Start with simple regex/pattern matching
- Add more complex patterns iteratively
- Return error for unsupported prompts
- Suggest valid alternatives

#### 2.3. Implement `z3ed agent test` Command (1.5 hours)
**Files**:
- `src/cli/handlers/agent.cc` (add `HandleTestCommand`)
- Update `src/cli/modern_cli.cc` routing

**Command Interface**:
```bash
z3ed agent test --prompt "..." [--host localhost] [--port 50052] [--timeout 30s]
```

**Implementation**:
```cpp
absl::Status HandleTestCommand(const AgentOptions& options) {
  // 1. Parse prompt → workflow
  auto workflow_result = TestWorkflowGenerator::GenerateFromPrompt(
      options.prompt);
  if (!workflow_result.ok()) {
    return workflow_result.status();
  }
  TestWorkflow workflow = std::move(*workflow_result);
  
  // 2. Connect to test harness
  auto& client = GuiAutomationClient::Instance();
  auto status = client.Connect(options.host, options.port);
  if (!status.ok()) {
    return status;
  }
  
  // 3. Execute workflow steps
  for (const auto& step : workflow.steps) {
    auto result = ExecuteStep(client, step);
    if (!result.ok()) {
      return result;
    }
    PrintStepResult(step, *result);
  }
  
  std::cout << "\nTest passed!\n";
  return absl::OkStatus();
}
```

**Output Format**:
- Progress indicators for each step
- Execution time per step
- Success/failure status
- Error messages with context
- Final summary

#### 2.4. Testing and Documentation (1 hour)
**Test Cases**:
1. Simple open editor test
2. Multi-step workflow test
3. Timeout handling test
4. Connection error test
5. Invalid widget test

**Documentation**:
- Add IT-02 completion doc
- Update implementation plan
- Add examples to IT-01-QUICKSTART.md
- Update resource catalog with `agent test` command

**Success Criteria**:
- `z3ed agent test` works with 5+ different prompts
- Error messages helpful for debugging
- Documentation complete with examples
- Ready for AI agent integration

---

## Priority 3: Policy Evaluation Framework (AW-04) 📋

**Goal**: YAML-based constraint system for gating proposal acceptance  
**Time Estimate**: 6-8 hours  
**Status**: Can work in parallel with Priority 2  
**Blocking Dependency**: None (UI integration requires AW-03)

### Why This Matters?
- Prevents dangerous/unwanted changes from being accepted
- Enforces project-specific constraints (byte limits, bank restrictions)
- Requires test coverage before acceptance
- Provides audit trail for policy violations

### Design Overview

**Policy Configuration** (`.yaze/policies/agent.yaml`):
```yaml
version: 1.0
policies:
  # Test Requirements
  - name: require_tests
    type: test_requirement
    enabled: true
    severity: critical  # critical | warning | info
    rules:
      - test_suite: "overworld_rendering"
        min_pass_rate: 0.95
      - test_suite: "palette_integrity"
        min_pass_rate: 1.0
  
  # Change Constraints
  - name: limit_change_scope
    type: change_constraint
    enabled: true
    severity: critical
    rules:
      - max_bytes_changed: 10240  # 10KB limit
      - allowed_banks: [0x00, 0x01, 0x0E]  # Graphics banks only
      - forbidden_ranges:
          - start: 0xFFB0  # ROM header
            end: 0xFFFF
          - start: 0x0000  # System RAM
            end: 0x1FFF
  
  # Review Requirements
  - name: human_review_required
    type: review_requirement
    enabled: true
    severity: warning
    rules:
      - if: bytes_changed > 1024
        then: require_diff_review
      - if: commands_executed > 10
        then: require_log_review
      - if: new_files_created
        then: require_approval
  
  # CVE Checks
  - name: security_validation
    type: security_check
    enabled: true
    severity: critical
    rules:
      - check: no_known_cves
        message: "Dependencies must not have known CVEs"
      - check: checksum_valid
        message: "ROM checksum must be valid after changes"
```

### Implementation Tasks

#### 3.1. Policy Schema and Parser (2 hours)
**Files**:
- `src/cli/service/policy_evaluator.h`
- `src/cli/service/policy_evaluator.cc`
- `.yaze/policies/agent.yaml` (example)

**Data Structures**:
```cpp
enum class PolicySeverity { kCritical, kWarning, kInfo };
enum class PolicyType {
  kTestRequirement,
  kChangeConstraint,
  kReviewRequirement,
  kSecurityCheck
};

struct PolicyRule {
  std::string condition;
  std::string action;
  std::map<std::string, std::string> parameters;
};

struct Policy {
  std::string name;
  PolicyType type;
  PolicySeverity severity;
  bool enabled;
  std::vector<PolicyRule> rules;
};

struct PolicyViolation {
  std::string policy_name;
  PolicySeverity severity;
  std::string message;
  std::string actual_value;
  std::string expected_value;
};

struct PolicyResult {
  bool passed;
  std::vector<PolicyViolation> violations;
  
  bool HasCriticalViolations() const;
  bool HasWarnings() const;
};
```

**YAML Parsing**:
- Use `yaml-cpp` library (already in vcpkg)
- Parse policy file on startup
- Validate schema (version, required fields)
- Cache parsed policies in memory

#### 3.2. Policy Evaluation Engine (2.5 hours)
**Interface**:
```cpp
class PolicyEvaluator {
 public:
  static PolicyEvaluator& Instance();
  
  absl::Status LoadPolicies(const std::string& policy_dir = ".yaze/policies");
  absl::StatusOr<PolicyResult> EvaluateProposal(const std::string& proposal_id);
  
 private:
  absl::StatusOr<PolicyResult> EvaluateTestRequirements(
      const ProposalMetadata& proposal);
  absl::StatusOr<PolicyResult> EvaluateChangeConstraints(
      const ProposalMetadata& proposal);
  absl::StatusOr<PolicyResult> EvaluateReviewRequirements(
      const ProposalMetadata& proposal);
  absl::StatusOr<PolicyResult> EvaluateSecurityChecks(
      const ProposalMetadata& proposal);
  
  std::vector<Policy> policies_;
};
```

**Evaluation Logic**:
1. Load proposal metadata (bytes changed, commands executed, etc.)
2. Load proposal diff (for bank/range analysis)
3. For each enabled policy:
   - Evaluate all rules
   - Collect violations
   - Determine overall pass/fail
4. Return structured result

**Example Evaluations**:
- **Test Requirements**: Check if test results exist and meet thresholds
- **Change Constraints**: Analyze diff for byte count, bank ranges, forbidden areas
- **Review Requirements**: Check metadata (bytes, commands, files)
- **Security Checks**: Run ROM validation, checksum verification

#### 3.3. ProposalDrawer Integration (2 hours)
**Files**:
- `src/app/editor/system/proposal_drawer.cc` (update)

**UI Changes**:
1. **Add Policy Status Section** (in detail view):
   ```
   Policy Status: [✓ Passed | ⚠ Warnings | ⛔ Failed]
   
   Critical Issues:
     ⛔ Test pass rate 85% < 95% (overworld_rendering)
     ⛔ Forbidden range modified: 0xFFB0-0xFFFF (ROM header)
   
   Warnings:
     ⚠ 2048 bytes changed > 1024 (requires diff review)
   ```

2. **Gate Accept Button**:
   - Disable if critical violations exist
   - Show tooltip: "Accept blocked: 2 critical policy violations"
   - Enable override button (with confirmation + logging)

3. **Policy Override Dialog**:
   ```
   Override Policy Violations?
   
   This action will be logged for audit purposes.
   
   Violations:
     • Test pass rate below threshold
     • ROM header modified
   
   Reason (required): [___________________________]
   
   [Cancel] [Override and Accept]
   ```

**Integration Points**:
```cpp
void ProposalDrawer::DrawProposalDetail(const ProposalMetadata& proposal) {
  // ... existing metadata, diff, log sections ...
  
  // Add policy section
  ImGui::Separator();
  if (ImGui::CollapsingHeader("Policy Status", ImGuiTreeNodeFlags_DefaultOpen)) {
    DrawPolicyStatus(proposal.id);
  }
}

void ProposalDrawer::DrawPolicyStatus(const std::string& proposal_id) {
  auto& evaluator = PolicyEvaluator::Instance();
  auto result = evaluator.EvaluateProposal(proposal_id);
  
  if (!result.ok()) {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error evaluating policies");
    return;
  }
  
  const auto& policy_result = *result;
  
  // Show overall status
  if (policy_result.passed) {
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "✓ All policies passed");
  } else if (policy_result.HasCriticalViolations()) {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "⛔ Critical violations");
  } else {
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "⚠ Warnings present");
  }
  
  // List violations
  for (const auto& violation : policy_result.violations) {
    DrawViolation(violation);
  }
}

void ProposalDrawer::AcceptProposal(const std::string& proposal_id) {
  // Evaluate policies before accepting
  auto& evaluator = PolicyEvaluator::Instance();
  auto result = evaluator.EvaluateProposal(proposal_id);
  
  if (result.ok() && result->HasCriticalViolations()) {
    // Show override dialog instead of accepting directly
    show_policy_override_dialog_ = true;
    pending_accept_proposal_id_ = proposal_id;
    return;
  }
  
  // ... existing accept logic ...
}
```

#### 3.4. Testing and Documentation (1.5 hours)
**Test Cases**:
1. Valid proposal (all policies pass)
2. Test requirement violation
3. Change constraint violation
4. Multiple violations
5. Policy override workflow

**Documentation**:
- Create AW-04-POLICY-FRAMEWORK.md with:
  - Policy schema reference
  - Built-in policy examples
  - How to write custom policies
  - Override audit trail
- Update implementation plan
- Update ProposalDrawer documentation

**Success Criteria**:
- Policies loaded and evaluated correctly
- UI clearly shows policy status
- Accept button gated on critical violations
- Override workflow functional with logging
- Documentation complete

---

## Timeline Summary

**Week of Oct 2-8, 2025**:
- Days 1-2: Priority 1 (E2E Validation)
- Days 3-4: Priority 2 (CLI Agent Test)
- Days 5-7: Priority 3 (Policy Framework)

**Expected Completion**: October 8, 2025

**Next After This**:
- Windows cross-platform testing
- Screenshot implementation
- Production telemetry (opt-in)
- Advanced policy features

---

## Success Metrics

**By End of Week**:
- ✅ Complete proposal workflow validated end-to-end
- ✅ `z3ed agent test` command operational with 5+ prompt patterns
- ✅ Policy framework implemented and integrated
- ✅ Documentation updated for all new features
- ✅ Zero known blockers for production use

**Quality Bar**:
- All code builds cleanly on macOS ARM64
- No crashes or hangs in normal workflows
- Error messages helpful and actionable
- Documentation sufficient for new contributors
- Ready for Windows testing phase

---

**Last Updated**: October 2, 2025  
**Contributors**: @scawful, GitHub Copilot  
**License**: Same as YAZE (see ../../LICENSE)
