# Windows Development Guide for YAZE

This guide will help you set up a Windows development environment for YAZE and build the project successfully.

## Prerequisites

### Required Software

1. **Visual Studio 2022** (Community, Professional, or Enterprise)
   - Download from: https://visualstudio.microsoft.com/downloads/
   - Required workloads:
     - Desktop development with C++
     - Game development with C++ (optional, for additional tools)

2. **Git for Windows**
   - Download from: https://git-scm.com/download/win
   - Use default installation options

3. **Python 3.8 or later**
   - Download from: https://www.python.org/downloads/
   - Make sure to check "Add Python to PATH" during installation

### Optional Software

- **PowerShell 7** (recommended for better script support)
- **Windows Terminal** (for better terminal experience)

## Quick Setup

### Automated Setup

The easiest way to get started is to use our automated setup script:

```powershell
# Run from the YAZE project root directory
.\scripts\setup-windows-dev.ps1
```

This script will:
- Check for required software
- Set up vcpkg
- Install dependencies
- Generate Visual Studio project files
- Perform a test build

### Manual Setup

If you prefer to set up manually or the automated script fails:

#### 1. Clone the Repository

```bash
git clone https://github.com/your-username/yaze.git
cd yaze
```

#### 2. Set up vcpkg

```bash
# Clone vcpkg
git clone https://github.com/Microsoft/vcpkg.git vcpkg

# Bootstrap vcpkg
cd vcpkg
.\bootstrap-vcpkg.bat
cd ..

# Install dependencies
.\vcpkg\vcpkg.exe install --triplet x64-windows
```

#### 3. Generate Visual Studio Project Files

```bash
python scripts/generate-vs-projects.py
```

#### 4. Build the Project

```bash
# Using PowerShell script (recommended)
.\scripts\build-windows.ps1 -Configuration Release -Platform x64

# Or using batch script
.\scripts\build-windows.bat Release x64

# Or using Visual Studio
# Open YAZE.sln in Visual Studio 2022 and build
```

## Building the Project

### Using Visual Studio

1. Open `YAZE.sln` in Visual Studio 2022
2. Select your desired configuration:
   - **Debug**: For development and debugging
   - **Release**: For optimized builds
   - **RelWithDebInfo**: Release with debug information
   - **MinSizeRel**: Minimal size release
3. Select your platform:
   - **x64**: 64-bit (recommended)
   - **x86**: 32-bit
   - **ARM64**: ARM64 (if supported)
4. Build the solution (Ctrl+Shift+B)

### Using Command Line

#### PowerShell Script (Recommended)

```powershell
# Build Release x64 (default)
.\scripts\build-windows.ps1

# Build Debug x64
.\scripts\build-windows.ps1 -Configuration Debug -Platform x64

# Build Release x86
.\scripts\build-windows.ps1 -Configuration Release -Platform x86

# Clean build
.\scripts\build-windows.ps1 -Clean

# Verbose output
.\scripts\build-windows.ps1 -Verbose
```

#### Batch Script

```batch
REM Build Release x64 (default)
.\scripts\build-windows.bat

REM Build Debug x64
.\scripts\build-windows.bat Debug x64

REM Build Release x86
.\scripts\build-windows.bat Release x86
```

#### Direct MSBuild

```bash
# Build Release x64
msbuild YAZE.sln /p:Configuration=Release /p:Platform=x64 /p:VcpkgEnabled=true /p:VcpkgManifestInstall=true /m

# Build Debug x64
msbuild YAZE.sln /p:Configuration=Debug /p:Platform=x64 /p:VcpkgEnabled=true /p:VcpkgManifestInstall=true /m
```

## Project Structure

```
yaze/
├── YAZE.sln              # Visual Studio solution file
├── YAZE.vcxproj          # Visual Studio project file
├── vcpkg.json            # vcpkg dependencies
├── scripts/              # Build and setup scripts
│   ├── build-windows.ps1 # PowerShell build script
│   ├── build-windows.bat # Batch build script
│   ├── setup-windows-dev.ps1 # Setup script
│   └── generate-vs-projects.py # Project generator
├── src/                  # Source code
├── incl/                 # Public headers
├── assets/               # Game assets
└── docs/                 # Documentation
```

