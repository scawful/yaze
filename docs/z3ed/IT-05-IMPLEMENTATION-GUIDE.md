# IT-05: T## Motivation

**Current Limitations**:
- ❌ Tests execute asynchronously with no way to query status
- ❌ Clients must poll blindly or give up early
- ❌ No visibility into test execution queue
- ❌ Results lost after test completion
- ❌ Can't track test history or identify flaky tests

**Why This Blocks AI Agent Autonomy**:

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

**Benefits After IT-05**:
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

### Step 1: Extend TestManager (2-3 hours)

#### 1.1 Add Test Execution Tracking

**File**: `src/app/core/test_manager.h`

```cpp
#include <map>
#include <vector>
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"

class TestManager {
 public:
  enum class TestStatus {
    UNKNOWN = 0,
    QUEUED = 1,
    RUNNING = 2,
    PASSED = 3,
    FAILED = 4,
    TIMEOUT = 5
  };
  
  struct TestExecution {
    std::string test_id;
    std::string name;
    std::string category;
    TestStatus status;
    absl::Time queued_at;
    absl::Time started_at;
    absl::Time completed_at;
    absl::Duration execution_time;
    std::string error_message;
    std::vector<std::string> assertion_failures;
    std::vector<std::string> logs;
    std::map<std::string, int32_t> metrics;
  };
  
  // NEW: Introspection API
  absl::StatusOr<TestExecution> GetTestStatus(const std::string& test_id);
  std::vector<TestExecution> ListTests(const std::string& category_filter = "");
  absl::StatusOr<TestExecution> GetTestResults(const std::string& test_id);
  
  // NEW: Recording test execution
  void RecordTestStart(const std::string& test_id, const std::string& name,
                       const std::string& category);
  void RecordTestComplete(const std::string& test_id, TestStatus status,
                          const std::string& error_message = "");
  void AddTestLog(const std::string& test_id, const std::string& log_entry);
  void AddTestMetric(const std::string& test_id, const std::string& key,
                     int32_t value);
  
 private:
  std::map<std::string, TestExecution> test_history_ ABSL_GUARDED_BY(history_mutex_);
  absl::Mutex history_mutex_;
  
  // Helper: Generate unique test ID
  std::string GenerateTestId(const std::string& prefix);
};
```

**File**: `src/app/core/test_manager.cc`

```cpp
#include "src/app/core/test_manager.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include <random>

std::string TestManager::GenerateTestId(const std::string& prefix) {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::uniform_int_distribution<> dis(10000000, 99999999);
  
  return absl::StrFormat("%s_%d", prefix, dis(gen));
}

void TestManager::RecordTestStart(const std::string& test_id,
                                  const std::string& name,
                                  const std::string& category) {
  absl::MutexLock lock(&history_mutex_);
  
  TestExecution& exec = test_history_[test_id];
  exec.test_id = test_id;
  exec.name = name;
  exec.category = category;
  exec.status = TestStatus::RUNNING;
  exec.started_at = absl::Now();
  exec.queued_at = exec.started_at;  // For now, no separate queue
}

void TestManager::RecordTestComplete(const std::string& test_id,
                                     TestStatus status,
                                     const std::string& error_message) {
  absl::MutexLock lock(&history_mutex_);
  
  auto it = test_history_.find(test_id);
  if (it == test_history_.end()) return;
  
  TestExecution& exec = it->second;
  exec.status = status;
  exec.completed_at = absl::Now();
  exec.execution_time = exec.completed_at - exec.started_at;
  exec.error_message = error_message;
}

void TestManager::AddTestLog(const std::string& test_id,
                             const std::string& log_entry) {
  absl::MutexLock lock(&history_mutex_);
  
  auto it = test_history_.find(test_id);
  if (it != test_history_.end()) {
    it->second.logs.push_back(log_entry);
  }
}

void TestManager::AddTestMetric(const std::string& test_id,
                                const std::string& key,
                                int32_t value) {
  absl::MutexLock lock(&history_mutex_);
  
  auto it = test_history_.find(test_id);
  if (it != test_history_.end()) {
    it->second.metrics[key] = value;
  }
}

absl::StatusOr<TestManager::TestExecution> TestManager::GetTestStatus(
    const std::string& test_id) {
  absl::MutexLock lock(&history_mutex_);
  
  auto it = test_history_.find(test_id);
  if (it == test_history_.end()) {
    return absl::NotFoundError(
        absl::StrFormat("Test ID '%s' not found", test_id));
  }
  
  return it->second;
}

std::vector<TestManager::TestExecution> TestManager::ListTests(
    const std::string& category_filter) {
  absl::MutexLock lock(&history_mutex_);
  
  std::vector<TestExecution> results;
  for (const auto& [id, exec] : test_history_) {
    if (category_filter.empty() || exec.category == category_filter) {
      results.push_back(exec);
    }
  }
  
  return results;
}

absl::StatusOr<TestManager::TestExecution> TestManager::GetTestResults(
    const std::string& test_id) {
  // Same as GetTestStatus for now
  return GetTestStatus(test_id);
}
```

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

