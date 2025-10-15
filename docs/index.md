# yaze Documentation

Welcome to the official documentation for yaze, a comprehensive ROM editor for The Legend of Zelda: A Link to the Past.

## A: Getting Started & Testing
- [A1: Getting Started](A1-getting-started.md) - Basic setup and usage
- [A1: Testing Guide](A1-testing-guide.md) - Testing framework and best practices
- [A2: Test Dashboard Refactoring](A2-test-dashboard-refactoring.md) - In-app test dashboard architecture

## B: Build & Platform
- [B1: Build Instructions](B1-build-instructions.md) - How to build yaze on Windows, macOS, and Linux
- [B2: Platform Compatibility](B2-platform-compatibility.md) - Cross-platform support details
- [B3: Build Presets](B3-build-presets.md) - CMake preset system guide
- [B4: Git Workflow](B4-git-workflow.md) - Branching strategy, commit conventions, and release process
- [B5: Architecture and Networking](B5-architecture-and-networking.md) - System architecture, gRPC, and networking
- [B6: Zelda3 Library Refactoring](B6-zelda3-library-refactoring.md) - Core library modularization plan

## C: `z3ed` CLI
- [C1: `z3ed` Agent Guide](C1-z3ed-agent-guide.md) - AI-powered command-line interface
- [C2: Testing Without ROMs](C2-testing-without-roms.md) - Mock ROM mode for testing and CI/CD
- [C3: Agent Architecture](C3-agent-architecture.md) - AI agent system architecture
- [C4: z3ed Refactoring](C4-z3ed-refactoring.md) - CLI tool refactoring summary
- [C5: z3ed Command Abstraction](C5-z3ed-command-abstraction.md) - Command system design

## E: Development & API
- [E1: Assembly Style Guide](E1-asm-style-guide.md) - 65816 assembly coding standards
- [E2: Development Guide](E2-development-guide.md) - Core architectural patterns, UI systems, and best practices
- [E3: API Reference](E3-api-reference.md) - C/C++ API documentation for extensions
- [E4: Emulator Development Guide](E4-Emulator-Development-Guide.md) - SNES emulator subsystem implementation guide
- [E5: Debugging Guide](E5-debugging-guide.md) - Debugging techniques and workflows
- [E6: Emulator Improvements](E6-emulator-improvements.md) - Core accuracy and performance improvements roadmap
- [E7: Debugging Startup Flags](E7-debugging-startup-flags.md) - CLI flags for quick editor access and testing
- [E8: Emulator Debugging Vision](E8-emulator-debugging-vision.md) - Long-term vision for Mesen2-level debugging features
- [E9: AI Agent Debugging Guide](E9-ai-agent-debugging-guide.md) - Debugging AI agent integration
- [E10: APU Timing Analysis](E10-apu-timing-analysis.md) - APU timing fix technical analysis

## F: Technical Documentation
- [F1: Dungeon Editor Guide](F1-dungeon-editor-guide.md) - Master guide to the dungeon editing system
- [F2: Tile16 Editor Palette System](F2-tile16-editor-palette-system.md) - Design of the palette system
- [F3: Overworld Loading](F3-overworld-loading.md) - How vanilla and ZSCustomOverworld maps are loaded
- [F4: Overworld Agent Guide](F4-overworld-agent-guide.md) - AI agent integration for overworld editing

## G: Graphics & GUI Systems
- [G1: Canvas System and Automation](G1-canvas-guide.md) - Core GUI drawing and interaction system
- [G2: Renderer Migration Plan](G2-renderer-migration-plan.md) - Historical plan for renderer refactoring
- [G3: Palette System Overview](G3-palete-system-overview.md) - SNES palette system and editor integration
- [G3: Renderer Migration Complete](G3-renderer-migration-complete.md) - Post-migration analysis and results
- [G4: Canvas Coordinate Fix](G4-canvas-coordinate-fix.md) - Canvas coordinate synchronization bug fix
- [G5: GUI Consistency Guide](G5-gui-consistency-guide.md) - Card-based architecture and UI standards

## H: Project Info
- [H1: Changelog](H1-changelog.md)

## I: Roadmap & Vision
- [I1: Roadmap](I1-roadmap.md) - Current development roadmap and planned releases
- [I2: Future Improvements](I2-future-improvements.md) - Long-term vision and aspirational features

## R: ROM Reference
- [R1: A Link to the Past ROM Reference](R1-alttp-rom-reference.md) - ALTTP ROM structures, graphics, palettes, and compression

---

## Documentation Standards

### Naming Convention
- **A-series**: Getting Started & Testing
- **B-series**: Build, Platform & Git Workflow
- **C-series**: CLI Tools (`z3ed`)
- **E-series**: Development, API & Emulator
- **F-series**: Feature-Specific Technical Docs
- **G-series**: Graphics & GUI Systems
- **H-series**: Project Info (Changelog, etc.)
- **I-series**: Roadmap & Vision
- **R-series**: ROM Technical Reference

### File Naming
- Use descriptive, kebab-case names
- Prefix with series letter and number (e.g., `E4-emulator-development-guide.md`)
- Keep filenames concise but clear

### Organization Tips
For Doxygen integration, this index can be enhanced with:
- `@mainpage` directive to make this the main documentation page
- `@defgroup` to create logical groupings across files
- `@tableofcontents` for automatic TOC generation
- See individual files for `@page` and `@section` usage

### Doxygen Integration Tips
- Add a short `@mainpage` block to `docs/index.md` so generated HTML mirrors the
  manual structure.
- Define high-level groups with `@defgroup` (`getting_started`, `building`,
  `graphics_gui`, etc.) and attach individual docs using `@page ... @ingroup`.
- Use `@subpage`, `@section`, and `@subsection` when a document benefits from
  nested navigation.
- Configure `Doxyfile` with `USE_MDFILE_AS_MAINPAGE = docs/index.md`,
  `FILE_PATTERNS = *.md *.h *.cc`, and `EXTENSION_MAPPING = md=C++` to combine
  Markdown and source comments.
- Keep Markdown reader-friendly—wrap Doxygen directives in comment fences (`/**`
  … `*/`) so they are ignored by GitHub while remaining visible to the
  generator.

---

*Last updated: October 13, 2025 - Version 0.3.2*
