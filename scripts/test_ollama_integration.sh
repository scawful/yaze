#!/bin/bash
# Test script for Ollama AI service integration
# This script validates Phase 1 implementation

set -e

echo "🧪 Testing Ollama AI Service Integration (Phase 1)"
echo "=================================================="
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

TESTS_PASSED=0
TESTS_FAILED=0

# Helper functions
pass_test() {
    echo -e "${GREEN}✓ PASS:${NC} $1"
    ((TESTS_PASSED++))
}

fail_test() {
    echo -e "${RED}✗ FAIL:${NC} $1"
    ((TESTS_FAILED++))
}

info() {
    echo -e "${BLUE}ℹ${NC} $1"
}

# Test 1: Check if z3ed built successfully
echo "Test 1: z3ed executable exists"
if [ -f "./build/bin/z3ed" ]; then
    pass_test "z3ed executable found"
else
    fail_test "z3ed executable not found"
    exit 1
fi
echo ""

# Test 2: Test MockAIService fallback (no LLM configured)
echo "Test 2: MockAIService fallback"
unset YAZE_AI_PROVIDER
unset GEMINI_API_KEY
unset CLAUDE_API_KEY

OUTPUT=$(./build/bin/z3ed agent plan --prompt "Place a tree" 2>&1 || true)
if echo "$OUTPUT" | grep -q "Using MockAIService"; then
    pass_test "MockAIService activated when no LLM configured"
    if echo "$OUTPUT" | grep -q "AI Agent Plan:"; then
        pass_test "MockAIService generated commands"
    fi
else
    fail_test "MockAIService fallback not working"
fi
echo ""

# Test 3: Test Ollama provider selection (without server)
echo "Test 3: Ollama provider selection (without server running)"
export YAZE_AI_PROVIDER=ollama

OUTPUT=$(./build/bin/z3ed agent plan --prompt "Validate ROM" 2>&1 || true)
if echo "$OUTPUT" | grep -q "Ollama unavailable"; then
    pass_test "Ollama health check detected unavailable server"
    if echo "$OUTPUT" | grep -q "Falling back to MockAIService"; then
        pass_test "Graceful fallback to MockAIService"
    else
        fail_test "Did not fall back to MockAIService"
    fi
else
    info "Note: If Ollama is running, this test will pass differently"
fi
echo ""

# Test 4: Check if Ollama is installed
echo "Test 4: Ollama installation check"
if command -v ollama &> /dev/null; then
    pass_test "Ollama is installed"
    
    # Test 5: Check if Ollama server is running
    echo ""
    echo "Test 5: Ollama server availability"
    if curl -s http://localhost:11434/api/tags > /dev/null 2>&1; then
        pass_test "Ollama server is running"
        
        # Test 6: Check for qwen2.5-coder model
        echo ""
        echo "Test 6: qwen2.5-coder:7b model availability"
        if ollama list | grep -q "qwen2.5-coder:7b"; then
            pass_test "Recommended model is available"
            
            # Test 7: End-to-end test with Ollama
            echo ""
            echo "Test 7: End-to-end LLM command generation"
            export YAZE_AI_PROVIDER=ollama
            export OLLAMA_MODEL=qwen2.5-coder:7b
            
            info "Testing: 'agent plan --prompt \"Validate the ROM\"'"
            OUTPUT=$(./build/bin/z3ed agent plan --prompt "Validate the ROM" 2>&1)
            
            if echo "$OUTPUT" | grep -q "Using Ollama AI"; then
                pass_test "Ollama AI service activated"
            else
                fail_test "Ollama AI service not activated"
            fi
            
            if echo "$OUTPUT" | grep -q "AI Agent Plan:"; then
                pass_test "Command generation completed"
                
                # Check if reasonable commands were generated
                if echo "$OUTPUT" | grep -q "rom"; then
                    pass_test "Generated ROM-related command"
                else
                    fail_test "Generated command doesn't seem ROM-related"
                fi
            else
                fail_test "No commands generated"
            fi
            
            echo ""
            echo "Generated output:"
            echo "---"
            echo "$OUTPUT"
            echo "---"
            
        else
            fail_test "qwen2.5-coder:7b not found"
            info "Install with: ollama pull qwen2.5-coder:7b"
        fi
    else
        fail_test "Ollama server not running"
        info "Start with: ollama serve"
    fi
else
    fail_test "Ollama not installed"
    info "Install with: brew install ollama (macOS)"
    info "Or visit: https://ollama.com/download"
fi

echo ""
echo "=================================================="
echo "Test Summary:"
echo -e "  ${GREEN}Passed: $TESTS_PASSED${NC}"
echo -e "  ${RED}Failed: $TESTS_FAILED${NC}"
echo ""

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}✓ All tests passed!${NC}"
    echo ""
    echo "Next steps:"
    echo "  1. If Ollama tests were skipped, install and configure:"
    echo "     brew install ollama"
    echo "     ollama serve &"
    echo "     ollama pull qwen2.5-coder:7b"
    echo ""
    echo "  2. Try the full agent workflow:"
    echo "     export YAZE_AI_PROVIDER=ollama"
    echo "     ./build/bin/z3ed agent run --prompt \"Validate ROM\" --rom zelda3.sfc --sandbox"
    echo ""
    echo "  3. Check the implementation checklist:"
    echo "     docs/z3ed/LLM-IMPLEMENTATION-CHECKLIST.md"
    exit 0
else
    echo -e "${RED}✗ Some tests failed${NC}"
    echo "Review the output above for details"
    exit 1
fi
