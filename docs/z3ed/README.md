# z3ed: AI-Powered CLI for YAZE

**Version**: 0.1.0-alpha
**Last Updated**: October 5, 2025

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

### Hybrid CLI ↔ GUI Workflow

1. **Build once for both surfaces**: `cmake -B build -DZ3ED_AI=ON -DYAZE_WITH_GRPC=ON` followed by `cmake --build build --target z3ed` ensures the CLI, editor chat widget, and ImGui test harness share the same AI and gRPC feature set.
2. **Plan in the CLI**: Use `z3ed agent plan --prompt "Describe the overworld tile 10,10" --rom zelda3.sfc --sandbox` to preview the command sequence the agent intends to execute against an isolated ROM copy.
3. **Execute and validate**: Run `z3ed agent run ... --sandbox` to apply the plan, then launch YAZE with the same ROM and open **Debug → Agent Chat** to review proposal details, streamed logs, and harness status without leaving the editor.
4. **Hand off to GUI automation**: From the Agent Chat widget, trigger the same plan or replay the last CLI run by selecting **Replay Last Plan** (uses the shared proposal registry) to watch the ImGui harness drive the UI.
5. **Tighten the loop**: While the harness executes, use `z3ed agent diff --proposal-id <id>` in the terminal and the Proposal Drawer inside YAZE to compare results side-by-side. Accept or reject directly in either surface—state stays in sync.
6. **Iterate rapidly**: When adjustments are needed, refine the prompt or modify the generated test script, rerun from the CLI, and immediately observe outcomes in the editor via the gRPC-backed harness telemetry panel.

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
│  └─ Automation API shared by CLI & Agent Chat           │
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
-   `agent learn ...`: **NEW**: Manage learned knowledge (preferences, ROM patterns, project context, conversation memory).
-   `agent todo create "Description" [--category=<category>] [--priority=<n>]`: Create a new TODO item.
-   `agent todo list [--status=<status>] [--category=<category>]`: List TODOs, with optional filters.
-   `agent todo update <id> --status=<status>`: Update the status of a TODO item.
-   `agent todo show <id>`: Display full details of a TODO item.
-   `agent todo delete <id>`: Delete a TODO item.
-   `agent todo clear-completed`: Remove all completed TODOs.
-   `agent todo next`: Get the next actionable TODO based on dependencies and priority.
-   `agent todo plan`: Generate a topologically-sorted execution plan for all TODOs.

### Resource Commands

-   `rom info|validate|diff`: Commands for ROM file inspection and comparison.
-   `palette export|import|list`: Commands for palette manipulation.
-   `overworld get-tile|find-tile|set-tile`: Commands for overworld editing.
-   `dungeon list-sprites|list-rooms`: Commands for dungeon inspection.

#### `agent test`: Live Harness Automation

-   **Discover widgets**: `z3ed agent test discover --rom zelda3.sfc --grpc localhost:50051` enumerates ImGui widget IDs through the gRPC-backed harness for later scripting.
-   **Record interactions**: `z3ed agent test record --suite harness/tests/overworld_entry.jsonl` launches YAZE, mirrors your clicks/keystrokes, and persists an editable JSONL trace.
-   **Replay & assert**: `z3ed agent test replay harness/tests/overworld_entry.jsonl --watch` drives the GUI in real time and streams pass/fail telemetry back to both the CLI and Agent Chat widget telemetry panel.
-   **Integrate with proposals**: `z3ed agent test verify --proposal-id <id>` links a recorded scenario with a proposal to guarantee UI state after sandboxed edits.
-   **Debug in the editor**: While a replay is running, open **Debug → Agent Chat → Harness Monitor** to step through events, capture screenshots, or restart the scenario without leaving ImGui.

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
-   `--ai_model=<model>`: Specifies the model name (e.g., `qwen2.5-coder:7b`, `gemini-2.5-flash`).
-   `--gemini_api_key=<key>`: Your Gemini API key.
-   `--ollama_host=<url>`: The URL for your Ollama server (default: `http://localhost:11434`).

### System Prompt Versions

Z3ED includes multiple system prompt versions for different use cases:

-   **v1 (default)**: Original reactive prompt with basic tool calling
-   **v2**: Enhanced with better JSON formatting and error handling
-   **v3 (latest)**: Proactive prompt with intelligent tool chaining and implicit iteration - **RECOMMENDED**

