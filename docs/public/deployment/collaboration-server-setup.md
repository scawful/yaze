# Collaboration Server Setup Guide

This guide explains how to set up a WebSocket server for yaze's real-time collaboration feature, enabling multiple users to edit ROMs together.

## Quick Start with yaze-server

The official collaboration server is **[yaze-server](https://github.com/scawful/yaze-server)**, a Node.js WebSocket server with:
- Real-time session management
- AI agent integration (Gemini/Genkit)
- ROM synchronization and diff broadcasting
- Rate limiting and security features

### Local Development
```bash
git clone https://github.com/scawful/yaze-server.git
cd yaze-server
npm install
npm start
# Server runs on ws://localhost:8765
```

### Production Deployment
For production, deploy yaze-server behind an SSL proxy:
- **halext.org**: `wss://yaze.halext.org/ws`
- **Self-hosted**: Deploy to Railway, Render, Fly.io, or your own VPS

---

## Overview

The yaze web app (WASM build) supports real-time collaboration through WebSocket connections. Since GitHub Pages only serves static files, you'll need a separate WebSocket server to enable this feature.

## Architecture

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│  User A         │     │  WebSocket      │     │  User B         │
│  (Browser)      │◄───►│  Server         │◄───►│  (Browser)      │
│  yaze WASM      │     │  (Your Server)  │     │  yaze WASM      │
└─────────────────┘     └─────────────────┘     └─────────────────┘
```

## Protocol Specification

### Message Format

All messages are JSON with a `type` field:

```json
{
  "type": "message_type",
  "...": "additional fields"
}
```

### Client → Server Messages

#### Create Session
```json
{
  "type": "create",
  "room": "ABC123",
  "name": "My Editing Session",
  "user": "username",
  "user_id": "uuid-string",
  "color": "#FF6B6B"
}
```

#### Join Session
```json
{
  "type": "join",
  "room": "ABC123",
  "user": "username",
  "user_id": "uuid-string",
  "color": "#4ECDC4"
}
```

#### Leave Session
```json
{
  "type": "leave",
  "room": "ABC123",
  "user_id": "uuid-string"
}
```

#### Broadcast Change
```json
{
  "type": "change",
  "room": "ABC123",
  "user_id": "uuid-string",
  "offset": 12345,
  "old_data": [1, 2, 3],
  "new_data": [4, 5, 6],
  "timestamp": 1699999999.123
}
```

#### Cursor Update
```json
{
  "type": "cursor",
  "room": "ABC123",
  "user_id": "uuid-string",
  "editor": "overworld",
  "x": 100,
  "y": 200,
  "map_id": 0
}
```

### Server → Client Messages

#### User List Update
```json
{
  "type": "user_list",
  "users": [
    {"id": "uuid-1", "name": "User1", "color": "#FF6B6B", "is_active": true},
    {"id": "uuid-2", "name": "User2", "color": "#4ECDC4", "is_active": true}
  ]
}
```

#### Change Broadcast
```json
{
  "type": "change",
  "user_id": "uuid-string",
  "offset": 12345,
  "old_data": [1, 2, 3],
  "new_data": [4, 5, 6],
  "timestamp": 1699999999.123
}
```

#### Cursor Broadcast
```json
{
  "type": "cursor",
  "user_id": "uuid-string",
  "editor": "overworld",
  "x": 100,
  "y": 200,
  "map_id": 0
}
```

#### Error
```json
{
  "type": "error",
  "message": "Room not found"
}
```

---

## Server Implementations

### Option 1: Node.js with ws

```javascript
// server.js
const WebSocket = require('ws');
const http = require('http');

const server = http.createServer();
const wss = new WebSocket.Server({ server });

// Room management
const rooms = new Map(); // room_code -> { users: Map, name: string }

wss.on('connection', (ws) => {
  let currentRoom = null;
  let currentUserId = null;

  ws.on('message', (data) => {
    try {
      const msg = JSON.parse(data);

      switch (msg.type) {
        case 'create':
          handleCreate(ws, msg);
          currentRoom = msg.room;
          currentUserId = msg.user_id;
          break;

        case 'join':
          handleJoin(ws, msg);
          currentRoom = msg.room;
          currentUserId = msg.user_id;
          break;

        case 'leave':
          handleLeave(ws, msg);
          currentRoom = null;
          currentUserId = null;
          break;

        case 'change':
          broadcastToRoom(msg.room, msg, ws);
          break;

        case 'cursor':
          broadcastToRoom(msg.room, msg, ws);
          break;
      }
    } catch (e) {
      console.error('Error processing message:', e);
      ws.send(JSON.stringify({ type: 'error', message: e.message }));
    }
  });

  ws.on('close', () => {
    if (currentRoom && currentUserId) {
      handleLeave(ws, { room: currentRoom, user_id: currentUserId });
    }
  });
});

function handleCreate(ws, msg) {
  const room = {
    name: msg.name,
    users: new Map()
  };
  room.users.set(msg.user_id, {
    ws,
    id: msg.user_id,
    name: msg.user,
    color: msg.color,
    is_active: true
  });
  rooms.set(msg.room, room);

  sendUserList(msg.room);
}

function handleJoin(ws, msg) {
  const room = rooms.get(msg.room);
  if (!room) {
    ws.send(JSON.stringify({ type: 'error', message: 'Room not found' }));
    return;
  }

  room.users.set(msg.user_id, {
    ws,
    id: msg.user_id,
    name: msg.user,
    color: msg.color,
    is_active: true
  });

  sendUserList(msg.room);
}

function handleLeave(ws, msg) {
  const room = rooms.get(msg.room);
  if (!room) return;

  room.users.delete(msg.user_id);

  if (room.users.size === 0) {
    rooms.delete(msg.room);
  } else {
    sendUserList(msg.room);
  }
}

function sendUserList(roomCode) {
  const room = rooms.get(roomCode);
  if (!room) return;

  const userList = Array.from(room.users.values()).map(u => ({
    id: u.id,
    name: u.name,
    color: u.color,
    is_active: u.is_active
  }));

  const msg = JSON.stringify({ type: 'user_list', users: userList });
  room.users.forEach(user => user.ws.send(msg));
}

function broadcastToRoom(roomCode, msg, excludeWs) {
  const room = rooms.get(roomCode);
  if (!room) return;

  const msgStr = JSON.stringify(msg);
  room.users.forEach(user => {
    if (user.ws !== excludeWs && user.ws.readyState === WebSocket.OPEN) {
      user.ws.send(msgStr);
    }
  });
}

const PORT = process.env.PORT || 8080;
server.listen(PORT, () => {
  console.log(`Collaboration server running on port ${PORT}`);
});
```

**package.json:**
```json
{
  "name": "yaze-collab-server",
  "version": "1.0.0",
  "main": "server.js",
  "scripts": {
    "start": "node server.js"
  },
  "dependencies": {
    "ws": "^8.14.0"
  }
}
```

### Option 2: Deno Deploy

```typescript
// main.ts
const rooms = new Map<string, Map<string, { socket: WebSocket; user: any }>>();

Deno.serve({ port: 8080 }, (req) => {
  if (req.headers.get("upgrade") !== "websocket") {
    return new Response("WebSocket endpoint", { status: 200 });
  }

  const { socket, response } = Deno.upgradeWebSocket(req);
  let currentRoom: string | null = null;
  let currentUserId: string | null = null;

  socket.onmessage = (e) => {
    try {
      const msg = JSON.parse(e.data);

      switch (msg.type) {
        case "create":
        case "join":
          currentRoom = msg.room;
          currentUserId = msg.user_id;

          if (!rooms.has(msg.room)) {
            rooms.set(msg.room, new Map());
          }
          rooms.get(msg.room)!.set(msg.user_id, {
            socket,
            user: { id: msg.user_id, name: msg.user, color: msg.color, is_active: true }
          });
          broadcastUserList(msg.room);
          break;

        case "leave":
          if (currentRoom) {
            rooms.get(currentRoom)?.delete(msg.user_id);
            broadcastUserList(currentRoom);
          }
          break;

        case "change":
        case "cursor":
          broadcast(msg.room, msg, socket);
          break;
      }
    } catch (e) {
      socket.send(JSON.stringify({ type: "error", message: String(e) }));
    }
  };

  socket.onclose = () => {
    if (currentRoom && currentUserId) {
      rooms.get(currentRoom)?.delete(currentUserId);
      broadcastUserList(currentRoom);
    }
  };

  return response;
});

function broadcastUserList(roomCode: string) {
  const room = rooms.get(roomCode);
  if (!room) return;

  const users = Array.from(room.values()).map(r => r.user);
  const msg = JSON.stringify({ type: "user_list", users });
  room.forEach(r => r.socket.send(msg));
}

function broadcast(roomCode: string, msg: any, exclude: WebSocket) {
  const room = rooms.get(roomCode);
  if (!room) return;

  const msgStr = JSON.stringify(msg);
  room.forEach(r => {
    if (r.socket !== exclude && r.socket.readyState === WebSocket.OPEN) {
      r.socket.send(msgStr);
    }
  });
}
```

Deploy to Deno Deploy:
```bash
deno deploy --project=yaze-collab main.ts
```

### Option 3: Cloudflare Workers with Durable Objects

```javascript
// worker.js
export class CollaborationRoom {
  constructor(state, env) {
    this.state = state;
    this.users = new Map();
  }

  async fetch(request) {
    const upgradeHeader = request.headers.get('Upgrade');
    if (upgradeHeader !== 'websocket') {
      return new Response('Expected WebSocket', { status: 426 });
    }

    const [client, server] = Object.values(new WebSocketPair());
    await this.handleSession(server);

    return new Response(null, { status: 101, webSocket: client });
  }

  async handleSession(ws) {
    ws.accept();
    let userId = null;

    ws.addEventListener('message', async (event) => {
      const msg = JSON.parse(event.data);

      if (msg.type === 'join' || msg.type === 'create') {
        userId = msg.user_id;
        this.users.set(userId, {
          ws,
          id: userId,
          name: msg.user,
          color: msg.color
        });
        this.broadcastUserList();
      } else if (msg.type === 'change' || msg.type === 'cursor') {
        this.broadcast(msg, ws);
      }
    });

    ws.addEventListener('close', () => {
      if (userId) {
        this.users.delete(userId);
        this.broadcastUserList();
      }
    });
  }

  broadcastUserList() {
    const users = Array.from(this.users.values()).map(u => ({
      id: u.id, name: u.name, color: u.color, is_active: true
    }));
    const msg = JSON.stringify({ type: 'user_list', users });
    this.users.forEach(u => u.ws.send(msg));
  }

  broadcast(msg, exclude) {
    const msgStr = JSON.stringify(msg);
    this.users.forEach(u => {
      if (u.ws !== exclude) u.ws.send(msgStr);
    });
  }
}

export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    const roomCode = url.pathname.slice(1) || 'default';

    const roomId = env.ROOMS.idFromName(roomCode);
    const room = env.ROOMS.get(roomId);

    return room.fetch(request);
  }
};
```

**wrangler.toml:**
```toml
name = "yaze-collab"
main = "worker.js"

