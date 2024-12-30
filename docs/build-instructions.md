# Build Instructions

For VSCode users, use the following CMake extensions

- https://marketplace.visualstudio.com/items?itemName=twxs.cmake
- https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools

Yaze uses CMake to build the project. If you are unexperienced with CMake, please refer to the [CMake documentation](https://cmake.org/documentation/).

The gui editor is built using SDL2 and ImGui. For reference on how to use ImGui, see the [Getting Started](https://github.com/ocornut/imgui/wiki/Getting-Started) guide. For SDL2, see the [SDL2 documentation](https://wiki.libsdl.org/).

For those who want to reduce compile times, consider installing the dependencies on your system.

## Windows

### vcpkg

For Visual Studio users, follow the [Install and use packages with CMake](https://learn.microsoft.com/en-us/vcpkg/get_started/get-started) tutorial from Microsoft.

Define the following dependencies in `vcpkg.json`

```
{
  "dependencies": [
    "abseil",
    "sdl2",
    "libpng"
  ]
}
```

Target the architecture in `CMakePresets.json`

```
{
  "name": "vcpkg",
  "generator": "Ninja",
  "binaryDir": "${sourceDir}/build",
  "architecture": {
    "value": "arm64/x64",
    "strategy": "external"
  },
  "cacheVariables": {
    "CMAKE_TOOLCHAIN_FILE": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake",
    "CMAKE_SYSTEM_PROCESSOR": "arm64/x64"
  }
}
```

### msys2

[msys2](https://www.msys2.org/) is an alternative you may use for a Unix-like environment on Windows. Beware that this is for more experienced developers who know how to manage their system PATH.

Add to environment variables `C:\msys64\mingw64\bin`

Install the following packages using `pacman -S <package-name>`

- `mingw-w64-x86_64-gcc`
- `mingw-w64-x86_64-gcc-libs`
- `mingw-w64-x86_64-cmake`
- `mingw-w64-x86_64-sdl2`
- `mingw-w64-x86_64-libpng`
- `mingw-w64-x86_64-abseil-cpp`

For `yaze_py` you will need Boost Python

- `mingw-w64-x86_64-boost`

# macOS

Prefer to use clang provided with XCode command line tools over gcc.

Install the following packages using `brew install <package-name>`

- `cmake`
- `sdl2`
- `zlib`
- `libpng`
- `abseil`
- `boost-python3`

# iOS

Xcode is required to build for iOS. Currently testing with iOS 18 on iPad Pro.

The xcodeproject file is located in the `ios` directory.

You will need to link `SDL2.framework` and `libpng.a` to the project.

# GNU/Linux

You can use your package manager to install the same dependencies as macOS.

I trust you know how to use your package manager.
