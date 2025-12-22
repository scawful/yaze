#!/bin/bash
# Comprehensive test script for Ollama and Gemini AI providers with tool calling

set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

BUILD_DIR="${YAZE_BUILD_DIR:-./build}"
Z3ED="${BUILD_DIR}/bin/z3ed"
RESULTS_FILE="/tmp/z3ed_ai_test_results.txt"
USE_MOCK_ROM=true  # Set to false if you want to test with a real ROM
OLLAMA_MODEL="${OLLAMA_MODEL:-qwen2.5-coder:0.5b}"
OLLAMA_PID=""

echo "=========================================="
echo "  Z3ED AI Provider Test Suite"
echo "=========================================="
echo ""

# Clear results file
> "$RESULTS_FILE"

# Cleanup function
cleanup() {
    if [ -n "$OLLAMA_PID" ]; then
        echo ""
        echo "Stopping Ollama server (PID: $OLLAMA_PID)..."
        kill "$OLLAMA_PID" 2>/dev/null || true
        wait "$OLLAMA_PID" 2>/dev/null || true
    fi
}

# Register cleanup on exit
trap cleanup EXIT INT TERM

# --- Helper Functions ---

# Start Ollama server if not already running
start_ollama_server() {
    echo "Checking Ollama server status..."
    
    # Check if Ollama is already running
    if curl -s http://localhost:11434/api/tags > /dev/null 2>&1; then
        echo -e "${GREEN}✓ Ollama server already running${NC}"
        return 0
    fi
    
    # Check if ollama command exists
    if ! command -v ollama &> /dev/null; then
        echo -e "${YELLOW}⚠ Ollama command not found. Skipping Ollama tests.${NC}"
        echo "PATH: $PATH"
        echo "Which ollama: $(which ollama 2>&1 || echo 'not found')"
        return 1
    fi
    
    echo "Starting Ollama server..."
    echo "Ollama path: $(which ollama)"
    ollama serve > /tmp/ollama_server.log 2>&1 &
    OLLAMA_PID=$!
    echo "Ollama PID: $OLLAMA_PID"
    
    # Wait for server to be ready (max 60 seconds for CI)
    local max_wait=60
    local waited=0
    echo -n "Waiting for Ollama server to start"
    while [ $waited -lt $max_wait ]; do
        if curl -s http://localhost:11434/api/tags > /dev/null 2>&1; then
            echo ""
            echo -e "${GREEN}✓ Ollama server started (PID: $OLLAMA_PID) after ${waited}s${NC}"
            return 0
        fi
        echo -n "."
        sleep 1
        waited=$((waited + 1))
        
        # Check if process is still alive
        if ! kill -0 "$OLLAMA_PID" 2>/dev/null; then
            echo ""
            echo -e "${RED}✗ Ollama server process died${NC}"
            echo "Last 20 lines of server log:"
            tail -20 /tmp/ollama_server.log || echo "No log available"
            return 1
        fi
    done
    
    echo ""
    echo -e "${RED}✗ Ollama server failed to start within ${max_wait}s${NC}"
    echo "Last 20 lines of server log:"
    tail -20 /tmp/ollama_server.log || echo "No log available"
    return 1
}

# Ensure Ollama model is available
setup_ollama_model() {
    local model="$1"
    echo "Checking for Ollama model: $model"
    
    echo "Current models:"
    ollama list || echo "Failed to list models"
    
    if ollama list | grep -q "${model%:*}"; then
        echo -e "${GREEN}✓ Model $model already available${NC}"
        return 0
    fi
    
    echo "Pulling Ollama model: $model (this may take a while)..."
    echo "This is required for first-time setup in CI"
    if ollama pull "$model"; then
        echo -e "${GREEN}✓ Model $model pulled successfully${NC}"
        return 0
    else
        echo -e "${RED}✗ Failed to pull model $model${NC}"
        return 1
    fi
}

# --- Pre-flight Checks ---

if [ -z "$1" ]; then
  echo "❌ Error: No AI provider specified."
  echo "Usage: $0 <ollama|gemini|mock>"
  echo ""
  echo "Environment Variables:"
  echo "  OLLAMA_MODEL - Ollama model to use (default: qwen2.5-coder:0.5b)"
  echo "  GEMINI_API_KEY - Required for Gemini provider"
  echo ""
  echo "Examples:"
  echo "  $0 ollama                                    # Use Ollama with default model"
  echo "  OLLAMA_MODEL=llama3:8b $0 ollama            # Use Ollama with llama3"
  echo "  GEMINI_API_KEY=xyz $0 gemini                # Use Gemini"
  exit 1
