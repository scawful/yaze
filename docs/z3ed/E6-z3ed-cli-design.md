# z3ed CLI Architecture & Design

## 1. Overview

This document is the **source of truth** for the z3ed CLI architecture and design. It outlines the evolution of `z3ed`, the command-line interface for the YAZE project, from a collection of utility commands into a powerful, scriptable, and extensible tool for both manual and automated ROM hacking, with full support for AI-driven generative development.

**Related Documents**:
- **[E6-z3ed-implementation-plan.md](E6-z3ed-implementation-plan.md)** - Implementation tracker, task backlog, and roadmap
- **[E6-z3ed-reference.md](E6-z3ed-reference.md)** - Technical reference: commands, APIs, troubleshooting
- **[README.md](README.md)** - Quick overview and documentation index

**Last Updated**: [Current Date]

`z3ed` has successfully implemented its core infrastructure and is **production-ready on macOS**:

**✅ Completed Features**:
- **Resource-Oriented CLI**: Clean `z3ed <resource> <action>` command structure
- **Resource Catalogue**: Machine-readable API specs in YAML/JSON for AI consumption
- **Acceptance Workflow**: Full proposal lifecycle (create → review → accept/reject → commit)
- **ImGuiTestHarness (IT-01)**: gRPC-based GUI automation with 6 RPC methods
- **CLI Agent Test (IT-02)**: Natural language prompts → automated GUI testing
- **ProposalDrawer GUI**: Integrated review interface in YAZE editor
- **ROM Sandbox Manager**: Isolated testing environment for safe experimentation
- **Proposal Registry**: Cross-session proposal tracking with disk persistence

**🔄 In Progress**:
- **Test Harness Enhancements (IT-05 to IT-09)**: Expanding from basic automation to comprehensive testing platform
  - Test introspection APIs for status/results polling
  - Widget discovery for AI-driven interactions
  - Test recording/replay for regression testing
  - Enhanced error reporting with screenshots
  - CI/CD integration with standardized test formats

**📋 Planned Next**:
- **Policy Evaluation Framework (AW-04)**: YAML-based constraints for proposal acceptance
- **Windows Cross-Platform Testing**: Validate on Windows with vcpkg
- **Production Readiness**: Telemetry, screenshot implementation, expanded test coverage

## 2. Design Goals

The z3ed CLI is built on three core pillars:

1.  **Power & Usability for ROM Hackers**: Empower users with fine-grained control over all aspects of the ROM directly from the command line, supporting both interactive exploration and scripted automation.

2.  **Testability & Automation**: Provide robust commands for validating ROM integrity, automating complex testing scenarios, and enabling reproducible workflows through scripting.

3.  **AI & Generative Hacking**: Establish a powerful, scriptable API that an AI agent (LLM/MCP) can use to perform complex, generative tasks on the ROM, with human oversight and approval workflows.

### 2.1. Key Architectural Decisions

**Resource-Oriented Command Structure**: Adopted `z3ed <resource> <action>` pattern (similar to kubectl, gcloud) for clarity and extensibility.

**Machine-Readable API**: All commands documented in `docs/api/z3ed-resources.yaml` with structured schemas for AI consumption.

**Proposal-Based Workflow**: AI-generated changes are sandboxed and tracked as "proposals" requiring human review and acceptance.

**gRPC Test Harness**: Embedded gRPC server in YAZE enables remote GUI automation for testing and AI-driven workflows.

**Comprehensive Testing Platform**: Test harness evolved beyond basic automation to support:
- **Widget Discovery**: AI agents can enumerate available GUI interactions dynamically
- **Test Introspection**: Query test status, results, and execution queue in real-time
- **Recording & Replay**: Capture test sessions as JSON scripts for regression testing
- **CI/CD Integration**: Standardized test suite format with JUnit XML output
- **Enhanced Debugging**: Screenshot capture, widget state dumps, and execution context on failures

