# Platform Compatibility Improvements

Recent improvements to ensure YAZE works reliably across all supported platforms.

## Native File Dialog Support

YAZE now features native file dialogs on all platforms:
- **macOS**: Cocoa-based file selection with proper sandboxing support
- **Windows**: Windows Explorer integration with COM APIs
- **Linux**: GTK3 dialogs that match system appearance
- **Fallback**: Bespoke implementation when native dialogs unavailable

## Filesystem Abstraction

To ensure robust and consistent behavior across platforms, YAZE has standardized its filesystem operations:

-   **`std::filesystem`**: All new and refactored code uses the C++17 `std::filesystem` library for path manipulation, directory iteration, and file operations. This eliminates a major source of platform-specific bugs related to path separators (`/` vs `\`) and other inconsistencies.

-   **`PlatformPaths` Utility**: A dedicated utility class, `yaze::util::PlatformPaths`, provides a centralized and platform-aware API for retrieving standard directory locations. It correctly resolves paths for:
    -   **Application Data**: Uses `%APPDATA%` on Windows, `~/Library/Application Support` on macOS, and the XDG Base Directory Specification on Linux.
    -   **Configuration Files**: Provides a semantically clear API for config file locations.
    -   **Home and Temporary Directories**: Safely resolves user-specific and temporary folders.

This strategy removes legacy, platform-specific APIs (like `dirent.h` or Win32 directory functions) from the application codebase, leading to cleaner, more maintainable, and more reliable file handling.

## Build Configuration Options

YAZE supports different build configurations for various use cases:

### Full Build (Development)
Includes all features: emulator, CLI tools, UI testing framework, and optional libraries.

### Minimal Build  
Streamlined build excluding complex components, optimized for automated testing and CI environments.

## Implementation Details

The build system automatically detects platform capabilities and adjusts feature sets accordingly:

- **File Dialogs**: Uses native platform dialogs when available, with cross-platform fallbacks
- **Dependencies**: Bundles all required libraries to eliminate external package requirements  
- **Testing**: Separates ROM-dependent tests from unit tests for CI compatibility
- **Architecture**: Supports both Intel and Apple Silicon on macOS without conflicts

## Platform-Specific Adaptations

### Windows
- Complete COM-based file dialog implementation
- MSVC compatibility improvements for modern C++ features
- Resource file handling for proper application integration

### macOS  
- Cocoa-based native file dialogs with sandboxing support
- Universal binary support for Intel and Apple Silicon
- Proper bundle configuration for macOS applications

### Linux
- GTK3 integration for native file dialogs
- Package manager integration for system dependencies
- Support for multiple compiler toolchains (GCC, Clang)
