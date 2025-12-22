#!/bin/bash
# Static analysis script to find potentially unsafe array accesses
# that could cause "index out of bounds" errors in WASM
#
# Run from yaze root: ./scripts/find-unsafe-array-access.sh

set -e

RED='\033[0;31m'
YELLOW='\033[1;33m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

echo "====================================="
echo "YAZE Unsafe Array Access Scanner"
echo "====================================="
echo ""

# Directory to scan
SCAN_DIR="${1:-src}"

echo "Scanning: $SCAN_DIR"
echo ""

# Pattern categories - each needs manual review
declare -a CRITICAL_PATTERNS=(
    # Direct buffer access without bounds check
    'tiledata\[[^]]+\]'
    'gfx_sheets_\[[^]]+\]'
    'canvas\[[^]]+\]'
    'pixels\[[^]]+\]'
    'buffer_\[[^]]+\]'
    'tiles_\[[^]]+\]'
    '\.data\(\)\[[^]]+\]'
)

declare -a HIGH_PATTERNS=(
    # ROM data access
    'rom\.data\(\)\[[^]]+\]'
    'rom_data\[[^]]+\]'
    # Palette access
    'palette\[[^]]+\]'
    '->colors\[[^]]+\]'
    # Map/room access
    'overworld_maps_\[[^]]+\]'
    'rooms_\[[^]]+\]'
    'sprites_\[[^]]+\]'
)

declare -a MEDIUM_PATTERNS=(
    # Graphics sheet access
    'gfx_sheet\([^)]+\)'
    'mutable_gfx_sheet\([^)]+\)'
    # VRAM/CGRAM/OAM (usually masked, but worth checking)
    'vram\[[^]]+\]'
    'cgram\[[^]]+\]'
    'oam\[[^]]+\]'
)

echo "=== CRITICAL: Direct buffer access patterns ==="
echo "(These are most likely to cause WASM crashes)"
echo ""

for pattern in "${CRITICAL_PATTERNS[@]}"; do
    echo -e "${RED}Pattern: $pattern${NC}"
    grep -rn --include="*.cc" --include="*.h" -E "$pattern" "$SCAN_DIR" 2>/dev/null | \
        grep -v "test/" | grep -v "_test.cc" | head -20 || echo "  No matches"
    echo ""
done

echo "=== HIGH: ROM/Map/Sprite data patterns ==="
echo "(These access external data that may be corrupt)"
echo ""

for pattern in "${HIGH_PATTERNS[@]}"; do
    echo -e "${YELLOW}Pattern: $pattern${NC}"
    grep -rn --include="*.cc" --include="*.h" -E "$pattern" "$SCAN_DIR" 2>/dev/null | \
        grep -v "test/" | grep -v "_test.cc" | head -20 || echo "  No matches"
    echo ""
done

echo "=== MEDIUM: Graphics accessor patterns ==="
echo "(Usually safe but verify bounds checks exist)"
echo ""

for pattern in "${MEDIUM_PATTERNS[@]}"; do
    echo -e "${GREEN}Pattern: $pattern${NC}"
    grep -rn --include="*.cc" --include="*.h" -E "$pattern" "$SCAN_DIR" 2>/dev/null | \
        grep -v "test/" | grep -v "_test.cc" | head -20 || echo "  No matches"
    echo ""
done

echo "====================================="
echo "Analysis complete."
echo ""
echo "GUIDELINES FOR FIXES:"
echo "1. Add bounds validation BEFORE array access"
echo "2. Use early return for invalid indices"
echo "3. Consider using .at() for checked access in debug builds"
echo "4. For tile data: validate tile_id < 0x400 (64 rows * 16 cols)"
echo "5. For palettes: validate index < palette_size"
echo "6. For graphics sheets: validate index < 223"
echo "7. For ROM data: validate offset < rom.size()"
echo "====================================="
