# Yet Another Zelda3 Editor

    Platform: Windows, macOS, GNU/Linux
    Dependencies: SDL2, ImGui

## Description
General purpose editor for The Legend of Zelda: A Link to the Past for the Super Nintendo.

Building and installation
-------------------------
[CMake](http://www.cmake.org "CMake") is required to build Premia 

1. Clone the repository

        git clone --recurse-submodules https://github.com/scawful/yaze.git 

3. Create a build directory in the root workspace folder

        mkdir build
      
4. Move to the build directory and generate the build files specified in CMakeLists.txt

        cmake -G "<MinGW Makefiles/Unix Makefiles>" ../

5. Build and run.

        make yaze
        cmake --build <yaze_root/build> --config Debug --target yaze

Screenshots
--------
![image](https://user-images.githubusercontent.com/47263509/175393817-39ba86c1-d940-4426-b4db-e2c0f6bd857f.png)

