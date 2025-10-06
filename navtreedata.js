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
        [ "Widget Registration for Automation", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md73", null ],
        [ "Writing E2E Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md74", null ],
        [ "Running GUI Tests with <tt>z3ed</tt>", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md75", null ],
        [ "Widget Discovery", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md76", null ],
        [ "Integration with AI Agent", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md77", null ],
        [ "Visual Testing", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md78", null ],
        [ "Performance", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md79", null ]
      ] ]
    ] ],
    [ "z3ed: AI-Powered CLI for YAZE", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html", [
      [ "1. Overview", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md81", [
        [ "Core Capabilities", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md82", null ]
      ] ],
      [ "2. Quick Start", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md83", [
        [ "Build", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md84", null ],
        [ "AI Setup", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md85", null ],
        [ "Example Commands", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md86", null ],
        [ "Hybrid CLI ↔ GUI Workflow", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md87", null ]
      ] ],
      [ "3. Architecture", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md88", [
        [ "System Components Diagram", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md89", null ]
      ] ],
      [ "4. Agentic & Generative Workflow (MCP)", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md90", null ],
      [ "5. Command Reference", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md91", [
        [ "Agent Commands", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md92", null ],
        [ "Resource Commands", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md93", [
          [ "<tt>agent test</tt>: Live Harness Automation", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md94", null ]
        ] ]
      ] ],
      [ "6. Chat Modes", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md95", [
        [ "FTXUI Chat (<tt>agent chat</tt>)", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md96", null ],
        [ "Simple Chat (<tt>agent simple-chat</tt>)", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md97", null ],
        [ "GUI Chat Widget (Editor Integration)", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md98", null ]
      ] ],
      [ "7. AI Provider Configuration", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md99", [
        [ "System Prompt Versions", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md100", null ]
      ] ],
      [ "8. Learn Command - Knowledge Management", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md101", [
        [ "Basic Usage", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md102", null ],
        [ "Project Context", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md103", null ],
        [ "Conversation Memory", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md104", null ],
        [ "Storage Location", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md105", null ]
      ] ],
      [ "9. TODO Management System", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md106", [
        [ "Core Capabilities", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md107", null ],
        [ "Storage Location", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md108", null ]
      ] ],
      [ "10. CLI Output & Help System", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md109", [
        [ "Verbose Logging", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md110", null ],
        [ "Hierarchical Help System", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md111", null ]
      ] ],
      [ "10. Collaborative Sessions & Multimodal Vision", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md112", [
        [ "Overview", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md113", null ],
        [ "Local Collaboration Mode", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md115", [
          [ "How to Use", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md116", null ],
          [ "Features", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md117", null ],
          [ "Cloud Folder Workaround", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md118", null ]
        ] ],
        [ "Network Collaboration Mode (yaze-server v2.0)", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md120", [
          [ "Requirements", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md121", null ],
          [ "Server Setup", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md122", null ],
          [ "Client Connection", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md123", null ],
          [ "Core Features", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md124", null ],
          [ "Advanced Features (v2.0)", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md125", null ],
          [ "Protocol Reference", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md126", null ],
          [ "Server Configuration", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md127", null ],
          [ "Database Schema (Server v2.0)", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md128", null ],
          [ "Deployment", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md129", null ],
          [ "Testing", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md130", null ],
          [ "Security Considerations", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md131", null ]
        ] ],
        [ "Multimodal Vision (Gemini)", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md133", [
          [ "Requirements", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md134", null ],
          [ "Capture Modes", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md135", null ],
          [ "How to Use", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md136", null ],
          [ "Example Prompts", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md137", null ]
        ] ],
        [ "Architecture", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md139", null ],
        [ "Troubleshooting", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md141", null ],
        [ "References", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md143", null ]
      ] ],
      [ "11. Roadmap & Implementation Status", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md144", [
        [ "✅ Completed", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md145", null ],
        [ "📌 Current Progress Highlights (October 5, 2025)", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md146", null ],
        [ "🚧 Active & Next Steps", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md147", null ],
        [ "✅ Recently Completed (v0.2.0-alpha - October 5, 2025)", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md148", [
          [ "Core AI Features", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md149", null ],
          [ "Version Management & Protection", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md150", null ],
          [ "Networking & Collaboration (NEW)", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md151", null ],
          [ "UI/UX Enhancements", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md152", null ],
          [ "Build System & Infrastructure", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md153", null ]
        ] ]
      ] ],
      [ "12. Troubleshooting", "d0/dc8/md_docs_2A2-z3ed-agent-readme.html#autotoc_md154", null ]
    ] ],
    [ "Platform Compatibility Improvements", "d1/d30/md_docs_2B2-platform-compatibility.html", [
      [ "Native File Dialog Support", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md156", null ],
      [ "Cross-Platform Build Reliability", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md157", null ],
      [ "Build Configuration Options", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md158", [
        [ "Full Build (Development)", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md159", null ],
        [ "Minimal Build", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md160", null ]
      ] ],
      [ "Implementation Details", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md161", null ],
      [ "Platform-Specific Adaptations", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md162", [
        [ "Windows", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md163", null ],
        [ "macOS", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md164", null ],
        [ "Linux", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md165", null ]
      ] ]
    ] ],
    [ "Build Presets Guide", "d7/d51/md_docs_2B3-build-presets.html", [
      [ "Design Principles", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md167", null ],
      [ "Quick Start", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md168", [
        [ "macOS Development", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md169", null ],
        [ "Windows Development", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md170", null ]
      ] ],
      [ "All Presets", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md171", [
        [ "macOS Presets", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md172", null ],
        [ "Windows Presets", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md173", null ],
        [ "Linux Presets", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md174", null ],
        [ "Special Purpose", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md175", null ]
      ] ],
      [ "Warning Control", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md176", [
        [ "To Enable Verbose Warnings:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md177", null ]
      ] ],
      [ "Architecture Support", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md178", [
        [ "macOS", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md179", null ],
        [ "Windows", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md180", null ]
      ] ],
      [ "Build Directories", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md181", null ],
      [ "Feature Flags", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md182", null ],
      [ "Examples", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md183", [
        [ "Development with AI features and verbose warnings", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md184", null ],
        [ "Release build for distribution (macOS Universal)", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md185", null ],
        [ "Quick minimal build for testing", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md186", null ]
      ] ],
      [ "Updating compile_commands.json", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md187", null ],
      [ "Migration from Old Presets", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md188", null ],
      [ "Troubleshooting", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md189", [
        [ "Warnings are still showing", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md190", null ],
        [ "clangd can't find nlohmann/json", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md191", null ],
        [ "Build fails with missing dependencies", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md192", null ]
      ] ]
    ] ],
    [ "Release Workflows Documentation", "d3/d49/md_docs_2B4-release-workflows.html", [
      [ "Overview", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md194", null ],
      [ "1. Release-Simplified (<tt>release-simplified.yml</tt>)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md196", [
        [ "Purpose", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md197", null ],
        [ "Key Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md198", null ],
        [ "Use Cases", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md199", null ],
        [ "Configuration", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md200", null ],
        [ "Platforms Supported", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md201", null ]
      ] ],
      [ "2. Release (<tt>release.yml</tt>)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md203", [
        [ "Purpose", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md204", null ],
        [ "Key Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md205", null ],
        [ "Use Cases", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md206", null ],
        [ "Configuration", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md207", null ],
        [ "Advantages over Simplified", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md208", null ]
      ] ],
      [ "3. Release-Complex (<tt>release-complex.yml</tt>)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md210", [
        [ "Purpose", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md211", null ],
        [ "Key Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md212", null ],
        [ "Use Cases", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md213", null ],
        [ "Fallback Mechanisms", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md214", [
          [ "vcpkg Fallback", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md215", null ],
          [ "Chocolatey Integration", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md216", null ],
          [ "Build Configuration Fallback", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md217", null ]
        ] ],
        [ "Advanced Features", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md218", null ]
      ] ],
      [ "Workflow Comparison Matrix", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md220", null ],
      [ "When to Use Each Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md222", [
        [ "Use Simplified When:", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md223", null ],
        [ "Use Release When:", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md224", null ],
        [ "Use Complex When:", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md225", null ]
      ] ],
      [ "Workflow Selection Guide", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md227", [
        [ "For Development Team", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md228", null ],
        [ "For Release Manager", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md229", null ],
        [ "For CI/CD Pipeline", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md230", null ]
      ] ],
      [ "Configuration Examples", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md232", [
        [ "Triggering a Release", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md233", [
          [ "Manual Release (All Workflows)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md234", null ],
          [ "Automatic Release (Tag Push)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md235", null ]
        ] ],
        [ "Customizing Release Notes", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md236", null ]
      ] ],
      [ "Troubleshooting", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md238", [
        [ "Common Issues", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md239", [
          [ "vcpkg Failures (Windows)", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md240", null ],
          [ "Dependency Conflicts", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md241", null ],
          [ "Build Failures", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md242", null ]
        ] ],
        [ "Debug Information", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md243", [
          [ "Simplified Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md244", null ],
          [ "Release Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md245", null ],
          [ "Complex Workflow", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md246", null ]
        ] ]
      ] ],
      [ "Best Practices", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md248", [
        [ "Workflow Selection", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md249", null ],
        [ "Release Process", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md250", null ],
        [ "Maintenance", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md251", null ]
      ] ],
      [ "Future Improvements", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md253", [
        [ "Planned Enhancements", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md254", null ],
        [ "Integration Opportunities", "d3/d49/md_docs_2B4-release-workflows.html#autotoc_md255", null ]
      ] ]
    ] ],
    [ "Changelog", "d7/d6f/md_docs_2C1-changelog.html", [
      [ "0.3.3 (October 2025)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md258", [
        [ "GUI & UX Modernization", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md259", null ],
        [ "Overworld Editor Refactoring", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md260", null ],
        [ "Build System & Stability", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md261", null ]
      ] ],
      [ "0.3.2 (December 2025) - In Development", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md262", [
        [ "Tile16 Editor Improvements (In Progress)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md263", null ],
        [ "Graphics System Optimizations", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md264", null ],
        [ "Windows Platform Stability", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md265", null ],
        [ "Memory Safety & Stability", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md266", null ],
        [ "Testing & CI/CD Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md267", null ],
        [ "Future Optimizations (Planned)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md268", null ],
        [ "Technical Notes", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md269", null ]
      ] ],
      [ "0.3.1 (September 2025)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md270", [
        [ "Major Features", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md271", null ],
        [ "Tile16 Editor Enhancements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md272", null ],
        [ "ZSCustomOverworld v3 Implementation", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md273", null ],
        [ "Technical Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md274", null ],
        [ "User Interface", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md275", null ],
        [ "Bug Fixes", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md276", null ],
        [ "ZScream Compatibility Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md277", null ]
      ] ],
      [ "0.3.0 (September 2025)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md278", [
        [ "Major Features", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md279", null ],
        [ "User Interface & Theming", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md280", null ],
        [ "Enhancements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md281", null ],
        [ "Technical Improvements", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md282", null ],
        [ "Bug Fixes", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md283", null ]
      ] ],
      [ "0.2.2 (December 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md284", null ],
      [ "0.2.1 (August 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md285", null ],
      [ "0.2.0 (July 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md286", null ],
      [ "0.1.0 (May 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md287", null ],
      [ "0.0.9 (April 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md288", null ],
      [ "0.0.8 (February 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md289", null ],
      [ "0.0.7 (January 2024)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md290", null ],
      [ "0.0.6 (November 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md291", null ],
      [ "0.0.5 (November 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md292", null ],
      [ "0.0.4 (November 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md293", null ],
      [ "0.0.3 (October 2023)", "d7/d6f/md_docs_2C1-changelog.html#autotoc_md294", null ]
    ] ],
    [ "Roadmap", "d6/db4/md_docs_2D1-roadmap.html", [
      [ "Current Focus: AI & Editor Polish", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md296", null ],
      [ "0.4.X (Next Major Release) - Advanced Tooling & UX", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md298", [
        [ "Priority 1: Editor Features & UX", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md299", null ],
        [ "Priority 2: <tt>z3ed</tt> AI Agent", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md300", null ],
        [ "Priority 3: Testing & Stability", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md301", null ]
      ] ],
      [ "0.5.X - Feature Expansion", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md303", null ],
      [ "0.6.X - Content & Integration", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md305", null ],
      [ "Recently Completed (v0.3.2)", "d6/db4/md_docs_2D1-roadmap.html#autotoc_md307", null ]
    ] ],
    [ "Yaze Dungeon Editor: Master Guide", "d5/d47/md_docs_2D2-dungeon-editor-guide.html", [
      [ "1. Current Status: Production Ready", "d5/d47/md_docs_2D2-dungeon-editor-guide.html#autotoc_md309", [
        [ "Known Issues & Next Steps", "d5/d47/md_docs_2D2-dungeon-editor-guide.html#autotoc_md310", null ]
      ] ],
      [ "2. Architecture: A Component-Based Design", "d5/d47/md_docs_2D2-dungeon-editor-guide.html#autotoc_md311", [
        [ "Core Components", "d5/d47/md_docs_2D2-dungeon-editor-guide.html#autotoc_md312", null ],
        [ "Data Flow", "d5/d47/md_docs_2D2-dungeon-editor-guide.html#autotoc_md313", null ]
      ] ],
      [ "3. Key Recent Fixes", "d5/d47/md_docs_2D2-dungeon-editor-guide.html#autotoc_md314", [
        [ "Critical Fix 1: The Coordinate System", "d5/d47/md_docs_2D2-dungeon-editor-guide.html#autotoc_md315", null ],
        [ "Critical Fix 2: The Test Suite", "d5/d47/md_docs_2D2-dungeon-editor-guide.html#autotoc_md316", null ]
      ] ],
      [ "4. ROM Internals & Data Structures", "d5/d47/md_docs_2D2-dungeon-editor-guide.html#autotoc_md317", [
        [ "Object Encoding", "d5/d47/md_docs_2D2-dungeon-editor-guide.html#autotoc_md318", null ],
        [ "Core Data Tables in ROM", "d5/d47/md_docs_2D2-dungeon-editor-guide.html#autotoc_md319", null ]
      ] ],
      [ "5. Testing: 100% Pass Rate", "d5/d47/md_docs_2D2-dungeon-editor-guide.html#autotoc_md320", [
        [ "How to Run Tests", "d5/d47/md_docs_2D2-dungeon-editor-guide.html#autotoc_md321", null ]
      ] ],
      [ "6. Dungeon Object Reference Tables", "d5/d47/md_docs_2D2-dungeon-editor-guide.html#autotoc_md322", [
        [ "Type 1 Object Reference Table", "d5/d47/md_docs_2D2-dungeon-editor-guide.html#autotoc_md323", null ],
        [ "Type 2 Object Reference Table", "d5/d47/md_docs_2D2-dungeon-editor-guide.html#autotoc_md324", null ],
        [ "Type 3 Object Reference Table", "d5/d47/md_docs_2D2-dungeon-editor-guide.html#autotoc_md325", null ]
      ] ]
    ] ],
    [ "Asm Style Guide", "d7/d9a/md_docs_2E1-asm-style-guide.html", [
      [ "Table of Contents", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md327", null ],
      [ "File Structure", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md328", null ],
      [ "Labels and Symbols", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md329", null ],
      [ "Comments", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md330", null ],
      [ "Instructions", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md331", null ],
      [ "Macros", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md332", null ],
      [ "Loops and Branching", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md333", null ],
      [ "Data Structures", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md334", null ],
      [ "Code Organization", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md335", null ],
      [ "Custom Code", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md336", null ]
    ] ],
    [ "Tile16 Editor Palette System", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html", [
      [ "Executive Summary", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md338", null ],
      [ "Problem Analysis", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md339", [
        [ "Critical Issues Identified", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md340", null ]
      ] ],
      [ "Solution Architecture", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md341", [
        [ "Core Design Principles", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md342", null ],
        [ "256-Color Overworld Palette Structure", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md343", null ],
        [ "Sheet-to-Palette Mapping", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md344", null ],
        [ "Palette Button Mapping", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md345", null ]
      ] ],
      [ "Implementation Details", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md346", [
        [ "1. Fixed SetPaletteWithTransparent Method", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md347", null ],
        [ "2. Corrected Tile16 Editor Palette System", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md348", null ],
        [ "3. Palette Coordination Flow", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md349", null ]
      ] ],
      [ "UI/UX Refactoring", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md350", [
        [ "New Three-Column Layout", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md351", null ],
        [ "Canvas Context Menu Fixes", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md352", null ],
        [ "Dynamic Zoom Controls", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md353", null ]
      ] ],
      [ "Testing Protocol", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md354", [
        [ "Crash Prevention Testing", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md355", null ],
        [ "Color Alignment Testing", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md356", null ],
        [ "UI/UX Testing", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md357", null ]
      ] ],
      [ "Error Handling", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md358", [
        [ "Bounds Checking", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md359", null ],
        [ "Fallback Mechanisms", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md360", null ]
      ] ],
      [ "Debug Information Display", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md361", null ],
      [ "Known Issues and Ongoing Work", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md362", [
        [ "Completed Items ✅", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md363", null ],
        [ "Active Issues ⚠️", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md364", null ],
        [ "Current Status Summary", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md365", null ],
        [ "Future Enhancements", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md366", null ]
      ] ],
      [ "Maintenance Notes", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md367", null ],
      [ "Next Steps", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md369", [
        [ "Immediate Priorities", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md370", null ],
        [ "Investigation Areas", "d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md371", null ]
      ] ]
    ] ],
    [ "Overworld Loading Guide", "dc/d12/md_docs_2F1-overworld-loading.html", [
      [ "Table of Contents", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md374", null ],
      [ "Overview", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md375", null ],
      [ "ROM Types and Versions", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md376", [
        [ "Version Detection", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md377", null ],
        [ "Feature Support by Version", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md378", null ]
      ] ],
      [ "Overworld Map Structure", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md379", [
        [ "Core Properties", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md380", null ]
      ] ],
      [ "Overlays and Special Area Maps", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md381", [
        [ "Understanding Overlays", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md382", null ],
        [ "Special Area Maps (0x80-0x9F)", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md383", null ],
        [ "Overlay ID Mappings", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md384", null ],
        [ "Drawing Order", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md385", null ],
        [ "Vanilla Overlay Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md386", null ],
        [ "Special Area Graphics Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md387", null ]
      ] ],
      [ "Loading Process", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md388", [
        [ "1. Version Detection", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md389", null ],
        [ "2. Map Initialization", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md390", null ],
        [ "3. Property Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md391", [
          [ "Vanilla ROMs (asm_version == 0xFF)", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md392", null ],
          [ "ZSCustomOverworld v2/v3", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md393", null ]
        ] ],
        [ "4. Custom Data Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md394", null ]
      ] ],
      [ "ZScream Implementation", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md395", [
        [ "OverworldMap Constructor", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md396", null ],
        [ "Key Methods", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md397", null ]
      ] ],
      [ "Yaze Implementation", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md398", [
        [ "OverworldMap Constructor", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md399", null ],
        [ "Key Methods", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md400", null ],
        [ "Current Status", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md401", null ]
      ] ],
      [ "Key Differences", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md402", [
        [ "1. Language and Architecture", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md403", null ],
        [ "2. Data Structures", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md404", null ],
        [ "3. Error Handling", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md405", null ],
        [ "4. Graphics Processing", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md406", null ]
      ] ],
      [ "Common Issues and Solutions", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md407", [
        [ "1. Version Detection Issues", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md408", null ],
        [ "2. Palette Loading Errors", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md409", null ],
        [ "3. Graphics Not Loading", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md410", null ],
        [ "4. Overlay Issues", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md411", null ],
        [ "5. Large Map Problems", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md412", null ],
        [ "6. Special Area Graphics Issues", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md413", null ]
      ] ],
      [ "Best Practices", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md414", [
        [ "1. Version-Specific Code", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md415", null ],
        [ "2. Error Handling", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md416", null ],
        [ "3. Memory Management", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md417", null ],
        [ "4. Thread Safety", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md418", null ]
      ] ],
      [ "Conclusion", "dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md419", null ]
    ] ],
    [ "Canvas System", "d0/d7d/md_docs_2G2-canvas-guide.html", [
      [ "Overview", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md421", null ],
      [ "Core Concepts", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md422", [
        [ "Canvas Structure", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md423", null ],
        [ "Coordinate Systems", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md424", null ]
      ] ],
      [ "Usage Patterns", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md425", [
        [ "Pattern 1: Basic Bitmap Display", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md426", null ],
        [ "Pattern 2: Modern Begin/End", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md427", null ],
        [ "Pattern 3: RAII ScopedCanvas", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md428", null ]
      ] ],
      [ "Feature: Tile Painting", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md429", [
        [ "Single Tile Painting", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md430", null ],
        [ "Tilemap Painting", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md431", null ],
        [ "Color Painting", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md432", null ]
      ] ],
      [ "Feature: Tile Selection", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md433", [
        [ "Single Tile Selection", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md434", null ],
        [ "Multi-Tile Rectangle Selection", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md435", null ],
        [ "Rectangle Drag & Paint", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md436", null ]
      ] ],
      [ "Feature: Custom Overlays", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md437", [
        [ "Manual Points Manipulation", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md438", null ]
      ] ],
      [ "Feature: Large Map Support", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md439", [
        [ "Map Types", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md440", null ],
        [ "Boundary Clamping", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md441", null ],
        [ "Custom Map Sizes", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md442", null ]
      ] ],
      [ "Feature: Context Menu", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md443", [
        [ "Adding Custom Items", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md444", null ],
        [ "Overworld Editor Example", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md445", null ]
      ] ],
      [ "Feature: Scratch Space (In Progress)", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md446", null ],
      [ "Common Workflows", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md447", [
        [ "Workflow 1: Overworld Tile Painting", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md448", null ],
        [ "Workflow 2: Tile16 Blockset Selection", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md449", null ],
        [ "Workflow 3: Graphics Sheet Display", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md450", null ]
      ] ],
      [ "Configuration", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md451", [
        [ "Grid Settings", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md452", null ],
        [ "Scale Settings", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md453", null ],
        [ "Interaction Settings", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md454", null ],
        [ "Large Map Settings", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md455", null ]
      ] ],
      [ "Known Issues", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md456", [
        [ "⚠️ Rectangle Selection Wrapping", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md457", null ]
      ] ],
      [ "API Reference", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md458", [
        [ "Drawing Methods", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md459", null ],
        [ "State Accessors", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md460", null ],
        [ "Configuration", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md461", null ]
      ] ],
      [ "Implementation Notes", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md462", [
        [ "Points Management", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md463", null ],
        [ "Selection State", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md464", null ],
        [ "Overworld Rectangle Painting Flow", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md465", null ]
      ] ],
      [ "Best Practices", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md466", [
        [ "DO ✅", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md467", null ],
        [ "DON'T ❌", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md468", null ]
      ] ],
      [ "Troubleshooting", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md469", [
        [ "Issue: Rectangle wraps at boundaries", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md470", null ],
        [ "Issue: Painting in wrong location", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md471", null ],
        [ "Issue: Array index out of bounds", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md472", null ],
        [ "Issue: Forgot to call End()", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md473", null ]
      ] ],
      [ "Future: Scratch Space", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md474", null ],
      [ "Summary", "d0/d7d/md_docs_2G2-canvas-guide.html#autotoc_md475", null ]
    ] ],
    [ "yaze Documentation", "d3/d4c/md_docs_2index.html", [
      [ "Core Guides", "d3/d4c/md_docs_2index.html#autotoc_md477", null ],
      [ "Development & API", "d3/d4c/md_docs_2index.html#autotoc_md478", null ],
      [ "Technical Documentation", "d3/d4c/md_docs_2index.html#autotoc_md479", null ]
    ] ],
    [ "Networking and Collaboration", "dd/d91/md_docs_2networking.html", [
      [ "1. Overview", "dd/d91/md_docs_2networking.html#autotoc_md482", null ],
      [ "2. Architecture", "dd/d91/md_docs_2networking.html#autotoc_md483", [
        [ "2.1. System Diagram", "dd/d91/md_docs_2networking.html#autotoc_md484", null ],
        [ "2.2. Core Components", "dd/d91/md_docs_2networking.html#autotoc_md485", null ]
      ] ],
      [ "3. Protocols", "dd/d91/md_docs_2networking.html#autotoc_md486", [
        [ "3.1. WebSocket Protocol (yaze-server v2.0)", "dd/d91/md_docs_2networking.html#autotoc_md487", null ],
        [ "3.2. gRPC Service (Remote ROM Manipulation)", "dd/d91/md_docs_2networking.html#autotoc_md488", null ]
      ] ],
      [ "4. Client Integration", "dd/d91/md_docs_2networking.html#autotoc_md489", [
        [ "4.1. YAZE App Integration", "dd/d91/md_docs_2networking.html#autotoc_md490", null ],
        [ "4.2. z3ed CLI Integration", "dd/d91/md_docs_2networking.html#autotoc_md491", null ]
      ] ],
      [ "5. Best Practices & Troubleshooting", "dd/d91/md_docs_2networking.html#autotoc_md492", [
        [ "Best Practices", "dd/d91/md_docs_2networking.html#autotoc_md493", null ],
        [ "Troubleshooting", "dd/d91/md_docs_2networking.html#autotoc_md494", null ]
      ] ],
      [ "6. Security Considerations", "dd/d91/md_docs_2networking.html#autotoc_md495", null ],
      [ "7. Future Enhancements", "dd/d91/md_docs_2networking.html#autotoc_md496", null ]
    ] ],
    [ "yaze - Yet Another Zelda3 Editor", "d0/d30/md_README.html", [
      [ "Version 0.3.2 - Release", "d0/d30/md_README.html#autotoc_md498", null ],
      [ "Quick Start", "d0/d30/md_README.html#autotoc_md503", [
        [ "Build", "d0/d30/md_README.html#autotoc_md504", null ],
        [ "Applications", "d0/d30/md_README.html#autotoc_md505", null ]
      ] ],
      [ "Usage", "d0/d30/md_README.html#autotoc_md506", [
        [ "GUI Editor", "d0/d30/md_README.html#autotoc_md507", null ],
        [ "Command Line Tool", "d0/d30/md_README.html#autotoc_md508", null ],
        [ "C++ API", "d0/d30/md_README.html#autotoc_md509", null ]
      ] ],
      [ "Documentation", "d0/d30/md_README.html#autotoc_md510", null ],
      [ "Supported Platforms", "d0/d30/md_README.html#autotoc_md511", null ],
      [ "ROM Compatibility", "d0/d30/md_README.html#autotoc_md512", null ],
      [ "Contributing", "d0/d30/md_README.html#autotoc_md513", null ],
      [ "License", "d0/d30/md_README.html#autotoc_md514", null ],
      [ "🙏 Acknowledgments", "d0/d30/md_README.html#autotoc_md515", null ],
      [ "📸 Screenshots", "d0/d30/md_README.html#autotoc_md516", null ]
    ] ],
    [ "yaze Build Scripts", "de/d82/md_scripts_2README.html", [
      [ "Windows Scripts", "de/d82/md_scripts_2README.html#autotoc_md519", [
        [ "vcpkg Setup (Optional)", "de/d82/md_scripts_2README.html#autotoc_md520", null ]
      ] ],
      [ "Windows Build Workflow", "de/d82/md_scripts_2README.html#autotoc_md521", [
        [ "Recommended: Visual Studio CMake Mode", "de/d82/md_scripts_2README.html#autotoc_md522", null ],
        [ "Command Line Build", "de/d82/md_scripts_2README.html#autotoc_md523", null ],
        [ "Compiler Notes", "de/d82/md_scripts_2README.html#autotoc_md524", null ]
      ] ],
      [ "Quick Start (Windows)", "de/d82/md_scripts_2README.html#autotoc_md525", [
        [ "Option 1: Visual Studio (Recommended)", "de/d82/md_scripts_2README.html#autotoc_md526", null ],
        [ "Option 2: Command Line", "de/d82/md_scripts_2README.html#autotoc_md527", null ],
        [ "Option 3: With vcpkg (Optional)", "de/d82/md_scripts_2README.html#autotoc_md528", null ]
      ] ],
      [ "Troubleshooting", "de/d82/md_scripts_2README.html#autotoc_md529", [
        [ "Common Issues", "de/d82/md_scripts_2README.html#autotoc_md530", null ],
        [ "Getting Help", "de/d82/md_scripts_2README.html#autotoc_md531", null ]
      ] ],
      [ "Other Scripts", "de/d82/md_scripts_2README.html#autotoc_md532", null ],
      [ "Build Environment Verification", "de/d82/md_scripts_2README.html#autotoc_md533", [
        [ "<tt>verify-build-environment.ps1</tt> / <tt>.sh</tt>", "de/d82/md_scripts_2README.html#autotoc_md534", null ],
        [ "Usage", "de/d82/md_scripts_2README.html#autotoc_md535", null ]
      ] ]
    ] ],
    [ "Agent Editor Module", "d6/df7/md_src_2app_2editor_2agent_2README.html", [
      [ "Overview", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md537", null ],
      [ "Architecture", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md538", [
        [ "Core Components", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md539", [
          [ "AgentEditor (<tt>agent_editor.h/cc</tt>)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md540", null ],
          [ "AgentChatWidget (<tt>agent_chat_widget.h/cc</tt>)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md541", null ],
          [ "AgentChatHistoryCodec (<tt>agent_chat_history_codec.h/cc</tt>)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md542", null ]
        ] ],
        [ "Collaboration Coordinators", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md543", [
          [ "AgentCollaborationCoordinator (<tt>agent_collaboration_coordinator.h/cc</tt>)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md544", null ],
          [ "NetworkCollaborationCoordinator (<tt>network_collaboration_coordinator.h/cc</tt>)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md545", null ]
        ] ]
      ] ],
      [ "Usage", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md546", [
        [ "Initialization", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md547", null ],
        [ "Drawing", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md548", null ],
        [ "Session Management", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md549", null ],
        [ "Network Mode (requires YAZE_WITH_GRPC and YAZE_WITH_JSON)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md550", null ]
      ] ],
      [ "File Structure", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md551", null ],
      [ "Build Configuration", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md552", [
        [ "Required", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md553", null ],
        [ "Optional", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md554", null ]
      ] ],
      [ "Data Files", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md555", [
        [ "Local Storage", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md556", null ],
        [ "Session File Format", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md557", null ]
      ] ],
      [ "Integration with EditorManager", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md558", null ],
      [ "Dependencies", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md559", [
        [ "Internal", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md560", null ],
        [ "External (when enabled)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md561", null ]
      ] ],
      [ "Advanced Features (v2.0)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md562", [
        [ "ROM Synchronization", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md563", null ],
        [ "Multimodal Snapshot Sharing", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md564", null ],
        [ "Proposal Management", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md565", null ],
        [ "AI Agent Integration", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md566", null ],
        [ "Health Monitoring", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md567", null ]
      ] ],
      [ "Future Enhancements", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md568", null ],
      [ "Server Protocol", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md569", [
        [ "Client → Server", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md570", null ],
        [ "Server → Client", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md571", null ]
      ] ]
    ] ],
    [ "End-to-End (E2E) Tests", "d9/db0/md_test_2e2e_2README.html", [
      [ "Active Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md573", [
        [ "✅ Working Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md574", null ],
        [ "📝 Dungeon Editor Smoke Test", "d9/db0/md_test_2e2e_2README.html#autotoc_md575", null ]
      ] ],
      [ "Running Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md576", [
        [ "All E2E Tests (GUI Mode)", "d9/db0/md_test_2e2e_2README.html#autotoc_md577", null ],
        [ "Specific Test Category", "d9/db0/md_test_2e2e_2README.html#autotoc_md578", null ],
        [ "Dungeon Editor Test Only", "d9/db0/md_test_2e2e_2README.html#autotoc_md579", null ]
      ] ],
      [ "Test Development", "d9/db0/md_test_2e2e_2README.html#autotoc_md580", [
        [ "Creating New Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md581", null ],
        [ "Register in yaze_test.cc", "d9/db0/md_test_2e2e_2README.html#autotoc_md582", null ],
        [ "ImGui Test Engine API", "d9/db0/md_test_2e2e_2README.html#autotoc_md583", null ]
      ] ],
      [ "Test Logging", "d9/db0/md_test_2e2e_2README.html#autotoc_md584", null ],
      [ "Test Infrastructure", "d9/db0/md_test_2e2e_2README.html#autotoc_md585", [
        [ "File Organization", "d9/db0/md_test_2e2e_2README.html#autotoc_md586", null ],
        [ "Helper Functions", "d9/db0/md_test_2e2e_2README.html#autotoc_md587", null ]
      ] ],
      [ "Future Test Ideas", "d9/db0/md_test_2e2e_2README.html#autotoc_md588", null ],
      [ "Troubleshooting", "d9/db0/md_test_2e2e_2README.html#autotoc_md589", [
        [ "Test Crashes in GUI Mode", "d9/db0/md_test_2e2e_2README.html#autotoc_md590", null ],
        [ "Tests Not Found", "d9/db0/md_test_2e2e_2README.html#autotoc_md591", null ],
        [ "ImGui Items Not Found", "d9/db0/md_test_2e2e_2README.html#autotoc_md592", null ]
      ] ],
      [ "References", "d9/db0/md_test_2e2e_2README.html#autotoc_md593", null ],
      [ "Status", "d9/db0/md_test_2e2e_2README.html#autotoc_md594", null ]
    ] ],
    [ "yaze Test Suite", "d0/d46/md_test_2README.html", [
      [ "Directory Structure", "d0/d46/md_test_2README.html#autotoc_md596", null ],
      [ "Test Categories", "d0/d46/md_test_2README.html#autotoc_md597", [
        [ "Unit Tests (<tt>unit/</tt>)", "d0/d46/md_test_2README.html#autotoc_md598", null ],
        [ "Integration Tests (<tt>integration/</tt>)", "d0/d46/md_test_2README.html#autotoc_md599", null ],
        [ "End-to-End Tests (<tt>e2e/</tt>)", "d0/d46/md_test_2README.html#autotoc_md600", null ]
      ] ],
      [ "Enhanced Test Runner", "d0/d46/md_test_2README.html#autotoc_md601", [
        [ "Usage Examples", "d0/d46/md_test_2README.html#autotoc_md602", null ],
        [ "Test Modes", "d0/d46/md_test_2README.html#autotoc_md603", null ],
        [ "Options", "d0/d46/md_test_2README.html#autotoc_md604", null ]
      ] ],
      [ "E2E ROM Testing", "d0/d46/md_test_2README.html#autotoc_md605", [
        [ "Test Cases", "d0/d46/md_test_2README.html#autotoc_md606", null ]
      ] ],
      [ "ZSCustomOverworld Upgrade Testing", "d0/d46/md_test_2README.html#autotoc_md607", [
        [ "Supported Upgrades", "d0/d46/md_test_2README.html#autotoc_md608", null ],
        [ "Test Cases", "d0/d46/md_test_2README.html#autotoc_md609", null ],
        [ "Version-Specific Features", "d0/d46/md_test_2README.html#autotoc_md610", [
          [ "Vanilla", "d0/d46/md_test_2README.html#autotoc_md611", null ],
          [ "v2", "d0/d46/md_test_2README.html#autotoc_md612", null ],
          [ "v3", "d0/d46/md_test_2README.html#autotoc_md613", null ]
        ] ]
      ] ],
      [ "Environment Variables", "d0/d46/md_test_2README.html#autotoc_md614", null ],
      [ "CI/CD Integration", "d0/d46/md_test_2README.html#autotoc_md615", null ],
      [ "Deprecated Tests", "d0/d46/md_test_2README.html#autotoc_md616", null ],
      [ "Best Practices", "d0/d46/md_test_2README.html#autotoc_md617", null ],
      [ "AI Agent Testing", "d0/d46/md_test_2README.html#autotoc_md618", null ]
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
"d0/dfd/overworld__inspect_8cc.html#a574fec16a6b0e5343423968948610c0b",
"d1/d1f/overworld__entrance_8h.html#a88804f6687b1c9636b20c37c5ead16e6",
"d1/d3e/namespaceyaze_1_1editor.html#a00c4d23b26706fde3a72cdc112aef45b",
"d1/d4b/namespaceyaze_1_1gfx_1_1lc__lz2.html#a32ff40d7c0cee688d9eb2d808ef4cb98",
"d1/d74/structyaze_1_1emu_1_1BGHOFS.html#aae4e7796795ffa0e774a95a2fddeef3b",
"d1/da6/classyaze_1_1app_1_1net_1_1CollaborationService.html#a78545e1c1829f70b882669855251c396",
"d1/ddb/structyaze_1_1editor_1_1MenuBuilder_1_1MenuItem.html#ab8b06b982854fe52fec397bbdf933a71",
"d1/df7/structyaze_1_1cli_1_1TestCaseDefinition.html#ab1ca695ce3af1080a0a78a1f5cb78fe2",
"d2/d07/classyaze_1_1emu_1_1Spc700.html#ac33c01c05dce9ebfbb121c3540a63c47",
"d2/d39/classyaze_1_1gui_1_1MultiSelect.html#a2d5be49cc17281465b2be2e8992ba4ea",
"d2/d4f/structyaze_1_1emu_1_1OPHCT.html#a2296dac17215cb3aaaa3993570ef4e28",
"d2/d99/structyaze_1_1emu_1_1BGSC.html",
"d2/de7/classyaze_1_1editor_1_1DungeonCanvasViewer.html#a1ba8c2d5ba0bcdf4929e82b3a02eea3e",
"d2/dfe/structyaze_1_1gui_1_1EnhancedTheme.html#a2127c593307e7fd003733daa993fd5a5",
"d3/d15/classyaze_1_1emu_1_1Snes.html#a1a64bd261939eac11ca3a014a4d4fdac",
"d3/d21/classyaze_1_1editor_1_1EditorSet.html#a52760cfa4105c4e46b39ea8c6b966963",
"d3/d44/classyaze_1_1editor_1_1OverworldEditor.html#a0e6c6c8c1610af66417ef0b447f54d7c",
"d3/d44/classyaze_1_1editor_1_1OverworldEditor.html#ae333ba5d7c41f14ad9f56e8414c5369f",
"d3/d68/structyaze_1_1editor_1_1WorkspaceManager_1_1SessionInfo.html#ab9de1cb8cb58dad2b870ef9fefaf26bf",
"d3/d81/group__dungeon.html#ga5244f869702daf5c1972d1c45f5abd67",
"d3/d93/structyaze_1_1gfx_1_1CgxHeader.html#af7f39d215fb5283bbd1b7ff126c9823f",
"d3/db7/structyaze_1_1gui_1_1WidgetIdRegistry_1_1WidgetInfo.html#a32dfc62bc9d1c7ba505fa2d54559f402",
"d3/dd5/vim__mode_8cc.html#ab807de69ecb0d8a74c214b5e38000dec",
"d3/ded/classyaze_1_1emu_1_1Ppu.html#aa1d3ea33ab636e8211f3ed62d7c0e0fe",
"d4/d0a/gui_2widgets_2agent__chat__widget_8cc_source.html",
"d4/d0a/namespaceyaze_1_1test.html#aed20ffc478f883290e78a5723a931cfa",
"d4/d57/classyaze_1_1zelda3_1_1SpriteBuilder.html#a07b8381001dd642a653b928aef343c8a",
"d4/d84/classyaze_1_1core_1_1Controller.html#a024bd35620ff4d5e2fd5dd899e23fc07",
"d4/db3/prompt__manager_8h.html#a35cbc4e2a28bdee3fd04f391c3ed8001ac7cf00cf740d75270a88d45e3177b98f",
"d4/de1/namespaceyaze_1_1util.html#a31a3bd17d34979da958912e00b13cc56ae3f917d82e495f1c002357925ca3047e",
"d5/d1b/apu_8h.html",
"d5/d1f/namespaceyaze_1_1zelda3.html#a683777016b10ae5998e7c5f9fe946769",
"d5/d1f/namespaceyaze_1_1zelda3.html#acbd549e1691fc9f4bcef8d4b2970507a",
"d5/d48/cli__main_8cc.html#ac59920fe19f2557d694ac0c6fff95805",
"d5/d7c/structyaze_1_1app_1_1net_1_1RomVersionManager_1_1Config.html#a1f2bd4965242a8456ee1bcec82dd659f",
"d5/da6/performance__profiler_8cc_source.html",
"d5/dd0/classyaze_1_1test_1_1RomDependentTestSuite.html#aa4de34407432fad0b96fc0c8e9788388",
"d6/d0d/classyaze_1_1editor_1_1AgentEditor.html#a8fab2a6cd2364f06aebc69fff04bc35b",
"d6/d2d/build__cleaner_8py.html#aa89e8313cdf3f2872598f0d933b8bb43",
"d6/d30/classyaze_1_1Rom.html#aaf2600f3334bb925086268e63e225418",
"d6/d4f/classyaze_1_1test_1_1DungeonRoomTest.html#abe74b5287d1ccb15edf619f9a4b347f4",
"d6/d92/structyaze_1_1test_1_1TestConfig.html#a0b51ac9ed8aa9cf2e1ef8b12bb9576cf",
"d6/db4/md_docs_2D1-roadmap.html#autotoc_md303",
"d6/dd9/classyaze_1_1app_1_1net_1_1RomVersionManager.html#aa01f8e3fb25dff1abec77852c0e791b2",
"d7/d0d/md_docs_2E7-tile16-editor-palette-system.html#autotoc_md351",
"d7/d61/classyaze_1_1zelda3_1_1DungeonEditorSystem.html#a0e8b04d2928009a813ccd116617d45fb",
"d7/d61/classyaze_1_1zelda3_1_1DungeonEditorSystem.html#ac1fe990c241228ce7133b04919c6d9b2",
"d7/d7f/log_8h.html#ac889c033203bc2d55163d23e252285b3a551b723eafd6a31d444fcb2f5920fbd3",
"d7/da7/classyaze_1_1emu_1_1Apu.html#a4f15d1b9a01829048e315b7bd7b9725c",
"d7/dd7/classyaze_1_1gui_1_1TileSelectorWidget.html#a1dc7fe04d74b4aac1528e98b73108850",
"d7/df6/classyaze_1_1zelda3_1_1Room.html#a532f36c7182551a95510f7afd792eff4",
"d7/dfa/classyaze_1_1zelda3_1_1ObjectRenderer.html#a9908b52a00f2f133a5d97e68baa46d10",
"d8/d1e/classyaze_1_1zelda3_1_1RoomEntrance.html#ae486a80384b93539389cea9e6b7d1aa3",
"d8/d4d/structyaze_1_1core_1_1ResourceLabelManager.html#ac89d382d952d2e24140ddff197a617d9",
"d8/d98/classyaze_1_1editor_1_1DungeonEditor.html#a25fa3fb007e6c33e57a0a360376ee0f7",
"d8/db1/structyaze_1_1cli_1_1agent_1_1AgentConfig.html#a2ddddff3909c153398d76144320e79c6",
"d8/dd5/classyaze_1_1gui_1_1WidgetIdScope.html#a02ede42fdb452647f45a88b328d104f8",
"d8/ddf/classyaze_1_1editor_1_1ProjectFileEditor.html#afb0116cc3fd3dbc12af513d5c52238c1",
"d9/d2b/classyaze_1_1editor_1_1AgentChatWidget.html#a38e577491bfdbe631d728e5a8899469b",
"d9/d46/structyaze_1_1zelda3_1_1music_1_1SpcCommand.html#ac68a57b9e3b1359935c1cc6bf92d513b",
"d9/d8d/general__commands_8cc.html#a2ff357fa1f94a07051d15276d26264b3",
"d9/dc0/room_8h.html#a62286f1cb2eebc9d5834551ead453371",
"d9/dc5/classyaze_1_1zelda3_1_1Overworld.html#aa6422613ca34b9a9b29a2feafc8b2033",
"d9/dcc/classyaze_1_1zelda3_1_1OverworldMap.html#aa61e7e7116b2d790e886768b65ea9414",
"d9/dee/structyaze_1_1app_1_1net_1_1VersionDiff.html#a154c20a9a757134c9479184bb371c328",
"da/d26/structyaze_1_1emu_1_1BGNBA.html#a271beccb69eb3ee9352b8842df924d9f",
"da/d2c/classyaze_1_1gui_1_1Canvas.html#a9dd7730360615a9764c20ec5437fa232",
"da/d3e/classyaze_1_1test_1_1TestManager.html#a78007abafb53216ed047a6f721b006cd",
"da/d5e/structyaze_1_1core_1_1YazeProject.html#a32d0e30cdd0df9a279316f5031172d50",
"da/d84/classyaze_1_1core_1_1anonymous__namespace_02asar__wrapper__test_8cc_03_1_1AsarWrapperTest.html#a6711875bab327cc373e0ecacb506f4bf",
"da/dcb/bitmap_8h.html#ad59175cdad18379972f52c0aafa32d08",
"db/d00/classyaze_1_1editor_1_1DungeonToolset.html#a6d271b966c263928162467c4741ea47d",
"db/d39/structyaze_1_1app_1_1gui_1_1SnapshotEntry.html#a5b345b7450ec53a56201236cdce6156d",
"db/d62/classyaze_1_1app_1_1net_1_1ProposalApprovalManager.html#a131f81b7213f42039e49d9a3029d3d32",
"db/d82/classyaze_1_1editor_1_1Tile16Editor.html#a573dbc7f1a3ec659352e7eaf15502cf4",
"db/d9a/classyaze_1_1editor_1_1MusicEditor.html#ae1d595782ad417ddd2dc03abac8fb2fd",
"db/dcc/classyaze_1_1editor_1_1ScreenEditor.html#a020825a81755d371ca1664192c29286a",
"db/de8/structyaze_1_1Transaction_1_1Operation.html",
"dc/d12/md_docs_2F1-overworld-loading.html#autotoc_md409",
"dc/d31/classyaze_1_1editor_1_1GraphicsEditor.html#abecc99781b8ac20e8d8aa04ed833a6e7",
"dc/d4f/classyaze_1_1editor_1_1DungeonObjectSelector.html#aa4d999f1deb328480e9a762a45955713",
"dc/da3/structyaze_1_1cli_1_1agent_1_1AdvancedRouter_1_1RouteContext.html#a6ac1370ec0686db3220bcb1024d515f3",
"dc/de0/classyaze_1_1test_1_1ArenaTestSuite.html#a4369c935e5b96dda483dd0c7fe2efb6e",
"dc/df4/classyaze_1_1emu_1_1Cpu.html#a5671d42ce5614150e078202dbbd5ad45",
"dc/df4/classyaze_1_1emu_1_1Cpu.html#adde9d945ee33e7eefa0e0953b0d9562b",
"dd/d12/classyaze_1_1editor_1_1EditorManager.html#a5a195f2663d455707edd59a1fda60fa4",
"dd/d26/structyaze_1_1cli_1_1overworld_1_1MapSummary.html#a1f0e26506b7463ef13525c08a31441ed",
"dd/d52/structyaze_1_1app_1_1net_1_1RomVersionManager_1_1Stats.html#aab77e8a862e9038c5ab3c744030691b6",
"dd/d71/classyaze_1_1gui_1_1canvas_1_1CanvasInteractionHandler.html#af718341d9448d51ccd8e78a65e44db46",
"dd/dcc/classyaze_1_1editor_1_1ProposalDrawer.html#a10c37259d6177a6997b34828c7fd68c4",
"dd/ddf/classyaze_1_1gui_1_1canvas_1_1CanvasContextMenu.html#afbe1d2e2ebdd21eac7adeb018bd04d51",
"de/d0d/structzelda3__version__pointers.html",
"de/d3d/yaze__test_8cc.html#a48974bc37811e72ae810e23d0f38b817",
"de/d7d/toast__manager_8h.html#a6b8cab865089eface70ba97013bc8bc7a176a473e63c17ccdac91640c67f149bf",
"de/dab/tui_8h.html#a556e883eb9174f820cdce1c4e8ac7395aee7a7f2434d5efb1cf13f722f9b19cde",
"de/dbf/icons_8h.html#a1ac5296f45e09a210a993bd48692b59d",
"de/dbf/icons_8h.html#a39df1f7e01692cd3c90691c3e83aa6e9",
"de/dbf/icons_8h.html#a5451a4bd4b8cfacd178de759bb4a2c2e",
"de/dbf/icons_8h.html#a722e5cecee310c1e1d5d64cf6814b5e1",
"de/dbf/icons_8h.html#a921392274d64cea276c00e817c3b79a2",
"de/dbf/icons_8h.html#aaf67d104fc88c3b3b65614794871d3c4",
"de/dbf/icons_8h.html#acbf77dab0bb7e4f66b57dec21eb5f5e5",
"de/dbf/icons_8h.html#ae7b66309729b0f2b2448a7dcc562a80d",
"de/dd1/dungeon__integration__test_8cc.html#ae5315c9c221d6f951660fad9faefa87a",
"de/de7/classyaze_1_1zelda3_1_1DungeonObjectEditor.html#a8b449a11bd225d0ba823743124904b6ca644102c8c1020755f0f2e6b07adec5d9",
"df/d0e/classyaze_1_1gfx_1_1Bitmap.html#a9cfe67b27a2d9c6600697d48ba262735",
"df/d41/structyaze_1_1editor_1_1zsprite_1_1ZSprite.html#acf492ea2e2e894173ea2098ab8d4bb74",
"df/d9d/classyaze_1_1cli_1_1gui_1_1GuiActionGenerator.html#a2c48cf0ac524e3945edc8138fdcdc665",
"df/dcb/classyaze_1_1cli_1_1ai_1_1VisionActionRefiner.html",
"dir_6550b3ac391899c7ebf05357b8388b6b.html",
"namespacemembers_vars_r.html"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';