# Changelog

## 0.3.3 (October 2025) - IN FLUX

**Note**: Versions 0.3.2-0.3.3 experienced CI/CD issues with Windows releases. See B4-release-workflows.md for details on the fixes applied.

### Rendering Pipeline Fixes

**Graphics Editor White Sheets Fixed**:
- Graphics sheets now receive appropriate default palettes during ROM loading
- Sheets 0-112: Dungeon main palettes
- Sheets 113-127: Sprite palettes
- Sheets 128-222: HUD/menu palettes
- Eliminated white/blank graphics on initial load

**Message Editor Preview Updates**:
- Fixed static message preview issue where changes weren't visible
- Corrected `mutable_data()` usage to `set_data()` for proper SDL surface synchronization
- Message preview now updates in real-time when selecting or editing messages

**Cross-Editor Graphics Synchronization**:
- Added `Arena::NotifySheetModified()` for centralized texture management
- Graphics changes in one editor now propagate to all other editors
- Improves workflow when editing graphics that appear in multiple contexts

**Logging System Migration**:
- Replaced raw `printf()` calls with structured `LOG_*` macros throughout graphics pipeline
- Better integration with logging system for debugging

### Card-Based UI System

**EditorCardManager**:
- Centralized card registration and visibility management
- Context-sensitive card controls in main menu bar
- Category-based keyboard shortcuts (Ctrl+Shift+D for Dungeon, etc.)
- Card browser for visual card management (Ctrl+Shift+B)

**Editor Integration**:
- DungeonEditor, GraphicsEditor, ScreenEditor, SpriteEditor, OverworldEditor, AssemblyEditor, MessageEditor, and Emulator now use card system
- Cards can be closed with X button like normal windows
- Proper docking behavior across all editors
- Cards hidden by default to prevent crashes on ROM load

## 0.3.2 (October 2025) - IN FLUX

**Note**: CI/CD issues with Windows releases. Fixes implemented in 0.3.3.

## 0.3.1 (October 2025)

### Emulator: Audio System Infrastructure ✅ COMPLETE

**Audio Backend Abstraction:**
- **IAudioBackend Interface**: Clean abstraction layer for audio implementations, enabling easy migration between SDL2, SDL3, and custom backends
- **SDL2AudioBackend**: Complete implementation with volume control, status queries, and smart buffer management (2-6 frames)
- **AudioBackendFactory**: Factory pattern for creating backends with minimal coupling
- **Benefits**: Future-proof audio system, easy to add platform-native backends (CoreAudio, WASAPI, PulseAudio)

**APU Debugging System:**
- **ApuHandshakeTracker**: Monitors CPU-SPC700 communication in real-time
- **Phase Tracking**: Tracks handshake progression (RESET → IPL_BOOT → WAITING_BBAA → HANDSHAKE_CC → TRANSFER_ACTIVE → RUNNING)
- **Port Activity Monitor**: Records last 1000 port write events with PC addresses
- **Visual Debugger UI**: Real-time phase display, port activity log, transfer progress bars, force handshake testing
- **Integration**: Connected to both CPU (Snes::WriteBBus) and SPC700 (Apu::Write) port operations

**Music Editor Integration:**
- **Live Playback**: `PlaySong(int song_id)` triggers songs via $7E012C memory write
- **Volume Control**: `SetVolume(float)` controls backend volume at abstraction layer
- **Playback Controls**: Stop/pause/resume functionality ready for UI integration

**Documentation:**
- Created comprehensive audio system guides covering IPL ROM protocol, handshake debugging, and testing procedures

### Emulator: Critical Performance Fixes

**Console Logging Performance Killer Fixed:**
- **Issue**: Console logging code was executing on EVERY instruction even when disabled, causing severe performance degradation (< 1 FPS)
- **Impact**: ~1,791,000 console writes per second with mutex locks and buffer flushes
- **Fix**: Removed 73 lines of console output from CPU instruction execution hot path
- **Result**: Emulator now runs at full 60 FPS

**Instruction Logging Default Changed:**
- **Changed**: `kLogInstructions` flag default from `true` to `false`
- **Reason**: Even without console spam, logging every instruction to DisassemblyViewer caused significant slowdown
- **Impact**: No logging overhead unless explicitly enabled by user

**Instruction Log Unbounded Growth Fixed:**
- **Issue**: Legacy `instruction_log_` vector growing to 60+ million entries after 10 minutes, consuming 6GB+ RAM
- **Fix**: Added automatic trimming to 10,000 most recent instructions
- **Result**: Memory usage stays bounded at ~50MB

**Audio Buffer Allocation Bug Fixed:**
- **Issue**: Audio buffer allocated as single `int16_t` instead of array, causing immediate buffer overflow
- **Fix**: Properly allocate as array using `new int16_t[size]` with custom deleter
- **Result**: Audio system can now queue samples without corruption

### Emulator: UI Organization & Input System

**New UI Architecture:**
- **Created `src/app/emu/ui/` directory** for separation of concerns
- **EmulatorUI Layer**: Separated all ImGui rendering code from emulator logic
- **Input Abstraction**: `IInputBackend` interface with SDL2 implementation for future SDL3 migration
- **InputHandler**: Continuous polling system using `SDL_GetKeyboardState()` instead of event-based ImGui keys

