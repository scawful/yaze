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
      [ "Quick Start", "d9/d41/md_docs_202-build-instructions.html#autotoc_md7", [
        [ "macOS (Apple Silicon)", "d9/d41/md_docs_202-build-instructions.html#autotoc_md8", null ],
        [ "Linux", "d9/d41/md_docs_202-build-instructions.html#autotoc_md9", null ],
        [ "Windows (Recommended - CMake Mode)", "d9/d41/md_docs_202-build-instructions.html#autotoc_md10", null ],
        [ "Minimal Build (CI/Fast)", "d9/d41/md_docs_202-build-instructions.html#autotoc_md11", null ]
      ] ],
      [ "Dependencies", "d9/d41/md_docs_202-build-instructions.html#autotoc_md12", [
        [ "Required", "d9/d41/md_docs_202-build-instructions.html#autotoc_md13", null ],
        [ "Bundled Libraries", "d9/d41/md_docs_202-build-instructions.html#autotoc_md14", null ]
      ] ],
      [ "Platform Setup", "d9/d41/md_docs_202-build-instructions.html#autotoc_md15", [
        [ "macOS", "d9/d41/md_docs_202-build-instructions.html#autotoc_md16", null ],
        [ "Linux (Ubuntu/Debian)", "d9/d41/md_docs_202-build-instructions.html#autotoc_md17", null ],
        [ "Windows", "d9/d41/md_docs_202-build-instructions.html#autotoc_md18", [
          [ "Automated Setup (Recommended)", "d9/d41/md_docs_202-build-instructions.html#autotoc_md19", null ],
          [ "Manual Setup Options", "d9/d41/md_docs_202-build-instructions.html#autotoc_md20", null ],
          [ "vcpkg Integration", "d9/d41/md_docs_202-build-instructions.html#autotoc_md21", null ],
          [ "Windows Build Commands", "d9/d41/md_docs_202-build-instructions.html#autotoc_md22", null ]
        ] ]
      ] ],
      [ "Build Targets", "d9/d41/md_docs_202-build-instructions.html#autotoc_md23", [
        [ "Applications", "d9/d41/md_docs_202-build-instructions.html#autotoc_md24", null ],
        [ "Libraries", "d9/d41/md_docs_202-build-instructions.html#autotoc_md25", null ],
        [ "Development (Debug Builds Only)", "d9/d41/md_docs_202-build-instructions.html#autotoc_md26", null ]
      ] ],
      [ "Build Configurations", "d9/d41/md_docs_202-build-instructions.html#autotoc_md27", [
        [ "Debug (Full Features)", "d9/d41/md_docs_202-build-instructions.html#autotoc_md28", null ],
        [ "Minimal (CI/Fast Builds)", "d9/d41/md_docs_202-build-instructions.html#autotoc_md29", null ],
        [ "Release", "d9/d41/md_docs_202-build-instructions.html#autotoc_md30", null ]
      ] ],
      [ "Testing System", "d9/d41/md_docs_202-build-instructions.html#autotoc_md31", [
        [ "Test Categories", "d9/d41/md_docs_202-build-instructions.html#autotoc_md32", [
          [ "Unit Tests", "d9/d41/md_docs_202-build-instructions.html#autotoc_md33", null ],
          [ "Integration Tests", "d9/d41/md_docs_202-build-instructions.html#autotoc_md34", null ],
          [ "End-to-End (E2E) Tests", "d9/d41/md_docs_202-build-instructions.html#autotoc_md35", null ]
        ] ],
        [ "Running Tests", "d9/d41/md_docs_202-build-instructions.html#autotoc_md36", [
          [ "Local Development", "d9/d41/md_docs_202-build-instructions.html#autotoc_md37", null ],
          [ "CI/CD Environment", "d9/d41/md_docs_202-build-instructions.html#autotoc_md38", null ],
          [ "ROM-Dependent Tests", "d9/d41/md_docs_202-build-instructions.html#autotoc_md39", null ]
        ] ],
        [ "Test Organization", "d9/d41/md_docs_202-build-instructions.html#autotoc_md40", null ],
        [ "Test Executables", "d9/d41/md_docs_202-build-instructions.html#autotoc_md41", [
          [ "Development Build (<tt>yaze_test.cc</tt>)", "d9/d41/md_docs_202-build-instructions.html#autotoc_md42", null ],
          [ "CI Build (<tt>yaze_test_ci.cc</tt>)", "d9/d41/md_docs_202-build-instructions.html#autotoc_md43", null ]
        ] ],
        [ "Test Configuration", "d9/d41/md_docs_202-build-instructions.html#autotoc_md44", [
          [ "CMake Options", "d9/d41/md_docs_202-build-instructions.html#autotoc_md45", null ],
          [ "Test Filters", "d9/d41/md_docs_202-build-instructions.html#autotoc_md46", null ]
        ] ]
      ] ],
      [ "IDE Integration", "d9/d41/md_docs_202-build-instructions.html#autotoc_md47", [
        [ "Visual Studio (Windows)", "d9/d41/md_docs_202-build-instructions.html#autotoc_md48", null ],
        [ "VS Code", "d9/d41/md_docs_202-build-instructions.html#autotoc_md49", null ],
        [ "CLion", "d9/d41/md_docs_202-build-instructions.html#autotoc_md50", null ],
        [ "Xcode (macOS)", "d9/d41/md_docs_202-build-instructions.html#autotoc_md51", null ]
      ] ],
      [ "Windows Development Scripts", "d9/d41/md_docs_202-build-instructions.html#autotoc_md52", [
        [ "Setup Scripts", "d9/d41/md_docs_202-build-instructions.html#autotoc_md53", null ],
        [ "Project Generation Scripts", "d9/d41/md_docs_202-build-instructions.html#autotoc_md54", null ],
        [ "Testing Scripts", "d9/d41/md_docs_202-build-instructions.html#autotoc_md55", null ]
      ] ],
      [ "Features by Build Type", "d9/d41/md_docs_202-build-instructions.html#autotoc_md56", null ],
      [ "CMake Compatibility", "d9/d41/md_docs_202-build-instructions.html#autotoc_md57", [
        [ "Submodule Compatibility Issues", "d9/d41/md_docs_202-build-instructions.html#autotoc_md58", null ]
      ] ],
      [ "CI/CD and Release Builds", "d9/d41/md_docs_202-build-instructions.html#autotoc_md59", [
        [ "GitHub Actions Workflows", "d9/d41/md_docs_202-build-instructions.html#autotoc_md60", null ],
        [ "Test Execution in CI", "d9/d41/md_docs_202-build-instructions.html#autotoc_md61", null ],
        [ "vcpkg Fallback Mechanisms", "d9/d41/md_docs_202-build-instructions.html#autotoc_md62", null ],
        [ "Supported Architectures", "d9/d41/md_docs_202-build-instructions.html#autotoc_md63", null ]
      ] ],
      [ "Troubleshooting", "d9/d41/md_docs_202-build-instructions.html#autotoc_md64", [
        [ "Windows CMake Issues", "d9/d41/md_docs_202-build-instructions.html#autotoc_md65", null ],
        [ "vcpkg Issues", "d9/d41/md_docs_202-build-instructions.html#autotoc_md66", null ],
        [ "Test Issues", "d9/d41/md_docs_202-build-instructions.html#autotoc_md67", null ],
        [ "Architecture Errors (macOS)", "d9/d41/md_docs_202-build-instructions.html#autotoc_md68", null ],
        [ "Missing Headers (Language Server)", "d9/d41/md_docs_202-build-instructions.html#autotoc_md69", null ],
        [ "CI Build Failures", "d9/d41/md_docs_202-build-instructions.html#autotoc_md70", null ],
        [ "Common Error Solutions", "d9/d41/md_docs_202-build-instructions.html#autotoc_md71", null ]
      ] ]
    ] ],
    [ "Asar 65816 Assembler Integration", "d2/dd3/md_docs_203-asar-integration.html", [
      [ "Quick Examples", "d2/dd3/md_docs_203-asar-integration.html#autotoc_md73", [
        [ "Command Line", "d2/dd3/md_docs_203-asar-integration.html#autotoc_md74", null ],
        [ "C++ API", "d2/dd3/md_docs_203-asar-integration.html#autotoc_md75", null ]
      ] ],
      [ "Assembly Patch Examples", "d2/dd3/md_docs_203-asar-integration.html#autotoc_md76", [
        [ "Basic Hook", "d2/dd3/md_docs_203-asar-integration.html#autotoc_md77", null ],
        [ "Advanced Features", "d2/dd3/md_docs_203-asar-integration.html#autotoc_md78", null ]
      ] ],
      [ "API Reference", "d2/dd3/md_docs_203-asar-integration.html#autotoc_md79", [
        [ "AsarWrapper Class", "d2/dd3/md_docs_203-asar-integration.html#autotoc_md80", null ],
        [ "Data Structures", "d2/dd3/md_docs_203-asar-integration.html#autotoc_md81", null ]
      ] ],
      [ "Testing", "d2/dd3/md_docs_203-asar-integration.html#autotoc_md82", [
        [ "ROM-Dependent Tests", "d2/dd3/md_docs_203-asar-integration.html#autotoc_md83", null ]
      ] ],
      [ "Error Handling", "d2/dd3/md_docs_203-asar-integration.html#autotoc_md84", null ],
      [ "Development Workflow", "d2/dd3/md_docs_203-asar-integration.html#autotoc_md85", null ]
    ] ],
    [ "API Reference", "dd/d96/md_docs_204-api-reference.html", [
      [ "C API (<tt>incl/yaze.h</tt>, <tt>incl/zelda.h</tt>)", "dd/d96/md_docs_204-api-reference.html#autotoc_md87", [
        [ "Core Library Functions", "dd/d96/md_docs_204-api-reference.html#autotoc_md88", null ],
        [ "ROM Operations", "dd/d96/md_docs_204-api-reference.html#autotoc_md89", null ],
        [ "Graphics Operations", "dd/d96/md_docs_204-api-reference.html#autotoc_md90", null ],
        [ "Palette System", "dd/d96/md_docs_204-api-reference.html#autotoc_md91", null ],
        [ "Message System", "dd/d96/md_docs_204-api-reference.html#autotoc_md92", null ]
      ] ],
      [ "C++ API", "dd/d96/md_docs_204-api-reference.html#autotoc_md93", [
        [ "AsarWrapper (<tt>src/app/core/asar_wrapper.h</tt>)", "dd/d96/md_docs_204-api-reference.html#autotoc_md94", null ],
        [ "Data Structures", "dd/d96/md_docs_204-api-reference.html#autotoc_md95", [
          [ "ROM Version Support", "dd/d96/md_docs_204-api-reference.html#autotoc_md96", null ],
          [ "SNES Graphics", "dd/d96/md_docs_204-api-reference.html#autotoc_md97", null ],
          [ "Message System", "dd/d96/md_docs_204-api-reference.html#autotoc_md98", null ]
        ] ]
      ] ],
      [ "Error Handling", "dd/d96/md_docs_204-api-reference.html#autotoc_md99", [
        [ "Status Codes", "dd/d96/md_docs_204-api-reference.html#autotoc_md100", null ],
        [ "Error Handling Pattern", "dd/d96/md_docs_204-api-reference.html#autotoc_md101", null ]
      ] ],
      [ "Extension System", "dd/d96/md_docs_204-api-reference.html#autotoc_md102", [
        [ "Plugin Architecture", "dd/d96/md_docs_204-api-reference.html#autotoc_md103", null ],
        [ "Capability Flags", "dd/d96/md_docs_204-api-reference.html#autotoc_md104", null ]
      ] ],
      [ "Backward Compatibility", "dd/d96/md_docs_204-api-reference.html#autotoc_md105", null ]
    ] ],
    [ "Testing Guide", "d6/d10/md_docs_2A1-testing-guide.html", [
      [ "Test Categories", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md107", [
        [ "Stable Tests (STABLE)", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md108", null ],
        [ "ROM-Dependent Tests (ROM_DEPENDENT)", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md109", null ],
        [ "Experimental Tests (EXPERIMENTAL)", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md110", null ]
      ] ],
      [ "Command Line Usage", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md111", null ],
      [ "CMake Presets", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md112", null ],
      [ "Writing Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md113", [
        [ "Stable Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md114", null ],
        [ "ROM-Dependent Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md115", null ],
        [ "Experimental Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md116", null ]
      ] ],
      [ "CI/CD Integration", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md117", [
        [ "GitHub Actions", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md118", null ],
        [ "Test Execution Strategy", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md119", null ]
      ] ],
      [ "Test Development Guidelines", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md120", [
        [ "Writing Stable Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md121", null ],
        [ "Writing ROM-Dependent Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md122", null ],
        [ "Writing Experimental Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md123", null ]
      ] ],
      [ "E2E GUI Testing Framework", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md124", [
        [ "Overview", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md125", null ],
        [ "Architecture", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md126", null ],
        [ "Writing E2E Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md127", null ],
        [ "Helper Functions", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md128", null ],
        [ "Running GUI Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md129", null ],
        [ "Test Categories for E2E", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md130", null ],
        [ "Development Status", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md131", null ],
        [ "Best Practices", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md132", null ]
      ] ],
      [ "Performance and Maintenance", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md133", [
        [ "Regular Review", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md134", null ],
        [ "Performance Monitoring", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md135", null ],
        [ "E2E Test Maintenance", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md136", null ]
      ] ]
    ] ],
    [ "Comprehensive ZScream vs YAZE Overworld Analysis", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html", [
      [ "Executive Summary", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md138", null ],
      [ "Key Findings", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md139", [
        [ "✅ <strong>Confirmed Correct Implementations</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md140", [
          [ "1. <strong>Tile32 Expansion Detection Logic</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md141", null ],
          [ "2. <strong>Tile16 Expansion Detection Logic</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md142", null ],
          [ "3. <strong>Entrance Coordinate Calculation</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md143", null ],
          [ "4. <strong>Hole Coordinate Calculation (with 0x400 offset)</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md144", null ],
          [ "5. <strong>Exit Data Loading</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md145", null ],
          [ "6. <strong>Item Loading with ASM Version Detection</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md146", null ]
        ] ],
        [ "⚠️ <strong>Key Differences Found</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md147", [
          [ "1. <strong>Entrance Expansion Detection</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md148", null ],
          [ "2. <strong>Address Constants</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md149", null ],
          [ "3. <strong>Decompression Logic</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md150", null ]
        ] ],
        [ "🔍 <strong>Additional Findings</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md151", [
          [ "1. <strong>Error Handling</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md152", null ],
          [ "2. <strong>Memory Management</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md153", null ],
          [ "3. <strong>Data Structures</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md154", null ],
          [ "4. <strong>Threading</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md155", null ]
        ] ],
        [ "📊 <strong>Validation Results</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md156", null ],
        [ "🎯 <strong>Conclusion</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md157", null ]
      ] ]
    ] ],
    [ "DungeonEditor Parallel Optimization Implementation", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html", [
      [ "🚀 <strong>Parallelization Strategy Implemented</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md159", [
        [ "<strong>Problem Identified</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md160", null ],
        [ "<strong>Solution: Multi-Threaded Room Loading</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md161", [
          [ "<strong>Key Optimizations</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md162", null ],
          [ "<strong>Parallel Processing Flow</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md163", null ],
          [ "<strong>Thread Safety Features</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md164", null ]
        ] ],
        [ "<strong>Expected Performance Impact</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md165", [
          [ "<strong>Theoretical Speedup</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md166", null ],
          [ "<strong>Expected Results</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md167", null ]
        ] ],
        [ "<strong>Technical Implementation Details</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md168", [
          [ "<strong>Thread Management</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md169", null ],
          [ "<strong>Result Processing</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md170", null ]
        ] ],
        [ "<strong>Monitoring and Validation</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md171", [
          [ "<strong>Performance Timing Added</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md172", null ],
          [ "<strong>Logging and Debugging</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md173", null ]
        ] ],
        [ "<strong>Benefits of This Approach</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md174", null ],
        [ "<strong>Next Steps</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md175", null ]
      ] ]
    ] ],
    [ "Overworld::Load Performance Analysis and Optimization Plan", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html", [
      [ "Current Performance Profile", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md177", null ],
      [ "Detailed Analysis of Overworld::Load", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md178", [
        [ "Current Implementation Breakdown", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md179", null ]
      ] ],
      [ "Major Bottlenecks Identified", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md180", [
        [ "1. <strong>DecompressAllMapTiles() - PRIMARY BOTTLENECK (~1.5-2.0 seconds)</strong>", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md181", null ],
        [ "2. <strong>AssembleMap32Tiles() - SECONDARY BOTTLENECK (~200-400ms)</strong>", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md182", null ],
        [ "3. <strong>AssembleMap16Tiles() - MODERATE BOTTLENECK (~100-200ms)</strong>", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md183", null ]
      ] ],
      [ "Optimization Strategies", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md184", [
        [ "1. <strong>Parallelize Decompression Operations</strong>", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md185", null ],
        [ "2. <strong>Optimize ROM Access Patterns</strong>", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md186", null ],
        [ "3. <strong>Implement Lazy Map Loading</strong>", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md187", null ],
        [ "4. <strong>Optimize HyruleMagicDecompress</strong>", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md188", null ],
        [ "5. <strong>Memory Pool Optimization</strong>", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md189", null ]
      ] ],
      [ "Implementation Priority", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md190", [
        [ "Phase 1: High Impact, Low Risk (Immediate)", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md191", null ],
        [ "Phase 2: Medium Impact, Medium Risk (Next)", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md192", null ],
        [ "Phase 3: Lower Impact, Higher Risk (Future)", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md193", null ]
      ] ],
      [ "Expected Performance Improvements", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md194", [
        [ "Conservative Estimates", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md195", null ],
        [ "Aggressive Estimates", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md196", null ]
      ] ],
      [ "Conclusion", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md197", null ]
    ] ],
    [ "yaze Performance Optimization Summary", "db/de6/md_docs_2analysis_2performance__optimization__summary.html", [
      [ "🎉 <strong>Massive Performance Improvements Achieved!</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md199", [
        [ "📊 <strong>Overall Performance Results</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md200", null ]
      ] ],
      [ "🚀 <strong>Optimizations Implemented</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md201", [
        [ "1. <strong>Performance Monitoring System with Feature Flag</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md202", [
          [ "<strong>Features Added</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md203", null ],
          [ "<strong>Implementation</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md204", null ]
        ] ],
        [ "2. <strong>DungeonEditor Parallel Loading (79% Speedup)</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md205", [
          [ "<strong>Problem Solved</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md206", null ],
          [ "<strong>Solution: Multi-Threaded Room Loading</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md207", null ],
          [ "<strong>Key Features</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md208", null ]
        ] ],
        [ "3. <strong>Incremental Overworld Map Loading</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md209", [
          [ "<strong>Problem Solved</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md210", null ],
          [ "<strong>Solution: Priority-Based Incremental Loading</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md211", null ],
          [ "<strong>Key Features</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md212", null ]
        ] ],
        [ "4. <strong>On-Demand Map Reloading</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md213", [
          [ "<strong>Problem Solved</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md214", null ],
          [ "<strong>Solution: Intelligent Refresh System</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md215", null ],
          [ "<strong>Key Features</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md216", null ]
        ] ]
      ] ],
      [ "🎯 <strong>Technical Architecture</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md217", [
        [ "<strong>Performance Monitoring System</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md218", null ],
        [ "<strong>Parallel Loading Architecture</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md219", null ],
        [ "<strong>Incremental Loading Flow</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md220", null ]
      ] ],
      [ "📈 <strong>Performance Impact Analysis</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md221", [
        [ "<strong>DungeonEditor Optimization</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md222", null ],
        [ "<strong>OverworldEditor Optimization</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md223", null ],
        [ "<strong>Overall System Impact</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md224", null ]
      ] ],
      [ "🔧 <strong>Configuration Options</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md225", [
        [ "<strong>Performance Monitoring</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md226", null ],
        [ "<strong>Parallel Loading Tuning</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md227", null ],
        [ "<strong>Incremental Loading Tuning</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md228", null ]
      ] ],
      [ "🎯 <strong>Future Optimization Opportunities</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md229", [
        [ "<strong>Potential Further Improvements</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md230", null ],
        [ "<strong>Monitoring and Profiling</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md231", null ]
      ] ],
      [ "✅ <strong>Success Metrics Achieved</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md232", null ],
      [ "🚀 <strong>Result: Lightning-Fast YAZE</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md233", null ]
    ] ],
    [ "Renderer Class Performance Analysis and Optimization", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html", [
      [ "Overview", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md235", null ],
      [ "Original Performance Issues", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md236", [
        [ "1. Blocking Texture Creation", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md237", null ],
        [ "2. Inefficient Loading Pattern", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md238", null ]
      ] ],
      [ "Optimizations Implemented", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md239", [
        [ "1. Deferred Texture Creation", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md240", null ],
        [ "2. Lazy Loading System", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md241", null ],
        [ "3. Performance Monitoring", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md242", null ]
      ] ],
      [ "Thread Safety Considerations", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md243", [
        [ "Main Thread Requirement", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md244", null ],
        [ "Safe Optimization Approach", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md245", null ]
      ] ],
      [ "Performance Improvements", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md246", [
        [ "Loading Time Reduction", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md247", null ],
        [ "Memory Efficiency", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md248", null ]
      ] ],
      [ "Implementation Details", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md249", [
        [ "Modified Files", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md250", null ],
        [ "Key Methods Added", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md251", null ]
      ] ],
      [ "Usage Guidelines", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md252", [
        [ "For Developers", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md253", null ],
        [ "For ROM Loading", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md254", null ]
      ] ],
      [ "Future Optimization Opportunities", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md255", [
        [ "1. Background Threading (Pending)", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md256", null ],
        [ "2. Arena Management Optimization (Pending)", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md257", null ],
        [ "3. Advanced Lazy Loading (Pending)", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md258", null ]
      ] ],
      [ "Conclusion", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md259", null ]
    ] ],
    [ "ZScream C# vs YAZE C++ Overworld Implementation Analysis", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html", [
      [ "Overview", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md261", null ],
      [ "Executive Summary", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md262", null ],
      [ "Detailed Comparison", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md263", [
        [ "1. Tile32 Loading and Expansion Detection", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md264", [
          [ "ZScream C# Logic (<tt>Overworld.cs:706-756</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md265", null ],
          [ "yaze C++ Logic (<tt>overworld.cc:AssembleMap32Tiles</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md266", null ]
        ] ],
        [ "2. Tile16 Loading and Expansion Detection", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md267", [
          [ "ZScream C# Logic (<tt>Overworld.cs:652-705</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md268", null ],
          [ "yaze C++ Logic (<tt>overworld.cc:AssembleMap16Tiles</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md269", null ]
        ] ],
        [ "3. Map Decompression", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md270", [
          [ "ZScream C# Logic (<tt>Overworld.cs:767-904</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md271", null ],
          [ "yaze C++ Logic (<tt>overworld.cc:DecompressAllMapTiles</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md272", null ]
        ] ],
        [ "4. Entrance Coordinate Calculation", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md273", [
          [ "ZScream C# Logic (<tt>Overworld.cs:974-1001</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md274", null ],
          [ "yaze C++ Logic (<tt>overworld.cc:LoadEntrances</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md275", null ]
        ] ],
        [ "5. Hole Coordinate Calculation with 0x400 Offset", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md276", [
          [ "ZScream C# Logic (<tt>Overworld.cs:1002-1025</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md277", null ],
          [ "yaze C++ Logic (<tt>overworld.cc:LoadHoles</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md278", null ]
        ] ],
        [ "6. ASM Version Detection for Item Loading", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md279", [
          [ "ZScream C# Logic (<tt>Overworld.cs:1032-1094</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md280", null ],
          [ "yaze C++ Logic (<tt>overworld.cc:LoadItems</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md281", null ]
        ] ],
        [ "7. Game State Handling for Sprite Loading", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md282", [
          [ "ZScream C# Logic (<tt>Overworld.cs:1276-1494</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md283", null ],
          [ "yaze C++ Logic (<tt>overworld.cc:LoadSprites</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md284", null ]
        ] ],
        [ "8. Map Size Assignment Logic", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md285", [
          [ "ZScream C# Logic (<tt>Overworld.cs:296-390</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md286", null ],
          [ "yaze C++ Logic (<tt>overworld.cc:AssignMapSizes</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md287", null ]
        ] ]
      ] ],
      [ "ZSCustomOverworld Integration", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md288", [
        [ "Version Detection", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md289", null ],
        [ "Feature Enablement", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md290", null ]
      ] ],
      [ "Integration Test Coverage", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md291", null ],
      [ "Conclusion", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md292", null ]
    ] ],
    [ "Atlas Rendering Implementation - YAZE Graphics Optimizations", "db/d26/md_docs_2atlas__rendering__implementation.html", [
      [ "Overview", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md294", null ],
      [ "Implementation Details", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md295", [
        [ "Core Components", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md296", [
          [ "1. AtlasRenderer Class (<tt>src/app/gfx/atlas_renderer.h/cc</tt>)", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md297", null ],
          [ "2. RenderCommand Structure", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md298", null ],
          [ "3. Atlas Statistics Tracking", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md299", null ]
        ] ],
        [ "Integration Points", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md300", [
          [ "1. Tilemap Integration (<tt>src/app/gfx/tilemap.h/cc</tt>)", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md301", null ],
          [ "2. Performance Dashboard Integration", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md302", null ],
          [ "3. Benchmarking Suite (<tt>test/gfx_optimization_benchmarks.cc</tt>)", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md303", null ]
        ] ],
        [ "Technical Implementation", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md304", [
          [ "Atlas Packing Algorithm", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md305", null ],
          [ "Batch Rendering Process", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md306", null ]
        ] ],
        [ "Performance Improvements", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md307", [
          [ "Measured Performance Gains", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md308", null ],
          [ "Benchmark Results", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md309", null ]
        ] ],
        [ "ROM Hacking Workflow Benefits", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md310", [
          [ "Graphics Sheet Management", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md311", null ],
          [ "Editor Performance", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md312", null ]
        ] ],
        [ "API Usage Examples", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md313", [
          [ "Basic Atlas Usage", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md314", null ],
          [ "Tilemap Integration", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md315", null ]
        ] ],
        [ "Memory Management", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md316", [
          [ "Automatic Cleanup", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md317", null ],
          [ "Resource Management", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md318", null ]
        ] ],
        [ "Future Enhancements", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md319", [
          [ "Planned Improvements", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md320", null ],
          [ "Integration Opportunities", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md321", null ]
        ] ]
      ] ],
      [ "Conclusion", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md322", null ],
      [ "Files Modified", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md323", null ]
    ] ],
    [ "Contributing", "dd/d5b/md_docs_2B1-contributing.html", [
      [ "Development Setup", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md325", [
        [ "Prerequisites", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md326", null ],
        [ "Quick Start", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md327", null ]
      ] ],
      [ "Code Style", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md328", [
        [ "C++ Standards", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md329", null ],
        [ "File Organization", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md330", null ],
        [ "Error Handling", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md331", null ]
      ] ],
      [ "Testing Requirements", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md332", [
        [ "Test Categories", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md333", null ],
        [ "Writing Tests", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md334", null ],
        [ "Test Execution", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md335", null ]
      ] ],
      [ "Pull Request Process", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md336", [
        [ "Before Submitting", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md337", null ],
        [ "Pull Request Template", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md338", null ]
      ] ],
      [ "Development Workflow", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md339", [
        [ "Branch Strategy", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md340", null ],
        [ "Commit Messages", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md341", null ],
        [ "Types", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md342", null ]
      ] ],
      [ "Architecture Guidelines", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md343", [
        [ "Component Design", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md344", null ],
        [ "Memory Management", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md345", null ],
        [ "Performance", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md346", null ]
      ] ],
      [ "Documentation", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md347", [
        [ "Code Documentation", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md348", null ],
        [ "API Documentation", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md349", null ]
      ] ],
      [ "Community Guidelines", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md350", [
        [ "Communication", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md351", null ],
        [ "Getting Help", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md352", null ]
      ] ],
      [ "Release Process", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md353", [
        [ "Version Numbering", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md354", null ],
        [ "Release Checklist", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md355", null ]
      ] ]
    ] ],
    [ "Platform Compatibility Improvements", "d1/d30/md_docs_2B2-platform-compatibility.html", [
      [ "Native File Dialog Support", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md357", null ],
      [ "Cross-Platform Build Reliability", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md358", null ],
      [ "Build Configuration Options", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md359", [
        [ "Full Build (Development)", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md360", null ],
        [ "Minimal Build", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md361", null ]
      ] ],
      [ "Implementation Details", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md362", null ],
      [ "Platform-Specific Adaptations", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md363", [
        [ "Windows", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md364", null ],
        [ "macOS", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md365", null ],
        [ "Linux", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md366", null ]
      ] ]
    ] ],
    [ "Build Presets Guide", "d7/d51/md_docs_2B3-build-presets.html", [
      [ "🍎 macOS ARM64 Presets (Recommended for Apple Silicon)", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md368", [
        [ "For Development Work:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md369", null ],
        [ "For Distribution:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md370", null ]
      ] ],
      [ "🔧 Why This Fixes Architecture Errors", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md371", null ],
      [ "📋 Available Presets", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md372", null ],
      [ "🚀 Quick Start", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md373", null ],
      [ "🛠️ IDE Integration", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md374", [
        [ "VS Code with CMake Tools:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md375", null ],
        [ "CLion:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md376", null ],
        [ "Xcode:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md377", null ]
      ] ],
      [ "🔍 Troubleshooting", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md378", null ],
      [ "📝 Notes", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md379", null ]
    ] ],
    [ "Release Workflows Documentation", "d3/d49/md_docs_2B4-release-workflows.html", [
      [ "Overview", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md381", null ],
      [ "1. Release-Simplified (<tt>release-simplified.yml</tt>)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md383", [
        [ "Purpose", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md384", null ],
        [ "Key Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md385", null ],
        [ "Use Cases", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md386", null ],
        [ "Configuration", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md387", null ],
        [ "Platforms Supported", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md388", null ]
      ] ],
      [ "2. Release (<tt>release.yml</tt>)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md390", [
        [ "Purpose", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md391", null ],
        [ "Key Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md392", null ],
        [ "Use Cases", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md393", null ],
        [ "Configuration", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md394", null ],
        [ "Advantages over Simplified", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md395", null ]
      ] ],
      [ "3. Release-Complex (<tt>release-complex.yml</tt>)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md397", [
        [ "Purpose", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md398", null ],
        [ "Key Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md399", null ],
        [ "Use Cases", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md400", null ],
        [ "Fallback Mechanisms", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md401", [
          [ "vcpkg Fallback", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md402", null ],
          [ "Chocolatey Integration", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md403", null ],
          [ "Build Configuration Fallback", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md404", null ]
        ] ],
        [ "Advanced Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md405", null ]
      ] ],
      [ "Workflow Comparison Matrix", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md407", null ],
      [ "When to Use Each Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md409", [
        [ "Use Simplified When:", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md410", null ],
        [ "Use Release When:", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md411", null ],
        [ "Use Complex When:", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md412", null ]
      ] ],
      [ "Workflow Selection Guide", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md414", [
        [ "For Development Team", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md415", null ],
        [ "For Release Manager", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md416", null ],
        [ "For CI/CD Pipeline", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md417", null ]
      ] ],
      [ "Configuration Examples", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md419", [
        [ "Triggering a Release", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md420", [
          [ "Manual Release (All Workflows)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md421", null ],
          [ "Automatic Release (Tag Push)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md422", null ]
        ] ],
        [ "Customizing Release Notes", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md423", null ]
      ] ],
      [ "Troubleshooting", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md425", [
        [ "Common Issues", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md426", [
          [ "vcpkg Failures (Windows)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md427", null ],
          [ "Dependency Conflicts", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md428", null ],
          [ "Build Failures", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md429", null ]
        ] ],
        [ "Debug Information", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md430", [
          [ "Simplified Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md431", null ],
          [ "Release Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md432", null ],
          [ "Complex Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md433", null ]
        ] ]
      ] ],
      [ "Best Practices", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md435", [
        [ "Workflow Selection", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md436", null ],
        [ "Release Process", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md437", null ],
        [ "Maintenance", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md438", null ]
      ] ],
      [ "Future Improvements", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md440", [
        [ "Planned Enhancements", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md441", null ],
        [ "Integration Opportunities", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md442", null ]
      ] ]
    ] ],
    [ "Stability, Testability & Release Workflow Improvements", "d9/d6b/md_docs_2B5-stability-improvements.html", [
      [ "Recent Improvements (v0.3.2)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md445", [
        [ "Windows Platform Stability", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md446", [
          [ "Stack Size Optimization", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md447", null ],
          [ "Development Utility Isolation", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md448", null ]
        ] ],
        [ "Graphics System Stability", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md449", [
          [ "Segmentation Fault Resolution", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md450", null ],
          [ "Comprehensive Bounds Checking", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md451", null ]
        ] ],
        [ "Build System Improvements", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md452", [
          [ "Modern Windows Workflow", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md453", null ],
          [ "Enhanced CI/CD Reliability", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md454", null ]
        ] ]
      ] ],
      [ "Recommended Optimizations", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md455", [
        [ "High Priority", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md456", [
          [ "1. Lazy Graphics Loading", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md457", null ],
          [ "2. Heap-Based Large Allocations", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md458", null ],
          [ "3. Streaming ROM Assets", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md459", null ]
        ] ],
        [ "Medium Priority", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md460", [
          [ "4. Enhanced Test Isolation", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md461", null ],
          [ "5. Dependency Caching Optimization", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md462", null ],
          [ "6. Memory Pool for Graphics", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md463", null ]
        ] ],
        [ "Low Priority", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md464", [
          [ "7. Build Time Optimization", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md465", null ],
          [ "8. Release Workflow Simplification", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md466", null ]
        ] ]
      ] ],
      [ "Testing Improvements", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md467", [
        [ "Current State", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md468", null ],
        [ "Recommendations", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md469", [
          [ "1. Visual Regression Testing", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md470", null ],
          [ "2. Performance Benchmarks", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md471", null ],
          [ "3. Fuzz Testing", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md472", null ]
        ] ]
      ] ],
      [ "Metrics & Monitoring", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md473", [
        [ "Current Measurements", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md474", null ],
        [ "Target Improvements", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md475", null ]
      ] ],
      [ "Action Items", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md476", [
        [ "Immediate (v0.3.2)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md477", null ],
        [ "Short Term (v0.3.3)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md478", null ],
        [ "Medium Term (v0.4.0)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md479", null ],
        [ "Long Term (v0.5.0+)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md480", null ]
      ] ],
      [ "Conclusion", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md481", null ]
    ] ],
    [ "Changelog", "d7/d6f/md_docs_2C1-changelog.html", [
      [ "0.3.2 (December 2025) - In Development", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md483", [
        [ "Tile16 Editor Improvements (In Progress)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md484", null ],
        [ "Graphics System Optimizations", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md485", null ],
        [ "Windows Platform Stability", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md486", null ],
        [ "Memory Safety & Stability", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md487", null ],
        [ "Testing & CI/CD Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md488", null ],
        [ "Future Optimizations (Planned)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md489", null ],
        [ "Technical Notes", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md490", null ]
      ] ],
      [ "0.3.1 (September 2025)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md491", [
        [ "Major Features", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md492", null ],
        [ "Tile16 Editor Enhancements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md493", null ],
        [ "ZSCustomOverworld v3 Implementation", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md494", null ],
        [ "Technical Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md495", null ],
        [ "User Interface", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md496", null ],
        [ "Bug Fixes", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md497", null ],
        [ "ZScream Compatibility Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md498", null ]
      ] ],
      [ "0.3.0 (September 2025)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md499", [
        [ "Major Features", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md500", null ],
        [ "User Interface & Theming", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md501", null ],
        [ "Enhancements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md502", null ],
        [ "Technical Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md503", null ],
        [ "Bug Fixes", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md504", null ]
      ] ],
      [ "0.2.2 (December 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md505", null ],
      [ "0.2.1 (August 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md506", null ],
      [ "0.2.0 (July 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md507", null ],
      [ "0.1.0 (May 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md508", null ],
      [ "0.0.9 (April 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md509", null ],
      [ "0.0.8 (February 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md510", null ],
      [ "0.0.7 (January 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md511", null ],
      [ "0.0.6 (November 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md512", null ],
      [ "0.0.5 (November 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md513", null ],
      [ "0.0.4 (November 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md514", null ],
      [ "0.0.3 (October 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md515", null ]
    ] ],
    [ "Canvas System - Comprehensive Guide", "d2/db2/md_docs_2CANVAS__GUIDE.html", [
      [ "Overview", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md517", null ],
      [ "Core Concepts", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md518", [
        [ "Canvas Structure", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md519", null ],
        [ "Coordinate Systems", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md520", null ]
      ] ],
      [ "Usage Patterns", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md521", [
        [ "Pattern 1: Basic Bitmap Display", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md522", null ],
        [ "Pattern 2: Modern Begin/End", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md523", null ],
        [ "Pattern 3: RAII ScopedCanvas", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md524", null ]
      ] ],
      [ "Feature: Tile Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md525", [
        [ "Single Tile Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md526", null ],
        [ "Tilemap Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md527", null ],
        [ "Color Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md528", null ]
      ] ],
      [ "Feature: Tile Selection", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md529", [
        [ "Single Tile Selection", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md530", null ],
        [ "Multi-Tile Rectangle Selection", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md531", null ],
        [ "Rectangle Drag & Paint", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md532", null ]
      ] ],
      [ "Feature: Custom Overlays", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md533", [
        [ "Manual Points Manipulation", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md534", null ]
      ] ],
      [ "Feature: Large Map Support", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md535", [
        [ "Map Types", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md536", null ],
        [ "Boundary Clamping", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md537", null ],
        [ "Custom Map Sizes", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md538", null ]
      ] ],
      [ "Feature: Context Menu", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md539", [
        [ "Adding Custom Items", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md540", null ],
        [ "Overworld Editor Example", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md541", null ]
      ] ],
      [ "Feature: Scratch Space (In Progress)", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md542", null ],
      [ "Common Workflows", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md543", [
        [ "Workflow 1: Overworld Tile Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md544", null ],
        [ "Workflow 2: Tile16 Blockset Selection", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md545", null ],
        [ "Workflow 3: Graphics Sheet Display", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md546", null ]
      ] ],
      [ "Configuration", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md547", [
        [ "Grid Settings", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md548", null ],
        [ "Scale Settings", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md549", null ],
        [ "Interaction Settings", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md550", null ],
        [ "Large Map Settings", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md551", null ]
      ] ],
      [ "Bug Fixes Applied", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md552", [
        [ "1. Rectangle Selection Wrapping in Large Maps ✅", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md553", null ],
        [ "2. Drag-Time Preview Clamping ✅", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md554", null ]
      ] ],
      [ "API Reference", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md555", [
        [ "Drawing Methods", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md556", null ],
        [ "State Accessors", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md557", null ],
        [ "Configuration", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md558", null ]
      ] ],
      [ "Implementation Notes", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md559", [
        [ "Points Management", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md560", null ],
        [ "Selection State", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md561", null ],
        [ "Overworld Rectangle Painting Flow", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md562", null ]
      ] ],
      [ "Best Practices", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md563", [
        [ "DO ✅", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md564", null ],
        [ "DON'T ❌", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md565", null ]
      ] ],
      [ "Troubleshooting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md566", [
        [ "Issue: Rectangle wraps at boundaries", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md567", null ],
        [ "Issue: Painting in wrong location", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md568", null ],
        [ "Issue: Array index out of bounds", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md569", null ],
        [ "Issue: Forgot to call End()", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md570", null ]
      ] ],
      [ "Future: Scratch Space", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md571", null ],
      [ "Documentation Files", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md572", null ],
      [ "Summary", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md573", null ]
    ] ],
    [ "Canvas Refactoring - Current Status & Future Work", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html", [
      [ "✅ Successfully Completed", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md575", [
        [ "1. Modern ImGui-Style Interface (Working)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md576", null ],
        [ "2. Context Menu Improvements (Working)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md577", null ],
        [ "3. Optional CanvasInteractionHandler Component (Available)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md578", null ],
        [ "4. Code Cleanup", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md579", null ]
      ] ],
      [ "⚠️ Outstanding Issue: Rectangle Selection Wrapping", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md580", [
        [ "The Problem", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md581", null ],
        [ "What Was Attempted", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md582", null ],
        [ "Root Cause Analysis", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md583", null ],
        [ "Suspected Issue", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md584", null ],
        [ "What Needs Investigation", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md585", null ],
        [ "Debugging Steps for Future Agent", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md586", null ],
        [ "Possible Fixes to Try", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md587", null ]
      ] ],
      [ "🔧 Files Modified", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md588", [
        [ "Core Canvas", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md589", null ],
        [ "Overworld Editor", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md590", null ],
        [ "Components Created", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md591", null ]
      ] ],
      [ "📚 Documentation (Consolidated)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md592", null ],
      [ "🎯 Future Refactoring Steps", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md593", [
        [ "Priority 1: Fix Rectangle Wrapping (High)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md594", null ],
        [ "Priority 2: Extract Coordinate Conversion Helpers (Low Impact)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md595", null ],
        [ "Priority 3: Move Components to canvas/ Namespace (Organizational)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md596", null ],
        [ "Priority 4: Complete Scratch Space Feature (Feature)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md597", null ],
        [ "Priority 5: Simplify Canvas State (Refactoring)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md598", null ]
      ] ],
      [ "🔍 Known Working Patterns", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md599", [
        [ "Overworld Tile Painting (Working)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md600", null ],
        [ "Blockset Selection (Working)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md601", null ],
        [ "Manual Overlay Highlighting (Working)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md602", null ]
      ] ],
      [ "🎓 Lessons Learned", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md603", [
        [ "What Worked", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md604", null ],
        [ "What Didn't Work", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md605", null ],
        [ "Key Insights", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md606", null ]
      ] ],
      [ "📋 For Future Agent", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md607", [
        [ "Immediate Task: Fix Rectangle Wrapping", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md608", null ],
        [ "Medium Term: Namespace Organization", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md609", null ],
        [ "Long Term: State Management Simplification", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md610", null ],
        [ "Stretch Goals: Enhanced Features", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md611", null ]
      ] ],
      [ "📊 Current Metrics", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md612", null ],
      [ "🔑 Key Files Reference", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md613", [
        [ "Core Canvas", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md614", null ],
        [ "Canvas Components", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md615", null ],
        [ "Utilities", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md616", null ],
        [ "Major Consumers", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md617", null ]
      ] ],
      [ "🎯 Recommended Next Steps", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md618", [
        [ "Step 1: Fix Rectangle Wrapping Bug (Critical)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md619", null ],
        [ "Step 2: Test All Editors (Verification)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md620", null ],
        [ "Step 3: Adopt Modern Patterns (Optional)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md621", null ]
      ] ],
      [ "📖 Documentation", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md622", [
        [ "Read This", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md623", null ],
        [ "Background (Optional)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md624", null ]
      ] ],
      [ "💡 Quick Reference", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md625", [
        [ "Modern Usage", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md626", null ],
        [ "Legacy Usage (Still Works)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md627", null ],
        [ "Revert Clamping", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md628", null ],
        [ "Add Context Menu", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md629", null ]
      ] ],
      [ "✅ Current Status", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md630", null ]
    ] ],
    [ "Roadmap", "d6/db4/md_docs_2D1-roadmap.html", [
      [ "0.4.X (Next Major Release)", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md633", [
        [ "Core Features", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md634", null ],
        [ "Technical Improvements", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md635", null ]
      ] ],
      [ "0.5.X", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md636", [
        [ "Advanced Features", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md637", null ]
      ] ],
      [ "0.6.X", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md638", [
        [ "Platform & Integration", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md639", null ]
      ] ],
      [ "0.7.X", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md640", [
        [ "Performance & Polish", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md641", null ]
      ] ],
      [ "0.8.X", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md642", [
        [ "Beta Preparation", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md643", null ]
      ] ],
      [ "1.0.0", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md644", [
        [ "Stable Release", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md645", null ]
      ] ],
      [ "Current Focus Areas", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md646", [
        [ "Immediate Priorities (v0.4.X)", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md647", null ],
        [ "Long-term Vision", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md648", null ]
      ] ]
    ] ],
    [ "Asm Style Guide", "d7/d9a/md_docs_2E1-asm-style-guide.html", [
      [ "Table of Contents", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md650", null ],
      [ "File Structure", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md651", null ],
      [ "Labels and Symbols", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md652", null ],
      [ "Comments", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md653", null ],
      [ "Instructions", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md654", null ],
      [ "Macros", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md655", null ],
      [ "Loops and Branching", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md656", null ],
      [ "Data Structures", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md657", null ],
      [ "Code Organization", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md658", null ],
      [ "Custom Code", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md659", null ]
    ] ],
    [ "Dungeon Editor Guide", "dd/d33/md_docs_2E2-dungeon-editor-guide.html", [
      [ "Overview", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md661", null ],
      [ "Architecture", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md662", [
        [ "Core Components", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md663", [
          [ "1. DungeonEditorSystem", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md664", null ],
          [ "2. DungeonObjectEditor", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md665", null ],
          [ "3. ObjectRenderer", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md666", null ],
          [ "4. DungeonEditor (UI Layer)", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md667", null ]
        ] ]
      ] ],
      [ "Coordinate System", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md668", [
        [ "Room Coordinates vs Canvas Coordinates", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md669", [
          [ "Conversion Functions", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md670", null ]
        ] ],
        [ "Coordinate System Features", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md671", null ]
      ] ],
      [ "Object Rendering System", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md672", [
        [ "Object Types", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md673", null ],
        [ "Rendering Pipeline", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md674", null ],
        [ "Performance Optimizations", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md675", null ]
      ] ],
      [ "User Interface", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md676", [
        [ "Integrated Editing Panels", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md677", [
          [ "Main Canvas", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md678", null ],
          [ "Compact Editing Panels", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md679", null ]
        ] ],
        [ "Object Preview System", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md680", null ]
      ] ],
      [ "Integration with ZScream", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md681", [
        [ "Room Loading", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md682", null ],
        [ "Object Parsing", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md683", null ],
        [ "Coordinate System", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md684", null ]
      ] ],
      [ "Testing and Validation", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md685", [
        [ "Integration Tests", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md686", null ],
        [ "Test Data", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md687", null ],
        [ "Performance Benchmarks", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md688", null ]
      ] ],
      [ "Usage Examples", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md689", [
        [ "Basic Object Editing", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md690", null ],
        [ "Coordinate Conversion", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md691", null ],
        [ "Object Preview", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md692", null ]
      ] ],
      [ "Configuration Options", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md693", [
        [ "Editor Configuration", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md694", null ],
        [ "Performance Configuration", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md695", null ]
      ] ],
      [ "Troubleshooting", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md696", [
        [ "Common Issues", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md697", null ],
        [ "Debug Information", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md698", null ]
      ] ],
      [ "Future Enhancements", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md699", [
        [ "Planned Features", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md700", null ]
      ] ],
      [ "Conclusion", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md701", null ]
    ] ],
    [ "Dungeon Editor Design Plan", "dc/d82/md_docs_2E3-dungeon-editor-design.html", [
      [ "Overview", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md703", null ],
      [ "Architecture Overview", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md704", [
        [ "Core Components", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md705", null ],
        [ "File Structure", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md706", null ]
      ] ],
      [ "Component Responsibilities", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md707", [
        [ "DungeonEditor (Main Orchestrator)", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md708", null ],
        [ "DungeonRoomSelector", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md709", null ],
        [ "DungeonCanvasViewer", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md710", null ],
        [ "DungeonObjectSelector", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md711", null ]
      ] ],
      [ "Data Flow", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md712", [
        [ "Initialization Flow", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md713", null ],
        [ "Runtime Data Flow", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md714", null ],
        [ "Key Data Structures", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md715", null ]
      ] ],
      [ "Integration Patterns", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md716", [
        [ "Component Communication", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md717", null ],
        [ "ROM Data Management", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md718", null ],
        [ "State Synchronization", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md719", null ]
      ] ],
      [ "UI Layout Architecture", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md720", [
        [ "3-Column Layout", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md721", null ],
        [ "Component Internal Layout", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md722", null ]
      ] ],
      [ "Coordinate System", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md723", [
        [ "Room Coordinates vs Canvas Coordinates", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md724", null ],
        [ "Bounds Checking", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md725", null ]
      ] ],
      [ "Error Handling & Validation", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md726", [
        [ "ROM Validation", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md727", null ],
        [ "Bounds Validation", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md728", null ]
      ] ],
      [ "Performance Considerations", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md729", [
        [ "Caching Strategy", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md730", null ],
        [ "Rendering Optimization", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md731", null ]
      ] ],
      [ "Testing Strategy", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md732", [
        [ "Integration Tests", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md733", null ],
        [ "Test Categories", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md734", null ]
      ] ],
      [ "Future Development Guidelines", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md735", [
        [ "Adding New Features", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md736", null ],
        [ "Component Extension Patterns", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md737", null ],
        [ "Data Flow Extension", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md738", null ],
        [ "UI Layout Extension", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md739", null ]
      ] ],
      [ "Common Pitfalls & Solutions", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md740", [
        [ "Memory Management", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md741", null ],
        [ "Coordinate System", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md742", null ],
        [ "State Synchronization", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md743", null ],
        [ "Performance Issues", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md744", null ]
      ] ],
      [ "Debugging Tools", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md745", [
        [ "Debug Popup", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md746", null ],
        [ "Logging", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md747", null ]
      ] ],
      [ "Build Integration", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md748", [
        [ "CMake Configuration", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md749", null ],
        [ "Dependencies", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md750", null ]
      ] ],
      [ "Conclusion", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md751", null ]
    ] ],
    [ "DungeonEditor Refactoring Plan", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html", [
      [ "Overview", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md753", null ],
      [ "Component Structure", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md754", [
        [ "✅ Created Components", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md755", [
          [ "1. DungeonToolset (<tt>dungeon_toolset.h/cc</tt>)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md756", null ],
          [ "2. DungeonObjectInteraction (<tt>dungeon_object_interaction.h/cc</tt>)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md757", null ],
          [ "3. DungeonRenderer (<tt>dungeon_renderer.h/cc</tt>)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md758", null ],
          [ "4. DungeonRoomLoader (<tt>dungeon_room_loader.h/cc</tt>)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md759", null ],
          [ "5. DungeonUsageTracker (<tt>dungeon_usage_tracker.h/cc</tt>)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md760", null ]
        ] ]
      ] ],
      [ "Refactored DungeonEditor Structure", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md761", [
        [ "Before Refactoring: 1444 lines", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md762", null ],
        [ "After Refactoring: ~400 lines", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md763", null ]
      ] ],
      [ "Method Migration Map", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md764", [
        [ "Core Editor Methods (Keep in main file)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md765", null ],
        [ "UI Methods (Keep for coordination)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md766", null ],
        [ "Methods Moved to Components", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md767", [
          [ "→ DungeonToolset", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md768", null ],
          [ "→ DungeonObjectInteraction", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md769", null ],
          [ "→ DungeonRenderer", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md770", null ],
          [ "→ DungeonRoomLoader", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md771", null ],
          [ "→ DungeonUsageTracker", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md772", null ]
        ] ]
      ] ],
      [ "Component Communication", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md773", [
        [ "Callback System", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md774", null ],
        [ "Data Sharing", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md775", null ]
      ] ],
      [ "Benefits of Refactoring", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md776", [
        [ "1. <strong>Reduced Complexity</strong>", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md777", null ],
        [ "2. <strong>Improved Testability</strong>", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md778", null ],
        [ "3. <strong>Better Maintainability</strong>", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md779", null ],
        [ "4. <strong>Enhanced Extensibility</strong>", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md780", null ],
        [ "5. <strong>Cleaner Dependencies</strong>", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md781", null ]
      ] ],
      [ "Implementation Status", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md782", [
        [ "✅ Completed", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md783", null ],
        [ "🔄 In Progress", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md784", null ],
        [ "⏳ Pending", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md785", null ]
      ] ],
      [ "Migration Strategy", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md786", [
        [ "Phase 1: Create Components ✅", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md787", null ],
        [ "Phase 2: Integrate Components 🔄", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md788", null ],
        [ "Phase 3: Move Methods", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md789", null ],
        [ "Phase 4: Cleanup", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md790", null ]
      ] ]
    ] ],
    [ "Dungeon Object System", "da/d11/md_docs_2E5-dungeon-object-system.html", [
      [ "Overview", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md792", null ],
      [ "Architecture", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md793", [
        [ "Core Components", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md794", [
          [ "1. DungeonEditor (<tt>src/app/editor/dungeon/dungeon_editor.h</tt>)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md795", null ],
          [ "2. DungeonObjectSelector (<tt>src/app/editor/dungeon/dungeon_object_selector.h</tt>)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md796", null ],
          [ "3. DungeonCanvasViewer (<tt>src/app/editor/dungeon/dungeon_canvas_viewer.h</tt>)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md797", null ],
          [ "4. Room Management System (<tt>src/app/zelda3/dungeon/room.h</tt>)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md798", null ]
        ] ]
      ] ],
      [ "Object Types and Hierarchies", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md799", [
        [ "Room Objects", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md800", [
          [ "Type 1 Objects (0x00-0xFF)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md801", null ],
          [ "Type 2 Objects (0x100-0x1FF)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md802", null ],
          [ "Type 3 Objects (0x200+)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md803", null ]
        ] ],
        [ "Object Properties", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md804", null ]
      ] ],
      [ "How Object Placement Works", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md805", [
        [ "Selection Process", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md806", null ],
        [ "Placement Process", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md807", null ],
        [ "Code Flow", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md808", null ]
      ] ],
      [ "Rendering Pipeline", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md809", [
        [ "Object Rendering", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md810", null ],
        [ "Performance Optimizations", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md811", null ]
      ] ],
      [ "User Interface Components", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md812", [
        [ "Three-Column Layout", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md813", [
          [ "Column 1: Room Control Panel (280px fixed)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md814", null ],
          [ "Column 2: Windowed Canvas (800px fixed)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md815", null ],
          [ "Column 3: Object Selector/Editor (stretch)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md816", null ]
        ] ],
        [ "Debug and Control Features", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md817", [
          [ "Room Properties Table", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md818", null ],
          [ "Object Statistics", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md819", null ]
        ] ]
      ] ],
      [ "Integration with ROM Data", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md820", [
        [ "Data Sources", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md821", [
          [ "Room Headers (<tt>0x1F8000</tt>)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md822", null ],
          [ "Object Data", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md823", null ],
          [ "Graphics Data", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md824", null ]
        ] ],
        [ "Assembly Integration", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md825", null ]
      ] ],
      [ "Comparison with ZScream", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md826", [
        [ "Architectural Differences", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md827", [
          [ "Component-Based Architecture", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md828", null ],
          [ "Real-time Rendering", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md829", null ],
          [ "UI Organization", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md830", null ],
          [ "Caching Strategy", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md831", null ]
        ] ],
        [ "Shared Concepts", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md832", null ]
      ] ],
      [ "Best Practices", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md833", [
        [ "Performance", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md834", null ],
        [ "Code Organization", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md835", null ],
        [ "User Experience", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md836", null ]
      ] ],
      [ "Future Enhancements", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md837", [
        [ "Planned Features", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md838", null ],
        [ "Technical Improvements", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md839", null ]
      ] ]
    ] ],
    [ "1. Overview", "d0/da5/md_docs_2E6-z3ed-cli-design.html", [
      [ "2. Design Goals", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md842", null ],
      [ "3. Proposed CLI Architecture: Resource-Oriented Commands", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md843", [
        [ "1.1. Current State", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md841", null ],
        [ "3.1. Top-Level Resources", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md844", null ],
        [ "3.2. Example Command Mapping", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md845", null ]
      ] ],
      [ "4. New Features & Commands", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md846", [
        [ "4.1. For the ROM Hacker (Power & Scriptability)", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md847", null ],
        [ "4.2. For Testing & Automation", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md848", null ]
      ] ],
      [ "5. TUI Enhancements", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md849", null ],
      [ "6. Generative & Agentic Workflows (MCP Integration)", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md850", [
        [ "6.1. The Generative Workflow", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md851", null ],
        [ "6.2. Key Enablers", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md852", null ]
      ] ],
      [ "7. Implementation Roadmap", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md853", [
        [ "Phase 1: Core CLI & TUI Foundation (Done)", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md854", null ],
        [ "Phase 2: Interactive TUI & Command Palette (Done)", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md855", null ],
        [ "Phase 3: Testing & Project Management (Done)", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md856", null ],
        [ "Phase 4: Agentic Framework & Generative AI (In Progress)", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md857", null ],
        [ "Phase 5: Code Structure & UX Improvements (Completed)", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md858", null ]
      ] ],
      [ "8. Agentic Framework Architecture - Advanced Dive", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md859", [
        [ "8.1. The <tt>z3ed agent</tt> Command", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md860", null ],
        [ "8.2. The Agentic Loop (MCP) - Detailed Workflow", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md861", null ],
        [ "8.3. AI Model & Protocol Strategy", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md862", null ],
        [ "8.4. GUI Integration & User Experience", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md863", null ],
        [ "8.5. Testing & Verification", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md864", null ],
        [ "8.6. Safety & Sandboxing", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md865", null ],
        [ "8.7. Optional JSON Dependency", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md866", null ],
        [ "8.8. Contextual Awareness & Feedback Loop", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md867", null ],
        [ "8.9. Error Handling and Recovery", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md868", null ],
        [ "8.10. Extensibility", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md869", null ]
      ] ],
      [ "9. UX Improvements and Architectural Decisions", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md870", [
        [ "9.1. TUI Component Architecture", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md871", null ],
        [ "9.2. Command Handler Unification", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md872", null ],
        [ "9.3. Interface Consolidation", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md873", null ],
        [ "9.4. Code Organization Improvements", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md874", null ],
        [ "9.5. Future UX Enhancements", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md875", null ]
      ] ],
      [ "10. Implementation Status and Code Quality", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md876", [
        [ "10.1. Recent Refactoring Improvements (January 2025)", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md877", null ],
        [ "10.2. File Organization", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md878", null ],
        [ "10.3. Code Quality Improvements", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md879", null ],
        [ "10.4. TUI Component System", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md880", null ],
        [ "10.5. Known Limitations", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md881", null ],
        [ "10.6. Future Code Quality Goals", "d0/da5/md_docs_2E6-z3ed-cli-design.html#autotoc_md882", null ]
      ] ]
    ] ],
    [ "Tile16 Editor Palette System", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html", [
      [ "Executive Summary", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md884", null ],
      [ "Problem Analysis", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md885", [
        [ "Critical Issues Identified", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md886", null ]
      ] ],
      [ "Solution Architecture", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md887", [
        [ "Core Design Principles", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md888", null ],
        [ "256-Color Overworld Palette Structure", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md889", null ],
        [ "Sheet-to-Palette Mapping", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md890", null ],
        [ "Palette Button Mapping", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md891", null ]
      ] ],
      [ "Implementation Details", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md892", [
        [ "1. Fixed SetPaletteWithTransparent Method", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md893", null ],
        [ "2. Corrected Tile16 Editor Palette System", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md894", null ],
        [ "3. Palette Coordination Flow", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md895", null ]
      ] ],
      [ "UI/UX Refactoring", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md896", [
        [ "New Three-Column Layout", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md897", null ],
        [ "Canvas Context Menu Fixes", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md898", null ],
        [ "Dynamic Zoom Controls", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md899", null ]
      ] ],
      [ "Testing Protocol", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md900", [
        [ "Crash Prevention Testing", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md901", null ],
        [ "Color Alignment Testing", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md902", null ],
        [ "UI/UX Testing", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md903", null ]
      ] ],
      [ "Error Handling", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md904", [
        [ "Bounds Checking", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md905", null ],
        [ "Fallback Mechanisms", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md906", null ]
      ] ],
      [ "Debug Information Display", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md907", null ],
      [ "Known Issues and Ongoing Work", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md908", [
        [ "Completed Items ✅", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md909", null ],
        [ "Active Issues ⚠️", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md910", null ],
        [ "Current Status Summary", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md911", null ],
        [ "Future Enhancements", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md912", null ]
      ] ],
      [ "Maintenance Notes", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md913", null ],
      [ "Next Steps", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md915", [
        [ "Immediate Priorities", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md916", null ],
        [ "Investigation Areas", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md917", null ]
      ] ]
    ] ],
    [ "Overworld Loading Guide", "dc/d12/md_docs_2F1-overworld-loading.html", [
      [ "Table of Contents", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md920", null ],
      [ "Overview", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md921", null ],
      [ "ROM Types and Versions", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md922", [
        [ "Version Detection", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md923", null ],
        [ "Feature Support by Version", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md924", null ]
      ] ],
      [ "Overworld Map Structure", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md925", [
        [ "Core Properties", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md926", null ]
      ] ],
      [ "Overlays and Special Area Maps", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md927", [
        [ "Understanding Overlays", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md928", null ],
        [ "Special Area Maps (0x80-0x9F)", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md929", null ],
        [ "Overlay ID Mappings", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md930", null ],
        [ "Drawing Order", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md931", null ],
        [ "Vanilla Overlay Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md932", null ],
        [ "Special Area Graphics Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md933", null ]
      ] ],
      [ "Loading Process", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md934", [
        [ "1. Version Detection", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md935", null ],
        [ "2. Map Initialization", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md936", null ],
        [ "3. Property Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md937", [
          [ "Vanilla ROMs (asm_version == 0xFF)", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md938", null ],
          [ "ZSCustomOverworld v2/v3", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md939", null ]
        ] ],
        [ "4. Custom Data Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md940", null ]
      ] ],
      [ "ZScream Implementation", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md941", [
        [ "OverworldMap Constructor", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md942", null ],
        [ "Key Methods", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md943", null ]
      ] ],
      [ "Yaze Implementation", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md944", [
        [ "OverworldMap Constructor", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md945", null ],
        [ "Key Methods", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md946", null ],
        [ "Current Status", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md947", null ]
      ] ],
      [ "Key Differences", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md948", [
        [ "1. Language and Architecture", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md949", null ],
        [ "2. Data Structures", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md950", null ],
        [ "3. Error Handling", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md951", null ],
        [ "4. Graphics Processing", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md952", null ]
      ] ],
      [ "Common Issues and Solutions", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md953", [
        [ "1. Version Detection Issues", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md954", null ],
        [ "2. Palette Loading Errors", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md955", null ],
        [ "3. Graphics Not Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md956", null ],
        [ "4. Overlay Issues", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md957", null ],
        [ "5. Large Map Problems", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md958", null ],
        [ "6. Special Area Graphics Issues", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md959", null ]
      ] ],
      [ "Best Practices", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md960", [
        [ "1. Version-Specific Code", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md961", null ],
        [ "2. Error Handling", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md962", null ],
        [ "3. Memory Management", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md963", null ],
        [ "4. Thread Safety", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md964", null ]
      ] ],
      [ "Conclusion", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md965", null ]
    ] ],
    [ "YAZE Graphics System Improvements Summary", "d9/df4/md_docs_2gfx__improvements__summary.html", [
      [ "Overview", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md967", null ],
      [ "Files Modified", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md968", [
        [ "Core Graphics Classes", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md969", [
          [ "1. <tt>/src/app/gfx/bitmap.h</tt>", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md970", null ],
          [ "2. <tt>/src/app/gfx/bitmap.cc</tt>", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md971", null ],
          [ "3. <tt>/src/app/gfx/arena.h</tt>", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md972", null ],
          [ "4. <tt>/src/app/gfx/arena.cc</tt>", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md973", null ],
          [ "5. <tt>/src/app/gfx/tilemap.h</tt>", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md974", null ]
        ] ],
        [ "Editor Classes", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md975", [
          [ "6. <tt>/src/app/editor/graphics/graphics_editor.cc</tt>", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md976", null ],
          [ "7. <tt>/src/app/editor/graphics/palette_editor.cc</tt>", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md977", null ],
          [ "8. <tt>/src/app/editor/graphics/screen_editor.cc</tt>", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md978", null ]
        ] ]
      ] ],
      [ "New Documentation Files", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md979", [
        [ "9. <tt>/docs/gfx_optimization_recommendations.md</tt>", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md980", null ]
      ] ],
      [ "Performance Optimization Recommendations", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md981", [
        [ "High Impact, Low Risk (Phase 1)", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md982", null ],
        [ "Medium Impact, Medium Risk (Phase 2)", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md983", null ],
        [ "High Impact, High Risk (Phase 3)", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md984", null ]
      ] ],
      [ "ROM Hacking Workflow Improvements", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md985", [
        [ "Graphics Editor Enhancements", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md986", null ],
        [ "Palette Editor Enhancements", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md987", null ],
        [ "Screen Editor Enhancements", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md988", null ]
      ] ],
      [ "Code Quality Improvements", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md989", [
        [ "Documentation Standards", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md990", null ],
        [ "Code Organization", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md991", null ]
      ] ],
      [ "Future Development Recommendations", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md992", [
        [ "Immediate Improvements", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md993", null ],
        [ "Medium-term Enhancements", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md994", null ],
        [ "Long-term Goals", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md995", null ]
      ] ],
      [ "Conclusion", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md996", null ]
    ] ],
    [ "YAZE Graphics System Optimization Recommendations", "de/def/md_docs_2gfx__optimization__recommendations.html", [
      [ "Overview", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md998", null ],
      [ "Current Architecture Analysis", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md999", [
        [ "Strengths", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md1000", null ],
        [ "Performance Bottlenecks Identified", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md1001", [
          [ "1. Bitmap Class Issues", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md1002", null ],
          [ "2. Arena Resource Management", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md1003", null ],
          [ "3. Tilemap Performance", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md1004", null ]
        ] ]
      ] ],
      [ "Optimization Recommendations", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md1005", [
        [ "1. Bitmap Class Optimizations", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md1006", [
          [ "A. Palette Lookup Optimization", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md1007", null ],
          [ "B. Dirty Region Tracking", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md1008", null ]
        ] ],
        [ "2. Arena Resource Management Improvements", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md1009", [
          [ "A. Resource Pooling", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md1010", null ],
          [ "B. Batch Operations", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md1011", null ]
        ] ],
        [ "3. Tilemap Performance Enhancements", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md1012", [
          [ "A. Smart Tile Caching", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md1013", null ],
          [ "B. Atlas-based Rendering", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md1014", null ]
        ] ],
        [ "4. Editor-Specific Optimizations", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md1015", [
          [ "A. Graphics Editor Improvements", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md1016", null ],
          [ "B. Palette Editor Optimizations", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md1017", null ]
        ] ],
        [ "5. Memory Management Improvements", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md1018", [
          [ "A. Custom Allocator for Graphics Data", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md1019", null ],
          [ "B. Smart Pointer Management", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md1020", null ]
        ] ]
      ] ],
      [ "Implementation Priority", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md1021", [
        [ "Phase 1 (High Impact, Low Risk)", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md1022", null ],
        [ "Phase 2 (Medium Impact, Medium Risk)", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md1023", null ],
        [ "Phase 3 (High Impact, High Risk)", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md1024", null ]
      ] ],
      [ "Performance Metrics", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md1025", [
        [ "Target Improvements", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md1026", null ],
        [ "Measurement Tools", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md1027", null ]
      ] ],
      [ "Conclusion", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md1028", null ]
    ] ],
    [ "YAZE Graphics System Optimizations - Complete Implementation", "d6/df4/md_docs_2gfx__optimizations__complete.html", [
      [ "Overview", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1030", null ],
      [ "Implemented Optimizations", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1031", [
        [ "1. Palette Lookup Optimization ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1032", null ],
        [ "2. Dirty Region Tracking ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1033", null ],
        [ "3. Resource Pooling ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1034", null ],
        [ "4. LRU Tile Caching ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1035", null ],
        [ "5. Batch Operations ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1036", null ],
        [ "6. Memory Pool Allocator ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1037", null ],
        [ "7. Atlas-Based Rendering ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1038", null ],
        [ "8. Performance Profiling System ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1039", null ],
        [ "9. Performance Monitoring Dashboard ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1040", null ],
        [ "10. Optimization Validation Suite ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1041", null ]
      ] ],
      [ "Performance Metrics", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1042", [
        [ "Expected Improvements", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1043", null ],
        [ "Measurement Tools", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1044", null ]
      ] ],
      [ "Integration Points", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1045", [
        [ "Graphics Editor", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1046", null ],
        [ "Palette Editor", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1047", null ],
        [ "Screen Editor", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1048", null ]
      ] ],
      [ "Backward Compatibility", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1049", null ],
      [ "Usage Examples", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1050", [
        [ "Using Batch Operations", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1051", null ],
        [ "Using Memory Pool", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1052", null ],
        [ "Using Atlas Rendering", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1053", null ],
        [ "Using Performance Monitoring", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1054", null ]
      ] ],
      [ "Future Enhancements", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1055", [
        [ "Phase 2 Optimizations (Medium Priority)", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1056", null ],
        [ "Phase 3 Optimizations (High Priority)", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1057", null ]
      ] ],
      [ "Testing and Validation", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1058", [
        [ "Performance Testing", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1059", null ],
        [ "ROM Hacking Workflow Testing", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1060", null ]
      ] ],
      [ "Conclusion", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1061", null ],
      [ "Files Modified/Created", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1062", [
        [ "Core Graphics Classes", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1063", null ],
        [ "New Optimization Components", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1064", null ],
        [ "Testing and Validation", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1065", null ],
        [ "Build System", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1066", null ],
        [ "Documentation", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1067", null ]
      ] ]
    ] ],
    [ "yaze Graphics System Optimizations - Implementation Summary", "d3/d7b/md_docs_2gfx__optimizations__implemented.html", [
      [ "Overview", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1069", null ],
      [ "Implemented Optimizations", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1070", [
        [ "1. Palette Lookup Optimization ✅ COMPLETED", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1071", null ],
        [ "2. Dirty Region Tracking ✅ COMPLETED", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1072", null ],
        [ "3. Resource Pooling ✅ COMPLETED", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1073", null ],
        [ "4. LRU Tile Caching ✅ COMPLETED", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1074", null ],
        [ "5. Region-Specific Texture Updates ✅ COMPLETED", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1075", null ],
        [ "6. Performance Profiling System ✅ COMPLETED", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1076", null ]
      ] ],
      [ "Performance Metrics", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1077", [
        [ "Expected Improvements", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1078", null ],
        [ "Measurement Tools", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1079", null ]
      ] ],
      [ "Integration Points", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1080", [
        [ "Graphics Editor", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1081", null ],
        [ "Palette Editor", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1082", null ],
        [ "Screen Editor", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1083", null ]
      ] ],
      [ "Backward Compatibility", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1084", null ],
      [ "Future Enhancements", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1085", [
        [ "Phase 2 Optimizations (Medium Priority)", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1086", null ],
        [ "Phase 3 Optimizations (High Priority)", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1087", null ]
      ] ],
      [ "Testing and Validation", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1088", [
        [ "Performance Testing", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1089", null ],
        [ "ROM Hacking Workflow Testing", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1090", null ]
      ] ],
      [ "Conclusion", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1091", null ]
    ] ],
    [ "YAZE Graphics Optimizations Project - Final Summary", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html", [
      [ "Project Overview", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1093", null ],
      [ "Completed Optimizations", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1094", [
        [ "✅ 1. Batch Operations for Texture Updates", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1095", null ],
        [ "✅ 2. Memory Pool Allocator", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1096", null ],
        [ "✅ 3. Atlas-Based Rendering System", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1097", null ],
        [ "✅ 4. Performance Monitoring Dashboard", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1098", null ],
        [ "✅ 5. Optimization Validation Suite", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1099", null ],
        [ "✅ 6. Debug Menu Integration", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1100", null ]
      ] ],
      [ "Performance Metrics Achieved", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1101", [
        [ "Expected Improvements (Based on Implementation)", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1102", null ],
        [ "Real Performance Data (From Timing Report)", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1103", null ]
      ] ],
      [ "Technical Implementation Details", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1104", [
        [ "Architecture Improvements", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1105", null ],
        [ "Code Quality Enhancements", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1106", null ]
      ] ],
      [ "Integration Points", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1107", [
        [ "Graphics System Integration", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1108", null ],
        [ "Editor Integration", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1109", null ]
      ] ],
      [ "Future Enhancements", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1110", [
        [ "Remaining Optimization (Pending)", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1111", null ],
        [ "Potential Extensions", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1112", null ]
      ] ],
      [ "Build and Testing", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1113", [
        [ "Build Status", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1114", null ],
        [ "Testing Status", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1115", null ]
      ] ],
      [ "Conclusion", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1116", null ]
    ] ],
    [ "yaze Documentation", "d3/d4c/md_docs_2index.html", [
      [ "Quick Start", "d3/d4c/md_docs_2index.html#autotoc_md1118", null ],
      [ "Development", "d3/d4c/md_docs_2index.html#autotoc_md1119", null ],
      [ "Technical Documentation", "d3/d4c/md_docs_2index.html#autotoc_md1120", [
        [ "Assembly & Code", "d3/d4c/md_docs_2index.html#autotoc_md1121", null ],
        [ "Editor Systems", "d3/d4c/md_docs_2index.html#autotoc_md1122", null ],
        [ "Overworld System", "d3/d4c/md_docs_2index.html#autotoc_md1123", null ]
      ] ],
      [ "Key Features", "d3/d4c/md_docs_2index.html#autotoc_md1124", null ]
    ] ],
    [ "yaze Overworld Testing Guide", "de/d8a/md_docs_2overworld__testing__guide.html", [
      [ "Overview", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1127", null ],
      [ "Test Architecture", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1128", [
        [ "1. Golden Data System", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1129", null ],
        [ "2. Test Categories", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1130", [
          [ "Unit Tests (<tt>test/unit/zelda3/</tt>)", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1131", null ],
          [ "Integration Tests (<tt>test/e2e/</tt>)", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1132", null ],
          [ "Golden Data Tools", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1133", null ]
        ] ]
      ] ],
      [ "Quick Start", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1134", [
        [ "Prerequisites", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1135", null ],
        [ "Running Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1136", [
          [ "Basic Test Run", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1137", null ],
          [ "Selective Test Execution", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1138", null ]
        ] ]
      ] ],
      [ "Test Components", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1139", [
        [ "1. Golden Data Extractor", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1140", null ],
        [ "2. Integration Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1141", null ],
        [ "3. End-to-End Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1142", null ]
      ] ],
      [ "Test Validation Points", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1143", [
        [ "1. ZScream Compatibility", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1144", null ],
        [ "2. ROM State Validation", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1145", null ],
        [ "3. Performance and Stability", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1146", null ]
      ] ],
      [ "Environment Variables", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1147", [
        [ "Test Configuration", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1148", null ],
        [ "Build Configuration", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1149", null ]
      ] ],
      [ "Test Reports", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1150", [
        [ "Generated Reports", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1151", null ],
        [ "Report Location", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1152", null ]
      ] ],
      [ "Troubleshooting", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1153", [
        [ "Common Issues", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1154", [
          [ "1. ROM Not Found", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1155", null ],
          [ "2. Build Failures", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1156", null ],
          [ "3. Test Failures", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1157", null ]
        ] ],
        [ "Debug Mode", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1158", null ]
      ] ],
      [ "Advanced Usage", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1159", [
        [ "Custom Test Scenarios", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1160", [
          [ "1. Testing Custom ROMs", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1161", null ],
          [ "2. Regression Testing", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1162", null ],
          [ "3. Performance Testing", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1163", null ]
        ] ]
      ] ],
      [ "Integration with CI/CD", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1164", [
        [ "GitHub Actions Example", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1165", null ]
      ] ],
      [ "Contributing", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1166", [
        [ "Adding New Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1167", null ],
        [ "Test Guidelines", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1168", null ],
        [ "Example Test Structure", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1169", null ]
      ] ],
      [ "Conclusion", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1170", null ]
    ] ],
    [ "yaze - Yet Another Zelda3 Editor", "d0/d30/md_README.html", [
      [ "Version 0.3.2 - Release", "d0/d30/md_README.html#autotoc_md1172", [
        [ "🛠️ Technical Improvements", "d0/d30/md_README.html#autotoc_md1176", null ]
      ] ],
      [ "Quick Start", "d0/d30/md_README.html#autotoc_md1177", [
        [ "Build", "d0/d30/md_README.html#autotoc_md1178", null ],
        [ "Applications", "d0/d30/md_README.html#autotoc_md1179", null ]
      ] ],
      [ "Usage", "d0/d30/md_README.html#autotoc_md1180", [
        [ "GUI Editor", "d0/d30/md_README.html#autotoc_md1181", null ],
        [ "Command Line Tool", "d0/d30/md_README.html#autotoc_md1182", null ],
        [ "C++ API", "d0/d30/md_README.html#autotoc_md1183", null ]
      ] ],
      [ "Documentation", "d0/d30/md_README.html#autotoc_md1184", null ],
      [ "Supported Platforms", "d0/d30/md_README.html#autotoc_md1185", null ],
      [ "ROM Compatibility", "d0/d30/md_README.html#autotoc_md1186", null ],
      [ "Contributing", "d0/d30/md_README.html#autotoc_md1187", null ],
      [ "License", "d0/d30/md_README.html#autotoc_md1188", null ],
      [ "🙏 Acknowledgments", "d0/d30/md_README.html#autotoc_md1189", null ],
      [ "📸 Screenshots", "d0/d30/md_README.html#autotoc_md1190", null ]
    ] ],
    [ "yaze Build Scripts", "de/d82/md_scripts_2README.html", [
      [ "Windows Scripts", "de/d82/md_scripts_2README.html#autotoc_md1193", [
        [ "Setup Scripts", "de/d82/md_scripts_2README.html#autotoc_md1194", null ],
        [ "Build Scripts", "de/d82/md_scripts_2README.html#autotoc_md1195", null ],
        [ "Validation Scripts", "de/d82/md_scripts_2README.html#autotoc_md1196", null ],
        [ "Project Generation", "de/d82/md_scripts_2README.html#autotoc_md1197", null ]
      ] ],
      [ "Windows Compiler Recommendations", "de/d82/md_scripts_2README.html#autotoc_md1198", [
        [ "⚠️ Important: MSVC vs Clang on Windows", "de/d82/md_scripts_2README.html#autotoc_md1199", [
          [ "Why Clang is Recommended:", "de/d82/md_scripts_2README.html#autotoc_md1200", null ],
          [ "MSVC Issues:", "de/d82/md_scripts_2README.html#autotoc_md1201", null ]
        ] ],
        [ "Compiler Setup Options", "de/d82/md_scripts_2README.html#autotoc_md1202", [
          [ "Option 1: Clang (Recommended)", "de/d82/md_scripts_2README.html#autotoc_md1203", null ],
          [ "Option 2: MSVC with Workarounds", "de/d82/md_scripts_2README.html#autotoc_md1204", null ]
        ] ]
      ] ],
      [ "Quick Start (Windows)", "de/d82/md_scripts_2README.html#autotoc_md1205", [
        [ "Option 1: Automated Setup (Recommended)", "de/d82/md_scripts_2README.html#autotoc_md1206", null ],
        [ "Option 2: Manual Setup", "de/d82/md_scripts_2README.html#autotoc_md1207", null ],
        [ "Option 3: Using Batch Scripts", "de/d82/md_scripts_2README.html#autotoc_md1208", null ]
      ] ],
      [ "Script Options", "de/d82/md_scripts_2README.html#autotoc_md1209", [
        [ "setup-windows-dev.ps1", "de/d82/md_scripts_2README.html#autotoc_md1210", null ],
        [ "build-windows.ps1", "de/d82/md_scripts_2README.html#autotoc_md1211", null ],
        [ "build-windows.bat", "de/d82/md_scripts_2README.html#autotoc_md1212", null ]
      ] ],
      [ "Examples", "de/d82/md_scripts_2README.html#autotoc_md1213", null ],
      [ "Troubleshooting", "de/d82/md_scripts_2README.html#autotoc_md1214", [
        [ "Common Issues", "de/d82/md_scripts_2README.html#autotoc_md1215", null ],
        [ "Getting Help", "de/d82/md_scripts_2README.html#autotoc_md1216", null ]
      ] ],
      [ "Other Scripts", "de/d82/md_scripts_2README.html#autotoc_md1217", null ]
    ] ],
    [ "yaze Test Suite", "d0/d46/md_test_2README.html", [
      [ "Directory Structure", "d0/d46/md_test_2README.html#autotoc_md1219", null ],
      [ "Test Categories", "d0/d46/md_test_2README.html#autotoc_md1220", [
        [ "Unit Tests (<tt>unit/</tt>)", "d0/d46/md_test_2README.html#autotoc_md1221", null ],
        [ "Integration Tests (<tt>integration/</tt>)", "d0/d46/md_test_2README.html#autotoc_md1222", null ],
        [ "End-to-End Tests (<tt>e2e/</tt>)", "d0/d46/md_test_2README.html#autotoc_md1223", null ]
      ] ],
      [ "Enhanced Test Runner", "d0/d46/md_test_2README.html#autotoc_md1224", [
        [ "Usage Examples", "d0/d46/md_test_2README.html#autotoc_md1225", null ],
        [ "Test Modes", "d0/d46/md_test_2README.html#autotoc_md1226", null ],
        [ "Options", "d0/d46/md_test_2README.html#autotoc_md1227", null ]
      ] ],
      [ "E2E ROM Testing", "d0/d46/md_test_2README.html#autotoc_md1228", [
        [ "Test Cases", "d0/d46/md_test_2README.html#autotoc_md1229", null ]
      ] ],
      [ "ZSCustomOverworld Upgrade Testing", "d0/d46/md_test_2README.html#autotoc_md1230", [
        [ "Supported Upgrades", "d0/d46/md_test_2README.html#autotoc_md1231", null ],
        [ "Test Cases", "d0/d46/md_test_2README.html#autotoc_md1232", null ],
        [ "Version-Specific Features", "d0/d46/md_test_2README.html#autotoc_md1233", [
          [ "Vanilla", "d0/d46/md_test_2README.html#autotoc_md1234", null ],
          [ "v2", "d0/d46/md_test_2README.html#autotoc_md1235", null ],
          [ "v3", "d0/d46/md_test_2README.html#autotoc_md1236", null ]
        ] ]
      ] ],
      [ "Environment Variables", "d0/d46/md_test_2README.html#autotoc_md1237", null ],
      [ "CI/CD Integration", "d0/d46/md_test_2README.html#autotoc_md1238", null ],
      [ "Deprecated Tests", "d0/d46/md_test_2README.html#autotoc_md1239", null ],
      [ "Best Practices", "d0/d46/md_test_2README.html#autotoc_md1240", null ],
      [ "AI Agent Testing", "d0/d46/md_test_2README.html#autotoc_md1241", null ]
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
"d0/d48/structyaze_1_1emu_1_1Mosaic.html#a5d435be08e95d6ab87e44edd7b76e5a1",
"d0/d9d/classyaze_1_1test_1_1UnitTestSuite.html#a09444cf0dcdc43f1e3a3676d6ad9a0af",
"d0/ded/classyaze_1_1test_1_1OverworldE2ETest.html",
"d1/d31/structyaze_1_1editor_1_1Tile16Editor_1_1UndoState.html#a7c9b313be2cb71ea241beea450b54a83",
"d1/d46/structyaze_1_1gui_1_1BackgroundRenderer_1_1GridSettings.html#a6cc4c2931c8e6e4faaed4106e0ccfb09",
"d1/d7c/structyaze_1_1editor_1_1CommandManager_1_1CommandInfo.html#a62b321569c3d22a230c471944021b496",
"d1/dc6/classyaze_1_1zelda3_1_1Subtype3.html#a3b4bcd85b289010eef764aa2ae1578f9",
"d2/d07/classyaze_1_1emu_1_1Spc700.html#a3165e23c6917b89d547e139d037bdbe2",
"d2/d16/namespaceyaze_1_1test_1_1anonymous__namespace_02compression__test_8cc_03.html#aea2a498a83c2d824402e1632a4c0c14b",
"d2/d5e/classyaze_1_1emu_1_1Memory.html#a4de051dd6298e74353cab01771006340",
"d2/dc1/structyaze_1_1gui_1_1Button.html#ab88e64066c56418393e6fd510e0bf87e",
"d2/def/structyaze_1_1emu_1_1WindowLayer.html#a71938d37bbd6259034218156fd44c163",
"d2/dfe/structyaze_1_1gui_1_1EnhancedTheme.html#af732377a2ac7af4f5dc2ff835446260c",
"d3/d1a/classyaze_1_1gui_1_1BppComparisonTool.html#aad01244ffd74c0c00e5500e27d393f43",
"d3/d44/classyaze_1_1editor_1_1OverworldEditor.html#a073cb5a6d2e8cd029fb970f8ef674952",
"d3/d44/classyaze_1_1editor_1_1OverworldEditor.html#ad7ab4bb78250192917437afcc1ebb995",
"d3/d6a/classyaze_1_1zelda3_1_1RoomObject.html#accd7cf42d441d8a9cd7e8afa944658e5",
"d3/d8a/classyaze_1_1editor_1_1CommandManager.html#adf0aeba0580d0246dbb85f18481b38d8",
"d3/d9f/classyaze_1_1editor_1_1Editor.html",
"d3/ddf/classyaze_1_1zelda3_1_1OverworldTest.html",
"d3/ded/classyaze_1_1emu_1_1Ppu.html#ab3834ba4f57fce087aaf0821a58522fa",
"d4/d0a/namespaceyaze_1_1test.html#a71b1a357ed57d0f4085d6affe4d4cbcb",
"d4/d45/classyaze_1_1editor_1_1AssemblyEditor.html#aa7de13cccee71b653be543e0958996af",
"d4/d84/classyaze_1_1core_1_1Controller.html",
"d4/dd2/structyaze_1_1emu_1_1W34SEL.html#a8843bf9a86122eff7ff84473eaa3111b",
"d5/d1b/room_8cc.html#a62286f1cb2eebc9d5834551ead453371",
"d5/d1f/namespaceyaze_1_1zelda3.html#a693df677b1b2754452c8b8b4c4a98468a382fc72aaa2ac4c921dcb2b7e6ca625e",
"d5/d1f/namespaceyaze_1_1zelda3.html#ad2ad871f946a2bab43dd92bc159c2bb9",
"d5/d60/structyaze_1_1zelda3_1_1music_1_1SongRange.html#ae7405a603e5e64dd8493023c720ed8fa",
"d5/db4/structyaze_1_1emu_1_1TM.html",
"d6/d0a/canvas__utils__moved_8cc.html#a8b09024c825a7452289a675413be558e",
"d6/d2e/classyaze_1_1zelda3_1_1TitleScreen.html#ac0944b93d7d33a52c5028526fa2129fe",
"d6/d3c/classyaze_1_1editor_1_1MapPropertiesSystem.html#aed17440867d75349b50fed89e86726fc",
"d6/d8e/classyaze_1_1cli_1_1RomValidate.html",
"d6/dc5/structyaze_1_1gfx_1_1PerformanceDashboard_1_1PerformanceMetrics.html#a6bf9e39e7b72299a533f2d4e77a43b04",
"d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md1031",
"d7/d4d/classyaze_1_1gui_1_1canvas_1_1CanvasUsageTracker.html#a9a73bc8047d7bc529b425bf66996117e",
"d7/d61/classyaze_1_1zelda3_1_1DungeonEditorSystem.html#a9aa036f39f2eca1db0dcdd222c6d88e2",
"d7/d6f/md_docs_2C1-changelog.html#autotoc_md503",
"d7/d9c/overworld_8h.html#ad7f4c2a68fdd515015aae0aa460adc47",
"d7/ddb/classyaze_1_1gfx_1_1ScopedTimer.html#a0592bff172bc2e38d5efd359954f8bdb",
"d7/df6/classyaze_1_1zelda3_1_1Room.html#aeef0d945dccf1e24cbc6bdba13726df4",
"d8/d05/style_8cc.html#a96948e55f3d549bc0b88765fced3725f",
"d8/d52/structyaze_1_1gui_1_1Table.html",
"d8/d98/classyaze_1_1editor_1_1DungeonEditor.html#a790e20561541405fc372b70be9104aa0",
"d8/dc3/classyaze_1_1zelda3_1_1DungeonObjectRendererIntegrationTest.html#a4005e44d8e80ed2ec1e086b015d67132",
"d8/ddf/classyaze_1_1zelda3_1_1ObjectParser.html#aa8a97d0784f8abd164700eaf9444ce31",
"d9/d5a/structyaze_1_1emu_1_1M7A.html#a3efddecd68c8545349a0675e764c6760",
"d9/dc0/room_8h.html#a693df677b1b2754452c8b8b4c4a98468a2d745a47567cd5d54eb480f3caaca77c",
"d9/dc5/classyaze_1_1zelda3_1_1Overworld.html#aafe9299354e8c01d7a940fdb5112a667",
"d9/dcc/classyaze_1_1zelda3_1_1OverworldMap.html#aceb6f85e5129289463d83efd292c9390",
"d9/dff/ppu__registers_8h.html#aba2368fa52dee82c2b6e07edc822ef3d",
"da/d2c/classyaze_1_1gui_1_1Canvas.html#a5ba6f22f74c3f3c3a955d9677d3a0b40",
"da/d35/structyaze_1_1zelda3_1_1ObjectCategories_1_1ObjectInfo.html#a87d2c59c322bbea8e0aee87911513874",
"da/d5e/structyaze_1_1core_1_1YazeProject.html#ac891dacd5c2244a0b7dc6fc367227427",
"da/da8/test__dungeon__objects_8cc.html#adcfcd757eda237443195790998408ecb",
"db/d00/classyaze_1_1editor_1_1DungeonToolset.html#a47e8e7a0781970c9429ea561ae8c4deb",
"db/d56/classyaze_1_1gui_1_1BppConversionDialog.html#ae58a3c2254024fc4a654e474a07790e9",
"db/d82/classyaze_1_1editor_1_1Tile16Editor.html#aa374ed80931aeb3e49b3b344588320d0",
"db/da4/classyaze_1_1emu_1_1MockMemory.html#aa4e9f87520058d748bced1ad68a64eb9",
"db/dd7/classyaze_1_1zelda3_1_1ObjectRenderer_1_1GraphicsCache.html#aa02cabddc91d0afba651db1e9dbd2acd",
"dc/d0a/structyaze_1_1test_1_1DungeonObjectRenderingTests_1_1DungeonScenario.html#a7e998f5ab86cfb378b8e1b809547ad46",
"dc/d31/classyaze_1_1editor_1_1GraphicsEditor.html#aa818fec1097238e3b456f9cb7104b68e",
"dc/d4f/classyaze_1_1editor_1_1DungeonObjectSelector.html#ad40f587d64e1b68993a4bf33388f5625",
"dc/dae/classyaze_1_1test_1_1ZSCustomOverworldTestSuite.html",
"dc/df4/classyaze_1_1emu_1_1Cpu.html#a1eac3c2018da93be574296583e94dcfa",
"dc/df4/classyaze_1_1emu_1_1Cpu.html#aad8a065fd52e121ae96c0149489f62ee",
"dd/d12/classyaze_1_1editor_1_1EditorManager.html#a132e0a078804474631890db676aa1f74",
"dd/d19/structyaze_1_1core_1_1FeatureFlags_1_1Flags.html#a851b0fe4ed380000b937328fa4f47d1e",
"dd/d4b/classyaze_1_1editor_1_1DungeonUsageTracker.html#ab3cd5088fa84d79e4a1781a6d52b40cb",
"dd/d80/classyaze_1_1cli_1_1CommandHandler.html",
"dd/ddf/classyaze_1_1gui_1_1canvas_1_1CanvasContextMenu.html#a728e925cc68d6c08332b2f55f4146715a35c3ace1970663a16e5c65baa5941b13",
"de/d0d/structzelda3__version__pointers.html#a75e32f7b41723072950549a94d319131",
"de/d4c/input_8cc.html#a7b898e6c361ce2d367d1e1570f8f6c03",
"de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1158",
"de/dbf/icons_8h.html#a0cf7ba553fbb3a067ea728ef99b8c927",
"de/dbf/icons_8h.html#a2af5d00433b3cf6006a7d497da613950",
"de/dbf/icons_8h.html#a4919e269b78a06a8ee840d4c3d2b474c",
"de/dbf/icons_8h.html#a650d4395170840265667ef419f53addf",
"de/dbf/icons_8h.html#a83f1ea7200285fcf7aadb04ec0b2e196",
"de/dbf/icons_8h.html#aa425846cce0d7bdc82a356f56f39be45",
"de/dbf/icons_8h.html#abfe8c6434bd2c8679cf560f91ae8a20f",
"de/dbf/icons_8h.html#adc3579894fce186c38ade2a03de03b75",
"de/dbf/icons_8h.html#af6a7148e2bb4149805edf61d373960f2",
"de/de7/classyaze_1_1zelda3_1_1DungeonObjectEditor.html#aa6424ce197379b9d60267cd430116377",
"df/d0e/classyaze_1_1gfx_1_1Bitmap.html#addd611c1e3b131e9025116995eb326d9",
"df/d6c/namespaceextract__changelog.html#acda8a6283bdc25c5bae76b6a726601dd",
"df/dfd/test__utils_8h.html#acde8466936427e1b1ba331bccb124a9e",
"namespacemembers_vars.html"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';