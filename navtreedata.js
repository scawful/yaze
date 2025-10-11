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
    [ "APU Timing Fix - Technical Analysis", "df/dc1/md_docs_2apu-timing-analysis.html", [
      [ "Implementation Status", "df/dc1/md_docs_2apu-timing-analysis.html#autotoc_md26", null ],
      [ "Problem Summary", "df/dc1/md_docs_2apu-timing-analysis.html#autotoc_md27", null ],
      [ "Current Implementation Analysis", "df/dc1/md_docs_2apu-timing-analysis.html#autotoc_md29", [
        [ "1. <strong>Cycle Counting System</strong> (<tt>spc700.cc</tt>)", "df/dc1/md_docs_2apu-timing-analysis.html#autotoc_md30", null ],
        [ "2. <strong>The <tt>bstep</tt> Mechanism</strong> (<tt>spc700.cc</tt>)", "df/dc1/md_docs_2apu-timing-analysis.html#autotoc_md31", null ],
        [ "3. <strong>APU Main Loop</strong> (<tt>apu.cc:73-143</tt>)", "df/dc1/md_docs_2apu-timing-analysis.html#autotoc_md32", null ],
        [ "4. <strong>Floating-Point Precision</strong> (<tt>apu.cc:17</tt>)", "df/dc1/md_docs_2apu-timing-analysis.html#autotoc_md33", null ]
      ] ],
      [ "Root Cause: Handshake Timing Failure", "df/dc1/md_docs_2apu-timing-analysis.html#autotoc_md35", [
        [ "The Handshake Protocol", "df/dc1/md_docs_2apu-timing-analysis.html#autotoc_md36", null ],
        [ "Where It Gets Stuck", "df/dc1/md_docs_2apu-timing-analysis.html#autotoc_md37", null ]
      ] ],
      [ "LakeSnes Comparison Analysis", "df/dc1/md_docs_2apu-timing-analysis.html#autotoc_md39", [
        [ "What LakeSnes Does Right", "df/dc1/md_docs_2apu-timing-analysis.html#autotoc_md40", null ],
        [ "Where LakeSnes Falls Short (And How We Can Do Better)", "df/dc1/md_docs_2apu-timing-analysis.html#autotoc_md41", null ],
        [ "What We're Adopting from LakeSnes", "df/dc1/md_docs_2apu-timing-analysis.html#autotoc_md42", null ],
        [ "What We're Improving Over LakeSnes", "df/dc1/md_docs_2apu-timing-analysis.html#autotoc_md43", null ]
      ] ],
      [ "Solution Design", "df/dc1/md_docs_2apu-timing-analysis.html#autotoc_md45", [
        [ "Phase 1: Atomic Instruction Execution", "df/dc1/md_docs_2apu-timing-analysis.html#autotoc_md46", null ],
        [ "Phase 2: Precise Cycle Calculation", "df/dc1/md_docs_2apu-timing-analysis.html#autotoc_md47", null ],
        [ "Phase 3: Refactor <tt>Apu::RunCycles</tt> to Cycle Budget Model", "df/dc1/md_docs_2apu-timing-analysis.html#autotoc_md48", null ],
        [ "Phase 4: Fixed-Point Cycle Ratio", "df/dc1/md_docs_2apu-timing-analysis.html#autotoc_md49", null ]
      ] ],
      [ "Implementation Plan", "df/dc1/md_docs_2apu-timing-analysis.html#autotoc_md51", [
        [ "Step 1: Add <tt>Spc700::Step()</tt> Function", "df/dc1/md_docs_2apu-timing-analysis.html#autotoc_md52", null ],
        [ "Step 2: Implement Precise Cycle Calculation", "df/dc1/md_docs_2apu-timing-analysis.html#autotoc_md53", null ],
        [ "Step 3: Eliminate <tt>bstep</tt> Mechanism", "df/dc1/md_docs_2apu-timing-analysis.html#autotoc_md54", null ],
        [ "Step 4: Refactor <tt>Apu::RunCycles</tt>", "df/dc1/md_docs_2apu-timing-analysis.html#autotoc_md55", null ],
        [ "Step 5: Convert to Fixed-Point Ratio", "df/dc1/md_docs_2apu-timing-analysis.html#autotoc_md56", null ],
        [ "Step 6: Testing", "df/dc1/md_docs_2apu-timing-analysis.html#autotoc_md57", null ]
      ] ],
      [ "Files to Modify", "df/dc1/md_docs_2apu-timing-analysis.html#autotoc_md59", null ],
      [ "Success Criteria", "df/dc1/md_docs_2apu-timing-analysis.html#autotoc_md61", null ],
      [ "Implementation Completed", "df/dc1/md_docs_2apu-timing-analysis.html#autotoc_md63", null ]
    ] ],
    [ "Build Instructions", "dc/d42/md_docs_2B1-build-instructions.html", [
      [ "1. Environment Verification", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md66", [
        [ "Windows (PowerShell)", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md67", null ],
        [ "macOS & Linux (Bash)", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md68", null ]
      ] ],
      [ "2. Quick Start: Building with Presets", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md69", [
        [ "macOS", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md70", null ],
        [ "Linux", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md71", null ],
        [ "Windows", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md72", null ],
        [ "AI-Enabled Build (All Platforms)", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md73", null ]
      ] ],
      [ "3. Dependencies", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md74", null ],
      [ "4. Platform Setup", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md75", [
        [ "macOS", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md76", null ],
        [ "Linux (Ubuntu/Debian)", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md77", null ],
        [ "Windows", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md78", null ]
      ] ],
      [ "5. Testing", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md79", [
        [ "Running Tests with Presets", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md80", null ],
        [ "Running Tests Manually", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md81", null ]
      ] ],
      [ "6. IDE Integration", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md82", [
        [ "VS Code (Recommended)", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md83", null ],
        [ "Visual Studio (Windows)", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md84", null ],
        [ "Xcode (macOS)", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md85", null ]
      ] ],
      [ "7. Windows Build Optimization", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md86", [
        [ "gRPC v1.67.1 and MSVC Compatibility", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md87", null ],
        [ "The Problem: Slow gRPC Builds", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md88", null ],
        [ "Solution A: Use vcpkg for Pre-compiled Binaries (Recommended - FAST)", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md89", null ],
        [ "Solution B: FetchContent Build (Slow but Automatic)", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md90", null ]
      ] ],
      [ "8. Troubleshooting", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md91", [
        [ "Automatic Fixes", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md92", null ],
        [ "Cleaning Stale Builds", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md93", null ],
        [ "Common Issues", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md94", [
          [ "\"nlohmann/json.hpp: No such file or directory\"", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md95", null ],
          [ "\"Cannot open file 'yaze.exe': Permission denied\"", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md96", null ],
          [ "\"C++ standard 'cxx_std_23' not supported\"", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md97", null ],
          [ "Visual Studio Can't Find Presets", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md98", null ],
          [ "Git Line Ending (CRLF) Issues", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md99", null ],
          [ "File Path Length Limit on Windows", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md100", null ]
        ] ]
      ] ]
    ] ],
    [ "Platform Compatibility & CI/CD Fixes", "d1/d30/md_docs_2B2-platform-compatibility.html", [
      [ "Platform-Specific Notes", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md103", [
        [ "Windows", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md104", null ],
        [ "macOS", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md105", null ],
        [ "Linux", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md106", null ]
      ] ],
      [ "Cross-Platform Code Validation", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md108", [
        [ "Audio System", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md109", null ],
        [ "Input System", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md110", null ],
        [ "Debugger System", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md111", null ],
        [ "UI Layer", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md112", null ]
      ] ],
      [ "Common Build Issues & Solutions", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md114", [
        [ "Windows: \"use of undefined type 'PromiseLike'\"", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md115", null ],
        [ "macOS: \"absl not found\"", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md116", null ],
        [ "Linux: CMake configuration fails", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md117", null ],
        [ "Windows: \"DWORD syntax error\"", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md118", null ]
      ] ],
      [ "CI/CD Validation Checklist", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md120", null ],
      [ "Testing Strategy", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md122", [
        [ "Automated (CI)", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md123", null ],
        [ "Manual Testing", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md124", null ]
      ] ],
      [ "Quick Reference", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md126", [
        [ "Build Command (All Platforms)", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md127", null ],
        [ "Enable Features", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md128", null ],
        [ "Windows Troubleshooting", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md129", null ]
      ] ],
      [ "Filesystem Abstraction", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md131", null ],
      [ "Native File Dialog Support", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md133", null ]
    ] ],
    [ "Build Presets Guide", "d7/d51/md_docs_2B3-build-presets.html", [
      [ "Design Principles", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md136", null ],
      [ "Quick Start", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md137", [
        [ "macOS Development", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md138", null ],
        [ "Windows Development", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md139", null ]
      ] ],
      [ "All Presets", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md140", [
        [ "macOS Presets", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md141", null ],
        [ "Windows Presets", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md142", null ],
        [ "Linux Presets", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md143", null ],
        [ "Special Purpose", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md144", null ]
      ] ],
      [ "Warning Control", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md145", [
        [ "To Enable Verbose Warnings:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md146", null ]
      ] ],
      [ "Architecture Support", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md147", [
        [ "macOS", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md148", null ],
        [ "Windows", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md149", null ]
      ] ],
      [ "Build Directories", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md150", null ],
      [ "Feature Flags", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md151", null ],
      [ "Examples", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md152", [
        [ "Development with AI features and verbose warnings", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md153", null ],
        [ "Release build for distribution (macOS Universal)", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md154", null ],
        [ "Quick minimal build for testing", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md155", null ]
      ] ],
      [ "Updating compile_commands.json", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md156", null ],
      [ "Migration from Old Presets", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md157", null ],
      [ "Troubleshooting", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md158", [
        [ "Warnings are still showing", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md159", null ],
        [ "clangd can't find nlohmann/json", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md160", null ],
        [ "Build fails with missing dependencies", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md161", null ]
      ] ]
    ] ],
    [ "Git Workflow and Branching Strategy", "da/dc1/md_docs_2B4-git-workflow.html", [
      [ "⚠️ Pre-1.0 Workflow (Current)", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md163", null ],
      [ "Pre-1.0 Release Strategy: Best Effort Releases", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md165", [
        [ "Core Principles", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md166", null ],
        [ "How It Works in Practice", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md167", null ]
      ] ],
      [ "📚 Full Workflow Reference (Future/Formal)", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md169", null ],
      [ "Branch Structure", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md170", [
        [ "Main Branches", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md171", [
          [ "<tt>master</tt>", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md172", null ],
          [ "<tt>develop</tt>", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md173", null ]
        ] ],
        [ "Supporting Branches", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md174", [
          [ "Feature Branches", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md175", null ],
          [ "Bugfix Branches", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md176", null ],
          [ "Hotfix Branches", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md177", null ],
          [ "Release Branches", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md178", null ],
          [ "Experimental Branches", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md179", null ]
        ] ]
      ] ],
      [ "Commit Message Conventions", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md180", [
        [ "Format", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md181", null ],
        [ "Types", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md182", null ],
        [ "Scopes (optional)", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md183", null ],
        [ "Examples", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md184", null ]
      ] ],
      [ "Pull Request Guidelines", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md185", [
        [ "PR Title", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md186", null ],
        [ "PR Description Template", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md187", null ],
        [ "PR Review Process", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md188", null ]
      ] ],
      [ "Version Numbering", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md189", [
        [ "MAJOR (e.g., 1.0.0)", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md190", null ],
        [ "MINOR (e.g., 0.4.0)", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md191", null ],
        [ "PATCH (e.g., 0.3.2)", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md192", null ],
        [ "Pre-release Tags", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md193", null ]
      ] ],
      [ "Release Process", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md194", [
        [ "For Minor/Major Releases (0.x.0, x.0.0)", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md195", null ],
        [ "For Patch Releases (0.3.x)", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md196", null ]
      ] ],
      [ "Long-Running Feature Branches", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md197", null ],
      [ "Tagging Strategy", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md198", [
        [ "Release Tags", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md199", null ],
        [ "Internal Milestones", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md200", null ]
      ] ],
      [ "Best Practices", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md201", [
        [ "DO ✅", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md202", null ],
        [ "DON'T ❌", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md203", null ]
      ] ],
      [ "Quick Reference", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md204", null ],
      [ "Emergency Procedures", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md205", [
        [ "If master is broken", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md206", null ],
        [ "If develop is broken", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md207", null ],
        [ "If release needs to be rolled back", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md208", null ]
      ] ],
      [ "🚀 Current Simplified Workflow (Pre-1.0)", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md210", [
        [ "Daily Development Pattern", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md211", null ],
        [ "When to Use Branches (Pre-1.0)", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md212", null ],
        [ "Current Branch Usage", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md213", null ],
        [ "Commit Message (Simplified)", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md214", null ],
        [ "Releases (Pre-1.0)", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md215", null ]
      ] ]
    ] ],
    [ "B5 - Architecture and Networking", "dd/de3/md_docs_2B5-architecture-and-networking.html", [
      [ "1. High-Level Architecture", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md218", null ],
      [ "2. Service Taxonomy", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md219", [
        [ "APP Services (gRPC Servers)", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md220", null ],
        [ "CLI Services (Business Logic)", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md221", null ]
      ] ],
      [ "3. gRPC Services", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md222", [
        [ "ImGuiTestHarness Service", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md223", null ],
        [ "RomService", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md224", null ],
        [ "CanvasAutomation Service", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md225", null ]
      ] ],
      [ "4. Real-Time Collaboration", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md226", [
        [ "Architecture", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md227", null ],
        [ "Core Components", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md228", null ],
        [ "WebSocket Protocol", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md229", null ]
      ] ],
      [ "5. Data Flow Example: AI Agent Edits a Tile", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md230", null ]
    ] ],
    [ "z3ed: AI-Powered CLI for YAZE", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html", [
      [ "1. Overview", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md232", [
        [ "Core Capabilities", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md233", null ]
      ] ],
      [ "2. Quick Start", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md234", [
        [ "Build", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md235", null ],
        [ "AI Setup", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md236", null ],
        [ "Example Commands", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md237", null ],
        [ "Hybrid CLI ↔ GUI Workflow", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md238", null ]
      ] ],
      [ "3. Architecture", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md239", [
        [ "System Components Diagram", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md240", null ],
        [ "Command Abstraction Layer (v0.2.1)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md241", null ]
      ] ],
      [ "4. Agentic & Generative Workflow (MCP)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md242", null ],
      [ "5. Command Reference", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md243", [
        [ "Agent Commands", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md244", null ],
        [ "Resource Commands", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md245", [
          [ "<tt>agent test</tt>: Live Harness Automation", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md246", null ]
        ] ]
      ] ],
      [ "6. Chat Modes", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md247", [
        [ "FTXUI Chat (<tt>agent chat</tt>)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md248", null ],
        [ "Simple Chat (<tt>agent simple-chat</tt>)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md249", null ],
        [ "GUI Chat Widget (Editor Integration)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md250", null ]
      ] ],
      [ "7. AI Provider Configuration", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md251", [
        [ "System Prompt Versions", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md252", null ]
      ] ],
      [ "8. Learn Command - Knowledge Management", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md253", [
        [ "Basic Usage", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md254", null ],
        [ "Project Context", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md255", null ],
        [ "Conversation Memory", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md256", null ],
        [ "Storage Location", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md257", null ]
      ] ],
      [ "9. TODO Management System", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md258", [
        [ "Core Capabilities", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md259", null ],
        [ "Storage Location", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md260", null ]
      ] ],
      [ "10. CLI Output & Help System", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md261", [
        [ "Verbose Logging", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md262", null ],
        [ "Hierarchical Help System", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md263", null ]
      ] ],
      [ "10. Collaborative Sessions & Multimodal Vision", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md264", [
        [ "Overview", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md265", null ],
        [ "Local Collaboration Mode", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md267", [
          [ "How to Use", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md268", null ],
          [ "Features", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md269", null ],
          [ "Cloud Folder Workaround", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md270", null ]
        ] ],
        [ "Network Collaboration Mode (yaze-server v2.0)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md272", [
          [ "Requirements", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md273", null ],
          [ "Server Setup", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md274", null ],
          [ "Client Connection", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md275", null ],
          [ "Core Features", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md276", null ],
          [ "Advanced Features (v2.0)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md277", null ],
          [ "Protocol Reference", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md278", null ],
          [ "Server Configuration", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md279", null ],
          [ "Database Schema (Server v2.0)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md280", null ],
          [ "Deployment", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md281", null ],
          [ "Testing", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md282", null ],
          [ "Security Considerations", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md283", null ]
        ] ],
        [ "Multimodal Vision (Gemini)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md285", [
          [ "Requirements", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md286", null ],
          [ "Capture Modes", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md287", null ],
          [ "How to Use", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md288", null ],
          [ "Example Prompts", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md289", null ]
        ] ],
        [ "Architecture", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md291", null ],
        [ "Troubleshooting", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md293", null ],
        [ "References", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md295", null ]
      ] ],
      [ "11. Roadmap & Implementation Status", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md296", [
        [ "✅ Completed", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md297", null ],
        [ "📌 Current Progress Highlights (October 5, 2025)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md298", null ],
        [ "🚧 Active & Next Steps", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md299", null ],
        [ "✅ Recently Completed (v0.2.1-alpha - October 11, 2025)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md300", [
          [ "CLI Architecture Improvements (NEW)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md301", null ],
          [ "Code Quality & Maintainability", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md302", null ]
        ] ],
        [ "✅ Previously Completed (v0.2.0-alpha - October 5, 2025)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md303", [
          [ "Core AI Features", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md304", null ],
          [ "Version Management & Protection", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md305", null ],
          [ "Networking & Collaboration (NEW)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md306", null ],
          [ "UI/UX Enhancements", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md307", null ],
          [ "Build System & Infrastructure", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md308", null ]
        ] ]
      ] ],
      [ "12. Troubleshooting", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md309", null ]
    ] ],
    [ "Testing z3ed Without ROM Files", "d7/d43/md_docs_2C2-testing-without-roms.html", [
      [ "Overview", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md311", null ],
      [ "How Mock ROM Mode Works", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md312", null ],
      [ "Usage", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md313", [
        [ "Command Line Flag", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md314", null ],
        [ "Test Suite", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md315", null ]
      ] ],
      [ "What Works with Mock ROM", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md316", [
        [ "✅ Fully Supported", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md317", null ],
        [ "⚠️ Limited Support", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md318", null ],
        [ "❌ Not Supported", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md319", null ]
      ] ],
      [ "Testing Strategy", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md320", [
        [ "For Agent Logic", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md321", null ],
        [ "For ROM Operations", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md322", null ]
      ] ],
      [ "CI/CD Integration", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md323", [
        [ "GitHub Actions Example", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md324", null ]
      ] ],
      [ "Embedded Labels Reference", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md325", null ],
      [ "Troubleshooting", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md326", [
        [ "\"No ROM loaded\" error", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md327", null ],
        [ "Mock ROM fails to initialize", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md328", null ],
        [ "Agent returns empty/wrong results", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md329", null ]
      ] ],
      [ "Development", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md330", [
        [ "Adding New Labels", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md331", null ],
        [ "Testing Mock ROM Directly", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md332", null ]
      ] ],
      [ "Best Practices", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md333", [
        [ "DO ✅", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md334", null ],
        [ "DON'T ❌", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md335", null ]
      ] ],
      [ "Related Documentation", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md336", null ]
    ] ],
    [ "Asm Style Guide", "d7/d9a/md_docs_2E1-asm-style-guide.html", [
      [ "Table of Contents", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md339", null ],
      [ "File Structure", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md340", null ],
      [ "Labels and Symbols", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md341", null ],
      [ "Comments", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md342", null ],
      [ "Instructions", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md343", null ],
      [ "Macros", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md344", null ],
      [ "Loops and Branching", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md345", null ],
      [ "Data Structures", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md346", null ],
      [ "Code Organization", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md347", null ],
      [ "Custom Code", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md348", null ]
    ] ],
    [ "E2 - Development Guide", "d5/d18/md_docs_2E2-development-guide.html", [
      [ "Editor Status", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md350", null ],
      [ "1. Core Architectural Patterns", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md351", [
        [ "Pattern 1: Modular Systems", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md352", null ],
        [ "Pattern 2: Callback-Based Communication", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md353", null ],
        [ "Pattern 3: Centralized Progressive Loading via <tt>gfx::Arena</tt>", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md354", null ]
      ] ],
      [ "2. UI & Theming System", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md355", [
        [ "2.1. The Theme System (<tt>AgentUITheme</tt>)", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md356", null ],
        [ "2.2. Reusable UI Helper Functions", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md357", null ],
        [ "2.3. Toolbar Implementation (<tt>CompactToolbar</tt>)", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md358", null ]
      ] ],
      [ "3. Key System Implementations & Gotchas", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md359", [
        [ "3.1. Graphics Refresh Logic", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md360", null ],
        [ "3.2. Multi-Area Map Configuration", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md361", null ],
        [ "3.3. Version-Specific Feature Gating", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md362", null ],
        [ "3.4. Entity Visibility for Visual Testing", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md363", null ]
      ] ],
      [ "4. Debugging and Testing", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md364", [
        [ "4.1. Quick Debugging with Startup Flags", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md365", null ],
        [ "4.2. Testing Strategies", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md366", null ]
      ] ],
      [ "5. Command-Line Flag Standardization", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md367", [
        [ "Rationale", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md368", null ],
        [ "Migration Plan", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md369", null ],
        [ "3.6. Graphics Sheet Management", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md371", null ],
        [ "Naming Conventions", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md372", null ]
      ] ]
    ] ],
    [ "API Reference", "d8/d73/md_docs_2E3-api-reference.html", [
      [ "C API (<tt>incl/yaze.h</tt>)", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md374", [
        [ "Core Library Functions", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md375", null ],
        [ "ROM Operations", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md376", null ]
      ] ],
      [ "C++ API", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md377", [
        [ "AsarWrapper (<tt>src/app/core/asar_wrapper.h</tt>)", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md378", [
          [ "CLI Examples (<tt>z3ed</tt>)", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md379", null ],
          [ "C++ API Example", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md380", null ],
          [ "Class Definition", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md381", null ]
        ] ]
      ] ],
      [ "Data Structures", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md382", [
        [ "<tt>snes_color</tt>", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md383", null ],
        [ "<tt>zelda3_message</tt>", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md384", null ]
      ] ],
      [ "Error Handling", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md385", [
        [ "C API Error Pattern", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md386", null ]
      ] ]
    ] ],
    [ "E4 - Emulator Development Guide", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html", [
      [ "Table of Contents", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md389", null ],
      [ "1. Current Status", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md391", [
        [ "🎉 Major Breakthrough: Game is Running!", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md392", null ],
        [ "✅ Confirmed Working", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md393", null ],
        [ "🔧 Known Issues (Non-Critical)", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md394", null ]
      ] ],
      [ "2. How to Use the Emulator", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md396", [
        [ "Method 1: Main Yaze Application (GUI)", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md397", null ],
        [ "Method 2: Standalone Emulator (<tt>yaze_emu</tt>)", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md398", null ],
        [ "Method 3: Dungeon Object Emulator Preview", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md399", null ]
      ] ],
      [ "3. Architecture", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md401", [
        [ "Memory System", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md402", [
          [ "SNES Memory Map", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md403", null ]
        ] ],
        [ "CPU-APU-SPC700 Interaction", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md404", null ],
        [ "Component Architecture", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md405", null ]
      ] ],
      [ "4. The Debugging Journey: Critical Fixes", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md407", [
        [ "SPC700 & APU Fixes", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md408", null ],
        [ "The Critical Pattern for Multi-Step Instructions", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md409", null ]
      ] ],
      [ "5. Display & Performance Improvements", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md411", [
        [ "PPU Color Display Fix", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md412", null ],
        [ "Frame Timing & Speed Control", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md413", null ],
        [ "Performance Optimizations", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md414", [
          [ "Frame Skipping", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md415", null ],
          [ "Audio Buffer Management", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md416", null ],
          [ "Performance Gains", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md417", null ]
        ] ],
        [ "ROM Loading Improvements", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md418", null ]
      ] ],
      [ "6. Advanced Features", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md420", [
        [ "Professional Disassembly Viewer", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md421", [
          [ "Architecture", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md422", null ],
          [ "Visual Features", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md423", null ],
          [ "Interactive Elements", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md424", null ],
          [ "UI Features", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md425", null ],
          [ "Performance", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md426", null ]
        ] ],
        [ "Breakpoint System", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md427", null ],
        [ "UI/UX Enhancements", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md428", null ]
      ] ],
      [ "7. Emulator Preview Tool", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md430", [
        [ "Purpose", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md431", null ],
        [ "Critical Fixes Applied", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md432", [
          [ "1. Memory Access Fix (SIGSEGV Crash)", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md433", null ],
          [ "2. RTL vs RTS Fix (Timeout)", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md434", null ],
          [ "3. Palette Validation", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md435", null ],
          [ "4. PPU Configuration", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md436", null ]
        ] ],
        [ "How to Use", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md437", null ],
        [ "What You'll Learn", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md438", null ],
        [ "Reverse Engineering Workflow", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md439", null ],
        [ "UI Enhancements", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md440", null ]
      ] ],
      [ "8. Logging System", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md442", [
        [ "How to Enable", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md443", null ]
      ] ],
      [ "9. Testing", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md445", [
        [ "Unit Tests", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md446", null ],
        [ "Standalone Emulator", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md447", null ],
        [ "Running Tests", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md448", null ],
        [ "Testing Checklist", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md449", null ]
      ] ],
      [ "10. Technical Reference", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md451", [
        [ "PPU Registers", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md452", null ],
        [ "CPU Instructions", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md453", null ],
        [ "Color Format", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md454", null ],
        [ "Performance Metrics", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md455", null ]
      ] ],
      [ "11. Troubleshooting", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md457", [
        [ "Emulator Preview Issues", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md458", null ],
        [ "Color Display Issues", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md459", null ],
        [ "Performance Issues", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md460", null ],
        [ "Build Issues", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md461", null ]
      ] ],
      [ "11.5 Audio System Architecture (October 2025)", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md463", [
        [ "Overview", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md464", null ],
        [ "Audio Backend Abstraction", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md465", null ],
        [ "APU Handshake Debugging System", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md466", null ],
        [ "IPL ROM Handshake Protocol", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md467", null ],
        [ "Music Editor Integration", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md468", null ],
        [ "Audio Testing & Diagnostics", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md469", null ],
        [ "Future Enhancements", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md470", null ]
      ] ],
      [ "12. Next Steps & Roadmap", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md472", [
        [ "🎯 Immediate Priorities (Critical Path to Full Functionality)", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md473", null ],
        [ "🚀 Enhancement Priorities (After Core is Stable)", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md474", null ],
        [ "📝 Technical Debt", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md475", null ],
        [ "Long-Term Enhancements", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md476", null ]
      ] ],
      [ "13. Build Instructions", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md478", [
        [ "Quick Build", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md479", null ],
        [ "Platform-Specific", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md480", null ],
        [ "Build Optimizations", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md481", null ]
      ] ],
      [ "File Reference", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md483", [
        [ "Core Emulation", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md484", null ],
        [ "Debugging", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md485", null ],
        [ "UI", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md486", null ],
        [ "Core", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md487", null ],
        [ "Testing", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md488", null ]
      ] ],
      [ "Status Summary", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md490", [
        [ "✅ Production Ready", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md491", null ]
      ] ]
    ] ],
    [ "E5 - Debugging and Testing Guide", "de/dc5/md_docs_2E5-debugging-guide.html", [
      [ "1. Standardized Logging for Print Debugging", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md494", [
        [ "Log Levels and Usage", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md495", null ],
        [ "Log Categories", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md496", null ],
        [ "Enabling and Configuring Logs via CLI", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md497", null ],
        [ "2. Command-Line Workflows for Testing", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md500", [
          [ "Launching the GUI for Specific Tasks", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md501", null ]
        ] ]
      ] ],
      [ "Open the Dungeon Editor with the Room Matrix and two specific room cards", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md502", null ],
      [ "Available editors: Assembly, Dungeon, Graphics, Music, Overworld, Palette,", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md503", null ],
      [ "Screen, Sprite, Message, Hex, Agent, Settings", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md504", null ],
      [ "Dungeon editor cards: Rooms List, Room Matrix, Entrances List, Room Graphics,", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md505", null ],
      [ "Object Editor, Palette Editor, Room N (where N is room ID)", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md506", null ],
      [ "Fast dungeon room testing", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md507", null ],
      [ "Compare multiple rooms side-by-side", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md508", null ],
      [ "Full dungeon workspace with all tools", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md509", null ],
      [ "Jump straight to overworld editing", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md510", null ],
      [ "Run only fast, dependency-free unit tests", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md512", null ],
      [ "Run tests that require a ROM file", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md513", [
        [ "3. GUI Automation for AI Agents", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md517", [
          [ "Inspecting ROMs with <tt>z3ed</tt>", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md515", null ],
          [ "Architecture Overview", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md518", null ],
          [ "Step-by-Step Workflow for AI", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md519", [
            [ "Step 1: Launch <tt>yaze</tt> with the Test Harness", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md520", null ],
            [ "Step 2: Discover UI Elements", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md521", null ],
            [ "Step 3: Record or Write a Test Script", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md522", null ]
          ] ]
        ] ]
      ] ],
      [ "Start yaze with the room already open", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md523", null ],
      [ "Then your test script just needs to validate the state", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md524", [
        [ "4. Advanced Debugging Tools", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md527", null ]
      ] ]
    ] ],
    [ "Emulator Core Improvements Roadmap", "d3/d49/md_docs_2E6-emulator-improvements.html", [
      [ "Overview", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md529", null ],
      [ "Critical Priority: APU Timing Fix", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md531", [
        [ "Problem Statement", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md532", null ],
        [ "Root Cause: CPU-APU Handshake Timing", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md533", [
          [ "The Handshake Protocol", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md534", null ],
          [ "Point of Failure", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md535", null ]
        ] ],
        [ "Technical Analysis", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md536", [
          [ "Issue 1: Incomplete Opcode Timing", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md537", null ],
          [ "Issue 2: Fragile Multi-Step Execution Model", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md538", null ],
          [ "Issue 3: Floating-Point Precision", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md539", null ]
        ] ],
        [ "Proposed Solution: Cycle-Accurate Refactoring", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md540", [
          [ "Step 1: Implement Cycle-Accurate Instruction Execution", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md541", null ],
          [ "Step 2: Centralize the APU Execution Loop", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md542", null ],
          [ "Step 3: Use Integer-Based Cycle Ratios", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md543", null ]
        ] ]
      ] ],
      [ "High Priority: Core Architecture & Timing Model", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md545", [
        [ "CPU Cycle Counting", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md546", null ],
        [ "Main Synchronization Loop", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md547", null ]
      ] ],
      [ "Medium Priority: PPU Performance", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md549", [
        [ "Rendering Approach Optimization", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md550", null ]
      ] ],
      [ "Low Priority: Code Quality & Refinements", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md552", [
        [ "APU Code Modernization", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md553", null ],
        [ "Audio Subsystem & Buffering", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md554", null ],
        [ "Debugger & Tooling Optimizations", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md555", [
          [ "DisassemblyViewer Data Structure", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md556", null ],
          [ "BreakpointManager Lookups", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md557", null ]
        ] ]
      ] ],
      [ "Implementation Priority", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md559", null ],
      [ "Success Metrics", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md561", [
        [ "APU Timing Fix Success", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md562", null ],
        [ "Overall Emulation Accuracy", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md563", null ],
        [ "Performance Targets", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md564", null ]
      ] ],
      [ "Related Documentation", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md566", null ]
    ] ],
    [ "YAZE Startup Debugging Flags", "db/da6/md_docs_2E7-debugging-startup-flags.html", [
      [ "Basic Usage", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md569", null ],
      [ "Available Flags", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md570", [
        [ "<tt>--rom_file</tt>", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md571", null ],
        [ "<tt>--debug</tt>", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md572", null ],
        [ "<tt>--editor</tt>", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md573", null ],
        [ "<tt>--cards</tt>", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md574", null ]
      ] ],
      [ "Common Debugging Scenarios", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md575", [
        [ "1. Quick Dungeon Room Testing", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md576", null ],
        [ "2. Multiple Room Comparison", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md577", null ],
        [ "3. Full Dungeon Editor Workspace", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md578", null ],
        [ "4. Debug Mode with Logging", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md579", null ],
        [ "5. Quick Overworld Editing", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md580", null ]
      ] ],
      [ "gRPC Test Harness (Developer Feature)", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md581", null ],
      [ "Combining Flags", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md582", null ],
      [ "Notes", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md583", null ],
      [ "Troubleshooting", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md584", null ]
    ] ],
    [ "YAZE Emulator Enhancement Roadmap", "df/d0c/md_docs_2E8-emulator-debugging-vision.html", [
      [ "📋 Executive Summary", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md587", [
        [ "Core Objectives", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md588", null ]
      ] ],
      [ "🎯 Current State Analysis", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md590", [
        [ "What Works ✅", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md591", null ],
        [ "What's Broken ❌", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md592", null ],
        [ "What's Missing 🚧", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md593", null ]
      ] ],
      [ "🔧 Phase 1: Audio System Fix (Priority: CRITICAL)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md595", [
        [ "Problem Analysis", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md596", null ],
        [ "Investigation Steps", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md597", null ],
        [ "Likely Fixes", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md598", null ],
        [ "Quick Win Actions", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md599", null ]
      ] ],
      [ "🐛 Phase 2: Advanced Debugger (Mesen2 Feature Parity)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md601", [
        [ "Feature Comparison: YAZE vs Mesen2", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md602", null ],
        [ "2.1 Breakpoint System", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md603", null ],
        [ "2.2 Memory Watchpoints", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md605", null ],
        [ "2.3 Live Disassembly Viewer", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md607", null ],
        [ "2.4 Enhanced Memory Viewer", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md609", null ]
      ] ],
      [ "🚀 Phase 3: Performance Optimizations", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md611", [
        [ "3.1 Cycle-Accurate Timing", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md612", null ],
        [ "3.2 Dynamic Recompilation (Dynarec)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md614", null ],
        [ "3.3 Frame Pacing Improvements", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md616", null ]
      ] ],
      [ "🎮 Phase 4: SPC700 Audio CPU Debugger", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md618", [
        [ "4.1 APU Inspector Window", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md619", null ],
        [ "4.2 Audio Sample Export", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md621", null ]
      ] ],
      [ "🤖 Phase 5: z3ed AI Agent Integration", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md623", [
        [ "5.1 Emulator State Access", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md624", null ],
        [ "5.2 Automated Test Generation", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md626", null ],
        [ "5.3 Memory Map Learning", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md628", null ]
      ] ],
      [ "📊 Phase 6: Performance Profiling", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md630", [
        [ "6.1 Cycle Counter & Profiler", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md631", null ],
        [ "6.2 Frame Time Analysis", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md633", null ]
      ] ],
      [ "🎯 Phase 7: Event System & Timeline", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md635", [
        [ "7.1 Event Logger", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md636", null ]
      ] ],
      [ "🧠 Phase 8: AI-Powered Debugging", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md638", [
        [ "8.1 Intelligent Crash Analysis", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md639", null ],
        [ "8.2 Automated Bug Reproduction", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md641", null ]
      ] ],
      [ "🗺️ Implementation Roadmap", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md643", [
        [ "Sprint 1: Audio Fix (Week 1)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md644", null ],
        [ "Sprint 2: Basic Debugger (Weeks 2-3)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md645", null ],
        [ "Sprint 3: SPC700 Debugger (Week 4)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md646", null ],
        [ "Sprint 4: AI Integration (Weeks 5-6)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md647", null ],
        [ "Sprint 5: Performance (Weeks 7-8)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md648", null ]
      ] ],
      [ "🔬 Technical Deep Dives", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md650", [
        [ "Audio System Architecture (SDL2)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md651", null ],
        [ "Memory Regions Reference", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md653", null ]
      ] ],
      [ "🎮 Phase 9: Advanced Features (Mesen2 Parity)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md655", [
        [ "9.1 Rewind Feature", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md656", null ],
        [ "9.2 TAS (Tool-Assisted Speedrun) Input Recording", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md658", null ],
        [ "9.3 Comparison Mode", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md660", null ]
      ] ],
      [ "🛠️ Optimization Summary", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md662", [
        [ "Quick Wins (< 1 week)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md663", null ],
        [ "Medium Term (1-2 months)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md664", null ],
        [ "Long Term (3-6 months)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md665", null ]
      ] ],
      [ "🤖 z3ed Agent Emulator Tools", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md667", [
        [ "New Tool Categories", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md668", null ],
        [ "Example AI Conversations", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md669", null ]
      ] ],
      [ "📁 File Structure for New Features", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md671", null ],
      [ "🎨 UI Mockups", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md673", [
        [ "Debugger Layout (ImGui)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md674", null ],
        [ "APU Debugger Layout", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md675", null ]
      ] ],
      [ "🚀 Performance Targets", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md677", [
        [ "Current Performance", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md678", null ],
        [ "Target Performance (Post-Optimization)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md679", null ],
        [ "Optimization Strategy Priority", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md680", null ]
      ] ],
      [ "🧪 Testing Integration", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md682", [
        [ "Automated Emulator Tests (z3ed)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md683", null ]
      ] ],
      [ "🔌 z3ed Agent + Emulator Integration", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md685", [
        [ "New Agent Tools", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md686", null ]
      ] ],
      [ "🎓 Learning from Mesen2", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md688", [
        [ "What Makes Mesen2 Great", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md689", null ],
        [ "Our Unique Advantages", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md690", null ]
      ] ],
      [ "📊 Resource Requirements", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md692", [
        [ "Development Time Estimates", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md693", null ],
        [ "Memory Requirements", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md694", null ],
        [ "CPU Requirements", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md695", null ]
      ] ],
      [ "🛣️ Recommended Implementation Order", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md697", [
        [ "Month 1: Foundation", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md698", null ],
        [ "Month 2: Audio & Events", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md699", null ],
        [ "Month 3: AI Integration", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md700", null ],
        [ "Month 4: Performance", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md701", null ],
        [ "Month 5: Polish", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md702", null ]
      ] ],
      [ "🔮 Future Vision: AI-Powered ROM Hacking", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md704", [
        [ "The Ultimate Workflow", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md705", null ]
      ] ],
      [ "🐛 Appendix A: Audio Debugging Checklist", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md707", [
        [ "Check 1: Device Status", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md708", null ],
        [ "Check 2: Queue Size", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md709", null ],
        [ "Check 3: Sample Validation", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md710", null ],
        [ "Check 4: Buffer Allocation", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md711", null ],
        [ "Check 5: SPC700 Execution", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md712", null ],
        [ "Quick Fixes to Try", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md713", null ]
      ] ],
      [ "📝 Appendix B: Mesen2 Feature Reference", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md715", [
        [ "Debugger Windows (Inspiration)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md716", null ],
        [ "Event Types Tracked", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md717", null ],
        [ "Trace Logger Format", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md718", null ]
      ] ],
      [ "🎯 Success Criteria", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md720", [
        [ "Phase 1 Complete When:", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md721", null ],
        [ "Phase 2 Complete When:", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md722", null ],
        [ "Phase 3 Complete When:", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md723", null ],
        [ "Phase 4 Complete When:", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md724", null ],
        [ "Phase 5 Complete When:", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md725", null ]
      ] ],
      [ "🎓 Learning Resources", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md727", [
        [ "SNES Emulation", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md728", null ],
        [ "Audio Debugging", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md729", null ],
        [ "Performance Optimization", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md730", null ]
      ] ],
      [ "🙏 Credits & Acknowledgments", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md732", null ]
    ] ],
    [ "F2: Dungeon Editor v2 - Complete Guide", "d5/d83/md_docs_2F1-dungeon-editor-guide.html", [
      [ "Overview", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md736", [
        [ "Key Features", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md737", null ],
        [ "Architecture Improvements ✅", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md739", null ],
        [ "UI Improvements ✅", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md740", null ]
      ] ],
      [ "Architecture", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md742", [
        [ "Component Overview", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md743", null ],
        [ "Room Rendering Pipeline", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md744", null ],
        [ "Room Structure (Bottom to Top)", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md745", null ]
      ] ],
      [ "Next Development Steps", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md747", [
        [ "High Priority (Must Do)", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md748", [
          [ "1. Door Rendering at Room Edges", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md749", null ],
          [ "2. Object Name Labels from String Array", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md751", null ],
          [ "4. Fix Plus Button to Select Any Room", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md753", null ]
        ] ],
        [ "Medium Priority (Should Do)", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md755", [
          [ "6. Fix InputHexByte +/- Button Events", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md756", null ]
        ] ],
        [ "Lower Priority (Nice to Have)", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md757", [
          [ "9. Move Backend Logic to DungeonEditorSystem", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md758", null ]
        ] ]
      ] ],
      [ "Quick Start", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md760", [
        [ "Build & Run", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md761", null ]
      ] ],
      [ "Testing & Verification", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md763", [
        [ "Debug Commands", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md764", null ]
      ] ],
      [ "Related Documentation", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md765", null ]
    ] ],
    [ "Tile16 Editor Palette System", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html", [
      [ "Executive Summary", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md768", null ],
      [ "Problem Analysis", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md769", [
        [ "Critical Issues Identified", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md770", null ]
      ] ],
      [ "Solution Architecture", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md771", [
        [ "Core Design Principles", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md772", null ],
        [ "256-Color Overworld Palette Structure", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md773", null ],
        [ "Sheet-to-Palette Mapping", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md774", null ],
        [ "Palette Button Mapping", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md775", null ]
      ] ],
      [ "Implementation Details", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md776", [
        [ "1. Fixed SetPaletteWithTransparent Method", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md777", null ],
        [ "2. Corrected Tile16 Editor Palette System", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md778", null ],
        [ "3. Palette Coordination Flow", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md779", null ]
      ] ],
      [ "UI/UX Refactoring", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md780", [
        [ "New Three-Column Layout", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md781", null ],
        [ "Canvas Context Menu Fixes", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md782", null ],
        [ "Dynamic Zoom Controls", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md783", null ]
      ] ],
      [ "Testing Protocol", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md784", [
        [ "Crash Prevention Testing", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md785", null ],
        [ "Color Alignment Testing", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md786", null ],
        [ "UI/UX Testing", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md787", null ]
      ] ],
      [ "Error Handling", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md788", [
        [ "Bounds Checking", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md789", null ],
        [ "Fallback Mechanisms", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md790", null ]
      ] ],
      [ "Debug Information Display", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md791", null ],
      [ "Known Issues and Ongoing Work", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md792", [
        [ "Completed Items ✅", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md793", null ],
        [ "Active Issues ⚠️", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md794", null ],
        [ "Current Status Summary", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md795", null ],
        [ "Future Enhancements", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md796", null ]
      ] ],
      [ "Maintenance Notes", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md797", null ],
      [ "Next Steps", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md799", [
        [ "Immediate Priorities", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md800", null ],
        [ "Investigation Areas", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md801", null ]
      ] ]
    ] ],
    [ "Overworld Loading Guide", "d9/d85/md_docs_2F3-overworld-loading.html", [
      [ "Table of Contents", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md804", null ],
      [ "Overview", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md805", null ],
      [ "ROM Types and Versions", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md806", [
        [ "Version Detection", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md807", null ],
        [ "Feature Support by Version", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md808", null ]
      ] ],
      [ "Overworld Map Structure", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md809", [
        [ "Core Properties", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md810", null ]
      ] ],
      [ "Overlays and Special Area Maps", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md811", [
        [ "Understanding Overlays", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md812", null ],
        [ "Special Area Maps (0x80-0x9F)", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md813", null ],
        [ "Overlay ID Mappings", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md814", null ],
        [ "Drawing Order", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md815", null ],
        [ "Vanilla Overlay Loading", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md816", null ],
        [ "Special Area Graphics Loading", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md817", null ]
      ] ],
      [ "Loading Process", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md818", [
        [ "1. Version Detection", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md819", null ],
        [ "2. Map Initialization", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md820", null ],
        [ "3. Property Loading", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md821", [
          [ "Vanilla ROMs (asm_version == 0xFF)", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md822", null ],
          [ "ZSCustomOverworld v2/v3", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md823", null ]
        ] ],
        [ "4. Custom Data Loading", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md824", null ]
      ] ],
      [ "ZScream Implementation", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md825", [
        [ "OverworldMap Constructor", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md826", null ],
        [ "Key Methods", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md827", null ]
      ] ],
      [ "Yaze Implementation", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md828", [
        [ "OverworldMap Constructor", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md829", null ],
        [ "Key Methods", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md830", null ],
        [ "Current Status", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md831", null ]
      ] ],
      [ "Key Differences", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md832", [
        [ "1. Language and Architecture", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md833", null ],
        [ "2. Data Structures", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md834", null ],
        [ "3. Error Handling", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md835", null ],
        [ "4. Graphics Processing", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md836", null ]
      ] ],
      [ "Common Issues and Solutions", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md837", [
        [ "1. Version Detection Issues", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md838", null ],
        [ "2. Palette Loading Errors", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md839", null ],
        [ "3. Graphics Not Loading", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md840", null ],
        [ "4. Overlay Issues", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md841", null ],
        [ "5. Large Map Problems", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md842", null ],
        [ "6. Special Area Graphics Issues", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md843", null ]
      ] ],
      [ "Best Practices", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md844", [
        [ "1. Version-Specific Code", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md845", null ],
        [ "2. Error Handling", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md846", null ],
        [ "3. Memory Management", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md847", null ],
        [ "4. Thread Safety", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md848", null ]
      ] ],
      [ "Conclusion", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md849", null ]
    ] ],
    [ "Overworld Agent Guide - AI-Powered Overworld Editing", "de/d00/md_docs_2F4-overworld-agent-guide.html", [
      [ "Overview", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md852", null ],
      [ "Quick Start", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md854", [
        [ "Prerequisites", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md855", null ],
        [ "First Agent Interaction", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md856", null ]
      ] ],
      [ "Available Tools", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md858", [
        [ "Read-Only Tools (Safe for AI)", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md859", [
          [ "overworld-get-tile", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md860", null ],
          [ "overworld-get-visible-region", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md862", null ],
          [ "overworld-analyze-region", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md864", null ]
        ] ],
        [ "Write Tools (Sandboxed - Creates Proposals)", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md866", [
          [ "overworld-set-tile", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md867", null ],
          [ "overworld-set-tiles-batch", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md869", null ]
        ] ]
      ] ],
      [ "Multimodal Vision Workflow", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md871", [
        [ "Step 1: Capture Canvas Screenshot", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md872", null ],
        [ "Step 2: AI Analyzes Screenshot", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md873", null ],
        [ "Step 3: Generate Edit Plan", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md874", null ],
        [ "Step 4: Execute Plan (Sandbox)", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md875", null ],
        [ "Step 5: Human Review", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md876", null ]
      ] ],
      [ "Example Workflows", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md878", [
        [ "Workflow 1: Create Forest Area", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md879", null ],
        [ "Workflow 2: Fix Tile Placement Errors", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md881", null ],
        [ "Workflow 3: Generate Path", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md883", null ]
      ] ],
      [ "Common Tile IDs Reference", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md885", [
        [ "Grass & Ground", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md886", null ],
        [ "Trees & Plants", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md887", null ],
        [ "Water", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md888", null ],
        [ "Paths & Roads", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md889", null ],
        [ "Structures", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md890", null ]
      ] ],
      [ "Best Practices for AI Agents", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md892", [
        [ "1. Always Analyze Before Editing", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md893", null ],
        [ "2. Use Batch Operations", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md894", null ],
        [ "3. Provide Clear Reasoning", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md895", null ],
        [ "4. Respect Tile Boundaries", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md896", null ],
        [ "5. Check Visibility", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md897", null ],
        [ "6. Create Reversible Edits", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md898", null ]
      ] ],
      [ "Error Handling", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md900", [
        [ "\"Tile ID out of range\"", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md901", null ],
        [ "\"Coordinates out of bounds\"", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md902", null ],
        [ "\"Proposal rejected\"", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md903", null ],
        [ "\"ROM file locked\"", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md904", null ]
      ] ],
      [ "Testing AI-Generated Edits", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md906", [
        [ "Manual Testing", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md907", null ],
        [ "Automated Testing", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md908", null ]
      ] ],
      [ "Advanced Techniques", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md910", [
        [ "Technique 1: Pattern Recognition", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md911", null ],
        [ "Technique 2: Style Transfer", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md912", null ],
        [ "Technique 3: Procedural Generation", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md913", null ]
      ] ],
      [ "Integration with GUI Automation", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md915", [
        [ "Record Human Edits", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md916", null ],
        [ "Replay for AI Training", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md917", null ],
        [ "Validate AI Edits", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md918", null ]
      ] ],
      [ "Collaboration Features", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md920", [
        [ "Network Collaboration", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md921", null ],
        [ "Proposal Voting", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md922", null ]
      ] ],
      [ "Troubleshooting", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md924", [
        [ "Agent Not Responding", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md925", null ],
        [ "Tools Not Available", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md926", null ],
        [ "gRPC Connection Failed", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md927", null ]
      ] ],
      [ "See Also", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md929", null ]
    ] ],
    [ "Canvas System Overview", "d1/dc6/md_docs_2G1-canvas-guide.html", [
      [ "Canvas Architecture", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md931", null ],
      [ "Core API Patterns", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md932", null ],
      [ "Context Menu Sections", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md933", null ],
      [ "Interaction Modes & Capabilities", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md934", null ],
      [ "Debug & Diagnostics", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md935", null ],
      [ "Automation API", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md936", null ],
      [ "Integration Steps for Editors", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md937", null ],
      [ "Migration Checklist", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md938", null ],
      [ "Testing Notes", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md939", null ]
    ] ],
    [ "SDL2 to SDL3 Migration and Rendering Abstraction Plan", "d6/df2/md_docs_2G2-renderer-migration-plan.html", [
      [ "1. Introduction", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md941", null ],
      [ "2. Current State Analysis", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md942", null ],
      [ "3. Proposed Architecture: The <tt>Renderer</tt> Abstraction", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md943", [
        [ "3.1. The <tt>IRenderer</tt> Interface", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md944", null ],
        [ "3.2. The <tt>SDL2Renderer</tt> Implementation", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md945", null ]
      ] ],
      [ "4. Migration Plan", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md946", [
        [ "Phase 1: Implement the Abstraction Layer", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md947", null ],
        [ "Phase 2: Migrate to SDL3", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md948", null ],
        [ "Phase 3: Support for Multiple Rendering Backends", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md949", null ]
      ] ],
      [ "5. Conclusion", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md950", null ]
    ] ],
    [ "SNES Palette System Overview", "da/dfd/md_docs_2G3-palete-system-overview.html", [
      [ "Understanding SNES Color and Palette Organization", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md952", [
        [ "Core Concepts", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md953", [
          [ "1. SNES Color Format (15-bit BGR555)", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md954", null ],
          [ "2. Palette Groups in Zelda 3", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md955", null ]
        ] ],
        [ "Dungeon Palette System", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md956", [
          [ "Structure", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md957", null ],
          [ "Usage", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md958", null ],
          [ "Color Distribution (90 colors)", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md959", null ]
        ] ],
        [ "Overworld Palette System", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md960", [
          [ "Structure", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md961", null ],
          [ "3BPP Graphics and Left/Right Palettes", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md962", null ]
        ] ],
        [ "Common Issues and Solutions", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md963", [
          [ "Issue 1: Empty Palette", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md964", null ],
          [ "Issue 2: Bitmap Corruption", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md965", null ],
          [ "Issue 3: ROM Not Loaded in Preview", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md966", null ]
        ] ],
        [ "Palette Editor Integration", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md967", [
          [ "Key Functions for UI", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md968", null ],
          [ "Palette Widget Requirements", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md969", null ]
        ] ],
        [ "Graphics Manager Integration", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md970", [
          [ "Sheet Palette Assignment", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md971", null ]
        ] ],
        [ "Best Practices", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md972", null ],
        [ "ROM Addresses (for reference)", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md973", null ]
      ] ],
      [ "Graphics Sheet Palette Application", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md974", [
        [ "Default Palette Assignment", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md975", null ],
        [ "Palette Update Workflow", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md976", null ],
        [ "Common Pitfalls", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md977", null ]
      ] ]
    ] ],
    [ "Graphics Renderer Migration - Complete Documentation", "d5/dc8/md_docs_2G3-renderer-migration-complete.html", [
      [ "📋 Executive Summary", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md980", [
        [ "Key Achievements", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md981", null ]
      ] ],
      [ "🎯 Migration Goals & Results", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md983", null ],
      [ "🏗️ Architecture Overview", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md985", [
        [ "Before: Singleton Pattern", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md986", null ],
        [ "After: Dependency Injection + Deferred Queue", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md987", null ]
      ] ],
      [ "📦 Component Details", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md989", [
        [ "1. IRenderer Interface (<tt>src/app/gfx/backend/irenderer.h</tt>)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md990", null ],
        [ "2. SDL2Renderer (<tt>src/app/gfx/backend/sdl2_renderer.{h,cc}</tt>)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md992", null ],
        [ "3. Arena Deferred Texture Queue (<tt>src/app/gfx/arena.{h,cc}</tt>)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md994", null ],
        [ "4. Bitmap Palette Refactoring (<tt>src/app/gfx/bitmap.{h,cc}</tt>)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md996", null ],
        [ "5. Canvas Optional Renderer (<tt>src/app/gui/canvas.{h,cc}</tt>)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md998", null ],
        [ "6. Tilemap Texture Queue Integration (<tt>src/app/gfx/tilemap.cc</tt>)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1000", null ]
      ] ],
      [ "🔄 Dependency Injection Flow", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1002", [
        [ "Controller → EditorManager → Editors", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1003", null ]
      ] ],
      [ "⚡ Performance Optimizations", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1005", [
        [ "1. Batched Texture Processing", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1006", null ],
        [ "2. Frame Rate Limiting", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1007", null ],
        [ "3. Auto-Pause on Focus Loss", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1008", null ],
        [ "4. Surface/Texture Pooling", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1009", null ]
      ] ],
      [ "🗺️ Migration Map: File Changes", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1011", [
        [ "Core Architecture Files (New)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1012", null ],
        [ "Core Modified Files (Major)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1013", null ],
        [ "Editor Files (Renderer Injection)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1014", null ],
        [ "Emulator Files (Special Handling)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1015", null ],
        [ "GUI/Widget Files", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1016", null ],
        [ "Test Files (Updated for DI)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1017", null ]
      ] ],
      [ "🔧 Critical Fixes Applied", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1019", [
        [ "1. Bitmap::SetPalette() Crash", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1020", null ],
        [ "2. SDL2Renderer::UpdateTexture() SIGSEGV", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1022", null ],
        [ "3. Emulator Audio System Corruption", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1024", null ],
        [ "4. Emulator Cleanup During Shutdown", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1026", null ],
        [ "5. Controller/CreateWindow Initialization Order", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1028", null ]
      ] ],
      [ "🎨 Canvas Refactoring", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1030", [
        [ "The Challenge", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1031", null ],
        [ "The Solution: Backwards-Compatible Dual API", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1032", null ]
      ] ],
      [ "🧪 Testing Strategy", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1034", [
        [ "Test Files Updated", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1035", null ]
      ] ],
      [ "🛣️ Road to SDL3", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1037", [
        [ "Why This Migration Matters", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1038", null ],
        [ "Our Abstraction Layer Handles This", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1039", null ]
      ] ],
      [ "📊 Performance Benchmarks", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1041", [
        [ "Texture Loading Performance", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1042", null ],
        [ "Graphics Editor Performance", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1043", null ],
        [ "Emulator Performance", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1044", null ]
      ] ],
      [ "🐛 Bugs Fixed During Migration", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1046", [
        [ "Critical Crashes", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1047", null ],
        [ "Build Errors", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1048", null ]
      ] ],
      [ "💡 Key Design Patterns Used", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1050", [
        [ "1. Dependency Injection", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1051", null ],
        [ "2. Command Pattern (Deferred Queue)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1052", null ],
        [ "3. RAII (Resource Management)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1053", null ],
        [ "4. Adapter Pattern (Backend Abstraction)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1054", null ],
        [ "5. Singleton with DI (Arena)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1055", null ]
      ] ],
      [ "🔮 Future Enhancements", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1057", [
        [ "Short Term (SDL2)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1058", null ],
        [ "Medium Term (SDL3 Prep)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1059", null ],
        [ "Long Term (SDL3 Migration)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1060", null ]
      ] ],
      [ "📝 Lessons Learned", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1062", [
        [ "What Went Well", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1063", null ],
        [ "Challenges Overcome", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1064", null ],
        [ "Best Practices Established", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1065", null ]
      ] ],
      [ "🎓 Technical Deep Dive: Texture Queue System", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1067", [
        [ "Why Deferred Rendering?", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1068", null ],
        [ "Queue Processing Algorithm", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1069", null ]
      ] ],
      [ "🏆 Success Metrics", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1071", [
        [ "Build Health", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1072", null ],
        [ "Runtime Stability", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1073", null ],
        [ "Performance", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1074", null ],
        [ "Code Quality", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1075", null ]
      ] ],
      [ "📚 References", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1077", [
        [ "Related Documents", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1078", null ],
        [ "Key Commits", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1079", null ],
        [ "External Resources", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1080", null ]
      ] ],
      [ "🙏 Acknowledgments", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1082", null ],
      [ "🎉 Conclusion", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1084", null ],
      [ "🚧 Known Issues & Next Steps", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1086", [
        [ "macOS-Specific Issues (Not Renderer-Related)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1087", null ],
        [ "Stability Improvements for Next Session", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1088", [
          [ "High Priority", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1089", null ],
          [ "Medium Priority", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1090", null ],
          [ "Low Priority (SDL3 Migration)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1091", null ]
        ] ],
        [ "Testing Recommendations", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1092", null ]
      ] ],
      [ "🎵 Final Notes", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1094", null ]
    ] ],
    [ "Canvas Coordinate Synchronization and Scroll Fix", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html", [
      [ "Problem Summary", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1097", null ],
      [ "Root Cause", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1098", [
        [ "Issue 1: Wrong Coordinate System (Line 1041)", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1099", null ],
        [ "Issue 2: Hover Position Not Updated (Line 416)", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1100", null ]
      ] ],
      [ "Technical Details", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1101", [
        [ "Coordinate Spaces", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1102", null ],
        [ "Usage Patterns", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1103", null ]
      ] ],
      [ "Testing", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1104", [
        [ "Visual Testing", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1105", null ],
        [ "Unit Tests", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1106", null ]
      ] ],
      [ "Impact Analysis", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1107", [
        [ "Files Changed", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1108", null ],
        [ "Affected Functionality", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1109", null ],
        [ "Related Code That Works Correctly", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1110", null ]
      ] ],
      [ "Multi-Area Map Support", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1111", [
        [ "Standard Maps (512x512)", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1112", null ],
        [ "ZSCustomOverworld v3 Large Maps (1024x1024)", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1113", null ]
      ] ],
      [ "Issue 3: Wrong Canvas Being Scrolled (Line 2344-2366)", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1114", null ],
      [ "Issue 4: Wrong Hover Check (Line 1403)", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1115", null ],
      [ "Issue 5: Vanilla Large Map World Offset (Line 1132-1136)", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1116", null ],
      [ "Commit Reference", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1117", null ],
      [ "Future Improvements", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1118", null ],
      [ "Related Documentation", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1119", null ]
    ] ],
    [ "Changelog", "d6/da7/md_docs_2H1-changelog.html", [
      [ "0.3.2 (October 2025)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1121", [
        [ "CI/CD & Release Improvements", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1122", null ],
        [ "Rendering Pipeline Fixes", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1123", null ],
        [ "Card-Based UI System", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1124", null ],
        [ "Tile16 Editor & Graphics System", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1125", null ],
        [ "Windows Platform Stability", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1126", null ],
        [ "Emulator: Audio System Infrastructure", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1127", null ],
        [ "Emulator: Critical Performance Fixes", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1128", null ],
        [ "Emulator: UI Organization & Input System", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1129", null ],
        [ "Debugger: Breakpoint & Watchpoint Systems", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1130", null ],
        [ "Build System Simplifications", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1131", null ],
        [ "Build System: Windows Platform Improvements", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1132", null ],
        [ "GUI & UX Modernization", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1133", null ],
        [ "Overworld Editor Refactoring", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1134", null ],
        [ "Build System & Stability", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1135", null ],
        [ "Future Optimizations (Planned)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1136", null ],
        [ "Technical Notes", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1137", null ]
      ] ],
      [ "0.3.1 (September 2025)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1138", [
        [ "Major Features", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1139", null ],
        [ "Tile16 Editor Enhancements", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1140", null ],
        [ "ZSCustomOverworld v3 Implementation", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1141", null ],
        [ "Technical Improvements", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1142", null ],
        [ "User Interface", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1143", null ],
        [ "Bug Fixes", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1144", null ],
        [ "ZScream Compatibility Improvements", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1145", null ]
      ] ],
      [ "0.3.0 (September 2025)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1146", [
        [ "Major Features", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1147", null ],
        [ "User Interface & Theming", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1148", null ],
        [ "Enhancements", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1149", null ],
        [ "Technical Improvements", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1150", null ],
        [ "Bug Fixes", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1151", null ]
      ] ],
      [ "0.2.2 (December 2024)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1152", null ],
      [ "0.2.1 (August 2024)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1153", null ],
      [ "0.2.0 (July 2024)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1154", null ],
      [ "0.1.0 (May 2024)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1155", null ],
      [ "0.0.9 (April 2024)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1156", null ],
      [ "0.0.8 (February 2024)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1157", null ],
      [ "0.0.7 (January 2024)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1158", null ],
      [ "0.0.6 (November 2023)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1159", null ],
      [ "0.0.5 (November 2023)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1160", null ],
      [ "0.0.4 (November 2023)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1161", null ],
      [ "0.0.3 (October 2023)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1162", null ]
    ] ],
    [ "Roadmap", "d8/d97/md_docs_2I1-roadmap.html", [
      [ "Current Focus: AI & Editor Polish", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1164", null ],
      [ "0.4.0 (Next Major Release) - SDL3 Modernization & Core Improvements", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1166", [
        [ "🎯 Primary Goals", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1167", null ],
        [ "Phase 1: Infrastructure (Week 1-2)", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1168", null ],
        [ "Phase 2: SDL3 Core Migration (Week 3-4)", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1169", null ],
        [ "Phase 3: Complete SDL3 Integration (Week 5-6)", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1170", null ],
        [ "Phase 4: Editor Features & UX (Week 7-8)", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1171", null ],
        [ "Phase 5: AI Agent Enhancements (Throughout)", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1172", null ],
        [ "Success Criteria", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1173", null ]
      ] ],
      [ "0.5.X - Feature Expansion", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1175", null ],
      [ "0.6.X - Content & Integration", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1177", null ],
      [ "Recently Completed (v0.3.3 - October 6, 2025)", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1179", null ],
      [ "Recently Completed (v0.3.2)", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1180", null ]
    ] ],
    [ "Future Improvements & Long-Term Vision", "db/da4/md_docs_2I2-future-improvements.html", [
      [ "Architecture & Performance", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1183", [
        [ "Emulator Core Improvements", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1184", null ],
        [ "Plugin Architecture (v0.5.x+)", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1185", null ],
        [ "Multi-Threading Improvements", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1186", null ]
      ] ],
      [ "Graphics & Rendering", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1188", [
        [ "Advanced Graphics Editing", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1189", null ],
        [ "Alternative Rendering Backends", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1190", null ],
        [ "High-DPI / 4K Support", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1191", null ]
      ] ],
      [ "AI & Automation", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1193", [
        [ "Multi-Modal AI Input", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1194", null ],
        [ "Collaborative AI Sessions", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1195", null ],
        [ "Automation & Scripting", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1196", null ]
      ] ],
      [ "Content Editors", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1198", [
        [ "Music Editor UI", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1199", null ],
        [ "Dialogue Editor", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1200", null ],
        [ "Event Editor", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1201", null ],
        [ "Hex Editor Enhancements", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1202", null ]
      ] ],
      [ "Collaboration & Networking", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1204", [
        [ "Real-Time Collaboration Improvements", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1205", null ],
        [ "Cloud ROM Storage", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1206", null ]
      ] ],
      [ "Platform Support", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1208", [
        [ "Web Assembly Build", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1209", null ],
        [ "Mobile Support (iOS/Android)", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1210", null ]
      ] ],
      [ "Quality of Life", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1212", [
        [ "Undo/Redo System Enhancement", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1213", null ],
        [ "Project Templates", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1214", null ],
        [ "Asset Library", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1215", null ],
        [ "Accessibility", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1216", null ]
      ] ],
      [ "Testing & Quality", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1218", [
        [ "Automated Regression Testing", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1219", null ],
        [ "ROM Validation", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1220", null ],
        [ "Continuous Integration Enhancements", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1221", null ]
      ] ],
      [ "Documentation & Community", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1223", [
        [ "API Documentation Generator", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1224", null ],
        [ "Video Tutorial System", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1225", null ],
        [ "ROM Hacking Wiki Integration", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1226", null ]
      ] ],
      [ "Experimental / Research", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1228", [
        [ "Machine Learning Integration", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1229", null ],
        [ "VR/AR Visualization", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1230", null ],
        [ "Symbolic Execution", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1231", null ]
      ] ],
      [ "Implementation Priority", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1233", null ],
      [ "Contributing Ideas", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1235", null ]
    ] ],
    [ "yaze Documentation", "d3/d4c/md_docs_2index.html", [
      [ "A: Getting Started & Testing", "d3/d4c/md_docs_2index.html#autotoc_md1238", null ],
      [ "B: Build & Platform", "d3/d4c/md_docs_2index.html#autotoc_md1239", null ],
      [ "C: <tt>z3ed</tt> CLI", "d3/d4c/md_docs_2index.html#autotoc_md1240", null ],
      [ "E: Development & API", "d3/d4c/md_docs_2index.html#autotoc_md1241", null ],
      [ "F: Technical Documentation", "d3/d4c/md_docs_2index.html#autotoc_md1242", null ],
      [ "G: Graphics & GUI Systems", "d3/d4c/md_docs_2index.html#autotoc_md1243", null ],
      [ "H: Project Info", "d3/d4c/md_docs_2index.html#autotoc_md1244", null ],
      [ "I: Roadmap & Vision", "d3/d4c/md_docs_2index.html#autotoc_md1245", null ],
      [ "R: ROM Reference", "d3/d4c/md_docs_2index.html#autotoc_md1246", null ],
      [ "Documentation Standards", "d3/d4c/md_docs_2index.html#autotoc_md1248", [
        [ "Naming Convention", "d3/d4c/md_docs_2index.html#autotoc_md1249", null ],
        [ "File Naming", "d3/d4c/md_docs_2index.html#autotoc_md1250", null ]
      ] ]
    ] ],
    [ "A Link to the Past ROM Reference", "d7/d4f/md_docs_2R1-alttp-rom-reference.html", [
      [ "Graphics System", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1253", [
        [ "Graphics Sheets", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1254", null ],
        [ "Palette System", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1255", [
          [ "Color Format", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1256", null ],
          [ "Palette Groups", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1257", null ]
        ] ]
      ] ],
      [ "Dungeon System", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1258", [
        [ "Room Data Structure", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1259", null ],
        [ "Tile16 Format", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1260", null ],
        [ "Blocksets", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1261", null ]
      ] ],
      [ "Message System", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1262", [
        [ "Text Data Locations", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1263", null ],
        [ "Character Encoding", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1264", null ],
        [ "Text Commands", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1265", null ],
        [ "Font Graphics", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1266", null ]
      ] ],
      [ "Overworld System", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1267", [
        [ "Map Structure", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1268", null ],
        [ "Area Sizes (ZSCustomOverworld v3+)", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1269", null ],
        [ "Tile Format", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1270", null ]
      ] ],
      [ "Compression", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1271", [
        [ "LC-LZ2 Algorithm", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1272", null ]
      ] ],
      [ "Memory Map", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1273", [
        [ "ROM Banks (LoROM)", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1274", null ],
        [ "Important ROM Locations", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1275", null ]
      ] ]
    ] ],
    [ "z3ed Command Abstraction Layer Guide", "d4/dd7/md_docs_2z3ed-command-abstraction-guide.html", [
      [ "Overview", "d4/dd7/md_docs_2z3ed-command-abstraction-guide.html#autotoc_md1278", null ],
      [ "Problem Statement", "d4/dd7/md_docs_2z3ed-command-abstraction-guide.html#autotoc_md1279", [
        [ "Before Abstraction", "d4/dd7/md_docs_2z3ed-command-abstraction-guide.html#autotoc_md1280", null ],
        [ "Code Duplication Example", "d4/dd7/md_docs_2z3ed-command-abstraction-guide.html#autotoc_md1281", null ]
      ] ],
      [ "Solution Architecture", "d4/dd7/md_docs_2z3ed-command-abstraction-guide.html#autotoc_md1282", [
        [ "Three-Layer Abstraction", "d4/dd7/md_docs_2z3ed-command-abstraction-guide.html#autotoc_md1283", null ],
        [ "File Structure", "d4/dd7/md_docs_2z3ed-command-abstraction-guide.html#autotoc_md1284", null ]
      ] ],
      [ "Core Components", "d4/dd7/md_docs_2z3ed-command-abstraction-guide.html#autotoc_md1285", [
        [ "1. CommandContext", "d4/dd7/md_docs_2z3ed-command-abstraction-guide.html#autotoc_md1286", null ],
        [ "2. ArgumentParser", "d4/dd7/md_docs_2z3ed-command-abstraction-guide.html#autotoc_md1287", null ],
        [ "3. OutputFormatter", "d4/dd7/md_docs_2z3ed-command-abstraction-guide.html#autotoc_md1288", null ],
        [ "4. CommandHandler (Optional Base Class)", "d4/dd7/md_docs_2z3ed-command-abstraction-guide.html#autotoc_md1289", null ]
      ] ],
      [ "Migration Guide", "d4/dd7/md_docs_2z3ed-command-abstraction-guide.html#autotoc_md1290", [
        [ "Step-by-Step Refactoring", "d4/dd7/md_docs_2z3ed-command-abstraction-guide.html#autotoc_md1291", [
          [ "Before (80 lines):", "d4/dd7/md_docs_2z3ed-command-abstraction-guide.html#autotoc_md1292", null ],
          [ "After (30 lines):", "d4/dd7/md_docs_2z3ed-command-abstraction-guide.html#autotoc_md1293", null ]
        ] ],
        [ "Commands to Refactor", "d4/dd7/md_docs_2z3ed-command-abstraction-guide.html#autotoc_md1294", null ],
        [ "Estimated Impact", "d4/dd7/md_docs_2z3ed-command-abstraction-guide.html#autotoc_md1295", null ]
      ] ],
      [ "Testing Strategy", "d4/dd7/md_docs_2z3ed-command-abstraction-guide.html#autotoc_md1296", [
        [ "Unit Testing", "d4/dd7/md_docs_2z3ed-command-abstraction-guide.html#autotoc_md1297", null ],
        [ "Integration Testing", "d4/dd7/md_docs_2z3ed-command-abstraction-guide.html#autotoc_md1298", null ]
      ] ],
      [ "Benefits Summary", "d4/dd7/md_docs_2z3ed-command-abstraction-guide.html#autotoc_md1299", [
        [ "For Developers", "d4/dd7/md_docs_2z3ed-command-abstraction-guide.html#autotoc_md1300", null ],
        [ "For Maintainability", "d4/dd7/md_docs_2z3ed-command-abstraction-guide.html#autotoc_md1301", null ],
        [ "For AI Integration", "d4/dd7/md_docs_2z3ed-command-abstraction-guide.html#autotoc_md1302", null ]
      ] ],
      [ "Next Steps", "d4/dd7/md_docs_2z3ed-command-abstraction-guide.html#autotoc_md1303", [
        [ "Immediate (Current PR)", "d4/dd7/md_docs_2z3ed-command-abstraction-guide.html#autotoc_md1304", null ],
        [ "Phase 2 (Next PR)", "d4/dd7/md_docs_2z3ed-command-abstraction-guide.html#autotoc_md1305", null ],
        [ "Phase 3 (Future)", "d4/dd7/md_docs_2z3ed-command-abstraction-guide.html#autotoc_md1306", null ]
      ] ],
      [ "Migration Checklist", "d4/dd7/md_docs_2z3ed-command-abstraction-guide.html#autotoc_md1307", null ],
      [ "References", "d4/dd7/md_docs_2z3ed-command-abstraction-guide.html#autotoc_md1308", null ],
      [ "Questions & Answers", "d4/dd7/md_docs_2z3ed-command-abstraction-guide.html#autotoc_md1309", null ]
    ] ],
    [ "z3ed CLI Refactoring Summary", "db/d5d/md_docs_2z3ed-refactoring-summary.html", [
      [ "Overview", "db/d5d/md_docs_2z3ed-refactoring-summary.html#autotoc_md1312", null ],
      [ "Key Achievements", "db/d5d/md_docs_2z3ed-refactoring-summary.html#autotoc_md1313", [
        [ "1. Command Abstraction Layer Implementation ✅", "db/d5d/md_docs_2z3ed-refactoring-summary.html#autotoc_md1314", null ],
        [ "2. Enhanced TUI System ✅", "db/d5d/md_docs_2z3ed-refactoring-summary.html#autotoc_md1315", null ],
        [ "3. Comprehensive Testing Suite ✅", "db/d5d/md_docs_2z3ed-refactoring-summary.html#autotoc_md1316", null ],
        [ "4. Build System Updates ✅", "db/d5d/md_docs_2z3ed-refactoring-summary.html#autotoc_md1317", null ]
      ] ],
      [ "Technical Implementation Details", "db/d5d/md_docs_2z3ed-refactoring-summary.html#autotoc_md1318", [
        [ "Command Abstraction Architecture", "db/d5d/md_docs_2z3ed-refactoring-summary.html#autotoc_md1319", null ],
        [ "Refactored Commands", "db/d5d/md_docs_2z3ed-refactoring-summary.html#autotoc_md1320", null ],
        [ "TUI Architecture", "db/d5d/md_docs_2z3ed-refactoring-summary.html#autotoc_md1321", null ]
      ] ],
      [ "Code Quality Improvements", "db/d5d/md_docs_2z3ed-refactoring-summary.html#autotoc_md1322", [
        [ "Before Refactoring", "db/d5d/md_docs_2z3ed-refactoring-summary.html#autotoc_md1323", null ],
        [ "After Refactoring", "db/d5d/md_docs_2z3ed-refactoring-summary.html#autotoc_md1324", null ]
      ] ],
      [ "Testing Strategy", "db/d5d/md_docs_2z3ed-refactoring-summary.html#autotoc_md1325", [
        [ "Unit Tests", "db/d5d/md_docs_2z3ed-refactoring-summary.html#autotoc_md1326", null ],
        [ "Integration Tests", "db/d5d/md_docs_2z3ed-refactoring-summary.html#autotoc_md1327", null ],
        [ "Test Coverage", "db/d5d/md_docs_2z3ed-refactoring-summary.html#autotoc_md1328", null ]
      ] ],
      [ "Migration Guide", "db/d5d/md_docs_2z3ed-refactoring-summary.html#autotoc_md1329", [
        [ "For Developers", "db/d5d/md_docs_2z3ed-refactoring-summary.html#autotoc_md1330", null ],
        [ "For AI Integration", "db/d5d/md_docs_2z3ed-refactoring-summary.html#autotoc_md1331", null ]
      ] ],
      [ "Performance Impact", "db/d5d/md_docs_2z3ed-refactoring-summary.html#autotoc_md1332", [
        [ "Build Time", "db/d5d/md_docs_2z3ed-refactoring-summary.html#autotoc_md1333", null ],
        [ "Runtime Performance", "db/d5d/md_docs_2z3ed-refactoring-summary.html#autotoc_md1334", null ],
        [ "Development Velocity", "db/d5d/md_docs_2z3ed-refactoring-summary.html#autotoc_md1335", null ]
      ] ],
      [ "Future Roadmap", "db/d5d/md_docs_2z3ed-refactoring-summary.html#autotoc_md1336", [
        [ "Phase 2 (Next Release)", "db/d5d/md_docs_2z3ed-refactoring-summary.html#autotoc_md1337", null ],
        [ "Phase 3 (Future)", "db/d5d/md_docs_2z3ed-refactoring-summary.html#autotoc_md1338", null ]
      ] ],
      [ "Conclusion", "db/d5d/md_docs_2z3ed-refactoring-summary.html#autotoc_md1339", null ]
    ] ],
    [ "yaze - Yet Another Zelda3 Editor", "d0/d30/md_README.html", [
      [ "Version 0.3.2 - Release", "d0/d30/md_README.html#autotoc_md1342", null ],
      [ "Quick Start", "d0/d30/md_README.html#autotoc_md1347", [
        [ "Build", "d0/d30/md_README.html#autotoc_md1348", null ],
        [ "Applications", "d0/d30/md_README.html#autotoc_md1349", null ]
      ] ],
      [ "Usage", "d0/d30/md_README.html#autotoc_md1350", [
        [ "GUI Editor", "d0/d30/md_README.html#autotoc_md1351", null ],
        [ "Command Line Tool", "d0/d30/md_README.html#autotoc_md1352", null ],
        [ "C++ API", "d0/d30/md_README.html#autotoc_md1353", null ]
      ] ],
      [ "Documentation", "d0/d30/md_README.html#autotoc_md1354", null ],
      [ "Supported Platforms", "d0/d30/md_README.html#autotoc_md1355", null ],
      [ "ROM Compatibility", "d0/d30/md_README.html#autotoc_md1356", null ],
      [ "Contributing", "d0/d30/md_README.html#autotoc_md1357", null ],
      [ "License", "d0/d30/md_README.html#autotoc_md1358", null ],
      [ "🙏 Acknowledgments", "d0/d30/md_README.html#autotoc_md1359", null ],
      [ "📸 Screenshots", "d0/d30/md_README.html#autotoc_md1360", null ]
    ] ],
    [ "yaze Build Scripts", "de/d82/md_scripts_2README.html", [
      [ "Windows Scripts", "de/d82/md_scripts_2README.html#autotoc_md1363", [
        [ "vcpkg Setup (Optional)", "de/d82/md_scripts_2README.html#autotoc_md1364", null ]
      ] ],
      [ "Windows Build Workflow", "de/d82/md_scripts_2README.html#autotoc_md1365", [
        [ "Recommended: Visual Studio CMake Mode", "de/d82/md_scripts_2README.html#autotoc_md1366", null ],
        [ "Command Line Build", "de/d82/md_scripts_2README.html#autotoc_md1367", null ],
        [ "Compiler Notes", "de/d82/md_scripts_2README.html#autotoc_md1368", null ]
      ] ],
      [ "Quick Start (Windows)", "de/d82/md_scripts_2README.html#autotoc_md1369", [
        [ "Option 1: Visual Studio (Recommended)", "de/d82/md_scripts_2README.html#autotoc_md1370", null ],
        [ "Option 2: Command Line", "de/d82/md_scripts_2README.html#autotoc_md1371", null ],
        [ "Option 3: With vcpkg (Optional)", "de/d82/md_scripts_2README.html#autotoc_md1372", null ]
      ] ],
      [ "Troubleshooting", "de/d82/md_scripts_2README.html#autotoc_md1373", [
        [ "Common Issues", "de/d82/md_scripts_2README.html#autotoc_md1374", null ],
        [ "Getting Help", "de/d82/md_scripts_2README.html#autotoc_md1375", null ]
      ] ],
      [ "Other Scripts", "de/d82/md_scripts_2README.html#autotoc_md1376", null ],
      [ "Build Environment Verification", "de/d82/md_scripts_2README.html#autotoc_md1377", [
        [ "<tt>verify-build-environment.ps1</tt> / <tt>.sh</tt>", "de/d82/md_scripts_2README.html#autotoc_md1378", null ],
        [ "Usage", "de/d82/md_scripts_2README.html#autotoc_md1379", null ]
      ] ]
    ] ],
    [ "Agent Editor Module", "d6/df7/md_src_2app_2editor_2agent_2README.html", [
      [ "Overview", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1381", null ],
      [ "Architecture", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1382", [
        [ "Core Components", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1383", [
          [ "AgentEditor (<tt>agent_editor.h/cc</tt>)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1384", null ],
          [ "AgentChatWidget (<tt>agent_chat_widget.h/cc</tt>)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1385", null ],
          [ "AgentChatHistoryCodec (<tt>agent_chat_history_codec.h/cc</tt>)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1386", null ]
        ] ],
        [ "Collaboration Coordinators", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1387", [
          [ "AgentCollaborationCoordinator (<tt>agent_collaboration_coordinator.h/cc</tt>)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1388", null ],
          [ "NetworkCollaborationCoordinator (<tt>network_collaboration_coordinator.h/cc</tt>)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1389", null ]
        ] ]
      ] ],
      [ "Usage", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1390", [
        [ "Initialization", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1391", null ],
        [ "Drawing", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1392", null ],
        [ "Session Management", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1393", null ],
        [ "Network Mode (requires YAZE_WITH_GRPC and YAZE_WITH_JSON)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1394", null ]
      ] ],
      [ "File Structure", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1395", null ],
      [ "Build Configuration", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1396", [
        [ "Required", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1397", null ],
        [ "Optional", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1398", null ]
      ] ],
      [ "Data Files", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1399", [
        [ "Local Storage", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1400", null ],
        [ "Session File Format", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1401", null ]
      ] ],
      [ "Integration with EditorManager", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1402", null ],
      [ "Dependencies", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1403", [
        [ "Internal", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1404", null ],
        [ "External (when enabled)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1405", null ]
      ] ],
      [ "Advanced Features (v2.0)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1406", [
        [ "ROM Synchronization", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1407", null ],
        [ "Multimodal Snapshot Sharing", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1408", null ],
        [ "Proposal Management", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1409", null ],
        [ "AI Agent Integration", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1410", null ],
        [ "Health Monitoring", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1411", null ]
      ] ],
      [ "Future Enhancements", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1412", null ],
      [ "Server Protocol", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1413", [
        [ "Client → Server", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1414", null ],
        [ "Server → Client", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1415", null ]
      ] ]
    ] ],
    [ "YAZE Modern Command Handler Architecture", "dd/dee/md_src_2cli_2handlers_2README.html", [
      [ "Architecture Overview", "dd/dee/md_src_2cli_2handlers_2README.html#autotoc_md1417", null ],
      [ "Namespace Structure", "dd/dee/md_src_2cli_2handlers_2README.html#autotoc_md1418", null ],
      [ "Directory Organization", "dd/dee/md_src_2cli_2handlers_2README.html#autotoc_md1419", null ],
      [ "Creating a New Command Handler", "dd/dee/md_src_2cli_2handlers_2README.html#autotoc_md1420", [
        [ "1. Define the Handler Class", "dd/dee/md_src_2cli_2handlers_2README.html#autotoc_md1421", null ],
        [ "2. Implement the Handler", "dd/dee/md_src_2cli_2handlers_2README.html#autotoc_md1422", null ],
        [ "3. Register in Factory", "dd/dee/md_src_2cli_2handlers_2README.html#autotoc_md1423", null ],
        [ "4. Add Forward Declaration", "dd/dee/md_src_2cli_2handlers_2README.html#autotoc_md1424", null ]
      ] ],
      [ "Command Handler Base Class", "dd/dee/md_src_2cli_2handlers_2README.html#autotoc_md1425", [
        [ "Lifecycle Methods", "dd/dee/md_src_2cli_2handlers_2README.html#autotoc_md1426", null ],
        [ "Helper Methods", "dd/dee/md_src_2cli_2handlers_2README.html#autotoc_md1427", null ]
      ] ],
      [ "Argument Parsing", "dd/dee/md_src_2cli_2handlers_2README.html#autotoc_md1428", null ],
      [ "Output Formatting", "dd/dee/md_src_2cli_2handlers_2README.html#autotoc_md1429", null ],
      [ "Integration with Public API", "dd/dee/md_src_2cli_2handlers_2README.html#autotoc_md1430", null ],
      [ "Best Practices", "dd/dee/md_src_2cli_2handlers_2README.html#autotoc_md1431", null ],
      [ "Testing", "dd/dee/md_src_2cli_2handlers_2README.html#autotoc_md1432", null ],
      [ "Future Enhancements", "dd/dee/md_src_2cli_2handlers_2README.html#autotoc_md1433", null ]
    ] ],
    [ "End-to-End (E2E) Tests", "d9/db0/md_test_2e2e_2README.html", [
      [ "Active Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md1437", [
        [ "✅ Working Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md1438", null ],
        [ "📝 Dungeon Editor Smoke Test", "d9/db0/md_test_2e2e_2README.html#autotoc_md1439", null ]
      ] ],
      [ "Running Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md1440", [
        [ "All E2E Tests (GUI Mode)", "d9/db0/md_test_2e2e_2README.html#autotoc_md1441", null ],
        [ "Specific Test Category", "d9/db0/md_test_2e2e_2README.html#autotoc_md1442", null ],
        [ "Dungeon Editor Test Only", "d9/db0/md_test_2e2e_2README.html#autotoc_md1443", null ]
      ] ],
      [ "Test Development", "d9/db0/md_test_2e2e_2README.html#autotoc_md1444", [
        [ "Creating New Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md1445", null ],
        [ "Register in yaze_test.cc", "d9/db0/md_test_2e2e_2README.html#autotoc_md1446", null ],
        [ "ImGui Test Engine API", "d9/db0/md_test_2e2e_2README.html#autotoc_md1447", null ]
      ] ],
      [ "Test Logging", "d9/db0/md_test_2e2e_2README.html#autotoc_md1448", null ],
      [ "Test Infrastructure", "d9/db0/md_test_2e2e_2README.html#autotoc_md1449", [
        [ "File Organization", "d9/db0/md_test_2e2e_2README.html#autotoc_md1450", null ],
        [ "Helper Functions", "d9/db0/md_test_2e2e_2README.html#autotoc_md1451", null ]
      ] ],
      [ "Future Test Ideas", "d9/db0/md_test_2e2e_2README.html#autotoc_md1452", null ],
      [ "Troubleshooting", "d9/db0/md_test_2e2e_2README.html#autotoc_md1453", [
        [ "Test Crashes in GUI Mode", "d9/db0/md_test_2e2e_2README.html#autotoc_md1454", null ],
        [ "Tests Not Found", "d9/db0/md_test_2e2e_2README.html#autotoc_md1455", null ],
        [ "ImGui Items Not Found", "d9/db0/md_test_2e2e_2README.html#autotoc_md1456", null ]
      ] ],
      [ "References", "d9/db0/md_test_2e2e_2README.html#autotoc_md1457", null ],
      [ "Status", "d9/db0/md_test_2e2e_2README.html#autotoc_md1458", null ]
    ] ],
    [ "yaze Test Suite", "d0/d46/md_test_2README.html", [
      [ "Directory Structure", "d0/d46/md_test_2README.html#autotoc_md1460", null ],
      [ "Test Categories", "d0/d46/md_test_2README.html#autotoc_md1461", [
        [ "Unit Tests (<tt>unit/</tt>)", "d0/d46/md_test_2README.html#autotoc_md1462", null ],
        [ "Integration Tests (<tt>integration/</tt>)", "d0/d46/md_test_2README.html#autotoc_md1463", null ],
        [ "End-to-End Tests (<tt>e2e/</tt>)", "d0/d46/md_test_2README.html#autotoc_md1464", null ]
      ] ],
      [ "Enhanced Test Runner", "d0/d46/md_test_2README.html#autotoc_md1465", [
        [ "Usage Examples", "d0/d46/md_test_2README.html#autotoc_md1466", null ],
        [ "Test Modes", "d0/d46/md_test_2README.html#autotoc_md1467", null ],
        [ "Options", "d0/d46/md_test_2README.html#autotoc_md1468", null ]
      ] ],
      [ "E2E ROM Testing", "d0/d46/md_test_2README.html#autotoc_md1469", [
        [ "Test Cases", "d0/d46/md_test_2README.html#autotoc_md1470", null ]
      ] ],
      [ "ZSCustomOverworld Upgrade Testing", "d0/d46/md_test_2README.html#autotoc_md1471", [
        [ "Supported Upgrades", "d0/d46/md_test_2README.html#autotoc_md1472", null ],
        [ "Test Cases", "d0/d46/md_test_2README.html#autotoc_md1473", null ],
        [ "Version-Specific Features", "d0/d46/md_test_2README.html#autotoc_md1474", [
          [ "Vanilla", "d0/d46/md_test_2README.html#autotoc_md1475", null ],
          [ "v2", "d0/d46/md_test_2README.html#autotoc_md1476", null ],
          [ "v3", "d0/d46/md_test_2README.html#autotoc_md1477", null ]
        ] ]
      ] ],
      [ "Environment Variables", "d0/d46/md_test_2README.html#autotoc_md1478", null ],
      [ "CI/CD Integration", "d0/d46/md_test_2README.html#autotoc_md1479", null ],
      [ "Deprecated Tests", "d0/d46/md_test_2README.html#autotoc_md1480", null ],
      [ "Best Practices", "d0/d46/md_test_2README.html#autotoc_md1481", null ],
      [ "AI Agent Testing", "d0/d46/md_test_2README.html#autotoc_md1482", null ]
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
"d0/d22/classyaze_1_1cli_1_1resources_1_1CommandHandler.html",
"d0/d28/classTextEditor.html#a7336396ef0e745643c3ad8b2f52a573c",
"d0/d54/background__buffer_8h_source.html",
"d0/da3/widget__state__capture_8h.html#afd7b0c7907c3c40350d164a541651f13",
"d0/dbd/classyaze_1_1emu_1_1Emulator.html#adf796a67c0a76cf771623841e6c90b15",
"d0/df3/structyaze_1_1gui_1_1FlagsMenu.html#ad9ea67c5147aa3fe052d68bb3a240e96",
"d1/d1d/cli_2service_2ai_2common_8h.html",
"d1/d33/structyaze_1_1emu_1_1OAMADDH.html#aae3c265b49d9d956376e6ee1a37487fb",
"d1/d42/classyaze_1_1util_1_1LogManager.html#a8b42d4e5c00db30145b06a4b237d7b69",
"d1/d60/structyaze_1_1emu_1_1Tile.html#a28f868ff211e1f149b2b768a24ed624c",
"d1/d8a/room__entrance_8h.html#a9962de09957a22a794ab96f1f2a41803",
"d1/daf/classyaze_1_1cli_1_1resources_1_1OutputFormatterTest.html",
"d1/de0/classyaze_1_1test_1_1TestSuite.html#aced53e3c8719be624da559bd7116a193",
"d1/def/structyaze_1_1core_1_1WorkspaceSettings.html#aeaff6a8411b303f46125b61fed42a0ad",
"d2/d07/classyaze_1_1emu_1_1Spc700.html#a6876a7fe1572582805bcadebf9086682",
"d2/d1f/canvas__interaction__handler_8cc.html#a1997cb7aec1a12055c39f4b36163a4c2",
"d2/d48/enhanced__tui_8cc.html#a05c2a79326914bff69c9a7dfb8e0b43a",
"d2/d4e/classyaze_1_1editor_1_1AgentChatHistoryPopup.html#a68c9bc728f36fdc1940a9ea3f3bf4dbf",
"d2/d5e/classyaze_1_1emu_1_1Memory.html#ade9b2caad926a43a74ceee46afd38244",
"d2/dae/overworld__entity__renderer_8cc.html#abbf10e85fffce1ef19a6a37f6adc1879",
"d2/de7/classyaze_1_1editor_1_1DungeonCanvasViewer.html#a7600bdee954bb8c25633e1b313c76032",
"d2/dfe/structyaze_1_1gui_1_1EnhancedTheme.html#a15c5d074b9f2f77bcd4a4bf173fc0219",
"d3/d03/emulator__commands_8cc.html#a9a8a8282bac36a3c8bd7ebea35c1ca4f",
"d3/d19/classyaze_1_1gfx_1_1GraphicsOptimizer.html",
"d3/d2e/compression_8cc.html#a4052e31abfb4c3aeb1e7c576ad5ba640",
"d3/d44/classyaze_1_1editor_1_1OverworldEditor.html#a40d5b1a587cfe694ba9cd32947b96ff9",
"d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md534",
"d3/d6a/classyaze_1_1zelda3_1_1RoomObject.html#a9271f3a50b053b6f15f5c2b4733dde6d",
"d3/d8a/classyaze_1_1editor_1_1CommandManager.html#a1bd784d67295a0381bd9ee6a59c93537",
"d3/d9d/classyaze_1_1zelda3_1_1GameEntity.html#a04318cae2fac826a7a06bdbb6e7948b2aa988cf0fcaba7dc6442ff03ead1cba26",
"d3/dae/structyaze_1_1zelda3_1_1music_1_1ZeldaInstrument.html#a7d854a930f079ad18ef4c44a216b3f6c",
"d3/dbf/namespaceyaze_1_1gui.html#ac637f25be078b37c2bdba7a57e6d1655",
"d3/ded/classyaze_1_1emu_1_1Ppu.html#a51abe4b4562e2608a214c337496edbd1",
"d4/d03/classyaze_1_1gui_1_1WidgetMeasurement.html#a5a615ad03285cb84d2ce7c490a1616d2",
"d4/d0a/namespaceyaze_1_1test.html#a9cab52836128fe2974108d482fc09594",
"d4/d45/classyaze_1_1editor_1_1AssemblyEditor.html#a404459a3eba8f510a68c55a1e90936ef",
"d4/d70/classyaze_1_1test_1_1IntegratedTestSuite.html#adb1c164d3230fa6477eab540add05fbf",
"d4/d96/classyaze_1_1gui_1_1EditorCard.html#a589a365d1295595b57af4560659e9257",
"d4/dca/structyaze_1_1gui_1_1Color.html#a48502467033d9191c22883f6a0fa9a09",
"d4/de6/classyaze_1_1gfx_1_1Arena.html#a700e6d369fd67535824bd9dba3606fc5",
"d5/d1f/classyaze_1_1util_1_1FlagRegistry.html#ad9400a85f05a64e580a6965fe09f6d00",
"d5/d1f/namespaceyaze_1_1zelda3.html#a693df677b1b2754452c8b8b4c4a98468abf3bef71311fbea6a7fda44090b85971",
"d5/d1f/namespaceyaze_1_1zelda3.html#ae743dd23bb238d5aa19fc3a8ac1aabbb",
"d5/d52/snes__color_8h.html#ac7f1a1c381048672e273ebdb96a677e0",
"d5/d77/classyaze_1_1core_1_1TimingManager.html#ae067e393092b67edd55f464723431943",
"d5/da7/classyaze_1_1emu_1_1AudioRamImpl.html",
"d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md985",
"d6/d00/classyaze_1_1cli_1_1EnhancedChatComponent.html#a1a39777648e50787ae8019dcb515b094",
"d6/d0d/classyaze_1_1editor_1_1AgentEditor.html#ad86dab9867728522dc05b48926f5ef5b",
"d6/d28/classyaze_1_1net_1_1RomVersionManager.html#aa7a977391390ad28abae781a25235152",
"d6/d30/classyaze_1_1Rom.html#a7113e4e4db134e18b9693a0065ed16ae",
"d6/d47/structyaze_1_1gui_1_1WidgetMetrics.html#af72eb01845203324ce00c1f2f318a4c5",
"d6/d74/rom__test_8cc.html#a117410ba9922a991669e65a23416583e",
"d6/dad/namespaceyaze_1_1cli_1_1resources.html",
"d6/dcb/classyaze_1_1gui_1_1canvas_1_1CanvasPerformanceIntegration.html#a5d5d8a783251f1cc39237abba8cc1840",
"d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1386",
"d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md394",
"d7/d61/classyaze_1_1zelda3_1_1DungeonEditorSystem.html#a0a40ae03bcfe1a31962b7f868be9f75d",
"d7/d61/classyaze_1_1zelda3_1_1DungeonEditorSystem.html#abf18bf4dc9d94a40de5d98067f231b93",
"d7/d84/classyaze_1_1editor_1_1ShortcutManager.html#aa126613baf1a31d8bdb2b032b01e3a6a",
"d7/db2/zscustomoverworld__upgrade__test_8cc_source.html",
"d7/dd7/classyaze_1_1gui_1_1TileSelectorWidget.html#ace75382a8154fb229e31a68b7b3b540f",
"d7/df6/classyaze_1_1zelda3_1_1Room.html#a7e231eabce5e285ddec8de4d0b01fc93",
"d8/d01/namespaceyaze_1_1cli_1_1anonymous__namespace_02resource__catalog__test_8cc_03.html#aac3a17ef588265540d821fbd8a0fc3bc",
"d8/d1e/classyaze_1_1zelda3_1_1RoomEntrance.html#a4cfcd4b6843cccb5f109045f9b4726c8",
"d8/d46/classyaze_1_1editor_1_1UserSettings.html#abd246cd1cfc54409eec3abb2af49e538",
"d8/d78/classyaze_1_1editor_1_1test_1_1Tile16EditorIntegrationTest.html#a91a9aa0044f83ed2c956caa2814222aa",
"d8/da5/palette__editor__widget_8cc_source.html",
"d8/dd3/namespaceyaze_1_1cli_1_1agent.html#a570346e91ec90d0baf95a883ca3665e1ac94218324f79fff8dbfc784471f7f8a7",
"d8/ddf/classOverworldGoldenDataExtractor.html#a4fae500428a0294042093fab0698154a",
"d9/d21/structyaze_1_1emu_1_1DmaRegisters.html",
"d9/d2b/classyaze_1_1editor_1_1AgentChatWidget.html#acbb92cc63fc2bd3a9f9cf075472f3fef",
"d9/d6c/classyaze_1_1editor_1_1HistoryManager.html#aa09be7516a692df2eb5af0cdf262dffd",
"d9/d89/yaze_8h_source.html",
"d9/dad/classyaze_1_1cli_1_1tui_1_1ChatTUI.html#a9f278ea9d5511b488afae3b703ebece6",
"d9/dc5/classyaze_1_1zelda3_1_1Overworld.html#a0850f59cde6afdd004e8508370f9d448",
"d9/dcc/classyaze_1_1zelda3_1_1OverworldMap.html#a254c5c8d5b9e78ab937e38cdccd5fe10",
"d9/dd2/classyaze_1_1cli_1_1agent_1_1VimMode.html#a5695db0751fd1d1868d964c630bd4075",
"d9/df8/structyaze_1_1cli_1_1overworld_1_1EntranceDetails.html#a6ef1aad98ec44f7cb711de6a3208f1fb",
"da/d2c/classyaze_1_1gui_1_1Canvas.html#a06eab9bafdad1cdd915c22937e07bf2d",
"da/d2c/classyaze_1_1gui_1_1Canvas.html#aa8401bef8f4151be1df65c0b18beaebc",
"da/d3e/classyaze_1_1test_1_1TestManager.html#a983fa38d2185127918f70b6ce701bcbe",
"da/d57/structyaze_1_1cli_1_1PolicyResult.html#a802c65059fd8c46489ab447e7377f007",
"da/d81/structyaze_1_1editor_1_1zsprite_1_1UserRoutine.html#a5a1d954fa3aa13b26bfd83cca84a6ccd",
"da/db1/namespaceyaze_1_1gfx_1_1palette__group__internal.html#af672f9ca4ca0b7621e9e6018910d73bc",
"da/dd2/classyaze_1_1core_1_1AsarWrapper.html#ade7b3915a5246e8c8e32c01008125094",
"da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md955",
"db/d23/classyaze_1_1cli_1_1UnifiedLayout.html#a9cc998fb10e070c71845e05412f4551a",
"db/d41/structyaze_1_1cli_1_1agent_1_1LearnedKnowledgeService_1_1Stats.html#a7d109576d7e6fe1b73c912721e7fa385",
"db/d71/classyaze_1_1editor_1_1GfxGroupEditor.html#a413a23b98a3b7ce5c691b53821453652",
"db/d82/classyaze_1_1editor_1_1Tile16Editor.html#abd40aed5b50ac9ec50004990d40afe59",
"db/da4/classyaze_1_1emu_1_1MockMemory.html#a54dc682bfeebe2107092e8ac09bf5a21",
"db/dc3/namespaceyaze_1_1gui_1_1canvas.html",
"db/de8/classyaze_1_1editor_1_1DungeonRoomSelector.html#a1213b1e19cf2e4e50eecd9022ea4f315",
"dc/d0c/tilemap_8h.html#a1de0ff190f518ffd0a28aeb9d9fb0169",
"dc/d31/classyaze_1_1editor_1_1GraphicsEditor.html#a60173ccc66c96004f5194089ef4a3c67",
"dc/d42/md_docs_2B1-build-instructions.html#autotoc_md92",
"dc/d5b/structyaze_1_1editor_1_1AgentEditor_1_1SessionInfo.html#af594a3a9964c7736a5f77dca17c3ef46",
"dc/daa/classyaze_1_1editor_1_1WelcomeScreen.html#a6c8ddb21424ffc21b31dae4c2791bb66",
"dc/dee/snes__color_8cc.html",
"dc/df4/classyaze_1_1emu_1_1Cpu.html#a83a5180ad53c1ba559df2f99c6a48202",
"dc/df6/classyaze_1_1cli_1_1handlers_1_1EmulatorWriteMemoryCommandHandler.html",
"dd/d12/classyaze_1_1editor_1_1EditorManager.html#a4571c1ab924da431149c66f118a319dd",
"dd/d26/structyaze_1_1cli_1_1overworld_1_1MapSummary.html#a0fb5a7f6f16323a5b4cd64c7efb35609",
"dd/d4b/classyaze_1_1editor_1_1DungeonUsageTracker.html#ab738ca434269bf7783ffa955789cb688",
"dd/d71/classyaze_1_1gui_1_1canvas_1_1CanvasInteractionHandler.html#a506e890d54834035ac55472d1fd6b9f9",
"dd/dc0/classyaze_1_1test_1_1WidgetDiscoveryService.html#a4d349f3e1ee7e07db0f535fa969ed5d9",
"dd/ddf/classyaze_1_1gui_1_1canvas_1_1CanvasContextMenu.html#aae04ae7acb8dd9f039442e8ad7ff13ee",
"dd/dfa/structyaze_1_1zelda3_1_1DungeonEditorSystem_1_1DoorData.html#aa80ff717e134d9c001b95e6e1b88d186",
"de/d0f/classyaze_1_1emu_1_1MemoryImpl.html#a6e3b08255f4262a5de232841001f99ee",
"de/d42/structyaze_1_1net_1_1SessionInfo.html#a4a0d98e9c33bfd32121a8b96dd820305",
"de/d7a/namespaceyaze_1_1cli_1_1agent_1_1anonymous__namespace_02vim__mode_8cc_03.html#a5608b1970aa86259a8a9953a12a7fc63",
"de/d9a/structyaze_1_1emu_1_1input_1_1InputConfig.html#aa4f5f8569f73f7056e6bfa990be5c920",
"de/dbf/icons_8h.html#a088d494eb185c276319e1f8174bf6e3e",
"de/dbf/icons_8h.html#a276b77c829d5f2a1a98737438ef4c9a9",
"de/dbf/icons_8h.html#a44f91279805a45ee94c5d66404a8d4b1",
"de/dbf/icons_8h.html#a625380b9359436edf222561fcdc9613b",
"de/dbf/icons_8h.html#a8184d72e03bbc7294646b3f2db29801e",
"de/dbf/icons_8h.html#aa19cd7ac39769ced04f8760ca954feeb",
"de/dbf/icons_8h.html#abdb042932365276be9d32833bda394a3",
"de/dbf/icons_8h.html#ad9aa52f9c0ebc840ee5913a981683e8c",
"de/dbf/icons_8h.html#af3cf154a0f600b71b1217fdfce340c77",
"de/dd4/classyaze_1_1cli_1_1ProposalRegistry.html#a35ea789ed0667b0a69a01f80a36331ffacea229a90d55b36e1f03b3d47e62552e",
"de/de7/classyaze_1_1zelda3_1_1DungeonObjectEditor.html#ad0b5ae829293d4c2378ec79df8d8f1c5",
"df/d0e/classyaze_1_1gfx_1_1Bitmap.html#a05432a1e204b43ea1cb4820449b7e0b8",
"df/d28/classyaze_1_1cli_1_1agent_1_1EnhancedTUI.html#a0f86dd10a00eaf6867e2296291516c53",
"df/d4c/namespaceyaze_1_1net_1_1anonymous__namespace_02rom__version__manager_8cc_03.html#ac68a169d97d06b7506d035a66ccd6e58",
"df/db7/classyaze_1_1gui_1_1canvas_1_1CanvasPerformanceManager.html#aef44552db865e0418822b836fc1f7fe0",
"df/de3/namespaceyaze_1_1editor_1_1anonymous__namespace_02overworld__entity__renderer_8cc_03.html#a1a7fd2da53fa95eb01e49e9d105b82f1",
"functions_func_j.html"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';