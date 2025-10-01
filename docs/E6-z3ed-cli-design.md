## 1. Overview

This document outlines a design for the evolution of `z3ed`, the command-line interface for the YAZE project. The goal is to transform `z3ed` from a collection of utility commands into a powerful, scriptable, and extensible tool for both manual and automated ROM hacking, with a forward-looking approach to AI-driven generative development.

### 1.1. Current State

`z3ed` has evolved significantly. The initial limitations regarding scope, inconsistent structure, basic TUI, and limited scriptability have largely been addressed through the implementation of resource-oriented commands, modular TUI components, and structured output. The focus has shifted towards building a robust foundation for AI-driven generative hacking.

## 2. Design Goals

The proposed redesign focuses on three core pillars:

1.  **Power & Usability for ROM Hackers**: Empower users with fine-grained control over all aspects of the ROM directly from the command line.
2.  **Testability & Automation**: Provide robust commands for validating ROM integrity and automating complex testing scenarios.
3.  **AI & Generative Hacking**: Establish a powerful, scriptable API that an AI agent (MCP) can use to perform complex, generative tasks on the ROM.

## 3. Proposed CLI Architecture: Resource-Oriented Commands

The CLI has adopted a `z3ed <resource> <action> [options]` structure, similar to modern CLIs like `gcloud` or `kubectl`, improving clarity and extensibility.

### 3.1. Top-Level Resources

- `rom`: Commands for interacting with the ROM file itself.
- `patch`: Commands for applying and creating patches.
- `gfx`: Commands for graphics manipulation.
- `palette`: Commands for palette manipulation.
- `overworld`: Commands for overworld editing.
- `dungeon`: Commands for dungeon editing.
- `sprite`: Commands for sprite management and creation.
- `test`: Commands for running tests.
- `tui`: The entrypoint for the enhanced Text User Interface.
- `agent`: Commands for interacting with the AI agent.

### 3.2. Example Command Mapping

The command mapping has been successfully implemented, transitioning from the old flat structure to the new resource-oriented approach.

## 4. New Features & Commands

### 4.1. For the ROM Hacker (Power & Scriptability)

These commands focus on exporting data to and from the original SCAD (Nintendo Super Famicom/SNES CAD) binary formats found in the gigaleak, as well as other relevant binary formats. This enables direct interaction with development assets, version control, and sharing. Many of these commands have been implemented or are in progress.

- **Dungeon Editing**: Commands for exporting, importing, listing, and adding objects.
- **Overworld Editing**: Commands for getting, setting tiles, listing, and moving sprites.
- **Graphics & Palettes**: Commands for exporting/importing sheets and palettes.

### 4.2. For Testing & Automation

- **ROM Validation & Comparison**: `z3ed rom validate`, `z3ed rom diff`, and `z3ed rom generate-golden` have been implemented.
- **Test Execution**: `z3ed test run` and `z3ed test list-suites` are in progress.

## 5. TUI Enhancements

The `--tui` flag now launches a significantly enhanced, interactive terminal application built with FTXUI. The TUI has been decomposed into a set of modular components, with each command handler responsible for its own TUI representation, making it more extensible and easier to maintain.

- **Dashboard View**: The main screen is evolving into a dashboard.
- **Interactive Palette Editor**: In progress.
- **Interactive Hex Viewer**: Implemented.
- **Command Palette**: In progress.
- **Tabbed Layout**: Implemented.

## 6. Generative & Agentic Workflows (MCP Integration)

The redesigned CLI serves as the foundational API for an AI-driven Model-Code-Program (MCP) loop. The AI agent's "program" is a script of `z3ed` commands.

### 6.1. The Generative Workflow

The generative workflow has been refined to incorporate more detailed planning and verification steps, leveraging the `z3ed agent` commands.

### 6.2. Key Enablers

- **Granular Commands**: The CLI provides commands to manipulate data within the binary formats (e.g., `palette set-color`, `gfx set-pixel`), abstracting complexity from the AI agent.
- **Idempotency**: Commands are designed to be idempotent where possible.
- **SpriteBuilder CLI**: Deprioritized for now, pending further research and development of the underlying assembly generation capabilities.

## 7. Implementation Roadmap

### Phase 1: Core CLI & TUI Foundation (Done)
- **CLI Structure**: Implemented.
- **Command Migration**: Implemented.
- **TUI Decomposition**: Implemented.

