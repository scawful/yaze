#!/bin/bash
# scripts/gemini_build.sh
#
# Builds AI-enabled yaze binaries. Historically this script was used for Gemini
# experimentation, but the project now uses unified AI presets.
#
# Prefer:
#   ./scripts/agent_build.sh [target]
#
# This script remains as a compatibility wrapper.

set -e

# Detect OS
OS="$(uname -s)"
case "${OS}" in
    Linux*)     PRESET="lin-ai";;
    Darwin*)    PRESET="mac-ai";;
    CYGWIN*)    PRESET="win-ai";;
    MINGW*)     PRESET="win-ai";;
    *)          echo "Unknown OS: ${OS}"; exit 1;;
esac

# Default to the AI build dir so wrappers (scripts/yaze, scripts/z3ed) find the
# most feature-complete binaries.
BUILD_DIR="${YAZE_BUILD_DIR:-build_ai}"

TARGET="yaze"
FRESH=""

for arg in "$@"; do
    case $arg in
        --fresh)
            FRESH="--fresh"
            ;;
        *)
            TARGET="$arg"
            ;;
    esac
done

echo "=================================================="
echo "AI Build System (gemini_build.sh compatibility)"
echo "Platform:  ${OS}"
echo "Preset:    ${PRESET}"
echo "Build Dir: ${BUILD_DIR}"
echo "Target:    ${TARGET}"
echo "=================================================="

if [ ! -f "CMakePresets.json" ]; then
    echo "❌ Error: CMakePresets.json not found. Must run from project root."
    exit 1
fi

if [ ! -d "${BUILD_DIR}" ] || [ -n "${FRESH}" ]; then
    echo "🔧 Configuring ${PRESET} ${FRESH}..."
    cmake --preset "${PRESET}" ${FRESH}
fi

echo "🔨 Building target: ${TARGET}..."
cmake --build "${BUILD_DIR}" --target "${TARGET}"

echo "✅ Build complete."

echo ""
echo "Run tests: ctest --test-dir ${BUILD_DIR} -L quick"
echo "Run app:   ./scripts/yaze"
