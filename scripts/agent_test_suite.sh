#!/bin/bash
# Comprehensive test script for Ollama and Gemini AI providers with tool calling

set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

Z3ED="./build_test/bin/z3ed"
ROM="assets/zelda3.sfc"
RESULTS_FILE="/tmp/z3ed_ai_test_results.txt"

echo "=========================================="
echo "  Z3ED AI Provider Test Suite"
echo "=========================================="
echo ""

# Clear results file
> "$RESULTS_FILE"

# --- Pre-flight Checks ---
print_header "Performing Pre-flight Checks"

if [ -z "$1" ]; then
  echo "❌ Error: No AI provider specified."
  echo "Usage: $0 <ollama|gemini|mock>"
  exit 1
fi
PROVIDER=$1
echo "✅ Provider: $PROVIDER"

# Check binaries and files
for f in "$Z3ED_BIN" "$ROM_PATH" "$TEST_DIR/../prompt_catalogue.yaml" "$TEST_DIR/function_schemas.json"; do
    if [ ! -f "$f" ]; then
        echo -e "${RED}✗ Prerequisite file not found: $f${NC}"
        exit 1
    fi
done
echo "✅ Core binaries and files found."

# Verify schemas
if python3 -m json.tool "$TEST_DIR/function_schemas.json" > /dev/null 2>&1; then
    echo "✅ Function schemas JSON is valid."
else
    echo "${RED}✗ Invalid JSON in function_schemas.json${NC}"
    exit 1
fi

# Verify manual tool execution
if "$Z3ED_BIN" agent overworld-find-tile --tile 0x02E --format json --rom "$ROM_PATH" > /dev/null 2>&1; then
    echo "✅ Manual tool execution successful."
else
    echo "${RED}✗ Manual tool execution failed.${NC}"
    exit 1
fi

# Test Ollama availability
OLLAMA_AVAILABLE=false
if command -v ollama &> /dev/null && curl -s http://localhost:11434/api/tags > /dev/null 2>&1; then
    if ollama list | grep -q "qwen2.5-coder"; then
        OLLAMA_AVAILABLE=true
        echo -e "${GREEN}✓ Ollama available (qwen2.5-coder)${NC}"
    else
        echo -e "${YELLOW}⚠ Ollama available but qwen2.5-coder not found${NC}"
    fi
else
    echo -e "${YELLOW}⚠ Ollama not available${NC}"
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
    
    local cmd="$Z3ED agent simple-chat \"$query\" --rom=\"$ROM\" --ai_provider=$provider $extra_args"
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
    
    local total=$(wc -l < "$RESULTS_FILE" | tr -d ' ')
    local passed=$(grep -c "PASSED" "$RESULTS_FILE" || echo "0")
    local failed=$(grep -c "FAILED" "$RESULTS_FILE" || echo "0")
    
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
echo "  - Try different models: ollama pull llama3:8b"
echo "  - Adjust temperature in ollama_ai_service.cc"
echo ""
echo "For Gemini:"
echo "  - Verify API key is valid"
echo "  - Check quota at: https://aistudio.google.com"
echo ""
echo "Results saved to: $RESULTS_FILE"
echo ""
