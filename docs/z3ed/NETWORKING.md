# Z3ED Networking & Collaboration

## Overview

Z3ED provides comprehensive networking capabilities across all three components:
- **yaze app**: GUI application with real-time collaboration
- **z3ed CLI**: Command-line interface for remote operations
- **yaze-server**: WebSocket server for coordination

## Architecture

### Cross-Platform Design

All networking code is designed to work on:
- ✅ **Windows** - Using native Win32 sockets (ws2_32)
- ✅ **macOS** - Using native BSD sockets
- ✅ **Linux** - Using native BSD sockets

### Components

```
┌─────────────┐         WebSocket          ┌──────────────┐
│  yaze app   │◄────────────────────────────►│ yaze-server  │
│  (GUI)      │                             │  (Node.js)   │
└─────────────┘                             └──────────────┘
       ▲                                            ▲
       │                                            │
       │         WebSocket                          │
       │                                            │
       └─────────────┐                              │
                     │                              │
              ┌──────▼──────┐                       │
              │  z3ed CLI   │◄──────────────────────┘
              │             │
              └─────────────┘

       gRPC (for GUI testing)
              │
              │
       ┌──────▼──────┐
       │  yaze app   │
       │  (ImGui)    │
       └─────────────┘
```

## WebSocket Protocol

### Connection

```cpp
#include "app/net/websocket_client.h"

net::WebSocketClient client;

// Connect to server
auto status = client.Connect("localhost", 8765);

// Set up callbacks
client.OnMessage("rom_sync", [](const nlohmann::json& payload) {
    // Handle ROM sync
});

client.OnStateChange([](net::ConnectionState state) {
    // Handle state changes
});
```

### Message Types

#### 1. Session Management

**Host Session**:
```json
{
  "type": "host_session",
  "payload": {
    "session_name": "My ROM Hack",
    "username": "host",
    "rom_hash": "abc123",
    "ai_enabled": true
  }
}
```

**Join Session**:
```json
{
  "type": "join_session",
  "payload": {
    "session_code": "ABC123",
    "username": "participant"
  }
}
```

#### 2. Proposal System (NEW)

**Share Proposal**:
```json
{
  "type": "proposal_share",
  "payload": {
    "sender": "username",
    "proposal_data": {
      "description": "Place tile 0x42 at (5,7)",
      "type": "tile_edit",
      "data": {...}
    }
  }
}
```

**Vote on Proposal** (NEW):
```json
{
  "type": "proposal_vote",
  "payload": {
    "proposal_id": "prop_123",
    "approved": true,
    "username": "voter"
  }
}
```

Response:
```json
{
  "type": "proposal_vote_received",
  "payload": {
    "proposal_id": "prop_123",
    "username": "voter",
    "approved": true,
    "votes": {
      "host": true,
      "user1": true,
      "user2": false
    },
    "timestamp": 1234567890
  }
}
```

**Update Proposal Status**:
```json
{
  "type": "proposal_update",
  "payload": {
    "proposal_id": "prop_123",
    "status": "approved"  // or "rejected", "applied"
  }
}
```

#### 3. ROM Synchronization

**Send ROM Sync**:
```json
{
  "type": "rom_sync",
  "payload": {
    "sender": "username",
    "diff_data": "base64_encoded_diff",
    "rom_hash": "new_hash"
  }
}
```

#### 4. Snapshots

**Share Snapshot**:
```json
{
  "type": "snapshot_share",
  "payload": {
    "sender": "username",
    "snapshot_data": "base64_encoded_image",
    "snapshot_type": "screenshot"
  }
}
```

## YAZE App Integration

### Using WebSocketClient

```cpp
#include "app/net/websocket_client.h"

// Create client
auto client = std::make_unique<net::WebSocketClient>();

// Connect
if (auto status = client->Connect("localhost", 8765); !status.ok()) {
    // Handle error
}

// Host a session
auto session_info = client->HostSession(
    "My Hack",
    "username",
    rom->GetHash(),
    true  // AI enabled
);

// Set up proposal callback
client->OnMessage("proposal_shared", [this](const nlohmann::json& payload) {
    std::string proposal_id = payload["proposal_id"];
    nlohmann::json proposal_data = payload["proposal_data"];
    
    // Add to approval manager
    approval_mgr->SubmitProposal(
        proposal_id,
        payload["sender"],
        proposal_data["description"],
        proposal_data
    );
});

// Vote on proposal
client->VoteOnProposal(proposal_id, true, "my_username");
```

### Using CollaborationService

```cpp
#include "app/net/collaboration_service.h"

// High-level service that integrates everything
auto collab_service = std::make_unique<net::CollaborationService>(rom);

// Initialize with version manager and approval manager
collab_service->Initialize(config, version_mgr, approval_mgr);

// Connect and host
collab_service->Connect("localhost", 8765);
collab_service->HostSession("My Hack", "username");

// Submit local changes as proposal
collab_service->SubmitChangesAsProposal(
    "Modified dungeon room 5",
    "username"
);

// Auto-sync is handled automatically
```

## Z3ED CLI Integration

### Connection Commands

```bash
# Connect to collaboration server
z3ed net connect --host localhost --port 8765

# Join session
z3ed net join --code ABC123 --username myname

# Leave session
z3ed net leave
```

### Proposal Commands

```bash
# Submit proposal from z3ed
z3ed agent run --prompt "Place tile 42 at (5,7)" --submit-proposal

# Check proposal status
z3ed net proposal status --id prop_123

# Wait for approval (blocking)
z3ed net proposal wait --id prop_123 --timeout 60
```

