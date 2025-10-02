# IT-05: Test Introspection API – Implementation Guide

**Status (Oct 2, 2025)**: 🟡 *Server-side RPCs complete; CLI + E2E pending*

## Progress Snapshot

- ✅ Proto definitions and service stubs added for `GetTestStatus`, `ListTests`, `GetTestResults`.
- ✅ `TestManager` now records execution lifecycle, aggregates, logs, and metrics with thread-safe history trimming.
- ✅ `ImGuiTestHarnessServiceImpl` implements the three RPC handlers, including pagination and status conversion helpers.
- ⚠️ CLI wiring, automation client calls, and user-facing output still TODO.
- ⚠️ End-to-end validation script (`scripts/test_introspection_e2e.sh`) not yet authored.

**Current Limitations**:
- ❌ Tests execute asynchronously with no way to query status
- ❌ Clients must poll blindly or give up early
- ❌ No visibility into test execution queue
- ❌ Results lost after test completion
- ❌ Can't track test history or identify flaky tests

**Why This Blocks AI Agent Autonomy**

Without test introspection, **AI agents cannot implement closed-loop feedback**:

```
❌ BROKEN: AI Agent Without IT-05
1. AI generates commands: ["z3ed palette export ..."]
2. AI executes commands in sandbox
3. AI generates test: "Verify soldier is red"
4. AI runs test → Gets test_id
5. ??? AI has no way to check if test passed ???
6. AI presents proposal to user blindly
   (might be broken, AI doesn't know)

✅ WORKING: AI Agent With IT-05
1. AI generates commands
2. AI executes in sandbox
3. AI generates verification test
4. AI runs test → Gets test_id
5. AI polls: GetTestStatus(test_id)
6. Test FAILED? AI sees error + screenshot
7. AI adjusts strategy and retries
8. Test PASSED? AI presents successful proposal
```

**This is the difference between**:
- **Dumb automation**: Execute blindly, hope for the best
- **Intelligent agent**: Verify, learn, self-correct

**Benefits After IT-05**:
- ✅ AI agents can reliably poll for test completion
- ✅ AI agents can read detailed failure messages
- ✅ AI agents can implement retry logic with adjusted strategies
- ✅ CLI can show real-time progress bars
- ✅ Test history enables trend analysis (flaky tests, performance regressions)
- ✅ Foundation for test recording/replay (IT-07)
- ✅ **Enables autonomous agent operation**ion API - Implementation Guide

**Status**: 📋 Planned | Priority 1 | Time Estimate: 6-8 hours  
**Dependencies**: IT-01 Complete ✅, IT-02 Complete ✅  
**Blocking**: IT-06 (Widget Discovery needs introspection foundation)

## Overview

Add test introspection capabilities to enable clients to query test execution status, list available tests, and retrieve detailed results. This is critical for AI agents to reliably poll for test completion and make decisions based on results.

## Motivation

**Current Limitations**:
- ❌ Tests execute asynchronously with no way to query status
- ❌ Clients must poll blindly or give up early
- ❌ No visibility into test execution queue
- ❌ Results lost after test completion
- ❌ Can't track test history or identify flaky tests

**Benefits After IT-05**

- ✅ AI agents can reliably poll for test completion
- ✅ CLI can show real-time progress bars
- ✅ Test history enables trend analysis
- ✅ Foundation for test recording/replay (IT-07)

## Architecture

### New Service Components

```cpp
// src/app/core/test_manager.h
class TestManager {
  // Existing...
  
  // NEW: Test tracking
  struct TestExecution {
    std::string test_id;
    std::string name;
    std::string category;
    TestStatus status;  // QUEUED, RUNNING, PASSED, FAILED, TIMEOUT
    int64_t queued_at_ms;
    int64_t started_at_ms;
    int64_t completed_at_ms;
    int32_t execution_time_ms;
    std::string error_message;
    std::vector<std::string> assertion_failures;
    std::vector<std::string> logs;
  };
  
  // NEW: Test execution tracking
  absl::StatusOr<TestExecution> GetTestStatus(const std::string& test_id);
  std::vector<TestExecution> ListTests(const std::string& category_filter = "");
  absl::StatusOr<TestExecution> GetTestResults(const std::string& test_id);
  
 private:
  // NEW: Test execution history
  std::map<std::string, TestExecution> test_history_;
  absl::Mutex test_history_mutex_;  // Thread-safe access
};
```

