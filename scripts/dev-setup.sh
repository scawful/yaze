#!/bin/bash
# YAZE Developer Setup Script
# One-command setup for new developers

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Print functions
print_header() {
    echo -e "${BLUE}================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}================================${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

# Detect OS
detect_os() {
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        OS="linux"
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        OS="macos"
    elif [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "cygwin" ]]; then
        OS="windows"
    else
        OS="unknown"
    fi
    echo "Detected OS: $OS"
}

# Check prerequisites
check_prerequisites() {
    print_header "Checking Prerequisites"
    
    # Check Git
    if command -v git &> /dev/null; then
        print_success "Git found: $(git --version)"
    else
        print_error "Git not found. Please install Git first."
        exit 1
    fi
    
    # Check CMake
    if command -v cmake &> /dev/null; then
        CMAKE_VERSION=$(cmake --version | head -n1 | cut -d' ' -f3)
        print_success "CMake found: $CMAKE_VERSION"
        
        # Check version
        CMAKE_MAJOR=$(echo $CMAKE_VERSION | cut -d'.' -f1)
        CMAKE_MINOR=$(echo $CMAKE_VERSION | cut -d'.' -f2)
        if [ "$CMAKE_MAJOR" -lt 3 ] || ([ "$CMAKE_MAJOR" -eq 3 ] && [ "$CMAKE_MINOR" -lt 16 ]); then
            print_error "CMake 3.16+ required. Found: $CMAKE_VERSION"
            exit 1
        fi
    else
        print_error "CMake not found. Please install CMake 3.16+ first."
        exit 1
    fi
    
    # Check compiler
    if command -v gcc &> /dev/null; then
        GCC_VERSION=$(gcc --version | head -n1 | cut -d' ' -f4)
        print_success "GCC found: $GCC_VERSION"
    elif command -v clang &> /dev/null; then
        CLANG_VERSION=$(clang --version | head -n1 | cut -d' ' -f4)
        print_success "Clang found: $CLANG_VERSION"
    else
        print_error "No C++ compiler found. Please install GCC 12+ or Clang 14+."
        exit 1
    fi
    
    # Check Ninja
    if command -v ninja &> /dev/null; then
        print_success "Ninja found: $(ninja --version)"
    else
        print_warning "Ninja not found. Will use Make instead."
    fi
}

# Install dependencies
install_dependencies() {
    print_header "Installing Dependencies"
    
    case $OS in
        "linux")
            if command -v apt-get &> /dev/null; then
                print_success "Installing dependencies via apt..."
                sudo apt-get update
                sudo apt-get install -y build-essential ninja-build pkg-config ccache \
                    libsdl2-dev libyaml-cpp-dev libgtk-3-dev libglew-dev
            elif command -v dnf &> /dev/null; then
                print_success "Installing dependencies via dnf..."
                sudo dnf install -y gcc-c++ ninja-build pkgconfig SDL2-devel yaml-cpp-devel
            elif command -v pacman &> /dev/null; then
                print_success "Installing dependencies via pacman..."
                sudo pacman -S --needed base-devel ninja pkgconfig sdl2 yaml-cpp
            else
                print_warning "Unknown Linux distribution. Please install dependencies manually."
            fi
            ;;
        "macos")
            if command -v brew &> /dev/null; then
                print_success "Installing dependencies via Homebrew..."
                brew install cmake ninja pkg-config ccache sdl2 yaml-cpp
            else
                print_warning "Homebrew not found. Please install dependencies manually."
            fi
            ;;
        "windows")
            print_warning "Windows detected. Please install dependencies manually:"
            echo "1. Install Visual Studio Build Tools"
            echo "2. Install vcpkg and packages: sdl2, yaml-cpp"
            echo "3. Install Ninja from https://ninja-build.org/"
            ;;
        *)
            print_warning "Unknown OS. Please install dependencies manually."
            ;;
    esac
}

# Setup repository
setup_repository() {
    print_header "Setting up Repository"
    
    # Check if we're in a git repository
    if [ ! -d ".git" ]; then
        print_error "Not in a git repository. Please run this script from the YAZE root directory."
        exit 1
    fi
    
    # Update submodules
    print_success "Updating submodules..."
    git submodule update --init --recursive
    
    # Check for uncommitted changes
    if ! git diff-index --quiet HEAD --; then
        print_warning "You have uncommitted changes. Consider committing or stashing them."
    fi
}

# Configure IDE
configure_ide() {
    print_header "Configuring IDE"
    
    # Generate compile_commands.json
    print_success "Generating compile_commands.json..."
    cmake -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    
    # Create VS Code settings
    if [ ! -d ".vscode" ]; then
        mkdir -p .vscode
        print_success "Creating VS Code configuration..."
        
        cat > .vscode/settings.json << EOF
{
    "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools",
    "C_Cpp.default.compileCommands": "build/compile_commands.json",
    "C_Cpp.default.intelliSenseMode": "macos-clang-x64",
    "files.associations": {
        "*.h": "c",
        "*.cc": "cpp"
    },
    "cmake.buildDirectory": "build",
    "cmake.configureOnOpen": true,
    "cmake.generator": "Ninja"
}
EOF
        
        cat > .vscode/tasks.json << EOF
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "build",
            "type": "shell",
            "command": "cmake",
            "args": ["--build", "build"],
            "group": "build",
            "presentation": {
                "echo": true,
                "reveal": "always",
                "focus": false,
                "panel": "shared"
            }
        },
        {
            "label": "configure",
            "type": "shell",
            "command": "cmake",
            "args": ["--preset", "dev"],
            "group": "build"
        }
    ]
}
EOF
    fi
}

# Run first build
run_first_build() {
    print_header "Running First Build"
    
    # Configure project
    print_success "Configuring project..."
    cmake --preset dev
    
    # Build project
    print_success "Building project..."
    cmake --build build
    
    # Check if build succeeded
    if [ -f "build/bin/yaze" ] || [ -f "build/bin/yaze.exe" ]; then
        print_success "Build successful! YAZE executable created."
    else
        print_error "Build failed. Check the output above for errors."
        exit 1
    fi
}

# Run tests
run_tests() {
    print_header "Running Tests"
    
    print_success "Running test suite..."
    cd build
    ctest --output-on-failure -L stable || print_warning "Some tests failed (this is normal for first setup)"
    cd ..
}

# Print next steps
print_next_steps() {
    print_header "Setup Complete!"
    
    echo -e "${GREEN}✓ YAZE development environment is ready!${NC}"
    echo ""
    echo "Next steps:"
    echo "1. Run the application:"
    echo "   ./build/bin/yaze"
    echo ""
    echo "2. Run tests:"
    echo "   cd build && ctest"
    echo ""
    echo "3. Format code:"
    echo "   cmake --build build --target yaze-format"
    echo ""
    echo "4. Check formatting:"
    echo "   cmake --build build --target yaze-format-check"
    echo ""
    echo "5. Read the documentation:"
    echo "   docs/BUILD.md"
    echo ""
    echo "Happy coding! 🎮"
}

# Main function
main() {
    print_header "YAZE Developer Setup"
    echo "This script will set up your YAZE development environment."
    echo ""
    
    detect_os
    check_prerequisites
    install_dependencies
    setup_repository
    configure_ide
    run_first_build
    run_tests
    print_next_steps
}

# Run main function
main "$@"
