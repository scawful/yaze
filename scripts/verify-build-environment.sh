#!/bin/bash
# YAZE Build Environment Verification Script for Unix/macOS
# This script verifies the build environment is properly configured and ready to build
# Run this before building to catch common configuration issues early

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Flags
VERBOSE=0
FIX_ISSUES=0
CLEAN_CACHE=0

# Counters
ISSUES_FOUND=()
WARNINGS=()
SUCCESS=()

# Parse arguments
while [[ $# -gt 0 ]]; do
  case $1 in
    -v|--verbose)
      VERBOSE=1
      shift
      ;;
    -f|--fix)
      FIX_ISSUES=1
      shift
      ;;
    -c|--clean)
      CLEAN_CACHE=1
      shift
      ;;
    -h|--help)
      echo "Usage: $0 [OPTIONS]"
      echo "Options:"
      echo "  -v, --verbose    Verbose output"
      echo "  -f, --fix        Automatically fix issues"
      echo "  -c, --clean      Clean CMake cache"
      echo "  -h, --help       Show this help message"
      exit 0
      ;;
    *)
      echo "Unknown option: $1"
      exit 1
      ;;
  esac
done

function write_status() {
    local message="$1"
    local type="${2:-Info}"
    local timestamp=$(date +"%H:%M:%S")
    
    case "$type" in
        Success)
            echo -e "[$timestamp] ${GREEN}✓${NC} $message"
            ;;
        Error)
            echo -e "[$timestamp] ${RED}✗${NC} $message"
            ;;
        Warning)
            echo -e "[$timestamp] ${YELLOW}⚠${NC} $message"
            ;;
        Info)
            echo -e "[$timestamp] ${CYAN}ℹ${NC} $message"
            ;;
        Step)
            echo -e "\n[$timestamp] ${BLUE}▶${NC} $message"
            ;;
    esac
}

function test_command() {
    command -v "$1" &> /dev/null
}

function get_cmake_version() {
    if test_command cmake; then
        cmake --version 2>&1 | head -n1 | grep -oE '[0-9]+\.[0-9]+\.[0-9]+'
    fi
}

function test_git_submodules() {
    local submodules=(
    "ext/SDL"
    "src/lib/abseil-cpp"
    "ext/asar"
    "ext/imgui"
    "ext/imgui_test_engine"
    "ext/nativefiledialog-extended"
    )
    
    local all_present=1
    for submodule in "${submodules[@]}"; do
        if [[ ! -d "$submodule" ]] || [[ -z "$(ls -A "$submodule" 2>/dev/null)" ]]; then
            write_status "Submodule missing or empty: $submodule" "Error"
            ISSUES_FOUND+=("Missing/empty submodule: $submodule")
            all_present=0
        elif [[ $VERBOSE -eq 1 ]]; then
            write_status "Submodule found: $submodule" "Success"
        fi
    done
    return $all_present
}

function test_cmake_cache() {
    local build_dirs=(
        "build"
        "build-wasm"
        "build-test"
        "build-grpc-test"
        "build-rooms"
        "build-windows"
        "build_ai"
        "build_ai_claude"
        "build_agent"
        "build_ci"
        "build_fast"
        "build_test"
        "build-wasm-debug"
        "build_wasm_ai"
        "build_wasm"
    )
    local cache_issues=0
    
    for dir in "${build_dirs[@]}"; do
        if [[ -f "$dir/CMakeCache.txt" ]]; then
            local cache_age=$(($(date +%s) - $(stat -f %m "$dir/CMakeCache.txt" 2>/dev/null || stat -c %Y "$dir/CMakeCache.txt" 2>/dev/null)))
            local days=$((cache_age / 86400))
            if [[ $days -gt 7 ]]; then
                write_status "CMake cache in '$dir' is $days days old" "Warning"
                WARNINGS+=("Old CMake cache in $dir (consider cleaning)")
                cache_issues=1
            elif [[ $VERBOSE -eq 1 ]]; then
                write_status "CMake cache in '$dir' is recent" "Success"
            fi
        fi
    done
    return $cache_issues
}

