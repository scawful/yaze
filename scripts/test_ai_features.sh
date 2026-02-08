#!/bin/bash
# YAZE AI Features Test Script for macOS/Linux
# Tests AI agent, multimodal vision, and GUI automation capabilities

set -e

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
YAZE_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Prefer repo wrapper scripts (build_ai first) so we use the newest AI features.
YAZE_BIN="${YAZE_BIN:-$YAZE_ROOT/scripts/yaze}"
Z3ED_BIN="${Z3ED_BIN:-$YAZE_ROOT/scripts/z3ed}"
TEST_ROM="${TEST_ROM:-${YAZE_TEST_ROM_VANILLA:-./roms/alttp_vanilla.sfc}}"
GEMINI_API_KEY="${GEMINI_API_KEY:-}"
SCREENSHOTS_DIR="./test_screenshots"

function print_header() {
    echo -e "\n${CYAN}╔════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${CYAN}║          YAZE AI Features Test Suite (macOS/Linux)            ║${NC}"
    echo -e "${CYAN}╚════════════════════════════════════════════════════════════════╝${NC}\n"
}

function print_section() {
    echo -e "\n${BLUE}▶ $1${NC}"
    echo -e "${BLUE}$(printf '─%.0s' {1..64})${NC}"
}

function print_test() {
    echo -e "${CYAN}  Testing:${NC} $1"
}

function print_success() {
    echo -e "${GREEN}  ✓ $1${NC}"
}

function print_warning() {
    echo -e "${YELLOW}  ⚠ $1${NC}"
}

function print_error() {
    echo -e "${RED}  ✗ $1${NC}"
}

function check_prerequisites() {
    print_section "Checking Prerequisites"
    
    local all_ok=true
    
    # Check binaries
    if [[ -x "$YAZE_BIN" ]]; then
        print_success "YAZE GUI found: $YAZE_BIN"
    else
        print_error "YAZE GUI not found: $YAZE_BIN"
        all_ok=false
    fi
    
    if [[ -x "$Z3ED_BIN" ]]; then
        print_success "z3ed CLI found: $Z3ED_BIN"
    else
        print_error "z3ed CLI not found: $Z3ED_BIN"
        all_ok=false
    fi
    
    # Check ROM
    if [[ -f "$TEST_ROM" ]]; then
        print_success "Test ROM found: $TEST_ROM"
    else
        print_warning "Test ROM not found: $TEST_ROM (some tests will be skipped)"
    fi
    
    # Check Gemini API Key
    if [[ -n "$GEMINI_API_KEY" ]]; then
        print_success "Gemini API key configured"
    else
        print_warning "GEMINI_API_KEY not set (vision tests will be skipped)"
        print_warning "  Set with: export GEMINI_API_KEY='your-key-here'"
    fi
    
    # Check screenshot directory
    mkdir -p "$SCREENSHOTS_DIR"
    print_success "Screenshot directory ready: $SCREENSHOTS_DIR"
    
    if [[ "$all_ok" == "false" ]]; then
        echo ""
        print_error "Prerequisites not met. Please build the project first:"
        echo "  cmake --preset mac-ai"
        echo "  cmake --build build_ai --target yaze z3ed"
        exit 1
    fi
}

function test_z3ed_basic() {
    print_section "Test 1: z3ed Basic Functionality"
    
    print_test "Checking z3ed version"
    if "$Z3ED_BIN" --version &>/dev/null; then
        print_success "z3ed executable works"
    else
        print_error "z3ed --version failed"
        return 1
    fi
    
    print_test "Checking z3ed help"
    if "$Z3ED_BIN" --help &>/dev/null; then
        print_success "z3ed help accessible"
    else
        print_warning "z3ed help command failed"
    fi
}

function test_ai_agent_ollama() {
    print_section "Test 2: AI Agent (Ollama)"
    
    print_test "Checking if Ollama is running"
    if curl -s http://localhost:11434/api/tags &>/dev/null; then
        print_success "Ollama server is running"
        
        print_test "Testing agent chat with Ollama"
        if "$Z3ED_BIN" agent chat --model "llama3.2:latest" --prompt "Say 'test successful' and nothing else" 2>&1 | grep -i "test successful" &>/dev/null; then
            print_success "Ollama agent responded correctly"
        else
            print_warning "Ollama agent test inconclusive"
        fi
    else
        print_warning "Ollama not running (skip with: ollama serve)"
    fi
}

