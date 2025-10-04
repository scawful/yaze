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
    [ "Build Modularization Plan for Yaze", "d7/daf/md_docs_2build__modularization__plan.html", [
      [ "Executive Summary", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md323", null ],
      [ "Current Problems", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md324", null ],
      [ "Proposed Architecture", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md325", [
        [ "Library Hierarchy", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md326", null ]
      ] ],
      [ "Implementation Guide", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md327", [
        [ "Quick Start", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md328", [
          [ "Option 1: Enable Modular Build (Recommended)", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md329", null ],
          [ "Option 2: Keep Existing Build (Default)", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md330", null ]
        ] ],
        [ "Implementation Steps", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md331", [
          [ "Step 1: Add Build Option", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md332", null ],
          [ "Step 2: Update src/CMakeLists.txt", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md333", null ],
          [ "Step 3: Update yaze Application Linking", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md334", null ],
          [ "Step 4: Update Test Executable", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md335", null ]
        ] ]
      ] ],
      [ "Expected Performance Improvements", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md336", [
        [ "Build Time Reductions", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md337", null ],
        [ "CI/CD Benefits", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md338", null ]
      ] ],
      [ "Rollback Procedure", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md339", null ]
    ] ],
    [ "Changelog", "d7/d6f/md_docs_2C1-changelog.html", [
      [ "0.3.2 (December 2025) - In Development", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md341", [
        [ "Tile16 Editor Improvements (In Progress)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md342", null ],
        [ "Graphics System Optimizations", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md343", null ],
        [ "Windows Platform Stability", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md344", null ],
        [ "Memory Safety & Stability", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md345", null ],
        [ "Testing & CI/CD Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md346", null ],
        [ "Future Optimizations (Planned)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md347", null ],
        [ "Technical Notes", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md348", null ]
      ] ],
      [ "0.3.1 (September 2025)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md349", [
        [ "Major Features", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md350", null ],
        [ "Tile16 Editor Enhancements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md351", null ],
        [ "ZSCustomOverworld v3 Implementation", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md352", null ],
        [ "Technical Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md353", null ],
        [ "User Interface", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md354", null ],
        [ "Bug Fixes", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md355", null ],
        [ "ZScream Compatibility Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md356", null ]
      ] ],
      [ "0.3.0 (September 2025)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md357", [
        [ "Major Features", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md358", null ],
        [ "User Interface & Theming", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md359", null ],
        [ "Enhancements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md360", null ],
        [ "Technical Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md361", null ],
        [ "Bug Fixes", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md362", null ]
      ] ],
      [ "0.2.2 (December 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md363", null ],
      [ "0.2.1 (August 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md364", null ],
      [ "0.2.0 (July 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md365", null ],
      [ "0.1.0 (May 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md366", null ],
      [ "0.0.9 (April 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md367", null ],
      [ "0.0.8 (February 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md368", null ],
      [ "0.0.7 (January 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md369", null ],
      [ "0.0.6 (November 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md370", null ],
      [ "0.0.5 (November 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md371", null ],
      [ "0.0.4 (November 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md372", null ],
      [ "0.0.3 (October 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md373", null ]
    ] ],
    [ "Canvas System - Comprehensive Guide", "d2/db2/md_docs_2CANVAS__GUIDE.html", [
      [ "Overview", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md375", null ],
      [ "Core Concepts", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md376", [
        [ "Canvas Structure", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md377", null ],
        [ "Coordinate Systems", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md378", null ]
      ] ],
      [ "Usage Patterns", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md379", [
        [ "Pattern 1: Basic Bitmap Display", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md380", null ],
        [ "Pattern 2: Modern Begin/End", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md381", null ],
        [ "Pattern 3: RAII ScopedCanvas", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md382", null ]
      ] ],
      [ "Feature: Tile Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md383", [
        [ "Single Tile Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md384", null ],
        [ "Tilemap Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md385", null ],
        [ "Color Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md386", null ]
      ] ],
      [ "Feature: Tile Selection", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md387", [
        [ "Single Tile Selection", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md388", null ],
        [ "Multi-Tile Rectangle Selection", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md389", null ],
        [ "Rectangle Drag & Paint", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md390", null ]
      ] ],
      [ "Feature: Custom Overlays", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md391", [
        [ "Manual Points Manipulation", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md392", null ]
      ] ],
      [ "Feature: Large Map Support", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md393", [
        [ "Map Types", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md394", null ],
        [ "Boundary Clamping", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md395", null ],
        [ "Custom Map Sizes", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md396", null ]
      ] ],
      [ "Feature: Context Menu", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md397", [
        [ "Adding Custom Items", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md398", null ],
        [ "Overworld Editor Example", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md399", null ]
      ] ],
      [ "Feature: Scratch Space (In Progress)", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md400", null ],
      [ "Common Workflows", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md401", [
        [ "Workflow 1: Overworld Tile Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md402", null ],
        [ "Workflow 2: Tile16 Blockset Selection", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md403", null ],
        [ "Workflow 3: Graphics Sheet Display", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md404", null ]
      ] ],
      [ "Configuration", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md405", [
        [ "Grid Settings", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md406", null ],
        [ "Scale Settings", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md407", null ],
        [ "Interaction Settings", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md408", null ],
        [ "Large Map Settings", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md409", null ]
      ] ],
      [ "Known Issues", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md410", [
        [ "⚠️ Rectangle Selection Wrapping", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md411", null ]
      ] ],
      [ "API Reference", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md412", [
        [ "Drawing Methods", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md413", null ],
        [ "State Accessors", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md414", null ],
        [ "Configuration", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md415", null ]
      ] ],
      [ "Implementation Notes", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md416", [
        [ "Points Management", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md417", null ],
        [ "Selection State", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md418", null ],
        [ "Overworld Rectangle Painting Flow", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md419", null ]
      ] ],
      [ "Best Practices", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md420", [
        [ "DO ✅", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md421", null ],
        [ "DON'T ❌", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md422", null ]
      ] ],
      [ "Troubleshooting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md423", [
        [ "Issue: Rectangle wraps at boundaries", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md424", null ],
        [ "Issue: Painting in wrong location", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md425", null ],
        [ "Issue: Array index out of bounds", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md426", null ],
        [ "Issue: Forgot to call End()", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md427", null ]
      ] ],
      [ "Future: Scratch Space", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md428", null ],
      [ "Summary", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md429", null ]
    ] ],
    [ "Roadmap", "d6/db4/md_docs_2D1-roadmap.html", [
      [ "0.4.X (Next Major Release)", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md431", [
        [ "Core Features", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md432", null ],
        [ "Technical Improvements", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md433", null ]
      ] ],
      [ "0.5.X", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md434", [
        [ "Advanced Features", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md435", null ]
      ] ],
      [ "0.6.X", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md436", [
        [ "Platform & Integration", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md437", null ]
      ] ],
      [ "0.7.X", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md438", [
        [ "Performance & Polish", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md439", null ]
      ] ],
      [ "0.8.X", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md440", [
        [ "Beta Preparation", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md441", null ]
      ] ],
      [ "1.0.0", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md442", [
        [ "Stable Release", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md443", null ]
      ] ],
      [ "Current Focus Areas", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md444", [
        [ "Immediate Priorities (v0.4.X)", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md445", null ],
        [ "Long-term Vision", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md446", null ]
      ] ]
    ] ],
    [ "Dungeon Canvas Blank Screen - Fix Summary", "db/d2c/md_docs_2dungeon__canvas__fix__summary.html", [
      [ "Issue", "db/d2c/md_docs_2dungeon__canvas__fix__summary.html#autotoc_md448", null ],
      [ "Root Cause", "db/d2c/md_docs_2dungeon__canvas__fix__summary.html#autotoc_md449", [
        [ "The Missing Step", "db/d2c/md_docs_2dungeon__canvas__fix__summary.html#autotoc_md450", null ],
        [ "How ZScream Does It", "db/d2c/md_docs_2dungeon__canvas__fix__summary.html#autotoc_md451", null ]
      ] ],
      [ "Solution Implemented", "db/d2c/md_docs_2dungeon__canvas__fix__summary.html#autotoc_md452", [
        [ "Changes Made", "db/d2c/md_docs_2dungeon__canvas__fix__summary.html#autotoc_md453", null ],
        [ "Technical Details", "db/d2c/md_docs_2dungeon__canvas__fix__summary.html#autotoc_md454", null ]
      ] ],
      [ "Testing", "db/d2c/md_docs_2dungeon__canvas__fix__summary.html#autotoc_md455", null ],
      [ "Future Enhancements", "db/d2c/md_docs_2dungeon__canvas__fix__summary.html#autotoc_md456", null ],
      [ "Related Files", "db/d2c/md_docs_2dungeon__canvas__fix__summary.html#autotoc_md457", null ],
      [ "Build Status", "db/d2c/md_docs_2dungeon__canvas__fix__summary.html#autotoc_md458", null ]
    ] ],
    [ "Dungeon Editor Developer Guide", "d5/d3e/md_docs_2dungeon__developer__guide.html", [
      [ "1. Implementation Plan", "d5/d3e/md_docs_2dungeon__developer__guide.html#autotoc_md460", [
        [ "1.1. Current State Analysis", "d5/d3e/md_docs_2dungeon__developer__guide.html#autotoc_md461", null ],
        [ "1.2. ZScream's Approach", "d5/d3e/md_docs_2dungeon__developer__guide.html#autotoc_md462", null ],
        [ "1.3. Implementation Tasks", "d5/d3e/md_docs_2dungeon__developer__guide.html#autotoc_md463", null ]
      ] ],
      [ "2. Graphics Rendering Pipeline", "d5/d3e/md_docs_2dungeon__developer__guide.html#autotoc_md465", [
        [ "2.1. Architecture Diagram", "d5/d3e/md_docs_2dungeon__developer__guide.html#autotoc_md466", null ],
        [ "2.2. Blank Canvas Bug: Root Cause Analysis", "d5/d3e/md_docs_2dungeon__developer__guide.html#autotoc_md467", null ]
      ] ]
    ] ],
    [ "Dungeon Editing Implementation Plan for Yaze", "df/d42/md_docs_2dungeon__editing__implementation__plan.html", [
      [ "Executive Summary", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md469", null ],
      [ "Table of Contents", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md470", null ],
      [ "Current State Analysis", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md472", [
        [ "What Works in Yaze", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md473", null ],
        [ "What Doesn't Work", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md474", null ],
        [ "Key Problems Identified", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md475", null ]
      ] ],
      [ "ZScream's Working Approach", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md477", [
        [ "Room Data Structure (ZScream)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md478", null ],
        [ "Object Data Structure (ZScream)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md479", null ],
        [ "Object Encoding Format (Critical!)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md480", [
          [ "Type 1 Objects (ID 0x000-0x0FF) - Standard Objects", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md481", null ],
          [ "Type 2 Objects (ID 0x100-0x1FF) - Special Objects", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md482", null ],
          [ "Type 3 Objects (ID 0xF00-0xFFF) - Extended Objects", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md483", null ]
        ] ],
        [ "Layer and Door Markers", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md484", null ],
        [ "Object Loading Process (ZScream)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md485", null ],
        [ "Object Saving Process (ZScream)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md486", null ],
        [ "Object Tile Loading (ZScream)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md487", null ],
        [ "Object Drawing (ZScream)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md488", null ]
      ] ],
      [ "ROM Data Structure Reference", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md490", [
        [ "Key ROM Addresses (from ALTTP Disassembly)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md491", null ],
        [ "Room Header Format (14 bytes)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md492", null ],
        [ "Object Data Format", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md493", null ],
        [ "Tile Data Format", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md494", null ]
      ] ],
      [ "Implementation Tasks", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md496", [
        [ "Phase 1: Core Object System (HIGH PRIORITY)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md497", [
          [ "Task 1.1: Object Encoding/Decoding ✅ CRITICAL", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md498", null ],
          [ "Task 1.2: Enhanced Object Parsing ✅ CRITICAL", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md499", null ],
          [ "Task 1.3: Object Tile Loading ✅ CRITICAL", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md500", null ],
          [ "Task 1.4: Object Drawing System ⚠️ HIGH PRIORITY", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md501", null ]
        ] ],
        [ "Phase 2: Editor UI Integration (MEDIUM PRIORITY)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md503", [
          [ "Task 2.1: Object Placement System", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md504", null ],
          [ "Task 2.2: Object Selection System", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md505", null ],
          [ "Task 2.3: Object Properties Editor", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md506", null ],
          [ "Task 2.4: Layer Management UI", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md507", null ]
        ] ],
        [ "Phase 3: Save System (HIGH PRIORITY)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md509", [
          [ "Task 3.1: Room Object Encoding ✅ CRITICAL", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md510", null ],
          [ "Task 3.2: ROM Writing System", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md511", null ],
          [ "Task 3.3: Save Validation", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md512", null ]
        ] ],
        [ "Phase 4: Advanced Features (LOW PRIORITY)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md514", [
          [ "Task 4.1: Undo/Redo System", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md515", null ],
          [ "Task 4.2: Copy/Paste Objects", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md516", null ],
          [ "Task 4.3: Object Library/Templates", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md517", null ],
          [ "Task 4.4: Room Import/Export", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md518", null ]
        ] ]
      ] ],
      [ "Technical Architecture", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md520", [
        [ "Class Hierarchy", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md521", null ],
        [ "Data Flow", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md522", null ],
        [ "Key Algorithms", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md523", [
          [ "Object Type Detection", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md524", null ],
          [ "Object Decoding", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md525", null ],
          [ "Object Encoding", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md526", null ]
        ] ]
      ] ],
      [ "Testing Strategy", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md528", [
        [ "Unit Tests", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md529", [
          [ "Encoding/Decoding Tests", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md530", null ],
          [ "Room Parsing Tests", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md531", null ]
        ] ],
        [ "Integration Tests", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md532", [
          [ "Round-Trip Tests", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md533", null ]
        ] ],
        [ "Manual Testing", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md534", [
          [ "Test Cases", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md535", null ]
        ] ]
      ] ],
      [ "References", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md537", [
        [ "Documentation", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md538", null ],
        [ "Key Files", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md539", [
          [ "ZScream (Reference)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md540", null ],
          [ "Yaze (Implementation)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md541", null ]
        ] ],
        [ "External Resources", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md542", null ]
      ] ],
      [ "Appendix: Object ID Reference", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md544", [
        [ "Type 1 Objects (0x00-0xFF)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md545", null ],
        [ "Type 2 Objects (0x100-0x1FF)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md546", null ],
        [ "Type 3 Objects (0xF00-0xFFF)", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md547", null ]
      ] ],
      [ "Status Tracking", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md549", [
        [ "Current Progress", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md550", null ],
        [ "Next Steps", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md551", null ],
        [ "Timeline Estimate", "df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md552", null ]
      ] ]
    ] ],
    [ "Dungeon Graphics Rendering Pipeline Analysis", "d1/dbe/md_docs_2dungeon__graphics__pipeline__analysis.html", [
      [ "Overview", "d1/dbe/md_docs_2dungeon__graphics__pipeline__analysis.html#autotoc_md555", null ],
      [ "Architecture Diagram", "d1/dbe/md_docs_2dungeon__graphics__pipeline__analysis.html#autotoc_md556", null ],
      [ "Component Breakdown", "d1/dbe/md_docs_2dungeon__graphics__pipeline__analysis.html#autotoc_md557", [
        [ "1. Arena (gfx/arena.h/.cc)", "d1/dbe/md_docs_2dungeon__graphics__pipeline__analysis.html#autotoc_md558", null ],
        [ "2. BackgroundBuffer (gfx/background_buffer.h/.cc)", "d1/dbe/md_docs_2dungeon__graphics__pipeline__analysis.html#autotoc_md559", [
          [ "<tt>SetTileAt(int x, int y, uint16_t value)</tt>", "d1/dbe/md_docs_2dungeon__graphics__pipeline__analysis.html#autotoc_md560", null ],
          [ "<tt>DrawBackground(std::span<uint8_t> gfx16_data)</tt>", "d1/dbe/md_docs_2dungeon__graphics__pipeline__analysis.html#autotoc_md561", null ],
          [ "<tt>DrawFloor()</tt>", "d1/dbe/md_docs_2dungeon__graphics__pipeline__analysis.html#autotoc_md562", null ]
        ] ],
        [ "3. Bitmap (gfx/bitmap.h/.cc)", "d1/dbe/md_docs_2dungeon__graphics__pipeline__analysis.html#autotoc_md563", [
          [ "<tt>Create(int w, int h, int depth, vector<uint8_t> data)</tt>", "d1/dbe/md_docs_2dungeon__graphics__pipeline__analysis.html#autotoc_md564", null ],
          [ "<tt>SetPaletteWithTransparent(SnesPalette palette, size_t index)</tt>", "d1/dbe/md_docs_2dungeon__graphics__pipeline__analysis.html#autotoc_md565", null ],
          [ "<tt>CreateTexture(SDL_Renderer* renderer)</tt> / <tt>UpdateTexture()</tt>", "d1/dbe/md_docs_2dungeon__graphics__pipeline__analysis.html#autotoc_md566", null ]
        ] ],
        [ "4. Room (zelda3/dungeon/room.h/.cc)", "d1/dbe/md_docs_2dungeon__graphics__pipeline__analysis.html#autotoc_md567", [
          [ "<tt>LoadRoomGraphics(uint8_t entrance_blockset)</tt>", "d1/dbe/md_docs_2dungeon__graphics__pipeline__analysis.html#autotoc_md568", null ],
          [ "<tt>CopyRoomGraphicsToBuffer()</tt>", "d1/dbe/md_docs_2dungeon__graphics__pipeline__analysis.html#autotoc_md569", null ],
          [ "<tt>RenderRoomGraphics()</tt> ⭐ <strong>Main Rendering Method</strong>", "d1/dbe/md_docs_2dungeon__graphics__pipeline__analysis.html#autotoc_md570", null ],
          [ "<tt>RenderObjectsToBackground()</tt> ⚠️ <strong>Critical New Method</strong> ✅ <strong>Fixed</strong>", "d1/dbe/md_docs_2dungeon__graphics__pipeline__analysis.html#autotoc_md571", null ]
        ] ]
      ] ],
      [ "Palette System", "d1/dbe/md_docs_2dungeon__graphics__pipeline__analysis.html#autotoc_md572", [
        [ "Palette Hierarchy", "d1/dbe/md_docs_2dungeon__graphics__pipeline__analysis.html#autotoc_md573", null ],
        [ "Palette Loading Flow", "d1/dbe/md_docs_2dungeon__graphics__pipeline__analysis.html#autotoc_md574", null ]
      ] ],
      [ "Appendix: Blank Canvas Bug - Detailed Root Cause Analysis", "d1/dbe/md_docs_2dungeon__graphics__pipeline__analysis.html#autotoc_md576", [
        [ "Problem", "d1/dbe/md_docs_2dungeon__graphics__pipeline__analysis.html#autotoc_md577", null ],
        [ "Diagnostic Output Analysis", "d1/dbe/md_docs_2dungeon__graphics__pipeline__analysis.html#autotoc_md578", [
          [ "❌ Step 7: PALETTE MISSING", "d1/dbe/md_docs_2dungeon__graphics__pipeline__analysis.html#autotoc_md579", null ]
        ] ],
        [ "Root Cause Analysis", "d1/dbe/md_docs_2dungeon__graphics__pipeline__analysis.html#autotoc_md580", [
          [ "Cause 1: Missing Palette Application", "d1/dbe/md_docs_2dungeon__graphics__pipeline__analysis.html#autotoc_md581", null ],
          [ "Cause 2: All Black Canvas (Incorrect Tile Word)", "d1/dbe/md_docs_2dungeon__graphics__pipeline__analysis.html#autotoc_md582", null ],
          [ "Cause 3: Wrong Palette (All Rooms Look \"Gargoyle-y\")", "d1/dbe/md_docs_2dungeon__graphics__pipeline__analysis.html#autotoc_md583", null ]
        ] ],
        [ "Key Takeaway", "d1/dbe/md_docs_2dungeon__graphics__pipeline__analysis.html#autotoc_md584", null ]
      ] ]
    ] ],
    [ "Asm Style Guide", "d7/d9a/md_docs_2E1-asm-style-guide.html", [
      [ "Table of Contents", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md586", null ],
      [ "File Structure", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md587", null ],
      [ "Labels and Symbols", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md588", null ],
      [ "Comments", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md589", null ],
      [ "Instructions", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md590", null ],
      [ "Macros", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md591", null ],
      [ "Loops and Branching", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md592", null ],
      [ "Data Structures", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md593", null ],
      [ "Code Organization", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md594", null ],
      [ "Custom Code", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md595", null ]
    ] ],
    [ "Dungeon Editor Guide", "dd/d33/md_docs_2E2-dungeon-editor-guide.html", [
      [ "Overview", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md597", null ],
      [ "Architecture", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md598", [
        [ "Core Components", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md599", [
          [ "1. DungeonEditorSystem", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md600", null ],
          [ "2. DungeonObjectEditor", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md601", null ],
          [ "3. ObjectRenderer", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md602", null ],
          [ "4. DungeonEditor (UI Layer)", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md603", null ]
        ] ]
      ] ],
      [ "Coordinate System", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md604", [
        [ "Room Coordinates vs Canvas Coordinates", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md605", [
          [ "Conversion Functions", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md606", null ]
        ] ],
        [ "Coordinate System Features", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md607", null ]
      ] ],
      [ "Object Rendering System", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md608", [
        [ "Object Types", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md609", null ],
        [ "Rendering Pipeline", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md610", null ],
        [ "Performance Optimizations", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md611", null ]
      ] ],
      [ "User Interface", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md612", [
        [ "Integrated Editing Panels", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md613", [
          [ "Main Canvas", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md614", null ],
          [ "Compact Editing Panels", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md615", null ]
        ] ],
        [ "Object Preview System", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md616", null ]
      ] ],
      [ "Integration with ZScream", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md617", [
        [ "Room Loading", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md618", null ],
        [ "Object Parsing", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md619", null ],
        [ "Coordinate System", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md620", null ]
      ] ],
      [ "Testing and Validation", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md621", [
        [ "Integration Tests", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md622", null ],
        [ "Test Data", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md623", null ],
        [ "Performance Benchmarks", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md624", null ]
      ] ],
      [ "Usage Examples", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md625", [
        [ "Basic Object Editing", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md626", null ],
        [ "Coordinate Conversion", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md627", null ],
        [ "Object Preview", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md628", null ]
      ] ],
      [ "Configuration Options", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md629", [
        [ "Editor Configuration", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md630", null ],
        [ "Performance Configuration", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md631", null ]
      ] ],
      [ "Troubleshooting", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md632", [
        [ "Common Issues", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md633", null ],
        [ "Debug Information", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md634", null ]
      ] ],
      [ "Future Enhancements", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md635", [
        [ "Planned Features", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md636", null ]
      ] ],
      [ "Conclusion", "dd/d33/md_docs_2E2-dungeon-editor-guide.html#autotoc_md637", null ]
    ] ],
    [ "Dungeon Editor Design Plan", "dc/d82/md_docs_2E3-dungeon-editor-design.html", [
      [ "Overview", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md639", null ],
      [ "Architecture Overview", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md640", [
        [ "Core Components", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md641", null ],
        [ "File Structure", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md642", null ]
      ] ],
      [ "Component Responsibilities", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md643", [
        [ "DungeonEditor (Main Orchestrator)", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md644", null ],
        [ "DungeonRoomSelector", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md645", null ],
        [ "DungeonCanvasViewer", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md646", null ],
        [ "DungeonObjectSelector", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md647", null ]
      ] ],
      [ "Data Flow", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md648", [
        [ "Initialization Flow", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md649", null ],
        [ "Runtime Data Flow", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md650", null ],
        [ "Key Data Structures", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md651", null ]
      ] ],
      [ "Integration Patterns", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md652", [
        [ "Component Communication", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md653", null ],
        [ "ROM Data Management", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md654", null ],
        [ "State Synchronization", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md655", null ]
      ] ],
      [ "UI Layout Architecture", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md656", [
        [ "3-Column Layout", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md657", null ],
        [ "Component Internal Layout", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md658", null ]
      ] ],
      [ "Coordinate System", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md659", [
        [ "Room Coordinates vs Canvas Coordinates", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md660", null ],
        [ "Bounds Checking", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md661", null ]
      ] ],
      [ "Error Handling & Validation", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md662", [
        [ "ROM Validation", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md663", null ],
        [ "Bounds Validation", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md664", null ]
      ] ],
      [ "Performance Considerations", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md665", [
        [ "Caching Strategy", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md666", null ],
        [ "Rendering Optimization", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md667", null ]
      ] ],
      [ "Testing Strategy", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md668", [
        [ "Integration Tests", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md669", null ],
        [ "Test Categories", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md670", null ]
      ] ],
      [ "Future Development Guidelines", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md671", [
        [ "Adding New Features", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md672", null ],
        [ "Component Extension Patterns", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md673", null ],
        [ "Data Flow Extension", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md674", null ],
        [ "UI Layout Extension", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md675", null ]
      ] ],
      [ "Common Pitfalls & Solutions", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md676", [
        [ "Memory Management", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md677", null ],
        [ "Coordinate System", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md678", null ],
        [ "State Synchronization", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md679", null ],
        [ "Performance Issues", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md680", null ]
      ] ],
      [ "Debugging Tools", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md681", [
        [ "Debug Popup", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md682", null ],
        [ "Logging", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md683", null ]
      ] ],
      [ "Build Integration", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md684", [
        [ "CMake Configuration", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md685", null ],
        [ "Dependencies", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md686", null ]
      ] ],
      [ "Conclusion", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md687", null ]
    ] ],
    [ "DungeonEditor Refactoring Plan", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html", [
      [ "Overview", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md689", null ],
      [ "Component Structure", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md690", [
        [ "✅ Created Components", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md691", [
          [ "1. DungeonToolset (<tt>dungeon_toolset.h/cc</tt>)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md692", null ],
          [ "2. DungeonObjectInteraction (<tt>dungeon_object_interaction.h/cc</tt>)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md693", null ],
          [ "3. DungeonRenderer (<tt>dungeon_renderer.h/cc</tt>)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md694", null ],
          [ "4. DungeonRoomLoader (<tt>dungeon_room_loader.h/cc</tt>)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md695", null ],
          [ "5. DungeonUsageTracker (<tt>dungeon_usage_tracker.h/cc</tt>)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md696", null ]
        ] ]
      ] ],
      [ "Refactored DungeonEditor Structure", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md697", [
        [ "Before Refactoring: 1444 lines", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md698", null ],
        [ "After Refactoring: ~400 lines", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md699", null ]
      ] ],
      [ "Method Migration Map", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md700", [
        [ "Core Editor Methods (Keep in main file)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md701", null ],
        [ "UI Methods (Keep for coordination)", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md702", null ],
        [ "Methods Moved to Components", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md703", [
          [ "→ DungeonToolset", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md704", null ],
          [ "→ DungeonObjectInteraction", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md705", null ],
          [ "→ DungeonRenderer", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md706", null ],
          [ "→ DungeonRoomLoader", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md707", null ],
          [ "→ DungeonUsageTracker", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md708", null ]
        ] ]
      ] ],
      [ "Component Communication", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md709", [
        [ "Callback System", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md710", null ],
        [ "Data Sharing", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md711", null ]
      ] ],
      [ "Benefits of Refactoring", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md712", [
        [ "1. <strong>Reduced Complexity</strong>", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md713", null ],
        [ "2. <strong>Improved Testability</strong>", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md714", null ],
        [ "3. <strong>Better Maintainability</strong>", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md715", null ],
        [ "4. <strong>Enhanced Extensibility</strong>", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md716", null ],
        [ "5. <strong>Cleaner Dependencies</strong>", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md717", null ]
      ] ],
      [ "Implementation Status", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md718", [
        [ "✅ Completed", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md719", null ],
        [ "🔄 In Progress", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md720", null ],
        [ "⏳ Pending", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md721", null ]
      ] ],
      [ "Migration Strategy", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md722", [
        [ "Phase 1: Create Components ✅", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md723", null ],
        [ "Phase 2: Integrate Components 🔄", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md724", null ],
        [ "Phase 3: Move Methods", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md725", null ],
        [ "Phase 4: Cleanup", "d7/d72/md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md726", null ]
      ] ]
    ] ],
    [ "Dungeon Object System", "da/d11/md_docs_2E5-dungeon-object-system.html", [
      [ "Overview", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md728", null ],
      [ "Architecture", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md729", [
        [ "Core Components", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md730", [
          [ "1. DungeonEditor (<tt>src/app/editor/dungeon/dungeon_editor.h</tt>)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md731", null ],
          [ "2. DungeonObjectSelector (<tt>src/app/editor/dungeon/dungeon_object_selector.h</tt>)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md732", null ],
          [ "3. DungeonCanvasViewer (<tt>src/app/editor/dungeon/dungeon_canvas_viewer.h</tt>)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md733", null ],
          [ "4. Room Management System (<tt>src/app/zelda3/dungeon/room.h</tt>)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md734", null ]
        ] ]
      ] ],
      [ "Object Types and Hierarchies", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md735", [
        [ "Room Objects", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md736", [
          [ "Type 1 Objects (0x00-0xFF)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md737", null ],
          [ "Type 2 Objects (0x100-0x1FF)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md738", null ],
          [ "Type 3 Objects (0x200+)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md739", null ]
        ] ],
        [ "Object Properties", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md740", null ]
      ] ],
      [ "How Object Placement Works", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md741", [
        [ "Selection Process", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md742", null ],
        [ "Placement Process", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md743", null ],
        [ "Code Flow", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md744", null ]
      ] ],
      [ "Rendering Pipeline", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md745", [
        [ "Object Rendering", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md746", null ],
        [ "Performance Optimizations", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md747", null ]
      ] ],
      [ "User Interface Components", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md748", [
        [ "Three-Column Layout", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md749", [
          [ "Column 1: Room Control Panel (280px fixed)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md750", null ],
          [ "Column 2: Windowed Canvas (800px fixed)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md751", null ],
          [ "Column 3: Object Selector/Editor (stretch)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md752", null ]
        ] ],
        [ "Debug and Control Features", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md753", [
          [ "Room Properties Table", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md754", null ],
          [ "Object Statistics", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md755", null ]
        ] ]
      ] ],
      [ "Integration with ROM Data", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md756", [
        [ "Data Sources", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md757", [
          [ "Room Headers (<tt>0x1F8000</tt>)", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md758", null ],
          [ "Object Data", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md759", null ],
          [ "Graphics Data", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md760", null ]
        ] ],
        [ "Assembly Integration", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md761", null ]
      ] ],
      [ "Comparison with ZScream", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md762", [
        [ "Architectural Differences", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md763", [
          [ "Component-Based Architecture", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md764", null ],
          [ "Real-time Rendering", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md765", null ],
          [ "UI Organization", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md766", null ],
          [ "Caching Strategy", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md767", null ]
        ] ],
        [ "Shared Concepts", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md768", null ]
      ] ],
      [ "Best Practices", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md769", [
        [ "Performance", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md770", null ],
        [ "Code Organization", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md771", null ],
        [ "User Experience", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md772", null ]
      ] ],
      [ "Future Enhancements", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md773", [
        [ "Planned Features", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md774", null ],
        [ "Technical Improvements", "da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md775", null ]
      ] ]
    ] ],
    [ "Tile16 Editor Palette System", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html", [
      [ "Executive Summary", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md777", null ],
      [ "Problem Analysis", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md778", [
        [ "Critical Issues Identified", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md779", null ]
      ] ],
      [ "Solution Architecture", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md780", [
        [ "Core Design Principles", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md781", null ],
        [ "256-Color Overworld Palette Structure", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md782", null ],
        [ "Sheet-to-Palette Mapping", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md783", null ],
        [ "Palette Button Mapping", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md784", null ]
      ] ],
      [ "Implementation Details", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md785", [
        [ "1. Fixed SetPaletteWithTransparent Method", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md786", null ],
        [ "2. Corrected Tile16 Editor Palette System", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md787", null ],
        [ "3. Palette Coordination Flow", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md788", null ]
      ] ],
      [ "UI/UX Refactoring", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md789", [
        [ "New Three-Column Layout", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md790", null ],
        [ "Canvas Context Menu Fixes", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md791", null ],
        [ "Dynamic Zoom Controls", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md792", null ]
      ] ],
      [ "Testing Protocol", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md793", [
        [ "Crash Prevention Testing", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md794", null ],
        [ "Color Alignment Testing", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md795", null ],
        [ "UI/UX Testing", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md796", null ]
      ] ],
      [ "Error Handling", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md797", [
        [ "Bounds Checking", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md798", null ],
        [ "Fallback Mechanisms", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md799", null ]
      ] ],
      [ "Debug Information Display", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md800", null ],
      [ "Known Issues and Ongoing Work", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md801", [
        [ "Completed Items ✅", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md802", null ],
        [ "Active Issues ⚠️", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md803", null ],
        [ "Current Status Summary", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md804", null ],
        [ "Future Enhancements", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md805", null ]
      ] ],
      [ "Maintenance Notes", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md806", null ],
      [ "Next Steps", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md808", [
        [ "Immediate Priorities", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md809", null ],
        [ "Investigation Areas", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md810", null ]
      ] ]
    ] ],
    [ "Overworld Loading Guide", "dc/d12/md_docs_2F1-overworld-loading.html", [
      [ "Table of Contents", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md813", null ],
      [ "Overview", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md814", null ],
      [ "ROM Types and Versions", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md815", [
        [ "Version Detection", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md816", null ],
        [ "Feature Support by Version", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md817", null ]
      ] ],
      [ "Overworld Map Structure", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md818", [
        [ "Core Properties", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md819", null ]
      ] ],
      [ "Overlays and Special Area Maps", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md820", [
        [ "Understanding Overlays", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md821", null ],
        [ "Special Area Maps (0x80-0x9F)", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md822", null ],
        [ "Overlay ID Mappings", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md823", null ],
        [ "Drawing Order", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md824", null ],
        [ "Vanilla Overlay Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md825", null ],
        [ "Special Area Graphics Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md826", null ]
      ] ],
      [ "Loading Process", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md827", [
        [ "1. Version Detection", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md828", null ],
        [ "2. Map Initialization", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md829", null ],
        [ "3. Property Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md830", [
          [ "Vanilla ROMs (asm_version == 0xFF)", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md831", null ],
          [ "ZSCustomOverworld v2/v3", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md832", null ]
        ] ],
        [ "4. Custom Data Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md833", null ]
      ] ],
      [ "ZScream Implementation", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md834", [
        [ "OverworldMap Constructor", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md835", null ],
        [ "Key Methods", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md836", null ]
      ] ],
      [ "Yaze Implementation", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md837", [
        [ "OverworldMap Constructor", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md838", null ],
        [ "Key Methods", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md839", null ],
        [ "Current Status", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md840", null ]
      ] ],
      [ "Key Differences", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md841", [
        [ "1. Language and Architecture", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md842", null ],
        [ "2. Data Structures", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md843", null ],
        [ "3. Error Handling", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md844", null ],
        [ "4. Graphics Processing", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md845", null ]
      ] ],
      [ "Common Issues and Solutions", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md846", [
        [ "1. Version Detection Issues", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md847", null ],
        [ "2. Palette Loading Errors", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md848", null ],
        [ "3. Graphics Not Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md849", null ],
        [ "4. Overlay Issues", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md850", null ],
        [ "5. Large Map Problems", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md851", null ],
        [ "6. Special Area Graphics Issues", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md852", null ]
      ] ],
      [ "Best Practices", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md853", [
        [ "1. Version-Specific Code", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md854", null ],
        [ "2. Error Handling", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md855", null ],
        [ "3. Memory Management", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md856", null ],
        [ "4. Thread Safety", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md857", null ]
      ] ],
      [ "Conclusion", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md858", null ]
    ] ],
    [ "YAZE Graphics System Optimization Recommendations", "de/def/md_docs_2gfx__optimization__recommendations.html", [
      [ "Overview", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md860", null ],
      [ "Current Architecture Analysis", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md861", [
        [ "Strengths", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md862", null ],
        [ "Performance Bottlenecks Identified", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md863", [
          [ "1. Bitmap Class Issues", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md864", null ],
          [ "2. Arena Resource Management", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md865", null ],
          [ "3. Tilemap Performance", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md866", null ]
        ] ]
      ] ],
      [ "Optimization Recommendations", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md867", [
        [ "1. Bitmap Class Optimizations", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md868", [
          [ "A. Palette Lookup Optimization", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md869", null ],
          [ "B. Dirty Region Tracking", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md870", null ]
        ] ],
        [ "2. Arena Resource Management Improvements", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md871", [
          [ "A. Resource Pooling", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md872", null ],
          [ "B. Batch Operations", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md873", null ]
        ] ],
        [ "3. Tilemap Performance Enhancements", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md874", [
          [ "A. Smart Tile Caching", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md875", null ],
          [ "B. Atlas-based Rendering", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md876", null ]
        ] ],
        [ "4. Editor-Specific Optimizations", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md877", [
          [ "A. Graphics Editor Improvements", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md878", null ],
          [ "B. Palette Editor Optimizations", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md879", null ]
        ] ],
        [ "5. Memory Management Improvements", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md880", [
          [ "A. Custom Allocator for Graphics Data", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md881", null ],
          [ "B. Smart Pointer Management", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md882", null ]
        ] ]
      ] ],
      [ "Implementation Priority", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md883", [
        [ "Phase 1 (High Impact, Low Risk)", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md884", null ],
        [ "Phase 2 (Medium Impact, Medium Risk)", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md885", null ],
        [ "Phase 3 (High Impact, High Risk)", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md886", null ]
      ] ],
      [ "Performance Metrics", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md887", [
        [ "Target Improvements", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md888", null ],
        [ "Measurement Tools", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md889", null ]
      ] ],
      [ "Conclusion", "de/def/md_docs_2gfx__optimization__recommendations.html#autotoc_md890", null ]
    ] ],
    [ "YAZE Graphics System Optimizations", "d6/df4/md_docs_2gfx__optimizations__complete.html", [
      [ "Overview", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md892", null ],
      [ "Implemented Optimizations", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md893", [
        [ "1. Palette Lookup Optimization", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md894", null ],
        [ "2. Dirty Region Tracking", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md895", null ],
        [ "3. Resource Pooling", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md896", null ],
        [ "4. LRU Tile Caching", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md897", null ],
        [ "5. Batch Operations", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md898", null ],
        [ "6. Memory Pool Allocator", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md899", null ],
        [ "7. Atlas-Based Rendering", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md900", null ],
        [ "8. Performance Monitoring & Validation", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md901", null ]
      ] ],
      [ "Future Optimization Recommendations", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md902", [
        [ "High Priority", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md903", null ],
        [ "Medium Priority", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md904", null ]
      ] ],
      [ "Conclusion", "d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md905", null ]
    ] ],
    [ "ImGui Widget Testing Guide", "d8/d20/md_docs_2imgui__widget__testing__guide.html", [
      [ "Overview", "d8/d20/md_docs_2imgui__widget__testing__guide.html#autotoc_md907", null ],
      [ "Architecture", "d8/d20/md_docs_2imgui__widget__testing__guide.html#autotoc_md908", [
        [ "Components", "d8/d20/md_docs_2imgui__widget__testing__guide.html#autotoc_md909", null ],
        [ "Widget Hierarchy", "d8/d20/md_docs_2imgui__widget__testing__guide.html#autotoc_md910", null ]
      ] ],
      [ "Integration Guide", "d8/d20/md_docs_2imgui__widget__testing__guide.html#autotoc_md911", [
        [ "1. Add Auto-Registration to Your Editor", "d8/d20/md_docs_2imgui__widget__testing__guide.html#autotoc_md912", null ],
        [ "2. Register Canvas and Tables", "d8/d20/md_docs_2imgui__widget__testing__guide.html#autotoc_md913", null ],
        [ "3. Hierarchical Scoping", "d8/d20/md_docs_2imgui__widget__testing__guide.html#autotoc_md914", null ],
        [ "4. Register Custom Widgets", "d8/d20/md_docs_2imgui__widget__testing__guide.html#autotoc_md915", null ]
      ] ],
      [ "Writing Tests", "d8/d20/md_docs_2imgui__widget__testing__guide.html#autotoc_md916", [
        [ "Basic Test Structure", "d8/d20/md_docs_2imgui__widget__testing__guide.html#autotoc_md917", null ],
        [ "Test Actions", "d8/d20/md_docs_2imgui__widget__testing__guide.html#autotoc_md918", null ],
        [ "Test with Variables", "d8/d20/md_docs_2imgui__widget__testing__guide.html#autotoc_md919", null ]
      ] ],
      [ "Agent Integration", "d8/d20/md_docs_2imgui__widget__testing__guide.html#autotoc_md920", [
        [ "Widget Discovery", "d8/d20/md_docs_2imgui__widget__testing__guide.html#autotoc_md921", null ],
        [ "Remote Testing via gRPC", "d8/d20/md_docs_2imgui__widget__testing__guide.html#autotoc_md922", null ]
      ] ],
      [ "Best Practices", "d8/d20/md_docs_2imgui__widget__testing__guide.html#autotoc_md923", [
        [ "1. Use Stable IDs", "d8/d20/md_docs_2imgui__widget__testing__guide.html#autotoc_md924", null ],
        [ "2. Hierarchical Naming", "d8/d20/md_docs_2imgui__widget__testing__guide.html#autotoc_md925", null ],
        [ "3. Descriptive Names", "d8/d20/md_docs_2imgui__widget__testing__guide.html#autotoc_md926", null ],
        [ "4. Frame Lifecycle", "d8/d20/md_docs_2imgui__widget__testing__guide.html#autotoc_md927", null ],
        [ "5. Test Organization", "d8/d20/md_docs_2imgui__widget__testing__guide.html#autotoc_md928", null ]
      ] ],
      [ "Debugging", "d8/d20/md_docs_2imgui__widget__testing__guide.html#autotoc_md929", [
        [ "View Widget Registry", "d8/d20/md_docs_2imgui__widget__testing__guide.html#autotoc_md930", null ],
        [ "Check if Widget Registered", "d8/d20/md_docs_2imgui__widget__testing__guide.html#autotoc_md931", null ],
        [ "Test Engine Debug UI", "d8/d20/md_docs_2imgui__widget__testing__guide.html#autotoc_md932", null ]
      ] ],
      [ "Performance Considerations", "d8/d20/md_docs_2imgui__widget__testing__guide.html#autotoc_md933", null ],
      [ "Common Issues", "d8/d20/md_docs_2imgui__widget__testing__guide.html#autotoc_md934", [
        [ "Widget Not Found", "d8/d20/md_docs_2imgui__widget__testing__guide.html#autotoc_md935", null ],
        [ "ID Collisions", "d8/d20/md_docs_2imgui__widget__testing__guide.html#autotoc_md936", null ],
        [ "Scope Issues", "d8/d20/md_docs_2imgui__widget__testing__guide.html#autotoc_md937", null ]
      ] ],
      [ "Examples", "d8/d20/md_docs_2imgui__widget__testing__guide.html#autotoc_md938", null ],
      [ "References", "d8/d20/md_docs_2imgui__widget__testing__guide.html#autotoc_md939", null ]
    ] ],
    [ "yaze Documentation", "d3/d4c/md_docs_2index.html", [
      [ "Quick Start", "d3/d4c/md_docs_2index.html#autotoc_md941", null ],
      [ "Development", "d3/d4c/md_docs_2index.html#autotoc_md942", null ],
      [ "Technical Documentation", "d3/d4c/md_docs_2index.html#autotoc_md943", [
        [ "Assembly & Code", "d3/d4c/md_docs_2index.html#autotoc_md944", null ],
        [ "Editor Systems", "d3/d4c/md_docs_2index.html#autotoc_md945", null ],
        [ "Overworld System", "d3/d4c/md_docs_2index.html#autotoc_md946", null ]
      ] ],
      [ "Key Features", "d3/d4c/md_docs_2index.html#autotoc_md947", null ]
    ] ],
    [ "Ollama Integration Status - Updated# Ollama Integration Status", "da/d40/md_docs_2ollama__integration__status.html", [
      [ "✅ Completed## ✅ Completed", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md950", [
        [ "Infrastructure### Flag Parsing", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md951", null ],
        [ "Current Issue: Empty Tool Results### Tool System", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md952", null ]
      ] ],
      [ "🎨 New Features Added  - OR doesn't provide a <tt>text_response</tt> field in the JSON", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md953", [
        [ "Verbose Mode**Solution Needed**: Update system prompt to include explicit instructions like:", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md954", null ],
        [ "Priority 2: Refine Prompts (MEDIUM)- ✅ Model loads (qwen2.5-coder:7b)", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md955", null ],
        [ "Priority 3: Documentation (LOW)```", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md956", null ]
      ] ],
      [ "🧪 Testing Commands### Priority 1: Fix Tool Calling Loop (High Priority)", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md957", null ],
      [ "📊 Performance   - codellama:7b", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md958", null ],
      [ "🎯 Success Criteria", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md959", [
        [ "Priority 3: Documentation (Low Priority)", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md960", null ]
      ] ],
      [ "🐛 Known Issues", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md961", null ],
      [ "💡 Recommendations", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md962", [
        [ "For Users", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md963", null ],
        [ "For Developers", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md964", null ]
      ] ],
      [ "📝 Related Files", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md965", null ],
      [ "🎯 Success Criteria", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md966", [
        [ "Minimum Viable", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md967", null ],
        [ "Full Success", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md968", null ]
      ] ]
    ] ],
    [ "yaze Overworld Testing Guide", "de/d8a/md_docs_2overworld__testing__guide.html", [
      [ "Overview", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md971", null ],
      [ "Test Architecture", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md972", [
        [ "1. Golden Data System", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md973", null ],
        [ "2. Test Categories", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md974", [
          [ "Unit Tests (<tt>test/unit/zelda3/</tt>)", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md975", null ],
          [ "Integration Tests (<tt>test/e2e/</tt>)", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md976", null ],
          [ "Golden Data Tools", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md977", null ]
        ] ]
      ] ],
      [ "Quick Start", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md978", [
        [ "Prerequisites", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md979", null ],
        [ "Running Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md980", [
          [ "Basic Test Run", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md981", null ],
          [ "Selective Test Execution", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md982", null ]
        ] ]
      ] ],
      [ "Test Components", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md983", [
        [ "1. Golden Data Extractor", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md984", null ],
        [ "2. Integration Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md985", null ],
        [ "3. End-to-End Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md986", null ]
      ] ],
      [ "Test Validation Points", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md987", [
        [ "1. ZScream Compatibility", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md988", null ],
        [ "2. ROM State Validation", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md989", null ],
        [ "3. Performance and Stability", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md990", null ]
      ] ],
      [ "Environment Variables", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md991", [
        [ "Test Configuration", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md992", null ],
        [ "Build Configuration", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md993", null ]
      ] ],
      [ "Test Reports", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md994", [
        [ "Generated Reports", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md995", null ],
        [ "Report Location", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md996", null ]
      ] ],
      [ "Troubleshooting", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md997", [
        [ "Common Issues", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md998", [
          [ "1. ROM Not Found", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md999", null ],
          [ "2. Build Failures", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1000", null ],
          [ "3. Test Failures", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1001", null ]
        ] ],
        [ "Debug Mode", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1002", null ]
      ] ],
      [ "Advanced Usage", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1003", [
        [ "Custom Test Scenarios", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1004", [
          [ "1. Testing Custom ROMs", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1005", null ],
          [ "2. Regression Testing", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1006", null ],
          [ "3. Performance Testing", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1007", null ]
        ] ]
      ] ],
      [ "Integration with CI/CD", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1008", [
        [ "GitHub Actions Example", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1009", null ]
      ] ],
      [ "Contributing", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1010", [
        [ "Adding New Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1011", null ],
        [ "Test Guidelines", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1012", null ],
        [ "Example Test Structure", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1013", null ]
      ] ],
      [ "Conclusion", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md1014", null ]
    ] ],
    [ "z3ed Developer Guide", "d3/d04/md_docs_2z3ed_2developer__guide.html", [
      [ "1. Overview", "d3/d04/md_docs_2z3ed_2developer__guide.html#autotoc_md1016", [
        [ "Core Capabilities", "d3/d04/md_docs_2z3ed_2developer__guide.html#autotoc_md1017", null ]
      ] ],
      [ "2. Architecture", "d3/d04/md_docs_2z3ed_2developer__guide.html#autotoc_md1018", [
        [ "System Components Diagram", "d3/d04/md_docs_2z3ed_2developer__guide.html#autotoc_md1019", null ],
        [ "Key Architectural Decisions", "d3/d04/md_docs_2z3ed_2developer__guide.html#autotoc_md1020", null ]
      ] ],
      [ "3. Command Reference", "d3/d04/md_docs_2z3ed_2developer__guide.html#autotoc_md1021", [
        [ "Agent Commands", "d3/d04/md_docs_2z3ed_2developer__guide.html#autotoc_md1022", null ],
        [ "Resource Commands", "d3/d04/md_docs_2z3ed_2developer__guide.html#autotoc_md1023", null ]
      ] ],
      [ "4. Agentic & Generative Workflow (MCP)", "d3/d04/md_docs_2z3ed_2developer__guide.html#autotoc_md1024", null ],
      [ "5. Roadmap & Implementation Status", "d3/d04/md_docs_2z3ed_2developer__guide.html#autotoc_md1025", [
        [ "✅ Completed", "d3/d04/md_docs_2z3ed_2developer__guide.html#autotoc_md1026", null ],
        [ "🚧 Active & Next Steps", "d3/d04/md_docs_2z3ed_2developer__guide.html#autotoc_md1027", null ]
      ] ],
      [ "6. Technical Implementation Details", "d3/d04/md_docs_2z3ed_2developer__guide.html#autotoc_md1028", [
        [ "Build System", "d3/d04/md_docs_2z3ed_2developer__guide.html#autotoc_md1029", null ]
      ] ],
      [ "7. AI Provider Configuration", "d3/d04/md_docs_2z3ed_2developer__guide.html#autotoc_md1031", [
        [ "Supported Providers", "d3/d04/md_docs_2z3ed_2developer__guide.html#autotoc_md1032", null ],
        [ "Core Flags", "d3/d04/md_docs_2z3ed_2developer__guide.html#autotoc_md1033", null ]
      ] ],
      [ "8. Agent Chat Input Methods", "d3/d04/md_docs_2z3ed_2developer__guide.html#autotoc_md1035", null ],
      [ "9. Test Harness (gRPC)", "d3/d04/md_docs_2z3ed_2developer__guide.html#autotoc_md1037", null ]
    ] ],
    [ "z3ed: AI-Powered CLI for YAZE", "d3/d63/md_docs_2z3ed_2README.html", [
      [ "Overview", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1039", null ],
      [ "Quick Start", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1040", [
        [ "Build", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1041", null ],
        [ "AI Setup", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1042", null ],
        [ "Example Commands", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1043", null ]
      ] ],
      [ "Chat Modes", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1044", [
        [ "1. FTXUI Chat (<tt>agent chat</tt>)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1045", null ],
        [ "2. Simple Chat (<tt>agent simple-chat</tt>)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1046", null ],
        [ "3. GUI Chat Widget (In Progress)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1047", null ]
      ] ],
      [ "Available Tools", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1048", null ],
      [ "Documentation", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1049", null ],
      [ "Recent Updates (Oct 3, 2025)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1050", [
        [ "✅ Implemented", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1051", null ],
        [ "🎯 Next Steps", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1052", null ]
      ] ],
      [ "Troubleshooting", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1053", [
        [ "\"AI features not available\"", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1054", null ],
        [ "\"OpenSSL not found\"", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1055", null ],
        [ "Chat mode freezes", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1056", null ],
        [ "Tool not being called", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1057", null ]
      ] ],
      [ "Example Workflows", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1058", [
        [ "Explore ROM", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1059", null ],
        [ "Make Changes", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1060", null ]
      ] ],
      [ "Overview", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1061", null ],
      [ "Quick Start", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1062", [
        [ "Build Options", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1063", null ],
        [ "AI Agent Commands", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1064", null ],
        [ "GUI Testing Commands", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1065", null ]
      ] ],
      [ "AI Service Setup", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1066", [
        [ "Ollama (Local LLM - Recommended for Development)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1067", null ],
        [ "Gemini (Google Cloud API)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1068", null ],
        [ "Example Prompts", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1069", null ]
      ] ],
      [ "Core Documentation", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1070", [
        [ "Essential Reads", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1071", null ]
      ] ],
      [ "Current Status (October 3, 2025)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1072", [
        [ "✅ Production Ready", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1073", null ],
        [ "� In Progress (Priority Order)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1074", null ],
        [ "📋 Next Steps", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1075", null ]
      ] ],
      [ "AI Editing Focus Areas", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1076", [
        [ "Overworld Tile16 Editing ⭐ PRIMARY FOCUS", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1077", null ],
        [ "Dungeon Editing", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1078", null ],
        [ "Palette Editing", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1079", null ],
        [ "Additional Capabilities", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1080", null ]
      ] ],
      [ "Example Workflows", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1081", [
        [ "Basic Tile16 Edit", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1082", null ],
        [ "Complex Multi-Step Edit", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1083", null ],
        [ "Locate Existing Tiles", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1084", null ],
        [ "Label-Aware Dungeon Edit", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1085", null ]
      ] ],
      [ "Dependencies Guard", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1086", null ],
      [ "Recent Changes (Oct 3, 2025)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1087", [
        [ "Z3ED_AI Build Flag (Major Improvement)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1088", null ],
        [ "Build System", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1089", null ],
        [ "Documentation", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1090", null ]
      ] ],
      [ "Troubleshooting", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1091", [
        [ "\"OpenSSL not found\" warning", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1092", null ],
        [ "\"Build with -DZ3ED_AI=ON\" warning", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1093", null ],
        [ "\"gRPC not available\" error", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1094", null ],
        [ "AI generates invalid commands", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1095", null ],
        [ "Testing the conversational agent", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1096", null ],
        [ "Verifying ImGui test harness", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1097", [
          [ "Gemini-Specific Issues", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md1098", null ]
        ] ]
      ] ]
    ] ],
    [ "yaze - Yet Another Zelda3 Editor", "d0/d30/md_README.html", [
      [ "Version 0.3.2 - Release", "d0/d30/md_README.html#autotoc_md1100", [
        [ "🛠️ Technical Improvements", "d0/d30/md_README.html#autotoc_md1104", null ]
      ] ],
      [ "Quick Start", "d0/d30/md_README.html#autotoc_md1105", [
        [ "Build", "d0/d30/md_README.html#autotoc_md1106", null ],
        [ "Applications", "d0/d30/md_README.html#autotoc_md1107", null ]
      ] ],
      [ "Usage", "d0/d30/md_README.html#autotoc_md1108", [
        [ "GUI Editor", "d0/d30/md_README.html#autotoc_md1109", null ],
        [ "Command Line Tool", "d0/d30/md_README.html#autotoc_md1110", null ],
        [ "C++ API", "d0/d30/md_README.html#autotoc_md1111", null ]
      ] ],
      [ "Documentation", "d0/d30/md_README.html#autotoc_md1112", null ],
      [ "Supported Platforms", "d0/d30/md_README.html#autotoc_md1113", null ],
      [ "ROM Compatibility", "d0/d30/md_README.html#autotoc_md1114", null ],
      [ "Contributing", "d0/d30/md_README.html#autotoc_md1115", null ],
      [ "License", "d0/d30/md_README.html#autotoc_md1116", null ],
      [ "🙏 Acknowledgments", "d0/d30/md_README.html#autotoc_md1117", null ],
      [ "📸 Screenshots", "d0/d30/md_README.html#autotoc_md1118", null ]
    ] ],
    [ "yaze Build Scripts", "de/d82/md_scripts_2README.html", [
      [ "Windows Scripts", "de/d82/md_scripts_2README.html#autotoc_md1121", [
        [ "vcpkg Setup (Optional)", "de/d82/md_scripts_2README.html#autotoc_md1122", null ]
      ] ],
      [ "Windows Build Workflow", "de/d82/md_scripts_2README.html#autotoc_md1123", [
        [ "Recommended: Visual Studio CMake Mode", "de/d82/md_scripts_2README.html#autotoc_md1124", null ],
        [ "Command Line Build", "de/d82/md_scripts_2README.html#autotoc_md1125", null ],
        [ "Compiler Notes", "de/d82/md_scripts_2README.html#autotoc_md1126", null ]
      ] ],
      [ "Quick Start (Windows)", "de/d82/md_scripts_2README.html#autotoc_md1127", [
        [ "Option 1: Visual Studio (Recommended)", "de/d82/md_scripts_2README.html#autotoc_md1128", null ],
        [ "Option 2: Command Line", "de/d82/md_scripts_2README.html#autotoc_md1129", null ],
        [ "Option 3: With vcpkg (Optional)", "de/d82/md_scripts_2README.html#autotoc_md1130", null ]
      ] ],
      [ "Troubleshooting", "de/d82/md_scripts_2README.html#autotoc_md1131", [
        [ "Common Issues", "de/d82/md_scripts_2README.html#autotoc_md1132", null ],
        [ "Getting Help", "de/d82/md_scripts_2README.html#autotoc_md1133", null ]
      ] ],
      [ "Other Scripts", "de/d82/md_scripts_2README.html#autotoc_md1134", null ]
    ] ],
    [ "YAZE Build Environment Verification Scripts", "dc/db4/md_scripts_2README__VERIFICATION.html", [
      [ "Quick Start", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1136", [
        [ "Verify Build Environment", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1137", null ]
      ] ],
      [ "Scripts Overview", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1138", [
        [ "<tt>verify-build-environment.ps1</tt> / <tt>.sh</tt>", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1139", [
          [ "Usage", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1140", null ],
          [ "Exit Codes", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1141", null ]
        ] ]
      ] ],
      [ "Common Workflows", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1142", [
        [ "First-Time Setup", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1143", null ],
        [ "After Pulling Changes", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1144", null ],
        [ "Troubleshooting Build Issues", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1145", null ],
        [ "Before Opening Pull Request", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1146", null ]
      ] ],
      [ "Automatic Fixes", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1147", [
        [ "Always Auto-Fixed (No Confirmation Required)", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1148", null ],
        [ "Fixed with <tt>-FixIssues</tt> / <tt>--fix</tt> Flag", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1149", null ],
        [ "Fixed with <tt>-CleanCache</tt> / <tt>--clean</tt> Flag", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1150", null ],
        [ "Optional Verbose Tests", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1151", null ]
      ] ],
      [ "Integration with Visual Studio", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1152", null ],
      [ "What Gets Checked", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1153", [
        [ "CMake (Required)", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1154", null ],
        [ "Git (Required)", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1155", null ],
        [ "Compilers (Required)", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1156", null ],
        [ "Platform Dependencies", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1157", null ],
        [ "CMake Cache", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1158", null ],
        [ "Dependencies", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1159", null ]
      ] ],
      [ "Automatic Fixes", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1160", null ],
      [ "CI/CD Integration", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1161", null ],
      [ "Troubleshooting", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1162", [
        [ "Script Reports \"CMake Not Found\"", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1163", null ],
        [ "\"Git Submodules Missing\"", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1164", null ],
        [ "\"CMake Cache Too Old\"", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1165", null ],
        [ "\"Visual Studio Not Found\" (Windows)", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1166", null ],
        [ "Script Fails on Network Issues (gRPC)", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1167", null ]
      ] ],
      [ "See Also", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1168", null ]
    ] ],
    [ "yaze Test Suite", "d0/d46/md_test_2README.html", [
      [ "Directory Structure", "d0/d46/md_test_2README.html#autotoc_md1170", null ],
      [ "Test Categories", "d0/d46/md_test_2README.html#autotoc_md1171", [
        [ "Unit Tests (<tt>unit/</tt>)", "d0/d46/md_test_2README.html#autotoc_md1172", null ],
        [ "Integration Tests (<tt>integration/</tt>)", "d0/d46/md_test_2README.html#autotoc_md1173", null ],
        [ "End-to-End Tests (<tt>e2e/</tt>)", "d0/d46/md_test_2README.html#autotoc_md1174", null ]
      ] ],
      [ "Enhanced Test Runner", "d0/d46/md_test_2README.html#autotoc_md1175", [
        [ "Usage Examples", "d0/d46/md_test_2README.html#autotoc_md1176", null ],
        [ "Test Modes", "d0/d46/md_test_2README.html#autotoc_md1177", null ],
        [ "Options", "d0/d46/md_test_2README.html#autotoc_md1178", null ]
      ] ],
      [ "E2E ROM Testing", "d0/d46/md_test_2README.html#autotoc_md1179", [
        [ "Test Cases", "d0/d46/md_test_2README.html#autotoc_md1180", null ]
      ] ],
      [ "ZSCustomOverworld Upgrade Testing", "d0/d46/md_test_2README.html#autotoc_md1181", [
        [ "Supported Upgrades", "d0/d46/md_test_2README.html#autotoc_md1182", null ],
        [ "Test Cases", "d0/d46/md_test_2README.html#autotoc_md1183", null ],
        [ "Version-Specific Features", "d0/d46/md_test_2README.html#autotoc_md1184", [
          [ "Vanilla", "d0/d46/md_test_2README.html#autotoc_md1185", null ],
          [ "v2", "d0/d46/md_test_2README.html#autotoc_md1186", null ],
          [ "v3", "d0/d46/md_test_2README.html#autotoc_md1187", null ]
        ] ]
      ] ],
      [ "Environment Variables", "d0/d46/md_test_2README.html#autotoc_md1188", null ],
      [ "CI/CD Integration", "d0/d46/md_test_2README.html#autotoc_md1189", null ],
      [ "Deprecated Tests", "d0/d46/md_test_2README.html#autotoc_md1190", null ],
      [ "Best Practices", "d0/d46/md_test_2README.html#autotoc_md1191", null ],
      [ "AI Agent Testing", "d0/d46/md_test_2README.html#autotoc_md1192", null ]
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
"d0/d27/namespaceyaze_1_1gfx.html#a79031ccfd970eb7febc221e6a71dd16a",
"d0/d30/md_README.html#autotoc_md1107",
"d0/d78/structyaze__editor__context.html",
"d0/dd9/editor__manager_8cc.html",
"d1/d1f/overworld__entrance_8h.html#a10a271919ba5056cbf1c23ce2463769e",
"d1/d3b/structyaze_1_1cli_1_1DiscoverWidgetsQuery.html#aa8c6d5c9c9b732a7ceb7efdd3c4b18c8",
"d1/d4b/namespaceyaze_1_1gfx_1_1lc__lz2.html#a811f14da55752f836220fa108858424b",
"d1/d8a/room__entrance_8h.html#a0c3ac89b9810205e3a0dc8495687b6dd",
"d1/dbe/md_docs_2dungeon__graphics__pipeline__analysis.html#autotoc_md579",
"d1/def/structyaze_1_1core_1_1WorkspaceSettings.html#aec05bff888367bfdb665fc2ccfb2e17d",
"d2/d07/classyaze_1_1emu_1_1Spc700.html#aaaa5925cb3350f190c21bacba8efd4f5",
"d2/d39/classyaze_1_1gui_1_1MultiSelect.html#a2d5be49cc17281465b2be2e8992ba4ea",
"d2/d5e/classyaze_1_1emu_1_1Memory.html#addd524c2c6a5d113f7f2956e7800cde0",
"d2/dc2/classyaze_1_1gui_1_1BppFormatUI.html#a0630f792705ff6b6ab9c3048c28a0af5",
"d2/def/classyaze_1_1emu_1_1InstructionEntry.html#a5654f63a191f81bfa2ff2affad74613a",
"d2/dfe/structyaze_1_1gui_1_1EnhancedTheme.html#a84f7ec07c3bb6c269e9402ca193599c2",
"d3/d15/classyaze_1_1emu_1_1Snes.html#afb993056a77c17a13ecb5e4be8526dc4",
"d3/d2e/compression_8cc.html#a79e586563008984b912926a967592418",
"d3/d44/classyaze_1_1editor_1_1OverworldEditor.html#a79db880a2423f4e80d35825043331e00",
"d3/d5a/classyaze_1_1zelda3_1_1OverworldEntrance.html#a7e0af673ad4fc7e0e942b8abbe53ca8d",
"d3/d6c/classyaze_1_1editor_1_1MessageEditor.html#a769d6f1ee603665e95a6b1864eced36b",
"d3/d8f/bps_8cc_source.html",
"d3/da7/structyaze_1_1gfx_1_1lc__lz2_1_1CompressionContext.html#a668144cb7dba4855ea7279948d29968b",
"d3/dc3/structyaze_1_1test_1_1HarnessTestExecution.html#aa0198b98fe02792dffb675bfacad36f8",
"d3/ded/classyaze_1_1emu_1_1Ppu.html#aa4b97e21c21284d6b5885f274ad7284f",
"d4/d0a/namespaceyaze_1_1test.html#a35d42c73f2ee66d915eadbc8c129a131",
"d4/d42/structyaze_1_1zelda3_1_1Zelda3Labels.html",
"d4/d70/classyaze_1_1test_1_1IntegratedTestSuite.html#af4d3cfd3b0e98ff105f46f444e5f5590",
"d4/dae/structyaze_1_1cli_1_1AssertionOutcome.html#a41fe595dc90b78ae3d7e5364b61dbbe9",
"d4/de6/classyaze_1_1gfx_1_1Arena.html#a700e6d369fd67535824bd9dba3606fc5",
"d5/d1f/namespaceyaze_1_1zelda3.html#a15147ef5d5b48248b07ffacdb989c1f1",
"d5/d1f/namespaceyaze_1_1zelda3.html#a71e42702970582dcdfd1e19263d45401",
"d5/d1f/namespaceyaze_1_1zelda3.html#afb47ec71853bc72be86c1b0c481a3496a58a15dcd8225b3b354d3696f1f713a77",
"d5/d66/structyaze_1_1gui_1_1TextBox.html#a542d43ce29b28763c8c5043598ef8d91",
"d5/da0/structyaze_1_1emu_1_1DspChannel.html#af37368558dd3474eca2cd12bc4e1efc1",
"d5/dcb/structyaze_1_1cli_1_1ToolSpecification.html",
"d6/d20/namespaceyaze_1_1emu.html#a22bf51ed91189695bf4e76bf6b85f836af3bc1bce25569418c897b2e9b215af92",
"d6/d2f/structyaze_1_1core_1_1FontConfig.html",
"d6/d44/canvas__performance__integration_8cc_source.html",
"d6/d9f/editor__integration__test_8h_source.html",
"d6/dc7/snes__palette__test_8cc.html",
"d6/df4/md_docs_2gfx__optimizations__complete.html#autotoc_md901",
"d7/d4d/classyaze_1_1gui_1_1canvas_1_1CanvasUsageTracker.html#ae1827cff80f86ef7fea679f85222a835",
"d7/d61/classyaze_1_1zelda3_1_1DungeonEditorSystem.html#aa6649b21ffa38945d2d65499dc646b57",
"d7/d6f/md_docs_2C1-changelog.html#autotoc_md368",
"d7/d9c/overworld_8h.html#ab88b0302f6b34e8bacba72d88a2ebc9a",
"d7/dce/structyaze_1_1cli_1_1ListTestsResult.html",
"d7/df6/classyaze_1_1zelda3_1_1Room.html#ab266a2ab277b3b59374e9b75e8654f4e",
"d7/dfb/classyaze_1_1editor_1_1OverworldEditorManager.html#a9d3d9337e591aa0128216fabbe142e2d",
"d8/d1e/classyaze_1_1zelda3_1_1RoomEntrance.html#adfc0772e2989b3527d388d0fa7122310",
"d8/d6e/classyaze_1_1gfx_1_1AtlasRenderer.html#a986c0ef7e724cfe62c88707caaecff3e",
"d8/d98/classyaze_1_1editor_1_1DungeonEditor.html#aceea5938ab7e6c50601edc00cfdfe9a9",
"d8/dd3/namespaceyaze_1_1cli_1_1agent.html#a139624813695b61e3beb469862cad3fa",
"d8/ddf/classyaze_1_1zelda3_1_1ObjectParser.html",
"d9/d47/message__editor_8cc.html#aef9df899003974391ac30afec5b1d4c5",
"d9/d97/structyaze_1_1test_1_1TestResult.html",
"d9/dc5/classyaze_1_1zelda3_1_1Overworld.html#a02bcff47c67812030c284952bbb60491",
"d9/dcc/classyaze_1_1zelda3_1_1OverworldMap.html#a20cde22bd5f4d8dd04269f7b3205ebd8",
"d9/dd4/structyaze_1_1emu_1_1Spc700_1_1Flags.html#a0b394943bb22ebf53fdf050477da8256",
"da/d11/md_docs_2E5-dungeon-object-system.html#autotoc_md762",
"da/d2c/classyaze_1_1gui_1_1Canvas.html#a9084927e7e6b80627d9f2b2f1d655b67",
"da/d3e/classyaze_1_1test_1_1TestManager.html#a2c642eb77f5ecd3a3867997f6c78f5e1",
"da/d5e/structyaze_1_1core_1_1YazeProject.html#a786bb2bd1ffba5f39ed8dcf67a78edd5",
"da/d92/structyaze_1_1zelda3_1_1SubtypeInfo.html#a38007237e22fe85ce0047c7fd68b15f1",
"da/dd2/classyaze_1_1core_1_1AsarWrapper.html#a532beea9f0fe141b922bd47af6100415",
"db/d07/dungeon__editor__system_8h.html",
"db/d56/classyaze_1_1gui_1_1BppConversionDialog.html#ae58a3c2254024fc4a654e474a07790e9",
"db/d82/classyaze_1_1editor_1_1Tile16Editor.html#a961501ef13d5a77ee8327defdddf6b3f",
"db/da4/classyaze_1_1emu_1_1MockMemory.html#a714ac6e2ebcaa35e3782025db8a4f028",
"db/dcc/classyaze_1_1editor_1_1ScreenEditor.html#af6ff0f96f40d97009ee73fca23443569",
"dc/d01/classyaze_1_1emu_1_1Dsp.html#a9309edf47dad0c794eee9fa0a6cc8fd7",
"dc/d31/classyaze_1_1editor_1_1GraphicsEditor.html#a1a5791825052e50494da22e67f17b803",
"dc/d46/namespaceyaze.html#a30653b94f9a3bf78bee51a203e141dd4",
"dc/d65/overworld__integration__test_8cc.html#aab17fb6bf62c4e58a881ba091a9e96af",
"dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md1152",
"dc/df4/classyaze_1_1emu_1_1Cpu.html#a2ef3e26830cc01fa5401e964e933547a",
"dc/df4/classyaze_1_1emu_1_1Cpu.html#abf4901ecb41882bfa98dbed207c26b24",
"dd/d12/classyaze_1_1editor_1_1EditorManager.html#a36209b8a5b17d613060c3d06f0359ad8",
"dd/d22/structyaze_1_1cli_1_1CommandInfo.html#ab6cc18b2355a365a03d21c720757b80f",
"dd/d4b/classyaze_1_1editor_1_1DungeonUsageTracker.html#a3e05726af978f10b27b249e343ff7f88",
"dd/d71/classyaze_1_1gui_1_1canvas_1_1CanvasInteractionHandler.html#a71f05374b5147658de96ea35ecb76dbd",
"dd/dc0/classyaze_1_1test_1_1WidgetDiscoveryService.html#ac598b81602cb451841bd144390de2784",
"dd/ddf/classyaze_1_1gui_1_1canvas_1_1CanvasContextMenu.html#adc02271a8b4ce751b13afe85e100b9b9",
"de/d0f/classyaze_1_1emu_1_1MemoryImpl.html#a2ffc5aed469902475cdca6763c4ff119",
"de/d52/test__recorder_8cc.html#a49a20ad191167cf33ea7e86b6ead55c4",
"de/d89/snes__palette_8cc.html#a74c13a0430b4a38eb7c964fef26d5663",
"de/dbf/icons_8h.html#a017db9e19654e3f887b332894905cc78",
"de/dbf/icons_8h.html#a1f33448153f99d7338175831c1775dc7",
"de/dbf/icons_8h.html#a3d56a843cb16c1e3151e36ce8208abf9",
"de/dbf/icons_8h.html#a5a81c17f2e5674030b1491f73cf91de5",
"de/dbf/icons_8h.html#a78ed46184ead7c13d5891bf75686d594",
"de/dbf/icons_8h.html#a97f454e65b3f7c907511c547bfbb131e",
"de/dbf/icons_8h.html#ab4b5350df8d9cfac35fcdc3ceebde81e",
"de/dbf/icons_8h.html#ad177ed7eebda34a0c8c010b2cc49541d",
"de/dbf/icons_8h.html#aeb469c31ebf054e3cd59a458f315cbb1",
"de/dd9/mock__rom_8h_source.html",
"de/dea/background__renderer_8cc.html#ae71449b1cc6e6250b91f539153a7a0d3",
"df/d1d/classyaze_1_1test_1_1UITestSuite.html#a33e15d83f7b1f2d6fd1c63211837f343",
"df/d42/md_docs_2dungeon__editing__implementation__plan.html#autotoc_md504",
"df/db9/classyaze_1_1cli_1_1ModernCLI.html#a6ecbacd9a8ca97bf245bec88d0b6832c",
"dir_2a6e74d3eed253325bbc2e3058622b3c.html",
"namespacemembers_vars_h.html"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';