**Cross-Platform Foundation**: Core built for macOS/Linux with Windows support planned via vcpkg.

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
- **`z3ed agent` command**: ✅ Implemented with `run`, `plan`, `diff`, `test`, `commit`, `revert`, `describe`, `learn`, and `list` subcommands.
- **Resource Catalog System**: ✅ Complete - comprehensive schema for all CLI commands with effects and returns metadata.
- **Agent Describe Command**: ✅ Fully operational - exports command catalog in JSON/YAML formats for AI consumption.
- **Agent List Command**: ✅ Complete - enumerates all proposals with status and metadata.
- **Agent Diff Enhancement**: ✅ Complete - reads proposals from registry, supports `--proposal-id` flag, displays execution logs and metadata.
- **Machine-Readable API**: ✅ `docs/api/z3ed-resources.yaml` generated and maintained for automation.
- **AI Model Interaction**: In progress, with `MockAIService` and `GeminiAIService` (conditional) implemented.
- **Execution Loop (MCP)**: In progress, with command parsing and execution logic.
- **Leveraging `ImGuiTestEngine`**: In progress, with `agent test` subcommand for GUI verification.
- **Sandbox ROM Management**: ✅ Complete - `RomSandboxManager` operational with full lifecycle management.
- **Proposal Tracking**: ✅ Complete - `ProposalRegistry` implemented with metadata, diffs, logs, and lifecycle management.
- **Granular Data Commands**: Partially complete - rom, palette, overworld, dungeon commands operational.
- **SpriteBuilder CLI**: Deprioritized.

### Phase 5: Code Structure & UX Improvements (Completed)
- **Modular Architecture**: Refactored CLI handlers into clean, focused modules with proper separation of concerns.
- **TUI Component System**: Implemented `TuiComponent` interface for consistent UI components across the application.
- **Unified Command Interface**: Standardized `CommandHandler` base class with both CLI and TUI execution paths.
- **Error Handling**: Improved error handling with consistent `absl::Status` usage throughout the codebase.
- **Build System**: Streamlined CMake configuration with proper dependency management and conditional compilation.
- **Code Quality**: Resolved linting errors and improved code maintainability through better header organization and forward declarations.

### Phase 6: Resource Catalogue & API Documentation (✅ Completed - Oct 1, 2025)
- **Resource Schema System**: ✅ Comprehensive schema definitions for all CLI resources (ROM, Patch, Palette, Overworld, Dungeon, Agent).
- **Metadata Annotations**: ✅ All commands annotated with arguments, effects, returns, and stability levels.
- **Serialization Framework**: ✅ Dual-format export (JSON compact, YAML human-readable) with resource filtering.
- **Agent Describe Command**: ✅ Full implementation with `--format`, `--resource`, `--output`, `--version` flags.
- **API Documentation Generation**: ✅ Automated generation of `docs/api/z3ed-resources.yaml` for AI/tooling consumption.
- **Flag-Based Dispatch**: ✅ Hardened command routing - all ROM commands use `FLAGS_rom` consistently.
- **ROM Info Fix**: ✅ Created dedicated `RomInfo` handler, resolving segfault issue.

**Key Achievements**:
- Machine-readable API catalog enables LLM integration for automated ROM hacking workflows
- Comprehensive command documentation with argument types, effects, and return schemas
- Stable foundation for AI agents to discover and invoke CLI commands programmatically
- Validation layer for ensuring command compatibility and argument correctness

**Testing Coverage**:
- ✅ All ROM commands tested: `info`, `validate`, `diff`, `generate-golden`
- ✅ Agent describe tested: YAML output, JSON output, resource filtering, file generation
- ✅ Help system integration verified with updated command listings
- ✅ Build system validated on macOS (arm64) with no critical warnings

## 8. Agentic Framework Architecture - Advanced Dive

The agentic framework is designed to allow an AI agent to make edits to the ROM based on high-level natural language prompts. The framework is built around the `z3ed` CLI and the `ImGuiTestEngine`. This section provides a more advanced look into its architecture and future development.

### 8.1. The `z3ed agent` Command

The `z3ed agent` command is the main entry point for the agent. It has the following subcommands:

