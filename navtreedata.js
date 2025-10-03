/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "yaze", "index.html", [
    [ "Getting Started", "d0/d63/md_docs_201-getting-started.html", [
      [ "Quick Start", "d0/d63/md_docs_201-getting-started.html#autotoc_md1", null ],
      [ "General Tips", "d0/d63/md_docs_201-getting-started.html#autotoc_md2", null ],
      [ "Supported Features", "d0/d63/md_docs_201-getting-started.html#autotoc_md3", null ],
      [ "Command Line Interface", "d0/d63/md_docs_201-getting-started.html#autotoc_md4", null ],
      [ "Extending Functionality", "d0/d63/md_docs_201-getting-started.html#autotoc_md5", null ]
    ] ],
    [ "Build Instructions", "d9/d41/md_docs_202-build-instructions.html", [
      [ "⚡ Build Environment Verification", "d9/d41/md_docs_202-build-instructions.html#autotoc_md7", [
        [ "Windows (PowerShell)", "d9/d41/md_docs_202-build-instructions.html#autotoc_md8", null ],
        [ "macOS/Linux", "d9/d41/md_docs_202-build-instructions.html#autotoc_md9", null ]
      ] ],
      [ "Quick Start", "d9/d41/md_docs_202-build-instructions.html#autotoc_md10", [
        [ "macOS (Apple Silicon)", "d9/d41/md_docs_202-build-instructions.html#autotoc_md11", null ],
        [ "Linux", "d9/d41/md_docs_202-build-instructions.html#autotoc_md12", null ],
        [ "Windows (Visual Studio CMake Workflow)", "d9/d41/md_docs_202-build-instructions.html#autotoc_md13", null ],
        [ "Minimal Build (CI/Fast)", "d9/d41/md_docs_202-build-instructions.html#autotoc_md14", null ]
      ] ],
      [ "Dependencies", "d9/d41/md_docs_202-build-instructions.html#autotoc_md15", [
        [ "Required", "d9/d41/md_docs_202-build-instructions.html#autotoc_md16", null ],
        [ "Bundled Libraries (Header-Only & Source)", "d9/d41/md_docs_202-build-instructions.html#autotoc_md17", null ],
        [ "Dependency Isolation", "d9/d41/md_docs_202-build-instructions.html#autotoc_md18", null ]
      ] ],
      [ "Platform Setup", "d9/d41/md_docs_202-build-instructions.html#autotoc_md19", [
        [ "macOS", "d9/d41/md_docs_202-build-instructions.html#autotoc_md20", null ],
        [ "Linux (Ubuntu/Debian)", "d9/d41/md_docs_202-build-instructions.html#autotoc_md21", null ],
        [ "Windows", "d9/d41/md_docs_202-build-instructions.html#autotoc_md22", [
          [ "Requirements", "d9/d41/md_docs_202-build-instructions.html#autotoc_md23", null ],
          [ "Optional: vcpkg for SDL2", "d9/d41/md_docs_202-build-instructions.html#autotoc_md24", null ],
          [ "vcpkg Integration (Optional)", "d9/d41/md_docs_202-build-instructions.html#autotoc_md25", null ]
        ] ]
      ] ],
      [ "Build Targets", "d9/d41/md_docs_202-build-instructions.html#autotoc_md26", [
        [ "Applications", "d9/d41/md_docs_202-build-instructions.html#autotoc_md27", null ],
        [ "Libraries", "d9/d41/md_docs_202-build-instructions.html#autotoc_md28", null ],
        [ "Development (Debug Builds Only)", "d9/d41/md_docs_202-build-instructions.html#autotoc_md29", null ]
      ] ],
      [ "Build Configurations", "d9/d41/md_docs_202-build-instructions.html#autotoc_md30", [
        [ "Debug (Full Features)", "d9/d41/md_docs_202-build-instructions.html#autotoc_md31", null ],
        [ "Minimal (CI/Fast Builds)", "d9/d41/md_docs_202-build-instructions.html#autotoc_md32", null ],
        [ "Release", "d9/d41/md_docs_202-build-instructions.html#autotoc_md33", null ]
      ] ],
      [ "Testing System", "d9/d41/md_docs_202-build-instructions.html#autotoc_md34", [
        [ "Test Categories", "d9/d41/md_docs_202-build-instructions.html#autotoc_md35", [
          [ "Unit Tests", "d9/d41/md_docs_202-build-instructions.html#autotoc_md36", null ],
          [ "Integration Tests", "d9/d41/md_docs_202-build-instructions.html#autotoc_md37", null ],
          [ "End-to-End (E2E) Tests", "d9/d41/md_docs_202-build-instructions.html#autotoc_md38", null ]
        ] ],
        [ "Running Tests", "d9/d41/md_docs_202-build-instructions.html#autotoc_md39", [
          [ "Local Development", "d9/d41/md_docs_202-build-instructions.html#autotoc_md40", null ],
          [ "CI/CD Environment", "d9/d41/md_docs_202-build-instructions.html#autotoc_md41", null ],
          [ "ROM-Dependent Tests", "d9/d41/md_docs_202-build-instructions.html#autotoc_md42", null ]
        ] ],
        [ "Test Organization", "d9/d41/md_docs_202-build-instructions.html#autotoc_md43", null ],
        [ "Test Executables", "d9/d41/md_docs_202-build-instructions.html#autotoc_md44", [
          [ "Development Build (<tt>yaze_test.cc</tt>)", "d9/d41/md_docs_202-build-instructions.html#autotoc_md45", null ],
          [ "CI Build (<tt>yaze_test_ci.cc</tt>)", "d9/d41/md_docs_202-build-instructions.html#autotoc_md46", null ]
        ] ],
        [ "Test Configuration", "d9/d41/md_docs_202-build-instructions.html#autotoc_md47", [
          [ "CMake Options", "d9/d41/md_docs_202-build-instructions.html#autotoc_md48", null ],
          [ "Test Filters", "d9/d41/md_docs_202-build-instructions.html#autotoc_md49", null ]
        ] ]
      ] ],
      [ "IDE Integration", "d9/d41/md_docs_202-build-instructions.html#autotoc_md50", [
        [ "Visual Studio (Windows)", "d9/d41/md_docs_202-build-instructions.html#autotoc_md51", null ],
        [ "VS Code", "d9/d41/md_docs_202-build-instructions.html#autotoc_md52", null ],
        [ "CLion", "d9/d41/md_docs_202-build-instructions.html#autotoc_md53", null ],
        [ "Xcode (macOS)", "d9/d41/md_docs_202-build-instructions.html#autotoc_md54", null ]
      ] ],
      [ "Windows Development Scripts", "d9/d41/md_docs_202-build-instructions.html#autotoc_md55", [
        [ "vcpkg Setup (Optional)", "d9/d41/md_docs_202-build-instructions.html#autotoc_md56", null ]
      ] ],
      [ "Features by Build Type", "d9/d41/md_docs_202-build-instructions.html#autotoc_md57", null ],
      [ "CMake Compatibility", "d9/d41/md_docs_202-build-instructions.html#autotoc_md58", [
        [ "Submodule Compatibility Issues", "d9/d41/md_docs_202-build-instructions.html#autotoc_md59", null ]
      ] ],
      [ "CI/CD and Release Builds", "d9/d41/md_docs_202-build-instructions.html#autotoc_md60", [
        [ "GitHub Actions Workflows", "d9/d41/md_docs_202-build-instructions.html#autotoc_md61", null ],
        [ "Test Execution in CI", "d9/d41/md_docs_202-build-instructions.html#autotoc_md62", null ],
        [ "vcpkg Fallback Mechanisms", "d9/d41/md_docs_202-build-instructions.html#autotoc_md63", null ],
        [ "Supported Architectures", "d9/d41/md_docs_202-build-instructions.html#autotoc_md64", null ]
      ] ],
      [ "Troubleshooting", "d9/d41/md_docs_202-build-instructions.html#autotoc_md65", [
        [ "Build Environment Issues", "d9/d41/md_docs_202-build-instructions.html#autotoc_md66", null ],
        [ "Dependency Conflicts", "d9/d41/md_docs_202-build-instructions.html#autotoc_md67", null ],
        [ "Windows CMake Issues", "d9/d41/md_docs_202-build-instructions.html#autotoc_md68", null ],
        [ "vcpkg Issues", "d9/d41/md_docs_202-build-instructions.html#autotoc_md69", null ],
        [ "Test Issues", "d9/d41/md_docs_202-build-instructions.html#autotoc_md70", null ],
        [ "Architecture Errors (macOS)", "d9/d41/md_docs_202-build-instructions.html#autotoc_md71", null ],
        [ "Missing Headers (Language Server)", "d9/d41/md_docs_202-build-instructions.html#autotoc_md72", null ],
        [ "CI Build Failures", "d9/d41/md_docs_202-build-instructions.html#autotoc_md73", null ],
        [ "Common Error Solutions", "d9/d41/md_docs_202-build-instructions.html#autotoc_md74", null ]
      ] ]
    ] ],
    [ "Asar 65816 Assembler Integration", "d2/dd3/md_docs_203-asar-integration.html", [
      [ "Quick Examples", "d2/dd3/md_docs_203-asar-integration.html#autotoc_md76", [
        [ "Command Line", "d2/dd3/md_docs_203-asar-integration.html#autotoc_md77", null ],
        [ "C++ API", "d2/dd3/md_docs_203-asar-integration.html#autotoc_md78", null ]
      ] ],
      [ "Assembly Patch Examples", "d2/dd3/md_docs_203-asar-integration.html#autotoc_md79", [
        [ "Basic Hook", "d2/dd3/md_docs_203-asar-integration.html#autotoc_md80", null ],
        [ "Advanced Features", "d2/dd3/md_docs_203-asar-integration.html#autotoc_md81", null ]
      ] ],
      [ "API Reference", "d2/dd3/md_docs_203-asar-integration.html#autotoc_md82", [
        [ "AsarWrapper Class", "d2/dd3/md_docs_203-asar-integration.html#autotoc_md83", null ],
        [ "Data Structures", "d2/dd3/md_docs_203-asar-integration.html#autotoc_md84", null ]
      ] ],
      [ "Testing", "d2/dd3/md_docs_203-asar-integration.html#autotoc_md85", [
        [ "ROM-Dependent Tests", "d2/dd3/md_docs_203-asar-integration.html#autotoc_md86", null ]
      ] ],
      [ "Error Handling", "d2/dd3/md_docs_203-asar-integration.html#autotoc_md87", null ],
      [ "Development Workflow", "d2/dd3/md_docs_203-asar-integration.html#autotoc_md88", null ]
    ] ],
    [ "API Reference", "dd/d96/md_docs_204-api-reference.html", [
      [ "C API (<tt>incl/yaze.h</tt>, <tt>incl/zelda.h</tt>)", "dd/d96/md_docs_204-api-reference.html#autotoc_md90", [
        [ "Core Library Functions", "dd/d96/md_docs_204-api-reference.html#autotoc_md91", null ],
        [ "ROM Operations", "dd/d96/md_docs_204-api-reference.html#autotoc_md92", null ],
        [ "Graphics Operations", "dd/d96/md_docs_204-api-reference.html#autotoc_md93", null ],
        [ "Palette System", "dd/d96/md_docs_204-api-reference.html#autotoc_md94", null ],
        [ "Message System", "dd/d96/md_docs_204-api-reference.html#autotoc_md95", null ]
      ] ],
      [ "C++ API", "dd/d96/md_docs_204-api-reference.html#autotoc_md96", [
        [ "AsarWrapper (<tt>src/app/core/asar_wrapper.h</tt>)", "dd/d96/md_docs_204-api-reference.html#autotoc_md97", null ],
        [ "Data Structures", "dd/d96/md_docs_204-api-reference.html#autotoc_md98", [
          [ "ROM Version Support", "dd/d96/md_docs_204-api-reference.html#autotoc_md99", null ],
          [ "SNES Graphics", "dd/d96/md_docs_204-api-reference.html#autotoc_md100", null ],
          [ "Message System", "dd/d96/md_docs_204-api-reference.html#autotoc_md101", null ]
        ] ]
      ] ],
      [ "Error Handling", "dd/d96/md_docs_204-api-reference.html#autotoc_md102", [
        [ "Status Codes", "dd/d96/md_docs_204-api-reference.html#autotoc_md103", null ],
        [ "Error Handling Pattern", "dd/d96/md_docs_204-api-reference.html#autotoc_md104", null ]
      ] ],
      [ "Extension System", "dd/d96/md_docs_204-api-reference.html#autotoc_md105", [
        [ "Plugin Architecture", "dd/d96/md_docs_204-api-reference.html#autotoc_md106", null ],
        [ "Capability Flags", "dd/d96/md_docs_204-api-reference.html#autotoc_md107", null ]
      ] ],
      [ "Backward Compatibility", "dd/d96/md_docs_204-api-reference.html#autotoc_md108", null ]
    ] ],
    [ "Testing Guide", "d6/d10/md_docs_2A1-testing-guide.html", [
      [ "Test Categories", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md110", [
        [ "Stable Tests (STABLE)", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md111", null ],
        [ "ROM-Dependent Tests (ROM_DEPENDENT)", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md112", null ],
        [ "Experimental Tests (EXPERIMENTAL)", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md113", null ]
      ] ],
      [ "Command Line Usage", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md114", null ],
      [ "CMake Presets", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md115", null ],
      [ "Writing Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md116", [
        [ "Stable Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md117", null ],
        [ "ROM-Dependent Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md118", null ],
        [ "Experimental Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md119", null ]
      ] ],
      [ "CI/CD Integration", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md120", [
        [ "GitHub Actions", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md121", null ],
        [ "Test Execution Strategy", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md122", null ]
      ] ],
      [ "Test Development Guidelines", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md123", [
        [ "Writing Stable Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md124", null ],
        [ "Writing ROM-Dependent Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md125", null ],
        [ "Writing Experimental Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md126", null ]
      ] ],
      [ "E2E GUI Testing Framework", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md127", [
        [ "Overview", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md128", null ],
        [ "Architecture", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md129", null ],
        [ "Writing E2E Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md130", null ],
        [ "Helper Functions", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md131", null ],
        [ "Running GUI Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md132", null ],
        [ "Test Categories for E2E", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md133", null ],
        [ "Development Status", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md134", null ],
        [ "Best Practices", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md135", null ]
      ] ],
      [ "Performance and Maintenance", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md136", [
        [ "Regular Review", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md137", null ],
        [ "Performance Monitoring", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md138", null ],
        [ "E2E Test Maintenance", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md139", null ]
      ] ]
    ] ],
    [ "Comprehensive ZScream vs YAZE Overworld Analysis", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html", [
      [ "Executive Summary", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md141", null ],
      [ "Key Findings", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md142", [
        [ "✅ <strong>Confirmed Correct Implementations</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md143", [
          [ "1. <strong>Tile32 Expansion Detection Logic</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md144", null ],
          [ "2. <strong>Tile16 Expansion Detection Logic</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md145", null ],
          [ "3. <strong>Entrance Coordinate Calculation</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md146", null ],
          [ "4. <strong>Hole Coordinate Calculation (with 0x400 offset)</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md147", null ],
          [ "5. <strong>Exit Data Loading</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md148", null ],
          [ "6. <strong>Item Loading with ASM Version Detection</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md149", null ]
        ] ],
        [ "⚠️ <strong>Key Differences Found</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md150", [
          [ "1. <strong>Entrance Expansion Detection</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md151", null ],
          [ "2. <strong>Address Constants</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md152", null ],
          [ "3. <strong>Decompression Logic</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md153", null ]
        ] ],
        [ "🔍 <strong>Additional Findings</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md154", [
          [ "1. <strong>Error Handling</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md155", null ],
          [ "2. <strong>Memory Management</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md156", null ],
          [ "3. <strong>Data Structures</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md157", null ],
          [ "4. <strong>Threading</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md158", null ]
        ] ],
        [ "📊 <strong>Validation Results</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md159", null ],
        [ "🎯 <strong>Conclusion</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md160", null ]
      ] ]
    ] ],
    [ "DungeonEditor Parallel Optimization Implementation", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html", [
      [ "🚀 <strong>Parallelization Strategy Implemented</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md162", [
        [ "<strong>Problem Identified</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md163", null ],
        [ "<strong>Solution: Multi-Threaded Room Loading</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md164", [
          [ "<strong>Key Optimizations</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md165", null ],
          [ "<strong>Parallel Processing Flow</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md166", null ],
          [ "<strong>Thread Safety Features</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md167", null ]
        ] ],
        [ "<strong>Expected Performance Impact</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md168", [
          [ "<strong>Theoretical Speedup</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md169", null ],
          [ "<strong>Expected Results</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md170", null ]
        ] ],
        [ "<strong>Technical Implementation Details</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md171", [
          [ "<strong>Thread Management</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md172", null ],
          [ "<strong>Result Processing</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md173", null ]
        ] ],
        [ "<strong>Monitoring and Validation</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md174", [
          [ "<strong>Performance Timing Added</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md175", null ],
          [ "<strong>Logging and Debugging</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md176", null ]
        ] ],
        [ "<strong>Benefits of This Approach</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md177", null ],
        [ "<strong>Next Steps</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md178", null ]
      ] ]
    ] ],
    [ "Overworld::Load Performance Analysis and Optimization Plan", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html", [
      [ "Current Performance Profile", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md180", null ],
      [ "Detailed Analysis of Overworld::Load", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md181", [
        [ "Current Implementation Breakdown", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md182", null ]
      ] ],
      [ "Major Bottlenecks Identified", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md183", [
        [ "1. <strong>DecompressAllMapTiles() - PRIMARY BOTTLENECK (~1.5-2.0 seconds)</strong>", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md184", null ],
        [ "2. <strong>AssembleMap32Tiles() - SECONDARY BOTTLENECK (~200-400ms)</strong>", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md185", null ],
        [ "3. <strong>AssembleMap16Tiles() - MODERATE BOTTLENECK (~100-200ms)</strong>", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md186", null ]
      ] ],
      [ "Optimization Strategies", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md187", [
        [ "1. <strong>Parallelize Decompression Operations</strong>", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md188", null ],
        [ "2. <strong>Optimize ROM Access Patterns</strong>", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md189", null ],
        [ "3. <strong>Implement Lazy Map Loading</strong>", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md190", null ],
        [ "4. <strong>Optimize HyruleMagicDecompress</strong>", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md191", null ],
        [ "5. <strong>Memory Pool Optimization</strong>", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md192", null ]
      ] ],
      [ "Implementation Priority", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md193", [
        [ "Phase 1: High Impact, Low Risk (Immediate)", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md194", null ],
        [ "Phase 2: Medium Impact, Medium Risk (Next)", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md195", null ],
        [ "Phase 3: Lower Impact, Higher Risk (Future)", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md196", null ]
      ] ],
      [ "Expected Performance Improvements", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md197", [
        [ "Conservative Estimates", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md198", null ],
        [ "Aggressive Estimates", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md199", null ]
      ] ],
      [ "Conclusion", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md200", null ]
    ] ],
    [ "yaze Performance Optimization Summary", "db/de6/md_docs_2analysis_2performance__optimization__summary.html", [
      [ "🎉 <strong>Massive Performance Improvements Achieved!</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md202", [
        [ "📊 <strong>Overall Performance Results</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md203", null ]
      ] ],
      [ "🚀 <strong>Optimizations Implemented</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md204", [
        [ "1. <strong>Performance Monitoring System with Feature Flag</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md205", [
          [ "<strong>Features Added</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md206", null ],
          [ "<strong>Implementation</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md207", null ]
        ] ],
        [ "2. <strong>DungeonEditor Parallel Loading (79% Speedup)</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md208", [
          [ "<strong>Problem Solved</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md209", null ],
          [ "<strong>Solution: Multi-Threaded Room Loading</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md210", null ],
          [ "<strong>Key Features</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md211", null ]
        ] ],
        [ "3. <strong>Incremental Overworld Map Loading</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md212", [
          [ "<strong>Problem Solved</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md213", null ],
          [ "<strong>Solution: Priority-Based Incremental Loading</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md214", null ],
          [ "<strong>Key Features</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md215", null ]
        ] ],
        [ "4. <strong>On-Demand Map Reloading</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md216", [
          [ "<strong>Problem Solved</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md217", null ],
          [ "<strong>Solution: Intelligent Refresh System</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md218", null ],
          [ "<strong>Key Features</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md219", null ]
        ] ]
      ] ],
      [ "🎯 <strong>Technical Architecture</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md220", [
        [ "<strong>Performance Monitoring System</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md221", null ],
        [ "<strong>Parallel Loading Architecture</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md222", null ],
        [ "<strong>Incremental Loading Flow</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md223", null ]
      ] ],
      [ "📈 <strong>Performance Impact Analysis</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md224", [
        [ "<strong>DungeonEditor Optimization</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md225", null ],
        [ "<strong>OverworldEditor Optimization</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md226", null ],
        [ "<strong>Overall System Impact</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md227", null ]
      ] ],
      [ "🔧 <strong>Configuration Options</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md228", [
        [ "<strong>Performance Monitoring</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md229", null ],
        [ "<strong>Parallel Loading Tuning</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md230", null ],
        [ "<strong>Incremental Loading Tuning</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md231", null ]
      ] ],
      [ "🎯 <strong>Future Optimization Opportunities</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md232", [
        [ "<strong>Potential Further Improvements</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md233", null ],
        [ "<strong>Monitoring and Profiling</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md234", null ]
      ] ],
      [ "✅ <strong>Success Metrics Achieved</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md235", null ],
      [ "🚀 <strong>Result: Lightning-Fast YAZE</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md236", null ]
    ] ],
    [ "Renderer Class Performance Analysis and Optimization", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html", [
      [ "Overview", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md238", null ],
      [ "Original Performance Issues", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md239", [
        [ "1. Blocking Texture Creation", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md240", null ],
        [ "2. Inefficient Loading Pattern", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md241", null ]
      ] ],
      [ "Optimizations Implemented", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md242", [
        [ "1. Deferred Texture Creation", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md243", null ],
        [ "2. Lazy Loading System", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md244", null ],
        [ "3. Performance Monitoring", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md245", null ]
      ] ],
      [ "Thread Safety Considerations", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md246", [
        [ "Main Thread Requirement", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md247", null ],
        [ "Safe Optimization Approach", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md248", null ]
      ] ],
      [ "Performance Improvements", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md249", [
        [ "Loading Time Reduction", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md250", null ],
        [ "Memory Efficiency", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md251", null ]
      ] ],
      [ "Implementation Details", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md252", [
        [ "Modified Files", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md253", null ],
        [ "Key Methods Added", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md254", null ]
      ] ],
      [ "Usage Guidelines", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md255", [
        [ "For Developers", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md256", null ],
        [ "For ROM Loading", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md257", null ]
      ] ],
      [ "Future Optimization Opportunities", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md258", [
        [ "1. Background Threading (Pending)", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md259", null ],
        [ "2. Arena Management Optimization (Pending)", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md260", null ],
        [ "3. Advanced Lazy Loading (Pending)", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md261", null ]
      ] ],
      [ "Conclusion", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md262", null ]
    ] ],
    [ "ZScream C# vs YAZE C++ Overworld Implementation Analysis", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html", [
      [ "Overview", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md264", null ],
      [ "Executive Summary", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md265", null ],
      [ "Detailed Comparison", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md266", [
        [ "1. Tile32 Loading and Expansion Detection", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md267", [
          [ "ZScream C# Logic (<tt>Overworld.cs:706-756</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md268", null ],
          [ "yaze C++ Logic (<tt>overworld.cc:AssembleMap32Tiles</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md269", null ]
        ] ],
        [ "2. Tile16 Loading and Expansion Detection", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md270", [
          [ "ZScream C# Logic (<tt>Overworld.cs:652-705</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md271", null ],
          [ "yaze C++ Logic (<tt>overworld.cc:AssembleMap16Tiles</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md272", null ]
        ] ],
        [ "3. Map Decompression", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md273", [
          [ "ZScream C# Logic (<tt>Overworld.cs:767-904</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md274", null ],
          [ "yaze C++ Logic (<tt>overworld.cc:DecompressAllMapTiles</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md275", null ]
        ] ],
        [ "4. Entrance Coordinate Calculation", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md276", [
          [ "ZScream C# Logic (<tt>Overworld.cs:974-1001</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md277", null ],
          [ "yaze C++ Logic (<tt>overworld.cc:LoadEntrances</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md278", null ]
        ] ],
        [ "5. Hole Coordinate Calculation with 0x400 Offset", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md279", [
          [ "ZScream C# Logic (<tt>Overworld.cs:1002-1025</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md280", null ],
          [ "yaze C++ Logic (<tt>overworld.cc:LoadHoles</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md281", null ]
        ] ],
        [ "6. ASM Version Detection for Item Loading", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md282", [
          [ "ZScream C# Logic (<tt>Overworld.cs:1032-1094</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md283", null ],
          [ "yaze C++ Logic (<tt>overworld.cc:LoadItems</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md284", null ]
        ] ],
        [ "7. Game State Handling for Sprite Loading", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md285", [
          [ "ZScream C# Logic (<tt>Overworld.cs:1276-1494</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md286", null ],
          [ "yaze C++ Logic (<tt>overworld.cc:LoadSprites</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md287", null ]
        ] ],
        [ "8. Map Size Assignment Logic", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md288", [
          [ "ZScream C# Logic (<tt>Overworld.cs:296-390</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md289", null ],
          [ "yaze C++ Logic (<tt>overworld.cc:AssignMapSizes</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md290", null ]
        ] ]
      ] ],
      [ "ZSCustomOverworld Integration", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md291", [
        [ "Version Detection", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md292", null ],
        [ "Feature Enablement", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md293", null ]
      ] ],
      [ "Integration Test Coverage", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md294", null ],
      [ "Conclusion", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md295", null ]
    ] ],
    [ "Atlas Rendering Implementation - YAZE Graphics Optimizations", "db/d26/md_docs_2atlas__rendering__implementation.html", [
      [ "Overview", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md297", null ],
      [ "Implementation Details", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md298", [
        [ "Core Components", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md299", [
          [ "1. AtlasRenderer Class (<tt>src/app/gfx/atlas_renderer.h/cc</tt>)", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md300", null ],
          [ "2. RenderCommand Structure", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md301", null ],
          [ "3. Atlas Statistics Tracking", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md302", null ]
        ] ],
        [ "Integration Points", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md303", [
          [ "1. Tilemap Integration (<tt>src/app/gfx/tilemap.h/cc</tt>)", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md304", null ],
          [ "2. Performance Dashboard Integration", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md305", null ],
          [ "3. Benchmarking Suite (<tt>test/gfx_optimization_benchmarks.cc</tt>)", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md306", null ]
        ] ],
        [ "Technical Implementation", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md307", [
          [ "Atlas Packing Algorithm", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md308", null ],
          [ "Batch Rendering Process", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md309", null ]
        ] ],
        [ "Performance Improvements", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md310", [
          [ "Measured Performance Gains", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md311", null ],
          [ "Benchmark Results", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md312", null ]
        ] ],
        [ "ROM Hacking Workflow Benefits", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md313", [
          [ "Graphics Sheet Management", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md314", null ],
          [ "Editor Performance", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md315", null ]
        ] ],
        [ "API Usage Examples", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md316", [
          [ "Basic Atlas Usage", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md317", null ],
          [ "Tilemap Integration", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md318", null ]
        ] ],
        [ "Memory Management", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md319", [
          [ "Automatic Cleanup", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md320", null ],
          [ "Resource Management", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md321", null ]
        ] ],
        [ "Future Enhancements", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md322", [
          [ "Planned Improvements", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md323", null ],
          [ "Integration Opportunities", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md324", null ]
        ] ]
      ] ],
      [ "Conclusion", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md325", null ],
      [ "Files Modified", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md326", null ]
    ] ],
    [ "Contributing", "dd/d5b/md_docs_2B1-contributing.html", [
      [ "Development Setup", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md328", [
        [ "Prerequisites", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md329", null ],
        [ "Quick Start", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md330", null ]
      ] ],
      [ "Code Style", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md331", [
        [ "C++ Standards", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md332", null ],
        [ "File Organization", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md333", null ],
        [ "Error Handling", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md334", null ]
      ] ],
      [ "Testing Requirements", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md335", [
        [ "Test Categories", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md336", null ],
        [ "Writing Tests", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md337", null ],
        [ "Test Execution", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md338", null ]
      ] ],
      [ "Pull Request Process", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md339", [
        [ "Before Submitting", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md340", null ],
        [ "Pull Request Template", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md341", null ]
      ] ],
      [ "Development Workflow", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md342", [
        [ "Branch Strategy", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md343", null ],
        [ "Commit Messages", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md344", null ],
        [ "Types", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md345", null ]
      ] ],
      [ "Architecture Guidelines", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md346", [
        [ "Component Design", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md347", null ],
        [ "Memory Management", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md348", null ],
        [ "Performance", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md349", null ]
      ] ],
      [ "Documentation", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md350", [
        [ "Code Documentation", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md351", null ],
        [ "API Documentation", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md352", null ]
      ] ],
      [ "Community Guidelines", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md353", [
        [ "Communication", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md354", null ],
        [ "Getting Help", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md355", null ]
      ] ],
      [ "Release Process", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md356", [
        [ "Version Numbering", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md357", null ],
        [ "Release Checklist", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md358", null ]
      ] ]
    ] ],
    [ "Platform Compatibility Improvements", "d1/d30/md_docs_2B2-platform-compatibility.html", [
      [ "Native File Dialog Support", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md360", null ],
      [ "Cross-Platform Build Reliability", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md361", null ],
      [ "Build Configuration Options", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md362", [
        [ "Full Build (Development)", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md363", null ],
        [ "Minimal Build", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md364", null ]
      ] ],
      [ "Implementation Details", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md365", null ],
      [ "Platform-Specific Adaptations", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md366", [
        [ "Windows", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md367", null ],
        [ "macOS", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md368", null ],
        [ "Linux", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md369", null ]
      ] ]
    ] ],
    [ "Build Presets Guide", "d7/d51/md_docs_2B3-build-presets.html", [
      [ "🍎 macOS ARM64 Presets (Recommended for Apple Silicon)", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md371", [
        [ "For Development Work:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md372", null ],
        [ "For Distribution:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md373", null ]
      ] ],
      [ "🔧 Why This Fixes Architecture Errors", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md374", null ],
      [ "📋 Available Presets", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md375", null ],
      [ "🚀 Quick Start", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md376", null ],
      [ "🛠️ IDE Integration", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md377", [
        [ "VS Code with CMake Tools:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md378", null ],
        [ "CLion:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md379", null ],
        [ "Xcode:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md380", null ]
      ] ],
      [ "🔍 Troubleshooting", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md381", null ],
      [ "📝 Notes", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md382", null ]
    ] ],
    [ "Release Workflows Documentation", "d3/d49/md_docs_2B4-release-workflows.html", [
      [ "Overview", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md384", null ],
      [ "1. Release-Simplified (<tt>release-simplified.yml</tt>)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md386", [
        [ "Purpose", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md387", null ],
        [ "Key Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md388", null ],
        [ "Use Cases", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md389", null ],
        [ "Configuration", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md390", null ],
        [ "Platforms Supported", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md391", null ]
      ] ],
      [ "2. Release (<tt>release.yml</tt>)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md393", [
        [ "Purpose", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md394", null ],
        [ "Key Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md395", null ],
        [ "Use Cases", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md396", null ],
        [ "Configuration", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md397", null ],
        [ "Advantages over Simplified", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md398", null ]
      ] ],
      [ "3. Release-Complex (<tt>release-complex.yml</tt>)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md400", [
        [ "Purpose", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md401", null ],
        [ "Key Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md402", null ],
        [ "Use Cases", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md403", null ],
        [ "Fallback Mechanisms", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md404", [
          [ "vcpkg Fallback", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md405", null ],
          [ "Chocolatey Integration", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md406", null ],
          [ "Build Configuration Fallback", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md407", null ]
        ] ],
        [ "Advanced Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md408", null ]
      ] ],
      [ "Workflow Comparison Matrix", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md410", null ],
      [ "When to Use Each Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md412", [
        [ "Use Simplified When:", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md413", null ],
        [ "Use Release When:", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md414", null ],
        [ "Use Complex When:", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md415", null ]
      ] ],
      [ "Workflow Selection Guide", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md417", [
        [ "For Development Team", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md418", null ],
        [ "For Release Manager", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md419", null ],
        [ "For CI/CD Pipeline", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md420", null ]
      ] ],
      [ "Configuration Examples", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md422", [
        [ "Triggering a Release", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md423", [
          [ "Manual Release (All Workflows)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md424", null ],
          [ "Automatic Release (Tag Push)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md425", null ]
        ] ],
        [ "Customizing Release Notes", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md426", null ]
      ] ],
      [ "Troubleshooting", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md428", [
        [ "Common Issues", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md429", [
          [ "vcpkg Failures (Windows)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md430", null ],
          [ "Dependency Conflicts", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md431", null ],
          [ "Build Failures", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md432", null ]
        ] ],
        [ "Debug Information", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md433", [
          [ "Simplified Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md434", null ],
          [ "Release Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md435", null ],
          [ "Complex Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md436", null ]
        ] ]
      ] ],
      [ "Best Practices", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md438", [
        [ "Workflow Selection", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md439", null ],
        [ "Release Process", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md440", null ],
        [ "Maintenance", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md441", null ]
      ] ],
      [ "Future Improvements", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md443", [
        [ "Planned Enhancements", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md444", null ],
        [ "Integration Opportunities", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md445", null ]
      ] ]
    ] ],
    [ "Stability, Testability & Release Workflow Improvements", "d9/d6b/md_docs_2B5-stability-improvements.html", [
      [ "Recent Improvements (v0.3.2)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md448", [
        [ "Windows Platform Stability", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md449", [
          [ "Stack Size Optimization", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md450", null ],
          [ "Development Utility Isolation", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md451", null ]
        ] ],
        [ "Graphics System Stability", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md452", [
          [ "Segmentation Fault Resolution", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md453", null ],
          [ "Comprehensive Bounds Checking", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md454", null ]
        ] ],
        [ "Build System Improvements", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md455", [
          [ "Modern Windows Workflow", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md456", null ],
          [ "Enhanced CI/CD Reliability", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md457", null ]
        ] ]
      ] ],
      [ "Recommended Optimizations", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md458", [
        [ "High Priority", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md459", [
          [ "1. Lazy Graphics Loading", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md460", null ],
          [ "2. Heap-Based Large Allocations", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md461", null ],
          [ "3. Streaming ROM Assets", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md462", null ]
        ] ],
        [ "Medium Priority", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md463", [
          [ "4. Enhanced Test Isolation", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md464", null ],
          [ "5. Dependency Caching Optimization", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md465", null ],
          [ "6. Memory Pool for Graphics", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md466", null ]
        ] ],
        [ "Low Priority", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md467", [
          [ "7. Build Time Optimization", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md468", null ],
          [ "8. Release Workflow Simplification", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md469", null ]
        ] ]
      ] ],
      [ "Testing Improvements", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md470", [
        [ "Current State", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md471", null ],
        [ "Recommendations", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md472", [
          [ "1. Visual Regression Testing", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md473", null ],
          [ "2. Performance Benchmarks", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md474", null ],
          [ "3. Fuzz Testing", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md475", null ]
        ] ]
      ] ],
      [ "Metrics & Monitoring", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md476", [
        [ "Current Measurements", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md477", null ],
        [ "Target Improvements", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md478", null ]
      ] ],
      [ "Action Items", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md479", [
        [ "Immediate (v0.3.2)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md480", null ],
        [ "Short Term (v0.3.3)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md481", null ],
        [ "Medium Term (v0.4.0)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md482", null ],
        [ "Long Term (v0.5.0+)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md483", null ]
      ] ],
      [ "Conclusion", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md484", null ]
    ] ],
    [ "Changelog", "d7/d6f/md_docs_2C1-changelog.html", [
      [ "0.3.2 (December 2025) - In Development", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md486", [
        [ "Tile16 Editor Improvements (In Progress)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md487", null ],
        [ "Graphics System Optimizations", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md488", null ],
        [ "Windows Platform Stability", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md489", null ],
        [ "Memory Safety & Stability", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md490", null ],
        [ "Testing & CI/CD Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md491", null ],
        [ "Future Optimizations (Planned)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md492", null ],
        [ "Technical Notes", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md493", null ]
      ] ],
      [ "0.3.1 (September 2025)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md494", [
        [ "Major Features", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md495", null ],
        [ "Tile16 Editor Enhancements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md496", null ],
        [ "ZSCustomOverworld v3 Implementation", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md497", null ],
        [ "Technical Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md498", null ],
        [ "User Interface", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md499", null ],
        [ "Bug Fixes", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md500", null ],
        [ "ZScream Compatibility Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md501", null ]
      ] ],
      [ "0.3.0 (September 2025)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md502", [
        [ "Major Features", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md503", null ],
        [ "User Interface & Theming", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md504", null ],
        [ "Enhancements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md505", null ],
        [ "Technical Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md506", null ],
        [ "Bug Fixes", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md507", null ]
      ] ],
      [ "0.2.2 (December 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md508", null ],
      [ "0.2.1 (August 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md509", null ],
      [ "0.2.0 (July 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md510", null ],
      [ "0.1.0 (May 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md511", null ],
      [ "0.0.9 (April 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md512", null ],
      [ "0.0.8 (February 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md513", null ],
      [ "0.0.7 (January 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md514", null ],
      [ "0.0.6 (November 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md515", null ],
      [ "0.0.5 (November 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md516", null ],
      [ "0.0.4 (November 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md517", null ],
      [ "0.0.3 (October 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md518", null ]
    ] ],
    [ "Canvas System - Comprehensive Guide", "d2/db2/md_docs_2CANVAS__GUIDE.html", [
      [ "Overview", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md520", null ],
      [ "Core Concepts", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md521", [
        [ "Canvas Structure", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md522", null ],
        [ "Coordinate Systems", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md523", null ]
      ] ],
      [ "Usage Patterns", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md524", [
        [ "Pattern 1: Basic Bitmap Display", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md525", null ],
        [ "Pattern 2: Modern Begin/End", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md526", null ],
        [ "Pattern 3: RAII ScopedCanvas", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md527", null ]
      ] ],
      [ "Feature: Tile Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md528", [
        [ "Single Tile Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md529", null ],
        [ "Tilemap Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md530", null ],
        [ "Color Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md531", null ]
      ] ],
      [ "Feature: Tile Selection", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md532", [
        [ "Single Tile Selection", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md533", null ],
        [ "Multi-Tile Rectangle Selection", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md534", null ],
        [ "Rectangle Drag & Paint", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md535", null ]
      ] ],
      [ "Feature: Custom Overlays", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md536", [
        [ "Manual Points Manipulation", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md537", null ]
      ] ],
      [ "Feature: Large Map Support", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md538", [
        [ "Map Types", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md539", null ],
        [ "Boundary Clamping", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md540", null ],
        [ "Custom Map Sizes", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md541", null ]
      ] ],
      [ "Feature: Context Menu", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md542", [
        [ "Adding Custom Items", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md543", null ],
        [ "Overworld Editor Example", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md544", null ]
      ] ],
      [ "Feature: Scratch Space (In Progress)", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md545", null ],
      [ "Common Workflows", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md546", [
        [ "Workflow 1: Overworld Tile Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md547", null ],
        [ "Workflow 2: Tile16 Blockset Selection", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md548", null ],
        [ "Workflow 3: Graphics Sheet Display", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md549", null ]
      ] ],
      [ "Configuration", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md550", [
        [ "Grid Settings", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md551", null ],
        [ "Scale Settings", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md552", null ],
        [ "Interaction Settings", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md553", null ],
        [ "Large Map Settings", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md554", null ]
      ] ],
      [ "Bug Fixes Applied", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md555", [
        [ "1. Rectangle Selection Wrapping in Large Maps ✅", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md556", null ],
        [ "2. Drag-Time Preview Clamping ✅", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md557", null ]
      ] ],
      [ "API Reference", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md558", [
        [ "Drawing Methods", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md559", null ],
        [ "State Accessors", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md560", null ],
        [ "Configuration", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md561", null ]
      ] ],
      [ "Implementation Notes", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md562", [
        [ "Points Management", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md563", null ],
        [ "Selection State", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md564", null ],
        [ "Overworld Rectangle Painting Flow", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md565", null ]
      ] ],
      [ "Best Practices", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md566", [
        [ "DO ✅", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md567", null ],
        [ "DON'T ❌", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md568", null ]
      ] ],
      [ "Troubleshooting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md569", [
        [ "Issue: Rectangle wraps at boundaries", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md570", null ],
        [ "Issue: Painting in wrong location", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md571", null ],
        [ "Issue: Array index out of bounds", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md572", null ],
        [ "Issue: Forgot to call End()", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md573", null ]
      ] ],
      [ "Future: Scratch Space", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md574", null ],
      [ "Documentation Files", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md575", null ],
      [ "Summary", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md576", null ]
    ] ],
    [ "Canvas Refactoring - Current Status & Future Work", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html", [
      [ "✅ Successfully Completed", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md578", [
        [ "1. Modern ImGui-Style Interface (Working)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md579", null ],
        [ "2. Context Menu Improvements (Working)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md580", null ],
        [ "3. Optional CanvasInteractionHandler Component (Available)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md581", null ],
        [ "4. Code Cleanup", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md582", null ]
      ] ],
      [ "⚠️ Outstanding Issue: Rectangle Selection Wrapping", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md583", [
        [ "The Problem", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md584", null ],
        [ "What Was Attempted", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md585", null ],
        [ "Root Cause Analysis", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md586", null ],
        [ "Suspected Issue", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md587", null ],
        [ "What Needs Investigation", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md588", null ],
        [ "Debugging Steps for Future Agent", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md589", null ],
        [ "Possible Fixes to Try", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md590", null ]
      ] ],
      [ "🔧 Files Modified", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md591", [
        [ "Core Canvas", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md592", null ],
        [ "Overworld Editor", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md593", null ],
        [ "Components Created", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md594", null ]
      ] ],
      [ "📚 Documentation (Consolidated)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md595", null ],
      [ "🎯 Future Refactoring Steps", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md596", [
        [ "Priority 1: Fix Rectangle Wrapping (High)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md597", null ],
        [ "Priority 2: Extract Coordinate Conversion Helpers (Low Impact)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md598", null ],
        [ "Priority 3: Move Components to canvas/ Namespace (Organizational)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md599", null ],
        [ "Priority 4: Complete Scratch Space Feature (Feature)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md600", null ],
        [ "Priority 5: Simplify Canvas State (Refactoring)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md601", null ]
      ] ],
      [ "🔍 Known Working Patterns", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md602", [
        [ "Overworld Tile Painting (Working)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md603", null ],
        [ "Blockset Selection (Working)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md604", null ],
        [ "Manual Overlay Highlighting (Working)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md605", null ]
      ] ],
      [ "🎓 Lessons Learned", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md606", [
        [ "What Worked", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md607", null ],
        [ "What Didn't Work", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md608", null ],
        [ "Key Insights", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md609", null ]
      ] ],
      [ "📋 For Future Agent", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md610", [
        [ "Immediate Task: Fix Rectangle Wrapping", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md611", null ],
        [ "Medium Term: Namespace Organization", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md612", null ],
        [ "Long Term: State Management Simplification", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md613", null ],
        [ "Stretch Goals: Enhanced Features", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md614", null ]
      ] ],
      [ "📊 Current Metrics", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md615", null ],
      [ "🔑 Key Files Reference", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md616", [
        [ "Core Canvas", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md617", null ],
        [ "Canvas Components", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md618", null ],
        [ "Utilities", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md619", null ],
        [ "Major Consumers", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md620", null ]
      ] ],
      [ "🎯 Recommended Next Steps", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md621", [
        [ "Step 1: Fix Rectangle Wrapping Bug (Critical)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md622", null ],
        [ "Step 2: Test All Editors (Verification)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md623", null ],
        [ "Step 3: Adopt Modern Patterns (Optional)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md624", null ]
      ] ],
      [ "📖 Documentation", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md625", [
        [ "Read This", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md626", null ],
        [ "Background (Optional)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md627", null ]
      ] ],
      [ "💡 Quick Reference", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md628", [
        [ "Modern Usage", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md629", null ],
        [ "Legacy Usage (Still Works)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md630", null ],
        [ "Revert Clamping", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md631", null ],
        [ "Add Context Menu", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md632", null ]
      ] ],
      [ "✅ Current Status", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md633", null ]
    ] ],
    [ "Roadmap", "d6/db4/md_docs_2D1-roadmap.html", [
      [ "0.4.X (Next Major Release)", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md636", [
        [ "Core Features", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md637", null ],
        [ "Technical Improvements", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md638", null ]
      ] ],
      [ "0.5.X", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md639", [
        [ "Advanced Features", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md640", null ]
      ] ],
      [ "0.6.X", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md641", [
        [ "Platform & Integration", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md642", null ]
      ] ],
      [ "0.7.X", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md643", [
        [ "Performance & Polish", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md644", null ]
      ] ],
      [ "0.8.X", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md645", [
        [ "Beta Preparation", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md646", null ]
      ] ],
      [ "1.0.0", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md647", [
        [ "Stable Release", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md648", null ]
      ] ],
      [ "Current Focus Areas", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md649", [
        [ "Immediate Priorities (v0.4.X)", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md650", null ],
        [ "Long-term Vision", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md651", null ]
      ] ]
    ] ],
    [ "Asm Style Guide", "d7/d9a/md_docs_2E1-asm-style-guide.html", [
      [ "Table of Contents", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md653", null ],
      [ "File Structure", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md654", null ],
      [ "Labels and Symbols", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md655", null ],
      [ "Comments", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md656", null ],
      [ "Instructions", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md657", null ],
      [ "Macros", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md658", null ],
      [ "Loops and Branching", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md659", null ],
      [ "Data Structures", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md660", null ],
      [ "Code Organization", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md661", null ],
      [ "Custom Code", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md662", null ]
    ] ],
    [ "Dungeon Editor Guide", "dd/d33/md_docs_2E2-dungeon-editor-guide.html", [
      [ "Overview", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md664", null ],
      [ "Architecture", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md665", [
        [ "Core Components", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md666", [
          [ "1. DungeonEditorSystem", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md667", null ],
          [ "2. DungeonObjectEditor", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md668", null ],
          [ "3. ObjectRenderer", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md669", null ],
          [ "4. DungeonEditor (UI Layer)", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md670", null ]
        ] ]
      ] ],
      [ "Coordinate System", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md671", [
        [ "Room Coordinates vs Canvas Coordinates", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md672", [
          [ "Conversion Functions", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md673", null ]
        ] ],
        [ "Coordinate System Features", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md674", null ]
      ] ],
      [ "Object Rendering System", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md675", [
        [ "Object Types", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md676", null ],
        [ "Rendering Pipeline", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md677", null ],
        [ "Performance Optimizations", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md678", null ]
      ] ],
      [ "User Interface", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md679", [
        [ "Integrated Editing Panels", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md680", [
          [ "Main Canvas", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md681", null ],
          [ "Compact Editing Panels", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md682", null ]
        ] ],
        [ "Object Preview System", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md683", null ]
      ] ],
      [ "Integration with ZScream", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md684", [
        [ "Room Loading", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md685", null ],
        [ "Object Parsing", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md686", null ],
        [ "Coordinate System", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md687", null ]
      ] ],
      [ "Testing and Validation", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md688", [
        [ "Integration Tests", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md689", null ],
        [ "Test Data", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md690", null ],
        [ "Performance Benchmarks", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md691", null ]
      ] ],
      [ "Usage Examples", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md692", [
        [ "Basic Object Editing", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md693", null ],
        [ "Coordinate Conversion", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md694", null ],
        [ "Object Preview", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md695", null ]
      ] ],
      [ "Configuration Options", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md696", [
        [ "Editor Configuration", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md697", null ],
        [ "Performance Configuration", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md698", null ]
      ] ],
      [ "Troubleshooting", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md699", [
        [ "Common Issues", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md700", null ],
        [ "Debug Information", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md701", null ]
      ] ],
      [ "Future Enhancements", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md702", [
        [ "Planned Features", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md703", null ]
      ] ],
      [ "Conclusion", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md704", null ]
    ] ],
    [ "Dungeon Editor Design Plan", "dc/d82/md_docs_2E3-dungeon-editor-design.html", [
      [ "Overview", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md706", null ],
      [ "Architecture Overview", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md707", [
        [ "Core Components", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md708", null ],
        [ "File Structure", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md709", null ]
      ] ],
      [ "Component Responsibilities", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md710", [
        [ "DungeonEditor (Main Orchestrator)", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md711", null ],
        [ "DungeonRoomSelector", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md712", null ],
        [ "DungeonCanvasViewer", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md713", null ],
        [ "DungeonObjectSelector", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md714", null ]
      ] ],
      [ "Data Flow", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md715", [
        [ "Initialization Flow", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md716", null ],
        [ "Runtime Data Flow", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md717", null ],
        [ "Key Data Structures", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md718", null ]
      ] ],
      [ "Integration Patterns", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md719", [
        [ "Component Communication", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md720", null ],
        [ "ROM Data Management", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md721", null ],
        [ "State Synchronization", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md722", null ]
      ] ],
      [ "UI Layout Architecture", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md723", [
        [ "3-Column Layout", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md724", null ],
        [ "Component Internal Layout", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md725", null ]
      ] ],
      [ "Coordinate System", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md726", [
        [ "Room Coordinates vs Canvas Coordinates", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md727", null ],
        [ "Bounds Checking", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md728", null ]
      ] ],
      [ "Error Handling & Validation", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md729", [
        [ "ROM Validation", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md730", null ],
        [ "Bounds Validation", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md731", null ]
      ] ],
      [ "Performance Considerations", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md732", [
        [ "Caching Strategy", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md733", null ],
        [ "Rendering Optimization", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md734", null ]
      ] ],
      [ "Testing Strategy", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md735", [
        [ "Integration Tests", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md736", null ],
        [ "Test Categories", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md737", null ]
      ] ],
      [ "Future Development Guidelines", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md738", [
        [ "Adding New Features", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md739", null ],
        [ "Component Extension Patterns", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md740", null ],
        [ "Data Flow Extension", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md741", null ],
        [ "UI Layout Extension", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md742", null ]
      ] ],
      [ "Common Pitfalls & Solutions", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md743", [
        [ "Memory Management", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md744", null ],
        [ "Coordinate System", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md745", null ],
        [ "State Synchronization", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md746", null ],
        [ "Performance Issues", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md747", null ]
      ] ],
      [ "Debugging Tools", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md748", [
        [ "Debug Popup", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md749", null ],
        [ "Logging", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md750", null ]
      ] ],
      [ "Build Integration", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md751", [
        [ "CMake Configuration", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md752", null ],
        [ "Dependencies", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md753", null ]
      ] ],
      [ "Conclusion", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md754", null ]
    ] ],
    [ "DungeonEditor Refactoring Plan", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html", [
      [ "Overview", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md756", null ],
      [ "Component Structure", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md757", [
        [ "✅ Created Components", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md758", [
          [ "1. DungeonToolset (<tt>dungeon_toolset.h/cc</tt>)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md759", null ],
          [ "2. DungeonObjectInteraction (<tt>dungeon_object_interaction.h/cc</tt>)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md760", null ],
          [ "3. DungeonRenderer (<tt>dungeon_renderer.h/cc</tt>)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md761", null ],
          [ "4. DungeonRoomLoader (<tt>dungeon_room_loader.h/cc</tt>)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md762", null ],
          [ "5. DungeonUsageTracker (<tt>dungeon_usage_tracker.h/cc</tt>)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md763", null ]
        ] ]
      ] ],
      [ "Refactored DungeonEditor Structure", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md764", [
        [ "Before Refactoring: 1444 lines", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md765", null ],
        [ "After Refactoring: ~400 lines", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md766", null ]
      ] ],
      [ "Method Migration Map", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md767", [
        [ "Core Editor Methods (Keep in main file)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md768", null ],
        [ "UI Methods (Keep for coordination)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md769", null ],
        [ "Methods Moved to Components", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md770", [
          [ "→ DungeonToolset", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md771", null ],
          [ "→ DungeonObjectInteraction", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md772", null ],
          [ "→ DungeonRenderer", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md773", null ],
          [ "→ DungeonRoomLoader", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md774", null ],
          [ "→ DungeonUsageTracker", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md775", null ]
        ] ]
      ] ],
      [ "Component Communication", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md776", [
        [ "Callback System", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md777", null ],
        [ "Data Sharing", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md778", null ]
      ] ],
      [ "Benefits of Refactoring", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md779", [
        [ "1. <strong>Reduced Complexity</strong>", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md780", null ],
        [ "2. <strong>Improved Testability</strong>", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md781", null ],
        [ "3. <strong>Better Maintainability</strong>", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md782", null ],
        [ "4. <strong>Enhanced Extensibility</strong>", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md783", null ],
        [ "5. <strong>Cleaner Dependencies</strong>", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md784", null ]
      ] ],
      [ "Implementation Status", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md785", [
        [ "✅ Completed", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md786", null ],
        [ "🔄 In Progress", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md787", null ],
        [ "⏳ Pending", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md788", null ]
      ] ],
      [ "Migration Strategy", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md789", [
        [ "Phase 1: Create Components ✅", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md790", null ],
        [ "Phase 2: Integrate Components 🔄", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md791", null ],
        [ "Phase 3: Move Methods", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md792", null ],
        [ "Phase 4: Cleanup", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md793", null ]
      ] ]
    ] ],
    [ "Dungeon Object System", "da/d11/md_docs_2E5-dungeon-object-system.html", [
      [ "Overview", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md795", null ],
      [ "Architecture", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md796", [
        [ "Core Components", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md797", [
          [ "1. DungeonEditor (<tt>src/app/editor/dungeon/dungeon_editor.h</tt>)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md798", null ],
          [ "2. DungeonObjectSelector (<tt>src/app/editor/dungeon/dungeon_object_selector.h</tt>)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md799", null ],
          [ "3. DungeonCanvasViewer (<tt>src/app/editor/dungeon/dungeon_canvas_viewer.h</tt>)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md800", null ],
          [ "4. Room Management System (<tt>src/app/zelda3/dungeon/room.h</tt>)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md801", null ]
        ] ]
      ] ],
      [ "Object Types and Hierarchies", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md802", [
        [ "Room Objects", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md803", [
          [ "Type 1 Objects (0x00-0xFF)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md804", null ],
          [ "Type 2 Objects (0x100-0x1FF)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md805", null ],
          [ "Type 3 Objects (0x200+)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md806", null ]
        ] ],
        [ "Object Properties", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md807", null ]
      ] ],
      [ "How Object Placement Works", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md808", [
        [ "Selection Process", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md809", null ],
        [ "Placement Process", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md810", null ],
        [ "Code Flow", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md811", null ]
      ] ],
      [ "Rendering Pipeline", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md812", [
        [ "Object Rendering", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md813", null ],
        [ "Performance Optimizations", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md814", null ]
      ] ],
      [ "User Interface Components", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md815", [
        [ "Three-Column Layout", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md816", [
          [ "Column 1: Room Control Panel (280px fixed)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md817", null ],
          [ "Column 2: Windowed Canvas (800px fixed)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md818", null ],
          [ "Column 3: Object Selector/Editor (stretch)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md819", null ]
        ] ],
        [ "Debug and Control Features", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md820", [
          [ "Room Properties Table", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md821", null ],
          [ "Object Statistics", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md822", null ]
        ] ]
      ] ],
      [ "Integration with ROM Data", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md823", [
        [ "Data Sources", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md824", [
          [ "Room Headers (<tt>0x1F8000</tt>)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md825", null ],
          [ "Object Data", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md826", null ],
          [ "Graphics Data", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md827", null ]
        ] ],
        [ "Assembly Integration", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md828", null ]
      ] ],
      [ "Comparison with ZScream", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md829", [
        [ "Architectural Differences", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md830", [
          [ "Component-Based Architecture", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md831", null ],
          [ "Real-time Rendering", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md832", null ],
          [ "UI Organization", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md833", null ],
          [ "Caching Strategy", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md834", null ]
        ] ],
        [ "Shared Concepts", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md835", null ]
      ] ],
      [ "Best Practices", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md836", [
        [ "Performance", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md837", null ],
        [ "Code Organization", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md838", null ],
        [ "User Experience", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md839", null ]
      ] ],
      [ "Future Enhancements", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md840", [
        [ "Planned Features", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md841", null ],
        [ "Technical Improvements", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md842", null ]
      ] ]
    ] ],
    [ "Tile16 Editor Palette System", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html", [
      [ "Executive Summary", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md844", null ],
      [ "Problem Analysis", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md845", [
        [ "Critical Issues Identified", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md846", null ]
      ] ],
      [ "Solution Architecture", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md847", [
        [ "Core Design Principles", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md848", null ],
        [ "256-Color Overworld Palette Structure", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md849", null ],
        [ "Sheet-to-Palette Mapping", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md850", null ],
        [ "Palette Button Mapping", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md851", null ]
      ] ],
      [ "Implementation Details", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md852", [
        [ "1. Fixed SetPaletteWithTransparent Method", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md853", null ],
        [ "2. Corrected Tile16 Editor Palette System", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md854", null ],
        [ "3. Palette Coordination Flow", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md855", null ]
      ] ],
      [ "UI/UX Refactoring", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md856", [
        [ "New Three-Column Layout", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md857", null ],
        [ "Canvas Context Menu Fixes", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md858", null ],
        [ "Dynamic Zoom Controls", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md859", null ]
      ] ],
      [ "Testing Protocol", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md860", [
        [ "Crash Prevention Testing", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md861", null ],
        [ "Color Alignment Testing", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md862", null ],
        [ "UI/UX Testing", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md863", null ]
      ] ],
      [ "Error Handling", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md864", [
        [ "Bounds Checking", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md865", null ],
        [ "Fallback Mechanisms", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md866", null ]
      ] ],
      [ "Debug Information Display", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md867", null ],
      [ "Known Issues and Ongoing Work", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md868", [
        [ "Completed Items ✅", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md869", null ],
        [ "Active Issues ⚠️", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md870", null ],
        [ "Current Status Summary", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md871", null ],
        [ "Future Enhancements", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md872", null ]
      ] ],
      [ "Maintenance Notes", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md873", null ],
      [ "Next Steps", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md875", [
        [ "Immediate Priorities", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md876", null ],
        [ "Investigation Areas", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md877", null ]
      ] ]
    ] ],
    [ "Overworld Loading Guide", "dc/d12/md_docs_2F1-overworld-loading.html", [
      [ "Table of Contents", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md880", null ],
      [ "Overview", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md881", null ],
      [ "ROM Types and Versions", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md882", [
        [ "Version Detection", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md883", null ],
        [ "Feature Support by Version", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md884", null ]
      ] ],
      [ "Overworld Map Structure", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md885", [
        [ "Core Properties", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md886", null ]
      ] ],
      [ "Overlays and Special Area Maps", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md887", [
        [ "Understanding Overlays", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md888", null ],
        [ "Special Area Maps (0x80-0x9F)", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md889", null ],
        [ "Overlay ID Mappings", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md890", null ],
        [ "Drawing Order", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md891", null ],
        [ "Vanilla Overlay Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md892", null ],
        [ "Special Area Graphics Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md893", null ]
      ] ],
      [ "Loading Process", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md894", [
        [ "1. Version Detection", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md895", null ],
        [ "2. Map Initialization", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md896", null ],
        [ "3. Property Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md897", [
          [ "Vanilla ROMs (asm_version == 0xFF)", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md898", null ],
          [ "ZSCustomOverworld v2/v3", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md899", null ]
        ] ],
        [ "4. Custom Data Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md900", null ]
      ] ],
      [ "ZScream Implementation", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md901", [
        [ "OverworldMap Constructor", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md902", null ],
        [ "Key Methods", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md903", null ]
      ] ],
      [ "Yaze Implementation", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md904", [
        [ "OverworldMap Constructor", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md905", null ],
        [ "Key Methods", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md906", null ],
        [ "Current Status", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md907", null ]
      ] ],
      [ "Key Differences", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md908", [
        [ "1. Language and Architecture", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md909", null ],
        [ "2. Data Structures", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md910", null ],
        [ "3. Error Handling", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md911", null ],
        [ "4. Graphics Processing", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md912", null ]
      ] ],
      [ "Common Issues and Solutions", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md913", [
        [ "1. Version Detection Issues", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md914", null ],
        [ "2. Palette Loading Errors", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md915", null ],
        [ "3. Graphics Not Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md916", null ],
        [ "4. Overlay Issues", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md917", null ],
        [ "5. Large Map Problems", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md918", null ],
        [ "6. Special Area Graphics Issues", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md919", null ]
      ] ],
      [ "Best Practices", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md920", [
        [ "1. Version-Specific Code", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md921", null ],
        [ "2. Error Handling", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md922", null ],
        [ "3. Memory Management", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md923", null ],
        [ "4. Thread Safety", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md924", null ]
      ] ],
      [ "Conclusion", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md925", null ]
    ] ],
    [ "YAZE Graphics System Improvements Summary", "d9/df4/md_docs_2gfx__improvements__summary.html", [
      [ "Overview", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md927", null ],
      [ "Files Modified", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md928", [
        [ "Core Graphics Classes", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md929", [
          [ "1. <tt>/src/app/gfx/bitmap.h</tt>", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md930", null ],
          [ "2. <tt>/src/app/gfx/bitmap.cc</tt>", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md931", null ],
          [ "3. <tt>/src/app/gfx/arena.h</tt>", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md932", null ],
          [ "4. <tt>/src/app/gfx/arena.cc</tt>", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md933", null ],
          [ "5. <tt>/src/app/gfx/tilemap.h</tt>", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md934", null ]
        ] ],
        [ "Editor Classes", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md935", [
          [ "6. <tt>/src/app/editor/graphics/graphics_editor.cc</tt>", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md936", null ],
          [ "7. <tt>/src/app/editor/graphics/palette_editor.cc</tt>", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md937", null ],
          [ "8. <tt>/src/app/editor/graphics/screen_editor.cc</tt>", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md938", null ]
        ] ]
      ] ],
      [ "New Documentation Files", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md939", [
        [ "9. <tt>/docs/gfx_optimization_recommendations.md</tt>", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md940", null ]
      ] ],
      [ "Performance Optimization Recommendations", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md941", [
        [ "High Impact, Low Risk (Phase 1)", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md942", null ],
        [ "Medium Impact, Medium Risk (Phase 2)", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md943", null ],
        [ "High Impact, High Risk (Phase 3)", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md944", null ]
      ] ],
      [ "ROM Hacking Workflow Improvements", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md945", [
        [ "Graphics Editor Enhancements", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md946", null ],
        [ "Palette Editor Enhancements", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md947", null ],
        [ "Screen Editor Enhancements", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md948", null ]
      ] ],
      [ "Code Quality Improvements", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md949", [
        [ "Documentation Standards", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md950", null ],
        [ "Code Organization", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md951", null ]
      ] ],
      [ "Future Development Recommendations", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md952", [
        [ "Immediate Improvements", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md953", null ],
        [ "Medium-term Enhancements", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md954", null ],
        [ "Long-term Goals", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md955", null ]
      ] ],
      [ "Conclusion", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md956", null ]
    ] ],
    [ "YAZE Graphics System Optimization Recommendations", "de/def/md_docs_2gfx__optimization__recommendations.html", [
      [ "Overview", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md958", null ],
      [ "Current Architecture Analysis", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md959", [
        [ "Strengths", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md960", null ],
        [ "Performance Bottlenecks Identified", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md961", [
          [ "1. Bitmap Class Issues", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md962", null ],
          [ "2. Arena Resource Management", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md963", null ],
          [ "3. Tilemap Performance", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md964", null ]
        ] ]
      ] ],
      [ "Optimization Recommendations", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md965", [
        [ "1. Bitmap Class Optimizations", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md966", [
          [ "A. Palette Lookup Optimization", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md967", null ],
          [ "B. Dirty Region Tracking", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md968", null ]
        ] ],
        [ "2. Arena Resource Management Improvements", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md969", [
          [ "A. Resource Pooling", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md970", null ],
          [ "B. Batch Operations", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md971", null ]
        ] ],
        [ "3. Tilemap Performance Enhancements", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md972", [
          [ "A. Smart Tile Caching", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md973", null ],
          [ "B. Atlas-based Rendering", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md974", null ]
        ] ],
        [ "4. Editor-Specific Optimizations", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md975", [
          [ "A. Graphics Editor Improvements", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md976", null ],
          [ "B. Palette Editor Optimizations", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md977", null ]
        ] ],
        [ "5. Memory Management Improvements", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md978", [
          [ "A. Custom Allocator for Graphics Data", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md979", null ],
          [ "B. Smart Pointer Management", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md980", null ]
        ] ]
      ] ],
      [ "Implementation Priority", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md981", [
        [ "Phase 1 (High Impact, Low Risk)", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md982", null ],
        [ "Phase 2 (Medium Impact, Medium Risk)", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md983", null ],
        [ "Phase 3 (High Impact, High Risk)", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md984", null ]
      ] ],
      [ "Performance Metrics", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md985", [
        [ "Target Improvements", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md986", null ],
        [ "Measurement Tools", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md987", null ]
      ] ],
      [ "Conclusion", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md988", null ]
    ] ],
    [ "YAZE Graphics System Optimizations - Complete Implementation", "d6/df4/md_docs_2gfx__optimizations__complete.html", [
      [ "Overview", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md990", null ],
      [ "Implemented Optimizations", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md991", [
        [ "1. Palette Lookup Optimization ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md992", null ],
        [ "2. Dirty Region Tracking ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md993", null ],
        [ "3. Resource Pooling ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md994", null ],
        [ "4. LRU Tile Caching ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md995", null ],
        [ "5. Batch Operations ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md996", null ],
        [ "6. Memory Pool Allocator ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md997", null ],
        [ "7. Atlas-Based Rendering ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md998", null ],
        [ "8. Performance Profiling System ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md999", null ],
        [ "9. Performance Monitoring Dashboard ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1000", null ],
        [ "10. Optimization Validation Suite ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1001", null ]
      ] ],
      [ "Performance Metrics", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1002", [
        [ "Expected Improvements", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1003", null ],
        [ "Measurement Tools", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1004", null ]
      ] ],
      [ "Integration Points", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1005", [
        [ "Graphics Editor", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1006", null ],
        [ "Palette Editor", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1007", null ],
        [ "Screen Editor", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1008", null ]
      ] ],
      [ "Backward Compatibility", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1009", null ],
      [ "Usage Examples", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1010", [
        [ "Using Batch Operations", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1011", null ],
        [ "Using Memory Pool", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1012", null ],
        [ "Using Atlas Rendering", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1013", null ],
        [ "Using Performance Monitoring", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1014", null ]
      ] ],
      [ "Future Enhancements", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1015", [
        [ "Phase 2 Optimizations (Medium Priority)", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1016", null ],
        [ "Phase 3 Optimizations (High Priority)", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1017", null ]
      ] ],
      [ "Testing and Validation", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1018", [
        [ "Performance Testing", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1019", null ],
        [ "ROM Hacking Workflow Testing", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1020", null ]
      ] ],
      [ "Conclusion", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1021", null ],
      [ "Files Modified/Created", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1022", [
        [ "Core Graphics Classes", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1023", null ],
        [ "New Optimization Components", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1024", null ],
        [ "Testing and Validation", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1025", null ],
        [ "Build System", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1026", null ],
        [ "Documentation", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1027", null ]
      ] ]
    ] ],
    [ "yaze Graphics System Optimizations - Implementation Summary", "d3/d7b/md_docs_2gfx__optimizations__implemented.html", [
      [ "Overview", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1029", null ],
      [ "Implemented Optimizations", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1030", [
        [ "1. Palette Lookup Optimization ✅ COMPLETED", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1031", null ],
        [ "2. Dirty Region Tracking ✅ COMPLETED", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1032", null ],
        [ "3. Resource Pooling ✅ COMPLETED", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1033", null ],
        [ "4. LRU Tile Caching ✅ COMPLETED", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1034", null ],
        [ "5. Region-Specific Texture Updates ✅ COMPLETED", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1035", null ],
        [ "6. Performance Profiling System ✅ COMPLETED", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1036", null ]
      ] ],
      [ "Performance Metrics", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1037", [
        [ "Expected Improvements", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1038", null ],
        [ "Measurement Tools", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1039", null ]
      ] ],
      [ "Integration Points", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1040", [
        [ "Graphics Editor", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1041", null ],
        [ "Palette Editor", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1042", null ],
        [ "Screen Editor", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1043", null ]
      ] ],
      [ "Backward Compatibility", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1044", null ],
      [ "Future Enhancements", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1045", [
        [ "Phase 2 Optimizations (Medium Priority)", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1046", null ],
        [ "Phase 3 Optimizations (High Priority)", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1047", null ]
      ] ],
      [ "Testing and Validation", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1048", [
        [ "Performance Testing", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1049", null ],
        [ "ROM Hacking Workflow Testing", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1050", null ]
      ] ],
      [ "Conclusion", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1051", null ]
    ] ],
    [ "YAZE Graphics Optimizations Project - Final Summary", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html", [
      [ "Project Overview", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1053", null ],
      [ "Completed Optimizations", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1054", [
        [ "✅ 1. Batch Operations for Texture Updates", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1055", null ],
        [ "✅ 2. Memory Pool Allocator", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1056", null ],
        [ "✅ 3. Atlas-Based Rendering System", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1057", null ],
        [ "✅ 4. Performance Monitoring Dashboard", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1058", null ],
        [ "✅ 5. Optimization Validation Suite", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1059", null ],
        [ "✅ 6. Debug Menu Integration", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1060", null ]
      ] ],
      [ "Performance Metrics Achieved", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1061", [
        [ "Expected Improvements (Based on Implementation)", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1062", null ],
        [ "Real Performance Data (From Timing Report)", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1063", null ]
      ] ],
      [ "Technical Implementation Details", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1064", [
        [ "Architecture Improvements", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1065", null ],
        [ "Code Quality Enhancements", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1066", null ]
      ] ],
      [ "Integration Points", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1067", [
        [ "Graphics System Integration", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1068", null ],
        [ "Editor Integration", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1069", null ]
      ] ],
      [ "Future Enhancements", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1070", [
        [ "Remaining Optimization (Pending)", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1071", null ],
        [ "Potential Extensions", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1072", null ]
      ] ],
      [ "Build and Testing", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1073", [
        [ "Build Status", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1074", null ],
        [ "Testing Status", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1075", null ]
      ] ],
      [ "Conclusion", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1076", null ]
    ] ],
    [ "yaze Documentation", "d3/d4c/md_docs_2index.html", [
      [ "Quick Start", "d3/d4c/md_docs_2index.html#autotoc_md1078", null ],
      [ "Development", "d3/d4c/md_docs_2index.html#autotoc_md1079", null ],
      [ "Technical Documentation", "d3/d4c/md_docs_2index.html#autotoc_md1080", [
        [ "Assembly & Code", "d3/d4c/md_docs_2index.html#autotoc_md1081", null ],
        [ "Editor Systems", "d3/d4c/md_docs_2index.html#autotoc_md1082", null ],
        [ "Overworld System", "d3/d4c/md_docs_2index.html#autotoc_md1083", null ]
      ] ],
      [ "Key Features", "d3/d4c/md_docs_2index.html#autotoc_md1084", null ]
    ] ],
    [ "yaze Overworld Testing Guide", "de/d8a/md_docs_2overworld__testing__guide.html", [
      [ "Overview", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1087", null ],
      [ "Test Architecture", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1088", [
        [ "1. Golden Data System", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1089", null ],
        [ "2. Test Categories", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1090", [
          [ "Unit Tests (<tt>test/unit/zelda3/</tt>)", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1091", null ],
          [ "Integration Tests (<tt>test/e2e/</tt>)", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1092", null ],
          [ "Golden Data Tools", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1093", null ]
        ] ]
      ] ],
      [ "Quick Start", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1094", [
        [ "Prerequisites", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1095", null ],
        [ "Running Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1096", [
          [ "Basic Test Run", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1097", null ],
          [ "Selective Test Execution", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1098", null ]
        ] ]
      ] ],
      [ "Test Components", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1099", [
        [ "1. Golden Data Extractor", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1100", null ],
        [ "2. Integration Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1101", null ],
        [ "3. End-to-End Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1102", null ]
      ] ],
      [ "Test Validation Points", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1103", [
        [ "1. ZScream Compatibility", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1104", null ],
        [ "2. ROM State Validation", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1105", null ],
        [ "3. Performance and Stability", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1106", null ]
      ] ],
      [ "Environment Variables", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1107", [
        [ "Test Configuration", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1108", null ],
        [ "Build Configuration", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1109", null ]
      ] ],
      [ "Test Reports", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1110", [
        [ "Generated Reports", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1111", null ],
        [ "Report Location", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1112", null ]
      ] ],
      [ "Troubleshooting", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1113", [
        [ "Common Issues", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1114", [
          [ "1. ROM Not Found", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1115", null ],
          [ "2. Build Failures", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1116", null ],
          [ "3. Test Failures", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1117", null ]
        ] ],
        [ "Debug Mode", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1118", null ]
      ] ],
      [ "Advanced Usage", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1119", [
        [ "Custom Test Scenarios", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1120", [
          [ "1. Testing Custom ROMs", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1121", null ],
          [ "2. Regression Testing", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1122", null ],
          [ "3. Performance Testing", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1123", null ]
        ] ]
      ] ],
      [ "Integration with CI/CD", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1124", [
        [ "GitHub Actions Example", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1125", null ]
      ] ],
      [ "Contributing", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1126", [
        [ "Adding New Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1127", null ],
        [ "Test Guidelines", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1128", null ],
        [ "Example Test Structure", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1129", null ]
      ] ],
      [ "Conclusion", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1130", null ]
    ] ],
    [ "z3ed Agent Roadmap", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html", [
      [ "Core Vision: The Conversational ROM Hacking Assistant", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1132", [
        [ "Key Features", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1133", null ]
      ] ],
      [ "Technical Implementation Plan", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1134", [
        [ "1. Conversational Agent Service", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1135", null ],
        [ "2. Read-Only \"Tools\" for the Agent", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1136", null ],
        [ "3. TUI and GUI Chat Interfaces", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1137", null ],
        [ "4. Integration with the Proposal Workflow", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1138", null ]
      ] ],
      [ "Consolidated Next Steps", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1139", [
        [ "Immediate Priorities (Next Session)", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1140", null ],
        [ "Short-Term Goals (This Week)", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1141", null ],
        [ "Long-Term Vision (Next Week and Beyond)", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1142", null ]
      ] ]
    ] ],
    [ "z3ed CLI Architecture & Design", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html", [
      [ "1. Overview", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1144", null ],
      [ "2. Design Goals", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1145", [
        [ "2.1. Key Architectural Decisions", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1146", null ]
      ] ],
      [ "3. Proposed CLI Architecture: Resource-Oriented Commands", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1147", [
        [ "3.1. Top-Level Resources", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1148", null ],
        [ "3.2. Example Command Mapping", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1149", null ]
      ] ],
      [ "4. New Features & Commands", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1150", [
        [ "4.1. For the ROM Hacker (Power & Scriptability)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1151", null ],
        [ "4.2. For Testing & Automation", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1152", null ]
      ] ],
      [ "5. TUI Enhancements", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1153", null ],
      [ "6. Generative & Agentic Workflows (MCP Integration)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1154", [
        [ "6.1. The Generative Workflow", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1155", null ],
        [ "6.2. Key Enablers", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1156", null ]
      ] ],
      [ "7. Implementation Roadmap", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1157", [
        [ "Phase 1: Core CLI & TUI Foundation (Done)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1158", null ],
        [ "Phase 2: Interactive TUI & Command Palette (Done)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1159", null ],
        [ "Phase 3: Testing & Project Management (Done)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1160", null ],
        [ "Phase 4: Agentic Framework & Generative AI (In Progress)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1161", null ],
        [ "Phase 5: Code Structure & UX Improvements (Completed)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1162", null ],
        [ "Phase 6: Resource Catalogue & API Documentation (✅ Completed - Oct 1, 2025)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1163", null ]
      ] ],
      [ "8. Agentic Framework Architecture - Advanced Dive", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1164", [
        [ "8.1. The <tt>z3ed agent</tt> Command", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1165", null ],
        [ "8.2. The Agentic Loop (MCP) - Detailed Workflow", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1166", null ],
        [ "8.3. AI Model & Protocol Strategy", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1167", null ]
      ] ],
      [ "9. Test Harness Evolution: From Automation to Platform", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1169", [
        [ "9.1. Current Capabilities (IT-01 to IT-04) ✅", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1170", null ],
        [ "9.2. Limitations Identified", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1171", null ],
        [ "9.3. Enhancement Roadmap (IT-05 to IT-09)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1172", [
          [ "IT-05: Test Introspection API (6-8 hours)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1173", null ],
          [ "IT-06: Widget Discovery API (4-6 hours)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1174", null ],
          [ "IT-07: Test Recording & Replay ✅ COMPLETE", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1175", null ],
          [ "IT-08: Holistic Error Reporting (5-7 hours)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1176", null ],
          [ "IT-09: CI/CD Integration ✅ CLI Foundations Complete", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1177", null ]
        ] ],
        [ "9.4. Unified Testing Vision", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1178", null ],
        [ "9.5. Implementation Priority", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1179", null ],
        [ "8.4. GUI Integration & User Experience", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1181", null ],
        [ "8.5. Testing & Verification", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1182", null ],
        [ "8.6. Safety & Sandboxing", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1183", null ],
        [ "8.7. Optional JSON Dependency", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1184", null ],
        [ "8.8. Contextual Awareness & Feedback Loop", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1185", null ],
        [ "8.9. Error Handling and Recovery", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1186", null ],
        [ "8.10. Extensibility", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1187", null ]
      ] ],
      [ "9. UX Improvements and Architectural Decisions", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1188", [
        [ "9.1. TUI Component Architecture", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1189", null ],
        [ "9.2. Command Handler Unification", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1190", null ],
        [ "9.3. Interface Consolidation", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1191", null ],
        [ "9.4. Code Organization Improvements", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1192", null ],
        [ "9.5. Future UX Enhancements", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1193", null ]
      ] ],
      [ "10. Implementation Status and Code Quality", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1194", [
        [ "10.1. Recent Refactoring Improvements (January 2025)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1195", null ],
        [ "10.2. File Organization", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1196", null ],
        [ "10.3. Code Quality Improvements", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1197", null ],
        [ "10.4. TUI Component System", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1198", null ],
        [ "10.5. Known Limitations", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1199", null ],
        [ "10.6. Future Code Quality Goals", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1200", null ]
      ] ],
      [ "11. Agent-Ready API Surface Area", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1201", null ],
      [ "12. Acceptance & Review Workflow", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1202", [
        [ "12.1. Change Proposal Lifecycle", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1203", null ],
        [ "12.2. UI Extensions", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1204", null ],
        [ "12.3. Policy Configuration", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1205", null ]
      ] ],
      [ "13. ImGuiTestEngine Control Bridge", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1206", [
        [ "13.1. Bridge Architecture", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1207", [
          [ "13.1.1. Transport & Envelope", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1208", null ],
          [ "13.1.2. Harness Runtime Lifecycle", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1209", null ],
          [ "13.1.3. Integration with <tt>z3ed agent</tt>", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1210", null ]
        ] ],
        [ "13.2. Safety & Sandboxing", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1211", null ],
        [ "13.3. Script Generation Strategy", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1212", null ]
      ] ],
      [ "14. Test & Verification Strategy", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1213", [
        [ "14.1. Layered Test Suites", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1214", null ],
        [ "14.2. Continuous Verification", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1215", null ],
        [ "14.3. Telemetry-Informed Testing", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1216", null ]
      ] ],
      [ "15. Expanded Roadmap (Phase 6+)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1217", [
        [ "Phase 6: Agent Workflow Foundations (Planned)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1218", null ],
        [ "Phase 7: Controlled Mutation & Review (Planned)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1219", null ],
        [ "Phase 8: Learning & Self-Improvement (Exploratory)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1220", null ],
        [ "7.4. Widget ID Management for Test Automation", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1221", null ]
      ] ]
    ] ],
    [ "z3ed Agentic Workflow Plan", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html", [
      [ "Executive Summary", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1223", null ],
      [ "Quick Reference", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1224", null ],
      [ "1. Current Priorities (Week of Oct 2-8, 2025)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1226", [
        [ "Priority 1: Test Harness Enhancements (IT-05 to IT-09) 🔧 ACTIVE", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1227", [
          [ "IT-05: Test Introspection API (6-8 hours)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1228", null ],
          [ "IT-06: Widget Discovery API (4-6 hours)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1229", null ],
          [ "IT-07: Test Recording & Replay ✅ COMPLETE (Oct 2, 2025)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1230", null ],
          [ "IT-08: Enhanced Error Reporting (5-7 hours) ✅ COMPLETE", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1231", null ],
          [ "IT-09: CI/CD Integration ✅ CLI Tooling Shipped", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1232", null ],
          [ "IT-10: Collaborative Editing & Multiplayer Sessions ⏸️ DEPRIORITIZED", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1234", null ]
        ] ],
        [ "Priority 2: LLM Integration (Ollama + Gemini + Claude) 🤖 NEW PRIORITY", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1236", [
          [ "Phase 1: Ollama Local Integration (4-6 hours) 🎯 START HERE", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1237", null ],
          [ "Phase 2: Gemini Fixes (2-3 hours)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1238", null ],
          [ "Phase 3: Claude Integration (2-3 hours)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1239", null ],
          [ "Phase 4: Enhanced Prompt Engineering (3-4 hours)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1240", null ]
        ] ],
        [ "Priority 3: Windows Cross-Platform Testing 🪟", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1242", null ],
        [ "Priority 2: Windows Cross-Platform Testing 🪟", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1244", null ]
      ] ],
      [ "</blockquote>", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1245", null ],
      [ "2. Workstreams Overview", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1246", [
        [ "Completed Work Summary", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1247", null ]
      ] ],
      [ "3. Task Backlog", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1249", null ],
      [ "3. Immediate Next Steps (Week of Oct 1-7, 2025)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1250", [
        [ "Priority 0: Testing & Validation (Active)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1251", null ],
        [ "Priority 1: ImGuiTestHarness Foundation (IT-01) ✅ COMPLETE", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1252", [
          [ "Phase 1: gRPC Infrastructure ✅ COMPLETE", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1253", null ],
          [ "Phase 2: ImGuiTestEngine Integration ✅ COMPLETE", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1254", null ],
          [ "Phase 3: Full ImGuiTestEngine Integration ✅ COMPLETE (Oct 2, 2025)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1255", null ],
          [ "Phase 4: CLI Integration & Windows Testing (4-5 hours)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1256", null ]
        ] ],
        [ "IT-01 Quick Reference", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1257", null ],
        [ "Priority 2: Policy Evaluation Framework (AW-04, 4-6 hours)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1258", null ],
        [ "Priority 3: Documentation & Consolidation (2-3 hours)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1259", null ],
        [ "Later: Advanced Features", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1260", null ]
      ] ],
      [ "4. Current Issues & Blockers", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1261", [
        [ "Active Issues", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1262", null ],
        [ "Known Limitations (Non-Blocking)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1263", null ]
      ] ],
      [ "5. Architecture Overview", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1264", [
        [ "5.1. Proposal Lifecycle Flow", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1265", null ],
        [ "5.2. Component Interaction Diagram", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1266", null ],
        [ "5.3. Data Flow: Agent Run to ROM Merge", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1267", null ]
      ] ],
      [ "5. Open Questions", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1268", null ],
      [ "4. Work History & Key Decisions", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1269", [
        [ "Resource Catalogue Workstream (RC) - ✅ COMPLETE", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1270", null ],
        [ "Acceptance Workflow (AW-01, AW-02, AW-03) - ✅ COMPLETE", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1271", null ],
        [ "ImGuiTestHarness (IT-01, IT-02) - ✅ CORE COMPLETE", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1272", null ],
        [ "Files Modified/Created", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1273", null ]
      ] ],
      [ "5. Open Questions", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1274", null ],
      [ "6. References", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1275", null ]
    ] ],
    [ "z3ed CLI Technical Reference", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html", [
      [ "Table of Contents", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1279", null ],
      [ "Architecture Overview", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1281", [
        [ "System Components", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1282", null ],
        [ "Data Flow: Proposal Lifecycle", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1283", null ]
      ] ],
      [ "Command Reference", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1285", [
        [ "Agent Commands", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1286", [
          [ "<tt>agent run</tt> - Execute AI-driven ROM modifications", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1287", null ],
          [ "<tt>agent list</tt> - Show all proposals", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1288", null ],
          [ "<tt>agent diff</tt> - Show proposal changes", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1289", null ],
          [ "<tt>agent describe</tt> - Export machine-readable API specs", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1290", null ],
          [ "<tt>agent test</tt> - Automated GUI testing (IT-02)", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1291", null ],
          [ "<tt>agent gui</tt> - GUI Introspection & Control (IT-05/IT-06)", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1292", [
            [ "<tt>agent gui discover</tt> - Enumerate available widgets", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1293", null ],
            [ "<tt>agent test status</tt> - Query test execution state", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1294", null ],
            [ "<tt>agent test results</tt> - Get detailed test results", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1295", null ],
            [ "<tt>agent test list</tt> - List all tests", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1296", null ]
          ] ],
          [ "<tt>agent test record</tt> - Record test sessions (IT-07)", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1297", [
            [ "<tt>agent test record start</tt> - Begin recording", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1298", null ],
            [ "<tt>agent test record stop</tt> - Finish recording", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1299", null ]
          ] ],
          [ "<tt>agent test replay</tt> - Execute recorded tests", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1300", null ],
          [ "<tt>agent test suite</tt> - Manage test suites (IT-09)", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1301", null ]
        ] ],
        [ "ROM Commands", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1302", [
          [ "<tt>rom info</tt> - Display ROM metadata", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1303", null ],
          [ "<tt>rom validate</tt> - Verify ROM integrity", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1304", null ],
          [ "<tt>rom diff</tt> - Compare two ROMs", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1305", null ],
          [ "<tt>rom generate-golden</tt> - Create reference checksums", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1306", null ]
        ] ],
        [ "Palette Commands", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1307", [
          [ "<tt>palette export</tt> - Export palette to file", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1308", null ],
          [ "<tt>palette import</tt> - Import palette from file", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1309", null ],
          [ "<tt>palette list</tt> - Show available palettes", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1310", null ]
        ] ],
        [ "Overworld Commands", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1311", [
          [ "<tt>overworld get-tile</tt> - Get tile at coordinates", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1312", null ],
          [ "<tt>overworld set-tile</tt> - Set tile at coordinates", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1313", null ]
        ] ],
        [ "Dungeon Commands", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1314", [
          [ "<tt>dungeon list-rooms</tt> - List all dungeon rooms", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1315", null ],
          [ "<tt>dungeon add-object</tt> - Add object to room", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1316", null ]
        ] ]
      ] ],
      [ "Implementation Guide", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1318", [
        [ "Building with gRPC Support", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1319", [
          [ "macOS (Recommended)", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1320", null ],
          [ "Windows (Experimental)", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1321", null ]
        ] ],
        [ "Starting Test Harness", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1322", [
          [ "Basic Usage", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1323", null ],
          [ "Configuration Options", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1324", null ]
        ] ],
        [ "Testing RPCs with grpcurl", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1325", null ]
      ] ],
      [ "Testing & Validation", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1327", [
        [ "Automated E2E Test Script", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1328", null ],
        [ "Manual Testing Workflow", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1329", [
          [ "1. Create Proposal", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1330", null ],
          [ "2. List Proposals", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1331", null ],
          [ "3. View Diff", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1332", null ],
          [ "4. Review in GUI", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1333", null ]
        ] ],
        [ "Performance Benchmarks", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1334", null ]
      ] ],
      [ "Development Workflows", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1336", [
        [ "Adding New Agent Commands", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1337", null ],
        [ "Adding New Test Harness RPCs", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1338", null ],
        [ "Adding Test Workflow Patterns", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1339", null ]
      ] ],
      [ "Troubleshooting", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1341", [
        [ "Common Issues", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1342", [
          [ "Port Already in Use", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1343", null ],
          [ "Connection Refused", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1344", null ],
          [ "Widget Not Found", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1345", [
            [ "Widget Not Found or Stale State", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1346", null ]
          ] ],
          [ "Crashes in <tt>Wait</tt> or <tt>Assert</tt> RPCs", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1347", null ]
        ] ],
        [ "Build Errors - Boolean Flag", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1348", [
          [ "Build Errors - Incomplete Type", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1349", null ]
        ] ],
        [ "Debug Mode", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1350", null ],
        [ "Test Harness Diagnostics", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1351", null ]
      ] ],
      [ "API Reference", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1353", [
        [ "RPC Service Definition", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1354", null ],
        [ "Request/Response Schemas", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1355", [
          [ "Ping", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1356", null ],
          [ "Click", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1357", null ],
          [ "Type", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1358", null ],
          [ "Wait", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1359", null ],
          [ "Assert", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1360", null ],
          [ "Screenshot", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1361", null ]
        ] ],
        [ "Resource Catalog Schema", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1362", null ]
      ] ],
      [ "Platform Notes", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1364", [
        [ "macOS (ARM64) - Production Ready ✅", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1365", null ],
        [ "macOS (Intel) - Should Work ⚠️", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1366", null ],
        [ "Linux - Should Work ⚠️", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1367", null ],
        [ "Windows - Experimental 🔬", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1368", null ]
      ] ],
      [ "Appendix", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1370", [
        [ "File Structure", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1371", null ],
        [ "Related Documentation", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1372", null ],
        [ "Version History", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1373", null ],
        [ "Contributors", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1374", null ],
        [ "License", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1375", null ]
      ] ]
    ] ],
    [ "z3ed: AI-Powered CLI for YAZE", "d3/d63/md_docs_2z3ed_2README.html", [
      [ "Overview", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1378", null ],
      [ "Quick Start", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1379", [
        [ "Build Options", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1380", null ],
        [ "AI Agent Commands", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1381", null ],
        [ "GUI Testing Commands", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1382", null ]
      ] ],
      [ "AI Service Setup", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1383", [
        [ "Ollama (Local LLM - Recommended for Development)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1384", null ],
        [ "Gemini (Google Cloud API)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1385", null ],
        [ "Example Prompts", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1386", null ]
      ] ],
      [ "Core Documentation", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1387", [
        [ "Essential Reads", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1388", null ],
        [ "Quick References", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1389", null ],
        [ "Implementation Guides", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1390", null ]
      ] ],
      [ "Current Status (October 2025)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1391", [
        [ "🔄 In Progress", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1392", null ],
        [ "📋 Planned", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1393", null ]
      ] ],
      [ "AI Editing Focus Areas", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1394", [
        [ "Overworld Tile16 Editing ⭐ PRIMARY FOCUS", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1395", null ],
        [ "Dungeon Editing", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1396", null ],
        [ "Palette Editing", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1397", null ],
        [ "Additional Capabilities", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1398", null ]
      ] ],
      [ "Example Workflows", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1399", [
        [ "Basic Tile16 Edit", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1400", null ],
        [ "Complex Multi-Step Edit", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1401", null ],
        [ "Label-Aware Dungeon Edit", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1402", null ]
      ] ],
      [ "Dependencies Guard", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1403", null ],
      [ "Recent Changes (Oct 3, 2025)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1404", [
        [ "SSL/HTTPS Support", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1405", null ],
        [ "Prompt Engineering", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1406", null ],
        [ "Documentation Consolidation", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1407", null ]
      ] ],
      [ "Troubleshooting", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1408", [
        [ "\"OpenSSL not found\" warning", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1409", null ],
        [ "\"gRPC not available\" error", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1410", null ],
        [ "AI generates invalid commands", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1411", [
          [ "Gemini-Specific Issues", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1412", null ]
        ] ]
      ] ]
    ] ],
    [ "yaze - Yet Another Zelda3 Editor", "d0/d30/md_README.html", [
      [ "Version 0.3.2 - Release", "d0/d30/md_README.html#autotoc_md1414", [
        [ "🛠️ Technical Improvements", "d0/d30/md_README.html#autotoc_md1418", null ]
      ] ],
      [ "Quick Start", "d0/d30/md_README.html#autotoc_md1419", [
        [ "Build", "d0/d30/md_README.html#autotoc_md1420", null ],
        [ "Applications", "d0/d30/md_README.html#autotoc_md1421", null ]
      ] ],
      [ "Usage", "d0/d30/md_README.html#autotoc_md1422", [
        [ "GUI Editor", "d0/d30/md_README.html#autotoc_md1423", null ],
        [ "Command Line Tool", "d0/d30/md_README.html#autotoc_md1424", null ],
        [ "C++ API", "d0/d30/md_README.html#autotoc_md1425", null ]
      ] ],
      [ "Documentation", "d0/d30/md_README.html#autotoc_md1426", null ],
      [ "Supported Platforms", "d0/d30/md_README.html#autotoc_md1427", null ],
      [ "ROM Compatibility", "d0/d30/md_README.html#autotoc_md1428", null ],
      [ "Contributing", "d0/d30/md_README.html#autotoc_md1429", null ],
      [ "License", "d0/d30/md_README.html#autotoc_md1430", null ],
      [ "🙏 Acknowledgments", "d0/d30/md_README.html#autotoc_md1431", null ],
      [ "📸 Screenshots", "d0/d30/md_README.html#autotoc_md1432", null ]
    ] ],
    [ "yaze Build Scripts", "de/d82/md_scripts_2README.html", [
      [ "Windows Scripts", "de/d82/md_scripts_2README.html#autotoc_md1435", [
        [ "vcpkg Setup (Optional)", "de/d82/md_scripts_2README.html#autotoc_md1436", null ]
      ] ],
      [ "Windows Build Workflow", "de/d82/md_scripts_2README.html#autotoc_md1437", [
        [ "Recommended: Visual Studio CMake Mode", "de/d82/md_scripts_2README.html#autotoc_md1438", null ],
        [ "Command Line Build", "de/d82/md_scripts_2README.html#autotoc_md1439", null ],
        [ "Compiler Notes", "de/d82/md_scripts_2README.html#autotoc_md1440", null ]
      ] ],
      [ "Quick Start (Windows)", "de/d82/md_scripts_2README.html#autotoc_md1441", [
        [ "Option 1: Visual Studio (Recommended)", "de/d82/md_scripts_2README.html#autotoc_md1442", null ],
        [ "Option 2: Command Line", "de/d82/md_scripts_2README.html#autotoc_md1443", null ],
        [ "Option 3: With vcpkg (Optional)", "de/d82/md_scripts_2README.html#autotoc_md1444", null ]
      ] ],
      [ "Troubleshooting", "de/d82/md_scripts_2README.html#autotoc_md1445", [
        [ "Common Issues", "de/d82/md_scripts_2README.html#autotoc_md1446", null ],
        [ "Getting Help", "de/d82/md_scripts_2README.html#autotoc_md1447", null ]
      ] ],
      [ "Other Scripts", "de/d82/md_scripts_2README.html#autotoc_md1448", null ]
    ] ],
    [ "YAZE Build Environment Verification Scripts", "dc/db4/md_scripts_2README__VERIFICATION.html", [
      [ "Quick Start", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1450", [
        [ "Verify Build Environment", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1451", null ]
      ] ],
      [ "Scripts Overview", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1452", [
        [ "<tt>verify-build-environment.ps1</tt> / <tt>.sh</tt>", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1453", [
          [ "Usage", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1454", null ],
          [ "Exit Codes", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1455", null ]
        ] ]
      ] ],
      [ "Common Workflows", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1456", [
        [ "First-Time Setup", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1457", null ],
        [ "After Pulling Changes", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1458", null ],
        [ "Troubleshooting Build Issues", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1459", null ],
        [ "Before Opening Pull Request", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1460", null ]
      ] ],
      [ "Automatic Fixes", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1461", [
        [ "Always Auto-Fixed (No Confirmation Required)", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1462", null ],
        [ "Fixed with <tt>-FixIssues</tt> / <tt>--fix</tt> Flag", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1463", null ],
        [ "Fixed with <tt>-CleanCache</tt> / <tt>--clean</tt> Flag", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1464", null ],
        [ "Optional Verbose Tests", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1465", null ]
      ] ],
      [ "Integration with Visual Studio", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1466", null ],
      [ "What Gets Checked", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1467", [
        [ "CMake (Required)", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1468", null ],
        [ "Git (Required)", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1469", null ],
        [ "Compilers (Required)", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1470", null ],
        [ "Platform Dependencies", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1471", null ],
        [ "CMake Cache", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1472", null ],
        [ "Dependencies", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1473", null ]
      ] ],
      [ "Automatic Fixes", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1474", null ],
      [ "CI/CD Integration", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1475", null ],
      [ "Troubleshooting", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1476", [
        [ "Script Reports \"CMake Not Found\"", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1477", null ],
        [ "\"Git Submodules Missing\"", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1478", null ],
        [ "\"CMake Cache Too Old\"", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1479", null ],
        [ "\"Visual Studio Not Found\" (Windows)", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1480", null ],
        [ "Script Fails on Network Issues (gRPC)", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1481", null ]
      ] ],
      [ "See Also", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1482", null ]
    ] ],
    [ "yaze Test Suite", "d0/d46/md_test_2README.html", [
      [ "Directory Structure", "d0/d46/md_test_2README.html#autotoc_md1484", null ],
      [ "Test Categories", "d0/d46/md_test_2README.html#autotoc_md1485", [
        [ "Unit Tests (<tt>unit/</tt>)", "d0/d46/md_test_2README.html#autotoc_md1486", null ],
        [ "Integration Tests (<tt>integration/</tt>)", "d0/d46/md_test_2README.html#autotoc_md1487", null ],
        [ "End-to-End Tests (<tt>e2e/</tt>)", "d0/d46/md_test_2README.html#autotoc_md1488", null ]
      ] ],
      [ "Enhanced Test Runner", "d0/d46/md_test_2README.html#autotoc_md1489", [
        [ "Usage Examples", "d0/d46/md_test_2README.html#autotoc_md1490", null ],
        [ "Test Modes", "d0/d46/md_test_2README.html#autotoc_md1491", null ],
        [ "Options", "d0/d46/md_test_2README.html#autotoc_md1492", null ]
      ] ],
      [ "E2E ROM Testing", "d0/d46/md_test_2README.html#autotoc_md1493", [
        [ "Test Cases", "d0/d46/md_test_2README.html#autotoc_md1494", null ]
      ] ],
      [ "ZSCustomOverworld Upgrade Testing", "d0/d46/md_test_2README.html#autotoc_md1495", [
        [ "Supported Upgrades", "d0/d46/md_test_2README.html#autotoc_md1496", null ],
        [ "Test Cases", "d0/d46/md_test_2README.html#autotoc_md1497", null ],
        [ "Version-Specific Features", "d0/d46/md_test_2README.html#autotoc_md1498", [
          [ "Vanilla", "d0/d46/md_test_2README.html#autotoc_md1499", null ],
          [ "v2", "d0/d46/md_test_2README.html#autotoc_md1500", null ],
          [ "v3", "d0/d46/md_test_2README.html#autotoc_md1501", null ]
        ] ]
      ] ],
      [ "Environment Variables", "d0/d46/md_test_2README.html#autotoc_md1502", null ],
      [ "CI/CD Integration", "d0/d46/md_test_2README.html#autotoc_md1503", null ],
      [ "Deprecated Tests", "d0/d46/md_test_2README.html#autotoc_md1504", null ],
      [ "Best Practices", "d0/d46/md_test_2README.html#autotoc_md1505", null ],
      [ "AI Agent Testing", "d0/d46/md_test_2README.html#autotoc_md1506", null ]
    ] ],
    [ "Deprecated List", "da/d58/deprecated.html", null ],
    [ "Todo List", "dd/da0/todo.html", null ],
    [ "Topics", "topics.html", "topics" ],
    [ "Namespaces", "namespaces.html", [
      [ "Namespace List", "namespaces.html", "namespaces_dup" ],
      [ "Namespace Members", "namespacemembers.html", [
        [ "All", "namespacemembers.html", "namespacemembers_dup" ],
        [ "Functions", "namespacemembers_func.html", "namespacemembers_func" ],
        [ "Variables", "namespacemembers_vars.html", "namespacemembers_vars" ],
        [ "Typedefs", "namespacemembers_type.html", null ],
        [ "Enumerations", "namespacemembers_enum.html", null ],
        [ "Enumerator", "namespacemembers_eval.html", null ]
      ] ]
    ] ],
    [ "Classes", "annotated.html", [
      [ "Class List", "annotated.html", "annotated_dup" ],
      [ "Class Index", "classes.html", null ],
      [ "Class Hierarchy", "hierarchy.html", "hierarchy" ],
      [ "Class Members", "functions.html", [
        [ "All", "functions.html", "functions_dup" ],
        [ "Functions", "functions_func.html", "functions_func" ],
        [ "Variables", "functions_vars.html", "functions_vars" ],
        [ "Typedefs", "functions_type.html", null ],
        [ "Enumerations", "functions_enum.html", null ],
        [ "Enumerator", "functions_eval.html", null ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ],
      [ "File Members", "globals.html", [
        [ "All", "globals.html", "globals_dup" ],
        [ "Functions", "globals_func.html", null ],
        [ "Variables", "globals_vars.html", null ],
        [ "Typedefs", "globals_type.html", null ],
        [ "Enumerations", "globals_enum.html", null ],
        [ "Enumerator", "globals_eval.html", null ],
        [ "Macros", "globals_defs.html", "globals_defs" ]
      ] ]
    ] ]
  ] ]
];

var NAVTREEINDEX =
[
"annotated.html",
"d0/d27/namespaceyaze_1_1gfx.html#aa4117b319d832a812cae4e7e38216b56",
"d0/d46/md_test_2README.html#autotoc_md1496",
"d0/d85/structyaze_1_1cli_1_1WidgetDescriptor.html#acf00e29ffad50daf02005fd6db6a6b9c",
"d0/de1/group__core.html#ga3a3a4e60f272ceed1dea6ccaefd39e65",
"d1/d22/classyaze_1_1editor_1_1DungeonObjectInteraction.html#aaea2ec79332707ba797ae6160b88b223",
"d1/d3e/namespaceyaze_1_1editor.html#a7ca12a09e73dbe5ae06be665d9a68cc9",
"d1/d51/gui__automation__client_8h.html#ac2dd1cd8208462d16360cd77b564e536",
"d1/daf/compression_8h.html#a31d4914cc6f5a2d6cc4c5a06baf7336e",
"d1/deb/structyaze_1_1test_1_1TestScriptStep.html#a2543d5198bc0b8aff6590324ddbb5532",
"d2/d07/classyaze_1_1emu_1_1Spc700.html#a53e86b4a443e8ba505af7e056efcdd8f",
"d2/d20/classyaze_1_1cli_1_1PromptBuilder.html#a9f523cdb750ab354a806b8f925f10f02",
"d2/d5e/classyaze_1_1emu_1_1Memory.html#a4de051dd6298e74353cab01771006340",
"d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md565",
"d2/de7/namespaceyaze_1_1emu_1_1anonymous__namespace_02snes_8cc_03.html#a34f0bcbc0fe70ab704bb3d117f3a73b1",
"d2/dfe/structyaze_1_1gui_1_1EnhancedTheme.html#a6c7dbeb4d24aff833fa13e81876464d1",
"d3/d15/classyaze_1_1emu_1_1Snes.html#af60c99652ed0f5ea7c175cbf20158b7f",
"d3/d2e/compression_8cc.html#a6bb3ddaab48f29adaed1772397494806",
"d3/d44/classyaze_1_1editor_1_1OverworldEditor.html#a795074db61aaf19e9f687a1f357524e2",
"d3/d5a/classyaze_1_1zelda3_1_1OverworldEntrance.html#a69ded8417527cec8029c43131a80c6ac",
"d3/d6c/classyaze_1_1editor_1_1MessageEditor.html#ad8509fbb55cdbaa59f94a9b9fbf8fc6d",
"d3/d8f/structyaze_1_1gui_1_1canvas_1_1CanvasConfig.html#a8704be8268f5a2f53b9ac9efd657c78c",
"d3/da7/structyaze_1_1gfx_1_1lc__lz2_1_1CompressionContext.html#adbf21cd58a3f4f5b2618a2375fac884f",
"d3/de4/classyaze_1_1gfx_1_1TileInfo.html#a1fe1da177e22542d73b988dcf60413aa",
"d3/ded/classyaze_1_1emu_1_1Ppu.html#ac932013236cc141467a245f0225bb7b4",
"d4/d0a/namespaceyaze_1_1test.html#a6ba475db219e6dcb9a66cb047a014f74",
"d4/d45/classyaze_1_1editor_1_1AssemblyEditor.html#a9ce9d400bac9e7e0ca724071bfed22fc",
"d4/d79/app_2rom_8cc.html#aeb8ae3fba1064ff2cf19393b113fac1c",
"d4/db9/structyaze_1_1cli_1_1TestResultDetails.html#a9a6eff56323dfb62d29f0f782c4171cc",
"d4/df3/classyaze_1_1zelda3_1_1MockRom.html#a337b12888b291a309e759d6044866720",
"d5/d1f/namespaceyaze_1_1zelda3.html#a44b1e31e5857baaa00dc88ae724003c3a6fc4335f414e64a079526c84e5ec6218",
"d5/d1f/namespaceyaze_1_1zelda3.html#aa2fc6ec9ffffbab2934b1a3d84f0aa4e",
"d5/d3c/classyaze_1_1gfx_1_1GraphicsBuffer.html#a5a3ea3293db352ba41c6990eccf0f966",
"d5/d90/commands_8h.html#ab7fbe1f4946d07411f70942eed73a58e",
"d5/dc2/classyaze_1_1gui_1_1BackgroundRenderer.html#a399d6ee17ba199e6ed31fddb2b6e4a55",
"d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md151",
"d6/d2e/classyaze_1_1zelda3_1_1TitleScreen.html#a29be946a5c0abebe5e725b550670e9ad",
"d6/d30/classyaze_1_1Rom.html#ad68eaae59b6ff8aacd5ac74fa57c21dc",
"d6/d61/structobject__door.html",
"d6/db1/classyaze_1_1zelda3_1_1Sprite.html#a3070422abb2ea9f2b0ba795e36aeec9b",
"d6/dcb/classyaze_1_1gui_1_1canvas_1_1CanvasPerformanceIntegration.html#aba76be85193aa831ff11960858442b73",
"d7/d0d/md_docs_2E7-tile16-editor-palette-system.html",
"d7/d61/classyaze_1_1zelda3_1_1DungeonEditorSystem.html#a1496152628034c3d6c1adb121efa0d59",
"d7/d61/classyaze_1_1zelda3_1_1DungeonEditorSystem.html#ace38248a914f9815e62c13dcd2c177df",
"d7/d83/classyaze_1_1gfx_1_1Tile32.html#aa118e4518ff66a75ca084b25fda82870",
"d7/db2/zscustomoverworld__upgrade__test_8cc.html#a594b645f510a6c770fd9c134fccdccfb",
"d7/df6/classyaze_1_1zelda3_1_1Room.html#a18435570337fd27610a6f65f2e1ca3c8",
"d7/dfa/classyaze_1_1zelda3_1_1ObjectRenderer.html#a405b5884ad05376e545b498b80343aeb",
"d8/d0c/structyaze_1_1core_1_1ProjectMetadata.html#a71e64f64d2e0ac0bbc04f89508238b7f",
"d8/d4d/structyaze_1_1core_1_1ResourceLabelManager.html#a5d8b27d4c0adc23efd9f864dfcb45c9e",
"d8/d98/classyaze_1_1editor_1_1DungeonEditor.html#a3e9b861602c3affde3ed71d5f511b8f6",
"d8/dbd/structyaze_1_1editor_1_1TextElement.html#adae4011df81ed55417977322553191b9",
"d8/ddb/snes__palette_8h.html#a82a8956476ffc04750bcfc4120c8b8dba763760ff92c842ba0fba2b7916605884",
"d9/d41/md_docs_202-build-instructions.html#autotoc_md55",
"d9/d8e/structyaze_1_1gfx_1_1AtlasRenderer_1_1AtlasEntry.html#af0bbafbc312f81baa29be2c4aa6059d5",
"d9/dc5/classyaze_1_1zelda3_1_1Overworld.html#a0cb4ee0bc2ed6cb95855c227207a10d5",
"d9/dcc/classyaze_1_1zelda3_1_1OverworldMap.html#a4331d3eeb9aa2548be0d746883a5ce38",
"d9/dee/structyaze_1_1gfx_1_1Paletteset.html#a1f70d828e1407efe366a0ae3e389515c",
"da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md831",
"da/d2c/classyaze_1_1gui_1_1Canvas.html#a913cef934a9c4cfec90a4e1b5f0abeda",
"da/d3e/classyaze_1_1test_1_1TestManager.html#a2cafce8c64ca0935b8a121f2d06c72dd",
"da/d5e/structyaze_1_1core_1_1YazeProject.html#acb4db62c673fc40cbbcd426d312d8912",
"da/da6/structyaze_1_1editor_1_1SpriteItem.html#ae826d5c1e077d4bc2f485747cbc146a4",
"da/dd7/structyaze_1_1emu_1_1SETINI.html#a98be1bcc8f8b248c7bbb430b89751851",
"db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md298",
"db/d6c/app_2core_2project_8cc.html#ab275661a03c76657529623faa3f23c55",
"db/d82/classyaze_1_1editor_1_1Tile16Editor.html#ae63c61721d0513d744728bb1fea37996",
"db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1292",
"db/dd1/structyaze_1_1emu_1_1M7Y.html#ae043ece67222cf57094acd66b19e3b9a",
"dc/d01/classyaze_1_1emu_1_1Dsp.html#ac290b0ed7c7ac5d6a91d90e9418b5174",
"dc/d31/classyaze_1_1editor_1_1GraphicsEditor.html#a40ad43a832a24147d8b0521e71b41503",
"dc/d4c/structyaze_1_1emu_1_1WindowMaskSettings.html#a89fdccbe33cc749addc27a39c6e065a1",
"dc/d6b/classyaze_1_1cli_1_1TuiComponent.html#ac6b9596c7e3c8928c7571390e6418529",
"dc/dc6/structyaze_1_1editor_1_1palette__internal_1_1PaletteChange.html#aa89e9b22e3ca54fea486ae9eebd8eab0",
"dc/df4/classyaze_1_1emu_1_1Cpu.html#a46d1550e1ff864e71f022d5b38979bd5",
"dc/df4/classyaze_1_1emu_1_1Cpu.html#acbdbeaa61ec026582d69a2558c9b14dd",
"dd/d12/classyaze_1_1editor_1_1EditorManager.html#a5e505c03d894fb42fe96880d66861690",
"dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md683",
"dd/d5b/md_docs_2B1-contributing.html#autotoc_md352",
"dd/d80/classyaze_1_1cli_1_1CommandHandler.html#a5a563d425d0ee2d3779983ca83b35c3a",
"dd/dd2/structTextEditor_1_1LanguageDefinition.html#ab0c6722298c1d597ae4f3e266935b318",
"dd/df6/namespaceyaze_1_1zelda3_1_1anonymous__namespace_02tracker_8cc_03.html",
"de/d12/style_8h.html#a59476f506247500392e1f7512924649a",
"de/d76/classyaze_1_1editor_1_1SpriteEditor.html#ac8bbf35ecc525892a66eb7633b844a04",
"de/da2/namespaceyaze_1_1test_1_1anonymous__namespace_02test__script__parser_8cc_03.html#a8a312fbf8bf11e94ba9e4a8472a70182",
"de/dbf/icons_8h.html#a11a3d226a125389575798f873b673d8d",
"de/dbf/icons_8h.html#a301945cab72b0fa754c3df50e8ae3597",
"de/dbf/icons_8h.html#a4c5cc04de4dd8dc348218831fe596f55",
"de/dbf/icons_8h.html#a68061f81eec93a5ae826b1d61c9fa3b9",
"de/dbf/icons_8h.html#a888806a33477cfbe78c10fe6497c3615",
"de/dbf/icons_8h.html#aa778ea9556690dd1cd9e08d98dfe851a",
"de/dbf/icons_8h.html#ac3e85c7d07a49e64d711683c0f009a19",
"de/dbf/icons_8h.html#adf7dc462cac672799a010c2b8788ec7d",
"de/dbf/icons_8h.html#afa8d44d1d72b456218cde64aa76b8458",
"de/de7/classyaze_1_1zelda3_1_1DungeonObjectEditor.html#a7cb921c32e80acc9e7686e4e4acb1161",
"df/d0e/classyaze_1_1gfx_1_1Bitmap.html#a8b0be39d195a5d6d7002d2ecd70f2a3c",
"df/d41/structyaze_1_1editor_1_1zsprite_1_1ZSprite.html#aa5372405502e5d4a4458ad17da19349d",
"df/db9/classyaze_1_1cli_1_1ModernCLI.html#a4e1b40ecd2068cc9c053047bf8d8389d",
"functions_func.html"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';