To use v3 prompt: Set environment variable `Z3ED_PROMPT_VERSION=v3` or it will be auto-selected for Gemini 2.0+ models.

## 8. Learn Command - Knowledge Management

The learn command enables the AI agent to remember preferences, patterns, and context across sessions.

### Basic Usage

```bash
# Store a preference
z3ed agent learn --preference "default_palette=2"

# Get a preference
z3ed agent learn --get-preference default_palette

# List all preferences
z3ed agent learn --list-preferences

# View statistics
z3ed agent learn --stats

# Export all learned data
z3ed agent learn --export my_learned_data.json

# Import learned data
z3ed agent learn --import my_learned_data.json
```

### Project Context

Store project-specific information that the agent can reference:

```bash
# Save project context
z3ed agent learn --project "myrom" --context "Vanilla+ difficulty hack, focus on dungeon redesign"

# List projects
z3ed agent learn --list-projects

# Get project details
z3ed agent learn --get-project "myrom"
```

### Conversation Memory

The agent automatically stores summaries of conversations for future reference:

```bash
# View recent memories
z3ed agent learn --recent-memories 10

# Search memories by topic
z3ed agent learn --search-memories "room 5"
```

### Storage Location

All learned data is stored in `~/.yaze/agent/`:
- `preferences.json`: User preferences
- `patterns.json`: Learned ROM patterns
- `projects.json`: Project contexts
- `memories.json`: Conversation summaries

## 9. TODO Management System

The TODO Management System enables the z3ed AI agent to create, track, and execute complex multi-step tasks with dependency management and prioritization.

### Core Capabilities
- ✅ Create and manage TODO items with priorities
- ✅ Track task status (pending, in_progress, completed, blocked, cancelled)
- ✅ Dependency tracking between tasks
- ✅ Automatic execution plan generation
- ✅ Persistent storage in JSON format
- ✅ Category-based organization
- ✅ Tools/functions tracking per task

### Storage Location
TODOs are persisted to: `~/.yaze/agent/todos.json` (macOS/Linux) or `%APPDATA%/yaze/agent/todos.json` (Windows)

## 10. CLI Output & Help System

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

## 10. Collaborative Sessions & Multimodal Vision

### Overview

YAZE supports real-time collaboration for ROM hacking through dual modes: **Local** (filesystem-based) for same-machine collaboration, and **Network** (WebSocket-based via yaze-server v2.0) for internet-based collaboration with advanced features including ROM synchronization, snapshot sharing, and AI agent integration.

---

### Local Collaboration Mode

Perfect for multiple YAZE instances on the same machine or cloud-synced folders (Dropbox, iCloud).

#### How to Use

1. Open YAZE → **Debug → Agent Chat**
2. Select **"Local"** mode
3. **Host a Session:**
   - Enter session name: `Evening ROM Hack`
   - Click **"Host Session"**
   - Share the 6-character code (e.g., `ABC123`)
4. **Join a Session:**
   - Enter the session code
   - Click **"Join Session"**
   - Chat history syncs automatically

#### Features

- **Shared History**: `~/.yaze/agent/sessions/<code>_history.json`
- **Auto-Sync**: 2-second polling for new messages
- **Participant Tracking**: Real-time participant list
- **Toast Notifications**: Get notified when collaborators send messages
- **Zero Setup**: No server required

#### Cloud Folder Workaround

Enable internet collaboration without a server:

```bash
# Link your sessions directory to Dropbox/iCloud
ln -s ~/Dropbox/yaze-sessions ~/.yaze/agent/sessions

# Have your collaborator do the same
# Now you can collaborate through cloud sync!
```

---

### Network Collaboration Mode (yaze-server v2.0)

Real-time collaboration over the internet with advanced features powered by the yaze-server v2.0.

#### Requirements

- **Server**: Node.js 18+ with yaze-server running
- **Client**: YAZE built with `-DYAZE_WITH_GRPC=ON` and `-DZ3ED_AI=ON`
- **Network**: Connectivity between collaborators

#### Server Setup

**Option 1: Using z3ed CLI**
   ```bash
   z3ed collab start [--port=8765]
```

