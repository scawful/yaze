# Contributing

This project is looking for contributors to help improve the software and enhance the user experience. If you are interested in contributing, please read the following guidelines and suggestions for areas where you can make a difference.

Discussion on the editor and its development can be found on the [Oracle of Secrets Discord](https://discord.gg/MBFkMTPEmk) server.

## Style Guide

When contributing to the project, please follow these guidelines to ensure consistency and readability across the codebase:

C++ Code should follow the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) with the following exceptions:

- Boost libraries are allowed, but require cross platform compatibility.

Objective-C Code should follow the [Google Objective-C Style Guide](https://google.github.io/styleguide/objcguide.html).

Python Code should follow the [PEP 8 Style Guide](https://pep8.org/).

Assembly code should follow the [65816 Style Guide](docs/asm-style-guide.md).

## Testing Facilities

The project includes the `yaze_test` target which defines unit tests and an integration test window. The unit tests make use of GoogleTest and GoogleMock. The integration test window is an ImGui window build out of the yaze::core::Controller and yaze::test::integration::TestEditor. The integration test window can be accessed by passing the argument `integration` to the target.

New modules should define unit tests in the `src/test` directory and integration tests in the `src/test/integration` directory. The `yaze_test` target will automatically include all tests in these directories.

## Key Areas of Contribution

### 1. Sprite Builder System

The sprite builder system in yaze is based on the [ZSpriteMaker](https://github.com/Zarby89/ZSpriteMaker/) project and allows users to create custom sprites for use in ROM hacks. The goal is to support ZSM files and provide an intuitive interface for editing sprites without the need for writing assembly code. Contributions to the sprite builder system might include:

- Implementing new features for sprite editing, such as palette management, animation preview, or tileset manipulation.
- Extending the sprite builder interface by writing assembly code for sprite behavior.

### 2. Emulator Subsystem

yaze includes an emulator subsystem that allows developers to test their modifications directly within the editor. The emulator can currently run certain test ROMs but lacks the ability to play any complex games with audio because of timing issues with the APU and Spc700. Contributions to the emulator subsystem might include:

- Improving the accuracy and performance of the emulator to support more games and features.
- Implementing new debugging tools, such as memory viewers, breakpoints, or trace logs.
- Extending the emulator to support additional features, such as save states, cheat codes, or multiplayer modes.

## Building the Project

For detailed instructions on building YAZE, including its dependencies and supported platforms, refer to [build-instructions.md](docs/build-instructions.md).

## Getting Started

1. Clone the Repository:

```bash
git clone https://github.com/yourusername/yaze.git
cd yaze
```

2. Initialize the Submodules:

```bash
git submodule update --init --recursive
```

3. Build the Project:

Follow the instructions in the [build-instructions.md](docs/build-instructions.md). file to configure and build the project on your target platform.

4. Run the Application:

After building, you can run the application on your chosen platform and start exploring the existing features.

## Contributing your Changes

1. Fork the Repository:

Create a fork of the project on GitHub and clone your fork to your local machine.

2. Create a Branch:

Create a new branch for your feature or bugfix.

```bash
git checkout -b feature/my-new-feature
```

3. Implement Your Changes:

Follow the guidelines above to implement new features, extensions, or improvements.

4. Test Your Changes:

Ensure your changes don’t introduce new bugs or regressions. Write unit tests where applicable.

5. Submit a Pull Request:

Push your changes to your fork and submit a pull request to the main repository. Provide a clear description of your changes and why they are beneficial.
