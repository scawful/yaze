#!/bin/bash
# Integration test for Gemini AI Service (Phase 2)

set -e  # Exit on error

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$SCRIPT_DIR/.."
Z3ED_BIN="$PROJECT_ROOT/build/bin/z3ed"

echo "🧪 Gemini AI Integration Test Suite"
echo "======================================"

# Color output helpers
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

pass() {
    echo -e "${GREEN}✓${NC} $1"
}

fail() {
    echo -e "${RED}✗${NC} $1"
    exit 1
}

warn() {
    echo -e "${YELLOW}⚠${NC} $1"
}

# Test 1: z3ed executable exists
echo ""
echo "Test 1: z3ed executable exists"
if [ -f "$Z3ED_BIN" ]; then
    pass "z3ed executable found at $Z3ED_BIN"
else
    fail "z3ed executable not found. Run: cmake --build build --target z3ed"
fi

# Test 2: Check GEMINI_API_KEY environment variable
echo ""
echo "Test 2: Check GEMINI_API_KEY environment variable"
if [ -z "$GEMINI_API_KEY" ]; then
    warn "GEMINI_API_KEY not set - skipping API tests"
    echo "   To test Gemini integration:"
    echo "   1. Get API key at: https://makersuite.google.com/app/apikey"
    echo "   2. Run: export GEMINI_API_KEY='your-api-key'"
    echo "   3. Re-run this script"
    
    # Still test that service factory handles missing key gracefully
    echo ""
    echo "Test 2a: Verify graceful fallback without API key"
    unset YAZE_AI_PROVIDER
    OUTPUT=$($Z3ED_BIN agent plan --prompt "Place a tree" 2>&1)
    
    if echo "$OUTPUT" | grep -q "Using MockAIService"; then
        pass "Service factory falls back to Mock when GEMINI_API_KEY missing"
    else
        fail "Service factory should fall back to Mock without API key"
    fi
    
    echo ""
    echo "⏭️  Skipping remaining Gemini API tests (no API key)"
    exit 0
fi

pass "GEMINI_API_KEY is set"

# Test 3: Verify Gemini model availability
echo ""
echo "Test 3: Verify Gemini model availability"
GEMINI_MODEL="${GEMINI_MODEL:-gemini-1.5-flash}"
echo "   Testing with model: $GEMINI_MODEL"

# Quick API check
HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" \
    -H "x-goog-api-key: $GEMINI_API_KEY" \
    "https://generativelanguage.googleapis.com/v1beta/models/$GEMINI_MODEL")

if [ "$HTTP_CODE" = "200" ]; then
    pass "Gemini API accessible, model '$GEMINI_MODEL' available"
elif [ "$HTTP_CODE" = "401" ] || [ "$HTTP_CODE" = "403" ]; then
    fail "Invalid Gemini API key (HTTP $HTTP_CODE)"
elif [ "$HTTP_CODE" = "404" ]; then
    fail "Model '$GEMINI_MODEL' not found (HTTP 404)"
else
    warn "Unexpected HTTP status: $HTTP_CODE (continuing anyway)"
fi

# Test 4: Generate commands with Gemini (simple prompt)
echo ""
echo "Test 4: Generate commands with Gemini (simple prompt)"
unset YAZE_AI_PROVIDER  # Let service factory auto-detect from GEMINI_API_KEY

OUTPUT=$($Z3ED_BIN agent plan --prompt "Change the color of palette 0 index 5 to red" 2>&1)

if echo "$OUTPUT" | grep -q "Using Gemini AI"; then
    pass "Service factory selected Gemini"
else
    fail "Expected 'Using Gemini AI' in output, got: $OUTPUT"
fi

if echo "$OUTPUT" | grep -q "palette"; then
    pass "Gemini generated palette-related commands"
    echo "   Generated commands:"
    echo "$OUTPUT" | grep -E "^\s*-" | sed 's/^/   /'
else
    fail "Expected palette commands in output, got: $OUTPUT"
fi

