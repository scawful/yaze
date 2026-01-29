#!/bin/bash
set -eo pipefail

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${GREEN}=== YAZE Local CI Runner ===${NC}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

# 1. Build (Debug)
echo -e "\n${GREEN}[1/4] Building (Debug)...${NC}"
cmake --preset mac-dbg
if cmake --build build --parallel; then
    echo -e "${GREEN}Build Successful${NC}"
else
    echo -e "${RED}Build Failed${NC}"
    exit 1
fi

# Determine test binary path
TEST_BIN=""
if [ -f "./build/bin/Debug/yaze_test" ]; then
    TEST_BIN="./build/bin/Debug/yaze_test"
elif [ -f "./build/bin/yaze_test" ]; then
    TEST_BIN="./build/bin/yaze_test"
fi

# 2. Run Unit Tests
echo -e "\n${GREEN}[2/4] Running Unit Tests...${NC}"
if [ -n "$TEST_BIN" ] && "$TEST_BIN" --unit; then
    echo -e "${GREEN}Unit Tests Passed${NC}"
else
    echo -e "${RED}Unit Tests Failed or Binary Not Found${NC}"
    exit 1
fi

# 2b. Run Integration Tests (if ROM present)
ROM_PATH="$ROOT_DIR/zelda3.sfc"
if [ -f "$ROM_PATH" ]; then
    echo -e "\n${GREEN}[2.5/4] Running Integration Tests...${NC}"
    if "$TEST_BIN" --integration --rom-path "$ROM_PATH"; then
        echo -e "${GREEN}Integration Tests Passed${NC}"
    else
        echo -e "${RED}Integration Tests Failed${NC}"
        exit 1
    fi
else
    echo "ROM not found at $ROM_PATH. Skipping integration tests."
fi

# 3. Z3DK Integration Check
echo -e "\n${GREEN}[3/4] Checking Z3DK Integration...${NC}"
Z3DK_BIN="$ROOT_DIR/../z3dk/build/bin/z3asm"

if [ ! -f "$Z3DK_BIN" ]; then
    echo "z3asm not found at $Z3DK_BIN. Attempting to build..."
    # Attempt to build z3asm if source exists
    if [ -d "$ROOT_DIR/../z3dk" ]; then
        echo "Building z3dk..."
        (cd "$ROOT_DIR/../z3dk" && mkdir -p build && cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && make z3asm)
    fi
fi

if [ -f "$Z3DK_BIN" ]; then
    echo "Found z3asm at $Z3DK_BIN"
    TEST_ASM="/tmp/yaze_ci_test.asm"
    TEST_SFC="/tmp/yaze_ci_test.sfc"
    echo "org \$008000; lda #\$FF; rtl" > "$TEST_ASM"
    
    if "$Z3DK_BIN" "$TEST_ASM" "$TEST_SFC" > /dev/null; then
        if [ -f "$TEST_SFC" ]; then
            echo -e "${GREEN}z3asm Compilation Successful${NC}"
            rm "$TEST_ASM" "$TEST_SFC"
        else
            echo -e "${RED}z3asm failed to produce output${NC}"
            exit 1
        fi
    else
        echo -e "${RED}z3asm execution failed${NC}"
        # Don't fail the whole CI for this if optional, but user asked for integration.
        # Let's verify if they want soft fail? Assuming hard fail for now.
        exit 1
    fi
else
    echo "z3asm not found at $Z3DK_BIN. Skipping integration test."
    echo "Tip: Build z3dk with 'cmake --build build --target z3asm' in ../z3dk"
fi

# 4. Mesen2 Socket Check
echo -e "\n${GREEN}[4/4] Checking Mesen2 Connectivity...${NC}"
# We assume if the user is running "Local CI", they might want to verify they can connect to Mesen.
# If no Mesen is running, we can skip or try to launch headless.
# For simplicity, we'll try to connect to an existing socket, or skip.

SOCKET_PATH=$(find /tmp -name "mesen2-*.sock" -print -quit 2>/dev/null)

if [ -n "$SOCKET_PATH" ]; then
    echo "Found Mesen2 socket at $SOCKET_PATH"
    if python3 "$SCRIPT_DIR/mesen2_check.py" "$SOCKET_PATH"; then
         echo -e "${GREEN}Mesen2 Connectivity Verified${NC}"
    else
         echo -e "${RED}Mesen2 Connectivity Failed${NC}"
         exit 1
    fi
else
    echo "No running Mesen2 socket found. Skipping connectivity check."
    echo "Tip: Run 'mesen-agent launch test --headless' to enable this check."
fi

echo -e "\n${GREEN}=== Local CI Complete! All checks passed. ===${NC}"
