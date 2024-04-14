# Build Instructions

## Windows

For VSCode users, use the following CMake extensions with MinGW-w64

https://marketplace.visualstudio.com/items?itemName=twxs.cmake
https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools

https://www.msys2.org/

Add to environment variables `C:\msys64\mingw64\bin`

Install the following packages using `pacman -S <package-name>`

`mingw-w64-x86_64-gcc`
`mingw-w64-x86_64-gcc-libs`
`mingw-w64-x86_64-cmake`
`mingw-w64-x86_64-glew`
`mingw-w64-x86_64-lib-png`

# macOS

- Clang 15.0.1 x86_64-apple-darrwin22.5.0
- SDL2 Source v2.26.5
- Removed snes_spc
- Removed asar_static