#!/bin/bash

# Build script for z3ed WASM terminal mode
# This script builds z3ed CLI for web browsers without TUI dependencies

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}Building z3ed for WASM Terminal Mode${NC}"
echo "======================================="

# Check if emscripten is available
if ! command -v emcc &> /dev/null; then
    echo -e "${RED}Error: Emscripten (emcc) not found!${NC}"
    echo "Please install and activate Emscripten SDK:"
    echo "  git clone https://github.com/emscripten-core/emsdk.git"
    echo "  cd emsdk"
    echo "  ./emsdk install latest"
    echo "  ./emsdk activate latest"
    echo "  source ./emsdk_env.sh"
    exit 1
fi

# Get the script directory and project root
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$( cd "$SCRIPT_DIR/.." && pwd )"

# Build directory
BUILD_DIR="${PROJECT_ROOT}/build-wasm"

# Parse command line arguments
CLEAN_BUILD=false
BUILD_TYPE="Release"
VERBOSE=""

while [[ $# -gt 0 ]]; do
    case $1 in
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --verbose|-v)
            VERBOSE="-v"
            shift
            ;;
        --help|-h)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  --clean    Clean build directory before building"
            echo "  --debug    Build in debug mode (default: release)"
            echo "  --verbose  Enable verbose build output"
            echo "  --help     Show this help message"
            exit 0
            ;;
        *)
            echo -e "${YELLOW}Unknown option: $1${NC}"
            shift
            ;;
    esac
done

# Clean build directory if requested
if [ "$CLEAN_BUILD" = true ]; then
    echo -e "${YELLOW}Cleaning build directory...${NC}"
    rm -rf "${BUILD_DIR}"
fi

# Create build directory
mkdir -p "${BUILD_DIR}"

# Configure with CMake
echo -e "${GREEN}Configuring CMake...${NC}"
cd "${PROJECT_ROOT}"

if [ "$BUILD_TYPE" = "Debug" ]; then
    # For debug builds, we could create a wasm-debug preset or modify flags
    cmake --preset wasm-release \
        -DCMAKE_BUILD_TYPE=Debug \
        -DYAZE_BUILD_CLI=ON \
        -DYAZE_BUILD_Z3ED=ON \
        -DYAZE_WASM_TERMINAL=ON
else
    cmake --preset wasm-release
fi

# Build z3ed
echo -e "${GREEN}Building z3ed...${NC}"
cmake --build "${BUILD_DIR}" --target z3ed $VERBOSE

# Check if build succeeded
if [ -f "${BUILD_DIR}/bin/z3ed.js" ]; then
    echo -e "${GREEN}✓ Build successful!${NC}"
    echo ""
    echo "Output files:"
    echo "  - ${BUILD_DIR}/bin/z3ed.js"
    echo "  - ${BUILD_DIR}/bin/z3ed.wasm"
    echo ""
    echo "To use z3ed in a web page:"
    echo "1. Include z3ed.js in your HTML"
    echo "2. Initialize the module:"
    echo "   const Z3edTerminal = await Z3edTerminal();"
    echo "3. Call exported functions:"
    echo "   Z3edTerminal.ccall('z3ed_init', 'number', [], []);"
    echo "   const result = Z3edTerminal.ccall('z3ed_execute_command', 'string', ['string'], ['help']);"
else
    echo -e "${RED}✗ Build failed!${NC}"
    echo "Check the build output above for errors."
    exit 1
fi

# Optional: Generate HTML test page
if [ ! -f "${BUILD_DIR}/z3ed_test.html" ]; then
    echo -e "${YELLOW}Generating test HTML page...${NC}"
    cat > "${BUILD_DIR}/z3ed_test.html" << 'EOF'
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>z3ed WASM Terminal Test</title>
    <style>
        body {
            font-family: 'Courier New', monospace;
            background-color: #1e1e1e;
            color: #d4d4d4;
            margin: 20px;
        }
        #terminal {
            background-color: #000;
            border: 1px solid #444;
            padding: 10px;
            height: 400px;
            overflow-y: auto;
            white-space: pre-wrap;
        }
        #input-line {
            display: flex;
            margin-top: 10px;
        }
        #prompt {
            color: #4ec9b0;
            margin-right: 5px;
        }
        #command-input {
            flex-grow: 1;
            background-color: #1e1e1e;
            color: #d4d4d4;
            border: 1px solid #444;
            padding: 5px;
            font-family: inherit;
        }
        .output {
            color: #d4d4d4;
        }
        .error {
            color: #f44747;
        }
        .info {
            color: #4ec9b0;
        }
    </style>
</head>
<body>
    <h1>z3ed WASM Terminal Test</h1>
    <div id="terminal"></div>
    <div id="input-line">
        <span id="prompt">z3ed&gt;</span>
        <input type="text" id="command-input" placeholder="Type 'help' for commands...">
    </div>

    <script src="bin/z3ed.js"></script>
    <script>
        let z3edModule;
        const terminal = document.getElementById('terminal');
        const commandInput = document.getElementById('command-input');

        function addToTerminal(text, className = 'output') {
            const line = document.createElement('div');
            line.className = className;
            line.textContent = text;
            terminal.appendChild(line);
            terminal.scrollTop = terminal.scrollHeight;
        }

        window.z3edTerminal = {
            print: (text) => addToTerminal(text, 'output'),
            printError: (text) => addToTerminal(text, 'error')
        };

        async function initZ3ed() {
            try {
                addToTerminal('Initializing z3ed WASM terminal...', 'info');
                z3edModule = await Z3edTerminal();

                // Initialize the terminal
                const ready = z3edModule.ccall('Z3edIsReady', 'number', [], []);
                if (ready) {
                    addToTerminal('z3ed terminal ready!', 'info');
                    addToTerminal('Type "help" for available commands', 'info');
                } else {
                    addToTerminal('Failed to initialize z3ed', 'error');
                }
            } catch (error) {
                addToTerminal('Error loading z3ed: ' + error.message, 'error');
            }
        }

        function executeCommand(command) {
            if (!z3edModule) {
                addToTerminal('z3ed not initialized', 'error');
                return;
            }

            // Show the command in terminal
            addToTerminal('z3ed> ' + command, 'output');

            try {
                // Execute the command
                const result = z3edModule.ccall(
                    'Z3edProcessCommand',
                    'string',
                    ['string'],
                    [command]
                );

                if (result) {
                    addToTerminal(result, 'output');
                }
            } catch (error) {
                addToTerminal('Error: ' + error.message, 'error');
            }
        }

        commandInput.addEventListener('keydown', (e) => {
            if (e.key === 'Enter') {
                const command = commandInput.value.trim();
                if (command) {
                    executeCommand(command);
                    commandInput.value = '';
                }
            }
        });

        // Initialize on load
        initZ3ed();
    </script>
</body>
</html>
EOF
    echo -e "${GREEN}Test page created: ${BUILD_DIR}/z3ed_test.html${NC}"
fi

echo -e "${GREEN}Build complete!${NC}"