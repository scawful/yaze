#!/bin/bash
# scripts/agent_build.sh
# Agent build helper (shared build directory by default; override via YAZE_BUILD_DIR).
# Usage: ./scripts/agent_build.sh [target]
# Default target is "yaze" if not specified.

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

# For AI-enabled presets (mac-ai/win-ai/lin-ai), the preset uses binaryDir
# "build_ai". Default to that here so we consistently build the most feature-
# complete binaries unless explicitly overridden.
BUILD_DIR="${YAZE_BUILD_DIR:-build_ai}"
TARGET="${1:-yaze}"

echo "=================================================="
echo "🤖 Agent Build System"
echo "Platform: ${OS}"
echo "Preset:   ${PRESET}"
echo "Build Dir: ${BUILD_DIR}"
echo "Target:   ${TARGET}"
echo "=================================================="

# Ensure we are in the project root
if [ ! -f "CMakePresets.json" ]; then
    echo "❌ Error: CMakePresets.json not found. Must run from project root."
    exit 1
fi

# Configure if needed (using the preset which now enforces binaryDir)
if [ ! -d "${BUILD_DIR}" ]; then
    echo "🔧 Configuring ${PRESET}..."
    cmake --preset "${PRESET}"
fi

# Build
echo "🔨 Building target: ${TARGET}..."
cmake --build "${BUILD_DIR}" --target "${TARGET}"

echo "✅ Build complete."
