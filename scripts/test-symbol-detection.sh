#!/bin/bash
# Integration Test - Symbol Conflict Detection System
#
# Verifies that extract-symbols and check-duplicate-symbols work correctly
# This script is NOT for CI, but for manual testing and validation

set -euo pipefail

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "${SCRIPT_DIR}")"
BUILD_DIR="${PROJECT_ROOT}/build"

echo -e "${BLUE}=== Symbol Detection System Test ===${NC}"
echo ""

# Test 1: Verify scripts are executable
echo -e "${CYAN}[Test 1]${NC} Verifying scripts are executable..."
if [[ -x "${SCRIPT_DIR}/extract-symbols.sh" ]]; then
    echo -e "  ${GREEN}✓${NC} extract-symbols.sh is executable"
else
    echo -e "  ${RED}✗${NC} extract-symbols.sh is NOT executable"
    exit 1
fi

if [[ -x "${SCRIPT_DIR}/check-duplicate-symbols.sh" ]]; then
    echo -e "  ${GREEN}✓${NC} check-duplicate-symbols.sh is executable"
else
    echo -e "  ${RED}✗${NC} check-duplicate-symbols.sh is NOT executable"
    exit 1
fi

if [[ -x "${PROJECT_ROOT}/.githooks/pre-commit" ]]; then
    echo -e "  ${GREEN}✓${NC} .githooks/pre-commit is executable"
else
    echo -e "  ${RED}✗${NC} .githooks/pre-commit is NOT executable"
    exit 1
fi
echo ""

# Test 2: Verify build directory exists
echo -e "${CYAN}[Test 2]${NC} Checking build directory..."
if [[ -d "${BUILD_DIR}" ]]; then
    echo -e "  ${GREEN}✓${NC} Build directory exists: ${BUILD_DIR}"
else
    echo -e "  ${RED}✗${NC} Build directory not found: ${BUILD_DIR}"
    exit 1
fi

# Check for object files
OBJ_COUNT=$(find "${BUILD_DIR}" -name "*.o" -o -name "*.obj" 2>/dev/null | wc -l)
if [[ ${OBJ_COUNT} -gt 0 ]]; then
    echo -e "  ${GREEN}✓${NC} Found ${OBJ_COUNT} object files"
else
    echo -e "  ${RED}✗${NC} No object files found (run cmake --build build first)"
    exit 1
fi
echo ""

# Test 3: Run extract-symbols
echo -e "${CYAN}[Test 3]${NC} Running symbol extraction..."
if cd "${PROJECT_ROOT}" && ./scripts/extract-symbols.sh "${BUILD_DIR}" 2>&1 | tail -5; then
    echo -e "  ${GREEN}✓${NC} Symbol extraction completed"
else
    echo -e "  ${RED}✗${NC} Symbol extraction failed"
    exit 1
fi
echo ""

# Test 4: Verify symbol database was created
echo -e "${CYAN}[Test 4]${NC} Verifying symbol database..."
DB_FILE="${BUILD_DIR}/symbol_database.json"
if [[ -f "${DB_FILE}" ]]; then
    SIZE=$(du -h "${DB_FILE}" | cut -f1)
    echo -e "  ${GREEN}✓${NC} Symbol database created: ${DB_FILE} (${SIZE})"
else
    echo -e "  ${RED}✗${NC} Symbol database not created"
    exit 1
fi
echo ""

# Test 5: Verify JSON structure
echo -e "${CYAN}[Test 5]${NC} Validating JSON structure..."
if python3 -m json.tool "${DB_FILE}" > /dev/null 2>&1; then
    echo -e "  ${GREEN}✓${NC} Valid JSON format"
else
    echo -e "  ${RED}✗${NC} Invalid JSON format"
    exit 1
fi

# Check for expected fields
if python3 << PYTHON_EOF
import json
with open("${DB_FILE}") as f:
    data = json.load(f)

required = ["metadata", "conflicts", "symbols"]
for field in required:
    if field not in data:
        exit(1)

metadata = data.get("metadata", {})
required_meta = ["platform", "build_dir", "timestamp", "object_files_scanned", "total_symbols"]
for field in required_meta:
    if field not in metadata:
        exit(1)

print("  ${GREEN}✓${NC} JSON structure is correct")
exit(0)
PYTHON_EOF
; then
    : # Already printed
else
    echo -e "  ${RED}✗${NC} JSON structure validation failed"
    exit 1
fi
echo ""

# Test 6: Run duplicate checker
echo -e "${CYAN}[Test 6]${NC} Running duplicate symbol checker..."
if cd "${PROJECT_ROOT}" && ./scripts/check-duplicate-symbols.sh "${DB_FILE}" 2>&1 | tail -10; then
    echo -e "  ${GREEN}✓${NC} Duplicate checker completed (no conflicts)"
else
    # Duplicate checker returns 1 if conflicts found, which is expected behavior
    exit_code=$?
    if [[ ${exit_code} -eq 1 ]]; then
        echo -e "  ${YELLOW}✓${NC} Duplicate checker found conflicts (expected in some cases)"
    else
        echo -e "  ${RED}✗${NC} Duplicate checker failed with error"
        exit ${exit_code}
    fi
fi
echo ""

# Test 7: Verify pre-commit hook
echo -e "${CYAN}[Test 7]${NC} Checking pre-commit hook setup..."
HOOK_PATH="${PROJECT_ROOT}/.githooks/pre-commit"
if [[ -f "${HOOK_PATH}" ]]; then
    echo -e "  ${GREEN}✓${NC} Pre-commit hook exists"

    # Check if git hooks are configured
    cd "${PROJECT_ROOT}"
    if git config core.hooksPath 2>/dev/null | grep -q "\.githooks"; then
        echo -e "  ${GREEN}✓${NC} Git hooks path configured: $(git config core.hooksPath)"
    else
        echo -e "  ${YELLOW}!${NC} Git hooks path not configured"
        echo -e "     Run: ${CYAN}git config core.hooksPath .githooks${NC}"
    fi
else
    echo -e "  ${RED}✗${NC} Pre-commit hook not found: ${HOOK_PATH}"
    exit 1
fi
echo ""

# Test 8: Display sample output
echo -e "${CYAN}[Test 8]${NC} Sample symbol database contents..."
python3 << PYTHON_EOF
import json

with open("${DB_FILE}") as f:
    data = json.load(f)

meta = data.get("metadata", {})
print(f"  Platform: {meta.get('platform', '?')}")
print(f"  Object files: {meta.get('object_files_scanned', '?')}")
print(f"  Total symbols: {meta.get('total_symbols', '?')}")
print(f"  Conflicts: {meta.get('total_conflicts', 0)}")

conflicts = data.get("conflicts", [])
if conflicts:
    print(f"\n  Sample conflicts:")
    for conflict in conflicts[:3]:
        symbol = conflict.get("symbol", "?")
        count = conflict.get("count", 0)
        print(f"    - {symbol} (x{count})")
    if len(conflicts) > 3:
        print(f"    ... and {len(conflicts) - 3} more")
PYTHON_EOF
echo ""

# Final Summary
echo -e "${GREEN}=== All Tests Passed! ===${NC}"
echo ""
echo -e "Next steps:"
echo -e "  1. Configure git hooks: ${CYAN}git config core.hooksPath .githooks${NC}"
echo -e "  2. Review symbol database: ${CYAN}cat ${DB_FILE} | head -50${NC}"
echo -e "  3. Check for conflicts: ${CYAN}./scripts/check-duplicate-symbols.sh${NC}"
echo -e "  4. Integrate into CI: See docs/internal/testing/symbol-conflict-detection.md"
echo ""
