# z3ed Networking, Collaboration, and Remote Access

**Version**: 0.2.0-alpha
**Last Updated**: October 5, 2025

## 1. Overview

This document provides a comprehensive overview of the networking, collaboration, and remote access features within the z3ed ecosystem. These systems are designed to enable everything from real-time collaborative ROM hacking to powerful AI-driven remote automation.

The architecture is composed of three main communication layers:
1.  **WebSocket Protocol**: A real-time messaging layer for multi-user collaboration, managed by the `yaze-server`.
2.  **gRPC Service**: A high-performance RPC layer for programmatic remote ROM manipulation, primarily used by the `z3ed` CLI and automated testing harnesses.
3.  **Collaboration Service**: A high-level C++ API within the YAZE application that integrates version management with the networking protocols.

## 2. Architecture

### 2.1. System Diagram

```
┌─────────────┐         WebSocket          ┌──────────────┐
│  yaze app   │◄────────────────────────────►│ yaze-server  │
│  (GUI)      │                             │  (Node.js)   │
└─────────────┘                             └──────────────┘
       ▲                                            ▲
       │ gRPC (for GUI testing)                     │ WebSocket
       │                                            │
       └─────────────┐                              │
                     │                              │
              ┌──────▼──────┐                       │
              │  z3ed CLI   │◄──────────────────────┘
              │             │
              └─────────────┘
```

### 2.2. Core Components

The collaboration system is built on two key C++ components:

1.  **ROM Version Manager** (`app/net/rom_version_manager.h`)
    -   **Purpose**: Protects the ROM from corruption and unwanted changes.
    -   **Features**:
        -   Automatic periodic snapshots and manual checkpoints.
        -   "Safe points" to mark host-verified versions.
        -   Corruption detection and automatic recovery to the last safe point.
        -   Ability to roll back to any previous version.

2.  **Proposal Approval Manager** (`app/net/rom_version_manager.h`)
    -   **Purpose**: Manages a collaborative voting system for applying changes.
    -   **Features**:
        -   Multiple approval modes: `Host-Only` (default), `Majority`, and `Unanimous`.
        -   Automatically creates snapshots before and after a proposal is applied.
        -   Handles the submission, voting, and application/rejection of proposals.

3.  **Collaboration Panel** (`app/gui/widgets/collaboration_panel.h`)
    -   **Purpose**: Provides a dedicated UI within the YAZE editor for managing collaboration.
    -   **Features**:
        -   Version history timeline with one-click restore.
        -   ROM synchronization tracking.
        -   Visual snapshot gallery.
        -   A voting and approval interface for pending proposals.

## 3. Protocols

### 3.1. WebSocket Protocol (yaze-server v2.0)

Used for real-time, multi-user collaboration.

**Connection**:
```javascript
const ws = new WebSocket('ws://localhost:8765');
```

**Message Types**:

| Type | Sender | Payload Description |
| :--- | :--- | :--- |
| `host_session` | Client | Initiates a new session with a name, username, and optional ROM hash. |
| `join_session` | Client | Joins an existing session using a 6-character code. |
| `rom_sync` | Client | Broadcasts a base64-encoded ROM diff to all participants. |
| `proposal_share` | Client | Shares a new proposal (e.g., from an AI agent) with the group. |
| `proposal_vote` | Client | Submits a vote (approve/reject) for a specific proposal. |
| `snapshot_share` | Client | Shares a snapshot (e.g., a screenshot) with the group. |
| `proposal_update` | Server | Broadcasts the new status of a proposal (`approved`, `rejected`). |
| `proposal_vote_received`| Server | Confirms a vote was received and shows the current vote tally. |
| `session_hosted` | Server | Confirms a session was created and returns its code. |
| `session_joined` | Server | Confirms a user joined and provides session state (participants, history). |

### 3.2. gRPC Service (Remote ROM Manipulation)

Provides a high-performance API for programmatic access to the ROM, used by the `z3ed` CLI and test harnesses.

**Status**: ✅ Designed and Implemented. Pending final build system integration.

**Protocol Buffer (`protos/rom_service.proto`)**:

