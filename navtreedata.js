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
    [ "yaze Performance Optimization Summary", "db/de6/md_docs_2analysis_2performance__optimization__summary.html", [
      [ "🎉 <strong>Massive Performance Improvements Achieved!</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md162", [
        [ "📊 <strong>Overall Performance Results</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md163", null ]
      ] ],
      [ "🚀 <strong>Optimizations Implemented</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md164", [
        [ "1. <strong>Performance Monitoring System with Feature Flag</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md165", null ],
        [ "2. <strong>DungeonEditor Parallel Loading (79% Speedup)</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md166", null ],
        [ "3. <strong>Incremental Overworld Map Loading</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md167", null ],
        [ "4. <strong>On-Demand Map Reloading</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md168", null ]
      ] ],
      [ "Appendix A: Dungeon Editor Parallel Optimization", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md170", null ],
      [ "Appendix B: Overworld Load Optimization", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md172", null ],
      [ "Appendix C: Renderer Optimization", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md174", null ]
    ] ],
    [ "ZScream C# vs YAZE C++ Overworld Implementation Analysis", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html", [
      [ "Overview", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md176", null ],
      [ "Executive Summary", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md177", null ],
      [ "Detailed Comparison", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md178", [
        [ "1. Tile32 Loading and Expansion Detection", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md179", [
          [ "ZScream C# Logic (<tt>Overworld.cs:706-756</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md180", null ],
          [ "yaze C++ Logic (<tt>overworld.cc:AssembleMap32Tiles</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md181", null ]
        ] ],
        [ "2. Tile16 Loading and Expansion Detection", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md182", [
          [ "ZScream C# Logic (<tt>Overworld.cs:652-705</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md183", null ],
          [ "yaze C++ Logic (<tt>overworld.cc:AssembleMap16Tiles</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md184", null ]
        ] ],
        [ "3. Map Decompression", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md185", [
          [ "ZScream C# Logic (<tt>Overworld.cs:767-904</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md186", null ],
          [ "yaze C++ Logic (<tt>overworld.cc:DecompressAllMapTiles</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md187", null ]
        ] ],
        [ "4. Entrance Coordinate Calculation", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md188", [
          [ "ZScream C# Logic (<tt>Overworld.cs:974-1001</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md189", null ],
          [ "yaze C++ Logic (<tt>overworld.cc:LoadEntrances</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md190", null ]
        ] ],
        [ "5. Hole Coordinate Calculation with 0x400 Offset", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md191", [
          [ "ZScream C# Logic (<tt>Overworld.cs:1002-1025</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md192", null ],
          [ "yaze C++ Logic (<tt>overworld.cc:LoadHoles</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md193", null ]
        ] ],
        [ "6. ASM Version Detection for Item Loading", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md194", [
          [ "ZScream C# Logic (<tt>Overworld.cs:1032-1094</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md195", null ],
          [ "yaze C++ Logic (<tt>overworld.cc:LoadItems</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md196", null ]
        ] ],
        [ "7. Game State Handling for Sprite Loading", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md197", [
          [ "ZScream C# Logic (<tt>Overworld.cs:1276-1494</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md198", null ],
          [ "yaze C++ Logic (<tt>overworld.cc:LoadSprites</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md199", null ]
        ] ],
        [ "8. Map Size Assignment Logic", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md200", [
          [ "ZScream C# Logic (<tt>Overworld.cs:296-390</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md201", null ],
          [ "yaze C++ Logic (<tt>overworld.cc:AssignMapSizes</tt>)", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md202", null ]
        ] ]
      ] ],
      [ "ZSCustomOverworld Integration", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md203", [
        [ "Version Detection", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md204", null ],
        [ "Feature Enablement", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md205", null ]
      ] ],
      [ "Integration Test Coverage", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md206", null ],
      [ "Conclusion", "d8/d17/md_docs_2analysis_2zscream__yaze__overworld__comparison.html#autotoc_md207", null ]
    ] ],
    [ "Contributing", "dd/d5b/md_docs_2B1-contributing.html", [
      [ "Development Setup", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md209", [
        [ "Prerequisites", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md210", null ],
        [ "Quick Start", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md211", null ]
      ] ],
      [ "Code Style", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md212", [
        [ "C++ Standards", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md213", null ],
        [ "File Organization", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md214", null ],
        [ "Error Handling", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md215", null ]
      ] ],
      [ "Testing Requirements", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md216", [
        [ "Test Categories", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md217", null ],
        [ "Writing Tests", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md218", null ],
        [ "Test Execution", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md219", null ]
      ] ],
      [ "Pull Request Process", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md220", [
        [ "Before Submitting", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md221", null ],
        [ "Pull Request Template", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md222", null ]
      ] ],
      [ "Development Workflow", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md223", [
        [ "Branch Strategy", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md224", null ],
        [ "Commit Messages", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md225", null ],
        [ "Types", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md226", null ]
      ] ],
      [ "Architecture Guidelines", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md227", [
        [ "Component Design", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md228", null ],
        [ "Memory Management", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md229", null ],
        [ "Performance", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md230", null ]
      ] ],
      [ "Documentation", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md231", [
        [ "Code Documentation", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md232", null ],
        [ "API Documentation", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md233", null ]
      ] ],
      [ "Community Guidelines", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md234", [
        [ "Communication", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md235", null ],
        [ "Getting Help", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md236", null ]
      ] ],
      [ "Release Process", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md237", [
        [ "Version Numbering", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md238", null ],
        [ "Release Checklist", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md239", null ]
      ] ]
    ] ],
    [ "Platform Compatibility Improvements", "d1/d30/md_docs_2B2-platform-compatibility.html", [
      [ "Native File Dialog Support", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md241", null ],
      [ "Cross-Platform Build Reliability", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md242", null ],
      [ "Build Configuration Options", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md243", [
        [ "Full Build (Development)", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md244", null ],
        [ "Minimal Build", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md245", null ]
      ] ],
      [ "Implementation Details", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md246", null ],
      [ "Platform-Specific Adaptations", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md247", [
        [ "Windows", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md248", null ],
        [ "macOS", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md249", null ],
        [ "Linux", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md250", null ]
      ] ]
    ] ],
    [ "Build Presets Guide", "d7/d51/md_docs_2B3-build-presets.html", [
      [ "🍎 macOS ARM64 Presets (Recommended for Apple Silicon)", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md252", [
        [ "For Development Work:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md253", null ],
        [ "For Distribution:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md254", null ]
      ] ],
      [ "🔧 Why This Fixes Architecture Errors", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md255", null ],
      [ "📋 Available Presets", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md256", null ],
      [ "🚀 Quick Start", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md257", null ],
      [ "🛠️ IDE Integration", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md258", [
        [ "VS Code with CMake Tools:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md259", null ],
        [ "CLion:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md260", null ],
        [ "Xcode:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md261", null ]
      ] ],
      [ "🔍 Troubleshooting", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md262", null ],
      [ "📝 Notes", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md263", null ]
    ] ],
    [ "Release Workflows Documentation", "d3/d49/md_docs_2B4-release-workflows.html", [
      [ "Overview", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md265", null ],
      [ "1. Release-Simplified (<tt>release-simplified.yml</tt>)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md267", [
        [ "Purpose", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md268", null ],
        [ "Key Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md269", null ],
        [ "Use Cases", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md270", null ],
        [ "Configuration", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md271", null ],
        [ "Platforms Supported", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md272", null ]
      ] ],
      [ "2. Release (<tt>release.yml</tt>)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md274", [
        [ "Purpose", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md275", null ],
        [ "Key Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md276", null ],
        [ "Use Cases", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md277", null ],
        [ "Configuration", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md278", null ],
        [ "Advantages over Simplified", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md279", null ]
      ] ],
      [ "3. Release-Complex (<tt>release-complex.yml</tt>)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md281", [
        [ "Purpose", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md282", null ],
        [ "Key Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md283", null ],
        [ "Use Cases", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md284", null ],
        [ "Fallback Mechanisms", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md285", [
          [ "vcpkg Fallback", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md286", null ],
          [ "Chocolatey Integration", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md287", null ],
          [ "Build Configuration Fallback", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md288", null ]
        ] ],
        [ "Advanced Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md289", null ]
      ] ],
      [ "Workflow Comparison Matrix", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md291", null ],
      [ "When to Use Each Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md293", [
        [ "Use Simplified When:", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md294", null ],
        [ "Use Release When:", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md295", null ],
        [ "Use Complex When:", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md296", null ]
      ] ],
      [ "Workflow Selection Guide", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md298", [
        [ "For Development Team", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md299", null ],
        [ "For Release Manager", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md300", null ],
        [ "For CI/CD Pipeline", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md301", null ]
      ] ],
      [ "Configuration Examples", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md303", [
        [ "Triggering a Release", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md304", [
          [ "Manual Release (All Workflows)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md305", null ],
          [ "Automatic Release (Tag Push)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md306", null ]
        ] ],
        [ "Customizing Release Notes", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md307", null ]
      ] ],
      [ "Troubleshooting", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md309", [
        [ "Common Issues", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md310", [
          [ "vcpkg Failures (Windows)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md311", null ],
          [ "Dependency Conflicts", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md312", null ],
          [ "Build Failures", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md313", null ]
        ] ],
        [ "Debug Information", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md314", [
          [ "Simplified Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md315", null ],
          [ "Release Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md316", null ],
          [ "Complex Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md317", null ]
        ] ]
      ] ],
      [ "Best Practices", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md319", [
        [ "Workflow Selection", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md320", null ],
        [ "Release Process", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md321", null ],
        [ "Maintenance", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md322", null ]
      ] ],
      [ "Future Improvements", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md324", [
        [ "Planned Enhancements", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md325", null ],
        [ "Integration Opportunities", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md326", null ]
      ] ]
    ] ],
    [ "Stability, Testability & Release Workflow Improvements", "d9/d6b/md_docs_2B5-stability-improvements.html", [
      [ "Recent Improvements (v0.3.2)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md329", [
        [ "Windows Platform Stability", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md330", [
          [ "Stack Size Optimization", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md331", null ],
          [ "Development Utility Isolation", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md332", null ]
        ] ],
        [ "Graphics System Stability", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md333", [
          [ "Segmentation Fault Resolution", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md334", null ],
          [ "Comprehensive Bounds Checking", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md335", null ]
        ] ],
        [ "Build System Improvements", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md336", [
          [ "Modern Windows Workflow", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md337", null ],
          [ "Enhanced CI/CD Reliability", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md338", null ]
        ] ]
      ] ],
      [ "Recommended Optimizations", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md339", [
        [ "High Priority", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md340", [
          [ "1. Lazy Graphics Loading", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md341", null ],
          [ "2. Heap-Based Large Allocations", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md342", null ],
          [ "3. Streaming ROM Assets", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md343", null ]
        ] ],
        [ "Medium Priority", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md344", [
          [ "4. Enhanced Test Isolation", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md345", null ],
          [ "5. Dependency Caching Optimization", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md346", null ],
          [ "6. Memory Pool for Graphics", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md347", null ]
        ] ],
        [ "Low Priority", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md348", [
          [ "7. Build Time Optimization", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md349", null ],
          [ "8. Release Workflow Simplification", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md350", null ]
        ] ]
      ] ],
      [ "Testing Improvements", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md351", [
        [ "Current State", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md352", null ],
        [ "Recommendations", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md353", [
          [ "1. Visual Regression Testing", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md354", null ],
          [ "2. Performance Benchmarks", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md355", null ],
          [ "3. Fuzz Testing", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md356", null ]
        ] ]
      ] ],
      [ "Metrics & Monitoring", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md357", [
        [ "Current Measurements", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md358", null ],
        [ "Target Improvements", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md359", null ]
      ] ],
      [ "Action Items", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md360", [
        [ "Immediate (v0.3.2)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md361", null ],
        [ "Short Term (v0.3.3)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md362", null ],
        [ "Medium Term (v0.4.0)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md363", null ],
        [ "Long Term (v0.5.0+)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md364", null ]
      ] ],
      [ "Conclusion", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md365", null ]
    ] ],
    [ "Changelog", "d7/d6f/md_docs_2C1-changelog.html", [
      [ "0.3.2 (December 2025) - In Development", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md367", [
        [ "Tile16 Editor Improvements (In Progress)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md368", null ],
        [ "Graphics System Optimizations", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md369", null ],
        [ "Windows Platform Stability", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md370", null ],
        [ "Memory Safety & Stability", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md371", null ],
        [ "Testing & CI/CD Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md372", null ],
        [ "Future Optimizations (Planned)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md373", null ],
        [ "Technical Notes", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md374", null ]
      ] ],
      [ "0.3.1 (September 2025)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md375", [
        [ "Major Features", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md376", null ],
        [ "Tile16 Editor Enhancements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md377", null ],
        [ "ZSCustomOverworld v3 Implementation", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md378", null ],
        [ "Technical Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md379", null ],
        [ "User Interface", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md380", null ],
        [ "Bug Fixes", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md381", null ],
        [ "ZScream Compatibility Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md382", null ]
      ] ],
      [ "0.3.0 (September 2025)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md383", [
        [ "Major Features", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md384", null ],
        [ "User Interface & Theming", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md385", null ],
        [ "Enhancements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md386", null ],
        [ "Technical Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md387", null ],
        [ "Bug Fixes", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md388", null ]
      ] ],
      [ "0.2.2 (December 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md389", null ],
      [ "0.2.1 (August 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md390", null ],
      [ "0.2.0 (July 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md391", null ],
      [ "0.1.0 (May 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md392", null ],
      [ "0.0.9 (April 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md393", null ],
      [ "0.0.8 (February 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md394", null ],
      [ "0.0.7 (January 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md395", null ],
      [ "0.0.6 (November 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md396", null ],
      [ "0.0.5 (November 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md397", null ],
      [ "0.0.4 (November 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md398", null ],
      [ "0.0.3 (October 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md399", null ]
    ] ],
    [ "Canvas System - Comprehensive Guide", "d2/db2/md_docs_2CANVAS__GUIDE.html", [
      [ "Overview", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md401", null ],
      [ "Core Concepts", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md402", [
        [ "Canvas Structure", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md403", null ],
        [ "Coordinate Systems", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md404", null ]
      ] ],
      [ "Usage Patterns", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md405", [
        [ "Pattern 1: Basic Bitmap Display", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md406", null ],
        [ "Pattern 2: Modern Begin/End", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md407", null ],
        [ "Pattern 3: RAII ScopedCanvas", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md408", null ]
      ] ],
      [ "Feature: Tile Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md409", [
        [ "Single Tile Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md410", null ],
        [ "Tilemap Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md411", null ],
        [ "Color Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md412", null ]
      ] ],
      [ "Feature: Tile Selection", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md413", [
        [ "Single Tile Selection", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md414", null ],
        [ "Multi-Tile Rectangle Selection", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md415", null ],
        [ "Rectangle Drag & Paint", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md416", null ]
      ] ],
      [ "Feature: Custom Overlays", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md417", [
        [ "Manual Points Manipulation", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md418", null ]
      ] ],
      [ "Feature: Large Map Support", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md419", [
        [ "Map Types", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md420", null ],
        [ "Boundary Clamping", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md421", null ],
        [ "Custom Map Sizes", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md422", null ]
      ] ],
      [ "Feature: Context Menu", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md423", [
        [ "Adding Custom Items", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md424", null ],
        [ "Overworld Editor Example", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md425", null ]
      ] ],
      [ "Feature: Scratch Space (In Progress)", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md426", null ],
      [ "Common Workflows", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md427", [
        [ "Workflow 1: Overworld Tile Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md428", null ],
        [ "Workflow 2: Tile16 Blockset Selection", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md429", null ],
        [ "Workflow 3: Graphics Sheet Display", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md430", null ]
      ] ],
      [ "Configuration", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md431", [
        [ "Grid Settings", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md432", null ],
        [ "Scale Settings", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md433", null ],
        [ "Interaction Settings", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md434", null ],
        [ "Large Map Settings", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md435", null ]
      ] ],
      [ "Bug Fixes Applied", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md436", [
        [ "1. Rectangle Selection Wrapping in Large Maps ✅", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md437", null ],
        [ "2. Drag-Time Preview Clamping ✅", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md438", null ]
      ] ],
      [ "API Reference", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md439", [
        [ "Drawing Methods", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md440", null ],
        [ "State Accessors", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md441", null ],
        [ "Configuration", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md442", null ]
      ] ],
      [ "Implementation Notes", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md443", [
        [ "Points Management", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md444", null ],
        [ "Selection State", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md445", null ],
        [ "Overworld Rectangle Painting Flow", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md446", null ]
      ] ],
      [ "Best Practices", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md447", [
        [ "DO ✅", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md448", null ],
        [ "DON'T ❌", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md449", null ]
      ] ],
      [ "Troubleshooting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md450", [
        [ "Issue: Rectangle wraps at boundaries", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md451", null ],
        [ "Issue: Painting in wrong location", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md452", null ],
        [ "Issue: Array index out of bounds", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md453", null ],
        [ "Issue: Forgot to call End()", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md454", null ]
      ] ],
      [ "Future: Scratch Space", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md455", null ],
      [ "Documentation Files", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md456", null ],
      [ "Summary", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md457", null ]
    ] ],
    [ "Canvas Refactoring - Current Status & Future Work", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html", [
      [ "✅ Successfully Completed", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md459", [
        [ "1. Modern ImGui-Style Interface (Working)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md460", null ],
        [ "2. Context Menu Improvements (Working)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md461", null ],
        [ "3. Optional CanvasInteractionHandler Component (Available)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md462", null ],
        [ "4. Code Cleanup", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md463", null ]
      ] ],
      [ "⚠️ Outstanding Issue: Rectangle Selection Wrapping", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md464", [
        [ "The Problem", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md465", null ],
        [ "What Was Attempted", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md466", null ],
        [ "Root Cause Analysis", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md467", null ],
        [ "Suspected Issue", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md468", null ],
        [ "What Needs Investigation", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md469", null ],
        [ "Debugging Steps for Future Agent", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md470", null ],
        [ "Possible Fixes to Try", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md471", null ]
      ] ],
      [ "🔧 Files Modified", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md472", [
        [ "Core Canvas", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md473", null ],
        [ "Overworld Editor", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md474", null ],
        [ "Components Created", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md475", null ]
      ] ],
      [ "📚 Documentation (Consolidated)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md476", null ],
      [ "🎯 Future Refactoring Steps", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md477", [
        [ "Priority 1: Fix Rectangle Wrapping (High)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md478", null ],
        [ "Priority 2: Extract Coordinate Conversion Helpers (Low Impact)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md479", null ],
        [ "Priority 3: Move Components to canvas/ Namespace (Organizational)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md480", null ],
        [ "Priority 4: Complete Scratch Space Feature (Feature)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md481", null ],
        [ "Priority 5: Simplify Canvas State (Refactoring)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md482", null ]
      ] ],
      [ "🔍 Known Working Patterns", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md483", [
        [ "Overworld Tile Painting (Working)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md484", null ],
        [ "Blockset Selection (Working)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md485", null ],
        [ "Manual Overlay Highlighting (Working)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md486", null ]
      ] ],
      [ "🎓 Lessons Learned", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md487", [
        [ "What Worked", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md488", null ],
        [ "What Didn't Work", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md489", null ],
        [ "Key Insights", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md490", null ]
      ] ],
      [ "📋 For Future Agent", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md491", [
        [ "Immediate Task: Fix Rectangle Wrapping", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md492", null ],
        [ "Medium Term: Namespace Organization", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md493", null ],
        [ "Long Term: State Management Simplification", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md494", null ],
        [ "Stretch Goals: Enhanced Features", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md495", null ]
      ] ],
      [ "📊 Current Metrics", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md496", null ],
      [ "🔑 Key Files Reference", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md497", [
        [ "Core Canvas", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md498", null ],
        [ "Canvas Components", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md499", null ],
        [ "Utilities", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md500", null ],
        [ "Major Consumers", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md501", null ]
      ] ],
      [ "🎯 Recommended Next Steps", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md502", [
        [ "Step 1: Fix Rectangle Wrapping Bug (Critical)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md503", null ],
        [ "Step 2: Test All Editors (Verification)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md504", null ],
        [ "Step 3: Adopt Modern Patterns (Optional)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md505", null ]
      ] ],
      [ "📖 Documentation", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md506", [
        [ "Read This", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md507", null ],
        [ "Background (Optional)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md508", null ]
      ] ],
      [ "💡 Quick Reference", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md509", [
        [ "Modern Usage", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md510", null ],
        [ "Legacy Usage (Still Works)", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md511", null ],
        [ "Revert Clamping", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md512", null ],
        [ "Add Context Menu", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md513", null ]
      ] ],
      [ "✅ Current Status", "d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md514", null ]
    ] ],
    [ "Roadmap", "d6/db4/md_docs_2D1-roadmap.html", [
      [ "0.4.X (Next Major Release)", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md517", [
        [ "Core Features", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md518", null ],
        [ "Technical Improvements", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md519", null ]
      ] ],
      [ "0.5.X", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md520", [
        [ "Advanced Features", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md521", null ]
      ] ],
      [ "0.6.X", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md522", [
        [ "Platform & Integration", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md523", null ]
      ] ],
      [ "0.7.X", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md524", [
        [ "Performance & Polish", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md525", null ]
      ] ],
      [ "0.8.X", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md526", [
        [ "Beta Preparation", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md527", null ]
      ] ],
      [ "1.0.0", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md528", [
        [ "Stable Release", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md529", null ]
      ] ],
      [ "Current Focus Areas", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md530", [
        [ "Immediate Priorities (v0.4.X)", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md531", null ],
        [ "Long-term Vision", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md532", null ]
      ] ]
    ] ],
    [ "Dungeon Editing Implementation Plan for Yaze", "df/d42/md_docs_2dungeon__editing__implementation__plan.html", [
      [ "Executive Summary", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md534", null ],
      [ "Table of Contents", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md535", null ],
      [ "Current State Analysis", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md537", [
        [ "What Works in Yaze", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md538", null ],
        [ "What Doesn't Work", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md539", null ],
        [ "Key Problems Identified", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md540", null ]
      ] ],
      [ "ZScream's Working Approach", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md542", [
        [ "Room Data Structure (ZScream)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md543", null ],
        [ "Object Data Structure (ZScream)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md544", null ],
        [ "Object Encoding Format (Critical!)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md545", [
          [ "Type 1 Objects (ID 0x000-0x0FF) - Standard Objects", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md546", null ],
          [ "Type 2 Objects (ID 0x100-0x1FF) - Special Objects", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md547", null ],
          [ "Type 3 Objects (ID 0xF00-0xFFF) - Extended Objects", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md548", null ]
        ] ],
        [ "Layer and Door Markers", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md549", null ],
        [ "Object Loading Process (ZScream)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md550", null ],
        [ "Object Saving Process (ZScream)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md551", null ],
        [ "Object Tile Loading (ZScream)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md552", null ],
        [ "Object Drawing (ZScream)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md553", null ]
      ] ],
      [ "ROM Data Structure Reference", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md555", [
        [ "Key ROM Addresses (from ALTTP Disassembly)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md556", null ],
        [ "Room Header Format (14 bytes)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md557", null ],
        [ "Object Data Format", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md558", null ],
        [ "Tile Data Format", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md559", null ]
      ] ],
      [ "Implementation Tasks", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md561", [
        [ "Phase 1: Core Object System (HIGH PRIORITY)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md562", [
          [ "Task 1.1: Object Encoding/Decoding ✅ CRITICAL", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md563", null ],
          [ "Task 1.2: Enhanced Object Parsing ✅ CRITICAL", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md564", null ],
          [ "Task 1.3: Object Tile Loading ✅ CRITICAL", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md565", null ],
          [ "Task 1.4: Object Drawing System ⚠️ HIGH PRIORITY", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md566", null ]
        ] ],
        [ "Phase 2: Editor UI Integration (MEDIUM PRIORITY)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md568", [
          [ "Task 2.1: Object Placement System", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md569", null ],
          [ "Task 2.2: Object Selection System", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md570", null ],
          [ "Task 2.3: Object Properties Editor", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md571", null ],
          [ "Task 2.4: Layer Management UI", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md572", null ]
        ] ],
        [ "Phase 3: Save System (HIGH PRIORITY)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md574", [
          [ "Task 3.1: Room Object Encoding ✅ CRITICAL", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md575", null ],
          [ "Task 3.2: ROM Writing System", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md576", null ],
          [ "Task 3.3: Save Validation", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md577", null ]
        ] ],
        [ "Phase 4: Advanced Features (LOW PRIORITY)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md579", [
          [ "Task 4.1: Undo/Redo System", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md580", null ],
          [ "Task 4.2: Copy/Paste Objects", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md581", null ],
          [ "Task 4.3: Object Library/Templates", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md582", null ],
          [ "Task 4.4: Room Import/Export", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md583", null ]
        ] ]
      ] ],
      [ "Technical Architecture", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md585", [
        [ "Class Hierarchy", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md586", null ],
        [ "Data Flow", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md587", null ],
        [ "Key Algorithms", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md588", [
          [ "Object Type Detection", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md589", null ],
          [ "Object Decoding", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md590", null ],
          [ "Object Encoding", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md591", null ]
        ] ]
      ] ],
      [ "Testing Strategy", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md593", [
        [ "Unit Tests", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md594", [
          [ "Encoding/Decoding Tests", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md595", null ],
          [ "Room Parsing Tests", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md596", null ]
        ] ],
        [ "Integration Tests", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md597", [
          [ "Round-Trip Tests", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md598", null ]
        ] ],
        [ "Manual Testing", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md599", [
          [ "Test Cases", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md600", null ]
        ] ]
      ] ],
      [ "References", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md602", [
        [ "Documentation", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md603", null ],
        [ "Key Files", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md604", [
          [ "ZScream (Reference)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md605", null ],
          [ "Yaze (Implementation)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md606", null ]
        ] ],
        [ "External Resources", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md607", null ]
      ] ],
      [ "Appendix: Object ID Reference", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md609", [
        [ "Type 1 Objects (0x00-0xFF)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md610", null ],
        [ "Type 2 Objects (0x100-0x1FF)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md611", null ],
        [ "Type 3 Objects (0xF00-0xFFF)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md612", null ]
      ] ],
      [ "Status Tracking", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md614", [
        [ "Current Progress", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md615", null ],
        [ "Next Steps", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md616", null ],
        [ "Timeline Estimate", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md617", null ]
      ] ]
    ] ],
    [ "Asm Style Guide", "d7/d9a/md_docs_2E1-asm-style-guide.html", [
      [ "Table of Contents", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md620", null ],
      [ "File Structure", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md621", null ],
      [ "Labels and Symbols", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md622", null ],
      [ "Comments", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md623", null ],
      [ "Instructions", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md624", null ],
      [ "Macros", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md625", null ],
      [ "Loops and Branching", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md626", null ],
      [ "Data Structures", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md627", null ],
      [ "Code Organization", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md628", null ],
      [ "Custom Code", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md629", null ]
    ] ],
    [ "Dungeon Editor Guide", "dd/d33/md_docs_2E2-dungeon-editor-guide.html", [
      [ "Overview", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md631", null ],
      [ "Architecture", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md632", [
        [ "Core Components", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md633", [
          [ "1. DungeonEditorSystem", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md634", null ],
          [ "2. DungeonObjectEditor", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md635", null ],
          [ "3. ObjectRenderer", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md636", null ],
          [ "4. DungeonEditor (UI Layer)", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md637", null ]
        ] ]
      ] ],
      [ "Coordinate System", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md638", [
        [ "Room Coordinates vs Canvas Coordinates", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md639", [
          [ "Conversion Functions", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md640", null ]
        ] ],
        [ "Coordinate System Features", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md641", null ]
      ] ],
      [ "Object Rendering System", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md642", [
        [ "Object Types", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md643", null ],
        [ "Rendering Pipeline", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md644", null ],
        [ "Performance Optimizations", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md645", null ]
      ] ],
      [ "User Interface", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md646", [
        [ "Integrated Editing Panels", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md647", [
          [ "Main Canvas", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md648", null ],
          [ "Compact Editing Panels", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md649", null ]
        ] ],
        [ "Object Preview System", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md650", null ]
      ] ],
      [ "Integration with ZScream", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md651", [
        [ "Room Loading", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md652", null ],
        [ "Object Parsing", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md653", null ],
        [ "Coordinate System", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md654", null ]
      ] ],
      [ "Testing and Validation", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md655", [
        [ "Integration Tests", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md656", null ],
        [ "Test Data", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md657", null ],
        [ "Performance Benchmarks", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md658", null ]
      ] ],
      [ "Usage Examples", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md659", [
        [ "Basic Object Editing", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md660", null ],
        [ "Coordinate Conversion", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md661", null ],
        [ "Object Preview", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md662", null ]
      ] ],
      [ "Configuration Options", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md663", [
        [ "Editor Configuration", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md664", null ],
        [ "Performance Configuration", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md665", null ]
      ] ],
      [ "Troubleshooting", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md666", [
        [ "Common Issues", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md667", null ],
        [ "Debug Information", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md668", null ]
      ] ],
      [ "Future Enhancements", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md669", [
        [ "Planned Features", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md670", null ]
      ] ],
      [ "Conclusion", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md671", null ]
    ] ],
    [ "Dungeon Editor Design Plan", "dc/d82/md_docs_2E3-dungeon-editor-design.html", [
      [ "Overview", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md673", null ],
      [ "Architecture Overview", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md674", [
        [ "Core Components", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md675", null ],
        [ "File Structure", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md676", null ]
      ] ],
      [ "Component Responsibilities", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md677", [
        [ "DungeonEditor (Main Orchestrator)", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md678", null ],
        [ "DungeonRoomSelector", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md679", null ],
        [ "DungeonCanvasViewer", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md680", null ],
        [ "DungeonObjectSelector", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md681", null ]
      ] ],
      [ "Data Flow", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md682", [
        [ "Initialization Flow", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md683", null ],
        [ "Runtime Data Flow", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md684", null ],
        [ "Key Data Structures", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md685", null ]
      ] ],
      [ "Integration Patterns", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md686", [
        [ "Component Communication", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md687", null ],
        [ "ROM Data Management", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md688", null ],
        [ "State Synchronization", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md689", null ]
      ] ],
      [ "UI Layout Architecture", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md690", [
        [ "3-Column Layout", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md691", null ],
        [ "Component Internal Layout", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md692", null ]
      ] ],
      [ "Coordinate System", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md693", [
        [ "Room Coordinates vs Canvas Coordinates", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md694", null ],
        [ "Bounds Checking", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md695", null ]
      ] ],
      [ "Error Handling & Validation", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md696", [
        [ "ROM Validation", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md697", null ],
        [ "Bounds Validation", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md698", null ]
      ] ],
      [ "Performance Considerations", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md699", [
        [ "Caching Strategy", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md700", null ],
        [ "Rendering Optimization", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md701", null ]
      ] ],
      [ "Testing Strategy", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md702", [
        [ "Integration Tests", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md703", null ],
        [ "Test Categories", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md704", null ]
      ] ],
      [ "Future Development Guidelines", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md705", [
        [ "Adding New Features", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md706", null ],
        [ "Component Extension Patterns", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md707", null ],
        [ "Data Flow Extension", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md708", null ],
        [ "UI Layout Extension", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md709", null ]
      ] ],
      [ "Common Pitfalls & Solutions", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md710", [
        [ "Memory Management", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md711", null ],
        [ "Coordinate System", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md712", null ],
        [ "State Synchronization", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md713", null ],
        [ "Performance Issues", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md714", null ]
      ] ],
      [ "Debugging Tools", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md715", [
        [ "Debug Popup", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md716", null ],
        [ "Logging", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md717", null ]
      ] ],
      [ "Build Integration", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md718", [
        [ "CMake Configuration", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md719", null ],
        [ "Dependencies", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md720", null ]
      ] ],
      [ "Conclusion", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md721", null ]
    ] ],
    [ "DungeonEditor Refactoring Plan", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html", [
      [ "Overview", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md723", null ],
      [ "Component Structure", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md724", [
        [ "✅ Created Components", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md725", [
          [ "1. DungeonToolset (<tt>dungeon_toolset.h/cc</tt>)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md726", null ],
          [ "2. DungeonObjectInteraction (<tt>dungeon_object_interaction.h/cc</tt>)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md727", null ],
          [ "3. DungeonRenderer (<tt>dungeon_renderer.h/cc</tt>)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md728", null ],
          [ "4. DungeonRoomLoader (<tt>dungeon_room_loader.h/cc</tt>)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md729", null ],
          [ "5. DungeonUsageTracker (<tt>dungeon_usage_tracker.h/cc</tt>)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md730", null ]
        ] ]
      ] ],
      [ "Refactored DungeonEditor Structure", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md731", [
        [ "Before Refactoring: 1444 lines", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md732", null ],
        [ "After Refactoring: ~400 lines", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md733", null ]
      ] ],
      [ "Method Migration Map", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md734", [
        [ "Core Editor Methods (Keep in main file)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md735", null ],
        [ "UI Methods (Keep for coordination)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md736", null ],
        [ "Methods Moved to Components", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md737", [
          [ "→ DungeonToolset", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md738", null ],
          [ "→ DungeonObjectInteraction", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md739", null ],
          [ "→ DungeonRenderer", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md740", null ],
          [ "→ DungeonRoomLoader", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md741", null ],
          [ "→ DungeonUsageTracker", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md742", null ]
        ] ]
      ] ],
      [ "Component Communication", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md743", [
        [ "Callback System", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md744", null ],
        [ "Data Sharing", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md745", null ]
      ] ],
      [ "Benefits of Refactoring", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md746", [
        [ "1. <strong>Reduced Complexity</strong>", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md747", null ],
        [ "2. <strong>Improved Testability</strong>", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md748", null ],
        [ "3. <strong>Better Maintainability</strong>", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md749", null ],
        [ "4. <strong>Enhanced Extensibility</strong>", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md750", null ],
        [ "5. <strong>Cleaner Dependencies</strong>", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md751", null ]
      ] ],
      [ "Implementation Status", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md752", [
        [ "✅ Completed", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md753", null ],
        [ "🔄 In Progress", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md754", null ],
        [ "⏳ Pending", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md755", null ]
      ] ],
      [ "Migration Strategy", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md756", [
        [ "Phase 1: Create Components ✅", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md757", null ],
        [ "Phase 2: Integrate Components 🔄", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md758", null ],
        [ "Phase 3: Move Methods", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md759", null ],
        [ "Phase 4: Cleanup", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md760", null ]
      ] ]
    ] ],
    [ "Dungeon Object System", "da/d11/md_docs_2E5-dungeon-object-system.html", [
      [ "Overview", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md762", null ],
      [ "Architecture", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md763", [
        [ "Core Components", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md764", [
          [ "1. DungeonEditor (<tt>src/app/editor/dungeon/dungeon_editor.h</tt>)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md765", null ],
          [ "2. DungeonObjectSelector (<tt>src/app/editor/dungeon/dungeon_object_selector.h</tt>)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md766", null ],
          [ "3. DungeonCanvasViewer (<tt>src/app/editor/dungeon/dungeon_canvas_viewer.h</tt>)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md767", null ],
          [ "4. Room Management System (<tt>src/app/zelda3/dungeon/room.h</tt>)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md768", null ]
        ] ]
      ] ],
      [ "Object Types and Hierarchies", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md769", [
        [ "Room Objects", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md770", [
          [ "Type 1 Objects (0x00-0xFF)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md771", null ],
          [ "Type 2 Objects (0x100-0x1FF)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md772", null ],
          [ "Type 3 Objects (0x200+)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md773", null ]
        ] ],
        [ "Object Properties", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md774", null ]
      ] ],
      [ "How Object Placement Works", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md775", [
        [ "Selection Process", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md776", null ],
        [ "Placement Process", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md777", null ],
        [ "Code Flow", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md778", null ]
      ] ],
      [ "Rendering Pipeline", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md779", [
        [ "Object Rendering", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md780", null ],
        [ "Performance Optimizations", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md781", null ]
      ] ],
      [ "User Interface Components", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md782", [
        [ "Three-Column Layout", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md783", [
          [ "Column 1: Room Control Panel (280px fixed)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md784", null ],
          [ "Column 2: Windowed Canvas (800px fixed)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md785", null ],
          [ "Column 3: Object Selector/Editor (stretch)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md786", null ]
        ] ],
        [ "Debug and Control Features", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md787", [
          [ "Room Properties Table", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md788", null ],
          [ "Object Statistics", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md789", null ]
        ] ]
      ] ],
      [ "Integration with ROM Data", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md790", [
        [ "Data Sources", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md791", [
          [ "Room Headers (<tt>0x1F8000</tt>)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md792", null ],
          [ "Object Data", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md793", null ],
          [ "Graphics Data", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md794", null ]
        ] ],
        [ "Assembly Integration", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md795", null ]
      ] ],
      [ "Comparison with ZScream", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md796", [
        [ "Architectural Differences", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md797", [
          [ "Component-Based Architecture", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md798", null ],
          [ "Real-time Rendering", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md799", null ],
          [ "UI Organization", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md800", null ],
          [ "Caching Strategy", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md801", null ]
        ] ],
        [ "Shared Concepts", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md802", null ]
      ] ],
      [ "Best Practices", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md803", [
        [ "Performance", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md804", null ],
        [ "Code Organization", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md805", null ],
        [ "User Experience", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md806", null ]
      ] ],
      [ "Future Enhancements", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md807", [
        [ "Planned Features", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md808", null ],
        [ "Technical Improvements", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md809", null ]
      ] ]
    ] ],
    [ "Tile16 Editor Palette System", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html", [
      [ "Executive Summary", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md811", null ],
      [ "Problem Analysis", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md812", [
        [ "Critical Issues Identified", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md813", null ]
      ] ],
      [ "Solution Architecture", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md814", [
        [ "Core Design Principles", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md815", null ],
        [ "256-Color Overworld Palette Structure", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md816", null ],
        [ "Sheet-to-Palette Mapping", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md817", null ],
        [ "Palette Button Mapping", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md818", null ]
      ] ],
      [ "Implementation Details", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md819", [
        [ "1. Fixed SetPaletteWithTransparent Method", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md820", null ],
        [ "2. Corrected Tile16 Editor Palette System", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md821", null ],
        [ "3. Palette Coordination Flow", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md822", null ]
      ] ],
      [ "UI/UX Refactoring", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md823", [
        [ "New Three-Column Layout", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md824", null ],
        [ "Canvas Context Menu Fixes", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md825", null ],
        [ "Dynamic Zoom Controls", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md826", null ]
      ] ],
      [ "Testing Protocol", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md827", [
        [ "Crash Prevention Testing", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md828", null ],
        [ "Color Alignment Testing", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md829", null ],
        [ "UI/UX Testing", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md830", null ]
      ] ],
      [ "Error Handling", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md831", [
        [ "Bounds Checking", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md832", null ],
        [ "Fallback Mechanisms", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md833", null ]
      ] ],
      [ "Debug Information Display", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md834", null ],
      [ "Known Issues and Ongoing Work", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md835", [
        [ "Completed Items ✅", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md836", null ],
        [ "Active Issues ⚠️", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md837", null ],
        [ "Current Status Summary", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md838", null ],
        [ "Future Enhancements", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md839", null ]
      ] ],
      [ "Maintenance Notes", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md840", null ],
      [ "Next Steps", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md842", [
        [ "Immediate Priorities", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md843", null ],
        [ "Investigation Areas", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md844", null ]
      ] ]
    ] ],
    [ "Overworld Loading Guide", "dc/d12/md_docs_2F1-overworld-loading.html", [
      [ "Table of Contents", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md847", null ],
      [ "Overview", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md848", null ],
      [ "ROM Types and Versions", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md849", [
        [ "Version Detection", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md850", null ],
        [ "Feature Support by Version", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md851", null ]
      ] ],
      [ "Overworld Map Structure", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md852", [
        [ "Core Properties", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md853", null ]
      ] ],
      [ "Overlays and Special Area Maps", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md854", [
        [ "Understanding Overlays", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md855", null ],
        [ "Special Area Maps (0x80-0x9F)", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md856", null ],
        [ "Overlay ID Mappings", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md857", null ],
        [ "Drawing Order", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md858", null ],
        [ "Vanilla Overlay Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md859", null ],
        [ "Special Area Graphics Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md860", null ]
      ] ],
      [ "Loading Process", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md861", [
        [ "1. Version Detection", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md862", null ],
        [ "2. Map Initialization", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md863", null ],
        [ "3. Property Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md864", [
          [ "Vanilla ROMs (asm_version == 0xFF)", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md865", null ],
          [ "ZSCustomOverworld v2/v3", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md866", null ]
        ] ],
        [ "4. Custom Data Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md867", null ]
      ] ],
      [ "ZScream Implementation", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md868", [
        [ "OverworldMap Constructor", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md869", null ],
        [ "Key Methods", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md870", null ]
      ] ],
      [ "Yaze Implementation", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md871", [
        [ "OverworldMap Constructor", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md872", null ],
        [ "Key Methods", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md873", null ],
        [ "Current Status", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md874", null ]
      ] ],
      [ "Key Differences", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md875", [
        [ "1. Language and Architecture", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md876", null ],
        [ "2. Data Structures", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md877", null ],
        [ "3. Error Handling", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md878", null ],
        [ "4. Graphics Processing", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md879", null ]
      ] ],
      [ "Common Issues and Solutions", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md880", [
        [ "1. Version Detection Issues", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md881", null ],
        [ "2. Palette Loading Errors", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md882", null ],
        [ "3. Graphics Not Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md883", null ],
        [ "4. Overlay Issues", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md884", null ],
        [ "5. Large Map Problems", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md885", null ],
        [ "6. Special Area Graphics Issues", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md886", null ]
      ] ],
      [ "Best Practices", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md887", [
        [ "1. Version-Specific Code", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md888", null ],
        [ "2. Error Handling", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md889", null ],
        [ "3. Memory Management", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md890", null ],
        [ "4. Thread Safety", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md891", null ]
      ] ],
      [ "Conclusion", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md892", null ]
    ] ],
    [ "YAZE Graphics System Optimization Recommendations", "de/def/md_docs_2gfx__optimization__recommendations.html", [
      [ "Overview", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md894", null ],
      [ "Current Architecture Analysis", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md895", [
        [ "Strengths", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md896", null ],
        [ "Performance Bottlenecks Identified", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md897", [
          [ "1. Bitmap Class Issues", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md898", null ],
          [ "2. Arena Resource Management", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md899", null ],
          [ "3. Tilemap Performance", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md900", null ]
        ] ]
      ] ],
      [ "Optimization Recommendations", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md901", [
        [ "1. Bitmap Class Optimizations", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md902", [
          [ "A. Palette Lookup Optimization", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md903", null ],
          [ "B. Dirty Region Tracking", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md904", null ]
        ] ],
        [ "2. Arena Resource Management Improvements", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md905", [
          [ "A. Resource Pooling", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md906", null ],
          [ "B. Batch Operations", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md907", null ]
        ] ],
        [ "3. Tilemap Performance Enhancements", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md908", [
          [ "A. Smart Tile Caching", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md909", null ],
          [ "B. Atlas-based Rendering", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md910", null ]
        ] ],
        [ "4. Editor-Specific Optimizations", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md911", [
          [ "A. Graphics Editor Improvements", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md912", null ],
          [ "B. Palette Editor Optimizations", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md913", null ]
        ] ],
        [ "5. Memory Management Improvements", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md914", [
          [ "A. Custom Allocator for Graphics Data", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md915", null ],
          [ "B. Smart Pointer Management", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md916", null ]
        ] ]
      ] ],
      [ "Implementation Priority", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md917", [
        [ "Phase 1 (High Impact, Low Risk)", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md918", null ],
        [ "Phase 2 (Medium Impact, Medium Risk)", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md919", null ],
        [ "Phase 3 (High Impact, High Risk)", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md920", null ]
      ] ],
      [ "Performance Metrics", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md921", [
        [ "Target Improvements", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md922", null ],
        [ "Measurement Tools", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md923", null ]
      ] ],
      [ "Conclusion", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md924", null ]
    ] ],
    [ "YAZE Graphics System Optimizations - Complete Implementation", "d6/df4/md_docs_2gfx__optimizations__complete.html", [
      [ "Overview", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md926", null ],
      [ "Implemented Optimizations", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md927", [
        [ "1. Palette Lookup Optimization ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md928", null ],
        [ "2. Dirty Region Tracking ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md929", null ],
        [ "3. Resource Pooling ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md930", null ],
        [ "4. LRU Tile Caching ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md931", null ],
        [ "5. Batch Operations ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md932", null ],
        [ "6. Memory Pool Allocator ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md933", null ],
        [ "7. Atlas-Based Rendering ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md934", [
          [ "Core Components", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md935", [
            [ "1. AtlasRenderer Class (<tt>src/app/gfx/atlas_renderer.h/cc</tt>)", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md936", null ],
            [ "2. RenderCommand Structure", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md937", null ],
            [ "3. Atlas Statistics Tracking", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md938", null ]
          ] ],
          [ "Integration Points", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md939", [
            [ "1. Tilemap Integration (<tt>src/app/gfx/tilemap.h/cc</tt>)", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md940", null ],
            [ "2. Performance Dashboard Integration", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md941", null ]
          ] ],
          [ "Technical Implementation", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md942", [
            [ "Atlas Packing Algorithm", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md943", null ],
            [ "Batch Rendering Process", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md944", null ]
          ] ]
        ] ],
        [ "8. Performance Profiling System ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md945", null ],
        [ "9. Performance Monitoring Dashboard ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md946", null ],
        [ "10. Optimization Validation Suite ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md947", null ]
      ] ],
      [ "Performance Metrics", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md948", [
        [ "Expected Improvements", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md949", null ],
        [ "Measurement Tools", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md950", null ]
      ] ],
      [ "Integration Points", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md951", [
        [ "Graphics Editor", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md952", null ],
        [ "Palette Editor", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md953", null ],
        [ "Screen Editor", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md954", null ]
      ] ],
      [ "Backward Compatibility", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md955", null ],
      [ "Usage Examples", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md956", [
        [ "Using Batch Operations", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md957", null ],
        [ "Using Memory Pool", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md958", null ],
        [ "Using Atlas Rendering", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md959", null ],
        [ "Using Performance Monitoring", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md960", null ]
      ] ],
      [ "Future Enhancements", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md961", [
        [ "Phase 2 Optimizations (Medium Priority)", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md962", null ],
        [ "Phase 3 Optimizations (High Priority)", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md963", null ]
      ] ],
      [ "Testing and Validation", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md964", [
        [ "Performance Testing", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md965", null ],
        [ "ROM Hacking Workflow Testing", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md966", null ]
      ] ],
      [ "Conclusion", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md967", null ],
      [ "Files Modified/Created", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md968", [
        [ "Core Graphics Classes", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md969", null ],
        [ "New Optimization Components", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md970", null ],
        [ "Testing and Validation", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md971", null ],
        [ "Build System", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md972", null ],
        [ "Documentation", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md973", null ]
      ] ]
    ] ],
    [ "yaze Documentation", "d3/d4c/md_docs_2index.html", [
      [ "Quick Start", "d3/d4c/md_docs_2index.html#autotoc_md975", null ],
      [ "Development", "d3/d4c/md_docs_2index.html#autotoc_md976", null ],
      [ "Technical Documentation", "d3/d4c/md_docs_2index.html#autotoc_md977", [
        [ "Assembly & Code", "d3/d4c/md_docs_2index.html#autotoc_md978", null ],
        [ "Editor Systems", "d3/d4c/md_docs_2index.html#autotoc_md979", null ],
        [ "Overworld System", "d3/d4c/md_docs_2index.html#autotoc_md980", null ]
      ] ],
      [ "Key Features", "d3/d4c/md_docs_2index.html#autotoc_md981", null ]
    ] ],
    [ "yaze Overworld Testing Guide", "de/d8a/md_docs_2overworld__testing__guide.html", [
      [ "Overview", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md984", null ],
      [ "Test Architecture", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md985", [
        [ "1. Golden Data System", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md986", null ],
        [ "2. Test Categories", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md987", [
          [ "Unit Tests (<tt>test/unit/zelda3/</tt>)", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md988", null ],
          [ "Integration Tests (<tt>test/e2e/</tt>)", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md989", null ],
          [ "Golden Data Tools", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md990", null ]
        ] ]
      ] ],
      [ "Quick Start", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md991", [
        [ "Prerequisites", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md992", null ],
        [ "Running Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md993", [
          [ "Basic Test Run", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md994", null ],
          [ "Selective Test Execution", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md995", null ]
        ] ]
      ] ],
      [ "Test Components", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md996", [
        [ "1. Golden Data Extractor", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md997", null ],
        [ "2. Integration Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md998", null ],
        [ "3. End-to-End Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md999", null ]
      ] ],
      [ "Test Validation Points", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1000", [
        [ "1. ZScream Compatibility", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1001", null ],
        [ "2. ROM State Validation", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1002", null ],
        [ "3. Performance and Stability", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1003", null ]
      ] ],
      [ "Environment Variables", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1004", [
        [ "Test Configuration", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1005", null ],
        [ "Build Configuration", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1006", null ]
      ] ],
      [ "Test Reports", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1007", [
        [ "Generated Reports", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1008", null ],
        [ "Report Location", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1009", null ]
      ] ],
      [ "Troubleshooting", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1010", [
        [ "Common Issues", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1011", [
          [ "1. ROM Not Found", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1012", null ],
          [ "2. Build Failures", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1013", null ],
          [ "3. Test Failures", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1014", null ]
        ] ],
        [ "Debug Mode", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1015", null ]
      ] ],
      [ "Advanced Usage", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1016", [
        [ "Custom Test Scenarios", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1017", [
          [ "1. Testing Custom ROMs", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1018", null ],
          [ "2. Regression Testing", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1019", null ],
          [ "3. Performance Testing", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1020", null ]
        ] ]
      ] ],
      [ "Integration with CI/CD", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1021", [
        [ "GitHub Actions Example", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1022", null ]
      ] ],
      [ "Contributing", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1023", [
        [ "Adding New Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1024", null ],
        [ "Test Guidelines", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1025", null ],
        [ "Example Test Structure", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1026", null ]
      ] ],
      [ "Conclusion", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1027", null ]
    ] ],
    [ "z3ed Agent Roadmap", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html", [
      [ "Current Status", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1029", [
        [ "✅ Production Ready", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1030", null ],
        [ "🚧 Active Work", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1031", null ]
      ] ],
      [ "Core Vision", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1032", null ],
      [ "Technical Architecture", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1033", [
        [ "1. Conversational Agent Service ✅", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1034", null ],
        [ "2. Read-Only Tools ✅", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1035", null ],
        [ "3. Chat Interfaces", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1036", null ],
        [ "4. Proposal Workflow Integration", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1037", null ]
      ] ],
      [ "Immediate Priorities", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1038", [
        [ "Priority 1: Live LLM Testing (1-2 hours)", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1039", null ],
        [ "Priority 2: GUI Chat Integration (4-6 hours)", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1040", null ],
        [ "Priority 3: Proposal Generation (6-8 hours)", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1041", null ]
      ] ],
      [ "Technical Implementation Plan", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1042", [
        [ "1. Conversational Agent Service", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1043", null ],
        [ "2. Read-Only \"Tools\" for the Agent", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1044", null ],
        [ "3. TUI and GUI Chat Interfaces", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1045", null ],
        [ "4. Integration with the Proposal Workflow", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1046", null ]
      ] ],
      [ "Next Steps", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1047", [
        [ "Immediate Priorities", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1048", null ]
      ] ],
      [ "Current Status & Next Steps (Updated: October 3, 2025)", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1049", [
        [ "✅ Completed", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1050", null ],
        [ "🚧 In Progress", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1051", null ],
        [ "🚀 Next Steps (Priority Order)", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1052", [
          [ "Priority 1: Live LLM Testing with Function Calling (1-2 hours)", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1053", null ],
          [ "Priority 2: Implement GUI Chat Widget (6-8 hours)", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1054", null ],
          [ "Priority 3: Proposal Generation (6-8 hours)", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1055", null ]
        ] ]
      ] ],
      [ "Command Reference", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1056", [
        [ "Chat Modes", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1057", null ],
        [ "Tool Commands (for direct testing)", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1058", null ]
      ] ],
      [ "Build Quick Reference", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1059", null ],
      [ "Future Enhancements", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1060", [
        [ "Short Term (1-2 months)", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1061", null ],
        [ "Medium Term (3-6 months)", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1062", null ],
        [ "Long Term (6+ months)", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1063", [
          [ "Priority 4: Performance and Caching (4-6 hours)", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1064", null ]
        ] ]
      ] ],
      [ "z3ed Build Quick Reference", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1065", null ],
      [ "Build Flags Explained", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1066", null ],
      [ "Feature Matrix", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1067", null ],
      [ "Common Build Scenarios", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1068", [
        [ "Developer (AI features, no GUI testing)", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1069", null ],
        [ "Full Stack (AI + GUI automation)", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1070", null ],
        [ "CI/CD (minimal, fast)", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1071", null ],
        [ "Release Build (optimized)", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1072", null ]
      ] ],
      [ "Migration from Old Flags", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1073", [
        [ "Before (Confusing)", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1074", null ],
        [ "After (Clear Intent)", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1075", null ]
      ] ],
      [ "Troubleshooting", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1076", [
        [ "\"Build with -DZ3ED_AI=ON\" warning", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1077", null ],
        [ "\"OpenSSL not found\" warning", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1078", null ],
        [ "Ollama vs Gemini not auto-detecting", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1079", null ]
      ] ],
      [ "Environment Variables", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1080", null ],
      [ "Platform-Specific Notes", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1081", [
        [ "macOS", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1082", null ],
        [ "Linux", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1083", null ],
        [ "Windows", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1084", null ]
      ] ],
      [ "Performance Tips", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1085", [
        [ "Faster Incremental Builds", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1086", null ],
        [ "Reduce Build Scope", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1087", null ]
      ] ],
      [ "Related Documentation", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1088", null ],
      [ "Quick Test", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1089", null ],
      [ "Support", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1090", null ],
      [ "Summary", "d6/db7/md_docs_2z3ed_2AGENT-ROADMAP.html#autotoc_md1091", null ]
    ] ],
    [ "z3ed CLI Architecture & Design", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html", [
      [ "1. Overview", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1093", null ],
      [ "2. Design Goals", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1094", [
        [ "2.1. Key Architectural Decisions", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1095", null ]
      ] ],
      [ "3. Proposed CLI Architecture: Resource-Oriented Commands", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1096", [
        [ "3.1. Top-Level Resources", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1097", null ],
        [ "3.2. Example Command Mapping", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1098", null ]
      ] ],
      [ "4. New Features & Commands", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1099", [
        [ "4.1. For the ROM Hacker (Power & Scriptability)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1100", null ],
        [ "4.2. For Testing & Automation", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1101", null ]
      ] ],
      [ "5. TUI Enhancements", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1102", null ],
      [ "6. Generative & Agentic Workflows (MCP Integration)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1103", [
        [ "6.1. The Generative Workflow", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1104", null ],
        [ "6.2. Key Enablers", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1105", null ]
      ] ],
      [ "7. Implementation Roadmap", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1106", [
        [ "Phase 1: Core CLI & TUI Foundation (Done)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1107", null ],
        [ "Phase 2: Interactive TUI & Command Palette (Done)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1108", null ],
        [ "Phase 3: Testing & Project Management (Done)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1109", null ],
        [ "Phase 4: Agentic Framework & Generative AI (✅ Foundation Complete, 🚧 LLM Integration In Progress)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1110", null ],
        [ "Phase 5: Code Structure & UX Improvements (Completed)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1111", null ],
        [ "Phase 6: Resource Catalogue & API Documentation (✅ Completed - Oct 1, 2025)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1112", null ]
      ] ],
      [ "8. Agentic Framework Architecture - Advanced Dive", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1113", [
        [ "8.1. The <tt>z3ed agent</tt> Command", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1114", null ],
        [ "8.2. The Agentic Loop (MCP) - Detailed Workflow", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1115", null ],
        [ "8.3. AI Model & Protocol Strategy", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1116", null ]
      ] ],
      [ "9. Test Harness Evolution: From Automation to Platform", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1118", [
        [ "9.1. Current Capabilities (IT-01 to IT-04) ✅", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1119", null ],
        [ "9.2. Limitations Identified", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1120", null ],
        [ "9.3. Enhancement Roadmap (IT-05 to IT-09)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1121", [
          [ "IT-05: Test Introspection API (6-8 hours)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1122", null ],
          [ "IT-06: Widget Discovery API (4-6 hours)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1123", null ],
          [ "IT-07: Test Recording & Replay ✅ COMPLETE", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1124", null ],
          [ "IT-08: Holistic Error Reporting (5-7 hours)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1125", null ],
          [ "IT-09: CI/CD Integration ✅ CLI Foundations Complete", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1126", null ]
        ] ],
        [ "9.4. Unified Testing Vision", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1127", null ],
        [ "9.5. Implementation Priority", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1128", null ],
        [ "8.4. GUI Integration & User Experience", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1130", null ],
        [ "8.5. Testing & Verification", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1131", null ],
        [ "8.6. Safety & Sandboxing", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1132", null ],
        [ "8.7. Optional JSON Dependency", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1133", null ],
        [ "8.8. Contextual Awareness & Feedback Loop", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1134", null ],
        [ "8.9. Error Handling and Recovery", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1135", null ],
        [ "8.10. Extensibility", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1136", null ]
      ] ],
      [ "9. UX Improvements and Architectural Decisions", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1137", [
        [ "9.1. TUI Component Architecture", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1138", null ],
        [ "9.2. Command Handler Unification", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1139", null ],
        [ "9.3. Interface Consolidation", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1140", null ],
        [ "9.4. Code Organization Improvements", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1141", null ],
        [ "9.5. Future UX Enhancements", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1142", null ]
      ] ],
      [ "10. Implementation Status and Code Quality", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1143", [
        [ "10.1. Recent Refactoring Improvements (January 2025)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1144", null ],
        [ "10.2. File Organization", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1145", null ],
        [ "10.3. Code Quality Improvements", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1146", null ],
        [ "10.4. TUI Component System", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1147", null ],
        [ "10.5. Known Limitations", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1148", null ],
        [ "10.6. Future Code Quality Goals", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1149", null ]
      ] ],
      [ "11. Agent-Ready API Surface Area", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1150", null ],
      [ "12. Acceptance & Review Workflow", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1151", [
        [ "12.1. Change Proposal Lifecycle", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1152", null ],
        [ "12.2. UI Extensions", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1153", null ],
        [ "12.3. Policy Configuration", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1154", null ]
      ] ],
      [ "13. ImGuiTestEngine Control Bridge", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1155", [
        [ "13.1. Bridge Architecture", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1156", [
          [ "13.1.1. Transport & Envelope", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1157", null ],
          [ "13.1.2. Harness Runtime Lifecycle", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1158", null ],
          [ "13.1.3. Integration with <tt>z3ed agent</tt>", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1159", null ]
        ] ],
        [ "13.2. Safety & Sandboxing", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1160", null ],
        [ "13.3. Script Generation Strategy", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1161", null ]
      ] ],
      [ "14. Test & Verification Strategy", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1162", [
        [ "14.1. Layered Test Suites", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1163", null ],
        [ "14.2. Continuous Verification", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1164", null ],
        [ "14.3. Telemetry-Informed Testing", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1165", null ]
      ] ],
      [ "15. Expanded Roadmap (Phase 6+)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1166", [
        [ "Phase 6: Agent Workflow Foundations (Planned)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1167", null ],
        [ "Phase 7: Controlled Mutation & Review (Planned)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1168", null ],
        [ "Phase 8: Learning & Self-Improvement (Exploratory)", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1169", null ],
        [ "7.4. Widget ID Management for Test Automation", "d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1170", null ]
      ] ]
    ] ],
    [ "z3ed Agentic Workflow Plan", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html", [
      [ "Executive Summary", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1172", null ],
      [ "Quick Reference", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1173", null ],
      [ "1. Current Priorities (Week of Oct 2-8, 2025)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1175", [
        [ "Priority 1: Test Harness Enhancements (IT-05 to IT-09) 🔧 ACTIVE", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1176", [
          [ "IT-05: Test Introspection API (6-8 hours)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1177", null ],
          [ "IT-06: Widget Discovery API (4-6 hours)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1178", null ],
          [ "IT-07: Test Recording & Replay ✅ COMPLETE (Oct 2, 2025)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1179", null ],
          [ "IT-08: Enhanced Error Reporting (5-7 hours) ✅ COMPLETE", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1180", null ],
          [ "IT-09: CI/CD Integration ✅ CLI Tooling Shipped", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1181", null ],
          [ "IT-10: Collaborative Editing & Multiplayer Sessions ⏸️ DEPRIORITIZED", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1183", null ]
        ] ],
        [ "Priority 2: LLM Integration (Ollama + Gemini + Claude) 🤖 NEW PRIORITY", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1185", [
          [ "Phase 1: Ollama Local Integration (4-6 hours) 🎯 START HERE", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1186", null ],
          [ "Phase 2: Gemini Fixes (2-3 hours)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1187", null ],
          [ "Phase 3: Claude Integration (2-3 hours)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1188", null ],
          [ "Phase 4: Enhanced Prompt Engineering (3-4 hours)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1189", null ]
        ] ],
        [ "Priority 3: Windows Cross-Platform Testing 🪟", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1191", null ],
        [ "Priority 2: Windows Cross-Platform Testing 🪟", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1193", null ]
      ] ],
      [ "</blockquote>", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1194", null ],
      [ "2. Workstreams Overview", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1195", [
        [ "Completed Work Summary", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1196", null ]
      ] ],
      [ "3. Task Backlog", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1198", null ],
      [ "3. Immediate Next Steps (Week of Oct 1-7, 2025)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1199", [
        [ "Priority 0: Testing & Validation (Active)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1200", null ],
        [ "Priority 1: ImGuiTestHarness Foundation (IT-01) ✅ COMPLETE", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1201", [
          [ "Phase 1: gRPC Infrastructure ✅ COMPLETE", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1202", null ],
          [ "Phase 2: ImGuiTestEngine Integration ✅ COMPLETE", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1203", null ],
          [ "Phase 3: Full ImGuiTestEngine Integration ✅ COMPLETE (Oct 2, 2025)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1204", null ],
          [ "Phase 4: CLI Integration & Windows Testing (4-5 hours)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1205", null ]
        ] ],
        [ "IT-01 Quick Reference", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1206", null ],
        [ "Priority 2: Policy Evaluation Framework (AW-04, 4-6 hours)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1207", null ],
        [ "Priority 3: Documentation & Consolidation (2-3 hours)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1208", null ],
        [ "Later: Advanced Features", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1209", null ]
      ] ],
      [ "4. Current Issues & Blockers", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1210", [
        [ "Active Issues", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1211", null ],
        [ "Known Limitations (Non-Blocking)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1212", null ]
      ] ],
      [ "5. Architecture Overview", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1213", [
        [ "5.1. Proposal Lifecycle Flow", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1214", null ],
        [ "5.2. Component Interaction Diagram", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1215", null ],
        [ "5.3. Data Flow: Agent Run to ROM Merge", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1216", null ]
      ] ],
      [ "5. Open Questions", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1217", null ],
      [ "4. Work History & Key Decisions", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1218", [
        [ "Resource Catalogue Workstream (RC) - ✅ COMPLETE", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1219", null ],
        [ "Acceptance Workflow (AW-01, AW-02, AW-03) - ✅ COMPLETE", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1220", null ],
        [ "ImGuiTestHarness (IT-01, IT-02) - ✅ CORE COMPLETE", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1221", null ],
        [ "Files Modified/Created", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1222", null ]
      ] ],
      [ "5. Open Questions", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1223", null ],
      [ "Z3ED_AI Flag Migration Guide", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1224", [
        [ "Summary", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1225", null ],
        [ "Problem Statement", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1226", [
          [ "Before (Issues):", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1227", null ],
          [ "Root Cause of Crash:", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1228", null ]
        ] ],
        [ "Solution", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1229", [
          [ "1. Created Z3ED_AI Master Flag", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1230", null ],
          [ "2. Fixed PromptBuilder Crash", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1231", null ],
          [ "3. Updated z3ed Build Configuration", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1232", null ]
        ] ],
        [ "Migration Instructions", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1233", [
          [ "For Users", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1234", null ],
          [ "For Developers", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1235", null ]
        ] ],
        [ "Testing Results", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1236", [
          [ "Build Configurations Tested ✅", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1237", null ],
          [ "Crash Scenarios Fixed ✅", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1238", null ]
        ] ],
        [ "Impact on Build Modularization", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1239", [
          [ "Before:", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1240", null ],
          [ "After:", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1241", null ],
          [ "Future Modular Build Integration", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1242", null ]
        ] ],
        [ "Documentation Updates", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1243", null ],
        [ "Backward Compatibility", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1244", [
          [ "Old Flags Still Work ✅", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1245", null ],
          [ "No Breaking Changes", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1246", null ]
        ] ],
        [ "Next Steps", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1247", [
          [ "Short Term (Complete)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1248", null ],
          [ "Medium Term (Recommended)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1249", null ],
          [ "Long Term (Integration with Modular Build)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1250", null ]
        ] ],
        [ "References", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1251", null ],
        [ "Summary", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1252", null ],
        [ "6. References", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1253", null ]
      ] ],
      [ "Z3ED GUI Integration & Enhanced Gemini Support", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1255", [
        [ "Overview", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1256", null ],
        [ "New Features", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1257", [
          [ "1. GUI Agent Chat Widget", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1258", null ],
          [ "2. Enhanced Gemini Function Calling", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1259", null ],
          [ "3. ASCII Logo Branding", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1260", null ]
        ] ],
        [ "Build Requirements", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1261", [
          [ "GUI Chat Widget", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1262", null ],
          [ "Enhanced Gemini Support", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1263", null ]
        ] ],
        [ "Testing", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1264", [
          [ "Test GUI Chat Widget", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1265", null ],
          [ "Test Enhanced Gemini Function Calling", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1266", null ],
          [ "Test ASCII Logo", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1267", null ]
        ] ],
        [ "Implementation Details", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1268", [
          [ "AgentChatWidget Architecture", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1269", null ],
          [ "Gemini Function Calling Flow", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1270", null ]
        ] ],
        [ "Configuration", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1271", [
          [ "GUI Widget Settings", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1272", null ],
          [ "Gemini AI Settings", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1273", null ],
          [ "Function Calling Control", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1274", null ]
        ] ],
        [ "Troubleshooting", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1275", [
          [ "GUI Chat Widget Issues", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1276", null ],
          [ "Gemini Function Calling Issues", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1277", null ],
          [ "ASCII Logo Issues", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1278", null ]
        ] ],
        [ "Next Steps", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1279", null ],
        [ "Related Documentation", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1280", null ],
        [ "Examples", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1281", [
          [ "Example 1: Using GUI Chat for ROM Exploration", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1282", null ],
          [ "Example 2: Programmatic Function Calling", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1283", null ],
          [ "Example 3: Custom Tool Integration", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1284", null ]
        ] ],
        [ "Success Criteria", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1285", null ],
        [ "Performance Notes", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1286", null ],
        [ "Security Considerations", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1287", null ]
      ] ],
      [ "z3ed Implementation Status", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1289", [
        [ "Summary", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1290", null ],
        [ "Completed Infrastructure ✅", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1291", [
          [ "Conversational Agent Service", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1292", null ],
          [ "Chat Interfaces (3 Modes)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1293", null ],
          [ "Tool System", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1294", null ],
          [ "AI Backends", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1295", null ],
          [ "Build System", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1296", null ]
        ] ],
        [ "In Progress 🚧", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1297", [
          [ "Priority 1: Live LLM Testing (1-2h)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1298", null ],
          [ "Priority 2: Proposal Integration (6-8h)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1299", null ],
          [ "Priority 3: Tool Coverage (8-10h)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1300", null ]
        ] ],
        [ "Code Files Status", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1301", [
          [ "New Files Created ✅", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1302", null ],
          [ "Modified Files ✅", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1303", null ],
          [ "Existing Files (Already Working)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1304", null ],
          [ "Removed/Unused Files", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1305", null ]
        ] ],
        [ "Next Steps", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1306", [
          [ "Immediate (Today)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1307", null ],
          [ "Short Term (This Week)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1308", null ],
          [ "Medium Term (Next 2 Weeks)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1309", null ]
        ] ],
        [ "Testing Checklist", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1310", [
          [ "Manual Testing", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1311", null ],
          [ "LLM Testing", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1312", null ],
          [ "Integration Testing", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1313", null ]
        ] ],
        [ "Known Issues", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1314", null ],
        [ "Build Commands", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1315", null ],
        [ "Documentation Status", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1316", [
          [ "Updated ✅", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1317", null ],
          [ "Still Current", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1318", null ],
          [ "Could Be Condensed (Low Priority)", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1319", null ]
        ] ],
        [ "Success Metrics", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1320", [
          [ "Phase 1: Foundation ✅ COMPLETE", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1321", null ],
          [ "Phase 2: Integration 🚧 IN PROGRESS", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1322", null ],
          [ "Phase 3: Production 📋 PLANNED", "df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1323", null ]
        ] ]
      ] ]
    ] ],
    [ "z3ed CLI Technical Reference", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html", [
      [ "Table of Contents", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1326", null ],
      [ "Architecture Overview", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1328", [
        [ "System Components", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1329", null ],
        [ "Data Flow: Proposal Lifecycle", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1330", null ]
      ] ],
      [ "Command Reference", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1332", [
        [ "Agent Commands", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1333", [
          [ "<tt>agent run</tt> - Execute AI-driven ROM modifications", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1334", null ],
          [ "<tt>agent list</tt> - Show all proposals", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1335", null ],
          [ "<tt>agent diff</tt> - Show proposal changes", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1336", null ],
          [ "<tt>agent describe</tt> - Export machine-readable API specs", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1337", null ],
          [ "<tt>agent resource-list</tt> - Enumerate labeled resources for the AI", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1338", null ],
          [ "<tt>agent dungeon-list-sprites</tt> - Inspect sprites in a dungeon room", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1339", null ],
          [ "<tt>agent chat</tt> - Interactive terminal chat (TUI prototype)", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1340", null ],
          [ "<tt>agent test</tt> - Automated GUI testing (IT-02)", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1341", null ],
          [ "<tt>agent gui</tt> - GUI Introspection & Control (IT-05/IT-06)", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1342", [
            [ "<tt>agent gui discover</tt> - Enumerate available widgets", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1343", null ],
            [ "<tt>agent test status</tt> - Query test execution state", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1344", null ],
            [ "<tt>agent test results</tt> - Get detailed test results", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1345", null ],
            [ "<tt>agent test list</tt> - List all tests", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1346", null ]
          ] ],
          [ "<tt>agent test record</tt> - Record test sessions (IT-07)", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1347", [
            [ "<tt>agent test record start</tt> - Begin recording", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1348", null ],
            [ "<tt>agent test record stop</tt> - Finish recording", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1349", null ]
          ] ],
          [ "<tt>agent test replay</tt> - Execute recorded tests", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1350", null ],
          [ "<tt>agent test suite</tt> - Manage test suites (IT-09)", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1351", null ]
        ] ],
        [ "ROM Commands", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1352", [
          [ "<tt>rom info</tt> - Display ROM metadata", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1353", null ],
          [ "<tt>rom validate</tt> - Verify ROM integrity", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1354", null ],
          [ "<tt>rom diff</tt> - Compare two ROMs", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1355", null ],
          [ "<tt>rom generate-golden</tt> - Create reference checksums", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1356", null ]
        ] ],
        [ "Palette Commands", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1357", [
          [ "<tt>palette export</tt> - Export palette to file", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1358", null ],
          [ "<tt>palette import</tt> - Import palette from file", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1359", null ],
          [ "<tt>palette list</tt> - Show available palettes", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1360", null ]
        ] ],
        [ "Overworld Commands", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1361", [
          [ "<tt>overworld get-tile</tt> - Get tile at coordinates", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1362", null ],
          [ "<tt>overworld find-tile</tt> - Locate tile instances across maps", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1363", null ],
          [ "<tt>overworld set-tile</tt> - Set tile at coordinates", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1364", null ]
        ] ],
        [ "Dungeon Commands", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1365", [
          [ "<tt>dungeon list-rooms</tt> - List all dungeon rooms", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1366", null ],
          [ "<tt>dungeon add-object</tt> - Add object to room", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1367", null ]
        ] ]
      ] ],
      [ "Implementation Guide", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1369", [
        [ "Building with gRPC Support", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1370", [
          [ "macOS (Recommended)", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1371", null ],
          [ "Windows (Experimental)", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1372", null ]
        ] ],
        [ "Starting Test Harness", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1373", [
          [ "Basic Usage", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1374", null ],
          [ "Configuration Options", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1375", null ]
        ] ],
        [ "Testing RPCs with grpcurl", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1376", null ]
      ] ],
      [ "Testing & Validation", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1378", [
        [ "Automated E2E Test Script", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1379", null ],
        [ "Manual Testing Workflow", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1380", [
          [ "1. Create Proposal", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1381", null ],
          [ "2. List Proposals", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1382", null ],
          [ "3. View Diff", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1383", null ],
          [ "4. Review in GUI", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1384", null ]
        ] ],
        [ "Performance Benchmarks", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1385", null ]
      ] ],
      [ "Development Workflows", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1387", [
        [ "Adding New Agent Commands", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1388", null ],
        [ "Adding New Test Harness RPCs", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1389", null ],
        [ "Adding Test Workflow Patterns", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1390", null ]
      ] ],
      [ "Troubleshooting", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1392", [
        [ "Common Issues", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1393", [
          [ "Port Already in Use", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1394", null ],
          [ "Connection Refused", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1395", null ],
          [ "Widget Not Found", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1396", [
            [ "Widget Not Found or Stale State", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1397", null ]
          ] ],
          [ "Crashes in <tt>Wait</tt> or <tt>Assert</tt> RPCs", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1398", null ]
        ] ],
        [ "Build Errors - Boolean Flag", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1399", [
          [ "Build Errors - Incomplete Type", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1400", null ]
        ] ],
        [ "Debug Mode", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1401", null ],
        [ "Test Harness Diagnostics", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1402", null ]
      ] ],
      [ "API Reference", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1404", [
        [ "RPC Service Definition", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1405", null ],
        [ "Request/Response Schemas", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1406", [
          [ "Ping", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1407", null ],
          [ "Click", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1408", null ],
          [ "Type", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1409", null ],
          [ "Wait", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1410", null ],
          [ "Assert", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1411", null ],
          [ "Screenshot", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1412", null ]
        ] ],
        [ "Resource Catalog Schema", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1413", null ]
      ] ],
      [ "Platform Notes", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1415", [
        [ "macOS (ARM64) - Production Ready ✅", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1416", null ],
        [ "macOS (Intel) - Should Work ⚠️", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1417", null ],
        [ "Linux - Should Work ⚠️", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1418", null ],
        [ "Windows - Experimental 🔬", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1419", null ]
      ] ],
      [ "Appendix", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1421", [
        [ "File Structure", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1422", null ],
        [ "Related Documentation", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1423", null ],
        [ "Version History", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1424", null ],
        [ "Contributors", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1425", null ],
        [ "License", "db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1426", null ]
      ] ]
    ] ],
    [ "z3ed: AI-Powered CLI for YAZE", "d3/d63/md_docs_2z3ed_2README.html", [
      [ "Overview", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1429", null ],
      [ "Quick Start", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1430", [
        [ "Build", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1431", null ],
        [ "AI Setup", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1432", null ],
        [ "Example Commands", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1433", null ]
      ] ],
      [ "Chat Modes", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1434", [
        [ "1. FTXUI Chat (<tt>agent chat</tt>)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1435", null ],
        [ "2. Simple Chat (<tt>agent simple-chat</tt>)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1436", null ],
        [ "3. GUI Chat Widget (In Progress)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1437", null ]
      ] ],
      [ "Available Tools", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1438", null ],
      [ "Documentation", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1439", null ],
      [ "Recent Updates (Oct 3, 2025)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1440", [
        [ "✅ Implemented", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1441", null ],
        [ "🎯 Next Steps", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1442", null ]
      ] ],
      [ "Troubleshooting", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1443", [
        [ "\"AI features not available\"", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1444", null ],
        [ "\"OpenSSL not found\"", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1445", null ],
        [ "Chat mode freezes", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1446", null ],
        [ "Tool not being called", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1447", null ]
      ] ],
      [ "Example Workflows", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1448", [
        [ "Explore ROM", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1449", null ],
        [ "Make Changes", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1450", null ]
      ] ],
      [ "Overview", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1451", null ],
      [ "Quick Start", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1452", [
        [ "Build Options", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1453", null ],
        [ "AI Agent Commands", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1454", null ],
        [ "GUI Testing Commands", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1455", null ]
      ] ],
      [ "AI Service Setup", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1456", [
        [ "Ollama (Local LLM - Recommended for Development)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1457", null ],
        [ "Gemini (Google Cloud API)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1458", null ],
        [ "Example Prompts", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1459", null ]
      ] ],
      [ "Core Documentation", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1460", [
        [ "Essential Reads", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1461", null ]
      ] ],
      [ "Current Status (October 3, 2025)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1462", [
        [ "✅ Production Ready", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1463", null ],
        [ "� In Progress (Priority Order)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1464", null ],
        [ "📋 Next Steps", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1465", null ]
      ] ],
      [ "AI Editing Focus Areas", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1466", [
        [ "Overworld Tile16 Editing ⭐ PRIMARY FOCUS", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1467", null ],
        [ "Dungeon Editing", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1468", null ],
        [ "Palette Editing", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1469", null ],
        [ "Additional Capabilities", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1470", null ]
      ] ],
      [ "Example Workflows", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1471", [
        [ "Basic Tile16 Edit", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1472", null ],
        [ "Complex Multi-Step Edit", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1473", null ],
        [ "Locate Existing Tiles", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1474", null ],
        [ "Label-Aware Dungeon Edit", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1475", null ]
      ] ],
      [ "Dependencies Guard", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1476", null ],
      [ "Recent Changes (Oct 3, 2025)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1477", [
        [ "Z3ED_AI Build Flag (Major Improvement)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1478", null ],
        [ "Build System", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1479", null ],
        [ "Documentation", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1480", null ]
      ] ],
      [ "Troubleshooting", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1481", [
        [ "\"OpenSSL not found\" warning", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1482", null ],
        [ "\"Build with -DZ3ED_AI=ON\" warning", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1483", null ],
        [ "\"gRPC not available\" error", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1484", null ],
        [ "AI generates invalid commands", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1485", null ],
        [ "Testing the conversational agent", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1486", null ],
        [ "Verifying ImGui test harness", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1487", [
          [ "Gemini-Specific Issues", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1488", null ]
        ] ]
      ] ]
    ] ],
    [ "yaze - Yet Another Zelda3 Editor", "d0/d30/md_README.html", [
      [ "Version 0.3.2 - Release", "d0/d30/md_README.html#autotoc_md1490", [
        [ "🛠️ Technical Improvements", "d0/d30/md_README.html#autotoc_md1494", null ]
      ] ],
      [ "Quick Start", "d0/d30/md_README.html#autotoc_md1495", [
        [ "Build", "d0/d30/md_README.html#autotoc_md1496", null ],
        [ "Applications", "d0/d30/md_README.html#autotoc_md1497", null ]
      ] ],
      [ "Usage", "d0/d30/md_README.html#autotoc_md1498", [
        [ "GUI Editor", "d0/d30/md_README.html#autotoc_md1499", null ],
        [ "Command Line Tool", "d0/d30/md_README.html#autotoc_md1500", null ],
        [ "C++ API", "d0/d30/md_README.html#autotoc_md1501", null ]
      ] ],
      [ "Documentation", "d0/d30/md_README.html#autotoc_md1502", null ],
      [ "Supported Platforms", "d0/d30/md_README.html#autotoc_md1503", null ],
      [ "ROM Compatibility", "d0/d30/md_README.html#autotoc_md1504", null ],
      [ "Contributing", "d0/d30/md_README.html#autotoc_md1505", null ],
      [ "License", "d0/d30/md_README.html#autotoc_md1506", null ],
      [ "🙏 Acknowledgments", "d0/d30/md_README.html#autotoc_md1507", null ],
      [ "📸 Screenshots", "d0/d30/md_README.html#autotoc_md1508", null ]
    ] ],
    [ "yaze Build Scripts", "de/d82/md_scripts_2README.html", [
      [ "Windows Scripts", "de/d82/md_scripts_2README.html#autotoc_md1511", [
        [ "vcpkg Setup (Optional)", "de/d82/md_scripts_2README.html#autotoc_md1512", null ]
      ] ],
      [ "Windows Build Workflow", "de/d82/md_scripts_2README.html#autotoc_md1513", [
        [ "Recommended: Visual Studio CMake Mode", "de/d82/md_scripts_2README.html#autotoc_md1514", null ],
        [ "Command Line Build", "de/d82/md_scripts_2README.html#autotoc_md1515", null ],
        [ "Compiler Notes", "de/d82/md_scripts_2README.html#autotoc_md1516", null ]
      ] ],
      [ "Quick Start (Windows)", "de/d82/md_scripts_2README.html#autotoc_md1517", [
        [ "Option 1: Visual Studio (Recommended)", "de/d82/md_scripts_2README.html#autotoc_md1518", null ],
        [ "Option 2: Command Line", "de/d82/md_scripts_2README.html#autotoc_md1519", null ],
        [ "Option 3: With vcpkg (Optional)", "de/d82/md_scripts_2README.html#autotoc_md1520", null ]
      ] ],
      [ "Troubleshooting", "de/d82/md_scripts_2README.html#autotoc_md1521", [
        [ "Common Issues", "de/d82/md_scripts_2README.html#autotoc_md1522", null ],
        [ "Getting Help", "de/d82/md_scripts_2README.html#autotoc_md1523", null ]
      ] ],
      [ "Other Scripts", "de/d82/md_scripts_2README.html#autotoc_md1524", null ]
    ] ],
    [ "YAZE Build Environment Verification Scripts", "dc/db4/md_scripts_2README__VERIFICATION.html", [
      [ "Quick Start", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1526", [
        [ "Verify Build Environment", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1527", null ]
      ] ],
      [ "Scripts Overview", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1528", [
        [ "<tt>verify-build-environment.ps1</tt> / <tt>.sh</tt>", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1529", [
          [ "Usage", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1530", null ],
          [ "Exit Codes", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1531", null ]
        ] ]
      ] ],
      [ "Common Workflows", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1532", [
        [ "First-Time Setup", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1533", null ],
        [ "After Pulling Changes", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1534", null ],
        [ "Troubleshooting Build Issues", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1535", null ],
        [ "Before Opening Pull Request", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1536", null ]
      ] ],
      [ "Automatic Fixes", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1537", [
        [ "Always Auto-Fixed (No Confirmation Required)", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1538", null ],
        [ "Fixed with <tt>-FixIssues</tt> / <tt>--fix</tt> Flag", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1539", null ],
        [ "Fixed with <tt>-CleanCache</tt> / <tt>--clean</tt> Flag", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1540", null ],
        [ "Optional Verbose Tests", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1541", null ]
      ] ],
      [ "Integration with Visual Studio", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1542", null ],
      [ "What Gets Checked", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1543", [
        [ "CMake (Required)", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1544", null ],
        [ "Git (Required)", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1545", null ],
        [ "Compilers (Required)", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1546", null ],
        [ "Platform Dependencies", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1547", null ],
        [ "CMake Cache", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1548", null ],
        [ "Dependencies", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1549", null ]
      ] ],
      [ "Automatic Fixes", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1550", null ],
      [ "CI/CD Integration", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1551", null ],
      [ "Troubleshooting", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1552", [
        [ "Script Reports \"CMake Not Found\"", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1553", null ],
        [ "\"Git Submodules Missing\"", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1554", null ],
        [ "\"CMake Cache Too Old\"", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1555", null ],
        [ "\"Visual Studio Not Found\" (Windows)", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1556", null ],
        [ "Script Fails on Network Issues (gRPC)", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1557", null ]
      ] ],
      [ "See Also", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1558", null ]
    ] ],
    [ "yaze Test Suite", "d0/d46/md_test_2README.html", [
      [ "Directory Structure", "d0/d46/md_test_2README.html#autotoc_md1560", null ],
      [ "Test Categories", "d0/d46/md_test_2README.html#autotoc_md1561", [
        [ "Unit Tests (<tt>unit/</tt>)", "d0/d46/md_test_2README.html#autotoc_md1562", null ],
        [ "Integration Tests (<tt>integration/</tt>)", "d0/d46/md_test_2README.html#autotoc_md1563", null ],
        [ "End-to-End Tests (<tt>e2e/</tt>)", "d0/d46/md_test_2README.html#autotoc_md1564", null ]
      ] ],
      [ "Enhanced Test Runner", "d0/d46/md_test_2README.html#autotoc_md1565", [
        [ "Usage Examples", "d0/d46/md_test_2README.html#autotoc_md1566", null ],
        [ "Test Modes", "d0/d46/md_test_2README.html#autotoc_md1567", null ],
        [ "Options", "d0/d46/md_test_2README.html#autotoc_md1568", null ]
      ] ],
      [ "E2E ROM Testing", "d0/d46/md_test_2README.html#autotoc_md1569", [
        [ "Test Cases", "d0/d46/md_test_2README.html#autotoc_md1570", null ]
      ] ],
      [ "ZSCustomOverworld Upgrade Testing", "d0/d46/md_test_2README.html#autotoc_md1571", [
        [ "Supported Upgrades", "d0/d46/md_test_2README.html#autotoc_md1572", null ],
        [ "Test Cases", "d0/d46/md_test_2README.html#autotoc_md1573", null ],
        [ "Version-Specific Features", "d0/d46/md_test_2README.html#autotoc_md1574", [
          [ "Vanilla", "d0/d46/md_test_2README.html#autotoc_md1575", null ],
          [ "v2", "d0/d46/md_test_2README.html#autotoc_md1576", null ],
          [ "v3", "d0/d46/md_test_2README.html#autotoc_md1577", null ]
        ] ]
      ] ],
      [ "Environment Variables", "d0/d46/md_test_2README.html#autotoc_md1578", null ],
      [ "CI/CD Integration", "d0/d46/md_test_2README.html#autotoc_md1579", null ],
      [ "Deprecated Tests", "d0/d46/md_test_2README.html#autotoc_md1580", null ],
      [ "Best Practices", "d0/d46/md_test_2README.html#autotoc_md1581", null ],
      [ "AI Agent Testing", "d0/d46/md_test_2README.html#autotoc_md1582", null ]
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
"d0/d27/namespaceyaze_1_1gfx.html#a82a8956476ffc04750bcfc4120c8b8db",
"d0/d30/md_README.html#autotoc_md1500",
"d0/d7a/structyaze_1_1emu_1_1W12SEL.html",
"d0/dd9/editor__manager_8cc.html#a4662e7cb347696014d9405c738e568cf",
"d1/d1f/overworld__entrance_8h.html#a18e93386f312411eccce90503af5ae91",
"d1/d3e/namespaceyaze_1_1editor.html#a023f96d1797f02dbaad61f338fb0ccd0",
"d1/d4b/namespaceyaze_1_1gfx_1_1lc__lz2.html#a87bb9224f822a93b75406e4f0cd71c97",
"d1/d8a/room__entrance_8h.html#a1accbf4fe50512baf98b6a3d3e9c92e7",
"d1/dc4/structyaze_1_1emu_1_1BackgroundLayer.html#a86b69290e0396632ceec0a0f6e295822a7915383cc2c1fff7ccfb328167cde9bf",
"d2/d03/structyaze_1_1emu_1_1CounterIrqNmiRegisters.html#a3c49a3079228c1b6eacdfc4b15de0608",
"d2/d07/classyaze_1_1emu_1_1Spc700.html#ac95db952ce9a5c92225d4bcfc75c1bc6",
"d2/d46/classyaze_1_1editor_1_1PaletteEditor.html#a0ce0856ab44377de66bab970a2d20a2d",
"d2/d60/structyaze_1_1test_1_1TestResults.html#ad2633be017b0b229aff6039c48041353",
"d2/dc2/classyaze_1_1gui_1_1BppFormatUI.html#ab704dbfc967865bfc5104144955de43b",
"d2/def/classyaze_1_1test_1_1TestRecorder.html#a6b14e5cf081dd953c815a6419488daa7",
"d2/dfe/structyaze_1_1gui_1_1EnhancedTheme.html#ac2f613657046248fb65bcae76f133c54",
"d3/d19/classyaze_1_1gfx_1_1GraphicsOptimizer.html#aa854f2a5293bfed1173f2cf18afb8849",
"d3/d30/structyaze_1_1gui_1_1canvas_1_1CanvasPerformanceMetrics.html#a9493b12e4e09226c2dcb7aba2d5bf2a7",
"d3/d44/classyaze_1_1editor_1_1OverworldEditor.html#ab6eb80402cae2e3fd5bf824c0c415d15",
"d3/d63/md_docs_2z3ed_2README.html#autotoc_md1452",
"d3/d72/resource__catalog__test_8cc.html#a927288120abe03ece67d37e55557c482",
"d3/d92/structyaze_1_1emu_1_1WramAccessRegisters.html",
"d3/db6/structyaze_1_1test_1_1ResourceStats.html#a011e2afb19b3cc7211bf35e876cd0dba",
"d3/de4/classyaze_1_1gfx_1_1TileInfo.html#a8bdea21bc4f8f89decc82a55e25b5182",
"d3/ded/classyaze_1_1emu_1_1Ppu.html#ae43f85c3bac33708cb00e7e52604fdc5",
"d4/d0a/namespaceyaze_1_1test.html#a7426142a80faa5705a5a3498c4579ee5",
"d4/d45/classyaze_1_1editor_1_1AssemblyEditor.html#ab193c7227d5c1acc100a2df54964931d",
"d4/d7c/classyaze_1_1emu_1_1PpuInterface.html#ad72156d555df5ac5842cd83193dce464",
"d4/db9/structyaze_1_1cli_1_1TestResultDetails.html#acf561f9e261ad7a7af82fae1a25bdf1e",
"d4/de9/overworld__exit_8h.html#a6de18d8ea53ec6d36c607b2fc211c9e5",
"d5/d1f/namespaceyaze_1_1zelda3.html#a38b6b9e34f1c2e1b34b1a324829875f4",
"d5/d1f/namespaceyaze_1_1zelda3.html#a9333632aaab476fae6ef62883dff235e",
"d5/d31/classyaze_1_1gfx_1_1SnesColor.html#a996766ae028cc0f05ad7d6663d952195",
"d5/d67/classyaze_1_1cli_1_1PolicyEvaluator.html#af144c91f09003ff17f782ca33dfd3569",
"d5/dae/structyaze_1_1gui_1_1canvas_1_1CanvasRenderContext.html#a1a9d0ab43013cc22748c2d677e852e2e",
"d5/dc5/md_docs_2z3ed_2E6-z3ed-cli-design.html#autotoc_md1170",
"d6/d20/namespaceyaze_1_1emu.html#a01160051ea35134ba63fc7ba8ad2a1cb",
"d6/d2f/classyaze_1_1zelda3_1_1OverworldExit.html#a5b394da0b28d133a1c868d96c43c1eab",
"d6/d3c/classyaze_1_1zelda3_1_1Inventory.html#a7e50fac9004047e6db8508bea3734732",
"d6/d7e/md_docs_2CANVAS__REFACTORING__STATUS.html#autotoc_md497",
"d6/db2/structyaze_1_1gfx_1_1lc__lz2_1_1CompressionCommand.html",
"d6/dcb/classyaze_1_1gui_1_1canvas_1_1CanvasPerformanceIntegration.html#ae8991f470494057dcfa7c37d85dfd9d9",
"d7/d02/structyaze_1_1gui_1_1CanvasSelection.html#a0d9b071873690731abec276ce117d16c",
"d7/d61/classyaze_1_1zelda3_1_1DungeonEditorSystem.html#a132c0c2e89ac572ad687b00cccf6d671",
"d7/d61/classyaze_1_1zelda3_1_1DungeonEditorSystem.html#ac39aca2c991a12ba457342b7fd697f6a",
"d7/d83/classyaze_1_1gfx_1_1Tile32.html#a615d07f05d95aa4d1596b7f7d108384e",
"d7/da8/tui_2command__palette_8cc_source.html",
"d7/df6/classyaze_1_1zelda3_1_1Room.html#a081ef02646596e3b9c4a830cdaff6271",
"d7/df7/classyaze_1_1util_1_1IFlag.html",
"d8/d03/classyaze_1_1cli_1_1util_1_1LoadingIndicator.html#a71cb8d00ab58be4a4d5c8fbde97f49ef",
"d8/d44/classyaze_1_1gfx_1_1BppConversionScope.html",
"d8/d83/entity_8h.html",
"d8/da5/structyaze_1_1gui_1_1canvas_1_1CanvasUsageStats.html#aa7cf16cb4859bafd0613029e656050f0",
"d8/dd6/classyaze_1_1zelda3_1_1music_1_1Tracker.html#a6e425cf578c8769ae95e3b8e85f808a1",
"d9/d21/structyaze_1_1emu_1_1DmaRegisters.html#afc38aeb6a727eb4c58f3a3353eb1169d",
"d9/d70/classyaze_1_1zelda3_1_1RoomLayout.html#a91f9a6759ac57cd5df520733bb25524e",
"d9/dc0/room_8h.html#a377a454109e3d95614691ba3ad4d77af",
"d9/dc5/classyaze_1_1zelda3_1_1Overworld.html#a90cc1152067e0937c7e48d69e88a563f",
"d9/dcc/classyaze_1_1zelda3_1_1OverworldMap.html#aa08d56743cec6094b3a9109445e95125",
"d9/dfe/classyaze_1_1gui_1_1WidgetIdRegistry.html#ae3f4a7bad5b8027d85be447f2f65603b",
"da/d2c/classyaze_1_1gui_1_1Canvas.html#a35dd1b42d0cbd01e3a8c52856dfcefa0",
"da/d2c/classyaze_1_1gui_1_1Canvas.html#aecc92f45938fcabafd8f9ef09ab8de93",
"da/d46/dungeon__editor_8cc.html#af331a754aa3b338d655987058c1cf4cb",
"da/d74/structyaze__extension.html#a93cdd908fb3c54b68fa54307fcf3a090",
"da/dbd/classyaze_1_1gui_1_1ScopedCanvas.html#a2bd5991604ae43f20295d44ce1dbfa78",
"da/df6/structyaze_1_1util_1_1HexStringParams.html#aacc8ccc62374df2c51bded29cc20589d",
"db/d33/namespaceyaze_1_1core.html#ad2f59a19103453af1c33ddd072d81f07",
"db/d82/classyaze_1_1editor_1_1Tile16Editor.html#a1b87393c358ea74aa125a267368f6060",
"db/d9c/message__data_8h.html#a023f96d1797f02dbaad61f338fb0ccd0",
"db/dbf/md_docs_2z3ed_2E6-z3ed-reference.html#autotoc_md1405",
"db/de0/structyaze_1_1cli_1_1FewShotExample.html#a6cd1e3b62fae189a873f1da8cb11fff4",
"dc/d0c/structyaze_1_1cli_1_1GeminiConfig.html",
"dc/d31/classyaze_1_1editor_1_1GraphicsEditor.html#a768a1858615a0c413679f9c47318bf2f",
"dc/d4f/classyaze_1_1editor_1_1DungeonObjectSelector.html#a46860a79dca8b87b925d5f738e1f2e4b",
"dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md694",
"dc/dd1/structyaze_1_1editor_1_1MessagePreview.html#abcd3265e6a359497a6564a70e956f6a7",
"dc/df4/classyaze_1_1emu_1_1Cpu.html#a56fdfed546707d17ea1abc309cd8478f",
"dc/df4/classyaze_1_1emu_1_1Cpu.html#adf4f971ef3ebc1e2d8f3be664458ca42",
"dd/d12/classyaze_1_1editor_1_1EditorManager.html#a86f28c22cce1957d7756e0e041435899",
"dd/d2f/structyaze_1_1editor_1_1zsprite_1_1OamTile.html#ac98b42966db687401970fb7ba291b3e2",
"dd/d5b/md_docs_2B1-contributing.html#autotoc_md209",
"dd/d7f/widget__auto__register_8h.html",
"dd/dcc/classyaze_1_1editor_1_1ProposalDrawer.html#acc9647b7e8ca1a138ca7462da585a647",
"dd/df4/canvas__utils_8cc.html#a195c54ea0600ea6e367ee37e8c9aadca",
"de/d0f/classyaze_1_1emu_1_1MemoryImpl.html#aaf695407eab1c0d51a7dc0388db3f5f5",
"de/d71/classyaze_1_1cli_1_1ResourceContextBuilder.html#af8c49246bdbbc33eb0e3212a0d32bb68",
"de/d8d/tile16__editor_8h_source.html",
"de/dbf/icons_8h.html#a08b7bed08e3c4eb895625f068b70ab56",
"de/dbf/icons_8h.html#a27a241e577e93900379a8da29cb3d885",
"de/dbf/icons_8h.html#a450787dd9a041600870103a560202117",
"de/dbf/icons_8h.html#a625a5d79865bb18fccf686ac6ac5d215",
"de/dbf/icons_8h.html#a818ebcc63b1d4e0d3e165184a646ac38",
"de/dbf/icons_8h.html#aa1bbceafd943e51a45409f502a4ca10a",
"de/dbf/icons_8h.html#abdd5567f5d6d96eb871e61c8d4c1a191",
"de/dbf/icons_8h.html#ad9d56f8b23200a8ba32e869caf50ebb5",
"de/dbf/icons_8h.html#af3ff132fc7ac5c0eb7206004e79ae194",
"de/de7/classyaze_1_1zelda3_1_1DungeonObjectEditor.html#a28df953ad8335f47c9ad97397ab11ae0",
"de/df2/tool__commands_8cc.html#aa9c3df97146a4533c1c78f5ac7c9e36e",
"df/d26/classyaze_1_1Transaction.html#a51bbb27b4e677fbdb7015b1736195008",
"df/d6e/structyaze_1_1cli_1_1ToolCall.html#a79ee29d179b59e39dd78696d353c15f7",
"df/da9/md_docs_2z3ed_2E6-z3ed-implementation-plan.html#autotoc_md1303",
"df/df5/classyaze_1_1test_1_1MockRom.html#ad9ad3a32a7ef36ef739181900fcda231",
"globals_p.html"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';