### Step 2: Implement Introspection RPCs (2-3 hours)

**File**: `src/app/core/imgui_test_harness_service.cc`

```cpp
absl::Status ImGuiTestHarnessServiceImpl::GetTestStatus(
    const GetTestStatusRequest* request,
    GetTestStatusResponse* response) {
  
  auto status_or = test_manager_->GetTestStatus(request->test_id());
  if (!status_or.ok()) {
    response->set_status(GetTestStatusResponse::UNKNOWN);
    return absl::OkStatus();  // Not an RPC error, just test not found
  }
  
  const auto& exec = status_or.value();
  
  // Map internal status to proto status
  switch (exec.status) {
    case TestManager::TestStatus::QUEUED:
      response->set_status(GetTestStatusResponse::QUEUED);
      break;
    case TestManager::TestStatus::RUNNING:
      response->set_status(GetTestStatusResponse::RUNNING);
      break;
    case TestManager::TestStatus::PASSED:
      response->set_status(GetTestStatusResponse::PASSED);
      break;
    case TestManager::TestStatus::FAILED:
      response->set_status(GetTestStatusResponse::FAILED);
      break;
    case TestManager::TestStatus::TIMEOUT:
      response->set_status(GetTestStatusResponse::TIMEOUT);
      break;
    default:
      response->set_status(GetTestStatusResponse::UNKNOWN);
  }
  
  // Convert absl::Time to milliseconds since epoch
  response->set_queued_at_ms(absl::ToUnixMillis(exec.queued_at));
  response->set_started_at_ms(absl::ToUnixMillis(exec.started_at));
  response->set_completed_at_ms(absl::ToUnixMillis(exec.completed_at));
  response->set_execution_time_ms(absl::ToInt64Milliseconds(exec.execution_time));
  response->set_error_message(exec.error_message);
  
  for (const auto& failure : exec.assertion_failures) {
    response->add_assertion_failures(failure);
  }
  
  return absl::OkStatus();
}

absl::Status ImGuiTestHarnessServiceImpl::ListTests(
    const ListTestsRequest* request,
    ListTestsResponse* response) {
  
  auto tests = test_manager_->ListTests(request->category_filter());
  
  // TODO: Implement pagination if needed
  response->set_total_count(tests.size());
  
  for (const auto& exec : tests) {
    auto* test_info = response->add_tests();
    test_info->set_test_id(exec.test_id);
    test_info->set_name(exec.name);
    test_info->set_category(exec.category);
    test_info->set_last_run_timestamp_ms(absl::ToUnixMillis(exec.completed_at));
    test_info->set_total_runs(1);  // TODO: Track across multiple runs
    
    if (exec.status == TestManager::TestStatus::PASSED) {
      test_info->set_pass_count(1);
      test_info->set_fail_count(0);
    } else {
      test_info->set_pass_count(0);
      test_info->set_fail_count(1);
    }
    
    test_info->set_average_duration_ms(
        absl::ToInt64Milliseconds(exec.execution_time));
  }
  
  return absl::OkStatus();
}

absl::Status ImGuiTestHarnessServiceImpl::GetTestResults(
    const GetTestResultsRequest* request,
    GetTestResultsResponse* response) {
  
  auto status_or = test_manager_->GetTestResults(request->test_id());
  if (!status_or.ok()) {
    return absl::NotFoundError(
        absl::StrFormat("Test '%s' not found", request->test_id()));
  }
  
  const auto& exec = status_or.value();
  
  response->set_success(exec.status == TestManager::TestStatus::PASSED);
  response->set_test_name(exec.name);
  response->set_category(exec.category);
  response->set_executed_at_ms(absl::ToUnixMillis(exec.completed_at));
  response->set_duration_ms(absl::ToInt64Milliseconds(exec.execution_time));
  
  // Include logs if requested
  if (request->include_logs()) {
    for (const auto& log : exec.logs) {
      response->add_logs(log);
    }
  }
  
  // Add metrics
  for (const auto& [key, value] : exec.metrics) {
    (*response->mutable_metrics())[key] = value;
  }
  
  return absl::OkStatus();
}
```

### Step 3: CLI Integration (1-2 hours)

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

### Step 4: Testing & Validation (1 hour)

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

- [ ] All 3 new RPCs respond correctly
- [ ] Test IDs returned in Click/Type/Wait/Assert responses
- [ ] Status polling works with `--follow` flag
- [ ] Test history persists across multiple test runs
- [ ] CLI commands output clean YAML/JSON
- [ ] No memory leaks in test history tracking
- [ ] Thread-safe access to test history
- [ ] Documentation updated in E6-z3ed-reference.md

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
**Status**: Ready for implementation
