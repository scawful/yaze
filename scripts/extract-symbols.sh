#!/bin/bash
# Symbol Extraction Tool - Extract symbols from compiled object files
# Creates a JSON database of all symbols and their defining object files
#
# Usage: ./scripts/extract-symbols.sh [BUILD_DIR] [OUTPUT_FILE]
#   BUILD_DIR:   Path to CMake build directory (default: build)
#   OUTPUT_FILE: Path to output JSON file (default: build/symbol_database.json)

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
BUILD_DIR="${1:-.}"
OUTPUT_FILE="${2:-${BUILD_DIR}/symbol_database.json}"
TEMP_SYMBOLS="${BUILD_DIR}/.temp_symbols.txt"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "${SCRIPT_DIR}")"

# Platform detection
UNAME_S=$(uname -s)
IS_MACOS=false
IS_LINUX=false
IS_WINDOWS=false

case "${UNAME_S}" in
    Darwin*) IS_MACOS=true ;;
    Linux*) IS_LINUX=true ;;
    MINGW*|MSYS*|CYGWIN*) IS_WINDOWS=true ;;
esac

# Validation
if [[ ! -d "${BUILD_DIR}" ]]; then
    echo -e "${RED}Error: Build directory not found: ${BUILD_DIR}${NC}"
    exit 1
fi

echo -e "${BLUE}=== Symbol Extraction Tool ===${NC}"
echo -e "Build directory: ${BUILD_DIR}"
echo -e "Output file: ${OUTPUT_FILE}"
echo ""

# Function to extract symbols using nm (Unix/macOS)
extract_symbols_unix() {
    local obj_file="$1"
    local obj_name="${obj_file##*/}"

    if ! nm -P "${obj_file}" 2>/dev/null | while read -r sym rest; do
        # Filter out special symbols and undefined references
        if [[ -n "${sym}" ]] && [[ "${rest}" != *"U"* ]]; then
            # Get symbol type (T=text, D=data, R=read-only, etc.)
            local sym_type=$(echo "${rest}" | awk '{print $1}')
            if [[ "${sym_type}" != "U" ]]; then
                echo "${sym}|${obj_name}|${sym_type}"
            fi
        fi
    done; then
        return 1
    fi
    return 0
}

# Function to extract symbols using dumpbin (Windows)
extract_symbols_windows() {
    local obj_file="$1"
    local obj_name="${obj_file##*/}"

    # Use dumpbin to extract symbols
    if dumpbin /symbols "${obj_file}" 2>/dev/null | grep -E "^\s+[0-9A-F]+" | while read -r line; do
        # Parse dumpbin output
        local sym=$(echo "${line}" | awk '{print $NF}')
        if [[ -n "${sym}" ]]; then
            local sym_type="?" # Windows dumpbin doesn't clearly show type
            echo "${sym}|${obj_name}|${sym_type}"
        fi
    done; then
        return 1
    fi
    return 0
}

# Function to collect all object files
collect_object_files() {
    local obj_list="${BUILD_DIR}/.object_files.tmp"
    > "${obj_list}"

    # Find all .o files (Unix/macOS) and .obj files (Windows)
    if ${IS_WINDOWS}; then
        find "${BUILD_DIR}" -type f \( -name "*.obj" -o -name "*.o" \) 2>/dev/null >> "${obj_list}" || true
    else
        find "${BUILD_DIR}" -type f -name "*.o" 2>/dev/null >> "${obj_list}" || true
    fi

    echo "${obj_list}"
}

# Extract symbols from all object files
echo -e "${BLUE}Scanning for object files...${NC}"
OBJ_LIST=$(collect_object_files)
OBJ_COUNT=$(wc -l < "${OBJ_LIST}")

if [[ ${OBJ_COUNT} -eq 0 ]]; then
    echo -e "${YELLOW}Warning: No object files found in ${BUILD_DIR}${NC}"
    echo "Make sure to build the project first."
    exit 1
fi

echo -e "Found ${GREEN}${OBJ_COUNT}${NC} object files"
echo ""
echo -e "${BLUE}Extracting symbols (this may take a moment)...${NC}"

# Process object files and extract symbols
: > "${TEMP_SYMBOLS}"
PROCESSED=0
FAILED=0

