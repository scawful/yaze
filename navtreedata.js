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
    [ "YAZE Dependency Architecture & Build Optimization", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html", [
      [ "Executive Summary", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md26", [
        [ "Key Findings", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md27", null ],
        [ "Quick Stats", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md28", null ]
      ] ],
      [ "1. Complete Dependency Graph", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md30", [
        [ "1.1 Foundation Layer", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md31", null ],
        [ "1.2 Graphics Tier (Refactored)", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md32", null ],
        [ "1.3 GUI Tier (Refactored)", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md33", null ],
        [ "1.4 Zelda3 Library (Current - Monolithic)", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md34", null ],
        [ "1.5 Zelda3 Library (Proposed - Tiered)", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md35", null ],
        [ "1.6 Core Libraries", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md36", null ],
        [ "1.7 Application Layer", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md37", null ],
        [ "1.8 Executables", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md38", null ]
      ] ],
      [ "2. Current Dependency Issues", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md40", [
        [ "2.1 Circular Dependencies", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md41", [
          [ "Issue 1: yaze_test_support ↔ yaze_editor", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md42", null ],
          [ "Issue 2: Potential yaze_core_lib ↔ yaze_gfx Cycle", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md43", null ]
        ] ],
        [ "2.2 Over-Linking Problems", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md44", [
          [ "Problem 1: yaze_test_support Links to Everything", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md45", null ],
          [ "Problem 2: yaze_agent Links to Everything", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md46", null ],
          [ "Problem 3: yaze_editor Links to 8+ Major Libraries", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md47", null ]
        ] ],
        [ "2.3 Misplaced Components", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md48", [
          [ "Issue 1: zelda3 Library Location", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md49", null ],
          [ "Issue 2: Test Infrastructure Mixed into App", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md50", null ]
        ] ]
      ] ],
      [ "3. Build Time Impact Analysis", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md52", [
        [ "3.1 Current Rebuild Cascades", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md53", [
          [ "Scenario 1: Change snes_tile.cc (gfx_types)", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md54", null ],
          [ "Scenario 2: Change overworld_map.cc (zelda3)", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md55", null ],
          [ "Scenario 3: Change test_manager.cc", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md56", null ]
        ] ],
        [ "3.2 Optimized Rebuild Cascades (After Refactoring)", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md57", [
          [ "Scenario 1: Change snes_tile.cc (gfx_types) - Optimized", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md58", null ],
          [ "Scenario 2: Change overworld_map.cc (zelda3) - Optimized", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md59", null ],
          [ "Scenario 3: Change test_manager.cc - Optimized", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md60", null ]
        ] ],
        [ "3.3 Build Time Savings Summary", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md61", null ]
      ] ],
      [ "4. Proposed Refactoring Initiatives", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md63", [
        [ "4.1 Priority 1: Execute Existing Proposals", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md64", [
          [ "A. Test Dashboard Separation (A2)", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md65", null ],
          [ "B. Zelda3 Library Refactoring (B6)", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md66", null ],
          [ "C. z3ed Command Abstraction (C4)", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md67", null ]
        ] ],
        [ "4.2 Priority 2: New Refactoring Proposals", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md68", [
          [ "D. Split yaze_core_lib to Prevent Cycles", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md69", null ],
          [ "E. Split yaze_agent for Minimal CLI", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md70", null ]
        ] ],
        [ "4.3 Priority 3: Future Optimizations", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md71", [
          [ "F. Editor Modularization", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md72", null ],
          [ "G. Precompiled Headers Optimization", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md73", null ],
          [ "H. Unity Builds for Third-Party Code", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md74", null ]
        ] ]
      ] ],
      [ "5. Conditional Compilation Matrix", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md76", [
        [ "Build Configurations", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md77", null ],
        [ "Feature Flags", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md78", null ],
        [ "Library Availability Matrix", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md79", null ],
        [ "Executable Build Matrix", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md80", null ]
      ] ],
      [ "6. Migration Roadmap", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md82", [
        [ "Phase 1: Foundation Fixes ( This PR)", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md83", null ],
        [ "Phase 2: Test Separation (Next Sprint)", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md84", null ],
        [ "Phase 3: Zelda3 Refactoring (Week 2-3)", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md85", null ],
        [ "Phase 4: Core Library Split (Week 4)", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md86", null ],
        [ "Phase 5: Agent Split (Week 5)", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md87", null ],
        [ "Phase 6: Benchmarking & Optimization (Week 6)", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md88", null ],
        [ "Phase 7: Documentation & Polish (Week 7)", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md89", null ]
      ] ],
      [ "7. Expected Build Time Improvements", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md91", [
        [ "Baseline Measurements (Current)", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md92", null ],
        [ "Projected Improvements (After All Refactoring)", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md93", null ],
        [ "CI/CD Improvements", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md94", null ],
        [ "Developer Experience Improvements", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md95", null ]
      ] ],
      [ "8. Detailed Library Specifications", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md97", [
        [ "8.1 Foundation Libraries", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md98", [
          [ "yaze_common", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md99", null ],
          [ "yaze_util", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md100", null ]
        ] ],
        [ "8.2 Graphics Libraries", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md101", [
          [ "yaze_gfx_types", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md102", null ],
          [ "yaze_gfx_backend", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md103", null ],
          [ "yaze_gfx_resource", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md104", null ],
          [ "yaze_gfx_core", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md105", null ],
          [ "yaze_gfx_render", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md106", null ],
          [ "yaze_gfx_util", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md107", null ],
          [ "yaze_gfx_debug", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md108", null ]
        ] ],
        [ "8.3 GUI Libraries", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md109", [
          [ "yaze_gui_core", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md110", null ],
          [ "yaze_gui_canvas", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md111", null ],
          [ "yaze_gui_widgets", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md112", null ],
          [ "yaze_gui_automation", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md113", null ],
          [ "yaze_gui_app", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md114", null ]
        ] ],
        [ "8.4 Game Logic Libraries", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md115", [
          [ "yaze_zelda3 (Current)", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md116", null ],
          [ "yaze_zelda3_core (Proposed)", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md117", null ],
          [ "yaze_zelda3_sprite (Proposed)", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md118", null ],
          [ "yaze_zelda3_dungeon (Proposed)", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md119", null ],
          [ "yaze_zelda3_overworld (Proposed)", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md120", null ],
          [ "yaze_zelda3_screen (Proposed)", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md121", null ],
          [ "yaze_zelda3_music (Proposed)", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md122", null ]
        ] ],
        [ "8.5 Core System Libraries", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md123", [
          [ "yaze_core_lib (Current)", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md124", null ],
          [ "yaze_core_foundation (Proposed)", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md125", null ],
          [ "yaze_core_services (Proposed)", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md126", null ],
          [ "yaze_emulator", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md127", null ],
          [ "yaze_net", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md128", null ]
        ] ],
        [ "8.6 Application Layer Libraries", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md129", [
          [ "yaze_editor", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md130", null ],
          [ "yaze_agent (Current)", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md131", null ],
          [ "yaze_agent_core (Proposed)", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md132", null ],
          [ "yaze_agent_services (Proposed)", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md133", null ],
          [ "yaze_test_support (Current)", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md134", null ],
          [ "yaze_test_framework (Proposed)", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md135", null ],
          [ "yaze_test_suites (Proposed)", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md136", null ],
          [ "yaze_test_dashboard (Proposed)", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md137", null ]
        ] ]
      ] ],
      [ "9. References & Related Documents", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md139", [
        [ "Primary Documents", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md140", null ],
        [ "Related Refactoring Documents", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md141", null ],
        [ "Build System Documentation", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md142", null ],
        [ "Architecture Documentation", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md143", null ],
        [ "External Resources", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md144", null ]
      ] ],
      [ "10. Conclusion", "d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md146", null ]
    ] ],
    [ "A2-test-dashboard-refactoring", "d4/dc0/md_docs_2A2-test-dashboard-refactoring.html", null ],
    [ "Build Instructions", "dc/d42/md_docs_2B1-build-instructions.html", [
      [ "1. Environment Verification", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md149", [
        [ "Windows (PowerShell)", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md150", null ],
        [ "macOS & Linux (Bash)", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md151", null ]
      ] ],
      [ "2. Quick Start: Building with Presets", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md152", [
        [ "macOS", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md153", null ],
        [ "Linux", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md154", null ],
        [ "Windows", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md155", null ],
        [ "AI-Enabled Build (All Platforms)", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md156", null ]
      ] ],
      [ "3. Dependencies", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md157", null ],
      [ "4. Platform Setup", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md158", [
        [ "macOS", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md159", null ],
        [ "Linux (Ubuntu/Debian)", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md160", null ],
        [ "Windows", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md161", null ]
      ] ],
      [ "5. Testing", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md162", [
        [ "Running Tests with Presets", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md163", null ],
        [ "Running Tests Manually", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md164", null ]
      ] ],
      [ "6. IDE Integration", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md165", [
        [ "VS Code (Recommended)", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md166", null ],
        [ "Visual Studio (Windows)", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md167", null ],
        [ "Xcode (macOS)", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md168", null ]
      ] ],
      [ "7. Windows Build Optimization", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md169", [
        [ "GitHub Actions / CI Builds", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md170", null ],
        [ "Local Development", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md171", [
          [ "Fast Build (Recommended)", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md172", null ],
          [ "Using vcpkg (Optional)", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md173", null ]
        ] ],
        [ "Compiler Support", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md174", null ]
      ] ],
      [ "8. Troubleshooting", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md175", [
        [ "Automatic Fixes", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md176", null ],
        [ "Cleaning Stale Builds", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md177", null ],
        [ "Common Issues", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md178", [
          [ "\"nlohmann/json.hpp: No such file or directory\"", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md179", null ],
          [ "\"Cannot open file 'yaze.exe': Permission denied\"", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md180", null ],
          [ "\"C++ standard 'cxx_std_23' not supported\"", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md181", null ],
          [ "Visual Studio Can't Find Presets", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md182", null ],
          [ "Git Line Ending (CRLF) Issues", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md183", null ],
          [ "File Path Length Limit on Windows", "dc/d42/md_docs_2B1-build-instructions.html#autotoc_md184", null ]
        ] ]
      ] ]
    ] ],
    [ "Platform Compatibility & CI/CD Fixes", "d1/d30/md_docs_2B2-platform-compatibility.html", [
      [ "Platform-Specific Notes", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md187", [
        [ "Windows", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md188", null ],
        [ "macOS", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md189", null ],
        [ "Linux", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md190", null ]
      ] ],
      [ "Cross-Platform Code Validation", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md192", null ],
      [ "Common Build Issues & Solutions", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md194", [
        [ "Windows: \"use of undefined type 'PromiseLike'\"", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md195", null ],
        [ "macOS: \"absl not found\"", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md196", null ],
        [ "Linux: CMake configuration fails", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md197", null ],
        [ "Windows: \"DWORD syntax error\"", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md198", null ]
      ] ],
      [ "CI/CD Validation Checklist", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md200", [
        [ "CI/CD Performance Roadmap", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md201", null ]
      ] ],
      [ "Testing Strategy", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md203", [
        [ "Automated (CI)", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md204", null ],
        [ "Manual Testing", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md205", null ]
      ] ],
      [ "Quick Reference", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md207", [
        [ "Build Command (All Platforms)", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md208", null ],
        [ "Enable Features", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md209", null ],
        [ "Windows Troubleshooting", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md210", null ]
      ] ],
      [ "Filesystem Abstraction", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md212", null ],
      [ "Native File Dialog Support", "d1/d30/md_docs_2B2-platform-compatibility.html#autotoc_md214", null ]
    ] ],
    [ "Build Presets Guide", "d7/d51/md_docs_2B3-build-presets.html", [
      [ "Design Principles", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md217", null ],
      [ "Quick Start", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md218", [
        [ "macOS Development", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md219", null ],
        [ "Windows Development", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md220", null ]
      ] ],
      [ "All Presets", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md221", [
        [ "macOS Presets", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md222", null ],
        [ "Windows Presets", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md223", null ],
        [ "Linux Presets", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md224", null ],
        [ "Special Purpose", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md225", null ]
      ] ],
      [ "Warning Control", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md226", [
        [ "To Enable Verbose Warnings:", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md227", null ]
      ] ],
      [ "Architecture Support", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md228", [
        [ "macOS", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md229", null ],
        [ "Windows", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md230", null ]
      ] ],
      [ "Build Directories", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md231", null ],
      [ "Feature Flags", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md232", null ],
      [ "Examples", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md233", [
        [ "Development with AI features and verbose warnings", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md234", null ],
        [ "Release build for distribution (macOS Universal)", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md235", null ],
        [ "Quick minimal build for testing", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md236", null ]
      ] ],
      [ "Updating compile_commands.json", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md237", null ],
      [ "Migration from Old Presets", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md238", null ],
      [ "Troubleshooting", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md239", [
        [ "Warnings are still showing", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md240", null ],
        [ "clangd can't find nlohmann/json", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md241", null ],
        [ "Build fails with missing dependencies", "d7/d51/md_docs_2B3-build-presets.html#autotoc_md242", null ]
      ] ]
    ] ],
    [ "Git Workflow and Branching Strategy", "da/dc1/md_docs_2B4-git-workflow.html", [
      [ "Warning: Pre-1.0 Workflow (Current)", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md244", null ],
      [ "Pre-1.0 Release Strategy: Best Effort Releases", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md246", [
        [ "Core Principles", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md247", null ],
        [ "How It Works in Practice", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md248", null ]
      ] ],
      [ "📚 Full Workflow Reference (Future/Formal)", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md250", null ],
      [ "Branch Structure", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md251", [
        [ "Main Branches", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md252", [
          [ "<tt>master</tt>", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md253", null ],
          [ "<tt>develop</tt>", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md254", null ]
        ] ],
        [ "Supporting Branches", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md255", [
          [ "Feature Branches", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md256", null ],
          [ "Bugfix Branches", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md257", null ],
          [ "Hotfix Branches", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md258", null ],
          [ "Release Branches", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md259", null ],
          [ "Experimental Branches", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md260", null ]
        ] ]
      ] ],
      [ "Commit Message Conventions", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md261", [
        [ "Format", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md262", null ],
        [ "Types", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md263", null ],
        [ "Scopes (optional)", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md264", null ],
        [ "Examples", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md265", null ]
      ] ],
      [ "Pull Request Guidelines", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md266", [
        [ "PR Title", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md267", null ],
        [ "PR Description Template", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md268", null ],
        [ "PR Review Process", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md269", null ]
      ] ],
      [ "Version Numbering", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md270", [
        [ "MAJOR (e.g., 1.0.0)", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md271", null ],
        [ "MINOR (e.g., 0.4.0)", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md272", null ],
        [ "PATCH (e.g., 0.3.2)", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md273", null ],
        [ "Pre-release Tags", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md274", null ]
      ] ],
      [ "Release Process", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md275", [
        [ "For Minor/Major Releases (0.x.0, x.0.0)", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md276", null ],
        [ "For Patch Releases (0.3.x)", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md277", null ]
      ] ],
      [ "Long-Running Feature Branches", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md278", null ],
      [ "Tagging Strategy", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md279", [
        [ "Release Tags", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md280", null ],
        [ "Internal Milestones", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md281", null ]
      ] ],
      [ "Best Practices", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md282", [
        [ "DO", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md283", null ],
        [ "DON'T ❌", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md284", null ]
      ] ],
      [ "Quick Reference", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md285", null ],
      [ "Emergency Procedures", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md286", [
        [ "If master is broken", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md287", null ],
        [ "If develop is broken", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md288", null ],
        [ "If release needs to be rolled back", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md289", null ]
      ] ],
      [ "Current Simplified Workflow (Pre-1.0)", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md291", [
        [ "Daily Development Pattern", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md292", null ],
        [ "When to Use Branches (Pre-1.0)", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md293", null ],
        [ "Current Branch Usage", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md294", null ],
        [ "Commit Message (Simplified)", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md295", null ],
        [ "Releases (Pre-1.0)", "da/dc1/md_docs_2B4-git-workflow.html#autotoc_md296", null ]
      ] ]
    ] ],
    [ "B5 - Architecture and Networking", "dd/de3/md_docs_2B5-architecture-and-networking.html", [
      [ "1. High-Level Architecture", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md299", null ],
      [ "2. Service Taxonomy", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md300", [
        [ "APP Services (gRPC Servers)", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md301", null ],
        [ "CLI Services (Business Logic)", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md302", null ]
      ] ],
      [ "3. gRPC Services", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md303", [
        [ "ImGuiTestHarness Service", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md304", null ],
        [ "RomService", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md305", null ],
        [ "CanvasAutomation Service", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md306", null ]
      ] ],
      [ "4. Real-Time Collaboration", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md307", [
        [ "Architecture", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md308", null ],
        [ "Core Components", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md309", null ],
        [ "WebSocket Protocol", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md310", null ]
      ] ],
      [ "5. Data Flow Example: AI Agent Edits a Tile", "dd/de3/md_docs_2B5-architecture-and-networking.html#autotoc_md311", null ]
    ] ],
    [ "B6-zelda3-library-refactoring", "d8/d91/md_docs_2B6-zelda3-library-refactoring.html", null ],
    [ "B7 - Architecture Refactoring Plan", "de/ddc/md_docs_2B7-architecture-refactoring-plan.html", [
      [ "1. Overview & Goals", "de/ddc/md_docs_2B7-architecture-refactoring-plan.html#autotoc_md313", null ],
      [ "2. Proposed Target Architecture", "de/ddc/md_docs_2B7-architecture-refactoring-plan.html#autotoc_md314", null ],
      [ "3. Detailed Refactoring Plan", "de/ddc/md_docs_2B7-architecture-refactoring-plan.html#autotoc_md315", [
        [ "Phase 1: Create <tt>yaze_core_lib</tt> (Project & Asar Logic)", "de/ddc/md_docs_2B7-architecture-refactoring-plan.html#autotoc_md316", null ],
        [ "Phase 2: Elevate <tt>yaze_gfx_lib</tt> (Graphics Engine)", "de/ddc/md_docs_2B7-architecture-refactoring-plan.html#autotoc_md317", null ],
        [ "Phase 3: Streamline the <tt>app</tt> Layer", "de/ddc/md_docs_2B7-architecture-refactoring-plan.html#autotoc_md318", null ]
      ] ],
      [ "4. Alignment with EditorManager Refactoring", "de/ddc/md_docs_2B7-architecture-refactoring-plan.html#autotoc_md319", null ],
      [ "5. Migration Checklist", "de/ddc/md_docs_2B7-architecture-refactoring-plan.html#autotoc_md320", null ],
      [ "6. Completed Changes (October 15, 2025)", "de/ddc/md_docs_2B7-architecture-refactoring-plan.html#autotoc_md321", [
        [ "Phase 1: Foundational Core Library ✅", "de/ddc/md_docs_2B7-architecture-refactoring-plan.html#autotoc_md322", null ],
        [ "Phase 3: Application Layer Streamlining ✅", "de/ddc/md_docs_2B7-architecture-refactoring-plan.html#autotoc_md323", null ],
        [ "Deferred (Phase 2)", "de/ddc/md_docs_2B7-architecture-refactoring-plan.html#autotoc_md324", null ]
      ] ],
      [ "6. Expected Benefits", "de/ddc/md_docs_2B7-architecture-refactoring-plan.html#autotoc_md325", null ]
    ] ],
    [ "z3ed Command-Line Interface", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html", [
      [ "1. Overview", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md327", [
        [ "Core Capabilities", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md328", null ]
      ] ],
      [ "2. Quick Start", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md329", [
        [ "Build", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md330", null ],
        [ "AI Setup", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md331", null ],
        [ "Example Commands", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md332", null ],
        [ "Hybrid CLI ↔ GUI Workflow", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md333", null ]
      ] ],
      [ "3. Architecture", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md334", [
        [ "System Components Diagram", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md335", null ],
        [ "Command Abstraction Layer (v0.2.1)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md336", null ]
      ] ],
      [ "4. Agentic & Generative Workflow (MCP)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md337", null ],
      [ "5. Command Reference", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md338", [
        [ "Agent Commands", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md339", null ],
        [ "Resource Commands", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md340", [
          [ "<tt>agent test</tt>: Live Harness Automation", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md341", null ]
        ] ]
      ] ],
      [ "6. Chat Modes", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md342", [
        [ "FTXUI Chat (<tt>agent chat</tt>)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md343", null ],
        [ "Simple Chat (<tt>agent simple-chat</tt>)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md344", null ],
        [ "GUI Chat Widget (Editor Integration)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md345", null ]
      ] ],
      [ "7. AI Provider Configuration", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md346", [
        [ "System Prompt Versions", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md347", null ]
      ] ],
      [ "8. Learn Command - Knowledge Management", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md348", [
        [ "Basic Usage", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md349", null ],
        [ "Project Context", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md350", null ],
        [ "Conversation Memory", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md351", null ],
        [ "Storage Location", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md352", null ]
      ] ],
      [ "9. TODO Management System", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md353", [
        [ "Core Capabilities", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md354", null ],
        [ "Storage Location", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md355", null ]
      ] ],
      [ "10. CLI Output & Help System", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md356", [
        [ "Verbose Logging", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md357", null ],
        [ "Hierarchical Help System", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md358", null ]
      ] ],
      [ "10. Collaborative Sessions & Multimodal Vision", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md359", [
        [ "Overview", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md360", null ],
        [ "Local Collaboration Mode", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md362", [
          [ "How to Use", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md363", null ],
          [ "Features", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md364", null ],
          [ "Cloud Folder Workaround", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md365", null ]
        ] ],
        [ "Network Collaboration Mode (yaze-server v2.0)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md367", [
          [ "Requirements", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md368", null ],
          [ "Server Setup", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md369", null ],
          [ "Client Connection", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md370", null ],
          [ "Core Features", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md371", null ],
          [ "Advanced Features (v2.0)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md372", null ],
          [ "Protocol Reference", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md373", null ],
          [ "Server Configuration", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md374", null ],
          [ "Database Schema (Server v2.0)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md375", null ],
          [ "Deployment", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md376", null ],
          [ "Testing", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md377", null ],
          [ "Security Considerations", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md378", null ]
        ] ],
        [ "Multimodal Vision (Gemini)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md380", [
          [ "Requirements", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md381", null ],
          [ "Capture Modes", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md382", null ],
          [ "How to Use", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md383", null ],
          [ "Example Prompts", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md384", null ]
        ] ],
        [ "Architecture", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md386", null ],
        [ "Troubleshooting", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md388", null ],
        [ "References", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md390", null ]
      ] ],
      [ "11. Roadmap & Implementation Status", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md391", [
        [ "Completed", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md392", null ],
        [ "📌 Current Progress Highlights (October 5, 2025)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md393", null ],
        [ "Active & Next Steps", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md394", null ],
        [ "Recently Completed (v0.2.2-alpha - October 12, 2025)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md395", [
          [ "Emulator Debugging Infrastructure (NEW) 🔍", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md396", null ],
          [ "Benefits for AI Agents", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md397", null ]
        ] ],
        [ "Previously Completed (v0.2.1-alpha - October 11, 2025)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md398", [
          [ "CLI Architecture Improvements", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md399", null ],
          [ "Code Quality & Maintainability", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md400", null ]
        ] ],
        [ "Previously Completed (v0.2.0-alpha - October 5, 2025)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md401", [
          [ "Core AI Features", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md402", null ],
          [ "Version Management & Protection", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md403", null ],
          [ "Networking & Collaboration (NEW)", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md404", null ],
          [ "UI/UX Enhancements", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md405", null ],
          [ "Build System & Infrastructure", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md406", null ]
        ] ]
      ] ],
      [ "12. Troubleshooting", "d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md407", null ]
    ] ],
    [ "Testing z3ed Without ROM Files", "d7/d43/md_docs_2C2-testing-without-roms.html", [
      [ "Overview", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md409", null ],
      [ "How Mock ROM Mode Works", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md410", null ],
      [ "Usage", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md411", [
        [ "Command Line Flag", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md412", null ],
        [ "Test Suite", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md413", null ]
      ] ],
      [ "What Works with Mock ROM", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md414", [
        [ "Fully Supported", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md415", null ],
        [ "Limited Support", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md416", null ],
        [ "Not Supported", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md417", null ]
      ] ],
      [ "Testing Strategy", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md418", [
        [ "For Agent Logic", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md419", null ],
        [ "For ROM Operations", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md420", null ]
      ] ],
      [ "CI/CD Integration", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md421", [
        [ "GitHub Actions Example", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md422", null ]
      ] ],
      [ "Embedded Labels Reference", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md423", null ],
      [ "Troubleshooting", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md424", [
        [ "\"No ROM loaded\" error", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md425", null ],
        [ "Mock ROM fails to initialize", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md426", null ],
        [ "Agent returns empty/wrong results", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md427", null ]
      ] ],
      [ "Development", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md428", [
        [ "Adding New Labels", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md429", null ],
        [ "Testing Mock ROM Directly", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md430", null ]
      ] ],
      [ "Best Practices", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md431", [
        [ "DO", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md432", null ],
        [ "DON'T ❌", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md433", null ]
      ] ],
      [ "Related Documentation", "d7/d43/md_docs_2C2-testing-without-roms.html#autotoc_md434", null ]
    ] ],
    [ "C3 - z3ed Agent Architecture Guide", "dd/d7e/md_docs_2C3-agent-architecture.html", [
      [ "Overview", "dd/d7e/md_docs_2C3-agent-architecture.html#autotoc_md437", null ],
      [ "Architecture Overview", "dd/d7e/md_docs_2C3-agent-architecture.html#autotoc_md438", null ],
      [ "Feature 1: Learned Knowledge Service", "dd/d7e/md_docs_2C3-agent-architecture.html#autotoc_md439", [
        [ "What It Does", "dd/d7e/md_docs_2C3-agent-architecture.html#autotoc_md440", null ],
        [ "Integration Status:  Complete", "dd/d7e/md_docs_2C3-agent-architecture.html#autotoc_md441", null ],
        [ "Usage Examples", "dd/d7e/md_docs_2C3-agent-architecture.html#autotoc_md442", null ],
        [ "AI Agent Integration", "dd/d7e/md_docs_2C3-agent-architecture.html#autotoc_md443", null ],
        [ "Data Persistence", "dd/d7e/md_docs_2C3-agent-architecture.html#autotoc_md444", null ],
        [ "Current Integration", "dd/d7e/md_docs_2C3-agent-architecture.html#autotoc_md445", null ]
      ] ],
      [ "Feature 2: TODO Management System", "dd/d7e/md_docs_2C3-agent-architecture.html#autotoc_md446", [
        [ "What It Does", "dd/d7e/md_docs_2C3-agent-architecture.html#autotoc_md447", null ],
        [ "Current Integration", "dd/d7e/md_docs_2C3-agent-architecture.html#autotoc_md448", null ],
        [ "Usage Examples", "dd/d7e/md_docs_2C3-agent-architecture.html#autotoc_md449", null ],
        [ "AI Agent Integration", "dd/d7e/md_docs_2C3-agent-architecture.html#autotoc_md450", null ],
        [ "Storage", "dd/d7e/md_docs_2C3-agent-architecture.html#autotoc_md451", null ]
      ] ],
      [ "Feature 3: Advanced Routing", "dd/d7e/md_docs_2C3-agent-architecture.html#autotoc_md452", [
        [ "What It Does", "dd/d7e/md_docs_2C3-agent-architecture.html#autotoc_md453", null ],
        [ "Status", "dd/d7e/md_docs_2C3-agent-architecture.html#autotoc_md454", null ],
        [ "How to Integrate", "dd/d7e/md_docs_2C3-agent-architecture.html#autotoc_md455", null ]
      ] ],
      [ "Feature 4: Agent Pretraining", "dd/d7e/md_docs_2C3-agent-architecture.html#autotoc_md456", [
        [ "What It Does", "dd/d7e/md_docs_2C3-agent-architecture.html#autotoc_md457", null ],
        [ "Status", "dd/d7e/md_docs_2C3-agent-architecture.html#autotoc_md458", null ],
        [ "How to Integrate", "dd/d7e/md_docs_2C3-agent-architecture.html#autotoc_md459", null ],
        [ "Knowledge Modules", "dd/d7e/md_docs_2C3-agent-architecture.html#autotoc_md460", null ]
      ] ],
      [ "Feature 5: Agent Handoff", "dd/d7e/md_docs_2C3-agent-architecture.html#autotoc_md461", null ],
      [ "Current Integration Snapshot", "dd/d7e/md_docs_2C3-agent-architecture.html#autotoc_md462", null ],
      [ "References", "dd/d7e/md_docs_2C3-agent-architecture.html#autotoc_md463", null ]
    ] ],
    [ "z3ed CLI Refactoring Summary", "d9/dbf/md_docs_2C4-z3ed-refactoring.html", [
      [ "Overview", "d9/dbf/md_docs_2C4-z3ed-refactoring.html#autotoc_md466", null ],
      [ "Key Achievements", "d9/dbf/md_docs_2C4-z3ed-refactoring.html#autotoc_md467", [
        [ "1. Command Abstraction Layer Implementation", "d9/dbf/md_docs_2C4-z3ed-refactoring.html#autotoc_md468", null ],
        [ "2. Enhanced TUI System", "d9/dbf/md_docs_2C4-z3ed-refactoring.html#autotoc_md469", null ],
        [ "3. Comprehensive Testing Suite", "d9/dbf/md_docs_2C4-z3ed-refactoring.html#autotoc_md470", null ],
        [ "4. Build System Updates", "d9/dbf/md_docs_2C4-z3ed-refactoring.html#autotoc_md471", null ]
      ] ],
      [ "Technical Implementation Details", "d9/dbf/md_docs_2C4-z3ed-refactoring.html#autotoc_md472", [
        [ "Command Abstraction Architecture", "d9/dbf/md_docs_2C4-z3ed-refactoring.html#autotoc_md473", null ],
        [ "Refactored Commands", "d9/dbf/md_docs_2C4-z3ed-refactoring.html#autotoc_md474", null ],
        [ "TUI Architecture", "d9/dbf/md_docs_2C4-z3ed-refactoring.html#autotoc_md475", null ]
      ] ],
      [ "Code Quality Improvements", "d9/dbf/md_docs_2C4-z3ed-refactoring.html#autotoc_md476", [
        [ "Before Refactoring", "d9/dbf/md_docs_2C4-z3ed-refactoring.html#autotoc_md477", null ],
        [ "After Refactoring", "d9/dbf/md_docs_2C4-z3ed-refactoring.html#autotoc_md478", null ]
      ] ],
      [ "Testing Strategy", "d9/dbf/md_docs_2C4-z3ed-refactoring.html#autotoc_md479", [
        [ "Unit Tests", "d9/dbf/md_docs_2C4-z3ed-refactoring.html#autotoc_md480", null ],
        [ "Integration Tests", "d9/dbf/md_docs_2C4-z3ed-refactoring.html#autotoc_md481", null ],
        [ "Test Coverage", "d9/dbf/md_docs_2C4-z3ed-refactoring.html#autotoc_md482", null ]
      ] ],
      [ "Migration Guide", "d9/dbf/md_docs_2C4-z3ed-refactoring.html#autotoc_md483", [
        [ "For Developers", "d9/dbf/md_docs_2C4-z3ed-refactoring.html#autotoc_md484", null ],
        [ "For AI Integration", "d9/dbf/md_docs_2C4-z3ed-refactoring.html#autotoc_md485", null ]
      ] ],
      [ "Performance Impact", "d9/dbf/md_docs_2C4-z3ed-refactoring.html#autotoc_md486", [
        [ "Build Time", "d9/dbf/md_docs_2C4-z3ed-refactoring.html#autotoc_md487", null ],
        [ "Runtime Performance", "d9/dbf/md_docs_2C4-z3ed-refactoring.html#autotoc_md488", null ],
        [ "Development Velocity", "d9/dbf/md_docs_2C4-z3ed-refactoring.html#autotoc_md489", null ]
      ] ],
      [ "Future Roadmap", "d9/dbf/md_docs_2C4-z3ed-refactoring.html#autotoc_md490", [
        [ "Phase 2 (Next Release)", "d9/dbf/md_docs_2C4-z3ed-refactoring.html#autotoc_md491", null ],
        [ "Phase 3 (Future)", "d9/dbf/md_docs_2C4-z3ed-refactoring.html#autotoc_md492", null ]
      ] ],
      [ "Conclusion", "d9/dbf/md_docs_2C4-z3ed-refactoring.html#autotoc_md493", null ]
    ] ],
    [ "z3ed Command Abstraction Layer Guide", "d7/d9e/md_docs_2C5-z3ed-command-abstraction.html", [
      [ "Overview", "d7/d9e/md_docs_2C5-z3ed-command-abstraction.html#autotoc_md496", null ],
      [ "Problem Statement", "d7/d9e/md_docs_2C5-z3ed-command-abstraction.html#autotoc_md497", [
        [ "Before Abstraction", "d7/d9e/md_docs_2C5-z3ed-command-abstraction.html#autotoc_md498", null ],
        [ "Code Duplication Example", "d7/d9e/md_docs_2C5-z3ed-command-abstraction.html#autotoc_md499", null ]
      ] ],
      [ "Solution Architecture", "d7/d9e/md_docs_2C5-z3ed-command-abstraction.html#autotoc_md500", [
        [ "Three-Layer Abstraction", "d7/d9e/md_docs_2C5-z3ed-command-abstraction.html#autotoc_md501", null ],
        [ "File Structure", "d7/d9e/md_docs_2C5-z3ed-command-abstraction.html#autotoc_md502", null ]
      ] ],
      [ "Core Components", "d7/d9e/md_docs_2C5-z3ed-command-abstraction.html#autotoc_md503", [
        [ "1. CommandContext", "d7/d9e/md_docs_2C5-z3ed-command-abstraction.html#autotoc_md504", null ],
        [ "2. ArgumentParser", "d7/d9e/md_docs_2C5-z3ed-command-abstraction.html#autotoc_md505", null ],
        [ "3. OutputFormatter", "d7/d9e/md_docs_2C5-z3ed-command-abstraction.html#autotoc_md506", null ],
        [ "4. CommandHandler (Optional Base Class)", "d7/d9e/md_docs_2C5-z3ed-command-abstraction.html#autotoc_md507", null ]
      ] ],
      [ "Migration Guide", "d7/d9e/md_docs_2C5-z3ed-command-abstraction.html#autotoc_md508", [
        [ "Step-by-Step Refactoring", "d7/d9e/md_docs_2C5-z3ed-command-abstraction.html#autotoc_md509", [
          [ "Before (80 lines):", "d7/d9e/md_docs_2C5-z3ed-command-abstraction.html#autotoc_md510", null ],
          [ "After (30 lines):", "d7/d9e/md_docs_2C5-z3ed-command-abstraction.html#autotoc_md511", null ]
        ] ],
        [ "Commands to Refactor", "d7/d9e/md_docs_2C5-z3ed-command-abstraction.html#autotoc_md512", null ],
        [ "Estimated Impact", "d7/d9e/md_docs_2C5-z3ed-command-abstraction.html#autotoc_md513", null ]
      ] ],
      [ "Testing Strategy", "d7/d9e/md_docs_2C5-z3ed-command-abstraction.html#autotoc_md514", [
        [ "Unit Testing", "d7/d9e/md_docs_2C5-z3ed-command-abstraction.html#autotoc_md515", null ],
        [ "Integration Testing", "d7/d9e/md_docs_2C5-z3ed-command-abstraction.html#autotoc_md516", null ]
      ] ],
      [ "Benefits Summary", "d7/d9e/md_docs_2C5-z3ed-command-abstraction.html#autotoc_md517", [
        [ "For Developers", "d7/d9e/md_docs_2C5-z3ed-command-abstraction.html#autotoc_md518", null ],
        [ "For Maintainability", "d7/d9e/md_docs_2C5-z3ed-command-abstraction.html#autotoc_md519", null ],
        [ "For AI Integration", "d7/d9e/md_docs_2C5-z3ed-command-abstraction.html#autotoc_md520", null ]
      ] ],
      [ "Next Steps", "d7/d9e/md_docs_2C5-z3ed-command-abstraction.html#autotoc_md521", [
        [ "Immediate (Current PR)", "d7/d9e/md_docs_2C5-z3ed-command-abstraction.html#autotoc_md522", null ],
        [ "Phase 2 (Next PR)", "d7/d9e/md_docs_2C5-z3ed-command-abstraction.html#autotoc_md523", null ],
        [ "Phase 3 (Future)", "d7/d9e/md_docs_2C5-z3ed-command-abstraction.html#autotoc_md524", null ]
      ] ],
      [ "Migration Checklist", "d7/d9e/md_docs_2C5-z3ed-command-abstraction.html#autotoc_md525", null ],
      [ "References", "d7/d9e/md_docs_2C5-z3ed-command-abstraction.html#autotoc_md526", null ],
      [ "Questions & Answers", "d7/d9e/md_docs_2C5-z3ed-command-abstraction.html#autotoc_md527", null ]
    ] ],
    [ "Asm Style Guide", "d7/d9a/md_docs_2E1-asm-style-guide.html", [
      [ "Table of Contents", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md530", null ],
      [ "File Structure", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md531", null ],
      [ "Labels and Symbols", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md532", null ],
      [ "Comments", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md533", null ],
      [ "Instructions", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md534", null ],
      [ "Macros", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md535", null ],
      [ "Loops and Branching", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md536", null ],
      [ "Data Structures", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md537", null ],
      [ "Code Organization", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md538", null ],
      [ "Custom Code", "d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md539", null ]
    ] ],
    [ "APU Timing Fix - Technical Analysis", "dc/d88/md_docs_2E10-apu-timing-analysis.html", [
      [ "Implementation Status", "dc/d88/md_docs_2E10-apu-timing-analysis.html#autotoc_md542", null ],
      [ "Problem Summary", "dc/d88/md_docs_2E10-apu-timing-analysis.html#autotoc_md543", null ],
      [ "Current Implementation Analysis", "dc/d88/md_docs_2E10-apu-timing-analysis.html#autotoc_md545", [
        [ "1. <strong>Cycle Counting System</strong> (<tt>spc700.cc</tt>)", "dc/d88/md_docs_2E10-apu-timing-analysis.html#autotoc_md546", null ],
        [ "2. <strong>The <tt>bstep</tt> Mechanism</strong> (<tt>spc700.cc</tt>)", "dc/d88/md_docs_2E10-apu-timing-analysis.html#autotoc_md547", null ],
        [ "3. <strong>APU Main Loop</strong> (<tt>apu.cc:73-143</tt>)", "dc/d88/md_docs_2E10-apu-timing-analysis.html#autotoc_md548", null ],
        [ "4. <strong>Floating-Point Precision</strong> (<tt>apu.cc:17</tt>)", "dc/d88/md_docs_2E10-apu-timing-analysis.html#autotoc_md549", null ]
      ] ],
      [ "Root Cause: Handshake Timing Failure", "dc/d88/md_docs_2E10-apu-timing-analysis.html#autotoc_md551", [
        [ "The Handshake Protocol", "dc/d88/md_docs_2E10-apu-timing-analysis.html#autotoc_md552", null ],
        [ "Where It Gets Stuck", "dc/d88/md_docs_2E10-apu-timing-analysis.html#autotoc_md553", null ]
      ] ],
      [ "LakeSnes Comparison Analysis", "dc/d88/md_docs_2E10-apu-timing-analysis.html#autotoc_md555", [
        [ "What LakeSnes Does Right", "dc/d88/md_docs_2E10-apu-timing-analysis.html#autotoc_md556", null ],
        [ "Where LakeSnes Falls Short (And How We Can Do Better)", "dc/d88/md_docs_2E10-apu-timing-analysis.html#autotoc_md557", null ],
        [ "What We're Adopting from LakeSnes", "dc/d88/md_docs_2E10-apu-timing-analysis.html#autotoc_md558", null ],
        [ "What We're Improving Over LakeSnes", "dc/d88/md_docs_2E10-apu-timing-analysis.html#autotoc_md559", null ]
      ] ],
      [ "Solution Design", "dc/d88/md_docs_2E10-apu-timing-analysis.html#autotoc_md561", [
        [ "Phase 1: Atomic Instruction Execution", "dc/d88/md_docs_2E10-apu-timing-analysis.html#autotoc_md562", null ],
        [ "Phase 2: Precise Cycle Calculation", "dc/d88/md_docs_2E10-apu-timing-analysis.html#autotoc_md563", null ],
        [ "Phase 3: Refactor <tt>Apu::RunCycles</tt> to Cycle Budget Model", "dc/d88/md_docs_2E10-apu-timing-analysis.html#autotoc_md564", null ],
        [ "Phase 4: Fixed-Point Cycle Ratio", "dc/d88/md_docs_2E10-apu-timing-analysis.html#autotoc_md565", null ]
      ] ],
      [ "Implementation Plan", "dc/d88/md_docs_2E10-apu-timing-analysis.html#autotoc_md567", [
        [ "Step 1: Add <tt>Spc700::Step()</tt> Function", "dc/d88/md_docs_2E10-apu-timing-analysis.html#autotoc_md568", null ],
        [ "Step 2: Implement Precise Cycle Calculation", "dc/d88/md_docs_2E10-apu-timing-analysis.html#autotoc_md569", null ],
        [ "Step 3: Eliminate <tt>bstep</tt> Mechanism", "dc/d88/md_docs_2E10-apu-timing-analysis.html#autotoc_md570", null ],
        [ "Step 4: Refactor <tt>Apu::RunCycles</tt>", "dc/d88/md_docs_2E10-apu-timing-analysis.html#autotoc_md571", null ],
        [ "Step 5: Convert to Fixed-Point Ratio", "dc/d88/md_docs_2E10-apu-timing-analysis.html#autotoc_md572", null ],
        [ "Step 6: Testing", "dc/d88/md_docs_2E10-apu-timing-analysis.html#autotoc_md573", null ]
      ] ],
      [ "Files to Modify", "dc/d88/md_docs_2E10-apu-timing-analysis.html#autotoc_md575", null ],
      [ "Success Criteria", "dc/d88/md_docs_2E10-apu-timing-analysis.html#autotoc_md577", null ],
      [ "Implementation Completed", "dc/d88/md_docs_2E10-apu-timing-analysis.html#autotoc_md579", null ]
    ] ],
    [ "E2 - Development Guide", "d5/d18/md_docs_2E2-development-guide.html", [
      [ "Editor Status (October 2025)", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md582", [
        [ "Screen Editor Notes", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md583", null ]
      ] ],
      [ "1. Core Architectural Patterns", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md584", [
        [ "Pattern 1: Modular Systems", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md585", null ],
        [ "Pattern 2: Callback-Based Communication", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md586", null ],
        [ "Pattern 3: Centralized Progressive Loading via <tt>gfx::Arena</tt>", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md587", null ]
      ] ],
      [ "2. UI & Theming System", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md588", [
        [ "2.1. The Theme System (<tt>AgentUITheme</tt>)", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md589", null ],
        [ "2.2. Reusable UI Helper Functions", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md590", null ],
        [ "2.3. Toolbar Implementation (<tt>CompactToolbar</tt>)", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md591", null ]
      ] ],
      [ "3. Key System Implementations & Gotchas", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md592", [
        [ "3.1. Graphics Refresh Logic", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md593", null ],
        [ "3.2. Multi-Area Map Configuration", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md594", null ],
        [ "3.3. Version-Specific Feature Gating", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md595", null ],
        [ "3.4. Entity Visibility for Visual Testing", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md596", null ]
      ] ],
      [ "4. Clang Tooling Configuration", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md597", null ],
      [ "5. Debugging and Testing", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md598", [
        [ "5.1. Quick Debugging with Startup Flags", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md599", null ],
        [ "5.2. Testing Strategies", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md600", null ]
      ] ],
      [ "5. Command-Line Flag Standardization", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md601", [
        [ "Rationale", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md602", null ],
        [ "Migration Plan", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md603", null ],
        [ "3.6. Graphics Sheet Management", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md605", null ],
        [ "Naming Conventions", "d5/d18/md_docs_2E2-development-guide.html#autotoc_md606", null ]
      ] ]
    ] ],
    [ "API Reference", "d8/d73/md_docs_2E3-api-reference.html", [
      [ "C API (<tt>incl/yaze.h</tt>)", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md608", [
        [ "Core Library Functions", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md609", null ],
        [ "ROM Operations", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md610", null ]
      ] ],
      [ "C++ API", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md611", [
        [ "AsarWrapper (<tt>src/core/asar_wrapper.h</tt>)", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md612", [
          [ "CLI Examples (<tt>z3ed</tt>)", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md613", null ],
          [ "C++ API Example", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md614", null ],
          [ "Class Definition", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md615", null ]
        ] ]
      ] ],
      [ "Data Structures", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md616", [
        [ "<tt>snes_color</tt>", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md617", null ],
        [ "<tt>zelda3_message</tt>", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md618", null ]
      ] ],
      [ "Error Handling", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md619", [
        [ "C API Error Pattern", "d8/d73/md_docs_2E3-api-reference.html#autotoc_md620", null ]
      ] ]
    ] ],
    [ "E4 - Emulator Development Guide", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html", [
      [ "Table of Contents", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md623", null ],
      [ "1. Current Status", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md625", [
        [ "🎉 Major Breakthrough: Game is Running!", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md626", null ],
        [ "Confirmed Working", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md627", null ],
        [ "Tool Known Issues (Non-Critical)", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md628", null ]
      ] ],
      [ "2. How to Use the Emulator", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md630", [
        [ "Method 1: Main Yaze Application (GUI)", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md631", null ],
        [ "Method 2: Standalone Emulator (<tt>yaze_emu</tt>)", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md632", null ],
        [ "Method 3: Dungeon Object Emulator Preview", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md633", null ]
      ] ],
      [ "3. Architecture", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md635", [
        [ "Memory System", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md636", [
          [ "SNES Memory Map", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md637", null ]
        ] ],
        [ "CPU-APU-SPC700 Interaction", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md638", null ],
        [ "Component Architecture", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md639", null ]
      ] ],
      [ "4. The Debugging Journey: Critical Fixes", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md641", [
        [ "SPC700 & APU Fixes", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md642", null ],
        [ "The Critical Pattern for Multi-Step Instructions", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md643", null ]
      ] ],
      [ "5. Display & Performance Improvements", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md645", [
        [ "PPU Color Display Fix", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md646", null ],
        [ "Frame Timing & Speed Control", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md647", null ],
        [ "Performance Optimizations", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md648", [
          [ "Frame Skipping", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md649", null ],
          [ "Audio Buffer Management", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md650", null ],
          [ "Performance Gains", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md651", null ]
        ] ],
        [ "ROM Loading Improvements", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md652", null ]
      ] ],
      [ "6. Advanced Features", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md654", [
        [ "Professional Disassembly Viewer", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md655", [
          [ "Architecture", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md656", null ],
          [ "Visual Features", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md657", null ],
          [ "Interactive Elements", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md658", null ],
          [ "UI Features", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md659", null ],
          [ "Performance", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md660", null ]
        ] ],
        [ "Breakpoint System", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md661", null ],
        [ "UI/UX Enhancements", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md662", null ]
      ] ],
      [ "7. Emulator Preview Tool", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md664", [
        [ "Purpose", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md665", null ],
        [ "Critical Fixes Applied", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md666", [
          [ "1. Memory Access Fix (SIGSEGV Crash)", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md667", null ],
          [ "2. RTL vs RTS Fix (Timeout)", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md668", null ],
          [ "3. Palette Validation", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md669", null ],
          [ "4. PPU Configuration", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md670", null ]
        ] ],
        [ "How to Use", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md671", null ],
        [ "What You'll Learn", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md672", null ],
        [ "Reverse Engineering Workflow", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md673", null ],
        [ "UI Enhancements", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md674", null ]
      ] ],
      [ "8. Logging System", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md676", [
        [ "How to Enable", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md677", null ]
      ] ],
      [ "9. Testing", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md679", [
        [ "Unit Tests", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md680", null ],
        [ "Standalone Emulator", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md681", null ],
        [ "Running Tests", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md682", null ],
        [ "Testing Checklist", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md683", null ]
      ] ],
      [ "10. Technical Reference", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md685", [
        [ "PPU Registers", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md686", null ],
        [ "CPU Instructions", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md687", null ],
        [ "Color Format", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md688", null ],
        [ "Performance Metrics", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md689", null ]
      ] ],
      [ "11. Troubleshooting", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md691", [
        [ "Emulator Preview Issues", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md692", null ],
        [ "Color Display Issues", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md693", null ],
        [ "Performance Issues", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md694", null ],
        [ "Build Issues", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md695", null ]
      ] ],
      [ "11.5 Audio System Architecture (October 2025)", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md697", [
        [ "Overview", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md698", null ],
        [ "Audio Backend Abstraction", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md699", null ],
        [ "APU Handshake Debugging System", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md700", null ],
        [ "IPL ROM Handshake Protocol", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md701", null ],
        [ "Music Editor Integration", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md702", null ],
        [ "Audio Testing & Diagnostics", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md703", null ],
        [ "Future Enhancements", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md704", null ]
      ] ],
      [ "12. Next Steps & Roadmap", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md706", [
        [ "Immediate Priorities (Critical Path to Full Functionality)", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md707", null ],
        [ "Enhancement Priorities (After Core is Stable)", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md708", null ],
        [ "📝 Technical Debt", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md709", null ],
        [ "Long-Term Enhancements", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md710", null ]
      ] ],
      [ "13. Build Instructions", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md712", [
        [ "Quick Build", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md713", null ],
        [ "Platform-Specific", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md714", null ],
        [ "Build Optimizations", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md715", null ]
      ] ],
      [ "File Reference", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md717", [
        [ "Core Emulation", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md718", null ],
        [ "Debugging", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md719", null ],
        [ "UI", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md720", null ],
        [ "Core", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md721", null ],
        [ "Testing", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md722", null ]
      ] ],
      [ "Status Summary", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md724", [
        [ "Production Ready", "d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md725", null ]
      ] ]
    ] ],
    [ "E5 - Debugging and Testing Guide", "de/dc5/md_docs_2E5-debugging-guide.html", [
      [ "1. Standardized Logging for Print Debugging", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md728", [
        [ "Log Levels and Usage", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md729", null ],
        [ "Log Categories", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md730", null ],
        [ "Enabling and Configuring Logs via CLI", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md731", null ],
        [ "2. Command-Line Workflows for Testing", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md734", [
          [ "Launching the GUI for Specific Tasks", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md735", null ]
        ] ]
      ] ],
      [ "Open the Dungeon Editor with the Room Matrix and two specific room cards", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md736", null ],
      [ "Available editors: Assembly, Dungeon, Graphics, Music, Overworld, Palette,", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md737", null ],
      [ "Screen, Sprite, Message, Hex, Agent, Settings", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md738", null ],
      [ "Dungeon editor cards: Rooms List, Room Matrix, Entrances List, Room Graphics,", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md739", null ],
      [ "Object Editor, Palette Editor, Room N (where N is room ID)", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md740", null ],
      [ "Fast dungeon room testing", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md741", null ],
      [ "Compare multiple rooms side-by-side", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md742", null ],
      [ "Full dungeon workspace with all tools", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md743", null ],
      [ "Jump straight to overworld editing", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md744", null ],
      [ "Run only fast, dependency-free unit tests", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md746", null ],
      [ "Run tests that require a ROM file", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md747", [
        [ "3. GUI Automation for AI Agents", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md751", [
          [ "Inspecting ROMs with <tt>z3ed</tt>", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md749", null ],
          [ "Architecture Overview", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md752", null ],
          [ "Step-by-Step Workflow for AI", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md753", [
            [ "Step 1: Launch <tt>yaze</tt> with the Test Harness", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md754", null ],
            [ "Step 2: Discover UI Elements", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md755", null ],
            [ "Step 3: Record or Write a Test Script", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md756", null ]
          ] ]
        ] ]
      ] ],
      [ "Start yaze with the room already open", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md757", null ],
      [ "Then your test script just needs to validate the state", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md758", [
        [ "4. Advanced Debugging Tools", "de/dc5/md_docs_2E5-debugging-guide.html#autotoc_md761", null ]
      ] ]
    ] ],
    [ "Emulator Core Improvements Roadmap", "d3/d49/md_docs_2E6-emulator-improvements.html", [
      [ "Overview", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md763", null ],
      [ "Critical Priority: APU Timing Fix", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md765", [
        [ "Problem Statement", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md766", null ],
        [ "Root Cause: CPU-APU Handshake Timing", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md767", [
          [ "The Handshake Protocol", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md768", null ],
          [ "Point of Failure", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md769", null ]
        ] ],
        [ "Technical Analysis", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md770", [
          [ "Issue 1: Incomplete Opcode Timing", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md771", null ],
          [ "Issue 2: Fragile Multi-Step Execution Model", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md772", null ],
          [ "Issue 3: Floating-Point Precision", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md773", null ]
        ] ],
        [ "Proposed Solution: Cycle-Accurate Refactoring", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md774", [
          [ "Step 1: Implement Cycle-Accurate Instruction Execution", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md775", null ],
          [ "Step 2: Centralize the APU Execution Loop", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md776", null ],
          [ "Step 3: Use Integer-Based Cycle Ratios", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md777", null ]
        ] ]
      ] ],
      [ "High Priority: Core Architecture & Timing Model", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md779", [
        [ "CPU Cycle Counting", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md780", null ],
        [ "Main Synchronization Loop", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md781", null ]
      ] ],
      [ "Medium Priority: PPU Performance", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md783", [
        [ "Rendering Approach Optimization", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md784", null ]
      ] ],
      [ "Low Priority: Code Quality & Refinements", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md786", [
        [ "APU Code Modernization", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md787", null ],
        [ "Audio Subsystem & Buffering", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md788", null ],
        [ "Debugger & Tooling Optimizations", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md789", [
          [ "DisassemblyViewer Data Structure", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md790", null ],
          [ "BreakpointManager Lookups", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md791", null ]
        ] ]
      ] ],
      [ "Completed Improvements", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md793", [
        [ "Audio System Fixes (v0.4.0)", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md794", [
          [ "Problem Statement", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md795", null ],
          [ "Root Causes Fixed", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md796", null ],
          [ "Solutions Implemented", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md797", null ]
        ] ]
      ] ],
      [ "Implementation Priority", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md799", null ],
      [ "Success Metrics", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md801", [
        [ "APU Timing Fix Success", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md802", null ],
        [ "Overall Emulation Accuracy", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md803", null ],
        [ "Performance Targets", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md804", null ]
      ] ],
      [ "Related Documentation", "d3/d49/md_docs_2E6-emulator-improvements.html#autotoc_md806", null ]
    ] ],
    [ "YAZE Startup Debugging Flags", "db/da6/md_docs_2E7-debugging-startup-flags.html", [
      [ "Basic Usage", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md809", null ],
      [ "Available Flags", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md810", [
        [ "<tt>--rom_file</tt>", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md811", null ],
        [ "<tt>--debug</tt>", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md812", null ],
        [ "<tt>--editor</tt>", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md813", null ],
        [ "<tt>--cards</tt>", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md814", null ]
      ] ],
      [ "Common Debugging Scenarios", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md815", [
        [ "1. Quick Dungeon Room Testing", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md816", null ],
        [ "2. Multiple Room Comparison", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md817", null ],
        [ "3. Full Dungeon Editor Workspace", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md818", null ],
        [ "4. Debug Mode with Logging", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md819", null ],
        [ "5. Quick Overworld Editing", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md820", null ]
      ] ],
      [ "gRPC Test Harness (Developer Feature)", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md821", null ],
      [ "Combining Flags", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md822", null ],
      [ "Notes", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md823", null ],
      [ "Troubleshooting", "db/da6/md_docs_2E7-debugging-startup-flags.html#autotoc_md824", null ]
    ] ],
    [ "YAZE Emulator Enhancement Roadmap", "df/d0c/md_docs_2E8-emulator-debugging-vision.html", [
      [ "Executive Summary", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md827", [
        [ "Core Objectives", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md828", null ]
      ] ],
      [ "Current State Analysis", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md830", [
        [ "What Works", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md831", null ],
        [ "What's Broken ❌", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md832", null ],
        [ "What's Missing Pending:", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md833", null ]
      ] ],
      [ "Tool Phase 1: Audio System Fix (Priority: CRITICAL)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md835", [
        [ "Problem Analysis", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md836", null ],
        [ "Investigation Steps", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md837", null ],
        [ "Likely Fixes", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md838", null ],
        [ "Quick Win Actions", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md839", null ]
      ] ],
      [ "🐛 Phase 2: Advanced Debugger (Mesen2 Feature Parity)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md841", [
        [ "Feature Comparison: YAZE vs Mesen2", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md842", null ],
        [ "2.1 Breakpoint System", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md843", null ],
        [ "2.2 Memory Watchpoints", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md845", null ],
        [ "2.3 Live Disassembly Viewer", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md847", null ],
        [ "2.4 Enhanced Memory Viewer", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md849", null ]
      ] ],
      [ "Phase 3: Performance Optimizations", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md851", [
        [ "3.1 Cycle-Accurate Timing", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md852", null ],
        [ "3.2 Dynamic Recompilation (Dynarec)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md854", null ],
        [ "3.3 Frame Pacing Improvements", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md856", null ]
      ] ],
      [ "Game Phase 4: SPC700 Audio CPU Debugger", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md858", [
        [ "4.1 APU Inspector Window", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md859", null ],
        [ "4.2 Audio Sample Export", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md861", null ]
      ] ],
      [ "AI Phase 5: z3ed AI Agent Integration", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md863", [
        [ "5.1 Emulator State Access", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md864", null ],
        [ "5.2 Automated Test Generation", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md866", null ],
        [ "5.3 Memory Map Learning", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md868", null ]
      ] ],
      [ "📊 Phase 6: Performance Profiling", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md870", [
        [ "6.1 Cycle Counter & Profiler", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md871", null ],
        [ "6.2 Frame Time Analysis", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md873", null ]
      ] ],
      [ "Phase 7: Event System & Timeline", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md875", [
        [ "7.1 Event Logger", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md876", null ]
      ] ],
      [ "🧠 Phase 8: AI-Powered Debugging", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md878", [
        [ "8.1 Intelligent Crash Analysis", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md879", null ],
        [ "8.2 Automated Bug Reproduction", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md881", null ]
      ] ],
      [ "Implementation Roadmap", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md883", [
        [ "Sprint 1: Audio Fix (Week 1)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md884", null ],
        [ "Sprint 2: Basic Debugger (Weeks 2-3)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md885", null ],
        [ "Sprint 3: SPC700 Debugger (Week 4)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md886", null ],
        [ "Sprint 4: AI Integration (Weeks 5-6)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md887", null ],
        [ "Sprint 5: Performance (Weeks 7-8)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md888", null ]
      ] ],
      [ "🔬 Technical Deep Dives", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md890", [
        [ "Audio System Architecture (SDL2)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md891", null ],
        [ "Memory Regions Reference", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md893", null ]
      ] ],
      [ "Game Phase 9: Advanced Features (Mesen2 Parity)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md895", [
        [ "9.1 Rewind Feature", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md896", null ],
        [ "9.2 TAS (Tool-Assisted Speedrun) Input Recording", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md898", null ],
        [ "9.3 Comparison Mode", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md900", null ]
      ] ],
      [ "Optimization Summary", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md902", [
        [ "Quick Wins (< 1 week)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md903", null ],
        [ "Medium Term (1-2 months)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md904", null ],
        [ "Long Term (3-6 months)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md905", null ]
      ] ],
      [ "AI z3ed Agent Emulator Tools", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md907", [
        [ "New Tool Categories", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md908", null ],
        [ "Example AI Conversations", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md909", null ]
      ] ],
      [ "📁 File Structure for New Features", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md911", null ],
      [ "🎨 UI Mockups", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md913", [
        [ "Debugger Layout (ImGui)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md914", null ],
        [ "APU Debugger Layout", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md915", null ]
      ] ],
      [ "Performance Targets", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md917", [
        [ "Current Performance", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md918", null ],
        [ "Target Performance (Post-Optimization)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md919", null ],
        [ "Optimization Strategy Priority", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md920", null ]
      ] ],
      [ "🧪 Testing Integration", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md922", [
        [ "Automated Emulator Tests (z3ed)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md923", null ]
      ] ],
      [ "🔌 z3ed Agent + Emulator Integration", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md925", [
        [ "New Agent Tools", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md926", null ]
      ] ],
      [ "🎓 Learning from Mesen2", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md928", [
        [ "What Makes Mesen2 Great", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md929", null ],
        [ "Our Unique Advantages", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md930", null ]
      ] ],
      [ "📊 Resource Requirements", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md932", [
        [ "Development Time Estimates", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md933", null ],
        [ "Memory Requirements", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md934", null ],
        [ "CPU Requirements", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md935", null ]
      ] ],
      [ "Recommended Implementation Order", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md937", [
        [ "Month 1: Foundation", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md938", null ],
        [ "Month 2: Audio & Events", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md939", null ],
        [ "Month 3: AI Integration", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md940", null ],
        [ "Month 4: Performance", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md941", null ],
        [ "Month 5: Polish", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md942", null ]
      ] ],
      [ "🔮 Future Vision: AI-Powered ROM Hacking", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md944", [
        [ "The Ultimate Workflow", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md945", null ]
      ] ],
      [ "🐛 Appendix A: Audio Debugging Checklist", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md947", [
        [ "Check 1: Device Status", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md948", null ],
        [ "Check 2: Queue Size", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md949", null ],
        [ "Check 3: Sample Validation", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md950", null ],
        [ "Check 4: Buffer Allocation", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md951", null ],
        [ "Check 5: SPC700 Execution", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md952", null ],
        [ "Quick Fixes to Try", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md953", null ]
      ] ],
      [ "📝 Appendix B: Mesen2 Feature Reference", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md955", [
        [ "Debugger Windows (Inspiration)", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md956", null ],
        [ "Event Types Tracked", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md957", null ],
        [ "Trace Logger Format", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md958", null ]
      ] ],
      [ "Success Criteria", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md960", [
        [ "Phase 1 Complete When:", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md961", null ],
        [ "Phase 2 Complete When:", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md962", null ],
        [ "Phase 3 Complete When:", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md963", null ],
        [ "Phase 4 Complete When:", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md964", null ],
        [ "Phase 5 Complete When:", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md965", null ]
      ] ],
      [ "🎓 Learning Resources", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md967", [
        [ "SNES Emulation", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md968", null ],
        [ "Audio Debugging", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md969", null ],
        [ "Performance Optimization", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md970", null ]
      ] ],
      [ "Credits & Acknowledgments", "df/d0c/md_docs_2E8-emulator-debugging-vision.html#autotoc_md972", null ]
    ] ],
    [ "E9 - AI Agent Debugging Guide", "d3/d04/md_docs_2E9-ai-agent-debugging-guide.html", [
      [ "Overview", "d3/d04/md_docs_2E9-ai-agent-debugging-guide.html#autotoc_md975", null ],
      [ "Implementation Summary", "d3/d04/md_docs_2E9-ai-agent-debugging-guide.html#autotoc_md976", [
        [ "Features Implemented", "d3/d04/md_docs_2E9-ai-agent-debugging-guide.html#autotoc_md977", null ]
      ] ],
      [ "Architecture", "d3/d04/md_docs_2E9-ai-agent-debugging-guide.html#autotoc_md978", null ],
      [ "Available Tools", "d3/d04/md_docs_2E9-ai-agent-debugging-guide.html#autotoc_md979", [
        [ "1. Emulator Lifecycle", "d3/d04/md_docs_2E9-ai-agent-debugging-guide.html#autotoc_md980", null ],
        [ "2. Breakpoints", "d3/d04/md_docs_2E9-ai-agent-debugging-guide.html#autotoc_md981", null ],
        [ "3. Memory Inspection", "d3/d04/md_docs_2E9-ai-agent-debugging-guide.html#autotoc_md982", null ],
        [ "4. CPU State", "d3/d04/md_docs_2E9-ai-agent-debugging-guide.html#autotoc_md983", null ],
        [ "5. Execution Control", "d3/d04/md_docs_2E9-ai-agent-debugging-guide.html#autotoc_md984", null ]
      ] ],
      [ "Real-World Example: Debugging ALTTP Input Issues", "d3/d04/md_docs_2E9-ai-agent-debugging-guide.html#autotoc_md985", [
        [ "Problem Statement", "d3/d04/md_docs_2E9-ai-agent-debugging-guide.html#autotoc_md986", null ],
        [ "AI Agent Debugging Session", "d3/d04/md_docs_2E9-ai-agent-debugging-guide.html#autotoc_md987", null ],
        [ "Findings", "d3/d04/md_docs_2E9-ai-agent-debugging-guide.html#autotoc_md988", null ]
      ] ],
      [ "Advanced Use Cases", "d3/d04/md_docs_2E9-ai-agent-debugging-guide.html#autotoc_md989", [
        [ "Watchpoints for Input Debugging", "d3/d04/md_docs_2E9-ai-agent-debugging-guide.html#autotoc_md990", null ],
        [ "Symbol-Based Debugging (with Oracle of Secrets disassembly)", "d3/d04/md_docs_2E9-ai-agent-debugging-guide.html#autotoc_md991", null ],
        [ "Automated Test Scripts", "d3/d04/md_docs_2E9-ai-agent-debugging-guide.html#autotoc_md992", null ]
      ] ],
      [ "Benefits for AI-Driven Debugging", "d3/d04/md_docs_2E9-ai-agent-debugging-guide.html#autotoc_md993", [
        [ "Before (Manual Print Debugging)", "d3/d04/md_docs_2E9-ai-agent-debugging-guide.html#autotoc_md994", null ],
        [ "After (AI Agent with gRPC Service)", "d3/d04/md_docs_2E9-ai-agent-debugging-guide.html#autotoc_md995", null ]
      ] ],
      [ "Integration with Agent Chat Widget", "d3/d04/md_docs_2E9-ai-agent-debugging-guide.html#autotoc_md996", null ],
      [ "Function Schema for AI Tool Calling", "d3/d04/md_docs_2E9-ai-agent-debugging-guide.html#autotoc_md997", [
        [ "JSON Schema for Gemini/Ollama", "d3/d04/md_docs_2E9-ai-agent-debugging-guide.html#autotoc_md998", null ]
      ] ],
      [ "Practical Debugging Workflow", "d3/d04/md_docs_2E9-ai-agent-debugging-guide.html#autotoc_md999", [
        [ "Scenario: Input Button Not Registering", "d3/d04/md_docs_2E9-ai-agent-debugging-guide.html#autotoc_md1000", null ],
        [ "Solution Discovery", "d3/d04/md_docs_2E9-ai-agent-debugging-guide.html#autotoc_md1001", null ]
      ] ],
      [ "Comparison: Print Debugging vs AI-Driven Debugging", "d3/d04/md_docs_2E9-ai-agent-debugging-guide.html#autotoc_md1002", null ],
      [ "Performance Impact", "d3/d04/md_docs_2E9-ai-agent-debugging-guide.html#autotoc_md1003", [
        [ "Memory Overhead", "d3/d04/md_docs_2E9-ai-agent-debugging-guide.html#autotoc_md1004", null ],
        [ "CPU Overhead", "d3/d04/md_docs_2E9-ai-agent-debugging-guide.html#autotoc_md1005", null ],
        [ "Network Latency", "d3/d04/md_docs_2E9-ai-agent-debugging-guide.html#autotoc_md1006", null ]
      ] ],
      [ "Future Enhancements", "d3/d04/md_docs_2E9-ai-agent-debugging-guide.html#autotoc_md1007", [
        [ "Phase 2 (Next 2-4 weeks)", "d3/d04/md_docs_2E9-ai-agent-debugging-guide.html#autotoc_md1008", null ],
        [ "Phase 3 (1-2 months)", "d3/d04/md_docs_2E9-ai-agent-debugging-guide.html#autotoc_md1009", null ]
      ] ],
      [ "AI Agent System Prompt Extension", "d3/d04/md_docs_2E9-ai-agent-debugging-guide.html#autotoc_md1010", null ],
      [ "References", "d3/d04/md_docs_2E9-ai-agent-debugging-guide.html#autotoc_md1011", null ]
    ] ],
    [ "F2: Dungeon Editor v2 - Complete Guide", "d5/d83/md_docs_2F1-dungeon-editor-guide.html", [
      [ "Overview", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md1015", [
        [ "Key Features", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md1016", null ],
        [ "Architecture Improvements", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md1018", null ],
        [ "UI Improvements", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md1019", null ]
      ] ],
      [ "Architecture", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md1021", [
        [ "Component Overview", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md1022", null ],
        [ "Room Rendering Pipeline", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md1023", null ],
        [ "Room Structure (Bottom to Top)", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md1024", null ]
      ] ],
      [ "Next Development Steps", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md1026", [
        [ "High Priority (Must Do)", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md1027", [
          [ "1. Door Rendering at Room Edges", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md1028", null ],
          [ "2. Object Name Labels from String Array", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md1030", null ],
          [ "4. Fix Plus Button to Select Any Room", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md1032", null ]
        ] ],
        [ "Medium Priority (Should Do)", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md1034", [
          [ "6. Fix InputHexByte +/- Button Events", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md1035", null ]
        ] ],
        [ "Lower Priority (Nice to Have)", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md1036", [
          [ "9. Move Backend Logic to DungeonEditorSystem", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md1037", null ]
        ] ]
      ] ],
      [ "Quick Start", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md1039", [
        [ "Build & Run", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md1040", null ]
      ] ],
      [ "Testing & Verification", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md1042", [
        [ "Debug Commands", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md1043", null ]
      ] ],
      [ "Related Documentation", "d5/d83/md_docs_2F1-dungeon-editor-guide.html#autotoc_md1044", null ]
    ] ],
    [ "Tile16 Editor Palette System", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html", [
      [ "Executive Summary", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md1047", null ],
      [ "Problem Analysis", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md1048", [
        [ "Critical Issues Identified", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md1049", null ]
      ] ],
      [ "Solution Architecture", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md1050", [
        [ "Core Design Principles", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md1051", null ],
        [ "256-Color Overworld Palette Structure", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md1052", null ],
        [ "Sheet-to-Palette Mapping", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md1053", null ],
        [ "Palette Button Mapping", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md1054", null ]
      ] ],
      [ "Implementation Details", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md1055", [
        [ "1. Fixed SetPaletteWithTransparent Method", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md1056", null ],
        [ "2. Corrected Tile16 Editor Palette System", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md1057", null ],
        [ "3. Palette Coordination Flow", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md1058", null ]
      ] ],
      [ "UI/UX Refactoring", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md1059", [
        [ "New Three-Column Layout", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md1060", null ],
        [ "Canvas Context Menu Fixes", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md1061", null ],
        [ "Dynamic Zoom Controls", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md1062", null ]
      ] ],
      [ "Testing Protocol", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md1063", [
        [ "Crash Prevention Testing", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md1064", null ],
        [ "Color Alignment Testing", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md1065", null ],
        [ "UI/UX Testing", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md1066", null ]
      ] ],
      [ "Error Handling", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md1067", [
        [ "Bounds Checking", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md1068", null ],
        [ "Fallback Mechanisms", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md1069", null ]
      ] ],
      [ "Debug Information Display", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md1070", null ],
      [ "Known Issues and Ongoing Work", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md1071", [
        [ "Completed Items", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md1072", null ],
        [ "Active Issues Warning:", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md1073", null ],
        [ "Current Status Summary", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md1074", null ],
        [ "Future Enhancements", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md1075", null ]
      ] ],
      [ "Maintenance Notes", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md1076", null ],
      [ "Next Steps", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md1078", [
        [ "Immediate Priorities", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md1079", null ],
        [ "Investigation Areas", "d2/dde/md_docs_2F2-tile16-editor-palette-system.html#autotoc_md1080", null ]
      ] ]
    ] ],
    [ "Overworld Loading Guide", "d9/d85/md_docs_2F3-overworld-loading.html", [
      [ "Table of Contents", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1083", null ],
      [ "Overview", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1084", null ],
      [ "ROM Types and Versions", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1085", [
        [ "Version Detection", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1086", null ],
        [ "Feature Support by Version", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1087", null ]
      ] ],
      [ "Overworld Map Structure", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1088", [
        [ "Core Properties", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1089", null ]
      ] ],
      [ "Overlays and Special Area Maps", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1090", [
        [ "Understanding Overlays", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1091", null ],
        [ "Special Area Maps (0x80-0x9F)", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1092", null ],
        [ "Overlay ID Mappings", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1093", null ],
        [ "Drawing Order", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1094", null ],
        [ "Vanilla Overlay Loading", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1095", null ],
        [ "Special Area Graphics Loading", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1096", null ]
      ] ],
      [ "Loading Process", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1097", [
        [ "1. Version Detection", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1098", null ],
        [ "2. Map Initialization", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1099", null ],
        [ "3. Property Loading", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1100", [
          [ "Vanilla ROMs (asm_version == 0xFF)", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1101", null ],
          [ "ZSCustomOverworld v2/v3", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1102", null ]
        ] ],
        [ "4. Custom Data Loading", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1103", null ]
      ] ],
      [ "ZScream Implementation", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1104", [
        [ "OverworldMap Constructor", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1105", null ],
        [ "Key Methods", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1106", null ]
      ] ],
      [ "Yaze Implementation", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1107", [
        [ "OverworldMap Constructor", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1108", null ],
        [ "Key Methods", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1109", null ],
        [ "Mode 7 Tileset Conversion", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1110", null ],
        [ "Interleaved Tilemap Layout", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1111", null ],
        [ "Palette Addresses", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1112", null ],
        [ "Custom Map Import/Export", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1113", null ],
        [ "Current Status", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1114", null ]
      ] ],
      [ "Key Differences", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1115", [
        [ "1. Language and Architecture", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1116", null ],
        [ "2. Data Structures", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1117", null ],
        [ "3. Error Handling", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1118", null ],
        [ "4. Graphics Processing", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1119", null ]
      ] ],
      [ "Common Issues and Solutions", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1120", [
        [ "1. Version Detection Issues", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1121", null ],
        [ "2. Palette Loading Errors", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1122", null ],
        [ "3. Graphics Not Loading", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1123", null ],
        [ "4. Overlay Issues", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1124", null ],
        [ "5. Large Map Problems", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1125", null ],
        [ "6. Special Area Graphics Issues", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1126", null ]
      ] ],
      [ "Best Practices", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1127", [
        [ "1. Version-Specific Code", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1128", null ],
        [ "2. Error Handling", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1129", null ],
        [ "3. Memory Management", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1130", null ],
        [ "4. Thread Safety", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1131", null ]
      ] ],
      [ "Conclusion", "d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1132", null ]
    ] ],
    [ "Overworld Agent Guide - AI-Powered Overworld Editing", "de/d00/md_docs_2F4-overworld-agent-guide.html", [
      [ "Overview", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1135", null ],
      [ "Quick Start", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1137", [
        [ "Prerequisites", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1138", null ],
        [ "First Agent Interaction", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1139", null ]
      ] ],
      [ "Available Tools", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1141", [
        [ "Read-Only Tools (Safe for AI)", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1142", [
          [ "overworld-get-tile", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1143", null ],
          [ "overworld-get-visible-region", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1145", null ],
          [ "overworld-analyze-region", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1147", null ]
        ] ],
        [ "Write Tools (Sandboxed - Creates Proposals)", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1149", [
          [ "overworld-set-tile", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1150", null ],
          [ "overworld-set-tiles-batch", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1152", null ]
        ] ]
      ] ],
      [ "Multimodal Vision Workflow", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1154", [
        [ "Step 1: Capture Canvas Screenshot", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1155", null ],
        [ "Step 2: AI Analyzes Screenshot", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1156", null ],
        [ "Step 3: Generate Edit Plan", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1157", null ],
        [ "Step 4: Execute Plan (Sandbox)", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1158", null ],
        [ "Step 5: Human Review", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1159", null ]
      ] ],
      [ "Example Workflows", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1161", [
        [ "Workflow 1: Create Forest Area", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1162", null ],
        [ "Workflow 2: Fix Tile Placement Errors", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1164", null ],
        [ "Workflow 3: Generate Path", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1166", null ]
      ] ],
      [ "Common Tile IDs Reference", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1168", [
        [ "Grass & Ground", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1169", null ],
        [ "Trees & Plants", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1170", null ],
        [ "Water", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1171", null ],
        [ "Paths & Roads", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1172", null ],
        [ "Structures", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1173", null ]
      ] ],
      [ "Best Practices for AI Agents", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1175", [
        [ "1. Always Analyze Before Editing", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1176", null ],
        [ "2. Use Batch Operations", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1177", null ],
        [ "3. Provide Clear Reasoning", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1178", null ],
        [ "4. Respect Tile Boundaries", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1179", null ],
        [ "5. Check Visibility", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1180", null ],
        [ "6. Create Reversible Edits", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1181", null ]
      ] ],
      [ "Error Handling", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1183", [
        [ "\"Tile ID out of range\"", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1184", null ],
        [ "\"Coordinates out of bounds\"", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1185", null ],
        [ "\"Proposal rejected\"", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1186", null ],
        [ "\"ROM file locked\"", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1187", null ]
      ] ],
      [ "Testing AI-Generated Edits", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1189", [
        [ "Manual Testing", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1190", null ],
        [ "Automated Testing", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1191", null ]
      ] ],
      [ "Advanced Techniques", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1193", [
        [ "Technique 1: Pattern Recognition", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1194", null ],
        [ "Technique 2: Style Transfer", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1195", null ],
        [ "Technique 3: Procedural Generation", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1196", null ]
      ] ],
      [ "Integration with GUI Automation", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1198", [
        [ "Record Human Edits", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1199", null ],
        [ "Replay for AI Training", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1200", null ],
        [ "Validate AI Edits", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1201", null ]
      ] ],
      [ "Collaboration Features", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1203", [
        [ "Network Collaboration", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1204", null ],
        [ "Proposal Voting", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1205", null ]
      ] ],
      [ "Troubleshooting", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1207", [
        [ "Agent Not Responding", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1208", null ],
        [ "Tools Not Available", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1209", null ],
        [ "gRPC Connection Failed", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1210", null ]
      ] ],
      [ "See Also", "de/d00/md_docs_2F4-overworld-agent-guide.html#autotoc_md1212", null ]
    ] ],
    [ "Canvas System Overview", "d1/dc6/md_docs_2G1-canvas-guide.html", [
      [ "Canvas Architecture", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md1214", null ],
      [ "Core API Patterns", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md1215", null ],
      [ "Context Menu Sections", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md1216", null ],
      [ "Interaction Modes & Capabilities", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md1217", null ],
      [ "Debug & Diagnostics", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md1218", null ],
      [ "Automation API", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md1219", null ],
      [ "Integration Steps for Editors", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md1220", null ],
      [ "Migration Checklist", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md1221", null ],
      [ "Testing Notes", "d1/dc6/md_docs_2G1-canvas-guide.html#autotoc_md1222", null ]
    ] ],
    [ "SDL2 to SDL3 Migration and Rendering Abstraction Plan", "d6/df2/md_docs_2G2-renderer-migration-plan.html", [
      [ "1. Introduction", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md1224", null ],
      [ "2. Current State Analysis", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md1225", null ],
      [ "3. Proposed Architecture: The <tt>Renderer</tt> Abstraction", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md1226", [
        [ "3.1. The <tt>IRenderer</tt> Interface", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md1227", null ],
        [ "3.2. The <tt>SDL2Renderer</tt> Implementation", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md1228", null ]
      ] ],
      [ "4. Migration Plan", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md1229", [
        [ "Phase 1: Implement the Abstraction Layer", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md1230", null ],
        [ "Phase 2: Migrate to SDL3", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md1231", null ],
        [ "Phase 3: Support for Multiple Rendering Backends", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md1232", null ]
      ] ],
      [ "5. Conclusion", "d6/df2/md_docs_2G2-renderer-migration-plan.html#autotoc_md1233", null ]
    ] ],
    [ "SNES Palette System Overview", "da/dfd/md_docs_2G3-palete-system-overview.html", [
      [ "Understanding SNES Color and Palette Organization", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md1235", [
        [ "Core Concepts", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md1236", [
          [ "1. SNES Color Format (15-bit BGR555)", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md1237", null ],
          [ "2. Palette Groups in Zelda 3", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md1238", null ],
          [ "3. Color Representations in Code", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md1239", null ]
        ] ],
        [ "Dungeon Palette System", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md1240", [
          [ "Structure", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md1241", null ],
          [ "Usage", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md1242", null ],
          [ "Color Distribution (90 colors)", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md1243", null ]
        ] ],
        [ "Overworld Palette System", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md1244", [
          [ "Structure", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md1245", null ],
          [ "3BPP Graphics and Left/Right Palettes", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md1246", null ]
        ] ],
        [ "Common Issues and Solutions", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md1247", [
          [ "Issue 1: Empty Palette", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md1248", null ],
          [ "Issue 2: Bitmap Corruption", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md1249", null ]
        ] ],
        [ "Transparency and Conversion Best Practices", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md1250", [
          [ "Issue 3: ROM Not Loaded in Preview", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md1251", null ]
        ] ],
        [ "Palette Editor Integration", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md1252", [
          [ "Key Functions for UI", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md1253", null ],
          [ "Palette Widget Requirements", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md1254", null ],
          [ "Palette UI Helpers", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md1255", null ]
        ] ],
        [ "Graphics Manager Integration", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md1256", [
          [ "Sheet Palette Assignment", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md1257", null ]
        ] ],
        [ "Texture Synchronization and Regression Notes", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md1258", null ],
        [ "Best Practices", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md1259", null ],
        [ "User Workflow Tips", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md1260", null ],
        [ "ROM Addresses (for reference)", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md1261", null ]
      ] ],
      [ "Graphics Sheet Palette Application", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md1262", [
        [ "Default Palette Assignment", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md1263", null ],
        [ "Palette Update Workflow", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md1264", null ],
        [ "Common Pitfalls", "da/dfd/md_docs_2G3-palete-system-overview.html#autotoc_md1265", null ]
      ] ]
    ] ],
    [ "Graphics Renderer Migration - Complete Documentation", "d5/dc8/md_docs_2G3-renderer-migration-complete.html", [
      [ "Executive Summary", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1268", null ],
      [ "Architecture Overview", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1270", [
        [ "Before: Singleton Pattern", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1271", null ],
        [ "After: Dependency Injection + Deferred Queue", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1272", null ]
      ] ],
      [ "📦 Component Details", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1274", [
        [ "1. IRenderer Interface (<tt>src/app/gfx/backend/irenderer.h</tt>)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1275", null ],
        [ "2. SDL2Renderer (<tt>src/app/gfx/backend/sdl2_renderer.{h,cc}</tt>)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1277", null ],
        [ "3. Arena Deferred Texture Queue (<tt>src/app/gfx/arena.{h,cc}</tt>)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1279", null ],
        [ "4. Bitmap Palette Refactoring (<tt>src/app/gfx/bitmap.{h,cc}</tt>)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1281", null ],
        [ "5. Canvas Optional Renderer (<tt>src/app/gui/canvas.{h,cc}</tt>)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1283", null ],
        [ "6. Tilemap Texture Queue Integration (<tt>src/app/gfx/tilemap.cc</tt>)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1285", null ]
      ] ],
      [ "🔄 Dependency Injection Flow", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1287", [
        [ "Controller → EditorManager → Editors", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1288", null ]
      ] ],
      [ "⚡ Performance Optimizations", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1290", [
        [ "1. Batched Texture Processing", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1291", null ],
        [ "2. Frame Rate Limiting", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1292", null ],
        [ "3. Auto-Pause on Focus Loss", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1293", null ],
        [ "4. Surface/Texture Pooling", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1294", null ]
      ] ],
      [ "Migration Map: File Changes", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1296", [
        [ "Core Architecture Files (New)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1297", null ],
        [ "Core Modified Files (Major)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1298", null ],
        [ "Editor Files (Renderer Injection)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1299", null ],
        [ "Emulator Files (Special Handling)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1300", null ],
        [ "GUI/Widget Files", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1301", null ],
        [ "Test Files (Updated for DI)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1302", null ]
      ] ],
      [ "Tool Critical Fixes Applied", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1304", [
        [ "1. Bitmap::SetPalette() Crash", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1305", null ],
        [ "2. SDL2Renderer::UpdateTexture() SIGSEGV", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1307", null ],
        [ "3. Emulator Audio System Corruption", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1309", null ],
        [ "4. Emulator Cleanup During Shutdown", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1311", null ],
        [ "5. Controller/CreateWindow Initialization Order", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1313", null ]
      ] ],
      [ "🎨 Canvas Refactoring", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1315", [
        [ "The Challenge", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1316", null ],
        [ "The Solution: Backwards-Compatible Dual API", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1317", null ]
      ] ],
      [ "🧪 Testing Strategy", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1319", [
        [ "Test Files Updated", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1320", null ]
      ] ],
      [ "Road to SDL3", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1322", [
        [ "Why This Migration Matters", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1323", null ],
        [ "Our Abstraction Layer Handles This", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1324", null ]
      ] ],
      [ "📊 Performance Benchmarks", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1326", [
        [ "Texture Loading Performance", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1327", null ],
        [ "Graphics Editor Performance", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1328", null ],
        [ "Emulator Performance", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1329", null ]
      ] ],
      [ "🐛 Bugs Fixed During Migration", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1331", [
        [ "Critical Crashes", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1332", null ],
        [ "Build Errors", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1333", null ]
      ] ],
      [ "Key Design Patterns Used", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1335", [
        [ "1. Dependency Injection", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1336", null ],
        [ "2. Command Pattern (Deferred Queue)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1337", null ],
        [ "3. RAII (Resource Management)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1338", null ],
        [ "4. Adapter Pattern (Backend Abstraction)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1339", null ],
        [ "5. Singleton with DI (Arena)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1340", null ]
      ] ],
      [ "🔮 Future Enhancements", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1342", [
        [ "Short Term (SDL2)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1343", null ],
        [ "Medium Term (SDL3 Prep)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1344", null ],
        [ "Long Term (SDL3 Migration)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1345", null ]
      ] ],
      [ "📝 Lessons Learned", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1347", [
        [ "What Went Well", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1348", null ],
        [ "Challenges Overcome", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1349", null ],
        [ "Best Practices Established", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1350", null ]
      ] ],
      [ "🎓 Technical Deep Dive: Texture Queue System", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1352", [
        [ "Why Deferred Rendering?", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1353", null ],
        [ "Queue Processing Algorithm", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1354", null ]
      ] ],
      [ "🏆 Success Metrics", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1356", [
        [ "Build Health", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1357", null ],
        [ "Runtime Stability", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1358", null ],
        [ "Performance", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1359", null ],
        [ "Code Quality", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1360", null ]
      ] ],
      [ "📚 References", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1362", [
        [ "Related Documents", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1363", null ],
        [ "Key Commits", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1364", null ],
        [ "External Resources", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1365", null ]
      ] ],
      [ "Acknowledgments", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1367", null ],
      [ "🎉 Conclusion", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1369", null ],
      [ "Known Issues & Next Steps", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1371", [
        [ "macOS-Specific Issues (Not Renderer-Related)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1372", null ],
        [ "Stability Improvements for Next Session", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1373", [
          [ "High Priority", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1374", null ],
          [ "Medium Priority", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1375", null ],
          [ "Low Priority (SDL3 Migration)", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1376", null ]
        ] ],
        [ "Testing Recommendations", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1377", null ]
      ] ],
      [ "🎵 Final Notes", "d5/dc8/md_docs_2G3-renderer-migration-complete.html#autotoc_md1379", null ]
    ] ],
    [ "Canvas Coordinate Synchronization and Scroll Fix", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html", [
      [ "Problem Summary", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1382", null ],
      [ "Root Cause", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1383", [
        [ "Issue 1: Wrong Coordinate System (Line 1041)", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1384", null ],
        [ "Issue 2: Hover Position Not Updated (Line 416)", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1385", null ]
      ] ],
      [ "Technical Details", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1386", [
        [ "Coordinate Spaces", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1387", null ],
        [ "Usage Patterns", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1388", null ]
      ] ],
      [ "Testing", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1389", [
        [ "Visual Testing", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1390", null ],
        [ "Unit Tests", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1391", null ]
      ] ],
      [ "Impact Analysis", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1392", [
        [ "Files Changed", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1393", null ],
        [ "Affected Functionality", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1394", null ],
        [ "Related Code That Works Correctly", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1395", null ]
      ] ],
      [ "Multi-Area Map Support", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1396", [
        [ "Standard Maps (512x512)", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1397", null ],
        [ "ZSCustomOverworld v3 Large Maps (1024x1024)", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1398", null ]
      ] ],
      [ "Issue 3: Wrong Canvas Being Scrolled (Line 2344-2366)", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1399", null ],
      [ "Issue 4: Wrong Hover Check (Line 1403)", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1400", null ],
      [ "Issue 5: Vanilla Large Map World Offset (Line 1132-1136)", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1401", null ],
      [ "Commit Reference", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1402", null ],
      [ "Future Improvements", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1403", null ],
      [ "Related Documentation", "d5/db9/md_docs_2G4-canvas-coordinate-fix.html#autotoc_md1404", null ]
    ] ],
    [ "G5 - GUI Consistency and Card-Based Architecture Guide", "db/d01/md_docs_2G5-gui-consistency-guide.html", [
      [ "Table of Contents", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1406", null ],
      [ "1. Introduction", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1407", [
        [ "Purpose", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1408", null ],
        [ "Benefits", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1409", null ],
        [ "Target Audience", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1410", null ]
      ] ],
      [ "2. Card-Based Architecture", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1411", [
        [ "Philosophy", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1412", null ],
        [ "Core Components", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1413", [
          [ "EditorCardManager", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1414", null ],
          [ "EditorCard", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1415", null ]
        ] ],
        [ "Centralized Visibility Pattern", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1416", null ],
        [ "Reference Implementations", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1417", null ]
      ] ],
      [ "3. VSCode-Style Sidebar System", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1418", [
        [ "Overview", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1419", null ],
        [ "Usage", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1420", null ],
        [ "Card Browser", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1421", null ]
      ] ],
      [ "4. Toolset System", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1422", [
        [ "Overview", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1423", null ],
        [ "Basic Usage", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1424", null ],
        [ "Advanced Features", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1425", null ],
        [ "Best Practices", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1426", null ]
      ] ],
      [ "5. GUI Library Architecture", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1427", [
        [ "Modular Library Structure", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1428", null ],
        [ "Theme-Aware Sizing System", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1429", null ]
      ] ],
      [ "6. Themed Widget System", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1430", [
        [ "Philosophy", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1431", null ],
        [ "Themed Widget Prefixes", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1432", null ],
        [ "Usage Examples", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1433", null ],
        [ "Theme Colors", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1434", null ],
        [ "Migration from Hardcoded Colors", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1435", null ],
        [ "WhichKey Command System", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1436", null ]
      ] ],
      [ "7. Begin/End Patterns", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1437", [
        [ "Philosophy", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1438", null ],
        [ "EditorCard Begin/End", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1439", null ],
        [ "ImGui Native Begin/End", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1440", null ],
        [ "Toolset Begin/End", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1441", null ],
        [ "Error Handling", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1442", null ]
      ] ],
      [ "8. Currently Integrated Editors", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1443", null ],
      [ "9. Layout Helpers", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1444", [
        [ "Overview", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1445", null ],
        [ "Standard Input Widths", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1446", null ],
        [ "Help Markers", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1447", null ],
        [ "Spacing Utilities", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1448", null ],
        [ "Responsive Layout", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1449", null ],
        [ "Grid Layouts", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1450", null ]
      ] ],
      [ "10. Workspace Management", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1451", null ],
      [ "11. Future Editor Improvements", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1452", [
        [ "SettingsEditor", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1453", null ],
        [ "AgentEditor", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1454", null ]
      ] ],
      [ "12. Migration Checklist", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1455", [
        [ "Planning Phase", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1456", null ],
        [ "Implementation Phase - Core Structure", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1457", null ],
        [ "Implementation Phase - Toolset", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1458", null ],
        [ "Implementation Phase - Control Panel", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1459", null ],
        [ "Implementation Phase - Cards", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1460", null ],
        [ "Implementation Phase - Update Method", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1461", null ],
        [ "Implementation Phase - Theming", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1462", null ],
        [ "Testing Phase", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1463", null ],
        [ "Documentation Phase", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1464", null ]
      ] ],
      [ "13. Code Examples", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1465", [
        [ "Complete Editor Implementation", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1466", null ]
      ] ],
      [ "14. Common Pitfalls", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1467", [
        [ "1. Forgetting Bidirectional Visibility Sync", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1468", null ],
        [ "2. Using Hardcoded Colors", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1469", null ],
        [ "3. Not Calling Show() Before Draw()", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1470", null ],
        [ "4. Missing EditorCardManager Registration", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1471", null ],
        [ "5. Improper Begin/End Pairing", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1472", null ],
        [ "6. Not Testing Minimize-to-Icon", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1473", null ],
        [ "7. Wrong Card Position Enum", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1474", null ],
        [ "8. Not Handling Null Rom", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1475", null ],
        [ "9. Forgetting Toolset Begin/End", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1476", null ],
        [ "10. Hardcoded Shortcuts in Tooltips", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1477", null ]
      ] ],
      [ "Summary", "db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1479", null ]
    ] ],
    [ "Changelog", "d6/da7/md_docs_2H1-changelog.html", [
      [ "0.3.2 (October 2025)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1482", [
        [ "AI Agent Infrastructure", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1483", null ],
        [ "CI/CD & Release Improvements", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1484", null ],
        [ "Rendering Pipeline Fixes", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1485", null ],
        [ "Card-Based UI System", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1486", null ],
        [ "Tile16 Editor & Graphics System", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1487", null ],
        [ "Windows Platform Stability", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1488", null ],
        [ "Emulator: Audio System Infrastructure", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1489", null ],
        [ "Emulator: Critical Performance Fixes", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1490", null ],
        [ "Emulator: UI Organization & Input System", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1491", null ],
        [ "Debugger: Breakpoint & Watchpoint Systems", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1492", null ],
        [ "Build System Simplifications", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1493", null ],
        [ "Build System: Windows Platform Improvements", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1494", null ],
        [ "GUI & UX Modernization", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1495", null ],
        [ "Overworld Editor Refactoring", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1496", null ],
        [ "Build System & Stability", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1497", null ],
        [ "Future Optimizations (Planned)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1498", null ],
        [ "Technical Notes", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1499", null ]
      ] ],
      [ "0.3.1 (September 2025)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1500", [
        [ "Major Features", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1501", null ],
        [ "Tile16 Editor Enhancements", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1502", null ],
        [ "ZSCustomOverworld v3 Implementation", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1503", null ],
        [ "Technical Improvements", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1504", null ],
        [ "User Interface", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1505", null ],
        [ "Bug Fixes", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1506", null ],
        [ "ZScream Compatibility Improvements", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1507", null ]
      ] ],
      [ "0.3.0 (September 2025)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1508", [
        [ "Major Features", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1509", null ],
        [ "User Interface & Theming", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1510", null ],
        [ "Enhancements", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1511", null ],
        [ "Technical Improvements", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1512", null ],
        [ "Bug Fixes", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1513", null ]
      ] ],
      [ "0.2.2 (December 2024)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1514", [
        [ "Core Features", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1515", null ]
      ] ],
      [ "0.2.1 (August 2024)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1516", null ],
      [ "0.2.0 (July 2024)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1517", null ],
      [ "0.1.0 (May 2024)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1518", null ],
      [ "0.0.9 (April 2024)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1519", null ],
      [ "0.0.8 (February 2024)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1520", null ],
      [ "0.0.7 (January 2024)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1521", null ],
      [ "0.0.6 (November 2023)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1522", null ],
      [ "0.0.5 (November 2023)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1523", null ],
      [ "0.0.4 (November 2023)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1524", null ],
      [ "0.0.3 (October 2023)", "d6/da7/md_docs_2H1-changelog.html#autotoc_md1525", null ]
    ] ],
    [ "EditorManager Architecture & Refactoring Guide", "da/d2c/md_docs_2H2-editor-manager-architecture.html", [
      [ "Table of Contents", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1528", null ],
      [ "Current State", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1530", [
        [ "Build Status", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1531", null ],
        [ "What Works", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1532", null ],
        [ "Remaining Work", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1533", null ]
      ] ],
      [ "Outstanding Tasks", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1535", [
        [ "High Priority", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1536", null ],
        [ "Medium Priority", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1537", null ],
        [ "Low Priority", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1538", null ],
        [ "Documentation Tasks", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1539", null ],
        [ "Future Enhancements (Out of Scope)", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1540", null ]
      ] ],
      [ "Completed Work", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1542", [
        [ "1. PopupManager - Crash Fix & Type Safety", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1543", null ],
        [ "2. Card System Unification", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1544", null ],
        [ "3. UI Code Migration", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1545", null ],
        [ "4. Settings Editor → Card-Based", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1546", null ],
        [ "5. Card Visibility Flag Fixes", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1547", null ],
        [ "6. Session Card Control Fix", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1548", null ],
        [ "7. VSCode-Style Sidebar Styling", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1549", null ],
        [ "8. Debug Menu Restoration", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1550", null ],
        [ "9. Command Palette Debug Logging", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1551", null ]
      ] ],
      [ "All Critical Issues RESOLVED", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1553", [
        [ "Issue 1: Emulator Cards Can't Close - FIXED", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1554", null ],
        [ "Issue 2: Session Card Control Not Editor-Aware - FIXED", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1555", null ],
        [ "Issue 3: Card Visibility Flag Passing Pattern - FIXED", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1556", null ]
      ] ],
      [ "Architecture Patterns", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1558", [
        [ "Pattern 1: Popup (Modal Dialog)", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1559", null ],
        [ "Pattern 2: Window (Non-Modal)", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1560", null ],
        [ "Pattern 3: Editor Card (Session-Aware)", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1561", null ],
        [ "Pattern 4: ImGui Built-in Windows", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1562", null ]
      ] ],
      [ "Outstanding Tasks (October 2025)", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1564", null ],
      [ "Testing Plan", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1566", [
        [ "Manual Testing Checklist", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1567", null ],
        [ "Automated Testing", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1568", null ],
        [ "Regression Testing", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1569", null ]
      ] ],
      [ "File Reference", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1571", [
        [ "Core EditorManager Files", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1572", null ],
        [ "Delegation Components", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1573", null ],
        [ "All 10 Editors", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1574", null ],
        [ "Supporting Components", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1575", null ],
        [ "OLD System (Can be deleted after verification)", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1576", null ]
      ] ],
      [ "Instructions for Next Agent", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1578", [
        [ "Verification Process", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1579", null ],
        [ "Quick Wins (1-2 hours)", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1580", null ],
        [ "Medium Priority (2-4 hours)", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1581", null ],
        [ "Code Quality (ongoing)", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1582", null ]
      ] ],
      [ "Key Lessons", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1584", [
        [ "What Worked Well", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1585", null ],
        [ "What Caused Issues", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1586", null ],
        [ "Best Practices Going Forward", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1587", null ]
      ] ],
      [ "Quick Reference", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1589", [
        [ "Initialization Order (CRITICAL)", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1590", null ],
        [ "Common Fixes", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1591", null ],
        [ "Build & Test", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1592", null ]
      ] ],
      [ "Current Snapshot", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1594", null ],
      [ "Summary of Refactoring - October 15, 2025", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1595", [
        [ "Changes Made in This Session", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1596", null ],
        [ "Build Status", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1597", null ],
        [ "Testing Checklist", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1598", null ]
      ] ],
      [ "Phase Completion - October 15, 2025 (Continued)", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1600", [
        [ "Additional Refactoring Completed", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1601", null ],
        [ "Files Created", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1602", null ],
        [ "Files Modified (Major Changes)", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1603", null ],
        [ "Build Status", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1604", null ],
        [ "Success Metrics Achieved", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1605", null ],
        [ "Next Steps (Future Work)", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1606", null ]
      ] ],
      [ "Critical Bug Fixes - October 15, 2025 (Final Session)", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1609", [
        [ "Welcome Screen Not Appearing - ROOT CAUSE FOUND", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1610", null ]
      ] ],
      [ "Complete Feature Summary", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1612", [
        [ "What Works Now", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1613", null ],
        [ "Architecture Improvements", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1614", null ],
        [ "Code Quality Metrics", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1615", null ]
      ] ],
      [ "Final Refactoring Summary - October 15, 2025", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1617", [
        [ "What Was Accomplished", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1618", null ],
        [ "Files Created (2 files, 498 lines)", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1619", null ],
        [ "Files Deleted (2 files, ~1200 lines)", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1620", null ],
        [ "Files Modified (14 major files)", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1621", null ],
        [ "Net Code Change", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1622", null ],
        [ "Testing Status", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1623", null ],
        [ "Success Criteria Achieved", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1624", null ]
      ] ],
      [ "Master vs Develop Feature Parity Analysis", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1626", [
        [ "Code Statistics", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1627", null ],
        [ "Feature Parity Matrix", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1628", [
          [ "✅ COMPLETE (Feature Parity Achieved)", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1629", null ],
          [ "🟡 PARTIAL (Features Exist but Incomplete)", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1630", null ],
          [ "❌ NOT IMPLEMENTED (Enhancement Features)", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1631", null ]
        ] ],
        [ "Gap Analysis by Category", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1632", [
          [ "High Priority (Blocking Release)", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1633", null ],
          [ "Medium Priority (Nice to Have)", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1634", null ],
          [ "Low Priority (Future Enhancement)", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1635", null ]
        ] ],
        [ "Specific Master Branch Features Not Yet in Develop", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1636", [
          [ "1. Performance Dashboard Integration", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1637", null ],
          [ "2. Agent Integration (Conditional)", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1638", null ],
          [ "3. Hex Editor", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1639", null ],
          [ "4. Assembly Editor Features", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1640", null ]
        ] ],
        [ "Testing Checklist for Feature Parity", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1641", null ],
        [ "Master Branch Methods Not Found in Develop (Source of Truth)", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1642", null ],
        [ "Recommendations for Reaching 100% Parity", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1643", null ],
        [ "Success Criteria", "da/d2c/md_docs_2H2-editor-manager-architecture.html#autotoc_md1644", null ]
      ] ]
    ] ],
    [ "Roadmap", "d8/d97/md_docs_2I1-roadmap.html", [
      [ "Current Focus", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1647", null ],
      [ "0.4.0 (Next Major Release) - SDL3 Modernization & Core Improvements", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1648", [
        [ "Primary Goals", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1649", null ],
        [ "Phase 1: Infrastructure (Week 1-2)", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1650", null ],
        [ "Phase 2: SDL3 Core Migration (Week 3-4)", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1651", null ],
        [ "Phase 3: Complete SDL3 Integration (Week 5-6)", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1652", null ],
        [ "Phase 4: Editor Features & UX (Week 7-8)", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1653", null ],
        [ "Phase 5: AI Agent Enhancements (Throughout)", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1654", null ],
        [ "Success Criteria", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1655", null ]
      ] ],
      [ "0.5.X - Feature Expansion", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1657", null ],
      [ "0.6.X - Content & Integration", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1659", null ],
      [ "Recently Completed (v0.3.3 - October 6, 2025)", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1661", null ],
      [ "Recently Completed (v0.3.2)", "d8/d97/md_docs_2I1-roadmap.html#autotoc_md1662", null ]
    ] ],
    [ "Future Improvements & Long-Term Vision", "db/da4/md_docs_2I2-future-improvements.html", [
      [ "Architecture & Performance", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1665", [
        [ "Emulator Core Improvements", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1666", null ],
        [ "Plugin Architecture (v0.5.x+)", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1667", null ],
        [ "Multi-Threading Improvements", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1668", null ]
      ] ],
      [ "Graphics & Rendering", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1670", [
        [ "Advanced Graphics Editing", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1671", null ],
        [ "Alternative Rendering Backends", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1672", null ],
        [ "High-DPI / 4K Support", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1673", null ]
      ] ],
      [ "AI & Automation", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1675", [
        [ "Autonomous Debugging Enhancements", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1676", [
          [ "Pattern 1: Automated Bug Reproduction", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1677", null ],
          [ "Pattern 2: Automated Code Coverage Analysis", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1678", null ],
          [ "Pattern 3: Autonomous Bug Hunting", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1679", null ],
          [ "Future API Extensions", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1680", null ]
        ] ],
        [ "Multi-Modal AI Input", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1681", null ],
        [ "Collaborative AI Sessions", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1682", null ],
        [ "Automation & Scripting", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1683", null ]
      ] ],
      [ "Content Editors", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1685", [
        [ "Music Editor UI", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1686", null ],
        [ "Dialogue Editor", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1687", null ],
        [ "Event Editor", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1688", null ],
        [ "Hex Editor Enhancements", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1689", null ]
      ] ],
      [ "Collaboration & Networking", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1691", [
        [ "Real-Time Collaboration Improvements", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1692", null ],
        [ "Cloud ROM Storage", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1693", null ]
      ] ],
      [ "Platform Support", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1695", [
        [ "Web Assembly Build", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1696", null ],
        [ "Mobile Support (iOS/Android)", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1697", null ]
      ] ],
      [ "Quality of Life", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1699", [
        [ "Undo/Redo System Enhancement", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1700", null ],
        [ "Project Templates", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1701", null ],
        [ "Asset Library", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1702", null ],
        [ "Accessibility", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1703", null ]
      ] ],
      [ "Testing & Quality", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1705", [
        [ "Automated Regression Testing", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1706", null ],
        [ "ROM Validation", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1707", null ],
        [ "Continuous Integration Enhancements", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1708", null ]
      ] ],
      [ "Documentation & Community", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1710", [
        [ "API Documentation Generator", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1711", null ],
        [ "Video Tutorial System", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1712", null ],
        [ "ROM Hacking Wiki Integration", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1713", null ]
      ] ],
      [ "Experimental / Research", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1715", [
        [ "Machine Learning Integration", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1716", null ],
        [ "VR/AR Visualization", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1717", null ],
        [ "Symbolic Execution", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1718", null ]
      ] ],
      [ "Implementation Priority", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1720", null ],
      [ "Contributing Ideas", "db/da4/md_docs_2I2-future-improvements.html#autotoc_md1722", null ]
    ] ],
    [ "yaze Documentation", "d3/d4c/md_docs_2index.html", [
      [ "A: Getting Started & Testing", "d3/d4c/md_docs_2index.html#autotoc_md1725", null ],
      [ "B: Build & Platform", "d3/d4c/md_docs_2index.html#autotoc_md1726", null ],
      [ "C: <tt>z3ed</tt> CLI", "d3/d4c/md_docs_2index.html#autotoc_md1727", null ],
      [ "E: Development & API", "d3/d4c/md_docs_2index.html#autotoc_md1728", null ],
      [ "F: Technical Documentation", "d3/d4c/md_docs_2index.html#autotoc_md1729", null ],
      [ "G: Graphics & GUI Systems", "d3/d4c/md_docs_2index.html#autotoc_md1730", null ],
      [ "H: Project Info", "d3/d4c/md_docs_2index.html#autotoc_md1731", null ],
      [ "I: Roadmap & Vision", "d3/d4c/md_docs_2index.html#autotoc_md1732", null ],
      [ "R: ROM Reference", "d3/d4c/md_docs_2index.html#autotoc_md1733", null ],
      [ "Documentation Standards", "d3/d4c/md_docs_2index.html#autotoc_md1735", [
        [ "Naming Convention", "d3/d4c/md_docs_2index.html#autotoc_md1736", null ],
        [ "File Naming", "d3/d4c/md_docs_2index.html#autotoc_md1737", null ],
        [ "Organization Tips", "d3/d4c/md_docs_2index.html#autotoc_md1738", null ],
        [ "Doxygen Integration Tips", "d3/d4c/md_docs_2index.html#autotoc_md1739", null ]
      ] ]
    ] ],
    [ "A Link to the Past ROM Reference", "d7/d4f/md_docs_2R1-alttp-rom-reference.html", [
      [ "Graphics System", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1742", [
        [ "Graphics Sheets", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1743", null ],
        [ "Palette System", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1744", [
          [ "Color Format", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1745", null ],
          [ "16-Color Row Structure", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1746", null ],
          [ "Palette Groups", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1747", null ],
          [ "Palette Application to Graphics", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1748", null ]
        ] ]
      ] ],
      [ "Dungeon System", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1749", [
        [ "Room Data Structure", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1750", null ],
        [ "Tile16 Format", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1751", null ],
        [ "Blocksets", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1752", null ]
      ] ],
      [ "Message System", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1753", [
        [ "Text Data Locations", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1754", null ],
        [ "Character Encoding", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1755", null ],
        [ "Text Commands", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1756", null ],
        [ "Font Graphics", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1757", null ]
      ] ],
      [ "Overworld System", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1758", [
        [ "Map Structure", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1759", null ],
        [ "Area Sizes (ZSCustomOverworld v3+)", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1760", null ],
        [ "Tile Format", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1761", null ]
      ] ],
      [ "Compression", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1762", [
        [ "LC-LZ2 Algorithm", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1763", null ]
      ] ],
      [ "Memory Map", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1764", [
        [ "ROM Banks (LoROM)", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1765", null ],
        [ "Important ROM Locations", "d7/d4f/md_docs_2R1-alttp-rom-reference.html#autotoc_md1766", null ]
      ] ]
    ] ],
    [ "yaze - Yet Another Zelda3 Editor", "d0/d30/md_README.html", [
      [ "Version 0.3.2 - Release", "d0/d30/md_README.html#autotoc_md1769", null ],
      [ "Quick Start", "d0/d30/md_README.html#autotoc_md1774", [
        [ "Build", "d0/d30/md_README.html#autotoc_md1775", null ],
        [ "Applications", "d0/d30/md_README.html#autotoc_md1776", null ]
      ] ],
      [ "Usage", "d0/d30/md_README.html#autotoc_md1777", [
        [ "GUI Editor", "d0/d30/md_README.html#autotoc_md1778", null ],
        [ "Command Line Tool", "d0/d30/md_README.html#autotoc_md1779", null ],
        [ "C++ API", "d0/d30/md_README.html#autotoc_md1780", null ]
      ] ],
      [ "Documentation", "d0/d30/md_README.html#autotoc_md1781", null ],
      [ "Supported Platforms", "d0/d30/md_README.html#autotoc_md1782", null ],
      [ "ROM Compatibility", "d0/d30/md_README.html#autotoc_md1783", null ],
      [ "Contributing", "d0/d30/md_README.html#autotoc_md1784", null ],
      [ "License", "d0/d30/md_README.html#autotoc_md1785", null ],
      [ "🙏 Acknowledgments", "d0/d30/md_README.html#autotoc_md1786", null ],
      [ "📸 Screenshots", "d0/d30/md_README.html#autotoc_md1787", null ]
    ] ],
    [ "YAZE Build Scripts", "de/d82/md_scripts_2README.html", [
      [ "build_cleaner.py", "de/d82/md_scripts_2README.html#autotoc_md1790", [
        [ "Features", "de/d82/md_scripts_2README.html#autotoc_md1791", null ],
        [ "Usage", "de/d82/md_scripts_2README.html#autotoc_md1792", null ],
        [ "Opting-In to Auto-Maintenance", "de/d82/md_scripts_2README.html#autotoc_md1793", null ],
        [ "Excluding Files from Processing", "de/d82/md_scripts_2README.html#autotoc_md1794", null ],
        [ ".gitignore Support", "de/d82/md_scripts_2README.html#autotoc_md1795", null ],
        [ "IWYU Configuration", "de/d82/md_scripts_2README.html#autotoc_md1796", null ],
        [ "Integration with CMake", "de/d82/md_scripts_2README.html#autotoc_md1797", null ],
        [ "Dependencies", "de/d82/md_scripts_2README.html#autotoc_md1798", null ],
        [ "How It Works", "de/d82/md_scripts_2README.html#autotoc_md1799", null ],
        [ "Current Auto-Maintained Variables", "de/d82/md_scripts_2README.html#autotoc_md1800", null ]
      ] ]
    ] ],
    [ "Agent Editor Module", "d6/df7/md_src_2app_2editor_2agent_2README.html", [
      [ "Overview", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1802", null ],
      [ "Architecture", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1803", [
        [ "Core Components", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1804", [
          [ "AgentEditor (<tt>agent_editor.h/cc</tt>)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1805", null ],
          [ "AgentChatWidget (<tt>agent_chat_widget.h/cc</tt>)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1806", null ],
          [ "AgentChatHistoryCodec (<tt>agent_chat_history_codec.h/cc</tt>)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1807", null ]
        ] ],
        [ "Collaboration Coordinators", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1808", [
          [ "AgentCollaborationCoordinator (<tt>agent_collaboration_coordinator.h/cc</tt>)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1809", null ],
          [ "NetworkCollaborationCoordinator (<tt>network_collaboration_coordinator.h/cc</tt>)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1810", null ]
        ] ]
      ] ],
      [ "Usage", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1811", [
        [ "Initialization", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1812", null ],
        [ "Drawing", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1813", null ],
        [ "Session Management", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1814", null ],
        [ "Network Mode (requires YAZE_WITH_GRPC and YAZE_WITH_JSON)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1815", null ]
      ] ],
      [ "File Structure", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1816", null ],
      [ "Build Configuration", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1817", [
        [ "Required", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1818", null ],
        [ "Optional", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1819", null ]
      ] ],
      [ "Data Files", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1820", [
        [ "Local Storage", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1821", null ],
        [ "Session File Format", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1822", null ]
      ] ],
      [ "Integration with EditorManager", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1823", null ],
      [ "Dependencies", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1824", [
        [ "Internal", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1825", null ],
        [ "External (when enabled)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1826", null ]
      ] ],
      [ "Advanced Features (v2.0)", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1827", [
        [ "ROM Synchronization", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1828", null ],
        [ "Multimodal Snapshot Sharing", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1829", null ],
        [ "Proposal Management", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1830", null ],
        [ "AI Agent Integration", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1831", null ],
        [ "Health Monitoring", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1832", null ]
      ] ],
      [ "Future Enhancements", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1833", null ],
      [ "Server Protocol", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1834", [
        [ "Client → Server", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1835", null ],
        [ "Server → Client", "d6/df7/md_src_2app_2editor_2agent_2README.html#autotoc_md1836", null ]
      ] ]
    ] ],
    [ "YAZE Modern Command Handler Architecture", "dd/dee/md_src_2cli_2handlers_2README.html", [
      [ "Architecture Overview", "dd/dee/md_src_2cli_2handlers_2README.html#autotoc_md1838", null ],
      [ "Namespace Structure", "dd/dee/md_src_2cli_2handlers_2README.html#autotoc_md1839", null ],
      [ "Directory Organization", "dd/dee/md_src_2cli_2handlers_2README.html#autotoc_md1840", null ],
      [ "Creating a New Command Handler", "dd/dee/md_src_2cli_2handlers_2README.html#autotoc_md1841", [
        [ "1. Define the Handler Class", "dd/dee/md_src_2cli_2handlers_2README.html#autotoc_md1842", null ],
        [ "2. Implement the Handler", "dd/dee/md_src_2cli_2handlers_2README.html#autotoc_md1843", null ],
        [ "3. Register in Factory", "dd/dee/md_src_2cli_2handlers_2README.html#autotoc_md1844", null ],
        [ "4. Add Forward Declaration", "dd/dee/md_src_2cli_2handlers_2README.html#autotoc_md1845", null ]
      ] ],
      [ "Command Handler Base Class", "dd/dee/md_src_2cli_2handlers_2README.html#autotoc_md1846", [
        [ "Lifecycle Methods", "dd/dee/md_src_2cli_2handlers_2README.html#autotoc_md1847", null ],
        [ "Helper Methods", "dd/dee/md_src_2cli_2handlers_2README.html#autotoc_md1848", null ]
      ] ],
      [ "Argument Parsing", "dd/dee/md_src_2cli_2handlers_2README.html#autotoc_md1849", null ],
      [ "Output Formatting", "dd/dee/md_src_2cli_2handlers_2README.html#autotoc_md1850", null ],
      [ "Integration with Public API", "dd/dee/md_src_2cli_2handlers_2README.html#autotoc_md1851", null ],
      [ "Best Practices", "dd/dee/md_src_2cli_2handlers_2README.html#autotoc_md1852", null ],
      [ "Testing", "dd/dee/md_src_2cli_2handlers_2README.html#autotoc_md1853", null ],
      [ "Future Enhancements", "dd/dee/md_src_2cli_2handlers_2README.html#autotoc_md1854", null ]
    ] ],
    [ "SNES Palette Structure for ALTTP", "d3/d6f/md_src_2zelda3_2palette__structure.html", [
      [ "SNES Palette Memory Layout", "d3/d6f/md_src_2zelda3_2palette__structure.html#autotoc_md1856", [
        [ "Example Layout", "d3/d6f/md_src_2zelda3_2palette__structure.html#autotoc_md1857", null ]
      ] ],
      [ "ALTTP Palette Groups - Corrected Structure", "d3/d6f/md_src_2zelda3_2palette__structure.html#autotoc_md1858", [
        [ "Background Palettes (BG)", "d3/d6f/md_src_2zelda3_2palette__structure.html#autotoc_md1859", [
          [ "Overworld Main (35 colors per set)", "d3/d6f/md_src_2zelda3_2palette__structure.html#autotoc_md1860", null ],
          [ "Overworld Auxiliary (21 colors per set)", "d3/d6f/md_src_2zelda3_2palette__structure.html#autotoc_md1861", null ],
          [ "Overworld Animated (7 colors per set)", "d3/d6f/md_src_2zelda3_2palette__structure.html#autotoc_md1862", null ],
          [ "Dungeon Main (90 colors per set)", "d3/d6f/md_src_2zelda3_2palette__structure.html#autotoc_md1863", null ]
        ] ],
        [ "Sprite Palettes (OAM)", "d3/d6f/md_src_2zelda3_2palette__structure.html#autotoc_md1864", [
          [ "Global Sprites (60 colors total)", "d3/d6f/md_src_2zelda3_2palette__structure.html#autotoc_md1865", null ],
          [ "Sprites Auxiliary 1 (7 colors per palette)", "d3/d6f/md_src_2zelda3_2palette__structure.html#autotoc_md1866", null ],
          [ "Sprites Auxiliary 2 (7 colors per palette)", "d3/d6f/md_src_2zelda3_2palette__structure.html#autotoc_md1867", null ],
          [ "Sprites Auxiliary 3 (7 colors per palette)", "d3/d6f/md_src_2zelda3_2palette__structure.html#autotoc_md1868", null ]
        ] ],
        [ "Equipment/Link Palettes", "d3/d6f/md_src_2zelda3_2palette__structure.html#autotoc_md1869", [
          [ "Armor/Link (15 colors per palette)", "d3/d6f/md_src_2zelda3_2palette__structure.html#autotoc_md1870", null ],
          [ "Swords (3 colors per palette)", "d3/d6f/md_src_2zelda3_2palette__structure.html#autotoc_md1871", null ],
          [ "Shields (4 colors per palette)", "d3/d6f/md_src_2zelda3_2palette__structure.html#autotoc_md1872", null ]
        ] ],
        [ "HUD Palettes", "d3/d6f/md_src_2zelda3_2palette__structure.html#autotoc_md1873", [
          [ "HUD (32 colors per set)", "d3/d6f/md_src_2zelda3_2palette__structure.html#autotoc_md1874", null ]
        ] ],
        [ "Special Colors", "d3/d6f/md_src_2zelda3_2palette__structure.html#autotoc_md1875", [
          [ "Grass (3 individual colors)", "d3/d6f/md_src_2zelda3_2palette__structure.html#autotoc_md1876", null ],
          [ "3D Objects (8 colors per palette)", "d3/d6f/md_src_2zelda3_2palette__structure.html#autotoc_md1877", null ],
          [ "Overworld Mini Map (128 colors per set)", "d3/d6f/md_src_2zelda3_2palette__structure.html#autotoc_md1878", null ]
        ] ]
      ] ],
      [ "Key Principles", "d3/d6f/md_src_2zelda3_2palette__structure.html#autotoc_md1879", null ],
      [ "Implementation Notes", "d3/d6f/md_src_2zelda3_2palette__structure.html#autotoc_md1880", null ]
    ] ],
    [ "End-to-End (E2E) Tests", "d9/db0/md_test_2e2e_2README.html", [
      [ "Active Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md1884", [
        [ "✅ Working Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md1885", null ],
        [ "📝 Dungeon Editor Smoke Test", "d9/db0/md_test_2e2e_2README.html#autotoc_md1886", null ]
      ] ],
      [ "Running Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md1887", [
        [ "All E2E Tests (GUI Mode)", "d9/db0/md_test_2e2e_2README.html#autotoc_md1888", null ],
        [ "Specific Test Category", "d9/db0/md_test_2e2e_2README.html#autotoc_md1889", null ],
        [ "Dungeon Editor Test Only", "d9/db0/md_test_2e2e_2README.html#autotoc_md1890", null ]
      ] ],
      [ "Test Development", "d9/db0/md_test_2e2e_2README.html#autotoc_md1891", [
        [ "Creating New Tests", "d9/db0/md_test_2e2e_2README.html#autotoc_md1892", null ],
        [ "Register in yaze_test.cc", "d9/db0/md_test_2e2e_2README.html#autotoc_md1893", null ],
        [ "ImGui Test Engine API", "d9/db0/md_test_2e2e_2README.html#autotoc_md1894", null ]
      ] ],
      [ "Test Logging", "d9/db0/md_test_2e2e_2README.html#autotoc_md1895", null ],
      [ "Test Infrastructure", "d9/db0/md_test_2e2e_2README.html#autotoc_md1896", [
        [ "File Organization", "d9/db0/md_test_2e2e_2README.html#autotoc_md1897", null ],
        [ "Helper Functions", "d9/db0/md_test_2e2e_2README.html#autotoc_md1898", null ]
      ] ],
      [ "Future Test Ideas", "d9/db0/md_test_2e2e_2README.html#autotoc_md1899", null ],
      [ "Troubleshooting", "d9/db0/md_test_2e2e_2README.html#autotoc_md1900", [
        [ "Test Crashes in GUI Mode", "d9/db0/md_test_2e2e_2README.html#autotoc_md1901", null ],
        [ "Tests Not Found", "d9/db0/md_test_2e2e_2README.html#autotoc_md1902", null ],
        [ "ImGui Items Not Found", "d9/db0/md_test_2e2e_2README.html#autotoc_md1903", null ]
      ] ],
      [ "References", "d9/db0/md_test_2e2e_2README.html#autotoc_md1904", null ],
      [ "Status", "d9/db0/md_test_2e2e_2README.html#autotoc_md1905", null ]
    ] ],
    [ "yaze Test Suite", "d0/d46/md_test_2README.html", [
      [ "Directory Structure", "d0/d46/md_test_2README.html#autotoc_md1907", null ],
      [ "Test Categories", "d0/d46/md_test_2README.html#autotoc_md1908", [
        [ "Unit Tests (<tt>unit/</tt>)", "d0/d46/md_test_2README.html#autotoc_md1909", null ],
        [ "Integration Tests (<tt>integration/</tt>)", "d0/d46/md_test_2README.html#autotoc_md1910", null ],
        [ "End-to-End Tests (<tt>e2e/</tt>)", "d0/d46/md_test_2README.html#autotoc_md1911", null ]
      ] ],
      [ "Enhanced Test Runner", "d0/d46/md_test_2README.html#autotoc_md1912", [
        [ "Usage Examples", "d0/d46/md_test_2README.html#autotoc_md1913", null ],
        [ "Test Modes", "d0/d46/md_test_2README.html#autotoc_md1914", null ],
        [ "Options", "d0/d46/md_test_2README.html#autotoc_md1915", null ]
      ] ],
      [ "E2E ROM Testing", "d0/d46/md_test_2README.html#autotoc_md1916", [
        [ "Test Cases", "d0/d46/md_test_2README.html#autotoc_md1917", null ]
      ] ],
      [ "ZSCustomOverworld Upgrade Testing", "d0/d46/md_test_2README.html#autotoc_md1918", [
        [ "Supported Upgrades", "d0/d46/md_test_2README.html#autotoc_md1919", null ],
        [ "Test Cases", "d0/d46/md_test_2README.html#autotoc_md1920", null ],
        [ "Version-Specific Features", "d0/d46/md_test_2README.html#autotoc_md1921", [
          [ "Vanilla", "d0/d46/md_test_2README.html#autotoc_md1922", null ],
          [ "v2", "d0/d46/md_test_2README.html#autotoc_md1923", null ],
          [ "v3", "d0/d46/md_test_2README.html#autotoc_md1924", null ]
        ] ]
      ] ],
      [ "Environment Variables", "d0/d46/md_test_2README.html#autotoc_md1925", null ],
      [ "CI/CD Integration", "d0/d46/md_test_2README.html#autotoc_md1926", null ],
      [ "Deprecated Tests", "d0/d46/md_test_2README.html#autotoc_md1927", null ],
      [ "Best Practices", "d0/d46/md_test_2README.html#autotoc_md1928", null ],
      [ "AI Agent Testing", "d0/d46/md_test_2README.html#autotoc_md1929", null ]
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
"d0/d20/classyaze_1_1test_1_1E2ETestSuite.html#a0fae6fffca30ef53828194b842b11cf2",
"d0/d28/classTextEditor.html#a2e7564236465557858df1c3470b0f425",
"d0/d2e/classyaze_1_1editor_1_1EditorCardRegistry.html#aa432b853f721f941ba5e4cbb2a9e4943",
"d0/d4e/structyaze_1_1emu_1_1CGADD.html#af2a5cf14455776cdb3e6a920a8940736",
"d0/d6d/structyaze_1_1gfx_1_1AtlasRenderer_1_1Atlas.html#ae6e28abc35253b28f97d257f329f9404",
"d0/d91/classyaze_1_1gui_1_1CanvasContextMenu.html#afed6f6f062936d1b393eece86e4de5d7",
"d0/dbd/classyaze_1_1emu_1_1Emulator.html#a6798ec1f67786642627ef98155348597",
"d0/ddd/classyaze_1_1test_1_1TestEditor.html#a0bdcb7387a75d0693cd721a78f09318c",
"d1/d0f/classyaze_1_1cli_1_1agent_1_1LearnedKnowledgeService.html#a667e908fc20c3b0192b14b814670b096",
"d1/d22/classyaze_1_1editor_1_1DungeonObjectInteraction.html#a65efe169f73cb2fbd3b9c17ada0797c5",
"d1/d3e/namespaceyaze_1_1editor.html#a297b0603822af41a3d23fbc2da2a622a",
"d1/d4b/namespaceyaze_1_1gfx_1_1lc__lz2.html#a58f3379ee8d339cec06cd454555e1fac",
"d1/d6e/classyaze_1_1cli_1_1EnhancedStatusPanel.html#a0ccc049aa095c1fc0ce8afee6e0946dd",
"d1/d8c/classyaze_1_1agent_1_1EmulatorServiceImpl.html#a8f09aa0df402b398c593d2dd951c2479",
"d1/daf/classyaze_1_1cli_1_1resources_1_1OutputFormatterTest.html",
"d1/de0/classyaze_1_1test_1_1TestSuite.html#a0882944b46b0140c48b98cc4076dd08c",
"d2/d01/emulator__ui_8cc.html#a585d4b9456467f860c95bfcb72690414",
"d2/d07/classyaze_1_1emu_1_1Spc700.html#abc07a524008c6c68f9368b9d4e54c572",
"d2/d26/z3ed__network__client_8h_source.html",
"d2/d3a/classyaze_1_1editor_1_1MenuOrchestrator.html#ad4627af9e51a3d2b59bf4c6d6126ef59",
"d2/d4a/md_docs_2C1-z3ed-agent-guide.html#autotoc_md327",
"d2/d50/classyaze_1_1emu_1_1audio_1_1IAudioBackend.html#a4aac183d6e64188ed4652f58d63c5e2a",
"d2/d67/classyaze_1_1cli_1_1GuiAutomationClient.html#aab7cf24de814e6c8e2f56a3c833a181d",
"d2/dc2/classyaze_1_1gui_1_1BppFormatUI.html#ab22779c078839d9c0d039d86d0250d57",
"d2/de7/classyaze_1_1editor_1_1DungeonCanvasViewer.html#a3568cc0a1e7ddae2cf199546266137d7",
"d2/df8/namespaceyaze_1_1editor_1_1PopupID.html#a64530127a4a1ee7ac4a25a07910c7933",
"d2/dfe/structyaze_1_1gui_1_1EnhancedTheme.html#ae46c77728a1044539287069323011a3a",
"d3/d15/classyaze_1_1emu_1_1Snes.html#a25ca5cd6f570181c1d29924d2e844c1e",
"d3/d1e/structyaze_1_1zelda3_1_1LayerMergeType.html#a5cc43e491dc64309089c86482f34c8a8",
"d3/d37/structyaze_1_1cli_1_1agent_1_1LearnedKnowledgeService_1_1ConversationMemory.html#aeb61beaed587461b77995f4da4db09e1",
"d3/d44/classyaze_1_1editor_1_1OverworldEditor.html#aaf2aada67e6ca22ed036c562c722f85f",
"d3/d4f/classyaze_1_1editor_1_1AgentCollaborationCoordinator.html#a51cb500f862d082a879fd6a3fbad4421",
"d3/d6c/classyaze_1_1editor_1_1MessageEditor.html#aa053e9ab0942b2146c57bb8497497b31",
"d3/d8d/classyaze_1_1editor_1_1PopupManager.html#a6b4dd622a1ccd0b902b870a426ff87ea",
"d3/da7/structyaze_1_1gfx_1_1lc__lz2_1_1CompressionContext.html#a189481810b400d3f56acebf6533baa1f",
"d3/dbf/canvas__usage__tracker_8h.html#a25124d86cf150b32f72978054843bf38a3a09ec42f8f9af066489d8221bf9c144",
"d3/dbf/namespaceyaze_1_1gui.html#abd9a49ec6350e33700334b783e0c9cad",
"d3/ded/classyaze_1_1emu_1_1Ppu.html#a3308d453ff650d1f61f6dbfc82111c01",
"d4/d02/namespaceyaze_1_1test_1_1anonymous__namespace_02test__recorder_8cc_03.html",
"d4/d0a/namespaceyaze_1_1test.html#a8340569b4d6b8ea3527fbb450c73fc72",
"d4/d17/md_docs_2A1-yaze-dependency-architecture.html#autotoc_md37",
"d4/d4b/structyaze_1_1gui_1_1ExampleSelectionWithDeletion.html#afc719c78a285e4f75f6777137401c4a5",
"d4/d7c/classyaze_1_1emu_1_1PpuInterface.html#ad72156d555df5ac5842cd83193dce464",
"d4/da8/classyaze_1_1cli_1_1handlers_1_1GuiClickCommandHandler.html#a87f7d76abed493e9a781a98bb393e83f",
"d4/dde/namespaceyaze_1_1cli_1_1util_1_1icons.html#a5f5869cfe7ba9a7e4ef7f744067f01c2",
"d5/d18/md_docs_2E2-development-guide.html#autotoc_md593",
"d5/d1f/namespaceyaze_1_1zelda3.html#a599b7198a7dd0ec9da754a83f9a7e5f9",
"d5/d1f/namespaceyaze_1_1zelda3.html#aad378435391c77fde26fd3955221a24b",
"d5/d38/classyaze_1_1cli_1_1handlers_1_1MessageReadCommandHandler.html#a97322e42e4b99517e4a5455d2530a8f8",
"d5/d67/classyaze_1_1cli_1_1PolicyEvaluator.html#a8a59cbf06fdb470d466c03b5cac61a86",
"d5/d95/structyaze_1_1zelda3_1_1DungeonEditorSystem_1_1DungeonSettings.html#a5c93ebc16d77f5f3f08a173680ea5e62",
"d5/dbf/classyaze_1_1cli_1_1handlers_1_1SpritePropertiesCommandHandler.html#a036e80594ec4071618c6cb5b7dd2be01",
"d5/dcf/classyaze_1_1editor_1_1SessionCoordinator.html#a96609888b0b355db002522c73ed17a8d",
"d6/d00/classyaze_1_1cli_1_1EnhancedChatComponent.html#a1bc84771587274437c12c4f02c9be1e5",
"d6/d07/classyaze_1_1editor_1_1UICoordinator.html#abfd06e8f44082b73c99b8cda93bc7a5c",
"d6/d0e/session__coordinator_8h_source.html",
"d6/d28/classyaze_1_1net_1_1RomVersionManager.html#ab22e9d5e2c1d183422c5472e84ad337c",
"d6/d30/classyaze_1_1Rom.html#a97d2783a3b91dd4acb21af5547a4ecfa",
"d6/d4b/structyaze_1_1cli_1_1TestStep.html#aaca64ac25462cc48cdc5b6b7c37965ab",
"d6/d7a/test__suite_8h.html",
"d6/dac/classyaze_1_1gfx_1_1anonymous__namespace_02snes__color__test_8cc_03_1_1SnesColorTest.html",
"d6/dcc/classyaze_1_1gfx_1_1GraphicsOptimizationScope.html",
"d6/df5/classyaze_1_1cli_1_1agent_1_1ConversationalAgentService.html",
"d7/d17/classyaze_1_1emu_1_1WatchpointManager.html#accd708ed044c5f1ded617daff6061be9",
"d7/d44/md_docs_2E4-Emulator-Development-Guide.html#autotoc_md676",
"d7/d61/classyaze_1_1zelda3_1_1DungeonEditorSystem.html#a4352ffc2e6fe1231dafaed15e3a9e97f",
"d7/d61/classyaze_1_1zelda3_1_1DungeonEditorSystem.html#af3609dbad67e040fc87eaacc698f2aab",
"d7/d9a/md_docs_2E1-asm-style-guide.html#autotoc_md535",
"d7/dc5/classyaze_1_1util_1_1Flag.html#a508fe4bd18cca07a30cca1167ad8d7c4",
"d7/ddf/structyaze_1_1emu_1_1WindowMaskSettings2.html#ab8e4c636dddf40beb97cc84bed60be2f",
"d7/df6/classyaze_1_1zelda3_1_1Room.html#a8d99ac30411bcb54799c27c472f7d0c1",
"d7/dfd/structyaze_1_1gui_1_1CanvasUsageStats.html#aec9df395e4eb9f107e7346c94dc5c83d",
"d8/d1e/classyaze_1_1zelda3_1_1RoomEntrance.html",
"d8/d44/classyaze_1_1gfx_1_1BppConversionScope.html#a939f71369a83e42a3d77aab77673288c",
"d8/d6e/classyaze_1_1gfx_1_1AtlasRenderer.html#afe3b8496520920973c23d77bdab6df39",
"d8/d99/classyaze_1_1Controller.html",
"d8/dd3/namespaceyaze_1_1cli_1_1agent.html#a570346e91ec90d0baf95a883ca3665e1a1e0c83decaf6974c553c0fffec42c886",
"d8/ddb/snes__palette_8h.html#a82a8956476ffc04750bcfc4120c8b8dba88eb78a13c2d02166e0cf9c2fdd458c4",
"d9/d0f/main_8cc.html#a2c88155d1fb6995fa7b890ff689d8b17",
"d9/d2b/classyaze_1_1editor_1_1AgentChatWidget.html#aa2cb71d26e9d7dfdd5188c393b1e6f14",
"d9/d5d/structyaze_1_1gui_1_1RectSelectionEvent.html",
"d9/d85/md_docs_2F3-overworld-loading.html#autotoc_md1109",
"d9/da9/classyaze_1_1cli_1_1handlers_1_1HexReadCommandHandler.html#a444747fff1604e3cac95f6b463b247f5",
"d9/dc0/room_8h.html#a44b1e31e5857baaa00dc88ae724003c3",
"d9/dc5/classyaze_1_1zelda3_1_1Overworld.html#a9e4643e2a4359ab96dbdb48e80dbca13",
"d9/dcc/classyaze_1_1zelda3_1_1OverworldMap.html#aa604c0e0873870e1497efbdf6bbff59e",
"d9/de8/classyaze_1_1cli_1_1handlers_1_1ResourceSearchCommandHandler.html",
"d9/dfe/classyaze_1_1gui_1_1WidgetIdRegistry.html#a882fb026599d512e80a14072c223cec0",
"da/d2c/classyaze_1_1gui_1_1Canvas.html#a2995122ee4e7d273cddb9bbc2b4bbade",
"da/d2c/classyaze_1_1gui_1_1Canvas.html#ac6d2a15c340e837634f462b3311ad9a6",
"da/d37/hyrule__magic_8cc.html#a23f2763cd8a3044a6a63cfa1c4c0ce53",
"da/d45/classyaze_1_1editor_1_1DungeonEditorV2.html#ad437262b55cfd6a08a888ae1ccc05d45",
"da/d7e/classyaze_1_1gui_1_1CanvasUsageTracker.html#ad704bd30c1a75fc30a1883a1882cf8df",
"da/dae/classyaze_1_1net_1_1WebSocketClient.html#a5a4445e4842fd73c10303b26e5095ddb",
"da/dcb/bitmap_8h_source.html",
"da/dec/structyaze_1_1gfx_1_1BppFormatInfo.html#afc705e4b3d7f792164fe17bad007c6ca",
"db/d01/md_docs_2G5-gui-consistency-guide.html#autotoc_md1466",
"db/d29/namespaceyaze_1_1cli_1_1anonymous__namespace_02test__suite__writer_8cc_03.html",
"db/d50/command__context__test_8cc.html#abb49aef1e176b1ba59ec3886dba7d198",
"db/d7d/namespacebuild__cleaner.html#adb23d6d0b9c9dc8df766d4c198f19ace",
"db/d82/classyaze_1_1editor_1_1Tile16Editor.html#afbdefa661e6df094b1d62e8f4413452d",
"db/da4/md_docs_2I2-future-improvements.html#autotoc_md1666",
"db/dcc/classyaze_1_1editor_1_1ScreenEditor.html#a0d14d5dd0a32337eea0e2f885fc9a4ba",
"db/de8/classyaze_1_1editor_1_1DungeonRoomSelector.html#ace45952bdff186d407073d402dd411c2",
"dc/d0c/classyaze_1_1cli_1_1handlers_1_1DungeonListObjectsCommandHandler.html",
"dc/d31/classyaze_1_1editor_1_1GraphicsEditor.html#a2c2348d1bf32cf48ca84af4913495ed1",
"dc/d41/todo__commands_8cc.html#a451c910456e5af772ecd39ac26ea8ab3",
"dc/d55/classyaze_1_1gfx_1_1MemoryPool.html#adb48401e3ee771f8fd108c17e3642ece",
"dc/d71/structyaze_1_1editor_1_1UserSettings_1_1Preferences.html#ab03dfc03d6304137a25ccf170c0c4702",
"dc/daa/classyaze_1_1editor_1_1WelcomeScreen.html#a275a392e49bee7e8eeb4b4333a6411a4",
"dc/dc7/classyaze_1_1gui_1_1CanvasPerformanceIntegration.html#a79f8cb4369130822540533b86e1e7065",
"dc/df4/classyaze_1_1emu_1_1Cpu.html",
"dc/df4/classyaze_1_1emu_1_1Cpu.html#a9060970539a700d9eb29fe9675304a6f",
"dc/df8/structyaze_1_1project_1_1ProjectMetadata.html#a4dd0ca17cc2df371dca52679737bf7e2",
"dd/d12/classyaze_1_1editor_1_1EditorManager.html#a36209b8a5b17d613060c3d06f0359ad8",
"dd/d1b/websocket__client_8h.html#a7d076fc70bcff0085dcac770ae381cb2ae3587c730cc1aa530fa4ddc9c4204e97",
"dd/d40/classyaze_1_1test_1_1DungeonObjectRenderingTests.html#a2b2c2497cff68b90f96a5faf75c08807",
"dd/d63/namespaceyaze_1_1cli.html#a556e883eb9174f820cdce1c4e8ac7395a244ce4b6c7f56eaa446d64fc2d068bbb",
"dd/d80/structyaze_1_1cli_1_1TestCaseRunResult.html#a25768fd38c701c81d5f292c39f78f2a2",
"dd/dcc/classyaze_1_1editor_1_1ProposalDrawer.html#ad558518aec9061d1b5dcf7a4b8abd807",
"dd/df2/classyaze_1_1TimingManager.html#a0928c0e86dccc557d37a3ed0940b8757",
"de/d0a/classyaze_1_1test_1_1EmulatorTestSuite.html#ac95eb661cf0fb1b739302ef75763ac3e",
"de/d20/conversation__test_8cc.html#a30b918c694190f27a26b70fcd9c1cef9",
"de/d71/classyaze_1_1cli_1_1ResourceContextBuilder.html#a7b5e3f63aaeded48f01b4fbca8deb856",
"de/d8f/structyaze_1_1zelda3_1_1ObjectSubtypeInfo.html",
"de/dab/tui_8h.html#ab3acc1b59e84abb18e2d438f7ed2ad33aeaacebb7f1a391001a70fe2cb5681ab3",
"de/dbf/icons_8h.html#a17c1d840d223add98ddb7006950a30c7",
"de/dbf/icons_8h.html#a36beee3ebc412af5ac13eb0be3df00f5",
"de/dbf/icons_8h.html#a50fba6600f49e0a3047dc1e3af007d43",
"de/dbf/icons_8h.html#a6ecd903a96fa8b276d8625c223e187aa",
"de/dbf/icons_8h.html#a8ea3c36c2676528e4e3e387c7f78104f",
"de/dbf/icons_8h.html#aac763a8c2df4a7d248f7f9cd6fdc8366",
"de/dbf/icons_8h.html#ac99d99e1136dfbcbed012979f7e54344",
"de/dbf/icons_8h.html#ae5082113c9e7494b67688d80dca75275",
"de/dbf/icons_8h_source.html",
"de/ddf/structyaze_1_1gui_1_1CanvasGeometry.html#a6cfe095d49ac285431cc6529eb4fc45d",
"de/ded/structyaze_1_1emu_1_1WH0.html",
"df/d0e/classyaze_1_1gfx_1_1Bitmap.html#a8b0be39d195a5d6d7002d2ecd70f2a3c",
"df/d28/classyaze_1_1cli_1_1agent_1_1EnhancedTUI.html#a9e6982fee3c6b2de24b09a931081acbd",
"df/d64/structyaze_1_1emu_1_1ApuCallbacks.html#a118835693d22ce17120819b07e97e529",
"df/db9/classyaze_1_1cli_1_1ModernCLI.html#a3f19cf7256d5dd993b6de7802e5a407b",
"df/ded/terminal__colors_8h.html#a91bf2e919ffe81049c21c27d7eede39b",
"functions_vars_b.html"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';