# Yet Another Zelda3 Editor

- Platform: Windows, macOS, iOS, GNU/Linux
- Dependencies: SDL2, ImGui, abseil-cpp

## Description

General purpose editor for The Legend of Zelda: A Link to the Past for the Super Nintendo.

Provides bindings in C and Python for building custom tools and utilities.

Takes heavy inspiration from ALTTP community efforts such as [Hyrule Magic](https://www.romhacking.net/utilities/200/) and [ZScream](https://github.com/Zarby89/ZScreamDungeon)

Building and installation
-------------------------
[CMake](http://www.cmake.org "CMake") is required to build yaze 

1. Clone the repository
2. Create the build directory and configuration
3. Build and run the application 
4. (Optional) Run the tests

```
  git clone --recurse-submodules https://github.com/scawful/yaze.git 
  cmake -S . -B build
  cmake --build build
```

By default this will build all targets. 

- **yaze**:       Editor Application
- **yaze_c**:     C Library
- **yaze_emu**:   SNES Emulator
- **yaze_py**:    Python Module
- **yaze_test**:  Unit Tests
- **z3ed**:       Command Line Interface

Dependencies are included as submodules and will be built automatically. For those who want to reduce compile times, consider installing the dependencies on your system. See [build-instructions.md](docs/build-instructions.md) for more information.

## Documentation

- For users, please refer to [getting_started.md](docs/getting-started.md) for instructions on how to use yaze.
- For developers, please refer to the [documentation](https://scawful.github.io/yaze/index.html) for information on the project's infrastructure.

License
--------
YAZE is distributed under the [GNU GPLv3](https://www.gnu.org/licenses/gpl-3.0.txt) license.

SDL2, ImGui and Abseil are subject to respective licenses.

Screenshots
--------
![image](https://github.com/scawful/yaze/assets/47263509/8b62b142-1de4-4ca4-8c49-d50c08ba4c8e)

![image](https://github.com/scawful/yaze/assets/47263509/d8f0039d-d2e4-47d7-b420-554b20ac626f)

![image](https://github.com/scawful/yaze/assets/47263509/34b36666-cbea-420b-af90-626099470ae4)


