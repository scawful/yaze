# yaze - Yet Another Zelda3 Editor

A modern, cross-platform editor for The Legend of Zelda: A Link to the Past ROM hacking, built with C++23 and featuring complete Asar 65816 assembler integration.

[![Build Status](https://github.com/scawful/yaze/workflows/CI/badge.svg)](https://github.com/scawful/yaze/actions)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

## Version 0.3.2 - Release

#### z3ed agent - AI-powered CLI assistant
- **AI-assisted ROM hacking** with ollama and Gemini support
- **Natural language commands** for editing and querying ROM data
- **Tool calling** for structured data extraction and modification
- **Interactive chat** with conversation history and context

#### ZSCustomOverworld v3
- **Enhanced overworld editing** capabilities
- **Advanced map properties** and metadata support
- **Custom graphics support** and tile management
- **Improved compatibility** with existing projects

#### Asar 65816 Assembler Integration
- **Cross-platform ROM patching** with assembly code support
- **Symbol extraction** with addresses and opcodes from assembly files
- **Assembly validation** with comprehensive error reporting
- **Modern C++ API** with safe memory management

#### Advanced Features
- **Theme Management**: Complete theme system with 5+ built-in themes and custom theme editor
- **Multi-Session Support**: Work with multiple ROMs simultaneously in docked workspace
- **Enhanced Welcome Screen**: Themed interface with quick access to all editors
- **Message Editing**: Enhanced text editing interface with real-time preview
- **GUI Docking**: Flexible workspace management with customizable layouts
- **Modern CLI**: Enhanced z3ed tool with interactive TUI and subcommands
- **Cross-Platform**: Full support for Windows, macOS, and Linux

## Quick Start

### Build
```bash
# Clone with submodules
git clone --recursive https://github.com/scawful/yaze.git
cd yaze

# Build with CMake
cmake --preset debug        # macOS
cmake -B build && cmake --build build  # Linux/Windows

# Windows-specific
scripts\verify-build-environment.ps1   # Verify your setup
cmake --preset windows-debug           # Basic build
cmake --preset windows-ai-debug        # With AI features
cmake --build build --config Debug     # Build
```

### Applications
- **yaze**: Complete GUI editor for Zelda 3 ROM hacking
- **z3ed**: Command-line tool with interactive interface
- **yaze_test**: Comprehensive test suite for development

## Usage

### GUI Editor
Launch the main application to edit Zelda 3 ROMs:
- Load ROM files using native file dialogs
- Edit overworld maps, dungeons, sprites, and graphics
- Apply assembly patches with integrated Asar support
- Export modifications as patches or modified ROMs

### Command Line Tool
```bash
# Apply assembly patch
z3ed asar patch.asm --rom=zelda3.sfc

# Extract symbols from assembly
z3ed extract patch.asm

# Interactive mode
z3ed --tui
```

### C++ API
```cpp
#include "yaze.h"

// Load ROM and apply patch
yaze_project_t* project = yaze_load_project("zelda3.sfc");
yaze_apply_asar_patch(project, "patch.asm");
yaze_save_project(project, "modified.sfc");
```

## Documentation

- [Getting Started](docs/01-getting-started.md) - Setup and basic usage
- [Build Instructions](docs/02-build-instructions.md) - Building from source
- [API Reference](docs/04-api-reference.md) - Programming interface
- [Contributing](docs/B1-contributing.md) - Development guidelines

**[Complete Documentation](docs/index.md)**

## Supported Platforms

- **Windows** (MSVC 2019+, MinGW)
- **macOS** (Intel and Apple Silicon)  
- **Linux** (GCC 13+, Clang 16+)
## ROM Compatibility

- Original Zelda 3 ROMs (US/Japan versions)
- ZSCustomOverworld v2/v3 enhanced overworld features
- Community ROM hacks and modifications

## Contributing

See [Contributing Guide](docs/B1-contributing.md) for development guidelines.

**Community**: [Oracle of Secrets Discord](https://discord.gg/MBFkMTPEmk)

## License

GNU GPL v3 - See [LICENSE](LICENSE) for details.

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