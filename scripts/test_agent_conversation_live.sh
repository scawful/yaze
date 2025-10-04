#!/bin/bash
# Live testing script for conversational agent
# Tests agent function calling with real Ollama/Gemini backends

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
Z3ED="${PROJECT_ROOT}/build/bin/z3ed"
ROM_FILE="${PROJECT_ROOT}/assets/zelda3.sfc"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo "========================================="
echo "Live Conversational Agent Test"
echo "========================================="
echo ""

# Prerequisites check
if [ ! -f "$Z3ED" ]; then
    echo -e "${RED}✗ z3ed not found at $Z3ED${NC}"
    echo "Build with: cmake --build build --target z3ed"
    exit 1
fi

if [ ! -f "$ROM_FILE" ]; then
    echo -e "${RED}✗ ROM file not found at $ROM_FILE${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Prerequisites met${NC}"
echo ""

# Check for AI backends
BACKEND_AVAILABLE=false

echo "Checking AI Backends..."
echo "-----------------------"

# Check Ollama
if command -v ollama &> /dev/null; then
    if curl -s http://localhost:11434/api/tags > /dev/null 2>&1; then
        echo -e "${GREEN}✓ Ollama server running${NC}"
        if ollama list | grep -q "qwen2.5-coder"; then
            echo -e "${GREEN}✓ qwen2.5-coder model available${NC}"
            BACKEND_AVAILABLE=true
            AI_BACKEND="Ollama"
        else
            echo -e "${YELLOW}⚠ Recommended model qwen2.5-coder:7b not installed${NC}"
            echo "  Install with: ollama pull qwen2.5-coder:7b"
        fi
    else
        echo -e "${YELLOW}⚠ Ollama not running${NC}"
        echo "  Start with: ollama serve"
    fi
else
    echo -e "${YELLOW}⚠ Ollama not installed${NC}"
fi

# Check Gemini
if [ -n "$GEMINI_API_KEY" ]; then
    echo -e "${GREEN}✓ Gemini API key set${NC}"
    BACKEND_AVAILABLE=true
    if [ "$AI_BACKEND" != "Ollama" ]; then
        AI_BACKEND="Gemini"
    fi
else
    echo -e "${YELLOW}⚠ GEMINI_API_KEY not set${NC}"
fi

echo ""

if [ "$BACKEND_AVAILABLE" = false ]; then
    echo -e "${RED}✗ No AI backend available${NC}"
    echo ""
    echo "Please set up at least one backend:"
    echo "  - Ollama: brew install ollama && ollama serve && ollama pull qwen2.5-coder:7b"
    echo "  - Gemini: export GEMINI_API_KEY='your-key-here'"
    exit 1
fi

echo -e "${GREEN}✓ Using AI Backend: $AI_BACKEND${NC}"
echo ""

# Run the test-conversation command with default test cases
echo "========================================="
echo "Running Automated Conversation Tests"
echo "========================================="
echo ""
echo "This will run 5 default test cases:"
echo "  1. Simple ROM introspection (dungeon query)"
echo "  2. Overworld tile search"
echo "  3. Multi-step conversation"
echo "  4. Command generation (tile placement)"
echo "  5. Map description"
echo ""

read -p "Press Enter to start tests (or Ctrl+C to cancel)..."
echo ""

# Run the tests
"$Z3ED" agent test-conversation --rom "$ROM_FILE" --verbose

TEST_EXIT_CODE=$?

echo ""
echo "========================================="
echo "Test Results"
echo "========================================="

if [ $TEST_EXIT_CODE -eq 0 ]; then
    echo -e "${GREEN}✅ All tests completed successfully${NC}"
else
    echo -e "${RED}❌ Tests failed with exit code $TEST_EXIT_CODE${NC}"
fi

echo ""
echo "Next Steps:"
echo "  - Review the output above for any warnings"
echo "  - Check if tool calls are being invoked correctly"
echo "  - Verify JSON/table formatting is working"
echo "  - Test with custom conversation file: z3ed agent test-conversation --file my_tests.json"
echo ""

exit $TEST_EXIT_CODE