- `run --prompt "..."`: Executes a prompt by generating and running a sequence of `z3ed` commands.
- `plan --prompt "..."`: Shows the sequence of `z3ed` commands the AI plans to execute.
- `diff [--proposal-id <id>]`: Shows a diff of the changes made to the ROM after running a prompt. Displays the latest pending proposal by default, or a specific proposal if ID is provided.
- `list`: Lists all proposals with their status, creation time, prompt, and execution statistics.
- `test --prompt "..."`: Generates changes and then runs an `ImGuiTestEngine` test to verify them.
- `commit`: Saves the modified ROM and any new assets to the project.
- `revert`: Reverts the changes made by the agent.
- `describe [--resource <name>]`: Returns machine-readable schemas for CLI commands, enabling AI/LLM integration.
- `learn --description "..."`: Records a sequence of user actions (CLI commands and GUI interactions) and associates them with a natural language description, allowing the agent to learn new workflows.

### 8.2. The Agentic Loop (MCP) - Detailed Workflow

1.  **Model (Planner)**: The agent receives a high-level natural language prompt. It leverages an LLM to break down this goal into a detailed, executable plan. This plan is a sequence of `z3ed` CLI commands, potentially interleaved with `ImGuiTestEngine` test steps for intermediate verification. The LLM's prompt includes the user's request, a comprehensive list of available `z3ed` commands (with their parameters and expected effects), and relevant contextual information about the current ROM state (e.g., loaded ROM, project files, current editor view).
2.  **Code (Command & Test Generation)**: The LLM returns the generated plan as a structured JSON object. This JSON object contains an array of actions, where each action specifies a `z3ed` command (with its arguments) or an `ImGuiTestEngine` test to execute. This structured output is crucial for reliable parsing and execution by the `z3ed` agent.
3.  **Program (Execution Engine)**: The `z3ed agent` parses the JSON plan and executes each command sequentially. For `z3ed` commands, it directly invokes the corresponding internal `CommandHandler` methods. For `ImGuiTestEngine` steps, it launches the `yaze_test` executable with the appropriate test arguments. The output (stdout, stderr, exit codes) of each executed command is captured. This output, along with any visual feedback from `ImGuiTestEngine` (e.g., screenshots), can be fed back to the LLM for iterative refinement of the plan.
4.  **Verification (Tester)**: The `ImGuiTestEngine` plays a critical role here. After the agent executes a sequence of commands, it can generate and run a specific `ImGuiTestEngine` script. This script can interact with the YAZE GUI (e.g., open a specific editor, navigate to a location, assert visual properties) to verify that the changes were applied correctly and as intended. The results of these tests (pass/fail, detailed logs, comparison screenshots) are reported back to the user and can be used by the LLM to self-correct or refine its strategy.

### 8.3. AI Model & Protocol Strategy

- **Models**: The framework will support both local and remote AI models, offering flexibility and catering to different user needs.

---

## 9. Test Harness Evolution: From Automation to Platform

The ImGuiTestHarness has evolved from a basic GUI automation tool into a comprehensive testing platform that serves dual purposes: **AI-driven generative workflows** and **traditional GUI testing**.

### 9.1. Current Capabilities (IT-01 to IT-04) ✅

**Core Automation** (6 RPCs):
- `Ping` - Health check and version verification
- `Click` - Button, menu, and tab interactions
- `Type` - Text input with focus management
- `Wait` - Condition polling (window visibility, element state)
- `Assert` - State validation (visible, enabled, exists)
- `Screenshot` - Capture (stub, needs implementation)

**Integration Points**:
- ImGuiTestEngine dynamic test registration
- Async test queue with frame-accurate timing
- gRPC server embedded in YAZE process
- Cross-platform build (macOS validated, Windows planned)

**Proven Use Cases**:
- Menu-driven editor opening (Overworld, Dungeon, etc.)
- Window visibility validation
- Multi-step workflows with timing dependencies
- Natural language test prompts via `z3ed agent test`

### 9.2. Limitations Identified

**For AI Agents**:
- ❌ Can't discover available widgets → must hardcode target names
- ❌ No way to query test results → async tests return immediately with no status
- ❌ No structured error context → failures lack screenshots and state dumps
- ❌ Limited to predefined actions → can't learn new interaction patterns

**For Traditional Testing**:
- ❌ No test recording → can't capture manual workflows for regression
- ❌ No test suite format → can't organize tests into smoke/regression/nightly groups
- ❌ No CI integration → can't run tests in automated pipelines
- ❌ No result persistence → test history lost between sessions
- ❌ Poor debugging → failures don't capture visual or state context

