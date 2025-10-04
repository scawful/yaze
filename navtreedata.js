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
    [ "API Reference", "dd/d96/md_docs_204-api-reference.html", [
      [ "C API (<tt>incl/yaze.h</tt>, <tt>incl/zelda.h</tt>)", "dd/d96/md_docs_204-api-reference.html#autotoc_md71", [
        [ "Core Library Functions", "dd/d96/md_docs_204-api-reference.html#autotoc_md72", null ],
        [ "ROM Operations", "dd/d96/md_docs_204-api-reference.html#autotoc_md73", null ],
        [ "Graphics Operations", "dd/d96/md_docs_204-api-reference.html#autotoc_md74", null ],
        [ "Palette System", "dd/d96/md_docs_204-api-reference.html#autotoc_md75", null ],
        [ "Message System", "dd/d96/md_docs_204-api-reference.html#autotoc_md76", null ]
      ] ],
      [ "C++ API", "dd/d96/md_docs_204-api-reference.html#autotoc_md77", [
        [ "AsarWrapper (<tt>src/app/core/asar_wrapper.h</tt>)", "dd/d96/md_docs_204-api-reference.html#autotoc_md78", [
          [ "Quick Examples", "dd/d96/md_docs_204-api-reference.html#autotoc_md79", null ],
          [ "AsarWrapper Class", "dd/d96/md_docs_204-api-reference.html#autotoc_md80", null ],
          [ "Error Handling", "dd/d96/md_docs_204-api-reference.html#autotoc_md81", null ]
        ] ],
        [ "Data Structures", "dd/d96/md_docs_204-api-reference.html#autotoc_md82", [
          [ "ROM Version Support", "dd/d96/md_docs_204-api-reference.html#autotoc_md83", null ],
          [ "SNES Graphics", "dd/d96/md_docs_204-api-reference.html#autotoc_md84", null ],
          [ "Message System", "dd/d96/md_docs_204-api-reference.html#autotoc_md85", null ]
        ] ]
      ] ],
      [ "Error Handling", "dd/d96/md_docs_204-api-reference.html#autotoc_md86", [
        [ "Status Codes", "dd/d96/md_docs_204-api-reference.html#autotoc_md87", null ],
        [ "Error Handling Pattern", "dd/d96/md_docs_204-api-reference.html#autotoc_md88", null ]
      ] ],
      [ "Extension System", "dd/d96/md_docs_204-api-reference.html#autotoc_md89", [
        [ "Plugin Architecture", "dd/d96/md_docs_204-api-reference.html#autotoc_md90", null ],
        [ "Capability Flags", "dd/d96/md_docs_204-api-reference.html#autotoc_md91", null ]
      ] ],
      [ "Backward Compatibility", "dd/d96/md_docs_204-api-reference.html#autotoc_md92", null ]
    ] ],
    [ "Testing Guide", "d6/d10/md_docs_2A1-testing-guide.html", [
      [ "Test Categories", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md94", [
        [ "Stable Tests (STABLE)", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md95", null ],
        [ "ROM-Dependent Tests (ROM_DEPENDENT)", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md96", null ],
        [ "Experimental Tests (EXPERIMENTAL)", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md97", null ]
      ] ],
      [ "Command Line Usage", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md98", null ],
      [ "CMake Presets", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md99", null ],
      [ "Writing Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md100", [
        [ "Stable Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md101", null ],
        [ "ROM-Dependent Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md102", null ],
        [ "Experimental Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md103", null ]
      ] ],
      [ "CI/CD Integration", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md104", [
        [ "GitHub Actions", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md105", null ],
        [ "Test Execution Strategy", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md106", null ]
      ] ],
      [ "Test Development Guidelines", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md107", [
        [ "Writing Stable Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md108", null ],
        [ "Writing ROM-Dependent Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md109", null ],
        [ "Writing Experimental Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md110", null ]
      ] ],
      [ "E2E GUI Testing Framework", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md111", [
        [ "Architecture", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md112", null ],
        [ "Writing E2E Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md113", null ],
        [ "Running GUI Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md114", null ]
      ] ]
    ] ],
    [ "ZScream vs. yaze Overworld Implementation Analysis", "d9/d48/md_docs_2analysis_2overworld__implementation__analysis.html", [
      [ "Executive Summary", "d9/d48/md_docs_2analysis_2overworld__implementation__analysis.html#autotoc_md116", null ],
      [ "Key Findings", "d9/d48/md_docs_2analysis_2overworld__implementation__analysis.html#autotoc_md117", [
        [ "✅ <strong>Confirmed Correct Implementations</strong>", "d9/d48/md_docs_2analysis_2overworld__implementation__analysis.html#autotoc_md118", [
          [ "1. <strong>Tile32 & Tile16 Expansion Detection</strong>", "d9/d48/md_docs_2analysis_2overworld__implementation__analysis.html#autotoc_md119", null ],
          [ "2. <strong>Entrance & Hole Coordinate Calculation</strong>", "d9/d48/md_docs_2analysis_2overworld__implementation__analysis.html#autotoc_md120", null ],
          [ "3. <strong>Data Loading (Exits, Items, Sprites)</strong>", "d9/d48/md_docs_2analysis_2overworld__implementation__analysis.html#autotoc_md121", null ],
          [ "4. <strong>Map Decompression & Sizing</strong>", "d9/d48/md_docs_2analysis_2overworld__implementation__analysis.html#autotoc_md122", null ]
        ] ],
        [ "⚠️ <strong>Key Differences Found</strong>", "d9/d48/md_docs_2analysis_2overworld__implementation__analysis.html#autotoc_md123", null ],
        [ "🎯 <strong>Conclusion</strong>", "d9/d48/md_docs_2analysis_2overworld__implementation__analysis.html#autotoc_md124", null ]
      ] ]
    ] ],
    [ "Platform Compatibility Improvements", "d1/d30/md_docs_2B2-platform-compatibility.html", [
      [ "Native File Dialog Support", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md126", null ],
      [ "Cross-Platform Build Reliability", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md127", null ],
      [ "Build Configuration Options", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md128", [
        [ "Full Build (Development)", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md129", null ],
        [ "Minimal Build", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md130", null ]
      ] ],
      [ "Implementation Details", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md131", null ],
      [ "Platform-Specific Adaptations", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md132", [
        [ "Windows", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md133", null ],
        [ "macOS", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md134", null ],
        [ "Linux", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md135", null ]
      ] ]
    ] ],
    [ "Build Presets Guide", "d7/d51/md_docs_2B3-build-presets.html", [
      [ "🍎 macOS ARM64 Presets (Recommended for Apple Silicon)", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md137", [
        [ "For Development Work:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md138", null ],
        [ "For Distribution:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md139", null ]
      ] ],
      [ "🔧 Why This Fixes Architecture Errors", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md140", null ],
      [ "📋 Available Presets", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md141", null ],
      [ "🚀 Quick Start", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md142", null ],
      [ "🛠️ IDE Integration", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md143", [
        [ "VS Code with CMake Tools:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md144", null ],
        [ "CLion:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md145", null ],
        [ "Xcode:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md146", null ]
      ] ],
      [ "🔍 Troubleshooting", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md147", null ],
      [ "📝 Notes", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md148", null ]
    ] ],
    [ "Release Workflows Documentation", "d3/d49/md_docs_2B4-release-workflows.html", [
      [ "Overview", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md150", null ],
      [ "1. Release-Simplified (<tt>release-simplified.yml</tt>)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md152", [
        [ "Purpose", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md153", null ],
        [ "Key Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md154", null ],
        [ "Use Cases", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md155", null ],
        [ "Configuration", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md156", null ],
        [ "Platforms Supported", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md157", null ]
      ] ],
      [ "2. Release (<tt>release.yml</tt>)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md159", [
        [ "Purpose", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md160", null ],
        [ "Key Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md161", null ],
        [ "Use Cases", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md162", null ],
        [ "Configuration", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md163", null ],
        [ "Advantages over Simplified", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md164", null ]
      ] ],
      [ "3. Release-Complex (<tt>release-complex.yml</tt>)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md166", [
        [ "Purpose", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md167", null ],
        [ "Key Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md168", null ],
        [ "Use Cases", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md169", null ],
        [ "Fallback Mechanisms", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md170", [
          [ "vcpkg Fallback", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md171", null ],
          [ "Chocolatey Integration", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md172", null ],
          [ "Build Configuration Fallback", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md173", null ]
        ] ],
        [ "Advanced Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md174", null ]
      ] ],
      [ "Workflow Comparison Matrix", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md176", null ],
      [ "When to Use Each Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md178", [
        [ "Use Simplified When:", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md179", null ],
        [ "Use Release When:", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md180", null ],
        [ "Use Complex When:", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md181", null ]
      ] ],
      [ "Workflow Selection Guide", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md183", [
        [ "For Development Team", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md184", null ],
        [ "For Release Manager", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md185", null ],
        [ "For CI/CD Pipeline", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md186", null ]
      ] ],
      [ "Configuration Examples", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md188", [
        [ "Triggering a Release", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md189", [
          [ "Manual Release (All Workflows)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md190", null ],
          [ "Automatic Release (Tag Push)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md191", null ]
        ] ],
        [ "Customizing Release Notes", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md192", null ]
      ] ],
      [ "Troubleshooting", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md194", [
        [ "Common Issues", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md195", [
          [ "vcpkg Failures (Windows)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md196", null ],
          [ "Dependency Conflicts", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md197", null ],
          [ "Build Failures", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md198", null ]
        ] ],
        [ "Debug Information", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md199", [
          [ "Simplified Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md200", null ],
          [ "Release Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md201", null ],
          [ "Complex Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md202", null ]
        ] ]
      ] ],
      [ "Best Practices", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md204", [
        [ "Workflow Selection", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md205", null ],
        [ "Release Process", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md206", null ],
        [ "Maintenance", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md207", null ]
      ] ],
      [ "Future Improvements", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md209", [
        [ "Planned Enhancements", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md210", null ],
        [ "Integration Opportunities", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md211", null ]
      ] ]
    ] ],
    [ "Stability, Testability & Release Workflow Improvements", "d9/d6b/md_docs_2B5-stability-improvements.html", [
      [ "Recent Improvements (v0.3.2)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md214", [
        [ "Windows Platform Stability", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md215", [
          [ "Stack Size Optimization", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md216", null ],
          [ "Development Utility Isolation", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md217", null ]
        ] ],
        [ "Graphics System Stability", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md218", [
          [ "Segmentation Fault Resolution", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md219", null ],
          [ "Comprehensive Bounds Checking", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md220", null ]
        ] ],
        [ "Build System Improvements", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md221", [
          [ "Modern Windows Workflow", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md222", null ],
          [ "Enhanced CI/CD Reliability", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md223", null ]
        ] ]
      ] ],
      [ "Recommended Optimizations", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md224", [
        [ "High Priority", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md225", [
          [ "1. Lazy Graphics Loading", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md226", null ],
          [ "2. Heap-Based Large Allocations", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md227", null ],
          [ "3. Streaming ROM Assets", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md228", null ]
        ] ],
        [ "Medium Priority", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md229", [
          [ "4. Enhanced Test Isolation", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md230", null ],
          [ "5. Dependency Caching Optimization", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md231", null ],
          [ "6. Memory Pool for Graphics", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md232", null ]
        ] ],
        [ "Low Priority", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md233", [
          [ "7. Build Time Optimization", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md234", null ],
          [ "8. Release Workflow Simplification", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md235", null ]
        ] ]
      ] ],
      [ "Testing Improvements", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md236", [
        [ "Current State", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md237", null ],
        [ "Recommendations", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md238", [
          [ "1. Visual Regression Testing", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md239", null ],
          [ "2. Performance Benchmarks", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md240", null ],
          [ "3. Fuzz Testing", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md241", null ]
        ] ]
      ] ],
      [ "Metrics & Monitoring", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md242", [
        [ "Current Measurements", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md243", null ],
        [ "Target Improvements", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md244", null ]
      ] ],
      [ "Action Items", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md245", [
        [ "Immediate (v0.3.2)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md246", null ],
        [ "Short Term (v0.3.3)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md247", null ],
        [ "Medium Term (v0.4.0)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md248", null ],
        [ "Long Term (v0.5.0+)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md249", null ]
      ] ],
      [ "Conclusion", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md250", null ]
    ] ],
    [ "Build Modularization Plan for Yaze", "d7/daf/md_docs_2build__modularization__plan.html", [
      [ "Executive Summary", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md252", null ],
      [ "Current Problems", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md253", null ],
      [ "Proposed Architecture", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md254", [
        [ "Library Hierarchy", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md255", null ]
      ] ],
      [ "Implementation Guide", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md256", [
        [ "Quick Start", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md257", [
          [ "Option 1: Enable Modular Build (Recommended)", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md258", null ],
          [ "Option 2: Keep Existing Build (Default)", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md259", null ]
        ] ],
        [ "Implementation Steps", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md260", [
          [ "Step 1: Add Build Option", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md261", null ],
          [ "Step 2: Update src/CMakeLists.txt", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md262", null ],
          [ "Step 3: Update yaze Application Linking", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md263", null ],
          [ "Step 4: Update Test Executable", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md264", null ]
        ] ]
      ] ],
      [ "Expected Performance Improvements", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md265", [
        [ "Build Time Reductions", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md266", null ],
        [ "CI/CD Benefits", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md267", null ]
      ] ],
      [ "Rollback Procedure", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md268", null ]
    ] ],
    [ "Changelog", "d7/d6f/md_docs_2C1-changelog.html", [
      [ "0.3.2 (December 2025) - In Development", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md270", [
        [ "Tile16 Editor Improvements (In Progress)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md271", null ],
        [ "Graphics System Optimizations", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md272", null ],
        [ "Windows Platform Stability", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md273", null ],
        [ "Memory Safety & Stability", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md274", null ],
        [ "Testing & CI/CD Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md275", null ],
        [ "Future Optimizations (Planned)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md276", null ],
        [ "Technical Notes", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md277", null ]
      ] ],
      [ "0.3.1 (September 2025)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md278", [
        [ "Major Features", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md279", null ],
        [ "Tile16 Editor Enhancements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md280", null ],
        [ "ZSCustomOverworld v3 Implementation", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md281", null ],
        [ "Technical Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md282", null ],
        [ "User Interface", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md283", null ],
        [ "Bug Fixes", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md284", null ],
        [ "ZScream Compatibility Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md285", null ]
      ] ],
      [ "0.3.0 (September 2025)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md286", [
        [ "Major Features", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md287", null ],
        [ "User Interface & Theming", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md288", null ],
        [ "Enhancements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md289", null ],
        [ "Technical Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md290", null ],
        [ "Bug Fixes", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md291", null ]
      ] ],
      [ "0.2.2 (December 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md292", null ],
      [ "0.2.1 (August 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md293", null ],
      [ "0.2.0 (July 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md294", null ],
      [ "0.1.0 (May 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md295", null ],
      [ "0.0.9 (April 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md296", null ],
      [ "0.0.8 (February 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md297", null ],
      [ "0.0.7 (January 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md298", null ],
      [ "0.0.6 (November 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md299", null ],
      [ "0.0.5 (November 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md300", null ],
      [ "0.0.4 (November 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md301", null ],
      [ "0.0.3 (October 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md302", null ]
    ] ],
    [ "Canvas System - Comprehensive Guide", "d2/db2/md_docs_2CANVAS__GUIDE.html", [
      [ "Overview", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md304", null ],
      [ "Core Concepts", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md305", [
        [ "Canvas Structure", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md306", null ],
        [ "Coordinate Systems", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md307", null ]
      ] ],
      [ "Usage Patterns", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md308", [
        [ "Pattern 1: Basic Bitmap Display", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md309", null ],
        [ "Pattern 2: Modern Begin/End", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md310", null ],
        [ "Pattern 3: RAII ScopedCanvas", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md311", null ]
      ] ],
      [ "Feature: Tile Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md312", [
        [ "Single Tile Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md313", null ],
        [ "Tilemap Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md314", null ],
        [ "Color Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md315", null ]
      ] ],
      [ "Feature: Tile Selection", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md316", [
        [ "Single Tile Selection", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md317", null ],
        [ "Multi-Tile Rectangle Selection", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md318", null ],
        [ "Rectangle Drag & Paint", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md319", null ]
      ] ],
      [ "Feature: Custom Overlays", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md320", [
        [ "Manual Points Manipulation", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md321", null ]
      ] ],
      [ "Feature: Large Map Support", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md322", [
        [ "Map Types", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md323", null ],
        [ "Boundary Clamping", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md324", null ],
        [ "Custom Map Sizes", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md325", null ]
      ] ],
      [ "Feature: Context Menu", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md326", [
        [ "Adding Custom Items", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md327", null ],
        [ "Overworld Editor Example", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md328", null ]
      ] ],
      [ "Feature: Scratch Space (In Progress)", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md329", null ],
      [ "Common Workflows", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md330", [
        [ "Workflow 1: Overworld Tile Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md331", null ],
        [ "Workflow 2: Tile16 Blockset Selection", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md332", null ],
        [ "Workflow 3: Graphics Sheet Display", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md333", null ]
      ] ],
      [ "Configuration", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md334", [
        [ "Grid Settings", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md335", null ],
        [ "Scale Settings", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md336", null ],
        [ "Interaction Settings", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md337", null ],
        [ "Large Map Settings", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md338", null ]
      ] ],
      [ "Known Issues", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md339", [
        [ "⚠️ Rectangle Selection Wrapping", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md340", null ]
      ] ],
      [ "API Reference", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md341", [
        [ "Drawing Methods", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md342", null ],
        [ "State Accessors", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md343", null ],
        [ "Configuration", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md344", null ]
      ] ],
      [ "Implementation Notes", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md345", [
        [ "Points Management", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md346", null ],
        [ "Selection State", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md347", null ],
        [ "Overworld Rectangle Painting Flow", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md348", null ]
      ] ],
      [ "Best Practices", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md349", [
        [ "DO ✅", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md350", null ],
        [ "DON'T ❌", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md351", null ]
      ] ],
      [ "Troubleshooting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md352", [
        [ "Issue: Rectangle wraps at boundaries", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md353", null ],
        [ "Issue: Painting in wrong location", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md354", null ],
        [ "Issue: Array index out of bounds", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md355", null ],
        [ "Issue: Forgot to call End()", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md356", null ]
      ] ],
      [ "Future: Scratch Space", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md357", null ],
      [ "Summary", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md358", null ]
    ] ],
    [ "Roadmap", "d6/db4/md_docs_2D1-roadmap.html", [
      [ "Current Focus: Stability & Core Features", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md360", null ],
      [ "0.4.X (Next Major Release) - Stability & Core Tooling", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md362", [
        [ "Priority 1: Testing & Stability (BLOCKER)", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md363", null ],
        [ "Priority 2: <tt>z3ed</tt> AI Agent", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md364", null ],
        [ "Priority 3: Editor Features", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md365", null ]
      ] ],
      [ "0.5.X - Feature Expansion", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md367", null ],
      [ "0.6.X - Polish & Integration", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md369", null ],
      [ "0.7.X and Beyond - Path to 1.0", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md371", null ]
    ] ],
    [ "Yaze Dungeon Editor: Master Guide", "d5/dde/md_docs_2dungeon__editor__master__guide.html", [
      [ "1. Current Status & Known Issues", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md373", [
        [ "Next Steps", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md374", null ]
      ] ],
      [ "2. Architecture", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md375", [
        [ "Core Components (Backend)", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md376", null ],
        [ "UI Components (Frontend)", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md377", null ]
      ] ],
      [ "3. ROM Internals & Data Structures", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md378", [
        [ "Object Encoding", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md379", null ],
        [ "Object Types & Examples", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md380", null ],
        [ "Core Data Tables in ROM", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md381", null ]
      ] ],
      [ "4. User Interface and Usage", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md382", [
        [ "Coordinate System", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md383", null ],
        [ "Usage Examples", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md384", null ]
      ] ],
      [ "5. Testing", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md385", [
        [ "How to Run Tests", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md386", null ]
      ] ],
      [ "6. Dungeon Object Reference Tables", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md387", [
        [ "Type 1 Object Reference Table", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md388", null ],
        [ "Type 2 Object Reference Table", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md389", null ],
        [ "Type 3 Object Reference Table", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md390", null ]
      ] ]
    ] ],
    [ "Dungeon Editor Test Coverage Report", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html", [
      [ "Test Summary", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md392", null ],
      [ "Detailed Test Coverage", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md393", [
        [ "Unit Tests (14/14 PASSING) ✅", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md394", null ],
        [ "Integration Tests (14/14 PASSING) ✅", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md395", [
          [ "✅ All Tests Passing (14)", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md396", null ],
          [ "✅ Previously Failing - All Fixed!", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md397", null ]
        ] ],
        [ "E2E Tests (1 Test) ✅", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md398", null ]
      ] ],
      [ "Coverage by Feature", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md399", [
        [ "Core Functionality", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md400", null ],
        [ "Code Coverage Estimate", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md401", null ]
      ] ],
      [ "Test Infrastructure", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md402", [
        [ "Real ROM Integration", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md403", null ],
        [ "ImGui Test Engine Integration", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md404", null ],
        [ "Test Organization", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md405", null ]
      ] ],
      [ "Running Tests", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md406", [
        [ "All Tests", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md407", null ],
        [ "Unit Tests Only", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md408", null ],
        [ "Integration Tests Only", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md409", null ],
        [ "E2E Tests (GUI Mode)", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md410", null ],
        [ "Specific Test", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md411", null ]
      ] ],
      [ "Next Steps", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md412", [
        [ "Priority 1: Fix Failing Tests", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md413", null ],
        [ "Priority 2: Add More E2E Tests", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md414", null ],
        [ "Priority 3: Performance Tests", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md415", null ]
      ] ],
      [ "Conclusion", "dd/d40/md_docs_2DUNGEON__TEST__COVERAGE.html#autotoc_md416", null ]
    ] ],
    [ "Dungeon Testing Results - October 4, 2025", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html", [
      [ "Executive Summary", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md418", null ],
      [ "Test Execution Results", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md419", [
        [ "Unit Tests Status", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md420", [
          [ "✅ PASSING Tests (28/28)", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md421", null ],
          [ "❌ FAILING Tests (13/28)", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md422", null ]
        ] ],
        [ "Integration Tests Status", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md423", [
          [ "❌ CRASHED Tests", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md424", null ]
        ] ]
      ] ],
      [ "Code Cross-Reference: yaze vs ZScream", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md425", [
        [ "Room Implementation Comparison", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md426", [
          [ "✅ Implemented Features in yaze", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md427", null ],
          [ "⚠️ Partially Implemented Features", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md428", null ],
          [ "❌ Missing/Unclear Features", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md429", null ]
        ] ],
        [ "RoomObject Implementation Comparison", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md430", [
          [ "✅ Implemented in yaze", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md431", null ],
          [ "⚠️ Partial/Different Implementation", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md432", null ],
          [ "❌ Missing Features", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md433", null ]
        ] ]
      ] ],
      [ "Object Encoding/Decoding Analysis", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md434", [
        [ "ZScream Implementation (Room.cs:721-838)", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md435", null ],
        [ "yaze Implementation Status", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md436", null ]
      ] ],
      [ "Critical Issues Found", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md437", [
        [ "1. Integration Test Approach Changed ✅ RESOLVED", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md438", null ],
        [ "2. Object Encoding/Decoding ✅ VERIFIED (RESOLVED)", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md439", null ],
        [ "3. E2E Tests Not Adapted (MEDIUM)", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md440", null ]
      ] ],
      [ "Recommendations", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md441", [
        [ "Immediate Actions (This Week)", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md442", null ],
        [ "Short-term Actions (Next 2 Weeks)", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md443", null ],
        [ "Medium-term Actions (Next Month)", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md444", null ]
      ] ],
      [ "Test Coverage Summary", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md445", [
        [ "Current Coverage (Updated Oct 4, 2025 - Final)", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md446", null ],
        [ "Target Coverage (Per Testing Strategy)", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md447", null ]
      ] ],
      [ "Feature Completeness Analysis", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md448", [
        [ "Core Dungeon Features", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md449", null ],
        [ "Object Editing Features", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md450", null ],
        [ "Advanced Features", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md451", null ]
      ] ],
      [ "Next Steps", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md452", null ],
      [ "Conclusion", "df/dec/md_docs_2DUNGEON__TESTING__RESULTS.html#autotoc_md453", null ]
    ] ],
    [ "Asm Style Guide", "d7/d9a/md_docs_2E1-asm-style-guide.html", [
      [ "Table of Contents", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md456", null ],
      [ "File Structure", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md457", null ],
      [ "Labels and Symbols", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md458", null ],
      [ "Comments", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md459", null ],
      [ "Instructions", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md460", null ],
      [ "Macros", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md461", null ],
      [ "Loops and Branching", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md462", null ],
      [ "Data Structures", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md463", null ],
      [ "Code Organization", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md464", null ],
      [ "Custom Code", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md465", null ]
    ] ],
    [ "Dungeon Editor Design Plan", "dc/d82/md_docs_2E3-dungeon-editor-design.html", [
      [ "Overview", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md467", null ],
      [ "Architecture Overview", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md468", [
        [ "Core Components", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md469", null ],
        [ "File Structure", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md470", null ]
      ] ],
      [ "Component Responsibilities", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md471", [
        [ "DungeonEditor (Main Orchestrator)", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md472", null ],
        [ "DungeonRoomSelector", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md473", null ],
        [ "DungeonCanvasViewer", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md474", null ],
        [ "DungeonObjectSelector", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md475", null ]
      ] ],
      [ "Data Flow", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md476", [
        [ "Initialization Flow", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md477", null ],
        [ "Runtime Data Flow", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md478", null ],
        [ "Key Data Structures", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md479", null ]
      ] ],
      [ "Integration Patterns", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md480", [
        [ "Component Communication", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md481", null ],
        [ "ROM Data Management", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md482", null ],
        [ "State Synchronization", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md483", null ]
      ] ],
      [ "UI Layout Architecture", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md484", [
        [ "3-Column Layout", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md485", null ],
        [ "Component Internal Layout", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md486", null ]
      ] ],
      [ "Coordinate System", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md487", [
        [ "Room Coordinates vs Canvas Coordinates", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md488", null ],
        [ "Bounds Checking", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md489", null ]
      ] ],
      [ "Error Handling & Validation", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md490", [
        [ "ROM Validation", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md491", null ],
        [ "Bounds Validation", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md492", null ]
      ] ],
      [ "Performance Considerations", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md493", [
        [ "Caching Strategy", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md494", null ],
        [ "Rendering Optimization", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md495", null ]
      ] ],
      [ "Testing Strategy", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md496", [
        [ "Integration Tests", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md497", null ],
        [ "Test Categories", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md498", null ]
      ] ],
      [ "Future Development Guidelines", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md499", [
        [ "Adding New Features", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md500", null ],
        [ "Component Extension Patterns", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md501", null ],
        [ "Data Flow Extension", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md502", null ],
        [ "UI Layout Extension", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md503", null ]
      ] ],
      [ "Common Pitfalls & Solutions", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md504", [
        [ "Memory Management", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md505", null ],
        [ "Coordinate System", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md506", null ],
        [ "State Synchronization", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md507", null ],
        [ "Performance Issues", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md508", null ]
      ] ],
      [ "Debugging Tools", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md509", [
        [ "Debug Popup", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md510", null ],
        [ "Logging", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md511", null ]
      ] ],
      [ "Build Integration", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md512", [
        [ "CMake Configuration", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md513", null ],
        [ "Dependencies", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md514", null ]
      ] ],
      [ "Conclusion", "dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md515", null ]
    ] ],
    [ "Tile16 Editor Palette System", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html", [
      [ "Executive Summary", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md517", null ],
      [ "Problem Analysis", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md518", [
        [ "Critical Issues Identified", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md519", null ]
      ] ],
      [ "Solution Architecture", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md520", [
        [ "Core Design Principles", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md521", null ],
        [ "256-Color Overworld Palette Structure", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md522", null ],
        [ "Sheet-to-Palette Mapping", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md523", null ],
        [ "Palette Button Mapping", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md524", null ]
      ] ],
      [ "Implementation Details", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md525", [
        [ "1. Fixed SetPaletteWithTransparent Method", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md526", null ],
        [ "2. Corrected Tile16 Editor Palette System", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md527", null ],
        [ "3. Palette Coordination Flow", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md528", null ]
      ] ],
      [ "UI/UX Refactoring", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md529", [
        [ "New Three-Column Layout", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md530", null ],
        [ "Canvas Context Menu Fixes", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md531", null ],
        [ "Dynamic Zoom Controls", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md532", null ]
      ] ],
      [ "Testing Protocol", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md533", [
        [ "Crash Prevention Testing", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md534", null ],
        [ "Color Alignment Testing", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md535", null ],
        [ "UI/UX Testing", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md536", null ]
      ] ],
      [ "Error Handling", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md537", [
        [ "Bounds Checking", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md538", null ],
        [ "Fallback Mechanisms", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md539", null ]
      ] ],
      [ "Debug Information Display", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md540", null ],
      [ "Known Issues and Ongoing Work", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md541", [
        [ "Completed Items ✅", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md542", null ],
        [ "Active Issues ⚠️", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md543", null ],
        [ "Current Status Summary", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md544", null ],
        [ "Future Enhancements", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md545", null ]
      ] ],
      [ "Maintenance Notes", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md546", null ],
      [ "Next Steps", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md548", [
        [ "Immediate Priorities", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md549", null ],
        [ "Investigation Areas", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md550", null ]
      ] ]
    ] ],
    [ "Overworld Loading Guide", "dc/d12/md_docs_2F1-overworld-loading.html", [
      [ "Table of Contents", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md553", null ],
      [ "Overview", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md554", null ],
      [ "ROM Types and Versions", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md555", [
        [ "Version Detection", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md556", null ],
        [ "Feature Support by Version", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md557", null ]
      ] ],
      [ "Overworld Map Structure", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md558", [
        [ "Core Properties", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md559", null ]
      ] ],
      [ "Overlays and Special Area Maps", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md560", [
        [ "Understanding Overlays", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md561", null ],
        [ "Special Area Maps (0x80-0x9F)", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md562", null ],
        [ "Overlay ID Mappings", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md563", null ],
        [ "Drawing Order", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md564", null ],
        [ "Vanilla Overlay Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md565", null ],
        [ "Special Area Graphics Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md566", null ]
      ] ],
      [ "Loading Process", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md567", [
        [ "1. Version Detection", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md568", null ],
        [ "2. Map Initialization", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md569", null ],
        [ "3. Property Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md570", [
          [ "Vanilla ROMs (asm_version == 0xFF)", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md571", null ],
          [ "ZSCustomOverworld v2/v3", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md572", null ]
        ] ],
        [ "4. Custom Data Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md573", null ]
      ] ],
      [ "ZScream Implementation", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md574", [
        [ "OverworldMap Constructor", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md575", null ],
        [ "Key Methods", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md576", null ]
      ] ],
      [ "Yaze Implementation", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md577", [
        [ "OverworldMap Constructor", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md578", null ],
        [ "Key Methods", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md579", null ],
        [ "Current Status", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md580", null ]
      ] ],
      [ "Key Differences", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md581", [
        [ "1. Language and Architecture", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md582", null ],
        [ "2. Data Structures", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md583", null ],
        [ "3. Error Handling", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md584", null ],
        [ "4. Graphics Processing", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md585", null ]
      ] ],
      [ "Common Issues and Solutions", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md586", [
        [ "1. Version Detection Issues", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md587", null ],
        [ "2. Palette Loading Errors", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md588", null ],
        [ "3. Graphics Not Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md589", null ],
        [ "4. Overlay Issues", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md590", null ],
        [ "5. Large Map Problems", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md591", null ],
        [ "6. Special Area Graphics Issues", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md592", null ]
      ] ],
      [ "Best Practices", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md593", [
        [ "1. Version-Specific Code", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md594", null ],
        [ "2. Error Handling", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md595", null ],
        [ "3. Memory Management", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md596", null ],
        [ "4. Thread Safety", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md597", null ]
      ] ],
      [ "Conclusion", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md598", null ]
    ] ],
    [ "Graphics System Performance & Optimization", "db/dc2/md_docs_2GRAPHICS__PERFORMANCE.html", [
      [ "1. Executive Summary", "db/dc2/md_docs_2GRAPHICS__PERFORMANCE.html#autotoc_md600", [
        [ "Overall Performance Results", "db/dc2/md_docs_2GRAPHICS__PERFORMANCE.html#autotoc_md601", null ]
      ] ],
      [ "2. Implemented Optimizations", "db/dc2/md_docs_2GRAPHICS__PERFORMANCE.html#autotoc_md602", null ],
      [ "3. Future Optimization Recommendations", "db/dc2/md_docs_2GRAPHICS__PERFORMANCE.html#autotoc_md603", [
        [ "High Priority", "db/dc2/md_docs_2GRAPHICS__PERFORMANCE.html#autotoc_md604", null ],
        [ "Medium Priority", "db/dc2/md_docs_2GRAPHICS__PERFORMANCE.html#autotoc_md605", null ]
      ] ]
    ] ],
    [ "yaze Documentation", "d3/d4c/md_docs_2index.html", [
      [ "Quick Start", "d3/d4c/md_docs_2index.html#autotoc_md607", null ],
      [ "Development", "d3/d4c/md_docs_2index.html#autotoc_md608", null ],
      [ "Technical Documentation", "d3/d4c/md_docs_2index.html#autotoc_md609", [
        [ "Assembly & Code", "d3/d4c/md_docs_2index.html#autotoc_md610", null ],
        [ "Editor Systems", "d3/d4c/md_docs_2index.html#autotoc_md611", null ],
        [ "Overworld System", "d3/d4c/md_docs_2index.html#autotoc_md612", null ]
      ] ],
      [ "Key Features", "d3/d4c/md_docs_2index.html#autotoc_md613", null ]
    ] ],
    [ "Ollama Integration Status - Updated# Ollama Integration Status", "da/d40/md_docs_2ollama__integration__status.html", [
      [ "✅ Completed## ✅ Completed", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md616", [
        [ "Infrastructure### Flag Parsing", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md617", null ],
        [ "Current Issue: Empty Tool Results### Tool System", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md618", null ]
      ] ],
      [ "🎨 New Features Added  - OR doesn't provide a <tt>text_response</tt> field in the JSON", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md619", [
        [ "Verbose Mode**Solution Needed**: Update system prompt to include explicit instructions like:", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md620", null ],
        [ "Priority 2: Refine Prompts (MEDIUM)- ✅ Model loads (qwen2.5-coder:7b)", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md621", null ],
        [ "Priority 3: Documentation (LOW)```", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md622", null ]
      ] ],
      [ "🧪 Testing Commands### Priority 1: Fix Tool Calling Loop (High Priority)", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md623", null ],
      [ "📊 Performance   - codellama:7b", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md624", null ],
      [ "🎯 Success Criteria", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md625", [
        [ "Priority 3: Documentation (Low Priority)", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md626", null ]
      ] ],
      [ "🐛 Known Issues", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md627", null ],
      [ "💡 Recommendations", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md628", [
        [ "For Users", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md629", null ],
        [ "For Developers", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md630", null ]
      ] ],
      [ "📝 Related Files", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md631", null ],
      [ "🎯 Success Criteria", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md632", [
        [ "Minimum Viable", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md633", null ],
        [ "Full Success", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md634", null ]
      ] ]
    ] ],
    [ "yaze Overworld Testing Guide", "de/d8a/md_docs_2overworld__testing__guide.html", [
      [ "Overview", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md637", null ],
      [ "Test Architecture", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md638", [
        [ "1. Golden Data System", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md639", null ],
        [ "2. Test Categories", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md640", [
          [ "Unit Tests (<tt>test/unit/zelda3/</tt>)", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md641", null ],
          [ "Integration Tests (<tt>test/e2e/</tt>)", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md642", null ],
          [ "Golden Data Tools", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md643", null ]
        ] ]
      ] ],
      [ "Quick Start", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md644", [
        [ "Prerequisites", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md645", null ],
        [ "Running Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md646", [
          [ "Basic Test Run", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md647", null ],
          [ "Selective Test Execution", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md648", null ]
        ] ]
      ] ],
      [ "Test Components", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md649", [
        [ "1. Golden Data Extractor", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md650", null ],
        [ "2. Integration Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md651", null ],
        [ "3. End-to-End Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md652", null ]
      ] ],
      [ "Test Validation Points", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md653", [
        [ "1. ZScream Compatibility", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md654", null ],
        [ "2. ROM State Validation", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md655", null ],
        [ "3. Performance and Stability", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md656", null ]
      ] ],
      [ "Environment Variables", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md657", [
        [ "Test Configuration", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md658", null ],
        [ "Build Configuration", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md659", null ]
      ] ],
      [ "Test Reports", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md660", [
        [ "Generated Reports", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md661", null ],
        [ "Report Location", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md662", null ]
      ] ],
      [ "Troubleshooting", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md663", [
        [ "Common Issues", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md664", [
          [ "1. ROM Not Found", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md665", null ],
          [ "2. Build Failures", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md666", null ],
          [ "3. Test Failures", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md667", null ]
        ] ],
        [ "Debug Mode", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md668", null ]
      ] ],
      [ "Advanced Usage", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md669", [
        [ "Custom Test Scenarios", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md670", [
          [ "1. Testing Custom ROMs", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md671", null ],
          [ "2. Regression Testing", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md672", null ],
          [ "3. Performance Testing", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md673", null ]
        ] ]
      ] ],
      [ "Integration with CI/CD", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md674", [
        [ "GitHub Actions Example", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md675", null ]
      ] ],
      [ "Contributing", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md676", [
        [ "Adding New Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md677", null ],
        [ "Test Guidelines", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md678", null ],
        [ "Example Test Structure", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md679", null ]
      ] ],
      [ "Conclusion", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md680", null ]
    ] ],
    [ "z3ed: AI-Powered CLI for YAZE", "d3/d63/md_docs_2z3ed_2README.html", [
      [ "1. Overview", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md682", [
        [ "Core Capabilities", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md683", null ]
      ] ],
      [ "2. Quick Start", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md684", [
        [ "Build", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md685", null ],
        [ "AI Setup", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md686", null ],
        [ "Example Commands", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md687", null ]
      ] ],
      [ "3. Architecture", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md688", [
        [ "System Components Diagram", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md689", null ]
      ] ],
      [ "4. Agentic & Generative Workflow (MCP)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md690", null ],
      [ "5. Command Reference", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md691", [
        [ "Agent Commands", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md692", null ],
        [ "Resource Commands", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md693", null ]
      ] ],
      [ "6. Chat Modes", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md694", [
        [ "FTXUI Chat (<tt>agent chat</tt>)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md695", null ],
        [ "Simple Chat (<tt>agent simple-chat</tt>)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md696", null ],
        [ "GUI Chat Widget (Editor Integration Preview)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md697", null ]
      ] ],
      [ "7. AI Provider Configuration", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md698", null ],
      [ "8. CLI Output & Help System", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md699", [
        [ "Verbose Logging", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md700", null ],
        [ "Hierarchical Help System", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md701", null ]
      ] ],
      [ "9. Roadmap & Implementation Status", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md702", [
        [ "✅ Completed", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md703", null ],
        [ "🚧 Active & Next Steps", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md704", null ]
      ] ],
      [ "9. Troubleshooting", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md705", null ]
    ] ],
    [ "yaze - Yet Another Zelda3 Editor", "d0/d30/md_README.html", [
      [ "Version 0.3.2 - Release", "d0/d30/md_README.html#autotoc_md707", null ],
      [ "Quick Start", "d0/d30/md_README.html#autotoc_md712", [
        [ "Build", "d0/d30/md_README.html#autotoc_md713", null ],
        [ "Applications", "d0/d30/md_README.html#autotoc_md714", null ]
      ] ],
      [ "Usage", "d0/d30/md_README.html#autotoc_md715", [
        [ "GUI Editor", "d0/d30/md_README.html#autotoc_md716", null ],
        [ "Command Line Tool", "d0/d30/md_README.html#autotoc_md717", null ],
        [ "C++ API", "d0/d30/md_README.html#autotoc_md718", null ]
      ] ],
      [ "Documentation", "d0/d30/md_README.html#autotoc_md719", null ],
      [ "Supported Platforms", "d0/d30/md_README.html#autotoc_md720", null ],
      [ "ROM Compatibility", "d0/d30/md_README.html#autotoc_md721", null ],
      [ "Contributing", "d0/d30/md_README.html#autotoc_md722", null ],
      [ "License", "d0/d30/md_README.html#autotoc_md723", null ],
      [ "🙏 Acknowledgments", "d0/d30/md_README.html#autotoc_md724", null ],
      [ "📸 Screenshots", "d0/d30/md_README.html#autotoc_md725", null ]
    ] ],
    [ "yaze Build Scripts", "de/d82/md_scripts_2README.html", [
      [ "Windows Scripts", "de/d82/md_scripts_2README.html#autotoc_md728", [
        [ "vcpkg Setup (Optional)", "de/d82/md_scripts_2README.html#autotoc_md729", null ]
      ] ],
      [ "Windows Build Workflow", "de/d82/md_scripts_2README.html#autotoc_md730", [
        [ "Recommended: Visual Studio CMake Mode", "de/d82/md_scripts_2README.html#autotoc_md731", null ],
        [ "Command Line Build", "de/d82/md_scripts_2README.html#autotoc_md732", null ],
        [ "Compiler Notes", "de/d82/md_scripts_2README.html#autotoc_md733", null ]
      ] ],
      [ "Quick Start (Windows)", "de/d82/md_scripts_2README.html#autotoc_md734", [
        [ "Option 1: Visual Studio (Recommended)", "de/d82/md_scripts_2README.html#autotoc_md735", null ],
        [ "Option 2: Command Line", "de/d82/md_scripts_2README.html#autotoc_md736", null ],
        [ "Option 3: With vcpkg (Optional)", "de/d82/md_scripts_2README.html#autotoc_md737", null ]
      ] ],
      [ "Troubleshooting", "de/d82/md_scripts_2README.html#autotoc_md738", [
        [ "Common Issues", "de/d82/md_scripts_2README.html#autotoc_md739", null ],
        [ "Getting Help", "de/d82/md_scripts_2README.html#autotoc_md740", null ]
      ] ],
      [ "Other Scripts", "de/d82/md_scripts_2README.html#autotoc_md741", null ],
      [ "Build Environment Verification", "de/d82/md_scripts_2README.html#autotoc_md742", [
        [ "<tt>verify-build-environment.ps1</tt> / <tt>.sh</tt>", "de/d82/md_scripts_2README.html#autotoc_md743", null ],
        [ "Usage", "de/d82/md_scripts_2README.html#autotoc_md744", null ]
      ] ]
    ] ],
    [ "End-to-End (E2E) Tests", "d9/db0/md_test_2e2e_2README.html", [
      [ "Active Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md746", [
        [ "✅ Working Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md747", null ],
        [ "📝 Dungeon Editor Smoke Test", "d9/db0/md_test_2e2e_2README.html#autotoc_md748", null ]
      ] ],
      [ "Running Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md749", [
        [ "All E2E Tests (GUI Mode)", "d9/db0/md_test_2e2e_2README.html#autotoc_md750", null ],
        [ "Specific Test Category", "d9/db0/md_test_2e2e_2README.html#autotoc_md751", null ],
        [ "Dungeon Editor Test Only", "d9/db0/md_test_2e2e_2README.html#autotoc_md752", null ]
      ] ],
      [ "Test Development", "d9/db0/md_test_2e2e_2README.html#autotoc_md753", [
        [ "Creating New Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md754", null ],
        [ "Register in yaze_test.cc", "d9/db0/md_test_2e2e_2README.html#autotoc_md755", null ],
        [ "ImGui Test Engine API", "d9/db0/md_test_2e2e_2README.html#autotoc_md756", null ]
      ] ],
      [ "Test Logging", "d9/db0/md_test_2e2e_2README.html#autotoc_md757", null ],
      [ "Test Infrastructure", "d9/db0/md_test_2e2e_2README.html#autotoc_md758", [
        [ "File Organization", "d9/db0/md_test_2e2e_2README.html#autotoc_md759", null ],
        [ "Helper Functions", "d9/db0/md_test_2e2e_2README.html#autotoc_md760", null ]
      ] ],
      [ "Future Test Ideas", "d9/db0/md_test_2e2e_2README.html#autotoc_md761", null ],
      [ "Troubleshooting", "d9/db0/md_test_2e2e_2README.html#autotoc_md762", [
        [ "Test Crashes in GUI Mode", "d9/db0/md_test_2e2e_2README.html#autotoc_md763", null ],
        [ "Tests Not Found", "d9/db0/md_test_2e2e_2README.html#autotoc_md764", null ],
        [ "ImGui Items Not Found", "d9/db0/md_test_2e2e_2README.html#autotoc_md765", null ]
      ] ],
      [ "References", "d9/db0/md_test_2e2e_2README.html#autotoc_md766", null ],
      [ "Status", "d9/db0/md_test_2e2e_2README.html#autotoc_md767", null ]
    ] ],
    [ "yaze Test Suite", "d0/d46/md_test_2README.html", [
      [ "Directory Structure", "d0/d46/md_test_2README.html#autotoc_md769", null ],
      [ "Test Categories", "d0/d46/md_test_2README.html#autotoc_md770", [
        [ "Unit Tests (<tt>unit/</tt>)", "d0/d46/md_test_2README.html#autotoc_md771", null ],
        [ "Integration Tests (<tt>integration/</tt>)", "d0/d46/md_test_2README.html#autotoc_md772", null ],
        [ "End-to-End Tests (<tt>e2e/</tt>)", "d0/d46/md_test_2README.html#autotoc_md773", null ]
      ] ],
      [ "Enhanced Test Runner", "d0/d46/md_test_2README.html#autotoc_md774", [
        [ "Usage Examples", "d0/d46/md_test_2README.html#autotoc_md775", null ],
        [ "Test Modes", "d0/d46/md_test_2README.html#autotoc_md776", null ],
        [ "Options", "d0/d46/md_test_2README.html#autotoc_md777", null ]
      ] ],
      [ "E2E ROM Testing", "d0/d46/md_test_2README.html#autotoc_md778", [
        [ "Test Cases", "d0/d46/md_test_2README.html#autotoc_md779", null ]
      ] ],
      [ "ZSCustomOverworld Upgrade Testing", "d0/d46/md_test_2README.html#autotoc_md780", [
        [ "Supported Upgrades", "d0/d46/md_test_2README.html#autotoc_md781", null ],
        [ "Test Cases", "d0/d46/md_test_2README.html#autotoc_md782", null ],
        [ "Version-Specific Features", "d0/d46/md_test_2README.html#autotoc_md783", [
          [ "Vanilla", "d0/d46/md_test_2README.html#autotoc_md784", null ],
          [ "v2", "d0/d46/md_test_2README.html#autotoc_md785", null ],
          [ "v3", "d0/d46/md_test_2README.html#autotoc_md786", null ]
        ] ]
      ] ],
      [ "Environment Variables", "d0/d46/md_test_2README.html#autotoc_md787", null ],
      [ "CI/CD Integration", "d0/d46/md_test_2README.html#autotoc_md788", null ],
      [ "Deprecated Tests", "d0/d46/md_test_2README.html#autotoc_md789", null ],
      [ "Best Practices", "d0/d46/md_test_2README.html#autotoc_md790", null ],
      [ "AI Agent Testing", "d0/d46/md_test_2README.html#autotoc_md791", null ]
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
"d0/d27/namespaceyaze_1_1gfx.html#a7894b061b9f2f6302a6ae6e6a1dc21e5",
"d0/d30/md_README.html#autotoc_md713",
"d0/d77/file__dialog_8h_source.html",
"d0/dcb/structyaze_1_1cli_1_1overworld_1_1WarpQuery.html#ac1202e1db0ca9588a0a5c2b92cfab918",
"d1/d12/test__common_8h.html",
"d1/d36/dungeon__object__renderer__integration__test_8cc.html#a1515efb74afb3627af93c4279e14b7e8",
"d1/d47/structyaze_1_1core_1_1ProjectManager_1_1ProjectTemplate.html",
"d1/d71/classyaze_1_1util_1_1FlagParser.html#ab39a3030c2b11cf49a6ae7a796079b8a",
"d1/daf/compression_8h.html#aabcd25b770ebcbf38536778c3b1ffa3b",
"d1/dec/structyaze_1_1gfx_1_1PaletteGroup.html#afa676ebbdcba9304ec77b572a2a218c4",
"d2/d07/classyaze_1_1emu_1_1Spc700.html#a838a7624981aaa832edb8219aa7deb49",
"d2/d2a/classyaze_1_1cli_1_1AIService.html#a55b5fceb48e11c1ffc74820689e3cae0",
"d2/d5e/classyaze_1_1emu_1_1Memory.html#a7e6cf763ff5392d59a874e22f80bd2b9",
"d2/dba/dungeon__map_8h.html",
"d2/dea/structyaze_1_1gfx_1_1AtlasStats.html#ad617304d6320323214ac7c542367e700",
"d2/dfe/structyaze_1_1gui_1_1EnhancedTheme.html#a76a193e290cf7c50ad6975d9cc38ec38",
"d3/d15/classyaze_1_1emu_1_1Snes.html#ab97c6abec253b714f5fa1710477a38c7",
"d3/d27/classyaze_1_1gui_1_1ThemeManager.html#af1f03f4506065aaafe8efd13d5764000",
"d3/d44/classyaze_1_1editor_1_1OverworldEditor.html#a72df3470e3eb8b94765a4879c689a9d2",
"d3/d49/md_docs_2B4-release-workflows.html#autotoc_md206",
"d3/d6c/classyaze_1_1editor_1_1MessageEditor.html#a7c57b31fc61d0a6512ab1264142e70cb",
"d3/d8f/structyaze_1_1gui_1_1canvas_1_1CanvasConfig.html",
"d3/da7/structyaze_1_1gfx_1_1lc__lz2_1_1CompressionContext.html#a6788217347f010f3504c88c440f87589",
"d3/dc3/structyaze_1_1test_1_1HarnessTestExecution.html#aa894de1f6bfc31e06252b075e2d08ee2",
"d3/ded/classyaze_1_1emu_1_1Ppu.html#aa59a78abcdf4a2ef17feb401b7a3be65",
"d4/d0a/namespaceyaze_1_1test.html#a2920b49cde487363eb92e004de5c3a86",
"d4/d2a/asar__integration__test_8cc_source.html",
"d4/d6f/classyaze_1_1gfx_1_1PerformanceDashboard.html#ac85ac81b9997b5bb843ccb2de2276a69",
"d4/d9e/macro_8h.html#a6e9350746df40af9eff68f6fe9aff386",
"d4/dda/structyaze_1_1emu_1_1CGWSEL.html#aa172003176fca60ad21ad1a233b41823",
"d5/d1c/dungeon__component__unit__test_8cc.html#a8df6d3dd7e2abc4a07bd882bdfd8a004",
"d5/d1f/namespaceyaze_1_1zelda3.html#a693df677b1b2754452c8b8b4c4a98468a766ec8e12d24c885845b9beabaaf69f0",
"d5/d1f/namespaceyaze_1_1zelda3.html#ad9d8ea0a6ed16b70386c881af5ae07bc",
"d5/d5d/structyaze_1_1cli_1_1ResourceAction_1_1ReturnValue.html#addb9095b86d9d25ad3de5fc660512941",
"d5/d95/structyaze_1_1zelda3_1_1DungeonEditorSystem_1_1DungeonSettings.html#a7e9504268fddbb327aba7c85abf51d32",
"d5/dc2/classyaze_1_1gui_1_1BackgroundRenderer.html#ab7ff8d735f6d3af29ec3382da3c2582d",
"d6/d04/structyaze_1_1emu_1_1WOBJSEL.html#a8fc7b10b7a26459e46f28219cae32cab",
"d6/d2e/classyaze_1_1zelda3_1_1TitleScreen.html#a97cf6e74e226a371361612441d8d49ab",
"d6/d3c/classyaze_1_1editor_1_1MapPropertiesSystem.html#a521b96441ffdcc88c5472fe2ee443a94",
"d6/d74/rom__test_8cc.html#a64f91270e5fa8d506ccdba67987ca733",
"d6/db1/classyaze_1_1zelda3_1_1Sprite.html#ae0fcfdbfcab9c7edf416300229d60104",
"d6/de0/group__graphics.html#ga7010d1b3caf12995e0ecb41a1c446b94",
"d7/d47/classyaze_1_1cli_1_1PaletteImport.html",
"d7/d61/classyaze_1_1zelda3_1_1DungeonEditorSystem.html#a5258149cc3fcadae4d048a5507fde904",
"d7/d61/classyaze_1_1zelda3_1_1DungeonEditorSystem.html#afb42da7c0633b93f324cf42ac7257faf",
"d7/d9c/overworld_8h.html#a128cf7051a6ae5627ee49c88d6ecce26",
"d7/dca/structyaze_1_1editor_1_1DictionaryEntry.html",
"d7/df6/classyaze_1_1zelda3_1_1Room.html#a7e231eabce5e285ddec8de4d0b01fc93",
"d7/dfb/classyaze_1_1editor_1_1OverworldEditorManager.html#a39946fd6e6c2e8dcf410dc80f530b464",
"d8/d1c/structyaze_1_1cli_1_1Tile16Proposal.html#a86717cf96ce8a4fe63906f2a47760983ac69f06e1a9b016d133907b4e5f5864d2",
"d8/d5f/classyaze_1_1zelda3_1_1ObjectRenderer_1_1MemoryPool.html#a317296d8c7fb32ebbf5f42991530e19a",
"d8/d98/classyaze_1_1editor_1_1DungeonEditor.html#a9c06190d425a4336cd62e01a6a861016",
"d8/dc3/classyaze_1_1zelda3_1_1DungeonObjectRendererIntegrationTest.html#a4005e44d8e80ed2ec1e086b015d67132",
"d8/ddb/snes__palette_8h.html#a82a8956476ffc04750bcfc4120c8b8dba88eb78a13c2d02166e0cf9c2fdd458c4",
"d9/d2b/classyaze_1_1editor_1_1AgentChatWidget.html#af68620c0a9fe0b9a3cfe7d8c103c6225",
"d9/d70/classyaze_1_1zelda3_1_1RoomLayout.html#afd05fd7a8057f15f6c8e5bf3202592e1",
"d9/dc0/room_8h.html#a16a549dbe81fe1cb9d30e7df88fed890",
"d9/dc5/classyaze_1_1zelda3_1_1Overworld.html#a8372eafa3716fef61d318e210a68db3e",
"d9/dcc/classyaze_1_1zelda3_1_1OverworldMap.html#a95d451b92008cb4be5638afa811ef021",
"d9/dfe/classyaze_1_1gui_1_1WidgetIdRegistry.html#a476a252ad390c80bcf81446dcfe2142a",
"da/d2c/classyaze_1_1gui_1_1Canvas.html#a49a4f5576432d7bcfd218573304c4120",
"da/d30/classyaze_1_1cli_1_1SpriteCreate.html",
"da/d45/classyaze_1_1editor_1_1DungeonEditorV2.html#a3a95ccfe203e1233cec81d3a6f60baa3",
"da/d68/classyaze_1_1gfx_1_1SnesPalette.html#a837443013432f47923f16a4d671f1563",
"da/db1/namespaceyaze_1_1gfx_1_1palette__group__internal.html#a801bac1695e532e582be91b783a21768",
"da/de5/rom_8h.html#a70210423b80d30a75edab49bddef04d0",
"db/d2c/classyaze_1_1gui_1_1AutoWidgetScope.html#aa493ab837e9cd0992ecd3e1e789cf1b4",
"db/d71/classyaze_1_1editor_1_1GfxGroupEditor.html#a13c8d98d68d6e3d14339197da4fbec65",
"db/d82/classyaze_1_1editor_1_1Tile16Editor.html#adc21617a39533d791e6583a0d8d1a283",
"db/da4/classyaze_1_1emu_1_1MockMemory.html#aeabd0cf0d7350b215f85fdf1a2973d2c",
"db/dd7/classyaze_1_1zelda3_1_1ObjectRenderer_1_1GraphicsCache.html#a5c524764bb5c523bb31b2d6c6eb0653d",
"dc/d0a/structyaze_1_1test_1_1DungeonObjectRenderingTests_1_1DungeonScenario.html#aab4d75df7876cf29544b299b3802905c",
"dc/d31/classyaze_1_1editor_1_1GraphicsEditor.html#a6df417c71b3d310543f5bf5ba808ac1f",
"dc/d4f/classyaze_1_1editor_1_1DungeonObjectSelector.html#a2c93195d7cefad7deb3e6562fadd05a7",
"dc/d82/md_docs_2E3-dungeon-editor-design.html#autotoc_md476",
"dc/de0/classyaze_1_1test_1_1ArenaTestSuite.html#a4369c935e5b96dda483dd0c7fe2efb6e",
"dc/df4/classyaze_1_1emu_1_1Cpu.html#a6fae76a69e23ddce4bf89a1073581275",
"dc/df4/classyaze_1_1emu_1_1Cpu.html#aea682e47ae0952d8dfb1de5ad2eb6d3f",
"dd/d12/classyaze_1_1editor_1_1EditorManager.html#aa59de04fd9fec039cb3b962ecd4210ab",
"dd/d3f/structyaze_1_1zelda3_1_1MouseConfig.html#a21e709f32270f90042254282dd90df40",
"dd/d63/namespaceyaze_1_1cli.html#a0be616999fa8e675f482f53ca1830fcea2146194a4ced927d6327c1625c175554",
"dd/d80/structyaze_1_1editor_1_1AgentChatWidget_1_1CollaborationCallbacks.html#ace95643922108519b5e685056d7497e6",
"dd/dde/editor_2system_2agent__chat__widget_8cc_source.html",
"dd/dfa/classyaze_1_1test_1_1E2ERomDependentTest.html#a4924d00fea7cc331fe75eaa49d827ca4",
"de/d12/style_8h.html#a92b3511aa78e6fcaaa782d8ca28390eb",
"de/d76/classyaze_1_1editor_1_1SpriteEditor.html#aba67b34610ea4479d2c84b0e49e5cebf",
"de/da1/classyaze_1_1cli_1_1TestWorkflowGenerator.html#a907325fea4d9340b421b0064bc13f5b5",
"de/dbf/icons_8h.html#a1029c64fcff26409cd6a32080bf6dbe3",
"de/dbf/icons_8h.html#a2eac11554c9ef7f6ccc77b14ccc55b16",
"de/dbf/icons_8h.html#a4b540d2523872bf8a69313fb8053093a",
"de/dbf/icons_8h.html#a675fbf502117ad11346b8b4084d03331",
"de/dbf/icons_8h.html#a8776a5225186c803eb91be86ab711b53",
"de/dbf/icons_8h.html#aa65cb0ffa2b62a05f34570f2a9d0e77e",
"de/dbf/icons_8h.html#ac2ddfdf71577de7ea3d0370d78b9e353",
"de/dbf/icons_8h.html#adee4ef5390edbd3f80ee5b837bacaf55",
"de/dbf/icons_8h.html#af9938ba37b2c1f331adf09bcf3132d2b",
"de/de7/classyaze_1_1zelda3_1_1DungeonObjectEditor.html#a3bb51e6e28ec6f0437d1cad1c4a1b379",
"df/d0e/classyaze_1_1gfx_1_1Bitmap.html#a1725878c64e66f79cecade531d14d384",
"df/d26/classyaze_1_1Transaction.html#af75ed2e77634d39ac12a638d021918a3",
"df/da6/namespaceanonymous__namespace_02agent__chat__widget_8cc_03.html#a52ca0dae2c5ebc04512a3390d626d7ea",
"df/ded/terminal__colors_8h.html#a69233182ba3f536d1e14c3e09437cb7d",
"functions_vars_n.html"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';