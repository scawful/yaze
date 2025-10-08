#!/usr/bin/env bash
# YAZE Collaboration Server Launcher
# Starts the WebSocket collaboration server for networked YAZE sessions

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
SERVER_DIR="$PROJECT_ROOT/../yaze-server"

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${GREEN}🚀 YAZE Collaboration Server Launcher${NC}"
echo ""

# Check if server directory exists
if [ ! -d "$SERVER_DIR" ]; then
    echo -e "${RED}Error: Collaboration server not found at $SERVER_DIR${NC}"
    echo "Please ensure yaze-server is cloned alongside the yaze repository."
    exit 1
fi

cd "$SERVER_DIR"

# Check if Node.js is installed
if ! command -v node &> /dev/null; then
    echo -e "${RED}Error: Node.js is not installed${NC}"
    echo "Please install Node.js from https://nodejs.org/"
    exit 1
fi

# Check if npm packages are installed
if [ ! -d "node_modules" ]; then
    echo -e "${YELLOW}Installing server dependencies...${NC}"
    npm install
fi

# Get port from argument or use default
PORT="${1:-8765}"

echo -e "${GREEN}Starting collaboration server on port $PORT...${NC}"
echo ""
echo "Server will be accessible at:"
echo "  • ws://localhost:$PORT (local)"
echo "  • ws://$(hostname):$PORT (network)"
echo ""
echo -e "${YELLOW}Press Ctrl+C to stop the server${NC}"
echo ""

# Start the server
PORT="$PORT" node server.js
