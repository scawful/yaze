#!/bin/bash
# End-to-end smoke test for test introspection CLI commands
# Requires YAZE to be built with gRPC support (build-grpc-test preset)

set -euo pipefail

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

Z3ED_BIN="./build-grpc-test/bin/z3ed"
YAZE_BIN="./build-grpc-test/bin/yaze.app/Contents/MacOS/yaze"
ROM_FILE="assets/zelda3.sfc"
TEST_PORT="${TEST_PORT:-50052}"
PROMPT="Open Overworld editor and verify it loads"
HOST="localhost"

STATUS_LOG="$(mktemp /tmp/z3ed_status_XXXX.log)"
RESULTS_LOG="$(mktemp /tmp/z3ed_results_XXXX.log)"
LIST_LOG="$(mktemp /tmp/z3ed_list_XXXX.log)"
RUN_LOG="$(mktemp /tmp/z3ed_run_XXXX.log)"

cleanup() {
  if [[ -n "${YAZE_PID:-}" ]]; then
    kill "${YAZE_PID}" 2>/dev/null || true
  fi
  rm -f "$STATUS_LOG" "$RESULTS_LOG" "$LIST_LOG" "$RUN_LOG"
}
trap cleanup EXIT

if [[ ! -x "$Z3ED_BIN" ]]; then
  echo -e "${RED}Error:${NC} z3ed binary not found at $Z3ED_BIN"
  echo "Build with: cmake --build build-grpc-test --target z3ed"
  exit 1
fi

if [[ ! -x "$YAZE_BIN" ]]; then
  echo -e "${RED}Error:${NC} YAZE binary not found at $YAZE_BIN"
  echo "Build with: cmake --build build-grpc-test --target yaze"
  exit 1
fi

if [[ ! -f "$ROM_FILE" ]]; then
  echo -e "${RED}Error:${NC} ROM file not found at $ROM_FILE"
  exit 1
fi

echo -e "${YELLOW}=== Test Harness Introspection E2E ===${NC}"

# Ensure no previous YAZE instance is running
killall yaze 2>/dev/null || true
sleep 1

echo -e "${BLUE}→ Starting YAZE (port $TEST_PORT)...${NC}"
"$YAZE_BIN" \
  --enable_test_harness \
  --test_harness_port="$TEST_PORT" \
  --rom_file="$ROM_FILE" &
YAZE_PID=$!

ready=0
for attempt in {1..20}; do
  if lsof -i ":$TEST_PORT" >/dev/null 2>&1; then
    ready=1
    break
  fi
  sleep 0.5
done

if [[ "$ready" -ne 1 ]]; then
  echo -e "${RED}Error:${NC} ImGuiTestHarness server did not start on port $TEST_PORT"
  exit 1
fi

echo -e "${GREEN}✓ Harness ready${NC}"

echo -e "${BLUE}→ Running agent test workflow: $PROMPT${NC}"
if ! "$Z3ED_BIN" agent test --prompt "$PROMPT" --host "$HOST" --port "$TEST_PORT" | tee "$RUN_LOG"; then
  echo -e "${RED}Error:${NC} agent test run failed"
  exit 1
fi

PRIMARY_TEST_ID=$(sed -n 's/.*Test ID: \([^][]*\).*/\1/p' "$RUN_LOG" | tail -n 1 | tr -d ' ]')
if [[ -z "$PRIMARY_TEST_ID" ]]; then
  echo -e "${RED}Error:${NC} Unable to extract test id from agent test output"
  exit 1
fi

echo -e "${GREEN}✓ Captured Test ID:${NC} $PRIMARY_TEST_ID"

echo -e "${BLUE}→ Checking status${NC}"
"$Z3ED_BIN" agent test status --test-id "$PRIMARY_TEST_ID" --host "$HOST" --port "$TEST_PORT" | tee "$STATUS_LOG"
if ! grep -q "Status: " "$STATUS_LOG"; then
  echo -e "${RED}Error:${NC} status command did not return a status"
  exit 1
fi

if grep -q "Status: PASSED" "$STATUS_LOG"; then
  echo -e "${GREEN}✓ Status indicates PASS${NC}"
else
  echo -e "${YELLOW}! Status is not PASSED (see $STATUS_LOG)${NC}"
fi

echo -e "${BLUE}→ Fetching detailed results (YAML)${NC}"
"$Z3ED_BIN" agent test results --test-id "$PRIMARY_TEST_ID" --include-logs --host "$HOST" --port "$TEST_PORT" | tee "$RESULTS_LOG"
if ! grep -q "success: " "$RESULTS_LOG"; then
  echo -e "${RED}Error:${NC} results command failed"
  exit 1
fi

echo -e "${BLUE}→ Listing recent grpc tests${NC}"
"$Z3ED_BIN" agent test list --category grpc --limit 5 --host "$HOST" --port "$TEST_PORT" | tee "$LIST_LOG"
if ! grep -q "Test ID:" "$LIST_LOG"; then
  echo -e "${RED}Error:${NC} list command returned no tests"
  exit 1
fi

echo -e "${GREEN}✓ Introspection commands completed successfully${NC}"

echo -e "${YELLOW}Artifacts:${NC}"
echo "  Status log:   $STATUS_LOG"
echo "  Results log:  $RESULTS_LOG"
echo "  List log:     $LIST_LOG"

echo -e "${GREEN}All checks passed!${NC}"
exit 0