[[durable_objects.bindings]]
name = "ROOMS"
class_name = "CollaborationRoom"

[[migrations]]
tag = "v1"
new_classes = ["CollaborationRoom"]
```

---

## Deployment Options

### Self-Hosted (VPS/Dedicated Server)

1. Install Node.js 18+
2. Clone/copy the server code
3. Run with PM2 for process management:
   ```bash
   npm install -g pm2
   pm2 start server.js --name yaze-collab
   pm2 save
   ```

4. Set up nginx reverse proxy with SSL:
   ```nginx
   server {
       listen 443 ssl;
       server_name collab.yourdomain.com;

       ssl_certificate /path/to/cert.pem;
       ssl_certificate_key /path/to/key.pem;

       location / {
           proxy_pass http://localhost:8080;
           proxy_http_version 1.1;
           proxy_set_header Upgrade $http_upgrade;
           proxy_set_header Connection "upgrade";
           proxy_set_header Host $host;
           proxy_read_timeout 86400;
       }
   }
   ```

### Platform-as-a-Service

| Platform | Pros | Cons |
|----------|------|------|
| **Railway** | Easy deploy, free tier | Limited free hours |
| **Render** | Free tier, auto-deploy | Spins down on inactivity |
| **Fly.io** | Global edge, generous free | More complex setup |
| **Deno Deploy** | Free, edge deployment | Deno runtime only |
| **Cloudflare Workers** | Free tier, global edge | Durable Objects cost |

---

## Client Configuration

### Method 1: JavaScript Configuration (Recommended)

Add before loading yaze:
```html
<script>
  window.YAZE_CONFIG = {
    collaborationServerUrl: 'wss://collab.yourdomain.com'
  };
