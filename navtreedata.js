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
      [ "Contributing", "d9/d41/md_docs_202-build-instructions.html#autotoc_md65", [
        [ "Code Style", "d9/d41/md_docs_202-build-instructions.html#autotoc_md66", null ],
        [ "Error Handling", "d9/d41/md_docs_202-build-instructions.html#autotoc_md67", null ],
        [ "Pull Request Process", "d9/d41/md_docs_202-build-instructions.html#autotoc_md68", null ],
        [ "Commit Messages", "d9/d41/md_docs_202-build-instructions.html#autotoc_md69", null ]
      ] ]
    ] ],
    [ "Asar 65816 Assembler Integration", "d2/dd3/md_docs_203-asar-integration.html", [
      [ "Quick Examples", "d2/dd3/md_docs_203-asar-integration.html#autotoc_md71", [
        [ "Command Line", "d2/dd3/md_docs_203-asar-integration.html#autotoc_md72", null ],
        [ "C++ API", "d2/dd3/md_docs_203-asar-integration.html#autotoc_md73", null ]
      ] ],
      [ "Assembly Patch Examples", "d2/dd3/md_docs_203-asar-integration.html#autotoc_md74", [
        [ "Basic Hook", "d2/dd3/md_docs_203-asar-integration.html#autotoc_md75", null ],
        [ "Advanced Features", "d2/dd3/md_docs_203-asar-integration.html#autotoc_md76", null ]
      ] ],
      [ "API Reference", "d2/dd3/md_docs_203-asar-integration.html#autotoc_md77", [
        [ "AsarWrapper Class", "d2/dd3/md_docs_203-asar-integration.html#autotoc_md78", null ],
        [ "Data Structures", "d2/dd3/md_docs_203-asar-integration.html#autotoc_md79", null ]
      ] ],
      [ "Testing", "d2/dd3/md_docs_203-asar-integration.html#autotoc_md80", [
        [ "ROM-Dependent Tests", "d2/dd3/md_docs_203-asar-integration.html#autotoc_md81", null ]
      ] ],
      [ "Error Handling", "d2/dd3/md_docs_203-asar-integration.html#autotoc_md82", null ],
      [ "Development Workflow", "d2/dd3/md_docs_203-asar-integration.html#autotoc_md83", null ]
    ] ],
    [ "API Reference", "dd/d96/md_docs_204-api-reference.html", [
      [ "C API (<tt>incl/yaze.h</tt>, <tt>incl/zelda.h</tt>)", "dd/d96/md_docs_204-api-reference.html#autotoc_md85", [
        [ "Core Library Functions", "dd/d96/md_docs_204-api-reference.html#autotoc_md86", null ],
        [ "ROM Operations", "dd/d96/md_docs_204-api-reference.html#autotoc_md87", null ],
        [ "Graphics Operations", "dd/d96/md_docs_204-api-reference.html#autotoc_md88", null ],
        [ "Palette System", "dd/d96/md_docs_204-api-reference.html#autotoc_md89", null ],
        [ "Message System", "dd/d96/md_docs_204-api-reference.html#autotoc_md90", null ]
      ] ],
      [ "C++ API", "dd/d96/md_docs_204-api-reference.html#autotoc_md91", [
        [ "AsarWrapper (<tt>src/app/core/asar_wrapper.h</tt>)", "dd/d96/md_docs_204-api-reference.html#autotoc_md92", null ],
        [ "Data Structures", "dd/d96/md_docs_204-api-reference.html#autotoc_md93", [
          [ "ROM Version Support", "dd/d96/md_docs_204-api-reference.html#autotoc_md94", null ],
          [ "SNES Graphics", "dd/d96/md_docs_204-api-reference.html#autotoc_md95", null ],
          [ "Message System", "dd/d96/md_docs_204-api-reference.html#autotoc_md96", null ]
        ] ]
      ] ],
      [ "Error Handling", "dd/d96/md_docs_204-api-reference.html#autotoc_md97", [
        [ "Status Codes", "dd/d96/md_docs_204-api-reference.html#autotoc_md98", null ],
        [ "Error Handling Pattern", "dd/d96/md_docs_204-api-reference.html#autotoc_md99", null ]
      ] ],
      [ "Extension System", "dd/d96/md_docs_204-api-reference.html#autotoc_md100", [
        [ "Plugin Architecture", "dd/d96/md_docs_204-api-reference.html#autotoc_md101", null ],
        [ "Capability Flags", "dd/d96/md_docs_204-api-reference.html#autotoc_md102", null ]
      ] ],
      [ "Backward Compatibility", "dd/d96/md_docs_204-api-reference.html#autotoc_md103", null ]
    ] ],
    [ "Testing Guide", "d6/d10/md_docs_2A1-testing-guide.html", [
      [ "Test Categories", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md105", [
        [ "Stable Tests (STABLE)", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md106", null ],
        [ "ROM-Dependent Tests (ROM_DEPENDENT)", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md107", null ],
        [ "Experimental Tests (EXPERIMENTAL)", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md108", null ]
      ] ],
      [ "Command Line Usage", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md109", null ],
      [ "CMake Presets", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md110", null ],
      [ "Writing Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md111", [
        [ "Stable Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md112", null ],
        [ "ROM-Dependent Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md113", null ],
        [ "Experimental Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md114", null ]
      ] ],
      [ "CI/CD Integration", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md115", [
        [ "GitHub Actions", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md116", null ],
        [ "Test Execution Strategy", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md117", null ]
      ] ],
      [ "Test Development Guidelines", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md118", [
        [ "Writing Stable Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md119", null ],
        [ "Writing ROM-Dependent Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md120", null ],
        [ "Writing Experimental Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md121", null ]
      ] ],
      [ "E2E GUI Testing Framework", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md122", [
        [ "Architecture", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md123", null ],
        [ "Writing E2E Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md124", null ],
        [ "Running GUI Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md125", null ]
      ] ]
    ] ],
    [ "ZScream vs. yaze Overworld Implementation Analysis", "d9/d48/md_docs_2analysis_2overworld__implementation__analysis.html", [
      [ "Executive Summary", "d9/d48/md_docs_2analysis_2overworld__implementation__analysis.html#autotoc_md127", null ],
      [ "Key Findings", "d9/d48/md_docs_2analysis_2overworld__implementation__analysis.html#autotoc_md128", [
        [ "✅ <strong>Confirmed Correct Implementations</strong>", "d9/d48/md_docs_2analysis_2overworld__implementation__analysis.html#autotoc_md129", [
          [ "1. <strong>Tile32 & Tile16 Expansion Detection</strong>", "d9/d48/md_docs_2analysis_2overworld__implementation__analysis.html#autotoc_md130", null ],
          [ "2. <strong>Entrance & Hole Coordinate Calculation</strong>", "d9/d48/md_docs_2analysis_2overworld__implementation__analysis.html#autotoc_md131", null ],
          [ "3. <strong>Data Loading (Exits, Items, Sprites)</strong>", "d9/d48/md_docs_2analysis_2overworld__implementation__analysis.html#autotoc_md132", null ],
          [ "4. <strong>Map Decompression & Sizing</strong>", "d9/d48/md_docs_2analysis_2overworld__implementation__analysis.html#autotoc_md133", null ]
        ] ],
        [ "⚠️ <strong>Key Differences Found</strong>", "d9/d48/md_docs_2analysis_2overworld__implementation__analysis.html#autotoc_md134", null ],
        [ "🎯 <strong>Conclusion</strong>", "d9/d48/md_docs_2analysis_2overworld__implementation__analysis.html#autotoc_md135", null ]
      ] ]
    ] ],
    [ "Platform Compatibility Improvements", "d1/d30/md_docs_2B2-platform-compatibility.html", [
      [ "Native File Dialog Support", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md137", null ],
      [ "Cross-Platform Build Reliability", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md138", null ],
      [ "Build Configuration Options", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md139", [
        [ "Full Build (Development)", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md140", null ],
        [ "Minimal Build", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md141", null ]
      ] ],
      [ "Implementation Details", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md142", null ],
      [ "Platform-Specific Adaptations", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md143", [
        [ "Windows", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md144", null ],
        [ "macOS", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md145", null ],
        [ "Linux", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md146", null ]
      ] ]
    ] ],
    [ "Build Presets Guide", "d7/d51/md_docs_2B3-build-presets.html", [
      [ "🍎 macOS ARM64 Presets (Recommended for Apple Silicon)", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md148", [
        [ "For Development Work:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md149", null ],
        [ "For Distribution:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md150", null ]
      ] ],
      [ "🔧 Why This Fixes Architecture Errors", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md151", null ],
      [ "📋 Available Presets", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md152", null ],
      [ "🚀 Quick Start", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md153", null ],
      [ "🛠️ IDE Integration", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md154", [
        [ "VS Code with CMake Tools:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md155", null ],
        [ "CLion:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md156", null ],
        [ "Xcode:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md157", null ]
      ] ],
      [ "🔍 Troubleshooting", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md158", null ],
      [ "📝 Notes", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md159", null ]
    ] ],
    [ "Release Workflows Documentation", "d3/d49/md_docs_2B4-release-workflows.html", [
      [ "Overview", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md161", null ],
      [ "1. Release-Simplified (<tt>release-simplified.yml</tt>)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md163", [
        [ "Purpose", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md164", null ],
        [ "Key Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md165", null ],
        [ "Use Cases", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md166", null ],
        [ "Configuration", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md167", null ],
        [ "Platforms Supported", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md168", null ]
      ] ],
      [ "2. Release (<tt>release.yml</tt>)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md170", [
        [ "Purpose", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md171", null ],
        [ "Key Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md172", null ],
        [ "Use Cases", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md173", null ],
        [ "Configuration", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md174", null ],
        [ "Advantages over Simplified", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md175", null ]
      ] ],
      [ "3. Release-Complex (<tt>release-complex.yml</tt>)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md177", [
        [ "Purpose", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md178", null ],
        [ "Key Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md179", null ],
        [ "Use Cases", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md180", null ],
        [ "Fallback Mechanisms", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md181", [
          [ "vcpkg Fallback", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md182", null ],
          [ "Chocolatey Integration", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md183", null ],
          [ "Build Configuration Fallback", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md184", null ]
        ] ],
        [ "Advanced Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md185", null ]
      ] ],
      [ "Workflow Comparison Matrix", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md187", null ],
      [ "When to Use Each Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md189", [
        [ "Use Simplified When:", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md190", null ],
        [ "Use Release When:", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md191", null ],
        [ "Use Complex When:", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md192", null ]
      ] ],
      [ "Workflow Selection Guide", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md194", [
        [ "For Development Team", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md195", null ],
        [ "For Release Manager", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md196", null ],
        [ "For CI/CD Pipeline", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md197", null ]
      ] ],
      [ "Configuration Examples", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md199", [
        [ "Triggering a Release", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md200", [
          [ "Manual Release (All Workflows)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md201", null ],
          [ "Automatic Release (Tag Push)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md202", null ]
        ] ],
        [ "Customizing Release Notes", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md203", null ]
      ] ],
      [ "Troubleshooting", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md205", [
        [ "Common Issues", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md206", [
          [ "vcpkg Failures (Windows)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md207", null ],
          [ "Dependency Conflicts", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md208", null ],
          [ "Build Failures", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md209", null ]
        ] ],
        [ "Debug Information", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md210", [
          [ "Simplified Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md211", null ],
          [ "Release Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md212", null ],
          [ "Complex Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md213", null ]
        ] ]
      ] ],
      [ "Best Practices", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md215", [
        [ "Workflow Selection", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md216", null ],
        [ "Release Process", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md217", null ],
        [ "Maintenance", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md218", null ]
      ] ],
      [ "Future Improvements", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md220", [
        [ "Planned Enhancements", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md221", null ],
        [ "Integration Opportunities", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md222", null ]
      ] ]
    ] ],
    [ "Stability, Testability & Release Workflow Improvements", "d9/d6b/md_docs_2B5-stability-improvements.html", [
      [ "Recent Improvements (v0.3.2)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md225", [
        [ "Windows Platform Stability", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md226", [
          [ "Stack Size Optimization", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md227", null ],
          [ "Development Utility Isolation", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md228", null ]
        ] ],
        [ "Graphics System Stability", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md229", [
          [ "Segmentation Fault Resolution", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md230", null ],
          [ "Comprehensive Bounds Checking", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md231", null ]
        ] ],
        [ "Build System Improvements", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md232", [
          [ "Modern Windows Workflow", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md233", null ],
          [ "Enhanced CI/CD Reliability", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md234", null ]
        ] ]
      ] ],
      [ "Recommended Optimizations", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md235", [
        [ "High Priority", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md236", [
          [ "1. Lazy Graphics Loading", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md237", null ],
          [ "2. Heap-Based Large Allocations", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md238", null ],
          [ "3. Streaming ROM Assets", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md239", null ]
        ] ],
        [ "Medium Priority", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md240", [
          [ "4. Enhanced Test Isolation", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md241", null ],
          [ "5. Dependency Caching Optimization", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md242", null ],
          [ "6. Memory Pool for Graphics", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md243", null ]
        ] ],
        [ "Low Priority", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md244", [
          [ "7. Build Time Optimization", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md245", null ],
          [ "8. Release Workflow Simplification", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md246", null ]
        ] ]
      ] ],
      [ "Testing Improvements", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md247", [
        [ "Current State", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md248", null ],
        [ "Recommendations", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md249", [
          [ "1. Visual Regression Testing", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md250", null ],
          [ "2. Performance Benchmarks", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md251", null ],
          [ "3. Fuzz Testing", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md252", null ]
        ] ]
      ] ],
      [ "Metrics & Monitoring", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md253", [
        [ "Current Measurements", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md254", null ],
        [ "Target Improvements", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md255", null ]
      ] ],
      [ "Action Items", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md256", [
        [ "Immediate (v0.3.2)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md257", null ],
        [ "Short Term (v0.3.3)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md258", null ],
        [ "Medium Term (v0.4.0)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md259", null ],
        [ "Long Term (v0.5.0+)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md260", null ]
      ] ],
      [ "Conclusion", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md261", null ]
    ] ],
    [ "Build Modularization Plan for Yaze", "d7/daf/md_docs_2build__modularization__plan.html", [
      [ "Executive Summary", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md263", null ],
      [ "Current Problems", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md264", null ],
      [ "Proposed Architecture", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md265", [
        [ "Library Hierarchy", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md266", null ]
      ] ],
      [ "Implementation Guide", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md267", [
        [ "Quick Start", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md268", [
          [ "Option 1: Enable Modular Build (Recommended)", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md269", null ],
          [ "Option 2: Keep Existing Build (Default)", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md270", null ]
        ] ],
        [ "Implementation Steps", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md271", [
          [ "Step 1: Add Build Option", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md272", null ],
          [ "Step 2: Update src/CMakeLists.txt", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md273", null ],
          [ "Step 3: Update yaze Application Linking", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md274", null ],
          [ "Step 4: Update Test Executable", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md275", null ]
        ] ]
      ] ],
      [ "Expected Performance Improvements", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md276", [
        [ "Build Time Reductions", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md277", null ],
        [ "CI/CD Benefits", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md278", null ]
      ] ],
      [ "Rollback Procedure", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md279", null ]
    ] ],
    [ "Changelog", "d7/d6f/md_docs_2C1-changelog.html", [
      [ "0.3.2 (December 2025) - In Development", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md281", [
        [ "Tile16 Editor Improvements (In Progress)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md282", null ],
        [ "Graphics System Optimizations", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md283", null ],
        [ "Windows Platform Stability", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md284", null ],
        [ "Memory Safety & Stability", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md285", null ],
        [ "Testing & CI/CD Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md286", null ],
        [ "Future Optimizations (Planned)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md287", null ],
        [ "Technical Notes", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md288", null ]
      ] ],
      [ "0.3.1 (September 2025)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md289", [
        [ "Major Features", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md290", null ],
        [ "Tile16 Editor Enhancements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md291", null ],
        [ "ZSCustomOverworld v3 Implementation", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md292", null ],
        [ "Technical Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md293", null ],
        [ "User Interface", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md294", null ],
        [ "Bug Fixes", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md295", null ],
        [ "ZScream Compatibility Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md296", null ]
      ] ],
      [ "0.3.0 (September 2025)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md297", [
        [ "Major Features", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md298", null ],
        [ "User Interface & Theming", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md299", null ],
        [ "Enhancements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md300", null ],
        [ "Technical Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md301", null ],
        [ "Bug Fixes", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md302", null ]
      ] ],
      [ "0.2.2 (December 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md303", null ],
      [ "0.2.1 (August 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md304", null ],
      [ "0.2.0 (July 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md305", null ],
      [ "0.1.0 (May 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md306", null ],
      [ "0.0.9 (April 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md307", null ],
      [ "0.0.8 (February 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md308", null ],
      [ "0.0.7 (January 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md309", null ],
      [ "0.0.6 (November 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md310", null ],
      [ "0.0.5 (November 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md311", null ],
      [ "0.0.4 (November 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md312", null ],
      [ "0.0.3 (October 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md313", null ]
    ] ],
    [ "Canvas System - Comprehensive Guide", "d2/db2/md_docs_2CANVAS__GUIDE.html", [
      [ "Overview", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md315", null ],
      [ "Core Concepts", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md316", [
        [ "Canvas Structure", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md317", null ],
        [ "Coordinate Systems", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md318", null ]
      ] ],
      [ "Usage Patterns", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md319", [
        [ "Pattern 1: Basic Bitmap Display", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md320", null ],
        [ "Pattern 2: Modern Begin/End", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md321", null ],
        [ "Pattern 3: RAII ScopedCanvas", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md322", null ]
      ] ],
      [ "Feature: Tile Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md323", [
        [ "Single Tile Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md324", null ],
        [ "Tilemap Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md325", null ],
        [ "Color Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md326", null ]
      ] ],
      [ "Feature: Tile Selection", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md327", [
        [ "Single Tile Selection", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md328", null ],
        [ "Multi-Tile Rectangle Selection", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md329", null ],
        [ "Rectangle Drag & Paint", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md330", null ]
      ] ],
      [ "Feature: Custom Overlays", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md331", [
        [ "Manual Points Manipulation", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md332", null ]
      ] ],
      [ "Feature: Large Map Support", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md333", [
        [ "Map Types", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md334", null ],
        [ "Boundary Clamping", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md335", null ],
        [ "Custom Map Sizes", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md336", null ]
      ] ],
      [ "Feature: Context Menu", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md337", [
        [ "Adding Custom Items", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md338", null ],
        [ "Overworld Editor Example", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md339", null ]
      ] ],
      [ "Feature: Scratch Space (In Progress)", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md340", null ],
      [ "Common Workflows", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md341", [
        [ "Workflow 1: Overworld Tile Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md342", null ],
        [ "Workflow 2: Tile16 Blockset Selection", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md343", null ],
        [ "Workflow 3: Graphics Sheet Display", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md344", null ]
      ] ],
      [ "Configuration", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md345", [
        [ "Grid Settings", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md346", null ],
        [ "Scale Settings", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md347", null ],
        [ "Interaction Settings", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md348", null ],
        [ "Large Map Settings", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md349", null ]
      ] ],
      [ "Known Issues", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md350", [
        [ "⚠️ Rectangle Selection Wrapping", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md351", null ]
      ] ],
      [ "API Reference", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md352", [
        [ "Drawing Methods", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md353", null ],
        [ "State Accessors", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md354", null ],
        [ "Configuration", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md355", null ]
      ] ],
      [ "Implementation Notes", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md356", [
        [ "Points Management", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md357", null ],
        [ "Selection State", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md358", null ],
        [ "Overworld Rectangle Painting Flow", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md359", null ]
      ] ],
      [ "Best Practices", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md360", [
        [ "DO ✅", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md361", null ],
        [ "DON'T ❌", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md362", null ]
      ] ],
      [ "Troubleshooting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md363", [
        [ "Issue: Rectangle wraps at boundaries", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md364", null ],
        [ "Issue: Painting in wrong location", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md365", null ],
        [ "Issue: Array index out of bounds", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md366", null ],
        [ "Issue: Forgot to call End()", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md367", null ]
      ] ],
      [ "Future: Scratch Space", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md368", null ],
      [ "Summary", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md369", null ]
    ] ],
    [ "Roadmap", "d6/db4/md_docs_2D1-roadmap.html", [
      [ "0.4.X (Next Major Release)", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md371", [
        [ "Core Features", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md372", null ],
        [ "Technical Improvements", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md373", null ]
      ] ],
      [ "0.5.X", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md374", [
        [ "Advanced Features", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md375", null ]
      ] ],
      [ "0.6.X", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md376", [
        [ "Platform & Integration", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md377", null ]
      ] ],
      [ "0.7.X", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md378", [
        [ "Performance & Polish", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md379", null ]
      ] ],
      [ "0.8.X", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md380", [
        [ "Beta Preparation", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md381", null ]
      ] ],
      [ "1.0.0", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md382", [
        [ "Stable Release", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md383", null ]
      ] ],
      [ "Current Focus Areas", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md384", [
        [ "Immediate Priorities (v0.4.X)", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md385", null ],
        [ "Long-term Vision", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md386", null ]
      ] ]
    ] ],
    [ "Yaze Dungeon Editor: Master Guide", "d5/dde/md_docs_2dungeon__editor__master__guide.html", [
      [ "1. Current Status & Known Issues", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md388", [
        [ "Next Steps", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md389", null ]
      ] ],
      [ "2. Architecture", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md390", [
        [ "Core Components (Backend)", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md391", null ],
        [ "UI Components (Frontend)", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md392", null ]
      ] ],
      [ "3. ROM Internals & Data Structures", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md393", [
        [ "Object Encoding", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md394", null ],
        [ "Object Types & Examples", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md395", null ],
        [ "Core Data Tables in ROM", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md396", null ]
      ] ],
      [ "4. User Interface and Usage", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md397", [
        [ "Coordinate System", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md398", null ],
        [ "Usage Examples", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md399", null ]
      ] ],
      [ "5. Testing", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md400", [
        [ "How to Run Tests", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md401", null ]
      ] ],
      [ "6. Dungeon Object Reference Tables", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md402", [
        [ "Type 1 Object Reference Table", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md403", null ],
        [ "Type 2 Object Reference Table", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md404", null ],
        [ "Type 3 Object Reference Table", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md405", null ]
      ] ]
    ] ],
    [ "Asm Style Guide", "d7/d9a/md_docs_2E1-asm-style-guide.html", [
      [ "Table of Contents", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md407", null ],
      [ "File Structure", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md408", null ],
      [ "Labels and Symbols", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md409", null ],
      [ "Comments", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md410", null ],
      [ "Instructions", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md411", null ],
      [ "Macros", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md412", null ],
      [ "Loops and Branching", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md413", null ],
      [ "Data Structures", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md414", null ],
      [ "Code Organization", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md415", null ],
      [ "Custom Code", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md416", null ]
    ] ],
    [ "Dungeon Editor Design Plan", "dc/d82/md_docs_2E3-dungeon-editor-design.html", [
      [ "Overview", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md418", null ],
      [ "Architecture Overview", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md419", [
        [ "Core Components", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md420", null ],
        [ "File Structure", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md421", null ]
      ] ],
      [ "Component Responsibilities", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md422", [
        [ "DungeonEditor (Main Orchestrator)", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md423", null ],
        [ "DungeonRoomSelector", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md424", null ],
        [ "DungeonCanvasViewer", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md425", null ],
        [ "DungeonObjectSelector", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md426", null ]
      ] ],
      [ "Data Flow", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md427", [
        [ "Initialization Flow", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md428", null ],
        [ "Runtime Data Flow", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md429", null ],
        [ "Key Data Structures", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md430", null ]
      ] ],
      [ "Integration Patterns", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md431", [
        [ "Component Communication", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md432", null ],
        [ "ROM Data Management", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md433", null ],
        [ "State Synchronization", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md434", null ]
      ] ],
      [ "UI Layout Architecture", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md435", [
        [ "3-Column Layout", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md436", null ],
        [ "Component Internal Layout", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md437", null ]
      ] ],
      [ "Coordinate System", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md438", [
        [ "Room Coordinates vs Canvas Coordinates", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md439", null ],
        [ "Bounds Checking", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md440", null ]
      ] ],
      [ "Error Handling & Validation", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md441", [
        [ "ROM Validation", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md442", null ],
        [ "Bounds Validation", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md443", null ]
      ] ],
      [ "Performance Considerations", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md444", [
        [ "Caching Strategy", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md445", null ],
        [ "Rendering Optimization", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md446", null ]
      ] ],
      [ "Testing Strategy", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md447", [
        [ "Integration Tests", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md448", null ],
        [ "Test Categories", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md449", null ]
      ] ],
      [ "Future Development Guidelines", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md450", [
        [ "Adding New Features", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md451", null ],
        [ "Component Extension Patterns", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md452", null ],
        [ "Data Flow Extension", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md453", null ],
        [ "UI Layout Extension", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md454", null ]
      ] ],
      [ "Common Pitfalls & Solutions", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md455", [
        [ "Memory Management", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md456", null ],
        [ "Coordinate System", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md457", null ],
        [ "State Synchronization", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md458", null ],
        [ "Performance Issues", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md459", null ]
      ] ],
      [ "Debugging Tools", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md460", [
        [ "Debug Popup", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md461", null ],
        [ "Logging", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md462", null ]
      ] ],
      [ "Build Integration", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md463", [
        [ "CMake Configuration", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md464", null ],
        [ "Dependencies", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md465", null ]
      ] ],
      [ "Conclusion", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md466", null ]
    ] ],
    [ "Tile16 Editor Palette System", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html", [
      [ "Executive Summary", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md468", null ],
      [ "Problem Analysis", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md469", [
        [ "Critical Issues Identified", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md470", null ]
      ] ],
      [ "Solution Architecture", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md471", [
        [ "Core Design Principles", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md472", null ],
        [ "256-Color Overworld Palette Structure", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md473", null ],
        [ "Sheet-to-Palette Mapping", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md474", null ],
        [ "Palette Button Mapping", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md475", null ]
      ] ],
      [ "Implementation Details", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md476", [
        [ "1. Fixed SetPaletteWithTransparent Method", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md477", null ],
        [ "2. Corrected Tile16 Editor Palette System", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md478", null ],
        [ "3. Palette Coordination Flow", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md479", null ]
      ] ],
      [ "UI/UX Refactoring", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md480", [
        [ "New Three-Column Layout", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md481", null ],
        [ "Canvas Context Menu Fixes", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md482", null ],
        [ "Dynamic Zoom Controls", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md483", null ]
      ] ],
      [ "Testing Protocol", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md484", [
        [ "Crash Prevention Testing", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md485", null ],
        [ "Color Alignment Testing", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md486", null ],
        [ "UI/UX Testing", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md487", null ]
      ] ],
      [ "Error Handling", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md488", [
        [ "Bounds Checking", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md489", null ],
        [ "Fallback Mechanisms", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md490", null ]
      ] ],
      [ "Debug Information Display", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md491", null ],
      [ "Known Issues and Ongoing Work", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md492", [
        [ "Completed Items ✅", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md493", null ],
        [ "Active Issues ⚠️", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md494", null ],
        [ "Current Status Summary", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md495", null ],
        [ "Future Enhancements", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md496", null ]
      ] ],
      [ "Maintenance Notes", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md497", null ],
      [ "Next Steps", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md499", [
        [ "Immediate Priorities", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md500", null ],
        [ "Investigation Areas", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md501", null ]
      ] ]
    ] ],
    [ "Overworld Loading Guide", "dc/d12/md_docs_2F1-overworld-loading.html", [
      [ "Table of Contents", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md504", null ],
      [ "Overview", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md505", null ],
      [ "ROM Types and Versions", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md506", [
        [ "Version Detection", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md507", null ],
        [ "Feature Support by Version", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md508", null ]
      ] ],
      [ "Overworld Map Structure", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md509", [
        [ "Core Properties", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md510", null ]
      ] ],
      [ "Overlays and Special Area Maps", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md511", [
        [ "Understanding Overlays", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md512", null ],
        [ "Special Area Maps (0x80-0x9F)", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md513", null ],
        [ "Overlay ID Mappings", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md514", null ],
        [ "Drawing Order", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md515", null ],
        [ "Vanilla Overlay Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md516", null ],
        [ "Special Area Graphics Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md517", null ]
      ] ],
      [ "Loading Process", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md518", [
        [ "1. Version Detection", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md519", null ],
        [ "2. Map Initialization", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md520", null ],
        [ "3. Property Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md521", [
          [ "Vanilla ROMs (asm_version == 0xFF)", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md522", null ],
          [ "ZSCustomOverworld v2/v3", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md523", null ]
        ] ],
        [ "4. Custom Data Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md524", null ]
      ] ],
      [ "ZScream Implementation", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md525", [
        [ "OverworldMap Constructor", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md526", null ],
        [ "Key Methods", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md527", null ]
      ] ],
      [ "Yaze Implementation", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md528", [
        [ "OverworldMap Constructor", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md529", null ],
        [ "Key Methods", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md530", null ],
        [ "Current Status", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md531", null ]
      ] ],
      [ "Key Differences", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md532", [
        [ "1. Language and Architecture", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md533", null ],
        [ "2. Data Structures", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md534", null ],
        [ "3. Error Handling", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md535", null ],
        [ "4. Graphics Processing", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md536", null ]
      ] ],
      [ "Common Issues and Solutions", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md537", [
        [ "1. Version Detection Issues", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md538", null ],
        [ "2. Palette Loading Errors", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md539", null ],
        [ "3. Graphics Not Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md540", null ],
        [ "4. Overlay Issues", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md541", null ],
        [ "5. Large Map Problems", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md542", null ],
        [ "6. Special Area Graphics Issues", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md543", null ]
      ] ],
      [ "Best Practices", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md544", [
        [ "1. Version-Specific Code", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md545", null ],
        [ "2. Error Handling", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md546", null ],
        [ "3. Memory Management", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md547", null ],
        [ "4. Thread Safety", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md548", null ]
      ] ],
      [ "Conclusion", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md549", null ]
    ] ],
    [ "yaze Documentation", "d3/d4c/md_docs_2index.html", [
      [ "Quick Start", "d3/d4c/md_docs_2index.html#autotoc_md551", null ],
      [ "Development", "d3/d4c/md_docs_2index.html#autotoc_md552", null ],
      [ "Technical Documentation", "d3/d4c/md_docs_2index.html#autotoc_md553", [
        [ "Assembly & Code", "d3/d4c/md_docs_2index.html#autotoc_md554", null ],
        [ "Editor Systems", "d3/d4c/md_docs_2index.html#autotoc_md555", null ],
        [ "Overworld System", "d3/d4c/md_docs_2index.html#autotoc_md556", null ]
      ] ],
      [ "Key Features", "d3/d4c/md_docs_2index.html#autotoc_md557", null ]
    ] ],
    [ "Ollama Integration Status - Updated# Ollama Integration Status", "da/d40/md_docs_2ollama__integration__status.html", [
      [ "✅ Completed## ✅ Completed", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md560", [
        [ "Infrastructure### Flag Parsing", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md561", null ],
        [ "Current Issue: Empty Tool Results### Tool System", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md562", null ]
      ] ],
      [ "🎨 New Features Added  - OR doesn't provide a <tt>text_response</tt> field in the JSON", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md563", [
        [ "Verbose Mode**Solution Needed**: Update system prompt to include explicit instructions like:", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md564", null ],
        [ "Priority 2: Refine Prompts (MEDIUM)- ✅ Model loads (qwen2.5-coder:7b)", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md565", null ],
        [ "Priority 3: Documentation (LOW)```", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md566", null ]
      ] ],
      [ "🧪 Testing Commands### Priority 1: Fix Tool Calling Loop (High Priority)", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md567", null ],
      [ "📊 Performance   - codellama:7b", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md568", null ],
      [ "🎯 Success Criteria", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md569", [
        [ "Priority 3: Documentation (Low Priority)", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md570", null ]
      ] ],
      [ "🐛 Known Issues", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md571", null ],
      [ "💡 Recommendations", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md572", [
        [ "For Users", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md573", null ],
        [ "For Developers", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md574", null ]
      ] ],
      [ "📝 Related Files", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md575", null ],
      [ "🎯 Success Criteria", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md576", [
        [ "Minimum Viable", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md577", null ],
        [ "Full Success", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md578", null ]
      ] ]
    ] ],
    [ "yaze Overworld Testing Guide", "de/d8a/md_docs_2overworld__testing__guide.html", [
      [ "Overview", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md581", null ],
      [ "Test Architecture", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md582", [
        [ "1. Golden Data System", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md583", null ],
        [ "2. Test Categories", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md584", [
          [ "Unit Tests (<tt>test/unit/zelda3/</tt>)", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md585", null ],
          [ "Integration Tests (<tt>test/e2e/</tt>)", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md586", null ],
          [ "Golden Data Tools", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md587", null ]
        ] ]
      ] ],
      [ "Quick Start", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md588", [
        [ "Prerequisites", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md589", null ],
        [ "Running Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md590", [
          [ "Basic Test Run", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md591", null ],
          [ "Selective Test Execution", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md592", null ]
        ] ]
      ] ],
      [ "Test Components", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md593", [
        [ "1. Golden Data Extractor", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md594", null ],
        [ "2. Integration Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md595", null ],
        [ "3. End-to-End Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md596", null ]
      ] ],
      [ "Test Validation Points", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md597", [
        [ "1. ZScream Compatibility", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md598", null ],
        [ "2. ROM State Validation", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md599", null ],
        [ "3. Performance and Stability", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md600", null ]
      ] ],
      [ "Environment Variables", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md601", [
        [ "Test Configuration", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md602", null ],
        [ "Build Configuration", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md603", null ]
      ] ],
      [ "Test Reports", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md604", [
        [ "Generated Reports", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md605", null ],
        [ "Report Location", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md606", null ]
      ] ],
      [ "Troubleshooting", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md607", [
        [ "Common Issues", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md608", [
          [ "1. ROM Not Found", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md609", null ],
          [ "2. Build Failures", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md610", null ],
          [ "3. Test Failures", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md611", null ]
        ] ],
        [ "Debug Mode", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md612", null ]
      ] ],
      [ "Advanced Usage", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md613", [
        [ "Custom Test Scenarios", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md614", [
          [ "1. Testing Custom ROMs", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md615", null ],
          [ "2. Regression Testing", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md616", null ],
          [ "3. Performance Testing", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md617", null ]
        ] ]
      ] ],
      [ "Integration with CI/CD", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md618", [
        [ "GitHub Actions Example", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md619", null ]
      ] ],
      [ "Contributing", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md620", [
        [ "Adding New Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md621", null ],
        [ "Test Guidelines", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md622", null ],
        [ "Example Test Structure", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md623", null ]
      ] ],
      [ "Conclusion", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md624", null ]
    ] ],
    [ "z3ed: AI-Powered CLI for YAZE", "d3/d63/md_docs_2z3ed_2README.html", [
      [ "1. Overview", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md626", [
        [ "Core Capabilities", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md627", null ]
      ] ],
      [ "2. Quick Start", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md628", [
        [ "Build", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md629", null ],
        [ "AI Setup", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md630", null ],
        [ "Example Commands", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md631", null ]
      ] ],
      [ "3. Architecture", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md632", [
        [ "System Components Diagram", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md633", null ]
      ] ],
      [ "4. Agentic & Generative Workflow (MCP)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md634", null ],
      [ "5. Command Reference", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md635", [
        [ "Agent Commands", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md636", null ],
        [ "Resource Commands", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md637", null ]
      ] ],
      [ "6. Chat Modes", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md638", [
        [ "FTXUI Chat (<tt>agent chat</tt>)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md639", null ],
        [ "Simple Chat (<tt>agent simple-chat</tt>)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md640", null ],
        [ "GUI Chat Widget (Editor Integration Preview)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md641", null ]
      ] ],
      [ "7. AI Provider Configuration", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md642", null ],
      [ "8. Roadmap & Implementation Status", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md643", [
        [ "✅ Completed", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md644", null ],
        [ "🚧 Active & Next Steps", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md645", null ]
      ] ],
      [ "9. Troubleshooting", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md646", null ]
    ] ],
    [ "yaze - Yet Another Zelda3 Editor", "d0/d30/md_README.html", [
      [ "Version 0.3.2 - Release", "d0/d30/md_README.html#autotoc_md648", null ],
      [ "Quick Start", "d0/d30/md_README.html#autotoc_md653", [
        [ "Build", "d0/d30/md_README.html#autotoc_md654", null ],
        [ "Applications", "d0/d30/md_README.html#autotoc_md655", null ]
      ] ],
      [ "Usage", "d0/d30/md_README.html#autotoc_md656", [
        [ "GUI Editor", "d0/d30/md_README.html#autotoc_md657", null ],
        [ "Command Line Tool", "d0/d30/md_README.html#autotoc_md658", null ],
        [ "C++ API", "d0/d30/md_README.html#autotoc_md659", null ]
      ] ],
      [ "Documentation", "d0/d30/md_README.html#autotoc_md660", null ],
      [ "Supported Platforms", "d0/d30/md_README.html#autotoc_md661", null ],
      [ "ROM Compatibility", "d0/d30/md_README.html#autotoc_md662", null ],
      [ "Contributing", "d0/d30/md_README.html#autotoc_md663", null ],
      [ "License", "d0/d30/md_README.html#autotoc_md664", null ],
      [ "🙏 Acknowledgments", "d0/d30/md_README.html#autotoc_md665", null ],
      [ "📸 Screenshots", "d0/d30/md_README.html#autotoc_md666", null ]
    ] ],
    [ "yaze Build Scripts", "de/d82/md_scripts_2README.html", [
      [ "Windows Scripts", "de/d82/md_scripts_2README.html#autotoc_md669", [
        [ "vcpkg Setup (Optional)", "de/d82/md_scripts_2README.html#autotoc_md670", null ]
      ] ],
      [ "Windows Build Workflow", "de/d82/md_scripts_2README.html#autotoc_md671", [
        [ "Recommended: Visual Studio CMake Mode", "de/d82/md_scripts_2README.html#autotoc_md672", null ],
        [ "Command Line Build", "de/d82/md_scripts_2README.html#autotoc_md673", null ],
        [ "Compiler Notes", "de/d82/md_scripts_2README.html#autotoc_md674", null ]
      ] ],
      [ "Quick Start (Windows)", "de/d82/md_scripts_2README.html#autotoc_md675", [
        [ "Option 1: Visual Studio (Recommended)", "de/d82/md_scripts_2README.html#autotoc_md676", null ],
        [ "Option 2: Command Line", "de/d82/md_scripts_2README.html#autotoc_md677", null ],
        [ "Option 3: With vcpkg (Optional)", "de/d82/md_scripts_2README.html#autotoc_md678", null ]
      ] ],
      [ "Troubleshooting", "de/d82/md_scripts_2README.html#autotoc_md679", [
        [ "Common Issues", "de/d82/md_scripts_2README.html#autotoc_md680", null ],
        [ "Getting Help", "de/d82/md_scripts_2README.html#autotoc_md681", null ]
      ] ],
      [ "Other Scripts", "de/d82/md_scripts_2README.html#autotoc_md682", null ]
    ] ],
    [ "YAZE Build Environment Verification Scripts", "dc/db4/md_scripts_2README__VERIFICATION.html", [
      [ "Quick Start", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md684", [
        [ "Verify Build Environment", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md685", null ]
      ] ],
      [ "Scripts Overview", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md686", [
        [ "<tt>verify-build-environment.ps1</tt> / <tt>.sh</tt>", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md687", [
          [ "Usage", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md688", null ],
          [ "Exit Codes", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md689", null ]
        ] ]
      ] ],
      [ "Common Workflows", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md690", [
        [ "First-Time Setup", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md691", null ],
        [ "After Pulling Changes", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md692", null ],
        [ "Troubleshooting Build Issues", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md693", null ],
        [ "Before Opening Pull Request", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md694", null ]
      ] ],
      [ "Automatic Fixes", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md695", [
        [ "Always Auto-Fixed (No Confirmation Required)", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md696", null ],
        [ "Fixed with <tt>-FixIssues</tt> / <tt>--fix</tt> Flag", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md697", null ],
        [ "Fixed with <tt>-CleanCache</tt> / <tt>--clean</tt> Flag", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md698", null ],
        [ "Optional Verbose Tests", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md699", null ]
      ] ],
      [ "Integration with Visual Studio", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md700", null ],
      [ "What Gets Checked", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md701", [
        [ "CMake (Required)", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md702", null ],
        [ "Git (Required)", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md703", null ],
        [ "Compilers (Required)", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md704", null ],
        [ "Platform Dependencies", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md705", null ],
        [ "CMake Cache", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md706", null ],
        [ "Dependencies", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md707", null ]
      ] ],
      [ "Automatic Fixes", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md708", null ],
      [ "CI/CD Integration", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md709", null ],
      [ "Troubleshooting", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md710", [
        [ "Script Reports \"CMake Not Found\"", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md711", null ],
        [ "\"Git Submodules Missing\"", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md712", null ],
        [ "\"CMake Cache Too Old\"", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md713", null ],
        [ "\"Visual Studio Not Found\" (Windows)", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md714", null ],
        [ "Script Fails on Network Issues (gRPC)", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md715", null ]
      ] ],
      [ "See Also", "dc/db4/md_scripts_2README__VERIFICATION.html#autotoc_md716", null ]
    ] ],
    [ "yaze Test Suite", "d0/d46/md_test_2README.html", [
      [ "Directory Structure", "d0/d46/md_test_2README.html#autotoc_md718", null ],
      [ "Test Categories", "d0/d46/md_test_2README.html#autotoc_md719", [
        [ "Unit Tests (<tt>unit/</tt>)", "d0/d46/md_test_2README.html#autotoc_md720", null ],
        [ "Integration Tests (<tt>integration/</tt>)", "d0/d46/md_test_2README.html#autotoc_md721", null ],
        [ "End-to-End Tests (<tt>e2e/</tt>)", "d0/d46/md_test_2README.html#autotoc_md722", null ]
      ] ],
      [ "Enhanced Test Runner", "d0/d46/md_test_2README.html#autotoc_md723", [
        [ "Usage Examples", "d0/d46/md_test_2README.html#autotoc_md724", null ],
        [ "Test Modes", "d0/d46/md_test_2README.html#autotoc_md725", null ],
        [ "Options", "d0/d46/md_test_2README.html#autotoc_md726", null ]
      ] ],
      [ "E2E ROM Testing", "d0/d46/md_test_2README.html#autotoc_md727", [
        [ "Test Cases", "d0/d46/md_test_2README.html#autotoc_md728", null ]
      ] ],
      [ "ZSCustomOverworld Upgrade Testing", "d0/d46/md_test_2README.html#autotoc_md729", [
        [ "Supported Upgrades", "d0/d46/md_test_2README.html#autotoc_md730", null ],
        [ "Test Cases", "d0/d46/md_test_2README.html#autotoc_md731", null ],
        [ "Version-Specific Features", "d0/d46/md_test_2README.html#autotoc_md732", [
          [ "Vanilla", "d0/d46/md_test_2README.html#autotoc_md733", null ],
          [ "v2", "d0/d46/md_test_2README.html#autotoc_md734", null ],
          [ "v3", "d0/d46/md_test_2README.html#autotoc_md735", null ]
        ] ]
      ] ],
      [ "Environment Variables", "d0/d46/md_test_2README.html#autotoc_md736", null ],
      [ "CI/CD Integration", "d0/d46/md_test_2README.html#autotoc_md737", null ],
      [ "Deprecated Tests", "d0/d46/md_test_2README.html#autotoc_md738", null ],
      [ "Best Practices", "d0/d46/md_test_2README.html#autotoc_md739", null ],
      [ "AI Agent Testing", "d0/d46/md_test_2README.html#autotoc_md740", null ]
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
"d0/d30/md_README.html#autotoc_md656",
"d0/d78/structyaze__editor__context.html#a50323bf40f414a711612e3dbf2c2ff88",
"d0/dcb/structyaze_1_1cli_1_1overworld_1_1WarpQuery.html#ad4f2c838f2c5f1f400765f3808712579",
"d1/d12/test__common_8h.html#a25c8f8af1b95d588e6b8897c8ab4eb48",
"d1/d36/dungeon__object__renderer__integration__test_8cc.html#a3a3b68cab34162c9baae273d0d82cb4e",
"d1/d47/structyaze_1_1core_1_1ProjectManager_1_1ProjectTemplate.html#a728609012fb8e98c36ff27b4568725f3",
"d1/d71/classyaze_1_1util_1_1FlagParser.html#ae87e95c42728b7d9e5b30b79aa688d58",
"d1/daf/compression_8h.html#aaf7eadbb2c70ee68f7f5ad79472b017c",
"d1/def/structyaze_1_1core_1_1WorkspaceSettings.html",
"d2/d07/classyaze_1_1emu_1_1Spc700.html#a8b3ff8197f0bbaec2b26c75c3f7fb104",
"d2/d2a/classyaze_1_1cli_1_1AIService.html#a5f7442759278c708114d6d188eba912b",
"d2/d5e/classyaze_1_1emu_1_1Memory.html#a8822e658b05dd7df7edc9c36d2038310",
"d2/dba/dungeon__map_8h.html#a13bd041f5d57b7f9c6a99d9ce4bcc729",
"d2/de7/namespaceyaze_1_1emu_1_1anonymous__namespace_02snes_8cc_03.html#a34f0bcbc0fe70ab704bb3d117f3a73b1",
"d2/dfe/structyaze_1_1gui_1_1EnhancedTheme.html#a6c7dbeb4d24aff833fa13e81876464d1",
"d3/d15/classyaze_1_1emu_1_1Snes.html#aee7a85cbf294342b9110a6708db17200",
"d3/d2e/compression_8cc.html#a47fd97f5c33aa1279f26a7c0ad7c1e3b",
"d3/d44/classyaze_1_1editor_1_1OverworldEditor.html#a7651ac8fd6e5bcb126af837d95465f8ead74c735013cdd2b883e25ccca502a2e9",
"d3/d5a/classyaze_1_1zelda3_1_1OverworldEntrance.html",
"d3/d6c/classyaze_1_1editor_1_1MessageEditor.html#accbb3e03d3bfac92d85de0ad8044b35b",
"d3/d8f/structyaze_1_1gui_1_1canvas_1_1CanvasConfig.html#af39665815d5c2f96c519e755e34b6f71",
"d3/dae/structyaze_1_1zelda3_1_1music_1_1ZeldaInstrument.html#a626b8dbaf4d8c3c51eee8f358c70dff6",
"d3/de1/color_8cc.html",
"d3/ded/classyaze_1_1emu_1_1Ppu.html#abbd6786c09a98bc802a2d4c0e4835297",
"d4/d0a/namespaceyaze_1_1test.html#a48974bc37811e72ae810e23d0f38b817ade8900e4c323b0c667bfd5ca5ecf9e5c",
"d4/d45/classyaze_1_1editor_1_1AssemblyEditor.html#a1a633715802319bf04bb52adcd40f4c0",
"d4/d72/structyaze_1_1zelda3_1_1music_1_1SampleEdit.html#a553d3dac9e73dae2d52f774132dcea99",
"d4/daf/structyaze_1_1cli_1_1AutomationResult.html#a05882e104e1570670ab78cc6ee372b23",
"d4/de6/classyaze_1_1gfx_1_1Arena.html#aa41e0aa5873588ea59e80e2a8fb91605",
"d5/d1f/namespaceyaze_1_1zelda3.html#a15147ef5d5b48248b07ffacdb989c1f1",
"d5/d1f/namespaceyaze_1_1zelda3.html#a71e42702970582dcdfd1e19263d45401",
"d5/d1f/namespaceyaze_1_1zelda3.html#afb47ec71853bc72be86c1b0c481a3496a58a15dcd8225b3b354d3696f1f713a77",
"d5/d66/structyaze_1_1gui_1_1TextBox.html#a89c0349e9a967ba2141bb661bfcf5286",
"d5/da0/structyaze_1_1emu_1_1DspChannel.html#abbb8bd5519f388259898ace5fc41085e",
"d5/dc4/structyaze_1_1gui_1_1UnsortedAsset.html#a4797d528da0369d7f0963452b3f15625",
"d6/d20/namespaceyaze_1_1emu.html#a22bf51ed91189695bf4e76bf6b85f836a4aa601cf14d34696e777daadac4d992e",
"d6/d2f/classyaze_1_1zelda3_1_1OverworldExit.html#aa2a84c487b9f3f9b807b613dd44b1472",
"d6/d3c/classyaze_1_1zelda3_1_1Inventory.html#ad76d5f631ded2c8d98a3ec4784c63a62",
"d6/d99/structyaze_1_1cli_1_1agent_1_1ProposalCreationRequest.html#a1251aae79f45f939a7b5aad91794dfc4",
"d6/dbf/classyaze_1_1cli_1_1OverworldDescribeMap.html#a84d5ffba8abc4002cd5e43f224c37b1e",
"d6/df1/structyaze_1_1emu_1_1OAMAddress.html#a1c24190a6ec10552970cc29b620d33bf",
"d7/d4d/classyaze_1_1gui_1_1canvas_1_1CanvasUsageTracker.html#a992006be8cffbae8211a470c430b5583",
"d7/d61/classyaze_1_1zelda3_1_1DungeonEditorSystem.html#a94c07e8126f35694c25d61054d98165f",
"d7/d6f/md_docs_2C1-changelog.html#autotoc_md293",
"d7/da7/classyaze_1_1emu_1_1Apu.html#a0fc510255750895c85bb6dc035baec71",
"d7/dd7/classyaze_1_1test_1_1DungeonEditorIntegrationTest.html#a459ed0766da40be6dac2060695b3313f",
"d7/df6/classyaze_1_1zelda3_1_1Room.html#acba4b7999de632ca60768b2a6d6d3954",
"d7/dfb/object__parser__structs__test_8cc.html#a7a70fe649e52e84d73879aabc04936d7",
"d8/d31/structyaze_1_1emu_1_1INIDISP.html",
"d8/d78/entity_8cc.html#a462358c32f22f0046f4faf984a8254a8",
"d8/d9f/structyaze_1_1editor_1_1ExampleTreeNode.html#aa3765d8d3588911db68deae637d8a358",
"d8/dd5/classyaze_1_1gui_1_1WidgetIdScope.html#af28bb32fa0f40bb2dc6bd37ec90d949a",
"d8/df1/scratch__space_8cc_source.html",
"d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md239",
"d9/dad/classyaze_1_1cli_1_1tui_1_1ChatTUI.html#a4a3d32670d995d957bd9f5d0a59a7ba7",
"d9/dc5/classyaze_1_1zelda3_1_1Overworld.html#a33b58f0fcecbcfd47dae3203fcfade96",
"d9/dcc/classyaze_1_1zelda3_1_1OverworldMap.html#a641e67fe17f5766f129f9ecdc62fb74e",
"d9/df1/classyaze_1_1editor_1_1DungeonRenderer.html",
"da/d2c/classyaze_1_1gui_1_1Canvas.html#a26794e293b9d996abc70409d6aebfd8e",
"da/d2c/classyaze_1_1gui_1_1Canvas.html#ad7081d52f5a736dde87a62caf2ce63dc",
"da/d3e/classyaze_1_1test_1_1TestManager.html#ae7cbaf88ee1e98b4e2fa8c598fc21442",
"da/d68/classyaze_1_1gfx_1_1SnesPalette.html#acb2001e70c7d35d073f13449041b21fa",
"da/dbb/canvas_8cc.html#a359973f01787648b92a53d2aa84120a9",
"da/de7/simple__chat__session_8cc.html#aef26d3d3782ab75b4645a02a8838f114",
"db/d2d/classyaze_1_1cli_1_1GeminiAIService.html#a8fabee613135908e8ef20253f8261658",
"db/d71/classyaze_1_1editor_1_1GfxGroupEditor.html#ab5e0e5c9df01f8f3aa57253c98c41516",
"db/d82/classyaze_1_1editor_1_1Tile16Editor.html#af1eaa07503781612672898f8b7cb613d",
"db/dbd/classyaze_1_1cli_1_1agent_1_1SimpleChatSession.html#a3d6796ac1d1fc39856ac595f1f012a89",
"db/de0/structyaze_1_1cli_1_1FewShotExample.html#a36dd93b386ba37fd0f16baeacd2bd10e",
"dc/d0c/tilemap_8h.html#a85cdb560638714d7afc71b20b1b8526c",
"dc/d31/classyaze_1_1editor_1_1GraphicsEditor.html#a8cfe302d85d5f0f260d6636ffb1f39aa",
"dc/d4f/classyaze_1_1editor_1_1DungeonObjectSelector.html#a89648f4a11839d96dfb4abaf11502bd0",
"dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md463",
"dc/de0/classyaze_1_1test_1_1ArenaTestSuite.html#a4c94f94ea3e53235c2688ba35aef35df",
"dc/df4/classyaze_1_1emu_1_1Cpu.html#a7035a9d4ef932fa372091b210688ca53",
"dc/df4/classyaze_1_1emu_1_1Cpu.html#aeae802e557d03bfd5064e25def5d5686",
"dd/d12/classyaze_1_1editor_1_1EditorManager.html#aa6c97335949b04ba51ae275497c11d30",
"dd/d3f/structyaze_1_1zelda3_1_1MouseConfig.html#a7e4f85c092f64324e18ddec705252401",
"dd/d63/namespaceyaze_1_1cli.html#a8939ba98dedba4fb2d1e4b0a56c02807a0fa3a54d8627d575611d75e3c4288834",
"dd/dac/classyaze_1_1gfx_1_1BackgroundBuffer.html#a06f7ea3079966e11d4844461ac7da7d7",
"dd/ddf/classyaze_1_1gui_1_1canvas_1_1CanvasContextMenu.html#a728e925cc68d6c08332b2f55f4146715",
"de/d07/structyaze_1_1cli_1_1PolicyEvaluator_1_1PolicyConfig_1_1ReviewRequirement.html#ab45367e9b2a221c969a9bcf34a92c47b",
"de/d3c/namespaceyaze_1_1cli_1_1anonymous__namespace_02test__suite__loader_8cc_03.html#a4688dd3bdc90a89dd261ddfbb1066d82",
"de/d7d/structyaze_1_1editor_1_1ExampleAppPropertyEditor.html#aeda01e4c4b91917adb93eb266916fdf3",
"de/da7/structyaze_1_1gui_1_1canvas_1_1CanvasPaletteManager.html#a9b73908eb2a2ed26a42e8e12cf4931ab",
"de/dbf/icons_8h.html#a16ee59409cfdbef1d387c2c7e2cba511",
"de/dbf/icons_8h.html#a35ed0cc06c8568a66f7e2e8161c7c31e",
"de/dbf/icons_8h.html#a502371e38b010aada0d82ad04d11c859",
"de/dbf/icons_8h.html#a6d32f01cda5f08cde819948336584a4b",
"de/dbf/icons_8h.html#a8d3f55ab18c9fcde0f333de7cd1a3fed",
"de/dbf/icons_8h.html#aab8e2e0b1b1c08fe389fa9e85bcc6532",
"de/dbf/icons_8h.html#ac8ec4836a53a11e307e07d003dfd5af5",
"de/dbf/icons_8h.html#ae43649c476fa8a774198b7ba8336214c",
"de/dbf/icons_8h.html#aff2b432d74413cdf317314b100ab6f87",
"de/de7/classyaze_1_1zelda3_1_1DungeonObjectEditor.html#a8b449a11bd225d0ba823743124904b6c",
"df/d0e/classyaze_1_1gfx_1_1Bitmap.html#a9c987690a2aa0099aa0531749a660c06",
"df/d41/structyaze_1_1editor_1_1zsprite_1_1ZSprite.html#a32432d22b18af188414e828c430dad5b",
"df/db9/classyaze_1_1cli_1_1ModernCLI.html#a2e36509c08e30abf8e7499d57228be7a",
"dir_12e106757ecdaf4af0ac0afab505b8fc.html",
"namespacemembers_t.html"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';