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
    [ "Getting Started", "d7/dee/md_docs_2A1-getting-started.html", [
      [ "Quick Start", "d7/dee/md_docs_2A1-getting-started.html#autotoc_md1", null ],
      [ "General Tips", "d7/dee/md_docs_2A1-getting-started.html#autotoc_md2", null ],
      [ "Feature Status", "d7/dee/md_docs_2A1-getting-started.html#autotoc_md3", null ],
      [ "Command-Line Interface (<tt>z3ed</tt>)", "d7/dee/md_docs_2A1-getting-started.html#autotoc_md4", [
        [ "AI Agent Chat", "d7/dee/md_docs_2A1-getting-started.html#autotoc_md5", null ],
        [ "Resource Inspection", "d7/dee/md_docs_2A1-getting-started.html#autotoc_md6", null ],
        [ "Patching", "d7/dee/md_docs_2A1-getting-started.html#autotoc_md7", null ]
      ] ],
      [ "Extending Functionality", "d7/dee/md_docs_2A1-getting-started.html#autotoc_md8", null ]
    ] ],
    [ "A1 - Testing Guide", "d6/d10/md_docs_2A1-testing-guide.html", [
      [ "1. Test Organization", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md10", null ],
      [ "2. Test Categories", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md11", [
        [ "Unit Tests (<tt>unit/</tt>)", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md12", null ],
        [ "Integration Tests (<tt>integration/</tt>)", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md13", null ],
        [ "End-to-End (E2E) Tests (<tt>e2e/</tt>)", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md14", null ],
        [ "Benchmarks (<tt>benchmarks/</tt>)", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md15", null ]
      ] ],
      [ "3. Running Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md16", [
        [ "Using the Enhanced Test Runner (<tt>yaze_test</tt>)", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md17", null ],
        [ "Using CTest and CMake Presets", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md18", null ]
      ] ],
      [ "4. Writing Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md19", null ],
      [ "5. E2E GUI Testing Framework", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md20", [
        [ "Architecture", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md21", null ],
        [ "Running GUI Tests", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md22", null ],
        [ "Widget Discovery and AI Integration", "d6/d10/md_docs_2A1-testing-guide.html#autotoc_md23", null ]
      ] ]
    ] ],
    [ "APU Timing and Handshake Bug Analysis & Refactoring Plan", "d4/db6/md_docs_2APU__Timing__Fix__Plan.html", [
      [ "1. Problem Statement", "d4/db6/md_docs_2APU__Timing__Fix__Plan.html#autotoc_md25", null ],
      [ "2. Analysis of the CPU-APU Handshake", "d4/db6/md_docs_2APU__Timing__Fix__Plan.html#autotoc_md26", [
        [ "2.1. The Conversation Protocol", "d4/db6/md_docs_2APU__Timing__Fix__Plan.html#autotoc_md27", null ],
        [ "2.2. Point of Failure", "d4/db6/md_docs_2APU__Timing__Fix__Plan.html#autotoc_md28", null ]
      ] ],
      [ "3. Root Cause: Cycle Inaccuracy in SPC700 Emulation", "d4/db6/md_docs_2APU__Timing__Fix__Plan.html#autotoc_md29", [
        [ "3.1. Incomplete Opcode Timing", "d4/db6/md_docs_2APU__Timing__Fix__Plan.html#autotoc_md30", null ],
        [ "3.2. Fragile Multi-Step Execution Model", "d4/db6/md_docs_2APU__Timing__Fix__Plan.html#autotoc_md31", null ],
        [ "3.3. Floating-Point Precision", "d4/db6/md_docs_2APU__Timing__Fix__Plan.html#autotoc_md32", null ]
      ] ],
      [ "4. Proposed Refactoring Plan", "d4/db6/md_docs_2APU__Timing__Fix__Plan.html#autotoc_md33", [
        [ "Step 1: Implement Cycle-Accurate Instruction Execution", "d4/db6/md_docs_2APU__Timing__Fix__Plan.html#autotoc_md34", null ],
        [ "Step 2: Centralize the APU Execution Loop", "d4/db6/md_docs_2APU__Timing__Fix__Plan.html#autotoc_md35", null ],
        [ "Step 3: Use Integer-Based Cycle Ratios", "d4/db6/md_docs_2APU__Timing__Fix__Plan.html#autotoc_md36", null ]
      ] ],
      [ "5. Conclusion", "d4/db6/md_docs_2APU__Timing__Fix__Plan.html#autotoc_md37", null ]
    ] ],
    [ "Build Instructions", "dc/d42/md_docs_2B1-build-instructions.html", [
      [ "1. Environment Verification", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md39", [
        [ "Windows (PowerShell)", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md40", null ],
        [ "macOS & Linux (Bash)", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md41", null ]
      ] ],
      [ "2. Quick Start: Building with Presets", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md42", [
        [ "macOS", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md43", null ],
        [ "Linux", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md44", null ],
        [ "Windows", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md45", null ],
        [ "AI-Enabled Build (All Platforms)", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md46", null ]
      ] ],
      [ "3. Dependencies", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md47", null ],
      [ "4. Platform Setup", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md48", [
        [ "macOS", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md49", null ],
        [ "Linux (Ubuntu/Debian)", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md50", null ],
        [ "Windows", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md51", null ]
      ] ],
      [ "5. Testing", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md52", [
        [ "Running Tests with Presets", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md53", null ],
        [ "Running Tests Manually", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md54", null ]
      ] ],
      [ "6. IDE Integration", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md55", [
        [ "VS Code (Recommended)", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md56", null ],
        [ "Visual Studio (Windows)", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md57", null ],
        [ "Xcode (macOS)", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md58", null ]
      ] ],
      [ "7. Windows Build Optimization", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md59", [
        [ "gRPC v1.67.1 and MSVC Compatibility", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md60", null ],
        [ "The Problem: Slow gRPC Builds", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md61", null ],
        [ "Solution A: Use vcpkg for Pre-compiled Binaries (Recommended - FAST)", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md62", null ],
        [ "Solution B: FetchContent Build (Slow but Automatic)", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md63", null ]
      ] ],
      [ "8. Troubleshooting", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md64", [
        [ "Automatic Fixes", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md65", null ],
        [ "Cleaning Stale Builds", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md66", null ],
        [ "Common Issues", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md67", [
          [ "\"nlohmann/json.hpp: No such file or directory\"", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md68", null ],
          [ "\"Cannot open file 'yaze.exe': Permission denied\"", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md69", null ],
          [ "\"C++ standard 'cxx_std_23' not supported\"", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md70", null ],
          [ "Visual Studio Can't Find Presets", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md71", null ],
          [ "Git Line Ending (CRLF) Issues", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md72", null ],
          [ "File Path Length Limit on Windows", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md73", null ]
        ] ]
      ] ]
    ] ],
    [ "Platform Compatibility & CI/CD Fixes", "d1/d30/md_docs_2B2-platform-compatibility.html", [
      [ "Platform-Specific Notes", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md76", [
        [ "Windows", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md77", null ],
        [ "macOS", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md78", null ],
        [ "Linux", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md79", null ]
      ] ],
      [ "Cross-Platform Code Validation", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md81", [
        [ "Audio System", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md82", null ],
        [ "Input System", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md83", null ],
        [ "Debugger System", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md84", null ],
        [ "UI Layer", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md85", null ]
      ] ],
      [ "Common Build Issues & Solutions", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md87", [
        [ "Windows: \"use of undefined type 'PromiseLike'\"", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md88", null ],
        [ "macOS: \"absl not found\"", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md89", null ],
        [ "Linux: CMake configuration fails", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md90", null ],
        [ "Windows: \"DWORD syntax error\"", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md91", null ]
      ] ],
      [ "CI/CD Validation Checklist", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md93", null ],
      [ "Testing Strategy", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md95", [
        [ "Automated (CI)", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md96", null ],
        [ "Manual Testing", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md97", null ]
      ] ],
      [ "Quick Reference", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md99", [
        [ "Build Command (All Platforms)", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md100", null ],
        [ "Enable Features", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md101", null ],
        [ "Windows Troubleshooting", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md102", null ]
      ] ],
      [ "Filesystem Abstraction", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md104", null ],
      [ "Native File Dialog Support", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md106", null ]
    ] ],
    [ "Build Presets Guide", "d7/d51/md_docs_2B3-build-presets.html", [
      [ "Design Principles", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md109", null ],
      [ "Quick Start", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md110", [
        [ "macOS Development", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md111", null ],
        [ "Windows Development", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md112", null ]
      ] ],
      [ "All Presets", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md113", [
        [ "macOS Presets", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md114", null ],
        [ "Windows Presets", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md115", null ],
        [ "Linux Presets", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md116", null ],
        [ "Special Purpose", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md117", null ]
      ] ],
      [ "Warning Control", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md118", [
        [ "To Enable Verbose Warnings:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md119", null ]
      ] ],
      [ "Architecture Support", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md120", [
        [ "macOS", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md121", null ],
        [ "Windows", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md122", null ]
      ] ],
      [ "Build Directories", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md123", null ],
      [ "Feature Flags", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md124", null ],
      [ "Examples", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md125", [
        [ "Development with AI features and verbose warnings", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md126", null ],
        [ "Release build for distribution (macOS Universal)", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md127", null ],
        [ "Quick minimal build for testing", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md128", null ]
      ] ],
      [ "Updating compile_commands.json", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md129", null ],
      [ "Migration from Old Presets", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md130", null ],
      [ "Troubleshooting", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md131", [
        [ "Warnings are still showing", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md132", null ],
        [ "clangd can't find nlohmann/json", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md133", null ],
        [ "Build fails with missing dependencies", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md134", null ]
      ] ]
    ] ],
    [ "B5 - Architecture and Networking", "dd/de3/md_docs_2B5-architecture-and-networking.html", [
      [ "1. High-Level Architecture", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md136", null ],
      [ "2. Service Taxonomy", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md137", [
        [ "APP Services (gRPC Servers)", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md138", null ],
        [ "CLI Services (Business Logic)", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md139", null ]
      ] ],
      [ "3. gRPC Services", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md140", [
        [ "ImGuiTestHarness Service", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md141", null ],
        [ "RomService", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md142", null ],
        [ "CanvasAutomation Service", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md143", null ]
      ] ],
      [ "4. Real-Time Collaboration", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md144", [
        [ "Architecture", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md145", null ],
        [ "Core Components", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md146", null ],
        [ "WebSocket Protocol", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md147", null ]
      ] ],
      [ "5. Data Flow Example: AI Agent Edits a Tile", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md148", null ]
    ] ],
    [ "z3ed: AI-Powered CLI for YAZE", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html", [
      [ "1. Overview", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md150", [
        [ "Core Capabilities", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md151", null ]
      ] ],
      [ "2. Quick Start", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md152", [
        [ "Build", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md153", null ],
        [ "AI Setup", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md154", null ],
        [ "Example Commands", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md155", null ],
        [ "Hybrid CLI ↔ GUI Workflow", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md156", null ]
      ] ],
      [ "3. Architecture", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md157", [
        [ "System Components Diagram", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md158", null ]
      ] ],
      [ "4. Agentic & Generative Workflow (MCP)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md159", null ],
      [ "5. Command Reference", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md160", [
        [ "Agent Commands", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md161", null ],
        [ "Resource Commands", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md162", [
          [ "<tt>agent test</tt>: Live Harness Automation", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md163", null ]
        ] ]
      ] ],
      [ "6. Chat Modes", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md164", [
        [ "FTXUI Chat (<tt>agent chat</tt>)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md165", null ],
        [ "Simple Chat (<tt>agent simple-chat</tt>)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md166", null ],
        [ "GUI Chat Widget (Editor Integration)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md167", null ]
      ] ],
      [ "7. AI Provider Configuration", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md168", [
        [ "System Prompt Versions", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md169", null ]
      ] ],
      [ "8. Learn Command - Knowledge Management", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md170", [
        [ "Basic Usage", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md171", null ],
        [ "Project Context", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md172", null ],
        [ "Conversation Memory", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md173", null ],
        [ "Storage Location", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md174", null ]
      ] ],
      [ "9. TODO Management System", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md175", [
        [ "Core Capabilities", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md176", null ],
        [ "Storage Location", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md177", null ]
      ] ],
      [ "10. CLI Output & Help System", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md178", [
        [ "Verbose Logging", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md179", null ],
        [ "Hierarchical Help System", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md180", null ]
      ] ],
      [ "10. Collaborative Sessions & Multimodal Vision", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md181", [
        [ "Overview", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md182", null ],
        [ "Local Collaboration Mode", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md184", [
          [ "How to Use", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md185", null ],
          [ "Features", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md186", null ],
          [ "Cloud Folder Workaround", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md187", null ]
        ] ],
        [ "Network Collaboration Mode (yaze-server v2.0)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md189", [
          [ "Requirements", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md190", null ],
          [ "Server Setup", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md191", null ],
          [ "Client Connection", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md192", null ],
          [ "Core Features", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md193", null ],
          [ "Advanced Features (v2.0)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md194", null ],
          [ "Protocol Reference", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md195", null ],
          [ "Server Configuration", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md196", null ],
          [ "Database Schema (Server v2.0)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md197", null ],
          [ "Deployment", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md198", null ],
          [ "Testing", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md199", null ],
          [ "Security Considerations", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md200", null ]
        ] ],
        [ "Multimodal Vision (Gemini)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md202", [
          [ "Requirements", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md203", null ],
          [ "Capture Modes", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md204", null ],
          [ "How to Use", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md205", null ],
          [ "Example Prompts", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md206", null ]
        ] ],
        [ "Architecture", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md208", null ],
        [ "Troubleshooting", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md210", null ],
        [ "References", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md212", null ]
      ] ],
      [ "11. Roadmap & Implementation Status", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md213", [
        [ "✅ Completed", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md214", null ],
        [ "📌 Current Progress Highlights (October 5, 2025)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md215", null ],
        [ "🚧 Active & Next Steps", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md216", null ],
        [ "✅ Recently Completed (v0.2.0-alpha - October 5, 2025)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md217", [
          [ "Core AI Features", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md218", null ],
          [ "Version Management & Protection", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md219", null ],
          [ "Networking & Collaboration (NEW)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md220", null ],
          [ "UI/UX Enhancements", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md221", null ],
          [ "Build System & Infrastructure", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md222", null ]
        ] ]
      ] ],
      [ "12. Troubleshooting", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md223", null ]
    ] ],
    [ "Asm Style Guide", "d7/d9a/md_docs_2E1-asm-style-guide.html", [
      [ "Table of Contents", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md225", null ],
      [ "File Structure", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md226", null ],
      [ "Labels and Symbols", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md227", null ],
      [ "Comments", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md228", null ],
      [ "Instructions", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md229", null ],
      [ "Macros", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md230", null ],
      [ "Loops and Branching", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md231", null ],
      [ "Data Structures", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md232", null ],
      [ "Code Organization", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md233", null ],
      [ "Custom Code", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md234", null ]
    ] ],
    [ "YAZE Emulator Enhancement Roadmap", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html", [
      [ "📋 Executive Summary", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md237", [
        [ "Core Objectives", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md238", null ]
      ] ],
      [ "🎯 Current State Analysis", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md240", [
        [ "What Works ✅", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md241", null ],
        [ "What's Broken ❌", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md242", null ],
        [ "What's Missing 🚧", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md243", null ]
      ] ],
      [ "🔧 Phase 1: Audio System Fix (Priority: CRITICAL)", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md245", [
        [ "Problem Analysis", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md246", null ],
        [ "Investigation Steps", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md247", null ],
        [ "Likely Fixes", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md248", null ],
        [ "Quick Win Actions", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md249", null ]
      ] ],
      [ "🐛 Phase 2: Advanced Debugger (Mesen2 Feature Parity)", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md251", [
        [ "Feature Comparison: YAZE vs Mesen2", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md252", null ],
        [ "2.1 Breakpoint System", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md253", null ],
        [ "2.2 Memory Watchpoints", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md255", null ],
        [ "2.3 Live Disassembly Viewer", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md257", null ],
        [ "2.4 Enhanced Memory Viewer", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md259", null ]
      ] ],
      [ "🚀 Phase 3: Performance Optimizations", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md261", [
        [ "3.1 Cycle-Accurate Timing", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md262", null ],
        [ "3.2 Dynamic Recompilation (Dynarec)", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md264", null ],
        [ "3.3 Frame Pacing Improvements", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md266", null ]
      ] ],
      [ "🎮 Phase 4: SPC700 Audio CPU Debugger", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md268", [
        [ "4.1 APU Inspector Window", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md269", null ],
        [ "4.2 Audio Sample Export", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md271", null ]
      ] ],
      [ "🤖 Phase 5: z3ed AI Agent Integration", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md273", [
        [ "5.1 Emulator State Access", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md274", null ],
        [ "5.2 Automated Test Generation", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md276", null ],
        [ "5.3 Memory Map Learning", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md278", null ]
      ] ],
      [ "📊 Phase 6: Performance Profiling", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md280", [
        [ "6.1 Cycle Counter & Profiler", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md281", null ],
        [ "6.2 Frame Time Analysis", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md283", null ]
      ] ],
      [ "🎯 Phase 7: Event System & Timeline", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md285", [
        [ "7.1 Event Logger", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md286", null ]
      ] ],
      [ "🧠 Phase 8: AI-Powered Debugging", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md288", [
        [ "8.1 Intelligent Crash Analysis", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md289", null ],
        [ "8.2 Automated Bug Reproduction", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md291", null ]
      ] ],
      [ "🗺️ Implementation Roadmap", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md293", [
        [ "Sprint 1: Audio Fix (Week 1)", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md294", null ],
        [ "Sprint 2: Basic Debugger (Weeks 2-3)", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md295", null ],
        [ "Sprint 3: SPC700 Debugger (Week 4)", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md296", null ],
        [ "Sprint 4: AI Integration (Weeks 5-6)", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md297", null ],
        [ "Sprint 5: Performance (Weeks 7-8)", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md298", null ]
      ] ],
      [ "🔬 Technical Deep Dives", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md300", [
        [ "Audio System Architecture (SDL2)", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md301", null ],
        [ "Memory Regions Reference", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md303", null ]
      ] ],
      [ "🎮 Phase 9: Advanced Features (Mesen2 Parity)", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md305", [
        [ "9.1 Rewind Feature", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md306", null ],
        [ "9.2 TAS (Tool-Assisted Speedrun) Input Recording", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md308", null ],
        [ "9.3 Comparison Mode", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md310", null ]
      ] ],
      [ "🛠️ Optimization Summary", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md312", [
        [ "Quick Wins (< 1 week)", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md313", null ],
        [ "Medium Term (1-2 months)", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md314", null ],
        [ "Long Term (3-6 months)", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md315", null ]
      ] ],
      [ "🤖 z3ed Agent Emulator Tools", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md317", [
        [ "New Tool Categories", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md318", null ],
        [ "Example AI Conversations", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md319", null ]
      ] ],
      [ "📁 File Structure for New Features", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md321", null ],
      [ "🎨 UI Mockups", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md323", [
        [ "Debugger Layout (ImGui)", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md324", null ],
        [ "APU Debugger Layout", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md325", null ]
      ] ],
      [ "🚀 Performance Targets", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md327", [
        [ "Current Performance", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md328", null ],
        [ "Target Performance (Post-Optimization)", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md329", null ],
        [ "Optimization Strategy Priority", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md330", null ]
      ] ],
      [ "🧪 Testing Integration", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md332", [
        [ "Automated Emulator Tests (z3ed)", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md333", null ]
      ] ],
      [ "🔌 z3ed Agent + Emulator Integration", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md335", [
        [ "New Agent Tools", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md336", null ]
      ] ],
      [ "🎓 Learning from Mesen2", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md338", [
        [ "What Makes Mesen2 Great", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md339", null ],
        [ "Our Unique Advantages", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md340", null ]
      ] ],
      [ "📊 Resource Requirements", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md342", [
        [ "Development Time Estimates", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md343", null ],
        [ "Memory Requirements", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md344", null ],
        [ "CPU Requirements", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md345", null ]
      ] ],
      [ "🛣️ Recommended Implementation Order", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md347", [
        [ "Month 1: Foundation", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md348", null ],
        [ "Month 2: Audio & Events", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md349", null ],
        [ "Month 3: AI Integration", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md350", null ],
        [ "Month 4: Performance", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md351", null ],
        [ "Month 5: Polish", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md352", null ]
      ] ],
      [ "🔮 Future Vision: AI-Powered ROM Hacking", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md354", [
        [ "The Ultimate Workflow", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md355", null ]
      ] ],
      [ "🐛 Appendix A: Audio Debugging Checklist", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md357", [
        [ "Check 1: Device Status", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md358", null ],
        [ "Check 2: Queue Size", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md359", null ],
        [ "Check 3: Sample Validation", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md360", null ],
        [ "Check 4: Buffer Allocation", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md361", null ],
        [ "Check 5: SPC700 Execution", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md362", null ],
        [ "Quick Fixes to Try", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md363", null ]
      ] ],
      [ "📝 Appendix B: Mesen2 Feature Reference", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md365", [
        [ "Debugger Windows (Inspiration)", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md366", null ],
        [ "Event Types Tracked", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md367", null ],
        [ "Trace Logger Format", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md368", null ]
      ] ],
      [ "🎯 Success Criteria", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md370", [
        [ "Phase 1 Complete When:", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md371", null ],
        [ "Phase 2 Complete When:", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md372", null ],
        [ "Phase 3 Complete When:", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md373", null ],
        [ "Phase 4 Complete When:", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md374", null ],
        [ "Phase 5 Complete When:", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md375", null ]
      ] ],
      [ "🎓 Learning Resources", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md377", [
        [ "SNES Emulation", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md378", null ],
        [ "Audio Debugging", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md379", null ],
        [ "Performance Optimization", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md380", null ]
      ] ],
      [ "🙏 Credits & Acknowledgments", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md382", null ]
    ] ],
    [ "E2 - Development Guide", "d5/d18/md_docs_2E2-development-guide.html", [
      [ "Editor Status", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md385", null ],
      [ "1. Core Architectural Patterns", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md386", [
        [ "Pattern 1: Modular Systems", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md387", null ],
        [ "Pattern 2: Callback-Based Communication", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md388", null ],
        [ "Pattern 3: Centralized Progressive Loading via <tt>gfx::Arena</tt>", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md389", null ]
      ] ],
      [ "2. UI & Theming System", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md390", [
        [ "2.1. The Theme System (<tt>AgentUITheme</tt>)", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md391", null ],
        [ "2.2. Reusable UI Helper Functions", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md392", null ],
        [ "2.3. Toolbar Implementation (<tt>CompactToolbar</tt>)", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md393", null ]
      ] ],
      [ "3. Key System Implementations & Gotchas", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md394", [
        [ "3.1. Graphics Refresh Logic", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md395", null ],
        [ "3.2. Multi-Area Map Configuration", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md396", null ],
        [ "3.3. Version-Specific Feature Gating", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md397", null ],
        [ "3.4. Entity Visibility for Visual Testing", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md398", null ]
      ] ],
      [ "4. Debugging and Testing", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md399", [
        [ "4.1. Quick Debugging with Startup Flags", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md400", null ],
        [ "4.2. Testing Strategies", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md401", null ],
        [ "3.6. Graphics Sheet Management", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md402", null ],
        [ "Naming Conventions", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md403", null ]
      ] ]
    ] ],
    [ "API Reference", "d8/d73/md_docs_2E3-api-reference.html", [
      [ "C API (<tt>incl/yaze.h</tt>)", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md405", [
        [ "Core Library Functions", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md406", null ],
        [ "ROM Operations", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md407", null ]
      ] ],
      [ "C++ API", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md408", [
        [ "AsarWrapper (<tt>src/app/core/asar_wrapper.h</tt>)", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md409", [
          [ "CLI Examples (<tt>z3ed</tt>)", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md410", null ],
          [ "C++ API Example", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md411", null ],
          [ "Class Definition", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md412", null ]
        ] ]
      ] ],
      [ "Data Structures", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md413", [
        [ "<tt>snes_color</tt>", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md414", null ],
        [ "<tt>zelda3_message</tt>", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md415", null ]
      ] ],
      [ "Error Handling", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md416", [
        [ "C API Error Pattern", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md417", null ]
      ] ]
    ] ],
    [ "E4 - Emulator Development Guide", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html", [
      [ "Table of Contents", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md420", null ],
      [ "1. Current Status", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md422", [
        [ "🎉 Major Breakthrough: Game is Running!", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md423", null ],
        [ "✅ Confirmed Working", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md424", null ],
        [ "🔧 Known Issues (Non-Critical)", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md425", null ]
      ] ],
      [ "2. How to Use the Emulator", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md427", [
        [ "Method 1: Main Yaze Application (GUI)", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md428", null ],
        [ "Method 2: Standalone Emulator (<tt>yaze_emu</tt>)", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md429", null ],
        [ "Method 3: Dungeon Object Emulator Preview", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md430", null ]
      ] ],
      [ "3. Architecture", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md432", [
        [ "Memory System", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md433", [
          [ "SNES Memory Map", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md434", null ]
        ] ],
        [ "CPU-APU-SPC700 Interaction", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md435", null ],
        [ "Component Architecture", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md436", null ]
      ] ],
      [ "4. The Debugging Journey: Critical Fixes", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md438", [
        [ "SPC700 & APU Fixes", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md439", null ],
        [ "The Critical Pattern for Multi-Step Instructions", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md440", null ]
      ] ],
      [ "5. Display & Performance Improvements", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md442", [
        [ "PPU Color Display Fix", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md443", null ],
        [ "Frame Timing & Speed Control", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md444", null ],
        [ "Performance Optimizations", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md445", [
          [ "Frame Skipping", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md446", null ],
          [ "Audio Buffer Management", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md447", null ],
          [ "Performance Gains", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md448", null ]
        ] ],
        [ "ROM Loading Improvements", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md449", null ]
      ] ],
      [ "6. Advanced Features", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md451", [
        [ "Professional Disassembly Viewer", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md452", [
          [ "Architecture", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md453", null ],
          [ "Visual Features", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md454", null ],
          [ "Interactive Elements", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md455", null ],
          [ "UI Features", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md456", null ],
          [ "Performance", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md457", null ]
        ] ],
        [ "Breakpoint System", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md458", null ],
        [ "UI/UX Enhancements", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md459", null ]
      ] ],
      [ "7. Emulator Preview Tool", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md461", [
        [ "Purpose", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md462", null ],
        [ "Critical Fixes Applied", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md463", [
          [ "1. Memory Access Fix (SIGSEGV Crash)", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md464", null ],
          [ "2. RTL vs RTS Fix (Timeout)", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md465", null ],
          [ "3. Palette Validation", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md466", null ],
          [ "4. PPU Configuration", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md467", null ]
        ] ],
        [ "How to Use", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md468", null ],
        [ "What You'll Learn", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md469", null ],
        [ "Reverse Engineering Workflow", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md470", null ],
        [ "UI Enhancements", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md471", null ]
      ] ],
      [ "8. Logging System", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md473", [
        [ "How to Enable", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md474", null ]
      ] ],
      [ "9. Testing", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md476", [
        [ "Unit Tests", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md477", null ],
        [ "Standalone Emulator", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md478", null ],
        [ "Running Tests", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md479", null ],
        [ "Testing Checklist", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md480", null ]
      ] ],
      [ "10. Technical Reference", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md482", [
        [ "PPU Registers", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md483", null ],
        [ "CPU Instructions", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md484", null ],
        [ "Color Format", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md485", null ],
        [ "Performance Metrics", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md486", null ]
      ] ],
      [ "11. Troubleshooting", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md488", [
        [ "Emulator Preview Issues", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md489", null ],
        [ "Color Display Issues", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md490", null ],
        [ "Performance Issues", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md491", null ],
        [ "Build Issues", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md492", null ]
      ] ],
      [ "11.5 Audio System Architecture (October 2025)", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md494", [
        [ "Overview", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md495", null ],
        [ "Audio Backend Abstraction", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md496", null ],
        [ "APU Handshake Debugging System", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md497", null ],
        [ "IPL ROM Handshake Protocol", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md498", null ],
        [ "Music Editor Integration", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md499", null ],
        [ "Audio Testing & Diagnostics", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md500", null ],
        [ "Future Enhancements", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md501", null ]
      ] ],
      [ "12. Next Steps & Roadmap", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md503", [
        [ "🎯 Immediate Priorities (Critical Path to Full Functionality)", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md504", null ],
        [ "🚀 Enhancement Priorities (After Core is Stable)", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md505", null ],
        [ "📝 Technical Debt", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md506", null ],
        [ "Long-Term Enhancements", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md507", null ]
      ] ],
      [ "13. Build Instructions", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md509", [
        [ "Quick Build", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md510", null ],
        [ "Platform-Specific", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md511", null ],
        [ "Build Optimizations", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md512", null ]
      ] ],
      [ "File Reference", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md514", [
        [ "Core Emulation", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md515", null ],
        [ "Debugging", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md516", null ],
        [ "UI", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md517", null ],
        [ "Core", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md518", null ],
        [ "Testing", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md519", null ]
      ] ],
      [ "Status Summary", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md521", [
        [ "✅ Production Ready", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md522", null ]
      ] ]
    ] ],
    [ "E5 - Debugging and Testing Guide", "de/dc5/md_docs_2E5-debugging-guide.html", [
      [ "1. Standardized Logging for Print Debugging", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md525", [
        [ "Log Levels and Usage", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md526", null ],
        [ "Log Categories", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md527", null ],
        [ "Enabling and Configuring Logs via CLI", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md528", null ],
        [ "2. Command-Line Workflows for Testing", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md531", [
          [ "Launching the GUI for Specific Tasks", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md532", null ]
        ] ]
      ] ],
      [ "Open the Dungeon Editor with the Room Matrix and two specific room cards", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md533", null ],
      [ "Available editors: Assembly, Dungeon, Graphics, Music, Overworld, Palette,", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md534", null ],
      [ "Screen, Sprite, Message, Hex, Agent, Settings", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md535", null ],
      [ "Dungeon editor cards: Rooms List, Room Matrix, Entrances List, Room Graphics,", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md536", null ],
      [ "Object Editor, Palette Editor, Room N (where N is room ID)", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md537", null ],
      [ "Fast dungeon room testing", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md538", null ],
      [ "Compare multiple rooms side-by-side", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md539", null ],
      [ "Full dungeon workspace with all tools", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md540", null ],
      [ "Jump straight to overworld editing", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md541", null ],
      [ "Run only fast, dependency-free unit tests", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md543", null ],
      [ "Run tests that require a ROM file", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md544", [
        [ "3. GUI Automation for AI Agents", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md548", [
          [ "Inspecting ROMs with <tt>z3ed</tt>", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md546", null ],
          [ "Architecture Overview", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md549", null ],
          [ "Step-by-Step Workflow for AI", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md550", [
            [ "Step 1: Launch <tt>yaze</tt> with the Test Harness", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md551", null ],
            [ "Step 2: Discover UI Elements", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md552", null ],
            [ "Step 3: Record or Write a Test Script", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md553", null ]
          ] ]
        ] ]
      ] ],
      [ "Start yaze with the room already open", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md554", null ],
      [ "Then your test script just needs to validate the state", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md555", [
        [ "4. Advanced Debugging Tools", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md558", null ]
      ] ]
    ] ],
    [ "Emulator Core Improvements & Optimization Roadmap", "d0/d63/md_docs_2Emulator__Improvements__Roadmap.html", [
      [ "1. Introduction", "d0/d63/md_docs_2Emulator__Improvements__Roadmap.html#autotoc_md560", null ],
      [ "2. Core Architecture & Timing Model (High Priority)", "d0/d63/md_docs_2Emulator__Improvements__Roadmap.html#autotoc_md561", null ],
      [ "3. PPU (Video Rendering) Performance", "d0/d63/md_docs_2Emulator__Improvements__Roadmap.html#autotoc_md562", null ],
      [ "4. APU (Audio) Code Quality & Refinements", "d0/d63/md_docs_2Emulator__Improvements__Roadmap.html#autotoc_md563", null ],
      [ "5. Audio Subsystem & Buffering", "d0/d63/md_docs_2Emulator__Improvements__Roadmap.html#autotoc_md564", null ],
      [ "6. Debugger & Tooling Optimizations (Lower Priority)", "d0/d63/md_docs_2Emulator__Improvements__Roadmap.html#autotoc_md565", null ]
    ] ],
    [ "F2: Dungeon Editor v2 - Complete Guide", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html", [
      [ "Overview", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md568", [
        [ "Key Features", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md569", null ]
      ] ],
      [ "Recent Refactoring (Oct 9-10, 2025)", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md571", [
        [ "Critical Bugs Fixed ✅", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md572", [
          [ "Bug #1: Segfault on Startup", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md573", null ],
          [ "Bug #2: Floor Values Always Zero", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md574", null ],
          [ "Bug #3: Duplicate Floor Variables", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md575", null ],
          [ "Bug #4: Wall Graphics Not Rendering", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md576", null ]
        ] ],
        [ "Architecture Improvements ✅", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md577", null ],
        [ "UI Improvements ✅", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md578", null ]
      ] ],
      [ "Architecture", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md580", [
        [ "Component Overview", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md581", null ],
        [ "Room Rendering Pipeline", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md582", null ],
        [ "Room Structure (Bottom to Top)", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md583", null ]
      ] ],
      [ "Next Development Steps", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md585", [
        [ "High Priority (Must Do)", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md586", [
          [ "1. Implement Room Layout Base Layer Rendering", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md587", null ],
          [ "2. Door Rendering at Room Edges", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md589", null ],
          [ "3. Object Name Labels from String Array", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md591", null ],
          [ "4. Fix Plus Button to Select Any Room", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md593", null ]
        ] ],
        [ "Medium Priority (Should Do)", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md595", [
          [ "5. Update current_room_id on Card Hover", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md596", null ],
          [ "6. Fix InputHexByte +/- Button Events", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md598", null ],
          [ "7. Update Room Graphics Card", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md600", null ]
        ] ],
        [ "Lower Priority (Nice to Have)", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md602", [
          [ "8. Selection System with Primitive Squares", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md603", null ],
          [ "9. Move Backend Logic to DungeonEditorSystem", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md605", null ],
          [ "10. Extract ROM Addresses to Separate File", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md607", null ]
        ] ]
      ] ],
      [ "Quick Start", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md609", [
        [ "Build & Run", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md610", null ],
        [ "Expected Visuals", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md611", null ]
      ] ],
      [ "Testing & Verification", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md613", [
        [ "Debug Commands", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md614", null ],
        [ "Visual Checklist", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md615", null ]
      ] ],
      [ "Technical Reference", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md617", [
        [ "Correct Loading Order", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md618", null ],
        [ "Floor Graphics Accessors", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md619", null ],
        [ "Object Visualization", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md620", null ],
        [ "Texture Atlas (Future-Proof Stub)", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md621", null ]
      ] ],
      [ "Files Modified (16 files)", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md623", [
        [ "Dungeon Editor", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md624", null ],
        [ "Room System", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md625", null ],
        [ "Graphics Infrastructure", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md626", null ],
        [ "Tests", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md627", null ]
      ] ],
      [ "Statistics", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md629", null ],
      [ "Troubleshooting", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md631", [
        [ "Floor tiles blank/wrong?", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md632", null ],
        [ "Objects not visible?", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md633", null ],
        [ "Wall graphics not rendering?", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md634", null ],
        [ "Performance issues?", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md635", null ]
      ] ],
      [ "Session Summary", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md637", [
        [ "Accomplished This Session", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md638", null ],
        [ "Statistics", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md639", null ]
      ] ],
      [ "Code Reference", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md641", [
        [ "Property Table (NEW)", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md642", null ],
        [ "Compact Layer Controls (NEW)", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md643", null ],
        [ "Room ID in Title (NEW)", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md644", null ]
      ] ],
      [ "Related Documentation", "dc/d27/md_docs_2F2-dungeon-editor-v2-guide.html#autotoc_md646", null ]
    ] ],
    [ "Tile16 Editor Palette System", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html", [
      [ "Executive Summary", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md649", null ],
      [ "Problem Analysis", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md650", [
        [ "Critical Issues Identified", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md651", null ]
      ] ],
      [ "Solution Architecture", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md652", [
        [ "Core Design Principles", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md653", null ],
        [ "256-Color Overworld Palette Structure", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md654", null ],
        [ "Sheet-to-Palette Mapping", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md655", null ],
        [ "Palette Button Mapping", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md656", null ]
      ] ],
      [ "Implementation Details", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md657", [
        [ "1. Fixed SetPaletteWithTransparent Method", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md658", null ],
        [ "2. Corrected Tile16 Editor Palette System", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md659", null ],
        [ "3. Palette Coordination Flow", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md660", null ]
      ] ],
      [ "UI/UX Refactoring", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md661", [
        [ "New Three-Column Layout", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md662", null ],
        [ "Canvas Context Menu Fixes", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md663", null ],
        [ "Dynamic Zoom Controls", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md664", null ]
      ] ],
      [ "Testing Protocol", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md665", [
        [ "Crash Prevention Testing", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md666", null ],
        [ "Color Alignment Testing", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md667", null ],
        [ "UI/UX Testing", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md668", null ]
      ] ],
      [ "Error Handling", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md669", [
        [ "Bounds Checking", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md670", null ],
        [ "Fallback Mechanisms", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md671", null ]
      ] ],
      [ "Debug Information Display", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md672", null ],
      [ "Known Issues and Ongoing Work", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md673", [
        [ "Completed Items ✅", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md674", null ],
        [ "Active Issues ⚠️", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md675", null ],
        [ "Current Status Summary", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md676", null ],
        [ "Future Enhancements", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md677", null ]
      ] ],
      [ "Maintenance Notes", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md678", null ],
      [ "Next Steps", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md680", [
        [ "Immediate Priorities", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md681", null ],
        [ "Investigation Areas", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md682", null ]
      ] ]
    ] ],
    [ "Overworld Loading Guide", "d9/d85/md_docs_2F3-overworld-loading.html", [
      [ "Table of Contents", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md685", null ],
      [ "Overview", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md686", null ],
      [ "ROM Types and Versions", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md687", [
        [ "Version Detection", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md688", null ],
        [ "Feature Support by Version", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md689", null ]
      ] ],
      [ "Overworld Map Structure", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md690", [
        [ "Core Properties", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md691", null ]
      ] ],
      [ "Overlays and Special Area Maps", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md692", [
        [ "Understanding Overlays", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md693", null ],
        [ "Special Area Maps (0x80-0x9F)", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md694", null ],
        [ "Overlay ID Mappings", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md695", null ],
        [ "Drawing Order", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md696", null ],
        [ "Vanilla Overlay Loading", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md697", null ],
        [ "Special Area Graphics Loading", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md698", null ]
      ] ],
      [ "Loading Process", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md699", [
        [ "1. Version Detection", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md700", null ],
        [ "2. Map Initialization", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md701", null ],
        [ "3. Property Loading", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md702", [
          [ "Vanilla ROMs (asm_version == 0xFF)", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md703", null ],
          [ "ZSCustomOverworld v2/v3", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md704", null ]
        ] ],
        [ "4. Custom Data Loading", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md705", null ]
      ] ],
      [ "ZScream Implementation", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md706", [
        [ "OverworldMap Constructor", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md707", null ],
        [ "Key Methods", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md708", null ]
      ] ],
      [ "Yaze Implementation", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md709", [
        [ "OverworldMap Constructor", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md710", null ],
        [ "Key Methods", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md711", null ],
        [ "Current Status", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md712", null ]
      ] ],
      [ "Key Differences", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md713", [
        [ "1. Language and Architecture", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md714", null ],
        [ "2. Data Structures", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md715", null ],
        [ "3. Error Handling", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md716", null ],
        [ "4. Graphics Processing", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md717", null ]
      ] ],
      [ "Common Issues and Solutions", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md718", [
        [ "1. Version Detection Issues", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md719", null ],
        [ "2. Palette Loading Errors", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md720", null ],
        [ "3. Graphics Not Loading", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md721", null ],
        [ "4. Overlay Issues", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md722", null ],
        [ "5. Large Map Problems", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md723", null ],
        [ "6. Special Area Graphics Issues", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md724", null ]
      ] ],
      [ "Best Practices", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md725", [
        [ "1. Version-Specific Code", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md726", null ],
        [ "2. Error Handling", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md727", null ],
        [ "3. Memory Management", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md728", null ],
        [ "4. Thread Safety", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md729", null ]
      ] ],
      [ "Conclusion", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md730", null ]
    ] ],
    [ "Overworld Agent Guide - AI-Powered Overworld Editing", "de/d00/md_docs_2F4-overworld-agent-guide.html", [
      [ "Overview", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md733", null ],
      [ "Quick Start", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md735", [
        [ "Prerequisites", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md736", null ],
        [ "First Agent Interaction", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md737", null ]
      ] ],
      [ "Available Tools", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md739", [
        [ "Read-Only Tools (Safe for AI)", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md740", [
          [ "overworld-get-tile", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md741", null ],
          [ "overworld-get-visible-region", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md743", null ],
          [ "overworld-analyze-region", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md745", null ]
        ] ],
        [ "Write Tools (Sandboxed - Creates Proposals)", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md747", [
          [ "overworld-set-tile", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md748", null ],
          [ "overworld-set-tiles-batch", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md750", null ]
        ] ]
      ] ],
      [ "Multimodal Vision Workflow", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md752", [
        [ "Step 1: Capture Canvas Screenshot", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md753", null ],
        [ "Step 2: AI Analyzes Screenshot", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md754", null ],
        [ "Step 3: Generate Edit Plan", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md755", null ],
        [ "Step 4: Execute Plan (Sandbox)", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md756", null ],
        [ "Step 5: Human Review", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md757", null ]
      ] ],
      [ "Example Workflows", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md759", [
        [ "Workflow 1: Create Forest Area", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md760", null ],
        [ "Workflow 2: Fix Tile Placement Errors", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md762", null ],
        [ "Workflow 3: Generate Path", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md764", null ]
      ] ],
      [ "Common Tile IDs Reference", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md766", [
        [ "Grass & Ground", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md767", null ],
        [ "Trees & Plants", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md768", null ],
        [ "Water", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md769", null ],
        [ "Paths & Roads", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md770", null ],
        [ "Structures", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md771", null ]
      ] ],
      [ "Best Practices for AI Agents", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md773", [
        [ "1. Always Analyze Before Editing", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md774", null ],
        [ "2. Use Batch Operations", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md775", null ],
        [ "3. Provide Clear Reasoning", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md776", null ],
        [ "4. Respect Tile Boundaries", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md777", null ],
        [ "5. Check Visibility", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md778", null ],
        [ "6. Create Reversible Edits", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md779", null ]
      ] ],
      [ "Error Handling", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md781", [
        [ "\"Tile ID out of range\"", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md782", null ],
        [ "\"Coordinates out of bounds\"", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md783", null ],
        [ "\"Proposal rejected\"", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md784", null ],
        [ "\"ROM file locked\"", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md785", null ]
      ] ],
      [ "Testing AI-Generated Edits", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md787", [
        [ "Manual Testing", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md788", null ],
        [ "Automated Testing", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md789", null ]
      ] ],
      [ "Advanced Techniques", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md791", [
        [ "Technique 1: Pattern Recognition", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md792", null ],
        [ "Technique 2: Style Transfer", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md793", null ],
        [ "Technique 3: Procedural Generation", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md794", null ]
      ] ],
      [ "Integration with GUI Automation", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md796", [
        [ "Record Human Edits", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md797", null ],
        [ "Replay for AI Training", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md798", null ],
        [ "Validate AI Edits", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md799", null ]
      ] ],
      [ "Collaboration Features", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md801", [
        [ "Network Collaboration", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md802", null ],
        [ "Proposal Voting", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md803", null ]
      ] ],
      [ "Troubleshooting", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md805", [
        [ "Agent Not Responding", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md806", null ],
        [ "Tools Not Available", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md807", null ],
        [ "gRPC Connection Failed", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md808", null ]
      ] ],
      [ "See Also", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md810", null ]
    ] ],
    [ "G1 - Canvas System and Automation", "d1/dc6/md_docs_2G1-canvas-guide.html", [
      [ "1. Core Concepts", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md812", [
        [ "Canvas Structure", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md813", null ],
        [ "Coordinate Systems", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md814", null ]
      ] ],
      [ "2. Canvas API and Usage", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md815", [
        [ "Modern Begin/End Pattern", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md816", null ],
        [ "Feature: Tile Painting", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md817", null ],
        [ "Feature: Tile Selection", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md818", null ],
        [ "Feature: Large Map Support", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md819", null ],
        [ "Feature: Context Menu", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md820", null ]
      ] ],
      [ "3. Canvas Automation API", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md821", [
        [ "Accessing the API", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md822", null ],
        [ "Tile Operations", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md823", null ],
        [ "Selection Operations", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md824", null ],
        [ "View Operations", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md825", null ],
        [ "Query Operations", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md826", null ],
        [ "Simulation Operations (for GUI Automation)", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md827", null ]
      ] ],
      [ "4. <tt>z3ed</tt> CLI Integration", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md828", null ],
      [ "5. gRPC Service", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md829", null ]
    ] ],
    [ "SDL2 to SDL3 Migration and Rendering Abstraction Plan", "d6/df2/md_docs_2G2-renderer-migration-plan.html", [
      [ "1. Introduction", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md831", null ],
      [ "2. Current State Analysis", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md832", null ],
      [ "3. Proposed Architecture: The <tt>Renderer</tt> Abstraction", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md833", [
        [ "3.1. The <tt>IRenderer</tt> Interface", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md834", null ],
        [ "3.2. The <tt>SDL2Renderer</tt> Implementation", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md835", null ]
      ] ],
      [ "4. Migration Plan", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md836", [
        [ "Phase 1: Implement the Abstraction Layer", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md837", null ],
        [ "Phase 2: Migrate to SDL3", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md838", null ],
        [ "Phase 3: Support for Multiple Rendering Backends", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md839", null ]
      ] ],
      [ "5. Conclusion", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md840", null ]
    ] ],
    [ "SNES Palette System Overview", "da/dfd/md_docs_2G3-palete-system-overview.html", [
      [ "Understanding SNES Color and Palette Organization", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md842", [
        [ "Core Concepts", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md843", [
          [ "1. SNES Color Format (15-bit BGR555)", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md844", null ],
          [ "2. Palette Groups in Zelda 3", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md845", null ]
        ] ],
        [ "Dungeon Palette System", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md846", [
          [ "Structure", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md847", null ],
          [ "Usage", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md848", null ],
          [ "Color Distribution (90 colors)", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md849", null ]
        ] ],
        [ "Overworld Palette System", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md850", [
          [ "Structure", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md851", null ],
          [ "3BPP Graphics and Left/Right Palettes", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md852", null ]
        ] ],
        [ "Common Issues and Solutions", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md853", [
          [ "Issue 1: Empty Palette", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md854", null ],
          [ "Issue 2: Bitmap Corruption", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md855", null ],
          [ "Issue 3: ROM Not Loaded in Preview", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md856", null ]
        ] ],
        [ "Palette Editor Integration", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md857", [
          [ "Key Functions for UI", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md858", null ],
          [ "Palette Widget Requirements", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md859", null ]
        ] ],
        [ "Graphics Manager Integration", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md860", [
          [ "Sheet Palette Assignment", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md861", null ]
        ] ],
        [ "Best Practices", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md862", null ],
        [ "ROM Addresses (for reference)", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md863", null ]
      ] ],
      [ "Graphics Sheet Palette Application", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md864", [
        [ "Default Palette Assignment", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md865", null ],
        [ "Palette Update Workflow", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md866", null ],
        [ "Common Pitfalls", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md867", null ]
      ] ]
    ] ],
    [ "Graphics Renderer Migration - Complete Documentation", "d5/dc8/md_docs_2G3-renderer-migration-complete.html", [
      [ "📋 Executive Summary", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md870", [
        [ "Key Achievements", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md871", null ]
      ] ],
      [ "🎯 Migration Goals & Results", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md873", null ],
      [ "🏗️ Architecture Overview", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md875", [
        [ "Before: Singleton Pattern", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md876", null ],
        [ "After: Dependency Injection + Deferred Queue", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md877", null ]
      ] ],
      [ "📦 Component Details", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md879", [
        [ "1. IRenderer Interface (<tt>src/app/gfx/backend/irenderer.h</tt>)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md880", null ],
        [ "2. SDL2Renderer (<tt>src/app/gfx/backend/sdl2_renderer.{h,cc}</tt>)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md882", null ],
        [ "3. Arena Deferred Texture Queue (<tt>src/app/gfx/arena.{h,cc}</tt>)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md884", null ],
        [ "4. Bitmap Palette Refactoring (<tt>src/app/gfx/bitmap.{h,cc}</tt>)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md886", null ],
        [ "5. Canvas Optional Renderer (<tt>src/app/gui/canvas.{h,cc}</tt>)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md888", null ],
        [ "6. Tilemap Texture Queue Integration (<tt>src/app/gfx/tilemap.cc</tt>)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md890", null ]
      ] ],
      [ "🔄 Dependency Injection Flow", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md892", [
        [ "Controller → EditorManager → Editors", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md893", null ]
      ] ],
      [ "⚡ Performance Optimizations", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md895", [
        [ "1. Batched Texture Processing", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md896", null ],
        [ "2. Frame Rate Limiting", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md897", null ],
        [ "3. Auto-Pause on Focus Loss", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md898", null ],
        [ "4. Surface/Texture Pooling", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md899", null ]
      ] ],
      [ "🗺️ Migration Map: File Changes", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md901", [
        [ "Core Architecture Files (New)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md902", null ],
        [ "Core Modified Files (Major)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md903", null ],
        [ "Editor Files (Renderer Injection)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md904", null ],
        [ "Emulator Files (Special Handling)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md905", null ],
        [ "GUI/Widget Files", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md906", null ],
        [ "Test Files (Updated for DI)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md907", null ]
      ] ],
      [ "🔧 Critical Fixes Applied", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md909", [
        [ "1. Bitmap::SetPalette() Crash", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md910", null ],
        [ "2. SDL2Renderer::UpdateTexture() SIGSEGV", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md912", null ],
        [ "3. Emulator Audio System Corruption", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md914", null ],
        [ "4. Emulator Cleanup During Shutdown", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md916", null ],
        [ "5. Controller/CreateWindow Initialization Order", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md918", null ]
      ] ],
      [ "🎨 Canvas Refactoring", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md920", [
        [ "The Challenge", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md921", null ],
        [ "The Solution: Backwards-Compatible Dual API", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md922", null ]
      ] ],
      [ "🧪 Testing Strategy", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md924", [
        [ "Test Files Updated", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md925", null ]
      ] ],
      [ "🛣️ Road to SDL3", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md927", [
        [ "Why This Migration Matters", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md928", null ],
        [ "Our Abstraction Layer Handles This", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md929", null ]
      ] ],
      [ "📊 Performance Benchmarks", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md931", [
        [ "Texture Loading Performance", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md932", null ],
        [ "Graphics Editor Performance", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md933", null ],
        [ "Emulator Performance", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md934", null ]
      ] ],
      [ "🐛 Bugs Fixed During Migration", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md936", [
        [ "Critical Crashes", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md937", null ],
        [ "Build Errors", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md938", null ]
      ] ],
      [ "💡 Key Design Patterns Used", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md940", [
        [ "1. Dependency Injection", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md941", null ],
        [ "2. Command Pattern (Deferred Queue)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md942", null ],
        [ "3. RAII (Resource Management)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md943", null ],
        [ "4. Adapter Pattern (Backend Abstraction)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md944", null ],
        [ "5. Singleton with DI (Arena)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md945", null ]
      ] ],
      [ "🔮 Future Enhancements", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md947", [
        [ "Short Term (SDL2)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md948", null ],
        [ "Medium Term (SDL3 Prep)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md949", null ],
        [ "Long Term (SDL3 Migration)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md950", null ]
      ] ],
      [ "📝 Lessons Learned", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md952", [
        [ "What Went Well", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md953", null ],
        [ "Challenges Overcome", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md954", null ],
        [ "Best Practices Established", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md955", null ]
      ] ],
      [ "🎓 Technical Deep Dive: Texture Queue System", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md957", [
        [ "Why Deferred Rendering?", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md958", null ],
        [ "Queue Processing Algorithm", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md959", null ]
      ] ],
      [ "🏆 Success Metrics", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md961", [
        [ "Build Health", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md962", null ],
        [ "Runtime Stability", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md963", null ],
        [ "Performance", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md964", null ],
        [ "Code Quality", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md965", null ]
      ] ],
      [ "📚 References", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md967", [
        [ "Related Documents", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md968", null ],
        [ "Key Commits", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md969", null ],
        [ "External Resources", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md970", null ]
      ] ],
      [ "🙏 Acknowledgments", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md972", null ],
      [ "🎉 Conclusion", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md974", null ],
      [ "🚧 Known Issues & Next Steps", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md976", [
        [ "macOS-Specific Issues (Not Renderer-Related)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md977", null ],
        [ "Stability Improvements for Next Session", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md978", [
          [ "High Priority", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md979", null ],
          [ "Medium Priority", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md980", null ],
          [ "Low Priority (SDL3 Migration)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md981", null ]
        ] ],
        [ "Testing Recommendations", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md982", null ]
      ] ],
      [ "🎵 Final Notes", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md984", null ]
    ] ],
    [ "Changelog", "d6/da7/md_docs_2H1-changelog.html", [
      [ "0.3.2 (October 2025)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md987", [
        [ "CI/CD & Release Improvements", "d6/da7/md_docs_2H1-changelog.html#autotoc_md988", null ],
        [ "Rendering Pipeline Fixes", "d6/da7/md_docs_2H1-changelog.html#autotoc_md989", null ],
        [ "Card-Based UI System", "d6/da7/md_docs_2H1-changelog.html#autotoc_md990", null ],
        [ "Tile16 Editor & Graphics System", "d6/da7/md_docs_2H1-changelog.html#autotoc_md991", null ],
        [ "Windows Platform Stability", "d6/da7/md_docs_2H1-changelog.html#autotoc_md992", null ],
        [ "Emulator: Audio System Infrastructure", "d6/da7/md_docs_2H1-changelog.html#autotoc_md993", null ],
        [ "Emulator: Critical Performance Fixes", "d6/da7/md_docs_2H1-changelog.html#autotoc_md994", null ],
        [ "Emulator: UI Organization & Input System", "d6/da7/md_docs_2H1-changelog.html#autotoc_md995", null ],
        [ "Debugger: Breakpoint & Watchpoint Systems", "d6/da7/md_docs_2H1-changelog.html#autotoc_md996", null ],
        [ "Build System Simplifications", "d6/da7/md_docs_2H1-changelog.html#autotoc_md997", null ],
        [ "Build System: Windows Platform Improvements", "d6/da7/md_docs_2H1-changelog.html#autotoc_md998", null ],
        [ "GUI & UX Modernization", "d6/da7/md_docs_2H1-changelog.html#autotoc_md999", null ],
        [ "Overworld Editor Refactoring", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1000", null ],
        [ "Build System & Stability", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1001", null ],
        [ "Future Optimizations (Planned)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1002", null ],
        [ "Technical Notes", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1003", null ]
      ] ],
      [ "0.3.1 (September 2025)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1004", [
        [ "Major Features", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1005", null ],
        [ "Tile16 Editor Enhancements", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1006", null ],
        [ "ZSCustomOverworld v3 Implementation", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1007", null ],
        [ "Technical Improvements", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1008", null ],
        [ "User Interface", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1009", null ],
        [ "Bug Fixes", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1010", null ],
        [ "ZScream Compatibility Improvements", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1011", null ]
      ] ],
      [ "0.3.0 (September 2025)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1012", [
        [ "Major Features", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1013", null ],
        [ "User Interface & Theming", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1014", null ],
        [ "Enhancements", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1015", null ],
        [ "Technical Improvements", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1016", null ],
        [ "Bug Fixes", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1017", null ]
      ] ],
      [ "0.2.2 (December 2024)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1018", null ],
      [ "0.2.1 (August 2024)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1019", null ],
      [ "0.2.0 (July 2024)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1020", null ],
      [ "0.1.0 (May 2024)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1021", null ],
      [ "0.0.9 (April 2024)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1022", null ],
      [ "0.0.8 (February 2024)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1023", null ],
      [ "0.0.7 (January 2024)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1024", null ],
      [ "0.0.6 (November 2023)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1025", null ],
      [ "0.0.5 (November 2023)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1026", null ],
      [ "0.0.4 (November 2023)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1027", null ],
      [ "0.0.3 (October 2023)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1028", null ]
    ] ],
    [ "Roadmap", "d8/d97/md_docs_2I1-roadmap.html", [
      [ "Current Focus: AI & Editor Polish", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1030", null ],
      [ "0.4.X (Next Major Release) - Advanced Tooling & UX", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1032", [
        [ "Priority 1: Editor Features & UX", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1033", null ],
        [ "Priority 2: <tt>z3ed</tt> AI Agent", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1034", null ],
        [ "Priority 3: Testing & Stability", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1035", null ]
      ] ],
      [ "0.5.X - Feature Expansion", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1037", null ],
      [ "0.6.X - Content & Integration", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1039", null ],
      [ "Recently Completed (v0.3.3 - October 6, 2025)", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1041", null ],
      [ "Recently Completed (v0.3.2)", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1042", null ]
    ] ],
    [ "yaze Documentation", "d3/d4c/md_docs_2index.html", [
      [ "A: Getting Started & Testing", "d3/d4c/md_docs_2index.html#autotoc_md1044", null ],
      [ "B: Build & Platform", "d3/d4c/md_docs_2index.html#autotoc_md1045", null ],
      [ "C: <tt>z3ed</tt> CLI", "d3/d4c/md_docs_2index.html#autotoc_md1046", null ],
      [ "E: Development & API", "d3/d4c/md_docs_2index.html#autotoc_md1047", null ],
      [ "F: Technical Documentation", "d3/d4c/md_docs_2index.html#autotoc_md1048", null ],
      [ "G: GUI Guides", "d3/d4c/md_docs_2index.html#autotoc_md1049", null ],
      [ "H: Project Info", "d3/d4c/md_docs_2index.html#autotoc_md1050", null ],
      [ "I: Roadmap", "d3/d4c/md_docs_2index.html#autotoc_md1051", null ],
      [ "R: ROM Reference", "d3/d4c/md_docs_2index.html#autotoc_md1052", null ]
    ] ],
    [ "A Link to the Past ROM Reference", "d7/d4f/md_docs_2R1-alttp-rom-reference.html", [
      [ "Graphics System", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1055", [
        [ "Graphics Sheets", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1056", null ],
        [ "Palette System", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1057", [
          [ "Color Format", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1058", null ],
          [ "Palette Groups", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1059", null ]
        ] ]
      ] ],
      [ "Dungeon System", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1060", [
        [ "Room Data Structure", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1061", null ],
        [ "Tile16 Format", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1062", null ],
        [ "Blocksets", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1063", null ]
      ] ],
      [ "Message System", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1064", [
        [ "Text Data Locations", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1065", null ],
        [ "Character Encoding", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1066", null ],
        [ "Text Commands", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1067", null ],
        [ "Font Graphics", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1068", null ]
      ] ],
      [ "Overworld System", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1069", [
        [ "Map Structure", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1070", null ],
        [ "Area Sizes (ZSCustomOverworld v3+)", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1071", null ],
        [ "Tile Format", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1072", null ]
      ] ],
      [ "Compression", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1073", [
        [ "LC-LZ2 Algorithm", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1074", null ]
      ] ],
      [ "Memory Map", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1075", [
        [ "ROM Banks (LoROM)", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1076", null ],
        [ "Important ROM Locations", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1077", null ]
      ] ]
    ] ],
    [ "yaze - Yet Another Zelda3 Editor", "d0/d30/md_README.html", [
      [ "Version 0.3.2 - Release", "d0/d30/md_README.html#autotoc_md1080", null ],
      [ "Quick Start", "d0/d30/md_README.html#autotoc_md1085", [
        [ "Build", "d0/d30/md_README.html#autotoc_md1086", null ],
        [ "Applications", "d0/d30/md_README.html#autotoc_md1087", null ]
      ] ],
      [ "Usage", "d0/d30/md_README.html#autotoc_md1088", [
        [ "GUI Editor", "d0/d30/md_README.html#autotoc_md1089", null ],
        [ "Command Line Tool", "d0/d30/md_README.html#autotoc_md1090", null ],
        [ "C++ API", "d0/d30/md_README.html#autotoc_md1091", null ]
      ] ],
      [ "Documentation", "d0/d30/md_README.html#autotoc_md1092", null ],
      [ "Supported Platforms", "d0/d30/md_README.html#autotoc_md1093", null ],
      [ "ROM Compatibility", "d0/d30/md_README.html#autotoc_md1094", null ],
      [ "Contributing", "d0/d30/md_README.html#autotoc_md1095", null ],
      [ "License", "d0/d30/md_README.html#autotoc_md1096", null ],
      [ "🙏 Acknowledgments", "d0/d30/md_README.html#autotoc_md1097", null ],
      [ "📸 Screenshots", "d0/d30/md_README.html#autotoc_md1098", null ]
    ] ],
    [ "yaze Build Scripts", "de/d82/md_scripts_2README.html", [
      [ "Windows Scripts", "de/d82/md_scripts_2README.html#autotoc_md1101", [
        [ "vcpkg Setup (Optional)", "de/d82/md_scripts_2README.html#autotoc_md1102", null ]
      ] ],
      [ "Windows Build Workflow", "de/d82/md_scripts_2README.html#autotoc_md1103", [
        [ "Recommended: Visual Studio CMake Mode", "de/d82/md_scripts_2README.html#autotoc_md1104", null ],
        [ "Command Line Build", "de/d82/md_scripts_2README.html#autotoc_md1105", null ],
        [ "Compiler Notes", "de/d82/md_scripts_2README.html#autotoc_md1106", null ]
      ] ],
      [ "Quick Start (Windows)", "de/d82/md_scripts_2README.html#autotoc_md1107", [
        [ "Option 1: Visual Studio (Recommended)", "de/d82/md_scripts_2README.html#autotoc_md1108", null ],
        [ "Option 2: Command Line", "de/d82/md_scripts_2README.html#autotoc_md1109", null ],
        [ "Option 3: With vcpkg (Optional)", "de/d82/md_scripts_2README.html#autotoc_md1110", null ]
      ] ],
      [ "Troubleshooting", "de/d82/md_scripts_2README.html#autotoc_md1111", [
        [ "Common Issues", "de/d82/md_scripts_2README.html#autotoc_md1112", null ],
        [ "Getting Help", "de/d82/md_scripts_2README.html#autotoc_md1113", null ]
      ] ],
      [ "Other Scripts", "de/d82/md_scripts_2README.html#autotoc_md1114", null ],
      [ "Build Environment Verification", "de/d82/md_scripts_2README.html#autotoc_md1115", [
        [ "<tt>verify-build-environment.ps1</tt> / <tt>.sh</tt>", "de/d82/md_scripts_2README.html#autotoc_md1116", null ],
        [ "Usage", "de/d82/md_scripts_2README.html#autotoc_md1117", null ]
      ] ]
    ] ],
    [ "Agent Editor Module", "d6/df7/md_src_2app_2editor_2agent_2README.html", [
      [ "Overview", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1119", null ],
      [ "Architecture", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1120", [
        [ "Core Components", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1121", [
          [ "AgentEditor (<tt>agent_editor.h/cc</tt>)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1122", null ],
          [ "AgentChatWidget (<tt>agent_chat_widget.h/cc</tt>)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1123", null ],
          [ "AgentChatHistoryCodec (<tt>agent_chat_history_codec.h/cc</tt>)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1124", null ]
        ] ],
        [ "Collaboration Coordinators", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1125", [
          [ "AgentCollaborationCoordinator (<tt>agent_collaboration_coordinator.h/cc</tt>)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1126", null ],
          [ "NetworkCollaborationCoordinator (<tt>network_collaboration_coordinator.h/cc</tt>)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1127", null ]
        ] ]
      ] ],
      [ "Usage", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1128", [
        [ "Initialization", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1129", null ],
        [ "Drawing", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1130", null ],
        [ "Session Management", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1131", null ],
        [ "Network Mode (requires YAZE_WITH_GRPC and YAZE_WITH_JSON)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1132", null ]
      ] ],
      [ "File Structure", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1133", null ],
      [ "Build Configuration", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1134", [
        [ "Required", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1135", null ],
        [ "Optional", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1136", null ]
      ] ],
      [ "Data Files", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1137", [
        [ "Local Storage", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1138", null ],
        [ "Session File Format", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1139", null ]
      ] ],
      [ "Integration with EditorManager", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1140", null ],
      [ "Dependencies", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1141", [
        [ "Internal", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1142", null ],
        [ "External (when enabled)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1143", null ]
      ] ],
      [ "Advanced Features (v2.0)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1144", [
        [ "ROM Synchronization", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1145", null ],
        [ "Multimodal Snapshot Sharing", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1146", null ],
        [ "Proposal Management", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1147", null ],
        [ "AI Agent Integration", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1148", null ],
        [ "Health Monitoring", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1149", null ]
      ] ],
      [ "Future Enhancements", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1150", null ],
      [ "Server Protocol", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1151", [
        [ "Client → Server", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1152", null ],
        [ "Server → Client", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1153", null ]
      ] ]
    ] ],
    [ "End-to-End (E2E) Tests", "d9/db0/md_test_2e2e_2README.html", [
      [ "Active Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md1157", [
        [ "✅ Working Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md1158", null ],
        [ "📝 Dungeon Editor Smoke Test", "d9/db0/md_test_2e2e_2README.html#autotoc_md1159", null ]
      ] ],
      [ "Running Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md1160", [
        [ "All E2E Tests (GUI Mode)", "d9/db0/md_test_2e2e_2README.html#autotoc_md1161", null ],
        [ "Specific Test Category", "d9/db0/md_test_2e2e_2README.html#autotoc_md1162", null ],
        [ "Dungeon Editor Test Only", "d9/db0/md_test_2e2e_2README.html#autotoc_md1163", null ]
      ] ],
      [ "Test Development", "d9/db0/md_test_2e2e_2README.html#autotoc_md1164", [
        [ "Creating New Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md1165", null ],
        [ "Register in yaze_test.cc", "d9/db0/md_test_2e2e_2README.html#autotoc_md1166", null ],
        [ "ImGui Test Engine API", "d9/db0/md_test_2e2e_2README.html#autotoc_md1167", null ]
      ] ],
      [ "Test Logging", "d9/db0/md_test_2e2e_2README.html#autotoc_md1168", null ],
      [ "Test Infrastructure", "d9/db0/md_test_2e2e_2README.html#autotoc_md1169", [
        [ "File Organization", "d9/db0/md_test_2e2e_2README.html#autotoc_md1170", null ],
        [ "Helper Functions", "d9/db0/md_test_2e2e_2README.html#autotoc_md1171", null ]
      ] ],
      [ "Future Test Ideas", "d9/db0/md_test_2e2e_2README.html#autotoc_md1172", null ],
      [ "Troubleshooting", "d9/db0/md_test_2e2e_2README.html#autotoc_md1173", [
        [ "Test Crashes in GUI Mode", "d9/db0/md_test_2e2e_2README.html#autotoc_md1174", null ],
        [ "Tests Not Found", "d9/db0/md_test_2e2e_2README.html#autotoc_md1175", null ],
        [ "ImGui Items Not Found", "d9/db0/md_test_2e2e_2README.html#autotoc_md1176", null ]
      ] ],
      [ "References", "d9/db0/md_test_2e2e_2README.html#autotoc_md1177", null ],
      [ "Status", "d9/db0/md_test_2e2e_2README.html#autotoc_md1178", null ]
    ] ],
    [ "yaze Test Suite", "d0/d46/md_test_2README.html", [
      [ "Directory Structure", "d0/d46/md_test_2README.html#autotoc_md1180", null ],
      [ "Test Categories", "d0/d46/md_test_2README.html#autotoc_md1181", [
        [ "Unit Tests (<tt>unit/</tt>)", "d0/d46/md_test_2README.html#autotoc_md1182", null ],
        [ "Integration Tests (<tt>integration/</tt>)", "d0/d46/md_test_2README.html#autotoc_md1183", null ],
        [ "End-to-End Tests (<tt>e2e/</tt>)", "d0/d46/md_test_2README.html#autotoc_md1184", null ]
      ] ],
      [ "Enhanced Test Runner", "d0/d46/md_test_2README.html#autotoc_md1185", [
        [ "Usage Examples", "d0/d46/md_test_2README.html#autotoc_md1186", null ],
        [ "Test Modes", "d0/d46/md_test_2README.html#autotoc_md1187", null ],
        [ "Options", "d0/d46/md_test_2README.html#autotoc_md1188", null ]
      ] ],
      [ "E2E ROM Testing", "d0/d46/md_test_2README.html#autotoc_md1189", [
        [ "Test Cases", "d0/d46/md_test_2README.html#autotoc_md1190", null ]
      ] ],
      [ "ZSCustomOverworld Upgrade Testing", "d0/d46/md_test_2README.html#autotoc_md1191", [
        [ "Supported Upgrades", "d0/d46/md_test_2README.html#autotoc_md1192", null ],
        [ "Test Cases", "d0/d46/md_test_2README.html#autotoc_md1193", null ],
        [ "Version-Specific Features", "d0/d46/md_test_2README.html#autotoc_md1194", [
          [ "Vanilla", "d0/d46/md_test_2README.html#autotoc_md1195", null ],
          [ "v2", "d0/d46/md_test_2README.html#autotoc_md1196", null ],
          [ "v3", "d0/d46/md_test_2README.html#autotoc_md1197", null ]
        ] ]
      ] ],
      [ "Environment Variables", "d0/d46/md_test_2README.html#autotoc_md1198", null ],
      [ "CI/CD Integration", "d0/d46/md_test_2README.html#autotoc_md1199", null ],
      [ "Deprecated Tests", "d0/d46/md_test_2README.html#autotoc_md1200", null ],
      [ "Best Practices", "d0/d46/md_test_2README.html#autotoc_md1201", null ],
      [ "AI Agent Testing", "d0/d46/md_test_2README.html#autotoc_md1202", null ]
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
"d0/d20/classyaze_1_1test_1_1E2ETestSuite.html#a90e47318c00bf5d8114d00a810e5283e",
"d0/d28/classTextEditor.html#a92ffe1172d67e7707b0f844a657126b2",
"d0/d59/structyaze_1_1gui_1_1canvas_1_1PerformanceOptions.html",
"d0/da5/classyaze_1_1cli_1_1ai_1_1AIGUIController.html#a202ee64010b221e2356a876d06db5bf9",
"d0/dc7/classyaze_1_1test_1_1DungeonObjectRenderingE2ETests.html#a2c771ec46be6acdb90ea75a9007fe649",
"d0/dff/room__object_8h.html#a93f46a6999be6a8b526030afb9b1d6be",
"d1/d1f/overworld__entrance_8h.html#afe00a37c0e43039237d6f7452d6daca6",
"d1/d3e/namespaceyaze_1_1editor.html#a09e3eaa0655502231991af8156e1fd60",
"d1/d4b/namespaceyaze_1_1gfx_1_1lc__lz2.html#a3917ad6c050f1d29d7dfc1f95751742d",
"d1/d6e/classyaze_1_1cli_1_1EnhancedStatusPanel.html#a760303239a0648b6115fc32d41166cad",
"d1/d95/ui__helpers_8h.html#aabcc7b6b8cb4ce76378dbc2a69cbd365",
"d1/dc4/structyaze_1_1emu_1_1BackgroundLayer.html#ac8d405ff4b82ef249bda766257dec6d0",
"d1/dea/classyaze_1_1gui_1_1EditorCardManager.html#aa94b852a8c559f829ec79c6b4cf0a616",
"d2/d07/classyaze_1_1emu_1_1Spc700.html#a02522223dec18642a4740be6028948f8",
"d2/d07/classyaze_1_1emu_1_1Spc700.html#aeb42192c0d5a149ebb94d394212f33f9",
"d2/d39/classyaze_1_1gui_1_1MultiSelect.html#a2d5be49cc17281465b2be2e8992ba4ea",
"d2/d4e/classyaze_1_1editor_1_1AgentChatHistoryPopup.html#a360d56fb93700882f932a1b456fc841b",
"d2/d60/structyaze_1_1test_1_1TestResults.html#a17c7507df532093fd331b4434c521313",
"d2/dc2/classyaze_1_1gui_1_1BppFormatUI.html#a0630f792705ff6b6ab9c3048c28a0af5",
"d2/de7/classyaze_1_1editor_1_1DungeonCanvasViewer.html#ab011c64bf322ecfa4720189642a21fdc",
"d2/dfe/structyaze_1_1gui_1_1EnhancedTheme.html#a366b8af12039498a6d3bfad36f25dade",
"d3/d0d/shortcut__manager_8cc.html#a2c9e59c481faf2614bea832ae9d19df0",
"d3/d1a/classyaze_1_1gui_1_1BppComparisonTool.html#a1571f3b0654eb7142285d06ea7d01694",
"d3/d30/structyaze_1_1gui_1_1canvas_1_1CanvasPerformanceMetrics.html#a55ed08d0b369b103b9aefefdd6240efa",
"d3/d44/classyaze_1_1editor_1_1OverworldEditor.html#a85c7e85183b05afd17cebf8925da2e7e",
"d3/d4f/classyaze_1_1editor_1_1AgentCollaborationCoordinator.html#a66ad79395a38b40a463f48722dd01a2e",
"d3/d6c/classyaze_1_1editor_1_1MessageEditor.html#a90d85cb6fc78a9bb717e13352fdc5f61",
"d3/d8f/bps_8cc.html",
"d3/da7/structyaze_1_1gfx_1_1lc__lz2_1_1CompressionContext.html#a189481810b400d3f56acebf6533baa1f",
"d3/dbf/canvas__usage__tracker_8h.html#a11c32674d05f23d59e00b23ee6c9c60ca999108d42924fd9a22a58e3f7b068018",
"d3/de4/classyaze_1_1gfx_1_1TileInfo.html#a5061718603d8b28656bcfedcb4727f98",
"d3/ded/classyaze_1_1emu_1_1Ppu.html#accea30561b2035d031d9bb9d5efe13bc",
"d4/d0a/namespaceyaze_1_1test.html#a33e8a96eb05a5da8733a1a7d46656c1e",
"d4/d12/compression__test_8cc.html#af3005defc83190d728d018695f20b0b3",
"d4/d68/structyaze_1_1gui_1_1canvas_1_1BppConversionOptions.html#ac4cf7baa6d3c2a7ba4f6118937f6100b",
"d4/d87/structyaze_1_1emu_1_1WBGLOG.html#aa39d0f8184b4099afb233b5cdf79325d",
"d4/db6/md_docs_2APU__Timing__Fix__Plan.html",
"d4/de1/namespaceyaze_1_1util.html#a70b26eeecc8d2802527d0587241f6f4c",
"d5/d1c/structyaze_1_1gui_1_1SnapshotEntry.html#a55ce07fb2fe15ee24a90aabc0a54cf4f",
"d5/d1f/namespaceyaze_1_1zelda3.html#a693df677b1b2754452c8b8b4c4a98468a2d745a47567cd5d54eb480f3caaca77c",
"d5/d1f/namespaceyaze_1_1zelda3.html#ace5c6ffb4b3d787a38c17e6ce49f45e4",
"d5/d52/snes__color_8h.html#a78da9ee9b2de37e218f0152a1ecc4e36",
"d5/d7e/workspace__manager_8cc_source.html",
"d5/da1/structyaze_1_1cli_1_1ProposalRegistry_1_1ProposalMetadata.html#af632212a1f70aec92ade8dfa144b26fa",
"d5/dce/title__screen_8cc_source.html",
"d6/d04/structyaze_1_1emu_1_1WOBJSEL.html#a037135951fa0322710411b5750d352be",
"d6/d20/namespaceyaze_1_1emu.html#a22bf51ed91189695bf4e76bf6b85f836ad43bb7d5a33aec2376fcec383643161c",
"d6/d2e/classyaze_1_1zelda3_1_1TitleScreen.html#a8d60680de49fc10b0c37725c22ea8347",
"d6/d3c/classyaze_1_1editor_1_1MapPropertiesSystem.html#a2bd278af0fe262342d95735db7fb1a6b",
"d6/d5d/classyaze_1_1cli_1_1net_1_1Z3edNetworkClient_1_1Impl.html#a1db5d54dfbb71b6ca257a3100b1d1391",
"d6/d9f/structyaze_1_1emu_1_1STAT77.html",
"d6/dc5/structyaze_1_1gfx_1_1PerformanceDashboard_1_1PerformanceMetrics.html#a5276be0f5a0f5b404fcfbfcd2a9faf8b",
"d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md833",
"d7/d1c/structyaze_1_1gui_1_1TileSelectorWidget_1_1RenderResult.html#aa2f5c040b82821c1b3ee65ed2fad19c1",
"d7/d51/md_docs_2B3-build-presets.html#autotoc_md117",
"d7/d61/classyaze_1_1zelda3_1_1DungeonEditorSystem.html#aa6649b21ffa38945d2d65499dc646b57",
"d7/d79/namespaceyaze_1_1test_1_1gui.html",
"d7/da7/classyaze_1_1emu_1_1Apu.html#a75b0ccd168858f9abb41b95646bc8be9",
"d7/dd0/screen__editor_8h.html",
"d7/df6/classyaze_1_1zelda3_1_1Room.html#a6f422cb5a4d82765f9aba638e9f647d5",
"d8/d01/structyaze_1_1core_1_1FeatureFlags_1_1Flags_1_1Overworld.html#acd6cf1f6ea1cbae92b37599f037646cc",
"d8/d1e/classyaze_1_1zelda3_1_1RoomEntrance.html#ac0146430de72355794c266520261bc1b",
"d8/d44/classyaze_1_1gfx_1_1BppConversionScope.html#a6617e6a87581b184a84c95f8e5e18270",
"d8/d72/structyaze_1_1emu_1_1Color.html#ad01d1926e6516da6fd95d83abeeef51b",
"d8/d9a/overworld__test_8cc_source.html",
"d8/dd3/namespaceyaze_1_1cli_1_1agent.html#a6a3d696815c6e8d815849cfb27cc7cc7a30437633d552b7229b65b11ec90cb153",
"d8/ddf/classOverworldGoldenDataExtractor.html#a8f53f4d3ab167ad06f11e22d81edc32d",
"d9/d21/structyaze_1_1emu_1_1DmaRegisters.html#afc38aeb6a727eb4c58f3a3353eb1169d",
"d9/d2b/classyaze_1_1editor_1_1AgentChatWidget.html#ae52e9fd4717ad866b5fa502e4ab942a0",
"d9/d6f/classyaze_1_1emu_1_1input_1_1IInputBackend.html#a9e586bc056f107979b29764107f153cf",
"d9/d8e/structyaze_1_1gfx_1_1AtlasRenderer_1_1AtlasEntry.html#a04fc3a1741612654c563fc9c17ab823d",
"d9/dc0/room_8h.html#a16a549dbe81fe1cb9d30e7df88fed890",
"d9/dc5/classyaze_1_1zelda3_1_1Overworld.html#a85b380c53cfe4769b4ba05cda14b3cd8",
"d9/dcc/classyaze_1_1zelda3_1_1OverworldMap.html#a94ef6f3a99a44970586f17992000ca43",
"d9/dd4/structyaze_1_1emu_1_1Spc700_1_1Flags.html#acf45a7295d64b098b27ece69e09595ea",
"d9/dff/ppu__registers_8h.html#a63d1e4343b820f6cfa9ba22f1b34fd38",
"da/d2c/classyaze_1_1gui_1_1Canvas.html#a5d77d64ec508607bcbf4bcaeb876161c",
"da/d30/classyaze_1_1cli_1_1SpriteCreate.html",
"da/d45/classyaze_1_1editor_1_1DungeonEditorV2.html#a83f71940529e7260f54e799aa863e2c9",
"da/d68/classyaze_1_1gfx_1_1SnesPalette.html#a19e74a4e0ebec56b1bb0f7e99acbc067",
"da/dae/classyaze_1_1net_1_1WebSocketClient.html#a0df544422762601c649314b2ba2d4d7a",
"da/dd2/classyaze_1_1core_1_1AsarWrapper.html#ac97d109b5e163e881098215989d86ada",
"db/d00/classyaze_1_1editor_1_1DungeonToolset.html#ac9170fbc1d88691828b594c519f8ad40a0c3ddb3f55a9212fe55c7a0dc72ef872",
"db/d2c/classyaze_1_1gui_1_1AutoWidgetScope.html#a020154c739a2b1b0ec6874f171902e82",
"db/d56/testing_8h.html#ace6494b75531bde42a021a993af2b3e1",
"db/d82/classyaze_1_1editor_1_1Tile16Editor.html#a4833d43e836e1671b9e17a818dee242a",
"db/d9a/classyaze_1_1editor_1_1MusicEditor.html#ac3774c5c4e9ca0f03672261ef385f365",
"db/dbe/classyaze_1_1emu_1_1debug_1_1ApuHandshakeTracker.html#a841d9f5808af79c20e140d8d8f1e2171a735503010c32f026b657ae9e9beb02c9",
"db/de0/structyaze_1_1cli_1_1FewShotExample.html#a98f6fa6c92bbc1150a91b0ba7277b110",
"dc/d0c/structyaze_1_1cli_1_1GeminiConfig.html#a3fabd23ee9039b82a0661b85c349e454",
"dc/d31/classyaze_1_1editor_1_1GraphicsEditor.html#a1820199f3fc06cf01e8cf608e60ab460",
"dc/d40/structyaze_1_1zelda3_1_1DungeonEditorSystem_1_1ChestData.html#a378fe56682f9b0e74bc265af0e085f6c",
"dc/d55/classyaze_1_1gfx_1_1MemoryPool.html#a80d1d1a5570f5aafe456f6d161adb6a8",
"dc/da3/structyaze_1_1cli_1_1agent_1_1AdvancedRouter_1_1RouteContext.html#ab04491b7f34a14deecfe33021162db2e",
"dc/de0/classyaze_1_1test_1_1ArenaTestSuite.html#a90f793b6f48683794289c3d8a98788cf",
"dc/df4/classyaze_1_1emu_1_1Cpu.html#a61614dc70bbd13d0be02bd45155826e1",
"dc/df4/classyaze_1_1emu_1_1Cpu.html#ae6fa6c1c9b120d33deb5d3d027a43fbc",
"dd/d12/classyaze_1_1editor_1_1EditorManager.html#a36209b8a5b17d613060c3d06f0359ad8",
"dd/d1b/websocket__client_8h.html#a7d076fc70bcff0085dcac770ae381cb2ab714c11518d545c225d456731dab0dd0",
"dd/d4b/classyaze_1_1editor_1_1DungeonUsageTracker.html#a1b59b2eb11189c1344fff906c8317437",
"dd/d71/classyaze_1_1gui_1_1canvas_1_1CanvasInteractionHandler.html",
"dd/db7/structyaze_1_1emu_1_1BgLayer.html#aac08a08df9df7047ecbf6548cccb63ae",
"dd/ddf/classyaze_1_1gui_1_1canvas_1_1CanvasContextMenu.html#aaaf9e44993de729b84029f9ac9aa1d26",
"dd/dfb/structyaze_1_1editor_1_1AgentChatWidget_1_1ScreenshotPreviewState.html#a21621f309bbb73428082d3b8d0d1b3e2",
"de/d0f/classyaze_1_1emu_1_1MemoryImpl.html#a80cc6145d28f82fae451e8b2ae13983a",
"de/d5d/overworld__golden__data__extractor_8cc.html",
"de/d77/structyaze_1_1cli_1_1TestSuiteConfig.html#a992e3ec679746b2f15924a8b6fc257ee",
"de/d9a/classyaze_1_1gfx_1_1TextureAtlas.html#afdd5235ab631cb775016fdaa5a562fb5",
"de/dbf/icons_8h.html#a0745b7f1e35b26e78971b31ed76d99c6",
"de/dbf/icons_8h.html#a269462795526fce3220390ed8a8595ff",
"de/dbf/icons_8h.html#a441f6c163e2e40cf96e0a33795ce83e7",
"de/dbf/icons_8h.html#a6121e69ee22aadb889cdf0716d7d0b1e",
"de/dbf/icons_8h.html#a80c62595f0b5d3189923450fbdd3c221",
"de/dbf/icons_8h.html#aa08008c335af19a5bb31a48929168e07",
"de/dbf/icons_8h.html#abc8ede223cc1fe2a28d17d91fd46fb3c",
"de/dbf/icons_8h.html#ad891ac88aa2b6a3889826d4e8f79da1e",
"de/dbf/icons_8h.html#af3306c538757ef855c59136870d87659",
"de/dd6/classyaze_1_1test_1_1TestRomManager_1_1BoundRomTest.html#a69bdb2cfed1c3c9ce2f73719b1752db0",
"de/dea/background__renderer_8cc.html",
"df/d19/namespaceyaze_1_1zelda3_1_1anonymous__namespace_02room__object__encoding__test_8cc_03.html#ab6fef3af8c9484a1c91c42c5ecbcc42c",
"df/d7d/dma_8cc.html#a2f5e5518d3fd67ea11625388d6d6c60c",
"df/db9/classyaze_1_1cli_1_1ModernCLI.html#a96e2acc639781b4069f7febea531ab6b",
"df/ded/terminal__colors_8h.html#ac8296623e8139f6fe12761ee43592429",
"functions_vars_x.html"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';