### 9.3. Enhancement Roadmap (IT-05 to IT-09)

#### IT-05: Test Introspection API (6-8 hours)
**Problem**: Tests execute asynchronously with no way to query status or results. Clients poll blindly or give up early.

**Solution**: Add 3 new RPCs:
- `GetTestStatus(test_id)` → Returns queued/running/passed/failed/timeout with execution time
- `ListTests(category_filter)` → Enumerates all registered tests with metadata
- `GetTestResults(test_id)` → Retrieves detailed results: logs, assertions, metrics

**Benefits**:
- AI agents can poll for test completion reliably
- CLI can show real-time progress bars
- Test history enables trend analysis (flaky tests, performance regressions)

**Example Flow**:
```bash
# Queue test (returns immediately with test_id)
TEST_ID=$(z3ed agent test --prompt "Open Overworld" --output json | jq -r '.test_id')

# Poll until complete
while true; do
  STATUS=$(z3ed agent test status --test-id $TEST_ID --format json | jq -r '.status')
  [[ "$STATUS" =~ ^(PASSED|FAILED|TIMEOUT)$ ]] && break
  sleep 0.5
done

# Get results
z3ed agent test results --test-id $TEST_ID --include-logs
```

#### IT-06: Widget Discovery API (4-6 hours)
**Problem**: AI agents must know widget names in advance. Can't adapt to UI changes or learn new editors.

**Solution**: Add `DiscoverWidgets` RPC:
- Enumerates all windows currently open
- Lists interactive widgets per window: buttons, inputs, menus, tabs
- Returns metadata: ID, label, type, enabled state, position
- Provides suggested action templates (e.g., "Click button:Save")

**Benefits**:
- AI agents discover GUI capabilities dynamically
- Test scripts validate expected widgets exist
- LLM prompts improved with natural language descriptions
- Reduces brittleness from hardcoded widget names

**Example Flow**:
```python
# AI agent workflow
widgets = z3ed_client.DiscoverWidgets(window_filter="Overworld")

# LLM prompt: "Which buttons are available in the Overworld editor?"
available_actions = [w.suggested_action for w in widgets.buttons if w.is_enabled]

# LLM generates: "Click button:Save Changes"
z3ed_client.Click(target="button:Save Changes")
```

#### IT-07: Test Recording & Replay (8-10 hours)
**Problem**: No way to capture manual workflows for regression. Testers repeat same actions every release.

**Solution**: Add recording workflow:
- `StartRecording(output_file)` → Begins capturing all RPC calls
- `StopRecording()` → Saves to JSON test script
- `ReplayTest(test_script)` → Executes recorded actions with validation

**Test Script Format** (JSON):
```json
{
  "name": "Overworld Tile Edit Test",
  "steps": [
    { "action": "Click", "target": "menuitem: Overworld Editor" },
    { "action": "Wait", "condition": "window_visible:Overworld", "timeout_ms": 5000 },
    { "action": "Click", "target": "button:Select Tile" },
    { "action": "Assert", "condition": "enabled:button:Apply" }
  ]
}
```

**Benefits**:
- QA engineers record test scenarios once, replay forever
- Test scripts version controlled alongside code
- Parameterized tests (e.g., test with different ROMs)
- Foundation for test suite management (smoke, regression, nightly)

#### IT-08: Enhanced Error Reporting (3-4 hours)
**Problem**: Test failures lack context. Developer sees "Window not visible" but doesn't know why.

**Solution**: Capture rich context on failure:
- Screenshot (implement stub RPC)
- Widget state dump (full hierarchy with properties)
- Execution context (active window, recent events, resource stats)
- HTML report generation with annotated screenshots

**Example Error Report**:
```json
{
  "test_id": "grpc_wait_12345678",
  "failure_reason": "Timeout waiting for window_visible:Overworld",
  "screenshot": "test-results/failure_12345678.png",
  "widget_state": {
    "visible_windows": ["Main Window", "Debug"],
    "overworld_window": { "exists": true, "visible": false, "reason": "not_initialized" }
  },
  "execution_context": {
    "last_click": "menuitem: Overworld Editor",
    "frames_since_click": 150,
    "resource_stats": { "memory_mb": 245, "framerate": 58.3 }
  }
}
```

