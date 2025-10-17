#!/bin/bash
# Quick test script for GUI automation tools

echo "=== Testing GUI Automation Tools ==="
echo ""

# Set up environment
export AI_PROVIDER=mock
export MOCK_RESPONSE="Testing GUI tools"
cd /Users/scawful/Code/yaze

echo "1. Testing gui-discover tool..."
echo "Query: What buttons are available?"
# This would normally trigger the AI to call gui-discover
# For now, we'll just verify the tool exists in the dispatcher

echo ""
echo "2. Testing gui-click tool..."
echo "Query: Click the Draw button"

echo ""
echo "3. Testing gui-place-tile tool..."
echo "Query: Place a tree at position 10, 15"

echo ""
echo "4. Testing gui-screenshot tool..."
echo "Query: Show me a screenshot"

echo ""
echo "=== Tool Registration Check ==="
echo "Checking if tools are registered in prompt_catalogue.yaml..."
grep -c "gui-place-tile" assets/agent/prompt_catalogue.yaml && echo "✓ gui-place-tile found"
grep -c "gui-click" assets/agent/prompt_catalogue.yaml && echo "✓ gui-click found"
grep -c "gui-discover" assets/agent/prompt_catalogue.yaml && echo "✓ gui-discover found"
grep -c "gui-screenshot" assets/agent/prompt_catalogue.yaml && echo "✓ gui-screenshot found"

echo ""
echo "=== Tool Dispatcher Check ==="
echo "Checking if tools are handled in tool_dispatcher.cc..."
grep -c "gui-place-tile" src/cli/service/agent/tool_dispatcher.cc && echo "✓ gui-place-tile dispatcher entry found"
grep -c "gui-click" src/cli/service/agent/tool_dispatcher.cc && echo "✓ gui-click dispatcher entry found"
grep -c "gui-discover" src/cli/service/agent/tool_dispatcher.cc && echo "✓ gui-discover dispatcher entry found"
grep -c "gui-screenshot" src/cli/service/agent/tool_dispatcher.cc && echo "✓ gui-screenshot dispatcher entry found"

echo ""
echo "=== Handler Implementation Check ==="
echo "Checking if handlers are implemented..."
grep -c "HandleGuiPlaceTileCommand" src/cli/handlers/agent/gui_tool_commands.cc && echo "✓ HandleGuiPlaceTileCommand implemented"
grep -c "HandleGuiClickCommand" src/cli/handlers/agent/gui_tool_commands.cc && echo "✓ HandleGuiClickCommand implemented"
grep -c "HandleGuiDiscoverToolCommand" src/cli/handlers/agent/gui_tool_commands.cc && echo "✓ HandleGuiDiscoverToolCommand implemented"
grep -c "HandleGuiScreenshotCommand" src/cli/handlers/agent/gui_tool_commands.cc && echo "✓ HandleGuiScreenshotCommand implemented"

echo ""
echo "=== System Prompt Check ==="
if [ -f "assets/agent/gui_automation_instructions.txt" ]; then
    echo "✓ GUI automation instructions found"
    echo "  Lines: $(wc -l < assets/agent/gui_automation_instructions.txt)"
else
    echo "✗ GUI automation instructions not found"
fi

echo ""
echo "=== Build Check ==="
if [ -f "build/bin/z3ed" ]; then
    echo "✓ z3ed binary exists"
    ls -lh build/bin/z3ed | awk '{print "  Size:", $5}'
else
    echo "✗ z3ed binary not found"
fi

echo ""
echo "=== Environment Check ==="
echo "Ollama availability:"
if command -v ollama &> /dev/null; then
    echo "  ✓ ollama command found"
    ollama list 2>/dev/null | head -5 || echo "  (ollama not running)"
else
    echo "  ✗ ollama not installed"
    echo "  Install with: brew install ollama"
fi

echo ""
echo "Gemini API key:"
if [ -n "$GEMINI_API_KEY" ]; then
    echo "  ✓ GEMINI_API_KEY is set"
else
    echo "  ⚠ GEMINI_API_KEY not set (optional)"
fi

echo ""
echo "=== Ready to Test! ==="
echo ""
echo "To start testing:"
echo "1. Terminal 1: ./build/bin/yaze assets/zelda3.sfc --enable-test-harness"
echo "2. Terminal 2: export AI_PROVIDER=ollama && ./build/bin/z3ed agent chat --rom assets/zelda3.sfc"
echo "3. Try: 'What buttons are available in the Overworld editor?'"
echo ""
echo "Or use Gemini:"
echo "2. Terminal 2: export AI_PROVIDER=gemini && export GEMINI_API_KEY='...' && ./build/bin/z3ed agent chat --rom assets/zelda3.sfc"
echo ""

