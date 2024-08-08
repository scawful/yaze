# Build Instructions

For VSCode users, use the following CMake extensions

- https://marketplace.visualstudio.com/items?itemName=twxs.cmake
- https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools

For those who want to reduce compile times, consider installing the dependencies on your system. 

## Windows

Recommended to use [msys2](https://www.msys2.org/) for a Unix-like environment on Windows.

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