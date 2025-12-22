#!/bin/bash
set -e

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_ROOT="${YAZE_BUILD_DIR:-$PROJECT_ROOT/build}"
# Try Debug dir first (multi-config), then root bin
if [ -d "$BUILD_ROOT/bin/Debug" ]; then
    BUILD_DIR="$BUILD_ROOT/bin/Debug"
else
    BUILD_DIR="$BUILD_ROOT/bin"
fi

# Handle macOS bundle
if [ -d "$BUILD_DIR/yaze.app" ]; then
    YAZE_BIN="$BUILD_DIR/yaze.app/Contents/MacOS/yaze"
else
    YAZE_BIN="$BUILD_DIR/yaze"
fi
Z3ED_BIN="$BUILD_DIR/z3ed"

# Check binaries
if [ ! -f "$YAZE_BIN" ] || [ ! -f "$Z3ED_BIN" ]; then
    echo -e "${RED}Error: Binaries not found in $BUILD_DIR${NC}"
    echo "Please run: cmake --preset mac-ai && cmake --build build"
    exit 1
fi

echo -e "${GREEN}Starting YAZE GUI with gRPC test harness...${NC}"
# Start yaze in background with test harness enabled
# We use a mock ROM to avoid needing a real file for this test, if supported
PORT=50055
echo "Launching YAZE binary: $YAZE_BIN"
"$YAZE_BIN" --enable_test_harness --test_harness_port=$PORT --log_to_console &
YAZE_PID=$!

# Wait for server to start
echo "Waiting for gRPC server on port $PORT (PID: $YAZE_PID)..."
# Loop to check if port is actually listening
for i in {1..20}; do
    if lsof -Pi :$PORT -sTCP:LISTEN -t >/dev/null; then
        echo -e "${GREEN}Server is listening!${NC}"
        break
    fi
    echo "..."
    sleep 1
done

# Check if process still alive
if ! kill -0 $YAZE_PID 2>/dev/null; then
    echo -e "${RED}Error: YAZE process died prematurely.${NC}"
    exit 1
fi

cleanup() {
    echo -e "${GREEN}Stopping YAZE GUI (PID: $YAZE_PID)...${NC}"
    kill "$YAZE_PID" 2>/dev/null || true
}
trap cleanup EXIT

echo -e "${GREEN}Step 1: Discover Widgets${NC}"
"$Z3ED_BIN" gui-discover-tool --format=text --mock-rom --gui_server_address="localhost:$PORT"

echo -e "${GREEN}Step 2: Take Screenshot (Before Click)${NC}"
"$Z3ED_BIN" gui-screenshot --region=full --format=json --mock-rom --gui_server_address="localhost:$PORT"

echo -e "${GREEN}Step 3: Click 'File' Menu${NC}"
"$Z3ED_BIN" gui-click --target="File" --format=text --mock-rom --gui_server_address="localhost:$PORT" || echo -e "${RED}Click failed (expected if ID wrong)${NC}"

echo -e "${GREEN}Step 4: Take Screenshot (After Click)${NC}"
"$Z3ED_BIN" gui-screenshot --region=full --format=json --mock-rom --gui_server_address="localhost:$PORT"

echo -e "${GREEN}Demo Complete! Keeping YAZE open for 60 seconds...${NC}"
sleep 60
