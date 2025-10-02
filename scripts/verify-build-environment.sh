#!/bin/bash
# YAZE Build Environment Verification Script for macOS/Linux
# This script verifies the build environment is properly configured and ready to build

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

VERBOSE=0
FIX_ISSUES=0
CLEAN_CACHE=0

issues_found=()
warnings=()
successes=()

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --verbose|-v)
            VERBOSE=1
            shift
            ;;
        --fix|-f)
            FIX_ISSUES=1
            shift
            ;;
        --clean|-c)
            CLEAN_CACHE=1
            shift
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --verbose, -v    Show detailed output"
            echo "  --fix, -f        Automatically fix issues"
            echo "  --clean, -c      Clean CMake cache"
            echo "  --help, -h       Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

print_status() {
    local type=$1
    local message=$2
    local timestamp=$(date +"%H:%M:%S")
    
    case $type in
        success)
            echo -e "[$timestamp] ${GREEN}✓${NC} $message"
            ;;
        error)
            echo -e "[$timestamp] ${RED}✗${NC} $message"
            ;;
        warning)
            echo -e "[$timestamp] ${YELLOW}⚠${NC} $message"
            ;;
        info)
            echo -e "[$timestamp] ${CYAN}ℹ${NC} $message"
            ;;
        step)
            echo ""
            echo -e "[$timestamp] ${BLUE}▶${NC} ${BLUE}$message${NC}"
            ;;
    esac
}

check_command() {
    if command -v "$1" &> /dev/null; then
        return 0
    else
        return 1
    fi
}

get_cmake_version() {
    local version=$(cmake --version 2>/dev/null | head -n1 | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -n1)
    if [ -n "$version" ]; then
        echo "$version"
    else
        echo "unknown"
    fi
}

check_git_submodules() {
    local submodules=(
        "src/lib/SDL"
        "src/lib/abseil-cpp"
        "src/lib/asar"
        "src/lib/imgui"
        "third_party/json"
        "third_party/httplib"
    )
    
    local all_present=0
    local script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    local root_dir="$(cd "$script_dir/.." && pwd)"
    
    for submodule in "${submodules[@]}"; do
        if [ ! -d "$root_dir/$submodule" ] || [ -z "$(ls -A "$root_dir/$submodule")" ]; then
            print_status error "Submodule missing or empty: $submodule"
            issues_found+=("Missing submodule: $submodule")
            all_present=1
        elif [ $VERBOSE -eq 1 ]; then
            print_status success "Submodule found: $submodule"
        fi
    done
    
    return $all_present
}

check_cmake_cache() {
    local build_dirs=("build" "build_test" "build-grpc-test")
    local cache_issues=0
    local script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    local root_dir="$(cd "$script_dir/.." && pwd)"
    
    for dir in "${build_dirs[@]}"; do
        local cache_path="$root_dir/$dir/CMakeCache.txt"
        if [ -f "$cache_path" ]; then
            local cache_age=$(($(date +%s) - $(stat -f %m "$cache_path" 2>/dev/null || stat -c %Y "$cache_path")))
            local cache_days=$((cache_age / 86400))
            
            if [ $cache_days -gt 7 ]; then
                print_status warning "CMake cache in '$dir' is $cache_days days old"
                warnings+=("Old CMake cache in $dir (consider cleaning)")
                cache_issues=1
            elif [ $VERBOSE -eq 1 ]; then
                print_status success "CMake cache in '$dir' is recent"
            fi
        fi
    done
    
    return $cache_issues
}

check_dependency_compatibility() {
    print_status step "Testing dependency configuration..."
    
    local script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    local root_dir="$(cd "$script_dir/.." && pwd)"
    
    # Check gRPC configuration
    local grpc_path="$root_dir/cmake/grpc.cmake"
    if [ -f "$grpc_path" ]; then
        if grep -q "CMAKE_DISABLE_FIND_PACKAGE_Protobuf TRUE" "$grpc_path"; then
            print_status success "gRPC isolation configured correctly"
        else
            print_status warning "gRPC may conflict with system protobuf"
            warnings+=("gRPC not properly isolated from system packages")
        fi
    fi
    
    # Check httplib
    if [ -d "$root_dir/third_party/httplib" ]; then
        print_status success "httplib found in third_party"
        successes+=("httplib header-only library available")
    fi
    
    # Check json library
    if [ -d "$root_dir/third_party/json/include" ]; then
        print_status success "nlohmann/json found in third_party"
        successes+=("nlohmann/json header-only library available")
    fi
}