### Phase 2: Interactive TUI & Command Palette (Done)
- **Interactive Palette Editor**: Implemented.
- **Interactive Hex Viewer**: Implemented.
- **Command Palette**: Implemented.

### Phase 3: Testing & Project Management (Done)
- **`rom validate`**: Implemented.
- **`rom diff`**: Implemented.
- **`rom generate-golden`**: Implemented.
- **Project Scaffolding**: Implemented.

### Phase 4: Agentic Framework & Generative AI (In Progress)
- **`z3ed agent` command**: Implemented with `run`, `plan`, `diff`, `test`, `commit`, `revert`, and `learn` subcommands.
- **AI Model Interaction**: In progress, with `MockAIService` and `GeminiAIService` (conditional) implemented.
- **Execution Loop (MCP)**: In progress, with command parsing and execution logic.
- **Leveraging `ImGuiTestEngine`**: In progress, with `agent test` subcommand.
- **Granular Data Commands**: Not started, but planned.
- **SpriteBuilder CLI**: Deprioritized.

### Phase 5: Code Structure & UX Improvements (Completed)
- **Modular Architecture**: Refactored CLI handlers into clean, focused modules with proper separation of concerns.
- **TUI Component System**: Implemented `TuiComponent` interface for consistent UI components across the application.
- **Unified Command Interface**: Standardized `CommandHandler` base class with both CLI and TUI execution paths.
- **Error Handling**: Improved error handling with consistent `absl::Status` usage throughout the codebase.
- **Build System**: Streamlined CMake configuration with proper dependency management and conditional compilation.
- **Code Quality**: Resolved linting errors and improved code maintainability through better header organization and forward declarations.

## 8. Agentic Framework Architecture - Advanced Dive

The agentic framework is designed to allow an AI agent to make edits to the ROM based on high-level natural language prompts. The framework is built around the `z3ed` CLI and the `ImGuiTestEngine`. This section provides a more advanced look into its architecture and future development.

### 8.1. The `z3ed agent` Command

The `z3ed agent` command is the main entry point for the agent. It has the following subcommands:

- `run --prompt "..."`: Executes a prompt by generating and running a sequence of `z3ed` commands.
- `plan --prompt "..."`: Shows the sequence of `z3ed` commands the AI plans to execute.
- `diff`: Shows a diff of the changes made to the ROM after running a prompt.
- `test --prompt "..."`: Generates changes and then runs an `ImGuiTestEngine` test to verify them.
- `commit`: Saves the modified ROM and any new assets to the project.
- `revert`: Reverts the changes made by the agent.
- `learn --description "..."`: Records a sequence of user actions (CLI commands and GUI interactions) and associates them with a natural language description, allowing the agent to learn new workflows.

### 8.2. The Agentic Loop (MCP) - Detailed Workflow

1.  **Model (Planner)**: The agent receives a high-level natural language prompt. It leverages an LLM to break down this goal into a detailed, executable plan. This plan is a sequence of `z3ed` CLI commands, potentially interleaved with `ImGuiTestEngine` test steps for intermediate verification. The LLM's prompt includes the user's request, a comprehensive list of available `z3ed` commands (with their parameters and expected effects), and relevant contextual information about the current ROM state (e.g., loaded ROM, project files, current editor view).
2.  **Code (Command & Test Generation)**: The LLM returns the generated plan as a structured JSON object. This JSON object contains an array of actions, where each action specifies a `z3ed` command (with its arguments) or an `ImGuiTestEngine` test to execute. This structured output is crucial for reliable parsing and execution by the `z3ed` agent.
3.  **Program (Execution Engine)**: The `z3ed agent` parses the JSON plan and executes each command sequentially. For `z3ed` commands, it directly invokes the corresponding internal `CommandHandler` methods. For `ImGuiTestEngine` steps, it launches the `yaze_test` executable with the appropriate test arguments. The output (stdout, stderr, exit codes) of each executed command is captured. This output, along with any visual feedback from `ImGuiTestEngine` (e.g., screenshots), can be fed back to the LLM for iterative refinement of the plan.
4.  **Verification (Tester)**: The `ImGuiTestEngine` plays a critical role here. After the agent executes a sequence of commands, it can generate and run a specific `ImGuiTestEngine` script. This script can interact with the YAZE GUI (e.g., open a specific editor, navigate to a location, assert visual properties) to verify that the changes were applied correctly and as intended. The results of these tests (pass/fail, detailed logs, comparison screenshots) are reported back to the user and can be used by the LLM to self-correct or refine its strategy.

