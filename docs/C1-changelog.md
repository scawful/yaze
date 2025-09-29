# Changelog

## 0.3.2 (December 2025) - In Development

### Tile16 Editor Improvements (In Progress)

**Palette System Enhancements:**
- **Comprehensive Palette Coordination**: Enhanced tile16 editor now properly coordinates with overworld palette system
- **Sheet-Based Palette Mapping**: Implemented sophisticated palette mapping where different graphics sheets use appropriate palette groups:
  - Sheets 0, 3-6: AUX palette group
  - Sheets 1-2: MAIN palette group  
  - Sheet 7: ANIMATED palette group
- **Enhanced UI Layout**: Improved scrollable layout with better organization and visual clarity
- **Right-Click Tile Picking**: New feature to pick tiles directly from the tile16 canvas for quick editing
- **Save/Discard Workflow**: Implemented proper save/discard workflow that prevents ROM changes until explicit user confirmation

**Known Issues:**
- ⚠️ Palette display still has some errors with certain sheet configurations (work in progress)
- Some edge cases in palette group selection need refinement

### Graphics System Optimizations

**Performance Improvements:**
- **Segmentation Fault Resolution**: Fixed critical crashes in tile16 editor caused by tile cache system using `std::move()` operations that invalidated Bitmap surface pointers
- **Direct SDL Texture Updates**: Disabled problematic tile cache and implemented direct texture update system for improved stability
- **Comprehensive Bounds Checking**: Added extensive bounds checking throughout graphics pipeline to prevent crashes and palette corruption
- **Surface/Texture Pooling**: Implemented graphics optimizations including surface/texture pooling while maintaining system stability
- **Performance Profiling**: Added performance monitoring and profiling capabilities for graphics operations

### Windows Platform Stability

**Build System Fixes:**
- **Stack Overflow Fix**: Increased Windows stack size from 1MB to 8MB to match macOS/Linux defaults
  - Prevents crashes during `EditorManager::LoadAssets()` which loads 223 graphics sheets
  - Handles deep call stacks from multiple editor initializations
  - Applied to both `yaze` executable and `yaze_test` test suite
- **Development Utility Fixes**: Fixed linker errors in `extract_vanilla_values` and `rom_patch_utility` executables
  - Resolved multiple `main()` definition conflicts
  - Added proper `yaze_core` library linkage
  - Prevented CI/release builds from attempting to build development-only utilities
- **Consistent Cross-Platform Behavior**: Windows builds now have equivalent stack resources and stability as Unix-like systems

### Memory Safety & Stability

**Critical Fixes:**
- **Bitmap Surface Invalidation**: Root cause analysis and fix for segmentation faults in graphics rendering
- **Tile Cache System**: Disabled move semantics in tile cache that caused pointer invalidation
- **Memory Management**: Enhanced RAII patterns and smart pointer usage throughout graphics pipeline
- **Bounds Verification**: Added comprehensive bounds checking for tile and palette access

### Testing & CI/CD Improvements

**Test Infrastructure:**
- **Windows Test Reliability**: Fixed test suite crashes by increasing stack size
- **Development-Only Builds**: Properly isolated development utilities from CI/release builds
- **Better Error Reporting**: Enhanced error messages for Windows build failures
- **Cross-Platform Consistency**: Ensured consistent test behavior across all platforms

### Future Optimizations (Planned)

**Graphics System:**
- Lazy loading of graphics sheets (load on-demand rather than all at once)
- Heap-based allocation for large data structures instead of stack
- Streaming/chunked loading for large ROM assets
- Consider if all 223 sheets need to be in memory simultaneously

**Build System:**
- Further reduce CI build times
- Enhanced dependency caching strategies
- Improved vcpkg integration reliability

### Technical Notes

**Breaking Changes:**
- None - this is a patch release focused on stability and fixes

**Deprecations:**
- None

**Migration Guide:**
- No migration required - this release is fully backward compatible with 0.3.1

## 0.3.1 (September 2025)