### Proto Additions

```protobuf
// src/app/core/proto/imgui_test_harness.proto

// Add to service definition
service ImGuiTestHarness {
  // ... existing RPCs ...
  
  // NEW: Test introspection
  rpc GetTestStatus(GetTestStatusRequest) returns (GetTestStatusResponse);
  rpc ListTests(ListTestsRequest) returns (ListTestsResponse);
  rpc GetTestResults(GetTestResultsRequest) returns (GetTestResultsResponse);
}

// ============================================================================
// GetTestStatus - Query test execution state
// ============================================================================

message GetTestStatusRequest {
  string test_id = 1;  // Test ID from Click/Type/Wait/Assert response
}

message GetTestStatusResponse {
  enum Status {
    UNKNOWN = 0;   // Test ID not found
    QUEUED = 1;    // Waiting to execute
    RUNNING = 2;   // Currently executing
    PASSED = 3;    // Completed successfully
    FAILED = 4;    // Assertion failed or error
    TIMEOUT = 5;   // Exceeded timeout
  }
  
  Status status = 1;
  int64 queued_at_ms = 2;      // When test was queued
  int64 started_at_ms = 3;     // When test started (0 if not started)
  int64 completed_at_ms = 4;   // When test completed (0 if not complete)
  int32 execution_time_ms = 5; // Total execution time
  string error_message = 6;    // Error details if FAILED/TIMEOUT
  repeated string assertion_failures = 7;  // Failed assertion details
}

// ============================================================================
// ListTests - Enumerate available tests
// ============================================================================

message ListTestsRequest {
  string category_filter = 1;  // Optional: "grpc", "unit", "integration", "e2e"
  int32 page_size = 2;         // Number of results per page (default 100)
  string page_token = 3;       // Pagination token from previous response
}

message ListTestsResponse {
  repeated TestInfo tests = 1;
  string next_page_token = 2;  // Token for next page (empty if no more)
  int32 total_count = 3;       // Total number of matching tests
}

message TestInfo {
  string test_id = 1;           // Unique test identifier
  string name = 2;              // Human-readable test name
  string category = 3;          // Category: grpc, unit, integration, e2e
  int64 last_run_timestamp_ms = 4;  // When test last executed
  int32 total_runs = 5;         // Total number of executions
  int32 pass_count = 6;         // Number of successful runs
  int32 fail_count = 7;         // Number of failed runs
  int32 average_duration_ms = 8;  // Average execution time
}

// ============================================================================
// GetTestResults - Retrieve detailed results
// ============================================================================

message GetTestResultsRequest {
  string test_id = 1;
  bool include_logs = 2;  // Include full execution logs
}

message GetTestResultsResponse {
  bool success = 1;         // Overall test result
  string test_name = 2;
  string category = 3;
  int64 executed_at_ms = 4;
  int32 duration_ms = 5;
  
  // Detailed results
  repeated AssertionResult assertions = 6;
  repeated string logs = 7;  // If include_logs=true
  
  // Performance metrics
  map<string, int32> metrics = 8;  // e.g., "frame_count": 123
}

message AssertionResult {
  string description = 1;
  bool passed = 2;
  string expected_value = 3;
  string actual_value = 4;
  string error_message = 5;
}
```

## Implementation Steps

### Step 1: Extend TestManager (✔️ Completed)

**What changed**:
- Introduced `HarnessTestExecution`, `HarnessTestSummary`, and related enums in `test_manager.h`.
- Added registration, running, completion, log, and metric helpers with `absl::Mutex` guarding (`RegisterHarnessTest`, `MarkHarnessTestRunning`, `MarkHarnessTestCompleted`, etc.).
- Stored executions in `harness_history_` + `harness_aggregates_` with deque-based trimming to avoid unbounded growth.

**Where to look**:
- `src/app/test/test_manager.h` (see *Harness test introspection (IT-05)* section around `HarnessTestExecution`).
- `src/app/test/test_manager.cc` (functions `RegisterHarnessTest`, `MarkHarnessTestCompleted`, `AppendHarnessTestLog`, `GetHarnessTestExecution`, `ListHarnessTestSummaries`).

**Next touch-ups**:
- Consider persisting assertion metadata (expected/actual) so `GetTestResults` can populate richer `AssertionResult` entries.
- Decide on retention limit (`harness_history_limit_`) tuning once CLI consumption patterns are known.