function test_agent_folder_structure() {
    write_status "Verifying agent folder structure..." "Step"
    
    local agent_files=(
        "src/app/editor/agent/agent_editor.h"
        "src/app/editor/agent/agent_editor.cc"
        "src/app/editor/agent/agent_chat.h"
        "src/app/editor/agent/agent_chat.cc"
        "src/app/editor/agent/agent_chat_history_codec.h"
        "src/app/editor/agent/agent_chat_history_codec.cc"
        "src/app/editor/agent/agent_collaboration_coordinator.h"
        "src/app/editor/agent/agent_collaboration_coordinator.cc"
        "src/app/editor/agent/network_collaboration_coordinator.h"
        "src/app/editor/agent/network_collaboration_coordinator.cc"
    )

    local old_system_files=(
        "src/app/gui/app/agent_chat_widget.h"
        "src/app/editor/agent/agent_collaboration_coordinator.h"
    )
    
    local all_present=1
    local has_old_structure=0
    
    for file in "${agent_files[@]}"; do
        if [[ ! -f "$file" ]]; then
            write_status "Agent file missing: $file" "Error"
            ISSUES_FOUND+=("Missing agent file: $file (may need to rebuild)")
            all_present=0
        elif [[ $VERBOSE -eq 1 ]]; then
            write_status "Agent file found: $file" "Success"
        fi
    done
    
    # Check for old structure (indicates stale cache)
    for file in "${old_system_files[@]}"; do
        if [[ -f "$file" ]]; then
            write_status "Old agent file still present: $file" "Warning"
            WARNINGS+=("Old agent structure detected (CMake cache needs cleaning)")
            has_old_structure=1
        fi
    done
    
    if [[ $all_present -eq 1 && $has_old_structure -eq 0 ]]; then
        write_status "Agent folder structure is correct" "Success"
        SUCCESS+=("Agent refactoring structure verified")
        return 0
    elif [[ $has_old_structure -eq 1 ]]; then
        write_status "Stale agent files detected - CMake cache should be cleaned" "Warning"
        return 1
    fi
    
    return $all_present
}

function clean_cmake_cache() {
    write_status "Cleaning CMake cache and build directories..." "Step"
    
    local build_dirs=(
        "build"
        "build-wasm"
        "build-test"
        "build-grpc-test"
        "build-rooms"
        "build-windows"
        "build_ai"
        "build_ai_claude"
        "build_agent"
        "build_ci"
        "build_fast"
        "build_test"
        "build-wasm-debug"
        "build_wasm_ai"
        "build_wasm"
    )
    local cleaned=0
    
    for dir in "${build_dirs[@]}"; do
        if [[ -d "$dir" ]]; then
            write_status "Removing $dir..." "Info"
            if rm -rf "$dir" 2>/dev/null; then
                cleaned=1
                write_status "  ✓ Removed $dir" "Success"
            else
                write_status "  ✗ Failed to remove $dir" "Error"
                WARNINGS+=("Could not fully clean $dir")
            fi
        fi
    done
    
    # Clean CMake generated files in root
    local cmake_files=("CMakeCache.txt" "cmake_install.cmake" "compile_commands.json")
    for file in "${cmake_files[@]}"; do
        if [[ -f "$file" ]]; then
            write_status "Removing root $file..." "Info"
            rm -f "$file" 2>/dev/null
        fi
    done
    
    if [[ $cleaned -eq 1 ]]; then
        write_status "CMake cache cleaned successfully" "Success"
        SUCCESS+=("Build directories cleaned - fresh build recommended")
    else
        write_status "No build directories found to clean" "Info"
    fi
}

function sync_git_submodules() {
    write_status "Syncing git submodules..." "Step"
    
    write_status "Running: git submodule sync --recursive" "Info"
    if git submodule sync --recursive &> /dev/null; then
        write_status "Running: git submodule update --init --recursive" "Info"
        if git submodule update --init --recursive; then
            write_status "Submodules synced successfully" "Success"
            return 0
        fi
    fi
    
    write_status "Failed to sync submodules" "Error"
    return 1
}