**Option 2: Manual Launch**
```bash
cd /path/to/yaze-server
npm install
npm start

# Server starts on http://localhost:8765
# Health check: curl http://localhost:8765/health
```

**Option 3: Docker**
```bash
docker build -t yaze-server .
docker run -p 8765:8765 yaze-server
```

#### Client Connection

1. Open YAZE → **Debug → Agent Chat**
2. Select **"Network"** mode
3. Enter server URL: `ws://localhost:8765` (or remote server)
4. Click **"Connect to Server"**
5. Host or join sessions like local mode

#### Core Features

**Session Management:**
- Unique 6-character session codes
- Participant tracking with join/leave notifications
- Real-time message broadcasting
- Persistent chat history

**Connection Management:**
- Health monitoring endpoints (`/health`, `/metrics`)
- Graceful shutdown notifications
- Automatic cleanup of inactive sessions
- Rate limiting (100 messages/minute per IP)

#### Advanced Features (v2.0)

**🎮 ROM Synchronization**
Share ROM edits in real-time:
- Send base64-encoded diffs to all participants
- Automatic ROM hash tracking
- Size limit: 5MB per diff
- Conflict detection via hash comparison

**📸 Multimodal Snapshot Sharing**
Share screenshots and images:
- Capture and share specific editor views
- Support for multiple snapshot types (overworld, dungeon, sprite, etc.)
- Base64 encoding for efficient transfer
- Size limit: 10MB per snapshot

**💡 Proposal Management**
Collaborative proposal workflow:
- Share AI-generated proposals with all participants
- Track proposal status: pending, accepted, rejected
- Real-time status updates broadcast to all users
- Proposal history tracked in server database

**🤖 AI Agent Integration**
Server-routed AI queries:
- Send queries through the collaboration server
- Shared AI responses visible to all participants
- Query history tracked in database
- Optional: Disable AI per session

#### Protocol Reference

The server uses JSON WebSocket messages over HTTP/WebSocket transport.

**Client → Server Messages:**

```json
// Host Session (v2.0 with optional ROM hash and AI control)
{
  "type": "host_session",
  "payload": {
    "session_name": "My Session",
    "username": "alice",
    "rom_hash": "abc123...",  // optional
    "ai_enabled": true         // optional, default true
  }
}

// Join Session
{
  "type": "join_session",
  "payload": {
    "session_code": "ABC123",
    "username": "bob"
  }
}

// Chat Message (v2.0 with metadata support)
{
  "type": "chat_message",
  "payload": {
    "sender": "alice",
    "message": "Hello!",
    "message_type": "chat",    // optional: chat, system, ai
    "metadata": {...}          // optional metadata
  }
}

// ROM Sync (NEW in v2.0)
{
  "type": "rom_sync",
  "payload": {
    "sender": "alice",
    "diff_data": "base64_encoded_diff...",
    "rom_hash": "sha256_hash"
  }
}

// Snapshot Share (NEW in v2.0)
{
  "type": "snapshot_share",
  "payload": {
    "sender": "alice",
    "snapshot_data": "base64_encoded_image...",
    "snapshot_type": "overworld_editor"
  }
}

// Proposal Share (NEW in v2.0)
{
  "type": "proposal_share",
  "payload": {
    "sender": "alice",
    "proposal_data": {
      "title": "Add new sprite",
      "description": "...",
      "changes": [...]
    }
  }
}

// Proposal Update (NEW in v2.0)
{
  "type": "proposal_update",
  "payload": {
    "proposal_id": "uuid",
    "status": "accepted"  // pending, accepted, rejected
  }
}

// AI Query (NEW in v2.0)
{
  "type": "ai_query",
  "payload": {
    "username": "alice",
    "query": "What enemies are in the eastern palace?"
  }
}

// Leave Session
{ "type": "leave_session" }

// Ping
{ "type": "ping" }
```

**Server → Client Messages:**