**Keyboard Input Fixed:**
- **Issue**: Event-based `ImGui::IsKeyPressed()` only fires once per press, doesn't work for held buttons
- **Fix**: New `InputHandler` uses continuous SDL keyboard state polling every frame
- **Result**: Proper game controls with held button detection

**DisassemblyViewer Enhancement:**
- **Sparse Address Map**: Mesen-style storage of unique addresses only, not every execution
- **Execution Counter**: Increments on re-execution for hotspot analysis
- **Performance**: Tracks millions of instructions with ~5MB RAM vs 6GB+ with old system
- **Always Active**: No need for toggle flag, efficiently active by default

**Feature Flags Cleanup:**
- Removed deprecated `kLogInstructions` flag entirely
- DisassemblyViewer now always active with zero performance cost

### Debugger: Breakpoint & Watchpoint Systems

**BreakpointManager:**
- **CRUD Operations**: Add/Remove/Enable/Disable breakpoints with unique IDs
- **Breakpoint Types**: Execute, Read, Write, Access, and Conditional breakpoints
- **Dual CPU Support**: Separate tracking for 65816 CPU and SPC700
- **Hit Counting**: Tracks how many times each breakpoint is triggered
- **CPU Integration**: Connected to CPU execution via callback system

**WatchpointManager:**
- **Memory Access Tracking**: Monitor reads/writes to memory ranges
- **Range-Based**: Watch single addresses or memory regions ($7E0000-$7E00FF)
- **Access History**: Deque-based storage of last 1000 memory accesses
- **Break-on-Access**: Optional execution pause when watchpoint triggered
- **Export**: CSV export of access history for analysis

**CPU Debugger UI Enhancements:**
- **Integrated Controls**: Play/Pause/Step/Reset buttons directly in debugger window
- **Breakpoint UI**: Address input (hex), add/remove buttons, enable/disable checkboxes, hit count display
- **Live Disassembly**: DisassemblyViewer showing real-time execution
- **Register Display**: Real-time CPU state (A, X, Y, D, SP, PC, PB, DB, flags)

### Build System Simplifications

**Eliminated Conditional Compilation:**
- **Before**: Optional flags for JSON (`YAZE_WITH_JSON`), gRPC (`YAZE_WITH_GRPC`), AI (`Z3ED_AI`)
- **After**: All features always enabled, no configuration required
- **Benefits**: Simpler development, easier onboarding, fewer ifdef-related bugs, consistent builds across all platforms
- **Build Command**: Just `cmake -B build && cmake --build build` - no flags needed!

**DisassemblyViewer Performance Limits:**
- Max 10,000 instructions stored (prevents memory bloat)
- Auto-trim to 8,000 when limit reached (keeps hottest code paths)
- Toggle recording on/off for performance testing
- Clear button to free memory

### Build System: Windows Platform Improvements

**gRPC v1.67.1 Upgrade:**
- **Issue**: v1.62.0 had template instantiation errors on MSVC
- **Fix**: Upgraded to v1.67.1 with MSVC template fixes and better C++17/20 compatibility
- **Result**: Builds successfully on Visual Studio 2022

**MSVC-Specific Compiler Flags:**
- `/bigobj` - Allow large object files (gRPC generates many)
- `/permissive-` - Standards conformance mode
- `/wd4267 /wd4244` - Suppress harmless conversion warnings
- `/constexpr:depth2048` - Handle deep template instantiations

**Cross-Platform Validation:**
- All new audio and input code uses cross-platform SDL2 APIs
- No platform-specific code in audio backend or input abstraction
- Ready for SDL3 migration with minimal changes

### GUI & UX Modernization
- **Theme System**: Implemented a comprehensive theme system (`AgentUITheme`) that centralizes all UI colors. All Agent UI components are now theme-aware, deriving colors from the main application theme.
- **UI Helper Library**: Created a library of 30+ reusable UI helper functions (`AgentUI::*` and `gui::*`) to standardize panel styles, section headers, status indicators, and buttons, reducing boilerplate code by over 50%.
- **Visual Polish**: Enhanced numerous UI panels (Agent Chat, Test Harness, Collaboration) with theme-aware colors, status badges (PASS/FAIL/RUN), connection indicators, and provider badges (Ollama/Gemini).

### Overworld Editor Refactoring
- **Modular Architecture**: Refactored the 3,400-line `OverworldEditor` into smaller, focused modules, including `OverworldEntityRenderer`.
- **Progressive Loading**: Implemented a priority-based progressive loading system in the central `gfx::Arena` to prevent UI freezes during asset loading. This benefits all editors.
- **Critical Graphics Fixes**: Resolved major bugs related to graphics not refreshing immediately, multi-quadrant map updates, and incorrect feature visibility on vanilla ROMs.
- **Multi-Area Map Configuration**: Implemented a robust `ConfigureMultiAreaMap()` method to correctly handle parent/child relationships for all area sizes (Small, Large, Wide, Tall).

### Build System & Stability
- **Build Fixes**: Resolved 7 critical build errors, including linker issues, missing virtual methods, and filesystem crashes.
- **C API Separation**: Decoupled the C API library from the main application to improve build modularity.


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
- **File Dialog Fallback Implementation**: Fixed non-functional file dialogs in minimal builds
  - Implemented proper Windows COM-based `IFileOpenDialog`/`IFileSaveDialog` fallback
  - Previously returned empty string when NFD (Native File Dialog) was unavailable
  - Now works in all Windows builds regardless of vcpkg/NFD availability
  - Supports file open, file save, and folder selection dialogs
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