function test_dependency_compatibility() {
    write_status "Testing dependency configuration..." "Step"

    local deps_found=0
    local build_dirs=(
        "build"
        "build-wasm"
        "build-test"
        "build-grpc-test"
        "build_agent"
        "build_ci"
    )

    for dir in "${build_dirs[@]}"; do
        if [[ -d "$dir/_deps/nlohmann_json-src" ]] || [[ -d "$dir/_deps/httplib-src" ]]; then
            write_status "CPM dependencies present in $dir/_deps" "Success"
            SUCCESS+=("CPM dependencies available in $dir")
            deps_found=1
        fi
    done

    if [[ $deps_found -eq 0 ]]; then
        write_status "cpp-httplib and nlohmann/json are fetched by CPM during configure" "Info"
    fi
}

# ============================================================================
# Main Verification Process
# ============================================================================

echo ""
echo -e "${CYAN}╔════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║  YAZE Build Environment Verification for Unix/macOS/Linux     ║${NC}"
echo -e "${CYAN}╚════════════════════════════════════════════════════════════════╝${NC}"
echo ""

START_TIME=$(date +%s)

# Change to script directory
cd "$(dirname "$0")/.."

# Step 1: Check CMake
write_status "Checking CMake installation..." "Step"
if test_command cmake; then
    cmake_version=$(get_cmake_version)
    write_status "CMake found: version $cmake_version" "Success"
    
    # Parse version
    IFS='.' read -ra VERSION_PARTS <<< "$cmake_version"
    major=${VERSION_PARTS[0]}
    minor=${VERSION_PARTS[1]}
    
    if [[ $major -lt 3 ]] || [[ $major -eq 3 && $minor -lt 16 ]]; then
        write_status "CMake version too old (need 3.16+)" "Error"
        ISSUES_FOUND+=("CMake version $cmake_version is below minimum 3.16")
    fi
else
    write_status "CMake not found in PATH" "Error"
    ISSUES_FOUND+=("CMake not installed or not in PATH")
fi

# Step 2: Check Git
write_status "Checking Git installation..." "Step"
if test_command git; then
    git_version=$(git --version | grep -oE '[0-9]+\.[0-9]+\.[0-9]+')
    write_status "Git found: version $git_version" "Success"
else
    write_status "Git not found in PATH" "Error"
    ISSUES_FOUND+=("Git not installed or not in PATH")
fi

# Step 3: Check Build Tools
write_status "Checking build tools..." "Step"
if test_command make; then
    write_status "Make found" "Success"
elif test_command ninja; then
    write_status "Ninja found" "Success"
else
    write_status "No build tool found (make or ninja)" "Warning"
    WARNINGS+=("Neither make nor ninja found")
fi

# Step 4: Check Compiler
if test_command clang++; then
    clang_version=$(clang++ --version | head -n1)
    write_status "Clang++ found: $clang_version" "Success"
elif test_command g++; then
    gpp_version=$(g++ --version | head -n1)
    write_status "G++ found: $gpp_version" "Success"
else
    write_status "No C++ compiler found" "Error"
    ISSUES_FOUND+=("No C++ compiler (clang++ or g++) found")
fi

# Step 5: Check Git Submodules
write_status "Checking git submodules..." "Step"
if test_git_submodules; then
    write_status "All required submodules present" "Success"
else
    write_status "Some submodules are missing or empty" "Error"
    if [[ $FIX_ISSUES -eq 1 ]]; then
        sync_git_submodules
        test_git_submodules
    else
        write_status "Automatically syncing submodules..." "Info"
        if sync_git_submodules; then
            test_git_submodules
        else
            write_status "Run with --fix to try again, or manually: git submodule update --init --recursive" "Info"
        fi
    fi
fi

# Step 6: Check CMake Cache
write_status "Checking CMake cache..." "Step"
if test_cmake_cache; then
    write_status "CMake cache is up to date" "Success"
else
    if [[ $CLEAN_CACHE -eq 1 ]]; then
        clean_cmake_cache
    elif [[ $FIX_ISSUES -eq 1 ]]; then
        read -p "CMake cache is old. Clean it? (Y/n): " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]] || [[ -z $REPLY ]]; then
            clean_cmake_cache
        fi
    else
        write_status "Run with --clean to remove old cache files" "Info"
    fi
