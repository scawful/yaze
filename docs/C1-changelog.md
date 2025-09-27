# Changelog

## 0.3.1 

- Minor bug fixes for color themes, ZSCustomOverworld v3 item loading, and Tile16 selection.

## 0.3.0 (September 2025)

### Major Features
- **Complete Theme Management System**: 5+ built-in themes with custom theme creation and editing
- **Multi-Session Workspace**: Work with multiple ROMs simultaneously in enhanced docked interface
- **Enhanced Welcome Screen**: Themed interface with quick access to all editors and features
- **Asar 65816 Assembler Integration**: Complete cross-platform ROM patching with assembly code
- **ZSCustomOverworld v3**: Full integration with enhanced overworld editing capabilities
- **Advanced Message Editing**: Enhanced text editing interface with improved parsing and real-time preview
- **GUI Docking System**: Improved docking and workspace management for better user workflow
- **Symbol Extraction**: Extract symbol names and opcodes from assembly files
- **Modernized Build System**: Upgraded to CMake 3.16+ with target-based configuration

### User Interface & Theming
- **Built-in Themes**: Classic YAZE, YAZE Tre, Cyberpunk, Sunset, Forest, and Midnight themes
- **Theme Editor**: Complete custom theme creation with save-to-file functionality
- **Animated Background Grid**: Optional moving grid with color breathing effects
- **Theme Import/Export**: Share custom themes with the community
- **Responsive UI**: All UI elements properly adapt to selected themes

### Enhancements
- **Enhanced CLI Tools**: Improved z3ed with modern command line interface and TUI
- **CMakePresets**: Added development workflow presets for better productivity
- **Cross-Platform CI/CD**: Multi-platform automated builds and testing with lenient code quality checks
- **Professional Packaging**: NSIS, DMG, and DEB/RPM installers
- **ROM-Dependent Testing**: Separated testing infrastructure for CI compatibility with 46+ core tests
- **Comprehensive Documentation**: Updated guides, help menus, and API documentation

### Technical Improvements
- **Modern C++23**: Latest language features for performance and safety
- **Memory Safety**: Enhanced memory management with RAII and smart pointers
- **Error Handling**: Improved error handling using absl::Status throughout
- **Cross-Platform**: Consistent experience across Windows, macOS, and Linux
- **Performance**: Optimized rendering and data processing

### Bug Fixes
- **Graphics Arena Crash**: Fixed double-free error during Arena singleton destruction
- **SNES Tile Format**: Corrected tile unpacking algorithm based on SnesLab documentation
- **Palette System**: Fixed color conversion functions (ImVec4 float to uint8_t conversion)
- **CI/CD**: Fixed missing cstring include for Ubuntu compilation
- **ROM Loading**: Fixed file path issues in tests

## 0.2.2 (December 2024)
- DungeonMap editing improvements
- ZSCustomOverworld support
- Cross platform file handling

## 0.2.1 (August 2024)
- Improved MessageEditor parsing
- Added integration test window
- Bitmap bug fixes

## 0.2.0 (July 2024)
- iOS app support
- Graphics Sheet Browser
- Project Files

## 0.1.0 (May 2024)
- Bitmap bug fixes
- Error handling improvements

## 0.0.9 (April 2024)
- Documentation updates
- Entrance tile types
- Emulator subsystem overhaul

## 0.0.8 (February 2024)
- Hyrule Magic Compression
- Dungeon Room Entrances
- PNG Export

## 0.0.7 (January 2024)
- OverworldEntities
  - Entrances
  - Exits
  - Items
  - Sprites

## 0.0.6 (November 2023)
- ScreenEditor DungeonMap
- Tile16 Editor
- Canvas updates

## 0.0.5 (November 2023)
- DungeonEditor
- DungeonObjectRenderer

## 0.0.4 (November 2023)
- Tile16Editor
- GfxGroupEditor
- Add GfxGroups functions to Rom
- Add Tile16Editor and GfxGroupEditor to OverworldEditor

## 0.0.3 (October 2023)
- Emulator subsystem
  - SNES PPU and PPURegisters