### Example Workflow

```bash
# 1. Connect to server
z3ed net connect --host localhost

# 2. Join session
z3ed net join --code XYZ789 --username alice

# 3. Submit AI-generated proposal
z3ed agent run --prompt "Make boss room more challenging" \
    --submit-proposal --wait-approval

# 4. If approved, changes are applied
# If rejected, original ROM is preserved
```

## Windows-Specific Notes

### Building on Windows

The networking library automatically links Windows socket support:

```cmake
if(WIN32)
  target_link_libraries(yaze_net PUBLIC ws2_32)
endif()
```

### vcpkg Dependencies

For Windows with vcpkg:

```powershell
# Install dependencies
vcpkg install openssl:x64-windows

# CMake will automatically detect and use them
```

### Windows Firewall

You may need to allow connections:

```powershell
# Allow yaze-server
netsh advfirewall firewall add rule name="YAZE Server" dir=in action=allow protocol=TCP localport=8765

# Or through UI: Windows Defender Firewall → Allow an app
```

## Security Considerations

### Transport Security

1. **Use WSS (WebSocket Secure)** in production:
   ```cpp
   client->Connect("wss://server.example.com", 443);
   ```

2. **Server configuration** with SSL:
   ```javascript
   const https = require('https');
   const fs = require('fs');
   
   const server = https.createServer({
     cert: fs.readFileSync('cert.pem'),
     key: fs.readFileSync('key.pem')
   });
   ```

### Approval Security

1. **Host-only mode** (safest):
   ```cpp
   approval_mgr->SetApprovalMode(ApprovalMode::kHostOnly);
   ```

2. **Verify identities**: Use authentication tokens

3. **Rate limiting**: Server limits messages to 100/minute

### ROM Protection

1. **Always create snapshots** before applying proposals:
   ```cpp
   config.create_snapshot_before_sync = true;
   ```

2. **Mark safe points** after verification:
   ```cpp
   version_mgr->MarkAsSafePoint(snapshot_id);
   ```

3. **Auto-rollback** on errors:
   ```cpp
   if (error) {
     version_mgr->RestoreSnapshot(snapshot_before);
   }
   ```

## Platform-Specific Implementation

### httplib WebSocket Support

The implementation uses `cpp-httplib` for cross-platform support:

- **Windows**: Uses Winsock2 (ws2_32.dll)
- **macOS/Linux**: Uses BSD sockets
- **SSL/TLS**: Optional OpenSSL support

### Threading

All platforms use C++11 threads:

```cpp
#include <thread>
#include <mutex>

std::thread receive_thread([this]() {
    // Platform-independent receive loop
});
```

## Error Handling

### Connection Errors

```cpp
auto status = client->Connect(host, port);
if (!status.ok()) {
    if (absl::IsUnavailable(status)) {
        // Server not reachable
    } else if (absl::IsDeadlineExceeded(status)) {
        // Connection timeout
    }
}
```

### Network State

```cpp
client->OnStateChange([](ConnectionState state) {
    switch (state) {
        case ConnectionState::kConnected:
            // Ready to use
            break;
        case ConnectionState::kDisconnected:
            // Clean shutdown
            break;
        case ConnectionState::kReconnecting:
            // Attempting reconnect
            break;
        case ConnectionState::kError:
            // Fatal error
            break;
    }
});
```

## Performance

### Compression

Large messages are compressed:

```cpp
// ROM diffs are compressed before sending
std::string compressed = CompressDiff(diff_data);
client->SendRomSync(compressed, hash, sender);
```

### Batching

Small changes are batched:

```cpp
config.sync_interval_ms = 5000;  // Batch changes over 5 seconds
```

### Rate Limiting

Server enforces:
- 100 messages per minute per client
- 5MB max ROM diff size
- 10MB max snapshot size

## Testing

### Unit Tests

```cpp
TEST(WebSocketClientTest, ConnectAndDisconnect) {
    net::WebSocketClient client;
    ASSERT_TRUE(client.Connect("localhost", 8765).ok());
    EXPECT_TRUE(client.IsConnected());
    client.Disconnect();
    EXPECT_FALSE(client.IsConnected());
}
```

### Integration Tests

```bash
# Start server
cd yaze-server
npm start

# Run tests
cd yaze
cmake --build build --target yaze_net_tests
./build/bin/yaze_net_tests
```

## Troubleshooting

### "Failed to connect"
- Check server is running: `ps aux | grep node`
- Check port is available: `netstat -an | grep 8765`
- Check firewall settings

### "Connection timeout"
- Increase timeout: `client->SetTimeout(10);`
- Check network connectivity
- Verify server address

### "SSL handshake failed"
- Verify OpenSSL is installed
- Check certificate validity
- Use WSS URL: `wss://` not `ws://`

### Windows-specific: "ws2_32.dll not found"
- Reinstall Windows SDK
- Check PATH environment variable
- Use vcpkg for dependencies

## Future Enhancements

- [ ] WebRTC for peer-to-peer connections
- [ ] Binary protocol for faster ROM syncs
- [ ] Automatic reconnection with exponential backoff
- [ ] Connection pooling for multiple sessions
- [ ] NAT traversal for home networks
- [ ] End-to-end encryption for proposals

## See Also

- [Collaboration Guide](COLLABORATION.md) - Version management and approval
- [Z3ED README](README.md) - Main documentation
- [yaze-server README](../../../yaze-server/README.md) - Server setup
