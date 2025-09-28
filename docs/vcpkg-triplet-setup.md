# Installing vcpkg Triplets for Windows

This guide explains how to install the `x64-windows` triplet that's required for building YAZE on Windows.

## What is a vcpkg Triplet?

A triplet defines the target platform, architecture, and linking configuration for vcpkg packages. The `x64-windows` triplet is the most common one for 64-bit Windows development.

## Method 1: Install via Package (Recommended)

The easiest way to ensure the triplet is available is to install any package with that triplet:

```cmd
# Navigate to your vcpkg directory
cd C:\path\to\your\vcpkg

# Install a package with the x64-windows triplet
vcpkg install sdl2:x64-windows
```

This will automatically create the triplet configuration if it doesn't exist.

## Method 2: Create Triplet File Manually

If you need to create the triplet configuration manually:

1. **Navigate to vcpkg triplets directory:**
   ```cmd
   cd C:\path\to\your\vcpkg\triplets
   ```

2. **Create or verify `x64-windows.cmake` exists:**
   ```cmd
   dir x64-windows.cmake
   ```

3. **If it doesn't exist, create it with this content:**
   ```cmake
   set(VCPKG_TARGET_ARCHITECTURE x64)
   set(VCPKG_CRT_LINKAGE dynamic)
   set(VCPKG_LIBRARY_LINKAGE dynamic)
   
   set(VCPKG_CMAKE_SYSTEM_NAME Windows)
   ```

## Method 3: Check Available Triplets

To see what triplets are currently available on your system:

```cmd
vcpkg help triplet
```

Or list all available triplet files:

```cmd
vcpkg help triplet | findstr "Available"
```

## Method 4: Install YAZE Dependencies

Since YAZE uses several vcpkg packages, installing them will ensure the triplet is properly set up:

```cmd
# From the YAZE project root
vcpkg install --triplet x64-windows sdl2 zlib libpng abseil
```

## Common Issues and Solutions

### Issue: "Invalid triplet"
**Solution:** Make sure vcpkg is properly installed and in your PATH:
```cmd
vcpkg version
```

### Issue: "Triplet not found"
**Solution:** Install a package with that triplet first:
```cmd
vcpkg install zlib:x64-windows
```

### Issue: "Permission denied"
**Solution:** Run Command Prompt as Administrator, or install vcpkg in a user-writable location.

## Alternative Triplets

If `x64-windows` doesn't work, you can try these alternatives:

- `x64-windows-static` - Static linking
- `x86-windows` - 32-bit Windows
- `x64-windows-static-md` - Static runtime, dynamic CRT

## Verification

To verify the triplet is working:

```cmd
vcpkg list --triplet x64-windows
```

This should show installed packages for that triplet.

## For YAZE Build

Once the triplet is installed, you can build YAZE using CMake presets:

```cmd
cmake --preset=windows-release
cmake --build build --config Release
```

Or with the Visual Studio solution:
```cmd
# Open yaze.sln in Visual Studio
# Build normally (F5 or Ctrl+Shift+B)
```