while IFS= read -r obj_file; do
    [[ -z "${obj_file}" ]] && continue

    if [[ ! -f "${obj_file}" ]]; then
        echo -e "${YELLOW}Skipping (not found): ${obj_file}${NC}"
        ((FAILED++))
        continue
    fi

    # Extract symbols based on platform
    if ${IS_WINDOWS}; then
        if extract_symbols_windows "${obj_file}" >> "${TEMP_SYMBOLS}" 2>/dev/null; then
            ((PROCESSED++))
        else
            ((FAILED++))
        fi
    else
        if extract_symbols_unix "${obj_file}" >> "${TEMP_SYMBOLS}" 2>/dev/null; then
            ((PROCESSED++))
        else
            ((FAILED++))
        fi
    fi

    # Progress indicator
    if (( PROCESSED % 50 == 0 )); then
        echo -ne "\r  Processed: ${PROCESSED}/${OBJ_COUNT} objects"
    fi
done < "${OBJ_LIST}"

echo -ne "\r  Processed: ${GREEN}${PROCESSED}${NC}/${OBJ_COUNT} objects (${FAILED} failed)     \n"
echo ""

# Generate JSON output
echo -e "${BLUE}Generating symbol database...${NC}"

# Start JSON
cat > "${OUTPUT_FILE}" << 'EOF'
{
  "metadata": {
    "platform": "",
    "build_dir": "",
    "timestamp": "",
    "object_files_scanned": 0,
    "total_symbols": 0
  },
  "conflicts": [],
  "symbols": {}
}
EOF

# Use Python to generate proper JSON (more portable than jq)
python3 << PYTHON_EOF
import json
import sys
from datetime import datetime
from collections import defaultdict

# Read extracted symbols
symbol_dict = defaultdict(list)
total_symbols = 0

try:
    with open("${TEMP_SYMBOLS}", "r") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            parts = line.split("|")
            if len(parts) >= 2:
                symbol = parts[0]
                obj_file = parts[1]
                sym_type = parts[2] if len(parts) > 2 else "?"

                symbol_dict[symbol].append({
                    "object_file": obj_file,
                    "type": sym_type
                })
                total_symbols += 1
except Exception as e:
    print(f"Error reading symbols: {e}", file=sys.stderr)
    sys.exit(1)

# Identify conflicts (symbols defined in multiple object files)
conflicts = []
for symbol, definitions in symbol_dict.items():
    if len(definitions) > 1:
        conflicts.append({
            "symbol": symbol,
            "count": len(definitions),
            "definitions": definitions
        })

# Sort conflicts by count (most duplicated first)
conflicts.sort(key=lambda x: x["count"], reverse=True)

# Build output JSON
output = {
    "metadata": {
        "platform": "${UNAME_S}",
        "build_dir": "${BUILD_DIR}",
        "timestamp": datetime.utcnow().isoformat() + "Z",
        "object_files_scanned": ${PROCESSED},
        "total_symbols": total_symbols,
        "total_conflicts": len(conflicts)
    },
    "conflicts": conflicts,
    "symbols": {}
}

# Add symbols to output (optional - only include conflicted symbols for smaller file)
for symbol, definitions in symbol_dict.items():
    if len(definitions) > 1:
        output["symbols"][symbol] = definitions

# Write JSON
try:
    with open("${OUTPUT_FILE}", "w") as f:
        json.dump(output, f, indent=2)
except Exception as e:
    print(f"Error writing JSON: {e}", file=sys.stderr)
    sys.exit(1)

print(f"Symbol database written to: ${OUTPUT_FILE}")
print(f"Total symbols: {total_symbols}")
print(f"Conflicts found: {len(conflicts)}")
PYTHON_EOF

# Cleanup
rm -f "${TEMP_SYMBOLS}" "${OBJ_LIST}"

# Report results
if [[ -f "${OUTPUT_FILE}" ]]; then
    echo -e "${GREEN}Success!${NC}"
    CONFLICT_COUNT=$(python3 -c "import json; f = json.load(open('${OUTPUT_FILE}')); print(f['metadata'].get('total_conflicts', 0))" 2>/dev/null || echo "?")

    if [[ "${CONFLICT_COUNT}" -gt 0 ]]; then
        echo -e "${YELLOW}Found ${RED}${CONFLICT_COUNT}${YELLOW} symbol conflicts${NC}"
        exit 1  # Exit with error if conflicts found
    else
        echo -e "${GREEN}No symbol conflicts detected!${NC}"
        exit 0
    fi
else
    echo -e "${RED}Failed to generate symbol database${NC}"
    exit 1
fi
