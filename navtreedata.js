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
    [ "z3ed Networking, Collaboration, and Remote Access", "d6/d3a/md_docs_2z3ed_2NETWORKING.html", [
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
        [ "Example Commands", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md419", null ]
      ] ],
      [ "3. Architecture", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md420", [
        [ "System Components Diagram", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md421", null ]
      ] ],
      [ "4. Agentic & Generative Workflow (MCP)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md422", null ],
      [ "5. Command Reference", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md423", [
        [ "Agent Commands", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md424", null ],
        [ "Resource Commands", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md425", null ]
      ] ],
      [ "6. Chat Modes", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md426", [
        [ "FTXUI Chat (<tt>agent chat</tt>)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md427", null ],
        [ "Simple Chat (<tt>agent simple-chat</tt>)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md428", null ],
        [ "GUI Chat Widget (Editor Integration)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md429", null ]
      ] ],
      [ "7. AI Provider Configuration", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md430", [
        [ "System Prompt Versions", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md431", null ]
      ] ],
      [ "8. Learn Command - Knowledge Management", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md432", [
        [ "Basic Usage", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md433", null ],
        [ "Project Context", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md434", null ],
        [ "Conversation Memory", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md435", null ],
        [ "Storage Location", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md436", null ]
      ] ],
      [ "9. TODO Management System", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md437", [
        [ "Core Capabilities", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md438", null ],
        [ "Storage Location", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md439", null ]
      ] ],
      [ "10. CLI Output & Help System", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md440", [
        [ "Verbose Logging", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md441", null ],
        [ "Hierarchical Help System", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md442", null ]
      ] ],
      [ "10. Collaborative Sessions & Multimodal Vision", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md443", [
        [ "Overview", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md444", null ],
        [ "Local Collaboration Mode", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md446", [
          [ "How to Use", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md447", null ],
          [ "Features", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md448", null ],
          [ "Cloud Folder Workaround", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md449", null ]
        ] ],
        [ "Network Collaboration Mode (yaze-server v2.0)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md451", [
          [ "Requirements", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md452", null ],
          [ "Server Setup", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md453", null ],
          [ "Client Connection", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md454", null ],
          [ "Core Features", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md455", null ],
          [ "Advanced Features (v2.0)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md456", null ],
          [ "Protocol Reference", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md457", null ],
          [ "Server Configuration", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md458", null ],
          [ "Database Schema (Server v2.0)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md459", null ],
          [ "Deployment", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md460", null ],
          [ "Testing", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md461", null ],
          [ "Security Considerations", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md462", null ]
        ] ],
        [ "Multimodal Vision (Gemini)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md464", [
          [ "Requirements", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md465", null ],
          [ "Capture Modes", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md466", null ],
          [ "How to Use", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md467", null ],
          [ "Example Prompts", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md468", null ]
        ] ],
        [ "Architecture", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md470", null ],
        [ "Troubleshooting", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md472", null ],
        [ "References", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md474", null ]
      ] ],
      [ "11. Roadmap & Implementation Status", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md475", [
        [ "✅ Completed", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md476", null ],
        [ "🚧 Active & Next Steps", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md477", null ],
        [ "✅ Recently Completed (v0.2.0-alpha - October 5, 2025)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md478", [
          [ "Core AI Features", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md479", null ],
          [ "Version Management & Protection", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md480", null ],
          [ "Networking & Collaboration (NEW)", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md481", null ],
          [ "UI/UX Enhancements", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md482", null ],
          [ "Build System & Infrastructure", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md483", null ]
        ] ]
      ] ],
      [ "12. Troubleshooting", "d3/d63/md_docs_2z3ed_2README.html#autotoc_md484", null ]
    ] ],
    [ "yaze - Yet Another Zelda3 Editor", "d0/d30/md_README.html", [
      [ "Version 0.3.2 - Release", "d0/d30/md_README.html#autotoc_md486", null ],
      [ "Quick Start", "d0/d30/md_README.html#autotoc_md491", [
        [ "Build", "d0/d30/md_README.html#autotoc_md492", null ],
        [ "Applications", "d0/d30/md_README.html#autotoc_md493", null ]
      ] ],
      [ "Usage", "d0/d30/md_README.html#autotoc_md494", [
        [ "GUI Editor", "d0/d30/md_README.html#autotoc_md495", null ],
        [ "Command Line Tool", "d0/d30/md_README.html#autotoc_md496", null ],
        [ "C++ API", "d0/d30/md_README.html#autotoc_md497", null ]
      ] ],
      [ "Documentation", "d0/d30/md_README.html#autotoc_md498", null ],
      [ "Supported Platforms", "d0/d30/md_README.html#autotoc_md499", null ],
      [ "ROM Compatibility", "d0/d30/md_README.html#autotoc_md500", null ],
      [ "Contributing", "d0/d30/md_README.html#autotoc_md501", null ],
      [ "License", "d0/d30/md_README.html#autotoc_md502", null ],
      [ "🙏 Acknowledgments", "d0/d30/md_README.html#autotoc_md503", null ],
      [ "📸 Screenshots", "d0/d30/md_README.html#autotoc_md504", null ]
    ] ],
    [ "yaze Build Scripts", "de/d82/md_scripts_2README.html", [
      [ "Windows Scripts", "de/d82/md_scripts_2README.html#autotoc_md507", [
        [ "vcpkg Setup (Optional)", "de/d82/md_scripts_2README.html#autotoc_md508", null ]
      ] ],
      [ "Windows Build Workflow", "de/d82/md_scripts_2README.html#autotoc_md509", [
        [ "Recommended: Visual Studio CMake Mode", "de/d82/md_scripts_2README.html#autotoc_md510", null ],
        [ "Command Line Build", "de/d82/md_scripts_2README.html#autotoc_md511", null ],
        [ "Compiler Notes", "de/d82/md_scripts_2README.html#autotoc_md512", null ]
      ] ],
      [ "Quick Start (Windows)", "de/d82/md_scripts_2README.html#autotoc_md513", [
        [ "Option 1: Visual Studio (Recommended)", "de/d82/md_scripts_2README.html#autotoc_md514", null ],
        [ "Option 2: Command Line", "de/d82/md_scripts_2README.html#autotoc_md515", null ],
        [ "Option 3: With vcpkg (Optional)", "de/d82/md_scripts_2README.html#autotoc_md516", null ]
      ] ],
      [ "Troubleshooting", "de/d82/md_scripts_2README.html#autotoc_md517", [
        [ "Common Issues", "de/d82/md_scripts_2README.html#autotoc_md518", null ],
        [ "Getting Help", "de/d82/md_scripts_2README.html#autotoc_md519", null ]
      ] ],
      [ "Other Scripts", "de/d82/md_scripts_2README.html#autotoc_md520", null ],
      [ "Build Environment Verification", "de/d82/md_scripts_2README.html#autotoc_md521", [
        [ "<tt>verify-build-environment.ps1</tt> / <tt>.sh</tt>", "de/d82/md_scripts_2README.html#autotoc_md522", null ],
        [ "Usage", "de/d82/md_scripts_2README.html#autotoc_md523", null ]
      ] ]
    ] ],
    [ "Agent Editor Module", "d6/df7/md_src_2app_2editor_2agent_2README.html", [
      [ "Overview", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md525", null ],
      [ "Architecture", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md526", [
        [ "Core Components", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md527", [
          [ "AgentEditor (<tt>agent_editor.h/cc</tt>)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md528", null ],
          [ "AgentChatWidget (<tt>agent_chat_widget.h/cc</tt>)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md529", null ],
          [ "AgentChatHistoryCodec (<tt>agent_chat_history_codec.h/cc</tt>)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md530", null ]
        ] ],
        [ "Collaboration Coordinators", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md531", [
          [ "AgentCollaborationCoordinator (<tt>agent_collaboration_coordinator.h/cc</tt>)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md532", null ],
          [ "NetworkCollaborationCoordinator (<tt>network_collaboration_coordinator.h/cc</tt>)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md533", null ]
        ] ]
      ] ],
      [ "Usage", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md534", [
        [ "Initialization", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md535", null ],
        [ "Drawing", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md536", null ],
        [ "Session Management", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md537", null ],
        [ "Network Mode (requires YAZE_WITH_GRPC and YAZE_WITH_JSON)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md538", null ]
      ] ],
      [ "File Structure", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md539", null ],
      [ "Build Configuration", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md540", [
        [ "Required", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md541", null ],
        [ "Optional", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md542", null ]
      ] ],
      [ "Data Files", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md543", [
        [ "Local Storage", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md544", null ],
        [ "Session File Format", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md545", null ]
      ] ],
      [ "Integration with EditorManager", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md546", null ],
      [ "Dependencies", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md547", [
        [ "Internal", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md548", null ],
        [ "External (when enabled)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md549", null ]
      ] ],
      [ "Advanced Features (v2.0)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md550", [
        [ "ROM Synchronization", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md551", null ],
        [ "Multimodal Snapshot Sharing", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md552", null ],
        [ "Proposal Management", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md553", null ],
        [ "AI Agent Integration", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md554", null ],
        [ "Health Monitoring", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md555", null ]
      ] ],
      [ "Future Enhancements", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md556", null ],
      [ "Server Protocol", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md557", [
        [ "Client → Server", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md558", null ],
        [ "Server → Client", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md559", null ]
      ] ]
    ] ],
    [ "End-to-End (E2E) Tests", "d9/db0/md_test_2e2e_2README.html", [
      [ "Active Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md561", [
        [ "✅ Working Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md562", null ],
        [ "📝 Dungeon Editor Smoke Test", "d9/db0/md_test_2e2e_2README.html#autotoc_md563", null ]
      ] ],
      [ "Running Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md564", [
        [ "All E2E Tests (GUI Mode)", "d9/db0/md_test_2e2e_2README.html#autotoc_md565", null ],
        [ "Specific Test Category", "d9/db0/md_test_2e2e_2README.html#autotoc_md566", null ],
        [ "Dungeon Editor Test Only", "d9/db0/md_test_2e2e_2README.html#autotoc_md567", null ]
      ] ],
      [ "Test Development", "d9/db0/md_test_2e2e_2README.html#autotoc_md568", [
        [ "Creating New Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md569", null ],
        [ "Register in yaze_test.cc", "d9/db0/md_test_2e2e_2README.html#autotoc_md570", null ],
        [ "ImGui Test Engine API", "d9/db0/md_test_2e2e_2README.html#autotoc_md571", null ]
      ] ],
      [ "Test Logging", "d9/db0/md_test_2e2e_2README.html#autotoc_md572", null ],
      [ "Test Infrastructure", "d9/db0/md_test_2e2e_2README.html#autotoc_md573", [
        [ "File Organization", "d9/db0/md_test_2e2e_2README.html#autotoc_md574", null ],
        [ "Helper Functions", "d9/db0/md_test_2e2e_2README.html#autotoc_md575", null ]
      ] ],
      [ "Future Test Ideas", "d9/db0/md_test_2e2e_2README.html#autotoc_md576", null ],
      [ "Troubleshooting", "d9/db0/md_test_2e2e_2README.html#autotoc_md577", [
        [ "Test Crashes in GUI Mode", "d9/db0/md_test_2e2e_2README.html#autotoc_md578", null ],
        [ "Tests Not Found", "d9/db0/md_test_2e2e_2README.html#autotoc_md579", null ],
        [ "ImGui Items Not Found", "d9/db0/md_test_2e2e_2README.html#autotoc_md580", null ]
      ] ],
      [ "References", "d9/db0/md_test_2e2e_2README.html#autotoc_md581", null ],
      [ "Status", "d9/db0/md_test_2e2e_2README.html#autotoc_md582", null ]
    ] ],
    [ "yaze Test Suite", "d0/d46/md_test_2README.html", [
      [ "Directory Structure", "d0/d46/md_test_2README.html#autotoc_md584", null ],
      [ "Test Categories", "d0/d46/md_test_2README.html#autotoc_md585", [
        [ "Unit Tests (<tt>unit/</tt>)", "d0/d46/md_test_2README.html#autotoc_md586", null ],
        [ "Integration Tests (<tt>integration/</tt>)", "d0/d46/md_test_2README.html#autotoc_md587", null ],
        [ "End-to-End Tests (<tt>e2e/</tt>)", "d0/d46/md_test_2README.html#autotoc_md588", null ]
      ] ],
      [ "Enhanced Test Runner", "d0/d46/md_test_2README.html#autotoc_md589", [
        [ "Usage Examples", "d0/d46/md_test_2README.html#autotoc_md590", null ],
        [ "Test Modes", "d0/d46/md_test_2README.html#autotoc_md591", null ],
        [ "Options", "d0/d46/md_test_2README.html#autotoc_md592", null ]
      ] ],
      [ "E2E ROM Testing", "d0/d46/md_test_2README.html#autotoc_md593", [
        [ "Test Cases", "d0/d46/md_test_2README.html#autotoc_md594", null ]
      ] ],
      [ "ZSCustomOverworld Upgrade Testing", "d0/d46/md_test_2README.html#autotoc_md595", [
        [ "Supported Upgrades", "d0/d46/md_test_2README.html#autotoc_md596", null ],
        [ "Test Cases", "d0/d46/md_test_2README.html#autotoc_md597", null ],
        [ "Version-Specific Features", "d0/d46/md_test_2README.html#autotoc_md598", [
          [ "Vanilla", "d0/d46/md_test_2README.html#autotoc_md599", null ],
          [ "v2", "d0/d46/md_test_2README.html#autotoc_md600", null ],
          [ "v3", "d0/d46/md_test_2README.html#autotoc_md601", null ]
        ] ]
      ] ],
      [ "Environment Variables", "d0/d46/md_test_2README.html#autotoc_md602", null ],
      [ "CI/CD Integration", "d0/d46/md_test_2README.html#autotoc_md603", null ],
      [ "Deprecated Tests", "d0/d46/md_test_2README.html#autotoc_md604", null ],
      [ "Best Practices", "d0/d46/md_test_2README.html#autotoc_md605", null ],
      [ "AI Agent Testing", "d0/d46/md_test_2README.html#autotoc_md606", null ]
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
"d0/d20/classyaze_1_1test_1_1E2ETestSuite.html#a90e47318c00bf5d8114d00a810e5283e",
"d0/d28/classTextEditor.html#a92ffe1172d67e7707b0f844a657126b2",
"d0/d59/structyaze_1_1gui_1_1canvas_1_1PerformanceOptions.html",
"d0/d8d/snes__tile_8h.html#aa5ae49b02472150a2ad972abf1f7d35a",
"d0/dcb/structyaze_1_1cli_1_1overworld_1_1WarpQuery.html#a983a813611496f5a625bc9ea3bf83067",
"d1/d06/z3ed__ascii__logo_8h.html#ab9420b276f246762e5cfbe584b520664",
"d1/d22/classyaze_1_1editor_1_1DungeonObjectInteraction.html#aa4bd0f97bdca33d17ae019a4d9b89b65",
"d1/d3e/namespaceyaze_1_1editor.html#a6d91eaa164b4529e047ae78ef99c47a0",
"d1/d4f/overworld__map_8h.html#a607408467c46951f2ab11ba3c694ca3dabd47c8dc6ac7236c198f4d982dcb5ac8",
"d1/d8a/room__entrance_8h.html#aefe1b66a013692818f9d5666771e41ae",
"d1/db8/structyaze_1_1cli_1_1PolicyEvaluator_1_1PolicyConfig_1_1TestRequirement.html#a0a5cbd4bf66c8c330427497763a9cb6f",
"d1/deb/structyaze_1_1test_1_1TestScriptStep.html#a5e9cf649a19e31b895a8dc8cc8bb0814",
"d2/d07/classyaze_1_1emu_1_1Spc700.html#a59aa2b0c8bf66ead0231103169360a89",
"d2/d20/classyaze_1_1cli_1_1PromptBuilder.html#a22b5e6474ad7b5ba12e8b92cb1c30da1",
"d2/d4e/classyaze_1_1editor_1_1AgentChatHistoryPopup.html#a2337e6dcb2f34c65690ccc9aec229329",
"d2/d67/classyaze_1_1cli_1_1GuiAutomationClient.html",
"d2/dc6/structyaze_1_1emu_1_1BGMODE.html#a81ab8eb7ca69deb21b0eb220a50422c2",
"d2/def/classyaze_1_1test_1_1TestRecorder.html#afe3caaf4cc37be382e16ab756a2905b7",
"d2/dfe/structyaze_1_1gui_1_1EnhancedTheme.html#ae2b6dade47267ac62fa9f34147b9f6dd",
"d3/d19/classyaze_1_1gfx_1_1GraphicsOptimizer.html#a8dd9f26b547578edf49fb6a1e9870a9c",
"d3/d30/structyaze_1_1gui_1_1canvas_1_1CanvasPerformanceMetrics.html#a711ca2566c4490e4e66daeb1735afc0e",
"d3/d44/classyaze_1_1editor_1_1OverworldEditor.html#a92f496396b365517e10fab89f317ea60",
"d3/d4f/classyaze_1_1editor_1_1AgentCollaborationCoordinator.html#aae9ffb472a0cc933da1cf4cabd0d5bee",
"d3/d6c/classyaze_1_1cli_1_1Palette.html",
"d3/d8d/classyaze_1_1editor_1_1PopupManager.html#a50f03fe0d22ea8b6d7b5f9bcd3672a4b",
"d3/d9f/classyaze_1_1editor_1_1Editor.html#a78070dc194ac0104406bc2ff4dd07b1f",
"d3/dbf/namespaceyaze_1_1gui.html#a9f7caa95981f846c2eac84bff36b952d",
"d3/ded/classyaze_1_1emu_1_1Ppu.html#a3beac535c74a280313a2c120d15877c6",
"d4/d03/structyaze_1_1editor_1_1DungeonRenderer_1_1ObjectRenderCache.html",
"d4/d0a/namespaceyaze_1_1test.html#ab99769ca77f8e9c7b9676ab5f276befc",
"d4/d45/classyaze_1_1editor_1_1AssemblyEditor.html#acce893a4b4cb8a9c48da9c85b955997d",
"d4/d79/app_2rom_8cc_source.html",
"d4/db3/prompt__manager_8h.html#a35cbc4e2a28bdee3fd04f391c3ed8001ab8eb7a62834cb1c7fc1b0d02d60bc7fa",
"d4/de1/namespaceyaze_1_1util.html#a31a3bd17d34979da958912e00b13cc56ad4b84d921444ff96b6ed220a036f9a8a",
"d5/d1b/room_8cc.html#a6174a39fc2fcf4733e85cda8eb083953",
"d5/d1f/namespaceyaze_1_1zelda3.html#a693df677b1b2754452c8b8b4c4a98468a2d745a47567cd5d54eb480f3caaca77c",
"d5/d1f/namespaceyaze_1_1zelda3.html#ad1ac0e51c4d6f33af2978a75e3a7e9fe",
"d5/d49/classyaze_1_1app_1_1net_1_1WebSocketClient_1_1Impl.html#a83223c44d558ad42000611cb7deb9778",
"d5/d81/editor__integration__test_8cc.html#adf4d73301961430d50d7edd96f92ce38",
"d5/dae/structyaze_1_1gui_1_1canvas_1_1CanvasRenderContext.html#a6bcfee3483be78f901249198a6a6ce10",
"d5/dd0/classyaze_1_1test_1_1RomDependentTestSuite.html#aea0889ee248d99cc5e8aecd0339d42eb",
"d6/d0d/classyaze_1_1editor_1_1AgentEditor.html#a7d64ebca7ac3b32b4a9e5fae9ff1865d",
"d6/d2e/classyaze_1_1zelda3_1_1TitleScreen.html#a00c11a329ff43505331079b74a68caf0",
"d6/d30/classyaze_1_1Rom.html#abfad5beaef8b6648f43c1ee018d1b6ee",
"d6/d5c/test__manager_8h.html#a0cda2abf7030eef1095ab53508a246ae",
"d6/d9f/structyaze_1_1emu_1_1STAT77.html#a592dbafcedefc7ff72205cf88451c801",
"d6/dc7/snes__palette__test_8cc.html#a1ec8eb5e2d7546d162986e9abb9b8457",
"d6/dea/structyaze_1_1Rom_1_1WriteAction.html#a270f1ebd51b1b8ef1958dfaa61cdbfe0",
"d7/d3f/classyaze_1_1cli_1_1AsarPatch.html#a4c6d927daa9561e112e62faf62063a67",
"d7/d61/classyaze_1_1zelda3_1_1DungeonEditorSystem.html#a3a5a44df3b1b4d6ead99a4b5f1630555a728fbb0413cb98efb85596fdf820132d",
"d7/d61/classyaze_1_1zelda3_1_1DungeonEditorSystem.html#aeada695031bd777562a3eb641434d09f",
"d7/d85/classyaze_1_1zelda3_1_1ComprehensiveIntegrationTest.html#a98100097e136cf44ede952f86f5d8348",
"d7/dc3/classyaze_1_1zelda3_1_1test_1_1RoomManipulationTest.html#a3d8660c93720caa1c56f9c1c86a75b63",
"d7/df0/classyaze_1_1app_1_1net_1_1WebSocketClient.html#aa6e988c6f5b1da1600e9ed40d2732eb1",
"d7/df6/classyaze_1_1zelda3_1_1Room.html#ad20790133f29f02fca9a23511559972a",
"d7/dfd/gfx_8cc.html#aa9c3df97146a4533c1c78f5ac7c9e36e",
"d8/d31/structyaze_1_1emu_1_1INIDISP.html",
"d8/d6e/classyaze_1_1gfx_1_1AtlasRenderer.html#aa44d449ef38235d5a7ac783c0f30e477",
"d8/d98/classyaze_1_1editor_1_1DungeonEditor.html#acdfc87bf1c3409ea52ee0e42ca23c64b",
"d8/dc3/classyaze_1_1zelda3_1_1DungeonObjectRendererIntegrationTest.html#a15c65fc2f39ab4924d8788abf62c4082",
"d8/dd6/classyaze_1_1zelda3_1_1music_1_1Tracker.html#afb752a19bf6759d884ad42026de9a552",
"d9/d21/structyaze_1_1emu_1_1DmaRegisters.html#af2051f4d084812bfd15f6e9242d3cacf",
"d9/d37/service__factory_8h.html",
"d9/d7f/classyaze_1_1gui_1_1PaletteWidget.html#aa82e1e47e4b7790d0d18d4e10c6bc627",
"d9/db0/md_test_2e2e_2README.html#autotoc_md581",
"d9/dc5/classyaze_1_1zelda3_1_1Overworld.html#a3a15f92a3a718c15b9be75ae134d9b94",
"d9/dcc/classyaze_1_1zelda3_1_1OverworldMap.html#a6bc074620ea2f0bf94d4513fba334626",
"d9/dd2/classyaze_1_1cli_1_1agent_1_1VimMode.html#af42556a6beafccbd4a5acf2f80cc366c",
"d9/dff/ppu__registers_8h.html#ae8a33722877db70d25b7f90ebb7181c1",
"da/d2c/classyaze_1_1gui_1_1Canvas.html#a80eb92149acc255c04a0b508dccf695f",
"da/d3e/classyaze_1_1test_1_1TestManager.html#a03de7389e6b12f25c0dc8eb098eac0a4",
"da/d51/test__editor_8h.html",
"da/d7f/structyaze_1_1gui_1_1CanvasConfig.html#a6ed1f3c116086b9afdc91b3e905683cb",
"da/dbd/classyaze_1_1gui_1_1ScopedCanvas.html#a0b0f71755ce4acadbcb04e7b4d7272ca",
"da/ded/namespaceyaze_1_1cli_1_1anonymous__namespace_02resource__catalog_8cc_03.html#af3ff64439f68346dc8d90ed83ffcb328",
"db/d2d/classyaze_1_1cli_1_1GeminiAIService.html#a8fabee613135908e8ef20253f8261658",
"db/d62/classyaze_1_1app_1_1net_1_1ProposalApprovalManager.html#a0713c41f2f350753fd8ea7f4571e7c6c",
"db/d82/classyaze_1_1editor_1_1Tile16Editor.html#a68b09cff964bc0b565a592f45a1f8159",
"db/d9c/message__data_8h.html#a7da8c45c704303a76331eaade35bc5da",
"db/dcc/classyaze_1_1editor_1_1ScreenEditor.html#a25e99f672b2648b9e5953431c8bbfc76",
"db/dfa/structyaze_1_1editor_1_1OverworldEditor_1_1ScratchSpaceSlot.html#a958f5d8d28bf5316c2080e53d6578b29",
"dc/d1d/classyaze_1_1cli_1_1OllamaAIService.html",
"dc/d31/classyaze_1_1editor_1_1GraphicsEditor.html#ae8336c5018893ccb7fba18d2ac97e20f",
"dc/d4f/classyaze_1_1editor_1_1DungeonObjectSelector.html#afdc4ff26bba49d224a2ab714d2f25417",
"dc/daa/classyaze_1_1editor_1_1WelcomeScreen.html#a0945d0faf5c1eb3d9cd749d96576d3bd",
"dc/deb/structyaze_1_1editor_1_1AgentChatWidget_1_1Z3EDCommandState.html",
"dc/df4/classyaze_1_1emu_1_1Cpu.html#a68274679132af197ed103a61bf1e3fa7",
"dc/df4/classyaze_1_1emu_1_1Cpu.html#aea4f8753924c8d547cf3963ca42305bb",
"dd/d12/classyaze_1_1editor_1_1EditorManager.html#a6dc2ae273f19652bf1f85f7b3da695c8",
"dd/d26/structyaze_1_1cli_1_1overworld_1_1MapSummary.html#ab385a931301b08f9bd139cd5afb8ba5b",
"dd/d59/classyaze_1_1zelda3_1_1DungeonEditorSystemIntegrationTest.html#aac5299ac99d70d0708516631eb18ebff",
"dd/d7f/widget__auto__register_8h.html#a40f845a0bcc86ef1432569400f378f58",
"dd/dcc/classyaze_1_1editor_1_1ProposalDrawer.html#ab811cae5845c007ae1fc6faf533e0e3b",
"dd/dee/structyaze_1_1editor_1_1CommandEntry.html#a2b73df9cc1290cb964c639d26a015108",
"de/d0f/classyaze_1_1emu_1_1MemoryImpl.html#a86ab3bf451d7c462e104e246bd99c7c8",
"de/d71/classyaze_1_1cli_1_1ResourceContextBuilder.html",
"de/d8d/tile16__editor_8h.html#ad1dc8b6a79c23b4ab819d66313a332d2",
"de/dbf/icons_8h.html#a068077d71999f84a659bc5ce02751fd1",
"de/dbf/icons_8h.html#a25b5800345d13008e13c60ca2a9aeb7e",
"de/dbf/icons_8h.html#a42b43eafdc8bb75cb638bfe3d45b10d3",
"de/dbf/icons_8h.html#a5ffcf6b8893dcd3479d51c9884fb835b",
"de/dbf/icons_8h.html#a7fb62a23b773d7fb97f09de8c1d53b67",
"de/dbf/icons_8h.html#a9ef2d8b1999c0f21928a6c2cc4c93eee",
"de/dbf/icons_8h.html#abbc927ed60077c1edc1a7d493d08e724",
"de/dbf/icons_8h.html#ad7c8978019af8e55eb1a534d07353c62",
"de/dbf/icons_8h.html#af24b1df13f51fbb9f527396611d4681a",
"de/dda/structyaze_1_1zelda3_1_1ObjectRenderer_1_1TileRenderInfo.html#a288bfde83c60c6debfd62d53dc130025",
"de/ded/input_8h.html#a28671c6edc49db8152f84414daa5ad05",
"df/d22/structyaze_1_1util_1_1SDL__Surface__Deleter.html",
"df/d45/test__common_8cc.html",
"df/db9/classyaze_1_1cli_1_1ModernCLI.html#a23e969cba7a3322f507369cdd146aa32",
"df/ded/terminal__colors_8h.html#ad2a330fa4d3345606886a98a7e4aa016",
"functions_vars_r.html"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';