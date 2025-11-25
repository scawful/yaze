#!/bin/bash
# GUI Test Script for YAZE Palette Verification

set -e

APP_PATH="/Users/scawful/Code/yaze/build_ai/bin/Debug/yaze.app"
SCREENSHOT_DIR="$HOME/Desktop/yaze_test"
mkdir -p "$SCREENSHOT_DIR"

echo "=== YAZE Palette Test ==="
echo "1. Launching application..."

# Launch app in background
open "$APP_PATH"

# Wait for app to start
sleep 3

echo "2. Taking screenshot..."
screencapture -x "$SCREENSHOT_DIR/yaze_window.png"

echo "3. Screenshot saved to: $SCREENSHOT_DIR/yaze_window.png"
echo ""
echo "To analyze palette colors, you can:"
echo "  - Open the image in Preview and use Digital Color Meter"
echo "  - Use ImageMagick: convert yaze_window.png -crop 100x100+X+Y -scale 1x1\! txt:-"
echo ""
echo "Killing app..."
pkill -f "yaze.app" || true

echo "Done!"