fi

# Step 7: Check vcpkg (Windows-specific but important)
write_status "Checking vcpkg availability..." "Step"
if [[ -d "vcpkg" ]]; then
    if [[ -f "vcpkg/vcpkg" ]] || [[ -f "vcpkg/vcpkg.exe" ]]; then
        write_status "vcpkg found and bootstrapped" "Success"
        SUCCESS+=("vcpkg available for dependency management")
        
        # Check vcpkg version if possible
        if [[ -x "vcpkg/vcpkg" ]]; then
            vcpkg_version=$(./vcpkg/vcpkg version 2>/dev/null | head -n1 || echo "unknown")
            write_status "vcpkg version: $vcpkg_version" "Info"
        fi
    else
        write_status "vcpkg directory exists but not bootstrapped" "Warning"
        WARNINGS+=("vcpkg not bootstrapped - run: cd vcpkg && ./bootstrap-vcpkg.sh")
    fi
else
    write_status "vcpkg not found (optional, required for Windows)" "Info"
    write_status "To install: git clone https://github.com/microsoft/vcpkg.git && vcpkg/bootstrap-vcpkg.sh" "Info"
fi

# Step 8: Check Dependencies
test_dependency_compatibility

# Step 9: Check Agent Folder Structure
if test_agent_folder_structure; then
    : # Structure is OK
else
    write_status "Agent folder structure has issues" "Warning"
    if [[ $CLEAN_CACHE -eq 1 ]] || [[ $FIX_ISSUES -eq 1 ]]; then
        write_status "Cleaning CMake cache due to structural changes..." "Info"
        clean_cmake_cache
        write_status "Please rebuild the project after cache cleaning" "Info"
    else
        write_status "Run with --clean to fix structural issues" "Info"
    fi
fi

# ============================================================================
# Summary Report
# ============================================================================

END_TIME=$(date +%s)
DURATION=$((END_TIME - START_TIME))

echo ""
echo -e "${CYAN}╔════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║                    Verification Summary                       ║${NC}"
echo -e "${CYAN}╚════════════════════════════════════════════════════════════════╝${NC}"
echo ""
echo "Duration: ${DURATION} seconds"
echo ""

if [[ ${#SUCCESS[@]} -gt 0 ]]; then
    echo -e "${GREEN}✓ Successes (${#SUCCESS[@]}):${NC}"
    for item in "${SUCCESS[@]}"; do
        echo -e "  ${GREEN}•${NC} $item"
    done
    echo ""
fi

if [[ ${#WARNINGS[@]} -gt 0 ]]; then
    echo -e "${YELLOW}⚠ Warnings (${#WARNINGS[@]}):${NC}"
    for item in "${WARNINGS[@]}"; do
        echo -e "  ${YELLOW}•${NC} $item"
    done
    echo ""
fi

if [[ ${#ISSUES_FOUND[@]} -gt 0 ]]; then
    echo -e "${RED}✗ Issues Found (${#ISSUES_FOUND[@]}):${NC}"
    for item in "${ISSUES_FOUND[@]}"; do
        echo -e "  ${RED}•${NC} $item"
    done
    echo ""
    
    echo -e "${YELLOW}Recommended Actions:${NC}"
    echo -e "  ${CYAN}1. Run: ./scripts/verify-build-environment.sh --fix${NC}"
    echo -e "  ${CYAN}2. Install missing dependencies${NC}"
    echo -e "  ${CYAN}3. Check build instructions: docs/02-build-instructions.md${NC}"
    echo ""
    
    exit 1
else
    echo -e "${GREEN}╔════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║          ✓ Build Environment Ready for Development!           ║${NC}"
    echo -e "${GREEN}╚════════════════════════════════════════════════════════════════╝${NC}"
    echo ""
    
    echo -e "${CYAN}Next Steps:${NC}"
    echo -e "  ${CYAN}Command Line Workflow:${NC}"
    echo -e "    ${NC}cmake -B build -DCMAKE_BUILD_TYPE=Debug${NC}"
    echo -e "    ${NC}cmake --build build --parallel \$(nproc)${NC}"
    echo ""
    
    exit 0
fi
