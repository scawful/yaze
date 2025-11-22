#!/bin/bash
# Duplicate Symbol Checker - Analyze symbol database for conflicts
#
# Usage: ./scripts/check-duplicate-symbols.sh [SYMBOL_DB] [--verbose] [--fix-suggestions]
#   SYMBOL_DB:     Path to symbol database JSON (default: build/symbol_database.json)
#   --verbose:     Show all symbols (not just conflicts)
#   --fix-suggestions: Include suggested fixes for conflicts

set -euo pipefail

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# Configuration
SYMBOL_DB="${1:-.}"
VERBOSE=false
FIX_SUGGESTIONS=false

# If first arg is a flag, use default database
if [[ "${SYMBOL_DB}" == --* ]]; then
    SYMBOL_DB="."
fi

# Handle case where SYMBOL_DB is a directory
if [[ -d "${SYMBOL_DB}" ]]; then
    SYMBOL_DB="${SYMBOL_DB}/symbol_database.json"
fi

# Parse additional arguments
for arg in "$@"; do
    case "${arg}" in
        --verbose) VERBOSE=true ;;
        --fix-suggestions) FIX_SUGGESTIONS=true ;;
    esac
done

# Validation
if [[ ! -f "${SYMBOL_DB}" ]]; then
    echo -e "${RED}Error: Symbol database not found: ${SYMBOL_DB}${NC}"
    echo "Generate it first with: ./scripts/extract-symbols.sh"
    exit 1
fi

# Function to show a symbol conflict with details
show_conflict() {
    local symbol="$1"
    local count="$2"
    local definitions_json="$3"

    echo -e "\n${RED}SYMBOL CONFLICT DETECTED${NC}"
    echo -e "  Symbol: ${CYAN}${symbol}${NC}"
    echo -e "  Defined in: ${RED}${count} object files${NC}"

    # Parse JSON and show each definition
    python3 << PYTHON_EOF
import json
import sys

definitions = json.loads('''${definitions_json}''')

for i, defn in enumerate(definitions, 1):
    obj_file = defn.get('object_file', '?')
    sym_type = defn.get('type', '?')
    print(f"    {i}. {obj_file} (type: {sym_type})")
PYTHON_EOF

    # Show fix suggestions if requested
    if ${FIX_SUGGESTIONS}; then
        echo -e "\n  ${YELLOW}Suggested fixes:${NC}"
        echo "    1. Add 'static' or 'inline' to make the symbol have internal linkage"
        echo "    2. Move definition to a header file with inline/constexpr"
        echo "    3. Use anonymous namespace {} in .cc file"
        echo "    4. Use 'extern' keyword to declare without defining"
        echo "    5. Use ODR-friendly patterns (Meyers' singleton, etc.)"
    fi
}

# Main analysis
echo -e "${BLUE}=== Duplicate Symbol Checker ===${NC}"
echo -e "Database: ${SYMBOL_DB}"
echo ""

# Parse JSON and check for conflicts
python3 << PYTHON_EOF
import json
import sys

try:
    with open("${SYMBOL_DB}", "r") as f:
        data = json.load(f)
except Exception as e:
    print(f"${RED}Error reading database: {e}${NC}", file=sys.stderr)
    sys.exit(1)

metadata = data.get("metadata", {})
conflicts = data.get("conflicts", [])
symbols = data.get("symbols", {})

# Display metadata
print(f"Platform: {metadata.get('platform', '?')}")
print(f"Build directory: {metadata.get('build_dir', '?')}")
print(f"Timestamp: {metadata.get('timestamp', '?')}")
print(f"Object files scanned: {metadata.get('object_files_scanned', 0)}")
print(f"Total symbols: {metadata.get('total_symbols', 0)}")
print(f"Total conflicts: {len(conflicts)}")
print("")

# Show conflicts
if conflicts:
    print(f"${RED}CONFLICTS FOUND:${NC}\n")

    for i, conflict in enumerate(conflicts, 1):
        symbol = conflict.get("symbol", "?")
        count = conflict.get("count", 0)
        definitions = conflict.get("definitions", [])

        print(f"${RED}[{i}/{len(conflicts)}]${NC} {symbol} (x{count})")
        for j, defn in enumerate(definitions, 1):
            obj = defn.get("object_file", "?")
            sym_type = defn.get("type", "?")
            print(f"      {j}. {obj} (type: {sym_type})")
        print("")

    print(f"${RED}=== Summary ===${NC}")
    print(f"Total conflicts: ${RED}{len(conflicts)}${NC}")
    print(f"Fix these before linking!${NC}")
    sys.exit(1)
else:
    print(f"${GREEN}No conflicts found! Symbol table is clean.${NC}")
    sys.exit(0)

PYTHON_EOF

exit_code=$?
exit ${exit_code}
