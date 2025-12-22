#!/bin/bash
# scripts/gemini_build.sh
# Build script for Gemini AI agent - builds full yaze with all features
# Usage: ./scripts/gemini_build.sh [target] [--fresh]
# Optional: set YAZE_BUILD_DIR to override the build directory.
#
# Examples:
#   ./scripts/gemini_build.sh              # Build yaze (default)
#   ./scripts/gemini_build.sh yaze_test    # Build tests
#   ./scripts/gemini_build.sh --fresh      # Clean reconfigure and build
#   ./scripts/gemini_build.sh z3ed         # Build CLI tool

set -e

# Configuration
BUILD_DIR="${YAZE_BUILD_DIR:-build}"
PRESET="mac-gemini"
TARGET="${1:-yaze}"
FRESH=""

# Parse arguments
for arg in "$@"; do
    case $arg in
        --fresh)
            FRESH="--fresh"
            shift
            ;;
        *)
            TARGET="$arg"
            ;;
    esac
done

echo "=================================================="
echo "Gemini Agent Build System"
echo "Build Dir: ${BUILD_DIR}"
echo "Preset:    ${PRESET}"
echo "Target:    ${TARGET}"
echo "=================================================="

# Ensure we are in the project root
if [ ! -f "CMakePresets.json" ]; then
    echo "Error: CMakePresets.json not found. Must run from project root."
    exit 1
fi

# Configure if needed or if --fresh specified
if [ ! -d "${BUILD_DIR}" ] || [ -n "${FRESH}" ]; then
    echo "Configuring ${PRESET}..."
    cmake --preset "${PRESET}" ${FRESH}
fi

# Build
echo "Building target: ${TARGET}..."
cmake --build "${BUILD_DIR}" --target "${TARGET}" -j$(sysctl -n hw.ncpu)

echo ""
echo "Build complete: ${BUILD_DIR}/${TARGET}"
echo ""
echo "Run tests:   ctest --test-dir ${BUILD_DIR} -L stable -j4"
echo "Run app:     ./${BUILD_DIR}/Debug/yaze"