```json
// Session Hosted
{
  "type": "session_hosted",
  "payload": {
    "session_id": "uuid",
    "session_code": "ABC123",
    "session_name": "My Session",
    "participants": ["alice"],
    "rom_hash": "abc123...",
    "ai_enabled": true
  }
}

// Session Joined
{
  "type": "session_joined",
  "payload": {
    "session_id": "uuid",
    "session_code": "ABC123",
    "session_name": "My Session",
    "participants": ["alice", "bob"],
    "messages": [...]
  }
}

// Chat Message (broadcast)
{
  "type": "chat_message",
  "payload": {
    "sender": "alice",
    "message": "Hello!",
    "timestamp": 1709567890123,
    "message_type": "chat",
    "metadata": null
  }
}

// ROM Sync (broadcast, NEW in v2.0)
{
  "type": "rom_sync",
  "payload": {
    "sync_id": "uuid",
    "sender": "alice",
    "diff_data": "base64...",
    "rom_hash": "sha256...",
    "timestamp": 1709567890123
  }
}

// Snapshot Shared (broadcast, NEW in v2.0)
{
  "type": "snapshot_shared",
  "payload": {
    "snapshot_id": "uuid",
    "sender": "alice",
    "snapshot_data": "base64...",
    "snapshot_type": "overworld_editor",
    "timestamp": 1709567890123
  }
}

// Proposal Shared (broadcast, NEW in v2.0)
{
  "type": "proposal_shared",
  "payload": {
    "proposal_id": "uuid",
    "sender": "alice",
    "proposal_data": {...},
    "status": "pending",
    "timestamp": 1709567890123
  }
}

// Proposal Updated (broadcast, NEW in v2.0)
{
  "type": "proposal_updated",
  "payload": {
    "proposal_id": "uuid",
    "status": "accepted",
    "timestamp": 1709567890123
  }
}

// AI Response (broadcast, NEW in v2.0)
{
  "type": "ai_response",
  "payload": {
    "query_id": "uuid",
    "username": "alice",
    "query": "What enemies are in the eastern palace?",
    "response": "The eastern palace contains...",
    "timestamp": 1709567890123
  }
}

// Participant Events
{
  "type": "participant_joined",  // or "participant_left"
  "payload": {
    "username": "bob",
    "participants": ["alice", "bob"]
  }
}

// Server Shutdown (NEW in v2.0)
{
  "type": "server_shutdown",
  "payload": {
    "message": "Server is shutting down. Please reconnect later."
  }
}

// Pong
{
  "type": "pong",
  "payload": { "timestamp": 1709567890123 }
}

// Error
{
  "type": "error",
  "payload": { "error": "Session ABC123 not found" }
}
```

#### Server Configuration

**Environment Variables:**
- `PORT` - Server port (default: 8765)
- `ENABLE_AI_AGENT` - Enable AI agent integration (default: true)
- `AI_AGENT_ENDPOINT` - External AI agent endpoint URL

**Rate Limiting:**
- Window: 60 seconds
- Max messages: 100 per IP per window
- Max snapshot size: 10 MB
- Max ROM diff size: 5 MB

#### Database Schema (Server v2.0)

The server uses SQLite with the following tables:

- **sessions**: Session metadata, ROM hash, AI enabled flag
- **participants**: User tracking with last_seen timestamps
- **messages**: Chat history with message types and metadata
- **rom_syncs**: ROM diff history with hashes
- **snapshots**: Shared screenshots and images
- **proposals**: AI proposal tracking with status
- **agent_interactions**: AI query and response history

#### Deployment

**Heroku:**
```bash
cd /path/to/yaze-server
heroku create yaze-collab
git push heroku main
heroku config:set ENABLE_AI_AGENT=true
```

**VPS (with PM2):**
```bash
git clone https://github.com/scawful/yaze-server
   cd yaze-server
   npm install
npm install -g pm2
pm2 start server.js --name yaze-collab
pm2 startup
pm2 save
```

**Docker:**
```bash
docker build -t yaze-server .
docker run -p 8765:8765 -e ENABLE_AI_AGENT=true yaze-server
```

#### Testing

**Health Check:**
```bash
curl http://localhost:8765/health
curl http://localhost:8765/metrics
```

**Test with wscat:**
```bash
npm install -g wscat
wscat -c ws://localhost:8765

# Host session
> {"type":"host_session","payload":{"session_name":"Test","username":"alice","ai_enabled":true}}

# Join session (in another terminal)
> {"type":"join_session","payload":{"session_code":"ABC123","username":"bob"}}

# Send message
> {"type":"chat_message","payload":{"sender":"alice","message":"Hello!"}}
```

#### Security Considerations

