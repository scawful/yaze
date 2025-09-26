# YAZE - Yet Another Zelda3 Editor

A modern, cross-platform editor for The Legend of Zelda: A Link to the Past ROM hacking, built with C++23 and featuring complete Asar 65816 assembler integration.

[![Build Status](https://github.com/scawful/yaze/workflows/CI/badge.svg)](https://github.com/scawful/yaze/actions)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

## 🚀 Version 0.3.0 - Major Release

**Complete Asar Integration • Modern Build System • Enhanced CLI Tools • Professional CI/CD**

### ✨ Key Features

#### Asar 65816 Assembler Integration
- **Cross-platform ROM patching** with assembly code support
- **Symbol extraction** with addresses and opcodes from assembly files
- **Assembly validation** with comprehensive error reporting
- **Modern C++ API** with safe memory management

#### ZSCustomOverworld v3
- **Enhanced overworld editing** capabilities
- **Advanced map properties** and metadata support
- **Custom graphics support** and tile management
- **Improved compatibility** with existing projects

#### Advanced Features
- **Message Editing**: Enhanced text editing interface with real-time preview
- **GUI Docking**: Flexible workspace management with customizable layouts
- **Modern CLI**: Enhanced z3ed tool with interactive TUI and subcommands
- **Cross-Platform**: Full support for Windows, macOS, and Linux

### 🛠️ Technical Improvements
- **Modern CMake 3.16+**: Target-based configuration and build system
- **CMakePresets**: Development workflow presets for better productivity
- **Cross-platform CI/CD**: Automated builds and testing for all platforms
- **Professional packaging**: NSIS, DMG, and DEB/RPM installers
- **Enhanced testing**: ROM-dependent test separation for CI compatibility

## 🏗️ Quick Start

### Prerequisites
- **CMake 3.16+**
- **C++23 Compiler** (GCC 13+, Clang 16+, MSVC 2022 17.8+)
- **Git** with submodule support

### Build
```bash
# Clone with submodules
git clone --recursive https://github.com/scawful/yaze.git
cd yaze

# Build with presets
cmake --preset dev
cmake --build --preset dev

# Run tests
ctest --preset stable
```

### Targets
- **yaze**: GUI Editor Application with docking system
- **z3ed**: Enhanced CLI Tool with Asar integration and TUI
- **yaze_c**: C Library for extensions and custom tools
- **yaze_test**: Comprehensive test suite executable
- **yaze_emu**: Standalone SNES emulator application

## 💻 Usage Examples

### Asar Assembly Patching
```bash
# Apply assembly patch to ROM
z3ed asar my_patch.asm --rom=zelda3.sfc
✅ Asar patch applied successfully!
📁 Output: zelda3_patched.sfc
🏷️  Extracted 6 symbols:
  main_routine @ $008000
  data_table @ $008020

# Extract symbols without patching
z3ed extract my_patch.asm

# Validate assembly syntax
z3ed validate my_patch.asm
✅ Assembly file is valid

# Launch interactive TUI
z3ed --tui
```

### C++ API Usage
```cpp
#include "app/core/asar_wrapper.h"

yaze::app::core::AsarWrapper wrapper;
wrapper.Initialize();

// Apply patch to ROM
auto result = wrapper.ApplyPatch("patch.asm", rom_data);
if (result.ok() && result->success) {
    for (const auto& symbol : result->symbols) {
        std::cout << symbol.name << " @ $" 
                  << std::hex << symbol.address << std::endl;
    }
}
```

## 📚 Documentation

### Core Documentation
- **[Getting Started](docs/01-getting-started.md)** - Basic setup and usage
- **[Build Instructions](docs/02-build-instructions.md)** - Detailed build guide
- **[Asar Integration](docs/03-asar-integration.md)** - Complete 65816 assembler guide
- **[API Reference](docs/04-api-reference.md)** - C/C++ API documentation

### Development
- **[Testing Guide](docs/A1-testing-guide.md)** - Comprehensive testing framework
- **[Contributing](docs/B1-contributing.md)** - Development guidelines and standards
- **[Changelog](docs/C1-changelog.md)** - Version history and changes
- **[Roadmap](docs/D1-roadmap.md)** - Development plans and timeline

### Technical Documentation
- **[Assembly Style Guide](docs/E1-asm-style-guide.md)** - 65816 assembly coding standards
- **[Dungeon Editor Guide](docs/E2-dungeon-editor-guide.md)** - Complete dungeon editing guide
- **[Overworld Loading](docs/F1-overworld-loading.md)** - ZSCustomOverworld v3 implementation

**[Complete Documentation Index](docs/index.md)**

## 🔧 Supported Platforms

- **Windows**: Full support with MSVC and MinGW compilers
- **macOS**: Universal binaries supporting Intel and Apple Silicon
- **Linux**: GCC and Clang support with package management
- **Professional Packaging**: Native installers for all platforms

## 🎮 ROM Compatibility

- **Vanilla ROMs**: Original Zelda 3 ROMs (US/JP)
- **ZSCustomOverworld v2/v3**: Enhanced overworld features and compatibility
- **Custom Modifications**: Support for community ROM hacks and modifications

## 🤝 Contributing

We welcome contributions! Please see our [Contributing Guide](docs/B1-contributing.md) for:

- Code style and C++23 standards
- Testing requirements and ROM handling
- Pull request process and review guidelines
- Development setup with CMake presets

### Community
- **Discord**: [Oracle of Secrets Discord](https://discord.gg/MBFkMTPEmk)
- **GitHub Issues**: Report bugs and request features
- **Discussions**: Community discussions and support

## 📄 License

YAZE is distributed under the [GNU GPL v3](https://www.gnu.org/licenses/gpl-3.0) license.

Third-party libraries (SDL2, ImGui, Abseil) are subject to their respective licenses.

## 🙏 Acknowledgments

Takes inspiration from:
- [Hyrule Magic](https://www.romhacking.net/utilities/200/) - Original Zelda 3 editor
- [ZScream](https://github.com/Zarby89/ZScreamDungeon) - Dungeon editing capabilities
- [Asar](https://github.com/RPGHacker/asar) - 65816 assembler integration

## 📸 Screenshots

![YAZE GUI Editor](https://github.com/scawful/yaze/assets/47263509/8b62b142-1de4-4ca4-8c49-d50c08ba4c8e)

![Dungeon Editor](https://github.com/scawful/yaze/assets/47263509/d8f0039d-d2e4-47d7-b420-554b20ac626f)

![Overworld Editor](https://github.com/scawful/yaze/assets/47263509/34b36666-cbea-420b-af90-626099470ae4)

---

**Ready to hack Zelda 3? [Get started now!](docs/01-getting-started.md)**