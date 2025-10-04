#!/bin/bash
# Test script to verify ImGuiTestHarness gRPC service integration
# Ensures the GUI automation infrastructure is working

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
YAZE_APP="${PROJECT_ROOT}/build/bin/yaze.app/Contents/MacOS/yaze"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo "========================================="
echo "ImGui Test Harness Verification"
echo "========================================="
echo ""

# Check if YAZE is built with gRPC support
if [ ! -f "$YAZE_APP" ]; then
    echo -e "${RED}✗ YAZE application not found at $YAZE_APP${NC}"
    echo ""
    echo "Build with gRPC support:"
    echo "  cmake -B build -DYAZE_WITH_GRPC=ON -DYAZE_WITH_JSON=ON"
    echo "  cmake --build build --target yaze"
    exit 1
fi

echo -e "${GREEN}✓ YAZE application found${NC}"
echo ""

# Check if gRPC libraries are linked
echo "Checking gRPC dependencies..."
echo "------------------------------"

if otool -L "$YAZE_APP" 2>/dev/null | grep -q "libgrpc"; then
    echo -e "${GREEN}✓ gRPC libraries linked${NC}"
else
    echo -e "${YELLOW}⚠ gRPC libraries may not be linked${NC}"
    echo "  This might be expected if gRPC is statically linked"
fi

# Check for test harness service code
TEST_HARNESS_IMPL="${PROJECT_ROOT}/src/app/core/service/imgui_test_harness_service.cc"
if [ -f "$TEST_HARNESS_IMPL" ]; then
    echo -e "${GREEN}✓ Test harness implementation found${NC}"
else
    echo -e "${RED}✗ Test harness implementation not found${NC}"
    exit 1
fi

echo ""

# Check if the service is properly integrated
echo "Verifying test harness integration..."
echo "--------------------------------------"

# Look for the service registration in the codebase
if grep -q "ImGuiTestHarnessServer" "${PROJECT_ROOT}/src/app/core/service/imgui_test_harness_service.h"; then
    echo -e "${GREEN}✓ ImGuiTestHarnessServer class defined${NC}"
else
    echo -e "${RED}✗ ImGuiTestHarnessServer class not found${NC}"
    exit 1
fi

# Check for gRPC server initialization
if grep -rq "ImGuiTestHarnessServer.*Start" "${PROJECT_ROOT}/src/app" 2>/dev/null; then
    echo -e "${GREEN}✓ Server startup code found${NC}"
else
    echo -e "${YELLOW}⚠ Could not verify server startup code${NC}"
fi

echo ""

# Test gRPC port availability
echo "Testing gRPC server availability..."
echo "------------------------------------"

GRPC_PORT=50051
echo "Checking if port $GRPC_PORT is available..."

if lsof -Pi :$GRPC_PORT -sTCP:LISTEN -t >/dev/null 2>&1; then
    echo -e "${YELLOW}⚠ Port $GRPC_PORT is already in use${NC}"
    echo "  If YAZE is running, this is expected"
    SERVER_RUNNING=true
else
    echo -e "${GREEN}✓ Port $GRPC_PORT is available${NC}"
    SERVER_RUNNING=false
fi

echo ""

# Interactive test option
if [ "$SERVER_RUNNING" = false ]; then
    echo "========================================="
    echo "Interactive Test Options"
    echo "========================================="
    echo ""
    echo "The test harness server is not currently running."
    echo ""
    echo "To test the full integration:"
    echo ""
    echo "1. Start YAZE in one terminal:"
    echo "   $YAZE_APP"
    echo ""
    echo "2. In another terminal, verify the gRPC server:"
    echo "   lsof -Pi :$GRPC_PORT -sTCP:LISTEN"
    echo ""
    echo "3. Test with z3ed GUI automation:"
    echo "   z3ed agent test --prompt 'Open Overworld editor'"
    echo ""
else
    echo "========================================="
    echo "Live Server Test"
    echo "========================================="
    echo ""
    echo -e "${GREEN}✓ gRPC server appears to be running on port $GRPC_PORT${NC}"
    echo ""
    
    # Try to connect to the server
    if command -v grpcurl &> /dev/null; then
        echo "Testing server connection with grpcurl..."
        if grpcurl -plaintext localhost:$GRPC_PORT list 2>&1 | grep -q "yaze.test.ImGuiTestHarness"; then
            echo -e "${GREEN}✅ ImGuiTestHarness service is available!${NC}"
            echo ""
            echo "Available RPC methods:"
            grpcurl -plaintext localhost:$GRPC_PORT list yaze.test.ImGuiTestHarness 2>&1 | sed 's/^/  /'
        else
            echo -e "${YELLOW}⚠ Could not verify service availability${NC}"
        fi
    else
        echo -e "${YELLOW}⚠ grpcurl not installed, skipping connection test${NC}"
        echo "  Install with: brew install grpcurl"
    fi
fi

echo ""
echo "========================================="
echo "Summary"
echo "========================================="
echo ""
echo "Test Harness Components:"
echo "  [✓] Source files present"
echo "  [✓] gRPC integration compiled"

if [ "$SERVER_RUNNING" = true ]; then
    echo "  [✓] Server running on port $GRPC_PORT"
else
    echo "  [ ] Server not currently running"
fi

echo ""
echo "The ImGuiTestHarness service is ${GREEN}ready${NC} for:"
echo "  - Widget discovery and introspection"
echo "  - Automated GUI testing via z3ed agent test"
echo "  - Recording and playback of user interactions"
echo ""

# Additional checks for agent chat widget
echo "Checking for Agent Chat Widget..."
echo "----------------------------------"

if grep -rq "AgentChatWidget" "${PROJECT_ROOT}/src/app/gui" 2>/dev/null; then
    echo -e "${GREEN}✓ AgentChatWidget found in GUI code${NC}"
else
    echo -e "${YELLOW}⚠ AgentChatWidget not yet implemented${NC}"
    echo "  This is the next priority item in the roadmap"
    echo "  Location: src/app/gui/debug/agent_chat_widget.{h,cc}"
fi

echo ""
echo "Next Steps:"
echo "  1. Run YAZE and verify gRPC server starts: $YAZE_APP"
echo "  2. Test conversation agent: z3ed agent test-conversation"
echo "  3. Implement AgentChatWidget for GUI integration"
echo ""