fi
PROVIDER=$1
echo "✅ Provider: $PROVIDER"

# Check for curl (needed for Ollama health checks)
if [ "$PROVIDER" == "ollama" ]; then
    if ! command -v curl &> /dev/null; then
        echo -e "${RED}✗ curl command not found (required for Ollama)${NC}"
        exit 1
    fi
    echo "✅ curl available"
fi

# Check binary exists
if [ ! -f "$Z3ED" ]; then
    echo -e "${RED}✗ z3ed binary not found at: $Z3ED${NC}"
    echo "Run: cmake --build $BUILD_DIR"
    exit 1
fi
echo "✅ z3ed binary found"

# Set ROM flags based on mode
if [ "$USE_MOCK_ROM" = true ]; then
    ROM_FLAGS="--mock-rom"
    echo "✅ Using mock ROM mode (no ROM file required)"
else
    ROM="assets/zelda3.sfc"
    if [ ! -f "$ROM" ]; then
        echo -e "${RED}✗ ROM file not found: $ROM${NC}"
        echo "Tip: Use mock ROM mode by setting USE_MOCK_ROM=true"
        exit 1
    fi
    ROM_FLAGS="--rom=\"$ROM\""
    echo "✅ Real ROM found: $ROM"
fi

# Verify z3ed can execute
if "$Z3ED" --help > /dev/null 2>&1; then
    echo "✅ z3ed executable works"
else
    echo "${RED}✗ z3ed failed to execute${NC}"
    exit 1
fi

# Setup Ollama if needed
OLLAMA_AVAILABLE=false
if [ "$PROVIDER" == "ollama" ] || [ -z "$PROVIDER" ]; then
    if start_ollama_server; then
        if setup_ollama_model "$OLLAMA_MODEL"; then
            OLLAMA_AVAILABLE=true
            echo -e "${GREEN}✓ Ollama ready with model: $OLLAMA_MODEL${NC}"
        else
            echo -e "${YELLOW}⚠ Ollama server running but model setup failed${NC}"
        fi
    else
        echo -e "${YELLOW}⚠ Ollama server not available${NC}"
    fi
fi

# Test Gemini availability
GEMINI_AVAILABLE=false
if [ -n "$GEMINI_API_KEY" ]; then
    GEMINI_AVAILABLE=true
    echo -e "${GREEN}✓ Gemini API key configured${NC}"
else
    echo -e "${YELLOW}⚠ Gemini API key not set${NC}"
fi

if [ "$PROVIDER" == "ollama" ] && [ "$OLLAMA_AVAILABLE" = false ]; then
    echo -e "${RED}✗ Exiting: Ollama provider requested but not available.${NC}"
    exit 1
fi

if [ "$PROVIDER" == "gemini" ] && [ "$GEMINI_AVAILABLE" = false ]; then
    echo -e "${RED}✗ Exiting: Gemini provider requested but GEMINI_API_KEY is not set.${NC}"
    exit 1
fi

# --- Run Test Suite ---

# Test function
run_test() {
    local test_name="$1"
    local provider="$2"
    local query="$3"
    local expected_pattern="$4"
    local extra_args="$5"
    
    echo "=========================================="
    echo "  Test: $test_name"
    echo "  Provider: $provider"
    echo "=========================================="
    echo ""
    echo "Query: $query"
    echo ""
    
    local provider_args="$extra_args"
    if [ "$provider" == "ollama" ]; then
        provider_args="--ai_model=\"$OLLAMA_MODEL\" $provider_args"
    fi
    local cmd="$Z3ED agent simple-chat \"$query\" $ROM_FLAGS --ai_provider=$provider $provider_args"
    echo "Running: $cmd"
    echo ""
    
    local output
    local exit_code=0
    output=$($cmd 2>&1) || exit_code=$?
    
    echo "$output"
    echo ""
    
    # Check for expected patterns
    local result="UNKNOWN"
    if [ $exit_code -ne 0 ]; then
        result="FAILED (exit code: $exit_code)"
    elif echo "$output" | grep -qi "$expected_pattern"; then
        result="PASSED"
        echo -e "${GREEN}✓ Response contains expected pattern: '$expected_pattern'${NC}"
    else
        result="FAILED (pattern not found)"
        echo -e "${YELLOW}⚠ Response missing expected pattern: '$expected_pattern'${NC}"
    fi
    
    # Check for error indicators
    if echo "$output" | grep -qi "error\|failed\|infinite loop"; then
        result="FAILED (error detected)"
        echo -e "${RED}✗ Error detected in output${NC}"
    fi
    
    # Record result
    echo "$test_name | $provider | $result" >> "$RESULTS_FILE"
    echo ""
    echo -e "${BLUE}Result: $result${NC}"
    echo ""
    
    sleep 2  # Avoid rate limiting
}