**Benefits**:
- Developers fix failing tests faster (visual + state context)
- Flaky test debugging (see exact UI state at failure)
- Test reports shareable with QA/PM (HTML with screenshots)

#### IT-09: CI/CD Integration (2-3 hours)
**Problem**: Tests run manually. No automated regression on PR/merge.

**Solution**: Standardize test execution for CI:
- YAML test suite format (groups, dependencies, parallel execution)
- `z3ed test suite run` command with `--ci-mode`
- JUnit XML output for CI parsers (Jenkins, GitHub Actions)
- Exit codes: 0=pass, 1=fail, 2=error

**GitHub Actions Example**:
```yaml
name: GUI Tests
on: [push, pull_request]
jobs:
  gui-tests:
    runs-on: macos-latest
    steps:
      - name: Build YAZE
        run: cmake --build build --target yaze --target z3ed
      - name: Start test harness
        run: ./build/bin/yaze --enable_test_harness --headless &
      - name: Run smoke tests
        run: ./build/bin/z3ed test suite run tests/smoke.yaml --ci-mode
      - name: Upload results
        uses: actions/upload-artifact@v2
        with:
          name: test-results
          path: test-results/
```

**Benefits**:
- Catch regressions before merge
- Test history tracked in CI dashboard
- Parallel execution for faster feedback
- Flaky test detection (retry logic, failure rates)

### 9.4. Unified Testing Vision

The enhanced test harness serves three audiences:

**For AI Agents** (Generative Workflows):
- Widget discovery enables dynamic learning
- Test introspection provides reliable feedback loops
- Recording captures expert workflows for training data

**For Developers** (Unit/Integration Testing):
- Test suites organize tests by scope (smoke, regression, nightly)
- CI integration catches regressions early
- Rich error reporting speeds up debugging

**For QA Engineers** (Manual Testing Automation):
- Record manual workflows once, replay forever
- Parameterized tests reduce maintenance burden
- Visual test reports simplify communication

**Shared Infrastructure**:
- Single gRPC server handles all test types
- Consistent test script format (JSON/YAML)
- Common result storage and reporting
- Cross-platform support (macOS, Windows, Linux)

### 9.5. Implementation Priority

**Phase 1: Foundation** (Already Complete ✅)
- Core automation RPCs (Ping, Click, Type, Wait, Assert)
- ImGuiTestEngine integration
- gRPC server lifecycle
- Basic E2E validation

**Phase 2: Introspection & Discovery** (IT-05, IT-06 - 10-14 hours)
- Test status/results querying
- Widget enumeration API
- Async test management
- *Critical for AI agents*

**Phase 3: Recording & Replay** (IT-07 - 8-10 hours)
- Test script format
- Recording workflow
- Replay engine
- *Unlocks regression testing*

**Phase 4: Production Readiness** (IT-08, IT-09 - 5-7 hours)
- Screenshot implementation
- Error context capture
- CI/CD integration
- *Enables automated pipelines*

**Total Estimated Effort**: 23-31 hours beyond current implementation

---
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

## 11. Agent-Ready API Surface Area

To unlock deeper agentic workflows, the CLI and application layers must expose a well-documented, machine-consumable API surface that mirrors the capabilities available in the GUI editors. The following initiatives expand the command coverage and standardize access for both humans and AI agents:

- **Resource Inventory**: Catalogue every actionable subsystem (ROM metadata, banks, tile16 atlas, actors, palettes, scripts) and map it to a resource/action pair (e.g., `rom header set`, `dungeon room copy`, `sprite spawn`). The catalogue will live in `docs/api/z3ed-resources.yaml` and be generated from source annotations; current machine-readable coverage includes palette, overworld, rom, patch, and dungeon actions.
- **Rich Metadata**: Schemas annotate each action with structured `effects` and `returns` arrays so agents can reason about side-effects and expected outputs when constructing plans.
- **Command Introspection Endpoint**: Introduce `z3ed agent describe --resource <name>` to return a structured schema describing arguments, enum values, preconditions, side-effects, and example invocations. Schemas will follow JSON Schema, enabling UI tooltips and LLM prompt construction.  _Prototype status (Oct 2025)_: the command now streams catalog JSON from `ResourceCatalog`, including `effects` and `returns` arrays for each action across palette, overworld, rom, patch, and dungeon resources.  
    ```json
    {
        "resources": [
            {
                "resource": "rom",
                "actions": [
                    {
                        "name": "validate",
                        "effects": [
                            "Reads ROM from disk, verifies checksum, and reports header status."
                        ],
                        "returns": [
                            { "field": "report", "type": "object", "description": "Checksum + header validation summary." }
                        ]
                    }
                ]
            },
            {
                "resource": "overworld",
                "actions": [
                    {
                        "name": "get-tile",
                        "returns": [
                            { "field": "tile", "type": "integer", "description": "Tile id located at the supplied coordinates." }
                        ]
                    }
                ]
            }
        ]
    }
    ```
