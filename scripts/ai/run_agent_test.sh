#!/usr/bin/env bash
# run_agent_test.sh: Headless Agent CI Harness
# Usage: ./run_agent_test.sh <test_script_path> [rom_path]

set -e

# Configuration
TEST_SCRIPT="$1"
ROM_PATH="${2:-$HOME/src/hobby/oracle-of-secrets/Roms/oos168x.sfc}"
INSTANCE_NAME="agent-test-$$"
TIMEOUT_SECONDS=300

if [[ -z "$TEST_SCRIPT" ]]; then
    echo "Usage: $0 <test_script_path> [rom_path]"
    exit 1
fi

if [[ ! -f "$ROM_PATH" ]]; then
    echo "Error: ROM not found at $ROM_PATH"
    exit 1
fi

echo "=== Agent Test Harness ==="
echo "Test: $TEST_SCRIPT"
echo "ROM: $ROM_PATH"
echo "Instance: $INSTANCE_NAME"

# 1. Cleanup previous runs (just in case)
# rm -rf "$HOME/.config/Mesen2-instances/$INSTANCE_NAME"

# 2. Launch Mesen in background
echo "Starting Mesen2 (Headless)..."
# We use --name to isolate config/saves
# We assume mesen-run is in PATH (from ~/bin)
MESEN_LOG="/tmp/mesen-$INSTANCE_NAME.log"
mesen-run --name="$INSTANCE_NAME" --headless "$ROM_PATH" > "$MESEN_LOG" 2>&1 &
WRAPPER_PID=$!

echo "Mesen Wrapper PID: $WRAPPER_PID"

# 3. Find the Socket
echo "Waiting for Socket..."
SOCKET_PATH=""
MESEN_PID=""
ATTEMPTS=0

while [[ -z "$SOCKET_PATH" && $ATTEMPTS -lt 20 ]]; do
    sleep 1
    # Find child process of wrapper (Mesen binary)
    # This works on macOS (pgrep -P)
    MESEN_PID=$(pgrep -P "$WRAPPER_PID" -n "Mesen")
    
    if [[ -n "$MESEN_PID" ]]; then
        CANDIDATE="/tmp/mesen2-${MESEN_PID}.sock"
        if [[ -S "$CANDIDATE" ]]; then
            SOCKET_PATH="$CANDIDATE"
        fi
    fi
    ((ATTEMPTS++))
done

if [[ -z "$SOCKET_PATH" ]]; then
    echo "Error: Timed out waiting for Mesen socket."
    echo "Logs:"
    cat "$MESEN_LOG"
    kill "$WRAPPER_PID" || true
    exit 1
fi

echo "Socket found: $SOCKET_PATH"

# 4. Run the Test
echo "=== Running Agent Test ==="
export MESEN2_SOCKET_PATH="$SOCKET_PATH"
export PYTHONUNBUFFERED=1

# Use a subshell to capture exit code properly
set +e
python3 "$TEST_SCRIPT"
TEST_EXIT_CODE=$?
set -e

# 5. Cleanup
echo "=== Teardown ==="
echo "Stopping Mesen..."
kill "$WRAPPER_PID" || true
# Wait for it to clean up child
wait "$WRAPPER_PID" 2>/dev/null || true

# Force kill child if still alive
if [[ -n "$MESEN_PID" ]]; then
    kill -9 "$MESEN_PID" 2>/dev/null || true
fi

# Cleanup temp files
rm -f "$MESEN_LOG"

if [[ $TEST_EXIT_CODE -eq 0 ]]; then
    echo "✅ Test Passed"
    exit 0
else
    echo "❌ Test Failed (Code: $TEST_EXIT_CODE)"
    exit $TEST_EXIT_CODE
fi