**Current Implementation:**
⚠️ Basic security - suitable for trusted networks
- No authentication or encryption by default
- Plain text message transmission
- Session codes are the only access control

**Recommended for Production:**
1. **SSL/TLS**: Use `wss://` with valid certificates
2. **Authentication**: Implement JWT tokens or OAuth
3. **Session Passwords**: Optional per-session passwords
4. **Persistent Storage**: Use PostgreSQL/MySQL for production
5. **Monitoring**: Add logging to CloudWatch/Datadog
6. **Backup**: Regular database backups

---

### Multimodal Vision (Gemini)

Analyze screenshots of your ROM editor using Gemini's vision capabilities for visual feedback and suggestions.

#### Requirements

- `GEMINI_API_KEY` environment variable set
- YAZE built with `-DYAZE_WITH_GRPC=ON` and `-DZ3ED_AI=ON`

#### Capture Modes

**Full Window**: Captures the entire YAZE application window

**Active Editor** (default): Captures only the currently focused editor window

**Specific Window**: Captures a named window (e.g., "Overworld Editor")

#### How to Use

1. Open **Debug → Agent Chat**
2. Expand **"Gemini Multimodal (Preview)"** panel
3. Select capture mode:
   - ○ Full Window
   - ● Active Editor (default)
   - ○ Specific Window
4. If Specific Window, enter window name: `Overworld Editor`
5. Click **"Capture Snapshot"**
6. Enter prompt: `"What issues do you see with this layout?"`
7. Click **"Send to Gemini"**

#### Example Prompts

- "Analyze the tile placement in this overworld screen"
- "What's wrong with the palette colors in this screenshot?"
- "Suggest improvements for this dungeon room layout"
- "Does this screen follow good level design practices?"
- "Are there any visual glitches or tile conflicts?"
- "How can I improve the composition of this room?"

The AI response appears in your chat history and can reference specific details from the screenshot. In network collaboration mode, multimodal snapshots can be shared with all participants.

---

### Architecture

```
┌──────────────────────────────────────────────────────┐
│                    YAZE Editor                       │
│                                                      │
│  ┌─────────────────────────────────────────────┐   │
│  │         Agent Chat Widget (ImGui)           │   │
│  │                                             │   │
│  │  [Collaboration Panel]                      │   │
│  │  ├─ Local Mode (filesystem)  ✓ Working     │   │
│  │  └─ Network Mode (websocket) ✓ Working     │   │
│  │                                             │   │
│  │  [Multimodal Panel]                         │   │
│  │  ├─ Capture Mode Selection   ✓ Working     │   │
│  │  ├─ Screenshot Capture        ✓ Working     │   │
│  │  └─ Send to Gemini           ✓ Working     │   │
│  └─────────────────────────────────────────────┘   │
│           │                    │                    │
│           ▼                    ▼                    │
│  ┌──────────────────┐  ┌──────────────────┐       │
│  │  Collaboration   │  │  Screenshot      │       │
│  │  Coordinators    │  │  Utils           │       │
│  └──────────────────┘  └──────────────────┘       │
│           │                    │                    │
└───────────┼────────────────────┼────────────────────┘
            │                    │
            ▼                    ▼
┌──────────────────┐    ┌──────────────────┐
│  ~/.yaze/agent/  │    │  Gemini Vision   │
│    sessions/     │    │      API         │
└──────────────────┘    └──────────────────┘
            │
            ▼
┌──────────────────────────────────────────┐
│         yaze-server v2.0                 │
│  - WebSocket Server (Node.js)            │
│  - SQLite Database                       │
│  - Session Management                    │
│  - ROM Sync                              │
│  - Snapshot Sharing                      │
│  - Proposal Management                   │
│  - AI Agent Integration                  │
└──────────────────────────────────────────┘
```

---

### Troubleshooting

**"Failed to start collaboration server"**
- Ensure Node.js is installed: `node --version`
- Check port availability: `lsof -i :8765`
- Verify server directory exists

**"Not connected to collaboration server"**
- Verify server is running: `curl http://localhost:8765/health`
- Check firewall settings
- Confirm server URL is correct

