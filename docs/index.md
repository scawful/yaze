# YAZE Documentation

Cross-platform Zelda 3 ROM editor built with modern C++23, SDL2, ImGui, and integrated Asar 65816 assembler support.

## Quick Start

- [Getting Started](01-getting-started.md) - Basic setup and usage
- [Build Instructions](02-build-instructions.md) - How to build from source
- [Asar Integration](03-asar-integration.md) - Complete 65816 assembler guide
- [API Reference](04-api-reference.md) - C/C++ API documentation

## Development

- [Testing Guide](A1-testing-guide.md) - Comprehensive testing framework
- [Contributing](B1-contributing.md) - Development guidelines and standards
- [Changelog](C1-changelog.md) - Version history and changes
- [Roadmap](D1-roadmap.md) - Development plans and timeline

## Technical Documentation

### Assembly & Code
- [Assembly Style Guide](E1-asm-style-guide.md) - 65816 assembly coding standards

### Editor Systems
- [Dungeon Editor Guide](E2-dungeon-editor-guide.md) - Complete dungeon editing guide
- [Dungeon Editor Design](E3-dungeon-editor-design.md) - Architecture and development guide
- [Dungeon Editor Refactoring](E4-dungeon-editor-refactoring.md) - Component-based architecture plan
- [Dungeon Object System](E5-dungeon-object-system.md) - Object management framework

### Overworld System
- [Overworld Loading](F1-overworld-loading.md) - ZSCustomOverworld v3 implementation
- [Overworld Expansion](F2-overworld-expansion.md) - Advanced overworld features

## Current Version: 0.3.0 (January 2025)

### ✅ Major Features

#### Asar 65816 Assembler Integration
- **Cross-platform ROM patching** with assembly code support
- **Symbol extraction** with addresses and opcodes
- **Assembly validation** and comprehensive error reporting
- **Modern C++ API** with safe memory management
- **Enhanced CLI tools** with improved UX

#### ZSCustomOverworld v3
- **Enhanced overworld editing** capabilities
- **Advanced map properties** and metadata
- **Custom graphics support** and tile management
- **Improved compatibility** with existing projects

#### Advanced Message Editing
- **Enhanced text editing interface** with improved parsing
- **Real-time preview** of message formatting
- **Better error handling** for message validation
- **Improved workflow** for text-based ROM hacking

#### GUI Docking System
- **Improved workspace management** with flexible layouts
- **Better window organization** for complex editing tasks
- **Enhanced user workflow** with customizable interfaces
- **Modern UI improvements** throughout the application

#### Technical Improvements
- **Modern CMake 3.16+**: Target-based configuration and build system
- **CMakePresets**: Development workflow presets for better productivity
- **Cross-platform CI/CD**: Automated builds and testing for all platforms
- **Professional packaging**: NSIS, DMG, and DEB/RPM installers
- **Enhanced testing**: ROM-dependent test separation for CI compatibility

### 🔄 In Progress
- **Sprite Builder System**: Custom sprite creation and editing tools
- **Emulator Subsystem**: SNES emulation for testing modifications
- **Music Editor**: Enhanced music data editing interface

### ⏳ Planned for 0.4.X
- **Overworld Sprites**: Complete sprite editing with add/remove functionality
- **Enhanced Dungeon Editing**: Advanced room object editing and manipulation
- **Tile16 Editing**: Enhanced editor for creating and modifying tile16 data
- **Plugin Architecture**: Framework for community extensions and custom tools

## Key Features

### ROM Editing Capabilities
- **Asar 65816 Assembly**: Apply assembly patches with symbol extraction
- **Overworld Editing**: ZSCustomOverworld v3 with enhanced features
- **Message Editing**: Advanced text editing with real-time preview
- **Palette Management**: Comprehensive color and palette editing
- **Graphics Editing**: SNES graphics manipulation and preview
- **Hex Editing**: Direct ROM data editing with validation

### Development Tools
- **Enhanced CLI**: Modern z3ed with subcommands and interactive TUI
- **Symbol Analysis**: Extract and analyze assembly symbols
- **ROM Validation**: Comprehensive ROM integrity checking
- **Project Management**: Save and load complete ROM hacking projects

### Cross-Platform Support
- **Windows**: Full support with MSVC and MinGW compilers
- **macOS**: Universal binaries supporting Intel and Apple Silicon
- **Linux**: GCC and Clang support with package management
- **Professional Packaging**: Native installers for all platforms

## Compatibility

### ROM Support
- **Vanilla ROMs**: Original Zelda 3 ROMs (US/JP)
- **ZSCustomOverworld v2/v3**: Enhanced overworld features and compatibility
- **Custom Modifications**: Support for community ROM hacks and modifications

### File Format Support
- **Assembly Files**: .asm files with Asar 65816 assembler
- **BPS Patches**: Standard ROM patching format
- **Graphics Files**: PNG, BMP, and SNES-specific formats
- **Project Files**: .yaze project format for complete editing sessions

## Development & Community

### Architecture
- **Modern C++23**: Latest language features for performance and safety
- **Modular Design**: Component-based architecture for maintainability
- **Cross-Platform**: Consistent experience across all supported platforms
- **Comprehensive Testing**: Unit tests, integration tests, and ROM validation

### Contributing
See [Contributing](B1-contributing.md) for guidelines on:
- Code style and C++23 standards
- Testing requirements and ROM handling
- Pull request process and review
- Development setup with CMake presets

### Community
- **Discord**: [Oracle of Secrets Discord](https://discord.gg/MBFkMTPEmk)
- **GitHub**: [YAZE Repository](https://github.com/scawful/yaze)
- **Issues**: Report bugs and request features on GitHub
- **Discussions**: Community discussions and support

## License

This project is licensed under the terms specified in the [LICENSE](../LICENSE) file.

---

*Last updated: January 2025 - Version 0.3.0*