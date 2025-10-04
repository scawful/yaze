#!/bin/bash

# Comprehensive test suite for the z3ed AI Agent.
# This script consolidates multiple older test scripts into one.
#
# Usage: ./scripts/agent_test_suite.sh <provider>
# provider: ollama, gemini, or mock

set -e # Exit immediately if a command exits with a non-zero status.

# --- Configuration ---
Z3ED_BIN="/Users/scawful/Code/yaze/build_test/bin/z3ed"
ROM_PATH="/Users/scawful/Code/yaze/assets/zelda3.sfc"
TEST_DIR="/Users/scawful/Code/yaze/assets/agent"
TEST_FILES=(
  "context_and_followup.txt"
  "complex_command_generation.txt"
  "error_handling_and_edge_cases.txt"
)

# --- Helper Functions ---
print_header() {
  echo ""
  echo "================================================="
  echo "$1"
  echo "================================================="
}

# --- Pre-flight Checks ---
print_header "Performing Pre-flight Checks"

if [ -z "$1" ]; then
  echo "❌ Error: No AI provider specified."
  echo "Usage: $0 <ollama|gemini|mock>"
  exit 1
fi
PROVIDER=$1
echo "✅ Provider: $PROVIDER"

if [ ! -f "$Z3ED_BIN" ]; then
  echo "❌ Error: z3ed binary not found at $Z3ED_BIN"
  echo "Please build the project first (e.g., in build_test)."
  exit 1
fi
echo "✅ z3ed binary found."

if [ ! -f "$ROM_PATH" ]; then
  echo "❌ Error: ROM not found at $ROM_PATH"
  exit 1
fi
echo "✅ ROM file found."

if [ "$PROVIDER" == "gemini" ] && [ -z "$GEMINI_API_KEY" ]; then
  echo "❌ Error: GEMINI_API_KEY environment variable is not set."
  echo "Please set it to your Gemini API key to run this test."
  exit 1
fi
if [ "$PROVIDER" == "gemini" ]; then
    echo "✅ GEMINI_API_KEY is set."
fi

if [ "$PROVIDER" == "ollama" ]; then
    if ! pgrep -x "Ollama" > /dev/null && ! pgrep -x "ollama" > /dev/null; then
        echo "⚠️ Warning: Ollama server process not found. The script might fail if it's not running."
    else
        echo "✅ Ollama server process found."
    fi
fi

# --- Run Test Suite ---
for test_file in "${TEST_FILES[@]}"; do
  print_header "Running Test File: $test_file (Provider: $PROVIDER)"
  FULL_TEST_PATH="$TEST_DIR/$test_file"
  
  if [ ! -f "$FULL_TEST_PATH" ]; then
    echo "❌ Error: Test file not found: $FULL_TEST_PATH"
    continue
  fi

  # Construct the command. Use --quiet for cleaner test logs.
  COMMAND="$Z3ED_BIN agent simple-chat --file=$FULL_TEST_PATH --rom=$ROM_PATH --ai_provider=$PROVIDER --quiet"
  
  echo "Executing command..."
  echo "--- Agent Output for $test_file ---"
  
  # Execute the command and print its output
  eval $COMMAND
  
  echo "--- Test Complete ---"
  echo ""
done

print_header "✅ All tests completed successfully!"
