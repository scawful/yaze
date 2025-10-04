# z3ed: AI-Powered CLI for YAZE

**Version**: 0.1.0-alpha
**Last Updated**: October 4, 2025

## 1. Overview

This document is the **source of truth** for the z3ed CLI architecture, design, and roadmap. It outlines the evolution of `z3ed` into a powerful, scriptable, and extensible tool for both manual and AI-driven ROM hacking.

`z3ed` has successfully implemented its core infrastructure and is **production-ready on macOS**.

### Core Capabilities

1.  **Conversational Agent**: Chat with an AI (Ollama or Gemini) to explore ROM contents and plan changes using natural language—available from the CLI, terminal UI, and now directly within the YAZE editor.
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

### GUI Chat Widget (Editor Integration)
Accessible from **Debug → Agent Chat** inside YAZE. Provides the same conversation loop as the CLI, including streaming history, JSON/table inspection, and ROM-aware tool dispatch.

**✨ New Features:**
- **Persistent Chat History**: Chat conversations are automatically saved and restored
- **Collaborative Sessions**: Multiple users can join the same session and share a chat history
- **Multimodal Vision**: Capture screenshots of your ROM editor and ask Gemini to analyze them

## 7. AI Provider Configuration

Z3ED supports multiple AI providers. Configuration is resolved with command-line flags taking precedence over environment variables.

-   `--ai_provider=<provider>`: Selects the AI provider (`mock`, `ollama`, `gemini`).
-   `--ai_model=<model>`: Specifies the model name (e.g., `qwen2.5-coder:7b`, `gemini-1.5-flash`).
-   `--gemini_api_key=<key>`: Your Gemini API key.
-   `--ollama_host=<url>`: The URL for your Ollama server (default: `http://localhost:11434`).

## 8. CLI Output & Help System

The `z3ed` CLI features a modernized output system designed to be clean for users and informative for developers.

### Verbose Logging

By default, `z3ed` provides clean, user-facing output. For detailed debugging, including API calls and internal state, use the `--verbose` flag.

**Default (Clean):**
```bash
🤖 AI Provider: gemini
   Model: gemini-2.5-flash
⠋ Thinking...
🔧 Calling tool: resource-list (type=room)
✓ Tool executed successfully
```

**Verbose Mode:**
```bash
# z3ed agent simple-chat "What is room 5?" --verbose
🤖 AI Provider: gemini
   Model: gemini-2.5-flash
[DEBUG] Initializing Gemini service...
[DEBUG] Function calling: disabled
[DEBUG] Using curl for HTTPS request...
⠋ Thinking...
[DEBUG] Parsing response...
🔧 Calling tool: resource-list (type=room)
✓ Tool executed successfully
```

### Hierarchical Help System

The help system is organized by category for easy navigation.

-   **Main Help**: `z3ed --help` or `z3ed -h` shows a high-level overview of command categories.
-   **Category Help**: `z3ed help <category>` provides detailed information for a specific group of commands (e.g., `agent`, `patch`, `rom`).

## 9. Collaborative Sessions & Multimodal Vision

### Collaborative Sessions

Z3ED supports lightweight collaborative sessions where multiple editors on the same machine can share a chat conversation.

**How to Use:**
1. Open YAZE and go to **Debug → Agent Chat**
2. In the Agent Chat widget, expand the **"Collaboration (Preview)"** panel
3. **Host a Session:**
   - Enter a session name (e.g., "Evening ROM Hack")
   - Click "Host Session"
   - Share the generated 6-character code (e.g., `ABC123`) with collaborators
4. **Join a Session:**
   - Enter the session code provided by the host
   - Click "Join Session"
   - Your chat will now sync with others in the session

**Features:**
- Shared chat history stored in `~/.yaze/agent/sessions/<code>_history.json`
- Automatic synchronization when sending/receiving messages
- Participant list shows all connected users
- When you leave a session, you return to your local chat history

### Multimodal Vision (Gemini)

Ask Gemini to analyze screenshots of your ROM editor to get visual feedback and suggestions.

**Requirements:**
- `GEMINI_API_KEY` environment variable set
- YAZE built with `-DYAZE_WITH_GRPC=ON` and `-DZ3ED_AI=ON`

**How to Use:**
1. Open the Agent Chat widget (**Debug → Agent Chat**)
2. Expand the **"Gemini Multimodal (Preview)"** panel
3. Click **"Capture Map Snapshot"** to take a screenshot of the current view
4. Enter a prompt in the text box (e.g., "What issues do you see with this overworld layout?")
5. Click **"Send to Gemini"** to get visual analysis

**Example Prompts:**
- "Analyze the tile placement in this overworld screen"
- "What's wrong with the palette colors in this screenshot?"
- "Suggest improvements for this dungeon room layout"
- "Does this screen follow good level design practices?"

The AI response will appear in your chat history and can reference specific details from the screenshot.

## 10. Roadmap & Implementation Status

**Last Updated**: October 4, 2025

### ✅ Completed

-   **Core Infrastructure**: Resource-oriented CLI, proposal workflow, sandbox manager, and resource catalog are all production-ready.
-   **AI Backends**: Both Ollama (local) and Gemini (cloud) are operational.
-   **Conversational Agent**: The agent service, tool dispatcher (with 5 read-only tools), TUI/simple chat interfaces, and ImGui editor chat widget with persistent history.
-   **GUI Test Harness**: A comprehensive GUI testing platform with introspection, widget discovery, recording/replay, and CI integration support.
-   **Collaborative Sessions**: Local filesystem-based collaborative editing with shared chat history.
-   **Multimodal Vision**: Gemini vision API integration for analyzing ROM editor screenshots.

### 🚧 Active & Next Steps

1.  **Live LLM Testing (1-2h)**: Verify function calling with real models (Ollama/Gemini).
2.  **Expand Tool Coverage (8-10h)**: Add new read-only tools for inspecting dialogue, sprites, and regions.
3.  **Network-Based Collaboration**: Upgrade the filesystem-based collaboration to support remote connections via WebSockets or gRPC.
4.  **Windows Cross-Platform Testing (8-10h)**: Validate `z3ed` and the test harness on Windows.

## 11. Troubleshooting

-   **"Build with -DZ3ED_AI=ON" warning**: AI features are disabled. Rebuild with the flag to enable them.
-   **"gRPC not available" error**: GUI testing is disabled. Rebuild with `-DYAZE_WITH_GRPC=ON`.
-   **AI generates invalid commands**: The prompt may be vague. Use specific coordinates, tile IDs, and map context.
-   **Chat mode freezes**: Use `agent simple-chat` instead of the FTXUI-based `agent chat` for better stability, especially in scripts.