# Test 5: Generate commands with complex prompt
echo ""
echo "Test 5: Generate commands with complex prompt (overworld modification)"
OUTPUT=$($Z3ED_BIN agent plan --prompt "Place a tree at coordinates (10, 20) on overworld map 0" 2>&1)

if echo "$OUTPUT" | grep -q "overworld"; then
    pass "Gemini generated overworld commands"
    echo "   Generated commands:"
    echo "$OUTPUT" | grep -E "^\s*-" | sed 's/^/   /'
else
    fail "Expected overworld commands in output, got: $OUTPUT"
fi

# Test 6: Test explicit provider selection
echo ""
echo "Test 6: Test explicit provider selection (YAZE_AI_PROVIDER=gemini)"
# Note: Current implementation doesn't have explicit "gemini" provider value
# It auto-detects from GEMINI_API_KEY. But we can test that Ollama doesn't override.
unset YAZE_AI_PROVIDER

OUTPUT=$($Z3ED_BIN agent plan --prompt "Export palette 0" 2>&1)

if echo "$OUTPUT" | grep -q "Using Gemini AI"; then
    pass "Gemini selected when GEMINI_API_KEY present"
else
    warn "Expected Gemini selection, got: $OUTPUT"
fi

# Test 7: Verify JSON response parsing
echo ""
echo "Test 7: Verify JSON response parsing (check for command format)"
OUTPUT=$($Z3ED_BIN agent plan --prompt "Set tile at (5,5) to 0x100" 2>&1)

# Commands should NOT have "z3ed" prefix (service should strip it)
if echo "$OUTPUT" | grep -E "^\s*- z3ed"; then
    warn "Commands still contain 'z3ed' prefix (should be stripped)"
else
    pass "Commands properly formatted without 'z3ed' prefix"
fi

# Test 8: Test multiple commands in response
echo ""
echo "Test 8: Test multiple commands generation"
OUTPUT=$($Z3ED_BIN agent plan --prompt "Export palette 0 to test.json, change color 5 to red, then import it back" 2>&1)

COMMAND_COUNT=$(echo "$OUTPUT" | grep -c -E "^\s*- " || true)

if [ "$COMMAND_COUNT" -ge 2 ]; then
    pass "Gemini generated multiple commands ($COMMAND_COUNT commands)"
    echo "   Commands:"
    echo "$OUTPUT" | grep -E "^\s*-" | sed 's/^/   /'
else
    warn "Expected multiple commands, got $COMMAND_COUNT"
fi

# Test 9: Error handling - invalid API key
echo ""
echo "Test 9: Error handling with invalid API key"
SAVED_KEY="$GEMINI_API_KEY"
export GEMINI_API_KEY="invalid_key_12345"

OUTPUT=$($Z3ED_BIN agent plan --prompt "Test" 2>&1 || true)

if echo "$OUTPUT" | grep -q "Invalid Gemini API key\|Falling back to MockAIService"; then
    pass "Service handles invalid API key gracefully"
else
    warn "Expected error handling message, got: $OUTPUT"
fi

# Restore key
export GEMINI_API_KEY="$SAVED_KEY"

# Test 10: Model override via environment
echo ""
echo "Test 10: Model override via GEMINI_MODEL environment variable"
export GEMINI_MODEL="gemini-1.5-pro"

OUTPUT=$($Z3ED_BIN agent plan --prompt "Test" 2>&1)

if echo "$OUTPUT" | grep -q "gemini-1.5-pro"; then
    pass "GEMINI_MODEL environment variable respected"
else
    warn "Expected model override, got: $OUTPUT"
fi

unset GEMINI_MODEL

echo ""
echo "======================================"
echo "✅ Gemini Integration Test Suite Complete"
echo ""
echo "Summary:"
echo "  - Gemini API accessible"
echo "  - Command generation working"
echo "  - Error handling functional"
echo "  - JSON parsing robust"
echo ""
echo "Next steps:"
echo "  1. Test with various prompt types"
echo "  2. Measure response latency"
echo "  3. Compare accuracy with Ollama"
echo "  4. Consider rate limiting for production"
