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
"d0/d52/classyaze_1_1zelda3_1_1RoomLayoutObject.html#ad1f980a79a8b25336f7ba4636b8ad79c",
"d0/d85/structyaze_1_1cli_1_1WidgetDescriptor.html#a8d38ac53fc1f3b7856583fa1218a55dd",
"d0/dc2/canvas__automation__service_8cc_source.html",
"d0/df3/structyaze_1_1gui_1_1FlagsMenu.html#ab98e89ff2b817bab22fc2f216b9983f1",
"d1/d1f/overworld__entrance_8h.html#a26e5a7b9e91ebbb569929295138dbc33",
"d1/d3b/structyaze_1_1cli_1_1DiscoverWidgetsQuery.html",
"d1/d4b/namespaceyaze_1_1gfx_1_1lc__lz2.html#a0882444317fe9f49790acb8ad38c7a77",
"d1/d72/cli_2tui_2command__palette_8cc.html",
"d1/da6/classyaze_1_1app_1_1net_1_1CollaborationService.html#a6c92b43386f7c1350dac2287120e66cf",
"d1/dda/menu__builder_8cc_source.html",
"d1/df6/test__suite__reporter_8cc_source.html",
"d2/d07/classyaze_1_1emu_1_1Spc700.html#aae94f629150a4ae22311377eeb036fb2",
"d2/d36/font__loader_8h_source.html",
"d2/d4e/classyaze_1_1editor_1_1AgentChatHistoryPopup.html#af6957cf5b5c0741e55cda69616f9d2e2a1a821ac3683242e6ca14989ce03d2601",
"d2/d94/group__rom__types.html#gaf2a830623580e2f70ad81d0f53b3339e",
"d2/de7/classyaze_1_1editor_1_1DungeonCanvasViewer.html#a06825291ef50ea1b1a21890f6376c1f4",
"d2/dfe/structyaze_1_1gui_1_1EnhancedTheme.html#a187d5d2f5e9e8ae07a2b0fe17cf86087",
"d3/d15/classyaze_1_1emu_1_1Snes.html#a003b4e192be32e666674acee717ac3be",
"d3/d1e/structyaze_1_1zelda3_1_1LayerMergeType.html#af82b727b5503ea104c90d28db2b9f65d",
"d3/d44/classyaze_1_1editor_1_1OverworldEditor.html#a0630e0b14adfe3dc400bfce64bd3eebc",
"d3/d44/classyaze_1_1editor_1_1OverworldEditor.html#adac2da0e5bfc87f31ae50e5c4c3edd2e",
"d3/d60/classyaze_1_1zelda3_1_1SpriteInstruction.html#ac14981bf7b1288751377cf81ed1d8a28",
"d3/d77/structyaze_1_1emu_1_1M7D.html",
"d3/d93/structyaze_1_1gfx_1_1CgxHeader.html#a26620143e07d6f2f4438437287f7ccfd",
"d3/db7/room__diagnostic_8cc_source.html",
"d3/dc5/dungeon__renderer_8h.html",
"d3/ded/classyaze_1_1emu_1_1Ppu.html#a97079b9ed7736fb3a7ca309e87cc9181",
"d4/d0a/classyaze_1_1gfx_1_1PerformanceProfiler.html#ad59f68893ba7cbf9af495f2c77795015",
"d4/d0a/namespaceyaze_1_1test.html#acef1a330a9e16d0360a7ac4142289edd",
"d4/d4b/classyaze_1_1cli_1_1ProjectBuild.html",
"d4/d7a/message_8cc.html#ad10a3c76cee8c40cee517334f011ebe1",
"d4/da1/classyaze_1_1gui_1_1canvas_1_1CanvasUsageManager.html#a6ad7f84dc329356bb45e1ff7c38e6d4e",
"d4/dd8/namespaceyaze_1_1cli_1_1agent_1_1anonymous__namespace_02conversational__agent__service_8cc_03.html#a419c7f2142bc028b821dbfaa81f06d4c",
"d5/d08/namespaceyaze_1_1gui_1_1CanvasUtils.html#a3179a79f843f6076dbf595c97da7342c",
"d5/d1f/namespaceyaze_1_1zelda3.html#a509ea7e55d813d6e70cca5e3c3d1877a",
"d5/d1f/namespaceyaze_1_1zelda3.html#aadcd3b43be9f68654fff223d71cd417d",
"d5/d46/classyaze_1_1gfx_1_1PoolAllocator.html#a077ad2cc9ce2c15bcdb106ee37dc6b34",
"d5/d6b/structyaze_1_1zelda3_1_1DungeonObjectEditor_1_1EditorConfig.html#a8ee7e8acc7d686d2d9f8928dfb55cc55",
"d5/da0/structyaze_1_1emu_1_1DspChannel.html#abc7bf098c4afbdf9a7ddd68ced64905f",
"d5/dcb/structyaze_1_1cli_1_1ToolSpecification.html#a8875617a93fe5e9e770d1386248a19c6",
"d6/d0d/classyaze_1_1editor_1_1AgentEditor.html#a3d500ebf108f9183f1330311776c5da7",
"d6/d20/namespaceyaze_1_1emu.html#aab4485d53b785a237309663ba9f72437",
"d6/d30/classyaze_1_1Rom.html#a3f76910dd3716dc67f477e7da77cee33",
"d6/d41/classyaze_1_1cli_1_1ai_1_1anonymous__namespace_02ai__gui__controller__test_8cc_03_1_1AIGUIControllerTest.html#aebf252de75d973d9c269d5d48c6d0dc3",
"d6/d74/rom__test_8cc_source.html",
"d6/db1/classyaze_1_1zelda3_1_1Sprite.html#a9a840a3052ff2cc2f851c8a3146feb92",
"d6/dcc/classyaze_1_1gfx_1_1GraphicsOptimizationScope.html#aea65242c83ea0c41d1a7e8255d3ef3c6",
"d6/df8/structyaze_1_1zelda3_1_1RoomSize.html",
"d7/d54/structsnes__tile16.html",
"d7/d61/classyaze_1_1zelda3_1_1DungeonEditorSystem.html#ab375dc81c4bd6eb16df910734023f3c2",
"d7/d73/classyaze_1_1editor_1_1MenuBuilder.html#a2631d349ada5abbe7fa077a8580ebe41",
"d7/d9c/overworld_8h.html#ab6fb240c3b6d1aedc43fd68c8078e773",
"d7/dcc/structyaze_1_1gfx_1_1PaletteGroupMap.html#a83d9f679033a24873a65da44a5ff62a0",
"d7/df6/classyaze_1_1zelda3_1_1Room.html#a17e6f13e72c3840cc30c976ae89ff2f7",
"d7/df7/classyaze_1_1util_1_1IFlag.html",
"d8/d1e/classyaze_1_1zelda3_1_1RoomEntrance.html#a048a3bd84be46a98af158e037036dcfa",
"d8/d4b/classyaze_1_1core_1_1RecentFilesManager.html#af39fda2f363c71393d931e218fe43e9b",
"d8/d83/entity_8h.html#a6b13948b033b60fa0dc966717fc52a82",
"d8/da5/structyaze_1_1gui_1_1canvas_1_1CanvasUsageStats.html#a11a38d36c7aacc786b642a7efd796047",
"d8/dd3/namespaceyaze_1_1cli_1_1agent.html#a6a3d696815c6e8d815849cfb27cc7cc7a30437633d552b7229b65b11ec90cb153",
"d8/ddf/classOverworldGoldenDataExtractor.html#acd4fdbafe0ff5680dce2537ff6c13c3f",
"d9/d2b/classyaze_1_1editor_1_1AgentChatWidget.html#a0de8fdb77895fdc4cfa82e0a3a33778e",
"d9/d41/md_docs_202-build-instructions.html#autotoc_md17",
"d9/d7f/classyaze_1_1gui_1_1PaletteWidget.html#a5bfe580853f7921ae02a7aa06653a89b",
"d9/dad/classyaze_1_1cli_1_1tui_1_1ChatTUI.html#a13ee433594ad645f19feee988b0cb881",
"d9/dc5/classyaze_1_1zelda3_1_1Overworld.html#a33b58f0fcecbcfd47dae3203fcfade96",
"d9/dcc/classyaze_1_1zelda3_1_1OverworldMap.html#a5f801b62529ffcd3a0da9a7cd245e52b",
"d9/dd2/classyaze_1_1cli_1_1agent_1_1VimMode.html#abd2e439973969af9806b91c43570e3d9",
"d9/dff/ppu__registers_8h.html#ab000d06262864a0e9d9781bf2b6ac13e",
"da/d2c/classyaze_1_1gui_1_1Canvas.html#a6e620185806a42d017fec9091565d0d9",
"da/d36/namespaceyaze_1_1core_1_1anonymous__namespace_02asar__wrapper__test_8cc_03.html#a9471ca4ef69dab3bc50c66fbbe79b2fb",
"da/d4e/structyaze_1_1gui_1_1CanvasPaletteManager.html#afb203f31708050bcb1cb11e0dbcd9fbd",
"da/d7a/classyaze_1_1cli_1_1OverworldSetTile.html",
"da/dbc/classyaze_1_1emu_1_1AsmParser.html#a4a893fda99e18179f324b2b0d1641b92",
"da/ded/widget__state__capture_8cc.html#adfa425625ae6d14a5f9da8c00bd7fcaf",
"db/d2d/classyaze_1_1cli_1_1GeminiAIService.html",
"db/d56/classyaze_1_1gui_1_1BppConversionDialog.html#ab200902860df8cbd65e4b39e4b5c1169",
"db/d80/structyaze_1_1cli_1_1agent_1_1ConversationalAgentService_1_1InternalMetrics.html#a66c15fd9baca52896c067ab1ea2e5cbf",
"db/d8e/zelda3_2dungeon_2room__object__encoding__test_8cc.html#a68416c714a819cd0865ae7a4f127f476",
"db/dbd/classyaze_1_1cli_1_1agent_1_1SimpleChatSession.html#aa248c3485aafbc7cb86a30d9e77e4f29",
"db/de0/structyaze_1_1cli_1_1FewShotExample.html",
"dc/d0c/structyaze_1_1cli_1_1GeminiConfig.html#a1f628bd4ba748480667786f0849fc31f",
"dc/d31/classyaze_1_1editor_1_1GraphicsEditor.html#a5954a4be7fefc8ba1b9fcac31ce98f9b",
"dc/d48/audio_2internal_2instructions_8cc_source.html",
"dc/d69/structyaze_1_1editor_1_1EditorContext.html#a68ea226ad4983687bb3cd4dee9a84021",
"dc/dae/classyaze_1_1test_1_1ZSCustomOverworldTestSuite.html#ad9019c30fddd180ab0766ca589753a50",
"dc/df4/classyaze_1_1emu_1_1Cpu.html#a20c9dcfe9db17b5514f4ab0caee49ed9",
"dc/df4/classyaze_1_1emu_1_1Cpu.html#ab45992e2aa377adb6355573852200e1a",
"dd/d10/structyaze_1_1cli_1_1overworld_1_1TileMatch.html#a60354f31c74d93bb18423aaf70c34611",
"dd/d13/classyaze_1_1test_1_1EditorIntegrationTest.html#a14909b85f39e40bd7bfb442b97ec8eab",
"dd/d44/structyaze_1_1gui_1_1GfxSheetAssetBrowser.html#a5155af2c0c1393af51be84ce99d46fff",
"dd/d63/namespaceyaze_1_1cli.html#ab9420b276f246762e5cfbe584b520664",
"dd/dac/classyaze_1_1gfx_1_1BackgroundBuffer.html#a234fd2a0401268d73c0d35c1111ab3b1",
"dd/ddf/classyaze_1_1gui_1_1canvas_1_1CanvasContextMenu.html#a728e925cc68d6c08332b2f55f4146715a35c3ace1970663a16e5c65baa5941b13",
"dd/df4/structyaze_1_1emu_1_1EmulatorKeybindings.html#a2bdd4d8bc84e492f6ea69eb6f2ac561d",
"de/d0f/classyaze_1_1emu_1_1MemoryImpl.html#ae286a6c904922ce9f44c7996a51e71b3",
"de/d76/classyaze_1_1editor_1_1SpriteEditor.html#a552785a0287ebe61e7f855e45c8d6777",
"de/da1/classyaze_1_1cli_1_1TestWorkflowGenerator.html#a2eb622983ab4c1ff94d555f1772f7dfb",
"de/dbf/icons_8h.html#a0e6f94b09ca8d35631ebc29e2239b77b",
"de/dbf/icons_8h.html#a2ca91a1169341c1e07f19cad044b59db",
"de/dbf/icons_8h.html#a4a0730b7321e2c8bc64dffd43bf2b869",
"de/dbf/icons_8h.html#a664bbc51b561d6021e4c608a4a29ff84",
"de/dbf/icons_8h.html#a85d3dfae4e5716de1a8baac34c19443e",
"de/dbf/icons_8h.html#aa51a314e683afe58dbe748b639622632",
"de/dbf/icons_8h.html#ac1e9fc1eb683ed00ba09e0feb6d2e705",
"de/dbf/icons_8h.html#add8481013b16e6ca1e4d61bbd90cdbec",
"de/dbf/icons_8h.html#af860463fa05b0d9dfb6c6eb7f12eb602",
"de/de7/classyaze_1_1zelda3_1_1DungeonObjectEditor.html#a20014e5d7be2567663fef4383bc3ac70",
"df/d02/dungeon__object__rendering__e2e__tests_8cc.html#add44829dd80690a18db170ee822d25f4",
"df/d33/scad__format_8h.html#a464001df38cc9e68bd80e777cf4ccf91",
"df/d71/structsnes__color.html#abe07ebebd2f735f932b7a7a065506f9f",
"df/db9/classyaze_1_1cli_1_1ModernCLI.html#a8e849465c04fc1de5b5a76e13141b965",
"df/df5/classyaze_1_1test_1_1MockRom.html#a25d2fc1232977befeec5a215384749e6",
"globals_defs_f.html"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';