</script>
```

### Method 2: Meta Tag

```html
<meta name="yaze-collab-server" content="wss://collab.yourdomain.com">
```

### Method 3: Runtime Configuration

In your integration code:
```cpp
auto& collab = WasmCollaboration::GetInstance();
collab.SetWebSocketUrl("wss://collab.yourdomain.com");
```

---

## Security Considerations

### Authentication (Optional Enhancement)

Add token-based authentication:

```javascript
// Server-side
ws.on('message', (data) => {
  const msg = JSON.parse(data);

  if (msg.type === 'auth') {
    // Verify token with your auth service
    const valid = await verifyToken(msg.token);
    if (!valid) {
      ws.close(4001, 'Unauthorized');
      return;
    }
    ws.authenticated = true;
  }

  if (!ws.authenticated) {
    ws.send(JSON.stringify({ type: 'error', message: 'Not authenticated' }));
    return;
  }

  // ... handle other messages
});
```

### Rate Limiting

```javascript
const rateLimit = new Map(); // user_id -> { count, resetTime }

function checkRateLimit(userId) {
  const now = Date.now();
  const limit = rateLimit.get(userId) || { count: 0, resetTime: now + 1000 };

  if (now > limit.resetTime) {
    limit.count = 0;
    limit.resetTime = now + 1000;
  }

  limit.count++;
  rateLimit.set(userId, limit);

  return limit.count <= 100; // 100 messages per second
}
```

### Room Expiration

```javascript
// Clean up inactive rooms
setInterval(() => {
  const now = Date.now();
  rooms.forEach((room, code) => {
    if (room.users.size === 0 && now - room.lastActivity > 3600000) {
      rooms.delete(code);
    }
  });
}, 60000);
```

---

## Troubleshooting

### Connection Issues

1. **CORS errors**: Ensure your server accepts connections from your domain
2. **SSL errors**: WebSocket over HTTPS requires valid SSL certificate
3. **Timeouts**: Implement ping/pong heartbeat:
   ```javascript
   setInterval(() => {
     wss.clients.forEach(ws => {
       if (!ws.isAlive) return ws.terminate();
       ws.isAlive = false;
       ws.ping();
     });
   }, 30000);

   ws.on('pong', () => { ws.isAlive = true; });
   ```

### Performance

- Use message compression for large change payloads
- Implement message batching for cursor updates
- Consider Redis pub/sub for multi-instance deployments

---

## Example: Complete Docker Deployment

**Dockerfile:**
```dockerfile
FROM node:18-alpine
WORKDIR /app
COPY package*.json ./
RUN npm ci --only=production
COPY server.js .
EXPOSE 8080
CMD ["node", "server.js"]
```

**docker-compose.yml:**
```yaml
version: '3.8'
services:
  collab:
    build: .
    ports:
      - "8080:8080"
    restart: unless-stopped
    environment:
      - NODE_ENV=production
```

Deploy:
```bash
docker-compose up -d
```

---

## Testing Your Server

Use wscat to test:
```bash
npm install -g wscat
wscat -c wss://collab.yourdomain.com

# Send create message
{"type":"create","room":"TEST01","name":"Test","user":"TestUser","user_id":"test-123","color":"#FF0000"}
```

Or use the browser console on your yaze deployment:
```javascript
window.YAZE_CONFIG = { collaborationServerUrl: 'wss://collab.yourdomain.com' };
// Then use the collaboration UI in yaze
```
