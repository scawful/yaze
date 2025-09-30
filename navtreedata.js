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
    [ "DungeonEditor Bottleneck Analysis", "d6/d6d/md_docs_2analysis_2dungeon__editor__bottleneck__analysis.html", [
      [ "🚨 <strong>Critical Performance Issue Identified</strong>", "d6/d6d/md_docs_2analysis_2dungeon__editor__bottleneck__analysis.html#autotoc_md149", [
        [ "<strong>Problem Summary</strong>", "d6/d6d/md_docs_2analysis_2dungeon__editor__bottleneck__analysis.html#autotoc_md150", null ],
        [ "<strong>Performance Breakdown</strong>", "d6/d6d/md_docs_2analysis_2dungeon__editor__bottleneck__analysis.html#autotoc_md151", null ]
      ] ],
      [ "🔍 <strong>Root Cause Analysis</strong>", "d6/d6d/md_docs_2analysis_2dungeon__editor__bottleneck__analysis.html#autotoc_md152", null ],
      [ "🎯 <strong>Detailed Timing Added</strong>", "d6/d6d/md_docs_2analysis_2dungeon__editor__bottleneck__analysis.html#autotoc_md153", null ],
      [ "📊 <strong>Expected Detailed Results</strong>", "d6/d6d/md_docs_2analysis_2dungeon__editor__bottleneck__analysis.html#autotoc_md154", null ],
      [ "🚀 <strong>Optimization Strategy</strong>", "d6/d6d/md_docs_2analysis_2dungeon__editor__bottleneck__analysis.html#autotoc_md155", [
        [ "<strong>Phase 1: Identify Specific Bottleneck</strong>", "d6/d6d/md_docs_2analysis_2dungeon__editor__bottleneck__analysis.html#autotoc_md156", null ],
        [ "<strong>Phase 2: Apply Targeted Optimizations</strong>", "d6/d6d/md_docs_2analysis_2dungeon__editor__bottleneck__analysis.html#autotoc_md157", [
          [ "<strong>If LoadAllRooms is the bottleneck:</strong>", "d6/d6d/md_docs_2analysis_2dungeon__editor__bottleneck__analysis.html#autotoc_md158", null ],
          [ "<strong>If CalculateUsageStats is the bottleneck:</strong>", "d6/d6d/md_docs_2analysis_2dungeon__editor__bottleneck__analysis.html#autotoc_md159", null ],
          [ "<strong>If LoadRoomEntrances is the bottleneck:</strong>", "d6/d6d/md_docs_2analysis_2dungeon__editor__bottleneck__analysis.html#autotoc_md160", null ]
        ] ],
        [ "<strong>Phase 3: Advanced Optimizations</strong>", "d6/d6d/md_docs_2analysis_2dungeon__editor__bottleneck__analysis.html#autotoc_md161", null ]
      ] ],
      [ "🎯 <strong>Expected Impact</strong>", "d6/d6d/md_docs_2analysis_2dungeon__editor__bottleneck__analysis.html#autotoc_md162", [
        [ "<strong>Current State</strong>", "d6/d6d/md_docs_2analysis_2dungeon__editor__bottleneck__analysis.html#autotoc_md163", null ],
        [ "<strong>After Optimization (Target)</strong>", "d6/d6d/md_docs_2analysis_2dungeon__editor__bottleneck__analysis.html#autotoc_md164", null ]
      ] ],
      [ "📈 <strong>Success Metrics</strong>", "d6/d6d/md_docs_2analysis_2dungeon__editor__bottleneck__analysis.html#autotoc_md165", null ],
      [ "🔄 <strong>Next Steps</strong>", "d6/d6d/md_docs_2analysis_2dungeon__editor__bottleneck__analysis.html#autotoc_md166", null ]
    ] ],
    [ "DungeonEditor Parallel Optimization Implementation", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html", [
      [ "🚀 <strong>Parallelization Strategy Implemented</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md168", [
        [ "<strong>Problem Identified</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md169", null ],
        [ "<strong>Solution: Multi-Threaded Room Loading</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md170", [
          [ "<strong>Key Optimizations</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md171", null ],
          [ "<strong>Parallel Processing Flow</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md172", null ],
          [ "<strong>Thread Safety Features</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md173", null ]
        ] ],
        [ "<strong>Expected Performance Impact</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md174", [
          [ "<strong>Theoretical Speedup</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md175", null ],
          [ "<strong>Expected Results</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md176", null ]
        ] ],
        [ "<strong>Technical Implementation Details</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md177", [
          [ "<strong>Thread Management</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md178", null ],
          [ "<strong>Result Processing</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md179", null ]
        ] ],
        [ "<strong>Monitoring and Validation</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md180", [
          [ "<strong>Performance Timing Added</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md181", null ],
          [ "<strong>Logging and Debugging</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md182", null ]
        ] ],
        [ "<strong>Benefits of This Approach</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md183", null ],
        [ "<strong>Next Steps</strong>", "d7/d30/md_docs_2analysis_2dungeon__parallel__optimization__summary.html#autotoc_md184", null ]
      ] ]
    ] ],
    [ "Editor Performance Monitoring Setup", "d2/dda/md_docs_2analysis_2editor__performance__monitoring__setup.html", [
      [ "Overview", "d2/dda/md_docs_2analysis_2editor__performance__monitoring__setup.html#autotoc_md186", null ],
      [ "✅ <strong>Completed Tasks</strong>", "d2/dda/md_docs_2analysis_2editor__performance__monitoring__setup.html#autotoc_md187", [
        [ "1. <strong>Performance Timer Standardization</strong>", "d2/dda/md_docs_2analysis_2editor__performance__monitoring__setup.html#autotoc_md188", null ],
        [ "2. <strong>Editor Timing Implementation</strong>", "d2/dda/md_docs_2analysis_2editor__performance__monitoring__setup.html#autotoc_md189", null ],
        [ "3. <strong>Implementation Details</strong>", "d2/dda/md_docs_2analysis_2editor__performance__monitoring__setup.html#autotoc_md190", null ]
      ] ],
      [ "🎯 <strong>Expected Results</strong>", "d2/dda/md_docs_2analysis_2editor__performance__monitoring__setup.html#autotoc_md191", null ],
      [ "🔍 <strong>Bottleneck Identification Strategy</strong>", "d2/dda/md_docs_2analysis_2editor__performance__monitoring__setup.html#autotoc_md192", [
        [ "<strong>Phase 1: Baseline Measurement</strong>", "d2/dda/md_docs_2analysis_2editor__performance__monitoring__setup.html#autotoc_md193", null ],
        [ "<strong>Phase 2: Targeted Optimization</strong>", "d2/dda/md_docs_2analysis_2editor__performance__monitoring__setup.html#autotoc_md194", null ],
        [ "<strong>Phase 3: Advanced Optimizations</strong>", "d2/dda/md_docs_2analysis_2editor__performance__monitoring__setup.html#autotoc_md195", null ]
      ] ],
      [ "🚀 <strong>Next Steps</strong>", "d2/dda/md_docs_2analysis_2editor__performance__monitoring__setup.html#autotoc_md196", null ],
      [ "📊 <strong>Expected Findings</strong>", "d2/dda/md_docs_2analysis_2editor__performance__monitoring__setup.html#autotoc_md197", null ],
      [ "🎉 <strong>Benefits</strong>", "d2/dda/md_docs_2analysis_2editor__performance__monitoring__setup.html#autotoc_md198", null ]
    ] ],
    [ "Lazy Loading Optimization Implementation Summary", "d5/df5/md_docs_2analysis_2lazy__loading__optimization__summary.html", [
      [ "Overview", "d5/df5/md_docs_2analysis_2lazy__loading__optimization__summary.html#autotoc_md200", null ],
      [ "Performance Problem Identified", "d5/df5/md_docs_2analysis_2lazy__loading__optimization__summary.html#autotoc_md201", [
        [ "Before Optimization", "d5/df5/md_docs_2analysis_2lazy__loading__optimization__summary.html#autotoc_md202", null ],
        [ "Root Cause", "d5/df5/md_docs_2analysis_2lazy__loading__optimization__summary.html#autotoc_md203", null ]
      ] ],
      [ "Solution: Selective Map Building + Lazy Loading", "d5/df5/md_docs_2analysis_2lazy__loading__optimization__summary.html#autotoc_md204", [
        [ "1. Selective Map Building", "d5/df5/md_docs_2analysis_2lazy__loading__optimization__summary.html#autotoc_md205", null ],
        [ "2. Lazy Loading System", "d5/df5/md_docs_2analysis_2lazy__loading__optimization__summary.html#autotoc_md206", null ]
      ] ],
      [ "Implementation Details", "d5/df5/md_docs_2analysis_2lazy__loading__optimization__summary.html#autotoc_md207", [
        [ "Core Changes", "d5/df5/md_docs_2analysis_2lazy__loading__optimization__summary.html#autotoc_md208", [
          [ "1. Overworld Class (<tt>overworld.h/cc</tt>)", "d5/df5/md_docs_2analysis_2lazy__loading__optimization__summary.html#autotoc_md209", null ],
          [ "2. OverworldMap Class (<tt>overworld_map.h</tt>)", "d5/df5/md_docs_2analysis_2lazy__loading__optimization__summary.html#autotoc_md210", null ],
          [ "3. OverworldEditor Class (<tt>overworld_editor.cc</tt>)", "d5/df5/md_docs_2analysis_2lazy__loading__optimization__summary.html#autotoc_md211", null ]
        ] ],
        [ "Performance Monitoring", "d5/df5/md_docs_2analysis_2lazy__loading__optimization__summary.html#autotoc_md212", null ]
      ] ],
      [ "Expected Performance Improvement", "d5/df5/md_docs_2analysis_2lazy__loading__optimization__summary.html#autotoc_md213", [
        [ "Theoretical Improvement", "d5/df5/md_docs_2analysis_2lazy__loading__optimization__summary.html#autotoc_md214", null ],
        [ "Real-World Benefits", "d5/df5/md_docs_2analysis_2lazy__loading__optimization__summary.html#autotoc_md215", null ]
      ] ],
      [ "Technical Advantages", "d5/df5/md_docs_2analysis_2lazy__loading__optimization__summary.html#autotoc_md216", [
        [ "1. Non-Breaking Changes", "d5/df5/md_docs_2analysis_2lazy__loading__optimization__summary.html#autotoc_md217", null ],
        [ "2. Intelligent Caching", "d5/df5/md_docs_2analysis_2lazy__loading__optimization__summary.html#autotoc_md218", null ],
        [ "3. Thread Safety", "d5/df5/md_docs_2analysis_2lazy__loading__optimization__summary.html#autotoc_md219", null ]
      ] ],
      [ "User Experience Impact", "d5/df5/md_docs_2analysis_2lazy__loading__optimization__summary.html#autotoc_md220", [
        [ "Immediate Benefits", "d5/df5/md_docs_2analysis_2lazy__loading__optimization__summary.html#autotoc_md221", null ],
        [ "Transparent Operation", "d5/df5/md_docs_2analysis_2lazy__loading__optimization__summary.html#autotoc_md222", null ]
      ] ],
      [ "Future Enhancements", "d5/df5/md_docs_2analysis_2lazy__loading__optimization__summary.html#autotoc_md223", [
        [ "Potential Optimizations", "d5/df5/md_docs_2analysis_2lazy__loading__optimization__summary.html#autotoc_md224", null ],
        [ "Monitoring and Metrics", "d5/df5/md_docs_2analysis_2lazy__loading__optimization__summary.html#autotoc_md225", null ]
      ] ],
      [ "Conclusion", "d5/df5/md_docs_2analysis_2lazy__loading__optimization__summary.html#autotoc_md226", null ]
    ] ],
    [ "Overworld::Load Performance Analysis and Optimization Plan", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html", [
      [ "Current Performance Profile", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md228", null ],
      [ "Detailed Analysis of Overworld::Load", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md229", [
        [ "Current Implementation Breakdown", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md230", null ]
      ] ],
      [ "Major Bottlenecks Identified", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md231", [
        [ "1. <strong>DecompressAllMapTiles() - PRIMARY BOTTLENECK (~1.5-2.0 seconds)</strong>", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md232", null ],
        [ "2. <strong>AssembleMap32Tiles() - SECONDARY BOTTLENECK (~200-400ms)</strong>", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md233", null ],
        [ "3. <strong>AssembleMap16Tiles() - MODERATE BOTTLENECK (~100-200ms)</strong>", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md234", null ]
      ] ],
      [ "Optimization Strategies", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md235", [
        [ "1. <strong>Parallelize Decompression Operations</strong>", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md236", null ],
        [ "2. <strong>Optimize ROM Access Patterns</strong>", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md237", null ],
        [ "3. <strong>Implement Lazy Map Loading</strong>", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md238", null ],
        [ "4. <strong>Optimize HyruleMagicDecompress</strong>", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md239", null ],
        [ "5. <strong>Memory Pool Optimization</strong>", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md240", null ]
      ] ],
      [ "Implementation Priority", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md241", [
        [ "Phase 1: High Impact, Low Risk (Immediate)", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md242", null ],
        [ "Phase 2: Medium Impact, Medium Risk (Next)", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md243", null ],
        [ "Phase 3: Lower Impact, Higher Risk (Future)", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md244", null ]
      ] ],
      [ "Expected Performance Improvements", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md245", [
        [ "Conservative Estimates", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md246", null ],
        [ "Aggressive Estimates", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md247", null ]
      ] ],
      [ "Conclusion", "d1/dc3/md_docs_2analysis_2overworld__load__optimization__analysis.html#autotoc_md248", null ]
    ] ],
    [ "Overworld Optimization Status Update", "df/df1/md_docs_2analysis_2overworld__optimization__status.html", [
      [ "Current Performance Analysis", "df/df1/md_docs_2analysis_2overworld__optimization__status.html#autotoc_md250", null ],
      [ "Key Findings", "df/df1/md_docs_2analysis_2overworld__optimization__status.html#autotoc_md251", [
        [ "✅ <strong>Successful Optimizations</strong>", "df/df1/md_docs_2analysis_2overworld__optimization__status.html#autotoc_md252", null ],
        [ "🎯 <strong>Real Bottleneck Identified</strong>", "df/df1/md_docs_2analysis_2overworld__optimization__status.html#autotoc_md253", null ],
        [ "📊 <strong>Performance Breakdown</strong>", "df/df1/md_docs_2analysis_2overworld__optimization__status.html#autotoc_md254", null ]
      ] ],
      [ "Root Cause Analysis", "df/df1/md_docs_2analysis_2overworld__optimization__status.html#autotoc_md255", null ],
      [ "Optimization Strategy", "df/df1/md_docs_2analysis_2overworld__optimization__status.html#autotoc_md256", [
        [ "Phase 1: Detailed Profiling (Immediate)", "df/df1/md_docs_2analysis_2overworld__optimization__status.html#autotoc_md257", null ],
        [ "Phase 2: Optimize BuildMap Operations (Next)", "df/df1/md_docs_2analysis_2overworld__optimization__status.html#autotoc_md258", null ],
        [ "Phase 3: Lazy Loading (Future)", "df/df1/md_docs_2analysis_2overworld__optimization__status.html#autotoc_md259", null ]
      ] ],
      [ "Current Status", "df/df1/md_docs_2analysis_2overworld__optimization__status.html#autotoc_md260", null ],
      [ "Expected Results", "df/df1/md_docs_2analysis_2overworld__optimization__status.html#autotoc_md261", null ]
    ] ],
    [ "YAZE Performance Optimization Summary", "db/de6/md_docs_2analysis_2performance__optimization__summary.html", [
      [ "🎉 <strong>Massive Performance Improvements Achieved!</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md263", [
        [ "📊 <strong>Overall Performance Results</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md264", null ]
      ] ],
      [ "🚀 <strong>Optimizations Implemented</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md265", [
        [ "1. <strong>Performance Monitoring System with Feature Flag</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md266", [
          [ "<strong>Features Added</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md267", null ],
          [ "<strong>Implementation</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md268", null ]
        ] ],
        [ "2. <strong>DungeonEditor Parallel Loading (79% Speedup)</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md269", [
          [ "<strong>Problem Solved</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md270", null ],
          [ "<strong>Solution: Multi-Threaded Room Loading</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md271", null ],
          [ "<strong>Key Features</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md272", null ]
        ] ],
        [ "3. <strong>Incremental Overworld Map Loading</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md273", [
          [ "<strong>Problem Solved</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md274", null ],
          [ "<strong>Solution: Priority-Based Incremental Loading</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md275", null ],
          [ "<strong>Key Features</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md276", null ]
        ] ],
        [ "4. <strong>On-Demand Map Reloading</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md277", [
          [ "<strong>Problem Solved</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md278", null ],
          [ "<strong>Solution: Intelligent Refresh System</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md279", null ],
          [ "<strong>Key Features</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md280", null ]
        ] ]
      ] ],
      [ "🎯 <strong>Technical Architecture</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md281", [
        [ "<strong>Performance Monitoring System</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md282", null ],
        [ "<strong>Parallel Loading Architecture</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md283", null ],
        [ "<strong>Incremental Loading Flow</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md284", null ]
      ] ],
      [ "📈 <strong>Performance Impact Analysis</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md285", [
        [ "<strong>DungeonEditor Optimization</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md286", null ],
        [ "<strong>OverworldEditor Optimization</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md287", null ],
        [ "<strong>Overall System Impact</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md288", null ]
      ] ],
      [ "🔧 <strong>Configuration Options</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md289", [
        [ "<strong>Performance Monitoring</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md290", null ],
        [ "<strong>Parallel Loading Tuning</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md291", null ],
        [ "<strong>Incremental Loading Tuning</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md292", null ]
      ] ],
      [ "🎯 <strong>Future Optimization Opportunities</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md293", [
        [ "<strong>Potential Further Improvements</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md294", null ],
        [ "<strong>Monitoring and Profiling</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md295", null ]
      ] ],
      [ "✅ <strong>Success Metrics Achieved</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md296", null ],
      [ "🚀 <strong>Result: Lightning-Fast YAZE</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md297", null ]
    ] ],
    [ "Renderer Class Performance Analysis and Optimization", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html", [
      [ "Overview", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md299", null ],
      [ "Original Performance Issues", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md300", [
        [ "1. Blocking Texture Creation", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md301", null ],
        [ "2. Inefficient Loading Pattern", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md302", null ]
      ] ],
      [ "Optimizations Implemented", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md303", [
        [ "1. Deferred Texture Creation", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md304", null ],
        [ "2. Lazy Loading System", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md305", null ],
        [ "3. Performance Monitoring", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md306", null ]
      ] ],
      [ "Thread Safety Considerations", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md307", [
        [ "Main Thread Requirement", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md308", null ],
        [ "Safe Optimization Approach", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md309", null ]
      ] ],
      [ "Performance Improvements", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md310", [
        [ "Loading Time Reduction", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md311", null ],
        [ "Memory Efficiency", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md312", null ]
      ] ],
      [ "Implementation Details", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md313", [
        [ "Modified Files", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md314", null ],
        [ "Key Methods Added", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md315", null ]
      ] ],
      [ "Usage Guidelines", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md316", [
        [ "For Developers", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md317", null ],
        [ "For ROM Loading", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md318", null ]
      ] ],
      [ "Future Optimization Opportunities", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md319", [
        [ "1. Background Threading (Pending)", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md320", null ],
        [ "2. Arena Management Optimization (Pending)", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md321", null ],
        [ "3. Advanced Lazy Loading (Pending)", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md322", null ]
      ] ],
      [ "Conclusion", "dd/d7c/md_docs_2analysis_2renderer__optimization__analysis.html#autotoc_md323", null ]
    ] ],
    [ "ZScream C# vs YAZE C++ Overworld Implementation Analysis", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html", [
      [ "Overview", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md325", null ],
      [ "Executive Summary", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md326", null ],
      [ "Detailed Comparison", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md327", [
        [ "1. Tile32 Loading and Expansion Detection", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md328", [
          [ "ZScream C# Logic (<tt>Overworld.cs:706-756</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md329", null ],
          [ "YAZE C++ Logic (<tt>overworld.cc:AssembleMap32Tiles</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md330", null ]
        ] ],
        [ "2. Tile16 Loading and Expansion Detection", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md331", [
          [ "ZScream C# Logic (<tt>Overworld.cs:652-705</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md332", null ],
          [ "YAZE C++ Logic (<tt>overworld.cc:AssembleMap16Tiles</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md333", null ]
        ] ],
        [ "3. Map Decompression", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md334", [
          [ "ZScream C# Logic (<tt>Overworld.cs:767-904</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md335", null ],
          [ "YAZE C++ Logic (<tt>overworld.cc:DecompressAllMapTiles</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md336", null ]
        ] ],
        [ "4. Entrance Coordinate Calculation", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md337", [
          [ "ZScream C# Logic (<tt>Overworld.cs:974-1001</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md338", null ],
          [ "YAZE C++ Logic (<tt>overworld.cc:LoadEntrances</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md339", null ]
        ] ],
        [ "5. Hole Coordinate Calculation with 0x400 Offset", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md340", [
          [ "ZScream C# Logic (<tt>Overworld.cs:1002-1025</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md341", null ],
          [ "YAZE C++ Logic (<tt>overworld.cc:LoadHoles</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md342", null ]
        ] ],
        [ "6. ASM Version Detection for Item Loading", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md343", [
          [ "ZScream C# Logic (<tt>Overworld.cs:1032-1094</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md344", null ],
          [ "YAZE C++ Logic (<tt>overworld.cc:LoadItems</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md345", null ]
        ] ],
        [ "7. Game State Handling for Sprite Loading", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md346", [
          [ "ZScream C# Logic (<tt>Overworld.cs:1276-1494</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md347", null ],
          [ "YAZE C++ Logic (<tt>overworld.cc:LoadSprites</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md348", null ]
        ] ],
        [ "8. Map Size Assignment Logic", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md349", [
          [ "ZScream C# Logic (<tt>Overworld.cs:296-390</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md350", null ],
          [ "YAZE C++ Logic (<tt>overworld.cc:AssignMapSizes</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md351", null ]
        ] ]
      ] ],
      [ "ZSCustomOverworld Integration", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md352", [
        [ "Version Detection", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md353", null ],
        [ "Feature Enablement", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md354", null ]
      ] ],
      [ "Integration Test Coverage", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md355", null ],
      [ "Conclusion", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md356", null ]
    ] ],
    [ "Atlas Rendering Implementation - YAZE Graphics Optimizations", "db/d26/md_docs_2atlas__rendering__implementation.html", [
      [ "Overview", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md358", null ],
      [ "Implementation Details", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md359", [
        [ "Core Components", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md360", [
          [ "1. AtlasRenderer Class (<tt>src/app/gfx/atlas_renderer.h/cc</tt>)", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md361", null ],
          [ "2. RenderCommand Structure", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md362", null ],
          [ "3. Atlas Statistics Tracking", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md363", null ]
        ] ],
        [ "Integration Points", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md364", [
          [ "1. Tilemap Integration (<tt>src/app/gfx/tilemap.h/cc</tt>)", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md365", null ],
          [ "2. Performance Dashboard Integration", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md366", null ],
          [ "3. Benchmarking Suite (<tt>test/gfx_optimization_benchmarks.cc</tt>)", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md367", null ]
        ] ],
        [ "Technical Implementation", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md368", [
          [ "Atlas Packing Algorithm", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md369", null ],
          [ "Batch Rendering Process", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md370", null ]
        ] ],
        [ "Performance Improvements", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md371", [
          [ "Measured Performance Gains", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md372", null ],
          [ "Benchmark Results", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md373", null ]
        ] ],
        [ "ROM Hacking Workflow Benefits", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md374", [
          [ "Graphics Sheet Management", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md375", null ],
          [ "Editor Performance", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md376", null ]
        ] ],
        [ "API Usage Examples", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md377", [
          [ "Basic Atlas Usage", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md378", null ],
          [ "Tilemap Integration", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md379", null ]
        ] ],
        [ "Memory Management", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md380", [
          [ "Automatic Cleanup", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md381", null ],
          [ "Resource Management", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md382", null ]
        ] ],
        [ "Future Enhancements", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md383", [
          [ "Planned Improvements", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md384", null ],
          [ "Integration Opportunities", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md385", null ]
        ] ]
      ] ],
      [ "Conclusion", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md386", null ],
      [ "Files Modified", "db/d26/md_docs_2atlas__rendering__implementation.html#autotoc_md387", null ]
    ] ],
    [ "Contributing", "dd/d5b/md_docs_2B1-contributing.html", [
      [ "Development Setup", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md389", [
        [ "Prerequisites", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md390", null ],
        [ "Quick Start", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md391", null ]
      ] ],
      [ "Code Style", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md392", [
        [ "C++ Standards", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md393", null ],
        [ "File Organization", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md394", null ],
        [ "Error Handling", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md395", null ]
      ] ],
      [ "Testing Requirements", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md396", [
        [ "Test Categories", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md397", null ],
        [ "Writing Tests", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md398", null ],
        [ "Test Execution", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md399", null ]
      ] ],
      [ "Pull Request Process", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md400", [
        [ "Before Submitting", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md401", null ],
        [ "Pull Request Template", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md402", null ]
      ] ],
      [ "Development Workflow", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md403", [
        [ "Branch Strategy", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md404", null ],
        [ "Commit Messages", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md405", null ],
        [ "Types", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md406", null ]
      ] ],
      [ "Architecture Guidelines", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md407", [
        [ "Component Design", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md408", null ],
        [ "Memory Management", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md409", null ],
        [ "Performance", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md410", null ]
      ] ],
      [ "Documentation", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md411", [
        [ "Code Documentation", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md412", null ],
        [ "API Documentation", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md413", null ]
      ] ],
      [ "Community Guidelines", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md414", [
        [ "Communication", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md415", null ],
        [ "Getting Help", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md416", null ]
      ] ],
      [ "Release Process", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md417", [
        [ "Version Numbering", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md418", null ],
        [ "Release Checklist", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md419", null ]
      ] ]
    ] ],
    [ "Platform Compatibility Improvements", "d1/d30/md_docs_2B2-platform-compatibility.html", [
      [ "Native File Dialog Support", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md421", null ],
      [ "Cross-Platform Build Reliability", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md422", null ],
      [ "Build Configuration Options", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md423", [
        [ "Full Build (Development)", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md424", null ],
        [ "Minimal Build", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md425", null ]
      ] ],
      [ "Implementation Details", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md426", null ],
      [ "Platform-Specific Adaptations", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md427", [
        [ "Windows", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md428", null ],
        [ "macOS", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md429", null ],
        [ "Linux", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md430", null ]
      ] ]
    ] ],
    [ "Build Presets Guide", "d7/d51/md_docs_2B3-build-presets.html", [
      [ "🍎 macOS ARM64 Presets (Recommended for Apple Silicon)", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md432", [
        [ "For Development Work:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md433", null ],
        [ "For Distribution:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md434", null ]
      ] ],
      [ "🔧 Why This Fixes Architecture Errors", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md435", null ],
      [ "📋 Available Presets", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md436", null ],
      [ "🚀 Quick Start", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md437", null ],
      [ "🛠️ IDE Integration", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md438", [
        [ "VS Code with CMake Tools:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md439", null ],
        [ "CLion:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md440", null ],
        [ "Xcode:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md441", null ]
      ] ],
      [ "🔍 Troubleshooting", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md442", null ],
      [ "📝 Notes", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md443", null ]
    ] ],
    [ "Release Workflows Documentation", "d3/d49/md_docs_2B4-release-workflows.html", [
      [ "Overview", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md445", null ],
      [ "1. Release-Simplified (<tt>release-simplified.yml</tt>)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md447", [
        [ "Purpose", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md448", null ],
        [ "Key Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md449", null ],
        [ "Use Cases", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md450", null ],
        [ "Configuration", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md451", null ],
        [ "Platforms Supported", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md452", null ]
      ] ],
      [ "2. Release (<tt>release.yml</tt>)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md454", [
        [ "Purpose", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md455", null ],
        [ "Key Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md456", null ],
        [ "Use Cases", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md457", null ],
        [ "Configuration", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md458", null ],
        [ "Advantages over Simplified", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md459", null ]
      ] ],
      [ "3. Release-Complex (<tt>release-complex.yml</tt>)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md461", [
        [ "Purpose", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md462", null ],
        [ "Key Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md463", null ],
        [ "Use Cases", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md464", null ],
        [ "Fallback Mechanisms", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md465", [
          [ "vcpkg Fallback", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md466", null ],
          [ "Chocolatey Integration", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md467", null ],
          [ "Build Configuration Fallback", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md468", null ]
        ] ],
        [ "Advanced Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md469", null ]
      ] ],
      [ "Workflow Comparison Matrix", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md471", null ],
      [ "When to Use Each Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md473", [
        [ "Use Simplified When:", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md474", null ],
        [ "Use Release When:", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md475", null ],
        [ "Use Complex When:", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md476", null ]
      ] ],
      [ "Workflow Selection Guide", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md478", [
        [ "For Development Team", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md479", null ],
        [ "For Release Manager", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md480", null ],
        [ "For CI/CD Pipeline", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md481", null ]
      ] ],
      [ "Configuration Examples", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md483", [
        [ "Triggering a Release", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md484", [
          [ "Manual Release (All Workflows)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md485", null ],
          [ "Automatic Release (Tag Push)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md486", null ]
        ] ],
        [ "Customizing Release Notes", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md487", null ]
      ] ],
      [ "Troubleshooting", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md489", [
        [ "Common Issues", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md490", [
          [ "vcpkg Failures (Windows)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md491", null ],
          [ "Dependency Conflicts", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md492", null ],
          [ "Build Failures", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md493", null ]
        ] ],
        [ "Debug Information", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md494", [
          [ "Simplified Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md495", null ],
          [ "Release Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md496", null ],
          [ "Complex Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md497", null ]
        ] ]
      ] ],
      [ "Best Practices", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md499", [
        [ "Workflow Selection", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md500", null ],
        [ "Release Process", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md501", null ],
        [ "Maintenance", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md502", null ]
      ] ],
      [ "Future Improvements", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md504", [
        [ "Planned Enhancements", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md505", null ],
        [ "Integration Opportunities", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md506", null ]
      ] ]
    ] ],
    [ "Stability, Testability & Release Workflow Improvements", "d9/d6b/md_docs_2B5-stability-improvements.html", [
      [ "Recent Improvements (v0.3.2)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md509", [
        [ "Windows Platform Stability", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md510", [
          [ "Stack Size Optimization", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md511", null ],
          [ "Development Utility Isolation", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md512", null ]
        ] ],
        [ "Graphics System Stability", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md513", [
          [ "Segmentation Fault Resolution", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md514", null ],
          [ "Comprehensive Bounds Checking", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md515", null ]
        ] ],
        [ "Build System Improvements", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md516", [
          [ "Modern Windows Workflow", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md517", null ],
          [ "Enhanced CI/CD Reliability", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md518", null ]
        ] ]
      ] ],
      [ "Recommended Optimizations", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md519", [
        [ "High Priority", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md520", [
          [ "1. Lazy Graphics Loading", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md521", null ],
          [ "2. Heap-Based Large Allocations", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md522", null ],
          [ "3. Streaming ROM Assets", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md523", null ]
        ] ],
        [ "Medium Priority", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md524", [
          [ "4. Enhanced Test Isolation", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md525", null ],
          [ "5. Dependency Caching Optimization", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md526", null ],
          [ "6. Memory Pool for Graphics", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md527", null ]
        ] ],
        [ "Low Priority", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md528", [
          [ "7. Build Time Optimization", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md529", null ],
          [ "8. Release Workflow Simplification", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md530", null ]
        ] ]
      ] ],
      [ "Testing Improvements", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md531", [
        [ "Current State", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md532", null ],
        [ "Recommendations", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md533", [
          [ "1. Visual Regression Testing", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md534", null ],
          [ "2. Performance Benchmarks", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md535", null ],
          [ "3. Fuzz Testing", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md536", null ]
        ] ]
      ] ],
      [ "Metrics & Monitoring", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md537", [
        [ "Current Measurements", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md538", null ],
        [ "Target Improvements", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md539", null ]
      ] ],
      [ "Action Items", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md540", [
        [ "Immediate (v0.3.2)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md541", null ],
        [ "Short Term (v0.3.3)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md542", null ],
        [ "Medium Term (v0.4.0)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md543", null ],
        [ "Long Term (v0.5.0+)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md544", null ]
      ] ],
      [ "Conclusion", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md545", null ]
    ] ],
    [ "Changelog", "d7/d6f/md_docs_2C1-changelog.html", [
      [ "0.3.2 (December 2025) - In Development", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md547", [
        [ "Tile16 Editor Improvements (In Progress)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md548", null ],
        [ "Graphics System Optimizations", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md549", null ],
        [ "Windows Platform Stability", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md550", null ],
        [ "Memory Safety & Stability", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md551", null ],
        [ "Testing & CI/CD Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md552", null ],
        [ "Future Optimizations (Planned)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md553", null ],
        [ "Technical Notes", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md554", null ]
      ] ],
      [ "0.3.1 (September 2025)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md555", [
        [ "Major Features", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md556", null ],
        [ "Tile16 Editor Enhancements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md557", null ],
        [ "ZSCustomOverworld v3 Implementation", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md558", null ],
        [ "Technical Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md559", null ],
        [ "User Interface", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md560", null ],
        [ "Bug Fixes", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md561", null ],
        [ "ZScream Compatibility Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md562", null ]
      ] ],
      [ "0.3.0 (September 2025)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md563", [
        [ "Major Features", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md564", null ],
        [ "User Interface & Theming", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md565", null ],
        [ "Enhancements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md566", null ],
        [ "Technical Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md567", null ],
        [ "Bug Fixes", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md568", null ]
      ] ],
      [ "0.2.2 (December 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md569", null ],
      [ "0.2.1 (August 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md570", null ],
      [ "0.2.0 (July 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md571", null ],
      [ "0.1.0 (May 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md572", null ],
      [ "0.0.9 (April 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md573", null ],
      [ "0.0.8 (February 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md574", null ],
      [ "0.0.7 (January 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md575", null ],
      [ "0.0.6 (November 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md576", null ],
      [ "0.0.5 (November 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md577", null ],
      [ "0.0.4 (November 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md578", null ],
      [ "0.0.3 (October 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md579", null ]
    ] ],
    [ "Roadmap", "d6/db4/md_docs_2D1-roadmap.html", [
      [ "0.4.X (Next Major Release)", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md581", [
        [ "Core Features", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md582", null ],
        [ "Technical Improvements", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md583", null ]
      ] ],
      [ "0.5.X", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md584", [
        [ "Advanced Features", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md585", null ]
      ] ],
      [ "0.6.X", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md586", [
        [ "Platform & Integration", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md587", null ]
      ] ],
      [ "0.7.X", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md588", [
        [ "Performance & Polish", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md589", null ]
      ] ],
      [ "0.8.X", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md590", [
        [ "Beta Preparation", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md591", null ]
      ] ],
      [ "1.0.0", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md592", [
        [ "Stable Release", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md593", null ]
      ] ],
      [ "Current Focus Areas", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md594", [
        [ "Immediate Priorities (v0.4.X)", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md595", null ],
        [ "Long-term Vision", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md596", null ]
      ] ]
    ] ],
    [ "Asm Style Guide", "d7/d9a/md_docs_2E1-asm-style-guide.html", [
      [ "Table of Contents", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md598", null ],
      [ "File Structure", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md599", null ],
      [ "Labels and Symbols", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md600", null ],
      [ "Comments", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md601", null ],
      [ "Instructions", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md602", null ],
      [ "Macros", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md603", null ],
      [ "Loops and Branching", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md604", null ],
      [ "Data Structures", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md605", null ],
      [ "Code Organization", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md606", null ],
      [ "Custom Code", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md607", null ]
    ] ],
    [ "Dungeon Editor Guide", "dd/d33/md_docs_2E2-dungeon-editor-guide.html", [
      [ "Overview", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md609", null ],
      [ "Architecture", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md610", [
        [ "Core Components", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md611", [
          [ "1. DungeonEditorSystem", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md612", null ],
          [ "2. DungeonObjectEditor", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md613", null ],
          [ "3. ObjectRenderer", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md614", null ],
          [ "4. DungeonEditor (UI Layer)", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md615", null ]
        ] ]
      ] ],
      [ "Coordinate System", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md616", [
        [ "Room Coordinates vs Canvas Coordinates", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md617", [
          [ "Conversion Functions", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md618", null ]
        ] ],
        [ "Coordinate System Features", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md619", null ]
      ] ],
      [ "Object Rendering System", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md620", [
        [ "Object Types", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md621", null ],
        [ "Rendering Pipeline", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md622", null ],
        [ "Performance Optimizations", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md623", null ]
      ] ],
      [ "User Interface", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md624", [
        [ "Integrated Editing Panels", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md625", [
          [ "Main Canvas", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md626", null ],
          [ "Compact Editing Panels", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md627", null ]
        ] ],
        [ "Object Preview System", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md628", null ]
      ] ],
      [ "Integration with ZScream", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md629", [
        [ "Room Loading", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md630", null ],
        [ "Object Parsing", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md631", null ],
        [ "Coordinate System", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md632", null ]
      ] ],
      [ "Testing and Validation", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md633", [
        [ "Integration Tests", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md634", null ],
        [ "Test Data", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md635", null ],
        [ "Performance Benchmarks", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md636", null ]
      ] ],
      [ "Usage Examples", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md637", [
        [ "Basic Object Editing", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md638", null ],
        [ "Coordinate Conversion", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md639", null ],
        [ "Object Preview", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md640", null ]
      ] ],
      [ "Configuration Options", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md641", [
        [ "Editor Configuration", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md642", null ],
        [ "Performance Configuration", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md643", null ]
      ] ],
      [ "Troubleshooting", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md644", [
        [ "Common Issues", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md645", null ],
        [ "Debug Information", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md646", null ]
      ] ],
      [ "Future Enhancements", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md647", [
        [ "Planned Features", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md648", null ]
      ] ],
      [ "Conclusion", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md649", null ]
    ] ],
    [ "Dungeon Editor Design Plan", "dc/d82/md_docs_2E3-dungeon-editor-design.html", [
      [ "Overview", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md651", null ],
      [ "Architecture Overview", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md652", [
        [ "Core Components", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md653", null ],
        [ "File Structure", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md654", null ]
      ] ],
      [ "Component Responsibilities", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md655", [
        [ "DungeonEditor (Main Orchestrator)", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md656", null ],
        [ "DungeonRoomSelector", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md657", null ],
        [ "DungeonCanvasViewer", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md658", null ],
        [ "DungeonObjectSelector", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md659", null ]
      ] ],
      [ "Data Flow", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md660", [
        [ "Initialization Flow", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md661", null ],
        [ "Runtime Data Flow", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md662", null ],
        [ "Key Data Structures", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md663", null ]
      ] ],
      [ "Integration Patterns", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md664", [
        [ "Component Communication", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md665", null ],
        [ "ROM Data Management", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md666", null ],
        [ "State Synchronization", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md667", null ]
      ] ],
      [ "UI Layout Architecture", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md668", [
        [ "3-Column Layout", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md669", null ],
        [ "Component Internal Layout", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md670", null ]
      ] ],
      [ "Coordinate System", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md671", [
        [ "Room Coordinates vs Canvas Coordinates", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md672", null ],
        [ "Bounds Checking", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md673", null ]
      ] ],
      [ "Error Handling & Validation", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md674", [
        [ "ROM Validation", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md675", null ],
        [ "Bounds Validation", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md676", null ]
      ] ],
      [ "Performance Considerations", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md677", [
        [ "Caching Strategy", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md678", null ],
        [ "Rendering Optimization", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md679", null ]
      ] ],
      [ "Testing Strategy", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md680", [
        [ "Integration Tests", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md681", null ],
        [ "Test Categories", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md682", null ]
      ] ],
      [ "Future Development Guidelines", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md683", [
        [ "Adding New Features", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md684", null ],
        [ "Component Extension Patterns", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md685", null ],
        [ "Data Flow Extension", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md686", null ],
        [ "UI Layout Extension", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md687", null ]
      ] ],
      [ "Common Pitfalls & Solutions", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md688", [
        [ "Memory Management", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md689", null ],
        [ "Coordinate System", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md690", null ],
        [ "State Synchronization", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md691", null ],
        [ "Performance Issues", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md692", null ]
      ] ],
      [ "Debugging Tools", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md693", [
        [ "Debug Popup", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md694", null ],
        [ "Logging", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md695", null ]
      ] ],
      [ "Build Integration", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md696", [
        [ "CMake Configuration", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md697", null ],
        [ "Dependencies", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md698", null ]
      ] ],
      [ "Conclusion", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md699", null ]
    ] ],
    [ "DungeonEditor Refactoring Plan", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html", [
      [ "Overview", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md701", null ],
      [ "Component Structure", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md702", [
        [ "✅ Created Components", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md703", [
          [ "1. DungeonToolset (<tt>dungeon_toolset.h/cc</tt>)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md704", null ],
          [ "2. DungeonObjectInteraction (<tt>dungeon_object_interaction.h/cc</tt>)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md705", null ],
          [ "3. DungeonRenderer (<tt>dungeon_renderer.h/cc</tt>)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md706", null ],
          [ "4. DungeonRoomLoader (<tt>dungeon_room_loader.h/cc</tt>)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md707", null ],
          [ "5. DungeonUsageTracker (<tt>dungeon_usage_tracker.h/cc</tt>)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md708", null ]
        ] ]
      ] ],
      [ "Refactored DungeonEditor Structure", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md709", [
        [ "Before Refactoring: 1444 lines", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md710", null ],
        [ "After Refactoring: ~400 lines", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md711", null ]
      ] ],
      [ "Method Migration Map", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md712", [
        [ "Core Editor Methods (Keep in main file)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md713", null ],
        [ "UI Methods (Keep for coordination)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md714", null ],
        [ "Methods Moved to Components", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md715", [
          [ "→ DungeonToolset", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md716", null ],
          [ "→ DungeonObjectInteraction", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md717", null ],
          [ "→ DungeonRenderer", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md718", null ],
          [ "→ DungeonRoomLoader", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md719", null ],
          [ "→ DungeonUsageTracker", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md720", null ]
        ] ]
      ] ],
      [ "Component Communication", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md721", [
        [ "Callback System", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md722", null ],
        [ "Data Sharing", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md723", null ]
      ] ],
      [ "Benefits of Refactoring", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md724", [
        [ "1. <strong>Reduced Complexity</strong>", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md725", null ],
        [ "2. <strong>Improved Testability</strong>", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md726", null ],
        [ "3. <strong>Better Maintainability</strong>", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md727", null ],
        [ "4. <strong>Enhanced Extensibility</strong>", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md728", null ],
        [ "5. <strong>Cleaner Dependencies</strong>", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md729", null ]
      ] ],
      [ "Implementation Status", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md730", [
        [ "✅ Completed", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md731", null ],
        [ "🔄 In Progress", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md732", null ],
        [ "⏳ Pending", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md733", null ]
      ] ],
      [ "Migration Strategy", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md734", [
        [ "Phase 1: Create Components ✅", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md735", null ],
        [ "Phase 2: Integrate Components 🔄", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md736", null ],
        [ "Phase 3: Move Methods", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md737", null ],
        [ "Phase 4: Cleanup", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md738", null ]
      ] ]
    ] ],
    [ "Dungeon Object System", "da/d11/md_docs_2E5-dungeon-object-system.html", [
      [ "Overview", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md740", null ],
      [ "Architecture", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md741", [
        [ "Core Components", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md742", [
          [ "1. DungeonEditor (<tt>src/app/editor/dungeon/dungeon_editor.h</tt>)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md743", null ],
          [ "2. DungeonObjectSelector (<tt>src/app/editor/dungeon/dungeon_object_selector.h</tt>)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md744", null ],
          [ "3. DungeonCanvasViewer (<tt>src/app/editor/dungeon/dungeon_canvas_viewer.h</tt>)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md745", null ],
          [ "4. Room Management System (<tt>src/app/zelda3/dungeon/room.h</tt>)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md746", null ]
        ] ]
      ] ],
      [ "Object Types and Hierarchies", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md747", [
        [ "Room Objects", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md748", [
          [ "Type 1 Objects (0x00-0xFF)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md749", null ],
          [ "Type 2 Objects (0x100-0x1FF)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md750", null ],
          [ "Type 3 Objects (0x200+)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md751", null ]
        ] ],
        [ "Object Properties", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md752", null ]
      ] ],
      [ "How Object Placement Works", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md753", [
        [ "Selection Process", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md754", null ],
        [ "Placement Process", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md755", null ],
        [ "Code Flow", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md756", null ]
      ] ],
      [ "Rendering Pipeline", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md757", [
        [ "Object Rendering", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md758", null ],
        [ "Performance Optimizations", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md759", null ]
      ] ],
      [ "User Interface Components", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md760", [
        [ "Three-Column Layout", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md761", [
          [ "Column 1: Room Control Panel (280px fixed)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md762", null ],
          [ "Column 2: Windowed Canvas (800px fixed)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md763", null ],
          [ "Column 3: Object Selector/Editor (stretch)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md764", null ]
        ] ],
        [ "Debug and Control Features", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md765", [
          [ "Room Properties Table", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md766", null ],
          [ "Object Statistics", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md767", null ]
        ] ]
      ] ],
      [ "Integration with ROM Data", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md768", [
        [ "Data Sources", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md769", [
          [ "Room Headers (<tt>0x1F8000</tt>)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md770", null ],
          [ "Object Data", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md771", null ],
          [ "Graphics Data", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md772", null ]
        ] ],
        [ "Assembly Integration", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md773", null ]
      ] ],
      [ "Comparison with ZScream", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md774", [
        [ "Architectural Differences", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md775", [
          [ "Component-Based Architecture", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md776", null ],
          [ "Real-time Rendering", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md777", null ],
          [ "UI Organization", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md778", null ],
          [ "Caching Strategy", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md779", null ]
        ] ],
        [ "Shared Concepts", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md780", null ]
      ] ],
      [ "Best Practices", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md781", [
        [ "Performance", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md782", null ],
        [ "Code Organization", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md783", null ],
        [ "User Experience", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md784", null ]
      ] ],
      [ "Future Enhancements", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md785", [
        [ "Planned Features", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md786", null ],
        [ "Technical Improvements", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md787", null ]
      ] ]
    ] ],
    [ "Overworld Loading Guide", "dc/d12/md_docs_2F1-overworld-loading.html", [
      [ "Table of Contents", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md789", null ],
      [ "Overview", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md790", null ],
      [ "ROM Types and Versions", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md791", [
        [ "Version Detection", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md792", null ],
        [ "Feature Support by Version", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md793", null ]
      ] ],
      [ "Overworld Map Structure", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md794", [
        [ "Core Properties", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md795", null ]
      ] ],
      [ "Overlays and Special Area Maps", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md796", [
        [ "Understanding Overlays", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md797", null ],
        [ "Special Area Maps (0x80-0x9F)", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md798", null ],
        [ "Overlay ID Mappings", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md799", null ],
        [ "Drawing Order", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md800", null ],
        [ "Vanilla Overlay Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md801", null ],
        [ "Special Area Graphics Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md802", null ]
      ] ],
      [ "Loading Process", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md803", [
        [ "1. Version Detection", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md804", null ],
        [ "2. Map Initialization", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md805", null ],
        [ "3. Property Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md806", [
          [ "Vanilla ROMs (asm_version == 0xFF)", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md807", null ],
          [ "ZSCustomOverworld v2/v3", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md808", null ]
        ] ],
        [ "4. Custom Data Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md809", null ]
      ] ],
      [ "ZScream Implementation", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md810", [
        [ "OverworldMap Constructor", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md811", null ],
        [ "Key Methods", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md812", null ]
      ] ],
      [ "Yaze Implementation", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md813", [
        [ "OverworldMap Constructor", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md814", null ],
        [ "Key Methods", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md815", null ],
        [ "Current Status", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md816", null ]
      ] ],
      [ "Key Differences", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md817", [
        [ "1. Language and Architecture", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md818", null ],
        [ "2. Data Structures", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md819", null ],
        [ "3. Error Handling", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md820", null ],
        [ "4. Graphics Processing", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md821", null ]
      ] ],
      [ "Common Issues and Solutions", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md822", [
        [ "1. Version Detection Issues", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md823", null ],
        [ "2. Palette Loading Errors", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md824", null ],
        [ "3. Graphics Not Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md825", null ],
        [ "4. Overlay Issues", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md826", null ],
        [ "5. Large Map Problems", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md827", null ],
        [ "6. Special Area Graphics Issues", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md828", null ]
      ] ],
      [ "Best Practices", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md829", [
        [ "1. Version-Specific Code", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md830", null ],
        [ "2. Error Handling", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md831", null ],
        [ "3. Memory Management", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md832", null ],
        [ "4. Thread Safety", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md833", null ]
      ] ],
      [ "Conclusion", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md834", null ]
    ] ],
    [ "YAZE Graphics System Improvements Summary", "d9/df4/md_docs_2gfx__improvements__summary.html", [
      [ "Overview", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md836", null ],
      [ "Files Modified", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md837", [
        [ "Core Graphics Classes", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md838", [
          [ "1. <tt>/src/app/gfx/bitmap.h</tt>", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md839", null ],
          [ "2. <tt>/src/app/gfx/bitmap.cc</tt>", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md840", null ],
          [ "3. <tt>/src/app/gfx/arena.h</tt>", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md841", null ],
          [ "4. <tt>/src/app/gfx/arena.cc</tt>", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md842", null ],
          [ "5. <tt>/src/app/gfx/tilemap.h</tt>", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md843", null ]
        ] ],
        [ "Editor Classes", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md844", [
          [ "6. <tt>/src/app/editor/graphics/graphics_editor.cc</tt>", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md845", null ],
          [ "7. <tt>/src/app/editor/graphics/palette_editor.cc</tt>", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md846", null ],
          [ "8. <tt>/src/app/editor/graphics/screen_editor.cc</tt>", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md847", null ]
        ] ]
      ] ],
      [ "New Documentation Files", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md848", [
        [ "9. <tt>/docs/gfx_optimization_recommendations.md</tt>", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md849", null ]
      ] ],
      [ "Performance Optimization Recommendations", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md850", [
        [ "High Impact, Low Risk (Phase 1)", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md851", null ],
        [ "Medium Impact, Medium Risk (Phase 2)", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md852", null ],
        [ "High Impact, High Risk (Phase 3)", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md853", null ]
      ] ],
      [ "ROM Hacking Workflow Improvements", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md854", [
        [ "Graphics Editor Enhancements", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md855", null ],
        [ "Palette Editor Enhancements", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md856", null ],
        [ "Screen Editor Enhancements", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md857", null ]
      ] ],
      [ "Code Quality Improvements", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md858", [
        [ "Documentation Standards", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md859", null ],
        [ "Code Organization", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md860", null ]
      ] ],
      [ "Future Development Recommendations", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md861", [
        [ "Immediate Improvements", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md862", null ],
        [ "Medium-term Enhancements", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md863", null ],
        [ "Long-term Goals", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md864", null ]
      ] ],
      [ "Conclusion", "d9/df4/md_docs_2gfx__improvements__summary.html#autotoc_md865", null ]
    ] ],
    [ "YAZE Graphics System Optimization Recommendations", "de/def/md_docs_2gfx__optimization__recommendations.html", [
      [ "Overview", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md867", null ],
      [ "Current Architecture Analysis", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md868", [
        [ "Strengths", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md869", null ],
        [ "Performance Bottlenecks Identified", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md870", [
          [ "1. Bitmap Class Issues", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md871", null ],
          [ "2. Arena Resource Management", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md872", null ],
          [ "3. Tilemap Performance", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md873", null ]
        ] ]
      ] ],
      [ "Optimization Recommendations", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md874", [
        [ "1. Bitmap Class Optimizations", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md875", [
          [ "A. Palette Lookup Optimization", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md876", null ],
          [ "B. Dirty Region Tracking", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md877", null ]
        ] ],
        [ "2. Arena Resource Management Improvements", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md878", [
          [ "A. Resource Pooling", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md879", null ],
          [ "B. Batch Operations", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md880", null ]
        ] ],
        [ "3. Tilemap Performance Enhancements", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md881", [
          [ "A. Smart Tile Caching", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md882", null ],
          [ "B. Atlas-based Rendering", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md883", null ]
        ] ],
        [ "4. Editor-Specific Optimizations", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md884", [
          [ "A. Graphics Editor Improvements", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md885", null ],
          [ "B. Palette Editor Optimizations", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md886", null ]
        ] ],
        [ "5. Memory Management Improvements", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md887", [
          [ "A. Custom Allocator for Graphics Data", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md888", null ],
          [ "B. Smart Pointer Management", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md889", null ]
        ] ]
      ] ],
      [ "Implementation Priority", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md890", [
        [ "Phase 1 (High Impact, Low Risk)", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md891", null ],
        [ "Phase 2 (Medium Impact, Medium Risk)", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md892", null ],
        [ "Phase 3 (High Impact, High Risk)", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md893", null ]
      ] ],
      [ "Performance Metrics", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md894", [
        [ "Target Improvements", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md895", null ],
        [ "Measurement Tools", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md896", null ]
      ] ],
      [ "Conclusion", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md897", null ]
    ] ],
    [ "YAZE Graphics System Optimizations - Complete Implementation", "d6/df4/md_docs_2gfx__optimizations__complete.html", [
      [ "Overview", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md899", null ],
      [ "Implemented Optimizations", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md900", [
        [ "1. Palette Lookup Optimization ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md901", null ],
        [ "2. Dirty Region Tracking ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md902", null ],
        [ "3. Resource Pooling ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md903", null ],
        [ "4. LRU Tile Caching ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md904", null ],
        [ "5. Batch Operations ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md905", null ],
        [ "6. Memory Pool Allocator ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md906", null ],
        [ "7. Atlas-Based Rendering ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md907", null ],
        [ "8. Performance Profiling System ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md908", null ],
        [ "9. Performance Monitoring Dashboard ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md909", null ],
        [ "10. Optimization Validation Suite ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md910", null ]
      ] ],
      [ "Performance Metrics", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md911", [
        [ "Expected Improvements", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md912", null ],
        [ "Measurement Tools", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md913", null ]
      ] ],
      [ "Integration Points", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md914", [
        [ "Graphics Editor", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md915", null ],
        [ "Palette Editor", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md916", null ],
        [ "Screen Editor", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md917", null ]
      ] ],
      [ "Backward Compatibility", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md918", null ],
      [ "Usage Examples", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md919", [
        [ "Using Batch Operations", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md920", null ],
        [ "Using Memory Pool", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md921", null ],
        [ "Using Atlas Rendering", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md922", null ],
        [ "Using Performance Monitoring", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md923", null ]
      ] ],
      [ "Future Enhancements", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md924", [
        [ "Phase 2 Optimizations (Medium Priority)", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md925", null ],
        [ "Phase 3 Optimizations (High Priority)", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md926", null ]
      ] ],
      [ "Testing and Validation", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md927", [
        [ "Performance Testing", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md928", null ],
        [ "ROM Hacking Workflow Testing", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md929", null ]
      ] ],
      [ "Conclusion", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md930", null ],
      [ "Files Modified/Created", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md931", [
        [ "Core Graphics Classes", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md932", null ],
        [ "New Optimization Components", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md933", null ],
        [ "Testing and Validation", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md934", null ],
        [ "Build System", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md935", null ],
        [ "Documentation", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md936", null ]
      ] ]
    ] ],
    [ "YAZE Graphics System Optimizations - Implementation Summary", "d3/d7b/md_docs_2gfx__optimizations__implemented.html", [
      [ "Overview", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md938", null ],
      [ "Implemented Optimizations", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md939", [
        [ "1. Palette Lookup Optimization ✅ COMPLETED", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md940", null ],
        [ "2. Dirty Region Tracking ✅ COMPLETED", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md941", null ],
        [ "3. Resource Pooling ✅ COMPLETED", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md942", null ],
        [ "4. LRU Tile Caching ✅ COMPLETED", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md943", null ],
        [ "5. Region-Specific Texture Updates ✅ COMPLETED", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md944", null ],
        [ "6. Performance Profiling System ✅ COMPLETED", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md945", null ]
      ] ],
      [ "Performance Metrics", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md946", [
        [ "Expected Improvements", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md947", null ],
        [ "Measurement Tools", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md948", null ]
      ] ],
      [ "Integration Points", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md949", [
        [ "Graphics Editor", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md950", null ],
        [ "Palette Editor", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md951", null ],
        [ "Screen Editor", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md952", null ]
      ] ],
      [ "Backward Compatibility", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md953", null ],
      [ "Future Enhancements", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md954", [
        [ "Phase 2 Optimizations (Medium Priority)", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md955", null ],
        [ "Phase 3 Optimizations (High Priority)", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md956", null ]
      ] ],
      [ "Testing and Validation", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md957", [
        [ "Performance Testing", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md958", null ],
        [ "ROM Hacking Workflow Testing", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md959", null ]
      ] ],
      [ "Conclusion", "d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md960", null ]
    ] ],
    [ "YAZE Graphics Optimizations Project - Final Summary", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html", [
      [ "Project Overview", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md962", null ],
      [ "Completed Optimizations", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md963", [
        [ "✅ 1. Batch Operations for Texture Updates", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md964", null ],
        [ "✅ 2. Memory Pool Allocator", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md965", null ],
        [ "✅ 3. Atlas-Based Rendering System", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md966", null ],
        [ "✅ 4. Performance Monitoring Dashboard", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md967", null ],
        [ "✅ 5. Optimization Validation Suite", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md968", null ],
        [ "✅ 6. Debug Menu Integration", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md969", null ]
      ] ],
      [ "Performance Metrics Achieved", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md970", [
        [ "Expected Improvements (Based on Implementation)", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md971", null ],
        [ "Real Performance Data (From Timing Report)", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md972", null ]
      ] ],
      [ "Technical Implementation Details", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md973", [
        [ "Architecture Improvements", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md974", null ],
        [ "Code Quality Enhancements", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md975", null ]
      ] ],
      [ "Integration Points", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md976", [
        [ "Graphics System Integration", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md977", null ],
        [ "Editor Integration", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md978", null ]
      ] ],
      [ "Future Enhancements", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md979", [
        [ "Remaining Optimization (Pending)", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md980", null ],
        [ "Potential Extensions", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md981", null ]
      ] ],
      [ "Build and Testing", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md982", [
        [ "Build Status", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md983", null ],
        [ "Testing Status", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md984", null ]
      ] ],
      [ "Conclusion", "d8/dfb/md_docs_2gfx__optimizations__project__summary.html#autotoc_md985", null ]
    ] ],
    [ "YAZE Documentation", "d3/d4c/md_docs_2index.html", [
      [ "Quick Start", "d3/d4c/md_docs_2index.html#autotoc_md987", null ],
      [ "Development", "d3/d4c/md_docs_2index.html#autotoc_md988", null ],
      [ "Technical Documentation", "d3/d4c/md_docs_2index.html#autotoc_md989", [
        [ "Assembly & Code", "d3/d4c/md_docs_2index.html#autotoc_md990", null ],
        [ "Editor Systems", "d3/d4c/md_docs_2index.html#autotoc_md991", null ],
        [ "Overworld System", "d3/d4c/md_docs_2index.html#autotoc_md992", null ]
      ] ],
      [ "Key Features", "d3/d4c/md_docs_2index.html#autotoc_md993", null ]
    ] ],
    [ "YAZE Overworld Testing Guide", "de/d8a/md_docs_2overworld__testing__guide.html", [
      [ "Overview", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md996", null ],
      [ "Test Architecture", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md997", [
        [ "1. Golden Data System", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md998", null ],
        [ "2. Test Categories", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md999", [
          [ "Unit Tests (<tt>test/unit/zelda3/</tt>)", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1000", null ],
          [ "Integration Tests (<tt>test/e2e/</tt>)", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1001", null ],
          [ "Golden Data Tools", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1002", null ]
        ] ]
      ] ],
      [ "Quick Start", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1003", [
        [ "Prerequisites", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1004", null ],
        [ "Running Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1005", [
          [ "Basic Test Run", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1006", null ],
          [ "Selective Test Execution", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1007", null ]
        ] ]
      ] ],
      [ "Test Components", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1008", [
        [ "1. Golden Data Extractor", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1009", null ],
        [ "2. Integration Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1010", null ],
        [ "3. End-to-End Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1011", null ]
      ] ],
      [ "Test Validation Points", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1012", [
        [ "1. ZScream Compatibility", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1013", null ],
        [ "2. ROM State Validation", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1014", null ],
        [ "3. Performance and Stability", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1015", null ]
      ] ],
      [ "Environment Variables", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1016", [
        [ "Test Configuration", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1017", null ],
        [ "Build Configuration", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1018", null ]
      ] ],
      [ "Test Reports", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1019", [
        [ "Generated Reports", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1020", null ],
        [ "Report Location", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1021", null ]
      ] ],
      [ "Troubleshooting", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1022", [
        [ "Common Issues", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1023", [
          [ "1. ROM Not Found", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1024", null ],
          [ "2. Build Failures", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1025", null ],
          [ "3. Test Failures", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1026", null ]
        ] ],
        [ "Debug Mode", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1027", null ]
      ] ],
      [ "Advanced Usage", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1028", [
        [ "Custom Test Scenarios", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1029", [
          [ "1. Testing Custom ROMs", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1030", null ],
          [ "2. Regression Testing", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1031", null ],
          [ "3. Performance Testing", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1032", null ]
        ] ]
      ] ],
      [ "Integration with CI/CD", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1033", [
        [ "GitHub Actions Example", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1034", null ]
      ] ],
      [ "Contributing", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1035", [
        [ "Adding New Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1036", null ],
        [ "Test Guidelines", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1037", null ],
        [ "Example Test Structure", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1038", null ]
      ] ],
      [ "Conclusion", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1039", null ]
    ] ],
    [ "YAZE - Yet Another Zelda3 Editor", "d0/d30/md_README.html", [
      [ "Version 0.3.2 - Release", "d0/d30/md_README.html#autotoc_md1041", [
        [ "🛠️ Technical Improvements", "d0/d30/md_README.html#autotoc_md1045", null ]
      ] ],
      [ "Quick Start", "d0/d30/md_README.html#autotoc_md1046", [
        [ "Build", "d0/d30/md_README.html#autotoc_md1047", null ],
        [ "Applications", "d0/d30/md_README.html#autotoc_md1048", null ]
      ] ],
      [ "Usage", "d0/d30/md_README.html#autotoc_md1049", [
        [ "GUI Editor", "d0/d30/md_README.html#autotoc_md1050", null ],
        [ "Command Line Tool", "d0/d30/md_README.html#autotoc_md1051", null ],
        [ "C++ API", "d0/d30/md_README.html#autotoc_md1052", null ]
      ] ],
      [ "Documentation", "d0/d30/md_README.html#autotoc_md1053", null ],
      [ "Supported Platforms", "d0/d30/md_README.html#autotoc_md1054", null ],
      [ "ROM Compatibility", "d0/d30/md_README.html#autotoc_md1055", null ],
      [ "Contributing", "d0/d30/md_README.html#autotoc_md1056", null ],
      [ "License", "d0/d30/md_README.html#autotoc_md1057", null ],
      [ "🙏 Acknowledgments", "d0/d30/md_README.html#autotoc_md1058", null ],
      [ "📸 Screenshots", "d0/d30/md_README.html#autotoc_md1059", null ]
    ] ],
    [ "YAZE Build Scripts", "de/d82/md_scripts_2README.html", [
      [ "Windows Scripts", "de/d82/md_scripts_2README.html#autotoc_md1062", [
        [ "Setup Scripts", "de/d82/md_scripts_2README.html#autotoc_md1063", null ],
        [ "Build Scripts", "de/d82/md_scripts_2README.html#autotoc_md1064", null ],
        [ "Validation Scripts", "de/d82/md_scripts_2README.html#autotoc_md1065", null ],
        [ "Project Generation", "de/d82/md_scripts_2README.html#autotoc_md1066", null ]
      ] ],
      [ "Windows Compiler Recommendations", "de/d82/md_scripts_2README.html#autotoc_md1067", [
        [ "⚠️ Important: MSVC vs Clang on Windows", "de/d82/md_scripts_2README.html#autotoc_md1068", [
          [ "Why Clang is Recommended:", "de/d82/md_scripts_2README.html#autotoc_md1069", null ],
          [ "MSVC Issues:", "de/d82/md_scripts_2README.html#autotoc_md1070", null ]
        ] ],
        [ "Compiler Setup Options", "de/d82/md_scripts_2README.html#autotoc_md1071", [
          [ "Option 1: Clang (Recommended)", "de/d82/md_scripts_2README.html#autotoc_md1072", null ],
          [ "Option 2: MSVC with Workarounds", "de/d82/md_scripts_2README.html#autotoc_md1073", null ]
        ] ]
      ] ],
      [ "Quick Start (Windows)", "de/d82/md_scripts_2README.html#autotoc_md1074", [
        [ "Option 1: Automated Setup (Recommended)", "de/d82/md_scripts_2README.html#autotoc_md1075", null ],
        [ "Option 2: Manual Setup", "de/d82/md_scripts_2README.html#autotoc_md1076", null ],
        [ "Option 3: Using Batch Scripts", "de/d82/md_scripts_2README.html#autotoc_md1077", null ]
      ] ],
      [ "Script Options", "de/d82/md_scripts_2README.html#autotoc_md1078", [
        [ "setup-windows-dev.ps1", "de/d82/md_scripts_2README.html#autotoc_md1079", null ],
        [ "build-windows.ps1", "de/d82/md_scripts_2README.html#autotoc_md1080", null ],
        [ "build-windows.bat", "de/d82/md_scripts_2README.html#autotoc_md1081", null ]
      ] ],
      [ "Examples", "de/d82/md_scripts_2README.html#autotoc_md1082", null ],
      [ "Troubleshooting", "de/d82/md_scripts_2README.html#autotoc_md1083", [
        [ "Common Issues", "de/d82/md_scripts_2README.html#autotoc_md1084", null ],
        [ "Getting Help", "de/d82/md_scripts_2README.html#autotoc_md1085", null ]
      ] ],
      [ "Other Scripts", "de/d82/md_scripts_2README.html#autotoc_md1086", null ]
    ] ],
    [ "YAZE Test Suite", "d0/d46/md_test_2README.html", [
      [ "Directory Structure", "d0/d46/md_test_2README.html#autotoc_md1088", null ],
      [ "Test Categories", "d0/d46/md_test_2README.html#autotoc_md1089", [
        [ "Unit Tests (<tt>unit/</tt>)", "d0/d46/md_test_2README.html#autotoc_md1090", null ],
        [ "Integration Tests (<tt>integration/</tt>)", "d0/d46/md_test_2README.html#autotoc_md1091", null ],
        [ "End-to-End Tests (<tt>e2e/</tt>)", "d0/d46/md_test_2README.html#autotoc_md1092", null ]
      ] ],
      [ "Enhanced Test Runner", "d0/d46/md_test_2README.html#autotoc_md1093", [
        [ "Usage Examples", "d0/d46/md_test_2README.html#autotoc_md1094", null ],
        [ "Test Modes", "d0/d46/md_test_2README.html#autotoc_md1095", null ],
        [ "Options", "d0/d46/md_test_2README.html#autotoc_md1096", null ]
      ] ],
      [ "E2E ROM Testing", "d0/d46/md_test_2README.html#autotoc_md1097", [
        [ "Test Cases", "d0/d46/md_test_2README.html#autotoc_md1098", null ]
      ] ],
      [ "ZSCustomOverworld Upgrade Testing", "d0/d46/md_test_2README.html#autotoc_md1099", [
        [ "Supported Upgrades", "d0/d46/md_test_2README.html#autotoc_md1100", null ],
        [ "Test Cases", "d0/d46/md_test_2README.html#autotoc_md1101", null ],
        [ "Version-Specific Features", "d0/d46/md_test_2README.html#autotoc_md1102", [
          [ "Vanilla", "d0/d46/md_test_2README.html#autotoc_md1103", null ],
          [ "v2", "d0/d46/md_test_2README.html#autotoc_md1104", null ],
          [ "v3", "d0/d46/md_test_2README.html#autotoc_md1105", null ]
        ] ]
      ] ],
      [ "Environment Variables", "d0/d46/md_test_2README.html#autotoc_md1106", null ],
      [ "CI/CD Integration", "d0/d46/md_test_2README.html#autotoc_md1107", null ],
      [ "Deprecated Tests", "d0/d46/md_test_2README.html#autotoc_md1108", null ],
      [ "Best Practices", "d0/d46/md_test_2README.html#autotoc_md1109", null ],
      [ "AI Agent Testing", "d0/d46/md_test_2README.html#autotoc_md1110", null ]
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
"d1/d8a/room__entrance_8h.html#ab151afe9a3897a707a02c42d00c2f05e",
"d1/de2/classyaze_1_1test_1_1ZSCustomOverworldUpgradeTest.html#a967caedd6d3384f419f0768d37467d75",
"d2/d07/classyaze_1_1emu_1_1Spc700.html#a6876a7fe1572582805bcadebf9086682",
"d2/d46/classyaze_1_1editor_1_1PaletteEditor.html#a3202a27e79a0d9035f733e0e7f5ca306",
"d2/d79/structyaze_1_1zelda3_1_1anonymous__namespace_02room__object_8cc_03_1_1SubtypeTableInfo.html#aa73ab5ded6ab19ec4fdfd844fa361aad",
"d2/de7/classyaze_1_1editor_1_1DungeonCanvasViewer.html#a13fe086070d63616d7338f29fee19ecb",
"d2/dfe/structyaze_1_1gui_1_1EnhancedTheme.html#a61cc7d2488deb8b1563642e29f87d0ba",
"d3/d15/classyaze_1_1emu_1_1Snes.html#aeaaad0c9605917bf76b9ee0ce4ad5526",
"d3/d2e/compression_8cc.html#a4891117728978a92b253a51ce8406060",
"d3/d44/classyaze_1_1editor_1_1OverworldEditor.html#a7651ac8fd6e5bcb126af837d95465f8eae6ee5369534ca2bd9eab6e8c01407356",
"d3/d5a/classyaze_1_1zelda3_1_1OverworldEntrance.html#abaf1b01353ada583a35f4c74a11b6343",
"d3/d7b/md_docs_2gfx__optimizations__implemented.html#autotoc_md951",
"d3/d93/structyaze_1_1gfx_1_1CgxHeader.html#af7f39d215fb5283bbd1b7ff126c9823f",
"d3/dbf/canvas__usage__tracker_8h.html#abdcad1e308cbd093d77259209fc11465ad121683abd250843d718b565be12a720",
"d3/ded/classyaze_1_1emu_1_1Ppu.html#a4e31b1c4e583682381d0a7266955075a",
"d4/d0a/classyaze_1_1gfx_1_1PerformanceProfiler.html#ad59f68893ba7cbf9af495f2c77795015",
"d4/d2a/asar__integration__test_8cc.html#a3c211f25c9fb86fd7663934e957074b6",
"d4/d70/classyaze_1_1test_1_1IntegratedTestSuite.html#a6eb53c35692ce7d839a8f712ea7e029e",
"d4/db5/extension__manager_8h_source.html",
"d4/df3/classyaze_1_1zelda3_1_1MockRom.html#a9963c736a049df5d82802562b079b16e",
"d5/d1f/namespaceyaze_1_1zelda3.html#a479da514040cddc99b5da1235244c421a52a3e405eb69e72cd81af3c5e1f93065",
"d5/d1f/namespaceyaze_1_1zelda3.html#aa91208cef184c7ddaf379fb8e25f2f7b",
"d5/d3c/classyaze_1_1gfx_1_1GraphicsBuffer.html#a6ee445b33c33ffea7fcada595162b1a4",
"d5/da0/structyaze_1_1emu_1_1DspChannel.html#a4af0ed15c68d6645bf942cb07ba430fa",
"d5/dd8/classyaze_1_1gui_1_1canvas_1_1CanvasModals.html#a3aed7f018b62457c5742d58e99726e37",
"d6/d20/namespaceyaze_1_1emu.html#aaa27643e56c9546c09d625282f69c2b9",
"d6/d30/classyaze_1_1Rom.html#a88ec2d03d01e7d4aec9c03e7b2ed78a9",
"d6/d60/classyaze_1_1editor_1_1DungeonRoomLoader.html#ace2ff1490b46c030d44505094a46416f",
"d6/db1/classyaze_1_1zelda3_1_1Sprite.html#ac33a999f64741229b0fff347ac8b36e3",
"d6/de0/group__graphics.html#gaefa71f38cc28578cb8876442d0a7ffa7",
"d7/d4d/classyaze_1_1gui_1_1canvas_1_1CanvasUsageTracker.html#a2f4fbe8cdff478af1779801ec7100bd3",
"d7/d61/classyaze_1_1zelda3_1_1DungeonEditorSystem.html#a94c07e8126f35694c25d61054d98165f",
"d7/d6f/md_docs_2C1-changelog.html#autotoc_md560",
"d7/d9c/overworld_8h.html#ac4f3724a3dfea3eae43142de9a263c2a",
"d7/ddb/classyaze_1_1gfx_1_1ScopedTimer.html#a44149da3a5a0fbe1424260ca158eab3a",
"d7/df6/classyaze_1_1zelda3_1_1Room.html#af421077dfe90aeae045e74d9aea749fa",
"d8/d05/style_8cc.html#ade93c7dcd1ffbb9d83002534f950c8bc",
"d8/d57/structyaze_1_1zelda3_1_1ItemTypes_1_1ItemInfo.html#aa2a1e484bf9d008b1f432e319abf1495",
"d8/d98/classyaze_1_1editor_1_1DungeonEditor.html#a954ed561e9d3a05717001140dfe0ddde",
"d8/dc8/structyaze_1_1gfx_1_1PerformanceDashboard_1_1OptimizationStatus.html",
"d8/de1/structyaze_1_1gfx_1_1RenderCommand.html#a978dd9d52e53be5791daf21c1eb5bce6",
"d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md529",
"d9/dc0/room_8h.html#a693df677b1b2754452c8b8b4c4a98468a9024dfd424600f401cba474d615fbfe6",
"d9/dc5/classyaze_1_1zelda3_1_1Overworld.html#ac8964f7153a9e49bd61c81b936feab3d",
"d9/dcc/classyaze_1_1zelda3_1_1OverworldMap.html#aeba5cd212336456c55fe3c54663fe673",
"da/d0c/arena_8h.html",
"da/d2c/classyaze_1_1gui_1_1Canvas.html#a790f3ebecc655c19929d6ede556e1fc1",
"da/d3e/classyaze_1_1test_1_1TestManager.html#a2c642eb77f5ecd3a3867997f6c78f5e1",
"da/d68/classyaze_1_1gfx_1_1SnesPalette.html#a5c62fc26c60793c8b358542357c35fa7",
"da/dbc/classyaze_1_1emu_1_1AsmParser.html#a4bba6e54fc6ee626fee3b511e9b4d10c",
"db/d03/hyrule__magic_8h.html#a170971bb7a8dd89072c7180edc712057",
"db/d71/classyaze_1_1editor_1_1GfxGroupEditor.html#a262a8d6676fecb5d8416311090f16566",
"db/d82/classyaze_1_1editor_1_1Tile16Editor.html#af36e0c6be26ef1ba0b5c135da5acc32d",
"db/dc5/structyaze_1_1gfx_1_1MemoryPool_1_1MemoryBlock.html",
"db/de8/classyaze_1_1editor_1_1DungeonRoomSelector.html#a5ceb5556005b76112afaef3dd0c02548",
"dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md829",
"dc/d31/classyaze_1_1editor_1_1GraphicsEditor.html#af92f09e87b0331386b4257572993ed79",
"dc/d5d/structyaze_1_1emu_1_1ScreenDisplay.html#a67069e2bb21f04c86db07d2f930e2c6a",
"dc/dd1/structyaze_1_1editor_1_1MessagePreview.html#a0e79b06f99de0627d9cd32debd968fd3",
"dc/df4/classyaze_1_1emu_1_1Cpu.html#a4f12397b60733bea20587a2e65121c34",
"dc/df4/classyaze_1_1emu_1_1Cpu.html#ad56f428bfdc024f2a4e143e617d1d373",
"dd/d12/classyaze_1_1editor_1_1EditorManager.html#a85198b73afea6af2f4d8816879841f1c",
"dd/d33/structyaze_1_1emu_1_1M7SEL.html",
"dd/d60/editor_8h.html#a297b0603822af41a3d23fbc2da2a622aac935a934100d15241aefe3f7081e7767",
"dd/dc9/classyaze_1_1app_1_1core_1_1AsarWrapper.html#a0482599a7fe09fef6cc6d1e949768714",
"dd/de3/classyaze_1_1test_1_1PerformanceTestSuite.html#ada5577df2052450eec595adcc010251c",
"de/d0f/classyaze_1_1emu_1_1MemoryImpl.html#a9fedbbb77d8a2ccad32d68b137955d61",
"de/d7d/structyaze_1_1editor_1_1ExampleAppPropertyEditor.html#a5248683f77bb9506bcfba23791f55f55",
"de/dab/tui_8h.html#ab3acc1b59e84abb18e2d438f7ed2ad33a3c8c2786293aac7d768025f195c03176",
"de/dbf/icons_8h.html#a1c4f1cd5c6067023c10f89e6f2c91b94",
"de/dbf/icons_8h.html#a3b24451beedb0162c2654582793490eb",
"de/dbf/icons_8h.html#a5643b9b6b00c3845cb29b3af82c55b96",
"de/dbf/icons_8h.html#a743e587d29a1956d9682f4aaaee65bc7",
"de/dbf/icons_8h.html#a93ee4258b5a375e6c9a38e1abfc6fcd8",
"de/dbf/icons_8h.html#ab12db13fd42df7d957130067d272c07d",
"de/dbf/icons_8h.html#acde4b4fa4f47f426d904cc70e6c29f2a",
"de/dbf/icons_8h.html#ae8eb901e72eda0f69e7bafe84dd8b4b1",
"de/de7/classyaze_1_1zelda3_1_1DungeonObjectEditor.html#a0e0716930246c42b22663d42459ca228",
"de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md882",
"df/d2f/structyaze_1_1zelda3_1_1music_1_1ZeldaWave.html#a0d50beb4f54b170a57466174b9ff3695",
"df/dc2/structyaze_1_1editor_1_1PopupParams.html#a6ee0299a3e7d0c71a5fcd0a7337ee359",
"functions_vars_w.html"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';