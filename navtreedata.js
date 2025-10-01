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
      [ "Performance and Maintenance", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md124", [
        [ "Regular Review", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md125", null ],
        [ "Performance Monitoring", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md126", null ]
      ] ]
    ] ],
    [ "Comprehensive ZScream vs YAZE Overworld Analysis", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html", [
      [ "Executive Summary", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md128", null ],
      [ "Key Findings", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md129", [
        [ "✅ <strong>Confirmed Correct Implementations</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md130", [
          [ "1. <strong>Tile32 Expansion Detection Logic</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md131", null ],
          [ "2. <strong>Tile16 Expansion Detection Logic</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md132", null ],
          [ "3. <strong>Entrance Coordinate Calculation</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md133", null ],
          [ "4. <strong>Hole Coordinate Calculation (with 0x400 offset)</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md134", null ],
          [ "5. <strong>Exit Data Loading</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md135", null ],
          [ "6. <strong>Item Loading with ASM Version Detection</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md136", null ]
        ] ],
        [ "⚠️ <strong>Key Differences Found</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md137", [
          [ "1. <strong>Entrance Expansion Detection</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md138", null ],
          [ "2. <strong>Address Constants</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md139", null ],
          [ "3. <strong>Decompression Logic</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md140", null ]
        ] ],
        [ "🔍 <strong>Additional Findings</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md141", [
          [ "1. <strong>Error Handling</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md142", null ],
          [ "2. <strong>Memory Management</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md143", null ],
          [ "3. <strong>Data Structures</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md144", null ],
          [ "4. <strong>Threading</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md145", null ]
        ] ],
        [ "📊 <strong>Validation Results</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md146", null ],
        [ "🎯 <strong>Conclusion</strong>", "d5/ddd/md_docs_2analysis_2comprehensive__overworld__analysis.html#autotoc_md147", null ]
      ] ]
    ] ],
    [ "DungeonEditor Parallel Optimization Implementation", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html", [
      [ "🚀 <strong>Parallelization Strategy Implemented</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md149", [
        [ "<strong>Problem Identified</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md150", null ],
        [ "<strong>Solution: Multi-Threaded Room Loading</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md151", [
          [ "<strong>Key Optimizations</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md152", null ],
          [ "<strong>Parallel Processing Flow</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md153", null ],
          [ "<strong>Thread Safety Features</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md154", null ]
        ] ],
        [ "<strong>Expected Performance Impact</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md155", [
          [ "<strong>Theoretical Speedup</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md156", null ],
          [ "<strong>Expected Results</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md157", null ]
        ] ],
        [ "<strong>Technical Implementation Details</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md158", [
          [ "<strong>Thread Management</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md159", null ],
          [ "<strong>Result Processing</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md160", null ]
        ] ],
        [ "<strong>Monitoring and Validation</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md161", [
          [ "<strong>Performance Timing Added</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md162", null ],
          [ "<strong>Logging and Debugging</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md163", null ]
        ] ],
        [ "<strong>Benefits of This Approach</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md164", null ],
        [ "<strong>Next Steps</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md165", null ]
      ] ]
    ] ],
    [ "Overworld::Load Performance Analysis and Optimization Plan", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html", [
      [ "Current Performance Profile", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md167", null ],
      [ "Detailed Analysis of Overworld::Load", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md168", [
        [ "Current Implementation Breakdown", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md169", null ]
      ] ],
      [ "Major Bottlenecks Identified", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md170", [
        [ "1. <strong>DecompressAllMapTiles() - PRIMARY BOTTLENECK (~1.5-2.0 seconds)</strong>", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md171", null ],
        [ "2. <strong>AssembleMap32Tiles() - SECONDARY BOTTLENECK (~200-400ms)</strong>", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md172", null ],
        [ "3. <strong>AssembleMap16Tiles() - MODERATE BOTTLENECK (~100-200ms)</strong>", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md173", null ]
      ] ],
      [ "Optimization Strategies", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md174", [
        [ "1. <strong>Parallelize Decompression Operations</strong>", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md175", null ],
        [ "2. <strong>Optimize ROM Access Patterns</strong>", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md176", null ],
        [ "3. <strong>Implement Lazy Map Loading</strong>", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md177", null ],
        [ "4. <strong>Optimize HyruleMagicDecompress</strong>", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md178", null ],
        [ "5. <strong>Memory Pool Optimization</strong>", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md179", null ]
      ] ],
      [ "Implementation Priority", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md180", [
        [ "Phase 1: High Impact, Low Risk (Immediate)", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md181", null ],
        [ "Phase 2: Medium Impact, Medium Risk (Next)", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md182", null ],
        [ "Phase 3: Lower Impact, Higher Risk (Future)", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md183", null ]
      ] ],
      [ "Expected Performance Improvements", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md184", [
        [ "Conservative Estimates", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md185", null ],
        [ "Aggressive Estimates", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md186", null ]
      ] ],
      [ "Conclusion", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md187", null ]
    ] ],
    [ "YAZE Performance Optimization Summary", "db/de6/md_docs_2analysis_2performance__optimization__summary.html", [
      [ "🎉 <strong>Massive Performance Improvements Achieved!</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md189", [
        [ "📊 <strong>Overall Performance Results</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md190", null ]
      ] ],
      [ "🚀 <strong>Optimizations Implemented</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md191", [
        [ "1. <strong>Performance Monitoring System with Feature Flag</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md192", [
          [ "<strong>Features Added</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md193", null ],
          [ "<strong>Implementation</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md194", null ]
        ] ],
        [ "2. <strong>DungeonEditor Parallel Loading (79% Speedup)</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md195", [
          [ "<strong>Problem Solved</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md196", null ],
          [ "<strong>Solution: Multi-Threaded Room Loading</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md197", null ],
          [ "<strong>Key Features</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md198", null ]
        ] ],
        [ "3. <strong>Incremental Overworld Map Loading</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md199", [
          [ "<strong>Problem Solved</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md200", null ],
          [ "<strong>Solution: Priority-Based Incremental Loading</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md201", null ],
          [ "<strong>Key Features</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md202", null ]
        ] ],
        [ "4. <strong>On-Demand Map Reloading</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md203", [
          [ "<strong>Problem Solved</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md204", null ],
          [ "<strong>Solution: Intelligent Refresh System</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md205", null ],
          [ "<strong>Key Features</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md206", null ]
        ] ]
      ] ],
      [ "🎯 <strong>Technical Architecture</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md207", [
        [ "<strong>Performance Monitoring System</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md208", null ],
        [ "<strong>Parallel Loading Architecture</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md209", null ],
        [ "<strong>Incremental Loading Flow</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md210", null ]
      ] ],
      [ "📈 <strong>Performance Impact Analysis</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md211", [
        [ "<strong>DungeonEditor Optimization</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md212", null ],
        [ "<strong>OverworldEditor Optimization</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md213", null ],
        [ "<strong>Overall System Impact</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md214", null ]
      ] ],
      [ "🔧 <strong>Configuration Options</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md215", [
        [ "<strong>Performance Monitoring</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md216", null ],
        [ "<strong>Parallel Loading Tuning</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md217", null ],
        [ "<strong>Incremental Loading Tuning</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md218", null ]
      ] ],
      [ "🎯 <strong>Future Optimization Opportunities</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md219", [
        [ "<strong>Potential Further Improvements</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md220", null ],
        [ "<strong>Monitoring and Profiling</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md221", null ]
      ] ],
      [ "✅ <strong>Success Metrics Achieved</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md222", null ],
      [ "🚀 <strong>Result: Lightning-Fast YAZE</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md223", null ]
    ] ],
    [ "Renderer Class Performance Analysis and Optimization", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html", [
      [ "Overview", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md225", null ],
      [ "Original Performance Issues", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md226", [
        [ "1. Blocking Texture Creation", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md227", null ],
        [ "2. Inefficient Loading Pattern", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md228", null ]
      ] ],
      [ "Optimizations Implemented", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md229", [
        [ "1. Deferred Texture Creation", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md230", null ],
        [ "2. Lazy Loading System", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md231", null ],
        [ "3. Performance Monitoring", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md232", null ]
      ] ],
      [ "Thread Safety Considerations", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md233", [
        [ "Main Thread Requirement", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md234", null ],
        [ "Safe Optimization Approach", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md235", null ]
      ] ],
      [ "Performance Improvements", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md236", [
        [ "Loading Time Reduction", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md237", null ],
        [ "Memory Efficiency", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md238", null ]
      ] ],
      [ "Implementation Details", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md239", [
        [ "Modified Files", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md240", null ],
        [ "Key Methods Added", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md241", null ]
      ] ],
      [ "Usage Guidelines", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md242", [
        [ "For Developers", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md243", null ],
        [ "For ROM Loading", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md244", null ]
      ] ],
      [ "Future Optimization Opportunities", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md245", [
        [ "1. Background Threading (Pending)", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md246", null ],
        [ "2. Arena Management Optimization (Pending)", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md247", null ],
        [ "3. Advanced Lazy Loading (Pending)", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md248", null ]
      ] ],
      [ "Conclusion", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md249", null ]
    ] ],
    [ "ZScream C# vs YAZE C++ Overworld Implementation Analysis", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html", [
      [ "Overview", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md251", null ],
      [ "Executive Summary", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md252", null ],
      [ "Detailed Comparison", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md253", [
        [ "1. Tile32 Loading and Expansion Detection", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md254", [
          [ "ZScream C# Logic (<tt>Overworld.cs:706-756</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md255", null ],
          [ "YAZE C++ Logic (<tt>overworld.cc:AssembleMap32Tiles</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md256", null ]
        ] ],
        [ "2. Tile16 Loading and Expansion Detection", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md257", [
          [ "ZScream C# Logic (<tt>Overworld.cs:652-705</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md258", null ],
          [ "YAZE C++ Logic (<tt>overworld.cc:AssembleMap16Tiles</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md259", null ]
        ] ],
        [ "3. Map Decompression", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md260", [
          [ "ZScream C# Logic (<tt>Overworld.cs:767-904</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md261", null ],
          [ "YAZE C++ Logic (<tt>overworld.cc:DecompressAllMapTiles</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md262", null ]
        ] ],
        [ "4. Entrance Coordinate Calculation", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md263", [
          [ "ZScream C# Logic (<tt>Overworld.cs:974-1001</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md264", null ],
          [ "YAZE C++ Logic (<tt>overworld.cc:LoadEntrances</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md265", null ]
        ] ],
        [ "5. Hole Coordinate Calculation with 0x400 Offset", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md266", [
          [ "ZScream C# Logic (<tt>Overworld.cs:1002-1025</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md267", null ],
          [ "YAZE C++ Logic (<tt>overworld.cc:LoadHoles</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md268", null ]
        ] ],
        [ "6. ASM Version Detection for Item Loading", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md269", [
          [ "ZScream C# Logic (<tt>Overworld.cs:1032-1094</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md270", null ],
          [ "YAZE C++ Logic (<tt>overworld.cc:LoadItems</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md271", null ]
        ] ],
        [ "7. Game State Handling for Sprite Loading", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md272", [
          [ "ZScream C# Logic (<tt>Overworld.cs:1276-1494</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md273", null ],
          [ "YAZE C++ Logic (<tt>overworld.cc:LoadSprites</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md274", null ]
        ] ],
        [ "8. Map Size Assignment Logic", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md275", [
          [ "ZScream C# Logic (<tt>Overworld.cs:296-390</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md276", null ],
          [ "YAZE C++ Logic (<tt>overworld.cc:AssignMapSizes</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md277", null ]
        ] ]
      ] ],
      [ "ZSCustomOverworld Integration", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md278", [
        [ "Version Detection", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md279", null ],
        [ "Feature Enablement", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md280", null ]
      ] ],
      [ "Integration Test Coverage", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md281", null ],
      [ "Conclusion", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md282", null ]
    ] ],
    [ "Atlas Rendering Implementation - YAZE Graphics Optimizations", "db/d26/md_docs_2atlas__rendering__implementation.html", [
      [ "Overview", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md284", null ],
      [ "Implementation Details", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md285", [
        [ "Core Components", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md286", [
          [ "1. AtlasRenderer Class (<tt>src/app/gfx/atlas_renderer.h/cc</tt>)", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md287", null ],
          [ "2. RenderCommand Structure", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md288", null ],
          [ "3. Atlas Statistics Tracking", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md289", null ]
        ] ],
        [ "Integration Points", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md290", [
          [ "1. Tilemap Integration (<tt>src/app/gfx/tilemap.h/cc</tt>)", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md291", null ],
          [ "2. Performance Dashboard Integration", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md292", null ],
          [ "3. Benchmarking Suite (<tt>test/gfx_optimization_benchmarks.cc</tt>)", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md293", null ]
        ] ],
        [ "Technical Implementation", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md294", [
          [ "Atlas Packing Algorithm", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md295", null ],
          [ "Batch Rendering Process", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md296", null ]
        ] ],
        [ "Performance Improvements", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md297", [
          [ "Measured Performance Gains", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md298", null ],
          [ "Benchmark Results", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md299", null ]
        ] ],
        [ "ROM Hacking Workflow Benefits", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md300", [
          [ "Graphics Sheet Management", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md301", null ],
          [ "Editor Performance", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md302", null ]
        ] ],
        [ "API Usage Examples", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md303", [
          [ "Basic Atlas Usage", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md304", null ],
          [ "Tilemap Integration", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md305", null ]
        ] ],
        [ "Memory Management", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md306", [
          [ "Automatic Cleanup", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md307", null ],
          [ "Resource Management", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md308", null ]
        ] ],
        [ "Future Enhancements", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md309", [
          [ "Planned Improvements", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md310", null ],
          [ "Integration Opportunities", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md311", null ]
        ] ]
      ] ],
      [ "Conclusion", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md312", null ],
      [ "Files Modified", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md313", null ]
    ] ],
    [ "Contributing", "dd/d5b/md_docs_2B1-contributing.html", [
      [ "Development Setup", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md315", [
        [ "Prerequisites", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md316", null ],
        [ "Quick Start", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md317", null ]
      ] ],
      [ "Code Style", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md318", [
        [ "C++ Standards", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md319", null ],
        [ "File Organization", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md320", null ],
        [ "Error Handling", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md321", null ]
      ] ],
      [ "Testing Requirements", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md322", [
        [ "Test Categories", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md323", null ],
        [ "Writing Tests", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md324", null ],
        [ "Test Execution", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md325", null ]
      ] ],
      [ "Pull Request Process", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md326", [
        [ "Before Submitting", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md327", null ],
        [ "Pull Request Template", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md328", null ]
      ] ],
      [ "Development Workflow", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md329", [
        [ "Branch Strategy", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md330", null ],
        [ "Commit Messages", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md331", null ],
        [ "Types", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md332", null ]
      ] ],
      [ "Architecture Guidelines", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md333", [
        [ "Component Design", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md334", null ],
        [ "Memory Management", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md335", null ],
        [ "Performance", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md336", null ]
      ] ],
      [ "Documentation", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md337", [
        [ "Code Documentation", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md338", null ],
        [ "API Documentation", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md339", null ]
      ] ],
      [ "Community Guidelines", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md340", [
        [ "Communication", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md341", null ],
        [ "Getting Help", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md342", null ]
      ] ],
      [ "Release Process", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md343", [
        [ "Version Numbering", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md344", null ],
        [ "Release Checklist", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md345", null ]
      ] ]
    ] ],
    [ "Platform Compatibility Improvements", "d1/d30/md_docs_2B2-platform-compatibility.html", [
      [ "Native File Dialog Support", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md347", null ],
      [ "Cross-Platform Build Reliability", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md348", null ],
      [ "Build Configuration Options", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md349", [
        [ "Full Build (Development)", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md350", null ],
        [ "Minimal Build", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md351", null ]
      ] ],
      [ "Implementation Details", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md352", null ],
      [ "Platform-Specific Adaptations", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md353", [
        [ "Windows", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md354", null ],
        [ "macOS", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md355", null ],
        [ "Linux", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md356", null ]
      ] ]
    ] ],
    [ "Build Presets Guide", "d7/d51/md_docs_2B3-build-presets.html", [
      [ "🍎 macOS ARM64 Presets (Recommended for Apple Silicon)", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md358", [
        [ "For Development Work:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md359", null ],
        [ "For Distribution:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md360", null ]
      ] ],
      [ "🔧 Why This Fixes Architecture Errors", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md361", null ],
      [ "📋 Available Presets", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md362", null ],
      [ "🚀 Quick Start", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md363", null ],
      [ "🛠️ IDE Integration", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md364", [
        [ "VS Code with CMake Tools:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md365", null ],
        [ "CLion:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md366", null ],
        [ "Xcode:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md367", null ]
      ] ],
      [ "🔍 Troubleshooting", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md368", null ],
      [ "📝 Notes", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md369", null ]
    ] ],
    [ "Release Workflows Documentation", "d3/d49/md_docs_2B4-release-workflows.html", [
      [ "Overview", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md371", null ],
      [ "1. Release-Simplified (<tt>release-simplified.yml</tt>)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md373", [
        [ "Purpose", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md374", null ],
        [ "Key Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md375", null ],
        [ "Use Cases", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md376", null ],
        [ "Configuration", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md377", null ],
        [ "Platforms Supported", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md378", null ]
      ] ],
      [ "2. Release (<tt>release.yml</tt>)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md380", [
        [ "Purpose", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md381", null ],
        [ "Key Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md382", null ],
        [ "Use Cases", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md383", null ],
        [ "Configuration", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md384", null ],
        [ "Advantages over Simplified", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md385", null ]
      ] ],
      [ "3. Release-Complex (<tt>release-complex.yml</tt>)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md387", [
        [ "Purpose", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md388", null ],
        [ "Key Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md389", null ],
        [ "Use Cases", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md390", null ],
        [ "Fallback Mechanisms", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md391", [
          [ "vcpkg Fallback", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md392", null ],
          [ "Chocolatey Integration", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md393", null ],
          [ "Build Configuration Fallback", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md394", null ]
        ] ],
        [ "Advanced Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md395", null ]
      ] ],
      [ "Workflow Comparison Matrix", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md397", null ],
      [ "When to Use Each Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md399", [
        [ "Use Simplified When:", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md400", null ],
        [ "Use Release When:", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md401", null ],
        [ "Use Complex When:", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md402", null ]
      ] ],
      [ "Workflow Selection Guide", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md404", [
        [ "For Development Team", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md405", null ],
        [ "For Release Manager", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md406", null ],
        [ "For CI/CD Pipeline", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md407", null ]
      ] ],
      [ "Configuration Examples", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md409", [
        [ "Triggering a Release", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md410", [
          [ "Manual Release (All Workflows)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md411", null ],
          [ "Automatic Release (Tag Push)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md412", null ]
        ] ],
        [ "Customizing Release Notes", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md413", null ]
      ] ],
      [ "Troubleshooting", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md415", [
        [ "Common Issues", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md416", [
          [ "vcpkg Failures (Windows)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md417", null ],
          [ "Dependency Conflicts", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md418", null ],
          [ "Build Failures", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md419", null ]
        ] ],
        [ "Debug Information", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md420", [
          [ "Simplified Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md421", null ],
          [ "Release Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md422", null ],
          [ "Complex Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md423", null ]
        ] ]
      ] ],
      [ "Best Practices", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md425", [
        [ "Workflow Selection", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md426", null ],
        [ "Release Process", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md427", null ],
        [ "Maintenance", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md428", null ]
      ] ],
      [ "Future Improvements", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md430", [
        [ "Planned Enhancements", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md431", null ],
        [ "Integration Opportunities", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md432", null ]
      ] ]
    ] ],
    [ "Stability, Testability & Release Workflow Improvements", "d9/d6b/md_docs_2B5-stability-improvements.html", [
      [ "Recent Improvements (v0.3.2)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md435", [
        [ "Windows Platform Stability", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md436", [
          [ "Stack Size Optimization", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md437", null ],
          [ "Development Utility Isolation", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md438", null ]
        ] ],
        [ "Graphics System Stability", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md439", [
          [ "Segmentation Fault Resolution", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md440", null ],
          [ "Comprehensive Bounds Checking", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md441", null ]
        ] ],
        [ "Build System Improvements", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md442", [
          [ "Modern Windows Workflow", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md443", null ],
          [ "Enhanced CI/CD Reliability", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md444", null ]
        ] ]
      ] ],
      [ "Recommended Optimizations", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md445", [
        [ "High Priority", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md446", [
          [ "1. Lazy Graphics Loading", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md447", null ],
          [ "2. Heap-Based Large Allocations", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md448", null ],
          [ "3. Streaming ROM Assets", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md449", null ]
        ] ],
        [ "Medium Priority", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md450", [
          [ "4. Enhanced Test Isolation", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md451", null ],
          [ "5. Dependency Caching Optimization", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md452", null ],
          [ "6. Memory Pool for Graphics", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md453", null ]
        ] ],
        [ "Low Priority", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md454", [
          [ "7. Build Time Optimization", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md455", null ],
          [ "8. Release Workflow Simplification", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md456", null ]
        ] ]
      ] ],
      [ "Testing Improvements", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md457", [
        [ "Current State", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md458", null ],
        [ "Recommendations", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md459", [
          [ "1. Visual Regression Testing", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md460", null ],
          [ "2. Performance Benchmarks", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md461", null ],
          [ "3. Fuzz Testing", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md462", null ]
        ] ]
      ] ],
      [ "Metrics & Monitoring", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md463", [
        [ "Current Measurements", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md464", null ],
        [ "Target Improvements", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md465", null ]
      ] ],
      [ "Action Items", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md466", [
        [ "Immediate (v0.3.2)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md467", null ],
        [ "Short Term (v0.3.3)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md468", null ],
        [ "Medium Term (v0.4.0)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md469", null ],
        [ "Long Term (v0.5.0+)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md470", null ]
      ] ],
      [ "Conclusion", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md471", null ]
    ] ],
    [ "Changelog", "d7/d6f/md_docs_2C1-changelog.html", [
      [ "0.3.2 (December 2025) - In Development", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md473", [
        [ "Tile16 Editor Improvements (In Progress)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md474", null ],
        [ "Graphics System Optimizations", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md475", null ],
        [ "Windows Platform Stability", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md476", null ],
        [ "Memory Safety & Stability", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md477", null ],
        [ "Testing & CI/CD Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md478", null ],
        [ "Future Optimizations (Planned)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md479", null ],
        [ "Technical Notes", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md480", null ]
      ] ],
      [ "0.3.1 (September 2025)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md481", [
        [ "Major Features", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md482", null ],
        [ "Tile16 Editor Enhancements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md483", null ],
        [ "ZSCustomOverworld v3 Implementation", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md484", null ],
        [ "Technical Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md485", null ],
        [ "User Interface", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md486", null ],
        [ "Bug Fixes", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md487", null ],
        [ "ZScream Compatibility Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md488", null ]
      ] ],
      [ "0.3.0 (September 2025)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md489", [
        [ "Major Features", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md490", null ],
        [ "User Interface & Theming", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md491", null ],
        [ "Enhancements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md492", null ],
        [ "Technical Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md493", null ],
        [ "Bug Fixes", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md494", null ]
      ] ],
      [ "0.2.2 (December 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md495", null ],
      [ "0.2.1 (August 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md496", null ],
      [ "0.2.0 (July 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md497", null ],
      [ "0.1.0 (May 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md498", null ],
      [ "0.0.9 (April 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md499", null ],
      [ "0.0.8 (February 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md500", null ],
      [ "0.0.7 (January 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md501", null ],
      [ "0.0.6 (November 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md502", null ],
      [ "0.0.5 (November 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md503", null ],
      [ "0.0.4 (November 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md504", null ],
      [ "0.0.3 (October 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md505", null ]
    ] ],
    [ "Canvas System - Comprehensive Guide", "d2/db2/md_docs_2CANVAS__GUIDE.html", [
      [ "Overview", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md507", null ],
      [ "Core Concepts", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md508", [
        [ "Canvas Structure", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md509", null ],
        [ "Coordinate Systems", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md510", null ]
      ] ],
      [ "Usage Patterns", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md511", [
        [ "Pattern 1: Basic Bitmap Display", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md512", null ],
        [ "Pattern 2: Modern Begin/End", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md513", null ],
        [ "Pattern 3: RAII ScopedCanvas", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md514", null ]
      ] ],
      [ "Feature: Tile Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md515", [
        [ "Single Tile Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md516", null ],
        [ "Tilemap Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md517", null ],
        [ "Color Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md518", null ]
      ] ],
      [ "Feature: Tile Selection", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md519", [
        [ "Single Tile Selection", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md520", null ],
        [ "Multi-Tile Rectangle Selection", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md521", null ],
        [ "Rectangle Drag & Paint", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md522", null ]
      ] ],
      [ "Feature: Custom Overlays", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md523", [
        [ "Manual Points Manipulation", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md524", null ]
      ] ],
      [ "Feature: Large Map Support", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md525", [
        [ "Map Types", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md526", null ],
        [ "Boundary Clamping", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md527", null ],
        [ "Custom Map Sizes", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md528", null ]
      ] ],
      [ "Feature: Context Menu", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md529", [
        [ "Adding Custom Items", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md530", null ],
        [ "Overworld Editor Example", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md531", null ]
      ] ],
      [ "Feature: Scratch Space (In Progress)", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md532", null ],
      [ "Common Workflows", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md533", [
        [ "Workflow 1: Overworld Tile Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md534", null ],
        [ "Workflow 2: Tile16 Blockset Selection", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md535", null ],
        [ "Workflow 3: Graphics Sheet Display", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md536", null ]
      ] ],
      [ "Configuration", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md537", [
        [ "Grid Settings", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md538", null ],
        [ "Scale Settings", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md539", null ],
        [ "Interaction Settings", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md540", null ],
        [ "Large Map Settings", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md541", null ]
      ] ],
      [ "Bug Fixes Applied", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md542", [
        [ "1. Rectangle Selection Wrapping in Large Maps ✅", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md543", null ],
        [ "2. Drag-Time Preview Clamping ✅", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md544", null ]
      ] ],
      [ "API Reference", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md545", [
        [ "Drawing Methods", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md546", null ],
        [ "State Accessors", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md547", null ],
        [ "Configuration", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md548", null ]
      ] ],
      [ "Implementation Notes", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md549", [
        [ "Points Management", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md550", null ],
        [ "Selection State", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md551", null ],
        [ "Overworld Rectangle Painting Flow", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md552", null ]
      ] ],
      [ "Best Practices", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md553", [
        [ "DO ✅", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md554", null ],
        [ "DON'T ❌", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md555", null ]
      ] ],
      [ "Troubleshooting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md556", [
        [ "Issue: Rectangle wraps at boundaries", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md557", null ],
        [ "Issue: Painting in wrong location", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md558", null ],
        [ "Issue: Array index out of bounds", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md559", null ],
        [ "Issue: Forgot to call End()", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md560", null ]
      ] ],
      [ "Future: Scratch Space", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md561", null ],
      [ "Documentation Files", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md562", null ],
      [ "Summary", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md563", null ]
    ] ],
    [ "Canvas Refactoring - Current Status & Future Work", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html", [
      [ "✅ Successfully Completed", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md565", [
        [ "1. Modern ImGui-Style Interface (Working)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md566", null ],
        [ "2. Context Menu Improvements (Working)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md567", null ],
        [ "3. Optional CanvasInteractionHandler Component (Available)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md568", null ],
        [ "4. Code Cleanup", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md569", null ]
      ] ],
      [ "⚠️ Outstanding Issue: Rectangle Selection Wrapping", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md570", [
        [ "The Problem", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md571", null ],
        [ "What Was Attempted", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md572", null ],
        [ "Root Cause Analysis", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md573", null ],
        [ "Suspected Issue", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md574", null ],
        [ "What Needs Investigation", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md575", null ],
        [ "Debugging Steps for Future Agent", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md576", null ],
        [ "Possible Fixes to Try", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md577", null ]
      ] ],
      [ "🔧 Files Modified", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md578", [
        [ "Core Canvas", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md579", null ],
        [ "Overworld Editor", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md580", null ],
        [ "Components Created", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md581", null ]
      ] ],
      [ "📚 Documentation (Consolidated)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md582", null ],
      [ "🎯 Future Refactoring Steps", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md583", [
        [ "Priority 1: Fix Rectangle Wrapping (High)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md584", null ],
        [ "Priority 2: Extract Coordinate Conversion Helpers (Low Impact)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md585", null ],
        [ "Priority 3: Move Components to canvas/ Namespace (Organizational)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md586", null ],
        [ "Priority 4: Complete Scratch Space Feature (Feature)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md587", null ],
        [ "Priority 5: Simplify Canvas State (Refactoring)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md588", null ]
      ] ],
      [ "🔍 Known Working Patterns", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md589", [
        [ "Overworld Tile Painting (Working)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md590", null ],
        [ "Blockset Selection (Working)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md591", null ],
        [ "Manual Overlay Highlighting (Working)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md592", null ]
      ] ],
      [ "🎓 Lessons Learned", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md593", [
        [ "What Worked", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md594", null ],
        [ "What Didn't Work", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md595", null ],
        [ "Key Insights", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md596", null ]
      ] ],
      [ "📋 For Future Agent", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md597", [
        [ "Immediate Task: Fix Rectangle Wrapping", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md598", null ],
        [ "Medium Term: Namespace Organization", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md599", null ],
        [ "Long Term: State Management Simplification", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md600", null ],
        [ "Stretch Goals: Enhanced Features", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md601", null ]
      ] ],
      [ "📊 Current Metrics", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md602", null ],
      [ "🔑 Key Files Reference", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md603", [
        [ "Core Canvas", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md604", null ],
        [ "Canvas Components", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md605", null ],
        [ "Utilities", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md606", null ],
        [ "Major Consumers", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md607", null ]
      ] ],
      [ "🎯 Recommended Next Steps", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md608", [
        [ "Step 1: Fix Rectangle Wrapping Bug (Critical)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md609", null ],
        [ "Step 2: Test All Editors (Verification)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md610", null ],
        [ "Step 3: Adopt Modern Patterns (Optional)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md611", null ]
      ] ],
      [ "📖 Documentation", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md612", [
        [ "Read This", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md613", null ],
        [ "Background (Optional)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md614", null ]
      ] ],
      [ "💡 Quick Reference", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md615", [
        [ "Modern Usage", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md616", null ],
        [ "Legacy Usage (Still Works)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md617", null ],
        [ "Revert Clamping", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md618", null ],
        [ "Add Context Menu", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md619", null ]
      ] ],
      [ "✅ Current Status", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md620", null ]
    ] ],
    [ "Roadmap", "d6/db4/md_docs_2D1-roadmap.html", [
      [ "0.4.X (Next Major Release)", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md623", [
        [ "Core Features", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md624", null ],
        [ "Technical Improvements", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md625", null ]
      ] ],
      [ "0.5.X", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md626", [
        [ "Advanced Features", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md627", null ]
      ] ],
      [ "0.6.X", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md628", [
        [ "Platform & Integration", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md629", null ]
      ] ],
      [ "0.7.X", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md630", [
        [ "Performance & Polish", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md631", null ]
      ] ],
      [ "0.8.X", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md632", [
        [ "Beta Preparation", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md633", null ]
      ] ],
      [ "1.0.0", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md634", [
        [ "Stable Release", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md635", null ]
      ] ],
      [ "Current Focus Areas", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md636", [
        [ "Immediate Priorities (v0.4.X)", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md637", null ],
        [ "Long-term Vision", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md638", null ]
      ] ]
    ] ],
    [ "Asm Style Guide", "d7/d9a/md_docs_2E1-asm-style-guide.html", [
      [ "Table of Contents", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md640", null ],
      [ "File Structure", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md641", null ],
      [ "Labels and Symbols", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md642", null ],
      [ "Comments", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md643", null ],
      [ "Instructions", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md644", null ],
      [ "Macros", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md645", null ],
      [ "Loops and Branching", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md646", null ],
      [ "Data Structures", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md647", null ],
      [ "Code Organization", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md648", null ],
      [ "Custom Code", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md649", null ]
    ] ],
    [ "Dungeon Editor Guide", "dd/d33/md_docs_2E2-dungeon-editor-guide.html", [
      [ "Overview", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md651", null ],
      [ "Architecture", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md652", [
        [ "Core Components", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md653", [
          [ "1. DungeonEditorSystem", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md654", null ],
          [ "2. DungeonObjectEditor", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md655", null ],
          [ "3. ObjectRenderer", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md656", null ],
          [ "4. DungeonEditor (UI Layer)", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md657", null ]
        ] ]
      ] ],
      [ "Coordinate System", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md658", [
        [ "Room Coordinates vs Canvas Coordinates", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md659", [
          [ "Conversion Functions", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md660", null ]
        ] ],
        [ "Coordinate System Features", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md661", null ]
      ] ],
      [ "Object Rendering System", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md662", [
        [ "Object Types", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md663", null ],
        [ "Rendering Pipeline", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md664", null ],
        [ "Performance Optimizations", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md665", null ]
      ] ],
      [ "User Interface", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md666", [
        [ "Integrated Editing Panels", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md667", [
          [ "Main Canvas", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md668", null ],
          [ "Compact Editing Panels", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md669", null ]
        ] ],
        [ "Object Preview System", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md670", null ]
      ] ],
      [ "Integration with ZScream", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md671", [
        [ "Room Loading", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md672", null ],
        [ "Object Parsing", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md673", null ],
        [ "Coordinate System", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md674", null ]
      ] ],
      [ "Testing and Validation", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md675", [
        [ "Integration Tests", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md676", null ],
        [ "Test Data", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md677", null ],
        [ "Performance Benchmarks", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md678", null ]
      ] ],
      [ "Usage Examples", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md679", [
        [ "Basic Object Editing", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md680", null ],
        [ "Coordinate Conversion", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md681", null ],
        [ "Object Preview", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md682", null ]
      ] ],
      [ "Configuration Options", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md683", [
        [ "Editor Configuration", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md684", null ],
        [ "Performance Configuration", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md685", null ]
      ] ],
      [ "Troubleshooting", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md686", [
        [ "Common Issues", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md687", null ],
        [ "Debug Information", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md688", null ]
      ] ],
      [ "Future Enhancements", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md689", [
        [ "Planned Features", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md690", null ]
      ] ],
      [ "Conclusion", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md691", null ]
    ] ],
    [ "Dungeon Editor Design Plan", "dc/d82/md_docs_2E3-dungeon-editor-design.html", [
      [ "Overview", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md693", null ],
      [ "Architecture Overview", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md694", [
        [ "Core Components", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md695", null ],
        [ "File Structure", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md696", null ]
      ] ],
      [ "Component Responsibilities", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md697", [
        [ "DungeonEditor (Main Orchestrator)", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md698", null ],
        [ "DungeonRoomSelector", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md699", null ],
        [ "DungeonCanvasViewer", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md700", null ],
        [ "DungeonObjectSelector", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md701", null ]
      ] ],
      [ "Data Flow", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md702", [
        [ "Initialization Flow", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md703", null ],
        [ "Runtime Data Flow", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md704", null ],
        [ "Key Data Structures", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md705", null ]
      ] ],
      [ "Integration Patterns", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md706", [
        [ "Component Communication", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md707", null ],
        [ "ROM Data Management", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md708", null ],
        [ "State Synchronization", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md709", null ]
      ] ],
      [ "UI Layout Architecture", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md710", [
        [ "3-Column Layout", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md711", null ],
        [ "Component Internal Layout", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md712", null ]
      ] ],
      [ "Coordinate System", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md713", [
        [ "Room Coordinates vs Canvas Coordinates", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md714", null ],
        [ "Bounds Checking", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md715", null ]
      ] ],
      [ "Error Handling & Validation", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md716", [
        [ "ROM Validation", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md717", null ],
        [ "Bounds Validation", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md718", null ]
      ] ],
      [ "Performance Considerations", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md719", [
        [ "Caching Strategy", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md720", null ],
        [ "Rendering Optimization", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md721", null ]
      ] ],
      [ "Testing Strategy", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md722", [
        [ "Integration Tests", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md723", null ],
        [ "Test Categories", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md724", null ]
      ] ],
      [ "Future Development Guidelines", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md725", [
        [ "Adding New Features", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md726", null ],
        [ "Component Extension Patterns", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md727", null ],
        [ "Data Flow Extension", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md728", null ],
        [ "UI Layout Extension", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md729", null ]
      ] ],
      [ "Common Pitfalls & Solutions", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md730", [
        [ "Memory Management", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md731", null ],
        [ "Coordinate System", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md732", null ],
        [ "State Synchronization", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md733", null ],
        [ "Performance Issues", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md734", null ]
      ] ],
      [ "Debugging Tools", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md735", [
        [ "Debug Popup", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md736", null ],
        [ "Logging", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md737", null ]
      ] ],
      [ "Build Integration", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md738", [
        [ "CMake Configuration", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md739", null ],
        [ "Dependencies", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md740", null ]
      ] ],
      [ "Conclusion", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md741", null ]
    ] ],
    [ "DungeonEditor Refactoring Plan", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html", [
      [ "Overview", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md743", null ],
      [ "Component Structure", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md744", [
        [ "✅ Created Components", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md745", [
          [ "1. DungeonToolset (<tt>dungeon_toolset.h/cc</tt>)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md746", null ],
          [ "2. DungeonObjectInteraction (<tt>dungeon_object_interaction.h/cc</tt>)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md747", null ],
          [ "3. DungeonRenderer (<tt>dungeon_renderer.h/cc</tt>)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md748", null ],
          [ "4. DungeonRoomLoader (<tt>dungeon_room_loader.h/cc</tt>)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md749", null ],
          [ "5. DungeonUsageTracker (<tt>dungeon_usage_tracker.h/cc</tt>)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md750", null ]
        ] ]
      ] ],
      [ "Refactored DungeonEditor Structure", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md751", [
        [ "Before Refactoring: 1444 lines", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md752", null ],
        [ "After Refactoring: ~400 lines", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md753", null ]
      ] ],
      [ "Method Migration Map", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md754", [
        [ "Core Editor Methods (Keep in main file)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md755", null ],
        [ "UI Methods (Keep for coordination)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md756", null ],
        [ "Methods Moved to Components", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md757", [
          [ "→ DungeonToolset", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md758", null ],
          [ "→ DungeonObjectInteraction", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md759", null ],
          [ "→ DungeonRenderer", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md760", null ],
          [ "→ DungeonRoomLoader", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md761", null ],
          [ "→ DungeonUsageTracker", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md762", null ]
        ] ]
      ] ],
      [ "Component Communication", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md763", [
        [ "Callback System", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md764", null ],
        [ "Data Sharing", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md765", null ]
      ] ],
      [ "Benefits of Refactoring", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md766", [
        [ "1. <strong>Reduced Complexity</strong>", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md767", null ],
        [ "2. <strong>Improved Testability</strong>", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md768", null ],
        [ "3. <strong>Better Maintainability</strong>", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md769", null ],
        [ "4. <strong>Enhanced Extensibility</strong>", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md770", null ],
        [ "5. <strong>Cleaner Dependencies</strong>", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md771", null ]
      ] ],
      [ "Implementation Status", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md772", [
        [ "✅ Completed", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md773", null ],
        [ "🔄 In Progress", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md774", null ],
        [ "⏳ Pending", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md775", null ]
      ] ],
      [ "Migration Strategy", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md776", [
        [ "Phase 1: Create Components ✅", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md777", null ],
        [ "Phase 2: Integrate Components 🔄", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md778", null ],
        [ "Phase 3: Move Methods", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md779", null ],
        [ "Phase 4: Cleanup", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md780", null ]
      ] ]
    ] ],
    [ "Dungeon Object System", "da/d11/md_docs_2E5-dungeon-object-system.html", [
      [ "Overview", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md782", null ],
      [ "Architecture", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md783", [
        [ "Core Components", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md784", [
          [ "1. DungeonEditor (<tt>src/app/editor/dungeon/dungeon_editor.h</tt>)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md785", null ],
          [ "2. DungeonObjectSelector (<tt>src/app/editor/dungeon/dungeon_object_selector.h</tt>)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md786", null ],
          [ "3. DungeonCanvasViewer (<tt>src/app/editor/dungeon/dungeon_canvas_viewer.h</tt>)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md787", null ],
          [ "4. Room Management System (<tt>src/app/zelda3/dungeon/room.h</tt>)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md788", null ]
        ] ]
      ] ],
      [ "Object Types and Hierarchies", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md789", [
        [ "Room Objects", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md790", [
          [ "Type 1 Objects (0x00-0xFF)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md791", null ],
          [ "Type 2 Objects (0x100-0x1FF)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md792", null ],
          [ "Type 3 Objects (0x200+)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md793", null ]
        ] ],
        [ "Object Properties", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md794", null ]
      ] ],
      [ "How Object Placement Works", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md795", [
        [ "Selection Process", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md796", null ],
        [ "Placement Process", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md797", null ],
        [ "Code Flow", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md798", null ]
      ] ],
      [ "Rendering Pipeline", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md799", [
        [ "Object Rendering", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md800", null ],
        [ "Performance Optimizations", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md801", null ]
      ] ],
      [ "User Interface Components", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md802", [
        [ "Three-Column Layout", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md803", [
          [ "Column 1: Room Control Panel (280px fixed)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md804", null ],
          [ "Column 2: Windowed Canvas (800px fixed)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md805", null ],
          [ "Column 3: Object Selector/Editor (stretch)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md806", null ]
        ] ],
        [ "Debug and Control Features", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md807", [
          [ "Room Properties Table", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md808", null ],
          [ "Object Statistics", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md809", null ]
        ] ]
      ] ],
      [ "Integration with ROM Data", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md810", [
        [ "Data Sources", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md811", [
          [ "Room Headers (<tt>0x1F8000</tt>)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md812", null ],
          [ "Object Data", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md813", null ],
          [ "Graphics Data", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md814", null ]
        ] ],
        [ "Assembly Integration", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md815", null ]
      ] ],
      [ "Comparison with ZScream", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md816", [
        [ "Architectural Differences", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md817", [
          [ "Component-Based Architecture", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md818", null ],
          [ "Real-time Rendering", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md819", null ],
          [ "UI Organization", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md820", null ],
          [ "Caching Strategy", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md821", null ]
        ] ],
        [ "Shared Concepts", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md822", null ]
      ] ],
      [ "Best Practices", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md823", [
        [ "Performance", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md824", null ],
        [ "Code Organization", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md825", null ],
        [ "User Experience", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md826", null ]
      ] ],
      [ "Future Enhancements", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md827", [
        [ "Planned Features", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md828", null ],
        [ "Technical Improvements", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md829", null ]
      ] ]
    ] ],
    [ "Overworld Loading Guide", "dc/d12/md_docs_2F1-overworld-loading.html", [
      [ "Table of Contents", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md831", null ],
      [ "Overview", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md832", null ],
      [ "ROM Types and Versions", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md833", [
        [ "Version Detection", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md834", null ],
        [ "Feature Support by Version", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md835", null ]
      ] ],
      [ "Overworld Map Structure", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md836", [
        [ "Core Properties", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md837", null ]
      ] ],
      [ "Overlays and Special Area Maps", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md838", [
        [ "Understanding Overlays", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md839", null ],
        [ "Special Area Maps (0x80-0x9F)", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md840", null ],
        [ "Overlay ID Mappings", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md841", null ],
        [ "Drawing Order", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md842", null ],
        [ "Vanilla Overlay Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md843", null ],
        [ "Special Area Graphics Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md844", null ]
      ] ],
      [ "Loading Process", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md845", [
        [ "1. Version Detection", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md846", null ],
        [ "2. Map Initialization", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md847", null ],
        [ "3. Property Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md848", [
          [ "Vanilla ROMs (asm_version == 0xFF)", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md849", null ],
          [ "ZSCustomOverworld v2/v3", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md850", null ]
        ] ],
        [ "4. Custom Data Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md851", null ]
      ] ],
      [ "ZScream Implementation", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md852", [
        [ "OverworldMap Constructor", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md853", null ],
        [ "Key Methods", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md854", null ]
      ] ],
      [ "Yaze Implementation", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md855", [
        [ "OverworldMap Constructor", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md856", null ],
        [ "Key Methods", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md857", null ],
        [ "Current Status", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md858", null ]
      ] ],
      [ "Key Differences", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md859", [
        [ "1. Language and Architecture", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md860", null ],
        [ "2. Data Structures", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md861", null ],
        [ "3. Error Handling", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md862", null ],
        [ "4. Graphics Processing", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md863", null ]
      ] ],
      [ "Common Issues and Solutions", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md864", [
        [ "1. Version Detection Issues", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md865", null ],
        [ "2. Palette Loading Errors", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md866", null ],
        [ "3. Graphics Not Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md867", null ],
        [ "4. Overlay Issues", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md868", null ],
        [ "5. Large Map Problems", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md869", null ],
        [ "6. Special Area Graphics Issues", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md870", null ]
      ] ],
      [ "Best Practices", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md871", [
        [ "1. Version-Specific Code", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md872", null ],
        [ "2. Error Handling", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md873", null ],
        [ "3. Memory Management", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md874", null ],
        [ "4. Thread Safety", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md875", null ]
      ] ],
      [ "Conclusion", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md876", null ]
    ] ],
    [ "YAZE Graphics System Improvements Summary", "d9/df4/md_docs_2gfx__improvements__summary.html", [
      [ "Overview", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md878", null ],
      [ "Files Modified", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md879", [
        [ "Core Graphics Classes", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md880", [
          [ "1. <tt>/src/app/gfx/bitmap.h</tt>", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md881", null ],
          [ "2. <tt>/src/app/gfx/bitmap.cc</tt>", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md882", null ],
          [ "3. <tt>/src/app/gfx/arena.h</tt>", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md883", null ],
          [ "4. <tt>/src/app/gfx/arena.cc</tt>", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md884", null ],
          [ "5. <tt>/src/app/gfx/tilemap.h</tt>", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md885", null ]
        ] ],
        [ "Editor Classes", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md886", [
          [ "6. <tt>/src/app/editor/graphics/graphics_editor.cc</tt>", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md887", null ],
          [ "7. <tt>/src/app/editor/graphics/palette_editor.cc</tt>", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md888", null ],
          [ "8. <tt>/src/app/editor/graphics/screen_editor.cc</tt>", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md889", null ]
        ] ]
      ] ],
      [ "New Documentation Files", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md890", [
        [ "9. <tt>/docs/gfx_optimization_recommendations.md</tt>", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md891", null ]
      ] ],
      [ "Performance Optimization Recommendations", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md892", [
        [ "High Impact, Low Risk (Phase 1)", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md893", null ],
        [ "Medium Impact, Medium Risk (Phase 2)", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md894", null ],
        [ "High Impact, High Risk (Phase 3)", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md895", null ]
      ] ],
      [ "ROM Hacking Workflow Improvements", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md896", [
        [ "Graphics Editor Enhancements", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md897", null ],
        [ "Palette Editor Enhancements", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md898", null ],
        [ "Screen Editor Enhancements", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md899", null ]
      ] ],
      [ "Code Quality Improvements", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md900", [
        [ "Documentation Standards", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md901", null ],
        [ "Code Organization", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md902", null ]
      ] ],
      [ "Future Development Recommendations", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md903", [
        [ "Immediate Improvements", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md904", null ],
        [ "Medium-term Enhancements", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md905", null ],
        [ "Long-term Goals", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md906", null ]
      ] ],
      [ "Conclusion", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md907", null ]
    ] ],
    [ "YAZE Graphics System Optimization Recommendations", "de/def/md_docs_2gfx__optimization__recommendations.html", [
      [ "Overview", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md909", null ],
      [ "Current Architecture Analysis", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md910", [
        [ "Strengths", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md911", null ],
        [ "Performance Bottlenecks Identified", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md912", [
          [ "1. Bitmap Class Issues", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md913", null ],
          [ "2. Arena Resource Management", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md914", null ],
          [ "3. Tilemap Performance", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md915", null ]
        ] ]
      ] ],
      [ "Optimization Recommendations", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md916", [
        [ "1. Bitmap Class Optimizations", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md917", [
          [ "A. Palette Lookup Optimization", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md918", null ],
          [ "B. Dirty Region Tracking", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md919", null ]
        ] ],
        [ "2. Arena Resource Management Improvements", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md920", [
          [ "A. Resource Pooling", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md921", null ],
          [ "B. Batch Operations", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md922", null ]
        ] ],
        [ "3. Tilemap Performance Enhancements", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md923", [
          [ "A. Smart Tile Caching", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md924", null ],
          [ "B. Atlas-based Rendering", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md925", null ]
        ] ],
        [ "4. Editor-Specific Optimizations", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md926", [
          [ "A. Graphics Editor Improvements", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md927", null ],
          [ "B. Palette Editor Optimizations", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md928", null ]
        ] ],
        [ "5. Memory Management Improvements", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md929", [
          [ "A. Custom Allocator for Graphics Data", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md930", null ],
          [ "B. Smart Pointer Management", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md931", null ]
        ] ]
      ] ],
      [ "Implementation Priority", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md932", [
        [ "Phase 1 (High Impact, Low Risk)", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md933", null ],
        [ "Phase 2 (Medium Impact, Medium Risk)", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md934", null ],
        [ "Phase 3 (High Impact, High Risk)", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md935", null ]
      ] ],
      [ "Performance Metrics", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md936", [
        [ "Target Improvements", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md937", null ],
        [ "Measurement Tools", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md938", null ]
      ] ],
      [ "Conclusion", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md939", null ]
    ] ],
    [ "YAZE Graphics System Optimizations - Complete Implementation", "d6/df4/md_docs_2gfx__optimizations__complete.html", [
      [ "Overview", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md941", null ],
      [ "Implemented Optimizations", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md942", [
        [ "1. Palette Lookup Optimization ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md943", null ],
        [ "2. Dirty Region Tracking ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md944", null ],
        [ "3. Resource Pooling ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md945", null ],
        [ "4. LRU Tile Caching ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md946", null ],
        [ "5. Batch Operations ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md947", null ],
        [ "6. Memory Pool Allocator ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md948", null ],
        [ "7. Atlas-Based Rendering ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md949", null ],
        [ "8. Performance Profiling System ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md950", null ],
        [ "9. Performance Monitoring Dashboard ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md951", null ],
        [ "10. Optimization Validation Suite ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md952", null ]
      ] ],
      [ "Performance Metrics", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md953", [
        [ "Expected Improvements", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md954", null ],
        [ "Measurement Tools", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md955", null ]
      ] ],
      [ "Integration Points", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md956", [
        [ "Graphics Editor", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md957", null ],
        [ "Palette Editor", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md958", null ],
        [ "Screen Editor", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md959", null ]
      ] ],
      [ "Backward Compatibility", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md960", null ],
      [ "Usage Examples", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md961", [
        [ "Using Batch Operations", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md962", null ],
        [ "Using Memory Pool", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md963", null ],
        [ "Using Atlas Rendering", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md964", null ],
        [ "Using Performance Monitoring", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md965", null ]
      ] ],
      [ "Future Enhancements", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md966", [
        [ "Phase 2 Optimizations (Medium Priority)", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md967", null ],
        [ "Phase 3 Optimizations (High Priority)", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md968", null ]
      ] ],
      [ "Testing and Validation", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md969", [
        [ "Performance Testing", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md970", null ],
        [ "ROM Hacking Workflow Testing", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md971", null ]
      ] ],
      [ "Conclusion", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md972", null ],
      [ "Files Modified/Created", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md973", [
        [ "Core Graphics Classes", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md974", null ],
        [ "New Optimization Components", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md975", null ],
        [ "Testing and Validation", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md976", null ],
        [ "Build System", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md977", null ],
        [ "Documentation", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md978", null ]
      ] ]
    ] ],
    [ "YAZE Graphics System Optimizations - Implementation Summary", "d3/d7b/md_docs_2gfx__optimizations__implemented.html", [
      [ "Overview", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md980", null ],
      [ "Implemented Optimizations", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md981", [
        [ "1. Palette Lookup Optimization ✅ COMPLETED", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md982", null ],
        [ "2. Dirty Region Tracking ✅ COMPLETED", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md983", null ],
        [ "3. Resource Pooling ✅ COMPLETED", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md984", null ],
        [ "4. LRU Tile Caching ✅ COMPLETED", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md985", null ],
        [ "5. Region-Specific Texture Updates ✅ COMPLETED", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md986", null ],
        [ "6. Performance Profiling System ✅ COMPLETED", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md987", null ]
      ] ],
      [ "Performance Metrics", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md988", [
        [ "Expected Improvements", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md989", null ],
        [ "Measurement Tools", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md990", null ]
      ] ],
      [ "Integration Points", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md991", [
        [ "Graphics Editor", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md992", null ],
        [ "Palette Editor", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md993", null ],
        [ "Screen Editor", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md994", null ]
      ] ],
      [ "Backward Compatibility", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md995", null ],
      [ "Future Enhancements", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md996", [
        [ "Phase 2 Optimizations (Medium Priority)", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md997", null ],
        [ "Phase 3 Optimizations (High Priority)", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md998", null ]
      ] ],
      [ "Testing and Validation", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md999", [
        [ "Performance Testing", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1000", null ],
        [ "ROM Hacking Workflow Testing", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1001", null ]
      ] ],
      [ "Conclusion", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md1002", null ]
    ] ],
    [ "YAZE Graphics Optimizations Project - Final Summary", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html", [
      [ "Project Overview", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1004", null ],
      [ "Completed Optimizations", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1005", [
        [ "✅ 1. Batch Operations for Texture Updates", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1006", null ],
        [ "✅ 2. Memory Pool Allocator", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1007", null ],
        [ "✅ 3. Atlas-Based Rendering System", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1008", null ],
        [ "✅ 4. Performance Monitoring Dashboard", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1009", null ],
        [ "✅ 5. Optimization Validation Suite", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1010", null ],
        [ "✅ 6. Debug Menu Integration", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1011", null ]
      ] ],
      [ "Performance Metrics Achieved", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1012", [
        [ "Expected Improvements (Based on Implementation)", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1013", null ],
        [ "Real Performance Data (From Timing Report)", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1014", null ]
      ] ],
      [ "Technical Implementation Details", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1015", [
        [ "Architecture Improvements", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1016", null ],
        [ "Code Quality Enhancements", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1017", null ]
      ] ],
      [ "Integration Points", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1018", [
        [ "Graphics System Integration", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1019", null ],
        [ "Editor Integration", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1020", null ]
      ] ],
      [ "Future Enhancements", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1021", [
        [ "Remaining Optimization (Pending)", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1022", null ],
        [ "Potential Extensions", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1023", null ]
      ] ],
      [ "Build and Testing", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1024", [
        [ "Build Status", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1025", null ],
        [ "Testing Status", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1026", null ]
      ] ],
      [ "Conclusion", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md1027", null ]
    ] ],
    [ "YAZE Documentation", "d3/d4c/md_docs_2index.html", [
      [ "Quick Start", "d3/d4c/md_docs_2index.html#autotoc_md1029", null ],
      [ "Development", "d3/d4c/md_docs_2index.html#autotoc_md1030", null ],
      [ "Technical Documentation", "d3/d4c/md_docs_2index.html#autotoc_md1031", [
        [ "Assembly & Code", "d3/d4c/md_docs_2index.html#autotoc_md1032", null ],
        [ "Editor Systems", "d3/d4c/md_docs_2index.html#autotoc_md1033", null ],
        [ "Overworld System", "d3/d4c/md_docs_2index.html#autotoc_md1034", null ]
      ] ],
      [ "Key Features", "d3/d4c/md_docs_2index.html#autotoc_md1035", null ]
    ] ],
    [ "YAZE Overworld Testing Guide", "de/d8a/md_docs_2overworld__testing__guide.html", [
      [ "Overview", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1038", null ],
      [ "Test Architecture", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1039", [
        [ "1. Golden Data System", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1040", null ],
        [ "2. Test Categories", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1041", [
          [ "Unit Tests (<tt>test/unit/zelda3/</tt>)", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1042", null ],
          [ "Integration Tests (<tt>test/e2e/</tt>)", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1043", null ],
          [ "Golden Data Tools", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1044", null ]
        ] ]
      ] ],
      [ "Quick Start", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1045", [
        [ "Prerequisites", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1046", null ],
        [ "Running Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1047", [
          [ "Basic Test Run", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1048", null ],
          [ "Selective Test Execution", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1049", null ]
        ] ]
      ] ],
      [ "Test Components", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1050", [
        [ "1. Golden Data Extractor", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1051", null ],
        [ "2. Integration Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1052", null ],
        [ "3. End-to-End Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1053", null ]
      ] ],
      [ "Test Validation Points", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1054", [
        [ "1. ZScream Compatibility", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1055", null ],
        [ "2. ROM State Validation", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1056", null ],
        [ "3. Performance and Stability", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1057", null ]
      ] ],
      [ "Environment Variables", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1058", [
        [ "Test Configuration", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1059", null ],
        [ "Build Configuration", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1060", null ]
      ] ],
      [ "Test Reports", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1061", [
        [ "Generated Reports", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1062", null ],
        [ "Report Location", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1063", null ]
      ] ],
      [ "Troubleshooting", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1064", [
        [ "Common Issues", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1065", [
          [ "1. ROM Not Found", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1066", null ],
          [ "2. Build Failures", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1067", null ],
          [ "3. Test Failures", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1068", null ]
        ] ],
        [ "Debug Mode", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1069", null ]
      ] ],
      [ "Advanced Usage", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1070", [
        [ "Custom Test Scenarios", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1071", [
          [ "1. Testing Custom ROMs", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1072", null ],
          [ "2. Regression Testing", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1073", null ],
          [ "3. Performance Testing", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1074", null ]
        ] ]
      ] ],
      [ "Integration with CI/CD", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1075", [
        [ "GitHub Actions Example", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1076", null ]
      ] ],
      [ "Contributing", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1077", [
        [ "Adding New Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1078", null ],
        [ "Test Guidelines", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1079", null ],
        [ "Example Test Structure", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1080", null ]
      ] ],
      [ "Conclusion", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1081", null ]
    ] ],
    [ "YAZE - Yet Another Zelda3 Editor", "d0/d30/md_README.html", [
      [ "Version 0.3.2 - Release", "d0/d30/md_README.html#autotoc_md1083", [
        [ "🛠️ Technical Improvements", "d0/d30/md_README.html#autotoc_md1087", null ]
      ] ],
      [ "Quick Start", "d0/d30/md_README.html#autotoc_md1088", [
        [ "Build", "d0/d30/md_README.html#autotoc_md1089", null ],
        [ "Applications", "d0/d30/md_README.html#autotoc_md1090", null ]
      ] ],
      [ "Usage", "d0/d30/md_README.html#autotoc_md1091", [
        [ "GUI Editor", "d0/d30/md_README.html#autotoc_md1092", null ],
        [ "Command Line Tool", "d0/d30/md_README.html#autotoc_md1093", null ],
        [ "C++ API", "d0/d30/md_README.html#autotoc_md1094", null ]
      ] ],
      [ "Documentation", "d0/d30/md_README.html#autotoc_md1095", null ],
      [ "Supported Platforms", "d0/d30/md_README.html#autotoc_md1096", null ],
      [ "ROM Compatibility", "d0/d30/md_README.html#autotoc_md1097", null ],
      [ "Contributing", "d0/d30/md_README.html#autotoc_md1098", null ],
      [ "License", "d0/d30/md_README.html#autotoc_md1099", null ],
      [ "🙏 Acknowledgments", "d0/d30/md_README.html#autotoc_md1100", null ],
      [ "📸 Screenshots", "d0/d30/md_README.html#autotoc_md1101", null ]
    ] ],
    [ "YAZE Build Scripts", "de/d82/md_scripts_2README.html", [
      [ "Windows Scripts", "de/d82/md_scripts_2README.html#autotoc_md1104", [
        [ "Setup Scripts", "de/d82/md_scripts_2README.html#autotoc_md1105", null ],
        [ "Build Scripts", "de/d82/md_scripts_2README.html#autotoc_md1106", null ],
        [ "Validation Scripts", "de/d82/md_scripts_2README.html#autotoc_md1107", null ],
        [ "Project Generation", "de/d82/md_scripts_2README.html#autotoc_md1108", null ]
      ] ],
      [ "Windows Compiler Recommendations", "de/d82/md_scripts_2README.html#autotoc_md1109", [
        [ "⚠️ Important: MSVC vs Clang on Windows", "de/d82/md_scripts_2README.html#autotoc_md1110", [
          [ "Why Clang is Recommended:", "de/d82/md_scripts_2README.html#autotoc_md1111", null ],
          [ "MSVC Issues:", "de/d82/md_scripts_2README.html#autotoc_md1112", null ]
        ] ],
        [ "Compiler Setup Options", "de/d82/md_scripts_2README.html#autotoc_md1113", [
          [ "Option 1: Clang (Recommended)", "de/d82/md_scripts_2README.html#autotoc_md1114", null ],
          [ "Option 2: MSVC with Workarounds", "de/d82/md_scripts_2README.html#autotoc_md1115", null ]
        ] ]
      ] ],
      [ "Quick Start (Windows)", "de/d82/md_scripts_2README.html#autotoc_md1116", [
        [ "Option 1: Automated Setup (Recommended)", "de/d82/md_scripts_2README.html#autotoc_md1117", null ],
        [ "Option 2: Manual Setup", "de/d82/md_scripts_2README.html#autotoc_md1118", null ],
        [ "Option 3: Using Batch Scripts", "de/d82/md_scripts_2README.html#autotoc_md1119", null ]
      ] ],
      [ "Script Options", "de/d82/md_scripts_2README.html#autotoc_md1120", [
        [ "setup-windows-dev.ps1", "de/d82/md_scripts_2README.html#autotoc_md1121", null ],
        [ "build-windows.ps1", "de/d82/md_scripts_2README.html#autotoc_md1122", null ],
        [ "build-windows.bat", "de/d82/md_scripts_2README.html#autotoc_md1123", null ]
      ] ],
      [ "Examples", "de/d82/md_scripts_2README.html#autotoc_md1124", null ],
      [ "Troubleshooting", "de/d82/md_scripts_2README.html#autotoc_md1125", [
        [ "Common Issues", "de/d82/md_scripts_2README.html#autotoc_md1126", null ],
        [ "Getting Help", "de/d82/md_scripts_2README.html#autotoc_md1127", null ]
      ] ],
      [ "Other Scripts", "de/d82/md_scripts_2README.html#autotoc_md1128", null ]
    ] ],
    [ "YAZE Test Suite", "d0/d46/md_test_2README.html", [
      [ "Directory Structure", "d0/d46/md_test_2README.html#autotoc_md1130", null ],
      [ "Test Categories", "d0/d46/md_test_2README.html#autotoc_md1131", [
        [ "Unit Tests (<tt>unit/</tt>)", "d0/d46/md_test_2README.html#autotoc_md1132", null ],
        [ "Integration Tests (<tt>integration/</tt>)", "d0/d46/md_test_2README.html#autotoc_md1133", null ],
        [ "End-to-End Tests (<tt>e2e/</tt>)", "d0/d46/md_test_2README.html#autotoc_md1134", null ]
      ] ],
      [ "Enhanced Test Runner", "d0/d46/md_test_2README.html#autotoc_md1135", [
        [ "Usage Examples", "d0/d46/md_test_2README.html#autotoc_md1136", null ],
        [ "Test Modes", "d0/d46/md_test_2README.html#autotoc_md1137", null ],
        [ "Options", "d0/d46/md_test_2README.html#autotoc_md1138", null ]
      ] ],
      [ "E2E ROM Testing", "d0/d46/md_test_2README.html#autotoc_md1139", [
        [ "Test Cases", "d0/d46/md_test_2README.html#autotoc_md1140", null ]
      ] ],
      [ "ZSCustomOverworld Upgrade Testing", "d0/d46/md_test_2README.html#autotoc_md1141", [
        [ "Supported Upgrades", "d0/d46/md_test_2README.html#autotoc_md1142", null ],
        [ "Test Cases", "d0/d46/md_test_2README.html#autotoc_md1143", null ],
        [ "Version-Specific Features", "d0/d46/md_test_2README.html#autotoc_md1144", [
          [ "Vanilla", "d0/d46/md_test_2README.html#autotoc_md1145", null ],
          [ "v2", "d0/d46/md_test_2README.html#autotoc_md1146", null ],
          [ "v3", "d0/d46/md_test_2README.html#autotoc_md1147", null ]
        ] ]
      ] ],
      [ "Environment Variables", "d0/d46/md_test_2README.html#autotoc_md1148", null ],
      [ "CI/CD Integration", "d0/d46/md_test_2README.html#autotoc_md1149", null ],
      [ "Deprecated Tests", "d0/d46/md_test_2README.html#autotoc_md1150", null ],
      [ "Best Practices", "d0/d46/md_test_2README.html#autotoc_md1151", null ],
      [ "AI Agent Testing", "d0/d46/md_test_2README.html#autotoc_md1152", null ]
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
"d0/d27/namespaceyaze_1_1gfx.html#aa5ae49b02472150a2ad972abf1f7d35a",
"d0/d48/structyaze_1_1emu_1_1Mosaic.html#a9371166acc9780c50b9c0ccd791384ff",
"d0/d9d/classyaze_1_1test_1_1UnitTestSuite.html#a6d16c7b89788f48f71ae723c3e2d1d08",
"d0/dff/room__object_8h.html#ac507bdbc1fe4503baebb7c8d6fcc0d55",
"d1/d39/canvas__utils_8h.html#a195c54ea0600ea6e367ee37e8c9aadca",
"d1/d4b/namespaceyaze_1_1gfx_1_1lc__lz2.html#a40999005b0886dc2f9e698bc7ad09cb7",
"d1/d8a/room__entrance_8h.html#aa6d65a0623a0d6aab487ebd9bc74f9bd",
"d1/de2/classyaze_1_1test_1_1ZSCustomOverworldUpgradeTest.html#a7be5b2e156935736e604afb8b3b9b921",
"d2/d07/classyaze_1_1emu_1_1Spc700.html#a644820116d504af3756431c5368796fb",
"d2/d46/classyaze_1_1editor_1_1PaletteEditor.html#a2ba58f619e57af4dddfcfdb0626b8512",
"d2/d71/structyaze_1_1gui_1_1canvas_1_1CanvasModals_1_1ModalState.html#a7bcfa02d28e4a4cfffddab959b255ad0",
"d2/dd4/structyaze_1_1zelda3_1_1music_1_1Song.html#ab0486f32fded23ea3644260b811ef286",
"d2/dfe/structyaze_1_1gui_1_1EnhancedTheme.html#a31fe0879c46ff26c38e794dfc86e0b00",
"d3/d15/classyaze_1_1emu_1_1Snes.html#a665b46b73e1ba8d1ea5684717dcae183",
"d3/d27/classyaze_1_1gui_1_1ThemeManager.html#a804427b8649eb8229addfc89d2a07bbd",
"d3/d44/classyaze_1_1editor_1_1OverworldEditor.html#a5873c4e7112f1b93c3572672f09026cd",
"d3/d49/md_docs_2B4-release-workflows.html#autotoc_md393",
"d3/d6c/classyaze_1_1editor_1_1MessageEditor.html#acd21220b42f482d84bb3cc36b2164ae7",
"d3/d8f/structyaze_1_1gui_1_1canvas_1_1CanvasConfig.html#ab62b5697520ea1230b04338793d52287",
"d3/dae/structyaze_1_1zelda3_1_1music_1_1ZeldaInstrument.html",
"d3/ded/classyaze_1_1emu_1_1Ppu.html#a1d119fd537dcde7f6e20c266363c88fa",
"d3/df6/structyaze_1_1gfx_1_1GraphicsSheetAnalysis.html#ace7c4e85d9e1633d063d6427b77e79ca",
"d4/d0a/namespaceyaze_1_1test.html#ae562cd7059ec7571b29261eff9ef82b2",
"d4/d6f/classyaze_1_1gfx_1_1PerformanceDashboard.html#a20b126e59008382222c6686d0b2f580b",
"d4/d9c/structyaze_1_1gui_1_1canvas_1_1TileInteractionResult.html#a453309396e38c43a44bce71341c112e3",
"d4/de6/classyaze_1_1gfx_1_1Arena.html#ada910dcae8ddc865bc1d63d04d59f44e",
"d5/d1f/namespaceyaze_1_1zelda3.html#a2bbb28f129a50e52015f75b3403b5455",
"d5/d1f/namespaceyaze_1_1zelda3.html#a8ac6522a2c1e4ab55f06350ba077013f",
"d5/d30/structyaze_1_1gui_1_1SpriteAsset.html",
"d5/d87/structyaze_1_1editor_1_1Shortcut.html#a6910a6a5b541d76e294ab597f40a1fc4",
"d5/dd0/classyaze_1_1test_1_1RomDependentTestSuite.html#a3325314ff8dfd69549541517f51bd1e8",
"d6/d20/namespaceyaze_1_1emu.html#a5447aec0bc02ba1ff714b6c4c8df382ba5baea5488eacdf0684fbad674206d40d",
"d6/d30/classyaze_1_1Rom.html#a3aacb1ac78a97db45382a8ad580ec4db",
"d6/d5e/structyaze_1_1gfx_1_1OptimizationResult.html#abb13254ec28dd5b5269e4cfb9366c3ea",
"d6/db1/classyaze_1_1zelda3_1_1Sprite.html#a0b4edfa96584451562217f75a7c2efa3",
"d6/dcb/classyaze_1_1gui_1_1canvas_1_1CanvasPerformanceIntegration.html#ad3c2c9c949e35a5ad4a110c7e7b57c2c",
"d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md151",
"d7/d61/classyaze_1_1zelda3_1_1DungeonEditorSystem.html#a47eb5f743becdeede970747654614648",
"d7/d61/classyaze_1_1zelda3_1_1DungeonEditorSystem.html#af911b0eef7100cddf4e1f2bf311ae318",
"d7/d9a/md_docs_2E1-asm-style-guide.html",
"d7/dcc/structyaze_1_1gfx_1_1PaletteGroupMap.html#a35199d150dfa83325939a618c3b9d4fa",
"d7/df6/classyaze_1_1zelda3_1_1Room.html#ab244664d49bb3675668265d369b1a3fc",
"d7/dfb/classyaze_1_1editor_1_1OverworldEditorManager.html#a9f0c37188d93d894440b1bd3dbee95fc",
"d8/d45/classyaze_1_1editor_1_1ExtensionManager.html",
"d8/d98/classyaze_1_1editor_1_1DungeonEditor.html",
"d8/dbd/structyaze_1_1editor_1_1DungeonCanvasViewer_1_1ObjectRenderCache.html#a01751d277a97a85fd2ac90bddd9df0d6",
"d8/ddb/snes__palette_8h.html#a8a2d6ba45adc4519dfa8c56056e197cb",
"d9/d41/md_docs_202-build-instructions.html#autotoc_md71",
"d9/dbd/canvas__utils__moved_8h.html#a5ae9a4ef2df4b3fccf04d3e83e585c96",
"d9/dc5/classyaze_1_1zelda3_1_1Overworld.html#a65384e147a9e89578169c45f3a8c4e4d",
"d9/dcc/classyaze_1_1zelda3_1_1OverworldMap.html#a89f6ebf42ff98632bbd7c947f021bcc9",
"d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md907",
"da/d2c/classyaze_1_1gui_1_1Canvas.html#a3849de72628476b91c0ee7d2711aacb9",
"da/d2c/classyaze_1_1gui_1_1Canvas.html#af09c65753661048e2cf78d8fb9572eaa",
"da/d5e/structyaze_1_1core_1_1YazeProject.html#a32d0e30cdd0df9a279316f5031172d50",
"da/da0/message__data_8cc.html#a023f96d1797f02dbaad61f338fb0ccd0",
"da/df6/structyaze_1_1util_1_1HexStringParams.html#aacc8ccc62374df2c51bded29cc20589d",
"db/d56/classyaze_1_1gui_1_1BppConversionDialog.html#a848360987e1e06dece73be5a265cd85f",
"db/d82/classyaze_1_1editor_1_1Tile16Editor.html#a961501ef13d5a77ee8327defdddf6b3f",
"db/da4/classyaze_1_1emu_1_1MockMemory.html#a714ac6e2ebcaa35e3782025db8a4f028",
"db/dd7/classyaze_1_1zelda3_1_1ObjectRenderer_1_1GraphicsCache.html#a2134a6c9f4c3f155ee7ff05ed3860bd9",
"dc/d01/classyaze_1_1emu_1_1Dsp.html#ade2538ea1915876ae7cf58d8722cd93c",
"dc/d31/classyaze_1_1editor_1_1GraphicsEditor.html#a930f5c8a78e02e9c68ba928d98d0b100",
"dc/d4f/classyaze_1_1editor_1_1DungeonObjectSelector.html#aa4d999f1deb328480e9a762a45955713",
"dc/daa/classyaze_1_1zelda3_1_1ObjectRenderer_1_1PerformanceMonitor.html#ab63228bf321b46668fa171fb03a48751",
"dc/df4/classyaze_1_1emu_1_1Cpu.html#a1e7fd1758cd1fc6c73ef84fda616b578",
"dc/df4/classyaze_1_1emu_1_1Cpu.html#aad7df7574682156aea5cd8da5c2896a9",
"dd/d12/classyaze_1_1editor_1_1EditorManager.html#a132e0a078804474631890db676aa1f74",
"dd/d19/structyaze_1_1core_1_1FeatureFlags_1_1Flags.html#a851b0fe4ed380000b937328fa4f47d1e",
"dd/d4b/classyaze_1_1editor_1_1DungeonUsageTracker.html#ae4cea41fb2973cb6460be14783d2a7dc",
"dd/d96/md_docs_204-api-reference.html#autotoc_md102",
"dd/ddf/classyaze_1_1gui_1_1canvas_1_1CanvasContextMenu.html#a728e925cc68d6c08332b2f55f4146715adb2f702656a6dfe0118b5a2b4b562619",
"de/d0d/structzelda3__version__pointers.html#ada0d386fc03a9b0d519e8dc09028fa0d",
"de/d5d/overworld__golden__data__extractor_8cc.html#a0ddf1224851353fc92bfbff6f499fa97",
"de/d8d/tile16__editor_8h.html#ad1dc8b6a79c23b4ab819d66313a332d2",
"de/dbf/icons_8h.html#a10cc3b4c035d67d84b7e352f9c1e3402",
"de/dbf/icons_8h.html#a2edf35fc16a771653f8211cf4ee2d442",
"de/dbf/icons_8h.html#a4b800b69236aafa8b9298b096a5f62f2",
"de/dbf/icons_8h.html#a6763afafc4e0d4e119cec9e02c1e8064",
"de/dbf/icons_8h.html#a87b39b1bb4cef9f57ccd421368c6576c",
"de/dbf/icons_8h.html#aa6986764d82962a7a1a41e5a2cd7c465",
"de/dbf/icons_8h.html#ac35eed05d553b7be287bbf4cee4db13c",
"de/dbf/icons_8h.html#adf153f6606a65c9db34760e927d77938",
"de/dbf/icons_8h.html#af9fbb61378cf9a7c969265d9b78a47fa",
"de/de7/classyaze_1_1zelda3_1_1DungeonObjectEditor.html#ad3eb7d7d7198c72456a86e97134fae6c",
"df/d20/classyaze_1_1gui_1_1EnhancedPaletteEditor.html#a04031fe78bc9a49e64d921b969e0e883",
"df/da2/classyaze_1_1gfx_1_1OamTile.html#a9bcd9f22e517cb53e23d84b192c9549f",
"dir_cf6dd3d856136905faaf5b7bba187d4d.html"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';