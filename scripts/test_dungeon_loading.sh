#!/bin/bash
# Test script for dungeon room loading (torches, blocks, doors, pits)
# Usage: ./scripts/test_dungeon_loading.sh [rom_file]

set -e

# Configuration
ROM_FILE="${1:-roms/alttp_vanilla.sfc}"
BUILD_DIR="build/bin"
LOG_FILE="dungeon_loading_test.log"

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Dungeon Room Loading Test Script${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""

# Check if ROM file exists
if [ ! -f "$ROM_FILE" ]; then
    echo -e "${RED}ERROR: ROM file not found: $ROM_FILE${NC}"
    echo "Usage: $0 [path/to/alttp_vanilla.sfc]"
    exit 1
fi

# Check if yaze binary exists
if [ ! -f "$BUILD_DIR/yaze" ]; then
    echo -e "${RED}ERROR: yaze binary not found at $BUILD_DIR/yaze${NC}"
    echo "Please build yaze first: cmake --build build"
    exit 1
fi

echo -e "${YELLOW}ROM File:${NC} $ROM_FILE"
echo -e "${YELLOW}Log File:${NC} $LOG_FILE"
echo ""

# Test 1: Launch with torch-heavy rooms
echo -e "${GREEN}Test 1: Loading rooms with torches${NC}"
echo "Starting yaze with Room 8 (Ganon's Tower - Torch 2)..."
$BUILD_DIR/yaze \
    --rom_file="$ROM_FILE" \
    --debug \
    --log_file="$LOG_FILE" \
    --editor=Dungeon \
    --cards="Room 8,Room 173" &

YAZE_PID=$!
echo "yaze PID: $YAZE_PID"

# Give it a moment to start and load
sleep 3

# Check if process is still running
if ! ps -p $YAZE_PID > /dev/null 2>&1; then
    echo -e "${RED}FAILED: yaze crashed or exited${NC}"
    echo "Check log file: $LOG_FILE"
    exit 1
fi

echo -e "${GREEN}SUCCESS: yaze is running${NC}"
echo ""

# Analyze log file
echo -e "${GREEN}Analyzing debug log...${NC}"
echo ""

# Check for torch loading
TORCH_COUNT=$(grep -c "LoadTorches:" "$LOG_FILE" || true)
TORCH_LOADED=$(grep -c "Loaded torch at" "$LOG_FILE" || true)

echo -e "${YELLOW}Torches:${NC}"
echo "  - LoadTorches called: $TORCH_COUNT times"
echo "  - Torches loaded: $TORCH_LOADED"

if [ $TORCH_LOADED -gt 0 ]; then
    echo -e "  ${GREEN}✓ Torches successfully loaded${NC}"
    # Show sample torch data
    echo "  Sample torch data:"
    grep "Loaded torch at" "$LOG_FILE" | head -3 | sed 's/^/    /'
else
    echo -e "  ${YELLOW}⚠ No torches loaded (may be normal for these rooms)${NC}"
fi
echo ""

# Check for block loading
BLOCK_COUNT=$(grep -c "LoadBlocks:" "$LOG_FILE" || true)
BLOCK_LOADED=$(grep -c "Loaded block at" "$LOG_FILE" || true)

echo -e "${YELLOW}Blocks:${NC}"
echo "  - LoadBlocks called: $BLOCK_COUNT times"
echo "  - Blocks loaded: $BLOCK_LOADED"

if [ $BLOCK_LOADED -gt 0 ]; then
    echo -e "  ${GREEN}✓ Blocks successfully loaded${NC}"
    echo "  Sample block data:"
    grep "Loaded block at" "$LOG_FILE" | head -3 | sed 's/^/    /'
else
    echo -e "  ${YELLOW}⚠ No blocks loaded (may be normal for these rooms)${NC}"
fi
echo ""

# Check for pit loading
PIT_COUNT=$(grep -c "LoadPits:" "$LOG_FILE" || true)
PIT_DEST=$(grep -c "Pit destination" "$LOG_FILE" || true)

echo -e "${YELLOW}Pits:${NC}"
echo "  - LoadPits called: $PIT_COUNT times"
echo "  - Pit destinations set: $PIT_DEST"

if [ $PIT_DEST -gt 0 ]; then
    echo -e "  ${GREEN}✓ Pit data successfully loaded${NC}"
    echo "  Sample pit data:"
    grep "Pit destination" "$LOG_FILE" | head -3 | sed 's/^/    /'
else
    echo -e "  ${YELLOW}⚠ No pit data (may be normal)${NC}"
fi
echo ""

# Check for door loading
DOOR_COUNT=$(grep -c "LoadDoors" "$LOG_FILE" || true)

echo -e "${YELLOW}Doors:${NC}"
echo "  - LoadDoors called: $DOOR_COUNT times"

if [ $DOOR_COUNT -gt 0 ]; then
    echo -e "  ${GREEN}✓ Door loading called${NC}"
    echo "  Sample door log:"
    grep "LoadDoors" "$LOG_FILE" | head -3 | sed 's/^/    /'
else
    echo -e "  ${RED}✗ LoadDoors not called${NC}"
fi
echo ""

# Check for errors
ERROR_COUNT=$(grep -i "error\|segmentation\|crash" "$LOG_FILE" | wc -l || true)

if [ $ERROR_COUNT -gt 0 ]; then
    echo -e "${RED}⚠ Errors found in log:${NC}"
    grep -i "error\|segmentation\|crash" "$LOG_FILE" | sed 's/^/  /'
else
    echo -e "${GREEN}✓ No errors found in log${NC}"
fi
echo ""

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Test Summary${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo "yaze is still running (PID: $YAZE_PID)"
echo "You can now manually test the dungeon editor."
echo ""
echo "To view live log updates:"
echo "  tail -f $LOG_FILE | grep -E 'Load(Torches|Blocks|Pits|Doors)|Loaded (torch|block)'"
echo ""
echo "To stop yaze:"
echo "  kill $YAZE_PID"
echo ""
echo "Full log available at: $LOG_FILE"
echo ""

# Don't automatically kill yaze - let user test manually
read -p "Press Enter to stop yaze and exit..."

kill $YAZE_PID 2>/dev/null || true
echo "yaze stopped."

exit 0