### 8.3. AI Model & Protocol Strategy

- **Models**: The framework will support both local and remote AI models, offering flexibility and catering to different user needs.
  - **Local Models (macOS Setup)**: For privacy, offline use, and reduced operational costs, integration with local LLMs via [Ollama](https://ollama.ai/) is a priority. Users can easily install Ollama on macOS and pull models optimized for code generation, such as `codellama:7b`. The `z3ed` agent will communicate with Ollama's local API endpoint.
  - **Remote Models (Gemini API)**: For more complex tasks requiring advanced reasoning capabilities, integration with powerful remote models like the Gemini API will be available. Users will need to provide a `GEMINI_API_KEY` environment variable. A new `GeminiAIService` class will be implemented to handle the secure API requests and responses.
- **Protocol**: A robust, yet simple, JSON-based protocol will be used for communication between `z3ed` and the AI model. This ensures structured data exchange, critical for reliable parsing and execution. The `z3ed` tool will serialize the user's prompt, current ROM context, available `z3ed` commands, and any relevant `ImGuiTestEngine` capabilities into a JSON object. The AI model will be expected to return a JSON object containing the sequence of commands to be executed, along with potential explanations or confidence scores.

### 8.4. GUI Integration & User Experience

- **Agent Control Panel**: A dedicated TUI/GUI panel will be created for managing the agent. This panel will serve as the primary interface for users to interact with the AI. It will feature:
    - A multi-line text input for entering natural language prompts.
    - Buttons for `Run`, `Plan`, `Diff`, `Test`, `Commit`, `Revert`, and `Learn` actions.
    - A real-time log view displaying the agent's thought process, executed commands, and their outputs.
    - A status bar indicating the agent's current state (e.g., "Idle", "Planning", "Executing Commands", "Verifying Changes").
- **Diff Editing UI**: A TUI-based visual diff viewer will be implemented. This UI will present a side-by-side comparison of the original ROM state (or a previous checkpoint) and the changes proposed or made by the agent. Users will be able to:
    - Navigate through individual differences (e.g., changed bytes, modified tiles, added objects).
    - Highlight specific changes.
    - Accept or reject individual changes or groups of changes, providing fine-grained control over the agent's output.
- **Interactive Planning**: The agent will present its generated plan in a human-readable format within the GUI. Users will have the opportunity to:
    - Review each step of the plan.
    - Approve the entire plan for execution.
    - Reject specific steps or the entire plan.
    - Edit the plan directly (e.g., modify command arguments, reorder steps, insert new commands) before allowing the agent to proceed.

### 8.5. Testing & Verification

- **`ImGuiTestEngine` Integration**: The agent will be able to dynamically generate and execute `ImGuiTestEngine` tests. This allows for automated visual verification of the agent's work, ensuring that changes are not only functionally correct but also visually appealing and consistent with design principles. The agent can be trained to generate test scripts that assert specific pixel colors, UI element positions, or overall visual layouts.
- **Mock Testing Framework**: A robust "mock" mode will be implemented for the `z3ed agent`. In this mode, the agent will simulate the execution of commands without modifying the actual ROM. This is crucial for safe and fast testing of the agent's planning and command generation capabilities. The existing `MockRom` class will be extended to fully support all `z3ed` commands, providing a consistent interface for both real and mock execution.
- **User-Facing Tests**: A "tutorial" or "challenge" mode will be created where users can test the agent with a series of predefined tasks. This will serve as an educational tool for users to understand the agent's capabilities and provide a way to benchmark its performance against specific ROM hacking challenges.

### 8.6. Safety & Sandboxing

- **Dry Run Mode**: The agent will always offer a "dry run" mode, where it only shows the commands it would execute without making any actual changes to the ROM. This provides a critical safety net for users.
- **Command Whitelisting**: The agent's execution environment will enforce a strict command whitelisting policy. Only a predefined set of "safe" `z3ed` commands will be executable by the AI. Any attempt to execute an unauthorized command will be blocked.
- **Resource Limits**: The agent will operate within defined resource limits (e.g., maximum number of commands per plan, maximum data modification size) to prevent unintended extensive changes or infinite loops.
- **Human Oversight**: Given the inherent unpredictability of AI models, human oversight will be a fundamental principle. The interactive planning and diff editing UIs are designed to keep the user in control at all times.

### 8.7. Optional JSON Dependency

To avoid breaking platform builds where a JSON library is not available or desired, the JSON-related code will be conditionally compiled using a preprocessor macro (e.g., `YAZE_WITH_JSON`). When this macro is not defined, the agentic features that rely on JSON will be disabled. The `nlohmann/json` library will be added as a submodule to the project and included in the build only when `YAZE_WITH_JSON` is defined.

### 8.8. Contextual Awareness & Feedback Loop

- **Contextual Information**: The agent's prompts to the LLM will be enriched with comprehensive contextual information, including:
    - The current state of the loaded ROM (e.g., ROM header, loaded assets, current editor view).
    - Relevant project files (e.g., `.yaze` project configuration, symbol files).
    - User preferences and previous interactions.
    - A dynamic list of available `z3ed` commands and their detailed usage.
- **Feedback Loop for Learning**: The results of `ImGuiTestEngine` verifications and user accept/reject actions will form a crucial feedback loop. This data can be used to fine-tune the LLM or train smaller, specialized models to improve the agent's planning and command generation capabilities over time.

### 8.9. Error Handling and Recovery

- **Robust Error Reporting**: The agent will provide clear and actionable error messages when commands fail or unexpected situations arise.
- **Rollback Mechanisms**: The `revert` command provides a basic rollback. More advanced mechanisms, such as transactional changes or snapshotting, could be explored for complex multi-step operations.
- **Interactive Debugging**: In case of errors, the agent could pause execution and allow the user to inspect the current state, modify the plan, or provide corrective instructions.

### 8.10. Extensibility

- **Modular Command Handlers**: The `z3ed` CLI's modular design allows for easy addition of new commands, which automatically become available to the AI agent.
- **Pluggable AI Models**: The `AIService` interface enables seamless integration of different AI models (local or remote) without modifying the core agent logic.
- **Custom Test Generation**: Users or developers can extend the `ImGuiTestEngine` capabilities to create custom verification tests for specific hacking scenarios.

## 9. UX Improvements and Architectural Decisions

### 9.1. TUI Component Architecture

The TUI system has been redesigned around a consistent component architecture:

- **`TuiComponent` Interface**: All UI components implement a standard interface with a `Render()` method, ensuring consistency across the application.
- **Component Composition**: Complex UIs are built by composing simpler components, making the code more maintainable and testable.
- **Event Handling**: Standardized event handling patterns across all components for consistent user experience.

### 9.2. Command Handler Unification

The CLI and TUI systems now share a unified command handler architecture:

- **Dual Execution Paths**: Each command handler supports both CLI (`Run()`) and TUI (`RunTUI()`) execution modes.
- **Shared State Management**: Common functionality like ROM loading and validation is centralized in the base `CommandHandler` class.
- **Consistent Error Handling**: All commands use `absl::Status` for uniform error reporting across CLI and TUI modes.

### 9.3. Interface Consolidation

Several interfaces have been combined and simplified:

- **Unified Menu System**: The main menu now serves as a central hub for both direct command execution and TUI mode switching.
- **Integrated Help System**: Help information is accessible from both CLI and TUI modes with consistent formatting.
- **Streamlined Navigation**: Reduced cognitive load by consolidating related functionality into single interfaces.

### 9.4. Code Organization Improvements

The codebase has been restructured for better maintainability:

- **Header Organization**: Proper forward declarations and include management to reduce compilation dependencies.
- **Namespace Management**: Clean namespace usage to avoid conflicts and improve code clarity.
- **Build System Optimization**: Streamlined CMake configuration with conditional compilation for optional features.

### 9.5. Future UX Enhancements

Based on the current architecture, several UX improvements are planned:

- **Progressive Disclosure**: Complex commands will offer both simple and advanced modes.
- **Context-Aware Help**: Help text will adapt based on current ROM state and available commands.
- **Undo/Redo System**: Command history tracking for safer experimentation.
- **Batch Operations**: Support for executing multiple related commands as a single operation.

## 10. Implementation Status and Code Quality

### 10.1. Recent Refactoring Improvements (January 2025)

The z3ed CLI underwent significant refactoring to improve code quality, fix linting errors, and enhance maintainability.

**Issues Resolved**:
- ✅ **Missing Headers**: Added proper forward declarations for `ftxui::ScreenInteractive` and `TuiComponent`
- ✅ **Include Path Issues**: Standardized all includes to use `cli/` prefix instead of `src/cli/`
- ✅ **Namespace Conflicts**: Resolved namespace pollution issues by properly organizing includes
- ✅ **Duplicate Definitions**: Removed duplicate `CommandInfo` and `ModernCLI` definitions
- ✅ **FLAGS_rom Multiple Definitions**: Changed duplicate `ABSL_FLAG` declarations to `ABSL_DECLARE_FLAG`

**Build System Improvements**:
- **CMake Configuration**: Cleaned up `z3ed.cmake` to properly configure all source files
- **Dependency Management**: Added proper includes for `absl/flags/declare.h` where needed
- **Conditional Compilation**: Properly wrapped JSON/HTTP library usage with `#ifdef YAZE_WITH_JSON`

**Architecture Improvements**:
- Removed `std::unique_ptr<TuiComponent>` members from command handlers to avoid incomplete type issues
- Simplified constructors and `RunTUI` methods
- Maintained clean separation between CLI and TUI execution paths

### 10.2. File Organization

```
src/cli/
  ├── cli_main.cc          (Entry point - defines FLAGS)
  ├── modern_cli.{h,cc}    (Command registry and dispatch)
  ├── tui.{h,cc}           (TUI components and layout management)
  ├── z3ed.{h,cc}          (Command handler base classes)
  ├── service/
  │   ├── ai_service.{h,cc}           (AI service interface)
  │   └── gemini_ai_service.{h,cc}    (Gemini API implementation)
  ├── handlers/            (Command implementations)
  │   ├── agent.cc
  │   ├── command_palette.cc
  │   ├── compress.cc
  │   ├── dungeon.cc
  │   ├── gfx.cc
  │   ├── overworld.cc
  │   ├── palette.cc
  │   ├── patch.cc
  │   ├── project.cc
  │   ├── rom.cc
  │   ├── sprite.cc
  │   └── tile16_transfer.cc
  └── tui/                 (TUI component implementations)
      ├── tui_component.h
      ├── asar_patch.{h,cc}
      ├── palette_editor.{h,cc}
      └── command_palette.{h,cc}
```

### 10.3. Code Quality Improvements

**Removed Problematic Patterns**:
- Eliminated returning raw pointers to temporary objects in `GetCommandHandler`
- Used `static` storage for handlers to ensure valid lifetimes
- Proper const-reference usage to avoid unnecessary copies

**Standardized Error Handling**:
- Consistent use of `absl::Status` return types
- Proper status checking with `RETURN_IF_ERROR` macro
- Clear error messages for user-facing commands

**API Corrections**:
- Fixed `Bitmap::bpp()` → `Bitmap::depth()`
- Fixed `PaletteGroup::set_palette()` → direct pointer manipulation
- Fixed `Bitmap::mutable_vector()` → `Bitmap::set_data()`

### 10.4. TUI Component System

**Implemented Components**:
- `TuiComponent` interface for consistent UI components
- `ApplyAsarPatchComponent` - Modular patch application UI
- `PaletteEditorComponent` - Interactive palette editing
- `CommandPaletteComponent` - Command search and execution

**Standardized Patterns**:
- Consistent navigation across all TUI screens
- Centralized error handling with dedicated error screen
- Direct component function calls instead of handler indirection

### 10.5. Known Limitations

**Remaining Warnings (Non-Critical)**:
- Unused parameter warnings (mostly for stub implementations)
- Nodiscard warnings for status returns that are logged elsewhere
- Copy-construction warnings (minor performance considerations)
- Virtual destructor warnings in third-party zelda3 classes

### 10.6. Future Code Quality Goals

1. **Complete TUI Components**: Finish implementing all planned TUI components with full functionality
2. **Error Handling**: Add proper status checking for all `LoadFromFile` calls
3. **API Methods**: Implement missing ROM validation methods
4. **JSON Integration**: Complete HTTP/JSON library integration for Gemini AI service
5. **Performance**: Address copy-construction warnings by using const references
6. **Testing**: Expand unit test coverage for command handlers
