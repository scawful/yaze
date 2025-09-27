# Changelog

## 0.3.1 

### Major Features
- **Complete Tile16 Editor Overhaul**: Professional-grade tile editing with modern UI and advanced capabilities
- **Advanced Palette Management**: Full access to all SNES palette groups with configurable normalization
- **Comprehensive Undo/Redo System**: 50-state history with intelligent time-based throttling
- **Scratch Space for Overworld**: Not yet compatible with ZScream

### Tile16 Editor Enhancements
- **Modern UI Layout**: Fully resizable 3-column interface (Tile8 Source, Editor, Preview & Controls)
- **Multi-Palette Group Support**: Access to Overworld Main/Aux1/Aux2, Dungeon Main, Global Sprites, Armors, and Swords palettes
- **Advanced Transform Operations**: Flip horizontal/vertical, rotate 90°, fill with tile8, clear operations
- **Professional Workflow**: Copy/paste, 4-slot scratch space, live preview with auto-commit
- **Pixel Normalization Settings**: Configurable pixel value masks (0x01-0xFF) for handling corrupted graphics sheets

### Technical Improvements
- **SNES Data Accuracy**: Proper 4-bit palette index handling with configurable normalization
- **Bitmap Pipeline Fixes**: Corrected tile16 extraction using `GetTilemapData()` with manual fallback
- **Real-time Updates**: Immediate visual feedback for all editing operations
- **Memory Safety**: Enhanced bounds checking and error handling throughout

### User Interface
- **Keyboard Shortcuts**: Comprehensive shortcuts for all operations (H/V/R for transforms, Q/E for palette cycling, 1-8 for direct palette selection)
- **Visual Feedback**: Hover preview restoration, current palette highlighting, texture status indicators
- **Compact Controls**: Streamlined property panel with essential tools easily accessible
- **Settings Dialog**: Advanced palette normalization controls with real-time application

### Bug Fixes
- **Tile16 Bitmap Display**: Fixed blank/white tile issue caused by unnormalized pixel values
- **Hover Preview**: Restored tile8 preview when hovering over tile16 canvas
- **Canvas Scaling**: Corrected coordinate scaling for 8x magnification factor
- **Palette Corruption**: Fixed high-bit contamination in graphics sheets
- **UI Layout**: Proper column sizing and resizing behavior
- **Linux CI/CD Build**: Fixed undefined reference errors for `ShowSaveFileDialog` method

- Minor bug fixes for color themes and ZSCustomOverworld v3 item loading.

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
