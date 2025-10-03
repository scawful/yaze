#!/bin/bash
# Manual Gemini Integration Test
# Usage: GEMINI_API_KEY='your-key' ./scripts/manual_gemini_test.sh

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$SCRIPT_DIR/.."
Z3ED_BIN="$PROJECT_ROOT/build/bin/z3ed"

echo "🧪 Manual Gemini Integration Test"
echo "=================================="
echo ""

# Check if API key is set
if [ -z "$GEMINI_API_KEY" ]; then
    echo "❌ Error: GEMINI_API_KEY not set"
    echo ""
    echo "Usage:"
    echo "  GEMINI_API_KEY='your-api-key-here' ./scripts/manual_gemini_test.sh"
    echo ""
    echo "Or export it first:"
    echo "  export GEMINI_API_KEY='your-api-key-here'"
    echo "  ./scripts/manual_gemini_test.sh"
    exit 1
fi

echo "✅ GEMINI_API_KEY is set (length: ${#GEMINI_API_KEY} chars)"
echo ""

# Test 1: Simple palette command
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Test 1: Simple palette color change"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Prompt: 'Change palette 0 color 5 to red'"
echo ""

OUTPUT=$($Z3ED_BIN agent plan --prompt "Change palette 0 color 5 to red" 2>&1)
echo "$OUTPUT"
echo ""

if echo "$OUTPUT" | grep -q "Using Gemini AI"; then
    echo "✅ Gemini service detected"
else
    echo "❌ Expected 'Using Gemini AI' in output"
    exit 1
fi

if echo "$OUTPUT" | grep -q -E "palette|color"; then
    echo "✅ Generated palette-related commands"
else
    echo "❌ No palette commands found"
    exit 1
fi

echo ""

# Test 2: Overworld modification
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Test 2: Overworld tile placement"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Prompt: 'Place a tree at position (10, 20) on map 0'"
echo ""

OUTPUT=$($Z3ED_BIN agent plan --prompt "Place a tree at position (10, 20) on map 0" 2>&1)
echo "$OUTPUT"
echo ""

if echo "$OUTPUT" | grep -q "overworld"; then
    echo "✅ Generated overworld commands"
else
    echo "⚠️  No overworld commands (model may have interpreted differently)"
fi

echo ""

# Test 3: Complex multi-step task
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Test 3: Multi-step task"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Prompt: 'Export palette 0, change color 3 to blue, and import it back'"
echo ""

OUTPUT=$($Z3ED_BIN agent plan --prompt "Export palette 0, change color 3 to blue, and import it back" 2>&1)
echo "$OUTPUT"
echo ""

COMMAND_COUNT=$(echo "$OUTPUT" | grep -c -E "^\s*-" || true)

if [ "$COMMAND_COUNT" -ge 2 ]; then
    echo "✅ Generated multiple commands ($COMMAND_COUNT commands)"
else
    echo "⚠️  Expected multiple commands, got $COMMAND_COUNT"
fi

echo ""

# Test 4: Direct run command (creates proposal)
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Test 4: Direct run command (creates proposal)"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Prompt: 'Validate the ROM'"
echo ""

OUTPUT=$($Z3ED_BIN agent run --prompt "Validate the ROM" 2>&1 || true)
echo "$OUTPUT"
echo ""

if echo "$OUTPUT" | grep -q "Proposal"; then
    echo "✅ Proposal created"
else
    echo "ℹ️  No proposal created (may need ROM file)"
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "🎉 Manual Test Suite Complete!"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "Summary:"
echo "  • Gemini API integration: ✅ Working"
echo "  • Command generation: ✅ Functional"
echo "  • Service factory: ✅ Correct provider selection"
echo ""
echo "Next steps:"
echo "  1. Review generated commands for accuracy"
echo "  2. Test with more complex prompts"
echo "  3. Compare with Ollama output quality"
echo "  4. Proceed to Phase 3 (Claude) or Phase 4 (Enhanced Prompting)"