- **State Snapshot APIs**: Extend `rom` and `project` resources with `export-state` actions that emit compact JSON snapshots (bank checksums, tile hashes, palette CRCs). Snapshots will seed the LLM context and accelerate change verification.
- **Write Guard Hooks**: All mutation-oriented commands will publish `PreChange` and `PostChange` events onto an internal bus (backed by `absl::Notification` + ring buffer). The agent loop subscribes to the bus to build a change proposal timeline used in review UIs and acceptance workflows.
- **Replayable Scripts**: Standardize a TOML-based script format (`.z3edscript`) that records CLI invocations with metadata (ROM hash, duration, success). Agents can emit scripts, humans can replay them via `z3ed script run <file>`.

## 12. Acceptance & Review Workflow

An explicit accept/reject system keeps humans in control while encouraging rapid agent iteration.

### 12.1. Change Proposal Lifecycle

1. **Draft**: Agent executes commands in a sandbox ROM (auto-cloned using `Rom::SaveToFile` with `save_new=true`). All diffs, test logs, and screenshots are attached to a proposal ID.
2. **Review**: The dashboard surfaces proposals with summary cards (changed resources, affected banks, test status). Users can open a detail view built atop the existing diff viewer, augmented with per-resource controls (accept tile, reject palette entry, etc.).
3. **Decision**: Accepting merges the delta into the primary ROM and commits associated assets. Rejecting discards the sandbox ROM and emits feedback signals (tagged reasons) that can be fed back to future LLM prompts.
4. **Archive**: Accepted proposals are archived with metadata for provenance; rejected ones are stored briefly for analytics before being pruned.

### 12.2. UI Extensions

- **Proposal Drawer**: Adds a right-hand drawer in the ImGui dashboard listing open proposals with filters (resource type, test pass/fail, age).
- **Inline Diff Controls**: Integrate checkboxes/buttons into the existing palette/tile hex viewers so users can cherry-pick changes without leaving the visual context.
- **Feedback Composer**: Provide quick tags (“Incorrect palette”, “Misplaced sprite”, “Regression detected”) and optional freeform text. Feedback is serialized into the agent telemetry channel.
- **Undo/Redo Enhancements**: Accepted proposals push onto the global undo stack with descriptive labels, enabling rapid rollback during exploratory sessions.

### 12.3. Policy Configuration

- **Gatekeeping Rules**: Define YAML-driven policies (e.g., “require passing `agent smoke` and `palette regression` suites before accept button activates”). Rules live in `.yaze/policies/agent.yaml` and are evaluated by the dashboard.
- **Access Control**: Integrate project roles so only maintainers can finalize proposals while contributors can submit drafts.
- **Telemetry Opt-In**: Provide toggles for sharing anonymized proposal statistics to improve default prompts and heuristics.

## 13. ImGuiTestEngine Control Bridge

Allowing an LLM to drive the ImGui UI safely requires a structured bridge between generated plans and the `ImGuiTestEngine` runtime.

### 13.1. Bridge Architecture