clean_cmake_cache() {
    print_status step "Cleaning CMake cache..."
    
    local script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    local root_dir="$(cd "$script_dir/.." && pwd)"
    local build_dirs=("build" "build_test" "build-grpc-test")
    
    for dir in "${build_dirs[@]}"; do
        if [ -d "$root_dir/$dir" ]; then
            print_status info "Removing $dir..."
            rm -rf "$root_dir/$dir"
        fi
    done
    
    print_status success "CMake cache cleaned"
}

sync_git_submodules() {
    print_status step "Syncing git submodules..."
    
    local script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    local root_dir="$(cd "$script_dir/.." && pwd)"
    
    cd "$root_dir"
    
    print_status info "Running: git submodule sync --recursive"
    git submodule sync --recursive
    
    print_status info "Running: git submodule update --init --recursive"
    git submodule update --init --recursive
    
    print_status success "Submodules synced successfully"
}

test_cmake_configuration() {
    print_status step "Testing CMake configuration..."
    
    local script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    local root_dir="$(cd "$script_dir/.." && pwd)"
    local test_build_dir="$root_dir/build_test_config"
    
    print_status info "Configuring CMake (this may take a moment)..."
    
    if cmake -B "$test_build_dir" -S "$root_dir" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DYAZE_MINIMAL_BUILD=ON \
        -DYAZE_BUILD_TESTS=OFF &>/dev/null; then
        
        print_status success "CMake configuration successful"
        successes+=("CMake configuration test passed")
        
        # Cleanup test build
        rm -rf "$test_build_dir"
        return 0
    else
        print_status error "CMake configuration failed"
        issues_found+=("CMake configuration test failed")
        return 1
    fi
}

# ============================================================================
# Main Verification Process
# ============================================================================

echo ""
echo -e "${CYAN}╔════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║       YAZE Build Environment Verification (macOS/Linux)       ║${NC}"
echo -e "${CYAN}╚════════════════════════════════════════════════════════════════╝${NC}"
echo ""

start_time=$(date +%s)

# Step 1: Check CMake
print_status step "Checking CMake installation..."
if check_command cmake; then
    cmake_version=$(get_cmake_version)
    
    if [ "$cmake_version" != "unknown" ]; then
        print_status success "CMake found: version $cmake_version"
        
        # Check version
        major=$(echo "$cmake_version" | cut -d. -f1)
        minor=$(echo "$cmake_version" | cut -d. -f2)
        
        # Validate that we got numeric values
        if [[ "$major" =~ ^[0-9]+$ ]] && [[ "$minor" =~ ^[0-9]+$ ]]; then
            if [ "$major" -lt 3 ] || ([ "$major" -eq 3 ] && [ "$minor" -lt 16 ]); then
                print_status error "CMake version too old (need 3.16+)"
                issues_found+=("CMake version $cmake_version is below minimum 3.16")
            fi
        else
            print_status warning "Could not parse CMake version: $cmake_version"
            warnings+=("Unable to verify CMake version requirement (need 3.16+)")
        fi
    else
        print_status warning "CMake found but version could not be determined"
        warnings+=("CMake version could not be parsed - ensure version 3.16+ is installed")
    fi
else
    print_status error "CMake not found in PATH"
    issues_found+=("CMake not installed or not in PATH")
fi

# Step 2: Check Git
print_status step "Checking Git installation..."
if check_command git; then
    git_version=$(git --version | grep -oE '[0-9]+\.[0-9]+\.[0-9]+')
    print_status success "Git found: version $git_version"
else
    print_status error "Git not found in PATH"
    issues_found+=("Git not installed or not in PATH")
fi

# Step 3: Check Platform-Specific Tools
print_status step "Checking platform-specific tools..."
if [[ "$OSTYPE" == "darwin"* ]]; then
    if check_command xcodebuild; then
        xcode_version=$(xcodebuild -version | head -n1)
        print_status success "Xcode found: $xcode_version"
    else
        print_status warning "Xcode not found (optional but recommended)"
        warnings+=("Xcode Command Line Tools not installed")
    fi
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    if check_command gcc || check_command clang; then
        print_status success "C++ compiler found"
    else
        print_status error "No C++ compiler found"
        issues_found+=("Install build-essential or clang")
    fi
    
    # Check for GTK (needed for NFD on Linux)
    if pkg-config --exists gtk+-3.0; then
        print_status success "GTK+3 found"
    else
        print_status warning "GTK+3 not found (needed for file dialogs)"
        warnings+=("Install libgtk-3-dev for native file dialogs")
    fi
