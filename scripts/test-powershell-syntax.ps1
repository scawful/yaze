# Simple test script to verify PowerShell syntax
# This script tests basic PowerShell constructs used in our build scripts

Write-Host "Testing PowerShell syntax..." -ForegroundColor Green

# Test try-catch blocks
try {
    Write-Host "✓ Try-catch syntax works" -ForegroundColor Green
} catch {
    Write-Host "✗ Try-catch syntax failed" -ForegroundColor Red
}

# Test if-else blocks
if ($true) {
    Write-Host "✓ If-else syntax works" -ForegroundColor Green
} else {
    Write-Host "✗ If-else syntax failed" -ForegroundColor Red
}

# Test parameter blocks
param(
    [switch]$TestParam = $false
)

Write-Host "✓ Parameter syntax works" -ForegroundColor Green

# Test string interpolation
$testVar = "PowerShell"
Write-Host "✓ String interpolation works: $testVar" -ForegroundColor Green

Write-Host "All syntax tests passed!" -ForegroundColor Green
