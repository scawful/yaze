# vcpkg Port Overlays

This directory contains custom vcpkg port overlays that modify the behavior of specific packages during installation.

## Purpose

vcpkg overlays allow us to patch specific ports without forking the entire vcpkg repository. These overlays are automatically used when `VCPKG_OVERLAY_PORTS` is set in the CMake configuration.

## Current Overlays

### sdl2/

**Problem**: The official SDL2 vcpkg port runs `vcpkg_fixup_pkgconfig()` on Windows, which attempts to download `pkgconf` from MSYS2 mirrors. These URLs frequently return 404 errors, causing build failures.

**Solution**: Patched portfile that skips pkgconfig fixup on Windows platforms while maintaining it for Linux/macOS where it's needed.

**Impact**: Prevents SDL2 installation failures on Windows without affecting functionality (pkgconfig isn't needed for Windows static builds).

## Usage

The overlays are automatically applied via the GitHub Actions workflows:

```cmake
-DVCPKG_OVERLAY_PORTS=${github.workspace}/cmake/overlays
```

For local development on Windows:
```bash
cmake -B build -DVCPKG_OVERLAY_PORTS=cmake/overlays ...
```

## Maintenance

- Overlays are version-specific (currently SDL2 2.30.9)
- When updating SDL2 version in vcpkg.json, check if the overlay needs updating
- Monitor upstream vcpkg for fixes that would allow removing these overlays

