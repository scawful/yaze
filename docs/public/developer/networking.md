# B5 - Architecture and Networking

This document provides a comprehensive overview of the yaze application's architecture, focusing on its service-oriented design, gRPC integration, and real-time collaboration features. For build/preset instructions when enabling gRPC/automation presets, refer to the [Build & Test Quick Reference](../build/quick-reference.md).

## 1. High-Level Architecture

The yaze ecosystem is split into two main components: the **YAZE GUI Application** and the **`z3ed` CLI Tool**.

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         YAZE GUI Application                             │
│                         (Runs on local machine)                          │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                           │
│  ┌─────────────────────────────────────────────────────────────┐        │
│  │  UnifiedGRPCServer (Port 50051)                              │        │
│  │  ════════════════════════════════════════════════════════    │        │
│  │  Hosts 3 gRPC Services on a SINGLE PORT:                     │        │
│  │                                                               │        │
│  │  1. ImGuiTestHarness Service                                 │        │
│  │     • GUI automation (click, type, wait, assert)             │        │
│  │                                                               │        │
│  │  2. RomService                                               │        │
│  │     • Read/write ROM bytes                                   │        │
│  │     • Proposal system for collaborative editing              │        │
│  │                                                               │        │
│  │  3. CanvasAutomation Service                                 │        │
│  │     • High-level canvas operations (tile ops, selection)     │        │
│  └─────────────────────────────────────────────────────────────┘        │
│                                    ↑                                      │
│                                    │ gRPC Connection                      │
└────────────────────────────────────┼──────────────────────────────────────┘
                                     │
                                     ↓
┌────────────────────────────────────┼──────────────────────────────────────┐
│                         z3ed CLI Tool                                     │
│                         (Command-line interface)                          │
├─────────────────────────────────────────────────────────────────────────┤
│  ┌──────────────────────────────────────────────────────────────┐       │
│  │  CLI Services (Business Logic - NOT gRPC servers)             │       │
│  │  ══════════════════════════════════════════════════           │       │
│  │  - AI Services (Gemini, Ollama)                               │       │
│  │  - Agent Services (Chat, Tool Dispatcher)                     │       │
│  │  - Network Clients (gRPC and WebSocket clients)               │       │
│  └──────────────────────────────────────────────────────────────┘       │
└───────────────────────────────────────────────────────────────────────────┘
```

## 2. Service Taxonomy

It's important to distinguish between the two types of "services" in the yaze project.

### APP Services (gRPC Servers)
- **Location**: `src/app/core/service/`, `src/app/net/`
- **Runs In**: YAZE GUI application
- **Purpose**: Expose application functionality to remote clients (like the `z3ed` CLI).
- **Type**: gRPC **SERVER** implementations.

### CLI Services (Business Logic)
- **Location**: `src/cli/service/`
- **Runs In**: `z3ed` CLI tool
- **Purpose**: Implement the business logic for the CLI commands.
- **Type**: These are helper classes, **NOT** gRPC servers. They may include gRPC **CLIENTS** to connect to the APP Services.

## 3. gRPC Services

yaze exposes its core functionality through a `UnifiedGRPCServer` that hosts multiple services on a single port (typically 50051).

### ImGuiTestHarness Service
- **Proto**: `imgui_test_harness.proto`
- **Purpose**: GUI automation.
- **Features**: Click, type, screenshots, widget discovery.

### RomService
- **Proto**: `rom_service.proto`
- **Purpose**: Low-level ROM manipulation.
- **Features**: Read/write bytes, proposal system for collaborative editing, snapshots for version management.

### CanvasAutomation Service
- **Proto**: `canvas_automation.proto`
- **Purpose**: High-level, abstracted control over canvas-based editors.
- **Features**: Tile operations (get/set), selection management, view control (pan/zoom).

## 4. Real-Time Collaboration

Real-time collaboration is enabled through a WebSocket-based protocol managed by the `yaze-server`, a separate Node.js application.

### Architecture
```
┌─────────────┐         WebSocket          ┌──────────────┐
│  yaze app   │◄────────────────────────────►│ yaze-server  │
│  (GUI)      │                             │  (Node.js)   │
└─────────────┘                             └──────────────┘
       ▲                                            ▲
       │ gRPC                                       │ WebSocket
       └─────────────┐                              │
              ┌──────▼──────┐                       │
              │  z3ed CLI   │◄──────────────────────┘
              └─────────────┘
```

### Core Components
- **ROM Version Manager**: Protects the ROM from corruption with snapshots and safe points.
- **Proposal Approval Manager**: Manages a collaborative voting system for applying changes.
- **Collaboration Panel**: A UI within the YAZE editor for managing collaboration.

### WebSocket Protocol
The WebSocket protocol handles real-time messaging for:
- Hosting and joining sessions.
- Broadcasting ROM diffs (`rom_sync`).
- Sharing and voting on proposals (`proposal_share`, `proposal_vote`).
- Sharing snapshots.

## 5. Data Flow Example: AI Agent Edits a Tile

1.  **User** runs `z3ed agent --prompt "Paint grass at 10,10"`.
2.  The **GeminiAIService** (a CLI Service) parses the prompt and returns a tool call to `overworld-set-tile`.
3.  The **ToolDispatcher** (a CLI Service) routes this to the appropriate handler.
4.  The **Tile16ProposalGenerator** (a CLI Service) creates a proposal.
5.  The **GuiAutomationClient** (a CLI Service acting as a gRPC client) calls the `CanvasAutomation.SetTile()` RPC method on the YAZE application's `UnifiedGRPCServer`.
6.  The **CanvasAutomationService** in the YAZE app receives the request and uses the `CanvasAutomationAPI` to paint the tile.
7.  If collaboration is active, the change is submitted through the **ProposalApprovalManager**, which communicates with the `yaze-server` via WebSocket to manage voting.
8.  Once approved, the change is applied to the ROM and synced with all collaborators.
