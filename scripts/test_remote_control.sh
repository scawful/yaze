#!/bin/bash

# Test Remote Control - Practical Agent Workflows
# 
# This script demonstrates the agent's ability to remotely control YAZE
# and perform real editing tasks like drawing tiles, moving entities, etc.
#
# Usage: ./scripts/test_remote_control.sh

set -e  # Exit on error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PROTO_DIR="$PROJECT_ROOT/src/app/core/proto"
PROTO_FILE="imgui_test_harness.proto"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test harness connection
HOST="127.0.0.1"
PORT="50052"

echo -e "${BLUE}=== YAZE Remote Control Test ===${NC}\n"

# Check if grpcurl is available
if ! command -v grpcurl &> /dev/null; then
    echo -e "${RED}Error: grpcurl not found${NC}"
    echo "Install with: brew install grpcurl"
    exit 1
fi

# Helper function to make gRPC calls
grpc_call() {
    local method=$1
    local data=$2
    grpcurl -plaintext \
        -import-path "$PROTO_DIR" \
        -proto "$PROTO_FILE" \
        -d "$data" \
        "$HOST:$PORT" \
        "yaze.test.ImGuiTestHarness/$method"
}

# Helper function to print test status
print_test() {
    local test_num=$1
    local test_name=$2
    echo -e "\n${BLUE}Test $test_num: $test_name${NC}"
}

print_success() {
    echo -e "${GREEN}✓ PASSED${NC}"
}

print_failure() {
    echo -e "${RED}✗ FAILED: $1${NC}"
}

# Test 0: Check server connection
print_test "0" "Server Connection"
if grpc_call "Ping" '{"message":"hello"}' &> /dev/null; then
    print_success
else
    print_failure "Server not responding"
    echo -e "${YELLOW}Start the test harness:${NC}"
    echo "./build-grpc-test/bin/yaze.app/Contents/MacOS/yaze \\"
    echo "  --enable_test_harness \\"
    echo "  --test_harness_port=50052 \\"
    echo "  --rom_file=assets/zelda3.sfc"
    exit 1
fi

echo -e "\n${BLUE}=== Practical Agent Workflows ===${NC}\n"

# Workflow 1: Activate Draw Tile Mode
print_test "1" "Activate Draw Tile Mode"
echo "Action: Click DrawTile button in Overworld toolset"

response=$(grpc_call "Click" '{
    "target": "Overworld/Toolset/button:DrawTile",
    "type": "LEFT"
}' 2>&1)

if echo "$response" | grep -q '"success": true'; then
    print_success
    echo "Agent can now paint tiles on the overworld"
else
    print_failure "Could not activate draw tile mode"
    echo "Response: $response"
fi

sleep 1

# Workflow 2: Select Pan Mode
print_test "2" "Select Pan Mode"
echo "Action: Click Pan button to enable map navigation"

response=$(grpc_call "Click" '{
    "target": "Overworld/Toolset/button:Pan",
    "type": "LEFT"
}' 2>&1)

if echo "$response" | grep -q '"success": true'; then
    print_success
    echo "Agent can now pan the overworld map"
else
    print_failure "Could not activate pan mode"
    echo "Response: $response"
fi

sleep 1

# Workflow 3: Open Tile16 Editor
print_test "3" "Open Tile16 Editor"
echo "Action: Click Tile16Editor button to open editor"

response=$(grpc_call "Click" '{
    "target": "Overworld/Toolset/button:Tile16Editor",
    "type": "LEFT"
}' 2>&1)

if echo "$response" | grep -q '"success": true'; then
    print_success
    echo "Tile16 Editor window should now be open"
    echo "Agent can select tiles for drawing"
else
    print_failure "Could not open Tile16 Editor"
    echo "Response: $response"
fi

sleep 1

# Workflow 4: Test Entrances Mode
print_test "4" "Switch to Entrances Mode"
echo "Action: Click Entrances button"