**"Harness client cannot reach gRPC"**
- Confirm YAZE was built with `-DYAZE_WITH_GRPC=ON` and the harness server is enabled via **Debug → Preferences → Automation**.
- Run `z3ed agent test ping --grpc localhost:50051` to verify the CLI can reach the embedded harness endpoint; restart YAZE if the ping fails.
- Inspect the Agent Chat **Harness Monitor** panel for connection status; use **Reconnect** to re-bind if the harness server was restarted.

**"Widget discovery returns empty"**
- Ensure the target ImGui window is open; the harness only indexes visible widgets.
- Toggle **Automation → Enable Introspection** in YAZE to allow the gRPC server to expose widget metadata.
- Run `z3ed agent test discover --window "ProposalDrawer"` to scope discovery to the window you have open.

**"Session not found"**
- Verify session code is correct (case-insensitive)
- Check if session expired (server restart clears sessions)
- Try hosting a new session

**"Rate limit exceeded"**
- Server enforces 100 messages per minute per IP
- Wait 60 seconds and try again

**Participants not updating**
- Click "Refresh Session" button
- Check network connectivity
- Verify server logs for errors

**Messages not broadcasting**
- Ensure all clients are in the same session
- Check session code matches exactly
- Verify network connectivity between client and server

---

### References

