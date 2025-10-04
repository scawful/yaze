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
    [ "ZScream vs. yaze Overworld Implementation Analysis", "d9/d48/md_docs_2analysis_2overworld__implementation__analysis.html", [
      [ "Executive Summary", "d9/d48/md_docs_2analysis_2overworld__implementation__analysis.html#autotoc_md141", null ],
      [ "Key Findings", "d9/d48/md_docs_2analysis_2overworld__implementation__analysis.html#autotoc_md142", [
        [ "✅ <strong>Confirmed Correct Implementations</strong>", "d9/d48/md_docs_2analysis_2overworld__implementation__analysis.html#autotoc_md143", [
          [ "1. <strong>Tile32 & Tile16 Expansion Detection</strong>", "d9/d48/md_docs_2analysis_2overworld__implementation__analysis.html#autotoc_md144", null ],
          [ "2. <strong>Entrance & Hole Coordinate Calculation</strong>", "d9/d48/md_docs_2analysis_2overworld__implementation__analysis.html#autotoc_md145", null ],
          [ "3. <strong>Data Loading (Exits, Items, Sprites)</strong>", "d9/d48/md_docs_2analysis_2overworld__implementation__analysis.html#autotoc_md146", null ],
          [ "4. <strong>Map Decompression & Sizing</strong>", "d9/d48/md_docs_2analysis_2overworld__implementation__analysis.html#autotoc_md147", null ]
        ] ],
        [ "⚠️ <strong>Key Differences Found</strong>", "d9/d48/md_docs_2analysis_2overworld__implementation__analysis.html#autotoc_md148", null ],
        [ "🎯 <strong>Conclusion</strong>", "d9/d48/md_docs_2analysis_2overworld__implementation__analysis.html#autotoc_md149", null ]
      ] ]
    ] ],
    [ "yaze Performance Optimization Summary", "db/de6/md_docs_2analysis_2performance__optimization__summary.html", [
      [ "🎉 <strong>Massive Performance Improvements Achieved!</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md151", [
        [ "📊 <strong>Overall Performance Results</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md152", null ]
      ] ],
      [ "🚀 <strong>Optimizations Implemented</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md153", [
        [ "1. <strong>Performance Monitoring System with Feature Flag</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md154", null ],
        [ "2. <strong>DungeonEditor Parallel Loading (79% Speedup)</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md155", null ],
        [ "3. <strong>Incremental Overworld Map Loading</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md156", null ],
        [ "4. <strong>On-Demand Map Reloading</strong>", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md157", null ]
      ] ],
      [ "Appendix A: Dungeon Editor Parallel Optimization", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md159", null ],
      [ "Appendix B: Overworld Load Optimization", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md161", null ],
      [ "Appendix C: Renderer Optimization", "db/de6/md_docs_2analysis_2performance__optimization__summary.html#autotoc_md163", null ]
    ] ],
    [ "Contributing", "dd/d5b/md_docs_2B1-contributing.html", [
      [ "Development Setup", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md165", [
        [ "Prerequisites", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md166", null ],
        [ "Quick Start", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md167", null ]
      ] ],
      [ "Code Style", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md168", [
        [ "C++ Standards", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md169", null ],
        [ "File Organization", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md170", null ],
        [ "Error Handling", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md171", null ]
      ] ],
      [ "Testing Requirements", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md172", [
        [ "Test Categories", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md173", null ],
        [ "Writing Tests", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md174", null ],
        [ "Test Execution", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md175", null ]
      ] ],
      [ "Pull Request Process", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md176", [
        [ "Before Submitting", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md177", null ],
        [ "Pull Request Template", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md178", null ]
      ] ],
      [ "Development Workflow", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md179", [
        [ "Branch Strategy", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md180", null ],
        [ "Commit Messages", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md181", null ],
        [ "Types", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md182", null ]
      ] ],
      [ "Architecture Guidelines", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md183", [
        [ "Component Design", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md184", null ],
        [ "Memory Management", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md185", null ],
        [ "Performance", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md186", null ]
      ] ],
      [ "Documentation", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md187", [
        [ "Code Documentation", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md188", null ],
        [ "API Documentation", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md189", null ]
      ] ],
      [ "Community Guidelines", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md190", [
        [ "Communication", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md191", null ],
        [ "Getting Help", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md192", null ]
      ] ],
      [ "Release Process", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md193", [
        [ "Version Numbering", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md194", null ],
        [ "Release Checklist", "dd/d5b/md_docs_2B1-contributing.html#autotoc_md195", null ]
      ] ]
    ] ],
    [ "Platform Compatibility Improvements", "d1/d30/md_docs_2B2-platform-compatibility.html", [
      [ "Native File Dialog Support", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md197", null ],
      [ "Cross-Platform Build Reliability", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md198", null ],
      [ "Build Configuration Options", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md199", [
        [ "Full Build (Development)", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md200", null ],
        [ "Minimal Build", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md201", null ]
      ] ],
      [ "Implementation Details", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md202", null ],
      [ "Platform-Specific Adaptations", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md203", [
        [ "Windows", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md204", null ],
        [ "macOS", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md205", null ],
        [ "Linux", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md206", null ]
      ] ]
    ] ],
    [ "Build Presets Guide", "d7/d51/md_docs_2B3-build-presets.html", [
      [ "🍎 macOS ARM64 Presets (Recommended for Apple Silicon)", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md208", [
        [ "For Development Work:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md209", null ],
        [ "For Distribution:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md210", null ]
      ] ],
      [ "🔧 Why This Fixes Architecture Errors", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md211", null ],
      [ "📋 Available Presets", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md212", null ],
      [ "🚀 Quick Start", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md213", null ],
      [ "🛠️ IDE Integration", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md214", [
        [ "VS Code with CMake Tools:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md215", null ],
        [ "CLion:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md216", null ],
        [ "Xcode:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md217", null ]
      ] ],
      [ "🔍 Troubleshooting", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md218", null ],
      [ "📝 Notes", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md219", null ]
    ] ],
    [ "Release Workflows Documentation", "d3/d49/md_docs_2B4-release-workflows.html", [
      [ "Overview", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md221", null ],
      [ "1. Release-Simplified (<tt>release-simplified.yml</tt>)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md223", [
        [ "Purpose", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md224", null ],
        [ "Key Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md225", null ],
        [ "Use Cases", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md226", null ],
        [ "Configuration", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md227", null ],
        [ "Platforms Supported", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md228", null ]
      ] ],
      [ "2. Release (<tt>release.yml</tt>)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md230", [
        [ "Purpose", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md231", null ],
        [ "Key Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md232", null ],
        [ "Use Cases", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md233", null ],
        [ "Configuration", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md234", null ],
        [ "Advantages over Simplified", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md235", null ]
      ] ],
      [ "3. Release-Complex (<tt>release-complex.yml</tt>)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md237", [
        [ "Purpose", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md238", null ],
        [ "Key Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md239", null ],
        [ "Use Cases", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md240", null ],
        [ "Fallback Mechanisms", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md241", [
          [ "vcpkg Fallback", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md242", null ],
          [ "Chocolatey Integration", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md243", null ],
          [ "Build Configuration Fallback", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md244", null ]
        ] ],
        [ "Advanced Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md245", null ]
      ] ],
      [ "Workflow Comparison Matrix", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md247", null ],
      [ "When to Use Each Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md249", [
        [ "Use Simplified When:", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md250", null ],
        [ "Use Release When:", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md251", null ],
        [ "Use Complex When:", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md252", null ]
      ] ],
      [ "Workflow Selection Guide", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md254", [
        [ "For Development Team", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md255", null ],
        [ "For Release Manager", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md256", null ],
        [ "For CI/CD Pipeline", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md257", null ]
      ] ],
      [ "Configuration Examples", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md259", [
        [ "Triggering a Release", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md260", [
          [ "Manual Release (All Workflows)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md261", null ],
          [ "Automatic Release (Tag Push)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md262", null ]
        ] ],
        [ "Customizing Release Notes", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md263", null ]
      ] ],
      [ "Troubleshooting", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md265", [
        [ "Common Issues", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md266", [
          [ "vcpkg Failures (Windows)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md267", null ],
          [ "Dependency Conflicts", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md268", null ],
          [ "Build Failures", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md269", null ]
        ] ],
        [ "Debug Information", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md270", [
          [ "Simplified Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md271", null ],
          [ "Release Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md272", null ],
          [ "Complex Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md273", null ]
        ] ]
      ] ],
      [ "Best Practices", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md275", [
        [ "Workflow Selection", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md276", null ],
        [ "Release Process", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md277", null ],
        [ "Maintenance", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md278", null ]
      ] ],
      [ "Future Improvements", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md280", [
        [ "Planned Enhancements", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md281", null ],
        [ "Integration Opportunities", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md282", null ]
      ] ]
    ] ],
    [ "Stability, Testability & Release Workflow Improvements", "d9/d6b/md_docs_2B5-stability-improvements.html", [
      [ "Recent Improvements (v0.3.2)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md285", [
        [ "Windows Platform Stability", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md286", [
          [ "Stack Size Optimization", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md287", null ],
          [ "Development Utility Isolation", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md288", null ]
        ] ],
        [ "Graphics System Stability", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md289", [
          [ "Segmentation Fault Resolution", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md290", null ],
          [ "Comprehensive Bounds Checking", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md291", null ]
        ] ],
        [ "Build System Improvements", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md292", [
          [ "Modern Windows Workflow", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md293", null ],
          [ "Enhanced CI/CD Reliability", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md294", null ]
        ] ]
      ] ],
      [ "Recommended Optimizations", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md295", [
        [ "High Priority", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md296", [
          [ "1. Lazy Graphics Loading", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md297", null ],
          [ "2. Heap-Based Large Allocations", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md298", null ],
          [ "3. Streaming ROM Assets", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md299", null ]
        ] ],
        [ "Medium Priority", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md300", [
          [ "4. Enhanced Test Isolation", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md301", null ],
          [ "5. Dependency Caching Optimization", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md302", null ],
          [ "6. Memory Pool for Graphics", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md303", null ]
        ] ],
        [ "Low Priority", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md304", [
          [ "7. Build Time Optimization", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md305", null ],
          [ "8. Release Workflow Simplification", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md306", null ]
        ] ]
      ] ],
      [ "Testing Improvements", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md307", [
        [ "Current State", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md308", null ],
        [ "Recommendations", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md309", [
          [ "1. Visual Regression Testing", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md310", null ],
          [ "2. Performance Benchmarks", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md311", null ],
          [ "3. Fuzz Testing", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md312", null ]
        ] ]
      ] ],
      [ "Metrics & Monitoring", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md313", [
        [ "Current Measurements", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md314", null ],
        [ "Target Improvements", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md315", null ]
      ] ],
      [ "Action Items", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md316", [
        [ "Immediate (v0.3.2)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md317", null ],
        [ "Short Term (v0.3.3)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md318", null ],
        [ "Medium Term (v0.4.0)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md319", null ],
        [ "Long Term (v0.5.0+)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md320", null ]
      ] ],
      [ "Conclusion", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md321", null ]
    ] ],
    [ "Changelog", "d7/d6f/md_docs_2C1-changelog.html", [
      [ "0.3.2 (December 2025) - In Development", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md323", [
        [ "Tile16 Editor Improvements (In Progress)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md324", null ],
        [ "Graphics System Optimizations", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md325", null ],
        [ "Windows Platform Stability", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md326", null ],
        [ "Memory Safety & Stability", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md327", null ],
        [ "Testing & CI/CD Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md328", null ],
        [ "Future Optimizations (Planned)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md329", null ],
        [ "Technical Notes", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md330", null ]
      ] ],
      [ "0.3.1 (September 2025)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md331", [
        [ "Major Features", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md332", null ],
        [ "Tile16 Editor Enhancements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md333", null ],
        [ "ZSCustomOverworld v3 Implementation", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md334", null ],
        [ "Technical Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md335", null ],
        [ "User Interface", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md336", null ],
        [ "Bug Fixes", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md337", null ],
        [ "ZScream Compatibility Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md338", null ]
      ] ],
      [ "0.3.0 (September 2025)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md339", [
        [ "Major Features", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md340", null ],
        [ "User Interface & Theming", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md341", null ],
        [ "Enhancements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md342", null ],
        [ "Technical Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md343", null ],
        [ "Bug Fixes", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md344", null ]
      ] ],
      [ "0.2.2 (December 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md345", null ],
      [ "0.2.1 (August 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md346", null ],
      [ "0.2.0 (July 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md347", null ],
      [ "0.1.0 (May 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md348", null ],
      [ "0.0.9 (April 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md349", null ],
      [ "0.0.8 (February 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md350", null ],
      [ "0.0.7 (January 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md351", null ],
      [ "0.0.6 (November 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md352", null ],
      [ "0.0.5 (November 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md353", null ],
      [ "0.0.4 (November 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md354", null ],
      [ "0.0.3 (October 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md355", null ]
    ] ],
    [ "Canvas System - Comprehensive Guide", "d2/db2/md_docs_2CANVAS__GUIDE.html", [
      [ "Overview", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md357", null ],
      [ "Core Concepts", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md358", [
        [ "Canvas Structure", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md359", null ],
        [ "Coordinate Systems", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md360", null ]
      ] ],
      [ "Usage Patterns", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md361", [
        [ "Pattern 1: Basic Bitmap Display", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md362", null ],
        [ "Pattern 2: Modern Begin/End", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md363", null ],
        [ "Pattern 3: RAII ScopedCanvas", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md364", null ]
      ] ],
      [ "Feature: Tile Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md365", [
        [ "Single Tile Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md366", null ],
        [ "Tilemap Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md367", null ],
        [ "Color Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md368", null ]
      ] ],
      [ "Feature: Tile Selection", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md369", [
        [ "Single Tile Selection", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md370", null ],
        [ "Multi-Tile Rectangle Selection", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md371", null ],
        [ "Rectangle Drag & Paint", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md372", null ]
      ] ],
      [ "Feature: Custom Overlays", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md373", [
        [ "Manual Points Manipulation", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md374", null ]
      ] ],
      [ "Feature: Large Map Support", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md375", [
        [ "Map Types", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md376", null ],
        [ "Boundary Clamping", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md377", null ],
        [ "Custom Map Sizes", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md378", null ]
      ] ],
      [ "Feature: Context Menu", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md379", [
        [ "Adding Custom Items", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md380", null ],
        [ "Overworld Editor Example", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md381", null ]
      ] ],
      [ "Feature: Scratch Space (In Progress)", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md382", null ],
      [ "Common Workflows", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md383", [
        [ "Workflow 1: Overworld Tile Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md384", null ],
        [ "Workflow 2: Tile16 Blockset Selection", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md385", null ],
        [ "Workflow 3: Graphics Sheet Display", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md386", null ]
      ] ],
      [ "Configuration", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md387", [
        [ "Grid Settings", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md388", null ],
        [ "Scale Settings", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md389", null ],
        [ "Interaction Settings", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md390", null ],
        [ "Large Map Settings", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md391", null ]
      ] ],
      [ "Known Issues", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md392", [
        [ "⚠️ Rectangle Selection Wrapping", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md393", null ]
      ] ],
      [ "API Reference", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md394", [
        [ "Drawing Methods", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md395", null ],
        [ "State Accessors", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md396", null ],
        [ "Configuration", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md397", null ]
      ] ],
      [ "Implementation Notes", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md398", [
        [ "Points Management", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md399", null ],
        [ "Selection State", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md400", null ],
        [ "Overworld Rectangle Painting Flow", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md401", null ]
      ] ],
      [ "Best Practices", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md402", [
        [ "DO ✅", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md403", null ],
        [ "DON'T ❌", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md404", null ]
      ] ],
      [ "Troubleshooting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md405", [
        [ "Issue: Rectangle wraps at boundaries", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md406", null ],
        [ "Issue: Painting in wrong location", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md407", null ],
        [ "Issue: Array index out of bounds", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md408", null ],
        [ "Issue: Forgot to call End()", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md409", null ]
      ] ],
      [ "Future: Scratch Space", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md410", null ],
      [ "Summary", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md411", null ]
    ] ],
    [ "Roadmap", "d6/db4/md_docs_2D1-roadmap.html", [
      [ "0.4.X (Next Major Release)", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md413", [
        [ "Core Features", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md414", null ],
        [ "Technical Improvements", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md415", null ]
      ] ],
      [ "0.5.X", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md416", [
        [ "Advanced Features", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md417", null ]
      ] ],
      [ "0.6.X", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md418", [
        [ "Platform & Integration", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md419", null ]
      ] ],
      [ "0.7.X", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md420", [
        [ "Performance & Polish", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md421", null ]
      ] ],
      [ "0.8.X", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md422", [
        [ "Beta Preparation", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md423", null ]
      ] ],
      [ "1.0.0", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md424", [
        [ "Stable Release", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md425", null ]
      ] ],
      [ "Current Focus Areas", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md426", [
        [ "Immediate Priorities (v0.4.X)", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md427", null ],
        [ "Long-term Vision", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md428", null ]
      ] ]
    ] ],
    [ "Dungeon Editing Implementation Plan for Yaze", "df/d42/md_docs_2dungeon__editing__implementation__plan.html", [
      [ "Executive Summary", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md430", null ],
      [ "Table of Contents", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md431", null ],
      [ "Current State Analysis", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md433", [
        [ "What Works in Yaze", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md434", null ],
        [ "What Doesn't Work", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md435", null ],
        [ "Key Problems Identified", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md436", null ]
      ] ],
      [ "ZScream's Working Approach", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md438", [
        [ "Room Data Structure (ZScream)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md439", null ],
        [ "Object Data Structure (ZScream)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md440", null ],
        [ "Object Encoding Format (Critical!)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md441", [
          [ "Type 1 Objects (ID 0x000-0x0FF) - Standard Objects", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md442", null ],
          [ "Type 2 Objects (ID 0x100-0x1FF) - Special Objects", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md443", null ],
          [ "Type 3 Objects (ID 0xF00-0xFFF) - Extended Objects", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md444", null ]
        ] ],
        [ "Layer and Door Markers", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md445", null ],
        [ "Object Loading Process (ZScream)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md446", null ],
        [ "Object Saving Process (ZScream)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md447", null ],
        [ "Object Tile Loading (ZScream)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md448", null ],
        [ "Object Drawing (ZScream)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md449", null ]
      ] ],
      [ "ROM Data Structure Reference", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md451", [
        [ "Key ROM Addresses (from ALTTP Disassembly)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md452", null ],
        [ "Room Header Format (14 bytes)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md453", null ],
        [ "Object Data Format", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md454", null ],
        [ "Tile Data Format", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md455", null ]
      ] ],
      [ "Implementation Tasks", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md457", [
        [ "Phase 1: Core Object System (HIGH PRIORITY)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md458", [
          [ "Task 1.1: Object Encoding/Decoding ✅ CRITICAL", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md459", null ],
          [ "Task 1.2: Enhanced Object Parsing ✅ CRITICAL", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md460", null ],
          [ "Task 1.3: Object Tile Loading ✅ CRITICAL", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md461", null ],
          [ "Task 1.4: Object Drawing System ⚠️ HIGH PRIORITY", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md462", null ]
        ] ],
        [ "Phase 2: Editor UI Integration (MEDIUM PRIORITY)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md464", [
          [ "Task 2.1: Object Placement System", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md465", null ],
          [ "Task 2.2: Object Selection System", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md466", null ],
          [ "Task 2.3: Object Properties Editor", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md467", null ],
          [ "Task 2.4: Layer Management UI", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md468", null ]
        ] ],
        [ "Phase 3: Save System (HIGH PRIORITY)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md470", [
          [ "Task 3.1: Room Object Encoding ✅ CRITICAL", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md471", null ],
          [ "Task 3.2: ROM Writing System", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md472", null ],
          [ "Task 3.3: Save Validation", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md473", null ]
        ] ],
        [ "Phase 4: Advanced Features (LOW PRIORITY)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md475", [
          [ "Task 4.1: Undo/Redo System", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md476", null ],
          [ "Task 4.2: Copy/Paste Objects", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md477", null ],
          [ "Task 4.3: Object Library/Templates", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md478", null ],
          [ "Task 4.4: Room Import/Export", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md479", null ]
        ] ]
      ] ],
      [ "Technical Architecture", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md481", [
        [ "Class Hierarchy", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md482", null ],
        [ "Data Flow", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md483", null ],
        [ "Key Algorithms", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md484", [
          [ "Object Type Detection", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md485", null ],
          [ "Object Decoding", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md486", null ],
          [ "Object Encoding", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md487", null ]
        ] ]
      ] ],
      [ "Testing Strategy", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md489", [
        [ "Unit Tests", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md490", [
          [ "Encoding/Decoding Tests", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md491", null ],
          [ "Room Parsing Tests", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md492", null ]
        ] ],
        [ "Integration Tests", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md493", [
          [ "Round-Trip Tests", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md494", null ]
        ] ],
        [ "Manual Testing", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md495", [
          [ "Test Cases", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md496", null ]
        ] ]
      ] ],
      [ "References", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md498", [
        [ "Documentation", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md499", null ],
        [ "Key Files", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md500", [
          [ "ZScream (Reference)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md501", null ],
          [ "Yaze (Implementation)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md502", null ]
        ] ],
        [ "External Resources", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md503", null ]
      ] ],
      [ "Appendix: Object ID Reference", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md505", [
        [ "Type 1 Objects (0x00-0xFF)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md506", null ],
        [ "Type 2 Objects (0x100-0x1FF)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md507", null ],
        [ "Type 3 Objects (0xF00-0xFFF)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md508", null ]
      ] ],
      [ "Status Tracking", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md510", [
        [ "Current Progress", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md511", null ],
        [ "Next Steps", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md512", null ],
        [ "Timeline Estimate", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md513", null ]
      ] ]
    ] ],
    [ "Asm Style Guide", "d7/d9a/md_docs_2E1-asm-style-guide.html", [
      [ "Table of Contents", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md516", null ],
      [ "File Structure", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md517", null ],
      [ "Labels and Symbols", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md518", null ],
      [ "Comments", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md519", null ],
      [ "Instructions", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md520", null ],
      [ "Macros", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md521", null ],
      [ "Loops and Branching", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md522", null ],
      [ "Data Structures", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md523", null ],
      [ "Code Organization", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md524", null ],
      [ "Custom Code", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md525", null ]
    ] ],
    [ "Dungeon Editor Guide", "dd/d33/md_docs_2E2-dungeon-editor-guide.html", [
      [ "Overview", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md527", null ],
      [ "Architecture", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md528", [
        [ "Core Components", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md529", [
          [ "1. DungeonEditorSystem", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md530", null ],
          [ "2. DungeonObjectEditor", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md531", null ],
          [ "3. ObjectRenderer", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md532", null ],
          [ "4. DungeonEditor (UI Layer)", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md533", null ]
        ] ]
      ] ],
      [ "Coordinate System", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md534", [
        [ "Room Coordinates vs Canvas Coordinates", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md535", [
          [ "Conversion Functions", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md536", null ]
        ] ],
        [ "Coordinate System Features", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md537", null ]
      ] ],
      [ "Object Rendering System", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md538", [
        [ "Object Types", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md539", null ],
        [ "Rendering Pipeline", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md540", null ],
        [ "Performance Optimizations", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md541", null ]
      ] ],
      [ "User Interface", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md542", [
        [ "Integrated Editing Panels", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md543", [
          [ "Main Canvas", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md544", null ],
          [ "Compact Editing Panels", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md545", null ]
        ] ],
        [ "Object Preview System", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md546", null ]
      ] ],
      [ "Integration with ZScream", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md547", [
        [ "Room Loading", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md548", null ],
        [ "Object Parsing", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md549", null ],
        [ "Coordinate System", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md550", null ]
      ] ],
      [ "Testing and Validation", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md551", [
        [ "Integration Tests", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md552", null ],
        [ "Test Data", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md553", null ],
        [ "Performance Benchmarks", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md554", null ]
      ] ],
      [ "Usage Examples", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md555", [
        [ "Basic Object Editing", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md556", null ],
        [ "Coordinate Conversion", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md557", null ],
        [ "Object Preview", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md558", null ]
      ] ],
      [ "Configuration Options", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md559", [
        [ "Editor Configuration", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md560", null ],
        [ "Performance Configuration", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md561", null ]
      ] ],
      [ "Troubleshooting", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md562", [
        [ "Common Issues", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md563", null ],
        [ "Debug Information", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md564", null ]
      ] ],
      [ "Future Enhancements", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md565", [
        [ "Planned Features", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md566", null ]
      ] ],
      [ "Conclusion", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md567", null ]
    ] ],
    [ "Dungeon Editor Design Plan", "dc/d82/md_docs_2E3-dungeon-editor-design.html", [
      [ "Overview", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md569", null ],
      [ "Architecture Overview", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md570", [
        [ "Core Components", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md571", null ],
        [ "File Structure", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md572", null ]
      ] ],
      [ "Component Responsibilities", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md573", [
        [ "DungeonEditor (Main Orchestrator)", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md574", null ],
        [ "DungeonRoomSelector", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md575", null ],
        [ "DungeonCanvasViewer", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md576", null ],
        [ "DungeonObjectSelector", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md577", null ]
      ] ],
      [ "Data Flow", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md578", [
        [ "Initialization Flow", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md579", null ],
        [ "Runtime Data Flow", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md580", null ],
        [ "Key Data Structures", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md581", null ]
      ] ],
      [ "Integration Patterns", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md582", [
        [ "Component Communication", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md583", null ],
        [ "ROM Data Management", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md584", null ],
        [ "State Synchronization", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md585", null ]
      ] ],
      [ "UI Layout Architecture", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md586", [
        [ "3-Column Layout", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md587", null ],
        [ "Component Internal Layout", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md588", null ]
      ] ],
      [ "Coordinate System", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md589", [
        [ "Room Coordinates vs Canvas Coordinates", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md590", null ],
        [ "Bounds Checking", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md591", null ]
      ] ],
      [ "Error Handling & Validation", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md592", [
        [ "ROM Validation", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md593", null ],
        [ "Bounds Validation", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md594", null ]
      ] ],
      [ "Performance Considerations", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md595", [
        [ "Caching Strategy", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md596", null ],
        [ "Rendering Optimization", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md597", null ]
      ] ],
      [ "Testing Strategy", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md598", [
        [ "Integration Tests", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md599", null ],
        [ "Test Categories", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md600", null ]
      ] ],
      [ "Future Development Guidelines", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md601", [
        [ "Adding New Features", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md602", null ],
        [ "Component Extension Patterns", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md603", null ],
        [ "Data Flow Extension", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md604", null ],
        [ "UI Layout Extension", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md605", null ]
      ] ],
      [ "Common Pitfalls & Solutions", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md606", [
        [ "Memory Management", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md607", null ],
        [ "Coordinate System", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md608", null ],
        [ "State Synchronization", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md609", null ],
        [ "Performance Issues", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md610", null ]
      ] ],
      [ "Debugging Tools", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md611", [
        [ "Debug Popup", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md612", null ],
        [ "Logging", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md613", null ]
      ] ],
      [ "Build Integration", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md614", [
        [ "CMake Configuration", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md615", null ],
        [ "Dependencies", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md616", null ]
      ] ],
      [ "Conclusion", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md617", null ]
    ] ],
    [ "DungeonEditor Refactoring Plan", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html", [
      [ "Overview", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md619", null ],
      [ "Component Structure", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md620", [
        [ "✅ Created Components", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md621", [
          [ "1. DungeonToolset (<tt>dungeon_toolset.h/cc</tt>)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md622", null ],
          [ "2. DungeonObjectInteraction (<tt>dungeon_object_interaction.h/cc</tt>)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md623", null ],
          [ "3. DungeonRenderer (<tt>dungeon_renderer.h/cc</tt>)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md624", null ],
          [ "4. DungeonRoomLoader (<tt>dungeon_room_loader.h/cc</tt>)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md625", null ],
          [ "5. DungeonUsageTracker (<tt>dungeon_usage_tracker.h/cc</tt>)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md626", null ]
        ] ]
      ] ],
      [ "Refactored DungeonEditor Structure", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md627", [
        [ "Before Refactoring: 1444 lines", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md628", null ],
        [ "After Refactoring: ~400 lines", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md629", null ]
      ] ],
      [ "Method Migration Map", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md630", [
        [ "Core Editor Methods (Keep in main file)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md631", null ],
        [ "UI Methods (Keep for coordination)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md632", null ],
        [ "Methods Moved to Components", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md633", [
          [ "→ DungeonToolset", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md634", null ],
          [ "→ DungeonObjectInteraction", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md635", null ],
          [ "→ DungeonRenderer", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md636", null ],
          [ "→ DungeonRoomLoader", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md637", null ],
          [ "→ DungeonUsageTracker", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md638", null ]
        ] ]
      ] ],
      [ "Component Communication", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md639", [
        [ "Callback System", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md640", null ],
        [ "Data Sharing", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md641", null ]
      ] ],
      [ "Benefits of Refactoring", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md642", [
        [ "1. <strong>Reduced Complexity</strong>", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md643", null ],
        [ "2. <strong>Improved Testability</strong>", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md644", null ],
        [ "3. <strong>Better Maintainability</strong>", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md645", null ],
        [ "4. <strong>Enhanced Extensibility</strong>", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md646", null ],
        [ "5. <strong>Cleaner Dependencies</strong>", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md647", null ]
      ] ],
      [ "Implementation Status", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md648", [
        [ "✅ Completed", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md649", null ],
        [ "🔄 In Progress", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md650", null ],
        [ "⏳ Pending", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md651", null ]
      ] ],
      [ "Migration Strategy", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md652", [
        [ "Phase 1: Create Components ✅", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md653", null ],
        [ "Phase 2: Integrate Components 🔄", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md654", null ],
        [ "Phase 3: Move Methods", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md655", null ],
        [ "Phase 4: Cleanup", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md656", null ]
      ] ]
    ] ],
    [ "Dungeon Object System", "da/d11/md_docs_2E5-dungeon-object-system.html", [
      [ "Overview", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md658", null ],
      [ "Architecture", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md659", [
        [ "Core Components", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md660", [
          [ "1. DungeonEditor (<tt>src/app/editor/dungeon/dungeon_editor.h</tt>)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md661", null ],
          [ "2. DungeonObjectSelector (<tt>src/app/editor/dungeon/dungeon_object_selector.h</tt>)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md662", null ],
          [ "3. DungeonCanvasViewer (<tt>src/app/editor/dungeon/dungeon_canvas_viewer.h</tt>)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md663", null ],
          [ "4. Room Management System (<tt>src/app/zelda3/dungeon/room.h</tt>)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md664", null ]
        ] ]
      ] ],
      [ "Object Types and Hierarchies", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md665", [
        [ "Room Objects", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md666", [
          [ "Type 1 Objects (0x00-0xFF)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md667", null ],
          [ "Type 2 Objects (0x100-0x1FF)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md668", null ],
          [ "Type 3 Objects (0x200+)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md669", null ]
        ] ],
        [ "Object Properties", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md670", null ]
      ] ],
      [ "How Object Placement Works", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md671", [
        [ "Selection Process", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md672", null ],
        [ "Placement Process", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md673", null ],
        [ "Code Flow", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md674", null ]
      ] ],
      [ "Rendering Pipeline", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md675", [
        [ "Object Rendering", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md676", null ],
        [ "Performance Optimizations", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md677", null ]
      ] ],
      [ "User Interface Components", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md678", [
        [ "Three-Column Layout", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md679", [
          [ "Column 1: Room Control Panel (280px fixed)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md680", null ],
          [ "Column 2: Windowed Canvas (800px fixed)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md681", null ],
          [ "Column 3: Object Selector/Editor (stretch)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md682", null ]
        ] ],
        [ "Debug and Control Features", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md683", [
          [ "Room Properties Table", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md684", null ],
          [ "Object Statistics", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md685", null ]
        ] ]
      ] ],
      [ "Integration with ROM Data", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md686", [
        [ "Data Sources", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md687", [
          [ "Room Headers (<tt>0x1F8000</tt>)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md688", null ],
          [ "Object Data", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md689", null ],
          [ "Graphics Data", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md690", null ]
        ] ],
        [ "Assembly Integration", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md691", null ]
      ] ],
      [ "Comparison with ZScream", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md692", [
        [ "Architectural Differences", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md693", [
          [ "Component-Based Architecture", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md694", null ],
          [ "Real-time Rendering", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md695", null ],
          [ "UI Organization", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md696", null ],
          [ "Caching Strategy", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md697", null ]
        ] ],
        [ "Shared Concepts", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md698", null ]
      ] ],
      [ "Best Practices", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md699", [
        [ "Performance", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md700", null ],
        [ "Code Organization", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md701", null ],
        [ "User Experience", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md702", null ]
      ] ],
      [ "Future Enhancements", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md703", [
        [ "Planned Features", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md704", null ],
        [ "Technical Improvements", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md705", null ]
      ] ]
    ] ],
    [ "Tile16 Editor Palette System", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html", [
      [ "Executive Summary", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md707", null ],
      [ "Problem Analysis", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md708", [
        [ "Critical Issues Identified", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md709", null ]
      ] ],
      [ "Solution Architecture", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md710", [
        [ "Core Design Principles", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md711", null ],
        [ "256-Color Overworld Palette Structure", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md712", null ],
        [ "Sheet-to-Palette Mapping", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md713", null ],
        [ "Palette Button Mapping", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md714", null ]
      ] ],
      [ "Implementation Details", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md715", [
        [ "1. Fixed SetPaletteWithTransparent Method", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md716", null ],
        [ "2. Corrected Tile16 Editor Palette System", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md717", null ],
        [ "3. Palette Coordination Flow", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md718", null ]
      ] ],
      [ "UI/UX Refactoring", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md719", [
        [ "New Three-Column Layout", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md720", null ],
        [ "Canvas Context Menu Fixes", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md721", null ],
        [ "Dynamic Zoom Controls", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md722", null ]
      ] ],
      [ "Testing Protocol", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md723", [
        [ "Crash Prevention Testing", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md724", null ],
        [ "Color Alignment Testing", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md725", null ],
        [ "UI/UX Testing", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md726", null ]
      ] ],
      [ "Error Handling", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md727", [
        [ "Bounds Checking", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md728", null ],
        [ "Fallback Mechanisms", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md729", null ]
      ] ],
      [ "Debug Information Display", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md730", null ],
      [ "Known Issues and Ongoing Work", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md731", [
        [ "Completed Items ✅", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md732", null ],
        [ "Active Issues ⚠️", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md733", null ],
        [ "Current Status Summary", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md734", null ],
        [ "Future Enhancements", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md735", null ]
      ] ],
      [ "Maintenance Notes", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md736", null ],
      [ "Next Steps", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md738", [
        [ "Immediate Priorities", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md739", null ],
        [ "Investigation Areas", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md740", null ]
      ] ]
    ] ],
    [ "Overworld Loading Guide", "dc/d12/md_docs_2F1-overworld-loading.html", [
      [ "Table of Contents", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md743", null ],
      [ "Overview", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md744", null ],
      [ "ROM Types and Versions", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md745", [
        [ "Version Detection", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md746", null ],
        [ "Feature Support by Version", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md747", null ]
      ] ],
      [ "Overworld Map Structure", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md748", [
        [ "Core Properties", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md749", null ]
      ] ],
      [ "Overlays and Special Area Maps", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md750", [
        [ "Understanding Overlays", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md751", null ],
        [ "Special Area Maps (0x80-0x9F)", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md752", null ],
        [ "Overlay ID Mappings", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md753", null ],
        [ "Drawing Order", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md754", null ],
        [ "Vanilla Overlay Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md755", null ],
        [ "Special Area Graphics Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md756", null ]
      ] ],
      [ "Loading Process", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md757", [
        [ "1. Version Detection", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md758", null ],
        [ "2. Map Initialization", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md759", null ],
        [ "3. Property Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md760", [
          [ "Vanilla ROMs (asm_version == 0xFF)", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md761", null ],
          [ "ZSCustomOverworld v2/v3", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md762", null ]
        ] ],
        [ "4. Custom Data Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md763", null ]
      ] ],
      [ "ZScream Implementation", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md764", [
        [ "OverworldMap Constructor", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md765", null ],
        [ "Key Methods", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md766", null ]
      ] ],
      [ "Yaze Implementation", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md767", [
        [ "OverworldMap Constructor", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md768", null ],
        [ "Key Methods", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md769", null ],
        [ "Current Status", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md770", null ]
      ] ],
      [ "Key Differences", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md771", [
        [ "1. Language and Architecture", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md772", null ],
        [ "2. Data Structures", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md773", null ],
        [ "3. Error Handling", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md774", null ],
        [ "4. Graphics Processing", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md775", null ]
      ] ],
      [ "Common Issues and Solutions", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md776", [
        [ "1. Version Detection Issues", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md777", null ],
        [ "2. Palette Loading Errors", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md778", null ],
        [ "3. Graphics Not Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md779", null ],
        [ "4. Overlay Issues", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md780", null ],
        [ "5. Large Map Problems", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md781", null ],
        [ "6. Special Area Graphics Issues", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md782", null ]
      ] ],
      [ "Best Practices", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md783", [
        [ "1. Version-Specific Code", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md784", null ],
        [ "2. Error Handling", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md785", null ],
        [ "3. Memory Management", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md786", null ],
        [ "4. Thread Safety", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md787", null ]
      ] ],
      [ "Conclusion", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md788", null ]
    ] ],
    [ "YAZE Graphics System Optimization Recommendations", "de/def/md_docs_2gfx__optimization__recommendations.html", [
      [ "Overview", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md790", null ],
      [ "Current Architecture Analysis", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md791", [
        [ "Strengths", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md792", null ],
        [ "Performance Bottlenecks Identified", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md793", [
          [ "1. Bitmap Class Issues", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md794", null ],
          [ "2. Arena Resource Management", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md795", null ],
          [ "3. Tilemap Performance", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md796", null ]
        ] ]
      ] ],
      [ "Optimization Recommendations", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md797", [
        [ "1. Bitmap Class Optimizations", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md798", [
          [ "A. Palette Lookup Optimization", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md799", null ],
          [ "B. Dirty Region Tracking", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md800", null ]
        ] ],
        [ "2. Arena Resource Management Improvements", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md801", [
          [ "A. Resource Pooling", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md802", null ],
          [ "B. Batch Operations", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md803", null ]
        ] ],
        [ "3. Tilemap Performance Enhancements", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md804", [
          [ "A. Smart Tile Caching", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md805", null ],
          [ "B. Atlas-based Rendering", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md806", null ]
        ] ],
        [ "4. Editor-Specific Optimizations", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md807", [
          [ "A. Graphics Editor Improvements", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md808", null ],
          [ "B. Palette Editor Optimizations", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md809", null ]
        ] ],
        [ "5. Memory Management Improvements", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md810", [
          [ "A. Custom Allocator for Graphics Data", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md811", null ],
          [ "B. Smart Pointer Management", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md812", null ]
        ] ]
      ] ],
      [ "Implementation Priority", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md813", [
        [ "Phase 1 (High Impact, Low Risk)", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md814", null ],
        [ "Phase 2 (Medium Impact, Medium Risk)", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md815", null ],
        [ "Phase 3 (High Impact, High Risk)", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md816", null ]
      ] ],
      [ "Performance Metrics", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md817", [
        [ "Target Improvements", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md818", null ],
        [ "Measurement Tools", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md819", null ]
      ] ],
      [ "Conclusion", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md820", null ]
    ] ],
    [ "YAZE Graphics System Optimizations - Complete Implementation", "d6/df4/md_docs_2gfx__optimizations__complete.html", [
      [ "Overview", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md822", null ],
      [ "Implemented Optimizations", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md823", [
        [ "1. Palette Lookup Optimization ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md824", null ],
        [ "2. Dirty Region Tracking ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md825", null ],
        [ "3. Resource Pooling ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md826", null ],
        [ "4. LRU Tile Caching ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md827", null ],
        [ "5. Batch Operations ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md828", null ],
        [ "6. Memory Pool Allocator ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md829", null ],
        [ "7. Atlas-Based Rendering ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md830", [
          [ "Core Components", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md831", [
            [ "1. AtlasRenderer Class (<tt>src/app/gfx/atlas_renderer.h/cc</tt>)", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md832", null ],
            [ "2. RenderCommand Structure", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md833", null ],
            [ "3. Atlas Statistics Tracking", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md834", null ]
          ] ],
          [ "Integration Points", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md835", [
            [ "1. Tilemap Integration (<tt>src/app/gfx/tilemap.h/cc</tt>)", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md836", null ],
            [ "2. Performance Dashboard Integration", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md837", null ]
          ] ],
          [ "Technical Implementation", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md838", [
            [ "Atlas Packing Algorithm", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md839", null ],
            [ "Batch Rendering Process", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md840", null ]
          ] ]
        ] ],
        [ "8. Performance Profiling System ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md841", null ],
        [ "9. Performance Monitoring Dashboard ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md842", null ],
        [ "10. Optimization Validation Suite ✅ COMPLETED", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md843", null ]
      ] ],
      [ "Performance Metrics", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md844", [
        [ "Expected Improvements", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md845", null ],
        [ "Measurement Tools", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md846", null ]
      ] ],
      [ "Integration Points", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md847", [
        [ "Graphics Editor", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md848", null ],
        [ "Palette Editor", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md849", null ],
        [ "Screen Editor", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md850", null ]
      ] ],
      [ "Backward Compatibility", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md851", null ],
      [ "Usage Examples", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md852", [
        [ "Using Batch Operations", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md853", null ],
        [ "Using Memory Pool", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md854", null ],
        [ "Using Atlas Rendering", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md855", null ],
        [ "Using Performance Monitoring", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md856", null ]
      ] ],
      [ "Future Enhancements", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md857", [
        [ "Phase 2 Optimizations (Medium Priority)", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md858", null ],
        [ "Phase 3 Optimizations (High Priority)", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md859", null ]
      ] ],
      [ "Testing and Validation", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md860", [
        [ "Performance Testing", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md861", null ],
        [ "ROM Hacking Workflow Testing", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md862", null ]
      ] ],
      [ "Conclusion", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md863", null ],
      [ "Files Modified/Created", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md864", [
        [ "Core Graphics Classes", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md865", null ],
        [ "New Optimization Components", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md866", null ],
        [ "Testing and Validation", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md867", null ],
        [ "Build System", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md868", null ],
        [ "Documentation", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md869", null ]
      ] ]
    ] ],
    [ "yaze Documentation", "d3/d4c/md_docs_2index.html", [
      [ "Quick Start", "d3/d4c/md_docs_2index.html#autotoc_md871", null ],
      [ "Development", "d3/d4c/md_docs_2index.html#autotoc_md872", null ],
      [ "Technical Documentation", "d3/d4c/md_docs_2index.html#autotoc_md873", [
        [ "Assembly & Code", "d3/d4c/md_docs_2index.html#autotoc_md874", null ],
        [ "Editor Systems", "d3/d4c/md_docs_2index.html#autotoc_md875", null ],
        [ "Overworld System", "d3/d4c/md_docs_2index.html#autotoc_md876", null ]
      ] ],
      [ "Key Features", "d3/d4c/md_docs_2index.html#autotoc_md877", null ]
    ] ],
    [ "yaze Overworld Testing Guide", "de/d8a/md_docs_2overworld__testing__guide.html", [
      [ "Overview", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md880", null ],
      [ "Test Architecture", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md881", [
        [ "1. Golden Data System", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md882", null ],
        [ "2. Test Categories", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md883", [
          [ "Unit Tests (<tt>test/unit/zelda3/</tt>)", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md884", null ],
          [ "Integration Tests (<tt>test/e2e/</tt>)", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md885", null ],
          [ "Golden Data Tools", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md886", null ]
        ] ]
      ] ],
      [ "Quick Start", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md887", [
        [ "Prerequisites", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md888", null ],
        [ "Running Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md889", [
          [ "Basic Test Run", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md890", null ],
          [ "Selective Test Execution", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md891", null ]
        ] ]
      ] ],
      [ "Test Components", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md892", [
        [ "1. Golden Data Extractor", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md893", null ],
        [ "2. Integration Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md894", null ],
        [ "3. End-to-End Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md895", null ]
      ] ],
      [ "Test Validation Points", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md896", [
        [ "1. ZScream Compatibility", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md897", null ],
        [ "2. ROM State Validation", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md898", null ],
        [ "3. Performance and Stability", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md899", null ]
      ] ],
      [ "Environment Variables", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md900", [
        [ "Test Configuration", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md901", null ],
        [ "Build Configuration", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md902", null ]
      ] ],
      [ "Test Reports", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md903", [
        [ "Generated Reports", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md904", null ],
        [ "Report Location", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md905", null ]
      ] ],
      [ "Troubleshooting", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md906", [
        [ "Common Issues", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md907", [
          [ "1. ROM Not Found", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md908", null ],
          [ "2. Build Failures", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md909", null ],
          [ "3. Test Failures", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md910", null ]
        ] ],
        [ "Debug Mode", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md911", null ]
      ] ],
      [ "Advanced Usage", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md912", [
        [ "Custom Test Scenarios", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md913", [
          [ "1. Testing Custom ROMs", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md914", null ],
          [ "2. Regression Testing", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md915", null ],
          [ "3. Performance Testing", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md916", null ]
        ] ]
      ] ],
      [ "Integration with CI/CD", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md917", [
        [ "GitHub Actions Example", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md918", null ]
      ] ],
      [ "Contributing", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md919", [
        [ "Adding New Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md920", null ],
        [ "Test Guidelines", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md921", null ],
        [ "Example Test Structure", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md922", null ]
      ] ],
      [ "Conclusion", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md923", null ]
    ] ],
    [ "z3ed Developer Guide", "d3/d04/md_docs_2z3ed_2developer__guide.html", [
      [ "1. Overview", "d3/d04/md_docs_2z3ed_2developer__guide.html#autotoc_md925", [
        [ "Core Capabilities", "d3/d04/md_docs_2z3ed_2developer__guide.html#autotoc_md926", null ]
      ] ],
      [ "2. Architecture", "d3/d04/md_docs_2z3ed_2developer__guide.html#autotoc_md927", [
        [ "System Components Diagram", "d3/d04/md_docs_2z3ed_2developer__guide.html#autotoc_md928", null ],
        [ "Key Architectural Decisions", "d3/d04/md_docs_2z3ed_2developer__guide.html#autotoc_md929", null ]
      ] ],
      [ "3. Command Reference", "d3/d04/md_docs_2z3ed_2developer__guide.html#autotoc_md930", [
        [ "Agent Commands", "d3/d04/md_docs_2z3ed_2developer__guide.html#autotoc_md931", null ],
        [ "Resource Commands", "d3/d04/md_docs_2z3ed_2developer__guide.html#autotoc_md932", null ]
      ] ],
      [ "4. Agentic & Generative Workflow (MCP)", "d3/d04/md_docs_2z3ed_2developer__guide.html#autotoc_md933", null ],
      [ "5. Roadmap & Implementation Status", "d3/d04/md_docs_2z3ed_2developer__guide.html#autotoc_md934", [
        [ "✅ Completed", "d3/d04/md_docs_2z3ed_2developer__guide.html#autotoc_md935", null ],
        [ "🚧 Active & Next Steps", "d3/d04/md_docs_2z3ed_2developer__guide.html#autotoc_md936", null ]
      ] ],
      [ "6. Technical Implementation Details", "d3/d04/md_docs_2z3ed_2developer__guide.html#autotoc_md937", [
        [ "Build System", "d3/d04/md_docs_2z3ed_2developer__guide.html#autotoc_md938", null ],
        [ "AI Service Configuration", "d3/d04/md_docs_2z3ed_2developer__guide.html#autotoc_md939", null ],
        [ "Test Harness (gRPC)", "d3/d04/md_docs_2z3ed_2developer__guide.html#autotoc_md940", null ]
      ] ]
    ] ],
    [ "z3ed: AI-Powered CLI for YAZE", "d3/d63/md_docs_2z3ed_2README.html", [
      [ "Overview", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md942", null ],
      [ "Quick Start", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md943", [
        [ "Build", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md944", null ],
        [ "AI Setup", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md945", null ],
        [ "Example Commands", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md946", null ]
      ] ],
      [ "Chat Modes", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md947", [
        [ "1. FTXUI Chat (<tt>agent chat</tt>)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md948", null ],
        [ "2. Simple Chat (<tt>agent simple-chat</tt>)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md949", null ],
        [ "3. GUI Chat Widget (In Progress)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md950", null ]
      ] ],
      [ "Available Tools", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md951", null ],
      [ "Documentation", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md952", null ],
      [ "Recent Updates (Oct 3, 2025)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md953", [
        [ "✅ Implemented", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md954", null ],
        [ "🎯 Next Steps", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md955", null ]
      ] ],
      [ "Troubleshooting", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md956", [
        [ "\"AI features not available\"", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md957", null ],
        [ "\"OpenSSL not found\"", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md958", null ],
        [ "Chat mode freezes", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md959", null ],
        [ "Tool not being called", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md960", null ]
      ] ],
      [ "Example Workflows", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md961", [
        [ "Explore ROM", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md962", null ],
        [ "Make Changes", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md963", null ]
      ] ],
      [ "Overview", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md964", null ],
      [ "Quick Start", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md965", [
        [ "Build Options", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md966", null ],
        [ "AI Agent Commands", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md967", null ],
        [ "GUI Testing Commands", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md968", null ]
      ] ],
      [ "AI Service Setup", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md969", [
        [ "Ollama (Local LLM - Recommended for Development)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md970", null ],
        [ "Gemini (Google Cloud API)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md971", null ],
        [ "Example Prompts", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md972", null ]
      ] ],
      [ "Core Documentation", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md973", [
        [ "Essential Reads", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md974", null ]
      ] ],
      [ "Current Status (October 3, 2025)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md975", [
        [ "✅ Production Ready", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md976", null ],
        [ "� In Progress (Priority Order)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md977", null ],
        [ "📋 Next Steps", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md978", null ]
      ] ],
      [ "AI Editing Focus Areas", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md979", [
        [ "Overworld Tile16 Editing ⭐ PRIMARY FOCUS", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md980", null ],
        [ "Dungeon Editing", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md981", null ],
        [ "Palette Editing", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md982", null ],
        [ "Additional Capabilities", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md983", null ]
      ] ],
      [ "Example Workflows", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md984", [
        [ "Basic Tile16 Edit", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md985", null ],
        [ "Complex Multi-Step Edit", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md986", null ],
        [ "Locate Existing Tiles", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md987", null ],
        [ "Label-Aware Dungeon Edit", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md988", null ]
      ] ],
      [ "Dependencies Guard", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md989", null ],
      [ "Recent Changes (Oct 3, 2025)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md990", [
        [ "Z3ED_AI Build Flag (Major Improvement)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md991", null ],
        [ "Build System", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md992", null ],
        [ "Documentation", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md993", null ]
      ] ],
      [ "Troubleshooting", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md994", [
        [ "\"OpenSSL not found\" warning", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md995", null ],
        [ "\"Build with -DZ3ED_AI=ON\" warning", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md996", null ],
        [ "\"gRPC not available\" error", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md997", null ],
        [ "AI generates invalid commands", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md998", null ],
        [ "Testing the conversational agent", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md999", null ],
        [ "Verifying ImGui test harness", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1000", [
          [ "Gemini-Specific Issues", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1001", null ]
        ] ]
      ] ]
    ] ],
    [ "yaze - Yet Another Zelda3 Editor", "d0/d30/md_README.html", [
      [ "Version 0.3.2 - Release", "d0/d30/md_README.html#autotoc_md1003", [
        [ "🛠️ Technical Improvements", "d0/d30/md_README.html#autotoc_md1007", null ]
      ] ],
      [ "Quick Start", "d0/d30/md_README.html#autotoc_md1008", [
        [ "Build", "d0/d30/md_README.html#autotoc_md1009", null ],
        [ "Applications", "d0/d30/md_README.html#autotoc_md1010", null ]
      ] ],
      [ "Usage", "d0/d30/md_README.html#autotoc_md1011", [
        [ "GUI Editor", "d0/d30/md_README.html#autotoc_md1012", null ],
        [ "Command Line Tool", "d0/d30/md_README.html#autotoc_md1013", null ],
        [ "C++ API", "d0/d30/md_README.html#autotoc_md1014", null ]
      ] ],
      [ "Documentation", "d0/d30/md_README.html#autotoc_md1015", null ],
      [ "Supported Platforms", "d0/d30/md_README.html#autotoc_md1016", null ],
      [ "ROM Compatibility", "d0/d30/md_README.html#autotoc_md1017", null ],
      [ "Contributing", "d0/d30/md_README.html#autotoc_md1018", null ],
      [ "License", "d0/d30/md_README.html#autotoc_md1019", null ],
      [ "🙏 Acknowledgments", "d0/d30/md_README.html#autotoc_md1020", null ],
      [ "📸 Screenshots", "d0/d30/md_README.html#autotoc_md1021", null ]
    ] ],
    [ "yaze Build Scripts", "de/d82/md_scripts_2README.html", [
      [ "Windows Scripts", "de/d82/md_scripts_2README.html#autotoc_md1024", [
        [ "vcpkg Setup (Optional)", "de/d82/md_scripts_2README.html#autotoc_md1025", null ]
      ] ],
      [ "Windows Build Workflow", "de/d82/md_scripts_2README.html#autotoc_md1026", [
        [ "Recommended: Visual Studio CMake Mode", "de/d82/md_scripts_2README.html#autotoc_md1027", null ],
        [ "Command Line Build", "de/d82/md_scripts_2README.html#autotoc_md1028", null ],
        [ "Compiler Notes", "de/d82/md_scripts_2README.html#autotoc_md1029", null ]
      ] ],
      [ "Quick Start (Windows)", "de/d82/md_scripts_2README.html#autotoc_md1030", [
        [ "Option 1: Visual Studio (Recommended)", "de/d82/md_scripts_2README.html#autotoc_md1031", null ],
        [ "Option 2: Command Line", "de/d82/md_scripts_2README.html#autotoc_md1032", null ],
        [ "Option 3: With vcpkg (Optional)", "de/d82/md_scripts_2README.html#autotoc_md1033", null ]
      ] ],
      [ "Troubleshooting", "de/d82/md_scripts_2README.html#autotoc_md1034", [
        [ "Common Issues", "de/d82/md_scripts_2README.html#autotoc_md1035", null ],
        [ "Getting Help", "de/d82/md_scripts_2README.html#autotoc_md1036", null ]
      ] ],
      [ "Other Scripts", "de/d82/md_scripts_2README.html#autotoc_md1037", null ]
    ] ],
    [ "YAZE Build Environment Verification Scripts", "dc/db4/md_scripts_2README__VERIFICATION.html", [
      [ "Quick Start", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1039", [
        [ "Verify Build Environment", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1040", null ]
      ] ],
      [ "Scripts Overview", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1041", [
        [ "<tt>verify-build-environment.ps1</tt> / <tt>.sh</tt>", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1042", [
          [ "Usage", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1043", null ],
          [ "Exit Codes", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1044", null ]
        ] ]
      ] ],
      [ "Common Workflows", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1045", [
        [ "First-Time Setup", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1046", null ],
        [ "After Pulling Changes", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1047", null ],
        [ "Troubleshooting Build Issues", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1048", null ],
        [ "Before Opening Pull Request", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1049", null ]
      ] ],
      [ "Automatic Fixes", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1050", [
        [ "Always Auto-Fixed (No Confirmation Required)", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1051", null ],
        [ "Fixed with <tt>-FixIssues</tt> / <tt>--fix</tt> Flag", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1052", null ],
        [ "Fixed with <tt>-CleanCache</tt> / <tt>--clean</tt> Flag", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1053", null ],
        [ "Optional Verbose Tests", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1054", null ]
      ] ],
      [ "Integration with Visual Studio", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1055", null ],
      [ "What Gets Checked", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1056", [
        [ "CMake (Required)", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1057", null ],
        [ "Git (Required)", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1058", null ],
        [ "Compilers (Required)", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1059", null ],
        [ "Platform Dependencies", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1060", null ],
        [ "CMake Cache", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1061", null ],
        [ "Dependencies", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1062", null ]
      ] ],
      [ "Automatic Fixes", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1063", null ],
      [ "CI/CD Integration", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1064", null ],
      [ "Troubleshooting", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1065", [
        [ "Script Reports \"CMake Not Found\"", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1066", null ],
        [ "\"Git Submodules Missing\"", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1067", null ],
        [ "\"CMake Cache Too Old\"", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1068", null ],
        [ "\"Visual Studio Not Found\" (Windows)", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1069", null ],
        [ "Script Fails on Network Issues (gRPC)", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1070", null ]
      ] ],
      [ "See Also", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1071", null ]
    ] ],
    [ "yaze Test Suite", "d0/d46/md_test_2README.html", [
      [ "Directory Structure", "d0/d46/md_test_2README.html#autotoc_md1073", null ],
      [ "Test Categories", "d0/d46/md_test_2README.html#autotoc_md1074", [
        [ "Unit Tests (<tt>unit/</tt>)", "d0/d46/md_test_2README.html#autotoc_md1075", null ],
        [ "Integration Tests (<tt>integration/</tt>)", "d0/d46/md_test_2README.html#autotoc_md1076", null ],
        [ "End-to-End Tests (<tt>e2e/</tt>)", "d0/d46/md_test_2README.html#autotoc_md1077", null ]
      ] ],
      [ "Enhanced Test Runner", "d0/d46/md_test_2README.html#autotoc_md1078", [
        [ "Usage Examples", "d0/d46/md_test_2README.html#autotoc_md1079", null ],
        [ "Test Modes", "d0/d46/md_test_2README.html#autotoc_md1080", null ],
        [ "Options", "d0/d46/md_test_2README.html#autotoc_md1081", null ]
      ] ],
      [ "E2E ROM Testing", "d0/d46/md_test_2README.html#autotoc_md1082", [
        [ "Test Cases", "d0/d46/md_test_2README.html#autotoc_md1083", null ]
      ] ],
      [ "ZSCustomOverworld Upgrade Testing", "d0/d46/md_test_2README.html#autotoc_md1084", [
        [ "Supported Upgrades", "d0/d46/md_test_2README.html#autotoc_md1085", null ],
        [ "Test Cases", "d0/d46/md_test_2README.html#autotoc_md1086", null ],
        [ "Version-Specific Features", "d0/d46/md_test_2README.html#autotoc_md1087", [
          [ "Vanilla", "d0/d46/md_test_2README.html#autotoc_md1088", null ],
          [ "v2", "d0/d46/md_test_2README.html#autotoc_md1089", null ],
          [ "v3", "d0/d46/md_test_2README.html#autotoc_md1090", null ]
        ] ]
      ] ],
      [ "Environment Variables", "d0/d46/md_test_2README.html#autotoc_md1091", null ],
      [ "CI/CD Integration", "d0/d46/md_test_2README.html#autotoc_md1092", null ],
      [ "Deprecated Tests", "d0/d46/md_test_2README.html#autotoc_md1093", null ],
      [ "Best Practices", "d0/d46/md_test_2README.html#autotoc_md1094", null ],
      [ "AI Agent Testing", "d0/d46/md_test_2README.html#autotoc_md1095", null ]
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
"d0/d30/md_README.html#autotoc_md1013",
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
"d2/dc2/classyaze_1_1gui_1_1BppFormatUI.html#ab78bf5f1ce18304de4a3903c54de71bd",
"d2/def/classyaze_1_1test_1_1TestRecorder.html#a7846f89a11bb4de48c82cb210c6afcba",
"d2/dfe/structyaze_1_1gui_1_1EnhancedTheme.html#acc781dd480f7d24b863f742d7185ef75",
"d3/d19/classyaze_1_1gfx_1_1GraphicsOptimizer.html#a622b74f635ecc15d2c3928cb0bc96839",
"d3/d30/structyaze_1_1gui_1_1canvas_1_1CanvasPerformanceMetrics.html#a36e532a6954f0435d2cf06c8065c0a0a",
"d3/d44/classyaze_1_1editor_1_1OverworldEditor.html#aaa4ee9e9095ee9f1faea6dfbf2c5217a",
"d3/d63/md_docs_2z3ed_2README.html#autotoc_md948",
"d3/d6c/classyaze_1_1editor_1_1MessageEditor.html#adaa239807ea089eec98ef78bc15c912a",
"d3/d90/structyaze_1_1zelda3_1_1music_1_1SongSpcBlock.html#a0958d2a773c085c7f1f8632c31002ddb",
"d3/db3/namespaceyaze_1_1cli_1_1anonymous__namespace_02tui_8cc_03.html#a2d7b3c2328f2fcab44ddbe20d7df3c32",
"d3/de4/classyaze_1_1gfx_1_1TileInfo.html",
"d3/ded/classyaze_1_1emu_1_1Ppu.html#ac5993a8229cdc5182cf3c760f722773f",
"d4/d0a/namespaceyaze_1_1test.html#a63f1fc7b6c95bee32feed740990cccb9",
"d4/d45/classyaze_1_1editor_1_1AssemblyEditor.html#a6654c410e6c0f5d15d15581901e71ef1",
"d4/d79/app_2rom_8cc.html#a3dde9364829e66bec50d5da15df53b6a",
"d4/db9/structyaze_1_1cli_1_1TestResultDetails.html#a0c4b6fe87187f97e36b0005e073a43b2",
"d4/de6/classyaze_1_1gfx_1_1Arena.html#ada910dcae8ddc865bc1d63d04d59f44e",
"d5/d1f/namespaceyaze_1_1zelda3.html#a2da013fe0766dd24e24e03dbeecf73e6",
"d5/d1f/namespaceyaze_1_1zelda3.html#a8bb3d290803b4304f0437bdfa4e7db21",
"d5/d31/classyaze_1_1gfx_1_1SnesColor.html#a0a6bc657e8a8778b7672d2e6102771e2",
"d5/d67/classyaze_1_1cli_1_1PolicyEvaluator.html#aa63f64730aceee50f6b6c3d7e3dcaca6",
"d5/da7/classyaze_1_1emu_1_1AudioRamImpl.html#a3233ea8b3f4eed7b38b61c51b717573f",
"d5/dd0/classyaze_1_1test_1_1RomDependentTestSuite.html#aa1227cbe4a9995ec93bd36485afd942d",
"d6/d20/namespaceyaze_1_1emu.html#a8ffb499695abe870b61d949824e25ec9",
"d6/d30/classyaze_1_1Rom.html#a557475d265ec948d7d5a565e0d159ac9",
"d6/d58/tile16__editor__test_8cc.html#aad0683c720dfb09cf817595adfeceeb5",
"d6/db0/structyaze_1_1gfx_1_1TileCache.html#a932698bad07b94e8d588d377623773d8",
"d6/dcb/classyaze_1_1gui_1_1canvas_1_1CanvasPerformanceIntegration.html#a73758191cf9f9874d583b999413403f5",
"d6/df5/dungeon__editor_8h.html#a5fbf9b9f005187d191c8378e6364072c",
"d7/d5d/structyaze_1_1gfx_1_1Arena_1_1SurfacePool.html#af7a27c7bf7f326dae682288ca6729ff2",
"d7/d61/classyaze_1_1zelda3_1_1DungeonEditorSystem.html#ab3ce77b1ac89eba00322bc505f9ff9c6a92819831c0af46554249f8ad30de686d",
"d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md641",
"d7/da7/classyaze_1_1emu_1_1Apu.html#a2764ac75dec1c12445109690f73ec509",
"d7/ddb/classyaze_1_1gfx_1_1ScopedTimer.html#a60ffb6a2c7835bc551997018648891c3",
"d7/df6/classyaze_1_1zelda3_1_1Room.html#ae3bb036eadb188accb603d3bdf0f0789",
"d8/d00/test__suite__loader_8cc.html#a68b880dd4eae738940e4ac53f1dd64b9",
"d8/d42/app_2editor_2graphics_2palette__editor_8cc.html#a8657cfd46f4a80e164e0b2331c98ad98",
"d8/d7c/structyaze_1_1gfx_1_1SheetOptimizationData.html#a1f2c0f5fc0a5059503d65bfe40e9b095",
"d8/da5/structyaze_1_1gui_1_1canvas_1_1CanvasUsageStats.html#a55284a2bc1cb66247b9eef129cf383ca",
"d8/dd6/classyaze_1_1zelda3_1_1music_1_1Tracker.html#a59d5ef52c2d4337ab61261ca8bdc511d",
"d9/d21/structyaze_1_1emu_1_1DmaRegisters.html#aa36c584000400683cd4817c2145dc8fb",
"d9/d6c/classyaze_1_1editor_1_1HistoryManager.html#ad391764dc1011efd29dd2830075fcba2",
"d9/dbd/canvas__utils__moved_8h.html#ad3764d0881c0fd928890783de32daa66",
"d9/dc5/classyaze_1_1zelda3_1_1Overworld.html#a65384e147a9e89578169c45f3a8c4e4d",
"d9/dcc/classyaze_1_1zelda3_1_1OverworldMap.html#a879fb9353f241b17c00916c238a00860",
"d9/dfe/classyaze_1_1gui_1_1WidgetIdRegistry.html#a34f7539fd3fb26d87cd2dfb68543295b",
"da/d2c/classyaze_1_1gui_1_1Canvas.html#a26794e293b9d996abc70409d6aebfd8e",
"da/d2c/classyaze_1_1gui_1_1Canvas.html#ad7081d52f5a736dde87a62caf2ce63dc",
"da/d3e/classyaze_1_1test_1_1TestManager.html#ae7cbaf88ee1e98b4e2fa8c598fc21442",
"da/d6e/structyaze_1_1editor_1_1FolderItem.html#ac5e89335715847a3f63303d028403f1e",
"da/dbc/classyaze_1_1emu_1_1AsmParser.html#a4a893fda99e18179f324b2b0d1641b92",
"da/dec/structyaze_1_1gfx_1_1BppFormatInfo.html#afc705e4b3d7f792164fe17bad007c6ca",
"db/d30/structsnes__tile8.html#afc5deb54b6f6b59dda0dea55d477348b",
"db/d7d/test__editor_8cc.html#a1641768a13910eb529e97ac3a66b7162",
"db/d9a/classyaze_1_1editor_1_1MusicEditor.html#a923b466ec76879507b53a6712ae54a02",
"db/dc9/classyaze_1_1emu_1_1AudioRam.html",
"db/de8/classyaze_1_1editor_1_1DungeonRoomSelector.html#ace45952bdff186d407073d402dd411c2",
"dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md771",
"dc/d31/classyaze_1_1editor_1_1GraphicsEditor.html#abc58ed712a04e70a770355272e5d76dd",
"dc/d4f/classyaze_1_1editor_1_1DungeonObjectSelector.html#af592568776a61de0d2c9d542603776cf",
"dc/da6/structyaze_1_1emu_1_1VMADDH.html",
"dc/dee/snes__color_8cc.html#a16bca2c689cfce1349f2d1f0790dbe96",
"dc/df4/classyaze_1_1emu_1_1Cpu.html#a872d70b49bfdf7976bc04e521df4cced",
"dc/dfb/structyaze_1_1zelda3_1_1EntranceTypes_1_1EntranceInfo.html#a74c3bde73c74571ea58305b4074b14bd",
"dd/d12/classyaze_1_1editor_1_1EditorManager.html#ada03bbe59d235d9a582cad48597f0109",
"dd/d3f/structyaze_1_1zelda3_1_1MouseConfig.html#ae361d9229ec8d1482c7f4b5eac6d0f50",
"dd/d63/namespaceyaze_1_1cli.html#a0be616999fa8e675f482f53ca1830fcea0ee3f6e6cda68e5b4670f70c31130ec8",
"dd/d96/md_docs_204-api-reference.html#autotoc_md103",
"dd/ddf/classyaze_1_1gui_1_1canvas_1_1CanvasContextMenu.html#a38da3c0cff3dcf932e256655a64f300d",
"dd/dfa/structyaze_1_1zelda3_1_1DungeonEditorSystem_1_1DoorData.html#a4ff749191485927056694fffd0a4a4a2",
"de/d20/conversation__test_8cc.html#ac7a12c496d564f6c9ad23a6ec4957836",
"de/d77/structyaze_1_1cli_1_1TestSuiteConfig.html",
"de/da4/structyaze_1_1core_1_1AsarSymbol.html#a39db07841c9aa5960be8425c25bff349",
"de/dbf/icons_8h.html#a12e766e199935404a0f619e9974dc180",
"de/dbf/icons_8h.html#a31b8ea9dc2332a7db93e9ed96321a785",
"de/dbf/icons_8h.html#a4dfae6ca81fc0df3a36fd0ce8c2ca827",
"de/dbf/icons_8h.html#a68b1cefadef01a86266353fca5a08f0a",
"de/dbf/icons_8h.html#a89dd5dc3840b873f4dbf5fa0fc160056",
"de/dbf/icons_8h.html#aa89facdf1ca257d09babdf84f98f1f21",
"de/dbf/icons_8h.html#ac4dd8b00b2e2fa983804ce4b747154a9",
"de/dbf/icons_8h.html#ae0d8afdd7708ebd0bd83913bc59e3ec5",
"de/dbf/icons_8h.html#afbb7a08dc973407c1473a45c2ae8d975",
"de/de7/classyaze_1_1zelda3_1_1DungeonObjectEditor.html#a8362a1c0e5ac5ea756913f5427e14f6d",
"df/d0e/classyaze_1_1gfx_1_1Bitmap.html#a748139853655afc687cf9668eeb01443",
"df/d3e/namespaceyaze_1_1zelda3_1_1anonymous__namespace_02hyrule__magic_8cc_03.html#a8c70882aa5e5b1bb8acfb67d19a1cc6d",
"df/d80/classyaze_1_1zelda3_1_1test_1_1RoomIntegrationTest.html#acc08ebcf755d34972d65b76d2e3970ba",
"df/df5/classyaze_1_1test_1_1MockRom.html#a016a95cca484c088f2d8c13a51fd2a21",
"functions_z.html"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';