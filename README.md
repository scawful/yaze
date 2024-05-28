# Yet Another Zelda3 Editor

- Platform: Windows, macOS, GNU/Linux
- Dependencies: SDL2, ImGui

## Description

General purpose editor for The Legend of Zelda: A Link to the Past for the Super Nintendo.

Takes heavy inspiration from ALTTP community efforts such as [Hyrule Magic](https://www.romhacking.net/utilities/200/) and [ZScream](https://github.com/Zarby89/ZScreamDungeon)

Building and installation
-------------------------
[CMake](http://www.cmake.org "CMake") is required to build yaze 

1. Clone the repository

```
  git clone --recurse-submodules https://github.com/scawful/yaze.git 
```

2. Create the build directory and configuration

```
  cmake -S . -B build
```

3. Build and run.

```
  cmake --build build
```

## Documentation

- For users, please refer to [getting_started.md](docs/getting-started.md) for instructions on how to use yaze.
- For developers, please refer to the [documentation](https://scawful.github.io/yaze/index.html) for information on the project's infrastructure.

License
--------
YAZE is distributed under the [GNU GPLv3](https://www.gnu.org/licenses/gpl-3.0.txt) license.

SDL2, ImGui and Abseil are subject to respective licenses.

Screenshots
--------
![image](https://user-images.githubusercontent.com/47263509/194669806-2b0da68d-9d38-4f52-bcce-c60ee861092c.png)

![image](https://github.com/scawful/yaze/assets/47263509/8913f7ff-6345-4295-ae05-782fd3949eb5)

![image](https://github.com/scawful/yaze/assets/47263509/e1cf3edb-a59e-4f0a-b4e0-d68925803e58)

