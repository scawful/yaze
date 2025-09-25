# Yet Another Zelda3 Editor

A modern, cross-platform editor for The Legend of Zelda: A Link to the Past ROM hacking.

- **Platform**: Windows, macOS, Linux
- **Language**: C++23 with modern CMake build system
- **Features**: ROM editing, Asar 65816 assembly patching, ZSCustomOverworld v3, GUI docking

## Key Features

- **Asar Integration**: Apply 65816 assembly patches and extract symbols
- **ZSCustomOverworld v3**: Enhanced overworld editing capabilities
- **Message Editing**: Advanced text editing with real-time preview  
- **GUI Docking**: Flexible workspace management
- **Modern CLI**: Enhanced z3ed tool with interactive TUI

Takes inspiration from [Hyrule Magic](https://www.romhacking.net/utilities/200/) and [ZScream](https://github.com/Zarby89/ZScreamDungeon)

## Building and Installation

### Quick Build
```bash
git clone --recurse-submodules https://github.com/scawful/yaze.git 
cd yaze
cmake --preset default
cmake --build --preset default
```

### Targets
- **yaze**: GUI Editor Application
- **z3ed**: Command Line Interface with Asar support
- **yaze_c**: C Library
- **yaze_test**: Unit Tests

### Asar Examples
```bash
# Apply assembly patch
z3ed asar patch.asm --rom=zelda3.sfc

# Extract symbols  
z3ed extract patch.asm

# Interactive TUI
z3ed --tui
```

See [build-instructions.md](docs/build-instructions.md) for detailed setup information.

## Documentation

- **[Getting Started](docs/getting-started.md)** - Setup and basic usage
- **[Asar Integration](docs/asar-integration.md)** - Assembly patching and symbol extraction
- **[Build Instructions](docs/build-instructions.md)** - Detailed build guide
- **[Contributing](docs/contributing.md)** - How to contribute
- **[Documentation Index](docs/index.md)** - Complete documentation overview

License
--------
YAZE is distributed under the [GNU GPLv3](https://www.gnu.org/licenses/gpl-3.0.txt) license.

SDL2, ImGui and Abseil are subject to respective licenses.

Screenshots
--------
![image](https://github.com/scawful/yaze/assets/47263509/8b62b142-1de4-4ca4-8c49-d50c08ba4c8e)

![image](https://github.com/scawful/yaze/assets/47263509/d8f0039d-d2e4-47d7-b420-554b20ac626f)

![image](https://github.com/scawful/yaze/assets/47263509/34b36666-cbea-420b-af90-626099470ae4)