### Major Features
- **Complete Tile16 Editor Overhaul**: Professional-grade tile editing with modern UI and advanced capabilities
- **Advanced Palette Management**: Full access to all SNES palette groups with configurable normalization
- **Comprehensive Undo/Redo System**: 50-state history with intelligent time-based throttling
- **ZSCustomOverworld v3 Full Support**: Complete implementation of ZScream Save.cs functionality with complex transition calculations
- **ZEML System Removal**: Converted overworld editor from markup to pure ImGui for better performance and maintainability
- **OverworldEditorManager**: New management system to handle complex v3 overworld features

### Tile16 Editor Enhancements
- **Modern UI Layout**: Fully resizable 3-column interface (Tile8 Source, Editor, Preview & Controls)
- **Multi-Palette Group Support**: Access to Overworld Main/Aux1/Aux2, Dungeon Main, Global Sprites, Armors, and Swords palettes
- **Advanced Transform Operations**: Flip horizontal/vertical, rotate 90°, fill with tile8, clear operations
- **Professional Workflow**: Copy/paste, 4-slot scratch space, live preview with auto-commit
- **Pixel Normalization Settings**: Configurable pixel value masks (0x01-0xFF) for handling corrupted graphics sheets

### ZSCustomOverworld v3 Implementation
- **SaveLargeMapsExpanded()**: Complex neighbor-aware transition calculations for all area sizes (Small, Large, Wide, Tall)
- **Interactive Overlay System**: Full `SaveMapOverlays()` with ASM code generation for revealing holes and changing map elements
- **SaveCustomOverworldASM()**: Complete custom overworld ASM application with feature toggles and data tables
- **Expanded Memory Support**: Automatic detection and use of v3 expanded memory locations (0x140xxx)
- **Area-Specific Features**: Background colors, main palettes, mosaic transitions, GFX groups, subscreen overlays, animated tiles
- **Transition Logic**: Sophisticated camera transition calculations based on neighboring area types and quadrants
- **Version Compatibility**: Maintains vanilla/v2 compatibility while adding full v3+ feature support

### Technical Improvements
- **SNES Data Accuracy**: Proper 4-bit palette index handling with configurable normalization
- **Bitmap Pipeline Fixes**: Corrected tile16 extraction using `GetTilemapData()` with manual fallback
- **Real-time Updates**: Immediate visual feedback for all editing operations
- **Memory Safety**: Enhanced bounds checking and error handling throughout
- **ASM Version Detection**: Automatic detection of custom overworld ASM version for feature availability
- **Conditional Save Logic**: Different save paths for vanilla, v2, and v3+ ROMs

### User Interface
- **Keyboard Shortcuts**: Comprehensive shortcuts for all operations (H/V/R for transforms, Q/E for palette cycling, 1-8 for direct palette selection)
- **Visual Feedback**: Hover preview restoration, current palette highlighting, texture status indicators
- **Compact Controls**: Streamlined property panel with essential tools easily accessible
- **Settings Dialog**: Advanced palette normalization controls with real-time application
- **Pure ImGui Layout**: Removed ZEML markup system in favor of native ImGui tabs and tables for better performance
- **v3 Settings Panel**: Dedicated UI for ZSCustomOverworld v3 features with ASM version detection and feature toggles

### Bug Fixes
- **Tile16 Bitmap Display**: Fixed blank/white tile issue caused by unnormalized pixel values
- **Hover Preview**: Restored tile8 preview when hovering over tile16 canvas
- **Canvas Scaling**: Corrected coordinate scaling for 8x magnification factor
- **Palette Corruption**: Fixed high-bit contamination in graphics sheets
- **UI Layout**: Proper column sizing and resizing behavior
- **Linux CI/CD Build**: Fixed undefined reference errors for `ShowSaveFileDialog` method
- **ZSCustomOverworld v3**: Fixed complex area transition calculations and neighbor-aware tilemap adjustments
- **ZEML Performance**: Eliminated markup parsing overhead by converting to native ImGui components

### ZScream Compatibility Improvements
- **Complete Save.cs Implementation**: All major methods from ZScream's Save.cs now implemented in YAZE
- **Area Size Support**: Full support for Small, Large, Wide, and Tall area types with proper transitions
- **Interactive Overlays**: Complete overlay save system matching ZScream's functionality  
- **Custom ASM Integration**: Proper handling of ZSCustomOverworld ASM versions 1-3+
- **Memory Layout**: Correct usage of expanded vs vanilla memory locations based on ROM type

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
