# z3ed: AI-Powered CLI for YAZE

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

## 2. Quick Start

### Build

A single `Z3ED_AI=ON` CMake flag enables all AI features, including JSON, YAML, and httplib dependencies. This simplifies the build process.

```bash
# Build with AI features (RECOMMENDED)
cmake -B build -DZ3ED_AI=ON
cmake --build build --target z3ed

# For GUI automation features, also include gRPC
cmake -B build -DZ3ED_AI=ON -DYAZE_WITH_GRPC=ON
cmake --build build --target z3ed
```

### AI Setup

**Ollama (Recommended for Development)**:
```bash
brew install ollama              # macOS
ollama pull qwen2.5-coder:7b    # Pull recommended model
ollama serve                     # Start server
```

**Gemini (Cloud API)**:
```bash
# Get API key from https://aistudio.google.com/apikey
export GEMINI_API_KEY="your-key-here"
```

### Example Commands

**Conversational Agent**:
```bash
# Interactive chat (FTXUI)
z3ed agent chat --rom zelda3.sfc

# Simple text mode (better for AI/automation)
z3ed agent simple-chat --rom zelda3.sfc

# Batch mode
z3ed agent simple-chat --file queries.txt --rom zelda3.sfc
```

**Proposal Workflow**:
```bash
# Generate from prompt
z3ed agent run --prompt "Place tree at 10,10" --rom zelda3.sfc --sandbox

# List proposals
z3ed agent list

# Review
z3ed agent diff --proposal-id <id>

# Accept
z3ed agent accept --proposal-id <id>
```

## 3. Architecture

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

## 4. Agentic & Generative Workflow (MCP)

The `z3ed` CLI is the foundation for an AI-driven Model-Code-Program (MCP) loop, where the AI agent's "program" is a script of `z3ed` commands.

1.  **Model (Planner)**: The agent receives a natural language prompt and leverages an LLM to create a plan, which is a sequence of `z3ed` commands.
2.  **Code (Generation)**: The LLM returns the plan as a structured JSON object containing actions.
3.  **Program (Execution)**: The `z3ed agent` parses the plan and executes each command sequentially in a sandboxed ROM environment.
4.  **Verification (Tester)**: The `ImGuiTestHarness` is used to run automated GUI tests to verify that the changes were applied correctly.

## 5. Command Reference

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

## 6. Chat Modes

### FTXUI Chat (`agent chat`)
Full-screen interactive terminal with table rendering, syntax highlighting, and scrollable history. Best for manual exploration.

### Simple Chat (`agent simple-chat`)
Lightweight, scriptable text-based REPL that supports single messages, interactive sessions, piped input, and batch files.

### GUI Chat Widget (In Progress)
An ImGui widget in the main YAZE editor that will provide the same functionality with a graphical interface.

## 7. AI Provider Configuration

Z3ED supports multiple AI providers. Configuration is resolved with command-line flags taking precedence over environment variables.

-   `--ai_provider=<provider>`: Selects the AI provider (`mock`, `ollama`, `gemini`).
-   `--ai_model=<model>`: Specifies the model name (e.g., `qwen2.5-coder:7b`, `gemini-1.5-flash`).
-   `--gemini_api_key=<key>`: Your Gemini API key.
-   `--ollama_host=<url>`: The URL for your Ollama server (default: `http://localhost:11434`).

## 8. Roadmap & Implementation Status

**Last Updated**: October 4, 2025

### ✅ Completed

-   **Core Infrastructure**: Resource-oriented CLI, proposal workflow, sandbox manager, and resource catalog are all production-ready.
-   **AI Backends**: Both Ollama (local) and Gemini (cloud) are operational.
-   **Conversational Agent**: The agent service, tool dispatcher (with 5 read-only tools), and TUI/simple chat interfaces are complete.
-   **GUI Test Harness**: A comprehensive GUI testing platform with introspection, widget discovery, recording/replay, and CI integration support.

### 🚧 Active & Next Steps

1.  **Live LLM Testing (1-2h)**: Verify function calling with real models (Ollama/Gemini).
2.  **GUI Chat Integration (6-8h)**: Wire the `AgentChatWidget` into the main YAZE editor.
3.  **Expand Tool Coverage (8-10h)**: Add new read-only tools for inspecting dialogue, sprites, and regions.
4.  **Windows Cross-Platform Testing (8-10h)**: Validate `z3ed` and the test harness on Windows.

## 9. Troubleshooting

-   **"Build with -DZ3ED_AI=ON" warning**: AI features are disabled. Rebuild with the flag to enable them.
-   **"gRPC not available" error**: GUI testing is disabled. Rebuild with `-DYAZE_WITH_GRPC=ON`.
-   **AI generates invalid commands**: The prompt may be vague. Use specific coordinates, tile IDs, and map context.
-   **Chat mode freezes**: Use `agent simple-chat` instead of the FTXUI-based `agent chat` for better stability, especially in scripts.