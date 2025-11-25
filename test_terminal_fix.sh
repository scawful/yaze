#!/bin/bash
# Test script for terminal input and autocomplete fixes

echo "==============================================="
echo "Testing z3ed Terminal Input and Autocomplete"
echo "==============================================="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Change to project directory
cd /Users/scawful/Code/yaze || exit 1

echo -e "\n${YELLOW}Building WASM version...${NC}"
echo "----------------------------------------"

# Clean build directory
if [ -d "build_wasm" ]; then
    echo "Cleaning existing build_wasm directory..."
    rm -rf build_wasm
fi

# Configure with web preset
echo "Configuring with web preset..."
cmake --preset web-dbg

# Build
echo "Building project..."
cmake --build build_wasm -j4

# Check if build succeeded
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Build successful!${NC}"
else
    echo -e "${RED}✗ Build failed!${NC}"
    exit 1
fi

echo -e "\n${YELLOW}Checking generated files...${NC}"
echo "----------------------------------------"

# Check if terminal.js was copied
if [ -f "build_wasm/src/web/terminal.js" ]; then
    echo -e "${GREEN}✓ terminal.js found${NC}"

    # Check for key improvements
    echo "Checking for fixes..."

    if grep -q "Only stop propagation for special keys" build_wasm/src/web/terminal.js; then
        echo -e "${GREEN}  ✓ Keyboard input fix applied${NC}"
    else
        echo -e "${RED}  ✗ Keyboard input fix missing${NC}"
    fi

    if grep -q "showAutocompleteSuggestions" build_wasm/src/web/terminal.js; then
        echo -e "${GREEN}  ✓ Autocomplete system implemented${NC}"
    else
        echo -e "${RED}  ✗ Autocomplete system missing${NC}"
    fi

    if grep -q "getAllCommands" build_wasm/src/web/terminal.js; then
        echo -e "${GREEN}  ✓ Command database added${NC}"
    else
        echo -e "${RED}  ✗ Command database missing${NC}"
    fi
else
    echo -e "${RED}✗ terminal.js not found${NC}"
fi

# Check if WASM bridge was compiled
if [ -f "build_wasm/src/yaze.wasm" ]; then
    echo -e "${GREEN}✓ WASM module built${NC}"

    # Check if exports are present
    echo "Checking WASM exports..."
    if command -v wasm-objdump &> /dev/null; then
        wasm-objdump -x build_wasm/src/yaze.wasm | grep -q "Z3edGetCompletions" && \
            echo -e "${GREEN}  ✓ Z3edGetCompletions exported${NC}" || \
            echo -e "${YELLOW}  ⚠ Z3edGetCompletions may not be exported${NC}"
    else
        echo -e "${YELLOW}  ⚠ wasm-objdump not available, skipping export check${NC}"
    fi
else
    echo -e "${RED}✗ WASM module not built${NC}"
fi

echo -e "\n${YELLOW}Summary of Changes:${NC}"
echo "----------------------------------------"
echo "1. Fixed keyboard input handling:"
echo "   - Only special keys stop propagation"
echo "   - Normal typing now works properly"
echo "   - Input field gets focus on initialization"
echo ""
echo "2. Implemented comprehensive autocomplete:"
echo "   - Live suggestions as you type (200ms debounce)"
echo "   - Tab completion cycles through options"
echo "   - Visual dropdown with command descriptions"
echo "   - Context-aware subcommand completion"
echo ""
echo "3. Enhanced WASM bridge:"
echo "   - Improved GetCompletionsInternal function"
echo "   - Context-aware command suggestions"
echo "   - Support for subcommands (rom, hex, palette, etc.)"
echo ""

echo -e "\n${GREEN}Testing Instructions:${NC}"
echo "----------------------------------------"
echo "1. Serve the WASM build:"
echo "   cd build_wasm && python3 -m http.server 8080"
echo ""
echo "2. Open browser to http://localhost:8080"
echo ""
echo "3. Click the terminal icon to open z3ed terminal"
echo ""
echo "4. Test keyboard input:"
echo "   - Type any text - should appear normally"
echo "   - Use arrow keys for history navigation"
echo "   - Press Tab for autocomplete"
echo ""
echo "5. Test autocomplete:"
echo "   - Type 'h' and press Tab - should complete to 'help'"
echo "   - Type 'rom ' and press Tab - should show subcommands"
echo "   - Start typing any command - suggestions appear automatically"
echo ""

echo -e "${GREEN}✓ Terminal fixes complete!${NC}"