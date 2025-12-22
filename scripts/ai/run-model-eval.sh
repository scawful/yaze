#!/bin/bash
# =============================================================================
# YAZE AI Model Evaluation Script
# 
# Runs AI model evaluations using the eval-runner.py engine.
#
# Usage:
#   ./run-model-eval.sh                          # Run with defaults
#   ./run-model-eval.sh --models llama3,qwen2.5  # Specific models
#   ./run-model-eval.sh --all                    # All available models
#   ./run-model-eval.sh --quick                  # Quick smoke test
#   ./run-model-eval.sh --compare                # Compare and report
#
# Prerequisites:
#   - Ollama running (ollama serve)
#   - Python 3.10+ with requests and pyyaml
#   - At least one model pulled (ollama pull llama3.2)
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
RESULTS_DIR="$SCRIPT_DIR/results"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Default settings
MODELS=""
TASKS="all"
TIMEOUT=120
DRY_RUN=false
COMPARE=false
QUICK_MODE=false
ALL_MODELS=false
DEFAULT_MODELS=false
VERBOSE=false

# =============================================================================
# Helper Functions
# =============================================================================

print_header() {
    echo -e "${CYAN}"
    echo "╔════════════════════════════════════════════════════════════════════╗"
    echo "║                    YAZE AI Model Evaluation                        ║"
    echo "╚════════════════════════════════════════════════════════════════════╝"
    echo -e "${NC}"
}

print_step() {
    echo -e "${BLUE}[*]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[✓]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[!]${NC} $1"
}

print_error() {
    echo -e "${RED}[✗]${NC} $1"
}

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --models, -m LIST    Comma-separated list of models to evaluate"
    echo "  --all                Evaluate all available models"
    echo "  --default            Evaluate default models from config"
    echo "  --tasks, -t LIST     Task categories (default: all)"
    echo "                       Options: rom_inspection, code_analysis, tool_calling, conversation"
    echo "  --timeout SEC        Timeout per task in seconds (default: 120)"
    echo "  --quick              Quick smoke test (fewer tasks)"
    echo "  --dry-run            Show what would run without executing"
    echo "  --compare            Generate comparison report after evaluation"
    echo "  --verbose, -v        Verbose output"
    echo "  --help, -h           Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 --models llama3.2,qwen2.5-coder --tasks tool_calling"
    echo "  $0 --all --compare"
    echo "  $0 --quick --default"
}

check_prerequisites() {
    print_step "Checking prerequisites..."
    
    local missing=false
    
    # Check Python
    if ! command -v python3 &> /dev/null; then
        print_error "Python 3 not found"
        missing=true
    else
        print_success "Python 3 found: $(python3 --version)"
    fi
    
    # Check Python packages
    if python3 -c "import requests" 2>/dev/null; then
        print_success "Python 'requests' package installed"
    else
        print_warning "Python 'requests' package missing - installing..."
        pip3 install requests --quiet || missing=true
    fi
    
    if python3 -c "import yaml" 2>/dev/null; then
        print_success "Python 'pyyaml' package installed"
    else
        print_warning "Python 'pyyaml' package missing - installing..."
        pip3 install pyyaml --quiet || missing=true
    fi
    
    # Check Ollama
    if ! command -v ollama &> /dev/null; then
        print_error "Ollama not found. Install from https://ollama.ai"
        missing=true
    else
        print_success "Ollama found: $(ollama --version 2>/dev/null || echo 'version unknown')"
    fi
    
    # Check if Ollama is running
    if curl -s http://localhost:11434/api/tags > /dev/null 2>&1; then
        print_success "Ollama server is running"
    else
        print_warning "Ollama server not running - attempting to start..."
        ollama serve &> /dev/null &
        sleep 3
        if curl -s http://localhost:11434/api/tags > /dev/null 2>&1; then
            print_success "Ollama server started"
        else
            print_error "Could not start Ollama server. Run 'ollama serve' manually."
            missing=true
        fi
    fi
    
    if $missing; then
        print_error "Prerequisites check failed"
        exit 1
    fi
    
    echo ""
}