```proto
service RomService {
  // Core
  rpc ReadBytes(ReadBytesRequest) returns (ReadBytesResponse);
  rpc WriteBytes(WriteBytesRequest) returns (WriteBytesResponse);
  rpc GetRomInfo(GetRomInfoRequest) returns (GetRomInfoResponse);

  // Overworld
  rpc ReadOverworldMap(ReadOverworldMapRequest) returns (ReadOverworldMapResponse);
  rpc WriteOverworldTile(WriteOverworldTileRequest) returns (WriteOverworldTileResponse);

  // Dungeon
  rpc ReadDungeonRoom(ReadDungeonRoomRequest) returns (ReadDungeonRoomResponse);
  rpc WriteDungeonTile(WriteDungeonTileRequest) returns (WriteDungeonTileResponse);

  // Proposals
  rpc SubmitRomProposal(SubmitRomProposalRequest) returns (SubmitRomProposalResponse);
  rpc GetProposalStatus(GetProposalStatusRequest) returns (GetProposalStatusResponse);

  // Version Management
  rpc CreateSnapshot(CreateSnapshotRequest) returns (CreateSnapshotResponse);
  rpc RestoreSnapshot(RestoreSnapshotRequest) returns (RestoreSnapshotResponse);
  rpc ListSnapshots(ListSnapshotsRequest) returns (ListSnapshotsResponse);
}
```

**Use Case: Write with Approval via `z3ed`**
The `z3ed` CLI can submit a change as a proposal via gRPC and wait for a host to approve it in the YAZE GUI.

```bash
# 1. CLI connects and submits a proposal via gRPC
z3ed net connect --host server.example.com --port 50051
z3ed agent run --prompt "Make dungeon 5 harder" --submit-grpc-proposal

# 2. The YAZE GUI receives the proposal and the host approves it.

# 3. The z3ed CLI polls for the status and receives the approval.
```

## 4. Client Integration

### 4.1. YAZE App Integration

The `CollaborationService` provides a high-level API that integrates the version manager, approval manager, and WebSocket client.

```cpp
#include "app/net/collaboration_service.h"

// High-level service that integrates everything
auto collab_service = std::make_unique<net::CollaborationService>(rom);

// Initialize with managers
collab_service->Initialize(config, version_mgr, approval_mgr);

// Connect and host
collab_service->Connect("localhost", 8765);
collab_service->HostSession("My Hack", "username");

// Submit local changes as a proposal to the group
collab_service->SubmitChangesAsProposal("Modified dungeon room 5", "username");
```

### 4.2. z3ed CLI Integration

The CLI provides `net` commands for interacting with the collaboration server.

```bash
# Connect to a collaboration server
z3ed net connect --host localhost --port 8765

# Join an existing session
z3ed net join --code ABC123 --username myname

# Submit an AI-generated change as a proposal and wait for it to be approved
z3ed agent run --prompt "Make boss room more challenging" --submit-proposal --wait-approval

# Manually check a proposal's status
z3ed net proposal status --id prop_123
```

## 5. Best Practices & Troubleshooting

### Best Practices
- **For Hosts**: Enable auto-backups, mark safe points after playtesting, and use `Host-Only` approval mode for maximum control.
- **For Participants**: Submit all changes as proposals, wait for approval, and use descriptive names.
- **For Everyone**: Test changes in an emulator before submitting, make small atomic changes, and communicate with the team.

### Troubleshooting
- **"Failed to connect"**: Check that the `yaze-server` is running and the port is not blocked by a firewall.
- **"Corruption detected"**: Use `version_mgr->AutoRecover()` or manually restore the last known safe point from the Collaboration Panel.
- **"Snapshot not found"**: Verify the snapshot ID is correct and wasn't deleted due to storage limits.
- **"SSL handshake failed"**: Ensure you are using a `wss://` URL and that a valid SSL certificate is configured on the server.

## 6. Security Considerations
- **Transport Security**: Production environments should use WebSocket Secure (`wss://`) with a valid SSL/TLS certificate.
- **Approval Security**: `Host-Only` mode is the safest. For more open collaboration, consider a token-based authentication system.
- **ROM Protection**: The `RomVersionManager` is the primary defense. Always create snapshots before applying proposals and mark safe points often.
    - **Rate Limiting**: The `yaze-server` enforces a rate limit (default: 100 messages/minute) to prevent abuse.

## 7. Future Enhancements

-   **Encrypted WebSocket (WSS) support**
-   **Diff visualization in UI**
-   **Merge conflict resolution**
-   **Branch/fork support for experimental changes**
-   **AI-assisted proposal review**
-   **Cloud snapshot backup**
-   **Multi-host sessions**
-   **Access control lists (ACLs)**
-   **WebRTC for peer-to-peer connections**
-   **Binary protocol for faster ROM syncs**
-   **Automatic reconnection with exponential backoff**
-   **Connection pooling for multiple sessions**
-   **NAT traversal for home networks**
-   **End-to-end encryption for proposals**