- **Test Harness API**: Expose a lightweight gRPC/IPC service (`ImGuiTestHarness`) that accepts serialized input events (click, drag, key, text), query requests (widget tree, screenshot), and expectations (assert widget text equals …). The service runs inside `yaze_test` when started with `--automation=sock`. Agents connect via domain sockets (macOS/Linux) or named pipes (Windows).
- **Command Translation Layer**: Extend `z3ed agent run` to recognize plan steps with type `imgui_action`. These steps translate to harness calls (e.g., `{ "type": "imgui_action", "action": "click", "target": "Palette/Cell[12]" }`).
- **Synchronization Primitives**: Provide `WaitForIdle`, `WaitForCondition`, and `Delay` primitives so LLMs can coordinate with frame updates. Each primitive enforces timeouts and returns explicit success/failure statuses.
- **State Queries**: Implement reflection endpoints retrieving ImGui widget hierarchy, enabling the agent to confirm UI states before issuing the next action—mirroring how `ImGuiTestEngine` DSL scripts work today.

#### 13.1.1. Transport & Envelope

- **Session bootstrap**: `yaze_test --automation=<socket path>` spins up the harness and prints a connection URI. The CLI or external agent opens a persistent stream (Unix domain socket on macOS/Linux, named pipe + overlapped IO on Windows). TLS is out-of-scope; trust is derived from local IPC.
- **Message format**: Each frame is a length-prefixed JSON envelope with optional binary attachments. Core fields:
    ```json
    {
        "id": "req-42",
        "type": "event" | "query" | "expect" | "control",
        "payload": { /* type-specific body */ },
        "attachments": [
            { "slot": 0, "mime": "image/png" }
        ]
    }
    ```
    Binary blobs (e.g., screenshots) follow immediately after the JSON payload in the same frame to avoid out-of-band coordination.
- **Streaming semantics**: Responses reuse the `id` field and include `status`, `error`, and optional attachments. Long-running operations (`WaitForCondition`) stream periodic `progress` updates before returning `status: "ok"` or `status: "timeout"`.

#### 13.1.2. Harness Runtime Lifecycle

1. **Attach**: Agent sends a `control` message (`{"command":"attach"}`) to lock in a session. Harness responds with negotiated capabilities (available input devices, screenshot formats, rate limits).
2. **Activate context**: Agent issues an `event` to focus a specific ImGui context (e.g., "main", "palette_editor"). Harness binds to the corresponding `ImGuiTestEngine` backend fixture.
3. **Execute actions**: Agent streams `event` objects (`click`, `drag`, `keystroke`, `text_input`). Harness feeds them into the ImGui event queue at the start of the next frame, waits for the frame to settle, then replies.
4. **Query & assert**: Agent interleaves `query` messages (`get_widget_tree`, `capture_screenshot`, `read_value`) and `expect` messages (`assert_property`, `assert_pixel`). Harness routes these to existing ImGuiTestEngine inspectors, lifting the results into structured JSON.
5. **Detach**: Agent issues `{"command":"detach"}` (or connection closes). Harness flushes pending frames, releases sandbox locks, and tears down the socket.

#### 13.1.3. Integration with `z3ed agent`

- **Plan annotation**: The CLI plan schema gains a new step kind `imgui_action` with fields `harness_uri`, `actions[]`, and optional `expect[]`. During execution `z3ed agent run` opens the harness stream, feeds each action, and short-circuits on first failure.
- **Sandbox awareness**: Harness sessions inherit the active sandbox ROM path from `RomSandboxManager`, ensuring UI assertions operate on the same data snapshot as CLI mutations.
- **Telemetry hooks**: Every harness response is appended to the proposal timeline (see §12) with thumbnails for screenshots. Failures bubble up as structured errors with hints (`"missing_widget": "Palette/Cell[12]"`).

### 13.2. Safety & Sandboxing

- **Read-Only Default**: Harness sessions start in read-only mode; mutation commands must explicitly request escalation after presenting a plan (triggering a UI prompt for the user to authorize). Without authorization, only `capture` and `assert` operations succeed.
- **Rate Limiting**: Cap concurrent interactions and enforce per-step quotas to prevent runaway agents.
- **Logging**: Every harness call is logged and linked to the proposal ID, with playback available inside the acceptance UI.

### 13.3. Script Generation Strategy

- **Template Library**: Publish a library of canonical ImGui action sequences (open file, expand tree, focus palette editor). Plans reference templates via IDs to reduce LLM token usage and improve reliability.
- **Auto-Healing**: When a widget lookup fails, the harness can suggest closest matches (Levenshtein distance) so the agent can retry with corrected IDs.
- **Hybrid Execution**: Encourage plans that mix CLI operations for bulk edits and ImGui actions for visual verification, minimizing UI-driven mutations.