#### 1.2 Update Existing RPC Handlers

**File**: `src/app/core/imgui_test_harness_service.cc`

Modify Click, Type, Wait, Assert handlers to record test execution:

```cpp
absl::Status ImGuiTestHarnessServiceImpl::Click(
    const ClickRequest* request, ClickResponse* response) {
  
  // Generate unique test ID
  std::string test_id = test_manager_->GenerateTestId("grpc_click");
  
  // Record test start
  test_manager_->RecordTestStart(
      test_id, 
      absl::StrFormat("Click: %s", request->target()),
      "grpc");
  
  // ... existing implementation ...
  
  // Record test completion
  if (success) {
    test_manager_->RecordTestComplete(test_id, TestManager::TestStatus::PASSED);
  } else {
    test_manager_->RecordTestComplete(
        test_id, TestManager::TestStatus::FAILED, error_message);
  }
  
  // Add test ID to response (requires proto update)
  response->set_test_id(test_id);
  
  return absl::OkStatus();
}
```

**Proto Update**: Add `test_id` field to all responses:

```protobuf
message ClickResponse {
  bool success = 1;
  string message = 2;
  int32 execution_time_ms = 3;
  string test_id = 4;  // NEW: Unique test identifier for introspection
}

// Repeat for TypeResponse, WaitResponse, AssertResponse
```

### Step 2: Implement Introspection RPCs (✔️ Completed)

**What changed**:
- Added helper utilities (`ConvertHarnessStatus`, `ToUnixMillisSafe`, `ClampDurationToInt32`) in `imgui_test_harness_service.cc`.
- Implemented `GetTestStatus`, `ListTests`, and `GetTestResults` with pagination, optional log inclusion, and structured metrics.mapping.
- Updated gRPC wrapper to surface new RPCs and translate Abseil status codes into gRPC codes.
- Ensured deque-backed `DynamicTestData` keep-alive remains bounded while reusing new tracking helpers.

**Where to look**:
- `src/app/core/imgui_test_harness_service.cc` (search for `GetTestStatus(`, `ListTests(`, `GetTestResults(`).
- `src/app/core/imgui_test_harness_service.h` (new method declarations).

**Follow-ups**:
- Expand `AssertionResult` population once `TestManager` captures structured expected/actual data.
- Evaluate pagination defaults (`page_size`, `page_token`) once CLI usage patterns are seen.

### Step 3: CLI Integration (🚧 TODO)

Goal: expose the new RPCs through `GuiAutomationClient` and user-facing `z3ed agent test` subcommands. The pseudo-code below illustrates the desired flow; implementation still pending.

**File**: `src/cli/handlers/agent.cc`

Add new CLI commands for test introspection:

```cpp
// z3ed agent test status --test-id <id> [--follow]
absl::Status HandleAgentTestStatus(const CommandOptions& options) {
  const std::string test_id = absl::GetFlag(FLAGS_test_id);
  const bool follow = absl::GetFlag(FLAGS_follow);
  
  GuiAutomationClient client("localhost", 50052);
  RETURN_IF_ERROR(client.Connect());
  
  while (true) {
    auto status_or = client.GetTestStatus(test_id);
    RETURN_IF_ERROR(status_or.status());
    
    const auto& status = status_or.value();
    
    // Print status
    std::cout << "Test ID: " << test_id << "\n";
    std::cout << "Status: " << StatusToString(status.status) << "\n";
    std::cout << "Execution Time: " << status.execution_time_ms << "ms\n";
    
    if (status.status == TestStatus::PASSED ||
        status.status == TestStatus::FAILED ||
        status.status == TestStatus::TIMEOUT) {
      break;  // Terminal state
    }
    
    if (!follow) break;
    
    // Poll every 500ms
    absl::SleepFor(absl::Milliseconds(500));
  }
  
  return absl::OkStatus();
}

// z3ed agent test results --test-id <id> [--format json] [--include-logs]
absl::Status HandleAgentTestResults(const CommandOptions& options) {
  const std::string test_id = absl::GetFlag(FLAGS_test_id);
  const std::string format = absl::GetFlag(FLAGS_format);
  const bool include_logs = absl::GetFlag(FLAGS_include_logs);
  
  GuiAutomationClient client("localhost", 50052);
  RETURN_IF_ERROR(client.Connect());
  
  auto results_or = client.GetTestResults(test_id, include_logs);
  RETURN_IF_ERROR(results_or.status());
  
  const auto& results = results_or.value();
  
  if (format == "json") {
    // Output JSON
    PrintTestResultsJson(results);
  } else {
    // Output YAML (default)
    PrintTestResultsYaml(results);
  }
  
  return absl::OkStatus();
}

// z3ed agent test list [--category <name>] [--status <filter>]
absl::Status HandleAgentTestList(const CommandOptions& options) {
  const std::string category = absl::GetFlag(FLAGS_category);
  const std::string status_filter = absl::GetFlag(FLAGS_status);
  
  GuiAutomationClient client("localhost", 50052);
  RETURN_IF_ERROR(client.Connect());
  
  auto tests_or = client.ListTests(category);
  RETURN_IF_ERROR(tests_or.status());
  
  const auto& tests = tests_or.value();
  
  // Print table
  std::cout << "=== Test List ===\n\n";
  std::cout << absl::StreamFormat("%-20s %-30s %-10s %-10s\n",
                                   "Test ID", "Name", "Category", "Status");
  std::cout << std::string(80, '-') << "\n";
  
  for (const auto& test : tests) {
    std::cout << absl::StreamFormat("%-20s %-30s %-10s %-10s\n",
                                     test.test_id, test.name, test.category,
                                     StatusToString(test.last_status));
  }
  
  return absl::OkStatus();
}
```

