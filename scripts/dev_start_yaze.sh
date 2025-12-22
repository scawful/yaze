#!/bin/bash
# scripts/dev_start_yaze.sh
# Quickly builds and starts YAZE with gRPC enabled for Agent testing.

# Exit on error
set -e

# Project root
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${YAZE_BUILD_DIR:-${PROJECT_ROOT}/build}"
# Prefer Debug binary (agent preset builds Debug by default)
YAZE_BIN="${BUILD_DIR}/bin/Debug/yaze.app/Contents/MacOS/yaze"
TEST_HARNESS_PORT="${YAZE_GRPC_PORT:-50052}"

# Fallbacks if layout differs
if [ ! -x "$YAZE_BIN" ]; then
    if [ -x "${BUILD_DIR}/bin/yaze" ]; then
        YAZE_BIN="${BUILD_DIR}/bin/yaze"
    elif [ -x "${BUILD_DIR}/bin/Debug/yaze" ]; then
        YAZE_BIN="${BUILD_DIR}/bin/Debug/yaze"
    elif [ -x "${BUILD_DIR}/bin/Release/yaze" ]; then
        YAZE_BIN="${BUILD_DIR}/bin/Release/yaze"
    else
        echo "❌ Could not find yaze binary in ${BUILD_DIR}/bin (checked app and flat)." >&2
        exit 1
    fi
fi
# Default to oos168.sfc if available, otherwise check common locations or ask user
ROM_PATH="/Users/scawful/Code/Oracle-of-Secrets/Roms/oos168.sfc"

# If the hardcoded path doesn't exist, try to find one
if [ ! -f "$ROM_PATH" ]; then
    FOUND_ROM=$(find "${PROJECT_ROOT}/../Oracle-of-Secrets/Roms" -name "*.sfc" | head -n 1)
    if [ -n "$FOUND_ROM" ]; then
        ROM_PATH="$FOUND_ROM"
    fi
fi

echo "=================================================="
echo "🚀 YAZE Agent Environment Launcher"
echo "=================================================="

# Navigate to project root
cd "${PROJECT_ROOT}" || exit 1

# 1. Build (Fast)
echo "📦 Building YAZE (Target: yaze)..."
"./scripts/agent_build.sh" yaze

# 2. Check ROM
if [ ! -f "$ROM_PATH" ]; then
    echo "❌ ROM not found at $ROM_PATH"
    echo "   Please edit this script to set a valid ROM_PATH."
    exit 1
fi

# 3. Start YAZE with gRPC and Debug flags
echo "🎮 Launching YAZE..."
echo "   - gRPC: Enabled (Port ${TEST_HARNESS_PORT})"
echo "   - ROM: $(basename "$ROM_PATH")"
echo "   - Editor: Dungeon"
echo "   - Cards: Object Editor"
echo "=================================================="

"${YAZE_BIN}" \
    --enable_test_harness \
    --test_harness_port "${TEST_HARNESS_PORT}" \
    --rom_file "$ROM_PATH" \
    --debug \
    --editor "Dungeon" \
    --cards "Object Editor"