- **Server Repository**: [yaze-server](https://github.com/scawful/yaze-server)
- **Agent Editor Docs**: `src/app/editor/agent/README.md`
- **Integration Guide**: `docs/z3ed/YAZE_SERVER_V2_INTEGRATION.md`

## 11. Roadmap & Implementation Status

**Last Updated**: October 4, 2025

### ✅ Completed

-   **Core Infrastructure**: Resource-oriented CLI, proposal workflow, sandbox manager, and resource catalog are all production-ready.
-   **AI Backends**: Both Ollama (local) and Gemini (cloud) are operational.
-   **Conversational Agent**: The agent service, tool dispatcher (with 5 read-only tools), TUI/simple chat interfaces, and ImGui editor chat widget with persistent history.
-   **GUI Test Harness**: A comprehensive GUI testing platform with introspection, widget discovery, recording/replay, and CI integration support.
-   **Collaborative Sessions**: 
    - Local filesystem-based collaborative editing with shared chat history
    - Network WebSocket-based collaboration via yaze-server v2.0
    - Dual-mode support (Local/Network) with seamless switching
-   **Multimodal Vision**: Gemini vision API integration with multiple capture modes (Full Window, Active Editor, Specific Window).
-   **yaze-server v2.0**: Production-ready Node.js WebSocket server with:
    - ROM synchronization with diff broadcasting
    - Multimodal snapshot sharing
    - Collaborative proposal management
    - AI agent integration and query routing
    - Health monitoring and metrics endpoints
    - Rate limiting and security features

### 📌 Current Progress Highlights (October 5, 2025)

-   **Agent Platform Expansion**: AgentEditor now delivers full bot lifecycle controls, live prompt editing, multi-session management, and metrics synchronized with chat history and popup views.
-   **Enhanced Chat Popup**: Left-side AgentChatHistoryPopup evolved into a theme-aware, fully interactive mini-chat with inline sending, multimodal capture, filtering, and proposal indicators to minimize context switching.
-   **Proposal Workflow**: Sandbox-backed proposal review is end-to-end with inline quick actions, ProposalDrawer tie-ins, ROM version protections, and collaboration-aware approvals.
-   **Collaboration & Networking**: yaze-server v2.0 protocol, cross-platform WebSocket client, collaboration panel, and gRPC ROM service unlock real-time edits, diff sharing, and remote automation.
-   **AI & Automation Stack**: Proactive prompt v3, native Gemini function calling, learn/TODO systems, GUI automation planners, multimodal vision suite, and dashboard-surfaced test harness coverage broaden intelligent tooling.

### 🚧 Active & Next Steps

1.  **Harden Live LLM Tooling**: Finalize native function-calling loops with Ollama/Gemini and broaden safe read-only tool coverage for dialogue, sprite, and region introspection.
2.  **Real-Time Transport Upgrade**: Replace HTTP polling with full WebSocket support across CLI/editor and expose ROM sync, snapshot, and proposal voting controls directly inside the AgentChat widget.
3.  **Cross-Platform Certification**: Complete Windows validation for AI, gRPC, collaboration, and build presets leveraging the documented vcpkg workflow.
4.  **UI/UX Roadmap Delivery**: Advance EditorManager menu refactors, enhanced hex/palette tooling, Vim-mode terminal chat, and richer popup affordances such as search, export, and resizing.
5.  **Collaboration Safeguards**: Layer encrypted sessions, conflict resolution flows, AI-assisted proposal review, and deeper gRPC ROM service integrations to strengthen multi-user safety.
6.  **Testing & Observability**: Automate multimodal/GUI harness scenarios, add performance benchmarks, and enable export/replay pipelines for the Test Dashboard.
7.  **Hybrid Workflow Examples**: Document and dogfood end-to-end CLI→GUI automation loops (plan/run/diff + harness replay) with screenshots and recorded sessions.
8.  **Automation API Unification**: Extract a reusable harness automation API consumed by both CLI `agent test` commands and the Agent Chat widget to prevent serialization drift.
9.  **UI Abstraction Cleanup**: Introduce dedicated presenter/controller layers so `editor_manager.cc` delegates to automation and collaboration services, keeping ImGui widgets declarative.

### ✅ Recently Completed (v0.2.0-alpha - October 5, 2025)

#### Core AI Features
-   **Enhanced System Prompt (v3)**: Proactive tool chaining with implicit iteration to minimize back-and-forth conversations
-   **Learn Command**: Full implementation with preferences, ROM patterns, project context, and conversation memory storage
-   **Native Gemini Function Calling**: Upgraded from manual curl to native function calling API with automatic tool schema generation
-   **Multimodal Vision Testing**: Comprehensive test suite for Gemini vision capabilities with screenshot integration
-   **AI-Controlled GUI Automation**: Natural language parsing (`AIActionParser`) and test script generation (`GuiActionGenerator`) for automated tile placement
-   **TODO Management System**: Full `TodoManager` class with CRUD operations, CLI commands, dependency tracking, execution planning, and JSON persistence.

#### Version Management & Protection
-   **ROM Version Management System**: `RomVersionManager` with automatic snapshots, safe points, corruption detection, and rollback capabilities
-   **Proposal Approval Framework**: `ProposalApprovalManager` with host/majority/unanimous voting modes to protect ROM from unwanted changes

#### Networking & Collaboration (NEW)
-   **Cross-Platform WebSocket Client**: `WebSocketClient` with Windows/macOS/Linux support using httplib
-   **Collaboration Service**: `CollaborationService` integrating version management with real-time networking
-   **yaze-server v2.0 Protocol**: Extended with proposal voting (`proposal_vote`, `proposal_vote_received`)
-   **z3ed Network Commands**: CLI commands for remote collaboration (`net connect`, `net join`, `proposal submit/wait`)
-   **Collaboration UI Panel**: `CollaborationPanel` widget with version history, ROM sync tracking, snapshot gallery, and approval workflow
-   **gRPC ROM Service**: Complete protocol buffer and implementation for remote ROM manipulation (pending build integration)

#### UI/UX Enhancements
-   **Welcome Screen Enhancement**: Dynamic theme integration, Zelda-themed animations, and project cards.
-   **Component Refactoring**: `PaletteWidget` renamed and moved, UI organization improved (`app/editor/ui/` for welcome_screen, editor_selection_dialog, background_renderer).

#### Build System & Infrastructure
-   **gRPC Windows Build Optimization**: vcpkg integration for 10-20x faster Windows builds, removed abseil-cpp submodule
-   **Cross-Platform Networking**: Native socket support (ws2_32 on Windows, BSD sockets on Unix)
-   **Namespace Refactoring**: Created `app/net` namespace for networking components
-   **Improved Documentation**: Consolidated architecture, enhancement plans, networking guide, and build instructions with JSON-first approach
-   **Build System Improvements**: `mac-ai` preset, proto fixes, and updated GEMINI.md with AI build policies.

## 12. Troubleshooting

-   **"Build with -DZ3ED_AI=ON" warning**: AI features are disabled. Rebuild with the flag to enable them.
-   **"gRPC not available" error**: GUI testing is disabled. Rebuild with `-DYAZE_WITH_GRPC=ON`.
-   **AI generates invalid commands**: The prompt may be vague. Use specific coordinates, tile IDs, and map context.
-   **Chat mode freezes**: Use `agent simple-chat` instead of the FTXUI-based `agent chat` for better stability, especially in scripts.