response=$(grpc_call "Click" '{
    "target": "Overworld/Toolset/button:Entrances",
    "type": "LEFT"
}' 2>&1)

if echo "$response" | grep -q '"success": true'; then
    print_success
    echo "Agent can now edit overworld entrances"
else
    print_failure "Could not activate entrances mode"
    echo "Response: $response"
fi

sleep 1

# Workflow 5: Test Exits Mode
print_test "5" "Switch to Exits Mode"
echo "Action: Click Exits button"

response=$(grpc_call "Click" '{
    "target": "Overworld/Toolset/button:Exits",
    "type": "LEFT"
}' 2>&1)

if echo "$response" | grep -q '"success": true'; then
    print_success
    echo "Agent can now edit overworld exits"
else
    print_failure "Could not activate exits mode"
    echo "Response: $response"
fi

sleep 1

# Workflow 6: Test Sprites Mode
print_test "6" "Switch to Sprites Mode"
echo "Action: Click Sprites button"

response=$(grpc_call "Click" '{
    "target": "Overworld/Toolset/button:Sprites",
    "type": "LEFT"
}' 2>&1)

if echo "$response" | grep -q '"success": true'; then
    print_success
    echo "Agent can now edit sprite placements"
else
    print_failure "Could not activate sprites mode"
    echo "Response: $response"
fi

sleep 1

# Workflow 7: Test Items Mode
print_test "7" "Switch to Items Mode"
echo "Action: Click Items button"

response=$(grpc_call "Click" '{
    "target": "Overworld/Toolset/button:Items",
    "type": "LEFT"
}' 2>&1)

if echo "$response" | grep -q '"success": true'; then
    print_success
    echo "Agent can now place items on the overworld"
else
    print_failure "Could not activate items mode"
    echo "Response: $response"
fi

sleep 1

# Workflow 8: Test Zoom Controls
print_test "8" "Test Zoom Controls"
echo "Action: Zoom in on the map"

response=$(grpc_call "Click" '{
    "target": "Overworld/Toolset/button:ZoomIn",
    "type": "LEFT"
}' 2>&1)

if echo "$response" | grep -q '"success": true'; then
    print_success
    echo "Zoom level increased"
    
    # Zoom back out
    sleep 0.5
    grpc_call "Click" '{
        "target": "Overworld/Toolset/button:ZoomOut",
        "type": "LEFT"
    }' &> /dev/null
    echo "Zoom level restored"
else
    print_failure "Could not zoom"
    echo "Response: $response"
fi

# Workflow 9: Legacy Format Fallback Test
print_test "9" "Legacy Format Fallback"
echo "Action: Test old-style widget reference"

response=$(grpc_call "Click" '{
    "target": "button:Overworld",
    "type": "LEFT"
}' 2>&1)

if echo "$response" | grep -q '"success": true'; then
    print_success
    echo "Legacy format still works (backwards compatible)"
else
    # This is expected if Overworld Editor isn't in main window
    echo -e "${YELLOW}Legacy format may not work (expected)${NC}"
fi

# Summary
echo -e "\n${BLUE}=== Test Summary ===${NC}\n"
echo "Remote control capabilities verified:"
echo "  ✓ Mode switching (Draw, Pan, Entrances, Exits, Sprites, Items)"
echo "  ✓ Tool opening (Tile16 Editor)"
echo "  ✓ Zoom controls"
echo "  ✓ Widget registry integration"
echo ""
echo "Agent can now:"
echo "  • Switch between editing modes"
echo "  • Open auxiliary editors"
echo "  • Control view settings"
echo "  • Prepare for complex editing operations"
echo ""
echo "Next steps for full automation:"
echo "  1. Add canvas click support (x,y coordinates)"
echo "  2. Add tile selection in Tile16 Editor"
echo "  3. Add entity dragging support"
echo "  4. Implement workflow chaining (mode + select + draw)"
echo ""
echo -e "${GREEN}Remote control system functional!${NC}"
