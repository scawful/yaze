#!/bin/bash
# End-to-end test script for ImGuiTestHarness gRPC service
# Tests all RPC methods to validate Phase 3 implementation

set -e  # Exit on error

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
YAZE_BIN="./build-grpc-test/bin/yaze.app/Contents/MacOS/yaze"
TEST_PORT=50052
PROTO_PATH="src/app/core/proto"
PROTO_FILE="imgui_test_harness.proto"
ROM_FILE="assets/zelda3.sfc"

echo -e "${YELLOW}=== ImGuiTestHarness E2E Test ===${NC}\n"

# Check if YAZE binary exists
if [ ! -f "$YAZE_BIN" ]; then
    echo -e "${RED}Error: YAZE binary not found at $YAZE_BIN${NC}"
    echo "Please build with: cmake --build build-grpc-test --target yaze"
    exit 1
fi

# Check if ROM file exists
if [ ! -f "$ROM_FILE" ]; then
    echo -e "${RED}Error: ROM file not found at $ROM_FILE${NC}"
    exit 1
fi

# Check if grpcurl is installed
if ! command -v grpcurl &> /dev/null; then
    echo -e "${RED}Error: grpcurl not found${NC}"
    echo "Install with: brew install grpcurl"
    exit 1
fi

# Kill any existing YAZE instances
echo -e "${YELLOW}Cleaning up existing YAZE instances...${NC}"
killall yaze 2>/dev/null || true
sleep 1

# Start YAZE in background
echo -e "${YELLOW}Starting YAZE with test harness...${NC}"
$YAZE_BIN \
  --enable_test_harness \
  --test_harness_port=$TEST_PORT \
  --rom_file=$ROM_FILE &

YAZE_PID=$!
echo "YAZE PID: $YAZE_PID"

# Wait for server to be ready
echo -e "${YELLOW}Waiting for server to start...${NC}"
sleep 3

# Check if server is running
if ! lsof -i :$TEST_PORT > /dev/null 2>&1; then
    echo -e "${RED}Error: Server not listening on port $TEST_PORT${NC}"
    kill $YAZE_PID 2>/dev/null || true
    exit 1
fi

echo -e "${GREEN}✓ Server started successfully${NC}\n"

# Test counter
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Helper function to run a test
run_test() {
    local test_name="$1"
    local rpc_method="$2"
    local request_data="$3"
    
    TESTS_RUN=$((TESTS_RUN + 1))
    echo -e "${YELLOW}Test $TESTS_RUN: $test_name${NC}"
    
    if grpcurl -plaintext \
        -import-path $PROTO_PATH \
        -proto $PROTO_FILE \
        -d "$request_data" \
        127.0.0.1:$TEST_PORT \
        yaze.test.ImGuiTestHarness/$rpc_method 2>&1 | tee /tmp/grpc_test_output.txt; then
        
        # Check for success in response
        if grep -q '"success":.*true' /tmp/grpc_test_output.txt || \
           grep -q '"message":.*"Pong' /tmp/grpc_test_output.txt || \
           grep -q 'yazeVersion' /tmp/grpc_test_output.txt; then
            echo -e "${GREEN}✓ PASSED${NC}\n"
            TESTS_PASSED=$((TESTS_PASSED + 1))
        else
            echo -e "${RED}✗ FAILED (unexpected response)${NC}\n"
            TESTS_FAILED=$((TESTS_FAILED + 1))
        fi
    else
        echo -e "${RED}✗ FAILED (connection/RPC error)${NC}\n"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
}

# Run all tests
echo -e "${YELLOW}=== Running RPC Tests ===${NC}\n"

# 1. Ping - Health Check
run_test "Ping (Health Check)" "Ping" '{"message":"test"}'

# 2. Click - Button Click (will fail if button doesn't exist, but RPC should work)
run_test "Click (Button)" "Click" '{"target":"button:TestButton","type":"LEFT"}'

# 3. Type - Text Input (will fail if input doesn't exist, but RPC should work)
run_test "Type (Text Input)" "Type" '{"target":"input:TestInput","text":"test","clear_first":true}'

# 4. Wait - Window Visible (Main Window should exist)
run_test "Wait (Window Visible)" "Wait" '{"condition":"window_visible:Main Window","timeout_ms":2000,"poll_interval_ms":100}'

# 5. Assert - Window Visible (Main Window should exist)
run_test "Assert (Window Visible)" "Assert" '{"condition":"visible:Main Window"}'

# 6. Screenshot - Not Implemented
run_test "Screenshot (Not Implemented)" "Screenshot" '{"region":"full","format":"PNG"}'

# Summary
echo -e "${YELLOW}=== Test Summary ===${NC}"
echo "Tests Run:    $TESTS_RUN"
echo -e "${GREEN}Tests Passed: $TESTS_PASSED${NC}"
if [ $TESTS_FAILED -gt 0 ]; then
    echo -e "${RED}Tests Failed: $TESTS_FAILED${NC}"
fi
echo ""

# Cleanup
echo -e "${YELLOW}Cleaning up...${NC}"
kill $YAZE_PID 2>/dev/null || true
rm -f /tmp/grpc_test_output.txt
sleep 1

# Exit with appropriate code
if [ $TESTS_FAILED -gt 0 ]; then
    echo -e "${RED}Some tests failed${NC}"
    exit 1
else
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
fi
