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
    [ "Dungeon Editor Test Coverage Report", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html", [
      [ "Test Summary", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md407", null ],
      [ "Detailed Test Coverage", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md408", [
        [ "Unit Tests (14/14 PASSING) ✅", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md409", null ],
        [ "Integration Tests (10/14 PASSING) ⚠️", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md410", [
          [ "✅ PASSING Tests (10)", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md411", null ],
          [ "⚠️ FAILING Tests (4)", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md412", null ]
        ] ],
        [ "E2E Tests (1 Test) ✅", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md413", null ]
      ] ],
      [ "Coverage by Feature", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md414", [
        [ "Core Functionality", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md415", null ],
        [ "Code Coverage Estimate", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md416", null ]
      ] ],
      [ "Test Infrastructure", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md417", [
        [ "Real ROM Integration", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md418", null ],
        [ "ImGui Test Engine Integration", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md419", null ],
        [ "Test Organization", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md420", null ]
      ] ],
      [ "Running Tests", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md421", [
        [ "All Tests", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md422", null ],
        [ "Unit Tests Only", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md423", null ],
        [ "Integration Tests Only", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md424", null ],
        [ "E2E Tests (GUI Mode)", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md425", null ],
        [ "Specific Test", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md426", null ]
      ] ],
      [ "Next Steps", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md427", [
        [ "Priority 1: Fix Failing Tests", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md428", null ],
        [ "Priority 2: Add More E2E Tests", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md429", null ],
        [ "Priority 3: Performance Tests", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md430", null ]
      ] ],
      [ "Conclusion", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md431", null ]
    ] ],
    [ "Dungeon Testing Results - October 4, 2025", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html", [
      [ "Executive Summary", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md433", null ],
      [ "Test Execution Results", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md434", [
        [ "Unit Tests Status", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md435", [
          [ "✅ PASSING Tests (28/28)", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md436", null ],
          [ "❌ FAILING Tests (13/28)", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md437", null ]
        ] ],
        [ "Integration Tests Status", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md438", [
          [ "❌ CRASHED Tests", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md439", null ]
        ] ]
      ] ],
      [ "Code Cross-Reference: yaze vs ZScream", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md440", [
        [ "Room Implementation Comparison", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md441", [
          [ "✅ Implemented Features in yaze", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md442", null ],
          [ "⚠️ Partially Implemented Features", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md443", null ],
          [ "❌ Missing/Unclear Features", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md444", null ]
        ] ],
        [ "RoomObject Implementation Comparison", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md445", [
          [ "✅ Implemented in yaze", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md446", null ],
          [ "⚠️ Partial/Different Implementation", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md447", null ],
          [ "❌ Missing Features", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md448", null ]
        ] ]
      ] ],
      [ "Object Encoding/Decoding Analysis", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md449", [
        [ "ZScream Implementation (Room.cs:721-838)", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md450", null ],
        [ "yaze Implementation Status", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md451", null ]
      ] ],
      [ "Critical Issues Found", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md452", [
        [ "1. Integration Test Approach Changed", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md453", null ],
        [ "2. Object Encoding/Decoding ✅ VERIFIED (RESOLVED)", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md454", null ],
        [ "3. E2E Tests Not Adapted (MEDIUM)", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md455", null ]
      ] ],
      [ "Recommendations", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md456", [
        [ "Immediate Actions (This Week)", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md457", null ],
        [ "Short-term Actions (Next 2 Weeks)", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md458", null ],
        [ "Medium-term Actions (Next Month)", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md459", null ]
      ] ],
      [ "Test Coverage Summary", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md460", [
        [ "Current Coverage (Updated Oct 4, 2025)", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md461", null ],
        [ "Target Coverage (Per Testing Strategy)", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md462", null ]
      ] ],
      [ "Feature Completeness Analysis", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md463", [
        [ "Core Dungeon Features", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md464", null ],
        [ "Object Editing Features", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md465", null ],
        [ "Advanced Features", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md466", null ]
      ] ],
      [ "Next Steps", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md467", null ],
      [ "Conclusion", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md468", null ]
    ] ],
    [ "Asm Style Guide", "d7/d9a/md_docs_2E1-asm-style-guide.html", [
      [ "Table of Contents", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md471", null ],
      [ "File Structure", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md472", null ],
      [ "Labels and Symbols", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md473", null ],
      [ "Comments", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md474", null ],
      [ "Instructions", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md475", null ],
      [ "Macros", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md476", null ],
      [ "Loops and Branching", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md477", null ],
      [ "Data Structures", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md478", null ],
      [ "Code Organization", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md479", null ],
      [ "Custom Code", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md480", null ]
    ] ],
    [ "Dungeon Editor Design Plan", "dc/d82/md_docs_2E3-dungeon-editor-design.html", [
      [ "Overview", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md482", null ],
      [ "Architecture Overview", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md483", [
        [ "Core Components", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md484", null ],
        [ "File Structure", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md485", null ]
      ] ],
      [ "Component Responsibilities", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md486", [
        [ "DungeonEditor (Main Orchestrator)", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md487", null ],
        [ "DungeonRoomSelector", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md488", null ],
        [ "DungeonCanvasViewer", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md489", null ],
        [ "DungeonObjectSelector", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md490", null ]
      ] ],
      [ "Data Flow", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md491", [
        [ "Initialization Flow", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md492", null ],
        [ "Runtime Data Flow", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md493", null ],
        [ "Key Data Structures", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md494", null ]
      ] ],
      [ "Integration Patterns", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md495", [
        [ "Component Communication", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md496", null ],
        [ "ROM Data Management", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md497", null ],
        [ "State Synchronization", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md498", null ]
      ] ],
      [ "UI Layout Architecture", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md499", [
        [ "3-Column Layout", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md500", null ],
        [ "Component Internal Layout", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md501", null ]
      ] ],
      [ "Coordinate System", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md502", [
        [ "Room Coordinates vs Canvas Coordinates", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md503", null ],
        [ "Bounds Checking", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md504", null ]
      ] ],
      [ "Error Handling & Validation", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md505", [
        [ "ROM Validation", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md506", null ],
        [ "Bounds Validation", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md507", null ]
      ] ],
      [ "Performance Considerations", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md508", [
        [ "Caching Strategy", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md509", null ],
        [ "Rendering Optimization", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md510", null ]
      ] ],
      [ "Testing Strategy", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md511", [
        [ "Integration Tests", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md512", null ],
        [ "Test Categories", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md513", null ]
      ] ],
      [ "Future Development Guidelines", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md514", [
        [ "Adding New Features", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md515", null ],
        [ "Component Extension Patterns", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md516", null ],
        [ "Data Flow Extension", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md517", null ],
        [ "UI Layout Extension", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md518", null ]
      ] ],
      [ "Common Pitfalls & Solutions", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md519", [
        [ "Memory Management", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md520", null ],
        [ "Coordinate System", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md521", null ],
        [ "State Synchronization", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md522", null ],
        [ "Performance Issues", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md523", null ]
      ] ],
      [ "Debugging Tools", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md524", [
        [ "Debug Popup", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md525", null ],
        [ "Logging", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md526", null ]
      ] ],
      [ "Build Integration", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md527", [
        [ "CMake Configuration", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md528", null ],
        [ "Dependencies", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md529", null ]
      ] ],
      [ "Conclusion", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md530", null ]
    ] ],
    [ "Tile16 Editor Palette System", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html", [
      [ "Executive Summary", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md532", null ],
      [ "Problem Analysis", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md533", [
        [ "Critical Issues Identified", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md534", null ]
      ] ],
      [ "Solution Architecture", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md535", [
        [ "Core Design Principles", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md536", null ],
        [ "256-Color Overworld Palette Structure", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md537", null ],
        [ "Sheet-to-Palette Mapping", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md538", null ],
        [ "Palette Button Mapping", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md539", null ]
      ] ],
      [ "Implementation Details", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md540", [
        [ "1. Fixed SetPaletteWithTransparent Method", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md541", null ],
        [ "2. Corrected Tile16 Editor Palette System", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md542", null ],
        [ "3. Palette Coordination Flow", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md543", null ]
      ] ],
      [ "UI/UX Refactoring", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md544", [
        [ "New Three-Column Layout", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md545", null ],
        [ "Canvas Context Menu Fixes", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md546", null ],
        [ "Dynamic Zoom Controls", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md547", null ]
      ] ],
      [ "Testing Protocol", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md548", [
        [ "Crash Prevention Testing", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md549", null ],
        [ "Color Alignment Testing", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md550", null ],
        [ "UI/UX Testing", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md551", null ]
      ] ],
      [ "Error Handling", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md552", [
        [ "Bounds Checking", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md553", null ],
        [ "Fallback Mechanisms", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md554", null ]
      ] ],
      [ "Debug Information Display", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md555", null ],
      [ "Known Issues and Ongoing Work", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md556", [
        [ "Completed Items ✅", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md557", null ],
        [ "Active Issues ⚠️", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md558", null ],
        [ "Current Status Summary", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md559", null ],
        [ "Future Enhancements", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md560", null ]
      ] ],
      [ "Maintenance Notes", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md561", null ],
      [ "Next Steps", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md563", [
        [ "Immediate Priorities", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md564", null ],
        [ "Investigation Areas", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md565", null ]
      ] ]
    ] ],
    [ "Overworld Loading Guide", "dc/d12/md_docs_2F1-overworld-loading.html", [
      [ "Table of Contents", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md568", null ],
      [ "Overview", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md569", null ],
      [ "ROM Types and Versions", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md570", [
        [ "Version Detection", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md571", null ],
        [ "Feature Support by Version", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md572", null ]
      ] ],
      [ "Overworld Map Structure", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md573", [
        [ "Core Properties", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md574", null ]
      ] ],
      [ "Overlays and Special Area Maps", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md575", [
        [ "Understanding Overlays", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md576", null ],
        [ "Special Area Maps (0x80-0x9F)", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md577", null ],
        [ "Overlay ID Mappings", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md578", null ],
        [ "Drawing Order", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md579", null ],
        [ "Vanilla Overlay Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md580", null ],
        [ "Special Area Graphics Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md581", null ]
      ] ],
      [ "Loading Process", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md582", [
        [ "1. Version Detection", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md583", null ],
        [ "2. Map Initialization", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md584", null ],
        [ "3. Property Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md585", [
          [ "Vanilla ROMs (asm_version == 0xFF)", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md586", null ],
          [ "ZSCustomOverworld v2/v3", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md587", null ]
        ] ],
        [ "4. Custom Data Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md588", null ]
      ] ],
      [ "ZScream Implementation", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md589", [
        [ "OverworldMap Constructor", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md590", null ],
        [ "Key Methods", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md591", null ]
      ] ],
      [ "Yaze Implementation", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md592", [
        [ "OverworldMap Constructor", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md593", null ],
        [ "Key Methods", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md594", null ],
        [ "Current Status", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md595", null ]
      ] ],
      [ "Key Differences", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md596", [
        [ "1. Language and Architecture", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md597", null ],
        [ "2. Data Structures", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md598", null ],
        [ "3. Error Handling", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md599", null ],
        [ "4. Graphics Processing", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md600", null ]
      ] ],
      [ "Common Issues and Solutions", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md601", [
        [ "1. Version Detection Issues", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md602", null ],
        [ "2. Palette Loading Errors", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md603", null ],
        [ "3. Graphics Not Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md604", null ],
        [ "4. Overlay Issues", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md605", null ],
        [ "5. Large Map Problems", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md606", null ],
        [ "6. Special Area Graphics Issues", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md607", null ]
      ] ],
      [ "Best Practices", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md608", [
        [ "1. Version-Specific Code", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md609", null ],
        [ "2. Error Handling", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md610", null ],
        [ "3. Memory Management", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md611", null ],
        [ "4. Thread Safety", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md612", null ]
      ] ],
      [ "Conclusion", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md613", null ]
    ] ],
    [ "Graphics System Performance & Optimization", "db/dc2/md_docs_2GRAPHICS__PERFORMANCE.html", [
      [ "1. Executive Summary", "db/dc2/md_docs_2GRAPHICS__PERFORMANCE.html#autotoc_md615", [
        [ "Overall Performance Results", "db/dc2/md_docs_2GRAPHICS__PERFORMANCE.html#autotoc_md616", null ]
      ] ],
      [ "2. Implemented Optimizations", "db/dc2/md_docs_2GRAPHICS__PERFORMANCE.html#autotoc_md617", null ],
      [ "3. Future Optimization Recommendations", "db/dc2/md_docs_2GRAPHICS__PERFORMANCE.html#autotoc_md618", [
        [ "High Priority", "db/dc2/md_docs_2GRAPHICS__PERFORMANCE.html#autotoc_md619", null ],
        [ "Medium Priority", "db/dc2/md_docs_2GRAPHICS__PERFORMANCE.html#autotoc_md620", null ]
      ] ]
    ] ],
    [ "yaze Documentation", "d3/d4c/md_docs_2index.html", [
      [ "Quick Start", "d3/d4c/md_docs_2index.html#autotoc_md622", null ],
      [ "Development", "d3/d4c/md_docs_2index.html#autotoc_md623", null ],
      [ "Technical Documentation", "d3/d4c/md_docs_2index.html#autotoc_md624", [
        [ "Assembly & Code", "d3/d4c/md_docs_2index.html#autotoc_md625", null ],
        [ "Editor Systems", "d3/d4c/md_docs_2index.html#autotoc_md626", null ],
        [ "Overworld System", "d3/d4c/md_docs_2index.html#autotoc_md627", null ]
      ] ],
      [ "Key Features", "d3/d4c/md_docs_2index.html#autotoc_md628", null ]
    ] ],
    [ "Ollama Integration Status - Updated# Ollama Integration Status", "da/d40/md_docs_2ollama__integration__status.html", [
      [ "✅ Completed## ✅ Completed", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md631", [
        [ "Infrastructure### Flag Parsing", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md632", null ],
        [ "Current Issue: Empty Tool Results### Tool System", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md633", null ]
      ] ],
      [ "🎨 New Features Added  - OR doesn't provide a <tt>text_response</tt> field in the JSON", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md634", [
        [ "Verbose Mode**Solution Needed**: Update system prompt to include explicit instructions like:", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md635", null ],
        [ "Priority 2: Refine Prompts (MEDIUM)- ✅ Model loads (qwen2.5-coder:7b)", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md636", null ],
        [ "Priority 3: Documentation (LOW)```", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md637", null ]
      ] ],
      [ "🧪 Testing Commands### Priority 1: Fix Tool Calling Loop (High Priority)", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md638", null ],
      [ "📊 Performance   - codellama:7b", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md639", null ],
      [ "🎯 Success Criteria", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md640", [
        [ "Priority 3: Documentation (Low Priority)", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md641", null ]
      ] ],
      [ "🐛 Known Issues", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md642", null ],
      [ "💡 Recommendations", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md643", [
        [ "For Users", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md644", null ],
        [ "For Developers", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md645", null ]
      ] ],
      [ "📝 Related Files", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md646", null ],
      [ "🎯 Success Criteria", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md647", [
        [ "Minimum Viable", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md648", null ],
        [ "Full Success", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md649", null ]
      ] ]
    ] ],
    [ "yaze Overworld Testing Guide", "de/d8a/md_docs_2overworld__testing__guide.html", [
      [ "Overview", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md652", null ],
      [ "Test Architecture", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md653", [
        [ "1. Golden Data System", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md654", null ],
        [ "2. Test Categories", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md655", [
          [ "Unit Tests (<tt>test/unit/zelda3/</tt>)", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md656", null ],
          [ "Integration Tests (<tt>test/e2e/</tt>)", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md657", null ],
          [ "Golden Data Tools", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md658", null ]
        ] ]
      ] ],
      [ "Quick Start", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md659", [
        [ "Prerequisites", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md660", null ],
        [ "Running Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md661", [
          [ "Basic Test Run", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md662", null ],
          [ "Selective Test Execution", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md663", null ]
        ] ]
      ] ],
      [ "Test Components", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md664", [
        [ "1. Golden Data Extractor", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md665", null ],
        [ "2. Integration Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md666", null ],
        [ "3. End-to-End Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md667", null ]
      ] ],
      [ "Test Validation Points", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md668", [
        [ "1. ZScream Compatibility", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md669", null ],
        [ "2. ROM State Validation", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md670", null ],
        [ "3. Performance and Stability", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md671", null ]
      ] ],
      [ "Environment Variables", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md672", [
        [ "Test Configuration", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md673", null ],
        [ "Build Configuration", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md674", null ]
      ] ],
      [ "Test Reports", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md675", [
        [ "Generated Reports", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md676", null ],
        [ "Report Location", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md677", null ]
      ] ],
      [ "Troubleshooting", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md678", [
        [ "Common Issues", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md679", [
          [ "1. ROM Not Found", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md680", null ],
          [ "2. Build Failures", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md681", null ],
          [ "3. Test Failures", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md682", null ]
        ] ],
        [ "Debug Mode", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md683", null ]
      ] ],
      [ "Advanced Usage", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md684", [
        [ "Custom Test Scenarios", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md685", [
          [ "1. Testing Custom ROMs", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md686", null ],
          [ "2. Regression Testing", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md687", null ],
          [ "3. Performance Testing", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md688", null ]
        ] ]
      ] ],
      [ "Integration with CI/CD", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md689", [
        [ "GitHub Actions Example", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md690", null ]
      ] ],
      [ "Contributing", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md691", [
        [ "Adding New Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md692", null ],
        [ "Test Guidelines", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md693", null ],
        [ "Example Test Structure", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md694", null ]
      ] ],
      [ "Conclusion", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md695", null ]
    ] ],
    [ "z3ed: AI-Powered CLI for YAZE", "d3/d63/md_docs_2z3ed_2README.html", [
      [ "1. Overview", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md697", [
        [ "Core Capabilities", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md698", null ]
      ] ],
      [ "2. Quick Start", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md699", [
        [ "Build", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md700", null ],
        [ "AI Setup", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md701", null ],
        [ "Example Commands", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md702", null ]
      ] ],
      [ "3. Architecture", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md703", [
        [ "System Components Diagram", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md704", null ]
      ] ],
      [ "4. Agentic & Generative Workflow (MCP)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md705", null ],
      [ "5. Command Reference", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md706", [
        [ "Agent Commands", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md707", null ],
        [ "Resource Commands", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md708", null ]
      ] ],
      [ "6. Chat Modes", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md709", [
        [ "FTXUI Chat (<tt>agent chat</tt>)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md710", null ],
        [ "Simple Chat (<tt>agent simple-chat</tt>)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md711", null ],
        [ "GUI Chat Widget (Editor Integration Preview)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md712", null ]
      ] ],
      [ "7. AI Provider Configuration", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md713", null ],
      [ "8. Roadmap & Implementation Status", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md714", [
        [ "✅ Completed", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md715", null ],
        [ "🚧 Active & Next Steps", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md716", null ]
      ] ],
      [ "9. Troubleshooting", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md717", null ]
    ] ],
    [ "yaze - Yet Another Zelda3 Editor", "d0/d30/md_README.html", [
      [ "Version 0.3.2 - Release", "d0/d30/md_README.html#autotoc_md719", null ],
      [ "Quick Start", "d0/d30/md_README.html#autotoc_md724", [
        [ "Build", "d0/d30/md_README.html#autotoc_md725", null ],
        [ "Applications", "d0/d30/md_README.html#autotoc_md726", null ]
      ] ],
      [ "Usage", "d0/d30/md_README.html#autotoc_md727", [
        [ "GUI Editor", "d0/d30/md_README.html#autotoc_md728", null ],
        [ "Command Line Tool", "d0/d30/md_README.html#autotoc_md729", null ],
        [ "C++ API", "d0/d30/md_README.html#autotoc_md730", null ]
      ] ],
      [ "Documentation", "d0/d30/md_README.html#autotoc_md731", null ],
      [ "Supported Platforms", "d0/d30/md_README.html#autotoc_md732", null ],
      [ "ROM Compatibility", "d0/d30/md_README.html#autotoc_md733", null ],
      [ "Contributing", "d0/d30/md_README.html#autotoc_md734", null ],
      [ "License", "d0/d30/md_README.html#autotoc_md735", null ],
      [ "🙏 Acknowledgments", "d0/d30/md_README.html#autotoc_md736", null ],
      [ "📸 Screenshots", "d0/d30/md_README.html#autotoc_md737", null ]
    ] ],
    [ "yaze Build Scripts", "de/d82/md_scripts_2README.html", [
      [ "Windows Scripts", "de/d82/md_scripts_2README.html#autotoc_md740", [
        [ "vcpkg Setup (Optional)", "de/d82/md_scripts_2README.html#autotoc_md741", null ]
      ] ],
      [ "Windows Build Workflow", "de/d82/md_scripts_2README.html#autotoc_md742", [
        [ "Recommended: Visual Studio CMake Mode", "de/d82/md_scripts_2README.html#autotoc_md743", null ],
        [ "Command Line Build", "de/d82/md_scripts_2README.html#autotoc_md744", null ],
        [ "Compiler Notes", "de/d82/md_scripts_2README.html#autotoc_md745", null ]
      ] ],
      [ "Quick Start (Windows)", "de/d82/md_scripts_2README.html#autotoc_md746", [
        [ "Option 1: Visual Studio (Recommended)", "de/d82/md_scripts_2README.html#autotoc_md747", null ],
        [ "Option 2: Command Line", "de/d82/md_scripts_2README.html#autotoc_md748", null ],
        [ "Option 3: With vcpkg (Optional)", "de/d82/md_scripts_2README.html#autotoc_md749", null ]
      ] ],
      [ "Troubleshooting", "de/d82/md_scripts_2README.html#autotoc_md750", [
        [ "Common Issues", "de/d82/md_scripts_2README.html#autotoc_md751", null ],
        [ "Getting Help", "de/d82/md_scripts_2README.html#autotoc_md752", null ]
      ] ],
      [ "Other Scripts", "de/d82/md_scripts_2README.html#autotoc_md753", null ],
      [ "Build Environment Verification", "de/d82/md_scripts_2README.html#autotoc_md754", [
        [ "<tt>verify-build-environment.ps1</tt> / <tt>.sh</tt>", "de/d82/md_scripts_2README.html#autotoc_md755", null ],
        [ "Usage", "de/d82/md_scripts_2README.html#autotoc_md756", null ]
      ] ]
    ] ],
    [ "End-to-End (E2E) Tests", "d9/db0/md_test_2e2e_2README.html", [
      [ "Active Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md758", [
        [ "✅ Working Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md759", null ],
        [ "📝 Dungeon Editor Smoke Test", "d9/db0/md_test_2e2e_2README.html#autotoc_md760", null ]
      ] ],
      [ "Running Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md761", [
        [ "All E2E Tests (GUI Mode)", "d9/db0/md_test_2e2e_2README.html#autotoc_md762", null ],
        [ "Specific Test Category", "d9/db0/md_test_2e2e_2README.html#autotoc_md763", null ],
        [ "Dungeon Editor Test Only", "d9/db0/md_test_2e2e_2README.html#autotoc_md764", null ]
      ] ],
      [ "Test Development", "d9/db0/md_test_2e2e_2README.html#autotoc_md765", [
        [ "Creating New Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md766", null ],
        [ "Register in yaze_test.cc", "d9/db0/md_test_2e2e_2README.html#autotoc_md767", null ],
        [ "ImGui Test Engine API", "d9/db0/md_test_2e2e_2README.html#autotoc_md768", null ]
      ] ],
      [ "Test Logging", "d9/db0/md_test_2e2e_2README.html#autotoc_md769", null ],
      [ "Test Infrastructure", "d9/db0/md_test_2e2e_2README.html#autotoc_md770", [
        [ "File Organization", "d9/db0/md_test_2e2e_2README.html#autotoc_md771", null ],
        [ "Helper Functions", "d9/db0/md_test_2e2e_2README.html#autotoc_md772", null ]
      ] ],
      [ "Future Test Ideas", "d9/db0/md_test_2e2e_2README.html#autotoc_md773", null ],
      [ "Troubleshooting", "d9/db0/md_test_2e2e_2README.html#autotoc_md774", [
        [ "Test Crashes in GUI Mode", "d9/db0/md_test_2e2e_2README.html#autotoc_md775", null ],
        [ "Tests Not Found", "d9/db0/md_test_2e2e_2README.html#autotoc_md776", null ],
        [ "ImGui Items Not Found", "d9/db0/md_test_2e2e_2README.html#autotoc_md777", null ]
      ] ],
      [ "References", "d9/db0/md_test_2e2e_2README.html#autotoc_md778", null ],
      [ "Status", "d9/db0/md_test_2e2e_2README.html#autotoc_md779", null ]
    ] ],
    [ "yaze Test Suite", "d0/d46/md_test_2README.html", [
      [ "Directory Structure", "d0/d46/md_test_2README.html#autotoc_md781", null ],
      [ "Test Categories", "d0/d46/md_test_2README.html#autotoc_md782", [
        [ "Unit Tests (<tt>unit/</tt>)", "d0/d46/md_test_2README.html#autotoc_md783", null ],
        [ "Integration Tests (<tt>integration/</tt>)", "d0/d46/md_test_2README.html#autotoc_md784", null ],
        [ "End-to-End Tests (<tt>e2e/</tt>)", "d0/d46/md_test_2README.html#autotoc_md785", null ]
      ] ],
      [ "Enhanced Test Runner", "d0/d46/md_test_2README.html#autotoc_md786", [
        [ "Usage Examples", "d0/d46/md_test_2README.html#autotoc_md787", null ],
        [ "Test Modes", "d0/d46/md_test_2README.html#autotoc_md788", null ],
        [ "Options", "d0/d46/md_test_2README.html#autotoc_md789", null ]
      ] ],
      [ "E2E ROM Testing", "d0/d46/md_test_2README.html#autotoc_md790", [
        [ "Test Cases", "d0/d46/md_test_2README.html#autotoc_md791", null ]
      ] ],
      [ "ZSCustomOverworld Upgrade Testing", "d0/d46/md_test_2README.html#autotoc_md792", [
        [ "Supported Upgrades", "d0/d46/md_test_2README.html#autotoc_md793", null ],
        [ "Test Cases", "d0/d46/md_test_2README.html#autotoc_md794", null ],
        [ "Version-Specific Features", "d0/d46/md_test_2README.html#autotoc_md795", [
          [ "Vanilla", "d0/d46/md_test_2README.html#autotoc_md796", null ],
          [ "v2", "d0/d46/md_test_2README.html#autotoc_md797", null ],
          [ "v3", "d0/d46/md_test_2README.html#autotoc_md798", null ]
        ] ]
      ] ],
      [ "Environment Variables", "d0/d46/md_test_2README.html#autotoc_md799", null ],
      [ "CI/CD Integration", "d0/d46/md_test_2README.html#autotoc_md800", null ],
      [ "Deprecated Tests", "d0/d46/md_test_2README.html#autotoc_md801", null ],
      [ "Best Practices", "d0/d46/md_test_2README.html#autotoc_md802", null ],
      [ "AI Agent Testing", "d0/d46/md_test_2README.html#autotoc_md803", null ]
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
"d0/d30/md_README.html#autotoc_md727",
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
"d4/d0a/namespaceyaze_1_1test.html#a48974bc37811e72ae810e23d0f38b817a9f2d923af819cb4370597b632b0b153a",
"d4/d45/classyaze_1_1editor_1_1AssemblyEditor.html",
"d4/d70/zsprite_8h_source.html",
"d4/dae/structyaze_1_1cli_1_1AssertionOutcome.html#addf2d6cb54dc0e127a174c37130fc08f",
"d4/de6/classyaze_1_1gfx_1_1Arena.html#a5dc521e07071ff8b9c84e96372a056e7",
"d5/d1f/namespaceyaze_1_1zelda3.html#a0b76a585e0f3e8fac732a803ef61f1f1",
"d5/d1f/namespaceyaze_1_1zelda3.html#a6ad5f0a9ef45edbbc2a8ac4ae5ababa8",
"d5/d1f/namespaceyaze_1_1zelda3.html#af5e428ab7a25bd7026cc338f5bb0d5d4",
"d5/d66/structyaze_1_1gui_1_1TextBox.html#a46f89a160f081966959cf39476024f80",
"d5/da0/structyaze_1_1emu_1_1DspChannel.html#a625e5c213d7f71b7e3bb8bc7790c63b4",
"d5/dc4/classyaze_1_1app_1_1gui_1_1AgentChatWidget.html#ac39c9573f88ae42ed7510cabeebfd05e",
"d6/d10/md_docs_2A1-testing-guide.html#autotoc_md119",
"d6/d2f/classyaze_1_1zelda3_1_1OverworldExit.html#a12bcb9d33b5a173a2e73ba6850e55835",
"d6/d3c/classyaze_1_1zelda3_1_1Inventory.html#a46f359bdd6ab773784d802a9056f04dc",
"d6/d92/structyaze_1_1test_1_1TestConfig.html",
"d6/dba/test__script__parser_8cc.html#aef7046e0e13529e668caa72421cf3006",
"d6/def/classyaze_1_1test_1_1SpriteBuilderTest.html",
"d7/d4d/classyaze_1_1gui_1_1canvas_1_1CanvasUsageTracker.html#a22dd736f8ee392b27b25fc0008c4cd73",
"d7/d61/classyaze_1_1zelda3_1_1DungeonEditorSystem.html#a764c31f0e0ffc818c9a2755c13a7464f",
"d7/d6c/structyaze_1_1zelda3_1_1ObjectRenderer_1_1PerformanceStats.html#a8ab46865968d16e54eddc343f01dac40",
"d7/d9c/overworld_8h.html#aa0148854719c3e4c4a49bdf6c21151c8",
"d7/dcc/structyaze_1_1gfx_1_1PaletteGroupMap.html#a91a5d448a2489e2a38c5b0c65e9702a3",
"d7/df6/classyaze_1_1zelda3_1_1Room.html#aae969d04c095a0574748d563e65baf5c",
"d7/dfb/classyaze_1_1editor_1_1OverworldEditorManager.html#a8b834825462dcadc57db9edd64cae5c0",
"d8/d1e/classyaze_1_1zelda3_1_1RoomEntrance.html#aba5054eca23b9de41b6382d61ab7e7da",
"d8/d6e/classyaze_1_1gfx_1_1AtlasRenderer.html#ae8e609bf19857aba15858f64e06f51ba",
"d8/d98/classyaze_1_1editor_1_1DungeonEditor.html#aee3a96eeb1862b3590d3b596c74ca1f0",
"d8/dd3/namespaceyaze_1_1cli_1_1agent.html#a6a3d696815c6e8d815849cfb27cc7cc7a104573db6e35d80a8a5bacc53b26a4a2",
"d8/ddf/classyaze_1_1zelda3_1_1ObjectParser.html#a3ef1e414f59a698b0295aac853b938f1",
"d9/d41/md_docs_202-build-instructions.html#autotoc_md44",
"d9/d8d/general__commands_8cc.html#ab9c0a392ce6c066aca8d36eac12dc2b9",
"d9/dc0/room_8h.html#a693df677b1b2754452c8b8b4c4a98468aa5bcfd8fcaafc0272893dabf2e4abcd4",
"d9/dc5/classyaze_1_1zelda3_1_1Overworld.html#ac213f830ec85c668d91f46bc27939eba",
"d9/dcc/classyaze_1_1zelda3_1_1OverworldMap.html#ae1fc91cdc7c32da17038eea1511012b1",
"d9/dff/ppu__registers_8h.html#ab000d06262864a0e9d9781bf2b6ac13e",
"da/d2c/classyaze_1_1gui_1_1Canvas.html#a7c4c7b27651a4090ba8c40a58d6939dc",
"da/d37/hyrule__magic_8cc.html#afbada1f258409089e8183f259c44f4eb",
"da/d5e/structyaze_1_1core_1_1YazeProject.html#a0ef9040cae9819621476d87f8b09cf76",
"da/d81/structyaze_1_1editor_1_1zsprite_1_1UserRoutine.html#a5a1d954fa3aa13b26bfd83cca84a6ccd",
"da/dc6/structyaze_1_1cli_1_1ResourceArgument.html#a60ea89b4f627bde840c3a03f22648889",
"db/d00/classyaze_1_1editor_1_1DungeonToolset.html#ac621ddf80f2ef7e0174d57a3759da625",
"db/d56/classyaze_1_1gui_1_1BppConversionDialog.html#a4df44d0e1e0ab88292dff729efc0bd4f",
"db/d82/classyaze_1_1editor_1_1Tile16Editor.html#a52f4d12e30c1e235b9750d8051d1fc39",
"db/d9c/message__data_8h.html#a70cc399beb413359a70ca254b143d6ec",
"db/dcc/classyaze_1_1editor_1_1ScreenEditor.html#a13a7d16ac36a8d5c380b1b520583b716",
"dc/d01/classyaze_1_1emu_1_1Dsp.html#a0096143804eb584a2823a4835dce3106",
"dc/d21/structyaze_1_1test_1_1TestScript.html",
"dc/d31/classyaze_1_1editor_1_1GraphicsEditor.html#af7f03c0070650c31c8339de87cb29fa2",
"dc/d57/structyaze_1_1cli_1_1ReplayTestResult.html#a643f09d47b736233c64a9f3f9a6cd04b",
"dc/dae/classyaze_1_1test_1_1ZSCustomOverworldTestSuite.html#a6e0f2db7273f9c576d148586020db49b",
"dc/df4/classyaze_1_1emu_1_1Cpu.html#a241abb5eeead09b3b2329ee5f09da24f",
"dc/df4/classyaze_1_1emu_1_1Cpu.html#ab4a8438b61718eb89533c828c54cfde4",
"dd/d12/classyaze_1_1editor_1_1EditorManager.html#a16b9cf15d20eb0af018bb421b87d2a67",
"dd/d19/structyaze_1_1core_1_1FeatureFlags_1_1Flags.html#a851b0fe4ed380000b937328fa4f47d1e",
"dd/d4b/classyaze_1_1editor_1_1DungeonUsageTracker.html#a1b59b2eb11189c1344fff906c8317437",
"dd/d71/classyaze_1_1gui_1_1canvas_1_1CanvasInteractionHandler.html#acf3231688d40c6f2e9ccd84f1d43887e",
"dd/dcc/classyaze_1_1editor_1_1ProposalDrawer.html#a10c37259d6177a6997b34828c7fd68c4",
"dd/ddf/classyaze_1_1gui_1_1canvas_1_1CanvasContextMenu.html#afb577ada6c4b21b7c1b6475ebe7e67e7",
"de/d0f/classyaze_1_1emu_1_1MemoryImpl.html#a41a7a43904f7fffa37a1a341a60bfdda",
"de/d5c/memory__pool_8h_source.html",
"de/d89/snes__palette_8cc.html#aa11a9c0e701d4fa356fb150cd4c679d0",
"de/dbf/icons_8h.html#a01d1e30a55e3e5c43550726d9a47a166",
"de/dbf/icons_8h.html#a1fde934c4ee142ab6652c839229c1166",
"de/dbf/icons_8h.html#a3dce3fad893b8d11e5832cbd03415ff7",
"de/dbf/icons_8h.html#a5ad315db4f7010836cd2d4eacfe539b2",
"de/dbf/icons_8h.html#a79afc0873b016e60991a10b8e16e8666",
"de/dbf/icons_8h.html#a98dc8be1fed631e736bfb3c61752356d",
"de/dbf/icons_8h.html#ab5285341bfdd1ebbc5ec239bde9e66bc",
"de/dbf/icons_8h.html#ad22af956b849cfd48755e322deff9417",
"de/dbf/icons_8h.html#aec6340604949bac36be667cddf8363be",
"de/dd4/classyaze_1_1cli_1_1ProposalRegistry.html#ad88799a96c3fa185e211ee3683908928",
"de/de7/classyaze_1_1zelda3_1_1DungeonObjectEditor.html#ae97bf7aafced76a3b14852cfb7f372de",
"df/d19/namespaceyaze_1_1zelda3_1_1anonymous__namespace_02room__object__encoding__test_8cc_03.html#a9dcad3cb813411dc67f6ffa6ee34bed4",
"df/d64/structyaze_1_1emu_1_1ApuCallbacks.html#a118835693d22ce17120819b07e97e529",
"df/dbc/namespaceyaze_1_1gui_1_1canvas_1_1anonymous__namespace_02canvas__context__menu_8cc_03.html#a221115e3643b67df13dae30534fc504a",
"dir_6550b3ac391899c7ebf05357b8388b6b.html",
"namespaces.html"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';