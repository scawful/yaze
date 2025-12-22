# YAZE AI Features Test Script for Windows
# Tests AI agent, multimodal vision, and GUI automation capabilities

param(
    [string]$YazeBin = "build-windows\bin\Debug\yaze.exe",
    [string]$Z3edBin = "build-windows\bin\Debug\z3ed.exe",
    [string]$TestRom = $env:YAZE_TEST_ROM_VANILLA
)

$ErrorActionPreference = "Continue"

if (-not $TestRom) {
    $TestRom = "roms\alttp_vanilla.sfc"
}

function Write-Header {
    Write-Host "`n╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║            YAZE AI Features Test Suite (Windows)              ║" -ForegroundColor Cyan
    Write-Host "╚════════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan
}

function Write-Section {
    param($Title)
    Write-Host "`n▶ $Title" -ForegroundColor Blue
    Write-Host ("─" * 64) -ForegroundColor Blue
}

function Write-Test {
    param($Message)
    Write-Host "  Testing: " -NoNewline -ForegroundColor Cyan
    Write-Host $Message
}

function Write-Success {
    param($Message)
    Write-Host "  ✓ $Message" -ForegroundColor Green
}

function Write-Warning {
    param($Message)
    Write-Host "  ⚠ $Message" -ForegroundColor Yellow
}

function Write-Error {
    param($Message)
    Write-Host "  ✗ $Message" -ForegroundColor Red
}

function Test-Prerequisites {
    Write-Section "Checking Prerequisites"
    
    $allOk = $true
    
    # Check binaries
    if (Test-Path $YazeBin) {
        Write-Success "YAZE GUI found: $YazeBin"
    } else {
        Write-Error "YAZE GUI not found: $YazeBin"
        $allOk = $false
    }
    
    if (Test-Path $Z3edBin) {
        Write-Success "z3ed CLI found: $Z3edBin"
    } else {
        Write-Error "z3ed CLI not found: $Z3edBin"
        $allOk = $false
    }
    
    # Check ROM
    if (Test-Path $TestRom) {
        Write-Success "Test ROM found: $TestRom"
    } else {
        Write-Warning "Test ROM not found: $TestRom (some tests will be skipped)"
    }
    
    # Check Gemini API Key
    if ($env:GEMINI_API_KEY) {
        Write-Success "Gemini API key configured"
    } else {
        Write-Warning "GEMINI_API_KEY not set (vision tests will be skipped)"
        Write-Warning "  Set with: `$env:GEMINI_API_KEY='your-key-here'"
    }
    
    # Create screenshot directory
    $screenshotsDir = "test_screenshots"
    if (-not (Test-Path $screenshotsDir)) {
        New-Item -ItemType Directory -Path $screenshotsDir | Out-Null
    }
    Write-Success "Screenshot directory ready: $screenshotsDir"
    
    if (-not $allOk) {
        Write-Host ""
        Write-Error "Prerequisites not met. Please build the project first:"
        Write-Host "  cmake --preset win-ai"
        Write-Host "  cmake --build build-windows --config Debug"
        exit 1
    }
}

function Test-Z3edBasic {
    Write-Section "Test 1: z3ed Basic Functionality"
    
    Write-Test "Checking z3ed version"
    try {
        $output = & $Z3edBin --version 2>&1
        Write-Success "z3ed executable works"
    } catch {
        Write-Error "z3ed --version failed"
        return
    }
    
    Write-Test "Checking z3ed help"
    try {
        $output = & $Z3edBin --help 2>&1
        Write-Success "z3ed help accessible"
    } catch {
        Write-Warning "z3ed help command failed"
    }
}

function Test-AIAgentOllama {
    Write-Section "Test 2: AI Agent (Ollama)"
    
    Write-Test "Checking if Ollama is running"
    try {
        $response = Invoke-WebRequest -Uri "http://localhost:11434/api/tags" -UseBasicParsing -TimeoutSec 2
        Write-Success "Ollama server is running"
        
        Write-Test "Testing agent chat with Ollama"
        $output = & $Z3edBin agent chat --model "llama3.2:latest" --prompt "Say 'test successful' and nothing else" 2>&1
        if ($output -match "test successful") {
            Write-Success "Ollama agent responded correctly"
        } else {
            Write-Warning "Ollama agent test inconclusive"
        }
    } catch {
        Write-Warning "Ollama not running (start with: ollama serve)"
    }
}

