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
    [ "Getting Started", "md_docs_201-getting-started.html", [
      [ "Quick Start", "md_docs_201-getting-started.html#autotoc_md1", null ],
      [ "General Tips", "md_docs_201-getting-started.html#autotoc_md2", null ],
      [ "Supported Features", "md_docs_201-getting-started.html#autotoc_md3", null ],
      [ "Command Line Interface", "md_docs_201-getting-started.html#autotoc_md4", null ],
      [ "Extending Functionality", "md_docs_201-getting-started.html#autotoc_md5", null ]
    ] ],
    [ "Build Instructions", "md_docs_202-build-instructions.html", [
      [ "Quick Build", "md_docs_202-build-instructions.html#autotoc_md7", null ],
      [ "Dependencies", "md_docs_202-build-instructions.html#autotoc_md8", [
        [ "Core Dependencies", "md_docs_202-build-instructions.html#autotoc_md9", null ],
        [ "v0.3.0 Additions", "md_docs_202-build-instructions.html#autotoc_md10", null ]
      ] ],
      [ "Platform-Specific Setup", "md_docs_202-build-instructions.html#autotoc_md11", [
        [ "Windows", "md_docs_202-build-instructions.html#autotoc_md12", [
          [ "vcpkg (Recommended)", "md_docs_202-build-instructions.html#autotoc_md13", null ],
          [ "msys2 (Advanced)", "md_docs_202-build-instructions.html#autotoc_md14", null ]
        ] ],
        [ "macOS", "md_docs_202-build-instructions.html#autotoc_md15", null ],
        [ "Linux", "md_docs_202-build-instructions.html#autotoc_md16", null ],
        [ "iOS", "md_docs_202-build-instructions.html#autotoc_md17", null ]
      ] ],
      [ "Build Targets", "md_docs_202-build-instructions.html#autotoc_md18", null ]
    ] ],
    [ "Asar 65816 Assembler Integration", "md_docs_203-asar-integration.html", [
      [ "Quick Examples", "md_docs_203-asar-integration.html#autotoc_md20", [
        [ "Command Line", "md_docs_203-asar-integration.html#autotoc_md21", null ],
        [ "C++ API", "md_docs_203-asar-integration.html#autotoc_md22", null ]
      ] ],
      [ "Assembly Patch Examples", "md_docs_203-asar-integration.html#autotoc_md23", [
        [ "Basic Hook", "md_docs_203-asar-integration.html#autotoc_md24", null ],
        [ "Advanced Features", "md_docs_203-asar-integration.html#autotoc_md25", null ]
      ] ],
      [ "API Reference", "md_docs_203-asar-integration.html#autotoc_md26", [
        [ "AsarWrapper Class", "md_docs_203-asar-integration.html#autotoc_md27", null ],
        [ "Data Structures", "md_docs_203-asar-integration.html#autotoc_md28", null ]
      ] ],
      [ "Testing", "md_docs_203-asar-integration.html#autotoc_md29", [
        [ "ROM-Dependent Tests", "md_docs_203-asar-integration.html#autotoc_md30", null ]
      ] ],
      [ "Error Handling", "md_docs_203-asar-integration.html#autotoc_md31", null ],
      [ "Development Workflow", "md_docs_203-asar-integration.html#autotoc_md32", null ]
    ] ],
    [ "API Reference", "md_docs_204-api-reference.html", [
      [ "C API (incl/yaze.h, incl/zelda.h)", "md_docs_204-api-reference.html#autotoc_md34", [
        [ "Core Library Functions", "md_docs_204-api-reference.html#autotoc_md35", null ],
        [ "ROM Operations", "md_docs_204-api-reference.html#autotoc_md36", null ],
        [ "Graphics Operations", "md_docs_204-api-reference.html#autotoc_md37", null ],
        [ "Palette System", "md_docs_204-api-reference.html#autotoc_md38", null ],
        [ "Message System", "md_docs_204-api-reference.html#autotoc_md39", null ]
      ] ],
      [ "C++ API", "md_docs_204-api-reference.html#autotoc_md40", [
        [ "AsarWrapper (src/app/core/asar_wrapper.h)", "md_docs_204-api-reference.html#autotoc_md41", null ],
        [ "Data Structures", "md_docs_204-api-reference.html#autotoc_md42", [
          [ "ROM Version Support", "md_docs_204-api-reference.html#autotoc_md43", null ],
          [ "SNES Graphics", "md_docs_204-api-reference.html#autotoc_md44", null ],
          [ "Message System", "md_docs_204-api-reference.html#autotoc_md45", null ]
        ] ]
      ] ],
      [ "Error Handling", "md_docs_204-api-reference.html#autotoc_md46", [
        [ "Status Codes", "md_docs_204-api-reference.html#autotoc_md47", null ],
        [ "Error Handling Pattern", "md_docs_204-api-reference.html#autotoc_md48", null ]
      ] ],
      [ "Extension System", "md_docs_204-api-reference.html#autotoc_md49", [
        [ "Plugin Architecture", "md_docs_204-api-reference.html#autotoc_md50", null ],
        [ "Capability Flags", "md_docs_204-api-reference.html#autotoc_md51", null ]
      ] ],
      [ "Backward Compatibility", "md_docs_204-api-reference.html#autotoc_md52", null ]
    ] ],
    [ "Testing Guide", "md_docs_2A1-testing-guide.html", [
      [ "Test Categories", "md_docs_2A1-testing-guide.html#autotoc_md54", [
        [ "Stable Tests (STABLE)", "md_docs_2A1-testing-guide.html#autotoc_md55", null ],
        [ "ROM-Dependent Tests (ROM_DEPENDENT)", "md_docs_2A1-testing-guide.html#autotoc_md56", null ],
        [ "Experimental Tests (EXPERIMENTAL)", "md_docs_2A1-testing-guide.html#autotoc_md57", null ]
      ] ],
      [ "Command Line Usage", "md_docs_2A1-testing-guide.html#autotoc_md58", null ],
      [ "CMake Presets", "md_docs_2A1-testing-guide.html#autotoc_md59", null ],
      [ "Writing Tests", "md_docs_2A1-testing-guide.html#autotoc_md60", [
        [ "Stable Tests", "md_docs_2A1-testing-guide.html#autotoc_md61", null ],
        [ "ROM-Dependent Tests", "md_docs_2A1-testing-guide.html#autotoc_md62", null ],
        [ "Experimental Tests", "md_docs_2A1-testing-guide.html#autotoc_md63", null ]
      ] ],
      [ "CI/CD Integration", "md_docs_2A1-testing-guide.html#autotoc_md64", [
        [ "GitHub Actions", "md_docs_2A1-testing-guide.html#autotoc_md65", null ],
        [ "Test Execution Strategy", "md_docs_2A1-testing-guide.html#autotoc_md66", null ]
      ] ],
      [ "Test Development Guidelines", "md_docs_2A1-testing-guide.html#autotoc_md67", [
        [ "Writing Stable Tests", "md_docs_2A1-testing-guide.html#autotoc_md68", null ],
        [ "Writing ROM-Dependent Tests", "md_docs_2A1-testing-guide.html#autotoc_md69", null ],
        [ "Writing Experimental Tests", "md_docs_2A1-testing-guide.html#autotoc_md70", null ]
      ] ],
      [ "Performance and Maintenance", "md_docs_2A1-testing-guide.html#autotoc_md71", [
        [ "Regular Review", "md_docs_2A1-testing-guide.html#autotoc_md72", null ],
        [ "Performance Monitoring", "md_docs_2A1-testing-guide.html#autotoc_md73", null ]
      ] ]
    ] ],
    [ "Contributing", "md_docs_2B1-contributing.html", [
      [ "Development Setup", "md_docs_2B1-contributing.html#autotoc_md75", [
        [ "Prerequisites", "md_docs_2B1-contributing.html#autotoc_md76", null ],
        [ "Quick Start", "md_docs_2B1-contributing.html#autotoc_md77", null ]
      ] ],
      [ "Code Style", "md_docs_2B1-contributing.html#autotoc_md78", [
        [ "C++ Standards", "md_docs_2B1-contributing.html#autotoc_md79", null ],
        [ "File Organization", "md_docs_2B1-contributing.html#autotoc_md80", null ],
        [ "Error Handling", "md_docs_2B1-contributing.html#autotoc_md81", null ]
      ] ],
      [ "Testing Requirements", "md_docs_2B1-contributing.html#autotoc_md82", [
        [ "Test Categories", "md_docs_2B1-contributing.html#autotoc_md83", null ],
        [ "Writing Tests", "md_docs_2B1-contributing.html#autotoc_md84", null ],
        [ "Test Execution", "md_docs_2B1-contributing.html#autotoc_md85", null ]
      ] ],
      [ "Pull Request Process", "md_docs_2B1-contributing.html#autotoc_md86", [
        [ "Before Submitting", "md_docs_2B1-contributing.html#autotoc_md87", null ],
        [ "Pull Request Template", "md_docs_2B1-contributing.html#autotoc_md88", null ]
      ] ],
      [ "Development Workflow", "md_docs_2B1-contributing.html#autotoc_md89", [
        [ "Branch Strategy", "md_docs_2B1-contributing.html#autotoc_md90", null ],
        [ "Commit Messages", "md_docs_2B1-contributing.html#autotoc_md91", null ],
        [ "Types", "md_docs_2B1-contributing.html#autotoc_md92", null ]
      ] ],
      [ "Architecture Guidelines", "md_docs_2B1-contributing.html#autotoc_md93", [
        [ "Component Design", "md_docs_2B1-contributing.html#autotoc_md94", null ],
        [ "Memory Management", "md_docs_2B1-contributing.html#autotoc_md95", null ],
        [ "Performance", "md_docs_2B1-contributing.html#autotoc_md96", null ]
      ] ],
      [ "Documentation", "md_docs_2B1-contributing.html#autotoc_md97", [
        [ "Code Documentation", "md_docs_2B1-contributing.html#autotoc_md98", null ],
        [ "API Documentation", "md_docs_2B1-contributing.html#autotoc_md99", null ]
      ] ],
      [ "Community Guidelines", "md_docs_2B1-contributing.html#autotoc_md100", [
        [ "Communication", "md_docs_2B1-contributing.html#autotoc_md101", null ],
        [ "Getting Help", "md_docs_2B1-contributing.html#autotoc_md102", null ]
      ] ],
      [ "Release Process", "md_docs_2B1-contributing.html#autotoc_md103", [
        [ "Version Numbering", "md_docs_2B1-contributing.html#autotoc_md104", null ],
        [ "Release Checklist", "md_docs_2B1-contributing.html#autotoc_md105", null ]
      ] ]
    ] ],
    [ "Changelog", "md_docs_2C1-changelog.html", [
      [ "0.3.0 (January 2025)", "md_docs_2C1-changelog.html#autotoc_md107", [
        [ "Major Features", "md_docs_2C1-changelog.html#autotoc_md108", null ],
        [ "Enhancements", "md_docs_2C1-changelog.html#autotoc_md109", null ],
        [ "Technical Improvements", "md_docs_2C1-changelog.html#autotoc_md110", null ],
        [ "Bug Fixes", "md_docs_2C1-changelog.html#autotoc_md111", null ]
      ] ],
      [ "0.2.2 (December 2024)", "md_docs_2C1-changelog.html#autotoc_md112", null ],
      [ "0.2.1 (August 2024)", "md_docs_2C1-changelog.html#autotoc_md113", null ],
      [ "0.2.0 (July 2024)", "md_docs_2C1-changelog.html#autotoc_md114", null ],
      [ "0.1.0 (May 2024)", "md_docs_2C1-changelog.html#autotoc_md115", null ],
      [ "0.0.9 (April 2024)", "md_docs_2C1-changelog.html#autotoc_md116", null ],
      [ "0.0.8 (February 2024)", "md_docs_2C1-changelog.html#autotoc_md117", null ],
      [ "0.0.7 (January 2024)", "md_docs_2C1-changelog.html#autotoc_md118", null ],
      [ "0.0.6 (November 2023)", "md_docs_2C1-changelog.html#autotoc_md119", null ],
      [ "0.0.5 (November 2023)", "md_docs_2C1-changelog.html#autotoc_md120", null ],
      [ "0.0.4 (November 2023)", "md_docs_2C1-changelog.html#autotoc_md121", null ],
      [ "0.0.3 (October 2023)", "md_docs_2C1-changelog.html#autotoc_md122", null ]
    ] ],
    [ "Roadmap", "md_docs_2D1-roadmap.html", [
      [ "0.4.X (Next Major Release)", "md_docs_2D1-roadmap.html#autotoc_md124", [
        [ "Core Features", "md_docs_2D1-roadmap.html#autotoc_md125", null ],
        [ "Technical Improvements", "md_docs_2D1-roadmap.html#autotoc_md126", null ]
      ] ],
      [ "0.5.X", "md_docs_2D1-roadmap.html#autotoc_md127", [
        [ "Advanced Features", "md_docs_2D1-roadmap.html#autotoc_md128", null ]
      ] ],
      [ "0.6.X", "md_docs_2D1-roadmap.html#autotoc_md129", [
        [ "Platform & Integration", "md_docs_2D1-roadmap.html#autotoc_md130", null ]
      ] ],
      [ "0.7.X", "md_docs_2D1-roadmap.html#autotoc_md131", [
        [ "Performance & Polish", "md_docs_2D1-roadmap.html#autotoc_md132", null ]
      ] ],
      [ "0.8.X", "md_docs_2D1-roadmap.html#autotoc_md133", [
        [ "Beta Preparation", "md_docs_2D1-roadmap.html#autotoc_md134", null ]
      ] ],
      [ "1.0.0", "md_docs_2D1-roadmap.html#autotoc_md135", [
        [ "Stable Release", "md_docs_2D1-roadmap.html#autotoc_md136", null ]
      ] ],
      [ "Current Focus Areas", "md_docs_2D1-roadmap.html#autotoc_md137", [
        [ "Immediate Priorities (v0.4.X)", "md_docs_2D1-roadmap.html#autotoc_md138", null ],
        [ "Long-term Vision", "md_docs_2D1-roadmap.html#autotoc_md139", null ]
      ] ]
    ] ],
    [ "Asm Style Guide", "md_docs_2E1-asm-style-guide.html", [
      [ "Table of Contents", "md_docs_2E1-asm-style-guide.html#autotoc_md141", null ],
      [ "File Structure", "md_docs_2E1-asm-style-guide.html#autotoc_md142", null ],
      [ "Labels and Symbols", "md_docs_2E1-asm-style-guide.html#autotoc_md143", null ],
      [ "Comments", "md_docs_2E1-asm-style-guide.html#autotoc_md144", null ],
      [ "Instructions", "md_docs_2E1-asm-style-guide.html#autotoc_md145", null ],
      [ "Macros", "md_docs_2E1-asm-style-guide.html#autotoc_md146", null ],
      [ "Loops and Branching", "md_docs_2E1-asm-style-guide.html#autotoc_md147", null ],
      [ "Data Structures", "md_docs_2E1-asm-style-guide.html#autotoc_md148", null ],
      [ "Code Organization", "md_docs_2E1-asm-style-guide.html#autotoc_md149", null ],
      [ "Custom Code", "md_docs_2E1-asm-style-guide.html#autotoc_md150", null ]
    ] ],
    [ "Dungeon Editor Guide", "md_docs_2E2-dungeon-editor-guide.html", [
      [ "Overview", "md_docs_2E2-dungeon-editor-guide.html#autotoc_md152", null ],
      [ "Architecture", "md_docs_2E2-dungeon-editor-guide.html#autotoc_md153", [
        [ "Core Components", "md_docs_2E2-dungeon-editor-guide.html#autotoc_md154", [
          [ "DungeonEditorSystem", "md_docs_2E2-dungeon-editor-guide.html#autotoc_md155", null ],
          [ "DungeonObjectEditor", "md_docs_2E2-dungeon-editor-guide.html#autotoc_md156", null ],
          [ "ObjectRenderer", "md_docs_2E2-dungeon-editor-guide.html#autotoc_md157", null ],
          [ "DungeonEditor (UI Layer)", "md_docs_2E2-dungeon-editor-guide.html#autotoc_md158", null ]
        ] ]
      ] ],
      [ "Coordinate System", "md_docs_2E2-dungeon-editor-guide.html#autotoc_md159", [
        [ "Room Coordinates vs Canvas Coordinates", "md_docs_2E2-dungeon-editor-guide.html#autotoc_md160", [
          [ "Conversion Functions", "md_docs_2E2-dungeon-editor-guide.html#autotoc_md161", null ]
        ] ],
        [ "Coordinate System Features", "md_docs_2E2-dungeon-editor-guide.html#autotoc_md162", null ]
      ] ],
      [ "Object Rendering System", "md_docs_2E2-dungeon-editor-guide.html#autotoc_md163", [
        [ "Object Types", "md_docs_2E2-dungeon-editor-guide.html#autotoc_md164", null ],
        [ "Rendering Pipeline", "md_docs_2E2-dungeon-editor-guide.html#autotoc_md165", null ],
        [ "Performance Optimizations", "md_docs_2E2-dungeon-editor-guide.html#autotoc_md166", null ]
      ] ],
      [ "User Interface", "md_docs_2E2-dungeon-editor-guide.html#autotoc_md167", [
        [ "Integrated Editing Panels", "md_docs_2E2-dungeon-editor-guide.html#autotoc_md168", [
          [ "Main Canvas", "md_docs_2E2-dungeon-editor-guide.html#autotoc_md169", null ],
          [ "Compact Editing Panels", "md_docs_2E2-dungeon-editor-guide.html#autotoc_md170", null ]
        ] ],
        [ "Object Preview System", "md_docs_2E2-dungeon-editor-guide.html#autotoc_md171", null ]
      ] ],
      [ "Integration with ZScream", "md_docs_2E2-dungeon-editor-guide.html#autotoc_md172", [
        [ "Room Loading", "md_docs_2E2-dungeon-editor-guide.html#autotoc_md173", null ],
        [ "Object Parsing", "md_docs_2E2-dungeon-editor-guide.html#autotoc_md174", null ],
        [ "Coordinate System", "md_docs_2E2-dungeon-editor-guide.html#autotoc_md175", null ]
      ] ],
      [ "Testing and Validation", "md_docs_2E2-dungeon-editor-guide.html#autotoc_md176", [
        [ "Integration Tests", "md_docs_2E2-dungeon-editor-guide.html#autotoc_md177", null ],
        [ "Test Data", "md_docs_2E2-dungeon-editor-guide.html#autotoc_md178", null ],
        [ "Performance Benchmarks", "md_docs_2E2-dungeon-editor-guide.html#autotoc_md179", null ]
      ] ],
      [ "Usage Examples", "md_docs_2E2-dungeon-editor-guide.html#autotoc_md180", [
        [ "Basic Object Editing", "md_docs_2E2-dungeon-editor-guide.html#autotoc_md181", null ],
        [ "Coordinate Conversion", "md_docs_2E2-dungeon-editor-guide.html#autotoc_md182", null ],
        [ "Object Preview", "md_docs_2E2-dungeon-editor-guide.html#autotoc_md183", null ]
      ] ],
      [ "Configuration Options", "md_docs_2E2-dungeon-editor-guide.html#autotoc_md184", [
        [ "Editor Configuration", "md_docs_2E2-dungeon-editor-guide.html#autotoc_md185", null ],
        [ "Performance Configuration", "md_docs_2E2-dungeon-editor-guide.html#autotoc_md186", null ]
      ] ],
      [ "Troubleshooting", "md_docs_2E2-dungeon-editor-guide.html#autotoc_md187", [
        [ "Common Issues", "md_docs_2E2-dungeon-editor-guide.html#autotoc_md188", null ],
        [ "Debug Information", "md_docs_2E2-dungeon-editor-guide.html#autotoc_md189", null ]
      ] ],
      [ "Future Enhancements", "md_docs_2E2-dungeon-editor-guide.html#autotoc_md190", [
        [ "Planned Features", "md_docs_2E2-dungeon-editor-guide.html#autotoc_md191", null ]
      ] ],
      [ "Conclusion", "md_docs_2E2-dungeon-editor-guide.html#autotoc_md192", null ]
    ] ],
    [ "Dungeon Editor Design Plan", "md_docs_2E3-dungeon-editor-design.html", [
      [ "Overview", "md_docs_2E3-dungeon-editor-design.html#autotoc_md194", null ],
      [ "Architecture Overview", "md_docs_2E3-dungeon-editor-design.html#autotoc_md195", [
        [ "Core Components", "md_docs_2E3-dungeon-editor-design.html#autotoc_md196", null ],
        [ "File Structure", "md_docs_2E3-dungeon-editor-design.html#autotoc_md197", null ]
      ] ],
      [ "Component Responsibilities", "md_docs_2E3-dungeon-editor-design.html#autotoc_md198", [
        [ "DungeonEditor (Main Orchestrator)", "md_docs_2E3-dungeon-editor-design.html#autotoc_md199", null ],
        [ "DungeonRoomSelector", "md_docs_2E3-dungeon-editor-design.html#autotoc_md200", null ],
        [ "DungeonCanvasViewer", "md_docs_2E3-dungeon-editor-design.html#autotoc_md201", null ],
        [ "DungeonObjectSelector", "md_docs_2E3-dungeon-editor-design.html#autotoc_md202", null ]
      ] ],
      [ "Data Flow", "md_docs_2E3-dungeon-editor-design.html#autotoc_md203", [
        [ "Initialization Flow", "md_docs_2E3-dungeon-editor-design.html#autotoc_md204", null ],
        [ "Runtime Data Flow", "md_docs_2E3-dungeon-editor-design.html#autotoc_md205", null ],
        [ "Key Data Structures", "md_docs_2E3-dungeon-editor-design.html#autotoc_md206", null ]
      ] ],
      [ "Integration Patterns", "md_docs_2E3-dungeon-editor-design.html#autotoc_md207", [
        [ "Component Communication", "md_docs_2E3-dungeon-editor-design.html#autotoc_md208", null ],
        [ "ROM Data Management", "md_docs_2E3-dungeon-editor-design.html#autotoc_md209", null ],
        [ "State Synchronization", "md_docs_2E3-dungeon-editor-design.html#autotoc_md210", null ]
      ] ],
      [ "UI Layout Architecture", "md_docs_2E3-dungeon-editor-design.html#autotoc_md211", [
        [ "3-Column Layout", "md_docs_2E3-dungeon-editor-design.html#autotoc_md212", null ],
        [ "Component Internal Layout", "md_docs_2E3-dungeon-editor-design.html#autotoc_md213", null ]
      ] ],
      [ "Coordinate System", "md_docs_2E3-dungeon-editor-design.html#autotoc_md214", [
        [ "Room Coordinates vs Canvas Coordinates", "md_docs_2E3-dungeon-editor-design.html#autotoc_md215", null ],
        [ "Bounds Checking", "md_docs_2E3-dungeon-editor-design.html#autotoc_md216", null ]
      ] ],
      [ "Error Handling & Validation", "md_docs_2E3-dungeon-editor-design.html#autotoc_md217", [
        [ "ROM Validation", "md_docs_2E3-dungeon-editor-design.html#autotoc_md218", null ],
        [ "Bounds Validation", "md_docs_2E3-dungeon-editor-design.html#autotoc_md219", null ]
      ] ],
      [ "Performance Considerations", "md_docs_2E3-dungeon-editor-design.html#autotoc_md220", [
        [ "Caching Strategy", "md_docs_2E3-dungeon-editor-design.html#autotoc_md221", null ],
        [ "Rendering Optimization", "md_docs_2E3-dungeon-editor-design.html#autotoc_md222", null ]
      ] ],
      [ "Testing Strategy", "md_docs_2E3-dungeon-editor-design.html#autotoc_md223", [
        [ "Integration Tests", "md_docs_2E3-dungeon-editor-design.html#autotoc_md224", null ],
        [ "Test Categories", "md_docs_2E3-dungeon-editor-design.html#autotoc_md225", null ]
      ] ],
      [ "Future Development Guidelines", "md_docs_2E3-dungeon-editor-design.html#autotoc_md226", [
        [ "Adding New Features", "md_docs_2E3-dungeon-editor-design.html#autotoc_md227", null ],
        [ "Component Extension Patterns", "md_docs_2E3-dungeon-editor-design.html#autotoc_md228", null ],
        [ "Data Flow Extension", "md_docs_2E3-dungeon-editor-design.html#autotoc_md229", null ],
        [ "UI Layout Extension", "md_docs_2E3-dungeon-editor-design.html#autotoc_md230", null ]
      ] ],
      [ "Common Pitfalls & Solutions", "md_docs_2E3-dungeon-editor-design.html#autotoc_md231", [
        [ "Memory Management", "md_docs_2E3-dungeon-editor-design.html#autotoc_md232", null ],
        [ "Coordinate System", "md_docs_2E3-dungeon-editor-design.html#autotoc_md233", null ],
        [ "State Synchronization", "md_docs_2E3-dungeon-editor-design.html#autotoc_md234", null ],
        [ "Performance Issues", "md_docs_2E3-dungeon-editor-design.html#autotoc_md235", null ]
      ] ],
      [ "Debugging Tools", "md_docs_2E3-dungeon-editor-design.html#autotoc_md236", [
        [ "Debug Popup", "md_docs_2E3-dungeon-editor-design.html#autotoc_md237", null ],
        [ "Logging", "md_docs_2E3-dungeon-editor-design.html#autotoc_md238", null ]
      ] ],
      [ "Build Integration", "md_docs_2E3-dungeon-editor-design.html#autotoc_md239", [
        [ "CMake Configuration", "md_docs_2E3-dungeon-editor-design.html#autotoc_md240", null ],
        [ "Dependencies", "md_docs_2E3-dungeon-editor-design.html#autotoc_md241", null ]
      ] ],
      [ "Conclusion", "md_docs_2E3-dungeon-editor-design.html#autotoc_md242", null ]
    ] ],
    [ "DungeonEditor Refactoring Plan", "md_docs_2E4-dungeon-editor-refactoring.html", [
      [ "Overview", "md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md244", null ],
      [ "Component Structure", "md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md245", [
        [ "✅ Created Components", "md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md246", [
          [ "DungeonToolset (dungeon_toolset.h/cc)", "md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md247", null ],
          [ "DungeonObjectInteraction (dungeon_object_interaction.h/cc)", "md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md248", null ],
          [ "DungeonRenderer (dungeon_renderer.h/cc)", "md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md249", null ],
          [ "DungeonRoomLoader (dungeon_room_loader.h/cc)", "md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md250", null ],
          [ "DungeonUsageTracker (dungeon_usage_tracker.h/cc)", "md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md251", null ]
        ] ]
      ] ],
      [ "Refactored DungeonEditor Structure", "md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md252", [
        [ "Before Refactoring: 1444 lines", "md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md253", null ],
        [ "After Refactoring: ~400 lines", "md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md254", null ]
      ] ],
      [ "Method Migration Map", "md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md255", [
        [ "Core Editor Methods (Keep in main file)", "md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md256", null ],
        [ "UI Methods (Keep for coordination)", "md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md257", null ],
        [ "Methods Moved to Components", "md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md258", [
          [ "→ DungeonToolset", "md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md259", null ],
          [ "→ DungeonObjectInteraction", "md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md260", null ],
          [ "→ DungeonRenderer", "md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md261", null ],
          [ "→ DungeonRoomLoader", "md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md262", null ],
          [ "→ DungeonUsageTracker", "md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md263", null ]
        ] ]
      ] ],
      [ "Component Communication", "md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md264", [
        [ "Callback System", "md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md265", null ],
        [ "Data Sharing", "md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md266", null ]
      ] ],
      [ "Benefits of Refactoring", "md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md267", [
        [ "Reduced Complexity", "md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md268", null ],
        [ "Improved Testability", "md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md269", null ],
        [ "Better Maintainability", "md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md270", null ],
        [ "Enhanced Extensibility", "md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md271", null ],
        [ "Cleaner Dependencies", "md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md272", null ]
      ] ],
      [ "Implementation Status", "md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md273", [
        [ "✅ Completed", "md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md274", null ],
        [ "🔄 In Progress", "md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md275", null ],
        [ "⏳ Pending", "md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md276", null ]
      ] ],
      [ "Migration Strategy", "md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md277", [
        [ "Phase 1: Create Components ✅", "md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md278", null ],
        [ "Phase 2: Integrate Components 🔄", "md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md279", null ],
        [ "Phase 3: Move Methods", "md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md280", null ],
        [ "Phase 4: Cleanup", "md_docs_2E4-dungeon-editor-refactoring.html#autotoc_md281", null ]
      ] ]
    ] ],
    [ "YAZE Dungeon Object System", "md_docs_2E5-dungeon-object-system.html", [
      [ "Overview", "md_docs_2E5-dungeon-object-system.html#autotoc_md283", null ],
      [ "Architecture", "md_docs_2E5-dungeon-object-system.html#autotoc_md284", [
        [ "Core Components", "md_docs_2E5-dungeon-object-system.html#autotoc_md285", [
          [ "DungeonEditor (src/app/editor/dungeon/dungeon_editor.h)", "md_docs_2E5-dungeon-object-system.html#autotoc_md286", null ],
          [ "DungeonObjectSelector (src/app/editor/dungeon/dungeon_object_selector.h)", "md_docs_2E5-dungeon-object-system.html#autotoc_md287", null ],
          [ "DungeonCanvasViewer (src/app/editor/dungeon/dungeon_canvas_viewer.h)", "md_docs_2E5-dungeon-object-system.html#autotoc_md288", null ],
          [ "Room Management System (src/app/zelda3/dungeon/room.h)", "md_docs_2E5-dungeon-object-system.html#autotoc_md289", null ]
        ] ]
      ] ],
      [ "Object Types and Hierarchies", "md_docs_2E5-dungeon-object-system.html#autotoc_md290", [
        [ "Room Objects", "md_docs_2E5-dungeon-object-system.html#autotoc_md291", [
          [ "Type 1 Objects (0x00-0xFF)", "md_docs_2E5-dungeon-object-system.html#autotoc_md292", null ],
          [ "Type 2 Objects (0x100-0x1FF)", "md_docs_2E5-dungeon-object-system.html#autotoc_md293", null ],
          [ "Type 3 Objects (0x200+)", "md_docs_2E5-dungeon-object-system.html#autotoc_md294", null ]
        ] ],
        [ "Object Properties", "md_docs_2E5-dungeon-object-system.html#autotoc_md295", null ]
      ] ],
      [ "How Object Placement Works", "md_docs_2E5-dungeon-object-system.html#autotoc_md296", [
        [ "Selection Process", "md_docs_2E5-dungeon-object-system.html#autotoc_md297", null ],
        [ "Placement Process", "md_docs_2E5-dungeon-object-system.html#autotoc_md298", null ],
        [ "Code Flow", "md_docs_2E5-dungeon-object-system.html#autotoc_md299", null ]
      ] ],
      [ "Rendering Pipeline", "md_docs_2E5-dungeon-object-system.html#autotoc_md300", [
        [ "Object Rendering", "md_docs_2E5-dungeon-object-system.html#autotoc_md301", null ],
        [ "Performance Optimizations", "md_docs_2E5-dungeon-object-system.html#autotoc_md302", null ]
      ] ],
      [ "User Interface Components", "md_docs_2E5-dungeon-object-system.html#autotoc_md303", [
        [ "Three-Column Layout", "md_docs_2E5-dungeon-object-system.html#autotoc_md304", [
          [ "Column 1: Room Control Panel (280px fixed)", "md_docs_2E5-dungeon-object-system.html#autotoc_md305", null ],
          [ "Column 2: Windowed Canvas (800px fixed)", "md_docs_2E5-dungeon-object-system.html#autotoc_md306", null ],
          [ "Column 3: Object Selector/Editor (stretch)", "md_docs_2E5-dungeon-object-system.html#autotoc_md307", null ]
        ] ],
        [ "Debug and Control Features", "md_docs_2E5-dungeon-object-system.html#autotoc_md308", [
          [ "Room Properties Table", "md_docs_2E5-dungeon-object-system.html#autotoc_md309", null ],
          [ "Object Statistics", "md_docs_2E5-dungeon-object-system.html#autotoc_md310", null ]
        ] ]
      ] ],
      [ "Integration with ROM Data", "md_docs_2E5-dungeon-object-system.html#autotoc_md311", [
        [ "Data Sources", "md_docs_2E5-dungeon-object-system.html#autotoc_md312", [
          [ "Room Headers (0x1F8000)", "md_docs_2E5-dungeon-object-system.html#autotoc_md313", null ],
          [ "Object Data", "md_docs_2E5-dungeon-object-system.html#autotoc_md314", null ],
          [ "Graphics Data", "md_docs_2E5-dungeon-object-system.html#autotoc_md315", null ]
        ] ],
        [ "Assembly Integration", "md_docs_2E5-dungeon-object-system.html#autotoc_md316", null ]
      ] ],
      [ "Comparison with ZScream", "md_docs_2E5-dungeon-object-system.html#autotoc_md317", [
        [ "Architectural Differences", "md_docs_2E5-dungeon-object-system.html#autotoc_md318", [
          [ "Component-Based Architecture", "md_docs_2E5-dungeon-object-system.html#autotoc_md319", null ],
          [ "Real-time Rendering", "md_docs_2E5-dungeon-object-system.html#autotoc_md320", null ],
          [ "UI Organization", "md_docs_2E5-dungeon-object-system.html#autotoc_md321", null ],
          [ "Caching Strategy", "md_docs_2E5-dungeon-object-system.html#autotoc_md322", null ]
        ] ],
        [ "Shared Concepts", "md_docs_2E5-dungeon-object-system.html#autotoc_md323", null ]
      ] ],
      [ "Best Practices", "md_docs_2E5-dungeon-object-system.html#autotoc_md324", [
        [ "Performance", "md_docs_2E5-dungeon-object-system.html#autotoc_md325", null ],
        [ "Code Organization", "md_docs_2E5-dungeon-object-system.html#autotoc_md326", null ],
        [ "User Experience", "md_docs_2E5-dungeon-object-system.html#autotoc_md327", null ]
      ] ],
      [ "Future Enhancements", "md_docs_2E5-dungeon-object-system.html#autotoc_md328", [
        [ "Planned Features", "md_docs_2E5-dungeon-object-system.html#autotoc_md329", null ],
        [ "Technical Improvements", "md_docs_2E5-dungeon-object-system.html#autotoc_md330", null ]
      ] ]
    ] ],
    [ "Overworld Loading Guide", "md_docs_2F1-overworld-loading.html", [
      [ "Table of Contents", "md_docs_2F1-overworld-loading.html#autotoc_md332", null ],
      [ "Overview", "md_docs_2F1-overworld-loading.html#autotoc_md333", null ],
      [ "ROM Types and Versions", "md_docs_2F1-overworld-loading.html#autotoc_md334", [
        [ "Version Detection", "md_docs_2F1-overworld-loading.html#autotoc_md335", null ],
        [ "Feature Support by Version", "md_docs_2F1-overworld-loading.html#autotoc_md336", null ]
      ] ],
      [ "Overworld Map Structure", "md_docs_2F1-overworld-loading.html#autotoc_md337", [
        [ "Core Properties", "md_docs_2F1-overworld-loading.html#autotoc_md338", null ]
      ] ],
      [ "Overlays and Special Area Maps", "md_docs_2F1-overworld-loading.html#autotoc_md339", [
        [ "Understanding Overlays", "md_docs_2F1-overworld-loading.html#autotoc_md340", null ],
        [ "Special Area Maps (0x80-0x9F)", "md_docs_2F1-overworld-loading.html#autotoc_md341", null ],
        [ "Overlay ID Mappings", "md_docs_2F1-overworld-loading.html#autotoc_md342", null ],
        [ "Drawing Order", "md_docs_2F1-overworld-loading.html#autotoc_md343", null ],
        [ "Vanilla Overlay Loading", "md_docs_2F1-overworld-loading.html#autotoc_md344", null ],
        [ "Special Area Graphics Loading", "md_docs_2F1-overworld-loading.html#autotoc_md345", null ]
      ] ],
      [ "Loading Process", "md_docs_2F1-overworld-loading.html#autotoc_md346", [
        [ "Version Detection", "md_docs_2F1-overworld-loading.html#autotoc_md347", null ],
        [ "Map Initialization", "md_docs_2F1-overworld-loading.html#autotoc_md348", null ],
        [ "Property Loading", "md_docs_2F1-overworld-loading.html#autotoc_md349", [
          [ "Vanilla ROMs (asm_version == 0xFF)", "md_docs_2F1-overworld-loading.html#autotoc_md350", null ],
          [ "ZSCustomOverworld v2/v3", "md_docs_2F1-overworld-loading.html#autotoc_md351", null ]
        ] ],
        [ "Custom Data Loading", "md_docs_2F1-overworld-loading.html#autotoc_md352", null ]
      ] ],
      [ "ZScream Implementation", "md_docs_2F1-overworld-loading.html#autotoc_md353", [
        [ "OverworldMap Constructor", "md_docs_2F1-overworld-loading.html#autotoc_md354", null ],
        [ "Key Methods", "md_docs_2F1-overworld-loading.html#autotoc_md355", null ]
      ] ],
      [ "Yaze Implementation", "md_docs_2F1-overworld-loading.html#autotoc_md356", [
        [ "OverworldMap Constructor", "md_docs_2F1-overworld-loading.html#autotoc_md357", null ],
        [ "Key Methods", "md_docs_2F1-overworld-loading.html#autotoc_md358", null ],
        [ "Current Status", "md_docs_2F1-overworld-loading.html#autotoc_md359", null ]
      ] ],
      [ "Key Differences", "md_docs_2F1-overworld-loading.html#autotoc_md360", [
        [ "Language and Architecture", "md_docs_2F1-overworld-loading.html#autotoc_md361", null ],
        [ "Data Structures", "md_docs_2F1-overworld-loading.html#autotoc_md362", null ],
        [ "Error Handling", "md_docs_2F1-overworld-loading.html#autotoc_md363", null ],
        [ "Graphics Processing", "md_docs_2F1-overworld-loading.html#autotoc_md364", null ]
      ] ],
      [ "Common Issues and Solutions", "md_docs_2F1-overworld-loading.html#autotoc_md365", [
        [ "Version Detection Issues", "md_docs_2F1-overworld-loading.html#autotoc_md366", null ],
        [ "Palette Loading Errors", "md_docs_2F1-overworld-loading.html#autotoc_md367", null ],
        [ "Graphics Not Loading", "md_docs_2F1-overworld-loading.html#autotoc_md368", null ],
        [ "Overlay Issues", "md_docs_2F1-overworld-loading.html#autotoc_md369", null ],
        [ "Large Map Problems", "md_docs_2F1-overworld-loading.html#autotoc_md370", null ],
        [ "Special Area Graphics Issues", "md_docs_2F1-overworld-loading.html#autotoc_md371", null ]
      ] ],
      [ "Best Practices", "md_docs_2F1-overworld-loading.html#autotoc_md372", [
        [ "Version-Specific Code", "md_docs_2F1-overworld-loading.html#autotoc_md373", null ],
        [ "Error Handling", "md_docs_2F1-overworld-loading.html#autotoc_md374", null ],
        [ "Memory Management", "md_docs_2F1-overworld-loading.html#autotoc_md375", null ],
        [ "Thread Safety", "md_docs_2F1-overworld-loading.html#autotoc_md376", null ]
      ] ],
      [ "Conclusion", "md_docs_2F1-overworld-loading.html#autotoc_md377", null ]
    ] ],
    [ "F2-overworld-expansion", "md_docs_2F2-overworld-expansion.html", null ],
    [ "YAZE Documentation", "md_docs_2index.html", [
      [ "Quick Start", "md_docs_2index.html#autotoc_md379", null ],
      [ "Development", "md_docs_2index.html#autotoc_md380", null ],
      [ "Technical Documentation", "md_docs_2index.html#autotoc_md381", [
        [ "Assembly & Code", "md_docs_2index.html#autotoc_md382", null ],
        [ "Editor Systems", "md_docs_2index.html#autotoc_md383", null ],
        [ "Overworld System", "md_docs_2index.html#autotoc_md384", null ]
      ] ],
      [ "Current Version: 0.3.0 (January 2025)", "md_docs_2index.html#autotoc_md385", [
        [ "✅ Major Features", "md_docs_2index.html#autotoc_md386", [
          [ "Asar 65816 Assembler Integration", "md_docs_2index.html#autotoc_md387", null ],
          [ "ZSCustomOverworld v3", "md_docs_2index.html#autotoc_md388", null ],
          [ "Advanced Message Editing", "md_docs_2index.html#autotoc_md389", null ],
          [ "GUI Docking System", "md_docs_2index.html#autotoc_md390", null ],
          [ "Technical Improvements", "md_docs_2index.html#autotoc_md391", null ]
        ] ],
        [ "🔄 In Progress", "md_docs_2index.html#autotoc_md392", null ],
        [ "⏳ Planned for 0.4.X", "md_docs_2index.html#autotoc_md393", null ]
      ] ],
      [ "Key Features", "md_docs_2index.html#autotoc_md394", [
        [ "ROM Editing Capabilities", "md_docs_2index.html#autotoc_md395", null ],
        [ "Development Tools", "md_docs_2index.html#autotoc_md396", null ],
        [ "Cross-Platform Support", "md_docs_2index.html#autotoc_md397", null ]
      ] ],
      [ "Compatibility", "md_docs_2index.html#autotoc_md398", [
        [ "ROM Support", "md_docs_2index.html#autotoc_md399", null ],
        [ "File Format Support", "md_docs_2index.html#autotoc_md400", null ]
      ] ],
      [ "Development & Community", "md_docs_2index.html#autotoc_md401", [
        [ "Architecture", "md_docs_2index.html#autotoc_md402", null ],
        [ "Contributing", "md_docs_2index.html#autotoc_md403", null ],
        [ "Community", "md_docs_2index.html#autotoc_md404", null ]
      ] ],
      [ "License", "md_docs_2index.html#autotoc_md405", null ]
    ] ],
    [ "YAZE - Yet Another Zelda3 Editor", "md_README.html", [
      [ "🚀 Version 0.3.0 - Major Release", "md_README.html#autotoc_md408", [
        [ "✨ Key Features", "md_README.html#autotoc_md409", [
          [ "Asar 65816 Assembler Integration", "md_README.html#autotoc_md410", null ],
          [ "ZSCustomOverworld v3", "md_README.html#autotoc_md411", null ],
          [ "Advanced Features", "md_README.html#autotoc_md412", null ]
        ] ],
        [ "🛠️ Technical Improvements", "md_README.html#autotoc_md413", null ]
      ] ],
      [ "🏗️ Quick Start", "md_README.html#autotoc_md414", [
        [ "Prerequisites", "md_README.html#autotoc_md415", null ],
        [ "Build", "md_README.html#autotoc_md416", null ],
        [ "Targets", "md_README.html#autotoc_md417", null ]
      ] ],
      [ "💻 Usage Examples", "md_README.html#autotoc_md418", [
        [ "Asar Assembly Patching", "md_README.html#autotoc_md419", null ],
        [ "C++ API Usage", "md_README.html#autotoc_md420", null ]
      ] ],
      [ "📚 Documentation", "md_README.html#autotoc_md421", [
        [ "Core Documentation", "md_README.html#autotoc_md422", null ],
        [ "Development", "md_README.html#autotoc_md423", null ],
        [ "Technical Documentation", "md_README.html#autotoc_md424", null ]
      ] ],
      [ "🔧 Supported Platforms", "md_README.html#autotoc_md425", null ],
      [ "🎮 ROM Compatibility", "md_README.html#autotoc_md426", null ],
      [ "🤝 Contributing", "md_README.html#autotoc_md427", [
        [ "Community", "md_README.html#autotoc_md428", null ]
      ] ],
      [ "📄 License", "md_README.html#autotoc_md429", null ],
      [ "🙏 Acknowledgments", "md_README.html#autotoc_md430", null ],
      [ "📸 Screenshots", "md_README.html#autotoc_md431", null ]
    ] ],
    [ "Deprecated List", "deprecated.html", null ],
    [ "Todo List", "todo.html", null ],
    [ "Test List", "test.html", null ],
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
"classyaze_1_1RecentFilesManager.html",
"classyaze_1_1app_1_1core_1_1AsarWrapper.html#ab2583f55b85ce00f473fbff231e94c4b",
"classyaze_1_1editor_1_1AssemblyEditor.html#adb7411335d90a8d6404e2ee2cb08e265",
"classyaze_1_1editor_1_1DungeonEditor.html#af22f8be27499699e66d1d22020b4662c",
"classyaze_1_1editor_1_1DungeonRoomLoader.html#a9b56597b2a46499419a631675608b693",
"classyaze_1_1editor_1_1EditorManager.html#a39f57090d59dfd0c039f61e38d4dd471",
"classyaze_1_1editor_1_1GraphicsEditor.html#a19f9ac18b70c9cd5c7fe218652176a9a",
"classyaze_1_1editor_1_1MapPropertiesSystem.html#a8af37e9dc145e7ecad7a1cae241dfd74",
"classyaze_1_1editor_1_1OverworldEditor.html#a5b81556632edff7b417e1c61efee4759",
"classyaze_1_1editor_1_1PopupManager.html#a04a53d2aab2eb7c1a9f5b1b9ec1b6538",
"classyaze_1_1editor_1_1SpriteEditor.html#a07ec22f50553dd8446ecc576b7d3367b",
"classyaze_1_1emu_1_1Apu.html#af8763b9841bce80621638b58fdbcac27",
"classyaze_1_1emu_1_1Cpu.html#a764eeea7b58f891e124ee0a3394e1d89",
"classyaze_1_1emu_1_1Cpu.html#af3a91027d2e71e313febcf1a7fd7856a",
"classyaze_1_1emu_1_1Memory.html#a8f18525d8d13d640cd7413c0c8f323eb",
"classyaze_1_1emu_1_1MockMemory.html#af835fd6ef0f5dc1f12b43ae9484ed3a9",
"classyaze_1_1emu_1_1PpuInterface.html#a4e3519f11646f16198c8c7d551098908",
"classyaze_1_1emu_1_1Spc700.html#a5933e96d79fd8cacf3373140d9132309",
"classyaze_1_1gfx_1_1Bitmap.html#a05432a1e204b43ea1cb4820449b7e0b8",
"classyaze_1_1gfx_1_1Tile16.html",
"classyaze_1_1gui_1_1Canvas.html#a49545e3d19ee62eaa59f0bdd7495d719",
"classyaze_1_1test_1_1Apu.html#af442e153f924430fdf8a9aa5651b65be",
"classyaze_1_1test_1_1Cpu.html#a6149eae26d4ac6e8b5578bd036f597ac",
"classyaze_1_1test_1_1Cpu.html#ae6fa6c1c9b120d33deb5d3d027a43fbc",
"classyaze_1_1test_1_1MemoryImpl.html#a3464e7748de5c434122b4455d6cbb836",
"classyaze_1_1test_1_1MockRom.html#a2dd724ce06862a92147d29e8df40bdfa",
"classyaze_1_1test_1_1RomDependentTestSuite.html#a41420773972e2ec9fdf337944643e191",
"classyaze_1_1test_1_1Spc700.html#ac6c908ae8e330bf0e32a0afeb887dc48",
"classyaze_1_1test_1_1TestManager.html#aea1fd8ef3a4ab247670d862ef50a4fa3",
"classyaze_1_1zelda3_1_1DungeonEditorSystem.html#a1cb260825663b8c49a3f105a6ec1ebe3",
"classyaze_1_1zelda3_1_1DungeonEditorSystem.html#ad80d712c78460498ba1c257387498558",
"classyaze_1_1zelda3_1_1DungeonObjectEditor.html#aa6424ce197379b9d60267cd430116377",
"classyaze_1_1zelda3_1_1ObjectParser.html#aa8a97d0784f8abd164700eaf9444ce31",
"classyaze_1_1zelda3_1_1Overworld.html#a39c12da24da3a1916c4f21036250c24a",
"classyaze_1_1zelda3_1_1OverworldMap.html#a23c6abbeb479d62bbc97c2d3b74d3888",
"classyaze_1_1zelda3_1_1Room.html#a0b29b34b14119afde3380809f48e5867",
"classyaze_1_1zelda3_1_1RoomEntrance.html#a714b0ac5d497a2d49b154f3c3d839289",
"classyaze_1_1zelda3_1_1Sprite.html#a3070422abb2ea9f2b0ba795e36aeec9b",
"classyaze_1_1zelda3_1_1TitleScreen.html#aadc3905a832ba76767c85f27e67cf601",
"dma_8h.html",
"globals_t.html",
"icons_8h.html#a0caf2828612a87b407855832cc4731c4",
"icons_8h.html#a2a7d31a24bce549ecf200c222fd3e821",
"icons_8h.html#a48615a414696247f1d6d74b851f9d282",
"icons_8h.html#a64f5c279a9f97affd005f7741568bae2",
"icons_8h.html#a83c5891ceb8181e1942cb386ccfbad86",
"icons_8h.html#aa3e33278b9ac5083ba4bfa293c1f4bab",
"icons_8h.html#abfaec4ba7e956e079694bf31a2b2f074",
"icons_8h.html#adbfa296beba53f44b6eb7c5b1a1990f8",
"icons_8h.html#af641d49379d8a4a744af7db6ac3b745b",
"md_docs_2B1-contributing.html#autotoc_md81",
"md_docs_2F1-overworld-loading.html#autotoc_md337",
"namespaceyaze_1_1cli_1_1anonymous__namespace_02tui_8cc_03.html#a879bdc125949b92f2e3b4e8f26d26521",
"namespaceyaze_1_1emu.html#a324c8c6f0c078ce6d1f4c73e2016b4ba",
"namespaceyaze_1_1gfx_1_1lc__lz2.html#a5b3fcbbb17d623662015ed426f345fe3",
"namespaceyaze_1_1test.html#a153b69862319c0d84c1c6fd1e393527c",
"namespaceyaze_1_1test.html#a99c38aefe9491cfaf57272bc8d28250a",
"namespaceyaze_1_1zelda3.html#a019f5467220daa0ae5321ff21e96b7d4",
"namespaceyaze_1_1zelda3.html#a693df677b1b2754452c8b8b4c4a98468af7edf4aa720007546d7e1e76cda507f5",
"namespaceyaze_1_1zelda3.html#afa47b9b687860f758b9b7b9c1acb91f6",
"structTextEditor_1_1Glyph.html#ac73459bb7dd69fa94409b5bd7009f5fc",
"structyaze_1_1core_1_1FlagsMenu.html#abc68206a2a17b5a9081d776338b5af1d",
"structyaze_1_1editor_1_1MessagePreview.html#a2a3c3ddfbee814d3f4f0fcc008d0cfaa",
"structyaze_1_1editor_1_1zsprite_1_1ZSprite.html#afad32f6eac364443e37f4e32fcd88a24",
"structyaze_1_1emu_1_1DmaRegisters.html#af2051f4d084812bfd15f6e9242d3cacf",
"structyaze_1_1emu_1_1OAMDATAREAD.html",
"structyaze_1_1emu_1_1VideoPortControl.html#ab703ab48a910ae17a451e0331abb5d49",
"structyaze_1_1gfx_1_1Paletteset.html#a4e691551109932639735b4390f69c98d",
"structyaze_1_1gui_1_1TextBox.html#a89c0349e9a967ba2141bb661bfcf5286",
"structyaze_1_1test_1_1TestResult.html#a05f2e331e42a54a70aa25181fe1584c4",
"structyaze_1_1zelda3_1_1DungeonMap.html#ac0db5386648063e45da045b79fbd67c6",
"structyaze_1_1zelda3_1_1ObjectSubtypeInfo.html#aa47bd956a0f339b24070bdf2435dd4cc",
"structyaze__extension.html#a78827a2290a2eea969ded6fec5fcc4e9"
];

var SYNCONMSG = 'click to disable panel synchronization';
var SYNCOFFMSG = 'click to enable panel synchronization';