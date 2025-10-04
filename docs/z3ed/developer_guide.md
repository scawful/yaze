# z3ed Developer Guide

**Version**: 0.1.0-alpha  
**Last Updated**: October 4, 2025

## 1. Overview

This document is the **source of truth** for the z3ed CLI architecture, design, and roadmap. It outlines the evolution of `z3ed` into a powerful, scriptable, and extensible tool for both manual and AI-driven ROM hacking.

`z3ed` has successfully implemented its core infrastructure and is **production-ready on macOS**.

### Core Capabilities

1.  **Conversational Agent**: Chat with an AI (Ollama or Gemini) to explore ROM contents and plan changes using natural language.
2.  **GUI Test Automation**: A gRPC-based test harness allows for widget discovery, test recording/replay, and introspection for debugging and AI-driven validation.
3.  **Proposal System**: A safe, sandboxed editing workflow where all changes are tracked as "proposals" that require human review and acceptance.
4.  **Resource-Oriented CLI**: A clean `z3ed <resource> <action>` command structure that is both human-readable and machine-parsable.

## 2. Architecture

The z3ed system is composed of several layers, from the high-level AI agent down to the YAZE GUI and test harness.

### System Components Diagram

```
┌─────────────────────────────────────────────────────────┐
│ AI Agent Layer (LLM: Ollama, Gemini)                    │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────┐
│ z3ed CLI (Command-Line Interface)                       │
│  ├─ agent run/plan/diff/test/list/describe              │
│  └─ rom/palette/overworld/dungeon commands              │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────┐
│ Service Layer (Singleton Services)                      │
│  ├─ ProposalRegistry (Proposal Tracking)                │
│  ├─ RomSandboxManager (Isolated ROM Copies)             │
│  ├─ ResourceCatalog (Machine-Readable API Specs)        │
│  └─ ConversationalAgentService (Chat & Tool Dispatch)   │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────┐
│ ImGuiTestHarness (gRPC Server in YAZE)                  │
│  ├─ Ping, Click, Type, Wait, Assert, Screenshot         │
│  └─ Introspection & Discovery RPCs                      │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────┐
│ YAZE GUI (ImGui Application)                            │
│  └─ ProposalDrawer & Editor Windows                     │
└─────────────────────────────────────────────────────────┘
```

### Key Architectural Decisions

-   **Resource-Oriented Command Structure**: `z3ed <resource> <action>` for clarity and extensibility.
-   **Machine-Readable API**: All commands are documented in `docs/api/z3ed-resources.yaml` with structured schemas for AI consumption.
-   **Proposal-Based Workflow**: AI-generated changes are sandboxed as "proposals" requiring human review.
-   **gRPC Test Harness**: An embedded gRPC server in YAZE enables remote GUI automation.

## 3. Command Reference

This section provides a reference for the core `z3ed` commands.

### Agent Commands

-   `agent run --prompt "..."`: Executes an AI-driven ROM modification in a sandbox.
-   `agent plan --prompt "..."`: Shows the sequence of commands the AI plans to execute.
-   `agent list`: Shows all proposals and their status.
-   `agent diff [--proposal-id <id>]`: Shows the changes, logs, and metadata for a proposal.
-   `agent describe [--resource <name>]`: Exports machine-readable API specifications for AI consumption.
-   `agent chat`: Opens an interactive terminal chat (TUI) with the AI agent.
-   `agent simple-chat`: A lightweight, non-TUI chat mode for scripting and automation.
-   `agent test ...`: Commands for running and managing automated GUI tests.

### Resource Commands

-   `rom info|validate|diff`: Commands for ROM file inspection and comparison.
-   `palette export|import|list`: Commands for palette manipulation.
-   `overworld get-tile|find-tile|set-tile`: Commands for overworld editing.
-   `dungeon list-sprites|list-rooms`: Commands for dungeon inspection.

## 4. Agentic & Generative Workflow (MCP)

The `z3ed` CLI is the foundation for an AI-driven Model-Code-Program (MCP) loop, where the AI agent's "program" is a script of `z3ed` commands.

1.  **Model (Planner)**: The agent receives a natural language prompt and leverages an LLM to create a plan, which is a sequence of `z3ed` commands.
2.  **Code (Generation)**: The LLM returns the plan as a structured JSON object containing actions.
3.  **Program (Execution)**: The `z3ed agent` parses the plan and executes each command sequentially in a sandboxed ROM environment.
4.  **Verification (Tester)**: The `ImGuiTestHarness` is used to run automated GUI tests to verify that the changes were applied correctly.

