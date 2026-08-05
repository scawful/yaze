#!/bin/bash
# Test Linux build in Docker container (simulates CI environment)
set -e

echo "🐧 Testing Linux build in Docker container..."
BUILD_JOBS="${YAZE_BUILD_JOBS:-${CMAKE_BUILD_PARALLEL_LEVEL:-4}}"

# Use same Ubuntu version as CI
docker run --rm -v "$PWD:/workspace" -w /workspace \
  -e YAZE_BUILD_JOBS="$BUILD_JOBS" \
  ubuntu:22.04 bash -c '
    set -e
    echo "📦 Installing dependencies..."
    export DEBIAN_FRONTEND=noninteractive
    apt-get update
    apt-get install -y \
      build-essential cmake ninja-build pkg-config gcc-12 g++-12 \
      libglew-dev libxext-dev libwavpack-dev libboost-all-dev \
      libpng-dev python3-dev libpython3-dev \
      libasound2-dev libpulse-dev libaudio-dev \
      libx11-dev libxrandr-dev libxcursor-dev libxinerama-dev libxi-dev \
      libxss-dev libxxf86vm-dev libxkbcommon-dev libwayland-dev libdecor-0-dev \
      libgtk-3-dev libdbus-1-dev git

    echo "⚙️  Configuring build..."
    cmake -B build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=gcc-12 \
      -DCMAKE_CXX_COMPILER=g++-12 \
      -DYAZE_BUILD_TESTS=OFF \
      -DYAZE_BUILD_EMU=ON \
      -DYAZE_BUILD_Z3ED=ON \
      -DYAZE_BUILD_TOOLS=ON \
      -DNFD_PORTAL=ON

    echo "🔨 Building..."
    cmake --build build --parallel "$YAZE_BUILD_JOBS"

    echo "✅ Linux build succeeded!"
    ls -lh build/bin/
'