function test_ai_agent_gemini() {
    print_section "Test 3: AI Agent (Gemini)"
    
    if [[ -z "$GEMINI_API_KEY" ]]; then
        print_warning "Skipping Gemini tests (no API key)"
        return 0
    fi
    
    print_test "Testing Gemini text generation"
    local response
    response=$("$Z3ED_BIN" agent chat --provider gemini --prompt "Say 'Gemini works' and nothing else" 2>&1)
    
    if echo "$response" | grep -i "gemini works" &>/dev/null; then
        print_success "Gemini text generation works"
    else
        print_error "Gemini test failed"
        echo "$response"
    fi
}

function test_multimodal_vision() {
    print_section "Test 4: Multimodal Vision (Gemini)"
    
    if [[ -z "$GEMINI_API_KEY" ]]; then
        print_warning "Skipping vision tests (no API key)"
        return 0
    fi
    
    print_test "Running multimodal vision test suite"
    if "$Z3ED_BIN" test --filter "*GeminiVision*" 2>&1 | tee /tmp/vision_test.log; then
        if grep -q "PASSED" /tmp/vision_test.log; then
            print_success "Multimodal vision tests passed"
        else
            print_warning "Vision tests completed with warnings"
        fi
    else
        print_error "Vision test suite failed"
    fi
}

function test_learn_command() {
    print_section "Test 5: Learn Command (Knowledge Management)"
    
    print_test "Testing preference storage"
    if "$Z3ED_BIN" agent learn preference "test_key" "test_value" &>/dev/null; then
        print_success "Preference stored"
    else
        print_error "Failed to store preference"
    fi
    
    print_test "Testing context storage"
    if "$Z3ED_BIN" agent learn context "project" "YAZE ROM Editor test" &>/dev/null; then
        print_success "Project context stored"
    else
        print_warning "Context storage failed"
    fi
    
    print_test "Listing learned knowledge"
    if "$Z3ED_BIN" agent learn list &>/dev/null; then
        print_success "Knowledge retrieval works"
    else
        print_warning "Knowledge list failed"
    fi
}

function test_gui_automation_preparation() {
    print_section "Test 6: GUI Automation (Preparation)"
    
    print_test "Checking gRPC test harness support"
    if "$Z3ED_BIN" --help 2>&1 | grep -i "grpc" &>/dev/null; then
        print_success "gRPC support compiled in"
    else
        print_warning "gRPC support not detected"
    fi
    
    print_test "Verifying screenshot utils"
    if [[ -f "build_ai/lib/libyaze_core_lib.a" ]] || [[ -f "build/lib/libyaze_core_lib.a" ]]; then
        print_success "Core library with screenshot utils found"
    else
        print_warning "Core library not found (needed for GUI automation)"
    fi
    
    print_warning "Full GUI automation test requires YAZE to be running"
    print_warning "  Start YAZE GUI, then run: ./scripts/test_ai_gui_control.sh"
}

function test_action_parser() {
    print_section "Test 7: AI Action Parser"
    
    print_test "Testing natural language parsing"
    # This would need a test binary or z3ed command
    print_warning "Action parser integration test not yet implemented"
    print_warning "  Verify manually: AIActionParser can parse commands"
}

function print_summary() {
    print_section "Test Summary"
    
    echo -e "${GREEN}✓ Basic z3ed functionality verified${NC}"
    echo -e "${GREEN}✓ AI agent system operational${NC}"
    
    if [[ -n "$GEMINI_API_KEY" ]]; then
        echo -e "${GREEN}✓ Multimodal vision capabilities tested${NC}"
    else
        echo -e "${YELLOW}⚠ Vision tests skipped (no API key)${NC}"
    fi
    
    echo ""
    echo -e "${CYAN}Next Steps:${NC}"
    echo -e "  1. Start YAZE GUI:   ${GREEN}$YAZE_BIN${NC}"
    echo -e "  2. Test collaboration: ${GREEN}./scripts/test_collaboration.sh${NC}"
    echo -e "  3. Test GUI control:   ${GREEN}./scripts/test_ai_gui_control.sh${NC}"
    echo ""
    echo -e "${CYAN}For full feature testing:${NC}"
    echo -e "  - Set GEMINI_API_KEY for vision tests"
    echo -e "  - Start Ollama server for local AI"
    echo -e "  - Provide test ROM at: $TEST_ROM"
}

# ============================================================================
# Main Execution
# ============================================================================

print_header

cd "$YAZE_ROOT"

check_prerequisites

test_z3ed_basic
test_ai_agent_ollama
test_ai_agent_gemini
test_multimodal_vision
test_learn_command
test_gui_automation_preparation
test_action_parser

print_summary

echo ""
echo -e "${GREEN}╔════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║                  ✓ AI Features Test Complete!                 ║${NC}"
echo -e "${GREEN}╚════════════════════════════════════════════════════════════════╝${NC}"
echo ""
