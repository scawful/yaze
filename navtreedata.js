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
      [ "📚 Full Workflow Reference (Future/Formal)", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md165", null ],
      [ "Branch Structure", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md166", [
        [ "Main Branches", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md167", [
          [ "<tt>master</tt>", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md168", null ],
          [ "<tt>develop</tt>", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md169", null ]
        ] ],
        [ "Supporting Branches", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md170", [
          [ "Feature Branches", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md171", null ],
          [ "Bugfix Branches", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md172", null ],
          [ "Hotfix Branches", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md173", null ],
          [ "Release Branches", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md174", null ],
          [ "Experimental Branches", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md175", null ]
        ] ]
      ] ],
      [ "Commit Message Conventions", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md176", [
        [ "Format", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md177", null ],
        [ "Types", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md178", null ],
        [ "Scopes (optional)", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md179", null ],
        [ "Examples", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md180", null ]
      ] ],
      [ "Pull Request Guidelines", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md181", [
        [ "PR Title", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md182", null ],
        [ "PR Description Template", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md183", null ],
        [ "PR Review Process", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md184", null ]
      ] ],
      [ "Version Numbering", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md185", [
        [ "MAJOR (e.g., 1.0.0)", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md186", null ],
        [ "MINOR (e.g., 0.4.0)", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md187", null ],
        [ "PATCH (e.g., 0.3.2)", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md188", null ],
        [ "Pre-release Tags", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md189", null ]
      ] ],
      [ "Release Process", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md190", [
        [ "For Minor/Major Releases (0.x.0, x.0.0)", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md191", null ],
        [ "For Patch Releases (0.3.x)", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md192", null ]
      ] ],
      [ "Long-Running Feature Branches", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md193", null ],
      [ "Tagging Strategy", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md194", [
        [ "Release Tags", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md195", null ],
        [ "Internal Milestones", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md196", null ]
      ] ],
      [ "Best Practices", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md197", [
        [ "DO ✅", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md198", null ],
        [ "DON'T ❌", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md199", null ]
      ] ],
      [ "Quick Reference", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md200", null ],
      [ "Emergency Procedures", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md201", [
        [ "If master is broken", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md202", null ],
        [ "If develop is broken", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md203", null ],
        [ "If release needs to be rolled back", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md204", null ]
      ] ],
      [ "🚀 Current Simplified Workflow (Pre-1.0)", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md206", [
        [ "Daily Development Pattern", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md207", null ],
        [ "When to Use Branches (Pre-1.0)", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md208", null ],
        [ "Current Branch Usage", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md209", null ],
        [ "Commit Message (Simplified)", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md210", null ],
        [ "Releases (Pre-1.0)", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md211", null ]
      ] ]
    ] ],
    [ "B5 - Architecture and Networking", "dd/de3/md_docs_2B5-architecture-and-networking.html", [
      [ "1. High-Level Architecture", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md214", null ],
      [ "2. Service Taxonomy", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md215", [
        [ "APP Services (gRPC Servers)", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md216", null ],
        [ "CLI Services (Business Logic)", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md217", null ]
      ] ],
      [ "3. gRPC Services", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md218", [
        [ "ImGuiTestHarness Service", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md219", null ],
        [ "RomService", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md220", null ],
        [ "CanvasAutomation Service", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md221", null ]
      ] ],
      [ "4. Real-Time Collaboration", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md222", [
        [ "Architecture", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md223", null ],
        [ "Core Components", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md224", null ],
        [ "WebSocket Protocol", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md225", null ]
      ] ],
      [ "5. Data Flow Example: AI Agent Edits a Tile", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md226", null ]
    ] ],
    [ "z3ed: AI-Powered CLI for YAZE", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html", [
      [ "1. Overview", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md228", [
        [ "Core Capabilities", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md229", null ]
      ] ],
      [ "2. Quick Start", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md230", [
        [ "Build", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md231", null ],
        [ "AI Setup", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md232", null ],
        [ "Example Commands", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md233", null ],
        [ "Hybrid CLI ↔ GUI Workflow", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md234", null ]
      ] ],
      [ "3. Architecture", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md235", [
        [ "System Components Diagram", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md236", null ]
      ] ],
      [ "4. Agentic & Generative Workflow (MCP)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md237", null ],
      [ "5. Command Reference", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md238", [
        [ "Agent Commands", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md239", null ],
        [ "Resource Commands", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md240", [
          [ "<tt>agent test</tt>: Live Harness Automation", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md241", null ]
        ] ]
      ] ],
      [ "6. Chat Modes", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md242", [
        [ "FTXUI Chat (<tt>agent chat</tt>)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md243", null ],
        [ "Simple Chat (<tt>agent simple-chat</tt>)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md244", null ],
        [ "GUI Chat Widget (Editor Integration)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md245", null ]
      ] ],
      [ "7. AI Provider Configuration", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md246", [
        [ "System Prompt Versions", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md247", null ]
      ] ],
      [ "8. Learn Command - Knowledge Management", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md248", [
        [ "Basic Usage", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md249", null ],
        [ "Project Context", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md250", null ],
        [ "Conversation Memory", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md251", null ],
        [ "Storage Location", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md252", null ]
      ] ],
      [ "9. TODO Management System", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md253", [
        [ "Core Capabilities", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md254", null ],
        [ "Storage Location", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md255", null ]
      ] ],
      [ "10. CLI Output & Help System", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md256", [
        [ "Verbose Logging", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md257", null ],
        [ "Hierarchical Help System", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md258", null ]
      ] ],
      [ "10. Collaborative Sessions & Multimodal Vision", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md259", [
        [ "Overview", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md260", null ],
        [ "Local Collaboration Mode", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md262", [
          [ "How to Use", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md263", null ],
          [ "Features", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md264", null ],
          [ "Cloud Folder Workaround", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md265", null ]
        ] ],
        [ "Network Collaboration Mode (yaze-server v2.0)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md267", [
          [ "Requirements", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md268", null ],
          [ "Server Setup", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md269", null ],
          [ "Client Connection", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md270", null ],
          [ "Core Features", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md271", null ],
          [ "Advanced Features (v2.0)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md272", null ],
          [ "Protocol Reference", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md273", null ],
          [ "Server Configuration", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md274", null ],
          [ "Database Schema (Server v2.0)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md275", null ],
          [ "Deployment", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md276", null ],
          [ "Testing", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md277", null ],
          [ "Security Considerations", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md278", null ]
        ] ],
        [ "Multimodal Vision (Gemini)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md280", [
          [ "Requirements", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md281", null ],
          [ "Capture Modes", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md282", null ],
          [ "How to Use", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md283", null ],
          [ "Example Prompts", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md284", null ]
        ] ],
        [ "Architecture", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md286", null ],
        [ "Troubleshooting", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md288", null ],
        [ "References", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md290", null ]
      ] ],
      [ "11. Roadmap & Implementation Status", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md291", [
        [ "✅ Completed", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md292", null ],
        [ "📌 Current Progress Highlights (October 5, 2025)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md293", null ],
        [ "🚧 Active & Next Steps", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md294", null ],
        [ "✅ Recently Completed (v0.2.0-alpha - October 5, 2025)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md295", [
          [ "Core AI Features", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md296", null ],
          [ "Version Management & Protection", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md297", null ],
          [ "Networking & Collaboration (NEW)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md298", null ],
          [ "UI/UX Enhancements", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md299", null ],
          [ "Build System & Infrastructure", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md300", null ]
        ] ]
      ] ],
      [ "12. Troubleshooting", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md301", null ]
    ] ],
    [ "Testing z3ed Without ROM Files", "d7/d43/md_docs_2C2-testing-without-roms.html", [
      [ "Overview", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md303", null ],
      [ "How Mock ROM Mode Works", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md304", null ],
      [ "Usage", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md305", [
        [ "Command Line Flag", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md306", null ],
        [ "Test Suite", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md307", null ]
      ] ],
      [ "What Works with Mock ROM", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md308", [
        [ "✅ Fully Supported", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md309", null ],
        [ "⚠️ Limited Support", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md310", null ],
        [ "❌ Not Supported", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md311", null ]
      ] ],
      [ "Testing Strategy", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md312", [
        [ "For Agent Logic", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md313", null ],
        [ "For ROM Operations", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md314", null ]
      ] ],
      [ "CI/CD Integration", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md315", [
        [ "GitHub Actions Example", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md316", null ]
      ] ],
      [ "Embedded Labels Reference", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md317", null ],
      [ "Troubleshooting", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md318", [
        [ "\"No ROM loaded\" error", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md319", null ],
        [ "Mock ROM fails to initialize", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md320", null ],
        [ "Agent returns empty/wrong results", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md321", null ]
      ] ],
      [ "Development", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md322", [
        [ "Adding New Labels", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md323", null ],
        [ "Testing Mock ROM Directly", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md324", null ]
      ] ],
      [ "Best Practices", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md325", [
        [ "DO ✅", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md326", null ],
        [ "DON'T ❌", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md327", null ]
      ] ],
      [ "Related Documentation", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md328", null ]
    ] ],
    [ "Asm Style Guide", "d7/d9a/md_docs_2E1-asm-style-guide.html", [
      [ "Table of Contents", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md331", null ],
      [ "File Structure", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md332", null ],
      [ "Labels and Symbols", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md333", null ],
      [ "Comments", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md334", null ],
      [ "Instructions", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md335", null ],
      [ "Macros", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md336", null ],
      [ "Loops and Branching", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md337", null ],
      [ "Data Structures", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md338", null ],
      [ "Code Organization", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md339", null ],
      [ "Custom Code", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md340", null ]
    ] ],
    [ "E2 - Development Guide", "d5/d18/md_docs_2E2-development-guide.html", [
      [ "Editor Status", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md342", null ],
      [ "1. Core Architectural Patterns", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md343", [
        [ "Pattern 1: Modular Systems", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md344", null ],
        [ "Pattern 2: Callback-Based Communication", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md345", null ],
        [ "Pattern 3: Centralized Progressive Loading via <tt>gfx::Arena</tt>", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md346", null ]
      ] ],
      [ "2. UI & Theming System", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md347", [
        [ "2.1. The Theme System (<tt>AgentUITheme</tt>)", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md348", null ],
        [ "2.2. Reusable UI Helper Functions", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md349", null ],
        [ "2.3. Toolbar Implementation (<tt>CompactToolbar</tt>)", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md350", null ]
      ] ],
      [ "3. Key System Implementations & Gotchas", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md351", [
        [ "3.1. Graphics Refresh Logic", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md352", null ],
        [ "3.2. Multi-Area Map Configuration", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md353", null ],
        [ "3.3. Version-Specific Feature Gating", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md354", null ],
        [ "3.4. Entity Visibility for Visual Testing", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md355", null ]
      ] ],
      [ "4. Debugging and Testing", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md356", [
        [ "4.1. Quick Debugging with Startup Flags", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md357", null ],
        [ "4.2. Testing Strategies", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md358", null ],
        [ "3.6. Graphics Sheet Management", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md359", null ],
        [ "Naming Conventions", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md360", null ]
      ] ]
    ] ],
    [ "API Reference", "d8/d73/md_docs_2E3-api-reference.html", [
      [ "C API (<tt>incl/yaze.h</tt>)", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md362", [
        [ "Core Library Functions", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md363", null ],
        [ "ROM Operations", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md364", null ]
      ] ],
      [ "C++ API", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md365", [
        [ "AsarWrapper (<tt>src/app/core/asar_wrapper.h</tt>)", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md366", [
          [ "CLI Examples (<tt>z3ed</tt>)", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md367", null ],
          [ "C++ API Example", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md368", null ],
          [ "Class Definition", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md369", null ]
        ] ]
      ] ],
      [ "Data Structures", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md370", [
        [ "<tt>snes_color</tt>", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md371", null ],
        [ "<tt>zelda3_message</tt>", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md372", null ]
      ] ],
      [ "Error Handling", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md373", [
        [ "C API Error Pattern", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md374", null ]
      ] ]
    ] ],
    [ "E4 - Emulator Development Guide", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html", [
      [ "Table of Contents", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md377", null ],
      [ "1. Current Status", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md379", [
        [ "🎉 Major Breakthrough: Game is Running!", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md380", null ],
        [ "✅ Confirmed Working", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md381", null ],
        [ "🔧 Known Issues (Non-Critical)", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md382", null ]
      ] ],
      [ "2. How to Use the Emulator", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md384", [
        [ "Method 1: Main Yaze Application (GUI)", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md385", null ],
        [ "Method 2: Standalone Emulator (<tt>yaze_emu</tt>)", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md386", null ],
        [ "Method 3: Dungeon Object Emulator Preview", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md387", null ]
      ] ],
      [ "3. Architecture", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md389", [
        [ "Memory System", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md390", [
          [ "SNES Memory Map", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md391", null ]
        ] ],
        [ "CPU-APU-SPC700 Interaction", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md392", null ],
        [ "Component Architecture", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md393", null ]
      ] ],
      [ "4. The Debugging Journey: Critical Fixes", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md395", [
        [ "SPC700 & APU Fixes", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md396", null ],
        [ "The Critical Pattern for Multi-Step Instructions", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md397", null ]
      ] ],
      [ "5. Display & Performance Improvements", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md399", [
        [ "PPU Color Display Fix", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md400", null ],
        [ "Frame Timing & Speed Control", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md401", null ],
        [ "Performance Optimizations", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md402", [
          [ "Frame Skipping", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md403", null ],
          [ "Audio Buffer Management", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md404", null ],
          [ "Performance Gains", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md405", null ]
        ] ],
        [ "ROM Loading Improvements", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md406", null ]
      ] ],
      [ "6. Advanced Features", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md408", [
        [ "Professional Disassembly Viewer", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md409", [
          [ "Architecture", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md410", null ],
          [ "Visual Features", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md411", null ],
          [ "Interactive Elements", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md412", null ],
          [ "UI Features", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md413", null ],
          [ "Performance", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md414", null ]
        ] ],
        [ "Breakpoint System", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md415", null ],
        [ "UI/UX Enhancements", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md416", null ]
      ] ],
      [ "7. Emulator Preview Tool", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md418", [
        [ "Purpose", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md419", null ],
        [ "Critical Fixes Applied", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md420", [
          [ "1. Memory Access Fix (SIGSEGV Crash)", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md421", null ],
          [ "2. RTL vs RTS Fix (Timeout)", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md422", null ],
          [ "3. Palette Validation", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md423", null ],
          [ "4. PPU Configuration", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md424", null ]
        ] ],
        [ "How to Use", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md425", null ],
        [ "What You'll Learn", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md426", null ],
        [ "Reverse Engineering Workflow", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md427", null ],
        [ "UI Enhancements", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md428", null ]
      ] ],
      [ "8. Logging System", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md430", [
        [ "How to Enable", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md431", null ]
      ] ],
      [ "9. Testing", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md433", [
        [ "Unit Tests", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md434", null ],
        [ "Standalone Emulator", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md435", null ],
        [ "Running Tests", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md436", null ],
        [ "Testing Checklist", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md437", null ]
      ] ],
      [ "10. Technical Reference", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md439", [
        [ "PPU Registers", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md440", null ],
        [ "CPU Instructions", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md441", null ],
        [ "Color Format", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md442", null ],
        [ "Performance Metrics", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md443", null ]
      ] ],
      [ "11. Troubleshooting", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md445", [
        [ "Emulator Preview Issues", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md446", null ],
        [ "Color Display Issues", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md447", null ],
        [ "Performance Issues", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md448", null ],
        [ "Build Issues", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md449", null ]
      ] ],
      [ "11.5 Audio System Architecture (October 2025)", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md451", [
        [ "Overview", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md452", null ],
        [ "Audio Backend Abstraction", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md453", null ],
        [ "APU Handshake Debugging System", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md454", null ],
        [ "IPL ROM Handshake Protocol", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md455", null ],
        [ "Music Editor Integration", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md456", null ],
        [ "Audio Testing & Diagnostics", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md457", null ],
        [ "Future Enhancements", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md458", null ]
      ] ],
      [ "12. Next Steps & Roadmap", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md460", [
        [ "🎯 Immediate Priorities (Critical Path to Full Functionality)", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md461", null ],
        [ "🚀 Enhancement Priorities (After Core is Stable)", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md462", null ],
        [ "📝 Technical Debt", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md463", null ],
        [ "Long-Term Enhancements", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md464", null ]
      ] ],
      [ "13. Build Instructions", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md466", [
        [ "Quick Build", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md467", null ],
        [ "Platform-Specific", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md468", null ],
        [ "Build Optimizations", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md469", null ]
      ] ],
      [ "File Reference", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md471", [
        [ "Core Emulation", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md472", null ],
        [ "Debugging", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md473", null ],
        [ "UI", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md474", null ],
        [ "Core", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md475", null ],
        [ "Testing", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md476", null ]
      ] ],
      [ "Status Summary", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md478", [
        [ "✅ Production Ready", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md479", null ]
      ] ]
    ] ],
    [ "E5 - Debugging and Testing Guide", "de/dc5/md_docs_2E5-debugging-guide.html", [
      [ "1. Standardized Logging for Print Debugging", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md482", [
        [ "Log Levels and Usage", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md483", null ],
        [ "Log Categories", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md484", null ],
        [ "Enabling and Configuring Logs via CLI", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md485", null ],
        [ "2. Command-Line Workflows for Testing", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md488", [
          [ "Launching the GUI for Specific Tasks", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md489", null ]
        ] ]
      ] ],
      [ "Open the Dungeon Editor with the Room Matrix and two specific room cards", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md490", null ],
      [ "Available editors: Assembly, Dungeon, Graphics, Music, Overworld, Palette,", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md491", null ],
      [ "Screen, Sprite, Message, Hex, Agent, Settings", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md492", null ],
      [ "Dungeon editor cards: Rooms List, Room Matrix, Entrances List, Room Graphics,", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md493", null ],
      [ "Object Editor, Palette Editor, Room N (where N is room ID)", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md494", null ],
      [ "Fast dungeon room testing", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md495", null ],
      [ "Compare multiple rooms side-by-side", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md496", null ],
      [ "Full dungeon workspace with all tools", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md497", null ],
      [ "Jump straight to overworld editing", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md498", null ],
      [ "Run only fast, dependency-free unit tests", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md500", null ],
      [ "Run tests that require a ROM file", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md501", [
        [ "3. GUI Automation for AI Agents", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md505", [
          [ "Inspecting ROMs with <tt>z3ed</tt>", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md503", null ],
          [ "Architecture Overview", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md506", null ],
          [ "Step-by-Step Workflow for AI", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md507", [
            [ "Step 1: Launch <tt>yaze</tt> with the Test Harness", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md508", null ],
            [ "Step 2: Discover UI Elements", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md509", null ],
            [ "Step 3: Record or Write a Test Script", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md510", null ]
          ] ]
        ] ]
      ] ],
      [ "Start yaze with the room already open", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md511", null ],
      [ "Then your test script just needs to validate the state", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md512", [
        [ "4. Advanced Debugging Tools", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md515", null ]
      ] ]
    ] ],
    [ "Emulator Core Improvements Roadmap", "d3/d49/md_docs_2E6-emulator-improvements.html", [
      [ "Overview", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md517", null ],
      [ "Critical Priority: APU Timing Fix", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md519", [
        [ "Problem Statement", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md520", null ],
        [ "Root Cause: CPU-APU Handshake Timing", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md521", [
          [ "The Handshake Protocol", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md522", null ],
          [ "Point of Failure", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md523", null ]
        ] ],
        [ "Technical Analysis", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md524", [
          [ "Issue 1: Incomplete Opcode Timing", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md525", null ],
          [ "Issue 2: Fragile Multi-Step Execution Model", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md526", null ],
          [ "Issue 3: Floating-Point Precision", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md527", null ]
        ] ],
        [ "Proposed Solution: Cycle-Accurate Refactoring", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md528", [
          [ "Step 1: Implement Cycle-Accurate Instruction Execution", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md529", null ],
          [ "Step 2: Centralize the APU Execution Loop", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md530", null ],
          [ "Step 3: Use Integer-Based Cycle Ratios", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md531", null ]
        ] ]
      ] ],
      [ "High Priority: Core Architecture & Timing Model", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md533", [
        [ "CPU Cycle Counting", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md534", null ],
        [ "Main Synchronization Loop", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md535", null ]
      ] ],
      [ "Medium Priority: PPU Performance", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md537", [
        [ "Rendering Approach Optimization", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md538", null ]
      ] ],
      [ "Low Priority: Code Quality & Refinements", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md540", [
        [ "APU Code Modernization", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md541", null ],
        [ "Audio Subsystem & Buffering", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md542", null ],
        [ "Debugger & Tooling Optimizations", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md543", [
          [ "DisassemblyViewer Data Structure", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md544", null ],
          [ "BreakpointManager Lookups", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md545", null ]
        ] ]
      ] ],
      [ "Implementation Priority", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md547", null ],
      [ "Success Metrics", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md549", [
        [ "APU Timing Fix Success", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md550", null ],
        [ "Overall Emulation Accuracy", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md551", null ],
        [ "Performance Targets", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md552", null ]
      ] ],
      [ "Related Documentation", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md554", null ]
    ] ],
    [ "YAZE Startup Debugging Flags", "db/da6/md_docs_2E7-debugging-startup-flags.html", [
      [ "Basic Usage", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md557", null ],
      [ "Available Flags", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md558", [
        [ "<tt>--rom_file</tt>", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md559", null ],
        [ "<tt>--debug</tt>", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md560", null ],
        [ "<tt>--editor</tt>", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md561", null ],
        [ "<tt>--cards</tt>", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md562", null ]
      ] ],
      [ "Common Debugging Scenarios", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md563", [
        [ "1. Quick Dungeon Room Testing", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md564", null ],
        [ "2. Multiple Room Comparison", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md565", null ],
        [ "3. Full Dungeon Editor Workspace", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md566", null ],
        [ "4. Debug Mode with Logging", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md567", null ],
        [ "5. Quick Overworld Editing", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md568", null ]
      ] ],
      [ "gRPC Test Harness (Developer Feature)", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md569", null ],
      [ "Combining Flags", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md570", null ],
      [ "Notes", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md571", null ],
      [ "Troubleshooting", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md572", null ]
    ] ],
    [ "YAZE Emulator Enhancement Roadmap", "df/d0c/md_docs_2E8-emulator-debugging-vision.html", [
      [ "📋 Executive Summary", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md575", [
        [ "Core Objectives", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md576", null ]
      ] ],
      [ "🎯 Current State Analysis", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md578", [
        [ "What Works ✅", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md579", null ],
        [ "What's Broken ❌", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md580", null ],
        [ "What's Missing 🚧", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md581", null ]
      ] ],
      [ "🔧 Phase 1: Audio System Fix (Priority: CRITICAL)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md583", [
        [ "Problem Analysis", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md584", null ],
        [ "Investigation Steps", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md585", null ],
        [ "Likely Fixes", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md586", null ],
        [ "Quick Win Actions", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md587", null ]
      ] ],
      [ "🐛 Phase 2: Advanced Debugger (Mesen2 Feature Parity)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md589", [
        [ "Feature Comparison: YAZE vs Mesen2", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md590", null ],
        [ "2.1 Breakpoint System", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md591", null ],
        [ "2.2 Memory Watchpoints", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md593", null ],
        [ "2.3 Live Disassembly Viewer", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md595", null ],
        [ "2.4 Enhanced Memory Viewer", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md597", null ]
      ] ],
      [ "🚀 Phase 3: Performance Optimizations", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md599", [
        [ "3.1 Cycle-Accurate Timing", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md600", null ],
        [ "3.2 Dynamic Recompilation (Dynarec)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md602", null ],
        [ "3.3 Frame Pacing Improvements", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md604", null ]
      ] ],
      [ "🎮 Phase 4: SPC700 Audio CPU Debugger", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md606", [
        [ "4.1 APU Inspector Window", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md607", null ],
        [ "4.2 Audio Sample Export", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md609", null ]
      ] ],
      [ "🤖 Phase 5: z3ed AI Agent Integration", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md611", [
        [ "5.1 Emulator State Access", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md612", null ],
        [ "5.2 Automated Test Generation", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md614", null ],
        [ "5.3 Memory Map Learning", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md616", null ]
      ] ],
      [ "📊 Phase 6: Performance Profiling", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md618", [
        [ "6.1 Cycle Counter & Profiler", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md619", null ],
        [ "6.2 Frame Time Analysis", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md621", null ]
      ] ],
      [ "🎯 Phase 7: Event System & Timeline", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md623", [
        [ "7.1 Event Logger", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md624", null ]
      ] ],
      [ "🧠 Phase 8: AI-Powered Debugging", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md626", [
        [ "8.1 Intelligent Crash Analysis", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md627", null ],
        [ "8.2 Automated Bug Reproduction", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md629", null ]
      ] ],
      [ "🗺️ Implementation Roadmap", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md631", [
        [ "Sprint 1: Audio Fix (Week 1)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md632", null ],
        [ "Sprint 2: Basic Debugger (Weeks 2-3)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md633", null ],
        [ "Sprint 3: SPC700 Debugger (Week 4)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md634", null ],
        [ "Sprint 4: AI Integration (Weeks 5-6)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md635", null ],
        [ "Sprint 5: Performance (Weeks 7-8)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md636", null ]
      ] ],
      [ "🔬 Technical Deep Dives", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md638", [
        [ "Audio System Architecture (SDL2)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md639", null ],
        [ "Memory Regions Reference", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md641", null ]
      ] ],
      [ "🎮 Phase 9: Advanced Features (Mesen2 Parity)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md643", [
        [ "9.1 Rewind Feature", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md644", null ],
        [ "9.2 TAS (Tool-Assisted Speedrun) Input Recording", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md646", null ],
        [ "9.3 Comparison Mode", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md648", null ]
      ] ],
      [ "🛠️ Optimization Summary", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md650", [
        [ "Quick Wins (< 1 week)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md651", null ],
        [ "Medium Term (1-2 months)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md652", null ],
        [ "Long Term (3-6 months)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md653", null ]
      ] ],
      [ "🤖 z3ed Agent Emulator Tools", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md655", [
        [ "New Tool Categories", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md656", null ],
        [ "Example AI Conversations", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md657", null ]
      ] ],
      [ "📁 File Structure for New Features", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md659", null ],
      [ "🎨 UI Mockups", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md661", [
        [ "Debugger Layout (ImGui)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md662", null ],
        [ "APU Debugger Layout", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md663", null ]
      ] ],
      [ "🚀 Performance Targets", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md665", [
        [ "Current Performance", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md666", null ],
        [ "Target Performance (Post-Optimization)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md667", null ],
        [ "Optimization Strategy Priority", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md668", null ]
      ] ],
      [ "🧪 Testing Integration", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md670", [
        [ "Automated Emulator Tests (z3ed)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md671", null ]
      ] ],
      [ "🔌 z3ed Agent + Emulator Integration", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md673", [
        [ "New Agent Tools", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md674", null ]
      ] ],
      [ "🎓 Learning from Mesen2", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md676", [
        [ "What Makes Mesen2 Great", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md677", null ],
        [ "Our Unique Advantages", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md678", null ]
      ] ],
      [ "📊 Resource Requirements", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md680", [
        [ "Development Time Estimates", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md681", null ],
        [ "Memory Requirements", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md682", null ],
        [ "CPU Requirements", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md683", null ]
      ] ],
      [ "🛣️ Recommended Implementation Order", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md685", [
        [ "Month 1: Foundation", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md686", null ],
        [ "Month 2: Audio & Events", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md687", null ],
        [ "Month 3: AI Integration", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md688", null ],
        [ "Month 4: Performance", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md689", null ],
        [ "Month 5: Polish", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md690", null ]
      ] ],
      [ "🔮 Future Vision: AI-Powered ROM Hacking", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md692", [
        [ "The Ultimate Workflow", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md693", null ]
      ] ],
      [ "🐛 Appendix A: Audio Debugging Checklist", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md695", [
        [ "Check 1: Device Status", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md696", null ],
        [ "Check 2: Queue Size", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md697", null ],
        [ "Check 3: Sample Validation", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md698", null ],
        [ "Check 4: Buffer Allocation", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md699", null ],
        [ "Check 5: SPC700 Execution", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md700", null ],
        [ "Quick Fixes to Try", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md701", null ]
      ] ],
      [ "📝 Appendix B: Mesen2 Feature Reference", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md703", [
        [ "Debugger Windows (Inspiration)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md704", null ],
        [ "Event Types Tracked", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md705", null ],
        [ "Trace Logger Format", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md706", null ]
      ] ],
      [ "🎯 Success Criteria", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md708", [
        [ "Phase 1 Complete When:", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md709", null ],
        [ "Phase 2 Complete When:", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md710", null ],
        [ "Phase 3 Complete When:", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md711", null ],
        [ "Phase 4 Complete When:", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md712", null ],
        [ "Phase 5 Complete When:", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md713", null ]
      ] ],
      [ "🎓 Learning Resources", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md715", [
        [ "SNES Emulation", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md716", null ],
        [ "Audio Debugging", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md717", null ],
        [ "Performance Optimization", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md718", null ]
      ] ],
      [ "🙏 Credits & Acknowledgments", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md720", null ]
    ] ],
    [ "F2: Dungeon Editor v2 - Complete Guide", "d5/d83/md_docs_2F1-dungeon-editor-guide.html", [
      [ "Overview", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md724", [
        [ "Key Features", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md725", null ],
        [ "Architecture Improvements ✅", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md727", null ],
        [ "UI Improvements ✅", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md728", null ]
      ] ],
      [ "Architecture", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md730", [
        [ "Component Overview", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md731", null ],
        [ "Room Rendering Pipeline", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md732", null ],
        [ "Room Structure (Bottom to Top)", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md733", null ]
      ] ],
      [ "Next Development Steps", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md735", [
        [ "High Priority (Must Do)", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md736", [
          [ "1. Door Rendering at Room Edges", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md737", null ],
          [ "2. Object Name Labels from String Array", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md739", null ],
          [ "4. Fix Plus Button to Select Any Room", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md741", null ]
        ] ],
        [ "Medium Priority (Should Do)", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md743", [
          [ "6. Fix InputHexByte +/- Button Events", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md744", null ]
        ] ],
        [ "Lower Priority (Nice to Have)", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md745", [
          [ "9. Move Backend Logic to DungeonEditorSystem", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md746", null ]
        ] ]
      ] ],
      [ "Quick Start", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md748", [
        [ "Build & Run", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md749", null ]
      ] ],
      [ "Testing & Verification", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md751", [
        [ "Debug Commands", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md752", null ]
      ] ],
      [ "Related Documentation", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md753", null ]
    ] ],
    [ "Tile16 Editor Palette System", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html", [
      [ "Executive Summary", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md756", null ],
      [ "Problem Analysis", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md757", [
        [ "Critical Issues Identified", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md758", null ]
      ] ],
      [ "Solution Architecture", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md759", [
        [ "Core Design Principles", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md760", null ],
        [ "256-Color Overworld Palette Structure", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md761", null ],
        [ "Sheet-to-Palette Mapping", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md762", null ],
        [ "Palette Button Mapping", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md763", null ]
      ] ],
      [ "Implementation Details", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md764", [
        [ "1. Fixed SetPaletteWithTransparent Method", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md765", null ],
        [ "2. Corrected Tile16 Editor Palette System", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md766", null ],
        [ "3. Palette Coordination Flow", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md767", null ]
      ] ],
      [ "UI/UX Refactoring", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md768", [
        [ "New Three-Column Layout", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md769", null ],
        [ "Canvas Context Menu Fixes", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md770", null ],
        [ "Dynamic Zoom Controls", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md771", null ]
      ] ],
      [ "Testing Protocol", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md772", [
        [ "Crash Prevention Testing", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md773", null ],
        [ "Color Alignment Testing", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md774", null ],
        [ "UI/UX Testing", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md775", null ]
      ] ],
      [ "Error Handling", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md776", [
        [ "Bounds Checking", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md777", null ],
        [ "Fallback Mechanisms", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md778", null ]
      ] ],
      [ "Debug Information Display", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md779", null ],
      [ "Known Issues and Ongoing Work", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md780", [
        [ "Completed Items ✅", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md781", null ],
        [ "Active Issues ⚠️", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md782", null ],
        [ "Current Status Summary", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md783", null ],
        [ "Future Enhancements", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md784", null ]
      ] ],
      [ "Maintenance Notes", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md785", null ],
      [ "Next Steps", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md787", [
        [ "Immediate Priorities", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md788", null ],
        [ "Investigation Areas", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md789", null ]
      ] ]
    ] ],
    [ "Overworld Loading Guide", "d9/d85/md_docs_2F3-overworld-loading.html", [
      [ "Table of Contents", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md792", null ],
      [ "Overview", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md793", null ],
      [ "ROM Types and Versions", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md794", [
        [ "Version Detection", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md795", null ],
        [ "Feature Support by Version", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md796", null ]
      ] ],
      [ "Overworld Map Structure", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md797", [
        [ "Core Properties", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md798", null ]
      ] ],
      [ "Overlays and Special Area Maps", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md799", [
        [ "Understanding Overlays", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md800", null ],
        [ "Special Area Maps (0x80-0x9F)", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md801", null ],
        [ "Overlay ID Mappings", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md802", null ],
        [ "Drawing Order", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md803", null ],
        [ "Vanilla Overlay Loading", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md804", null ],
        [ "Special Area Graphics Loading", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md805", null ]
      ] ],
      [ "Loading Process", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md806", [
        [ "1. Version Detection", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md807", null ],
        [ "2. Map Initialization", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md808", null ],
        [ "3. Property Loading", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md809", [
          [ "Vanilla ROMs (asm_version == 0xFF)", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md810", null ],
          [ "ZSCustomOverworld v2/v3", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md811", null ]
        ] ],
        [ "4. Custom Data Loading", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md812", null ]
      ] ],
      [ "ZScream Implementation", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md813", [
        [ "OverworldMap Constructor", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md814", null ],
        [ "Key Methods", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md815", null ]
      ] ],
      [ "Yaze Implementation", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md816", [
        [ "OverworldMap Constructor", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md817", null ],
        [ "Key Methods", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md818", null ],
        [ "Current Status", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md819", null ]
      ] ],
      [ "Key Differences", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md820", [
        [ "1. Language and Architecture", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md821", null ],
        [ "2. Data Structures", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md822", null ],
        [ "3. Error Handling", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md823", null ],
        [ "4. Graphics Processing", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md824", null ]
      ] ],
      [ "Common Issues and Solutions", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md825", [
        [ "1. Version Detection Issues", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md826", null ],
        [ "2. Palette Loading Errors", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md827", null ],
        [ "3. Graphics Not Loading", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md828", null ],
        [ "4. Overlay Issues", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md829", null ],
        [ "5. Large Map Problems", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md830", null ],
        [ "6. Special Area Graphics Issues", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md831", null ]
      ] ],
      [ "Best Practices", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md832", [
        [ "1. Version-Specific Code", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md833", null ],
        [ "2. Error Handling", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md834", null ],
        [ "3. Memory Management", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md835", null ],
        [ "4. Thread Safety", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md836", null ]
      ] ],
      [ "Conclusion", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md837", null ]
    ] ],
    [ "Overworld Agent Guide - AI-Powered Overworld Editing", "de/d00/md_docs_2F4-overworld-agent-guide.html", [
      [ "Overview", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md840", null ],
      [ "Quick Start", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md842", [
        [ "Prerequisites", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md843", null ],
        [ "First Agent Interaction", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md844", null ]
      ] ],
      [ "Available Tools", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md846", [
        [ "Read-Only Tools (Safe for AI)", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md847", [
          [ "overworld-get-tile", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md848", null ],
          [ "overworld-get-visible-region", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md850", null ],
          [ "overworld-analyze-region", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md852", null ]
        ] ],
        [ "Write Tools (Sandboxed - Creates Proposals)", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md854", [
          [ "overworld-set-tile", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md855", null ],
          [ "overworld-set-tiles-batch", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md857", null ]
        ] ]
      ] ],
      [ "Multimodal Vision Workflow", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md859", [
        [ "Step 1: Capture Canvas Screenshot", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md860", null ],
        [ "Step 2: AI Analyzes Screenshot", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md861", null ],
        [ "Step 3: Generate Edit Plan", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md862", null ],
        [ "Step 4: Execute Plan (Sandbox)", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md863", null ],
        [ "Step 5: Human Review", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md864", null ]
      ] ],
      [ "Example Workflows", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md866", [
        [ "Workflow 1: Create Forest Area", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md867", null ],
        [ "Workflow 2: Fix Tile Placement Errors", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md869", null ],
        [ "Workflow 3: Generate Path", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md871", null ]
      ] ],
      [ "Common Tile IDs Reference", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md873", [
        [ "Grass & Ground", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md874", null ],
        [ "Trees & Plants", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md875", null ],
        [ "Water", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md876", null ],
        [ "Paths & Roads", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md877", null ],
        [ "Structures", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md878", null ]
      ] ],
      [ "Best Practices for AI Agents", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md880", [
        [ "1. Always Analyze Before Editing", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md881", null ],
        [ "2. Use Batch Operations", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md882", null ],
        [ "3. Provide Clear Reasoning", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md883", null ],
        [ "4. Respect Tile Boundaries", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md884", null ],
        [ "5. Check Visibility", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md885", null ],
        [ "6. Create Reversible Edits", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md886", null ]
      ] ],
      [ "Error Handling", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md888", [
        [ "\"Tile ID out of range\"", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md889", null ],
        [ "\"Coordinates out of bounds\"", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md890", null ],
        [ "\"Proposal rejected\"", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md891", null ],
        [ "\"ROM file locked\"", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md892", null ]
      ] ],
      [ "Testing AI-Generated Edits", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md894", [
        [ "Manual Testing", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md895", null ],
        [ "Automated Testing", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md896", null ]
      ] ],
      [ "Advanced Techniques", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md898", [
        [ "Technique 1: Pattern Recognition", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md899", null ],
        [ "Technique 2: Style Transfer", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md900", null ],
        [ "Technique 3: Procedural Generation", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md901", null ]
      ] ],
      [ "Integration with GUI Automation", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md903", [
        [ "Record Human Edits", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md904", null ],
        [ "Replay for AI Training", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md905", null ],
        [ "Validate AI Edits", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md906", null ]
      ] ],
      [ "Collaboration Features", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md908", [
        [ "Network Collaboration", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md909", null ],
        [ "Proposal Voting", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md910", null ]
      ] ],
      [ "Troubleshooting", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md912", [
        [ "Agent Not Responding", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md913", null ],
        [ "Tools Not Available", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md914", null ],
        [ "gRPC Connection Failed", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md915", null ]
      ] ],
      [ "See Also", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md917", null ]
    ] ],
    [ "Canvas System Overview", "d1/dc6/md_docs_2G1-canvas-guide.html", [
      [ "Canvas Architecture", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md919", null ],
      [ "Core API Patterns", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md920", null ],
      [ "Context Menu Sections", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md921", null ],
      [ "Interaction Modes & Capabilities", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md922", null ],
      [ "Debug & Diagnostics", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md923", null ],
      [ "Automation API", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md924", null ],
      [ "Integration Steps for Editors", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md925", null ],
      [ "Migration Checklist", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md926", null ],
      [ "Testing Notes", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md927", null ]
    ] ],
    [ "SDL2 to SDL3 Migration and Rendering Abstraction Plan", "d6/df2/md_docs_2G2-renderer-migration-plan.html", [
      [ "1. Introduction", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md929", null ],
      [ "2. Current State Analysis", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md930", null ],
      [ "3. Proposed Architecture: The <tt>Renderer</tt> Abstraction", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md931", [
        [ "3.1. The <tt>IRenderer</tt> Interface", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md932", null ],
        [ "3.2. The <tt>SDL2Renderer</tt> Implementation", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md933", null ]
      ] ],
      [ "4. Migration Plan", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md934", [
        [ "Phase 1: Implement the Abstraction Layer", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md935", null ],
        [ "Phase 2: Migrate to SDL3", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md936", null ],
        [ "Phase 3: Support for Multiple Rendering Backends", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md937", null ]
      ] ],
      [ "5. Conclusion", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md938", null ]
    ] ],
    [ "SNES Palette System Overview", "da/dfd/md_docs_2G3-palete-system-overview.html", [
      [ "Understanding SNES Color and Palette Organization", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md940", [
        [ "Core Concepts", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md941", [
          [ "1. SNES Color Format (15-bit BGR555)", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md942", null ],
          [ "2. Palette Groups in Zelda 3", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md943", null ]
        ] ],
        [ "Dungeon Palette System", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md944", [
          [ "Structure", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md945", null ],
          [ "Usage", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md946", null ],
          [ "Color Distribution (90 colors)", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md947", null ]
        ] ],
        [ "Overworld Palette System", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md948", [
          [ "Structure", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md949", null ],
          [ "3BPP Graphics and Left/Right Palettes", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md950", null ]
        ] ],
        [ "Common Issues and Solutions", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md951", [
          [ "Issue 1: Empty Palette", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md952", null ],
          [ "Issue 2: Bitmap Corruption", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md953", null ],
          [ "Issue 3: ROM Not Loaded in Preview", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md954", null ]
        ] ],
        [ "Palette Editor Integration", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md955", [
          [ "Key Functions for UI", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md956", null ],
          [ "Palette Widget Requirements", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md957", null ]
        ] ],
        [ "Graphics Manager Integration", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md958", [
          [ "Sheet Palette Assignment", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md959", null ]
        ] ],
        [ "Best Practices", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md960", null ],
        [ "ROM Addresses (for reference)", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md961", null ]
      ] ],
      [ "Graphics Sheet Palette Application", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md962", [
        [ "Default Palette Assignment", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md963", null ],
        [ "Palette Update Workflow", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md964", null ],
        [ "Common Pitfalls", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md965", null ]
      ] ]
    ] ],
    [ "Graphics Renderer Migration - Complete Documentation", "d5/dc8/md_docs_2G3-renderer-migration-complete.html", [
      [ "📋 Executive Summary", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md968", [
        [ "Key Achievements", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md969", null ]
      ] ],
      [ "🎯 Migration Goals & Results", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md971", null ],
      [ "🏗️ Architecture Overview", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md973", [
        [ "Before: Singleton Pattern", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md974", null ],
        [ "After: Dependency Injection + Deferred Queue", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md975", null ]
      ] ],
      [ "📦 Component Details", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md977", [
        [ "1. IRenderer Interface (<tt>src/app/gfx/backend/irenderer.h</tt>)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md978", null ],
        [ "2. SDL2Renderer (<tt>src/app/gfx/backend/sdl2_renderer.{h,cc}</tt>)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md980", null ],
        [ "3. Arena Deferred Texture Queue (<tt>src/app/gfx/arena.{h,cc}</tt>)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md982", null ],
        [ "4. Bitmap Palette Refactoring (<tt>src/app/gfx/bitmap.{h,cc}</tt>)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md984", null ],
        [ "5. Canvas Optional Renderer (<tt>src/app/gui/canvas.{h,cc}</tt>)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md986", null ],
        [ "6. Tilemap Texture Queue Integration (<tt>src/app/gfx/tilemap.cc</tt>)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md988", null ]
      ] ],
      [ "🔄 Dependency Injection Flow", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md990", [
        [ "Controller → EditorManager → Editors", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md991", null ]
      ] ],
      [ "⚡ Performance Optimizations", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md993", [
        [ "1. Batched Texture Processing", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md994", null ],
        [ "2. Frame Rate Limiting", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md995", null ],
        [ "3. Auto-Pause on Focus Loss", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md996", null ],
        [ "4. Surface/Texture Pooling", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md997", null ]
      ] ],
      [ "🗺️ Migration Map: File Changes", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md999", [
        [ "Core Architecture Files (New)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1000", null ],
        [ "Core Modified Files (Major)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1001", null ],
        [ "Editor Files (Renderer Injection)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1002", null ],
        [ "Emulator Files (Special Handling)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1003", null ],
        [ "GUI/Widget Files", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1004", null ],
        [ "Test Files (Updated for DI)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1005", null ]
      ] ],
      [ "🔧 Critical Fixes Applied", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1007", [
        [ "1. Bitmap::SetPalette() Crash", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1008", null ],
        [ "2. SDL2Renderer::UpdateTexture() SIGSEGV", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1010", null ],
        [ "3. Emulator Audio System Corruption", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1012", null ],
        [ "4. Emulator Cleanup During Shutdown", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1014", null ],
        [ "5. Controller/CreateWindow Initialization Order", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1016", null ]
      ] ],
      [ "🎨 Canvas Refactoring", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1018", [
        [ "The Challenge", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1019", null ],
        [ "The Solution: Backwards-Compatible Dual API", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1020", null ]
      ] ],
      [ "🧪 Testing Strategy", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1022", [
        [ "Test Files Updated", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1023", null ]
      ] ],
      [ "🛣️ Road to SDL3", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1025", [
        [ "Why This Migration Matters", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1026", null ],
        [ "Our Abstraction Layer Handles This", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1027", null ]
      ] ],
      [ "📊 Performance Benchmarks", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1029", [
        [ "Texture Loading Performance", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1030", null ],
        [ "Graphics Editor Performance", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1031", null ],
        [ "Emulator Performance", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1032", null ]
      ] ],
      [ "🐛 Bugs Fixed During Migration", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1034", [
        [ "Critical Crashes", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1035", null ],
        [ "Build Errors", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1036", null ]
      ] ],
      [ "💡 Key Design Patterns Used", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1038", [
        [ "1. Dependency Injection", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1039", null ],
        [ "2. Command Pattern (Deferred Queue)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1040", null ],
        [ "3. RAII (Resource Management)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1041", null ],
        [ "4. Adapter Pattern (Backend Abstraction)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1042", null ],
        [ "5. Singleton with DI (Arena)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1043", null ]
      ] ],
      [ "🔮 Future Enhancements", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1045", [
        [ "Short Term (SDL2)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1046", null ],
        [ "Medium Term (SDL3 Prep)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1047", null ],
        [ "Long Term (SDL3 Migration)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1048", null ]
      ] ],
      [ "📝 Lessons Learned", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1050", [
        [ "What Went Well", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1051", null ],
        [ "Challenges Overcome", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1052", null ],
        [ "Best Practices Established", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1053", null ]
      ] ],
      [ "🎓 Technical Deep Dive: Texture Queue System", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1055", [
        [ "Why Deferred Rendering?", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1056", null ],
        [ "Queue Processing Algorithm", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1057", null ]
      ] ],
      [ "🏆 Success Metrics", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1059", [
        [ "Build Health", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1060", null ],
        [ "Runtime Stability", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1061", null ],
        [ "Performance", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1062", null ],
        [ "Code Quality", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1063", null ]
      ] ],
      [ "📚 References", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1065", [
        [ "Related Documents", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1066", null ],
        [ "Key Commits", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1067", null ],
        [ "External Resources", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1068", null ]
      ] ],
      [ "🙏 Acknowledgments", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1070", null ],
      [ "🎉 Conclusion", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1072", null ],
      [ "🚧 Known Issues & Next Steps", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1074", [
        [ "macOS-Specific Issues (Not Renderer-Related)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1075", null ],
        [ "Stability Improvements for Next Session", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1076", [
          [ "High Priority", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1077", null ],
          [ "Medium Priority", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1078", null ],
          [ "Low Priority (SDL3 Migration)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1079", null ]
        ] ],
        [ "Testing Recommendations", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1080", null ]
      ] ],
      [ "🎵 Final Notes", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1082", null ]
    ] ],
    [ "Canvas Coordinate Synchronization and Scroll Fix", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html", [
      [ "Problem Summary", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1085", null ],
      [ "Root Cause", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1086", [
        [ "Issue 1: Wrong Coordinate System (Line 1041)", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1087", null ],
        [ "Issue 2: Hover Position Not Updated (Line 416)", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1088", null ]
      ] ],
      [ "Technical Details", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1089", [
        [ "Coordinate Spaces", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1090", null ],
        [ "Usage Patterns", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1091", null ]
      ] ],
      [ "Testing", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1092", [
        [ "Visual Testing", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1093", null ],
        [ "Unit Tests", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1094", null ]
      ] ],
      [ "Impact Analysis", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1095", [
        [ "Files Changed", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1096", null ],
        [ "Affected Functionality", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1097", null ],
        [ "Related Code That Works Correctly", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1098", null ]
      ] ],
      [ "Multi-Area Map Support", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1099", [
        [ "Standard Maps (512x512)", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1100", null ],
        [ "ZSCustomOverworld v3 Large Maps (1024x1024)", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1101", null ]
      ] ],
      [ "Issue 3: Wrong Canvas Being Scrolled (Line 2344-2366)", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1102", null ],
      [ "Issue 4: Wrong Hover Check (Line 1403)", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1103", null ],
      [ "Issue 5: Vanilla Large Map World Offset (Line 1132-1136)", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1104", null ],
      [ "Commit Reference", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1105", null ],
      [ "Future Improvements", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1106", null ],
      [ "Related Documentation", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1107", null ]
    ] ],
    [ "Changelog", "d6/da7/md_docs_2H1-changelog.html", [
      [ "0.3.2 (October 2025)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1109", [
        [ "CI/CD & Release Improvements", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1110", null ],
        [ "Rendering Pipeline Fixes", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1111", null ],
        [ "Card-Based UI System", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1112", null ],
        [ "Tile16 Editor & Graphics System", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1113", null ],
        [ "Windows Platform Stability", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1114", null ],
        [ "Emulator: Audio System Infrastructure", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1115", null ],
        [ "Emulator: Critical Performance Fixes", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1116", null ],
        [ "Emulator: UI Organization & Input System", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1117", null ],
        [ "Debugger: Breakpoint & Watchpoint Systems", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1118", null ],
        [ "Build System Simplifications", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1119", null ],
        [ "Build System: Windows Platform Improvements", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1120", null ],
        [ "GUI & UX Modernization", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1121", null ],
        [ "Overworld Editor Refactoring", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1122", null ],
        [ "Build System & Stability", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1123", null ],
        [ "Future Optimizations (Planned)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1124", null ],
        [ "Technical Notes", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1125", null ]
      ] ],
      [ "0.3.1 (September 2025)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1126", [
        [ "Major Features", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1127", null ],
        [ "Tile16 Editor Enhancements", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1128", null ],
        [ "ZSCustomOverworld v3 Implementation", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1129", null ],
        [ "Technical Improvements", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1130", null ],
        [ "User Interface", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1131", null ],
        [ "Bug Fixes", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1132", null ],
        [ "ZScream Compatibility Improvements", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1133", null ]
      ] ],
      [ "0.3.0 (September 2025)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1134", [
        [ "Major Features", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1135", null ],
        [ "User Interface & Theming", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1136", null ],
        [ "Enhancements", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1137", null ],
        [ "Technical Improvements", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1138", null ],
        [ "Bug Fixes", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1139", null ]
      ] ],
      [ "0.2.2 (December 2024)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1140", null ],
      [ "0.2.1 (August 2024)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1141", null ],
      [ "0.2.0 (July 2024)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1142", null ],
      [ "0.1.0 (May 2024)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1143", null ],
      [ "0.0.9 (April 2024)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1144", null ],
      [ "0.0.8 (February 2024)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1145", null ],
      [ "0.0.7 (January 2024)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1146", null ],
      [ "0.0.6 (November 2023)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1147", null ],
      [ "0.0.5 (November 2023)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1148", null ],
      [ "0.0.4 (November 2023)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1149", null ],
      [ "0.0.3 (October 2023)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1150", null ]
    ] ],
    [ "Roadmap", "d8/d97/md_docs_2I1-roadmap.html", [
      [ "Current Focus: AI & Editor Polish", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1152", null ],
      [ "0.4.0 (Next Major Release) - SDL3 Modernization & Core Improvements", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1154", [
        [ "🎯 Primary Goals", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1155", null ],
        [ "Phase 1: Infrastructure (Week 1-2)", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1156", null ],
        [ "Phase 2: SDL3 Core Migration (Week 3-4)", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1157", null ],
        [ "Phase 3: Complete SDL3 Integration (Week 5-6)", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1158", null ],
        [ "Phase 4: Editor Features & UX (Week 7-8)", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1159", null ],
        [ "Phase 5: AI Agent Enhancements (Throughout)", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1160", null ],
        [ "Success Criteria", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1161", null ]
      ] ],
      [ "0.5.X - Feature Expansion", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1163", null ],
      [ "0.6.X - Content & Integration", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1165", null ],
      [ "Recently Completed (v0.3.3 - October 6, 2025)", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1167", null ],
      [ "Recently Completed (v0.3.2)", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1168", null ]
    ] ],
    [ "Future Improvements & Long-Term Vision", "db/da4/md_docs_2I2-future-improvements.html", [
      [ "Architecture & Performance", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1171", [
        [ "Emulator Core Improvements", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1172", null ],
        [ "Plugin Architecture (v0.5.x+)", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1173", null ],
        [ "Multi-Threading Improvements", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1174", null ]
      ] ],
      [ "Graphics & Rendering", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1176", [
        [ "Advanced Graphics Editing", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1177", null ],
        [ "Alternative Rendering Backends", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1178", null ],
        [ "High-DPI / 4K Support", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1179", null ]
      ] ],
      [ "AI & Automation", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1181", [
        [ "Multi-Modal AI Input", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1182", null ],
        [ "Collaborative AI Sessions", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1183", null ],
        [ "Automation & Scripting", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1184", null ]
      ] ],
      [ "Content Editors", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1186", [
        [ "Music Editor UI", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1187", null ],
        [ "Dialogue Editor", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1188", null ],
        [ "Event Editor", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1189", null ],
        [ "Hex Editor Enhancements", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1190", null ]
      ] ],
      [ "Collaboration & Networking", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1192", [
        [ "Real-Time Collaboration Improvements", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1193", null ],
        [ "Cloud ROM Storage", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1194", null ]
      ] ],
      [ "Platform Support", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1196", [
        [ "Web Assembly Build", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1197", null ],
        [ "Mobile Support (iOS/Android)", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1198", null ]
      ] ],
      [ "Quality of Life", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1200", [
        [ "Undo/Redo System Enhancement", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1201", null ],
        [ "Project Templates", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1202", null ],
        [ "Asset Library", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1203", null ],
        [ "Accessibility", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1204", null ]
      ] ],
      [ "Testing & Quality", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1206", [
        [ "Automated Regression Testing", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1207", null ],
        [ "ROM Validation", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1208", null ],
        [ "Continuous Integration Enhancements", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1209", null ]
      ] ],
      [ "Documentation & Community", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1211", [
        [ "API Documentation Generator", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1212", null ],
        [ "Video Tutorial System", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1213", null ],
        [ "ROM Hacking Wiki Integration", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1214", null ]
      ] ],
      [ "Experimental / Research", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1216", [
        [ "Machine Learning Integration", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1217", null ],
        [ "VR/AR Visualization", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1218", null ],
        [ "Symbolic Execution", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1219", null ]
      ] ],
      [ "Implementation Priority", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1221", null ],
      [ "Contributing Ideas", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1223", null ]
    ] ],
    [ "yaze Documentation", "d3/d4c/md_docs_2index.html", [
      [ "A: Getting Started & Testing", "d3/d4c/md_docs_2index.html#autotoc_md1226", null ],
      [ "B: Build & Platform", "d3/d4c/md_docs_2index.html#autotoc_md1227", null ],
      [ "C: <tt>z3ed</tt> CLI", "d3/d4c/md_docs_2index.html#autotoc_md1228", null ],
      [ "E: Development & API", "d3/d4c/md_docs_2index.html#autotoc_md1229", null ],
      [ "F: Technical Documentation", "d3/d4c/md_docs_2index.html#autotoc_md1230", null ],
      [ "G: Graphics & GUI Systems", "d3/d4c/md_docs_2index.html#autotoc_md1231", null ],
      [ "H: Project Info", "d3/d4c/md_docs_2index.html#autotoc_md1232", null ],
      [ "I: Roadmap & Vision", "d3/d4c/md_docs_2index.html#autotoc_md1233", null ],
      [ "R: ROM Reference", "d3/d4c/md_docs_2index.html#autotoc_md1234", null ],
      [ "Documentation Standards", "d3/d4c/md_docs_2index.html#autotoc_md1236", [
        [ "Naming Convention", "d3/d4c/md_docs_2index.html#autotoc_md1237", null ],
        [ "File Naming", "d3/d4c/md_docs_2index.html#autotoc_md1238", null ]
      ] ]
    ] ],
    [ "A Link to the Past ROM Reference", "d7/d4f/md_docs_2R1-alttp-rom-reference.html", [
      [ "Graphics System", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1241", [
        [ "Graphics Sheets", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1242", null ],
        [ "Palette System", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1243", [
          [ "Color Format", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1244", null ],
          [ "Palette Groups", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1245", null ]
        ] ]
      ] ],
      [ "Dungeon System", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1246", [
        [ "Room Data Structure", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1247", null ],
        [ "Tile16 Format", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1248", null ],
        [ "Blocksets", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1249", null ]
      ] ],
      [ "Message System", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1250", [
        [ "Text Data Locations", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1251", null ],
        [ "Character Encoding", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1252", null ],
        [ "Text Commands", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1253", null ],
        [ "Font Graphics", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1254", null ]
      ] ],
      [ "Overworld System", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1255", [
        [ "Map Structure", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1256", null ],
        [ "Area Sizes (ZSCustomOverworld v3+)", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1257", null ],
        [ "Tile Format", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1258", null ]
      ] ],
      [ "Compression", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1259", [
        [ "LC-LZ2 Algorithm", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1260", null ]
      ] ],
      [ "Memory Map", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1261", [
        [ "ROM Banks (LoROM)", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1262", null ],
        [ "Important ROM Locations", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1263", null ]
      ] ]
    ] ],
    [ "yaze - Yet Another Zelda3 Editor", "d0/d30/md_README.html", [
      [ "Version 0.3.2 - Release", "d0/d30/md_README.html#autotoc_md1266", null ],
      [ "Quick Start", "d0/d30/md_README.html#autotoc_md1271", [
        [ "Build", "d0/d30/md_README.html#autotoc_md1272", null ],
        [ "Applications", "d0/d30/md_README.html#autotoc_md1273", null ]
      ] ],
      [ "Usage", "d0/d30/md_README.html#autotoc_md1274", [
        [ "GUI Editor", "d0/d30/md_README.html#autotoc_md1275", null ],
        [ "Command Line Tool", "d0/d30/md_README.html#autotoc_md1276", null ],
        [ "C++ API", "d0/d30/md_README.html#autotoc_md1277", null ]
      ] ],
      [ "Documentation", "d0/d30/md_README.html#autotoc_md1278", null ],
      [ "Supported Platforms", "d0/d30/md_README.html#autotoc_md1279", null ],
      [ "ROM Compatibility", "d0/d30/md_README.html#autotoc_md1280", null ],
      [ "Contributing", "d0/d30/md_README.html#autotoc_md1281", null ],
      [ "License", "d0/d30/md_README.html#autotoc_md1282", null ],
      [ "🙏 Acknowledgments", "d0/d30/md_README.html#autotoc_md1283", null ],
      [ "📸 Screenshots", "d0/d30/md_README.html#autotoc_md1284", null ]
    ] ],
    [ "yaze Build Scripts", "de/d82/md_scripts_2README.html", [
      [ "Windows Scripts", "de/d82/md_scripts_2README.html#autotoc_md1287", [
        [ "vcpkg Setup (Optional)", "de/d82/md_scripts_2README.html#autotoc_md1288", null ]
      ] ],
      [ "Windows Build Workflow", "de/d82/md_scripts_2README.html#autotoc_md1289", [
        [ "Recommended: Visual Studio CMake Mode", "de/d82/md_scripts_2README.html#autotoc_md1290", null ],
        [ "Command Line Build", "de/d82/md_scripts_2README.html#autotoc_md1291", null ],
        [ "Compiler Notes", "de/d82/md_scripts_2README.html#autotoc_md1292", null ]
      ] ],
      [ "Quick Start (Windows)", "de/d82/md_scripts_2README.html#autotoc_md1293", [
        [ "Option 1: Visual Studio (Recommended)", "de/d82/md_scripts_2README.html#autotoc_md1294", null ],
        [ "Option 2: Command Line", "de/d82/md_scripts_2README.html#autotoc_md1295", null ],
        [ "Option 3: With vcpkg (Optional)", "de/d82/md_scripts_2README.html#autotoc_md1296", null ]
      ] ],
      [ "Troubleshooting", "de/d82/md_scripts_2README.html#autotoc_md1297", [
        [ "Common Issues", "de/d82/md_scripts_2README.html#autotoc_md1298", null ],
        [ "Getting Help", "de/d82/md_scripts_2README.html#autotoc_md1299", null ]
      ] ],
      [ "Other Scripts", "de/d82/md_scripts_2README.html#autotoc_md1300", null ],
      [ "Build Environment Verification", "de/d82/md_scripts_2README.html#autotoc_md1301", [
        [ "<tt>verify-build-environment.ps1</tt> / <tt>.sh</tt>", "de/d82/md_scripts_2README.html#autotoc_md1302", null ],
        [ "Usage", "de/d82/md_scripts_2README.html#autotoc_md1303", null ]
      ] ]
    ] ],
    [ "Agent Editor Module", "d6/df7/md_src_2app_2editor_2agent_2README.html", [
      [ "Overview", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1305", null ],
      [ "Architecture", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1306", [
        [ "Core Components", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1307", [
          [ "AgentEditor (<tt>agent_editor.h/cc</tt>)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1308", null ],
          [ "AgentChatWidget (<tt>agent_chat_widget.h/cc</tt>)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1309", null ],
          [ "AgentChatHistoryCodec (<tt>agent_chat_history_codec.h/cc</tt>)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1310", null ]
        ] ],
        [ "Collaboration Coordinators", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1311", [
          [ "AgentCollaborationCoordinator (<tt>agent_collaboration_coordinator.h/cc</tt>)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1312", null ],
          [ "NetworkCollaborationCoordinator (<tt>network_collaboration_coordinator.h/cc</tt>)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1313", null ]
        ] ]
      ] ],
      [ "Usage", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1314", [
        [ "Initialization", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1315", null ],
        [ "Drawing", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1316", null ],
        [ "Session Management", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1317", null ],
        [ "Network Mode (requires YAZE_WITH_GRPC and YAZE_WITH_JSON)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1318", null ]
      ] ],
      [ "File Structure", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1319", null ],
      [ "Build Configuration", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1320", [
        [ "Required", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1321", null ],
        [ "Optional", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1322", null ]
      ] ],
      [ "Data Files", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1323", [
        [ "Local Storage", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1324", null ],
        [ "Session File Format", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1325", null ]
      ] ],
      [ "Integration with EditorManager", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1326", null ],
      [ "Dependencies", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1327", [
        [ "Internal", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1328", null ],
        [ "External (when enabled)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1329", null ]
      ] ],
      [ "Advanced Features (v2.0)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1330", [
        [ "ROM Synchronization", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1331", null ],
        [ "Multimodal Snapshot Sharing", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1332", null ],
        [ "Proposal Management", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1333", null ],
        [ "AI Agent Integration", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1334", null ],
        [ "Health Monitoring", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1335", null ]
      ] ],
      [ "Future Enhancements", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1336", null ],
      [ "Server Protocol", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1337", [
        [ "Client → Server", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1338", null ],
        [ "Server → Client", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1339", null ]
      ] ]
    ] ],
    [ "End-to-End (E2E) Tests", "d9/db0/md_test_2e2e_2README.html", [
      [ "Active Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md1343", [
        [ "✅ Working Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md1344", null ],
        [ "📝 Dungeon Editor Smoke Test", "d9/db0/md_test_2e2e_2README.html#autotoc_md1345", null ]
      ] ],
      [ "Running Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md1346", [
        [ "All E2E Tests (GUI Mode)", "d9/db0/md_test_2e2e_2README.html#autotoc_md1347", null ],
        [ "Specific Test Category", "d9/db0/md_test_2e2e_2README.html#autotoc_md1348", null ],
        [ "Dungeon Editor Test Only", "d9/db0/md_test_2e2e_2README.html#autotoc_md1349", null ]
      ] ],
      [ "Test Development", "d9/db0/md_test_2e2e_2README.html#autotoc_md1350", [
        [ "Creating New Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md1351", null ],
        [ "Register in yaze_test.cc", "d9/db0/md_test_2e2e_2README.html#autotoc_md1352", null ],
        [ "ImGui Test Engine API", "d9/db0/md_test_2e2e_2README.html#autotoc_md1353", null ]
      ] ],
      [ "Test Logging", "d9/db0/md_test_2e2e_2README.html#autotoc_md1354", null ],
      [ "Test Infrastructure", "d9/db0/md_test_2e2e_2README.html#autotoc_md1355", [
        [ "File Organization", "d9/db0/md_test_2e2e_2README.html#autotoc_md1356", null ],
        [ "Helper Functions", "d9/db0/md_test_2e2e_2README.html#autotoc_md1357", null ]
      ] ],
      [ "Future Test Ideas", "d9/db0/md_test_2e2e_2README.html#autotoc_md1358", null ],
      [ "Troubleshooting", "d9/db0/md_test_2e2e_2README.html#autotoc_md1359", [
        [ "Test Crashes in GUI Mode", "d9/db0/md_test_2e2e_2README.html#autotoc_md1360", null ],
        [ "Tests Not Found", "d9/db0/md_test_2e2e_2README.html#autotoc_md1361", null ],
        [ "ImGui Items Not Found", "d9/db0/md_test_2e2e_2README.html#autotoc_md1362", null ]
      ] ],
      [ "References", "d9/db0/md_test_2e2e_2README.html#autotoc_md1363", null ],
      [ "Status", "d9/db0/md_test_2e2e_2README.html#autotoc_md1364", null ]
    ] ],
    [ "yaze Test Suite", "d0/d46/md_test_2README.html", [
      [ "Directory Structure", "d0/d46/md_test_2README.html#autotoc_md1366", null ],
      [ "Test Categories", "d0/d46/md_test_2README.html#autotoc_md1367", [
        [ "Unit Tests (<tt>unit/</tt>)", "d0/d46/md_test_2README.html#autotoc_md1368", null ],
        [ "Integration Tests (<tt>integration/</tt>)", "d0/d46/md_test_2README.html#autotoc_md1369", null ],
        [ "End-to-End Tests (<tt>e2e/</tt>)", "d0/d46/md_test_2README.html#autotoc_md1370", null ]
      ] ],
      [ "Enhanced Test Runner", "d0/d46/md_test_2README.html#autotoc_md1371", [
        [ "Usage Examples", "d0/d46/md_test_2README.html#autotoc_md1372", null ],
        [ "Test Modes", "d0/d46/md_test_2README.html#autotoc_md1373", null ],
        [ "Options", "d0/d46/md_test_2README.html#autotoc_md1374", null ]
      ] ],
      [ "E2E ROM Testing", "d0/d46/md_test_2README.html#autotoc_md1375", [
        [ "Test Cases", "d0/d46/md_test_2README.html#autotoc_md1376", null ]
      ] ],
      [ "ZSCustomOverworld Upgrade Testing", "d0/d46/md_test_2README.html#autotoc_md1377", [
        [ "Supported Upgrades", "d0/d46/md_test_2README.html#autotoc_md1378", null ],
        [ "Test Cases", "d0/d46/md_test_2README.html#autotoc_md1379", null ],
        [ "Version-Specific Features", "d0/d46/md_test_2README.html#autotoc_md1380", [
          [ "Vanilla", "d0/d46/md_test_2README.html#autotoc_md1381", null ],
          [ "v2", "d0/d46/md_test_2README.html#autotoc_md1382", null ],
          [ "v3", "d0/d46/md_test_2README.html#autotoc_md1383", null ]
        ] ]
      ] ],
      [ "Environment Variables", "d0/d46/md_test_2README.html#autotoc_md1384", null ],
      [ "CI/CD Integration", "d0/d46/md_test_2README.html#autotoc_md1385", null ],
      [ "Deprecated Tests", "d0/d46/md_test_2README.html#autotoc_md1386", null ],
      [ "Best Practices", "d0/d46/md_test_2README.html#autotoc_md1387", null ],
      [ "AI Agent Testing", "d0/d46/md_test_2README.html#autotoc_md1388", null ]
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
"d0/d59/structyaze_1_1gui_1_1canvas_1_1PerformanceOptions.html#a7e0916bc6d9f81d9e0106d409c3c512c",
"d0/da5/classyaze_1_1cli_1_1ai_1_1AIGUIController.html#a63b7d50e22ecfb6ff6e2f05f601a2497",
"d0/dc7/classyaze_1_1test_1_1DungeonObjectRenderingE2ETests.html#a14455cbb8a11175a136b666b81ee1a93",
"d0/dff/room__object_8h.html#a5f1053148284ef69ee84ef1d04d61fad",
"d1/d1f/overworld__entrance_8h.html#af2353873362052aac0583f2e63f7ad54",
"d1/d3e/namespaceyaze_1_1editor.html#a068e18c7e1f5b249c88204686e8bfafe",
"d1/d4b/namespaceyaze_1_1gfx_1_1lc__lz2.html#a31d4914cc6f5a2d6cc4c5a06baf7336e",
"d1/d6e/classyaze_1_1cli_1_1EnhancedStatusPanel.html#a71bca2fda6369931d57a3943e9fca3a9",
"d1/d95/ui__helpers_8h.html#aa5fe71a06f516353c07a7d49617e7f46",
"d1/dc4/structyaze_1_1emu_1_1BackgroundLayer.html#a99125faf9e0971e6bc3d2858b82a0305",
"d1/dea/classyaze_1_1gui_1_1EditorCardManager.html#ab3038f0d8ef6e6a3d7b59f8ec26c462b",
"d2/d07/classyaze_1_1emu_1_1Spc700.html#a03472018280044413a8c8b0950dd0366",
"d2/d07/classyaze_1_1emu_1_1Spc700.html#aeb73d338f215b0bb98ee7c601cd06446",
"d2/d39/classyaze_1_1gui_1_1MultiSelect.html#a341ac7e066a2ddcd9163da860c0997b4",
"d2/d4e/classyaze_1_1editor_1_1AgentChatHistoryPopup.html#a37eadafd97522a58e469ac25c743512e",
"d2/d60/structyaze_1_1test_1_1TestResults.html#a265fb9025adc5687e633fd67776c43b7",
"d2/dc2/classyaze_1_1gui_1_1BppFormatUI.html#a0630f792705ff6b6ab9c3048c28a0af5",
"d2/de7/classyaze_1_1editor_1_1DungeonCanvasViewer.html#ab4edefc7ea4fd2360cef02e1885b3dbd",
"d2/dfe/structyaze_1_1gui_1_1EnhancedTheme.html#a366b8af12039498a6d3bfad36f25dade",
"d3/d0d/shortcut__manager_8cc.html#a4145855c2b2efa1fad11c293681c231f",
"d3/d1a/classyaze_1_1gui_1_1BppComparisonTool.html#a1571f3b0654eb7142285d06ea7d01694",
"d3/d30/structyaze_1_1gui_1_1canvas_1_1CanvasPerformanceMetrics.html#a711ca2566c4490e4e66daeb1735afc0e",
"d3/d44/classyaze_1_1editor_1_1OverworldEditor.html#a8ab0533101903f9300f943d1e2ce69e7",
"d3/d4c/md_docs_2index.html#autotoc_md1226",
"d3/d6c/classyaze_1_1editor_1_1MessageEditor.html#a23c91e0627d15977203b2a71110fd287",
"d3/d8d/classyaze_1_1editor_1_1PopupManager.html#a79a4c0a49254f2c7a6bde3df1bd89480",
"d3/d9f/classyaze_1_1editor_1_1Editor.html#aa4444a268047e82e40ae536ecf805fa0",
"d3/db7/structyaze_1_1gui_1_1WidgetIdRegistry_1_1WidgetInfo.html#ad6577ae9ea2a1c4ea472c447b9ece70e",
"d3/ddd/classyaze_1_1test_1_1GeminiVisionTest.html#a579a0bb704309b87784b4ba2be0ad67a",
"d3/ded/classyaze_1_1emu_1_1Ppu.html#aab9f934d87a9e6384fda77078c55df9a",
"d4/d0a/namespaceyaze_1_1test.html#a11813a4209d8b1b884acc896acf6d931",
"d4/d0a/namespaceyaze_1_1test.html#af0d7a5c953818b118ce1c6b4f3d4b98f",
"d4/d57/classyaze_1_1zelda3_1_1SpriteBuilder.html#a07b8381001dd642a653b928aef343c8a",
"d4/d84/classyaze_1_1core_1_1Controller.html#a024bd35620ff4d5e2fd5dd899e23fc07",
"d4/da1/classyaze_1_1gui_1_1canvas_1_1CanvasUsageManager.html#a980bc86adf9b0444855dd06900ad0b6e",
"d4/dd7/classyaze_1_1emu_1_1test_1_1HeadlessEmulator.html#ab284219f09a0403d31fa2041a714e15d",
"d5/d10/structyaze_1_1gui_1_1CanvasUtils_1_1CanvasRenderContext.html#aaebb684762b4c984a8724850cf68233f",
"d5/d1f/namespaceyaze_1_1zelda3.html#a5459cf09bcd168153834261f8505f886",
"d5/d1f/namespaceyaze_1_1zelda3.html#aacf2dac35353426c864a8a4cefe412a8af80a4ad87fee7c9fdc19b7769495fdb5",
"d5/d3e/widget__auto__register_8cc_source.html",
"d5/d71/structyaze_1_1editor_1_1AgentEditor_1_1BotProfile.html#a612871ca7d5b0cb85b4606629e609712",
"d5/da0/structyaze_1_1emu_1_1DspChannel.html#a0e0cdc5b4acd293e791b0c60dea40065",
"d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1007",
"d5/de8/music__editor_8h.html",
"d6/d0d/classyaze_1_1editor_1_1AgentEditor.html#aa41bd0d2b7965ded8c5d6b7b799e5038",
"d6/d28/classyaze_1_1net_1_1RomVersionManager.html#a331000c976164a817fda9b62e778cbac",
"d6/d30/classyaze_1_1Rom.html#a55859062f001d9e7d49442ab85c7a413",
"d6/d47/structyaze_1_1gui_1_1WidgetMetrics.html#a031bf7fbf904ba6b86de4e29cfec5fa8",
"d6/d7a/test__suite_8h.html#af8e72af06684da4456850f7f3690e96aaf83deabecb881f963501ddab928fb58a",
"d6/db1/classyaze_1_1zelda3_1_1Sprite.html#a492be700d2470dd6cb6cd845d4713564",
"d6/dcb/classyaze_1_1gui_1_1canvas_1_1CanvasPerformanceIntegration.html#af00e9fae022f31cc9162cadc128d62fe",
"d6/dfb/classyaze_1_1net_1_1CollaborationService.html#a91aeaffd41c1b2759ebd651778ea0649",
"d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md445",
"d7/d61/classyaze_1_1zelda3_1_1DungeonEditorSystem.html#a254447f041e13a08e4b8b26b8e50efc9",
"d7/d61/classyaze_1_1zelda3_1_1DungeonEditorSystem.html#adacb55baa3841317acbb186e4af97b14",
"d7/d92/classyaze_1_1editor_1_1ToastManager.html#a76e066fc94ffecf7a1303c6ed454cd59",
"d7/dc5/classyaze_1_1util_1_1Flag.html#ace9b4f03672ed3ec8aac416df961d3a6",
"d7/de3/structyaze_1_1editor_1_1AgentChatWidget_1_1AutomationCallbacks.html#a0ab5095cb5cf1acf3acad4f35314305d",
"d7/df6/classyaze_1_1zelda3_1_1Room.html#ab2c8a0efb438bb714551c9b9c72eaadb",
"d8/d0c/structyaze_1_1core_1_1ProjectMetadata.html#a71e64f64d2e0ac0bbc04f89508238b7f",
"d8/d31/classyaze_1_1emu_1_1debug_1_1DisassemblyViewer.html#a19f175b6779f3bd401a7d437d4c67565",
"d8/d4d/structyaze_1_1core_1_1ResourceLabelManager.html#a85fded9a77639b41576fd3a5c6ccf7af",
"d8/d89/dungeon__rom__addresses_8h.html#a1d0bafb79d3bc3b354d37f9d13f567ba",
"d8/db7/structyaze_1_1emu_1_1Input.html#aea449ae46f450eb827c7dcf1181017b5",
"d8/dd6/classyaze_1_1zelda3_1_1music_1_1Tracker.html#a9e8c49eed084c7790daf07995f5d4c08",
"d8/de6/structyaze_1_1gui_1_1canvas_1_1ColorAnalysisOptions.html#a5c4624063c6cc894d6a3dcc12de65671",
"d9/d2b/classyaze_1_1editor_1_1AgentChatWidget.html#a5d2be5bd0d6d1483e16d10b391a0f8c9",
"d9/d54/canvas__automation__api__test_8cc.html#aa8a49b735b55aba68681d0249baffc03",
"d9/d7f/classyaze_1_1gui_1_1PaletteWidget.html#a7fd7af77e066779bed9652ce38d10229",
"d9/da7/namespaceyaze_1_1cli_1_1util_1_1colors.html#ad19257e30792e94b63157c78ed49995c",
"d9/dc0/room_8h.html#afb47ec71853bc72be86c1b0c481a3496a58a15dcd8225b3b354d3696f1f713a77",
"d9/dc5/classyaze_1_1zelda3_1_1Overworld.html#af729948f7b1336fec9512432e7b6b887",
"d9/dd1/structyaze_1_1emu_1_1COLDATA.html#a5e08aa69f281b18bd9be41475896d143",
"d9/df4/classyaze_1_1gui_1_1PaletteEditorWidget.html#a9d5e831074fd08d50209a986fae956a3",
"da/d2c/classyaze_1_1gui_1_1Canvas.html#a01bf7ad681e216aef246fd8f562a9a56",
"da/d2c/classyaze_1_1gui_1_1Canvas.html#aa0a32f21adbcf767c7adbf1c39feb401",
"da/d3e/classyaze_1_1test_1_1TestManager.html#a7db45a242780d055d1d12cbc23df3d62",
"da/d57/structyaze_1_1cli_1_1PolicyResult.html#a3ddbeba30768bcc9ac258900662e4db2",
"da/d7f/structyaze_1_1gui_1_1CanvasConfig.html#ae72f4069b6b57e5b5cde37d6d1d05359",
"da/dbc/classyaze_1_1emu_1_1AsmParser.html",
"da/de5/rom_8h_source.html",
"db/d07/dungeon__editor__system_8h.html#a96ac578ab867af965b3da48b2fd904a2",
"db/d33/namespaceyaze_1_1core.html#a1142e14777c50b720b0e9e102bff707ea1e3b2427a636684daa5c2b1d5a1dc4b5",
"db/d63/test__suite__writer_8h_source.html",
"db/d82/classyaze_1_1editor_1_1Tile16Editor.html#a9a7d0030a02d8620a86c2d2059b0b282",
"db/da1/structyaze_1_1emu_1_1VMADDL.html#ad690bbf35c50917973ee5be684d84035",
"db/dbe/classyaze_1_1emu_1_1debug_1_1ApuHandshakeTracker.html#a841d9f5808af79c20e140d8d8f1e2171ab8c43e05cbd2b65cd4647c6ac8da8474",
"db/de2/structyaze_1_1cli_1_1StopRecordingResult.html",
"dc/d0c/structyaze_1_1cli_1_1GeminiConfig.html#a8837c4ce46b219621005885eb94eb8ac",
"dc/d31/classyaze_1_1editor_1_1GraphicsEditor.html#a6df417c71b3d310543f5bf5ba808ac1f",
"dc/d46/namespaceyaze.html",
"dc/d64/structyaze_1_1cli_1_1ResourceAction.html",
"dc/daa/classyaze_1_1editor_1_1WelcomeScreen.html#ab1c985512c61b9e1d5b60bfc39d3d470",
"dc/dee/snes__color_8cc_source.html",
"dc/df4/classyaze_1_1emu_1_1Cpu.html#a8a6dd659543ec3cc58c186c9237ff522",
"dc/dfb/structyaze_1_1zelda3_1_1EntranceTypes_1_1EntranceInfo.html#abf29a678db3372e8afd90815a422908d",
"dd/d12/classyaze_1_1editor_1_1EditorManager.html#a74e62edb85a25578e5134ca56b043a87",
"dd/d26/structyaze_1_1cli_1_1overworld_1_1MapSummary.html#ae9ad32ccab85c461298a7c71fb272e59",
"dd/d54/graphics__optimizer_8h.html#a29ba92486c82108e08959f64864d2f61a1fd06b1ff50fc42e5c8a3098bdd81e4f",
"dd/d71/classyaze_1_1gui_1_1canvas_1_1CanvasInteractionHandler.html#ada7e6ac88e0b9914e7ed0ba2c06b7fcc",
"dd/dcc/classyaze_1_1editor_1_1ProposalDrawer.html#a5608c12dc598b8a336b7e49d02e197ab",
"dd/de3/classyaze_1_1test_1_1PerformanceTestSuite.html#a80f72bf6bf75b6e50e85462b0e022398",
"de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md903",
"de/d0f/classyaze_1_1emu_1_1MemoryImpl.html#afc2938889beeae99884ea9209fe4cbe5",
"de/d71/classyaze_1_1cli_1_1ResourceContextBuilder.html#ab22346d4d7c192a1ad8d086c7339f68a",
"de/d8f/structyaze_1_1zelda3_1_1ObjectSubtypeInfo.html",
"de/dae/zelda_8h.html#af25424b13374b13fd0cdb07f2f32a42a",
"de/dbf/icons_8h.html#a1ddd34793b551cc51285c642b56ce251",
"de/dbf/icons_8h.html#a3c1cc3e80f2941efe2274c8b76e81943",
"de/dbf/icons_8h.html#a57f657ebf4b0151726f8362d5513de2f",
"de/dbf/icons_8h.html#a7568eb650f9e51dfe4d44539e5cadd35",
"de/dbf/icons_8h.html#a95dce675794ec7a567ccfaea429f192c",
"de/dbf/icons_8h.html#ab29c3dc413ae9b3ec3e8cc7f66866f48",
"de/dbf/icons_8h.html#acf53ca603581bd06a132b6198f91ccbf",
"de/dbf/icons_8h.html#aea04f528fb4a57dfceff9d9a5d12ac04",
"de/dd3/structyaze_1_1cli_1_1agent_1_1ChatMessage_1_1ProposalSummary.html",
"de/de7/classyaze_1_1zelda3_1_1DungeonObjectEditor.html#a9822dd40408e58f89ed13844df3e450d",
"df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md697",
"df/d26/classyaze_1_1Transaction.html#ab3014197a44fc2397913b7ad9e61d7b5a3a543e453ac7cd8dba8460ebaf676c70",
"df/d80/classyaze_1_1zelda3_1_1test_1_1RoomIntegrationTest.html#a6fe61f22a852cbf6ccd4828f5a7cd014",
"df/dc1/md_docs_2apu-timing-analysis.html#autotoc_md52",
"df/df5/classyaze_1_1test_1_1MockRom.html#a090de4443a88d74c25cbb6708a7f0601",
"globals_defs_m.html"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';