# Test Suite

if [ "$OLLAMA_AVAILABLE" = true ]; then
    echo ""
    echo "=========================================="
    echo "  OLLAMA TESTS"
    echo "=========================================="
    echo ""

    run_test "Ollama: Simple Question" "ollama" \
        "What dungeons are in this ROM?" \
        "dungeon\|palace\|castle"
    
    run_test "Ollama: Sprite Query" "ollama" \
        "What sprites are in room 0?" \
        "sprite\|room"
    
    run_test "Ollama: Tile Search" "ollama" \
        "Where can I find trees in the overworld?" \
        "tree\|0x02E\|map\|coordinate"
    
    run_test "Ollama: Map Description" "ollama" \
        "Describe overworld map 0" \
        "light world\|map\|overworld"
    
    run_test "Ollama: Warp List" "ollama" \
        "List the warps in the Light World" \
        "warp\|entrance\|exit"
fi

if [ "$GEMINI_AVAILABLE" = true ]; then
    echo ""
    echo "=========================================="
    echo "  GEMINI TESTS"
    echo "=========================================="
    echo ""
    
    run_test "Gemini: Simple Question" "gemini" \
        "What dungeons are in this ROM?" \
        "dungeon\|palace\|castle" \
        "--gemini_api_key=\"$GEMINI_API_KEY\""
    
    run_test "Gemini: Sprite Query" "gemini" \
        "What sprites are in room 0?" \
        "sprite\|room" \
        "--gemini_api_key=\"$GEMINI_API_KEY\""
    
    run_test "Gemini: Tile Search" "gemini" \
        "Where can I find trees in the overworld?" \
        "tree\|0x02E\|map\|coordinate" \
        "--gemini_api_key=\"$GEMINI_API_KEY\""
    
    run_test "Gemini: Map Description" "gemini" \
        "Describe overworld map 0" \
        "light world\|map\|overworld" \
        "--gemini_api_key=\"$GEMINI_API_KEY\""
    
    run_test "Gemini: Warp List" "gemini" \
        "List the warps in the Light World" \
        "warp\|entrance\|exit" \
        "--gemini_api_key=\"$GEMINI_API_KEY\""
fi

echo ""
echo "=========================================="
echo "  TEST SUMMARY"
echo "=========================================="
echo ""

if [ -f "$RESULTS_FILE" ]; then
    cat "$RESULTS_FILE"
    echo ""
    
    total=$(wc -l < "$RESULTS_FILE" | tr -d ' ')
    passed=$(grep -c "PASSED" "$RESULTS_FILE" || echo "0")
    failed=$(grep -c "FAILED" "$RESULTS_FILE" || echo "0")
    
    echo "Total Tests: $total"
    echo -e "${GREEN}Passed: $passed${NC}"
    echo -e "${RED}Failed: $failed${NC}"
    echo ""
    
    if [ "$passed" -eq "$total" ]; then
        echo -e "${GREEN}🎉 All tests passed!${NC}"
    elif [ "$passed" -gt 0 ]; then
        echo -e "${YELLOW}⚠ Some tests failed. Review output above.${NC}"
    else
        echo -e "${RED}✗ All tests failed. Check configuration.${NC}"
    fi
else
    echo -e "${RED}✗ No results file generated${NC}"
fi

echo ""
echo "=========================================="
echo "  Recommendations"
echo "=========================================="
echo ""
echo "If tests are failing:"
echo "  1. Check that the ROM is valid and loaded properly"
echo "  2. Verify tool definitions in prompt_catalogue.yaml"
echo "  3. Review system prompts in prompt_builder.cc"
echo "  4. Check AI provider connectivity and quotas"
echo "  5. Examine tool execution logs for errors"
echo ""
echo "For Ollama:"
echo "  - Try different models: OLLAMA_MODEL=llama3:8b $0 ollama"
echo "  - Default model: $OLLAMA_MODEL"
echo "  - Adjust temperature in ollama_ai_service.cc"
echo "  - Server logs: /tmp/ollama_server.log"
echo ""
echo "For Gemini:"
echo "  - Verify API key is valid"
echo "  - Check quota at: https://aistudio.google.com"
echo ""
echo "Environment Variables:"
echo "  - OLLAMA_MODEL: Set the Ollama model (default: qwen2.5-coder:latest)"
echo "  - GEMINI_API_KEY: Required for Gemini tests"
echo ""
echo "Results saved to: $RESULTS_FILE"
echo ""
