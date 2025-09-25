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
    [ "Asm Style Guide", "md_docs_2asm-style-guide.html", [
      [ "Table of Contents", "md_docs_2asm-style-guide.html#autotoc_md1", null ],
      [ "File Structure", "md_docs_2asm-style-guide.html#autotoc_md2", null ],
      [ "Labels and Symbols", "md_docs_2asm-style-guide.html#autotoc_md3", null ],
      [ "Comments", "md_docs_2asm-style-guide.html#autotoc_md4", null ],
      [ "Instructions", "md_docs_2asm-style-guide.html#autotoc_md5", null ],
      [ "Macros", "md_docs_2asm-style-guide.html#autotoc_md6", null ],
      [ "Loops and Branching", "md_docs_2asm-style-guide.html#autotoc_md7", null ],
      [ "Data Structures", "md_docs_2asm-style-guide.html#autotoc_md8", null ],
      [ "Code Organization", "md_docs_2asm-style-guide.html#autotoc_md9", null ],
      [ "Custom Code", "md_docs_2asm-style-guide.html#autotoc_md10", null ]
    ] ],
    [ "Build Instructions", "md_docs_2build-instructions.html", [
      [ "Windows", "md_docs_2build-instructions.html#autotoc_md12", [
        [ "vcpkg", "md_docs_2build-instructions.html#autotoc_md13", null ],
        [ "msys2", "md_docs_2build-instructions.html#autotoc_md14", null ]
      ] ],
      [ "macOS", "md_docs_2build-instructions.html#autotoc_md15", null ],
      [ "iOS", "md_docs_2build-instructions.html#autotoc_md16", null ],
      [ "GNU/Linux", "md_docs_2build-instructions.html#autotoc_md17", null ]
    ] ],
    [ "Canvas Interface Migration Strategy", "md_docs_2canvas-migration.html", [
      [ "Overview", "md_docs_2canvas-migration.html#autotoc_md19", null ],
      [ "Current Issues", "md_docs_2canvas-migration.html#autotoc_md20", null ],
      [ "Migration Strategy", "md_docs_2canvas-migration.html#autotoc_md21", [
        [ "Phase 1: Create Pure Function Interface (COMPLETED)", "md_docs_2canvas-migration.html#autotoc_md22", null ],
        [ "Phase 2: Gradual Migration", "md_docs_2canvas-migration.html#autotoc_md23", [
          [ "Step 1: Add Compatibility Layer", "md_docs_2canvas-migration.html#autotoc_md24", null ],
          [ "Step 2: Update Existing Code", "md_docs_2canvas-migration.html#autotoc_md25", null ],
          [ "Step 3: Gradual Replacement", "md_docs_2canvas-migration.html#autotoc_md26", null ]
        ] ],
        [ "Phase 3: Benefits of New Interface", "md_docs_2canvas-migration.html#autotoc_md27", [
          [ "Pure Functions", "md_docs_2canvas-migration.html#autotoc_md28", null ],
          [ "Better Separation of Concerns", "md_docs_2canvas-migration.html#autotoc_md29", null ],
          [ "Easier Testing", "md_docs_2canvas-migration.html#autotoc_md30", null ]
        ] ]
      ] ],
      [ "Implementation Plan", "md_docs_2canvas-migration.html#autotoc_md31", [
        [ "Week 1: Core Infrastructure", "md_docs_2canvas-migration.html#autotoc_md32", null ],
        [ "Week 2: Compatibility Layer", "md_docs_2canvas-migration.html#autotoc_md33", null ],
        [ "Week 3: Gradual Migration", "md_docs_2canvas-migration.html#autotoc_md34", null ],
        [ "Week 4: Cleanup", "md_docs_2canvas-migration.html#autotoc_md35", null ]
      ] ],
      [ "Breaking Changes", "md_docs_2canvas-migration.html#autotoc_md36", [
        [ "None Expected", "md_docs_2canvas-migration.html#autotoc_md37", null ],
        [ "Optional Improvements", "md_docs_2canvas-migration.html#autotoc_md38", null ]
      ] ],
      [ "Testing Strategy", "md_docs_2canvas-migration.html#autotoc_md39", [
        [ "Unit Tests", "md_docs_2canvas-migration.html#autotoc_md40", null ],
        [ "Integration Tests", "md_docs_2canvas-migration.html#autotoc_md41", null ]
      ] ],
      [ "Performance Considerations", "md_docs_2canvas-migration.html#autotoc_md42", [
        [ "Minimal Overhead", "md_docs_2canvas-migration.html#autotoc_md43", null ],
        [ "Potential Optimizations", "md_docs_2canvas-migration.html#autotoc_md44", null ]
      ] ],
      [ "Rollback Plan", "md_docs_2canvas-migration.html#autotoc_md45", null ],
      [ "Conclusion", "md_docs_2canvas-migration.html#autotoc_md46", null ]
    ] ],
    [ "Canvas Interface Refactoring", "md_docs_2canvas-refactor-summary.html", [
      [ "Problem Statement", "md_docs_2canvas-refactor-summary.html#autotoc_md48", null ],
      [ "Solution Overview", "md_docs_2canvas-refactor-summary.html#autotoc_md49", null ],
      [ "Files Created", "md_docs_2canvas-refactor-summary.html#autotoc_md50", [
        [ "canvas_interface.h - Pure Function Interface", "md_docs_2canvas-refactor-summary.html#autotoc_md51", null ],
        [ "canvas_simplified.h - New Canvas Class", "md_docs_2canvas-refactor-summary.html#autotoc_md52", null ],
        [ "canvas_interface.cc - Pure Function Implementations", "md_docs_2canvas-refactor-summary.html#autotoc_md53", null ],
        [ "canvas_simplified.cc - Canvas Class Implementation", "md_docs_2canvas-refactor-summary.html#autotoc_md54", null ],
        [ "canvas_example.cc - Usage Examples", "md_docs_2canvas-refactor-summary.html#autotoc_md55", null ],
        [ "canvas_migration.md - Migration Strategy", "md_docs_2canvas-refactor-summary.html#autotoc_md56", null ]
      ] ],
      [ "Key Benefits", "md_docs_2canvas-refactor-summary.html#autotoc_md57", [
        [ "No Breaking Changes", "md_docs_2canvas-refactor-summary.html#autotoc_md58", null ],
        [ "Better Testability", "md_docs_2canvas-refactor-summary.html#autotoc_md59", null ],
        [ "Cleaner Separation of Concerns", "md_docs_2canvas-refactor-summary.html#autotoc_md60", null ],
        [ "Easier Maintenance", "md_docs_2canvas-refactor-summary.html#autotoc_md61", null ],
        [ "Extensibility", "md_docs_2canvas-refactor-summary.html#autotoc_md62", null ]
      ] ],
      [ "Migration Path", "md_docs_2canvas-refactor-summary.html#autotoc_md63", [
        [ "Phase 1: Immediate Fix ✅ COMPLETED", "md_docs_2canvas-refactor-summary.html#autotoc_md64", null ],
        [ "Phase 2: Gradual Migration ✅ COMPLETED", "md_docs_2canvas-refactor-summary.html#autotoc_md65", null ],
        [ "Phase 3: Full Migration ✅ COMPLETED", "md_docs_2canvas-refactor-summary.html#autotoc_md66", null ]
      ] ],
      [ "Current Status", "md_docs_2canvas-refactor-summary.html#autotoc_md67", [
        [ "✅ Build Issues Resolved", "md_docs_2canvas-refactor-summary.html#autotoc_md68", null ],
        [ "✅ Build System Integration", "md_docs_2canvas-refactor-summary.html#autotoc_md69", null ],
        [ "✅ Backward Compatibility Verified", "md_docs_2canvas-refactor-summary.html#autotoc_md70", null ]
      ] ],
      [ "Performance Considerations", "md_docs_2canvas-refactor-summary.html#autotoc_md71", null ],
      [ "Testing Strategy", "md_docs_2canvas-refactor-summary.html#autotoc_md72", [
        [ "Unit Tests", "md_docs_2canvas-refactor-summary.html#autotoc_md73", null ],
        [ "Integration Tests", "md_docs_2canvas-refactor-summary.html#autotoc_md74", null ]
      ] ],
      [ "Conclusion", "md_docs_2canvas-refactor-summary.html#autotoc_md75", null ]
    ] ],
    [ "Changelog", "md_docs_2changelog.html", [
      [ "0.2.2 (12-31-2024)", "md_docs_2changelog.html#autotoc_md77", null ],
      [ "0.2.1 (08-20-2024)", "md_docs_2changelog.html#autotoc_md78", null ],
      [ "0.2.0 (07-20-2024)", "md_docs_2changelog.html#autotoc_md79", null ],
      [ "0.1.0 (05-11-2024)", "md_docs_2changelog.html#autotoc_md80", null ],
      [ "0.0.9 (04-14-2024)", "md_docs_2changelog.html#autotoc_md81", null ],
      [ "0.0.8 (02-08-2024)", "md_docs_2changelog.html#autotoc_md82", null ],
      [ "0.0.7 (01-27-2024)", "md_docs_2changelog.html#autotoc_md83", null ],
      [ "0.0.6 (11-22-2023)", "md_docs_2changelog.html#autotoc_md84", null ],
      [ "0.0.5 (11-21-2023)", "md_docs_2changelog.html#autotoc_md85", null ],
      [ "0.0.4 (11-11-2023)", "md_docs_2changelog.html#autotoc_md86", null ],
      [ "0.0.3 (10-26-2023)", "md_docs_2changelog.html#autotoc_md87", null ],
      [ "0.0.2 (08-26-2023)", "md_docs_2changelog.html#autotoc_md88", null ],
      [ "0.0.1 (07-22-2023)", "md_docs_2changelog.html#autotoc_md89", null ],
      [ "0.0.0 (06-08-2022)", "md_docs_2changelog.html#autotoc_md90", null ]
    ] ],
    [ "Contributing", "md_docs_2contributing.html", [
      [ "Style Guide", "md_docs_2contributing.html#autotoc_md92", null ],
      [ "Testing Facilities", "md_docs_2contributing.html#autotoc_md93", null ],
      [ "Key Areas of Contribution", "md_docs_2contributing.html#autotoc_md94", [
        [ "Sprite Builder System", "md_docs_2contributing.html#autotoc_md95", null ],
        [ "Emulator Subsystem", "md_docs_2contributing.html#autotoc_md96", null ]
      ] ],
      [ "Building the Project", "md_docs_2contributing.html#autotoc_md97", null ],
      [ "Getting Started", "md_docs_2contributing.html#autotoc_md98", null ],
      [ "Contributing your Changes", "md_docs_2contributing.html#autotoc_md99", null ]
    ] ],
    [ "Dungeon Editor Guide", "md_docs_2dungeon-editor-comprehensive-guide.html", [
      [ "Overview", "md_docs_2dungeon-editor-comprehensive-guide.html#autotoc_md101", null ],
      [ "Architecture", "md_docs_2dungeon-editor-comprehensive-guide.html#autotoc_md102", [
        [ "Core Components", "md_docs_2dungeon-editor-comprehensive-guide.html#autotoc_md103", [
          [ "DungeonEditorSystem", "md_docs_2dungeon-editor-comprehensive-guide.html#autotoc_md104", null ],
          [ "DungeonObjectEditor", "md_docs_2dungeon-editor-comprehensive-guide.html#autotoc_md105", null ],
          [ "ObjectRenderer", "md_docs_2dungeon-editor-comprehensive-guide.html#autotoc_md106", null ],
          [ "DungeonEditor (UI Layer)", "md_docs_2dungeon-editor-comprehensive-guide.html#autotoc_md107", null ]
        ] ]
      ] ],
      [ "Coordinate System", "md_docs_2dungeon-editor-comprehensive-guide.html#autotoc_md108", [
        [ "Room Coordinates vs Canvas Coordinates", "md_docs_2dungeon-editor-comprehensive-guide.html#autotoc_md109", [
          [ "Conversion Functions", "md_docs_2dungeon-editor-comprehensive-guide.html#autotoc_md110", null ]
        ] ],
        [ "Coordinate System Features", "md_docs_2dungeon-editor-comprehensive-guide.html#autotoc_md111", null ]
      ] ],
      [ "Object Rendering System", "md_docs_2dungeon-editor-comprehensive-guide.html#autotoc_md112", [
        [ "Object Types", "md_docs_2dungeon-editor-comprehensive-guide.html#autotoc_md113", null ],
        [ "Rendering Pipeline", "md_docs_2dungeon-editor-comprehensive-guide.html#autotoc_md114", null ],
        [ "Performance Optimizations", "md_docs_2dungeon-editor-comprehensive-guide.html#autotoc_md115", null ]
      ] ],
      [ "User Interface", "md_docs_2dungeon-editor-comprehensive-guide.html#autotoc_md116", [
        [ "Integrated Editing Panels", "md_docs_2dungeon-editor-comprehensive-guide.html#autotoc_md117", [
          [ "Main Canvas", "md_docs_2dungeon-editor-comprehensive-guide.html#autotoc_md118", null ],
          [ "Compact Editing Panels", "md_docs_2dungeon-editor-comprehensive-guide.html#autotoc_md119", null ]
        ] ],
        [ "Object Preview System", "md_docs_2dungeon-editor-comprehensive-guide.html#autotoc_md120", null ]
      ] ],
      [ "Integration with ZScream", "md_docs_2dungeon-editor-comprehensive-guide.html#autotoc_md121", [
        [ "Room Loading", "md_docs_2dungeon-editor-comprehensive-guide.html#autotoc_md122", null ],
        [ "Object Parsing", "md_docs_2dungeon-editor-comprehensive-guide.html#autotoc_md123", null ],
        [ "Coordinate System", "md_docs_2dungeon-editor-comprehensive-guide.html#autotoc_md124", null ]
      ] ],
      [ "Testing and Validation", "md_docs_2dungeon-editor-comprehensive-guide.html#autotoc_md125", [
        [ "Integration Tests", "md_docs_2dungeon-editor-comprehensive-guide.html#autotoc_md126", null ],
        [ "Test Data", "md_docs_2dungeon-editor-comprehensive-guide.html#autotoc_md127", null ],
        [ "Performance Benchmarks", "md_docs_2dungeon-editor-comprehensive-guide.html#autotoc_md128", null ]
      ] ],
      [ "Usage Examples", "md_docs_2dungeon-editor-comprehensive-guide.html#autotoc_md129", [
        [ "Basic Object Editing", "md_docs_2dungeon-editor-comprehensive-guide.html#autotoc_md130", null ],
        [ "Coordinate Conversion", "md_docs_2dungeon-editor-comprehensive-guide.html#autotoc_md131", null ],
        [ "Object Preview", "md_docs_2dungeon-editor-comprehensive-guide.html#autotoc_md132", null ]
      ] ],
      [ "Configuration Options", "md_docs_2dungeon-editor-comprehensive-guide.html#autotoc_md133", [
        [ "Editor Configuration", "md_docs_2dungeon-editor-comprehensive-guide.html#autotoc_md134", null ],
        [ "Performance Configuration", "md_docs_2dungeon-editor-comprehensive-guide.html#autotoc_md135", null ]
      ] ],
      [ "Troubleshooting", "md_docs_2dungeon-editor-comprehensive-guide.html#autotoc_md136", [
        [ "Common Issues", "md_docs_2dungeon-editor-comprehensive-guide.html#autotoc_md137", null ],
        [ "Debug Information", "md_docs_2dungeon-editor-comprehensive-guide.html#autotoc_md138", null ]
      ] ],
      [ "Future Enhancements", "md_docs_2dungeon-editor-comprehensive-guide.html#autotoc_md139", [
        [ "Planned Features", "md_docs_2dungeon-editor-comprehensive-guide.html#autotoc_md140", null ]
      ] ],
      [ "Conclusion", "md_docs_2dungeon-editor-comprehensive-guide.html#autotoc_md141", null ]
    ] ],
    [ "Dungeon Editor Design Plan", "md_docs_2dungeon-editor-design-plan.html", [
      [ "Overview", "md_docs_2dungeon-editor-design-plan.html#autotoc_md143", null ],
      [ "Architecture Overview", "md_docs_2dungeon-editor-design-plan.html#autotoc_md144", [
        [ "Core Components", "md_docs_2dungeon-editor-design-plan.html#autotoc_md145", null ],
        [ "File Structure", "md_docs_2dungeon-editor-design-plan.html#autotoc_md146", null ]
      ] ],
      [ "Component Responsibilities", "md_docs_2dungeon-editor-design-plan.html#autotoc_md147", [
        [ "DungeonEditor (Main Orchestrator)", "md_docs_2dungeon-editor-design-plan.html#autotoc_md148", null ],
        [ "DungeonRoomSelector", "md_docs_2dungeon-editor-design-plan.html#autotoc_md149", null ],
        [ "DungeonCanvasViewer", "md_docs_2dungeon-editor-design-plan.html#autotoc_md150", null ],
        [ "DungeonObjectSelector", "md_docs_2dungeon-editor-design-plan.html#autotoc_md151", null ]
      ] ],
      [ "Data Flow", "md_docs_2dungeon-editor-design-plan.html#autotoc_md152", [
        [ "Initialization Flow", "md_docs_2dungeon-editor-design-plan.html#autotoc_md153", null ],
        [ "Runtime Data Flow", "md_docs_2dungeon-editor-design-plan.html#autotoc_md154", null ],
        [ "Key Data Structures", "md_docs_2dungeon-editor-design-plan.html#autotoc_md155", null ]
      ] ],
      [ "Integration Patterns", "md_docs_2dungeon-editor-design-plan.html#autotoc_md156", [
        [ "Component Communication", "md_docs_2dungeon-editor-design-plan.html#autotoc_md157", null ],
        [ "ROM Data Management", "md_docs_2dungeon-editor-design-plan.html#autotoc_md158", null ],
        [ "State Synchronization", "md_docs_2dungeon-editor-design-plan.html#autotoc_md159", null ]
      ] ],
      [ "UI Layout Architecture", "md_docs_2dungeon-editor-design-plan.html#autotoc_md160", [
        [ "3-Column Layout", "md_docs_2dungeon-editor-design-plan.html#autotoc_md161", null ],
        [ "Component Internal Layout", "md_docs_2dungeon-editor-design-plan.html#autotoc_md162", null ]
      ] ],
      [ "Coordinate System", "md_docs_2dungeon-editor-design-plan.html#autotoc_md163", [
        [ "Room Coordinates vs Canvas Coordinates", "md_docs_2dungeon-editor-design-plan.html#autotoc_md164", null ],
        [ "Bounds Checking", "md_docs_2dungeon-editor-design-plan.html#autotoc_md165", null ]
      ] ],
      [ "Error Handling & Validation", "md_docs_2dungeon-editor-design-plan.html#autotoc_md166", [
        [ "ROM Validation", "md_docs_2dungeon-editor-design-plan.html#autotoc_md167", null ],
        [ "Bounds Validation", "md_docs_2dungeon-editor-design-plan.html#autotoc_md168", null ]
      ] ],
      [ "Performance Considerations", "md_docs_2dungeon-editor-design-plan.html#autotoc_md169", [
        [ "Caching Strategy", "md_docs_2dungeon-editor-design-plan.html#autotoc_md170", null ],
        [ "Rendering Optimization", "md_docs_2dungeon-editor-design-plan.html#autotoc_md171", null ]
      ] ],
      [ "Testing Strategy", "md_docs_2dungeon-editor-design-plan.html#autotoc_md172", [
        [ "Integration Tests", "md_docs_2dungeon-editor-design-plan.html#autotoc_md173", null ],
        [ "Test Categories", "md_docs_2dungeon-editor-design-plan.html#autotoc_md174", null ]
      ] ],
      [ "Future Development Guidelines", "md_docs_2dungeon-editor-design-plan.html#autotoc_md175", [
        [ "Adding New Features", "md_docs_2dungeon-editor-design-plan.html#autotoc_md176", null ],
        [ "Component Extension Patterns", "md_docs_2dungeon-editor-design-plan.html#autotoc_md177", null ],
        [ "Data Flow Extension", "md_docs_2dungeon-editor-design-plan.html#autotoc_md178", null ],
        [ "UI Layout Extension", "md_docs_2dungeon-editor-design-plan.html#autotoc_md179", null ]
      ] ],
      [ "Common Pitfalls & Solutions", "md_docs_2dungeon-editor-design-plan.html#autotoc_md180", [
        [ "Memory Management", "md_docs_2dungeon-editor-design-plan.html#autotoc_md181", null ],
        [ "Coordinate System", "md_docs_2dungeon-editor-design-plan.html#autotoc_md182", null ],
        [ "State Synchronization", "md_docs_2dungeon-editor-design-plan.html#autotoc_md183", null ],
        [ "Performance Issues", "md_docs_2dungeon-editor-design-plan.html#autotoc_md184", null ]
      ] ],
      [ "Debugging Tools", "md_docs_2dungeon-editor-design-plan.html#autotoc_md185", [
        [ "Debug Popup", "md_docs_2dungeon-editor-design-plan.html#autotoc_md186", null ],
        [ "Logging", "md_docs_2dungeon-editor-design-plan.html#autotoc_md187", null ]
      ] ],
      [ "Build Integration", "md_docs_2dungeon-editor-design-plan.html#autotoc_md188", [
        [ "CMake Configuration", "md_docs_2dungeon-editor-design-plan.html#autotoc_md189", null ],
        [ "Dependencies", "md_docs_2dungeon-editor-design-plan.html#autotoc_md190", null ]
      ] ],
      [ "Conclusion", "md_docs_2dungeon-editor-design-plan.html#autotoc_md191", null ]
    ] ],
    [ "Dungeon Integration Tests", "md_docs_2dungeon-integration-tests.html", [
      [ "Overview", "md_docs_2dungeon-integration-tests.html#autotoc_md193", null ],
      [ "Test Files", "md_docs_2dungeon-integration-tests.html#autotoc_md194", [
        [ "dungeon_object_renderer_integration_test.cc", "md_docs_2dungeon-integration-tests.html#autotoc_md195", null ],
        [ "dungeon_object_renderer_mock_test.cc", "md_docs_2dungeon-integration-tests.html#autotoc_md196", null ],
        [ "dungeon_editor_system_integration_test.cc", "md_docs_2dungeon-integration-tests.html#autotoc_md197", null ]
      ] ],
      [ "Test Data Sources", "md_docs_2dungeon-integration-tests.html#autotoc_md198", [
        [ "Real ROM Data", "md_docs_2dungeon-integration-tests.html#autotoc_md199", null ],
        [ "Disassembly Integration", "md_docs_2dungeon-integration-tests.html#autotoc_md200", null ]
      ] ],
      [ "Running the Tests", "md_docs_2dungeon-integration-tests.html#autotoc_md201", [
        [ "Prerequisites", "md_docs_2dungeon-integration-tests.html#autotoc_md202", null ],
        [ "Command Line", "md_docs_2dungeon-integration-tests.html#autotoc_md203", null ],
        [ "CI/CD Considerations", "md_docs_2dungeon-integration-tests.html#autotoc_md204", null ]
      ] ],
      [ "Test Coverage", "md_docs_2dungeon-integration-tests.html#autotoc_md205", [
        [ "Object Rendering", "md_docs_2dungeon-integration-tests.html#autotoc_md206", null ],
        [ "Dungeon Editor System", "md_docs_2dungeon-integration-tests.html#autotoc_md207", null ],
        [ "Integration Features", "md_docs_2dungeon-integration-tests.html#autotoc_md208", null ]
      ] ],
      [ "Performance Benchmarks", "md_docs_2dungeon-integration-tests.html#autotoc_md209", null ],
      [ "Validation Points", "md_docs_2dungeon-integration-tests.html#autotoc_md210", [
        [ "ROM Integrity", "md_docs_2dungeon-integration-tests.html#autotoc_md211", null ],
        [ "Object Data", "md_docs_2dungeon-integration-tests.html#autotoc_md212", null ],
        [ "System Integration", "md_docs_2dungeon-integration-tests.html#autotoc_md213", null ]
      ] ],
      [ "Debugging and Troubleshooting", "md_docs_2dungeon-integration-tests.html#autotoc_md214", [
        [ "Common Issues", "md_docs_2dungeon-integration-tests.html#autotoc_md215", null ],
        [ "Debug Output", "md_docs_2dungeon-integration-tests.html#autotoc_md216", null ]
      ] ],
      [ "Extending the Tests", "md_docs_2dungeon-integration-tests.html#autotoc_md217", [
        [ "Adding New Tests", "md_docs_2dungeon-integration-tests.html#autotoc_md218", null ],
        [ "Adding New Mock Data", "md_docs_2dungeon-integration-tests.html#autotoc_md219", null ],
        [ "Adding New Validation", "md_docs_2dungeon-integration-tests.html#autotoc_md220", null ]
      ] ],
      [ "Maintenance", "md_docs_2dungeon-integration-tests.html#autotoc_md221", [
        [ "Regular Updates", "md_docs_2dungeon-integration-tests.html#autotoc_md222", null ],
        [ "Monitoring", "md_docs_2dungeon-integration-tests.html#autotoc_md223", null ]
      ] ]
    ] ],
    [ "Getting Started", "md_docs_2getting-started.html", [
      [ "General Tips", "md_docs_2getting-started.html#autotoc_md225", null ],
      [ "Extending Functionality", "md_docs_2getting-started.html#autotoc_md226", null ],
      [ "Supported Features", "md_docs_2getting-started.html#autotoc_md227", null ],
      [ "Command Line Interface", "md_docs_2getting-started.html#autotoc_md228", null ]
    ] ],
    [ "Yaze Documentation", "md_docs_2index.html", [
      [ "Quick Start", "md_docs_2index.html#autotoc_md230", null ],
      [ "Core Documentation", "md_docs_2index.html#autotoc_md231", [
        [ "Architecture & Infrastructure", "md_docs_2index.html#autotoc_md232", null ],
        [ "Editors", "md_docs_2index.html#autotoc_md233", [
          [ "Dungeon Editor", "md_docs_2index.html#autotoc_md234", null ],
          [ "Overworld Editor", "md_docs_2index.html#autotoc_md235", null ]
        ] ],
        [ "Graphics & UI", "md_docs_2index.html#autotoc_md236", null ],
        [ "Testing", "md_docs_2index.html#autotoc_md237", null ]
      ] ],
      [ "Project Status", "md_docs_2index.html#autotoc_md238", [
        [ "Current Version: 0.2.2 (12-31-2024)", "md_docs_2index.html#autotoc_md239", [
          [ "✅ Completed Features", "md_docs_2index.html#autotoc_md240", null ],
          [ "🔄 In Progress", "md_docs_2index.html#autotoc_md241", null ],
          [ "⏳ Planned", "md_docs_2index.html#autotoc_md242", null ]
        ] ]
      ] ],
      [ "Key Features", "md_docs_2index.html#autotoc_md243", [
        [ "Dungeon Editing", "md_docs_2index.html#autotoc_md244", null ],
        [ "Overworld Editing", "md_docs_2index.html#autotoc_md245", null ],
        [ "Graphics System", "md_docs_2index.html#autotoc_md246", null ]
      ] ],
      [ "Compatibility", "md_docs_2index.html#autotoc_md247", [
        [ "ROM Support", "md_docs_2index.html#autotoc_md248", null ],
        [ "Platform Support", "md_docs_2index.html#autotoc_md249", null ]
      ] ],
      [ "Development", "md_docs_2index.html#autotoc_md250", [
        [ "Architecture", "md_docs_2index.html#autotoc_md251", null ],
        [ "Contributing", "md_docs_2index.html#autotoc_md252", null ]
      ] ],
      [ "Community", "md_docs_2index.html#autotoc_md253", null ],
      [ "License", "md_docs_2index.html#autotoc_md254", null ]
    ] ],
    [ "Infrastructure Overview", "md_docs_2infrastructure.html", [
      [ "Targets", "md_docs_2infrastructure.html#autotoc_md257", null ],
      [ "Directory Structure", "md_docs_2infrastructure.html#autotoc_md258", null ],
      [ "Dependencies", "md_docs_2infrastructure.html#autotoc_md259", null ],
      [ "Flow of Control", "md_docs_2infrastructure.html#autotoc_md260", null ],
      [ "Rom", "md_docs_2infrastructure.html#autotoc_md261", null ],
      [ "Bitmap", "md_docs_2infrastructure.html#autotoc_md262", null ]
    ] ],
    [ "Integration Test Guide", "md_docs_2integration__test__guide.html", [
      [ "Table of Contents", "md_docs_2integration__test__guide.html#autotoc_md264", null ],
      [ "Overview", "md_docs_2integration__test__guide.html#autotoc_md265", [
        [ "Key Components", "md_docs_2integration__test__guide.html#autotoc_md266", null ]
      ] ],
      [ "Test Structure", "md_docs_2integration__test__guide.html#autotoc_md267", [
        [ "Test Files", "md_docs_2integration__test__guide.html#autotoc_md268", null ],
        [ "Test Categories", "md_docs_2integration__test__guide.html#autotoc_md269", [
          [ "Integration Tests (overworld_integration_test.cc)", "md_docs_2integration__test__guide.html#autotoc_md270", null ],
          [ "Comprehensive Integration Tests (comprehensive_integration_test.cc)", "md_docs_2integration__test__guide.html#autotoc_md271", null ],
          [ "Dungeon Integration Tests (dungeon_integration_test.cc)", "md_docs_2integration__test__guide.html#autotoc_md272", null ],
          [ "Dungeon Editor System Tests (dungeon_editor_system_integration_test.cc)", "md_docs_2integration__test__guide.html#autotoc_md273", null ]
        ] ]
      ] ],
      [ "Running Tests", "md_docs_2integration__test__guide.html#autotoc_md274", [
        [ "Prerequisites", "md_docs_2integration__test__guide.html#autotoc_md275", null ],
        [ "Basic Test Execution", "md_docs_2integration__test__guide.html#autotoc_md276", null ],
        [ "Test Filtering", "md_docs_2integration__test__guide.html#autotoc_md277", null ],
        [ "Parallel Execution", "md_docs_2integration__test__guide.html#autotoc_md278", null ]
      ] ],
      [ "Understanding Results", "md_docs_2integration__test__guide.html#autotoc_md279", [
        [ "Test Output Format", "md_docs_2integration__test__guide.html#autotoc_md280", null ],
        [ "Success Indicators", "md_docs_2integration__test__guide.html#autotoc_md281", null ],
        [ "Failure Indicators", "md_docs_2integration__test__guide.html#autotoc_md282", null ],
        [ "Example Failure Output", "md_docs_2integration__test__guide.html#autotoc_md283", null ]
      ] ],
      [ "Adding New Tests", "md_docs_2integration__test__guide.html#autotoc_md284", [
        [ "Unit Test Example", "md_docs_2integration__test__guide.html#autotoc_md285", null ],
        [ "Integration Test Example", "md_docs_2integration__test__guide.html#autotoc_md286", null ],
        [ "Performance Test Example", "md_docs_2integration__test__guide.html#autotoc_md287", null ]
      ] ],
      [ "Debugging Failed Tests", "md_docs_2integration__test__guide.html#autotoc_md288", [
        [ "Enable Debug Output", "md_docs_2integration__test__guide.html#autotoc_md289", null ],
        [ "Use GDB for Debugging", "md_docs_2integration__test__guide.html#autotoc_md290", null ],
        [ "Memory Debugging", "md_docs_2integration__test__guide.html#autotoc_md291", null ],
        [ "Common Debugging Scenarios", "md_docs_2integration__test__guide.html#autotoc_md292", [
          [ "ROM Loading Issues", "md_docs_2integration__test__guide.html#autotoc_md293", null ],
          [ "Map Property Issues", "md_docs_2integration__test__guide.html#autotoc_md294", null ],
          [ "Sprite Issues", "md_docs_2integration__test__guide.html#autotoc_md295", null ]
        ] ]
      ] ],
      [ "Best Practices", "md_docs_2integration__test__guide.html#autotoc_md296", [
        [ "Test Organization", "md_docs_2integration__test__guide.html#autotoc_md297", null ],
        [ "Test Data Management", "md_docs_2integration__test__guide.html#autotoc_md298", null ],
        [ "Error Handling in Tests", "md_docs_2integration__test__guide.html#autotoc_md299", null ],
        [ "Performance Considerations", "md_docs_2integration__test__guide.html#autotoc_md300", null ],
        [ "Continuous Integration", "md_docs_2integration__test__guide.html#autotoc_md301", null ]
      ] ],
      [ "Test Results Interpretation", "md_docs_2integration__test__guide.html#autotoc_md302", [
        [ "Viewing Results", "md_docs_2integration__test__guide.html#autotoc_md303", null ],
        [ "Key Metrics", "md_docs_2integration__test__guide.html#autotoc_md304", null ],
        [ "Performance Benchmarks", "md_docs_2integration__test__guide.html#autotoc_md305", null ]
      ] ],
      [ "Conclusion", "md_docs_2integration__test__guide.html#autotoc_md306", null ]
    ] ],
    [ "Overworld Loading Guide", "md_docs_2overworld__loading__guide.html", [
      [ "Table of Contents", "md_docs_2overworld__loading__guide.html#autotoc_md308", null ],
      [ "Overview", "md_docs_2overworld__loading__guide.html#autotoc_md309", null ],
      [ "ROM Types and Versions", "md_docs_2overworld__loading__guide.html#autotoc_md310", [
        [ "Version Detection", "md_docs_2overworld__loading__guide.html#autotoc_md311", null ],
        [ "Feature Support by Version", "md_docs_2overworld__loading__guide.html#autotoc_md312", null ]
      ] ],
      [ "Overworld Map Structure", "md_docs_2overworld__loading__guide.html#autotoc_md313", [
        [ "Core Properties", "md_docs_2overworld__loading__guide.html#autotoc_md314", null ]
      ] ],
      [ "Overlays and Special Area Maps", "md_docs_2overworld__loading__guide.html#autotoc_md315", [
        [ "Understanding Overlays", "md_docs_2overworld__loading__guide.html#autotoc_md316", null ],
        [ "Special Area Maps (0x80-0x9F)", "md_docs_2overworld__loading__guide.html#autotoc_md317", null ],
        [ "Overlay ID Mappings", "md_docs_2overworld__loading__guide.html#autotoc_md318", null ],
        [ "Drawing Order", "md_docs_2overworld__loading__guide.html#autotoc_md319", null ],
        [ "Vanilla Overlay Loading", "md_docs_2overworld__loading__guide.html#autotoc_md320", null ],
        [ "Special Area Graphics Loading", "md_docs_2overworld__loading__guide.html#autotoc_md321", null ]
      ] ],
      [ "Loading Process", "md_docs_2overworld__loading__guide.html#autotoc_md322", [
        [ "Version Detection", "md_docs_2overworld__loading__guide.html#autotoc_md323", null ],
        [ "Map Initialization", "md_docs_2overworld__loading__guide.html#autotoc_md324", null ],
        [ "Property Loading", "md_docs_2overworld__loading__guide.html#autotoc_md325", [
          [ "Vanilla ROMs (asm_version == 0xFF)", "md_docs_2overworld__loading__guide.html#autotoc_md326", null ],
          [ "ZSCustomOverworld v2/v3", "md_docs_2overworld__loading__guide.html#autotoc_md327", null ]
        ] ],
        [ "Custom Data Loading", "md_docs_2overworld__loading__guide.html#autotoc_md328", null ]
      ] ],
      [ "ZScream Implementation", "md_docs_2overworld__loading__guide.html#autotoc_md329", [
        [ "OverworldMap Constructor", "md_docs_2overworld__loading__guide.html#autotoc_md330", null ],
        [ "Key Methods", "md_docs_2overworld__loading__guide.html#autotoc_md331", null ]
      ] ],
      [ "Yaze Implementation", "md_docs_2overworld__loading__guide.html#autotoc_md332", [
        [ "OverworldMap Constructor", "md_docs_2overworld__loading__guide.html#autotoc_md333", null ],
        [ "Key Methods", "md_docs_2overworld__loading__guide.html#autotoc_md334", null ],
        [ "Current Status", "md_docs_2overworld__loading__guide.html#autotoc_md335", null ]
      ] ],
      [ "Key Differences", "md_docs_2overworld__loading__guide.html#autotoc_md336", [
        [ "Language and Architecture", "md_docs_2overworld__loading__guide.html#autotoc_md337", null ],
        [ "Data Structures", "md_docs_2overworld__loading__guide.html#autotoc_md338", null ],
        [ "Error Handling", "md_docs_2overworld__loading__guide.html#autotoc_md339", null ],
        [ "Graphics Processing", "md_docs_2overworld__loading__guide.html#autotoc_md340", null ]
      ] ],
      [ "Common Issues and Solutions", "md_docs_2overworld__loading__guide.html#autotoc_md341", [
        [ "Version Detection Issues", "md_docs_2overworld__loading__guide.html#autotoc_md342", null ],
        [ "Palette Loading Errors", "md_docs_2overworld__loading__guide.html#autotoc_md343", null ],
        [ "Graphics Not Loading", "md_docs_2overworld__loading__guide.html#autotoc_md344", null ],
        [ "Overlay Issues", "md_docs_2overworld__loading__guide.html#autotoc_md345", null ],
        [ "Large Map Problems", "md_docs_2overworld__loading__guide.html#autotoc_md346", null ],
        [ "Special Area Graphics Issues", "md_docs_2overworld__loading__guide.html#autotoc_md347", null ]
      ] ],
      [ "Best Practices", "md_docs_2overworld__loading__guide.html#autotoc_md348", [
        [ "Version-Specific Code", "md_docs_2overworld__loading__guide.html#autotoc_md349", null ],
        [ "Error Handling", "md_docs_2overworld__loading__guide.html#autotoc_md350", null ],
        [ "Memory Management", "md_docs_2overworld__loading__guide.html#autotoc_md351", null ],
        [ "Thread Safety", "md_docs_2overworld__loading__guide.html#autotoc_md352", null ]
      ] ],
      [ "Conclusion", "md_docs_2overworld__loading__guide.html#autotoc_md353", null ]
    ] ],
    [ "Roadmap", "md_docs_2roadmap.html", [
      [ "0.2.X (Current)", "md_docs_2roadmap.html#autotoc_md355", null ],
      [ "0.3.X", "md_docs_2roadmap.html#autotoc_md356", null ],
      [ "0.4.X", "md_docs_2roadmap.html#autotoc_md357", null ],
      [ "0.5.X", "md_docs_2roadmap.html#autotoc_md358", null ],
      [ "0.6.X", "md_docs_2roadmap.html#autotoc_md359", null ],
      [ "0.7.X", "md_docs_2roadmap.html#autotoc_md360", null ],
      [ "0.8.X", "md_docs_2roadmap.html#autotoc_md361", null ],
      [ "0.9.X", "md_docs_2roadmap.html#autotoc_md362", null ],
      [ "1.0.0", "md_docs_2roadmap.html#autotoc_md363", null ]
    ] ],
    [ "Yet Another Zelda3 Editor", "md_README.html", [
      [ "Description", "md_README.html#autotoc_md365", null ],
      [ "Building and installation", "md_README.html#autotoc_md366", null ],
      [ "Documentation", "md_README.html#autotoc_md367", [
        [ "Key Documentation", "md_README.html#autotoc_md368", null ]
      ] ],
      [ "License", "md_README.html#autotoc_md369", null ],
      [ "Screenshots", "md_README.html#autotoc_md370", null ]
    ] ],
    [ "Todo List", "todo.html", null ],
    [ "Deprecated List", "deprecated.html", null ],
    [ "Test List", "test.html", null ],
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
"classyaze_1_1RecentFilesManager.html#ac65ad1734fba8871faf2279510da4666",
"classyaze_1_1cli_1_1ReadFromRom.html",
"classyaze_1_1editor_1_1DungeonEditor.html#a11bf609e95fc11dcf8d7540e1ac47848",
"classyaze_1_1editor_1_1DungeonObjectSelector.html#a61e0d3f7d3d9cc67d4e86f291c7b50a9",
"classyaze_1_1editor_1_1EditorManager.html#ae9a431666f10d2734518acc911e504b7",
"classyaze_1_1editor_1_1GraphicsEditor.html#ab4f36209b7be085caa67ed5368523601",
"classyaze_1_1editor_1_1MusicEditor.html#a9516d58ac2dc4bf39c503b4f48374195",
"classyaze_1_1editor_1_1OverworldEditor.html#ae90149bd11c7e73da9d1e946764869c1",
"classyaze_1_1editor_1_1ScreenEditor.html#ab8e905b4affccdccaa214328ba8773ff",
"classyaze_1_1editor_1_1Tile16Editor.html#ae8cadf950dd10d0fe608925fcfedb65d",
"classyaze_1_1emu_1_1Cpu.html#a384e5451767eb095f526360259321012",
"classyaze_1_1emu_1_1Cpu.html#ac28d253c8960201e66fd272cc976eabb",
"classyaze_1_1emu_1_1Emulator.html#a8532516d071862efe4ad42f9e6eca403",
"classyaze_1_1emu_1_1MemoryImpl.html#aee42c803ab884f48aed70728cbad01e5",
"classyaze_1_1emu_1_1Ppu.html#a98a814b5ac9c6822c13f74682615eb3e",
"classyaze_1_1emu_1_1Spc700.html#a102409358a9d477d86e300c8445f27af",
"classyaze_1_1emu_1_1Spc700.html#af5e8217ac6bbcbc8487a48a35627a462",
"classyaze_1_1gfx_1_1SnesColor.html#a0abbae9b26b066a00a98afb18175623b",
"classyaze_1_1gui_1_1Bitmap.html#ae3d7a292bdf5c64aa11b34efa13fa38c",
"classyaze_1_1gui_1_1Renderer.html",
"classyaze_1_1test_1_1Cpu.html#a46d1550e1ff864e71f022d5b38979bd5",
"classyaze_1_1test_1_1Cpu.html#acbdbeaa61ec026582d69a2558c9b14dd",
"classyaze_1_1test_1_1MemoryImpl.html#a0e1947d5642b0a24b83ec980238a6bc1",
"classyaze_1_1test_1_1MockPpu.html#a78f763ebaf1616b77a7cb175e7cccc90",
"classyaze_1_1test_1_1Spc700.html#a16b76a437f9574f5981f4c46d907ee96",
"classyaze_1_1test_1_1Spc700Test.html#a570104843f19420f7cb77e38b5d3ebbc",
"classyaze_1_1zelda3_1_1DungeonEditorSystem.html#a3a5a44df3b1b4d6ead99a4b5f1630555a8731796709e7e0bc210e8e3cf68ce75d",
"classyaze_1_1zelda3_1_1DungeonEditorSystem.html#aec19796a9a385dfdf48942bfa39fc73a",
"classyaze_1_1zelda3_1_1DungeonObjectEditor.html#ae97bf7aafced76a3b14852cfb7f372de",
"classyaze_1_1zelda3_1_1ObjectRenderer.html#a98886c59ff899c70af8414a35c86837a",
"classyaze_1_1zelda3_1_1Overworld.html#a923a256c45ae441f308cceb87b3a7a98",
"classyaze_1_1zelda3_1_1OverworldMap.html#a5c3c401192c1c424cc8fc4c9f99d9e42",
"classyaze_1_1zelda3_1_1Room.html#a395788339cd3b480bbc18deaf32f1989",
"classyaze_1_1zelda3_1_1RoomLayout.html#a41db8a3a025e5bd1c7bf0d8dd73f4277",
"classyaze_1_1zelda3_1_1Sprite.html#aabb4093f4869708cb74d1fb2d6d7176e",
"classyaze_1_1zelda3_1_1music_1_1Tracker.html#a169d9ecb5a18433f6f757eebdd0df0ec",
"editor__manager_8cc_source.html",
"icons_8h.html#a067f691215cc664a66375a01f8aa5b8b",
"icons_8h.html#a259e37e9c222103770ea434add08107e",
"icons_8h.html#a42a9989eaf1822be4cb4b071eec53ba6",
"icons_8h.html#a5fda85dd85890c0e4ec00970aa4c4c10",
"icons_8h.html#a7f97aaac06d898495d89144ffdc17ca7",
"icons_8h.html#a9ec9a3d37064276a123822d6d273d960",
"icons_8h.html#abb7ad78d91c1686e3c0348fa9a8ba3a4",
"icons_8h.html#ad7afac6c47209ab965bd940932e516b7",
"icons_8h.html#af234e3035942bc14bdbc46310e8dccea",
"md_docs_2canvas-refactor-summary.html#autotoc_md64",
"md_docs_2overworld__loading__guide.html#autotoc_md315",
"namespaceyaze_1_1editor.html#a1d8e24b671e1338c02fbe726de12a0a3",
"namespaceyaze_1_1emu.html#a974daf65d141915c648ccb83aa84a72a",
"namespaceyaze_1_1gfx_1_1lc__lz2.html#af30dabbc0b3b901acecf2f990f31f63a",
"namespaceyaze_1_1test.html#a37544f7db70b1c35ee45a442b094d292",
"namespaceyaze_1_1test.html#abb1ba7436bcb95504eeda3a291345f35",
"namespaceyaze_1_1zelda3.html#a39eac68a666553cc277c9ed988be9675",
"namespaceyaze_1_1zelda3.html#a955976632b4be13438c7fd3992380788",
"object__parser_8h_source.html",
"structyaze_1_1Project.html#a37390efefd036cff957a29bf5241f9e0",
"structyaze_1_1editor_1_1EditorContext.html#a8f551fe84f6741f854d00eb2044c417c",
"structyaze_1_1editor_1_1zsprite_1_1AnimationGroup.html#afa0f3ea57d1a2c693e1b1567c5498b89",
"structyaze_1_1emu_1_1CGWSEL.html",
"structyaze_1_1emu_1_1M7B.html#a74d755758fd2cbdbab678851ea03f7f4",
"structyaze_1_1emu_1_1SpriteAttributes.html#aa2f9a75ed1ef5be94661376b284cfcc7",
"structyaze_1_1emu_1_1WindowPosition.html#a1af2e3e67ce5d35a0734cc33ecb785bc",
"structyaze_1_1gui_1_1GfxSheetAssetBrowser.html#a3dd4034a7413ec8239c99bcd96a770e6",
"structyaze_1_1test_1_1CompressionContext.html#afd1da16d17deec3d4fb2876490c1dadf",
"structyaze_1_1zelda3_1_1DungeonEditorSystem_1_1RoomProperties.html#a978156219f8e0a27d6aa1b84721a46b0",
"structyaze_1_1zelda3_1_1ObjectRenderer_1_1PerformanceStats.html#a80009955b094cddf218521a914a981ee",
"structyaze_1_1zelda3_1_1music_1_1SpcCommand.html#ac77e1929cfdff13a3d3772b95b8270ed",
"zelda_8h.html#a9ecfc19156f8f627881db2f6c16190ef"
];

var SYNCONMSG = 'click to disable panel synchronization';
var SYNCOFFMSG = 'click to enable panel synchronization';