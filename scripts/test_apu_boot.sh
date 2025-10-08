#!/bin/bash
# Test script for APU/SPC700 boot sequence debugging
# Runs yaze with specific filters to show only critical APU events

cd /Users/scawful/Code/yaze

echo "========================================="
echo "Testing ALTTP APU Boot Sequence"
echo "========================================="
echo ""

# Run yaze and filter for key events
./build/bin/yaze.app/Contents/MacOS/yaze 2>&1 | \
grep -E "(Frame [0-9]+: CPU|SPC wrote port.*F4|MOVSY writing|CPU read.*2140.*=.*(AA|BB|CC|01|02)|CPU wrote.*214[012]|TRANSFER LOOP.*FFDF)" | \
head -100

echo ""
echo "========================================="
echo "Test complete - check output above for:"
echo "1. Initial handshake ($AA/$BB)"
echo "2. Command echo ($CC)"
echo "3. Counter acknowledgments ($00, $01, $02...)"
echo "4. CPU progress beyond $88xx range"
echo "========================================="

