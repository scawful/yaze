#!/bin/bash
# Quick Start Script for Testing Ollama Integration with z3ed
# Usage: ./scripts/quickstart_ollama.sh

set -e

echo "🚀 z3ed + Ollama Quick Start"
echo "================================"
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Step 1: Check if Ollama is installed
echo "📦 Step 1: Checking Ollama installation..."
if ! command -v ollama &> /dev/null; then
    echo -e "${RED}✗ Ollama not found${NC}"
    echo ""
    echo "Install Ollama with:"
    echo "  macOS:  brew install ollama"
    echo "  Linux:  curl -fsSL https://ollama.com/install.sh | sh"
    echo ""
    exit 1
fi
echo -e "${GREEN}✓ Ollama installed${NC}"
echo ""

# Step 2: Check if Ollama server is running
echo "🔌 Step 2: Checking Ollama server..."
if ! curl -s http://localhost:11434/api/tags > /dev/null 2>&1; then
    echo -e "${YELLOW}⚠ Ollama server not running${NC}"
    echo ""
    echo "Starting Ollama server in background..."
    ollama serve > /dev/null 2>&1 &
    OLLAMA_PID=$!
    echo "Waiting for server to start..."
    sleep 3
    
    if ! curl -s http://localhost:11434/api/tags > /dev/null 2>&1; then
        echo -e "${RED}✗ Failed to start Ollama server${NC}"
        exit 1
    fi
    echo -e "${GREEN}✓ Ollama server started (PID: $OLLAMA_PID)${NC}"
else
    echo -e "${GREEN}✓ Ollama server running${NC}"
fi
echo ""

# Step 3: Check if recommended model is available
RECOMMENDED_MODEL="qwen2.5-coder:7b"
echo "🤖 Step 3: Checking for model: $RECOMMENDED_MODEL..."
if ! ollama list | grep -q "$RECOMMENDED_MODEL"; then
    echo -e "${YELLOW}⚠ Model not found${NC}"
    echo ""
    read -p "Pull $RECOMMENDED_MODEL? (~4.7GB download) [y/N]: " -n 1 -r
    echo ""
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "Pulling model (this may take a few minutes)..."
        ollama pull "$RECOMMENDED_MODEL"
        echo -e "${GREEN}✓ Model pulled successfully${NC}"
    else
        echo -e "${RED}✗ Model required for testing${NC}"
        exit 1
    fi
else
    echo -e "${GREEN}✓ Model available${NC}"
fi
echo ""

# Step 4: Check if z3ed is built
echo "🔨 Step 4: Checking z3ed build..."
if [ ! -f "./build/bin/z3ed" ]; then
    echo -e "${YELLOW}⚠ z3ed not found in ./build/bin/${NC}"
    echo ""
    echo "Building z3ed..."
    cmake --build build --target z3ed
    if [ ! -f "./build/bin/z3ed" ]; then
        echo -e "${RED}✗ Failed to build z3ed${NC}"
        exit 1
    fi
fi
echo -e "${GREEN}✓ z3ed ready${NC}"
echo ""

# Step 5: Test Ollama integration
echo "🧪 Step 5: Testing z3ed + Ollama integration..."
export YAZE_AI_PROVIDER=ollama
export OLLAMA_MODEL="$RECOMMENDED_MODEL"

echo ""
echo "Running test command:"
echo -e "${BLUE}z3ed agent plan --prompt \"Validate the ROM file\"${NC}"
echo ""

if ./build/bin/z3ed agent plan --prompt "Validate the ROM file"; then
    echo ""
    echo -e "${GREEN}✓ Integration test passed!${NC}"
else
    echo ""
    echo -e "${RED}✗ Integration test failed${NC}"
    echo "Check error messages above for details"
    exit 1
fi

echo ""
echo "================================"
echo -e "${GREEN}🎉 Setup Complete!${NC}"
echo ""
echo "Next steps:"
echo "  1. Try a full agent run:"
echo "     export YAZE_AI_PROVIDER=ollama"
echo "     z3ed agent run --prompt \"Export first palette\" --rom zelda3.sfc --sandbox"
echo ""
echo "  2. Review generated commands:"
echo "     z3ed agent list"
echo "     z3ed agent diff"
echo ""
echo "  3. Try different models:"
echo "     ollama pull codellama:13b"
echo "     export OLLAMA_MODEL=codellama:13b"
echo ""
echo "  4. Read the docs:"
echo "     docs/z3ed/LLM-INTEGRATION-PLAN.md"
echo ""
