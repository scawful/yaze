# Agent Editor Module

This directory contains all agent and network collaboration functionality for yaze and [yaze-server](https://github.com/scawful/yaze-server).

## Overview

The Agent Editor module provides AI-powered assistance and collaborative editing features for ROM hacking projects. It integrates conversational AI agents, local and network-based collaboration, and multimodal (vision) capabilities.

## Architecture

### Core Components

#### AgentEditor (`agent_editor.h/cc`)
The main manager class that coordinates all agent-related functionality:
- Manages the chat widget lifecycle
- Coordinates local and network collaboration modes
- Provides high-level API for session management
- Handles multimodal callbacks (screenshot capture, Gemini integration)

**Key Features:**
- Unified interface for all agent functionality
- Mode switching between local and network collaboration
- ROM context management for agent queries
- Integration with toast notifications and proposal drawer
- Agent Builder workspace for persona, tool-stack, automation, and validation planning

#### AgentChat (`agent_chat.h/cc`)
Unified ImGui-based chat interface for interacting with AI agents:
- Real-time conversation with AI assistant
- Message history with JSON persistence (Save/Load)
- Proposal preview and quick actions
- Toolbar with auto-scroll, timestamps, and reasoning toggles
- Table data visualization for structured responses

**Features:**
- Auto-scrolling chat with timestamps (togglable)
- JSON response formatting
- Table data visualization (TableData support)
- Proposal metadata display
- Code block rendering with syntax highlighting
- Automation telemetry support for test harness integration

#### AgentChatHistoryCodec (`agent_chat_history_codec.h/cc`)
Serialization/deserialization for chat history:
- JSON-based persistence (when built with `YAZE_WITH_JSON`)
- Graceful degradation when JSON support unavailable
- Saves collaboration state, multimodal state, and full chat history
- Shared history support for collaborative sessions

### Collaboration Coordinators

#### AgentCollaborationCoordinator (`agent_collaboration_coordinator.h/cc`)
Local filesystem-based collaboration:
- Creates session files in `~/.yaze/agent/sessions/`
- Generates shareable session codes
- Participant tracking via file system
- Polling-based synchronization

**Use Case:** Same-machine collaboration or cloud-folder syncing (Dropbox, iCloud)

#### NetworkCollaborationCoordinator (`network_collaboration_coordinator.h/cc`)
WebSocket-based network collaboration (requires `YAZE_WITH_GRPC` and `YAZE_WITH_JSON`):
- Real-time connection to collaboration server
- Message broadcasting to all session participants
- Live participant updates
- Session management (host/join/leave)

**Advanced Features (v2.0):**
- **ROM Synchronization** - Share ROM edits and diffs across all participants
- **Multimodal Snapshot Sharing** - Share screenshots and images with session members
- **Proposal Management** - Share and track AI-generated proposals with status updates
- **AI Agent Integration** - Route queries to AI agents for ROM analysis

**Use Case:** Remote collaboration across networks

**Server:** See `yaze-server` repository for the Node.js WebSocket server v2.0

## Usage

### Initialization

```cpp
// In EditorManager or main application:
agent_editor_.Initialize(&toast_manager_, &proposal_drawer_);

// Set up ROM context
agent_editor_.SetRomContext(current_rom_);

// Access the agent chat component
agent_editor_.GetAgentChat();
```

### Drawing

```cpp
// In main render loop:
agent_editor_.Draw();
```

### Session Management

```cpp
// Host a local session
auto session = agent_editor_.HostSession("My ROM Hack", 
                                         AgentEditor::CollaborationMode::kLocal);

// Join a session by code
auto session = agent_editor_.JoinSession("ABC123", 
                                         AgentEditor::CollaborationMode::kLocal);

// Leave session
agent_editor_.LeaveSession();
```

### Network Mode (requires YAZE_WITH_GRPC and YAZE_WITH_JSON)

```cpp
// Connect to collaboration server
agent_editor_.ConnectToServer("ws://localhost:8765");

// Host network session with optional ROM hash and AI support
auto session = agent_editor_.HostSession("Network Session", 
                                         AgentEditor::CollaborationMode::kNetwork);

// Using advanced features (v2.0)
// Send ROM sync
network_coordinator->SendRomSync(username, base64_diff_data, rom_hash);

// Share snapshot
network_coordinator->SendSnapshot(username, base64_image_data, "overworld_editor");

// Share proposal
network_coordinator->SendProposal(username, proposal_json);

// Send AI query
network_coordinator->SendAIQuery(username, "What enemies are in room 5?");
```

### Agent Builder Workflow

The `Agent Builder` tab inside AgentEditor walks you through five phases:

1. **Persona & Goals** – capture the agent’s tone, guardrails, and explicit objectives.
2. **Tool Stack** – toggle dispatcher categories (resources, dungeon, overworld, dialogue, GUI, music, sprite, emulator) and sync the plan to the chat widget.
3. **Automation Hooks** – configure automatic harness execution, ROM syncing, and proposal focus behaviour for full E2E runs.
4. **Validation** – document success criteria and testing notes.
5. **E2E Checklist** – track readiness (automation toggles, persona, ROM sync) before triggering full end-to-end harness runs. Builder stages can be exported/imported as JSON blueprints (`~/.yaze/agent/blueprints/*.json`) for reuse across projects.

Builder plans can be applied directly to the agent configuration state so that UI and CLI automation stay in sync.

## File Structure

```
agent/
├── README.md                               (this file)
├── agent_editor.h                          Main manager class
├── agent_editor.cc
├── agent_chat.h                            Unified chat interface
├── agent_chat.cc
├── agent_ui_controller.h                   UI coordination
├── agent_ui_controller.cc
├── agent_ui_theme.h                        Theme colors for agent UI
├── agent_ui_theme.cc
├── agent_chat_history_codec.h              History serialization
├── agent_chat_history_codec.cc
├── agent_collaboration_coordinator.h       Local file-based collaboration
├── agent_collaboration_coordinator.cc
├── network_collaboration_coordinator.h     WebSocket collaboration
├── network_collaboration_coordinator.cc
└── panels/                                 Agent editor panels
    └── agent_editor_panels.h/cc
```

## Build Configuration

### Required
- `YAZE_WITH_JSON` - Enables chat history persistence (via nlohmann/json)

### Optional
- `YAZE_WITH_GRPC` - Enables all agent features including network collaboration
  - Without this flag, agent functionality is completely disabled

## Data Files

### Local Storage
- **Chat History:** `~/.yaze/agent/chat_history.json`
- **Shared Sessions:** `~/.yaze/agent/sessions/<session_id>_history.json`
- **Session Metadata:** `~/.yaze/agent/sessions/<code>.session`

### Session File Format
```json
{
  "session_name": "My ROM Hack",
  "session_code": "ABC123",
  "host": "username",
  "participants": ["username", "friend1", "friend2"]
}
```

## Integration with EditorManager

The `AgentEditor` is instantiated as a member of `EditorManager` and integrated into the main UI:

```cpp
class EditorManager {
#ifdef YAZE_WITH_GRPC
  AgentEditor agent_editor_;
#endif
};
```

Menu integration:
```cpp
{ICON_MD_CHAT " Agent Chat", "", 
 [this]() { agent_editor_.ToggleChat(); },
 [this]() { return agent_editor_.IsChatActive(); }}
```

## Dependencies

### Internal
- `cli::agent::ConversationalAgentService` - AI agent backend
- `cli::GeminiAIService` - Gemini API for multimodal queries
- `yaze::test::*` - Screenshot capture utilities
- `ProposalDrawer` - Displays agent proposals
- `ToastManager` - User notifications

### External (when enabled)
- nlohmann/json - Chat history serialization
- httplib - WebSocket client implementation
- Abseil - Status handling, time utilities

## Advanced Features (v2.0)

The network collaboration coordinator now supports:

### ROM Synchronization
Share ROM edits in real-time:
- Send diff data (base64 encoded) to all participants
- Automatic ROM hash tracking
- Size limits enforced by server (5MB max)

### Multimodal Snapshot Sharing
Share screenshots and images:
- Capture and share specific editor views
- Support for multiple snapshot types (overworld, dungeon, sprite, etc.)
- Base64 encoding for efficient transfer
- Size limits enforced by server (10MB max)

### Proposal Management
Collaborative proposal workflow:
- Share AI-generated proposals with all participants
- Track proposal status (pending, accepted, rejected)
- Real-time status updates broadcast to all users

### AI Agent Integration
Server-side AI routing:
- Send queries through the collaboration server
- Shared AI responses visible to all participants
- Query history tracked in server database

### Health Monitoring
Server health and metrics:
- `/health` endpoint for server status
- `/metrics` endpoint for usage statistics
- Graceful shutdown notifications

## Future Enhancements

1. **Voice chat integration** - Audio channels for remote collaboration
2. **Shared cursor/viewport** - See what collaborators are editing
3. **Conflict resolution UI** - Handle concurrent edits gracefully
4. **Session replay** - Record and playback editing sessions
5. **Agent memory** - Persistent context across sessions
6. **Real-time cursor tracking** - See where collaborators are working
7. **Blueprint templates** - Share agent personas/tool stacks between teams

## Server Protocol

The server uses JSON WebSocket messages. Key message types:

### Client → Server
- `host_session` - Create new session (v2.0: supports `rom_hash`, `ai_enabled`)
- `join_session` - Join existing session
- `leave_session` - Leave current session
- `chat_message` - Send message (v2.0: supports `message_type`, `metadata`)
- `rom_sync` - **New in v2.0** - Share ROM diff
- `snapshot_share` - **New in v2.0** - Share screenshot/image
- `proposal_share` - **New in v2.0** - Share proposal
- `proposal_update` - **New in v2.0** - Update proposal status
- `ai_query` - **New in v2.0** - Query AI agent

### Server → Client
- `session_hosted` - Session created confirmation
- `session_joined` - Joined session confirmation
- `chat_message` - Broadcast message
- `participant_joined` / `participant_left` - Participant changes
- `rom_sync` - **New in v2.0** - ROM diff broadcast
- `snapshot_shared` - **New in v2.0** - Snapshot broadcast
- `proposal_shared` - **New in v2.0** - Proposal broadcast
- `proposal_updated` - **New in v2.0** - Proposal status update
- `ai_response` - **New in v2.0** - AI agent response
- `server_shutdown` - **New in v2.0** - Server shutting down
- `error` - Error message
