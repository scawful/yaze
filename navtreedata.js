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
    [ "z3ed CLI Architecture & Design", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html", [
      [ "1. Overview", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1132", null ],
      [ "2. Design Goals", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1133", [
        [ "2.1. Key Architectural Decisions", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1134", null ]
      ] ],
      [ "3. Proposed CLI Architecture: Resource-Oriented Commands", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1135", [
        [ "3.1. Top-Level Resources", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1136", null ],
        [ "3.2. Example Command Mapping", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1137", null ]
      ] ],
      [ "4. New Features & Commands", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1138", [
        [ "4.1. For the ROM Hacker (Power & Scriptability)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1139", null ],
        [ "4.2. For Testing & Automation", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1140", null ]
      ] ],
      [ "5. TUI Enhancements", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1141", null ],
      [ "6. Generative & Agentic Workflows (MCP Integration)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1142", [
        [ "6.1. The Generative Workflow", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1143", null ],
        [ "6.2. Key Enablers", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1144", null ]
      ] ],
      [ "7. Implementation Roadmap", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1145", [
        [ "Phase 1: Core CLI & TUI Foundation (Done)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1146", null ],
        [ "Phase 2: Interactive TUI & Command Palette (Done)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1147", null ],
        [ "Phase 3: Testing & Project Management (Done)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1148", null ],
        [ "Phase 4: Agentic Framework & Generative AI (In Progress)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1149", null ],
        [ "Phase 5: Code Structure & UX Improvements (Completed)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1150", null ],
        [ "Phase 6: Resource Catalogue & API Documentation (✅ Completed - Oct 1, 2025)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1151", null ]
      ] ],
      [ "8. Agentic Framework Architecture - Advanced Dive", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1152", [
        [ "8.1. The <tt>z3ed agent</tt> Command", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1153", null ],
        [ "8.2. The Agentic Loop (MCP) - Detailed Workflow", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1154", null ],
        [ "8.3. AI Model & Protocol Strategy", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1155", null ]
      ] ],
      [ "9. Test Harness Evolution: From Automation to Platform", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1157", [
        [ "9.1. Current Capabilities (IT-01 to IT-04) ✅", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1158", null ],
        [ "9.2. Limitations Identified", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1159", null ],
        [ "9.3. Enhancement Roadmap (IT-05 to IT-09)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1160", [
          [ "IT-05: Test Introspection API (6-8 hours)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1161", null ],
          [ "IT-06: Widget Discovery API (4-6 hours)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1162", null ],
          [ "IT-07: Test Recording & Replay ✅ COMPLETE", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1163", null ],
          [ "IT-08: Holistic Error Reporting (5-7 hours)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1164", null ],
          [ "IT-09: CI/CD Integration ✅ CLI Foundations Complete", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1165", null ]
        ] ],
        [ "9.4. Unified Testing Vision", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1166", null ],
        [ "9.5. Implementation Priority", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1167", null ],
        [ "8.4. GUI Integration & User Experience", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1169", null ],
        [ "8.5. Testing & Verification", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1170", null ],
        [ "8.6. Safety & Sandboxing", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1171", null ],
        [ "8.7. Optional JSON Dependency", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1172", null ],
        [ "8.8. Contextual Awareness & Feedback Loop", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1173", null ],
        [ "8.9. Error Handling and Recovery", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1174", null ],
        [ "8.10. Extensibility", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1175", null ]
      ] ],
      [ "9. UX Improvements and Architectural Decisions", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1176", [
        [ "9.1. TUI Component Architecture", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1177", null ],
        [ "9.2. Command Handler Unification", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1178", null ],
        [ "9.3. Interface Consolidation", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1179", null ],
        [ "9.4. Code Organization Improvements", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1180", null ],
        [ "9.5. Future UX Enhancements", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1181", null ]
      ] ],
      [ "10. Implementation Status and Code Quality", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1182", [
        [ "10.1. Recent Refactoring Improvements (January 2025)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1183", null ],
        [ "10.2. File Organization", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1184", null ],
        [ "10.3. Code Quality Improvements", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1185", null ],
        [ "10.4. TUI Component System", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1186", null ],
        [ "10.5. Known Limitations", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1187", null ],
        [ "10.6. Future Code Quality Goals", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1188", null ]
      ] ],
      [ "11. Agent-Ready API Surface Area", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1189", null ],
      [ "12. Acceptance & Review Workflow", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1190", [
        [ "12.1. Change Proposal Lifecycle", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1191", null ],
        [ "12.2. UI Extensions", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1192", null ],
        [ "12.3. Policy Configuration", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1193", null ]
      ] ],
      [ "13. ImGuiTestEngine Control Bridge", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1194", [
        [ "13.1. Bridge Architecture", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1195", [
          [ "13.1.1. Transport & Envelope", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1196", null ],
          [ "13.1.2. Harness Runtime Lifecycle", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1197", null ],
          [ "13.1.3. Integration with <tt>z3ed agent</tt>", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1198", null ]
        ] ],
        [ "13.2. Safety & Sandboxing", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1199", null ],
        [ "13.3. Script Generation Strategy", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1200", null ]
      ] ],
      [ "14. Test & Verification Strategy", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1201", [
        [ "14.1. Layered Test Suites", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1202", null ],
        [ "14.2. Continuous Verification", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1203", null ],
        [ "14.3. Telemetry-Informed Testing", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1204", null ]
      ] ],
      [ "15. Expanded Roadmap (Phase 6+)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1205", [
        [ "Phase 6: Agent Workflow Foundations (Planned)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1206", null ],
        [ "Phase 7: Controlled Mutation & Review (Planned)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1207", null ],
        [ "Phase 8: Learning & Self-Improvement (Exploratory)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1208", null ],
        [ "7.4. Widget ID Management for Test Automation", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1209", null ]
      ] ]
    ] ],
    [ "z3ed Agentic Workflow Plan", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html", [
      [ "Executive Summary", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1211", null ],
      [ "Quick Reference", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1212", null ],
      [ "1. Current Priorities (Week of Oct 2-8, 2025)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1214", [
        [ "Priority 1: Test Harness Enhancements (IT-05 to IT-09) 🔧 ACTIVE", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1215", [
          [ "IT-05: Test Introspection API (6-8 hours)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1216", null ],
          [ "IT-06: Widget Discovery API (4-6 hours)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1217", null ],
          [ "IT-07: Test Recording & Replay ✅ COMPLETE (Oct 2, 2025)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1218", null ],
          [ "IT-08: Enhanced Error Reporting (5-7 hours) ✅ COMPLETE", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1219", null ],
          [ "IT-09: CI/CD Integration ✅ CLI Tooling Shipped", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1220", null ],
          [ "IT-10: Collaborative Editing & Multiplayer Sessions ⏸️ DEPRIORITIZED", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1222", null ]
        ] ],
        [ "Priority 2: LLM Integration (Ollama + Gemini + Claude) 🤖 NEW PRIORITY", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1224", [
          [ "Phase 1: Ollama Local Integration (4-6 hours) 🎯 START HERE", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1225", null ],
          [ "Phase 2: Gemini Fixes (2-3 hours)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1226", null ],
          [ "Phase 3: Claude Integration (2-3 hours)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1227", null ],
          [ "Phase 4: Enhanced Prompt Engineering (3-4 hours)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1228", null ]
        ] ],
        [ "Priority 3: Windows Cross-Platform Testing 🪟", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1230", null ],
        [ "Priority 2: Windows Cross-Platform Testing 🪟", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1232", null ]
      ] ],
      [ "</blockquote>", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1233", null ],
      [ "2. Workstreams Overview", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1234", [
        [ "Completed Work Summary", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1235", null ]
      ] ],
      [ "3. Task Backlog", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1237", null ],
      [ "3. Immediate Next Steps (Week of Oct 1-7, 2025)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1238", [
        [ "Priority 0: Testing & Validation (Active)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1239", null ],
        [ "Priority 1: ImGuiTestHarness Foundation (IT-01) ✅ COMPLETE", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1240", [
          [ "Phase 1: gRPC Infrastructure ✅ COMPLETE", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1241", null ],
          [ "Phase 2: ImGuiTestEngine Integration ✅ COMPLETE", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1242", null ],
          [ "Phase 3: Full ImGuiTestEngine Integration ✅ COMPLETE (Oct 2, 2025)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1243", null ],
          [ "Phase 4: CLI Integration & Windows Testing (4-5 hours)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1244", null ]
        ] ],
        [ "IT-01 Quick Reference", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1245", null ],
        [ "Priority 2: Policy Evaluation Framework (AW-04, 4-6 hours)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1246", null ],
        [ "Priority 3: Documentation & Consolidation (2-3 hours)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1247", null ],
        [ "Later: Advanced Features", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1248", null ]
      ] ],
      [ "4. Current Issues & Blockers", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1249", [
        [ "Active Issues", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1250", null ],
        [ "Known Limitations (Non-Blocking)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1251", null ]
      ] ],
      [ "5. Architecture Overview", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1252", [
        [ "5.1. Proposal Lifecycle Flow", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1253", null ],
        [ "5.2. Component Interaction Diagram", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1254", null ],
        [ "5.3. Data Flow: Agent Run to ROM Merge", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1255", null ]
      ] ],
      [ "5. Open Questions", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1256", null ],
      [ "4. Work History & Key Decisions", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1257", [
        [ "Resource Catalogue Workstream (RC) - ✅ COMPLETE", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1258", null ],
        [ "Acceptance Workflow (AW-01, AW-02, AW-03) - ✅ COMPLETE", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1259", null ],
        [ "ImGuiTestHarness (IT-01, IT-02) - ✅ CORE COMPLETE", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1260", null ],
        [ "Files Modified/Created", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1261", null ]
      ] ],
      [ "5. Open Questions", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1262", null ],
      [ "6. References", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1263", null ]
    ] ],
    [ "z3ed CLI Technical Reference", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html", [
      [ "Table of Contents", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1267", null ],
      [ "Architecture Overview", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1269", [
        [ "System Components", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1270", null ],
        [ "Data Flow: Proposal Lifecycle", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1271", null ]
      ] ],
      [ "Command Reference", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1273", [
        [ "Agent Commands", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1274", [
          [ "<tt>agent run</tt> - Execute AI-driven ROM modifications", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1275", null ],
          [ "<tt>agent list</tt> - Show all proposals", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1276", null ],
          [ "<tt>agent diff</tt> - Show proposal changes", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1277", null ],
          [ "<tt>agent describe</tt> - Export machine-readable API specs", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1278", null ],
          [ "<tt>agent test</tt> - Automated GUI testing (IT-02)", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1279", null ],
          [ "<tt>agent gui</tt> - GUI Introspection & Control (IT-05/IT-06)", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1280", [
            [ "<tt>agent gui discover</tt> - Enumerate available widgets", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1281", null ],
            [ "<tt>agent test status</tt> - Query test execution state", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1282", null ],
            [ "<tt>agent test results</tt> - Get detailed test results", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1283", null ],
            [ "<tt>agent test list</tt> - List all tests", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1284", null ]
          ] ],
          [ "<tt>agent test record</tt> - Record test sessions (IT-07)", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1285", [
            [ "<tt>agent test record start</tt> - Begin recording", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1286", null ],
            [ "<tt>agent test record stop</tt> - Finish recording", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1287", null ]
          ] ],
          [ "<tt>agent test replay</tt> - Execute recorded tests", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1288", null ],
          [ "<tt>agent test suite</tt> - Manage test suites (IT-09)", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1289", null ]
        ] ],
        [ "ROM Commands", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1290", [
          [ "<tt>rom info</tt> - Display ROM metadata", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1291", null ],
          [ "<tt>rom validate</tt> - Verify ROM integrity", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1292", null ],
          [ "<tt>rom diff</tt> - Compare two ROMs", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1293", null ],
          [ "<tt>rom generate-golden</tt> - Create reference checksums", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1294", null ]
        ] ],
        [ "Palette Commands", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1295", [
          [ "<tt>palette export</tt> - Export palette to file", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1296", null ],
          [ "<tt>palette import</tt> - Import palette from file", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1297", null ],
          [ "<tt>palette list</tt> - Show available palettes", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1298", null ]
        ] ],
        [ "Overworld Commands", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1299", [
          [ "<tt>overworld get-tile</tt> - Get tile at coordinates", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1300", null ],
          [ "<tt>overworld set-tile</tt> - Set tile at coordinates", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1301", null ]
        ] ],
        [ "Dungeon Commands", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1302", [
          [ "<tt>dungeon list-rooms</tt> - List all dungeon rooms", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1303", null ],
          [ "<tt>dungeon add-object</tt> - Add object to room", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1304", null ]
        ] ]
      ] ],
      [ "Implementation Guide", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1306", [
        [ "Building with gRPC Support", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1307", [
          [ "macOS (Recommended)", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1308", null ],
          [ "Windows (Experimental)", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1309", null ]
        ] ],
        [ "Starting Test Harness", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1310", [
          [ "Basic Usage", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1311", null ],
          [ "Configuration Options", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1312", null ]
        ] ],
        [ "Testing RPCs with grpcurl", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1313", null ]
      ] ],
      [ "Testing & Validation", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1315", [
        [ "Automated E2E Test Script", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1316", null ],
        [ "Manual Testing Workflow", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1317", [
          [ "1. Create Proposal", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1318", null ],
          [ "2. List Proposals", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1319", null ],
          [ "3. View Diff", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1320", null ],
          [ "4. Review in GUI", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1321", null ]
        ] ],
        [ "Performance Benchmarks", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1322", null ]
      ] ],
      [ "Development Workflows", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1324", [
        [ "Adding New Agent Commands", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1325", null ],
        [ "Adding New Test Harness RPCs", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1326", null ],
        [ "Adding Test Workflow Patterns", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1327", null ]
      ] ],
      [ "Troubleshooting", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1329", [
        [ "Common Issues", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1330", [
          [ "Port Already in Use", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1331", null ],
          [ "Connection Refused", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1332", null ],
          [ "Widget Not Found", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1333", [
            [ "Widget Not Found or Stale State", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1334", null ]
          ] ],
          [ "Crashes in <tt>Wait</tt> or <tt>Assert</tt> RPCs", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1335", null ]
        ] ],
        [ "Build Errors - Boolean Flag", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1336", [
          [ "Build Errors - Incomplete Type", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1337", null ]
        ] ],
        [ "Debug Mode", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1338", null ],
        [ "Test Harness Diagnostics", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1339", null ]
      ] ],
      [ "API Reference", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1341", [
        [ "RPC Service Definition", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1342", null ],
        [ "Request/Response Schemas", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1343", [
          [ "Ping", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1344", null ],
          [ "Click", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1345", null ],
          [ "Type", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1346", null ],
          [ "Wait", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1347", null ],
          [ "Assert", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1348", null ],
          [ "Screenshot", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1349", null ]
        ] ],
        [ "Resource Catalog Schema", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1350", null ]
      ] ],
      [ "Platform Notes", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1352", [
        [ "macOS (ARM64) - Production Ready ✅", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1353", null ],
        [ "macOS (Intel) - Should Work ⚠️", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1354", null ],
        [ "Linux - Should Work ⚠️", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1355", null ],
        [ "Windows - Experimental 🔬", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1356", null ]
      ] ],
      [ "Appendix", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1358", [
        [ "File Structure", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1359", null ],
        [ "Related Documentation", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1360", null ],
        [ "Version History", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1361", null ],
        [ "Contributors", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1362", null ],
        [ "License", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1363", null ]
      ] ]
    ] ],
    [ "IT-05: Test Introspection API – Implementation Guide", "d5/d3d/md_docs_2z3ed_2IT-05-IMPLEMENTATION-GUIDE.html", [
      [ "Progress Snapshot", "d5/d3d/md_docs_2z3ed_2IT-05-IMPLEMENTATION-GUIDE.html#autotoc_md1366", null ],
      [ "Progress Snapshot", "d5/d3d/md_docs_2z3ed_2IT-05-IMPLEMENTATION-GUIDE.html#autotoc_md1367", null ],
      [ "Overview", "d5/d3d/md_docs_2z3ed_2IT-05-IMPLEMENTATION-GUIDE.html#autotoc_md1368", null ],
      [ "Motivation", "d5/d3d/md_docs_2z3ed_2IT-05-IMPLEMENTATION-GUIDE.html#autotoc_md1369", null ],
      [ "Architecture", "d5/d3d/md_docs_2z3ed_2IT-05-IMPLEMENTATION-GUIDE.html#autotoc_md1370", [
        [ "New Service Components", "d5/d3d/md_docs_2z3ed_2IT-05-IMPLEMENTATION-GUIDE.html#autotoc_md1371", null ],
        [ "Proto Additions", "d5/d3d/md_docs_2z3ed_2IT-05-IMPLEMENTATION-GUIDE.html#autotoc_md1372", null ]
      ] ],
      [ "Implementation Steps", "d5/d3d/md_docs_2z3ed_2IT-05-IMPLEMENTATION-GUIDE.html#autotoc_md1373", [
        [ "Step 1: Extend TestManager (✔️ Completed)", "d5/d3d/md_docs_2z3ed_2IT-05-IMPLEMENTATION-GUIDE.html#autotoc_md1374", [
          [ "1.2 Update Existing RPC Handlers", "d5/d3d/md_docs_2z3ed_2IT-05-IMPLEMENTATION-GUIDE.html#autotoc_md1375", null ]
        ] ],
        [ "Step 2: Implement Introspection RPCs (✔️ Completed)", "d5/d3d/md_docs_2z3ed_2IT-05-IMPLEMENTATION-GUIDE.html#autotoc_md1376", null ],
        [ "Step 3: CLI Integration (🚧 TODO)", "d5/d3d/md_docs_2z3ed_2IT-05-IMPLEMENTATION-GUIDE.html#autotoc_md1377", null ],
        [ "Step 4: Testing & Validation (🚧 TODO)", "d5/d3d/md_docs_2z3ed_2IT-05-IMPLEMENTATION-GUIDE.html#autotoc_md1378", [
          [ "Test Script: <tt>scripts/test_introspection_e2e.sh</tt>", "d5/d3d/md_docs_2z3ed_2IT-05-IMPLEMENTATION-GUIDE.html#autotoc_md1379", null ]
        ] ]
      ] ],
      [ "Success Criteria", "d5/d3d/md_docs_2z3ed_2IT-05-IMPLEMENTATION-GUIDE.html#autotoc_md1380", null ],
      [ "Migration Guide", "d5/d3d/md_docs_2z3ed_2IT-05-IMPLEMENTATION-GUIDE.html#autotoc_md1381", null ],
      [ "Next Steps", "d5/d3d/md_docs_2z3ed_2IT-05-IMPLEMENTATION-GUIDE.html#autotoc_md1382", null ],
      [ "References", "d5/d3d/md_docs_2z3ed_2IT-05-IMPLEMENTATION-GUIDE.html#autotoc_md1383", null ]
    ] ],
    [ "IT-08: Enhanced Error Reporting Implementation Guide", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html", [
      [ "Phase Overview", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1387", null ],
      [ "IT-08a: Screenshot RPC ✅ COMPLETE", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1389", [
        [ "Implementation Summary", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1390", null ],
        [ "What Was Built", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1391", null ],
        [ "Technical Implementation", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1392", null ]
      ] ],
      [ "Build (needs YAZE_WITH_GRPC=ON)", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1393", null ],
      [ "Start harness", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1394", null ],
      [ "Queue a failing automation step", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1395", null ],
      [ "Fetch diagnostics", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1396", null ],
      [ "Inspect artifact directory", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1397", [
        [ "IT-08b: Auto-Capture on Test Failure 🔄 IN PROGRESS", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1405", [
          [ "Next Steps", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1398", null ],
          [ "Technical Implementation", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1400", null ],
          [ "Testing", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1401", null ],
          [ "Success Criteria", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1402", null ],
          [ "Retro Notes", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1403", null ],
          [ "Implementation Plan", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1406", [
            [ "Step 1: Modify TestManager (30 minutes)", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1407", null ],
            [ "Step 2: Update TestHistory Structure (15 minutes)", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1408", null ],
            [ "Step 3: Update GetTestResults RPC (30 minutes)", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1409", null ],
            [ "Step 4: Update Proto Schema (15 minutes)", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1410", null ]
          ] ],
          [ "Testing", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1411", null ],
          [ "Success Criteria", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1412", null ]
        ] ],
        [ "IT-08c: Widget State Dumps ✅ COMPLETE", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1414", [
          [ "Implementation Summary", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1415", null ],
          [ "What Was Built", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1416", null ],
          [ "Technical Implementation", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1417", null ],
          [ "Output Example", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1418", null ],
          [ "Testing", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1419", null ],
          [ "Success Criteria", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1420", null ],
          [ "Benefits for Debugging", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1421", null ]
        ] ],
        [ "IT-08c: Widget State Dumps 📋 PLANNED", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1423", [
          [ "Implementation Plan", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1424", [
            [ "Step 1: Create Widget State Capture Utility (30 minutes)", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1425", null ],
            [ "Step 2: Integrate with TestManager (15 minutes)", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1426", null ]
          ] ],
          [ "Output Example", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1427", null ]
        ] ],
        [ "IT-08d: Error Envelope Standardization 📋 PLANNED", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1429", [
          [ "Proposed Error Envelope", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1430", null ],
          [ "Integration Points", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1431", null ]
        ] ],
        [ "IT-08e: CLI Error Improvements 📋 PLANNED", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1433", [
          [ "Enhanced CLI Output", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1434", null ]
        ] ],
        [ "Progress Tracking", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1436", [
          [ "Completed ✅", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1437", null ],
          [ "In Progress 🔄", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1438", null ],
          [ "Planned 📋", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1439", null ],
          [ "Time Investment", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1440", null ]
        ] ],
        [ "Next Steps", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1442", null ],
        [ "References", "d4/db6/md_docs_2z3ed_2IT-08-IMPLEMENTATION-GUIDE.html#autotoc_md1444", null ]
      ] ]
    ] ],
    [ "IT-08b: Auto-Capture on Test Failure - Implementation Guide", "d2/d98/md_docs_2z3ed_2IT-08b-AUTO-CAPTURE.html", [
      [ "Overview", "d2/d98/md_docs_2z3ed_2IT-08b-AUTO-CAPTURE.html#autotoc_md1449", null ],
      [ "Implementation Steps", "d2/d98/md_docs_2z3ed_2IT-08b-AUTO-CAPTURE.html#autotoc_md1451", [
        [ "Step 1: Update TestHistory Structure (15 minutes)", "d2/d98/md_docs_2z3ed_2IT-08b-AUTO-CAPTURE.html#autotoc_md1452", null ],
        [ "Step 2: Add CaptureFailureContext Method (30 minutes)", "d2/d98/md_docs_2z3ed_2IT-08b-AUTO-CAPTURE.html#autotoc_md1453", null ],
        [ "Step 3: Integrate with MarkHarnessTestCompleted (15 minutes)", "d2/d98/md_docs_2z3ed_2IT-08b-AUTO-CAPTURE.html#autotoc_md1454", null ]
      ] ],
      [ "Validation", "d2/d98/md_docs_2z3ed_2IT-08b-AUTO-CAPTURE.html#autotoc_md1456", null ],
      [ "Follow-Up", "d2/d98/md_docs_2z3ed_2IT-08b-AUTO-CAPTURE.html#autotoc_md1458", [
        [ "Query Test Results", "d2/d98/md_docs_2z3ed_2IT-08b-AUTO-CAPTURE.html#autotoc_md1459", null ],
        [ "End-to-End Test Script", "d2/d98/md_docs_2z3ed_2IT-08b-AUTO-CAPTURE.html#autotoc_md1460", null ]
      ] ],
      [ "Success Criteria", "d2/d98/md_docs_2z3ed_2IT-08b-AUTO-CAPTURE.html#autotoc_md1462", null ],
      [ "Files Modified", "d2/d98/md_docs_2z3ed_2IT-08b-AUTO-CAPTURE.html#autotoc_md1464", null ],
      [ "Next Steps", "d2/d98/md_docs_2z3ed_2IT-08b-AUTO-CAPTURE.html#autotoc_md1466", null ]
    ] ],
    [ "IT-10: Collaborative Editing & Multiplayer Sessions", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html", [
      [ "Vision", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1470", null ],
      [ "User Stories", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1472", [
        [ "US-1: Session Host & Join", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1473", null ],
        [ "US-2: Real-Time Edit Synchronization", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1475", null ],
        [ "US-3: Shared AI Agent", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1477", null ],
        [ "US-4: Live Cursors & Annotations", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1479", null ],
        [ "US-5: Session Recording & Replay", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1481", null ]
      ] ],
      [ "Architecture", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1483", [
        [ "Components", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1484", [
          [ "1. Collaboration Server (New)", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1485", null ],
          [ "2. Collaboration Client (New)", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1487", null ],
          [ "3. Edit Event Protocol (New)", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1489", null ],
          [ "4. Conflict Resolution System", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1491", null ],
          [ "5. GUI Integration", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1493", null ]
        ] ]
      ] ],
      [ "CLI Commands", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1495", [
        [ "Session Management", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1496", null ],
        [ "Session Replay", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1497", null ]
      ] ],
      [ "Implementation Plan", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1499", [
        [ "Phase 1: Core Networking (4-5 hours)", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1500", null ],
        [ "Phase 2: Edit Synchronization (3-4 hours)", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1502", null ],
        [ "Phase 3: GUI Integration (2-3 hours)", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1504", null ],
        [ "Phase 4: AI Agent Sharing (2-3 hours)", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1506", null ],
        [ "Phase 5: Session Recording & Replay (1-2 hours)", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1508", null ]
      ] ],
      [ "Security & Safety Considerations", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1510", [
        [ "Authentication", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1511", null ],
        [ "Authorization", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1513", null ],
        [ "Data Integrity", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1515", null ],
        [ "Network Security", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1517", null ]
      ] ],
      [ "Testing Strategy", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1519", [
        [ "Unit Tests", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1520", null ],
        [ "Integration Tests", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1522", null ],
        [ "E2E Tests", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1524", null ]
      ] ],
      [ "Performance Considerations", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1526", [
        [ "Bandwidth Usage", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1527", null ],
        [ "Latency Targets", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1529", null ]
      ] ],
      [ "Future Enhancements", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1531", [
        [ "Voice Chat Integration", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1532", null ],
        [ "Persistent Sessions", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1534", null ],
        [ "Cloud-Hosted Sessions", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1536", null ],
        [ "Integration with Version Control", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1538", null ]
      ] ],
      [ "Success Metrics", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1540", [
        [ "Adoption Metrics", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1541", null ],
        [ "Technical Metrics", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1542", null ],
        [ "User Satisfaction", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1543", null ]
      ] ],
      [ "Risks & Mitigation", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1545", [
        [ "Risk 1: Network Latency", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1546", null ],
        [ "Risk 2: Data Corruption", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1548", null ],
        [ "Risk 3: Security Vulnerabilities", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1550", null ],
        [ "Risk 4: Scalability", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1552", null ]
      ] ],
      [ "Summary", "de/d5d/md_docs_2z3ed_2IT-10-COLLABORATIVE-EDITING.html#autotoc_md1554", null ]
    ] ],
    [ "LLM Integration Implementation Checklist", "d2/d74/md_docs_2z3ed_2LLM-IMPLEMENTATION-CHECKLIST.html", [
      [ "Phase 1: Ollama Local Integration (4-6 hours) ✅ COMPLETE", "d2/d74/md_docs_2z3ed_2LLM-IMPLEMENTATION-CHECKLIST.html#autotoc_md1557", [
        [ "Prerequisites", "d2/d74/md_docs_2z3ed_2LLM-IMPLEMENTATION-CHECKLIST.html#autotoc_md1558", null ],
        [ "Implementation Tasks", "d2/d74/md_docs_2z3ed_2LLM-IMPLEMENTATION-CHECKLIST.html#autotoc_md1559", [
          [ "1.1 Create OllamaAIService Class", "d2/d74/md_docs_2z3ed_2LLM-IMPLEMENTATION-CHECKLIST.html#autotoc_md1560", null ],
          [ "1.2 Update CMake Configuration", "d2/d74/md_docs_2z3ed_2LLM-IMPLEMENTATION-CHECKLIST.html#autotoc_md1561", null ],
          [ "1.3 Wire into Agent Commands", "d2/d74/md_docs_2z3ed_2LLM-IMPLEMENTATION-CHECKLIST.html#autotoc_md1562", null ],
          [ "1.4 Testing & Validation", "d2/d74/md_docs_2z3ed_2LLM-IMPLEMENTATION-CHECKLIST.html#autotoc_md1563", null ]
        ] ],
        [ "Success Criteria", "d2/d74/md_docs_2z3ed_2LLM-IMPLEMENTATION-CHECKLIST.html#autotoc_md1564", null ]
      ] ],
      [ "Phase 2: Improve Gemini Integration (2-3 hours) ✅ COMPLETE", "d2/d74/md_docs_2z3ed_2LLM-IMPLEMENTATION-CHECKLIST.html#autotoc_md1566", [
        [ "Implementation Tasks", "d2/d74/md_docs_2z3ed_2LLM-IMPLEMENTATION-CHECKLIST.html#autotoc_md1567", [
          [ "2.1 Fix GeminiAIService", "d2/d74/md_docs_2z3ed_2LLM-IMPLEMENTATION-CHECKLIST.html#autotoc_md1568", null ],
          [ "2.2 Wire into Service Factory", "d2/d74/md_docs_2z3ed_2LLM-IMPLEMENTATION-CHECKLIST.html#autotoc_md1569", null ],
          [ "2.3 Testing", "d2/d74/md_docs_2z3ed_2LLM-IMPLEMENTATION-CHECKLIST.html#autotoc_md1570", null ]
        ] ],
        [ "Success Criteria", "d2/d74/md_docs_2z3ed_2LLM-IMPLEMENTATION-CHECKLIST.html#autotoc_md1571", null ]
      ] ],
      [ "Phase 3: Add Claude Integration (2-3 hours)", "d2/d74/md_docs_2z3ed_2LLM-IMPLEMENTATION-CHECKLIST.html#autotoc_md1573", [
        [ "Implementation Tasks", "d2/d74/md_docs_2z3ed_2LLM-IMPLEMENTATION-CHECKLIST.html#autotoc_md1574", [
          [ "3.1 Create ClaudeAIService", "d2/d74/md_docs_2z3ed_2LLM-IMPLEMENTATION-CHECKLIST.html#autotoc_md1575", null ],
          [ "3.2 Wire into Service Factory", "d2/d74/md_docs_2z3ed_2LLM-IMPLEMENTATION-CHECKLIST.html#autotoc_md1576", null ],
          [ "3.3 Testing", "d2/d74/md_docs_2z3ed_2LLM-IMPLEMENTATION-CHECKLIST.html#autotoc_md1577", null ]
        ] ],
        [ "Success Criteria", "d2/d74/md_docs_2z3ed_2LLM-IMPLEMENTATION-CHECKLIST.html#autotoc_md1578", null ]
      ] ],
      [ "Phase 4: Enhanced Prompt Engineering (3-4 hours) ✅ COMPLETE", "d2/d74/md_docs_2z3ed_2LLM-IMPLEMENTATION-CHECKLIST.html#autotoc_md1580", [
        [ "Implementation Tasks", "d2/d74/md_docs_2z3ed_2LLM-IMPLEMENTATION-CHECKLIST.html#autotoc_md1581", [
          [ "4.1 Create PromptBuilder Utility", "d2/d74/md_docs_2z3ed_2LLM-IMPLEMENTATION-CHECKLIST.html#autotoc_md1582", null ],
          [ "4.2 Integrate into Services", "d2/d74/md_docs_2z3ed_2LLM-IMPLEMENTATION-CHECKLIST.html#autotoc_md1583", null ],
          [ "4.3 Testing", "d2/d74/md_docs_2z3ed_2LLM-IMPLEMENTATION-CHECKLIST.html#autotoc_md1584", null ]
        ] ],
        [ "Success Criteria", "d2/d74/md_docs_2z3ed_2LLM-IMPLEMENTATION-CHECKLIST.html#autotoc_md1585", null ]
      ] ],
      [ "Configuration & Documentation", "d2/d74/md_docs_2z3ed_2LLM-IMPLEMENTATION-CHECKLIST.html#autotoc_md1587", [
        [ "Environment Variables Setup", "d2/d74/md_docs_2z3ed_2LLM-IMPLEMENTATION-CHECKLIST.html#autotoc_md1588", null ],
        [ "User Documentation", "d2/d74/md_docs_2z3ed_2LLM-IMPLEMENTATION-CHECKLIST.html#autotoc_md1589", null ],
        [ "CLI Enhancements", "d2/d74/md_docs_2z3ed_2LLM-IMPLEMENTATION-CHECKLIST.html#autotoc_md1590", null ]
      ] ],
      [ "Testing Matrix", "d2/d74/md_docs_2z3ed_2LLM-IMPLEMENTATION-CHECKLIST.html#autotoc_md1592", null ],
      [ "Rollout Plan", "d2/d74/md_docs_2z3ed_2LLM-IMPLEMENTATION-CHECKLIST.html#autotoc_md1594", [
        [ "Week 1 (Oct 7-11, 2025)", "d2/d74/md_docs_2z3ed_2LLM-IMPLEMENTATION-CHECKLIST.html#autotoc_md1595", null ],
        [ "Week 2 (Oct 14-18, 2025)", "d2/d74/md_docs_2z3ed_2LLM-IMPLEMENTATION-CHECKLIST.html#autotoc_md1596", null ]
      ] ],
      [ "Known Risks & Mitigation", "d2/d74/md_docs_2z3ed_2LLM-IMPLEMENTATION-CHECKLIST.html#autotoc_md1598", null ],
      [ "Success Metrics", "d2/d74/md_docs_2z3ed_2LLM-IMPLEMENTATION-CHECKLIST.html#autotoc_md1600", null ],
      [ "Next Steps After Completion", "d2/d74/md_docs_2z3ed_2LLM-IMPLEMENTATION-CHECKLIST.html#autotoc_md1602", null ],
      [ "Notes & Observations", "d2/d74/md_docs_2z3ed_2LLM-IMPLEMENTATION-CHECKLIST.html#autotoc_md1604", null ]
    ] ],
    [ "LLM Integration Architecture", "d6/d6a/md_docs_2z3ed_2LLM-INTEGRATION-ARCHITECTURE.html", [
      [ "System Architecture", "d6/d6a/md_docs_2z3ed_2LLM-INTEGRATION-ARCHITECTURE.html#autotoc_md1607", null ],
      [ "LLM Provider Flow", "d6/d6a/md_docs_2z3ed_2LLM-INTEGRATION-ARCHITECTURE.html#autotoc_md1608", [
        [ "Ollama (Local)", "d6/d6a/md_docs_2z3ed_2LLM-INTEGRATION-ARCHITECTURE.html#autotoc_md1609", null ],
        [ "Gemini (Remote)", "d6/d6a/md_docs_2z3ed_2LLM-INTEGRATION-ARCHITECTURE.html#autotoc_md1610", null ],
        [ "Claude (Remote)", "d6/d6a/md_docs_2z3ed_2LLM-INTEGRATION-ARCHITECTURE.html#autotoc_md1611", null ]
      ] ],
      [ "Prompt Engineering Pipeline", "d6/d6a/md_docs_2z3ed_2LLM-INTEGRATION-ARCHITECTURE.html#autotoc_md1612", null ],
      [ "Error Handling & Fallback Chain", "d6/d6a/md_docs_2z3ed_2LLM-INTEGRATION-ARCHITECTURE.html#autotoc_md1613", null ],
      [ "File Structure", "d6/d6a/md_docs_2z3ed_2LLM-INTEGRATION-ARCHITECTURE.html#autotoc_md1614", null ],
      [ "Data Flow Example: \"Make soldier armor red\"", "d6/d6a/md_docs_2z3ed_2LLM-INTEGRATION-ARCHITECTURE.html#autotoc_md1615", null ],
      [ "Comparison Matrix", "d6/d6a/md_docs_2z3ed_2LLM-INTEGRATION-ARCHITECTURE.html#autotoc_md1616", null ],
      [ "Next Steps", "d6/d6a/md_docs_2z3ed_2LLM-INTEGRATION-ARCHITECTURE.html#autotoc_md1617", null ]
    ] ],
    [ "LLM Integration Plan for z3ed Agent System", "de/da5/md_docs_2z3ed_2LLM-INTEGRATION-PLAN.html", [
      [ "Executive Summary", "de/da5/md_docs_2z3ed_2LLM-INTEGRATION-PLAN.html#autotoc_md1620", null ],
      [ "1. Implementation Priorities", "de/da5/md_docs_2z3ed_2LLM-INTEGRATION-PLAN.html#autotoc_md1622", [
        [ "Phase 1: Ollama Local Integration (4-6 hours) 🎯 START HERE", "de/da5/md_docs_2z3ed_2LLM-INTEGRATION-PLAN.html#autotoc_md1623", [
          [ "1.1. Create OllamaAIService Class", "de/da5/md_docs_2z3ed_2LLM-INTEGRATION-PLAN.html#autotoc_md1624", null ],
          [ "1.2. Add CMake Configuration", "de/da5/md_docs_2z3ed_2LLM-INTEGRATION-PLAN.html#autotoc_md1625", null ],
          [ "1.3. Wire into Agent Commands", "de/da5/md_docs_2z3ed_2LLM-INTEGRATION-PLAN.html#autotoc_md1626", null ],
          [ "1.4. Testing & Validation", "de/da5/md_docs_2z3ed_2LLM-INTEGRATION-PLAN.html#autotoc_md1627", null ]
        ] ],
        [ "Phase 2: Improve Gemini Integration (2-3 hours)", "de/da5/md_docs_2z3ed_2LLM-INTEGRATION-PLAN.html#autotoc_md1629", [
          [ "2.1. Fix GeminiAIService Implementation", "de/da5/md_docs_2z3ed_2LLM-INTEGRATION-PLAN.html#autotoc_md1630", null ]
        ] ]
      ] ],
      [ "AI Provider Selection", "de/da5/md_docs_2z3ed_2LLM-INTEGRATION-PLAN.html#autotoc_md1631", null ],
      [ "API Keys (remote providers)", "de/da5/md_docs_2z3ed_2LLM-INTEGRATION-PLAN.html#autotoc_md1632", null ],
      [ "Logging & Debugging", "de/da5/md_docs_2z3ed_2LLM-INTEGRATION-PLAN.html#autotoc_md1633", null ],
      [ "Override provider for single command", "de/da5/md_docs_2z3ed_2LLM-INTEGRATION-PLAN.html#autotoc_md1634", null ],
      [ "Override model", "de/da5/md_docs_2z3ed_2LLM-INTEGRATION-PLAN.html#autotoc_md1635", null ],
      [ "Dry run: show generated commands without executing", "de/da5/md_docs_2z3ed_2LLM-INTEGRATION-PLAN.html#autotoc_md1636", null ],
      [ "Interactive mode: confirm each command before execution", "de/da5/md_docs_2z3ed_2LLM-INTEGRATION-PLAN.html#autotoc_md1637", null ],
      [ "Test 1: Ollama (if available)", "de/da5/md_docs_2z3ed_2LLM-INTEGRATION-PLAN.html#autotoc_md1638", null ],
      [ "Test 2: Gemini (if key set)", "de/da5/md_docs_2z3ed_2LLM-INTEGRATION-PLAN.html#autotoc_md1639", null ],
      [ "Test 3: Claude (if key set)", "de/da5/md_docs_2z3ed_2LLM-INTEGRATION-PLAN.html#autotoc_md1640", null ],
      [ "Setting Up LLM Integration for z3ed", "de/da5/md_docs_2z3ed_2LLM-INTEGRATION-PLAN.html#autotoc_md1641", [
        [ "Quick Start: Ollama (Recommended)", "de/da5/md_docs_2z3ed_2LLM-INTEGRATION-PLAN.html#autotoc_md1642", null ],
        [ "Alternative: Gemini API (Remote)", "de/da5/md_docs_2z3ed_2LLM-INTEGRATION-PLAN.html#autotoc_md1643", null ],
        [ "Alternative: Claude API (Remote)", "de/da5/md_docs_2z3ed_2LLM-INTEGRATION-PLAN.html#autotoc_md1644", null ],
        [ "Troubleshooting", "de/da5/md_docs_2z3ed_2LLM-INTEGRATION-PLAN.html#autotoc_md1645", null ],
        [ "5. Implementation Timeline", "de/da5/md_docs_2z3ed_2LLM-INTEGRATION-PLAN.html#autotoc_md1647", [
          [ "Week 1 (October 7-11)", "de/da5/md_docs_2z3ed_2LLM-INTEGRATION-PLAN.html#autotoc_md1648", null ],
          [ "Week 2 (October 14-18)", "de/da5/md_docs_2z3ed_2LLM-INTEGRATION-PLAN.html#autotoc_md1649", null ]
        ] ],
        [ "6. Success Criteria", "de/da5/md_docs_2z3ed_2LLM-INTEGRATION-PLAN.html#autotoc_md1651", null ],
        [ "7. Future Enhancements (Post-MVP)", "de/da5/md_docs_2z3ed_2LLM-INTEGRATION-PLAN.html#autotoc_md1653", null ],
        [ "Appendix A: Recommended Models", "de/da5/md_docs_2z3ed_2LLM-INTEGRATION-PLAN.html#autotoc_md1655", null ],
        [ "Appendix B: Example Prompts", "de/da5/md_docs_2z3ed_2LLM-INTEGRATION-PLAN.html#autotoc_md1656", null ],
        [ "Next Steps", "de/da5/md_docs_2z3ed_2LLM-INTEGRATION-PLAN.html#autotoc_md1658", null ]
      ] ]
    ] ],
    [ "LLM Integration: Executive Summary & Getting Started", "d5/d60/md_docs_2z3ed_2LLM-INTEGRATION-SUMMARY.html", [
      [ "What Changed?", "d5/d60/md_docs_2z3ed_2LLM-INTEGRATION-SUMMARY.html#autotoc_md1660", null ],
      [ "Why This Matters", "d5/d60/md_docs_2z3ed_2LLM-INTEGRATION-SUMMARY.html#autotoc_md1661", null ],
      [ "What You Get", "d5/d60/md_docs_2z3ed_2LLM-INTEGRATION-SUMMARY.html#autotoc_md1662", null ],
      [ "Implementation Roadmap", "d5/d60/md_docs_2z3ed_2LLM-INTEGRATION-SUMMARY.html#autotoc_md1663", [
        [ "Phase 1: Ollama Integration (4-6 hours) 🎯 START HERE", "d5/d60/md_docs_2z3ed_2LLM-INTEGRATION-SUMMARY.html#autotoc_md1664", null ],
        [ "Phase 2: Gemini Fixes (2-3 hours)", "d5/d60/md_docs_2z3ed_2LLM-INTEGRATION-SUMMARY.html#autotoc_md1665", null ],
        [ "Phase 3: Claude Integration (2-3 hours)", "d5/d60/md_docs_2z3ed_2LLM-INTEGRATION-SUMMARY.html#autotoc_md1666", null ],
        [ "Phase 4: Enhanced Prompting (3-4 hours)", "d5/d60/md_docs_2z3ed_2LLM-INTEGRATION-SUMMARY.html#autotoc_md1667", null ]
      ] ],
      [ "Quick Start (After Implementation)", "d5/d60/md_docs_2z3ed_2LLM-INTEGRATION-SUMMARY.html#autotoc_md1668", [
        [ "For Developers (Implement Now)", "d5/d60/md_docs_2z3ed_2LLM-INTEGRATION-SUMMARY.html#autotoc_md1669", null ],
        [ "For End Users (After Development)", "d5/d60/md_docs_2z3ed_2LLM-INTEGRATION-SUMMARY.html#autotoc_md1670", null ]
      ] ],
      [ "Alternative Providers", "d5/d60/md_docs_2z3ed_2LLM-INTEGRATION-SUMMARY.html#autotoc_md1671", [
        [ "Gemini (Remote, API Key Required)", "d5/d60/md_docs_2z3ed_2LLM-INTEGRATION-SUMMARY.html#autotoc_md1672", null ],
        [ "Claude (Remote, API Key Required)", "d5/d60/md_docs_2z3ed_2LLM-INTEGRATION-SUMMARY.html#autotoc_md1673", null ]
      ] ],
      [ "Documentation Structure", "d5/d60/md_docs_2z3ed_2LLM-INTEGRATION-SUMMARY.html#autotoc_md1674", null ],
      [ "Key Architectural Decisions", "d5/d60/md_docs_2z3ed_2LLM-INTEGRATION-SUMMARY.html#autotoc_md1675", [
        [ "1. Service Interface Pattern", "d5/d60/md_docs_2z3ed_2LLM-INTEGRATION-SUMMARY.html#autotoc_md1676", null ],
        [ "2. Environment-Based Selection", "d5/d60/md_docs_2z3ed_2LLM-INTEGRATION-SUMMARY.html#autotoc_md1677", null ],
        [ "3. Graceful Degradation", "d5/d60/md_docs_2z3ed_2LLM-INTEGRATION-SUMMARY.html#autotoc_md1678", null ],
        [ "4. System Prompt Engineering", "d5/d60/md_docs_2z3ed_2LLM-INTEGRATION-SUMMARY.html#autotoc_md1679", null ]
      ] ],
      [ "Success Metrics", "d5/d60/md_docs_2z3ed_2LLM-INTEGRATION-SUMMARY.html#autotoc_md1680", [
        [ "Phase 1 Complete When:", "d5/d60/md_docs_2z3ed_2LLM-INTEGRATION-SUMMARY.html#autotoc_md1681", null ],
        [ "Full Integration Complete When:", "d5/d60/md_docs_2z3ed_2LLM-INTEGRATION-SUMMARY.html#autotoc_md1682", null ]
      ] ],
      [ "Known Limitations", "d5/d60/md_docs_2z3ed_2LLM-INTEGRATION-SUMMARY.html#autotoc_md1683", [
        [ "Current Implementation", "d5/d60/md_docs_2z3ed_2LLM-INTEGRATION-SUMMARY.html#autotoc_md1684", null ],
        [ "After LLM Integration", "d5/d60/md_docs_2z3ed_2LLM-INTEGRATION-SUMMARY.html#autotoc_md1685", null ]
      ] ],
      [ "FAQ", "d5/d60/md_docs_2z3ed_2LLM-INTEGRATION-SUMMARY.html#autotoc_md1686", [
        [ "Why Ollama first?", "d5/d60/md_docs_2z3ed_2LLM-INTEGRATION-SUMMARY.html#autotoc_md1687", null ],
        [ "Why not OpenAI?", "d5/d60/md_docs_2z3ed_2LLM-INTEGRATION-SUMMARY.html#autotoc_md1688", null ],
        [ "Can I use multiple providers?", "d5/d60/md_docs_2z3ed_2LLM-INTEGRATION-SUMMARY.html#autotoc_md1689", null ],
        [ "What if I don't want to use AI?", "d5/d60/md_docs_2z3ed_2LLM-INTEGRATION-SUMMARY.html#autotoc_md1690", null ]
      ] ],
      [ "Next Steps", "d5/d60/md_docs_2z3ed_2LLM-INTEGRATION-SUMMARY.html#autotoc_md1691", [
        [ "For @scawful (Project Owner)", "d5/d60/md_docs_2z3ed_2LLM-INTEGRATION-SUMMARY.html#autotoc_md1692", null ],
        [ "For Contributors", "d5/d60/md_docs_2z3ed_2LLM-INTEGRATION-SUMMARY.html#autotoc_md1693", null ],
        [ "For Users (Future)", "d5/d60/md_docs_2z3ed_2LLM-INTEGRATION-SUMMARY.html#autotoc_md1694", null ]
      ] ],
      [ "Timeline", "d5/d60/md_docs_2z3ed_2LLM-INTEGRATION-SUMMARY.html#autotoc_md1695", null ],
      [ "Related Documents", "d5/d60/md_docs_2z3ed_2LLM-INTEGRATION-SUMMARY.html#autotoc_md1696", null ],
      [ "Questions?", "d5/d60/md_docs_2z3ed_2LLM-INTEGRATION-SUMMARY.html#autotoc_md1697", null ]
    ] ],
    [ "LLM Integration Progress Update", "d9/df7/md_docs_2z3ed_2LLM-PROGRESS-UPDATE.html", [
      [ "🎉 Major Milestones", "d9/df7/md_docs_2z3ed_2LLM-PROGRESS-UPDATE.html#autotoc_md1700", [
        [ "✅ Phase 1: Ollama Local Integration (COMPLETE)", "d9/df7/md_docs_2z3ed_2LLM-PROGRESS-UPDATE.html#autotoc_md1701", null ],
        [ "✅ Phase 2: Gemini Integration Enhancement (COMPLETE)", "d9/df7/md_docs_2z3ed_2LLM-PROGRESS-UPDATE.html#autotoc_md1702", null ]
      ] ],
      [ "📊 Progress Overview", "d9/df7/md_docs_2z3ed_2LLM-PROGRESS-UPDATE.html#autotoc_md1703", [
        [ "Completed (6-8 hours of work)", "d9/df7/md_docs_2z3ed_2LLM-PROGRESS-UPDATE.html#autotoc_md1704", null ],
        [ "Remaining Work (6-7 hours)", "d9/df7/md_docs_2z3ed_2LLM-PROGRESS-UPDATE.html#autotoc_md1705", null ]
      ] ],
      [ "🏗️ Architecture Summary", "d9/df7/md_docs_2z3ed_2LLM-PROGRESS-UPDATE.html#autotoc_md1706", [
        [ "Service Layer", "d9/df7/md_docs_2z3ed_2LLM-PROGRESS-UPDATE.html#autotoc_md1707", null ],
        [ "Service Factory", "d9/df7/md_docs_2z3ed_2LLM-PROGRESS-UPDATE.html#autotoc_md1708", null ],
        [ "Environment Variables", "d9/df7/md_docs_2z3ed_2LLM-PROGRESS-UPDATE.html#autotoc_md1709", null ]
      ] ],
      [ "🧪 Testing Status", "d9/df7/md_docs_2z3ed_2LLM-PROGRESS-UPDATE.html#autotoc_md1710", [
        [ "Phase 1 (Ollama) Tests", "d9/df7/md_docs_2z3ed_2LLM-PROGRESS-UPDATE.html#autotoc_md1711", null ],
        [ "Phase 2 (Gemini) Tests", "d9/df7/md_docs_2z3ed_2LLM-PROGRESS-UPDATE.html#autotoc_md1712", null ]
      ] ],
      [ "📈 Quality Metrics", "d9/df7/md_docs_2z3ed_2LLM-PROGRESS-UPDATE.html#autotoc_md1713", [
        [ "Code Quality", "d9/df7/md_docs_2z3ed_2LLM-PROGRESS-UPDATE.html#autotoc_md1714", null ],
        [ "Architecture Quality", "d9/df7/md_docs_2z3ed_2LLM-PROGRESS-UPDATE.html#autotoc_md1715", null ]
      ] ],
      [ "🚀 Next Steps", "d9/df7/md_docs_2z3ed_2LLM-PROGRESS-UPDATE.html#autotoc_md1716", [
        [ "Option A: Validate Existing Work (Recommended)", "d9/df7/md_docs_2z3ed_2LLM-PROGRESS-UPDATE.html#autotoc_md1717", null ],
        [ "Option B: Continue to Phase 3 (Claude)", "d9/df7/md_docs_2z3ed_2LLM-PROGRESS-UPDATE.html#autotoc_md1718", null ],
        [ "Option C: Jump to Phase 4 (Enhanced Prompting)", "d9/df7/md_docs_2z3ed_2LLM-PROGRESS-UPDATE.html#autotoc_md1719", null ]
      ] ],
      [ "💡 Recommendations", "d9/df7/md_docs_2z3ed_2LLM-PROGRESS-UPDATE.html#autotoc_md1720", [
        [ "Immediate Priorities", "d9/df7/md_docs_2z3ed_2LLM-PROGRESS-UPDATE.html#autotoc_md1721", null ],
        [ "Long-Term Improvements", "d9/df7/md_docs_2z3ed_2LLM-PROGRESS-UPDATE.html#autotoc_md1722", null ]
      ] ],
      [ "📝 Files Changed Summary", "d9/df7/md_docs_2z3ed_2LLM-PROGRESS-UPDATE.html#autotoc_md1723", [
        [ "New Files (14 files)", "d9/df7/md_docs_2z3ed_2LLM-PROGRESS-UPDATE.html#autotoc_md1724", null ],
        [ "Modified Files (5 files)", "d9/df7/md_docs_2z3ed_2LLM-PROGRESS-UPDATE.html#autotoc_md1725", null ]
      ] ],
      [ "🎯 Session Summary", "d9/df7/md_docs_2z3ed_2LLM-PROGRESS-UPDATE.html#autotoc_md1726", null ]
    ] ],
    [ "Phase 1 Implementation Complete! 🎉", "d1/d18/md_docs_2z3ed_2PHASE1-COMPLETE.html", [
      [ "What Was Implemented", "d1/d18/md_docs_2z3ed_2PHASE1-COMPLETE.html#autotoc_md1729", [
        [ "1. OllamaAIService Class ✅", "d1/d18/md_docs_2z3ed_2PHASE1-COMPLETE.html#autotoc_md1730", null ],
        [ "2. Service Factory Pattern ✅", "d1/d18/md_docs_2z3ed_2PHASE1-COMPLETE.html#autotoc_md1731", null ],
        [ "3. Build System Integration ✅", "d1/d18/md_docs_2z3ed_2PHASE1-COMPLETE.html#autotoc_md1732", null ],
        [ "4. Testing Infrastructure ✅", "d1/d18/md_docs_2z3ed_2PHASE1-COMPLETE.html#autotoc_md1733", null ]
      ] ],
      [ "Current System State", "d1/d18/md_docs_2z3ed_2PHASE1-COMPLETE.html#autotoc_md1734", [
        [ "What Works Now", "d1/d18/md_docs_2z3ed_2PHASE1-COMPLETE.html#autotoc_md1735", null ]
      ] ],
      [ "Testing Results", "d1/d18/md_docs_2z3ed_2PHASE1-COMPLETE.html#autotoc_md1736", [
        [ "Build Status: ✅ PASS", "d1/d18/md_docs_2z3ed_2PHASE1-COMPLETE.html#autotoc_md1737", null ],
        [ "Runtime Status: ✅ PASS", "d1/d18/md_docs_2z3ed_2PHASE1-COMPLETE.html#autotoc_md1738", null ],
        [ "Integration Status: 🟡 READY FOR OLLAMA", "d1/d18/md_docs_2z3ed_2PHASE1-COMPLETE.html#autotoc_md1739", null ]
      ] ],
      [ "What's Next (To Use With Ollama)", "d1/d18/md_docs_2z3ed_2PHASE1-COMPLETE.html#autotoc_md1740", [
        [ "User Setup (5 minutes)", "d1/d18/md_docs_2z3ed_2PHASE1-COMPLETE.html#autotoc_md1741", null ],
        [ "Developer Next Steps", "d1/d18/md_docs_2z3ed_2PHASE1-COMPLETE.html#autotoc_md1742", null ]
      ] ],
      [ "Code Quality", "d1/d18/md_docs_2z3ed_2PHASE1-COMPLETE.html#autotoc_md1743", [
        [ "Architecture ✅", "d1/d18/md_docs_2z3ed_2PHASE1-COMPLETE.html#autotoc_md1744", null ],
        [ "Error Handling ✅", "d1/d18/md_docs_2z3ed_2PHASE1-COMPLETE.html#autotoc_md1745", null ],
        [ "User Experience ✅", "d1/d18/md_docs_2z3ed_2PHASE1-COMPLETE.html#autotoc_md1746", null ]
      ] ],
      [ "Documentation Status", "d1/d18/md_docs_2z3ed_2PHASE1-COMPLETE.html#autotoc_md1747", [
        [ "Created ✅", "d1/d18/md_docs_2z3ed_2PHASE1-COMPLETE.html#autotoc_md1748", null ],
        [ "Updated ✅", "d1/d18/md_docs_2z3ed_2PHASE1-COMPLETE.html#autotoc_md1749", null ],
        [ "Scripts ✅", "d1/d18/md_docs_2z3ed_2PHASE1-COMPLETE.html#autotoc_md1750", null ]
      ] ],
      [ "Key Achievements", "d1/d18/md_docs_2z3ed_2PHASE1-COMPLETE.html#autotoc_md1751", null ],
      [ "Known Limitations", "d1/d18/md_docs_2z3ed_2PHASE1-COMPLETE.html#autotoc_md1752", null ],
      [ "Comparison to Plan", "d1/d18/md_docs_2z3ed_2PHASE1-COMPLETE.html#autotoc_md1753", [
        [ "Original Estimate: 4-6 hours", "d1/d18/md_docs_2z3ed_2PHASE1-COMPLETE.html#autotoc_md1754", null ],
        [ "Actual Time: ~45 minutes", "d1/d18/md_docs_2z3ed_2PHASE1-COMPLETE.html#autotoc_md1755", null ],
        [ "Why Faster?", "d1/d18/md_docs_2z3ed_2PHASE1-COMPLETE.html#autotoc_md1756", null ],
        [ "What Helped:", "d1/d18/md_docs_2z3ed_2PHASE1-COMPLETE.html#autotoc_md1757", null ]
      ] ],
      [ "Verification Commands", "d1/d18/md_docs_2z3ed_2PHASE1-COMPLETE.html#autotoc_md1758", null ],
      [ "Next Action", "d1/d18/md_docs_2z3ed_2PHASE1-COMPLETE.html#autotoc_md1759", null ],
      [ "Checklist Update", "d1/d18/md_docs_2z3ed_2PHASE1-COMPLETE.html#autotoc_md1761", [
        [ "Phase 1: Ollama Local Integration ✅", "d1/d18/md_docs_2z3ed_2PHASE1-COMPLETE.html#autotoc_md1762", null ],
        [ "Pending (Requires Ollama Installation)", "d1/d18/md_docs_2z3ed_2PHASE1-COMPLETE.html#autotoc_md1763", null ]
      ] ]
    ] ],
    [ "Phase 2 Complete: Gemini AI Service Enhancement", "d0/d18/md_docs_2z3ed_2PHASE2-COMPLETE.html", [
      [ "Overview", "d0/d18/md_docs_2z3ed_2PHASE2-COMPLETE.html#autotoc_md1766", null ],
      [ "Objectives Completed", "d0/d18/md_docs_2z3ed_2PHASE2-COMPLETE.html#autotoc_md1767", [
        [ "1. ✅ Enhanced Configuration System", "d0/d18/md_docs_2z3ed_2PHASE2-COMPLETE.html#autotoc_md1768", null ],
        [ "2. ✅ Improved System Prompt", "d0/d18/md_docs_2z3ed_2PHASE2-COMPLETE.html#autotoc_md1769", null ],
        [ "3. ✅ Added Health Check System", "d0/d18/md_docs_2z3ed_2PHASE2-COMPLETE.html#autotoc_md1770", null ],
        [ "4. ✅ Enhanced JSON Parsing", "d0/d18/md_docs_2z3ed_2PHASE2-COMPLETE.html#autotoc_md1771", null ]
      ] ],
      [ "Auto-detect from GEMINI_API_KEY", "d0/d18/md_docs_2z3ed_2PHASE2-COMPLETE.html#autotoc_md1772", null ],
      [ "Use Pro model for complex tasks", "d0/d18/md_docs_2z3ed_2PHASE2-COMPLETE.html#autotoc_md1773", null ],
      [ "Run comprehensive tests (requires API key)", "d0/d18/md_docs_2z3ed_2PHASE2-COMPLETE.html#autotoc_md1774", [
        [ "Success Metrics", "d0/d18/md_docs_2z3ed_2PHASE2-COMPLETE.html#autotoc_md1777", [
          [ "Phase 3 Preview (Claude Integration)", "d0/d18/md_docs_2z3ed_2PHASE2-COMPLETE.html#autotoc_md1775", null ],
          [ "Phase 4 Preview (Enhanced Prompting)", "d0/d18/md_docs_2z3ed_2PHASE2-COMPLETE.html#autotoc_md1776", null ],
          [ "Code Quality", "d0/d18/md_docs_2z3ed_2PHASE2-COMPLETE.html#autotoc_md1778", null ],
          [ "Functionality", "d0/d18/md_docs_2z3ed_2PHASE2-COMPLETE.html#autotoc_md1779", null ],
          [ "Architecture", "d0/d18/md_docs_2z3ed_2PHASE2-COMPLETE.html#autotoc_md1780", null ]
        ] ],
        [ "Conclusion", "d0/d18/md_docs_2z3ed_2PHASE2-COMPLETE.html#autotoc_md1781", null ]
      ] ]
    ] ],
    [ "Phase 2 Validation Results", "da/df3/md_docs_2z3ed_2PHASE2-VALIDATION-RESULTS.html", [
      [ "Test Execution Summary", "da/df3/md_docs_2z3ed_2PHASE2-VALIDATION-RESULTS.html#autotoc_md1784", [
        [ "Environment", "da/df3/md_docs_2z3ed_2PHASE2-VALIDATION-RESULTS.html#autotoc_md1785", null ],
        [ "Test Results", "da/df3/md_docs_2z3ed_2PHASE2-VALIDATION-RESULTS.html#autotoc_md1786", [
          [ "Test 1: Simple Palette Color Change", "da/df3/md_docs_2z3ed_2PHASE2-VALIDATION-RESULTS.html#autotoc_md1787", null ],
          [ "Test 2: Overworld Tile Placement", "da/df3/md_docs_2z3ed_2PHASE2-VALIDATION-RESULTS.html#autotoc_md1789", null ],
          [ "Test 3: Multi-Step Task", "da/df3/md_docs_2z3ed_2PHASE2-VALIDATION-RESULTS.html#autotoc_md1791", null ],
          [ "Test 4: Direct Run Command", "da/df3/md_docs_2z3ed_2PHASE2-VALIDATION-RESULTS.html#autotoc_md1793", null ]
        ] ]
      ] ],
      [ "Overall Assessment", "da/df3/md_docs_2z3ed_2PHASE2-VALIDATION-RESULTS.html#autotoc_md1795", [
        [ "Strengths", "da/df3/md_docs_2z3ed_2PHASE2-VALIDATION-RESULTS.html#autotoc_md1796", null ],
        [ "Issues Found", "da/df3/md_docs_2z3ed_2PHASE2-VALIDATION-RESULTS.html#autotoc_md1797", null ],
        [ "Performance Metrics", "da/df3/md_docs_2z3ed_2PHASE2-VALIDATION-RESULTS.html#autotoc_md1798", null ],
        [ "Comparison with MockAIService", "da/df3/md_docs_2z3ed_2PHASE2-VALIDATION-RESULTS.html#autotoc_md1799", null ]
      ] ],
      [ "Recommendations", "da/df3/md_docs_2z3ed_2PHASE2-VALIDATION-RESULTS.html#autotoc_md1801", [
        [ "Immediate Actions", "da/df3/md_docs_2z3ed_2PHASE2-VALIDATION-RESULTS.html#autotoc_md1802", null ],
        [ "Next Steps", "da/df3/md_docs_2z3ed_2PHASE2-VALIDATION-RESULTS.html#autotoc_md1803", null ]
      ] ],
      [ "Sign-off", "da/df3/md_docs_2z3ed_2PHASE2-VALIDATION-RESULTS.html#autotoc_md1805", null ]
    ] ],
    [ "Phase 4 Complete: Enhanced Prompt Engineering", "d2/d0f/md_docs_2z3ed_2PHASE4-COMPLETE.html", [
      [ "Overview", "d2/d0f/md_docs_2z3ed_2PHASE4-COMPLETE.html#autotoc_md1808", null ],
      [ "Objectives Completed", "d2/d0f/md_docs_2z3ed_2PHASE4-COMPLETE.html#autotoc_md1809", [
        [ "1. ✅ Created PromptBuilder Utility Class", "d2/d0f/md_docs_2z3ed_2PHASE4-COMPLETE.html#autotoc_md1810", null ],
        [ "2. ✅ Implemented Few-Shot Learning", "d2/d0f/md_docs_2z3ed_2PHASE4-COMPLETE.html#autotoc_md1811", [
          [ "Palette Manipulation", "d2/d0f/md_docs_2z3ed_2PHASE4-COMPLETE.html#autotoc_md1812", null ],
          [ "Overworld Modification", "d2/d0f/md_docs_2z3ed_2PHASE4-COMPLETE.html#autotoc_md1813", null ],
          [ "Multi-Step Tasks", "d2/d0f/md_docs_2z3ed_2PHASE4-COMPLETE.html#autotoc_md1814", null ]
        ] ],
        [ "3. ✅ Comprehensive Command Documentation", "d2/d0f/md_docs_2z3ed_2PHASE4-COMPLETE.html#autotoc_md1815", null ],
        [ "4. ✅ Added Tile ID Reference", "d2/d0f/md_docs_2z3ed_2PHASE4-COMPLETE.html#autotoc_md1816", null ],
        [ "5. ✅ Implemented Constraints Section", "d2/d0f/md_docs_2z3ed_2PHASE4-COMPLETE.html#autotoc_md1817", null ]
      ] ],
      [ "Enhanced prompting enabled by default", "d2/d0f/md_docs_2z3ed_2PHASE4-COMPLETE.html#autotoc_md1818", null ],
      [ "Test with enhanced prompting", "d2/d0f/md_docs_2z3ed_2PHASE4-COMPLETE.html#autotoc_md1819", [
        [ "Performance Impact", "d2/d0f/md_docs_2z3ed_2PHASE4-COMPLETE.html#autotoc_md1820", [
          [ "Token Usage", "d2/d0f/md_docs_2z3ed_2PHASE4-COMPLETE.html#autotoc_md1821", null ],
          [ "Cost Impact", "d2/d0f/md_docs_2z3ed_2PHASE4-COMPLETE.html#autotoc_md1822", null ],
          [ "Response Time", "d2/d0f/md_docs_2z3ed_2PHASE4-COMPLETE.html#autotoc_md1823", null ]
        ] ],
        [ "Success Metrics", "d2/d0f/md_docs_2z3ed_2PHASE4-COMPLETE.html#autotoc_md1824", [
          [ "Code Quality", "d2/d0f/md_docs_2z3ed_2PHASE4-COMPLETE.html#autotoc_md1825", null ],
          [ "Functionality", "d2/d0f/md_docs_2z3ed_2PHASE4-COMPLETE.html#autotoc_md1826", null ],
          [ "Expected Outcomes", "d2/d0f/md_docs_2z3ed_2PHASE4-COMPLETE.html#autotoc_md1827", null ]
        ] ],
        [ "Conclusion", "d2/d0f/md_docs_2z3ed_2PHASE4-COMPLETE.html#autotoc_md1828", null ]
      ] ]
    ] ],
    [ "z3ed Quick Reference Card", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html", [
      [ "Build & Setup", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1832", [
        [ "Build with gRPC Support", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1833", null ],
        [ "Start Test Harness", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1834", null ]
      ] ],
      [ "CLI Commands", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1836", [
        [ "Agent Workflow", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1837", [
          [ "Create Proposal", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1838", null ],
          [ "List Proposals", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1839", null ],
          [ "View Diff", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1840", null ],
          [ "Review in GUI", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1841", null ],
          [ "Export API Schema", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1842", null ]
        ] ],
        [ "Agent Testing (IT-02)", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1843", [
          [ "Run Natural Language Test", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1844", null ],
          [ "Test Introspection (IT-05) 🔜 PLANNED", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1845", null ],
          [ "Widget Discovery (IT-06) � IN PROGRESS — telemetry available", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1846", null ],
          [ "Test Recording (IT-07) 🔜 PLANNED", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1847", null ]
        ] ],
        [ "ROM Commands", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1848", null ],
        [ "Palette Commands", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1849", null ],
        [ "Overworld Commands", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1850", null ],
        [ "Dungeon Commands", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1851", null ]
      ] ],
      [ "gRPC Testing with grpcurl", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1853", [
        [ "Setup", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1854", null ],
        [ "Core RPCs", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1855", [
          [ "Ping (Health Check)", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1856", null ],
          [ "Click", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1857", null ],
          [ "Type", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1858", null ],
          [ "Wait", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1859", null ],
          [ "Assert", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1860", null ],
          [ "Screenshot (Stub)", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1861", null ]
        ] ]
      ] ],
      [ "E2E Testing", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1863", [
        [ "Run Full Test Suite", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1864", null ],
        [ "Manual Workflow Test", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1865", null ]
      ] ],
      [ "Troubleshooting", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1867", [
        [ "Port Already in Use", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1868", null ],
        [ "Connection Refused", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1869", null ],
        [ "Widget Not Found", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1870", null ],
        [ "Build Errors", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1871", null ]
      ] ],
      [ "File Locations", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1873", [
        [ "Core Files", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1874", null ],
        [ "Build Artifacts", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1875", null ]
      ] ],
      [ "Environment Variables", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1877", null ],
      [ "Platform Support", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1879", null ],
      [ "Next Steps", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1881", null ],
      [ "Resources", "dd/d83/md_docs_2z3ed_2QUICK__REFERENCE.html#autotoc_md1883", null ]
    ] ],
    [ "z3ed: AI-Powered CLI for YAZE", "d3/d63/md_docs_2z3ed_2README.html", [
      [ "Overview", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1886", null ],
      [ "Core Documentation", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1887", null ],
      [ "Quick Start", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1888", [
        [ "Build z3ed", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1889", null ],
        [ "Common Commands", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1890", null ]
      ] ],
      [ "Recent Enhancements", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1891", null ],
      [ "Quick Navigation", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1892", null ]
    ] ],
    [ "Testing Gemini Integration", "d3/def/md_docs_2z3ed_2TESTING-GEMINI.html", [
      [ "Quick Test", "d3/def/md_docs_2z3ed_2TESTING-GEMINI.html#autotoc_md1894", null ],
      [ "Individual Command Tests", "d3/def/md_docs_2z3ed_2TESTING-GEMINI.html#autotoc_md1895", null ],
      [ "What to Look For", "d3/def/md_docs_2z3ed_2TESTING-GEMINI.html#autotoc_md1896", null ],
      [ "Expected Output Example", "d3/def/md_docs_2z3ed_2TESTING-GEMINI.html#autotoc_md1897", null ],
      [ "Troubleshooting", "d3/def/md_docs_2z3ed_2TESTING-GEMINI.html#autotoc_md1898", null ],
      [ "Running the Full Test Suite", "d3/def/md_docs_2z3ed_2TESTING-GEMINI.html#autotoc_md1899", null ],
      [ "What We're Testing", "d3/def/md_docs_2z3ed_2TESTING-GEMINI.html#autotoc_md1900", null ],
      [ "After Testing", "d3/def/md_docs_2z3ed_2TESTING-GEMINI.html#autotoc_md1901", null ]
    ] ],
    [ "yaze - Yet Another Zelda3 Editor", "d0/d30/md_README.html", [
      [ "Version 0.3.2 - Release", "d0/d30/md_README.html#autotoc_md1903", [
        [ "🛠️ Technical Improvements", "d0/d30/md_README.html#autotoc_md1907", null ]
      ] ],
      [ "Quick Start", "d0/d30/md_README.html#autotoc_md1908", [
        [ "Build", "d0/d30/md_README.html#autotoc_md1909", null ],
        [ "Applications", "d0/d30/md_README.html#autotoc_md1910", null ]
      ] ],
      [ "Usage", "d0/d30/md_README.html#autotoc_md1911", [
        [ "GUI Editor", "d0/d30/md_README.html#autotoc_md1912", null ],
        [ "Command Line Tool", "d0/d30/md_README.html#autotoc_md1913", null ],
        [ "C++ API", "d0/d30/md_README.html#autotoc_md1914", null ]
      ] ],
      [ "Documentation", "d0/d30/md_README.html#autotoc_md1915", null ],
      [ "Supported Platforms", "d0/d30/md_README.html#autotoc_md1916", null ],
      [ "ROM Compatibility", "d0/d30/md_README.html#autotoc_md1917", null ],
      [ "Contributing", "d0/d30/md_README.html#autotoc_md1918", null ],
      [ "License", "d0/d30/md_README.html#autotoc_md1919", null ],
      [ "🙏 Acknowledgments", "d0/d30/md_README.html#autotoc_md1920", null ],
      [ "📸 Screenshots", "d0/d30/md_README.html#autotoc_md1921", null ]
    ] ],
    [ "yaze Build Scripts", "de/d82/md_scripts_2README.html", [
      [ "Windows Scripts", "de/d82/md_scripts_2README.html#autotoc_md1924", [
        [ "vcpkg Setup (Optional)", "de/d82/md_scripts_2README.html#autotoc_md1925", null ]
      ] ],
      [ "Windows Build Workflow", "de/d82/md_scripts_2README.html#autotoc_md1926", [
        [ "Recommended: Visual Studio CMake Mode", "de/d82/md_scripts_2README.html#autotoc_md1927", null ],
        [ "Command Line Build", "de/d82/md_scripts_2README.html#autotoc_md1928", null ],
        [ "Compiler Notes", "de/d82/md_scripts_2README.html#autotoc_md1929", null ]
      ] ],
      [ "Quick Start (Windows)", "de/d82/md_scripts_2README.html#autotoc_md1930", [
        [ "Option 1: Visual Studio (Recommended)", "de/d82/md_scripts_2README.html#autotoc_md1931", null ],
        [ "Option 2: Command Line", "de/d82/md_scripts_2README.html#autotoc_md1932", null ],
        [ "Option 3: With vcpkg (Optional)", "de/d82/md_scripts_2README.html#autotoc_md1933", null ]
      ] ],
      [ "Troubleshooting", "de/d82/md_scripts_2README.html#autotoc_md1934", [
        [ "Common Issues", "de/d82/md_scripts_2README.html#autotoc_md1935", null ],
        [ "Getting Help", "de/d82/md_scripts_2README.html#autotoc_md1936", null ]
      ] ],
      [ "Other Scripts", "de/d82/md_scripts_2README.html#autotoc_md1937", null ]
    ] ],
    [ "YAZE Build Environment Verification Scripts", "dc/db4/md_scripts_2README__VERIFICATION.html", [
      [ "Quick Start", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1939", [
        [ "Verify Build Environment", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1940", null ]
      ] ],
      [ "Scripts Overview", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1941", [
        [ "<tt>verify-build-environment.ps1</tt> / <tt>.sh</tt>", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1942", [
          [ "Usage", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1943", null ],
          [ "Exit Codes", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1944", null ]
        ] ]
      ] ],
      [ "Common Workflows", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1945", [
        [ "First-Time Setup", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1946", null ],
        [ "After Pulling Changes", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1947", null ],
        [ "Troubleshooting Build Issues", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1948", null ],
        [ "Before Opening Pull Request", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1949", null ]
      ] ],
      [ "Automatic Fixes", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1950", [
        [ "Always Auto-Fixed (No Confirmation Required)", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1951", null ],
        [ "Fixed with <tt>-FixIssues</tt> / <tt>--fix</tt> Flag", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1952", null ],
        [ "Fixed with <tt>-CleanCache</tt> / <tt>--clean</tt> Flag", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1953", null ],
        [ "Optional Verbose Tests", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1954", null ]
      ] ],
      [ "Integration with Visual Studio", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1955", null ],
      [ "What Gets Checked", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1956", [
        [ "CMake (Required)", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1957", null ],
        [ "Git (Required)", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1958", null ],
        [ "Compilers (Required)", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1959", null ],
        [ "Platform Dependencies", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1960", null ],
        [ "CMake Cache", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1961", null ],
        [ "Dependencies", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1962", null ]
      ] ],
      [ "Automatic Fixes", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1963", null ],
      [ "CI/CD Integration", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1964", null ],
      [ "Troubleshooting", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1965", [
        [ "Script Reports \"CMake Not Found\"", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1966", null ],
        [ "\"Git Submodules Missing\"", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1967", null ],
        [ "\"CMake Cache Too Old\"", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1968", null ],
        [ "\"Visual Studio Not Found\" (Windows)", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1969", null ],
        [ "Script Fails on Network Issues (gRPC)", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1970", null ]
      ] ],
      [ "See Also", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1971", null ]
    ] ],
    [ "yaze Test Suite", "d0/d46/md_test_2README.html", [
      [ "Directory Structure", "d0/d46/md_test_2README.html#autotoc_md1973", null ],
      [ "Test Categories", "d0/d46/md_test_2README.html#autotoc_md1974", [
        [ "Unit Tests (<tt>unit/</tt>)", "d0/d46/md_test_2README.html#autotoc_md1975", null ],
        [ "Integration Tests (<tt>integration/</tt>)", "d0/d46/md_test_2README.html#autotoc_md1976", null ],
        [ "End-to-End Tests (<tt>e2e/</tt>)", "d0/d46/md_test_2README.html#autotoc_md1977", null ]
      ] ],
      [ "Enhanced Test Runner", "d0/d46/md_test_2README.html#autotoc_md1978", [
        [ "Usage Examples", "d0/d46/md_test_2README.html#autotoc_md1979", null ],
        [ "Test Modes", "d0/d46/md_test_2README.html#autotoc_md1980", null ],
        [ "Options", "d0/d46/md_test_2README.html#autotoc_md1981", null ]
      ] ],
      [ "E2E ROM Testing", "d0/d46/md_test_2README.html#autotoc_md1982", [
        [ "Test Cases", "d0/d46/md_test_2README.html#autotoc_md1983", null ]
      ] ],
      [ "ZSCustomOverworld Upgrade Testing", "d0/d46/md_test_2README.html#autotoc_md1984", [
        [ "Supported Upgrades", "d0/d46/md_test_2README.html#autotoc_md1985", null ],
        [ "Test Cases", "d0/d46/md_test_2README.html#autotoc_md1986", null ],
        [ "Version-Specific Features", "d0/d46/md_test_2README.html#autotoc_md1987", [
          [ "Vanilla", "d0/d46/md_test_2README.html#autotoc_md1988", null ],
          [ "v2", "d0/d46/md_test_2README.html#autotoc_md1989", null ],
          [ "v3", "d0/d46/md_test_2README.html#autotoc_md1990", null ]
        ] ]
      ] ],
      [ "Environment Variables", "d0/d46/md_test_2README.html#autotoc_md1991", null ],
      [ "CI/CD Integration", "d0/d46/md_test_2README.html#autotoc_md1992", null ],
      [ "Deprecated Tests", "d0/d46/md_test_2README.html#autotoc_md1993", null ],
      [ "Best Practices", "d0/d46/md_test_2README.html#autotoc_md1994", null ],
      [ "AI Agent Testing", "d0/d46/md_test_2README.html#autotoc_md1995", null ]
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
"d0/d27/namespaceyaze_1_1gfx.html#a89c37f0c4b0e5c24a13a6078163f4c3c",
"d0/d3d/structyaze_1_1cli_1_1OllamaConfig.html#ae1bf7f1a0f72507b5e4da8794caece1f",
"d0/d85/structyaze_1_1cli_1_1WidgetDescriptor.html#a4145c2233bfb8fc6d3feb87ebe528ade",
"d0/ddd/classyaze_1_1test_1_1TestEditor.html#a4235b48ee094a0ef8cced6778c7a12d7",
"d1/d22/classyaze_1_1editor_1_1DungeonObjectInteraction.html#a1310b83229a46724a8526470982c2d14",
"d1/d3e/namespaceyaze_1_1editor.html#a473fd190fc829bc2a054131640c2f620",
"d1/d4f/overworld__map_8h.html#a607408467c46951f2ab11ba3c694ca3da92676a91b88790d2bd6f6647442d42ff",
"d1/dac/namespaceyaze_1_1editor_1_1test.html#a464457653df1793cb45979fd706c4be0",
"d1/de2/classyaze_1_1test_1_1ZSCustomOverworldUpgradeTest.html#af0e94bcf0ab7f7943c6a8a16ef99d520",
"d2/d07/classyaze_1_1emu_1_1Spc700.html#a3228433d9f4638dcdab9912dd09ac67a",
"d2/d11/structyaze_1_1editor_1_1Toast.html#a4f29c7e82ca84c9f0eb3293e33b8fa17",
"d2/d4f/structyaze_1_1emu_1_1OPHCT.html#af9ad55ebb5ffa645dc10f1763cbb0914",
"d2/d94/group__rom__types.html#gga9b154d4f0904082e537a24b5ae25575dabc527f6cbb32739b65a293d12630647e",
"d2/dd9/structyaze_1_1zelda3_1_1DungeonEditorSystem_1_1EditorState.html#a157ecd74174f708e595b32ddf9f5ba79",
"d2/df9/ppu_8h.html#ab2d8079bb5330aedfb062604d2a0168a",
"d3/d15/classyaze_1_1emu_1_1Snes.html#a13d98a97fa3608fb26f6063aa0685b8a",
"d3/d21/classyaze_1_1editor_1_1EditorSet.html#a3b82c2833c1ce552c2335f74013536f2",
"d3/d44/classyaze_1_1editor_1_1OverworldEditor.html#a218e2870a41d305882b97d32c287a1e4",
"d3/d44/classyaze_1_1editor_1_1OverworldEditor.html#aea8267c4f7c87f90980a353edb9d27a1a859698b56b70e1a3dde0cc39cc00af89",
"d3/d6a/classyaze_1_1zelda3_1_1RoomObject.html#af4be44e2e6210a18098d18f78d7a5ee7",
"d3/d8d/classyaze_1_1editor_1_1PopupManager.html#a0961e701273476f3e83fa6d37024526d",
"d3/d9f/classyaze_1_1editor_1_1Editor.html#a127b4d84d1a76c5af2998a1a30f45e95",
"d3/dbf/namespaceyaze_1_1gui.html#ab950f3133e3cb956c3c76733c0e16909",
"d3/ded/classyaze_1_1emu_1_1Ppu.html#a7efc9afaf7bc6356e074ef44f079b36e",
"d4/d0a/classyaze_1_1gfx_1_1PerformanceProfiler.html#afc168478b710ec714fd0b52c033be7b4",
"d4/d2a/asar__integration__test_8cc.html#a1a0e772a79b82ee8a5308dff488a00ec",
"d4/d70/classyaze_1_1test_1_1IntegratedTestSuite.html",
"d4/da1/classyaze_1_1gui_1_1canvas_1_1CanvasUsageManager.html",
"d4/dd5/structyaze_1_1emu_1_1WOBJLOG.html#ab1cc39b2a815421327f40e2f08f81af7",
"d5/d1e/classTextEditor_1_1UndoRecord.html#a24e02136e36dae58bd477eae4f653b13",
"d5/d1f/namespaceyaze_1_1zelda3.html#a693df677b1b2754452c8b8b4c4a98468a82936f94ba9e69a0b8e6cb0d1a9d983d",
"d5/d1f/namespaceyaze_1_1zelda3.html#adc7a44403a309bba04259cd65b181719",
"d5/d60/md_docs_2z3ed_2LLM-INTEGRATION-SUMMARY.html#autotoc_md1669",
"d5/da0/structyaze_1_1emu_1_1DspChannel.html#a466d092d0065c96495c85d52f7e36d04",
"d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1160",
"d6/d0a/canvas__utils__moved_8cc.html#ad3764d0881c0fd928890783de32daa66",
"d6/d2e/classyaze_1_1zelda3_1_1TitleScreen.html#ac0944b93d7d33a52c5028526fa2129fe",
"d6/d3c/classyaze_1_1editor_1_1MapPropertiesSystem.html#ad621e76e0e77bf6d02efa5754313cc5a",
"d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md594",
"d6/db1/classyaze_1_1zelda3_1_1Sprite.html#ae019782d3eb9d0552f39307dfacdb5e6",
"d6/de0/group__graphics.html#ga100f9af9f8e1bb9d2ad73476ab774655",
"d7/d2d/test__commands_8cc.html#af7221a5846c3b201c74c6a07063b867e",
"d7/d61/classyaze_1_1zelda3_1_1DungeonEditorSystem.html#a3a5a44df3b1b4d6ead99a4b5f1630555a37a094ab3d6e2812fd18d57125d25c41",
"d7/d61/classyaze_1_1zelda3_1_1DungeonEditorSystem.html#ae3ea46b985195a59ba9f257a0e441d11",
"d7/d85/classyaze_1_1zelda3_1_1ComprehensiveIntegrationTest.html#afc95aa3f83be587b2018a48c701ff187",
"d7/dc5/classyaze_1_1util_1_1Flag.html#ad32fd87feac8b14cef4393f55410d938",
"d7/df6/classyaze_1_1zelda3_1_1Room.html#a5b27b3fc3a563549e39939d6da3049b0",
"d7/dfa/classyaze_1_1zelda3_1_1ObjectRenderer.html#afbce58547d7f88ae990f4d392c6b9d3a",
"d8/d17/structyaze_1_1gui_1_1canvas_1_1CanvasSelection.html#a09c5a4ede4d9a2d63015102c538f38b3",
"d8/d6e/classyaze_1_1gfx_1_1AtlasRenderer.html#a24eeea253bdbb542493a6aaa4a90ed6b",
"d8/d98/classyaze_1_1editor_1_1DungeonEditor.html#ac6dd0d449f54bd82ac3529436d6cbdb5",
"d8/dd3/namespaceyaze_1_1cli_1_1agent.html#ae9ea61b13bdce0cec2b2d67856a23417",
"d8/de1/structyaze_1_1gfx_1_1RenderCommand.html#a978dd9d52e53be5791daf21c1eb5bce6",
"d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md455",
"d9/dbf/structyaze_1_1cli_1_1PolicyEvaluator_1_1PolicyConfig_1_1ForbiddenRange.html#ae1b562b34ef3bf14ede67bae6faa3836",
"d9/dc5/classyaze_1_1zelda3_1_1Overworld.html#a7ffca1c6ac8cc8c6dc372b73743d1fcf",
"d9/dcc/classyaze_1_1zelda3_1_1OverworldMap.html#a95d451b92008cb4be5638afa811ef021",
"d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md955",
"da/d2c/classyaze_1_1gui_1_1Canvas.html#a04e6cede191a4014242857bc393fc58b",
"da/d2c/classyaze_1_1gui_1_1Canvas.html#ab4d9cde15a24acd4cac4dd9a552d9664",
"da/d3e/classyaze_1_1test_1_1TestManager.html#ab6308ac80eacf9acd6a8a1cfa61ce192",
"da/d68/classyaze_1_1gfx_1_1SnesPalette.html#aa3d5ad0a0ad19df994e243baf817f598",
"da/dbb/structanonymous__namespace_02cli__main_8cc_03_1_1ParsedGlobals.html#a4c6b3304731f4e4fe8304a26412a265b",
"da/dec/structyaze_1_1gfx_1_1BppFormatInfo.html#aa4dc8a73dd7a0b1b2e1779b08a2b2aee",
"db/d2d/classyaze_1_1cli_1_1GeminiAIService.html#a59e3ffc48a868e839573bfac3ccf7aa4",
"db/d76/classyaze_1_1test_1_1integration_1_1AsarRomIntegrationTest.html#a0ae872d8506dda4dbe37eeda880bc13e",
"db/d9a/classyaze_1_1editor_1_1MusicEditor.html#a2aa139b97d71e2e15d08900bfe9325df",
"db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1330",
"db/ddc/classyaze_1_1zelda3_1_1SpritePositionTest.html#a82f628fe76875a433a7a286b1517a485",
"dc/d0c/structyaze_1_1cli_1_1GeminiConfig.html#a142234473ee64bf7a5b5a9e10816ed83",
"dc/d31/classyaze_1_1editor_1_1GraphicsEditor.html#a7bb4384429c6e3dcebda1655c93e9075",
"dc/d4f/classyaze_1_1editor_1_1DungeonObjectSelector.html#a585482153e4cc102743fec85955be0ad",
"dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md707",
"dc/dd1/structyaze_1_1editor_1_1MessagePreview.html#a3ffd97ff05a6ee72963f87a0af9712f2",
"dc/df4/classyaze_1_1emu_1_1Cpu.html#a50035ffb781ab92696205c9c04c25af1",
"dc/df4/classyaze_1_1emu_1_1Cpu.html#ad796950c0e9f4682bced251e55db1116",
"dd/d12/classyaze_1_1editor_1_1EditorManager.html#a85198b73afea6af2f4d8816879841f1c",
"dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md702",
"dd/d5e/structyaze_1_1cli_1_1TestStatusDetails.html#acbc3cef5c19ae8b74ad0dc492edc3a86",
"dd/d80/structyaze_1_1cli_1_1TestCaseRunResult.html#a25768fd38c701c81d5f292c39f78f2a2",
"dd/dcc/classyaze_1_1editor_1_1ProposalDrawer.html#ad558518aec9061d1b5dcf7a4b8abd807",
"dd/df4/structyaze_1_1emu_1_1EmulatorKeybindings.html",
"de/d0f/classyaze_1_1emu_1_1MemoryImpl.html#ae3b177eff0e22eccee345080cebcf292",
"de/d76/classyaze_1_1editor_1_1SpriteEditor.html#a08b19adcef55093c57d5411ceeaeebeb",
"de/d8f/structyaze_1_1zelda3_1_1ObjectSubtypeInfo.html",
"de/dbf/icons_8h.html#a064b4eff7d43eb90f19579106a7431f8",
"de/dbf/icons_8h.html#a25094c972aef5f8bd70b50373f1fe42f",
"de/dbf/icons_8h.html#a425c63cfba24cec0cbcc53646d470fc2",
"de/dbf/icons_8h.html#a5f716c4862faf39c5ba760beecf71673",
"de/dbf/icons_8h.html#a7f27af196662cac0b89e61a67b42854d",
"de/dbf/icons_8h.html#a9e83d65bc1ca13216065c7f5d314c617",
"de/dbf/icons_8h.html#abb0ba61fb9e6ea6e514ec64537a6e3b1",
"de/dbf/icons_8h.html#ad7201a5160ac43c7f133748cad3b2fc5",
"de/dbf/icons_8h.html#af1d29a817fd1b030010c96a291672973",
"de/de7/classyaze_1_1zelda3_1_1DungeonObjectEditor.html#a14981172110a76948c1e24f8b214aa8c",
"de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md982",
"df/d2f/structyaze_1_1zelda3_1_1music_1_1ZeldaWave.html#a7a5514ad6bb962060a0f354a258bc072",
"df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1232",
"dir_12e106757ecdaf4af0ac0afab505b8fc.html",
"namespacemembers_vars_o.html"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';