list_available_models() {
    curl -s http://localhost:11434/api/tags | python3 -c "
import json, sys
data = json.load(sys.stdin)
for model in data.get('models', []):
    print(model['name'])
" 2>/dev/null || echo ""
}

ensure_model() {
    local model=$1
    local available=$(list_available_models)
    
    if echo "$available" | grep -q "^$model$"; then
        return 0
    else
        print_warning "Model '$model' not found, pulling..."
        ollama pull "$model"
        return $?
    fi
}

run_evaluation() {
    local args=()
    
    if [ -n "$MODELS" ]; then
        args+=(--models "$MODELS")
    elif $ALL_MODELS; then
        args+=(--all-models)
    elif $DEFAULT_MODELS; then
        args+=(--default-models)
    fi
    
    args+=(--tasks "$TASKS")
    args+=(--timeout "$TIMEOUT")
    args+=(--config "$SCRIPT_DIR/eval-tasks.yaml")
    
    if $DRY_RUN; then
        args+=(--dry-run)
    fi
    
    local output_file="$RESULTS_DIR/eval-$(date +%Y%m%d-%H%M%S).json"
    args+=(--output "$output_file")
    
    print_step "Running evaluation..."
    if $VERBOSE; then
        echo "  Command: python3 $SCRIPT_DIR/eval-runner.py ${args[*]}"
    fi
    echo ""
    
    python3 "$SCRIPT_DIR/eval-runner.py" "${args[@]}"
    local exit_code=$?
    
    if [ $exit_code -eq 0 ]; then
        print_success "Evaluation completed successfully"
    elif [ $exit_code -eq 1 ]; then
        print_warning "Evaluation completed with moderate scores"
    else
        print_error "Evaluation completed with poor scores"
    fi
    
    return 0
}

run_comparison() {
    print_step "Generating comparison report..."
    
    local result_files=$(ls -t "$RESULTS_DIR"/eval-*.json 2>/dev/null | head -5)
    
    if [ -z "$result_files" ]; then
        print_error "No result files found"
        return 1
    fi
    
    local report_file="$RESULTS_DIR/comparison-$(date +%Y%m%d-%H%M%S).md"
    
    python3 "$SCRIPT_DIR/compare-models.py" \
        --format markdown \
        --task-analysis \
        --output "$report_file" \
        $result_files
    
    print_success "Comparison report: $report_file"
    
    # Also print table to console
    echo ""
    python3 "$SCRIPT_DIR/compare-models.py" --format table $result_files
}

quick_test() {
    print_step "Running quick smoke test..."
    
    # Get first available model
    local available=$(list_available_models | head -1)
    
    if [ -z "$available" ]; then
        print_error "No models available. Pull a model with: ollama pull llama3.2"
        exit 1
    fi
    
    print_step "Using model: $available"
    
    # Run just one task category
    python3 "$SCRIPT_DIR/eval-runner.py" \
        --models "$available" \
        --tasks tool_calling \
        --timeout 60 \
        --config "$SCRIPT_DIR/eval-tasks.yaml"
}

# =============================================================================
# Main
# =============================================================================

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --models|-m)
            MODELS="$2"
            shift 2
            ;;
        --all)
            ALL_MODELS=true
            shift
            ;;
        --default)
            DEFAULT_MODELS=true
            shift
            ;;
        --tasks|-t)
            TASKS="$2"
            shift 2
            ;;
        --timeout)
            TIMEOUT="$2"
            shift 2
            ;;
        --quick)
            QUICK_MODE=true
            shift
            ;;
        --dry-run)
            DRY_RUN=true
            shift
            ;;
        --compare)
            COMPARE=true
            shift
            ;;
        --verbose|-v)
            VERBOSE=true
            shift
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

# Ensure results directory exists
mkdir -p "$RESULTS_DIR"

print_header
check_prerequisites

if $QUICK_MODE; then
    quick_test
elif $DRY_RUN; then
    run_evaluation
else
    run_evaluation
    
    if $COMPARE; then
        echo ""
        run_comparison
    fi
fi

echo ""
print_success "Done!"

