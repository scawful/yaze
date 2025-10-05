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
      [ "Feature Status", "d0/d63/md_docs_201-getting-started.html#autotoc_md3", null ],
      [ "Command-Line Interface (<tt>z3ed</tt>)", "d0/d63/md_docs_201-getting-started.html#autotoc_md4", [
        [ "AI Agent Chat", "d0/d63/md_docs_201-getting-started.html#autotoc_md5", null ],
        [ "Resource Inspection", "d0/d63/md_docs_201-getting-started.html#autotoc_md6", null ],
        [ "Patching", "d0/d63/md_docs_201-getting-started.html#autotoc_md7", null ]
      ] ],
      [ "Extending Functionality", "d0/d63/md_docs_201-getting-started.html#autotoc_md8", null ]
    ] ],
    [ "Build Instructions", "d9/d41/md_docs_202-build-instructions.html", [
      [ "1. Environment Verification", "d9/d41/md_docs_202-build-instructions.html#autotoc_md10", [
        [ "Windows (PowerShell)", "d9/d41/md_docs_202-build-instructions.html#autotoc_md11", null ],
        [ "macOS & Linux (Bash)", "d9/d41/md_docs_202-build-instructions.html#autotoc_md12", null ]
      ] ],
      [ "2. Quick Start: Building with Presets", "d9/d41/md_docs_202-build-instructions.html#autotoc_md13", [
        [ "macOS", "d9/d41/md_docs_202-build-instructions.html#autotoc_md14", null ],
        [ "Linux", "d9/d41/md_docs_202-build-instructions.html#autotoc_md15", null ],
        [ "Windows", "d9/d41/md_docs_202-build-instructions.html#autotoc_md16", null ],
        [ "AI-Enabled Build (All Platforms)", "d9/d41/md_docs_202-build-instructions.html#autotoc_md17", null ]
      ] ],
      [ "3. Dependencies", "d9/d41/md_docs_202-build-instructions.html#autotoc_md18", null ],
      [ "4. Platform Setup", "d9/d41/md_docs_202-build-instructions.html#autotoc_md19", [
        [ "macOS", "d9/d41/md_docs_202-build-instructions.html#autotoc_md20", null ],
        [ "Linux (Ubuntu/Debian)", "d9/d41/md_docs_202-build-instructions.html#autotoc_md21", null ],
        [ "Windows", "d9/d41/md_docs_202-build-instructions.html#autotoc_md22", null ]
      ] ],
      [ "5. Testing", "d9/d41/md_docs_202-build-instructions.html#autotoc_md23", [
        [ "Running Tests with Presets", "d9/d41/md_docs_202-build-instructions.html#autotoc_md24", null ],
        [ "Running Tests Manually", "d9/d41/md_docs_202-build-instructions.html#autotoc_md25", null ]
      ] ],
      [ "6. IDE Integration", "d9/d41/md_docs_202-build-instructions.html#autotoc_md26", [
        [ "VS Code (Recommended)", "d9/d41/md_docs_202-build-instructions.html#autotoc_md27", null ],
        [ "Visual Studio (Windows)", "d9/d41/md_docs_202-build-instructions.html#autotoc_md28", null ],
        [ "Xcode (macOS)", "d9/d41/md_docs_202-build-instructions.html#autotoc_md29", null ]
      ] ],
      [ "7. Windows Build Optimization", "d9/d41/md_docs_202-build-instructions.html#autotoc_md30", [
        [ "The Problem: Slow gRPC Builds", "d9/d41/md_docs_202-build-instructions.html#autotoc_md31", null ],
        [ "Solution: Use vcpkg for Pre-compiled Binaries", "d9/d41/md_docs_202-build-instructions.html#autotoc_md32", null ]
      ] ],
      [ "8. Troubleshooting", "d9/d41/md_docs_202-build-instructions.html#autotoc_md33", [
        [ "\"nlohmann/json.hpp: No such file or directory\"", "d9/d41/md_docs_202-build-instructions.html#autotoc_md34", null ],
        [ "\"Cannot open file 'yaze.exe': Permission denied\"", "d9/d41/md_docs_202-build-instructions.html#autotoc_md35", null ],
        [ "\"C++ standard 'cxx_std_23' not supported\"", "d9/d41/md_docs_202-build-instructions.html#autotoc_md36", null ],
        [ "Visual Studio Can't Find Presets", "d9/d41/md_docs_202-build-instructions.html#autotoc_md37", null ],
        [ "General Advice", "d9/d41/md_docs_202-build-instructions.html#autotoc_md38", null ]
      ] ]
    ] ],
    [ "API Reference", "dd/d96/md_docs_204-api-reference.html", [
      [ "C API (<tt>incl/yaze.h</tt>)", "dd/d96/md_docs_204-api-reference.html#autotoc_md40", [
        [ "Core Library Functions", "dd/d96/md_docs_204-api-reference.html#autotoc_md41", null ],
        [ "ROM Operations", "dd/d96/md_docs_204-api-reference.html#autotoc_md42", null ]
      ] ],
      [ "C++ API", "dd/d96/md_docs_204-api-reference.html#autotoc_md43", [
        [ "AsarWrapper (<tt>src/app/core/asar_wrapper.h</tt>)", "dd/d96/md_docs_204-api-reference.html#autotoc_md44", [
          [ "CLI Examples (<tt>z3ed</tt>)", "dd/d96/md_docs_204-api-reference.html#autotoc_md45", null ],
          [ "C++ API Example", "dd/d96/md_docs_204-api-reference.html#autotoc_md46", null ],
          [ "Class Definition", "dd/d96/md_docs_204-api-reference.html#autotoc_md47", null ]
        ] ]
      ] ],
      [ "Data Structures", "dd/d96/md_docs_204-api-reference.html#autotoc_md48", [
        [ "<tt>snes_color</tt>", "dd/d96/md_docs_204-api-reference.html#autotoc_md49", null ],
        [ "<tt>zelda3_message</tt>", "dd/d96/md_docs_204-api-reference.html#autotoc_md50", null ]
      ] ],
      [ "Error Handling", "dd/d96/md_docs_204-api-reference.html#autotoc_md51", [
        [ "C API Error Pattern", "dd/d96/md_docs_204-api-reference.html#autotoc_md52", null ]
      ] ]
    ] ],
    [ "Testing Guide", "d6/d10/md_docs_2A1-testing-guide.html", [
      [ "Test Categories", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md54", [
        [ "Stable Tests (STABLE)", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md55", null ],
        [ "ROM-Dependent Tests (ROM_DEPENDENT)", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md56", null ],
        [ "Experimental Tests (EXPERIMENTAL)", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md57", null ]
      ] ],
      [ "Command Line Usage", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md58", null ],
      [ "CMake Presets", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md59", null ],
      [ "Writing Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md60", [
        [ "Stable Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md61", null ],
        [ "ROM-Dependent Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md62", null ],
        [ "Experimental Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md63", null ]
      ] ],
      [ "CI/CD Integration", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md64", [
        [ "GitHub Actions", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md65", null ],
        [ "Test Execution Strategy", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md66", null ]
      ] ],
      [ "Test Development Guidelines", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md67", [
        [ "Writing Stable Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md68", null ],
        [ "Writing ROM-Dependent Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md69", null ],
        [ "Writing Experimental Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md70", null ]
      ] ],
      [ "E2E GUI Testing Framework", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md71", [
        [ "Architecture", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md72", null ],
        [ "Writing E2E Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md73", null ],
        [ "Running GUI Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md74", null ]
      ] ]
    ] ],
    [ "Platform Compatibility Improvements", "d1/d30/md_docs_2B2-platform-compatibility.html", [
      [ "Native File Dialog Support", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md76", null ],
      [ "Cross-Platform Build Reliability", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md77", null ],
      [ "Build Configuration Options", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md78", [
        [ "Full Build (Development)", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md79", null ],
        [ "Minimal Build", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md80", null ]
      ] ],
      [ "Implementation Details", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md81", null ],
      [ "Platform-Specific Adaptations", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md82", [
        [ "Windows", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md83", null ],
        [ "macOS", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md84", null ],
        [ "Linux", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md85", null ]
      ] ]
    ] ],
    [ "Build Presets Guide", "d7/d51/md_docs_2B3-build-presets.html", [
      [ "Design Principles", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md87", null ],
      [ "Quick Start", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md88", [
        [ "macOS Development", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md89", null ],
        [ "Windows Development", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md90", null ]
      ] ],
      [ "All Presets", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md91", [
        [ "macOS Presets", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md92", null ],
        [ "Windows Presets", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md93", null ],
        [ "Linux Presets", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md94", null ],
        [ "Special Purpose", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md95", null ]
      ] ],
      [ "Warning Control", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md96", [
        [ "To Enable Verbose Warnings:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md97", null ]
      ] ],
      [ "Architecture Support", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md98", [
        [ "macOS", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md99", null ],
        [ "Windows", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md100", null ]
      ] ],
      [ "Build Directories", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md101", null ],
      [ "Feature Flags", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md102", null ],
      [ "Examples", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md103", [
        [ "Development with AI features and verbose warnings", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md104", null ],
        [ "Release build for distribution (macOS Universal)", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md105", null ],
        [ "Quick minimal build for testing", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md106", null ]
      ] ],
      [ "Updating compile_commands.json", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md107", null ],
      [ "Migration from Old Presets", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md108", null ],
      [ "Troubleshooting", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md109", [
        [ "Warnings are still showing", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md110", null ],
        [ "clangd can't find nlohmann/json", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md111", null ],
        [ "Build fails with missing dependencies", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md112", null ]
      ] ]
    ] ],
    [ "Release Workflows Documentation", "d3/d49/md_docs_2B4-release-workflows.html", [
      [ "Overview", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md114", null ],
      [ "1. Release-Simplified (<tt>release-simplified.yml</tt>)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md116", [
        [ "Purpose", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md117", null ],
        [ "Key Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md118", null ],
        [ "Use Cases", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md119", null ],
        [ "Configuration", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md120", null ],
        [ "Platforms Supported", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md121", null ]
      ] ],
      [ "2. Release (<tt>release.yml</tt>)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md123", [
        [ "Purpose", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md124", null ],
        [ "Key Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md125", null ],
        [ "Use Cases", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md126", null ],
        [ "Configuration", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md127", null ],
        [ "Advantages over Simplified", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md128", null ]
      ] ],
      [ "3. Release-Complex (<tt>release-complex.yml</tt>)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md130", [
        [ "Purpose", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md131", null ],
        [ "Key Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md132", null ],
        [ "Use Cases", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md133", null ],
        [ "Fallback Mechanisms", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md134", [
          [ "vcpkg Fallback", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md135", null ],
          [ "Chocolatey Integration", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md136", null ],
          [ "Build Configuration Fallback", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md137", null ]
        ] ],
        [ "Advanced Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md138", null ]
      ] ],
      [ "Workflow Comparison Matrix", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md140", null ],
      [ "When to Use Each Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md142", [
        [ "Use Simplified When:", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md143", null ],
        [ "Use Release When:", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md144", null ],
        [ "Use Complex When:", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md145", null ]
      ] ],
      [ "Workflow Selection Guide", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md147", [
        [ "For Development Team", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md148", null ],
        [ "For Release Manager", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md149", null ],
        [ "For CI/CD Pipeline", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md150", null ]
      ] ],
      [ "Configuration Examples", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md152", [
        [ "Triggering a Release", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md153", [
          [ "Manual Release (All Workflows)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md154", null ],
          [ "Automatic Release (Tag Push)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md155", null ]
        ] ],
        [ "Customizing Release Notes", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md156", null ]
      ] ],
      [ "Troubleshooting", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md158", [
        [ "Common Issues", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md159", [
          [ "vcpkg Failures (Windows)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md160", null ],
          [ "Dependency Conflicts", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md161", null ],
          [ "Build Failures", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md162", null ]
        ] ],
        [ "Debug Information", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md163", [
          [ "Simplified Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md164", null ],
          [ "Release Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md165", null ],
          [ "Complex Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md166", null ]
        ] ]
      ] ],
      [ "Best Practices", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md168", [
        [ "Workflow Selection", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md169", null ],
        [ "Release Process", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md170", null ],
        [ "Maintenance", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md171", null ]
      ] ],
      [ "Future Improvements", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md173", [
        [ "Planned Enhancements", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md174", null ],
        [ "Integration Opportunities", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md175", null ]
      ] ]
    ] ],
    [ "Changelog", "d7/d6f/md_docs_2C1-changelog.html", [
      [ "0.3.2 (December 2025) - In Development", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md178", [
        [ "Tile16 Editor Improvements (In Progress)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md179", null ],
        [ "Graphics System Optimizations", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md180", null ],
        [ "Windows Platform Stability", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md181", null ],
        [ "Memory Safety & Stability", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md182", null ],
        [ "Testing & CI/CD Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md183", null ],
        [ "Future Optimizations (Planned)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md184", null ],
        [ "Technical Notes", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md185", null ]
      ] ],
      [ "0.3.1 (September 2025)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md186", [
        [ "Major Features", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md187", null ],
        [ "Tile16 Editor Enhancements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md188", null ],
        [ "ZSCustomOverworld v3 Implementation", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md189", null ],
        [ "Technical Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md190", null ],
        [ "User Interface", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md191", null ],
        [ "Bug Fixes", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md192", null ],
        [ "ZScream Compatibility Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md193", null ]
      ] ],
      [ "0.3.0 (September 2025)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md194", [
        [ "Major Features", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md195", null ],
        [ "User Interface & Theming", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md196", null ],
        [ "Enhancements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md197", null ],
        [ "Technical Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md198", null ],
        [ "Bug Fixes", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md199", null ]
      ] ],
      [ "0.2.2 (December 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md200", null ],
      [ "0.2.1 (August 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md201", null ],
      [ "0.2.0 (July 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md202", null ],
      [ "0.1.0 (May 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md203", null ],
      [ "0.0.9 (April 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md204", null ],
      [ "0.0.8 (February 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md205", null ],
      [ "0.0.7 (January 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md206", null ],
      [ "0.0.6 (November 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md207", null ],
      [ "0.0.5 (November 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md208", null ],
      [ "0.0.4 (November 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md209", null ],
      [ "0.0.3 (October 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md210", null ]
    ] ],
    [ "Roadmap", "d6/db4/md_docs_2D1-roadmap.html", [
      [ "Current Focus: AI & Editor Polish", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md212", null ],
      [ "0.4.X (Next Major Release) - Advanced Tooling & UX", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md214", [
        [ "Priority 1: Editor Features & UX", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md215", null ],
        [ "Priority 2: <tt>z3ed</tt> AI Agent", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md216", null ],
        [ "Priority 3: Testing & Stability", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md217", null ]
      ] ],
      [ "0.5.X - Feature Expansion", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md219", null ],
      [ "0.6.X - Content & Integration", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md221", null ],
      [ "Recently Completed (v0.3.2)", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md223", null ]
    ] ],
    [ "Yaze Dungeon Editor: Master Guide", "d5/d47/md_docs_2D2-dungeon-editor-guide.html", [
      [ "1. Current Status: Production Ready", "d5/d47/md_docs_2D2-dungeon-editor-guide.html#autotoc_md225", [
        [ "Known Issues & Next Steps", "d5/d47/md_docs_2D2-dungeon-editor-guide.html#autotoc_md226", null ]
      ] ],
      [ "2. Architecture: A Component-Based Design", "d5/d47/md_docs_2D2-dungeon-editor-guide.html#autotoc_md227", [
        [ "Core Components", "d5/d47/md_docs_2D2-dungeon-editor-guide.html#autotoc_md228", null ],
        [ "Data Flow", "d5/d47/md_docs_2D2-dungeon-editor-guide.html#autotoc_md229", null ]
      ] ],
      [ "3. Key Recent Fixes", "d5/d47/md_docs_2D2-dungeon-editor-guide.html#autotoc_md230", [
        [ "Critical Fix 1: The Coordinate System", "d5/d47/md_docs_2D2-dungeon-editor-guide.html#autotoc_md231", null ],
        [ "Critical Fix 2: The Test Suite", "d5/d47/md_docs_2D2-dungeon-editor-guide.html#autotoc_md232", null ]
      ] ],
      [ "4. ROM Internals & Data Structures", "d5/d47/md_docs_2D2-dungeon-editor-guide.html#autotoc_md233", [
        [ "Object Encoding", "d5/d47/md_docs_2D2-dungeon-editor-guide.html#autotoc_md234", null ],
        [ "Core Data Tables in ROM", "d5/d47/md_docs_2D2-dungeon-editor-guide.html#autotoc_md235", null ]
      ] ],
      [ "5. Testing: 100% Pass Rate", "d5/d47/md_docs_2D2-dungeon-editor-guide.html#autotoc_md236", [
        [ "How to Run Tests", "d5/d47/md_docs_2D2-dungeon-editor-guide.html#autotoc_md237", null ]
      ] ],
      [ "6. Dungeon Object Reference Tables", "d5/d47/md_docs_2D2-dungeon-editor-guide.html#autotoc_md238", [
        [ "Type 1 Object Reference Table", "d5/d47/md_docs_2D2-dungeon-editor-guide.html#autotoc_md239", null ],
        [ "Type 2 Object Reference Table", "d5/d47/md_docs_2D2-dungeon-editor-guide.html#autotoc_md240", null ],
        [ "Type 3 Object Reference Table", "d5/d47/md_docs_2D2-dungeon-editor-guide.html#autotoc_md241", null ]
      ] ]
    ] ],
    [ "Asm Style Guide", "d7/d9a/md_docs_2E1-asm-style-guide.html", [
      [ "Table of Contents", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md243", null ],
      [ "File Structure", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md244", null ],
      [ "Labels and Symbols", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md245", null ],
      [ "Comments", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md246", null ],
      [ "Instructions", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md247", null ],
      [ "Macros", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md248", null ],
      [ "Loops and Branching", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md249", null ],
      [ "Data Structures", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md250", null ],
      [ "Code Organization", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md251", null ],
      [ "Custom Code", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md252", null ]
    ] ],
    [ "Tile16 Editor Palette System", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html", [
      [ "Executive Summary", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md254", null ],
      [ "Problem Analysis", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md255", [
        [ "Critical Issues Identified", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md256", null ]
      ] ],
      [ "Solution Architecture", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md257", [
        [ "Core Design Principles", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md258", null ],
        [ "256-Color Overworld Palette Structure", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md259", null ],
        [ "Sheet-to-Palette Mapping", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md260", null ],
        [ "Palette Button Mapping", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md261", null ]
      ] ],
      [ "Implementation Details", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md262", [
        [ "1. Fixed SetPaletteWithTransparent Method", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md263", null ],
        [ "2. Corrected Tile16 Editor Palette System", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md264", null ],
        [ "3. Palette Coordination Flow", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md265", null ]
      ] ],
      [ "UI/UX Refactoring", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md266", [
        [ "New Three-Column Layout", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md267", null ],
        [ "Canvas Context Menu Fixes", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md268", null ],
        [ "Dynamic Zoom Controls", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md269", null ]
      ] ],
      [ "Testing Protocol", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md270", [
        [ "Crash Prevention Testing", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md271", null ],
        [ "Color Alignment Testing", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md272", null ],
        [ "UI/UX Testing", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md273", null ]
      ] ],
      [ "Error Handling", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md274", [
        [ "Bounds Checking", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md275", null ],
        [ "Fallback Mechanisms", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md276", null ]
      ] ],
      [ "Debug Information Display", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md277", null ],
      [ "Known Issues and Ongoing Work", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md278", [
        [ "Completed Items ✅", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md279", null ],
        [ "Active Issues ⚠️", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md280", null ],
        [ "Current Status Summary", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md281", null ],
        [ "Future Enhancements", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md282", null ]
      ] ],
      [ "Maintenance Notes", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md283", null ],
      [ "Next Steps", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md285", [
        [ "Immediate Priorities", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md286", null ],
        [ "Investigation Areas", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md287", null ]
      ] ]
    ] ],
    [ "Overworld Loading Guide", "dc/d12/md_docs_2F1-overworld-loading.html", [
      [ "Table of Contents", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md290", null ],
      [ "Overview", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md291", null ],
      [ "ROM Types and Versions", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md292", [
        [ "Version Detection", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md293", null ],
        [ "Feature Support by Version", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md294", null ]
      ] ],
      [ "Overworld Map Structure", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md295", [
        [ "Core Properties", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md296", null ]
      ] ],
      [ "Overlays and Special Area Maps", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md297", [
        [ "Understanding Overlays", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md298", null ],
        [ "Special Area Maps (0x80-0x9F)", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md299", null ],
        [ "Overlay ID Mappings", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md300", null ],
        [ "Drawing Order", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md301", null ],
        [ "Vanilla Overlay Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md302", null ],
        [ "Special Area Graphics Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md303", null ]
      ] ],
      [ "Loading Process", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md304", [
        [ "1. Version Detection", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md305", null ],
        [ "2. Map Initialization", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md306", null ],
        [ "3. Property Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md307", [
          [ "Vanilla ROMs (asm_version == 0xFF)", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md308", null ],
          [ "ZSCustomOverworld v2/v3", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md309", null ]
        ] ],
        [ "4. Custom Data Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md310", null ]
      ] ],
      [ "ZScream Implementation", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md311", [
        [ "OverworldMap Constructor", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md312", null ],
        [ "Key Methods", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md313", null ]
      ] ],
      [ "Yaze Implementation", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md314", [
        [ "OverworldMap Constructor", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md315", null ],
        [ "Key Methods", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md316", null ],
        [ "Current Status", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md317", null ]
      ] ],
      [ "Key Differences", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md318", [
        [ "1. Language and Architecture", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md319", null ],
        [ "2. Data Structures", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md320", null ],
        [ "3. Error Handling", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md321", null ],
        [ "4. Graphics Processing", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md322", null ]
      ] ],
      [ "Common Issues and Solutions", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md323", [
        [ "1. Version Detection Issues", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md324", null ],
        [ "2. Palette Loading Errors", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md325", null ],
        [ "3. Graphics Not Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md326", null ],
        [ "4. Overlay Issues", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md327", null ],
        [ "5. Large Map Problems", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md328", null ],
        [ "6. Special Area Graphics Issues", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md329", null ]
      ] ],
      [ "Best Practices", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md330", [
        [ "1. Version-Specific Code", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md331", null ],
        [ "2. Error Handling", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md332", null ],
        [ "3. Memory Management", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md333", null ],
        [ "4. Thread Safety", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md334", null ]
      ] ],
      [ "Conclusion", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md335", null ]
    ] ],
    [ "Canvas System", "d0/d7d/md_docs_2G2-canvas-guide.html", [
      [ "Overview", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md337", null ],
      [ "Core Concepts", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md338", [
        [ "Canvas Structure", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md339", null ],
        [ "Coordinate Systems", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md340", null ]
      ] ],
      [ "Usage Patterns", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md341", [
        [ "Pattern 1: Basic Bitmap Display", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md342", null ],
        [ "Pattern 2: Modern Begin/End", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md343", null ],
        [ "Pattern 3: RAII ScopedCanvas", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md344", null ]
      ] ],
      [ "Feature: Tile Painting", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md345", [
        [ "Single Tile Painting", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md346", null ],
        [ "Tilemap Painting", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md347", null ],
        [ "Color Painting", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md348", null ]
      ] ],
      [ "Feature: Tile Selection", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md349", [
        [ "Single Tile Selection", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md350", null ],
        [ "Multi-Tile Rectangle Selection", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md351", null ],
        [ "Rectangle Drag & Paint", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md352", null ]
      ] ],
      [ "Feature: Custom Overlays", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md353", [
        [ "Manual Points Manipulation", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md354", null ]
      ] ],
      [ "Feature: Large Map Support", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md355", [
        [ "Map Types", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md356", null ],
        [ "Boundary Clamping", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md357", null ],
        [ "Custom Map Sizes", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md358", null ]
      ] ],
      [ "Feature: Context Menu", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md359", [
        [ "Adding Custom Items", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md360", null ],
        [ "Overworld Editor Example", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md361", null ]
      ] ],
      [ "Feature: Scratch Space (In Progress)", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md362", null ],
      [ "Common Workflows", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md363", [
        [ "Workflow 1: Overworld Tile Painting", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md364", null ],
        [ "Workflow 2: Tile16 Blockset Selection", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md365", null ],
        [ "Workflow 3: Graphics Sheet Display", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md366", null ]
      ] ],
      [ "Configuration", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md367", [
        [ "Grid Settings", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md368", null ],
        [ "Scale Settings", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md369", null ],
        [ "Interaction Settings", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md370", null ],
        [ "Large Map Settings", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md371", null ]
      ] ],
      [ "Known Issues", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md372", [
        [ "⚠️ Rectangle Selection Wrapping", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md373", null ]
      ] ],
      [ "API Reference", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md374", [
        [ "Drawing Methods", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md375", null ],
        [ "State Accessors", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md376", null ],
        [ "Configuration", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md377", null ]
      ] ],
      [ "Implementation Notes", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md378", [
        [ "Points Management", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md379", null ],
        [ "Selection State", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md380", null ],
        [ "Overworld Rectangle Painting Flow", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md381", null ]
      ] ],
      [ "Best Practices", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md382", [
        [ "DO ✅", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md383", null ],
        [ "DON'T ❌", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md384", null ]
      ] ],
      [ "Troubleshooting", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md385", [
        [ "Issue: Rectangle wraps at boundaries", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md386", null ],
        [ "Issue: Painting in wrong location", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md387", null ],
        [ "Issue: Array index out of bounds", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md388", null ],
        [ "Issue: Forgot to call End()", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md389", null ]
      ] ],
      [ "Future: Scratch Space", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md390", null ],
      [ "Summary", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md391", null ]
    ] ],
    [ "yaze Documentation", "d3/d4c/md_docs_2index.html", [
      [ "Core Guides", "d3/d4c/md_docs_2index.html#autotoc_md393", null ],
      [ "Development & API", "d3/d4c/md_docs_2index.html#autotoc_md394", null ],
      [ "Technical Documentation", "d3/d4c/md_docs_2index.html#autotoc_md395", null ]
    ] ],
    [ "Networking and Collaboration", "d6/d3a/md_docs_2z3ed_2NETWORKING.html", [
      [ "1. Overview", "d6/d3a/md_docs_2z3ed_2NETWORKING.html#autotoc_md398", null ],
      [ "2. Architecture", "d6/d3a/md_docs_2z3ed_2NETWORKING.html#autotoc_md399", [
        [ "2.1. System Diagram", "d6/d3a/md_docs_2z3ed_2NETWORKING.html#autotoc_md400", null ],
        [ "2.2. Core Components", "d6/d3a/md_docs_2z3ed_2NETWORKING.html#autotoc_md401", null ]
      ] ],
      [ "3. Protocols", "d6/d3a/md_docs_2z3ed_2NETWORKING.html#autotoc_md402", [
        [ "3.1. WebSocket Protocol (yaze-server v2.0)", "d6/d3a/md_docs_2z3ed_2NETWORKING.html#autotoc_md403", null ],
        [ "3.2. gRPC Service (Remote ROM Manipulation)", "d6/d3a/md_docs_2z3ed_2NETWORKING.html#autotoc_md404", null ]
      ] ],
      [ "4. Client Integration", "d6/d3a/md_docs_2z3ed_2NETWORKING.html#autotoc_md405", [
        [ "4.1. YAZE App Integration", "d6/d3a/md_docs_2z3ed_2NETWORKING.html#autotoc_md406", null ],
        [ "4.2. z3ed CLI Integration", "d6/d3a/md_docs_2z3ed_2NETWORKING.html#autotoc_md407", null ]
      ] ],
      [ "5. Best Practices & Troubleshooting", "d6/d3a/md_docs_2z3ed_2NETWORKING.html#autotoc_md408", [
        [ "Best Practices", "d6/d3a/md_docs_2z3ed_2NETWORKING.html#autotoc_md409", null ],
        [ "Troubleshooting", "d6/d3a/md_docs_2z3ed_2NETWORKING.html#autotoc_md410", null ]
      ] ],
      [ "6. Security Considerations", "d6/d3a/md_docs_2z3ed_2NETWORKING.html#autotoc_md411", null ],
      [ "7. Future Enhancements", "d6/d3a/md_docs_2z3ed_2NETWORKING.html#autotoc_md412", null ]
    ] ],
    [ "z3ed: AI-Powered CLI for YAZE", "d3/d63/md_docs_2z3ed_2README.html", [
      [ "1. Overview", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md414", [
        [ "Core Capabilities", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md415", null ]
      ] ],
      [ "2. Quick Start", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md416", [
        [ "Build", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md417", null ],
        [ "AI Setup", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md418", null ],
        [ "Example Commands", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md419", null ],
        [ "Hybrid CLI ↔ GUI Workflow", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md420", null ]
      ] ],
      [ "3. Architecture", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md421", [
        [ "System Components Diagram", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md422", null ]
      ] ],
      [ "4. Agentic & Generative Workflow (MCP)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md423", null ],
      [ "5. Command Reference", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md424", [
        [ "Agent Commands", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md425", null ],
        [ "Resource Commands", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md426", [
          [ "<tt>agent test</tt>: Live Harness Automation", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md427", null ]
        ] ]
      ] ],
      [ "6. Chat Modes", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md428", [
        [ "FTXUI Chat (<tt>agent chat</tt>)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md429", null ],
        [ "Simple Chat (<tt>agent simple-chat</tt>)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md430", null ],
        [ "GUI Chat Widget (Editor Integration)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md431", null ]
      ] ],
      [ "7. AI Provider Configuration", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md432", [
        [ "System Prompt Versions", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md433", null ]
      ] ],
      [ "8. Learn Command - Knowledge Management", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md434", [
        [ "Basic Usage", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md435", null ],
        [ "Project Context", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md436", null ],
        [ "Conversation Memory", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md437", null ],
        [ "Storage Location", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md438", null ]
      ] ],
      [ "9. TODO Management System", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md439", [
        [ "Core Capabilities", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md440", null ],
        [ "Storage Location", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md441", null ]
      ] ],
      [ "10. CLI Output & Help System", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md442", [
        [ "Verbose Logging", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md443", null ],
        [ "Hierarchical Help System", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md444", null ]
      ] ],
      [ "10. Collaborative Sessions & Multimodal Vision", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md445", [
        [ "Overview", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md446", null ],
        [ "Local Collaboration Mode", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md448", [
          [ "How to Use", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md449", null ],
          [ "Features", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md450", null ],
          [ "Cloud Folder Workaround", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md451", null ]
        ] ],
        [ "Network Collaboration Mode (yaze-server v2.0)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md453", [
          [ "Requirements", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md454", null ],
          [ "Server Setup", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md455", null ],
          [ "Client Connection", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md456", null ],
          [ "Core Features", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md457", null ],
          [ "Advanced Features (v2.0)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md458", null ],
          [ "Protocol Reference", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md459", null ],
          [ "Server Configuration", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md460", null ],
          [ "Database Schema (Server v2.0)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md461", null ],
          [ "Deployment", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md462", null ],
          [ "Testing", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md463", null ],
          [ "Security Considerations", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md464", null ]
        ] ],
        [ "Multimodal Vision (Gemini)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md466", [
          [ "Requirements", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md467", null ],
          [ "Capture Modes", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md468", null ],
          [ "How to Use", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md469", null ],
          [ "Example Prompts", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md470", null ]
        ] ],
        [ "Architecture", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md472", null ],
        [ "Troubleshooting", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md474", null ],
        [ "References", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md476", null ]
      ] ],
      [ "11. Roadmap & Implementation Status", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md477", [
        [ "✅ Completed", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md478", null ],
        [ "📌 Current Progress Highlights (October 5, 2025)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md479", null ],
        [ "🚧 Active & Next Steps", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md480", null ],
        [ "✅ Recently Completed (v0.2.0-alpha - October 5, 2025)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md481", [
          [ "Core AI Features", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md482", null ],
          [ "Version Management & Protection", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md483", null ],
          [ "Networking & Collaboration (NEW)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md484", null ],
          [ "UI/UX Enhancements", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md485", null ],
          [ "Build System & Infrastructure", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md486", null ]
        ] ]
      ] ],
      [ "12. Troubleshooting", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md487", null ]
    ] ],
    [ "yaze - Yet Another Zelda3 Editor", "d0/d30/md_README.html", [
      [ "Version 0.3.2 - Release", "d0/d30/md_README.html#autotoc_md489", null ],
      [ "Quick Start", "d0/d30/md_README.html#autotoc_md494", [
        [ "Build", "d0/d30/md_README.html#autotoc_md495", null ],
        [ "Applications", "d0/d30/md_README.html#autotoc_md496", null ]
      ] ],
      [ "Usage", "d0/d30/md_README.html#autotoc_md497", [
        [ "GUI Editor", "d0/d30/md_README.html#autotoc_md498", null ],
        [ "Command Line Tool", "d0/d30/md_README.html#autotoc_md499", null ],
        [ "C++ API", "d0/d30/md_README.html#autotoc_md500", null ]
      ] ],
      [ "Documentation", "d0/d30/md_README.html#autotoc_md501", null ],
      [ "Supported Platforms", "d0/d30/md_README.html#autotoc_md502", null ],
      [ "ROM Compatibility", "d0/d30/md_README.html#autotoc_md503", null ],
      [ "Contributing", "d0/d30/md_README.html#autotoc_md504", null ],
      [ "License", "d0/d30/md_README.html#autotoc_md505", null ],
      [ "🙏 Acknowledgments", "d0/d30/md_README.html#autotoc_md506", null ],
      [ "📸 Screenshots", "d0/d30/md_README.html#autotoc_md507", null ]
    ] ],
    [ "yaze Build Scripts", "de/d82/md_scripts_2README.html", [
      [ "Windows Scripts", "de/d82/md_scripts_2README.html#autotoc_md510", [
        [ "vcpkg Setup (Optional)", "de/d82/md_scripts_2README.html#autotoc_md511", null ]
      ] ],
      [ "Windows Build Workflow", "de/d82/md_scripts_2README.html#autotoc_md512", [
        [ "Recommended: Visual Studio CMake Mode", "de/d82/md_scripts_2README.html#autotoc_md513", null ],
        [ "Command Line Build", "de/d82/md_scripts_2README.html#autotoc_md514", null ],
        [ "Compiler Notes", "de/d82/md_scripts_2README.html#autotoc_md515", null ]
      ] ],
      [ "Quick Start (Windows)", "de/d82/md_scripts_2README.html#autotoc_md516", [
        [ "Option 1: Visual Studio (Recommended)", "de/d82/md_scripts_2README.html#autotoc_md517", null ],
        [ "Option 2: Command Line", "de/d82/md_scripts_2README.html#autotoc_md518", null ],
        [ "Option 3: With vcpkg (Optional)", "de/d82/md_scripts_2README.html#autotoc_md519", null ]
      ] ],
      [ "Troubleshooting", "de/d82/md_scripts_2README.html#autotoc_md520", [
        [ "Common Issues", "de/d82/md_scripts_2README.html#autotoc_md521", null ],
        [ "Getting Help", "de/d82/md_scripts_2README.html#autotoc_md522", null ]
      ] ],
      [ "Other Scripts", "de/d82/md_scripts_2README.html#autotoc_md523", null ],
      [ "Build Environment Verification", "de/d82/md_scripts_2README.html#autotoc_md524", [
        [ "<tt>verify-build-environment.ps1</tt> / <tt>.sh</tt>", "de/d82/md_scripts_2README.html#autotoc_md525", null ],
        [ "Usage", "de/d82/md_scripts_2README.html#autotoc_md526", null ]
      ] ]
    ] ],
    [ "Agent Editor Module", "d6/df7/md_src_2app_2editor_2agent_2README.html", [
      [ "Overview", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md528", null ],
      [ "Architecture", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md529", [
        [ "Core Components", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md530", [
          [ "AgentEditor (<tt>agent_editor.h/cc</tt>)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md531", null ],
          [ "AgentChatWidget (<tt>agent_chat_widget.h/cc</tt>)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md532", null ],
          [ "AgentChatHistoryCodec (<tt>agent_chat_history_codec.h/cc</tt>)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md533", null ]
        ] ],
        [ "Collaboration Coordinators", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md534", [
          [ "AgentCollaborationCoordinator (<tt>agent_collaboration_coordinator.h/cc</tt>)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md535", null ],
          [ "NetworkCollaborationCoordinator (<tt>network_collaboration_coordinator.h/cc</tt>)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md536", null ]
        ] ]
      ] ],
      [ "Usage", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md537", [
        [ "Initialization", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md538", null ],
        [ "Drawing", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md539", null ],
        [ "Session Management", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md540", null ],
        [ "Network Mode (requires YAZE_WITH_GRPC and YAZE_WITH_JSON)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md541", null ]
      ] ],
      [ "File Structure", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md542", null ],
      [ "Build Configuration", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md543", [
        [ "Required", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md544", null ],
        [ "Optional", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md545", null ]
      ] ],
      [ "Data Files", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md546", [
        [ "Local Storage", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md547", null ],
        [ "Session File Format", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md548", null ]
      ] ],
      [ "Integration with EditorManager", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md549", null ],
      [ "Dependencies", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md550", [
        [ "Internal", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md551", null ],
        [ "External (when enabled)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md552", null ]
      ] ],
      [ "Advanced Features (v2.0)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md553", [
        [ "ROM Synchronization", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md554", null ],
        [ "Multimodal Snapshot Sharing", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md555", null ],
        [ "Proposal Management", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md556", null ],
        [ "AI Agent Integration", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md557", null ],
        [ "Health Monitoring", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md558", null ]
      ] ],
      [ "Future Enhancements", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md559", null ],
      [ "Server Protocol", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md560", [
        [ "Client → Server", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md561", null ],
        [ "Server → Client", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md562", null ]
      ] ]
    ] ],
    [ "End-to-End (E2E) Tests", "d9/db0/md_test_2e2e_2README.html", [
      [ "Active Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md564", [
        [ "✅ Working Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md565", null ],
        [ "📝 Dungeon Editor Smoke Test", "d9/db0/md_test_2e2e_2README.html#autotoc_md566", null ]
      ] ],
      [ "Running Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md567", [
        [ "All E2E Tests (GUI Mode)", "d9/db0/md_test_2e2e_2README.html#autotoc_md568", null ],
        [ "Specific Test Category", "d9/db0/md_test_2e2e_2README.html#autotoc_md569", null ],
        [ "Dungeon Editor Test Only", "d9/db0/md_test_2e2e_2README.html#autotoc_md570", null ]
      ] ],
      [ "Test Development", "d9/db0/md_test_2e2e_2README.html#autotoc_md571", [
        [ "Creating New Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md572", null ],
        [ "Register in yaze_test.cc", "d9/db0/md_test_2e2e_2README.html#autotoc_md573", null ],
        [ "ImGui Test Engine API", "d9/db0/md_test_2e2e_2README.html#autotoc_md574", null ]
      ] ],
      [ "Test Logging", "d9/db0/md_test_2e2e_2README.html#autotoc_md575", null ],
      [ "Test Infrastructure", "d9/db0/md_test_2e2e_2README.html#autotoc_md576", [
        [ "File Organization", "d9/db0/md_test_2e2e_2README.html#autotoc_md577", null ],
        [ "Helper Functions", "d9/db0/md_test_2e2e_2README.html#autotoc_md578", null ]
      ] ],
      [ "Future Test Ideas", "d9/db0/md_test_2e2e_2README.html#autotoc_md579", null ],
      [ "Troubleshooting", "d9/db0/md_test_2e2e_2README.html#autotoc_md580", [
        [ "Test Crashes in GUI Mode", "d9/db0/md_test_2e2e_2README.html#autotoc_md581", null ],
        [ "Tests Not Found", "d9/db0/md_test_2e2e_2README.html#autotoc_md582", null ],
        [ "ImGui Items Not Found", "d9/db0/md_test_2e2e_2README.html#autotoc_md583", null ]
      ] ],
      [ "References", "d9/db0/md_test_2e2e_2README.html#autotoc_md584", null ],
      [ "Status", "d9/db0/md_test_2e2e_2README.html#autotoc_md585", null ]
    ] ],
    [ "yaze Test Suite", "d0/d46/md_test_2README.html", [
      [ "Directory Structure", "d0/d46/md_test_2README.html#autotoc_md587", null ],
      [ "Test Categories", "d0/d46/md_test_2README.html#autotoc_md588", [
        [ "Unit Tests (<tt>unit/</tt>)", "d0/d46/md_test_2README.html#autotoc_md589", null ],
        [ "Integration Tests (<tt>integration/</tt>)", "d0/d46/md_test_2README.html#autotoc_md590", null ],
        [ "End-to-End Tests (<tt>e2e/</tt>)", "d0/d46/md_test_2README.html#autotoc_md591", null ]
      ] ],
      [ "Enhanced Test Runner", "d0/d46/md_test_2README.html#autotoc_md592", [
        [ "Usage Examples", "d0/d46/md_test_2README.html#autotoc_md593", null ],
        [ "Test Modes", "d0/d46/md_test_2README.html#autotoc_md594", null ],
        [ "Options", "d0/d46/md_test_2README.html#autotoc_md595", null ]
      ] ],
      [ "E2E ROM Testing", "d0/d46/md_test_2README.html#autotoc_md596", [
        [ "Test Cases", "d0/d46/md_test_2README.html#autotoc_md597", null ]
      ] ],
      [ "ZSCustomOverworld Upgrade Testing", "d0/d46/md_test_2README.html#autotoc_md598", [
        [ "Supported Upgrades", "d0/d46/md_test_2README.html#autotoc_md599", null ],
        [ "Test Cases", "d0/d46/md_test_2README.html#autotoc_md600", null ],
        [ "Version-Specific Features", "d0/d46/md_test_2README.html#autotoc_md601", [
          [ "Vanilla", "d0/d46/md_test_2README.html#autotoc_md602", null ],
          [ "v2", "d0/d46/md_test_2README.html#autotoc_md603", null ],
          [ "v3", "d0/d46/md_test_2README.html#autotoc_md604", null ]
        ] ]
      ] ],
      [ "Environment Variables", "d0/d46/md_test_2README.html#autotoc_md605", null ],
      [ "CI/CD Integration", "d0/d46/md_test_2README.html#autotoc_md606", null ],
      [ "Deprecated Tests", "d0/d46/md_test_2README.html#autotoc_md607", null ],
      [ "Best Practices", "d0/d46/md_test_2README.html#autotoc_md608", null ],
      [ "AI Agent Testing", "d0/d46/md_test_2README.html#autotoc_md609", null ]
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
        [ "Enumerator", "functions_eval.html", null ],
        [ "Related Symbols", "functions_rela.html", null ]
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
"d0/d20/classyaze_1_1test_1_1E2ETestSuite.html#a7c54e1f2c12e718754f335963bcf0cc8",
"d0/d28/classTextEditor.html#a8f5a14943d791c164cce0ea5c21a7db6",
"d0/d54/background__buffer_8h.html",
"d0/d85/structyaze_1_1cli_1_1WidgetDescriptor.html#add15383069164ad929e46004d4eb9d3f",
"d0/dc7/classyaze_1_1test_1_1DungeonObjectRenderingE2ETests.html#a44939dab1f85e672713389712d368c10",
"d1/d04/structyaze_1_1emu_1_1TilemapEntry.html#a828cbb0f34624b0c5e43bcf64972c957",
"d1/d22/classyaze_1_1editor_1_1DungeonObjectInteraction.html#a9868aefd984244c692bbe8f2167bb578",
"d1/d3e/namespaceyaze_1_1editor.html#a6b79d859cd4ca38f690950e082e8f025",
"d1/d4f/overworld__map_8h.html#a607408467c46951f2ab11ba3c694ca3d",
"d1/d8a/room__entrance_8h.html#aded4493595ac61d0856cb4a5e6a94e1f",
"d1/daf/compression_8h.html#a1eddb91cde2953096c2302e082d8c3b2",
"d1/de2/classyaze_1_1test_1_1ZSCustomOverworldUpgradeTest.html#ac29885265340452b7540c4afc9ef44eb",
"d2/d07/classyaze_1_1emu_1_1Spc700.html#a1e497b72d2a6157e23a5c8e863a85af1",
"d2/d09/structyaze_1_1emu_1_1ScrollRegister.html",
"d2/d46/classyaze_1_1editor_1_1PaletteEditor.html#ae9485cdf2293fff0dfaa3964dd0cb6c0",
"d2/d5e/classyaze_1_1emu_1_1Memory.html#ab13a96c08299c20cb973683f2c33199d",
"d2/dba/dungeon__map_8h.html#a4ac909ac2ccc7d3a70ae66895e832b28",
"d2/dea/structyaze_1_1gfx_1_1AtlasStats.html#ad8446cb4d432e656bb74e04ca148a21c",
"d2/dfe/structyaze_1_1gui_1_1EnhancedTheme.html#a6f3cbf921c5b68bc79223751da97fe06",
"d3/d15/classyaze_1_1emu_1_1Snes.html#aa83090e72f5a579aeff5f2654137264c",
"d3/d27/classyaze_1_1gui_1_1ThemeManager.html#aef7a53bab0bdbb234e5ccf5d20b1f5b9",
"d3/d44/classyaze_1_1editor_1_1OverworldEditor.html#a136f4a855e2b8baf01ba6b0256cb8733",
"d3/d44/classyaze_1_1editor_1_1OverworldEditor.html#aea8267c4f7c87f90980a353edb9d27a1a15e93c1f924790725359e5f3ffa062c8",
"d3/d63/md_docs_2z3ed_2README.html#autotoc_md439",
"d3/d6c/classyaze_1_1editor_1_1MessageEditor.html#adaa239807ea089eec98ef78bc15c912a",
"d3/d8f/structyaze_1_1gui_1_1canvas_1_1CanvasConfig.html#af39665815d5c2f96c519e755e34b6f71",
"d3/dae/structyaze_1_1zelda3_1_1music_1_1ZeldaInstrument.html#a626b8dbaf4d8c3c51eee8f358c70dff6",
"d3/dbf/namespaceyaze_1_1gui.html#acffe7391ebbf95b3313a697bcb629ab3",
"d3/ded/classyaze_1_1emu_1_1Ppu.html#a75b2ad5e324558a0dc2e49acb0f51483",
"d4/d0a/classyaze_1_1gfx_1_1PerformanceProfiler.html#a01ac5ddfcc99aee15a5f929f69b0ec42",
"d4/d0a/namespaceyaze_1_1test.html#accde1cc1e268583c589cf5eb583c37e8",
"d4/d4b/structyaze_1_1gui_1_1ExampleSelectionWithDeletion.html",
"d4/d7c/classyaze_1_1emu_1_1PpuInterface.html#a4e3519f11646f16198c8c7d551098908",
"d4/da1/classyaze_1_1gui_1_1canvas_1_1CanvasUsageManager.html#aa19a595c63eeb26b3a4567104432b6ff",
"d4/dda/structyaze_1_1emu_1_1CGWSEL.html#a0925fdb61d2617feda44714c3151a5aa",
"d5/d0b/dungeon__object__rendering__tests_8cc.html#a9d34ba2fcd6d9a4edf763cb7d960a0c3",
"d5/d1f/namespaceyaze_1_1zelda3.html#a5fed47e695c5c4b5ca8f849d873fcca8",
"d5/d1f/namespaceyaze_1_1zelda3.html#abdd516737496637f907aa46b74b54877",
"d5/d47/md_docs_2D2-dungeon-editor-guide.html#autotoc_md228",
"d5/d71/structyaze_1_1editor_1_1AgentEditor_1_1BotProfile.html#a612871ca7d5b0cb85b4606629e609712",
"d5/da1/structyaze_1_1cli_1_1ProposalRegistry_1_1ProposalMetadata.html#acaeabd4e89a81ed8f2c56895ae9630c1",
"d5/dd0/classyaze_1_1test_1_1RomDependentTestSuite.html#a4be3f725e0b0964b6f077a4ba303ca2b",
"d6/d0d/classyaze_1_1editor_1_1AgentEditor.html#a7f2f7bf2a19b66cdff05c2599b94cabb",
"d6/d2d/build__cleaner_8py.html#a3c0b15f8f302ce144e2b76796a87991b",
"d6/d30/classyaze_1_1Rom.html#a99c1cb4e80c3cafb975d1d74a842c833",
"d6/d48/structyaze_1_1cli_1_1AutocompleteEngine_1_1CommandDef.html#a8c0cbb4a627435a7c45f55fbc604acc9",
"d6/d84/structyaze_1_1emu_1_1OPVCT.html#ac5e39c129d017a3beff6eace51e9e323",
"d6/db1/classyaze_1_1zelda3_1_1Sprite.html#ae91674decf920e5ac6add25d972916d7",
"d6/dd9/classyaze_1_1app_1_1net_1_1RomVersionManager.html#a4041e0ff5917236002355b647a7f9ebb",
"d7/d02/structyaze_1_1gui_1_1CanvasSelection.html#a662a5c1f704255c79a1c88e67d1ef7fb",
"d7/d61/classyaze_1_1zelda3_1_1DungeonEditorSystem.html",
"d7/d61/classyaze_1_1zelda3_1_1DungeonEditorSystem.html#ab79cd979e998f4bb4b87ff3e27f28779ac871be9d01b0cc22ff4c9e37cda9225e",
"d7/d79/namespaceyaze_1_1test_1_1gui.html#af12c501448079ffb3ab9d73d71237882",
"d7/da7/classyaze_1_1emu_1_1Apu.html#a2782614192cfb79d5b0e9cb32f65651b",
"d7/dcc/structyaze_1_1gfx_1_1PaletteGroupMap.html#ac810c058bc9d74f40e58e5aa4dbb3d41",
"d7/df6/classyaze_1_1zelda3_1_1Room.html#a5b697afa221b809696c101e96dc07e35",
"d7/dfa/classyaze_1_1zelda3_1_1ObjectRenderer.html#ac3287a5b94e75fa74d0b917127722c8b",
"d8/d0c/structyaze_1_1core_1_1ProjectMetadata.html#aa58b5a58a549f47128307de8545fd9ec",
"d8/d45/classyaze_1_1editor_1_1ExtensionManager.html",
"d8/d78/entity_8cc.html#a333d24169dffcc3ea067a7234180dc4a",
"d8/d9a/overworld__test_8cc.html#a7e24e387dbfd3eb3e911b7e079d76ad7",
"d8/dc5/structyaze_1_1app_1_1gui_1_1RomSyncEntry.html#ae6281116aef894a8b7a08c792c6f5d9f",
"d8/ddb/snes__palette_8h.html#a82a8956476ffc04750bcfc4120c8b8dba44b225210525a1b9ad60c50d0b712c16",
"d9/d12/classyaze_1_1cli_1_1SnesToPcCommand.html",
"d9/d2b/classyaze_1_1editor_1_1AgentChatWidget.html#aef6e20427b23a1ad78fbfb250a423aa0",
"d9/d7f/classyaze_1_1gui_1_1PaletteWidget.html#a24b9b011307637201d296e51c0baa1f4",
"d9/da8/classyaze_1_1editor_1_1EditorSelectionDialog.html#abfc3f9233a8a6612506c3753815058a5",
"d9/dc5/classyaze_1_1zelda3_1_1Overworld.html#a255e6fcd46be3ed63a80b171f75f4c33a7adac9506b7acb3ff2d04a94242d5cd4",
"d9/dcc/classyaze_1_1zelda3_1_1OverworldMap.html#a565e09d96550a5bc57f9978b2067ff5b",
"d9/dd2/classyaze_1_1cli_1_1agent_1_1VimMode.html#aa43cf83756d16afc2a3f62d72637ce23",
"d9/dff/ppu__registers_8h.html#a7ee6d65c178e28558735b7004a10939f",
"da/d2c/classyaze_1_1gui_1_1Canvas.html#a6bd0fc774adeb1b17b8baf0069065c5a",
"da/d36/namespaceyaze_1_1core_1_1anonymous__namespace_02asar__wrapper__test_8cc_03.html#a7888e952156cef5357cfd6cef36836ad",
"da/d4e/structyaze_1_1gui_1_1CanvasPaletteManager.html#ab2fa8ddb3265f85bef56d7e8a7221791",
"da/d77/classyaze_1_1cli_1_1PaletteExport.html#ae7ff17c016b9eb8fe237e27efb3d626a",
"da/dbc/classyaze_1_1emu_1_1AsmParser.html#a2dc4cfff44721a57acc183a7e122a9b0",
"da/ded/widget__state__capture_8cc.html#aa0da7cf3c3d6df4effb940215596dc92",
"db/d2d/classyaze_1_1cli_1_1GeminiAIService.html#a3b983e01884a58d17271442bebcb72c6",
"db/d56/classyaze_1_1gui_1_1BppConversionDialog.html#ac3388333518b5d722c7af8c30ed21b99",
"db/d82/classyaze_1_1editor_1_1Tile16Editor.html#a0560f2ab4b4f1d99de973cba0282de79",
"db/d8e/zelda3_2dungeon_2room__object__encoding__test_8cc.html#ae49d51ff32fafb2184e8bfdb3f34a4e3",
"db/dbd/namespaceyaze_1_1zelda3_1_1anonymous__namespace_02overworld_8cc_03.html",
"db/de2/structyaze_1_1cli_1_1StopRecordingResult.html",
"dc/d0c/tilemap_8h.html#a85cdb560638714d7afc71b20b1b8526c",
"dc/d31/classyaze_1_1editor_1_1GraphicsEditor.html#a820ebf77cda15a3855d9502c63ffb09d",
"dc/d4f/classyaze_1_1editor_1_1DungeonObjectSelector.html#a2a4e487bb8a91d89c8373a035865e899",
"dc/d71/structyaze_1_1editor_1_1UserSettings_1_1Preferences.html#ac26defb16c3c325865bbbde8bf41e781",
"dc/dc8/unit__test__suite_8h_source.html",
"dc/df4/classyaze_1_1emu_1_1Cpu.html#a384e5451767eb095f526360259321012",
"dc/df4/classyaze_1_1emu_1_1Cpu.html#ac28d253c8960201e66fd272cc976eabb",
"dd/d12/classyaze_1_1editor_1_1EditorManager.html#a264fafc6a2aa6ef71c7a33dfc808deec",
"dd/d19/structyaze_1_1core_1_1FeatureFlags_1_1Flags.html",
"dd/d4b/classyaze_1_1editor_1_1DungeonUsageTracker.html#a1a18d56c63361d86936503a3e87ceb92",
"dd/d71/classyaze_1_1gui_1_1canvas_1_1CanvasInteractionHandler.html#a46235a5ac8edfd11b583154a0631c6e9",
"dd/db7/structyaze_1_1emu_1_1BgLayer.html#aa6810a0dfbb99424f2546f5804889106",
"dd/ddf/classyaze_1_1gui_1_1canvas_1_1CanvasContextMenu.html#aaaf9e44993de729b84029f9ac9aa1d26",
"de/d0d/structzelda3__version__pointers.html#a550d75c32f95a2104fe390aec01378b4",
"de/d3d/yaze__test_8cc.html#a76748b4fd1d8105313f78a75a8e321a8",
"de/d82/classyaze_1_1util_1_1NotifyValue.html#a4f215603b2e5501223736557760f59ec",
"de/dab/tui_8h.html#ab3acc1b59e84abb18e2d438f7ed2ad33a90bfbb340d0004281b380b5058c652fd",
"de/dbf/icons_8h.html#a1c0f5f4a39a6975f61d01594c66cd5ec",
"de/dbf/icons_8h.html#a3ad17c3474edab3b55151f2471aef399",
"de/dbf/icons_8h.html#a56024a39d22af47ce4b7ab40baaf661d",
"de/dbf/icons_8h.html#a73f5281fdfea565cc2e9e2afb2abe8d4",
"de/dbf/icons_8h.html#a9383aff13d321c394808374378fdfd5e",
"de/dbf/icons_8h.html#ab0fe292db74593fc9f5b51ea40ab5698",
"de/dbf/icons_8h.html#acdb9285f139bbeb08d86da32b459308a",
"de/dbf/icons_8h.html#ae8db0b908fc75c0795e45905e1f94be8",
"de/dd3/structyaze_1_1cli_1_1agent_1_1ChatMessage_1_1ProposalSummary.html#a10172ae4cafa97afcb3e203479f07454",
"de/de7/classyaze_1_1zelda3_1_1DungeonObjectEditor.html#a98ae91e5df2700e9c1f9f75d182720a7",
"df/d0e/classyaze_1_1gfx_1_1Bitmap.html#aa2fa6419b66b704db0eba2d6dfcdea6a",
"df/d41/structyaze_1_1editor_1_1zsprite_1_1ZSprite.html#ade9410a1d3ef4014dfc68527846e983a",
"df/d9d/classyaze_1_1cli_1_1gui_1_1GuiActionGenerator.html#a55aba84f1fbe4759bde8ec196c59cab0",
"df/dcb/classyaze_1_1cli_1_1ai_1_1VisionActionRefiner.html#a2e4d34df096cd29ada1b28f61fe046c1",
"dir_8c83c8b12307546e2fe7c08e26a70ba7.html",
"pages.html"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';