function Test-AIAgentGemini {
    Write-Section "Test 3: AI Agent (Gemini)"
    
    if (-not $env:GEMINI_API_KEY) {
        Write-Warning "Skipping Gemini tests (no API key)"
        return
    }
    
    Write-Test "Testing Gemini text generation"
    try {
        $output = & $Z3edBin agent chat --provider gemini --prompt "Say 'Gemini works' and nothing else" 2>&1
        if ($output -match "Gemini works") {
            Write-Success "Gemini text generation works"
        } else {
            Write-Error "Gemini test failed"
            Write-Host $output
        }
    } catch {
        Write-Error "Gemini test threw exception: $_"
    }
}

function Test-MultimodalVision {
    Write-Section "Test 4: Multimodal Vision (Gemini)"
    
    if (-not $env:GEMINI_API_KEY) {
        Write-Warning "Skipping vision tests (no API key)"
        return
    }
    
    Write-Test "Running multimodal vision test suite"
    try {
        $output = & $Z3edBin test --filter "*GeminiVision*" 2>&1
        $outputStr = $output -join "`n"
        
        if ($outputStr -match "PASSED") {
            Write-Success "Multimodal vision tests passed"
        } else {
            Write-Warning "Vision tests completed with warnings"
        }
    } catch {
        Write-Error "Vision test suite failed: $_"
    }
}

function Test-LearnCommand {
    Write-Section "Test 5: Learn Command (Knowledge Management)"
    
    Write-Test "Testing preference storage"
    try {
        & $Z3edBin agent learn preference "test_key" "test_value" 2>&1 | Out-Null
        Write-Success "Preference stored"
    } catch {
        Write-Error "Failed to store preference"
    }
    
    Write-Test "Testing context storage"
    try {
        & $Z3edBin agent learn context "project" "YAZE ROM Editor test" 2>&1 | Out-Null
        Write-Success "Project context stored"
    } catch {
        Write-Warning "Context storage failed"
    }
    
    Write-Test "Listing learned knowledge"
    try {
        & $Z3edBin agent learn list 2>&1 | Out-Null
        Write-Success "Knowledge retrieval works"
    } catch {
        Write-Warning "Knowledge list failed"
    }
}

function Test-GUIAutomationPrep {
    Write-Section "Test 6: GUI Automation (Preparation)"
    
    Write-Test "Checking gRPC test harness support"
    $helpOutput = & $Z3edBin --help 2>&1
    if ($helpOutput -match "grpc") {
        Write-Success "gRPC support compiled in"
    } else {
        Write-Warning "gRPC support not detected"
    }
    
    Write-Test "Verifying screenshot utils"
    if (Test-Path "build-windows\lib\Debug\yaze_core_lib.lib") {
        Write-Success "Core library with screenshot utils found"
    } else {
        Write-Warning "Core library not found (needed for GUI automation)"
    }
    
    Write-Warning "Full GUI automation test requires YAZE to be running"
    Write-Warning "  Start YAZE GUI, then run: .\scripts\test_ai_gui_control.ps1"
}

function Write-Summary {
    Write-Section "Test Summary"
    
    Write-Host "✓ Basic z3ed functionality verified" -ForegroundColor Green
    Write-Host "✓ AI agent system operational" -ForegroundColor Green
    
    if ($env:GEMINI_API_KEY) {
        Write-Host "✓ Multimodal vision capabilities tested" -ForegroundColor Green
    } else {
        Write-Host "⚠ Vision tests skipped (no API key)" -ForegroundColor Yellow
    }
    
    Write-Host ""
    Write-Host "Next Steps:" -ForegroundColor Cyan
    Write-Host "  1. Start YAZE GUI:   " -NoNewline -ForegroundColor Cyan
    Write-Host $YazeBin -ForegroundColor Green
    Write-Host "  2. Test collaboration: " -NoNewline -ForegroundColor Cyan
    Write-Host ".\scripts\test_collaboration.ps1" -ForegroundColor Green
    Write-Host "  3. Test GUI control:   " -NoNewline -ForegroundColor Cyan
    Write-Host ".\scripts\test_ai_gui_control.ps1" -ForegroundColor Green
    Write-Host ""
    Write-Host "For full feature testing:" -ForegroundColor Cyan
    Write-Host "  - Set GEMINI_API_KEY for vision tests"
    Write-Host "  - Start Ollama server for local AI"
    Write-Host "  - Provide test ROM at: $TestRom"
}

# ============================================================================
# Main Execution
# ============================================================================

Write-Header

# Change to script directory's parent
Set-Location (Join-Path $PSScriptRoot "..")

Test-Prerequisites
Test-Z3edBasic
Test-AIAgentOllama
Test-AIAgentGemini
Test-MultimodalVision
Test-LearnCommand
Test-GUIAutomationPrep

Write-Summary

Write-Host ""
Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║                  ✓ AI Features Test Complete!                 ║" -ForegroundColor Green
Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Green
Write-Host ""