### Step 4: Testing & Validation (🚧 TODO)

#### Test Script: `scripts/test_introspection_e2e.sh`

```bash
#!/bin/bash
# Test introspection API

set -e

# Start YAZE
./build-grpc-test/bin/yaze.app/Contents/MacOS/yaze \
  --enable_test_harness \
  --test_harness_port=50052 \
  --rom_file=assets/zelda3.sfc &

YAZE_PID=$!
sleep 3

# Test 1: Run a test and capture test ID
echo "Test 1: GetTestStatus"
TEST_ID=$(z3ed agent test --prompt "Open Overworld" --output json | jq -r '.test_id')
echo "Test ID: $TEST_ID"

# Test 2: Poll for status
echo "Test 2: Poll status"
z3ed agent test status --test-id $TEST_ID --follow

# Test 3: Get results
echo "Test 3: Get results"
z3ed agent test results --test-id $TEST_ID --format yaml --include-logs

# Test 4: List all tests
echo "Test 4: List tests"
z3ed agent test list --category grpc

# Cleanup
kill $YAZE_PID
```

## Success Criteria

- [x] All 3 new RPCs respond correctly
- [x] Test IDs returned in Click/Type/Wait/Assert responses
- [ ] Status polling works with `--follow` flag (CLI pending)
- [x] Test history persists across multiple test runs
- [ ] CLI commands output clean YAML/JSON
- [x] No memory leaks in test history tracking (bounded deque + pruning)
- [x] Thread-safe access to test history (mutex-protected)
- [ ] Documentation updated in `E6-z3ed-reference.md`

## Migration Guide

**For Existing Code**:
- No breaking changes - new RPCs only
- Existing tests continue to work
- Test ID field added to responses (backwards compatible)

**For CLI Users**:
```bash
# Old: Test runs, no way to check status
z3ed agent test --prompt "Open Overworld"

# New: Get test ID, poll for status
TEST_ID=$(z3ed agent test --prompt "Open Overworld" --output json | jq -r '.test_id')
z3ed agent test status --test-id $TEST_ID --follow
z3ed agent test results --test-id $TEST_ID
```

## Next Steps

After IT-05 completion:
1. **IT-06**: Widget Discovery API (uses introspection foundation)
2. **IT-07**: Test Recording & Replay (records test IDs and results)
3. **IT-08**: Enhanced Error Reporting (captures test context on failure)

## References

- **Proto Definition**: `src/app/core/proto/imgui_test_harness.proto`
- **Test Manager**: `src/app/core/test_manager.{h,cc}`
- **RPC Service**: `src/app/core/imgui_test_harness_service.{h,cc}`
- **CLI Handlers**: `src/cli/handlers/agent.cc`
- **Main Plan**: `docs/z3ed/E6-z3ed-implementation-plan.md`

---

**Author**: @scawful, GitHub Copilot  
**Created**: October 2, 2025  
**Status**: In progress (server-side complete; CLI + E2E pending)
