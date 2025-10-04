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
    [ "Platform Compatibility Improvements", "d1/d30/md_docs_2B2-platform-compatibility.html", [
      [ "Native File Dialog Support", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md116", null ],
      [ "Cross-Platform Build Reliability", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md117", null ],
      [ "Build Configuration Options", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md118", [
        [ "Full Build (Development)", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md119", null ],
        [ "Minimal Build", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md120", null ]
      ] ],
      [ "Implementation Details", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md121", null ],
      [ "Platform-Specific Adaptations", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md122", [
        [ "Windows", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md123", null ],
        [ "macOS", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md124", null ],
        [ "Linux", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md125", null ]
      ] ]
    ] ],
    [ "Build Presets Guide", "d7/d51/md_docs_2B3-build-presets.html", [
      [ "🍎 macOS ARM64 Presets (Recommended for Apple Silicon)", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md127", [
        [ "For Development Work:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md128", null ],
        [ "For Distribution:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md129", null ]
      ] ],
      [ "🔧 Why This Fixes Architecture Errors", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md130", null ],
      [ "📋 Available Presets", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md131", null ],
      [ "🚀 Quick Start", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md132", null ],
      [ "🛠️ IDE Integration", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md133", [
        [ "VS Code with CMake Tools:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md134", null ],
        [ "CLion:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md135", null ],
        [ "Xcode:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md136", null ]
      ] ],
      [ "🔍 Troubleshooting", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md137", null ],
      [ "📝 Notes", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md138", null ]
    ] ],
    [ "Release Workflows Documentation", "d3/d49/md_docs_2B4-release-workflows.html", [
      [ "Overview", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md140", null ],
      [ "1. Release-Simplified (<tt>release-simplified.yml</tt>)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md142", [
        [ "Purpose", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md143", null ],
        [ "Key Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md144", null ],
        [ "Use Cases", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md145", null ],
        [ "Configuration", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md146", null ],
        [ "Platforms Supported", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md147", null ]
      ] ],
      [ "2. Release (<tt>release.yml</tt>)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md149", [
        [ "Purpose", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md150", null ],
        [ "Key Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md151", null ],
        [ "Use Cases", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md152", null ],
        [ "Configuration", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md153", null ],
        [ "Advantages over Simplified", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md154", null ]
      ] ],
      [ "3. Release-Complex (<tt>release-complex.yml</tt>)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md156", [
        [ "Purpose", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md157", null ],
        [ "Key Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md158", null ],
        [ "Use Cases", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md159", null ],
        [ "Fallback Mechanisms", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md160", [
          [ "vcpkg Fallback", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md161", null ],
          [ "Chocolatey Integration", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md162", null ],
          [ "Build Configuration Fallback", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md163", null ]
        ] ],
        [ "Advanced Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md164", null ]
      ] ],
      [ "Workflow Comparison Matrix", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md166", null ],
      [ "When to Use Each Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md168", [
        [ "Use Simplified When:", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md169", null ],
        [ "Use Release When:", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md170", null ],
        [ "Use Complex When:", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md171", null ]
      ] ],
      [ "Workflow Selection Guide", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md173", [
        [ "For Development Team", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md174", null ],
        [ "For Release Manager", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md175", null ],
        [ "For CI/CD Pipeline", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md176", null ]
      ] ],
      [ "Configuration Examples", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md178", [
        [ "Triggering a Release", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md179", [
          [ "Manual Release (All Workflows)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md180", null ],
          [ "Automatic Release (Tag Push)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md181", null ]
        ] ],
        [ "Customizing Release Notes", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md182", null ]
      ] ],
      [ "Troubleshooting", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md184", [
        [ "Common Issues", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md185", [
          [ "vcpkg Failures (Windows)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md186", null ],
          [ "Dependency Conflicts", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md187", null ],
          [ "Build Failures", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md188", null ]
        ] ],
        [ "Debug Information", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md189", [
          [ "Simplified Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md190", null ],
          [ "Release Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md191", null ],
          [ "Complex Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md192", null ]
        ] ]
      ] ],
      [ "Best Practices", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md194", [
        [ "Workflow Selection", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md195", null ],
        [ "Release Process", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md196", null ],
        [ "Maintenance", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md197", null ]
      ] ],
      [ "Future Improvements", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md199", [
        [ "Planned Enhancements", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md200", null ],
        [ "Integration Opportunities", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md201", null ]
      ] ]
    ] ],
    [ "Stability, Testability & Release Workflow Improvements", "d9/d6b/md_docs_2B5-stability-improvements.html", [
      [ "Recent Improvements (v0.3.2)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md204", [
        [ "Windows Platform Stability", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md205", [
          [ "Stack Size Optimization", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md206", null ],
          [ "Development Utility Isolation", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md207", null ]
        ] ],
        [ "Graphics System Stability", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md208", [
          [ "Segmentation Fault Resolution", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md209", null ],
          [ "Comprehensive Bounds Checking", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md210", null ]
        ] ],
        [ "Build System Improvements", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md211", [
          [ "Modern Windows Workflow", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md212", null ],
          [ "Enhanced CI/CD Reliability", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md213", null ]
        ] ]
      ] ],
      [ "Recommended Optimizations", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md214", [
        [ "High Priority", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md215", [
          [ "1. Lazy Graphics Loading", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md216", null ],
          [ "2. Heap-Based Large Allocations", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md217", null ],
          [ "3. Streaming ROM Assets", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md218", null ]
        ] ],
        [ "Medium Priority", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md219", [
          [ "4. Enhanced Test Isolation", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md220", null ],
          [ "5. Dependency Caching Optimization", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md221", null ],
          [ "6. Memory Pool for Graphics", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md222", null ]
        ] ],
        [ "Low Priority", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md223", [
          [ "7. Build Time Optimization", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md224", null ],
          [ "8. Release Workflow Simplification", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md225", null ]
        ] ]
      ] ],
      [ "Testing Improvements", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md226", [
        [ "Current State", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md227", null ],
        [ "Recommendations", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md228", [
          [ "1. Visual Regression Testing", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md229", null ],
          [ "2. Performance Benchmarks", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md230", null ],
          [ "3. Fuzz Testing", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md231", null ]
        ] ]
      ] ],
      [ "Metrics & Monitoring", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md232", [
        [ "Current Measurements", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md233", null ],
        [ "Target Improvements", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md234", null ]
      ] ],
      [ "Action Items", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md235", [
        [ "Immediate (v0.3.2)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md236", null ],
        [ "Short Term (v0.3.3)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md237", null ],
        [ "Medium Term (v0.4.0)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md238", null ],
        [ "Long Term (v0.5.0+)", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md239", null ]
      ] ],
      [ "Conclusion", "d9/d6b/md_docs_2B5-stability-improvements.html#autotoc_md240", null ]
    ] ],
    [ "Build Modularization Plan for Yaze", "d7/daf/md_docs_2build__modularization__plan.html", [
      [ "Executive Summary", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md242", null ],
      [ "Current Problems", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md243", null ],
      [ "Proposed Architecture", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md244", [
        [ "Library Hierarchy", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md245", null ]
      ] ],
      [ "Implementation Guide", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md246", [
        [ "Quick Start", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md247", [
          [ "Option 1: Enable Modular Build (Recommended)", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md248", null ],
          [ "Option 2: Keep Existing Build (Default)", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md249", null ]
        ] ],
        [ "Implementation Steps", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md250", [
          [ "Step 1: Add Build Option", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md251", null ],
          [ "Step 2: Update src/CMakeLists.txt", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md252", null ],
          [ "Step 3: Update yaze Application Linking", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md253", null ],
          [ "Step 4: Update Test Executable", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md254", null ]
        ] ]
      ] ],
      [ "Expected Performance Improvements", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md255", [
        [ "Build Time Reductions", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md256", null ],
        [ "CI/CD Benefits", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md257", null ]
      ] ],
      [ "Rollback Procedure", "d7/daf/md_docs_2build__modularization__plan.html#autotoc_md258", null ]
    ] ],
    [ "Changelog", "d7/d6f/md_docs_2C1-changelog.html", [
      [ "0.3.2 (December 2025) - In Development", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md260", [
        [ "Tile16 Editor Improvements (In Progress)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md261", null ],
        [ "Graphics System Optimizations", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md262", null ],
        [ "Windows Platform Stability", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md263", null ],
        [ "Memory Safety & Stability", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md264", null ],
        [ "Testing & CI/CD Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md265", null ],
        [ "Future Optimizations (Planned)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md266", null ],
        [ "Technical Notes", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md267", null ]
      ] ],
      [ "0.3.1 (September 2025)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md268", [
        [ "Major Features", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md269", null ],
        [ "Tile16 Editor Enhancements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md270", null ],
        [ "ZSCustomOverworld v3 Implementation", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md271", null ],
        [ "Technical Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md272", null ],
        [ "User Interface", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md273", null ],
        [ "Bug Fixes", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md274", null ],
        [ "ZScream Compatibility Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md275", null ]
      ] ],
      [ "0.3.0 (September 2025)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md276", [
        [ "Major Features", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md277", null ],
        [ "User Interface & Theming", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md278", null ],
        [ "Enhancements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md279", null ],
        [ "Technical Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md280", null ],
        [ "Bug Fixes", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md281", null ]
      ] ],
      [ "0.2.2 (December 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md282", null ],
      [ "0.2.1 (August 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md283", null ],
      [ "0.2.0 (July 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md284", null ],
      [ "0.1.0 (May 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md285", null ],
      [ "0.0.9 (April 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md286", null ],
      [ "0.0.8 (February 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md287", null ],
      [ "0.0.7 (January 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md288", null ],
      [ "0.0.6 (November 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md289", null ],
      [ "0.0.5 (November 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md290", null ],
      [ "0.0.4 (November 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md291", null ],
      [ "0.0.3 (October 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md292", null ]
    ] ],
    [ "Canvas System - Comprehensive Guide", "d2/db2/md_docs_2CANVAS__GUIDE.html", [
      [ "Overview", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md294", null ],
      [ "Core Concepts", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md295", [
        [ "Canvas Structure", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md296", null ],
        [ "Coordinate Systems", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md297", null ]
      ] ],
      [ "Usage Patterns", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md298", [
        [ "Pattern 1: Basic Bitmap Display", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md299", null ],
        [ "Pattern 2: Modern Begin/End", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md300", null ],
        [ "Pattern 3: RAII ScopedCanvas", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md301", null ]
      ] ],
      [ "Feature: Tile Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md302", [
        [ "Single Tile Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md303", null ],
        [ "Tilemap Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md304", null ],
        [ "Color Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md305", null ]
      ] ],
      [ "Feature: Tile Selection", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md306", [
        [ "Single Tile Selection", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md307", null ],
        [ "Multi-Tile Rectangle Selection", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md308", null ],
        [ "Rectangle Drag & Paint", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md309", null ]
      ] ],
      [ "Feature: Custom Overlays", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md310", [
        [ "Manual Points Manipulation", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md311", null ]
      ] ],
      [ "Feature: Large Map Support", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md312", [
        [ "Map Types", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md313", null ],
        [ "Boundary Clamping", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md314", null ],
        [ "Custom Map Sizes", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md315", null ]
      ] ],
      [ "Feature: Context Menu", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md316", [
        [ "Adding Custom Items", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md317", null ],
        [ "Overworld Editor Example", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md318", null ]
      ] ],
      [ "Feature: Scratch Space (In Progress)", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md319", null ],
      [ "Common Workflows", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md320", [
        [ "Workflow 1: Overworld Tile Painting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md321", null ],
        [ "Workflow 2: Tile16 Blockset Selection", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md322", null ],
        [ "Workflow 3: Graphics Sheet Display", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md323", null ]
      ] ],
      [ "Configuration", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md324", [
        [ "Grid Settings", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md325", null ],
        [ "Scale Settings", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md326", null ],
        [ "Interaction Settings", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md327", null ],
        [ "Large Map Settings", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md328", null ]
      ] ],
      [ "Known Issues", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md329", [
        [ "⚠️ Rectangle Selection Wrapping", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md330", null ]
      ] ],
      [ "API Reference", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md331", [
        [ "Drawing Methods", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md332", null ],
        [ "State Accessors", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md333", null ],
        [ "Configuration", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md334", null ]
      ] ],
      [ "Implementation Notes", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md335", [
        [ "Points Management", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md336", null ],
        [ "Selection State", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md337", null ],
        [ "Overworld Rectangle Painting Flow", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md338", null ]
      ] ],
      [ "Best Practices", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md339", [
        [ "DO ✅", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md340", null ],
        [ "DON'T ❌", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md341", null ]
      ] ],
      [ "Troubleshooting", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md342", [
        [ "Issue: Rectangle wraps at boundaries", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md343", null ],
        [ "Issue: Painting in wrong location", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md344", null ],
        [ "Issue: Array index out of bounds", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md345", null ],
        [ "Issue: Forgot to call End()", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md346", null ]
      ] ],
      [ "Future: Scratch Space", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md347", null ],
      [ "Summary", "d2/db2/md_docs_2CANVAS__GUIDE.html#autotoc_md348", null ]
    ] ],
    [ "Roadmap", "d6/db4/md_docs_2D1-roadmap.html", [
      [ "Current Focus: Stability & Core Features", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md350", null ],
      [ "0.4.X (Next Major Release) - Stability & Core Tooling", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md352", [
        [ "Priority 1: Testing & Stability (BLOCKER)", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md353", null ],
        [ "Priority 2: <tt>z3ed</tt> AI Agent", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md354", null ],
        [ "Priority 3: Editor Features", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md355", null ]
      ] ],
      [ "0.5.X - Feature Expansion", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md357", null ],
      [ "0.6.X - Polish & Integration", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md359", null ],
      [ "0.7.X and Beyond - Path to 1.0", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md361", null ]
    ] ],
    [ "Yaze Dungeon Editor: Master Guide", "d5/dde/md_docs_2dungeon__editor__master__guide.html", [
      [ "1. Current Status: Production Ready", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md363", [
        [ "Known Issues & Next Steps", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md364", null ]
      ] ],
      [ "2. Architecture: A Component-Based Design", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md365", [
        [ "Core Components", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md366", null ],
        [ "Data Flow", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md367", null ]
      ] ],
      [ "3. Key Recent Fixes", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md368", [
        [ "Critical Fix 1: The Coordinate System", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md369", null ],
        [ "Critical Fix 2: The Test Suite", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md370", null ]
      ] ],
      [ "4. ROM Internals & Data Structures", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md371", [
        [ "Object Encoding", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md372", null ],
        [ "Core Data Tables in ROM", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md373", null ]
      ] ],
      [ "5. Testing: 100% Pass Rate", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md374", [
        [ "How to Run Tests", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md375", null ]
      ] ],
      [ "6. Dungeon Object Reference Tables", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md376", [
        [ "Type 1 Object Reference Table", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md377", null ],
        [ "Type 2 Object Reference Table", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md378", null ],
        [ "Type 3 Object Reference Table", "d5/dde/md_docs_2dungeon__editor__master__guide.html#autotoc_md379", null ]
      ] ]
    ] ],
    [ "Asm Style Guide", "d7/d9a/md_docs_2E1-asm-style-guide.html", [
      [ "Table of Contents", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md381", null ],
      [ "File Structure", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md382", null ],
      [ "Labels and Symbols", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md383", null ],
      [ "Comments", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md384", null ],
      [ "Instructions", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md385", null ],
      [ "Macros", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md386", null ],
      [ "Loops and Branching", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md387", null ],
      [ "Data Structures", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md388", null ],
      [ "Code Organization", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md389", null ],
      [ "Custom Code", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md390", null ]
    ] ],
    [ "Tile16 Editor Palette System", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html", [
      [ "Executive Summary", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md392", null ],
      [ "Problem Analysis", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md393", [
        [ "Critical Issues Identified", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md394", null ]
      ] ],
      [ "Solution Architecture", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md395", [
        [ "Core Design Principles", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md396", null ],
        [ "256-Color Overworld Palette Structure", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md397", null ],
        [ "Sheet-to-Palette Mapping", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md398", null ],
        [ "Palette Button Mapping", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md399", null ]
      ] ],
      [ "Implementation Details", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md400", [
        [ "1. Fixed SetPaletteWithTransparent Method", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md401", null ],
        [ "2. Corrected Tile16 Editor Palette System", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md402", null ],
        [ "3. Palette Coordination Flow", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md403", null ]
      ] ],
      [ "UI/UX Refactoring", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md404", [
        [ "New Three-Column Layout", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md405", null ],
        [ "Canvas Context Menu Fixes", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md406", null ],
        [ "Dynamic Zoom Controls", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md407", null ]
      ] ],
      [ "Testing Protocol", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md408", [
        [ "Crash Prevention Testing", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md409", null ],
        [ "Color Alignment Testing", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md410", null ],
        [ "UI/UX Testing", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md411", null ]
      ] ],
      [ "Error Handling", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md412", [
        [ "Bounds Checking", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md413", null ],
        [ "Fallback Mechanisms", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md414", null ]
      ] ],
      [ "Debug Information Display", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md415", null ],
      [ "Known Issues and Ongoing Work", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md416", [
        [ "Completed Items ✅", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md417", null ],
        [ "Active Issues ⚠️", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md418", null ],
        [ "Current Status Summary", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md419", null ],
        [ "Future Enhancements", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md420", null ]
      ] ],
      [ "Maintenance Notes", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md421", null ],
      [ "Next Steps", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md423", [
        [ "Immediate Priorities", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md424", null ],
        [ "Investigation Areas", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md425", null ]
      ] ]
    ] ],
    [ "Overworld Loading Guide", "dc/d12/md_docs_2F1-overworld-loading.html", [
      [ "Table of Contents", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md428", null ],
      [ "Overview", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md429", null ],
      [ "ROM Types and Versions", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md430", [
        [ "Version Detection", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md431", null ],
        [ "Feature Support by Version", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md432", null ]
      ] ],
      [ "Overworld Map Structure", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md433", [
        [ "Core Properties", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md434", null ]
      ] ],
      [ "Overlays and Special Area Maps", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md435", [
        [ "Understanding Overlays", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md436", null ],
        [ "Special Area Maps (0x80-0x9F)", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md437", null ],
        [ "Overlay ID Mappings", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md438", null ],
        [ "Drawing Order", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md439", null ],
        [ "Vanilla Overlay Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md440", null ],
        [ "Special Area Graphics Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md441", null ]
      ] ],
      [ "Loading Process", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md442", [
        [ "1. Version Detection", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md443", null ],
        [ "2. Map Initialization", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md444", null ],
        [ "3. Property Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md445", [
          [ "Vanilla ROMs (asm_version == 0xFF)", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md446", null ],
          [ "ZSCustomOverworld v2/v3", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md447", null ]
        ] ],
        [ "4. Custom Data Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md448", null ]
      ] ],
      [ "ZScream Implementation", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md449", [
        [ "OverworldMap Constructor", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md450", null ],
        [ "Key Methods", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md451", null ]
      ] ],
      [ "Yaze Implementation", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md452", [
        [ "OverworldMap Constructor", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md453", null ],
        [ "Key Methods", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md454", null ],
        [ "Current Status", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md455", null ]
      ] ],
      [ "Key Differences", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md456", [
        [ "1. Language and Architecture", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md457", null ],
        [ "2. Data Structures", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md458", null ],
        [ "3. Error Handling", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md459", null ],
        [ "4. Graphics Processing", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md460", null ]
      ] ],
      [ "Common Issues and Solutions", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md461", [
        [ "1. Version Detection Issues", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md462", null ],
        [ "2. Palette Loading Errors", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md463", null ],
        [ "3. Graphics Not Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md464", null ],
        [ "4. Overlay Issues", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md465", null ],
        [ "5. Large Map Problems", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md466", null ],
        [ "6. Special Area Graphics Issues", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md467", null ]
      ] ],
      [ "Best Practices", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md468", [
        [ "1. Version-Specific Code", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md469", null ],
        [ "2. Error Handling", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md470", null ],
        [ "3. Memory Management", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md471", null ],
        [ "4. Thread Safety", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md472", null ]
      ] ],
      [ "Conclusion", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md473", null ]
    ] ],
    [ "Graphics System Performance & Optimization", "db/dc2/md_docs_2GRAPHICS__PERFORMANCE.html", [
      [ "1. Executive Summary", "db/dc2/md_docs_2GRAPHICS__PERFORMANCE.html#autotoc_md475", [
        [ "Overall Performance Results", "db/dc2/md_docs_2GRAPHICS__PERFORMANCE.html#autotoc_md476", null ]
      ] ],
      [ "2. Implemented Optimizations", "db/dc2/md_docs_2GRAPHICS__PERFORMANCE.html#autotoc_md477", null ],
      [ "3. Future Optimization Recommendations", "db/dc2/md_docs_2GRAPHICS__PERFORMANCE.html#autotoc_md478", [
        [ "High Priority", "db/dc2/md_docs_2GRAPHICS__PERFORMANCE.html#autotoc_md479", null ],
        [ "Medium Priority", "db/dc2/md_docs_2GRAPHICS__PERFORMANCE.html#autotoc_md480", null ]
      ] ]
    ] ],
    [ "yaze Documentation", "d3/d4c/md_docs_2index.html", [
      [ "Quick Start", "d3/d4c/md_docs_2index.html#autotoc_md482", null ],
      [ "Development", "d3/d4c/md_docs_2index.html#autotoc_md483", null ],
      [ "Technical Documentation", "d3/d4c/md_docs_2index.html#autotoc_md484", [
        [ "Assembly & Code", "d3/d4c/md_docs_2index.html#autotoc_md485", null ],
        [ "Editor Systems", "d3/d4c/md_docs_2index.html#autotoc_md486", null ],
        [ "Overworld System", "d3/d4c/md_docs_2index.html#autotoc_md487", null ]
      ] ],
      [ "Key Features", "d3/d4c/md_docs_2index.html#autotoc_md488", null ]
    ] ],
    [ "Ollama Integration Status - Updated# Ollama Integration Status", "da/d40/md_docs_2ollama__integration__status.html", [
      [ "✅ Completed## ✅ Completed", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md491", [
        [ "Infrastructure### Flag Parsing", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md492", null ],
        [ "Current Issue: Empty Tool Results### Tool System", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md493", null ]
      ] ],
      [ "🎨 New Features Added  - OR doesn't provide a <tt>text_response</tt> field in the JSON", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md494", [
        [ "Verbose Mode**Solution Needed**: Update system prompt to include explicit instructions like:", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md495", null ],
        [ "Priority 2: Refine Prompts (MEDIUM)- ✅ Model loads (qwen2.5-coder:7b)", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md496", null ],
        [ "Priority 3: Documentation (LOW)```", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md497", null ]
      ] ],
      [ "🧪 Testing Commands### Priority 1: Fix Tool Calling Loop (High Priority)", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md498", null ],
      [ "📊 Performance   - codellama:7b", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md499", null ],
      [ "🎯 Success Criteria", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md500", [
        [ "Priority 3: Documentation (Low Priority)", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md501", null ]
      ] ],
      [ "🐛 Known Issues", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md502", null ],
      [ "💡 Recommendations", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md503", [
        [ "For Users", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md504", null ],
        [ "For Developers", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md505", null ]
      ] ],
      [ "📝 Related Files", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md506", null ],
      [ "🎯 Success Criteria", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md507", [
        [ "Minimum Viable", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md508", null ],
        [ "Full Success", "da/d40/md_docs_2ollama__integration__status.html#autotoc_md509", null ]
      ] ]
    ] ],
    [ "yaze Overworld Testing Guide", "de/d8a/md_docs_2overworld__testing__guide.html", [
      [ "Overview", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md512", null ],
      [ "Test Architecture", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md513", [
        [ "1. Golden Data System", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md514", null ],
        [ "2. Test Categories", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md515", [
          [ "Unit Tests (<tt>test/unit/zelda3/</tt>)", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md516", null ],
          [ "Integration Tests (<tt>test/e2e/</tt>)", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md517", null ],
          [ "Golden Data Tools", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md518", null ]
        ] ]
      ] ],
      [ "Quick Start", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md519", [
        [ "Prerequisites", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md520", null ],
        [ "Running Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md521", [
          [ "Basic Test Run", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md522", null ],
          [ "Selective Test Execution", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md523", null ]
        ] ]
      ] ],
      [ "Test Components", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md524", [
        [ "1. Golden Data Extractor", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md525", null ],
        [ "2. Integration Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md526", null ],
        [ "3. End-to-End Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md527", null ]
      ] ],
      [ "Test Validation Points", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md528", [
        [ "1. ZScream Compatibility", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md529", null ],
        [ "2. ROM State Validation", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md530", null ],
        [ "3. Performance and Stability", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md531", null ]
      ] ],
      [ "Environment Variables", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md532", [
        [ "Test Configuration", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md533", null ],
        [ "Build Configuration", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md534", null ]
      ] ],
      [ "Test Reports", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md535", [
        [ "Generated Reports", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md536", null ],
        [ "Report Location", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md537", null ]
      ] ],
      [ "Troubleshooting", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md538", [
        [ "Common Issues", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md539", [
          [ "1. ROM Not Found", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md540", null ],
          [ "2. Build Failures", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md541", null ],
          [ "3. Test Failures", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md542", null ]
        ] ],
        [ "Debug Mode", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md543", null ]
      ] ],
      [ "Advanced Usage", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md544", [
        [ "Custom Test Scenarios", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md545", [
          [ "1. Testing Custom ROMs", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md546", null ],
          [ "2. Regression Testing", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md547", null ],
          [ "3. Performance Testing", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md548", null ]
        ] ]
      ] ],
      [ "Integration with CI/CD", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md549", [
        [ "GitHub Actions Example", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md550", null ]
      ] ],
      [ "Contributing", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md551", [
        [ "Adding New Tests", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md552", null ],
        [ "Test Guidelines", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md553", null ],
        [ "Example Test Structure", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md554", null ]
      ] ],
      [ "Conclusion", "de/d8a/md_docs_2overworld__testing__guide.html#autotoc_md555", null ]
    ] ],
    [ "z3ed: AI-Powered CLI for YAZE", "d3/d63/md_docs_2z3ed_2README.html", [
      [ "1. Overview", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md557", [
        [ "Core Capabilities", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md558", null ]
      ] ],
      [ "2. Quick Start", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md559", [
        [ "Build", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md560", null ],
        [ "AI Setup", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md561", null ],
        [ "Example Commands", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md562", null ]
      ] ],
      [ "3. Architecture", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md563", [
        [ "System Components Diagram", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md564", null ]
      ] ],
      [ "4. Agentic & Generative Workflow (MCP)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md565", null ],
      [ "5. Command Reference", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md566", [
        [ "Agent Commands", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md567", null ],
        [ "Resource Commands", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md568", null ]
      ] ],
      [ "6. Chat Modes", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md569", [
        [ "FTXUI Chat (<tt>agent chat</tt>)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md570", null ],
        [ "Simple Chat (<tt>agent simple-chat</tt>)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md571", null ],
        [ "GUI Chat Widget (Editor Integration)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md572", null ]
      ] ],
      [ "7. AI Provider Configuration", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md573", null ],
      [ "8. CLI Output & Help System", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md574", [
        [ "Verbose Logging", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md575", null ],
        [ "Hierarchical Help System", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md576", null ]
      ] ],
      [ "9. Collaborative Sessions & Multimodal Vision", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md577", [
        [ "Collaborative Sessions", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md578", [
          [ "Local Collaboration Mode", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md579", null ],
          [ "Network Collaboration Mode (NEW!)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md580", null ]
        ] ],
        [ "Multimodal Vision (Gemini)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md581", null ]
      ] ],
      [ "10. Roadmap & Implementation Status", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md582", [
        [ "✅ Completed", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md583", null ],
        [ "🚧 Active & Next Steps", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md584", null ]
      ] ],
      [ "11. Troubleshooting", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md585", null ]
    ] ],
    [ "yaze - Yet Another Zelda3 Editor", "d0/d30/md_README.html", [
      [ "Version 0.3.2 - Release", "d0/d30/md_README.html#autotoc_md587", null ],
      [ "Quick Start", "d0/d30/md_README.html#autotoc_md592", [
        [ "Build", "d0/d30/md_README.html#autotoc_md593", null ],
        [ "Applications", "d0/d30/md_README.html#autotoc_md594", null ]
      ] ],
      [ "Usage", "d0/d30/md_README.html#autotoc_md595", [
        [ "GUI Editor", "d0/d30/md_README.html#autotoc_md596", null ],
        [ "Command Line Tool", "d0/d30/md_README.html#autotoc_md597", null ],
        [ "C++ API", "d0/d30/md_README.html#autotoc_md598", null ]
      ] ],
      [ "Documentation", "d0/d30/md_README.html#autotoc_md599", null ],
      [ "Supported Platforms", "d0/d30/md_README.html#autotoc_md600", null ],
      [ "ROM Compatibility", "d0/d30/md_README.html#autotoc_md601", null ],
      [ "Contributing", "d0/d30/md_README.html#autotoc_md602", null ],
      [ "License", "d0/d30/md_README.html#autotoc_md603", null ],
      [ "🙏 Acknowledgments", "d0/d30/md_README.html#autotoc_md604", null ],
      [ "📸 Screenshots", "d0/d30/md_README.html#autotoc_md605", null ]
    ] ],
    [ "yaze Build Scripts", "de/d82/md_scripts_2README.html", [
      [ "Windows Scripts", "de/d82/md_scripts_2README.html#autotoc_md608", [
        [ "vcpkg Setup (Optional)", "de/d82/md_scripts_2README.html#autotoc_md609", null ]
      ] ],
      [ "Windows Build Workflow", "de/d82/md_scripts_2README.html#autotoc_md610", [
        [ "Recommended: Visual Studio CMake Mode", "de/d82/md_scripts_2README.html#autotoc_md611", null ],
        [ "Command Line Build", "de/d82/md_scripts_2README.html#autotoc_md612", null ],
        [ "Compiler Notes", "de/d82/md_scripts_2README.html#autotoc_md613", null ]
      ] ],
      [ "Quick Start (Windows)", "de/d82/md_scripts_2README.html#autotoc_md614", [
        [ "Option 1: Visual Studio (Recommended)", "de/d82/md_scripts_2README.html#autotoc_md615", null ],
        [ "Option 2: Command Line", "de/d82/md_scripts_2README.html#autotoc_md616", null ],
        [ "Option 3: With vcpkg (Optional)", "de/d82/md_scripts_2README.html#autotoc_md617", null ]
      ] ],
      [ "Troubleshooting", "de/d82/md_scripts_2README.html#autotoc_md618", [
        [ "Common Issues", "de/d82/md_scripts_2README.html#autotoc_md619", null ],
        [ "Getting Help", "de/d82/md_scripts_2README.html#autotoc_md620", null ]
      ] ],
      [ "Other Scripts", "de/d82/md_scripts_2README.html#autotoc_md621", null ],
      [ "Build Environment Verification", "de/d82/md_scripts_2README.html#autotoc_md622", [
        [ "<tt>verify-build-environment.ps1</tt> / <tt>.sh</tt>", "de/d82/md_scripts_2README.html#autotoc_md623", null ],
        [ "Usage", "de/d82/md_scripts_2README.html#autotoc_md624", null ]
      ] ]
    ] ],
    [ "End-to-End (E2E) Tests", "d9/db0/md_test_2e2e_2README.html", [
      [ "Active Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md626", [
        [ "✅ Working Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md627", null ],
        [ "📝 Dungeon Editor Smoke Test", "d9/db0/md_test_2e2e_2README.html#autotoc_md628", null ]
      ] ],
      [ "Running Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md629", [
        [ "All E2E Tests (GUI Mode)", "d9/db0/md_test_2e2e_2README.html#autotoc_md630", null ],
        [ "Specific Test Category", "d9/db0/md_test_2e2e_2README.html#autotoc_md631", null ],
        [ "Dungeon Editor Test Only", "d9/db0/md_test_2e2e_2README.html#autotoc_md632", null ]
      ] ],
      [ "Test Development", "d9/db0/md_test_2e2e_2README.html#autotoc_md633", [
        [ "Creating New Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md634", null ],
        [ "Register in yaze_test.cc", "d9/db0/md_test_2e2e_2README.html#autotoc_md635", null ],
        [ "ImGui Test Engine API", "d9/db0/md_test_2e2e_2README.html#autotoc_md636", null ]
      ] ],
      [ "Test Logging", "d9/db0/md_test_2e2e_2README.html#autotoc_md637", null ],
      [ "Test Infrastructure", "d9/db0/md_test_2e2e_2README.html#autotoc_md638", [
        [ "File Organization", "d9/db0/md_test_2e2e_2README.html#autotoc_md639", null ],
        [ "Helper Functions", "d9/db0/md_test_2e2e_2README.html#autotoc_md640", null ]
      ] ],
      [ "Future Test Ideas", "d9/db0/md_test_2e2e_2README.html#autotoc_md641", null ],
      [ "Troubleshooting", "d9/db0/md_test_2e2e_2README.html#autotoc_md642", [
        [ "Test Crashes in GUI Mode", "d9/db0/md_test_2e2e_2README.html#autotoc_md643", null ],
        [ "Tests Not Found", "d9/db0/md_test_2e2e_2README.html#autotoc_md644", null ],
        [ "ImGui Items Not Found", "d9/db0/md_test_2e2e_2README.html#autotoc_md645", null ]
      ] ],
      [ "References", "d9/db0/md_test_2e2e_2README.html#autotoc_md646", null ],
      [ "Status", "d9/db0/md_test_2e2e_2README.html#autotoc_md647", null ]
    ] ],
    [ "yaze Test Suite", "d0/d46/md_test_2README.html", [
      [ "Directory Structure", "d0/d46/md_test_2README.html#autotoc_md649", null ],
      [ "Test Categories", "d0/d46/md_test_2README.html#autotoc_md650", [
        [ "Unit Tests (<tt>unit/</tt>)", "d0/d46/md_test_2README.html#autotoc_md651", null ],
        [ "Integration Tests (<tt>integration/</tt>)", "d0/d46/md_test_2README.html#autotoc_md652", null ],
        [ "End-to-End Tests (<tt>e2e/</tt>)", "d0/d46/md_test_2README.html#autotoc_md653", null ]
      ] ],
      [ "Enhanced Test Runner", "d0/d46/md_test_2README.html#autotoc_md654", [
        [ "Usage Examples", "d0/d46/md_test_2README.html#autotoc_md655", null ],
        [ "Test Modes", "d0/d46/md_test_2README.html#autotoc_md656", null ],
        [ "Options", "d0/d46/md_test_2README.html#autotoc_md657", null ]
      ] ],
      [ "E2E ROM Testing", "d0/d46/md_test_2README.html#autotoc_md658", [
        [ "Test Cases", "d0/d46/md_test_2README.html#autotoc_md659", null ]
      ] ],
      [ "ZSCustomOverworld Upgrade Testing", "d0/d46/md_test_2README.html#autotoc_md660", [
        [ "Supported Upgrades", "d0/d46/md_test_2README.html#autotoc_md661", null ],
        [ "Test Cases", "d0/d46/md_test_2README.html#autotoc_md662", null ],
        [ "Version-Specific Features", "d0/d46/md_test_2README.html#autotoc_md663", [
          [ "Vanilla", "d0/d46/md_test_2README.html#autotoc_md664", null ],
          [ "v2", "d0/d46/md_test_2README.html#autotoc_md665", null ],
          [ "v3", "d0/d46/md_test_2README.html#autotoc_md666", null ]
        ] ]
      ] ],
      [ "Environment Variables", "d0/d46/md_test_2README.html#autotoc_md667", null ],
      [ "CI/CD Integration", "d0/d46/md_test_2README.html#autotoc_md668", null ],
      [ "Deprecated Tests", "d0/d46/md_test_2README.html#autotoc_md669", null ],
      [ "Best Practices", "d0/d46/md_test_2README.html#autotoc_md670", null ],
      [ "AI Agent Testing", "d0/d46/md_test_2README.html#autotoc_md671", null ]
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
"d0/d30/md_README.html#autotoc_md593",
"d0/d77/file__dialog_8h.html#a91b15ab541fc413b5208c32d750b87c8",
"d0/dc7/classyaze_1_1test_1_1DungeonObjectRenderingE2ETests.html#aa219f1ce1b52d339a4a9f45f429643c5",
"d1/d0e/message__preview_8cc_source.html",
"d1/d33/structyaze_1_1emu_1_1OAMADDH.html",
"d1/d42/classyaze_1_1util_1_1LogManager.html#a9063796137583d959ad437b090570a21",
"d1/d67/structyaze_1_1cli_1_1overworld_1_1WarpEntry.html#a316b2c066964a6d64956a5e7b84f83dc",
"d1/daf/compression_8h.html#a0882444317fe9f49790acb8ad38c7a77",
"d1/deb/structyaze_1_1test_1_1TestScriptStep.html#a17cfbae5208e6f65faab0569b96c46e0",
"d2/d07/classyaze_1_1emu_1_1Spc700.html#a4f2f48be487d3f0f8cbda780b18487ee",
"d2/d20/classyaze_1_1cli_1_1PromptBuilder.html#a43367abf78f46983712f4ea8db62ef41",
"d2/d5b/structyaze_1_1gui_1_1canvas_1_1CanvasContextMenuItem.html#ad99e1c16260e6efb4d259aecd8b30a5d",
"d2/da4/flag_8h.html#ad1de0365ec903537745131d8fc935e59",
"d2/de7/classyaze_1_1editor_1_1DungeonCanvasViewer.html#a7542b525ceda3c877c9f190b1a829ca1",
"d2/dfe/structyaze_1_1gui_1_1EnhancedTheme.html#a37680f5c647e9f58664d13121128687c",
"d3/d15/classyaze_1_1emu_1_1Snes.html#a36610fdc93a127c79dc5ffc5e772dee4",
"d3/d21/classyaze_1_1editor_1_1EditorSet.html#ae585f2bf8457f4051612c66805263a71",
"d3/d44/classyaze_1_1editor_1_1OverworldEditor.html#a2a624d3d6d72347a47b888d1eeeaeb90",
"d3/d44/classyaze_1_1editor_1_1OverworldEditor.html#af15b9838fc18496532d3664c90db811f",
"d3/d6a/classyaze_1_1zelda3_1_1RoomObject.html#a5d8fb4913316418def7352f19cfe9014",
"d3/d89/structyaze_1_1editor_1_1MessageData.html#a2ebee010cbc75c331b31e761d90f6628",
"d3/d9d/classyaze_1_1zelda3_1_1GameEntity.html#a04318cae2fac826a7a06bdbb6e7948b2a31fd9968723a0676325446e2e85c02d8",
"d3/dbf/namespaceyaze_1_1gui.html#a24eebca5b3fe9d6e674cefcb0a64e7e4abe579a9078c06d1cd7c291ae5c92485f",
"d3/ded/classyaze_1_1emu_1_1Ppu.html#a319e339c61328ea20ab8c5afef36ce25",
"d4/d02/classyaze_1_1cli_1_1ResourceCatalog.html#aebd5cb61c532ecea7b7a456f5a5dffde",
"d4/d0a/namespaceyaze_1_1test.html#ab99769ca77f8e9c7b9676ab5f276befc",
"d4/d53/classyaze_1_1cli_1_1Tile16ProposalGenerator.html#a0b9a0a2e16d712fd72e6cdec5236d6da",
"d4/d84/classyaze_1_1core_1_1Controller.html#a4a8fb48e64d18f18c821c13f0a23666e",
"d4/dcf/canvas_8h.html#a16adfc7583097bf343bd2cc7065e445d",
"d4/de9/overworld__exit_8h.html#ac0ef12936001b8daaf166ca667a105af",
"d5/d1f/namespaceyaze_1_1zelda3.html#a38f5383a2e1609de1993b2d4fef76f87",
"d5/d1f/namespaceyaze_1_1zelda3.html#a93615225ba653354300a1cebe01f368d",
"d5/d31/classyaze_1_1gfx_1_1SnesColor.html#aa3c67dff6ac0a9a3fd79d85636b3d694",
"d5/d67/classyaze_1_1cli_1_1PolicyEvaluator.html#ae2b4ae50b514f9576fa888b072058141",
"d5/da6/namespaceyaze_1_1zelda3_1_1anonymous__namespace_02room__object_8cc_03.html",
"d5/dd0/classyaze_1_1test_1_1RomDependentTestSuite.html#a3325314ff8dfd69549541517f51bd1e8",
"d6/d10/md_docs_2A1-testing-guide.html",
"d6/d2e/classyaze_1_1zelda3_1_1TitleScreen.html#ad3877dfc872593f30ea8f25ed52e1e8b",
"d6/d3c/classyaze_1_1editor_1_1MapPropertiesSystem.html#af2a098e5cec6a74c4b324cc4ec3f1a78",
"d6/d87/structyaze_1_1gui_1_1DungeonAsset.html#a206502db76dc8bd6490c6c3384d8e4bf",
"d6/db4/md_docs_2D1-roadmap.html#autotoc_md357",
"d6/dec/overworld__editor__manager_8h.html",
"d7/d4a/structyaze_1_1editor_1_1Tile16Editor_1_1LayoutScratch.html#a8e5dec2ac04dc0c3db3b3260a2377c67",
"d7/d61/classyaze_1_1zelda3_1_1DungeonEditorSystem.html#a6887860f0c2f6dfbc217fd106d06b273",
"d7/d6a/z3ed_8cc.html#ac346cd7096e6638fef8f499c51ab776c",
"d7/d9c/overworld_8h.html#a3ec4ee08b8db64ab801fcf4218c0a068",
"d7/dca/structyaze_1_1editor_1_1DictionaryEntry.html#a53f59e0595aa655fdc29cd54269d8871",
"d7/df6/classyaze_1_1zelda3_1_1Room.html#a86d4c15f95a24e2b3e1a9a55b7e2978d",
"d7/dfb/classyaze_1_1editor_1_1OverworldEditorManager.html#a3ecd6d63e698de4de6690ed655952ecf",
"d8/d1e/classyaze_1_1zelda3_1_1RoomEntrance.html",
"d8/d5f/classyaze_1_1zelda3_1_1ObjectRenderer_1_1MemoryPool.html#a355231802aae7b52c726b0df4047423a",
"d8/d98/classyaze_1_1editor_1_1DungeonEditor.html#aa47cdda2cd034aa00f777f7f4f115499",
"d8/dc3/classyaze_1_1zelda3_1_1DungeonObjectRendererIntegrationTest.html#a47e765b236a9c3923e88bab40502ea8f",
"d8/ddb/snes__palette_8h.html#a82a8956476ffc04750bcfc4120c8b8dbad8fee2a50a54fcd05736b1cbded9b91a",
"d9/d2b/classyaze_1_1editor_1_1AgentChatWidget.html#acbb92cc63fc2bd3a9f9cf075472f3fef",
"d9/d70/classyaze_1_1zelda3_1_1RoomLayout.html#a91f9a6759ac57cd5df520733bb25524e",
"d9/dbd/canvas__utils__moved_8h.html#acf603fb9537f45b2ff6c2e7707212b15",
"d9/dc5/classyaze_1_1zelda3_1_1Overworld.html#a63cba68ac3f7f21e4359babfb4413e1a",
"d9/dcc/classyaze_1_1zelda3_1_1OverworldMap.html#a879fb9353f241b17c00916c238a00860",
"d9/dfe/classyaze_1_1gui_1_1WidgetIdRegistry.html",
"da/d2c/classyaze_1_1gui_1_1Canvas.html#a3eee9d072fffeaa1722d4f10c470862b",
"da/d2c/classyaze_1_1gui_1_1Canvas.html#af7b95c6c5a82d79926e00105dcc1cb14",
"da/d40/md_docs_2ollama__integration__status.html#autotoc_md506",
"da/d68/classyaze_1_1gfx_1_1SnesPalette.html",
"da/da8/test__dungeon__objects_8cc.html#ac924bf56d3b875c69d0e38cad8150a3e",
"da/de2/structyaze_1_1gui_1_1WidgetIdRegistry_1_1WidgetMetadata.html#a6560c98a0d114f641bcdc313262ffa43",
"db/d22/structyaze_1_1RomLoadOptions.html#affcb7268262bd3fdfcc9fa29d42fff55",
"db/d5f/structyaze_1_1cli_1_1AgentResponse.html#a6f42acd06be026e6b22783dbb591eccc",
"db/d82/classyaze_1_1editor_1_1Tile16Editor.html#abebf8fffcdcc5ed3d5404adca0c48eb4",
"db/da4/classyaze_1_1emu_1_1MockMemory.html#a963f7e8d4883a616c282e3403d8cd425",
"db/dcc/classyaze_1_1editor_1_1ScreenEditor.html#af6ff0f96f40d97009ee73fca23443569",
"dc/d01/classyaze_1_1emu_1_1Dsp.html#add68a31a651d61101d8751062b7c00b9",
"dc/d31/classyaze_1_1editor_1_1GraphicsEditor.html#a3f507d0ddb30c4ed200a0f87cf66fc82",
"dc/d4c/structyaze_1_1emu_1_1WindowMaskSettings.html#a45e607dec963a46f58ac0855a1dfb23f",
"dc/d69/structyaze_1_1zelda3_1_1ObjectRenderer_1_1GraphicsCache_1_1GraphicsSheetInfo.html#ade123bff981ce081b6e831b819bcd6d8",
"dc/de0/classyaze_1_1test_1_1ArenaTestSuite.html#a90f793b6f48683794289c3d8a98788cf",
"dc/df4/classyaze_1_1emu_1_1Cpu.html#a764eeea7b58f891e124ee0a3394e1d89",
"dc/df4/classyaze_1_1emu_1_1Cpu.html#af3a91027d2e71e313febcf1a7fd7856a",
"dd/d12/classyaze_1_1editor_1_1EditorManager.html#ab1925444bb0903c0375991ee96692c21",
"dd/d40/classyaze_1_1test_1_1DungeonObjectRenderingTests.html#a0b189b56c6bed14eb7bbfd67cd88ab42",
"dd/d63/namespaceyaze_1_1cli.html#ab3acc1b59e84abb18e2d438f7ed2ad33",
"dd/dac/classyaze_1_1gfx_1_1BackgroundBuffer.html",
"dd/ddf/classyaze_1_1gui_1_1canvas_1_1CanvasContextMenu.html#a728e925cc68d6c08332b2f55f4146715a04296f2325bb84e9144aa0301685e702",
"de/d07/structyaze_1_1cli_1_1PolicyEvaluator_1_1PolicyConfig_1_1ReviewRequirement.html#aec0fc3c99b6dac729254c0583a7c1c43",
"de/d3c/namespaceyaze_1_1cli_1_1anonymous__namespace_02test__suite__loader_8cc_03.html#a5a9a4534f75106e0580858f315504653",
"de/d7d/toast__manager_8h.html",
"de/da7/structyaze_1_1gui_1_1canvas_1_1CanvasPaletteManager.html#a9b73908eb2a2ed26a42e8e12cf4931ab",
"de/dbf/icons_8h.html#a16b7df4037c70da1a5ef9d5483b5b631",
"de/dbf/icons_8h.html#a35e820c92a1d1e5905892d8bf3072f14",
"de/dbf/icons_8h.html#a4fcd80bd25f0f55faa86085c81f6e456",
"de/dbf/icons_8h.html#a6d1f548594d76e3ad195b18f632effa9",
"de/dbf/icons_8h.html#a8d3dc06f14c80a196b9871d733ef730a",
"de/dbf/icons_8h.html#aab7c2e2f6cc992ca4b87eb5dfda8267f",
"de/dbf/icons_8h.html#ac8e69c940e3b42d466672c3b1825a593",
"de/dbf/icons_8h.html#ae41d7640725a3ebe8ae73dc911a18b5c",
"de/dbf/icons_8h.html#aff001c2bfc23f96dc67a5ec1e8929768",
"de/de7/classyaze_1_1zelda3_1_1DungeonObjectEditor.html#a7cb921c32e80acc9e7686e4e4acb1161",
"df/d0e/classyaze_1_1gfx_1_1Bitmap.html#a7fd93b6f41ab894ada99b7a89d254c3c",
"df/d41/structyaze_1_1editor_1_1zsprite_1_1ZSprite.html#a0fe51a591960bea89589f318b8a4ebe2",
"df/db7/classyaze_1_1gui_1_1canvas_1_1CanvasPerformanceManager.html#ae422893eb7667252446e725cfe6cbff3",
"df/dfe/namespaceyaze_1_1zelda3_1_1test.html#a5dc53f13b882c2f2c6287308cf74e87c",
"namespacemembers_func_g.html"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';