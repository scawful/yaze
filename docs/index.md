# Yaze Documentation

Welcome to the Yaze documentation. This cross-platform Zelda 3 ROM editor is built with C++20, SDL2, and ImGui, designed to be compatible with ZScream projects.

## Quick Start

- [Getting Started](getting-started.md) - Basic setup and usage
- [Build Instructions](build-instructions.md) - How to build yaze
- [Contributing](contributing.md) - How to contribute to the project

## Core Documentation

### Architecture & Infrastructure
- [Infrastructure](infrastructure.md) - Project structure and architecture
- [Assembly Style Guide](asm-style-guide.md) - 65816 assembly coding standards

### Editors

#### Dungeon Editor
- [Dungeon Editor Guide](dungeon-editor-comprehensive-guide.md) - Complete dungeon editing guide
- [Dungeon Editor Design Plan](dungeon-editor-design-plan.md) - Architecture and development guide
- [Dungeon Integration Tests](dungeon-integration-tests.md) - Testing framework

#### Overworld Editor
- [Overworld Loading Guide](overworld_loading_guide.md) - ZScream vs Yaze implementation
- [Overworld Expansion](overworld-expansion.md) - ZSCustomOverworld features

### Graphics & UI
- [Canvas Interface Refactoring](canvas-refactor-summary.md) - Canvas system architecture
- [Canvas Migration](canvas-migration.md) - Migration guide for canvas changes

### Testing
- [Integration Test Guide](integration_test_guide.md) - Comprehensive testing framework

## Project Status

### Current Version: 0.2.2 (12-31-2024)

#### ✅ Completed Features
- **Dungeon Editor**: Full dungeon editing with object rendering, sprite management, and room editing
- **Overworld Editor**: Complete overworld editing with ZSCustomOverworld v2/v3 support
- **Canvas System**: Refactored pure function interface with backward compatibility
- **Graphics System**: Optimized rendering with caching and performance monitoring
- **Integration Tests**: Comprehensive test suite for all major components

#### 🔄 In Progress
- **Sprite Builder System**: Custom sprite creation and editing
- **Emulator Subsystem**: SNES emulation for testing modifications
- **Music Editor**: Music data editing interface

#### ⏳ Planned
- **Advanced Object Editing**: Multi-object selection and manipulation
- **Plugin System**: Modular architecture for community extensions
- **Performance Optimizations**: Further rendering and memory optimizations

## Key Features

### Dungeon Editing
- Room object editing with real-time rendering
- Sprite management (enemies, NPCs, interactive objects)
- Item placement (keys, hearts, rupees, etc.)
- Entrance/exit management and room connections
- Door management with key requirements
- Chest management with treasure placement
- Undo/redo system with comprehensive state management

### Overworld Editing
- ZSCustomOverworld v2/v3 compatibility
- Custom background colors and overlays
- Map properties and metadata editing
- Sprite positioning and management
- Tile16 editing and graphics management

### Graphics System
- High-performance object rendering with caching
- SNES palette support and color management
- Graphics sheet editing and management
- Real-time preview and coordinate system management

## Compatibility

### ROM Support
- **Vanilla ROMs**: Original Zelda 3 ROMs (US/JP)
- **ZSCustomOverworld v2**: Enhanced overworld features
- **ZSCustomOverworld v3**: Advanced features with overlays and custom graphics

### Platform Support
- **Windows**: Full support with native builds
- **macOS**: Full support with native builds
- **Linux**: Full support with native builds
- **iOS**: Basic support (work in progress)

## Development

### Architecture
- **Modular Design**: Component-based architecture for maintainability
- **Cross-Platform**: SDL2 and ImGui for consistent UI across platforms
- **Modern C++**: C++20 features for performance and safety
- **Testing**: Comprehensive integration test suite

### Contributing
See [Contributing](contributing.md) for guidelines on:
- Code style and standards
- Testing requirements
- Pull request process
- Development setup

## Community

- **Discord**: [Oracle of Secrets Discord](https://discord.gg/MBFkMTPEmk)
- **GitHub**: [Yaze Repository](https://github.com/scawful/yaze)
- **Issues**: Report bugs and request features on GitHub

## License

This project is licensed under the terms specified in the [LICENSE](../LICENSE) file.

---

*Last updated: December 2024*