## 14. Test & Verification Strategy

### 14.1. Layered Test Suites

- **CLI Unit Tests**: Extend `test/cli/` with high-coverage tests for new resource handlers using sandbox ROM fixtures.
- **Harness Integration Tests**: Add `test/ui/automation/` cases that spin up the harness, replay canned plans, and validate deterministic behavior.
- **End-to-End Agent Scenarios**: Create golden scenarios (e.g., “Recolor Link tunic”, “Shift Dungeon Chest”) that exercise command + UI flows, verifying ROM diffs, UI captures, and pass/fail criteria.

### 14.2. Continuous Verification

- **CI Pipelines**: Introduce dedicated CI jobs for agent features, enabling `YAZE_WITH_JSON` builds, running harness smoke suites, and publishing artifacts (diffs, screenshots) on failure.
- **Nightly Regression**: Schedule nightly runs of expensive ImGui scenarios and long-running CLI scripts with hardware acceleration (Apple Metal) to detect flaky interactions.
- **Fuzzing Hooks**: Instrument command parsers with libFuzzer harnesses to catch malformed LLM output early.

### 14.3. Telemetry-Informed Testing

- **Flake Tracker**: Aggregate harness failures by widget/action to prioritize stabilization.
- **Adaptive Test Selection**: Use proposal metadata to select relevant regression suites dynamically (e.g., palette-focused proposals trigger palette regression tests).
- **Feedback Loop**: Feed test outcomes back into prompt engineering, e.g., annotate prompts with known flaky commands so the LLM favors safer alternatives.

## 15. Expanded Roadmap (Phase 6+)

### Phase 6: Agent Workflow Foundations (Planned)
- Implement resource catalogue tooling and `agent describe` schemas.
- Ship sandbox ROM workflow with proposal tracking and acceptance UI.
- Finalize ImGuiTestHarness MVP with read-only verification.
- Expand CLI surface with sprite/object manipulation commands flagged as agent-safe.

### Phase 7: Controlled Mutation & Review (Planned)
- Enable harness mutation mode with user authorization prompts.
- Deliver inline diff controls and feedback composer UI.
- Wire policy engine for gating accept buttons.
- Launch initial telemetry dashboards (opt-in) for agent performance metrics.

### Phase 8: Learning & Self-Improvement (Exploratory)
- Capture accept/reject rationales to train prompt selectors.
- Experiment with reinforcement signals for local models (reward accepted plans, penalize rejected ones).
- Explore collaborative agent sessions where multiple proposals merge or compete under defined heuristics.
- Investigate deterministic replay of LLM outputs for reliable regression testing.

### 7.4. Widget ID Management for Test Automation

A key challenge in GUI test automation is the fragility of identifying widgets. Relying on human-readable labels (e.g., `"button:Overworld"`) makes tests brittle; a simple text change in the UI can break the entire test suite.

To address this, the `z3ed` ecosystem includes a robust **Widget ID Management** system.

**Goals**:
-   **Decouple Tests from Labels**: Tests should refer to a stable, logical ID, not a display label.
-   **Hierarchical and Scoped IDs**: Allow for organized and unique identification of widgets within complex, nested UIs.
-   **Discoverability**: Enable the test harness to easily find and interact with widgets using these stable IDs.

**Implementation**:
-   **`WidgetIdRegistry`**: A central service that manages the mapping between stable, hierarchical IDs and the dynamic `ImGuiID`s used at runtime.
-   **Hierarchical Naming**: Widget IDs are structured like paths (e.g., `/editors/overworld/toolbar/save_button`). This avoids collisions and provides context.
-   **Registration**: Editor and tool developers are responsible for registering their interactive widgets with the `WidgetIdRegistry` upon creation.
-   **Test Harness Integration**: The `ImGuiTestHarness` uses the registry to look up the current `ImGuiID` for a given stable ID, ensuring it always interacts with the correct widget, regardless of label changes or UI refactoring.

This system is critical for the long-term maintainability of the automated E2E validation pipeline.