## 5. Roadmap & Implementation Status

**Last Updated**: October 4, 2025

### ✅ Completed

-   **Core Infrastructure**: Resource-oriented CLI, proposal workflow, sandbox manager, and resource catalog are all production-ready.
-   **AI Backends**: Both Ollama (local) and Gemini (cloud) are operational.
-   **Conversational Agent**: The agent service, tool dispatcher (with 5 read-only tools), and TUI/simple chat interfaces are complete.
-   **GUI Test Harness (IT-01 to IT-09)**: A comprehensive GUI testing platform with introspection, widget discovery, recording/replay, enhanced error reporting, and CI integration support.

### 🚧 Active & Next Steps

1.  **Live LLM Testing (1-2h)**: Verify function calling with real models (Ollama/Gemini).
2.  **GUI Chat Integration (6-8h)**: Wire the `AgentChatWidget` into the main YAZE editor.
3.  **Expand Tool Coverage (8-10h)**: Add new read-only tools for inspecting dialogue, sprites, and regions.
4.  **Windows Cross-Platform Testing (8-10h)**: Validate `z3ed` and the test harness on Windows.

## 6. Technical Implementation Details

### Build System

A single `Z3ED_AI=ON` CMake flag enables all AI features, including JSON, YAML, and httplib dependencies. This simplifies the build process and is designed for the upcoming build modularization.

**Build Command (with AI features):**
```bash
cmake -B build -DZ3ED_AI=ON
cmake --build build --target z3ed
```

--- 

## 7. AI Provider Configuration

Z3ED supports multiple AI providers for conversational agent features. You can configure the AI service using command-line flags, making it easy to switch between providers without modifying environment variables.

### Supported Providers

-   **Mock (Default)**: For testing and development. Returns placeholder responses. Use `--ai_provider=mock`.
-   **Ollama (Local)**: For local LLM inference. Requires an Ollama server and a downloaded model. Use `--ai_provider=ollama`.
-   **Gemini (Cloud)**: Google's cloud-based AI. Requires a Gemini API key. Use `--ai_provider=gemini`.

### Core Flags

-   `--ai_provider=<provider>`: Selects the AI provider (`mock`, `ollama`, `gemini`).
-   `--ai_model=<model>`: Specifies the model name (e.g., `qwen2.5-coder:7b`, `gemini-1.5-flash`).
-   `--gemini_api_key=<key>`: Your Gemini API key.
-   `--ollama_host=<url>`: The URL for your Ollama server (default: `http://localhost:11434`).

Configuration is resolved with flags taking precedence over environment variables, which take precedence over defaults.

--- 

## 8. Agent Chat Input Methods

The `z3ed agent simple-chat` command supports multiple input methods for flexibility.

1.  **Single Message Mode**: `z3ed agent simple-chat "<message>" --rom=<path>`
    *   **Use Case**: Quick, one-off scripted queries.

2.  **Interactive Mode**: `z3ed agent simple-chat --rom=<path>`
    *   **Use Case**: Multi-turn conversations and interactive exploration.
    *   **Commands**: `quit`, `exit`, `reset`.

3.  **Piped Input Mode**: `echo "<message>" | z3ed agent simple-chat --rom=<path>`
    *   **Use Case**: Integrating with shell scripts and Unix pipelines.

4.  **Batch File Mode**: `z3ed agent simple-chat --file=<input.txt> --rom=<path>`
    *   **Use Case**: Running documented test suites and performing repeatable validation.

--- 

## 9. Test Harness (gRPC)

The test harness is a gRPC server embedded in the YAZE application, enabling remote control for automated testing. It exposes RPCs for actions like `Click`, `Type`, and `Wait`, as well as advanced introspection and test management.

**Start Test Harness:**
```bash
./build-grpc-test/bin/yaze.app/Contents/MacOS/yaze \
  --enable_test_harness \
  --test_harness_port=50052 \
  --rom_file=assets/zelda3.sfc &
```

**Key RPCs:**
-   **Automation**: `Ping`, `Click`, `Type`, `Wait`, `Assert`, `Screenshot`
-   **Introspection**: `GetTestStatus`, `ListTests`, `GetTestResults`
-   **Discovery**: `DiscoverWidgets`
-   **Recording**: `StartRecording`, `StopRecording`, `ReplayTest`