fi

# Step 4: Check Git Submodules
print_status step "Checking git submodules..."
if check_git_submodules; then
    print_status success "All required submodules present"
else
    print_status error "Some submodules are missing or empty"
    if [ $FIX_ISSUES -eq 1 ]; then
        sync_git_submodules
        # Re-check after sync
        if ! check_git_submodules; then
            print_status warning "Submodule sync completed but some issues remain"
        fi
    else
        # Auto-fix without confirmation
        print_status info "Automatically syncing submodules..."
        if sync_git_submodules; then
            print_status success "Submodules synced successfully"
            # Re-check after sync
            check_git_submodules
        else
            print_status error "Failed to sync submodules automatically"
            print_status info "Run with --fix to try again, or manually run: git submodule update --init --recursive"
        fi
    fi
fi

# Step 5: Check CMake Cache
print_status step "Checking CMake cache..."
if check_cmake_cache; then
    print_status success "CMake cache is up to date"
else
    if [ $CLEAN_CACHE -eq 1 ]; then
        clean_cmake_cache
    elif [ $FIX_ISSUES -eq 1 ]; then
        # Ask for confirmation before cleaning cache
        echo ""
        echo -e "${YELLOW}CMake cache is older than 7 days. Clean it?${NC}"
        echo -e "${CYAN}This will remove build/, build_test/, and build-grpc-test/ directories.${NC}"
        read -p "Continue? (Y/n) " -n 1 -r
        echo ""
        if [[ $REPLY =~ ^[Yy]$ ]] || [[ -z $REPLY ]]; then
            clean_cmake_cache
        else
            print_status info "Skipping cache clean"
        fi
    else
        print_status warning "CMake cache is older than 7 days (consider cleaning)"
        print_status info "Run with --clean to remove old cache files"
    fi
fi

# Step 6: Check Dependencies
check_dependency_compatibility

# Step 7: Test CMake Configuration (if requested)
if [ $VERBOSE -eq 1 ] || [ $FIX_ISSUES -eq 1 ]; then
    test_cmake_configuration
fi

# ============================================================================
# Summary Report
# ============================================================================

end_time=$(date +%s)
duration=$((end_time - start_time))

echo ""
echo -e "${CYAN}╔════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║                    Verification Summary                       ║${NC}"
echo -e "${CYAN}╚════════════════════════════════════════════════════════════════╝${NC}"
echo ""

echo "Duration: $duration seconds"
echo ""

if [ ${#successes[@]} -gt 0 ]; then
    echo -e "${GREEN}✓ Successes (${#successes[@]}):${NC}"
    for item in "${successes[@]}"; do
        echo -e "  ${GREEN}•${NC} $item"
    done
    echo ""
fi

if [ ${#warnings[@]} -gt 0 ]; then
    echo -e "${YELLOW}⚠ Warnings (${#warnings[@]}):${NC}"
    for item in "${warnings[@]}"; do
        echo -e "  ${YELLOW}•${NC} $item"
    done
    echo ""
fi

if [ ${#issues_found[@]} -gt 0 ]; then
    echo -e "${RED}✗ Issues Found (${#issues_found[@]}):${NC}"
    for item in "${issues_found[@]}"; do
        echo -e "  ${RED}•${NC} $item"
    done
    echo ""
    
    echo -e "${YELLOW}Recommended Actions:${NC}"
    echo -e "  1. Run: ${CYAN}./scripts/verify-build-environment.sh --fix${NC}"
    echo -e "  2. Install missing dependencies"
    echo -e "  3. Check build instructions: ${CYAN}docs/02-build-instructions.md${NC}"
    echo ""
    
    exit 1
else
    echo -e "${GREEN}╔════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║          ✓ Build Environment Ready for Development!           ║${NC}"
    echo -e "${GREEN}╚════════════════════════════════════════════════════════════════╝${NC}"
    echo ""
    
    echo -e "${CYAN}Next Steps:${NC}"
    if [[ "$OSTYPE" == "darwin"* ]]; then
        echo -e "  ${BLUE}macOS:${NC}"
        echo "    cmake --preset debug"
        echo "    cmake --build build"
    else
        echo -e "  ${BLUE}Linux:${NC}"
        echo "    cmake -B build -DCMAKE_BUILD_TYPE=Debug"
        echo "    cmake --build build"
    fi
    echo ""
    
    exit 0
fi
