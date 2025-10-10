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
    [ "YAZE Dungeon Editor - Complete Technical Guide", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html", [
      [ "Table of Contents", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md226", null ],
      [ "Overview", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md228", [
        [ "Key Features", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md229", null ],
        [ "Architecture Principles", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md230", null ]
      ] ],
      [ "Critical Bugs Fixed", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md232", [
        [ "Bug #1: Segfault on Startup ✅", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md233", null ],
        [ "Bug #2: Floor Values Always Zero ✅", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md234", null ],
        [ "Bug #3: Duplicate Floor Variables ✅", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md235", null ],
        [ "Bug #4: ObjectRenderer Confusion ✅", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md236", null ],
        [ "Bug #5: Duplicate Property Detection ✅", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md237", null ]
      ] ],
      [ "Architecture", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md239", [
        [ "Component Overview", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md240", null ],
        [ "Room Rendering Pipeline", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md241", null ],
        [ "Key Insight: Object Rendering", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md242", null ]
      ] ],
      [ "Quick Start", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md244", [
        [ "Build", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md245", null ],
        [ "Run", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md246", null ],
        [ "Expected Visuals", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md247", null ]
      ] ],
      [ "Object Visualization", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md249", [
        [ "DrawObjectPositionOutlines()", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md250", null ]
      ] ],
      [ "Testing", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md252", [
        [ "Debug Commands", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md253", null ],
        [ "Expected Debug Output", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md254", null ],
        [ "Visual Checklist", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md255", null ]
      ] ],
      [ "Troubleshooting", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md257", [
        [ "Floor tiles still blank/wrong?", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md258", null ],
        [ "Objects not visible?", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md259", null ],
        [ "Floor editing doesn't work?", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md260", null ],
        [ "Performance degradation with multiple rooms?", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md261", null ]
      ] ],
      [ "Future Work", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md263", [
        [ "High Priority", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md264", null ],
        [ "Medium Priority", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md265", null ],
        [ "Low Priority", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md266", null ]
      ] ],
      [ "Code Reference", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md268", [
        [ "Loading Order (CRITICAL)", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md269", null ],
        [ "Floor Accessors", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md270", null ],
        [ "Object Visualization", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md271", null ]
      ] ],
      [ "Session Summary", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md273", [
        [ "What Was Accomplished", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md274", null ],
        [ "Files Modified", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md275", null ],
        [ "Statistics", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md276", null ]
      ] ],
      [ "User Decisions", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md278", null ],
      [ "Next Steps", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md280", [
        [ "Immediate", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md281", null ],
        [ "Short-term", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md282", null ],
        [ "Future", "d6/d84/md_docs_2DUNGEON__EDITOR__COMPLETE__GUIDE.html#autotoc_md283", null ]
      ] ]
    ] ],
    [ "YAZE Dungeon Editor: Complete Guide", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html", [
      [ "Table of Contents", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md287", null ],
      [ "Overview", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md289", [
        [ "Key Capabilities", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md290", null ]
      ] ],
      [ "Current Status", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md292", [
        [ "✅ Production Ready Features", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md293", null ],
        [ "🔧 Recently Fixed Issues", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md294", null ]
      ] ],
      [ "Architecture", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md296", [
        [ "Component Hierarchy", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md297", null ],
        [ "Card-Based Architecture Benefits", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md298", null ]
      ] ],
      [ "Quick Start", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md300", [
        [ "Launch from Command Line", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md301", null ],
        [ "From GUI", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md302", null ]
      ] ],
      [ "Core Features", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md304", [
        [ "1. Rooms List Card", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md305", null ],
        [ "2. Room Matrix Card (Visual Navigation)", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md306", null ],
        [ "3. Entrances List Card", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md307", null ],
        [ "4. Object Editor Card (Unified)", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md308", null ],
        [ "5. Palette Editor Card", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md309", null ],
        [ "6. Room Cards (Auto-Loading)", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md310", null ],
        [ "7. Canvas Context Menu (NEW)", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md311", null ]
      ] ],
      [ "Technical Details", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md313", [
        [ "Rendering Pipeline", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md314", null ],
        [ "Critical Fix: Texture Update After Object Rendering", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md315", null ],
        [ "SNES Graphics Format", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md316", null ],
        [ "Critical Math Formulas", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md317", null ],
        [ "Per-Room Buffers", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md318", null ]
      ] ],
      [ "Testing", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md320", [
        [ "Test Suite Status", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md321", null ],
        [ "Running Tests", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md322", null ],
        [ "Test Speed Modes (NEW)", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md323", null ]
      ] ],
      [ "Troubleshooting", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md325", [
        [ "Common Issues & Fixes", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md326", [
          [ "Issue 1: Objects Not Visible", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md327", null ],
          [ "Issue 2: Wrong Colors", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md328", null ],
          [ "Issue 3: Bitmap Stretched/Corrupted", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md329", null ],
          [ "Issue 4: Room Properties Don't Update", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md330", null ]
        ] ]
      ] ],
      [ "ROM Internals", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md332", [
        [ "Object Encoding", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md333", [
          [ "Type 1: Standard Objects (ID 0x00-0xFF)", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md334", null ],
          [ "Type 2: Large Coordinate Objects (ID 0x100-0x1FF)", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md335", null ],
          [ "Type 3: Special Objects (ID 0x200-0x27F)", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md336", null ]
        ] ],
        [ "Core Data Tables in ROM", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md337", null ],
        [ "Key ROM Addresses", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md338", null ]
      ] ],
      [ "Reference", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md340", [
        [ "File Organization", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md341", null ],
        [ "Quick Reference: Key Functions", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md342", null ],
        [ "Performance Metrics", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md343", null ]
      ] ],
      [ "Summary", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md345", [
        [ "Critical Points", "d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md346", null ]
      ] ]
    ] ],
    [ "Asm Style Guide", "d7/d9a/md_docs_2E1-asm-style-guide.html", [
      [ "Table of Contents", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md349", null ],
      [ "File Structure", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md350", null ],
      [ "Labels and Symbols", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md351", null ],
      [ "Comments", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md352", null ],
      [ "Instructions", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md353", null ],
      [ "Macros", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md354", null ],
      [ "Loops and Branching", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md355", null ],
      [ "Data Structures", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md356", null ],
      [ "Code Organization", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md357", null ],
      [ "Custom Code", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md358", null ]
    ] ],
    [ "YAZE Emulator Enhancement Roadmap", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html", [
      [ "📋 Executive Summary", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md361", [
        [ "Core Objectives", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md362", null ]
      ] ],
      [ "🎯 Current State Analysis", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md364", [
        [ "What Works ✅", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md365", null ],
        [ "What's Broken ❌", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md366", null ],
        [ "What's Missing 🚧", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md367", null ]
      ] ],
      [ "🔧 Phase 1: Audio System Fix (Priority: CRITICAL)", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md369", [
        [ "Problem Analysis", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md370", null ],
        [ "Investigation Steps", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md371", null ],
        [ "Likely Fixes", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md372", null ],
        [ "Quick Win Actions", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md373", null ]
      ] ],
      [ "🐛 Phase 2: Advanced Debugger (Mesen2 Feature Parity)", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md375", [
        [ "Feature Comparison: YAZE vs Mesen2", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md376", null ],
        [ "2.1 Breakpoint System", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md377", null ],
        [ "2.2 Memory Watchpoints", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md379", null ],
        [ "2.3 Live Disassembly Viewer", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md381", null ],
        [ "2.4 Enhanced Memory Viewer", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md383", null ]
      ] ],
      [ "🚀 Phase 3: Performance Optimizations", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md385", [
        [ "3.1 Cycle-Accurate Timing", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md386", null ],
        [ "3.2 Dynamic Recompilation (Dynarec)", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md388", null ],
        [ "3.3 Frame Pacing Improvements", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md390", null ]
      ] ],
      [ "🎮 Phase 4: SPC700 Audio CPU Debugger", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md392", [
        [ "4.1 APU Inspector Window", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md393", null ],
        [ "4.2 Audio Sample Export", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md395", null ]
      ] ],
      [ "🤖 Phase 5: z3ed AI Agent Integration", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md397", [
        [ "5.1 Emulator State Access", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md398", null ],
        [ "5.2 Automated Test Generation", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md400", null ],
        [ "5.3 Memory Map Learning", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md402", null ]
      ] ],
      [ "📊 Phase 6: Performance Profiling", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md404", [
        [ "6.1 Cycle Counter & Profiler", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md405", null ],
        [ "6.2 Frame Time Analysis", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md407", null ]
      ] ],
      [ "🎯 Phase 7: Event System & Timeline", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md409", [
        [ "7.1 Event Logger", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md410", null ]
      ] ],
      [ "🧠 Phase 8: AI-Powered Debugging", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md412", [
        [ "8.1 Intelligent Crash Analysis", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md413", null ],
        [ "8.2 Automated Bug Reproduction", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md415", null ]
      ] ],
      [ "🗺️ Implementation Roadmap", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md417", [
        [ "Sprint 1: Audio Fix (Week 1)", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md418", null ],
        [ "Sprint 2: Basic Debugger (Weeks 2-3)", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md419", null ],
        [ "Sprint 3: SPC700 Debugger (Week 4)", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md420", null ],
        [ "Sprint 4: AI Integration (Weeks 5-6)", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md421", null ],
        [ "Sprint 5: Performance (Weeks 7-8)", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md422", null ]
      ] ],
      [ "🔬 Technical Deep Dives", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md424", [
        [ "Audio System Architecture (SDL2)", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md425", null ],
        [ "Memory Regions Reference", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md427", null ]
      ] ],
      [ "🎮 Phase 9: Advanced Features (Mesen2 Parity)", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md429", [
        [ "9.1 Rewind Feature", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md430", null ],
        [ "9.2 TAS (Tool-Assisted Speedrun) Input Recording", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md432", null ],
        [ "9.3 Comparison Mode", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md434", null ]
      ] ],
      [ "🛠️ Optimization Summary", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md436", [
        [ "Quick Wins (< 1 week)", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md437", null ],
        [ "Medium Term (1-2 months)", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md438", null ],
        [ "Long Term (3-6 months)", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md439", null ]
      ] ],
      [ "🤖 z3ed Agent Emulator Tools", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md441", [
        [ "New Tool Categories", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md442", null ],
        [ "Example AI Conversations", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md443", null ]
      ] ],
      [ "📁 File Structure for New Features", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md445", null ],
      [ "🎨 UI Mockups", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md447", [
        [ "Debugger Layout (ImGui)", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md448", null ],
        [ "APU Debugger Layout", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md449", null ]
      ] ],
      [ "🚀 Performance Targets", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md451", [
        [ "Current Performance", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md452", null ],
        [ "Target Performance (Post-Optimization)", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md453", null ],
        [ "Optimization Strategy Priority", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md454", null ]
      ] ],
      [ "🧪 Testing Integration", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md456", [
        [ "Automated Emulator Tests (z3ed)", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md457", null ]
      ] ],
      [ "🔌 z3ed Agent + Emulator Integration", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md459", [
        [ "New Agent Tools", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md460", null ]
      ] ],
      [ "🎓 Learning from Mesen2", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md462", [
        [ "What Makes Mesen2 Great", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md463", null ],
        [ "Our Unique Advantages", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md464", null ]
      ] ],
      [ "📊 Resource Requirements", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md466", [
        [ "Development Time Estimates", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md467", null ],
        [ "Memory Requirements", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md468", null ],
        [ "CPU Requirements", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md469", null ]
      ] ],
      [ "🛣️ Recommended Implementation Order", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md471", [
        [ "Month 1: Foundation", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md472", null ],
        [ "Month 2: Audio & Events", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md473", null ],
        [ "Month 3: AI Integration", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md474", null ],
        [ "Month 4: Performance", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md475", null ],
        [ "Month 5: Polish", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md476", null ]
      ] ],
      [ "🔮 Future Vision: AI-Powered ROM Hacking", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md478", [
        [ "The Ultimate Workflow", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md479", null ]
      ] ],
      [ "🐛 Appendix A: Audio Debugging Checklist", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md481", [
        [ "Check 1: Device Status", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md482", null ],
        [ "Check 2: Queue Size", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md483", null ],
        [ "Check 3: Sample Validation", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md484", null ],
        [ "Check 4: Buffer Allocation", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md485", null ],
        [ "Check 5: SPC700 Execution", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md486", null ],
        [ "Quick Fixes to Try", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md487", null ]
      ] ],
      [ "📝 Appendix B: Mesen2 Feature Reference", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md489", [
        [ "Debugger Windows (Inspiration)", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md490", null ],
        [ "Event Types Tracked", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md491", null ],
        [ "Trace Logger Format", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md492", null ]
      ] ],
      [ "🎯 Success Criteria", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md494", [
        [ "Phase 1 Complete When:", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md495", null ],
        [ "Phase 2 Complete When:", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md496", null ],
        [ "Phase 3 Complete When:", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md497", null ],
        [ "Phase 4 Complete When:", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md498", null ],
        [ "Phase 5 Complete When:", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md499", null ]
      ] ],
      [ "🎓 Learning Resources", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md501", [
        [ "SNES Emulation", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md502", null ],
        [ "Audio Debugging", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md503", null ],
        [ "Performance Optimization", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md504", null ]
      ] ],
      [ "🙏 Credits & Acknowledgments", "de/d77/md_docs_2E1-emulator-enhancement-roadmap.html#autotoc_md506", null ]
    ] ],
    [ "E2 - Development Guide", "d5/d18/md_docs_2E2-development-guide.html", [
      [ "Editor Status", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md509", null ],
      [ "1. Core Architectural Patterns", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md510", [
        [ "Pattern 1: Modular Systems", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md511", null ],
        [ "Pattern 2: Callback-Based Communication", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md512", null ],
        [ "Pattern 3: Centralized Progressive Loading via <tt>gfx::Arena</tt>", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md513", null ]
      ] ],
      [ "2. UI & Theming System", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md514", [
        [ "2.1. The Theme System (<tt>AgentUITheme</tt>)", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md515", null ],
        [ "2.2. Reusable UI Helper Functions", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md516", null ],
        [ "2.3. Toolbar Implementation (<tt>CompactToolbar</tt>)", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md517", null ]
      ] ],
      [ "3. Key System Implementations & Gotchas", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md518", [
        [ "3.1. Graphics Refresh Logic", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md519", null ],
        [ "3.2. Multi-Area Map Configuration", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md520", null ],
        [ "3.3. Version-Specific Feature Gating", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md521", null ],
        [ "3.4. Entity Visibility for Visual Testing", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md522", null ]
      ] ],
      [ "4. Debugging and Testing", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md523", [
        [ "4.1. Quick Debugging with Startup Flags", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md524", null ],
        [ "4.2. Testing Strategies", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md525", null ],
        [ "3.6. Graphics Sheet Management", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md526", null ],
        [ "Naming Conventions", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md527", null ]
      ] ]
    ] ],
    [ "API Reference", "d8/d73/md_docs_2E3-api-reference.html", [
      [ "C API (<tt>incl/yaze.h</tt>)", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md529", [
        [ "Core Library Functions", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md530", null ],
        [ "ROM Operations", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md531", null ]
      ] ],
      [ "C++ API", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md532", [
        [ "AsarWrapper (<tt>src/app/core/asar_wrapper.h</tt>)", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md533", [
          [ "CLI Examples (<tt>z3ed</tt>)", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md534", null ],
          [ "C++ API Example", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md535", null ],
          [ "Class Definition", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md536", null ]
        ] ]
      ] ],
      [ "Data Structures", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md537", [
        [ "<tt>snes_color</tt>", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md538", null ],
        [ "<tt>zelda3_message</tt>", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md539", null ]
      ] ],
      [ "Error Handling", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md540", [
        [ "C API Error Pattern", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md541", null ]
      ] ]
    ] ],
    [ "E4 - Emulator Development Guide", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html", [
      [ "Table of Contents", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md544", null ],
      [ "1. Current Status", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md546", [
        [ "🎉 Major Breakthrough: Game is Running!", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md547", null ],
        [ "✅ Confirmed Working", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md548", null ],
        [ "🔧 Known Issues (Non-Critical)", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md549", null ]
      ] ],
      [ "2. How to Use the Emulator", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md551", [
        [ "Method 1: Main Yaze Application (GUI)", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md552", null ],
        [ "Method 2: Standalone Emulator (<tt>yaze_emu</tt>)", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md553", null ],
        [ "Method 3: Dungeon Object Emulator Preview", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md554", null ]
      ] ],
      [ "3. Architecture", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md556", [
        [ "Memory System", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md557", [
          [ "SNES Memory Map", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md558", null ]
        ] ],
        [ "CPU-APU-SPC700 Interaction", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md559", null ],
        [ "Component Architecture", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md560", null ]
      ] ],
      [ "4. The Debugging Journey: Critical Fixes", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md562", [
        [ "SPC700 & APU Fixes", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md563", null ],
        [ "The Critical Pattern for Multi-Step Instructions", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md564", null ]
      ] ],
      [ "5. Display & Performance Improvements", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md566", [
        [ "PPU Color Display Fix", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md567", null ],
        [ "Frame Timing & Speed Control", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md568", null ],
        [ "Performance Optimizations", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md569", [
          [ "Frame Skipping", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md570", null ],
          [ "Audio Buffer Management", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md571", null ],
          [ "Performance Gains", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md572", null ]
        ] ],
        [ "ROM Loading Improvements", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md573", null ]
      ] ],
      [ "6. Advanced Features", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md575", [
        [ "Professional Disassembly Viewer", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md576", [
          [ "Architecture", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md577", null ],
          [ "Visual Features", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md578", null ],
          [ "Interactive Elements", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md579", null ],
          [ "UI Features", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md580", null ],
          [ "Performance", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md581", null ]
        ] ],
        [ "Breakpoint System", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md582", null ],
        [ "UI/UX Enhancements", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md583", null ]
      ] ],
      [ "7. Emulator Preview Tool", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md585", [
        [ "Purpose", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md586", null ],
        [ "Critical Fixes Applied", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md587", [
          [ "1. Memory Access Fix (SIGSEGV Crash)", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md588", null ],
          [ "2. RTL vs RTS Fix (Timeout)", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md589", null ],
          [ "3. Palette Validation", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md590", null ],
          [ "4. PPU Configuration", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md591", null ]
        ] ],
        [ "How to Use", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md592", null ],
        [ "What You'll Learn", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md593", null ],
        [ "Reverse Engineering Workflow", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md594", null ],
        [ "UI Enhancements", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md595", null ]
      ] ],
      [ "8. Logging System", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md597", [
        [ "How to Enable", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md598", null ]
      ] ],
      [ "9. Testing", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md600", [
        [ "Unit Tests", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md601", null ],
        [ "Standalone Emulator", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md602", null ],
        [ "Running Tests", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md603", null ],
        [ "Testing Checklist", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md604", null ]
      ] ],
      [ "10. Technical Reference", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md606", [
        [ "PPU Registers", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md607", null ],
        [ "CPU Instructions", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md608", null ],
        [ "Color Format", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md609", null ],
        [ "Performance Metrics", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md610", null ]
      ] ],
      [ "11. Troubleshooting", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md612", [
        [ "Emulator Preview Issues", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md613", null ],
        [ "Color Display Issues", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md614", null ],
        [ "Performance Issues", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md615", null ],
        [ "Build Issues", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md616", null ]
      ] ],
      [ "11.5 Audio System Architecture (October 2025)", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md618", [
        [ "Overview", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md619", null ],
        [ "Audio Backend Abstraction", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md620", null ],
        [ "APU Handshake Debugging System", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md621", null ],
        [ "IPL ROM Handshake Protocol", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md622", null ],
        [ "Music Editor Integration", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md623", null ],
        [ "Audio Testing & Diagnostics", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md624", null ],
        [ "Future Enhancements", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md625", null ]
      ] ],
      [ "12. Next Steps & Roadmap", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md627", [
        [ "🎯 Immediate Priorities (Critical Path to Full Functionality)", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md628", null ],
        [ "🚀 Enhancement Priorities (After Core is Stable)", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md629", null ],
        [ "📝 Technical Debt", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md630", null ],
        [ "Long-Term Enhancements", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md631", null ]
      ] ],
      [ "13. Build Instructions", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md633", [
        [ "Quick Build", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md634", null ],
        [ "Platform-Specific", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md635", null ],
        [ "Build Optimizations", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md636", null ]
      ] ],
      [ "File Reference", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md638", [
        [ "Core Emulation", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md639", null ],
        [ "Debugging", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md640", null ],
        [ "UI", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md641", null ],
        [ "Core", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md642", null ],
        [ "Testing", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md643", null ]
      ] ],
      [ "Status Summary", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md645", [
        [ "✅ Production Ready", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md646", null ]
      ] ]
    ] ],
    [ "E5 - Debugging and Testing Guide", "de/dc5/md_docs_2E5-debugging-guide.html", [
      [ "1. Standardized Logging for Print Debugging", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md649", [
        [ "Log Levels and Usage", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md650", null ],
        [ "Log Categories", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md651", null ],
        [ "Enabling and Configuring Logs via CLI", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md652", null ],
        [ "2. Command-Line Workflows for Testing", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md655", [
          [ "Launching the GUI for Specific Tasks", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md656", null ]
        ] ]
      ] ],
      [ "Open the Dungeon Editor with the Room Matrix and two specific room cards", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md657", null ],
      [ "Available editors: Assembly, Dungeon, Graphics, Music, Overworld, Palette,", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md658", null ],
      [ "Screen, Sprite, Message, Hex, Agent, Settings", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md659", null ],
      [ "Dungeon editor cards: Rooms List, Room Matrix, Entrances List, Room Graphics,", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md660", null ],
      [ "Object Editor, Palette Editor, Room N (where N is room ID)", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md661", null ],
      [ "Fast dungeon room testing", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md662", null ],
      [ "Compare multiple rooms side-by-side", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md663", null ],
      [ "Full dungeon workspace with all tools", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md664", null ],
      [ "Jump straight to overworld editing", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md665", null ],
      [ "Run only fast, dependency-free unit tests", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md667", null ],
      [ "Run tests that require a ROM file", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md668", [
        [ "3. GUI Automation for AI Agents", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md672", [
          [ "Inspecting ROMs with <tt>z3ed</tt>", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md670", null ],
          [ "Architecture Overview", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md673", null ],
          [ "Step-by-Step Workflow for AI", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md674", [
            [ "Step 1: Launch <tt>yaze</tt> with the Test Harness", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md675", null ],
            [ "Step 2: Discover UI Elements", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md676", null ],
            [ "Step 3: Record or Write a Test Script", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md677", null ]
          ] ]
        ] ]
      ] ],
      [ "Start yaze with the room already open", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md678", null ],
      [ "Then your test script just needs to validate the state", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md679", [
        [ "4. Advanced Debugging Tools", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md682", null ]
      ] ]
    ] ],
    [ "Emulator Core Improvements & Optimization Roadmap", "d0/d63/md_docs_2Emulator__Improvements__Roadmap.html", [
      [ "1. Introduction", "d0/d63/md_docs_2Emulator__Improvements__Roadmap.html#autotoc_md684", null ],
      [ "2. Core Architecture & Timing Model (High Priority)", "d0/d63/md_docs_2Emulator__Improvements__Roadmap.html#autotoc_md685", null ],
      [ "3. PPU (Video Rendering) Performance", "d0/d63/md_docs_2Emulator__Improvements__Roadmap.html#autotoc_md686", null ],
      [ "4. APU (Audio) Code Quality & Refinements", "d0/d63/md_docs_2Emulator__Improvements__Roadmap.html#autotoc_md687", null ],
      [ "5. Audio Subsystem & Buffering", "d0/d63/md_docs_2Emulator__Improvements__Roadmap.html#autotoc_md688", null ],
      [ "6. Debugger & Tooling Optimizations (Lower Priority)", "d0/d63/md_docs_2Emulator__Improvements__Roadmap.html#autotoc_md689", null ]
    ] ],
    [ "Tile16 Editor Palette System", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html", [
      [ "Executive Summary", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md691", null ],
      [ "Problem Analysis", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md692", [
        [ "Critical Issues Identified", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md693", null ]
      ] ],
      [ "Solution Architecture", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md694", [
        [ "Core Design Principles", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md695", null ],
        [ "256-Color Overworld Palette Structure", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md696", null ],
        [ "Sheet-to-Palette Mapping", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md697", null ],
        [ "Palette Button Mapping", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md698", null ]
      ] ],
      [ "Implementation Details", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md699", [
        [ "1. Fixed SetPaletteWithTransparent Method", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md700", null ],
        [ "2. Corrected Tile16 Editor Palette System", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md701", null ],
        [ "3. Palette Coordination Flow", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md702", null ]
      ] ],
      [ "UI/UX Refactoring", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md703", [
        [ "New Three-Column Layout", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md704", null ],
        [ "Canvas Context Menu Fixes", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md705", null ],
        [ "Dynamic Zoom Controls", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md706", null ]
      ] ],
      [ "Testing Protocol", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md707", [
        [ "Crash Prevention Testing", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md708", null ],
        [ "Color Alignment Testing", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md709", null ],
        [ "UI/UX Testing", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md710", null ]
      ] ],
      [ "Error Handling", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md711", [
        [ "Bounds Checking", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md712", null ],
        [ "Fallback Mechanisms", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md713", null ]
      ] ],
      [ "Debug Information Display", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md714", null ],
      [ "Known Issues and Ongoing Work", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md715", [
        [ "Completed Items ✅", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md716", null ],
        [ "Active Issues ⚠️", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md717", null ],
        [ "Current Status Summary", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md718", null ],
        [ "Future Enhancements", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md719", null ]
      ] ],
      [ "Maintenance Notes", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md720", null ],
      [ "Next Steps", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md722", [
        [ "Immediate Priorities", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md723", null ],
        [ "Investigation Areas", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md724", null ]
      ] ]
    ] ],
    [ "Overworld Loading Guide", "d9/d85/md_docs_2F3-overworld-loading.html", [
      [ "Table of Contents", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md727", null ],
      [ "Overview", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md728", null ],
      [ "ROM Types and Versions", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md729", [
        [ "Version Detection", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md730", null ],
        [ "Feature Support by Version", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md731", null ]
      ] ],
      [ "Overworld Map Structure", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md732", [
        [ "Core Properties", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md733", null ]
      ] ],
      [ "Overlays and Special Area Maps", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md734", [
        [ "Understanding Overlays", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md735", null ],
        [ "Special Area Maps (0x80-0x9F)", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md736", null ],
        [ "Overlay ID Mappings", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md737", null ],
        [ "Drawing Order", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md738", null ],
        [ "Vanilla Overlay Loading", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md739", null ],
        [ "Special Area Graphics Loading", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md740", null ]
      ] ],
      [ "Loading Process", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md741", [
        [ "1. Version Detection", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md742", null ],
        [ "2. Map Initialization", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md743", null ],
        [ "3. Property Loading", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md744", [
          [ "Vanilla ROMs (asm_version == 0xFF)", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md745", null ],
          [ "ZSCustomOverworld v2/v3", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md746", null ]
        ] ],
        [ "4. Custom Data Loading", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md747", null ]
      ] ],
      [ "ZScream Implementation", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md748", [
        [ "OverworldMap Constructor", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md749", null ],
        [ "Key Methods", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md750", null ]
      ] ],
      [ "Yaze Implementation", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md751", [
        [ "OverworldMap Constructor", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md752", null ],
        [ "Key Methods", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md753", null ],
        [ "Current Status", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md754", null ]
      ] ],
      [ "Key Differences", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md755", [
        [ "1. Language and Architecture", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md756", null ],
        [ "2. Data Structures", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md757", null ],
        [ "3. Error Handling", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md758", null ],
        [ "4. Graphics Processing", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md759", null ]
      ] ],
      [ "Common Issues and Solutions", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md760", [
        [ "1. Version Detection Issues", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md761", null ],
        [ "2. Palette Loading Errors", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md762", null ],
        [ "3. Graphics Not Loading", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md763", null ],
        [ "4. Overlay Issues", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md764", null ],
        [ "5. Large Map Problems", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md765", null ],
        [ "6. Special Area Graphics Issues", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md766", null ]
      ] ],
      [ "Best Practices", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md767", [
        [ "1. Version-Specific Code", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md768", null ],
        [ "2. Error Handling", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md769", null ],
        [ "3. Memory Management", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md770", null ],
        [ "4. Thread Safety", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md771", null ]
      ] ],
      [ "Conclusion", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md772", null ]
    ] ],
    [ "Overworld Agent Guide - AI-Powered Overworld Editing", "de/d00/md_docs_2F4-overworld-agent-guide.html", [
      [ "Overview", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md775", null ],
      [ "Quick Start", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md777", [
        [ "Prerequisites", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md778", null ],
        [ "First Agent Interaction", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md779", null ]
      ] ],
      [ "Available Tools", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md781", [
        [ "Read-Only Tools (Safe for AI)", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md782", [
          [ "overworld-get-tile", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md783", null ],
          [ "overworld-get-visible-region", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md785", null ],
          [ "overworld-analyze-region", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md787", null ]
        ] ],
        [ "Write Tools (Sandboxed - Creates Proposals)", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md789", [
          [ "overworld-set-tile", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md790", null ],
          [ "overworld-set-tiles-batch", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md792", null ]
        ] ]
      ] ],
      [ "Multimodal Vision Workflow", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md794", [
        [ "Step 1: Capture Canvas Screenshot", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md795", null ],
        [ "Step 2: AI Analyzes Screenshot", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md796", null ],
        [ "Step 3: Generate Edit Plan", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md797", null ],
        [ "Step 4: Execute Plan (Sandbox)", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md798", null ],
        [ "Step 5: Human Review", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md799", null ]
      ] ],
      [ "Example Workflows", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md801", [
        [ "Workflow 1: Create Forest Area", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md802", null ],
        [ "Workflow 2: Fix Tile Placement Errors", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md804", null ],
        [ "Workflow 3: Generate Path", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md806", null ]
      ] ],
      [ "Common Tile IDs Reference", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md808", [
        [ "Grass & Ground", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md809", null ],
        [ "Trees & Plants", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md810", null ],
        [ "Water", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md811", null ],
        [ "Paths & Roads", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md812", null ],
        [ "Structures", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md813", null ]
      ] ],
      [ "Best Practices for AI Agents", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md815", [
        [ "1. Always Analyze Before Editing", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md816", null ],
        [ "2. Use Batch Operations", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md817", null ],
        [ "3. Provide Clear Reasoning", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md818", null ],
        [ "4. Respect Tile Boundaries", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md819", null ],
        [ "5. Check Visibility", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md820", null ],
        [ "6. Create Reversible Edits", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md821", null ]
      ] ],
      [ "Error Handling", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md823", [
        [ "\"Tile ID out of range\"", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md824", null ],
        [ "\"Coordinates out of bounds\"", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md825", null ],
        [ "\"Proposal rejected\"", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md826", null ],
        [ "\"ROM file locked\"", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md827", null ]
      ] ],
      [ "Testing AI-Generated Edits", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md829", [
        [ "Manual Testing", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md830", null ],
        [ "Automated Testing", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md831", null ]
      ] ],
      [ "Advanced Techniques", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md833", [
        [ "Technique 1: Pattern Recognition", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md834", null ],
        [ "Technique 2: Style Transfer", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md835", null ],
        [ "Technique 3: Procedural Generation", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md836", null ]
      ] ],
      [ "Integration with GUI Automation", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md838", [
        [ "Record Human Edits", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md839", null ],
        [ "Replay for AI Training", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md840", null ],
        [ "Validate AI Edits", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md841", null ]
      ] ],
      [ "Collaboration Features", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md843", [
        [ "Network Collaboration", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md844", null ],
        [ "Proposal Voting", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md845", null ]
      ] ],
      [ "Troubleshooting", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md847", [
        [ "Agent Not Responding", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md848", null ],
        [ "Tools Not Available", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md849", null ],
        [ "gRPC Connection Failed", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md850", null ]
      ] ],
      [ "See Also", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md852", null ]
    ] ],
    [ "G1 - Canvas System and Automation", "d1/dc6/md_docs_2G1-canvas-guide.html", [
      [ "1. Core Concepts", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md854", [
        [ "Canvas Structure", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md855", null ],
        [ "Coordinate Systems", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md856", null ]
      ] ],
      [ "2. Canvas API and Usage", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md857", [
        [ "Modern Begin/End Pattern", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md858", null ],
        [ "Feature: Tile Painting", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md859", null ],
        [ "Feature: Tile Selection", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md860", null ],
        [ "Feature: Large Map Support", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md861", null ],
        [ "Feature: Context Menu", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md862", null ]
      ] ],
      [ "3. Canvas Automation API", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md863", [
        [ "Accessing the API", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md864", null ],
        [ "Tile Operations", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md865", null ],
        [ "Selection Operations", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md866", null ],
        [ "View Operations", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md867", null ],
        [ "Query Operations", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md868", null ],
        [ "Simulation Operations (for GUI Automation)", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md869", null ]
      ] ],
      [ "4. <tt>z3ed</tt> CLI Integration", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md870", null ],
      [ "5. gRPC Service", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md871", null ]
    ] ],
    [ "SDL2 to SDL3 Migration and Rendering Abstraction Plan", "d6/df2/md_docs_2G2-renderer-migration-plan.html", [
      [ "1. Introduction", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md873", null ],
      [ "2. Current State Analysis", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md874", null ],
      [ "3. Proposed Architecture: The <tt>Renderer</tt> Abstraction", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md875", [
        [ "3.1. The <tt>IRenderer</tt> Interface", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md876", null ],
        [ "3.2. The <tt>SDL2Renderer</tt> Implementation", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md877", null ]
      ] ],
      [ "4. Migration Plan", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md878", [
        [ "Phase 1: Implement the Abstraction Layer", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md879", null ],
        [ "Phase 2: Migrate to SDL3", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md880", null ],
        [ "Phase 3: Support for Multiple Rendering Backends", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md881", null ]
      ] ],
      [ "5. Conclusion", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md882", null ]
    ] ],
    [ "SNES Palette System Overview", "da/dfd/md_docs_2G3-palete-system-overview.html", [
      [ "Understanding SNES Color and Palette Organization", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md884", [
        [ "Core Concepts", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md885", [
          [ "1. SNES Color Format (15-bit BGR555)", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md886", null ],
          [ "2. Palette Groups in Zelda 3", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md887", null ]
        ] ],
        [ "Dungeon Palette System", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md888", [
          [ "Structure", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md889", null ],
          [ "Usage", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md890", null ],
          [ "Color Distribution (90 colors)", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md891", null ]
        ] ],
        [ "Overworld Palette System", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md892", [
          [ "Structure", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md893", null ],
          [ "3BPP Graphics and Left/Right Palettes", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md894", null ]
        ] ],
        [ "Common Issues and Solutions", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md895", [
          [ "Issue 1: Empty Palette", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md896", null ],
          [ "Issue 2: Bitmap Corruption", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md897", null ],
          [ "Issue 3: ROM Not Loaded in Preview", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md898", null ]
        ] ],
        [ "Palette Editor Integration", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md899", [
          [ "Key Functions for UI", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md900", null ],
          [ "Palette Widget Requirements", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md901", null ]
        ] ],
        [ "Graphics Manager Integration", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md902", [
          [ "Sheet Palette Assignment", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md903", null ]
        ] ],
        [ "Best Practices", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md904", null ],
        [ "ROM Addresses (for reference)", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md905", null ]
      ] ],
      [ "Graphics Sheet Palette Application", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md906", [
        [ "Default Palette Assignment", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md907", null ],
        [ "Palette Update Workflow", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md908", null ],
        [ "Common Pitfalls", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md909", null ]
      ] ]
    ] ],
    [ "Graphics Renderer Migration - Complete Documentation", "d5/dc8/md_docs_2G3-renderer-migration-complete.html", [
      [ "📋 Executive Summary", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md912", [
        [ "Key Achievements", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md913", null ]
      ] ],
      [ "🎯 Migration Goals & Results", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md915", null ],
      [ "🏗️ Architecture Overview", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md917", [
        [ "Before: Singleton Pattern", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md918", null ],
        [ "After: Dependency Injection + Deferred Queue", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md919", null ]
      ] ],
      [ "📦 Component Details", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md921", [
        [ "1. IRenderer Interface (<tt>src/app/gfx/backend/irenderer.h</tt>)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md922", null ],
        [ "2. SDL2Renderer (<tt>src/app/gfx/backend/sdl2_renderer.{h,cc}</tt>)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md924", null ],
        [ "3. Arena Deferred Texture Queue (<tt>src/app/gfx/arena.{h,cc}</tt>)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md926", null ],
        [ "4. Bitmap Palette Refactoring (<tt>src/app/gfx/bitmap.{h,cc}</tt>)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md928", null ],
        [ "5. Canvas Optional Renderer (<tt>src/app/gui/canvas.{h,cc}</tt>)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md930", null ],
        [ "6. Tilemap Texture Queue Integration (<tt>src/app/gfx/tilemap.cc</tt>)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md932", null ]
      ] ],
      [ "🔄 Dependency Injection Flow", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md934", [
        [ "Controller → EditorManager → Editors", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md935", null ]
      ] ],
      [ "⚡ Performance Optimizations", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md937", [
        [ "1. Batched Texture Processing", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md938", null ],
        [ "2. Frame Rate Limiting", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md939", null ],
        [ "3. Auto-Pause on Focus Loss", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md940", null ],
        [ "4. Surface/Texture Pooling", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md941", null ]
      ] ],
      [ "🗺️ Migration Map: File Changes", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md943", [
        [ "Core Architecture Files (New)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md944", null ],
        [ "Core Modified Files (Major)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md945", null ],
        [ "Editor Files (Renderer Injection)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md946", null ],
        [ "Emulator Files (Special Handling)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md947", null ],
        [ "GUI/Widget Files", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md948", null ],
        [ "Test Files (Updated for DI)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md949", null ]
      ] ],
      [ "🔧 Critical Fixes Applied", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md951", [
        [ "1. Bitmap::SetPalette() Crash", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md952", null ],
        [ "2. SDL2Renderer::UpdateTexture() SIGSEGV", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md954", null ],
        [ "3. Emulator Audio System Corruption", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md956", null ],
        [ "4. Emulator Cleanup During Shutdown", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md958", null ],
        [ "5. Controller/CreateWindow Initialization Order", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md960", null ]
      ] ],
      [ "🎨 Canvas Refactoring", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md962", [
        [ "The Challenge", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md963", null ],
        [ "The Solution: Backwards-Compatible Dual API", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md964", null ]
      ] ],
      [ "🧪 Testing Strategy", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md966", [
        [ "Test Files Updated", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md967", null ]
      ] ],
      [ "🛣️ Road to SDL3", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md969", [
        [ "Why This Migration Matters", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md970", null ],
        [ "Our Abstraction Layer Handles This", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md971", null ]
      ] ],
      [ "📊 Performance Benchmarks", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md973", [
        [ "Texture Loading Performance", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md974", null ],
        [ "Graphics Editor Performance", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md975", null ],
        [ "Emulator Performance", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md976", null ]
      ] ],
      [ "🐛 Bugs Fixed During Migration", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md978", [
        [ "Critical Crashes", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md979", null ],
        [ "Build Errors", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md980", null ]
      ] ],
      [ "💡 Key Design Patterns Used", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md982", [
        [ "1. Dependency Injection", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md983", null ],
        [ "2. Command Pattern (Deferred Queue)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md984", null ],
        [ "3. RAII (Resource Management)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md985", null ],
        [ "4. Adapter Pattern (Backend Abstraction)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md986", null ],
        [ "5. Singleton with DI (Arena)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md987", null ]
      ] ],
      [ "🔮 Future Enhancements", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md989", [
        [ "Short Term (SDL2)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md990", null ],
        [ "Medium Term (SDL3 Prep)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md991", null ],
        [ "Long Term (SDL3 Migration)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md992", null ]
      ] ],
      [ "📝 Lessons Learned", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md994", [
        [ "What Went Well", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md995", null ],
        [ "Challenges Overcome", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md996", null ],
        [ "Best Practices Established", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md997", null ]
      ] ],
      [ "🎓 Technical Deep Dive: Texture Queue System", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md999", [
        [ "Why Deferred Rendering?", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1000", null ],
        [ "Queue Processing Algorithm", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1001", null ]
      ] ],
      [ "🏆 Success Metrics", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1003", [
        [ "Build Health", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1004", null ],
        [ "Runtime Stability", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1005", null ],
        [ "Performance", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1006", null ],
        [ "Code Quality", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1007", null ]
      ] ],
      [ "📚 References", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1009", [
        [ "Related Documents", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1010", null ],
        [ "Key Commits", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1011", null ],
        [ "External Resources", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1012", null ]
      ] ],
      [ "🙏 Acknowledgments", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1014", null ],
      [ "🎉 Conclusion", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1016", null ],
      [ "🚧 Known Issues & Next Steps", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1018", [
        [ "macOS-Specific Issues (Not Renderer-Related)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1019", null ],
        [ "Stability Improvements for Next Session", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1020", [
          [ "High Priority", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1021", null ],
          [ "Medium Priority", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1022", null ],
          [ "Low Priority (SDL3 Migration)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1023", null ]
        ] ],
        [ "Testing Recommendations", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1024", null ]
      ] ],
      [ "🎵 Final Notes", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1026", null ]
    ] ],
    [ "Changelog", "d6/da7/md_docs_2H1-changelog.html", [
      [ "0.3.2 (October 2025)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1029", [
        [ "CI/CD & Release Improvements", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1030", null ],
        [ "Rendering Pipeline Fixes", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1031", null ],
        [ "Card-Based UI System", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1032", null ],
        [ "Tile16 Editor & Graphics System", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1033", null ],
        [ "Windows Platform Stability", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1034", null ],
        [ "Emulator: Audio System Infrastructure", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1035", null ],
        [ "Emulator: Critical Performance Fixes", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1036", null ],
        [ "Emulator: UI Organization & Input System", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1037", null ],
        [ "Debugger: Breakpoint & Watchpoint Systems", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1038", null ],
        [ "Build System Simplifications", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1039", null ],
        [ "Build System: Windows Platform Improvements", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1040", null ],
        [ "GUI & UX Modernization", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1041", null ],
        [ "Overworld Editor Refactoring", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1042", null ],
        [ "Build System & Stability", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1043", null ],
        [ "Future Optimizations (Planned)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1044", null ],
        [ "Technical Notes", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1045", null ]
      ] ],
      [ "0.3.1 (September 2025)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1046", [
        [ "Major Features", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1047", null ],
        [ "Tile16 Editor Enhancements", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1048", null ],
        [ "ZSCustomOverworld v3 Implementation", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1049", null ],
        [ "Technical Improvements", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1050", null ],
        [ "User Interface", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1051", null ],
        [ "Bug Fixes", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1052", null ],
        [ "ZScream Compatibility Improvements", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1053", null ]
      ] ],
      [ "0.3.0 (September 2025)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1054", [
        [ "Major Features", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1055", null ],
        [ "User Interface & Theming", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1056", null ],
        [ "Enhancements", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1057", null ],
        [ "Technical Improvements", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1058", null ],
        [ "Bug Fixes", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1059", null ]
      ] ],
      [ "0.2.2 (December 2024)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1060", null ],
      [ "0.2.1 (August 2024)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1061", null ],
      [ "0.2.0 (July 2024)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1062", null ],
      [ "0.1.0 (May 2024)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1063", null ],
      [ "0.0.9 (April 2024)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1064", null ],
      [ "0.0.8 (February 2024)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1065", null ],
      [ "0.0.7 (January 2024)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1066", null ],
      [ "0.0.6 (November 2023)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1067", null ],
      [ "0.0.5 (November 2023)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1068", null ],
      [ "0.0.4 (November 2023)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1069", null ],
      [ "0.0.3 (October 2023)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1070", null ]
    ] ],
    [ "Roadmap", "d8/d97/md_docs_2I1-roadmap.html", [
      [ "Current Focus: AI & Editor Polish", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1072", null ],
      [ "0.4.X (Next Major Release) - Advanced Tooling & UX", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1074", [
        [ "Priority 1: Editor Features & UX", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1075", null ],
        [ "Priority 2: <tt>z3ed</tt> AI Agent", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1076", null ],
        [ "Priority 3: Testing & Stability", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1077", null ]
      ] ],
      [ "0.5.X - Feature Expansion", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1079", null ],
      [ "0.6.X - Content & Integration", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1081", null ],
      [ "Recently Completed (v0.3.3 - October 6, 2025)", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1083", null ],
      [ "Recently Completed (v0.3.2)", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1084", null ]
    ] ],
    [ "yaze Documentation", "d3/d4c/md_docs_2index.html", [
      [ "A: Getting Started & Testing", "d3/d4c/md_docs_2index.html#autotoc_md1086", null ],
      [ "B: Build & Platform", "d3/d4c/md_docs_2index.html#autotoc_md1087", null ],
      [ "C: <tt>z3ed</tt> CLI", "d3/d4c/md_docs_2index.html#autotoc_md1088", null ],
      [ "E: Development & API", "d3/d4c/md_docs_2index.html#autotoc_md1089", null ],
      [ "F: Technical Documentation", "d3/d4c/md_docs_2index.html#autotoc_md1090", null ],
      [ "G: GUI Guides", "d3/d4c/md_docs_2index.html#autotoc_md1091", null ],
      [ "H: Project Info", "d3/d4c/md_docs_2index.html#autotoc_md1092", null ],
      [ "I: Roadmap", "d3/d4c/md_docs_2index.html#autotoc_md1093", null ],
      [ "R: ROM Reference", "d3/d4c/md_docs_2index.html#autotoc_md1094", null ]
    ] ],
    [ "A Link to the Past ROM Reference", "d7/d4f/md_docs_2R1-alttp-rom-reference.html", [
      [ "Graphics System", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1097", [
        [ "Graphics Sheets", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1098", null ],
        [ "Palette System", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1099", [
          [ "Color Format", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1100", null ],
          [ "Palette Groups", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1101", null ]
        ] ]
      ] ],
      [ "Dungeon System", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1102", [
        [ "Room Data Structure", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1103", null ],
        [ "Tile16 Format", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1104", null ],
        [ "Blocksets", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1105", null ]
      ] ],
      [ "Message System", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1106", [
        [ "Text Data Locations", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1107", null ],
        [ "Character Encoding", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1108", null ],
        [ "Text Commands", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1109", null ],
        [ "Font Graphics", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1110", null ]
      ] ],
      [ "Overworld System", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1111", [
        [ "Map Structure", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1112", null ],
        [ "Area Sizes (ZSCustomOverworld v3+)", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1113", null ],
        [ "Tile Format", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1114", null ]
      ] ],
      [ "Compression", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1115", [
        [ "LC-LZ2 Algorithm", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1116", null ]
      ] ],
      [ "Memory Map", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1117", [
        [ "ROM Banks (LoROM)", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1118", null ],
        [ "Important ROM Locations", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1119", null ]
      ] ]
    ] ],
    [ "yaze - Yet Another Zelda3 Editor", "d0/d30/md_README.html", [
      [ "Version 0.3.2 - Release", "d0/d30/md_README.html#autotoc_md1122", null ],
      [ "Quick Start", "d0/d30/md_README.html#autotoc_md1127", [
        [ "Build", "d0/d30/md_README.html#autotoc_md1128", null ],
        [ "Applications", "d0/d30/md_README.html#autotoc_md1129", null ]
      ] ],
      [ "Usage", "d0/d30/md_README.html#autotoc_md1130", [
        [ "GUI Editor", "d0/d30/md_README.html#autotoc_md1131", null ],
        [ "Command Line Tool", "d0/d30/md_README.html#autotoc_md1132", null ],
        [ "C++ API", "d0/d30/md_README.html#autotoc_md1133", null ]
      ] ],
      [ "Documentation", "d0/d30/md_README.html#autotoc_md1134", null ],
      [ "Supported Platforms", "d0/d30/md_README.html#autotoc_md1135", null ],
      [ "ROM Compatibility", "d0/d30/md_README.html#autotoc_md1136", null ],
      [ "Contributing", "d0/d30/md_README.html#autotoc_md1137", null ],
      [ "License", "d0/d30/md_README.html#autotoc_md1138", null ],
      [ "🙏 Acknowledgments", "d0/d30/md_README.html#autotoc_md1139", null ],
      [ "📸 Screenshots", "d0/d30/md_README.html#autotoc_md1140", null ]
    ] ],
    [ "yaze Build Scripts", "de/d82/md_scripts_2README.html", [
      [ "Windows Scripts", "de/d82/md_scripts_2README.html#autotoc_md1143", [
        [ "vcpkg Setup (Optional)", "de/d82/md_scripts_2README.html#autotoc_md1144", null ]
      ] ],
      [ "Windows Build Workflow", "de/d82/md_scripts_2README.html#autotoc_md1145", [
        [ "Recommended: Visual Studio CMake Mode", "de/d82/md_scripts_2README.html#autotoc_md1146", null ],
        [ "Command Line Build", "de/d82/md_scripts_2README.html#autotoc_md1147", null ],
        [ "Compiler Notes", "de/d82/md_scripts_2README.html#autotoc_md1148", null ]
      ] ],
      [ "Quick Start (Windows)", "de/d82/md_scripts_2README.html#autotoc_md1149", [
        [ "Option 1: Visual Studio (Recommended)", "de/d82/md_scripts_2README.html#autotoc_md1150", null ],
        [ "Option 2: Command Line", "de/d82/md_scripts_2README.html#autotoc_md1151", null ],
        [ "Option 3: With vcpkg (Optional)", "de/d82/md_scripts_2README.html#autotoc_md1152", null ]
      ] ],
      [ "Troubleshooting", "de/d82/md_scripts_2README.html#autotoc_md1153", [
        [ "Common Issues", "de/d82/md_scripts_2README.html#autotoc_md1154", null ],
        [ "Getting Help", "de/d82/md_scripts_2README.html#autotoc_md1155", null ]
      ] ],
      [ "Other Scripts", "de/d82/md_scripts_2README.html#autotoc_md1156", null ],
      [ "Build Environment Verification", "de/d82/md_scripts_2README.html#autotoc_md1157", [
        [ "<tt>verify-build-environment.ps1</tt> / <tt>.sh</tt>", "de/d82/md_scripts_2README.html#autotoc_md1158", null ],
        [ "Usage", "de/d82/md_scripts_2README.html#autotoc_md1159", null ]
      ] ]
    ] ],
    [ "Agent Editor Module", "d6/df7/md_src_2app_2editor_2agent_2README.html", [
      [ "Overview", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1161", null ],
      [ "Architecture", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1162", [
        [ "Core Components", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1163", [
          [ "AgentEditor (<tt>agent_editor.h/cc</tt>)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1164", null ],
          [ "AgentChatWidget (<tt>agent_chat_widget.h/cc</tt>)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1165", null ],
          [ "AgentChatHistoryCodec (<tt>agent_chat_history_codec.h/cc</tt>)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1166", null ]
        ] ],
        [ "Collaboration Coordinators", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1167", [
          [ "AgentCollaborationCoordinator (<tt>agent_collaboration_coordinator.h/cc</tt>)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1168", null ],
          [ "NetworkCollaborationCoordinator (<tt>network_collaboration_coordinator.h/cc</tt>)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1169", null ]
        ] ]
      ] ],
      [ "Usage", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1170", [
        [ "Initialization", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1171", null ],
        [ "Drawing", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1172", null ],
        [ "Session Management", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1173", null ],
        [ "Network Mode (requires YAZE_WITH_GRPC and YAZE_WITH_JSON)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1174", null ]
      ] ],
      [ "File Structure", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1175", null ],
      [ "Build Configuration", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1176", [
        [ "Required", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1177", null ],
        [ "Optional", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1178", null ]
      ] ],
      [ "Data Files", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1179", [
        [ "Local Storage", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1180", null ],
        [ "Session File Format", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1181", null ]
      ] ],
      [ "Integration with EditorManager", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1182", null ],
      [ "Dependencies", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1183", [
        [ "Internal", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1184", null ],
        [ "External (when enabled)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1185", null ]
      ] ],
      [ "Advanced Features (v2.0)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1186", [
        [ "ROM Synchronization", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1187", null ],
        [ "Multimodal Snapshot Sharing", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1188", null ],
        [ "Proposal Management", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1189", null ],
        [ "AI Agent Integration", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1190", null ],
        [ "Health Monitoring", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1191", null ]
      ] ],
      [ "Future Enhancements", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1192", null ],
      [ "Server Protocol", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1193", [
        [ "Client → Server", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1194", null ],
        [ "Server → Client", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1195", null ]
      ] ]
    ] ],
    [ "End-to-End (E2E) Tests", "d9/db0/md_test_2e2e_2README.html", [
      [ "Active Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md1199", [
        [ "✅ Working Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md1200", null ],
        [ "📝 Dungeon Editor Smoke Test", "d9/db0/md_test_2e2e_2README.html#autotoc_md1201", null ]
      ] ],
      [ "Running Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md1202", [
        [ "All E2E Tests (GUI Mode)", "d9/db0/md_test_2e2e_2README.html#autotoc_md1203", null ],
        [ "Specific Test Category", "d9/db0/md_test_2e2e_2README.html#autotoc_md1204", null ],
        [ "Dungeon Editor Test Only", "d9/db0/md_test_2e2e_2README.html#autotoc_md1205", null ]
      ] ],
      [ "Test Development", "d9/db0/md_test_2e2e_2README.html#autotoc_md1206", [
        [ "Creating New Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md1207", null ],
        [ "Register in yaze_test.cc", "d9/db0/md_test_2e2e_2README.html#autotoc_md1208", null ],
        [ "ImGui Test Engine API", "d9/db0/md_test_2e2e_2README.html#autotoc_md1209", null ]
      ] ],
      [ "Test Logging", "d9/db0/md_test_2e2e_2README.html#autotoc_md1210", null ],
      [ "Test Infrastructure", "d9/db0/md_test_2e2e_2README.html#autotoc_md1211", [
        [ "File Organization", "d9/db0/md_test_2e2e_2README.html#autotoc_md1212", null ],
        [ "Helper Functions", "d9/db0/md_test_2e2e_2README.html#autotoc_md1213", null ]
      ] ],
      [ "Future Test Ideas", "d9/db0/md_test_2e2e_2README.html#autotoc_md1214", null ],
      [ "Troubleshooting", "d9/db0/md_test_2e2e_2README.html#autotoc_md1215", [
        [ "Test Crashes in GUI Mode", "d9/db0/md_test_2e2e_2README.html#autotoc_md1216", null ],
        [ "Tests Not Found", "d9/db0/md_test_2e2e_2README.html#autotoc_md1217", null ],
        [ "ImGui Items Not Found", "d9/db0/md_test_2e2e_2README.html#autotoc_md1218", null ]
      ] ],
      [ "References", "d9/db0/md_test_2e2e_2README.html#autotoc_md1219", null ],
      [ "Status", "d9/db0/md_test_2e2e_2README.html#autotoc_md1220", null ]
    ] ],
    [ "yaze Test Suite", "d0/d46/md_test_2README.html", [
      [ "Directory Structure", "d0/d46/md_test_2README.html#autotoc_md1222", null ],
      [ "Test Categories", "d0/d46/md_test_2README.html#autotoc_md1223", [
        [ "Unit Tests (<tt>unit/</tt>)", "d0/d46/md_test_2README.html#autotoc_md1224", null ],
        [ "Integration Tests (<tt>integration/</tt>)", "d0/d46/md_test_2README.html#autotoc_md1225", null ],
        [ "End-to-End Tests (<tt>e2e/</tt>)", "d0/d46/md_test_2README.html#autotoc_md1226", null ]
      ] ],
      [ "Enhanced Test Runner", "d0/d46/md_test_2README.html#autotoc_md1227", [
        [ "Usage Examples", "d0/d46/md_test_2README.html#autotoc_md1228", null ],
        [ "Test Modes", "d0/d46/md_test_2README.html#autotoc_md1229", null ],
        [ "Options", "d0/d46/md_test_2README.html#autotoc_md1230", null ]
      ] ],
      [ "E2E ROM Testing", "d0/d46/md_test_2README.html#autotoc_md1231", [
        [ "Test Cases", "d0/d46/md_test_2README.html#autotoc_md1232", null ]
      ] ],
      [ "ZSCustomOverworld Upgrade Testing", "d0/d46/md_test_2README.html#autotoc_md1233", [
        [ "Supported Upgrades", "d0/d46/md_test_2README.html#autotoc_md1234", null ],
        [ "Test Cases", "d0/d46/md_test_2README.html#autotoc_md1235", null ],
        [ "Version-Specific Features", "d0/d46/md_test_2README.html#autotoc_md1236", [
          [ "Vanilla", "d0/d46/md_test_2README.html#autotoc_md1237", null ],
          [ "v2", "d0/d46/md_test_2README.html#autotoc_md1238", null ],
          [ "v3", "d0/d46/md_test_2README.html#autotoc_md1239", null ]
        ] ]
      ] ],
      [ "Environment Variables", "d0/d46/md_test_2README.html#autotoc_md1240", null ],
      [ "CI/CD Integration", "d0/d46/md_test_2README.html#autotoc_md1241", null ],
      [ "Deprecated Tests", "d0/d46/md_test_2README.html#autotoc_md1242", null ],
      [ "Best Practices", "d0/d46/md_test_2README.html#autotoc_md1243", null ],
      [ "AI Agent Testing", "d0/d46/md_test_2README.html#autotoc_md1244", null ]
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
"d0/d52/classyaze_1_1zelda3_1_1RoomLayoutObject.html#a1cb17f9dcca5e20a4648aa456910c87c",
"d0/d8d/snes__tile_8h.html#abddef303b6a036e2cbce669f424f7317",
"d0/dbd/classyaze_1_1emu_1_1Emulator.html#abc3c1a7cbe20540ffdaea762229e7d08",
"d0/df3/structyaze_1_1gui_1_1FlagsMenu.html",
"d1/d17/structyaze_1_1core_1_1WidgetState.html#af08fce08f27a0a911f117ee498b965d6",
"d1/d33/structyaze_1_1emu_1_1OAMADDH.html#a0b96a11b2a4c9f66d5920d694fd5976c",
"d1/d46/structyaze_1_1gui_1_1BackgroundRenderer_1_1GridSettings.html#a2fcca066284dca340102cc086435492e",
"d1/d67/structyaze_1_1cli_1_1overworld_1_1WarpEntry.html#ad11039b39f87792164b3f3f8c74ee35d",
"d1/d95/structyaze_1_1test_1_1TestRecorder_1_1StopRecordingSummary.html#acd49184b7036c4858e8f7320f6293a6b",
"d1/daf/compression_8h.html#af7ea76e3f7fe3e688583c8e208c7c666",
"d1/de8/structyaze_1_1cli_1_1StartRecordingResult.html",
"d1/df9/structyaze_1_1emu_1_1WatchpointManager_1_1Watchpoint.html#a777d20c3c9c336091a081df2cf673274",
"d2/d07/classyaze_1_1emu_1_1Spc700.html#aac3dd88bb6a8dd6e7c650f0f0d830ee1",
"d2/d25/music__tool__commands_8cc.html",
"d2/d4b/asm__parser_8h.html#a22bf51ed91189695bf4e76bf6b85f836a584a5c33178ee320c2cf6328acc4ee82",
"d2/d5e/classyaze_1_1emu_1_1Memory.html#a77f49d5295dd11b819e70fce351bef96",
"d2/da4/flag_8h_source.html",
"d2/de7/classyaze_1_1editor_1_1DungeonCanvasViewer.html#a28255cdb7a9e028638443a32d539f5a5",
"d2/df9/ppu_8h.html#aa137d3787515b802157d6b1af1c83ef7",
"d3/d03/classyaze_1_1emu_1_1audio_1_1SDL2AudioBackend.html#ab042a9146e446a05e23d08f358f2508d",
"d3/d19/classyaze_1_1gfx_1_1GraphicsOptimizer.html#a092fe11bb77ef8e7339e13fc5c0f437e",
"d3/d2e/compression_8cc.html#a9eb4d71893f3ec5673e1f6d75599098c",
"d3/d44/classyaze_1_1editor_1_1OverworldEditor.html#a5958f041f070af4bd280eef1c669d8ab",
"d3/d4c/agent__ui__theme_8h.html#a0d4c8f93a59e331f4947cf5328160c24",
"d3/d6c/classyaze_1_1cli_1_1Palette.html",
"d3/d8d/classyaze_1_1editor_1_1PopupManager.html#a2e99b05364c27930477a9c1bb5e42e01",
"d3/d9f/classyaze_1_1editor_1_1Editor.html#a5c1b245617554f101535e1deb3cb5547",
"d3/db7/structyaze_1_1gui_1_1WidgetIdRegistry_1_1WidgetInfo.html#a1bc6f3c074b0da8e212934039568a2d4",
"d3/dc3/namespaceyaze_1_1cli_1_1net.html#ac2b64adf8870936f4002721d8df26b48",
"d3/ded/classyaze_1_1emu_1_1Ppu.html#a97079b9ed7736fb3a7ca309e87cc9181",
"d4/d0a/classyaze_1_1gfx_1_1PerformanceProfiler.html#afc168478b710ec714fd0b52c033be7b4",
"d4/d0a/namespaceyaze_1_1test.html#ad89734055c65f2ab92b2230a90fe6580",
"d4/d52/namespaceyaze_1_1gui_1_1anonymous__namespace_02widget__id__registry_8cc_03.html#aed8e0511d2d2ed009733d41ea5fe037b",
"d4/d7c/group__messages.html#gaeef987918fb23ce2afdc81653d0e1a1a",
"d4/d9e/macro_8h.html#a90be36f2e92f476b5bb13517da51afbe",
"d4/dd3/structyaze_1_1editor_1_1AgentChatWidget_1_1CollaborationCallbacks_1_1SessionContext.html#a6533a834a2857a039d0579a228eed82d",
"d5/d0b/dungeon__object__rendering__tests_8cc_source.html",
"d5/d1f/namespaceyaze_1_1zelda3.html#a4f255194eebaeff606f76992f4fa3b5b",
"d5/d1f/namespaceyaze_1_1zelda3.html#ab044ae61220ad060623de10691e64055",
"d5/d46/classyaze_1_1gfx_1_1PoolAllocator.html#a8bda13c70d1a5cff3e226b3dea474db5",
"d5/d71/structyaze_1_1editor_1_1AgentEditor_1_1BotProfile.html#afcfe60c963c844ccd996c1e9c6b868b5",
"d5/da0/structyaze_1_1emu_1_1DspChannel.html#abc7bf098c4afbdf9a7ddd68ced64905f",
"d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md967",
"d6/d00/classyaze_1_1cli_1_1EnhancedChatComponent.html#a4f4aad58a70780e18f2e90b9b2718f32",
"d6/d10/md_docs_2A1-testing-guide.html#autotoc_md11",
"d6/d2c/md_docs_2DUNGEON__EDITOR__GUIDE.html#autotoc_md314",
"d6/d30/classyaze_1_1Rom.html#a7bdc727db5fbd5eb40cfe62f9eb30afb",
"d6/d48/structyaze_1_1cli_1_1AutocompleteEngine_1_1CommandDef.html",
"d6/d7f/structyaze_1_1editor_1_1RecentProject.html#aac877e6ae4e1c71e36857996f6efc05f",
"d6/db0/structyaze_1_1gfx_1_1TileCache.html#abb5ff6d672372d77827185773a37a1a0",
"d6/dcb/classyaze_1_1gui_1_1canvas_1_1CanvasPerformanceIntegration.html#aba76be85193aa831ff11960858442b73",
"d6/df8/structyaze_1_1zelda3_1_1RoomSize.html",
"d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md612",
"d7/d61/classyaze_1_1zelda3_1_1DungeonEditorSystem.html#a1b83e08a1e5442fa8f886e5ccd86cdde",
"d7/d61/classyaze_1_1zelda3_1_1DungeonEditorSystem.html#ad7a74733aed6b7269d008ce8d5b2b65c",
"d7/d87/dungeon__editor__test_8cc.html#a2a11f299beab72535b885e4306437550",
"d7/dc3/classyaze_1_1zelda3_1_1test_1_1RoomManipulationTest.html#a3d8660c93720caa1c56f9c1c86a75b63",
"d7/ddb/classyaze_1_1gfx_1_1ScopedTimer.html#a9a2bc8615706dfbb0ab07cbfe1b108a0",
"d7/df6/classyaze_1_1zelda3_1_1Room.html#ab4a55ae24532bd0fea81b768ace05604",
"d8/d03/classyaze_1_1cli_1_1util_1_1LoadingIndicator.html#a37b0553a054a37e13a572893d7c56899",
"d8/d28/structyaze_1_1emu_1_1BreakpointManager_1_1Breakpoint.html#a4f9f9e28c97291d71ee64f0bd77b48d6",
"d8/d46/classyaze_1_1editor_1_1UserSettings.html#a4d7bc80e99ca101960a6764a5f9ead98",
"d8/d73/md_docs_2E3-api-reference.html#autotoc_md530",
"d8/da5/structyaze_1_1gui_1_1canvas_1_1CanvasUsageStats.html#ad925bd77a516c0122e380088bd84028e",
"d8/dd5/classyaze_1_1gui_1_1WidgetIdScope.html",
"d8/ddf/classyaze_1_1editor_1_1ProjectFileEditor.html#ae449c98b910eadcb279070206a9308d3",
"d9/d2b/classyaze_1_1editor_1_1AgentChatWidget.html#a185bb16899b47c16942d1f4a1186500f",
"d9/d36/structyaze_1_1Rom_1_1SaveSettings.html#a21e32750a0a04101b26825f6c31ce81c",
"d9/d71/classyaze_1_1gui_1_1DungeonObjectEmulatorPreview.html#a3c167356e7a890cfa4254cb5325c0b72",
"d9/d8f/classyaze_1_1cli_1_1ai_1_1anonymous__namespace_02ai__gui__controller__test_8cc_03_1_1MockGuiAutomationClient.html#a33b20ca4a7526d59a5308600ee0bdcd1",
"d9/dc0/room_8h.html#a693df677b1b2754452c8b8b4c4a98468a65ecc6bb8c038327b3d25cca07575c8f",
"d9/dc5/classyaze_1_1zelda3_1_1Overworld.html#ab19488f1a7a7d6e6b5b8320b17bf825e",
"d9/dcc/classyaze_1_1zelda3_1_1OverworldMap.html#ac15e10e3b13f68961e865fdb8b9f815e",
"d9/ded/classyaze_1_1emu_1_1BreakpointManager.html#a9547dc0782c7d748e86f796877f27878",
"da/d08/structyaze_1_1zelda3_1_1ObjectSizeInfo.html",
"da/d2c/classyaze_1_1gui_1_1Canvas.html#a7c4c7b27651a4090ba8c40a58d6939dc",
"da/d37/hyrule__magic_8cc.html#a23f2763cd8a3044a6a63cfa1c4c0ce53",
"da/d45/classyaze_1_1editor_1_1DungeonEditorV2.html#ae88baa59cc6893d93e15cd881bdf88ab",
"da/d6e/structyaze_1_1editor_1_1FolderItem.html",
"da/dae/classyaze_1_1net_1_1WebSocketClient.html#a813a7393599f7e9e2dae001c2a719c46",
"da/dd7/structyaze_1_1emu_1_1SETINI.html#af196a13908c98ae9011fe228dd36b0eb",
"db/d01/proposal__registry_8cc.html#a6bd4b541a3ab7c8fccffacb4cd574de3",
"db/d2d/classyaze_1_1cli_1_1GeminiAIService.html#a8fabee613135908e8ef20253f8261658",
"db/d5d/ui__constants_8h.html#a8ed3537167fd17cc908603bd55ad7ba0",
"db/d82/classyaze_1_1editor_1_1Tile16Editor.html#a605685fadf51e9b2f2bc5ebb8b1517a6",
"db/d9c/message__data_8h.html#a6e03f0e92717101837c29f9cc3bf172f",
"db/dc3/namespaceyaze_1_1gui_1_1canvas.html#a11c32674d05f23d59e00b23ee6c9c60ca25c2dc47991b3df171ed5192bcf70390",
"db/de0/structyaze_1_1cli_1_1FewShotExample.html#a6cd1e3b62fae189a873f1da8cb11fff4",
"dc/d0c/structyaze_1_1cli_1_1GeminiConfig.html#a2e5db5d9a0d192230ffa087cf48d8a8e",
"dc/d31/classyaze_1_1editor_1_1GraphicsEditor.html#a4ff651cfbe56f7801b81f49bfbb1f3a2",
"dc/d42/md_docs_2B1-build-instructions.html#autotoc_md53",
"dc/d5b/structyaze_1_1editor_1_1AgentEditor_1_1SessionInfo.html",
"dc/daa/classyaze_1_1editor_1_1WelcomeScreen.html#a482e6f0258b1effb974fb9dd3b80e26a",
"dc/de2/todo__commands_8h_source.html",
"dc/df4/classyaze_1_1emu_1_1Cpu.html#a7035a9d4ef932fa372091b210688ca53",
"dc/df4/classyaze_1_1emu_1_1Cpu.html#aea682e47ae0952d8dfb1de5ad2eb6d3f",
"dd/d12/classyaze_1_1editor_1_1EditorManager.html#a3d28e2258e82c102a02568824ed4dfe3",
"dd/d22/structyaze_1_1cli_1_1CommandInfo.html",
"dd/d4b/classyaze_1_1editor_1_1DungeonUsageTracker.html#a611a0fad6ebc537cc23ff2a193574dff",
"dd/d71/classyaze_1_1gui_1_1canvas_1_1CanvasInteractionHandler.html#a43752c0d63035dc3adf12d9fa817c224",
"dd/dc0/classyaze_1_1test_1_1WidgetDiscoveryService.html#a4d349f3e1ee7e07db0f535fa969ed5d9",
"dd/ddf/classyaze_1_1gui_1_1canvas_1_1CanvasContextMenu.html#ab249f939df1b20be6341ddbfc3dfa269",
"dd/dfb/structyaze_1_1editor_1_1AgentChatWidget_1_1ScreenshotPreviewState.html#ab6cd0f7b6155e9b6119e49661a0be70c",
"de/d0f/classyaze_1_1emu_1_1MemoryImpl.html#ae286a6c904922ce9f44c7996a51e71b3",
"de/d71/classyaze_1_1cli_1_1ResourceContextBuilder.html#a0aae5b8ee0c9a8087320793fc0d83c1b",
"de/d82/classyaze_1_1util_1_1NotifyValue.html",
"de/da1/classyaze_1_1cli_1_1TestWorkflowGenerator.html#a1344aa88e2692b92fdc5166fb1cf0788",
"de/dbf/icons_8h.html#a0a5d24ddfb1f3541a8024bfabab1727b",
"de/dbf/icons_8h.html#a2940f4b8c599d16ad695e464420eade7",
"de/dbf/icons_8h.html#a4667a1d09c96836632d66266c0904ddd",
"de/dbf/icons_8h.html#a6336bb6ac0112c7c010ac87615ae0b8a",
"de/dbf/icons_8h.html#a82a5aacdf7aff620b933e7c34adca420",
"de/dbf/icons_8h.html#aa2bbac1456a005aafbe5d6217ca218da",
"de/dbf/icons_8h.html#abe8b5cf5a4772153d59fa389747cbeb4",
"de/dbf/icons_8h.html#adad4457ea86c60d3974499a6672a985c",
"de/dbf/icons_8h.html#af4b97f1c021c2ffdc7a53f75db6f79e1",
"de/dda/structyaze_1_1zelda3_1_1ObjectRenderer_1_1TileRenderInfo.html#a1090fd3dc9aedf0a12a1209beb19bad6",
"de/ded/input_8h.html#a0fd30b00c2e36f50139ddf4980e72633",
"df/d19/namespaceyaze_1_1zelda3_1_1anonymous__namespace_02room__object__encoding__test_8cc_03.html#afdcf602b1704bab8750d0498c49f0d82",
"df/d7d/dma_8cc.html#a8e66944bc240cdef6f415f891a3e493b",
"df/db9/classyaze_1_1cli_1_1ModernCLI.html#aa71012bdf0ca4a98f033ad659be34161",
"df/ded/terminal__colors_8h.html#aef12bd55b12058163b8112b37f735e71",
"functions_y.html"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';