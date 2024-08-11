# Contributing

This project is looking for contributors to help improve the software and enhance the user experience. If you are interested in contributing, please read the following guidelines.

Discussion on the editor and its development can be found on the [Oracle of Secrets Discord](https://discord.gg/MBFkMTPEmk) server.

## Key Areas of Contribution

### 1. Extensions System

Yaze *(stylized as yaze)* emphasizes extensibility. The `yaze_ext` library allows developers to build and integrate extensions using C, C++, or Python. This system is central to yaze's modular design, enabling new features, custom editors, or tools to be added without modifying the core codebase.

- C/C++ Extensions: Utilize the `yaze_extension` interface to integrate custom functionality into the editor. You can add new tabs, manipulate ROM data, or extend the editor’s capabilities with custom tools.
- Python Extensions: Currently unimplemented, Python extensions will allow developers to write scripts that interact with the editor, modify ROM data, or automate repetitive tasks.

Examples of Extensions:

- UI enhancements like additional menus, panels, or status displays.
- Rom manipulation tools for editing data structures, such as the overworld maps or dungeon objects.
- Custom editors for specific tasks, like file format conversion, data visualization, or event scripting.

### 2. Sprite Builder System

The sprite builder system in yaze is based on the [ZSpriteMaker](https://github.com/Zarby89/ZSpriteMaker/) project and allows users to create custom sprites for use in ROM hacks. The goal is to support ZSM files and provide an intuitive interface for editing sprites without the need for writing assembly code. Contributions to the sprite builder system might include:

- Implementing new features for sprite editing, such as palette management, animation preview, or tileset manipulation.
- Extending the sprite builder interface by writing assembly code for sprite behavior.

### 3. Emulator Subsystem

yaze includes an emulator subsystem that allows developers to test their modifications directly within the editor. The emulator can currently run certain test ROMs but lacks the ability to play any complex games with audio because of timing issues with the APU and Spc700. Contributions to the emulator subsystem might include:

- Improving the accuracy and performance of the emulator to support more games and features.
- Implementing new debugging tools, such as memory viewers, breakpoints, or trace logs.
- Extending the emulator to support additional features, such as save states, cheat codes, or multiplayer modes.

### 4. Editor Management

The `MasterEditor` class manages the core functionalities of YAZE, including rendering the UI, handling user input, and managing multiple editors. While this class is central to yaze's operations, it has many responsibilities. You can help by:

- Refactoring `MasterEditor` to delegate responsibilities to specialized managers (e.g., `MenuManager`, `TabManager`, `StatusManager`).
- Optimizing the rendering and update loop to improve performance, especially when handling large textures or complex editors.
- Implementing new features that streamline the editing process, such as better keyboard shortcuts, command palette integration, or project management tools.

### 5. User Interface and UX

yaze's UI is built with ImGui, offering a flexible and customizable interface. Contributions to the UI might include:

- Designing and implementing new themes or layouts to improve the user experience.
- Adding new UI components, such as toolbars, context menus, or customizable panels.
- Improving the accessibility of the editor, ensuring it is usable by a wide range of users, including those with disabilities.

### 6. ROM Manipulation

The `Rom` class is at the heart of yaze's ability to modify and interact with ROM data. Contributions here might involve:

- Optimizing the loading and saving processes to handle larger ROMs or more complex modifications efficiently. 
- Extensions should be able to change the way the `Rom` class interacts with the ROM data with custom pointers to expanded data structures.

### 7. Testing and Documentation

Quality assurance and documentation are critical to yaze's success. Contributions in this area include:

- Writing unit tests for new and existing features to ensure they work correctly and remain stable over time.
- Contributing to the documentation, both for end-users and developers, to make yaze easier to use and extend.
- Creating tutorials or guides that help new developers get started with building extensions or contributing to the project.

## Building the Project

For detailed instructions on building YAZE, including its dependencies and supported platforms, refer to `build-instructions.md`.

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

Follow the instructions in the `build-instructions.md` file to configure and build the project on your target platform.

4. Run the Application:

After building, you can run the application on your chosen platform and start exploring the existing features.

## Contributing

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