## Troubleshooting

### Common Issues

#### 1. MSBuild Not Found

**Error**: `'msbuild' is not recognized as an internal or external command`

**Solution**: 
- Install Visual Studio 2022 with C++ workload
- Or add MSBuild to your PATH:
  ```bash
  # Add to PATH (adjust path as needed)
  C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin
  ```

#### 2. vcpkg Integration Issues

**Error**: `vcpkg.json not found` or dependency resolution fails

**Solution**:
- Ensure vcpkg is properly set up:
  ```bash
  .\scripts\setup-windows-dev.ps1
  ```
- Or manually set up vcpkg as described in the manual setup section

#### 3. Python Script Execution Policy

**Error**: `execution of scripts is disabled on this system`

**Solution**:
```powershell
# Run as Administrator
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```

#### 4. Missing Dependencies

**Error**: Linker errors about missing libraries

**Solution**:
- Ensure all dependencies are installed via vcpkg:
  ```bash
  .\vcpkg\vcpkg.exe install --triplet x64-windows
  ```
- Regenerate project files:
  ```bash
  python scripts/generate-vs-projects.py
  ```

#### 5. Build Failures

**Error**: Compilation or linking errors

**Solution**:
- Clean and rebuild:
  ```powershell
  .\scripts\build-windows.ps1 -Clean
  ```
- Check that all source files are included in the project
- Verify that include paths are correct

### Getting Help

If you encounter issues not covered here:

1. Check the [main build instructions](02-build-instructions.md)
2. Review the [troubleshooting section](02-build-instructions.md#troubleshooting)
3. Check the [GitHub Issues](https://github.com/your-username/yaze/issues)
4. Create a new issue with:
   - Your Windows version
   - Visual Studio version
   - Complete error message
   - Steps to reproduce

## Development Workflow

### Making Changes

1. Make your changes to the source code
2. Build the project:
   ```powershell
   .\scripts\build-windows.ps1 -Configuration Debug -Platform x64
   ```
3. Test your changes
4. If adding new source files, regenerate project files:
   ```bash
   python scripts/generate-vs-projects.py
   ```

### Debugging

1. Set breakpoints in Visual Studio
2. Build in Debug configuration
3. Run with debugger (F5)
4. Use Visual Studio's debugging tools

### Testing

1. Build the project
2. Run the executable:
   ```bash
   .\build\bin\Debug\yaze.exe
   ```
3. Test with a ROM file:
   ```bash
   .\build\bin\Debug\yaze.exe --rom_file=path\to\your\rom.sfc
   ```

## Performance Tips

### Build Performance

- Use the `/m` flag for parallel builds
- Use SSD storage for better I/O performance
- Exclude build directories from antivirus scanning
- Use Release configuration for final builds

### Development Performance

- Use Debug configuration for development
- Use incremental builds (default in Visual Studio)
- Use RelWithDebInfo for performance testing with debug info

## Advanced Configuration

### Custom Build Configurations

You can create custom build configurations by modifying the Visual Studio project file or using CMake directly.

### Cross-Platform Development

While this guide focuses on Windows, YAZE also supports:
- Linux (Ubuntu/Debian)
- macOS

See the main build instructions for other platforms.

## Contributing

When contributing to YAZE on Windows:

1. Follow the [coding standards](B1-contributing.md)
2. Test your changes on Windows
3. Ensure the build scripts still work
4. Update documentation if needed
5. Submit a pull request

## Additional Resources

- [Visual Studio Documentation](https://docs.microsoft.com/en-us/visualstudio/)
- [vcpkg Documentation](https://vcpkg.readthedocs.io/)
- [CMake Documentation](https://cmake.org/documentation/)
- [YAZE API Reference](04-api-reference.md)
