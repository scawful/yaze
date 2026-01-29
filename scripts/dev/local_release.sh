#!/bin/bash
set -eo pipefail

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${GREEN}=== YAZE Local Release Build ===${NC}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$ROOT_DIR/build_release"

# 1. Prepare Directory
echo -e "\n${GREEN}[1/3] Configuring Release Build...${NC}"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# Check for Ninja
GENERATOR="Unix Makefiles"
if command -v ninja >/dev/null; then
    GENERATOR="Ninja"
fi

cd "$ROOT_DIR"
if cmake -S . -B "$BUILD_DIR" -G "$GENERATOR" -DCMAKE_BUILD_TYPE=Release -DYAZE_BUILD_TESTS=OFF; then
    echo -e "${GREEN}Configuration Successful${NC}"
else
    echo -e "${RED}Configuration Failed${NC}"
    exit 1
fi

# 2. Build
echo -e "\n${GREEN}[2/3] Building Project...${NC}"
if cmake --build "$BUILD_DIR" --parallel; then
     echo -e "${GREEN}Build Successful${NC}"
else
     echo -e "${RED}Build Failed${NC}"
     exit 1
fi

# 3. Package
echo -e "\n${GREEN}[3/3] Packaging Artifacts...${NC}"
cd "$BUILD_DIR"
if cpack -G DragNDrop; then
    echo -e "${GREEN}Packaging Successful${NC}"
else
    echo -e "${RED}Packaging Failed${NC}"
    exit 1
fi

echo -e "\n${GREEN}=== Release Build Complete ===${NC}"
echo "Artifacts located in $BUILD_DIR/packages/"
ls -lh packages/*.dmg 2>/dev/null || true
