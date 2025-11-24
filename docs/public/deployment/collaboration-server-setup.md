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
npm ci
npm start
# Server runs on ws://localhost:8765 (default port 8765)
```

### Production Deployment
For production, deploy yaze-server behind an SSL proxy when possible:
- **halext-server**: `ws://org.halext.org:8765` (pm2 process `yaze-collab`, no TLS on 8765 today; front with nginx/Caddy for `wss://` if desired)
- **Self-hosted**: Deploy to Railway, Render, Fly.io, or your own VPS

### Current halext deployment (ssh halext-server)
- Process: pm2 `yaze-collab`
- Port: `8765` (plain WS/HTTP; add TLS proxy for WSS)
- Health: `http://org.halext.org:8765/health`, metrics at `/metrics`
- AI: enable with `GEMINI_API_KEY` or `AI_AGENT_ENDPOINT` + `ENABLE_AI_AGENT=true`

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

The halext deployment (`yaze-collab` pm2 on port 8765) runs the official **yaze-server v2.0** and speaks two compatible protocols:
- **WASM compatibility protocol** – used by the current web/WASM build (flat `type` JSON messages, no `payload` wrapper).
- **Session/AI protocol** – used by advanced clients (desktop/editor + AI/ROM sync/proposals) with `{ type, payload }` envelopes.

### WASM compatibility protocol (web build)
All messages are JSON with a `type` field and flat attributes.

**Client → Server**
- `create`: `room`, `name?`, `user`, `user_id`, `color?`, `password?`
- `join`: `room`, `user`, `user_id`, `color?`, `password?`
- `leave`: `room`, `user_id`
- `change`: `room`, `user_id`, `offset`, `old_data`, `new_data`, `timestamp?`
- `cursor`: `room`, `user_id`, `editor`, `x`, `y`, `map_id`
- `ping`: optional keep-alive (`{ "type": "ping" }`)

**Server → Client**
- `create_response`: `{ "type": "create_response", "success": true, "session_name": "..." }`
- `join_response`: `{ "type": "join_response", "success": true, "session_name": "..." }`
- `users`: `{ "type": "users", "list": [{ "id": "...", "name": "...", "color": "#4ECDC4", "active": true }] }`
- `change`: Echoed to room with `timestamp` added
- `cursor`: Broadcast presence updates
- `error`: `{ "type": "error", "message": "...", "payload": { "error": "..." } }`
- `pong`: `{ "type": "pong", "payload": { "timestamp": 1700000000000 } }`

**Notes**
- Passwords are supported (`password` hashed server-side); rooms are deleted when empty.
- Rate limits: 100 messages/min/IP; 10 join/host attempts/min/IP.
- Size limits: ROM diffs ≤ 5 MB, snapshots ≤ 10 MB. Heartbeat every 30s terminates dead sockets.

### Session/AI protocol (advanced clients)
Messages use `{ "type": "...", "payload": { ... } }`.

**Key client messages**
- `host_session`: `session_name`, `username`, `rom_hash?`, `ai_enabled? (default true)`, `session_password?`
- `join_session`: `session_code`, `username`, `session_password?`
- `chat_message`: `sender`, `message`, `message_type?`, `metadata?`
- `rom_sync`: `sender`, `diff_data` (base64), `rom_hash`
- `snapshot_share`: `sender`, `snapshot_data` (base64), `snapshot_type`
- `proposal_share` / `proposal_vote` / `proposal_update`
- `ai_query`: `username`, `query` (requires `ENABLE_AI_AGENT` plus `GEMINI_API_KEY` or `AI_AGENT_ENDPOINT`)
- `leave_session`, `ping`

**Key server broadcasts**
- `session_hosted`, `session_joined`, `participant_joined`, `participant_left`
- `chat_message`, `rom_sync`, `snapshot_shared`
- `proposal_shared`, `proposal_vote_received`, `proposal_updated`
- `ai_response` (only when AI is enabled and configured)
- `pong`, `error`, `server_shutdown`

See `yaze-server/README.md` for full payload examples.

---

## Deployment Options

### Self-Hosted (VPS/Dedicated Server)

1. Install Node.js 18+
2. Clone/copy the server code and install deps:
   ```bash
   npm ci
   ```
3. Configure environment (examples):
   - `PORT=8765` (default matches halext deployment)
   - `ENABLE_AI_AGENT=true|false`
   - `GEMINI_API_KEY` **or** `AI_AGENT_ENDPOINT` for AI responses
4. Run with PM2 for process management:
   ```bash
   npm install -g pm2
   pm2 start server.js --name yaze-collab --env production
   pm2 save
   ```

5. Add TLS reverse proxy (recommended)

**nginx example (translate `wss://` to local `ws://localhost:8765`):**
   ```nginx
   server {
       listen 443 ssl;
       server_name collab.yourdomain.com;

       ssl_certificate /path/to/cert.pem;
       ssl_certificate_key /path/to/key.pem;

       location /ws {
           proxy_pass http://localhost:8765;
           proxy_http_version 1.1;
           proxy_set_header Upgrade $http_upgrade;
           proxy_set_header Connection "upgrade";
           proxy_set_header Host $host;
           proxy_read_timeout 86400;
           proxy_send_timeout 86400;
       }
   }
   ```
**Caddy example:**
```caddy
collab.yourdomain.com {
  reverse_proxy /ws localhost:8765 {
    header_up Upgrade {>Upgrade}
    header_up Connection {>Connection}
  }
  tls you@example.com
}
```
**Why:** avoids mixed-content errors in browsers, encrypts ROM diffs/chat/passwords, and centralizes cert/ALPN handling.

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
    collaboration: {
      serverUrl: 'ws://org.halext.org:8765' // use wss:// if you front with TLS
    }
  };
</script>
```

### Method 2: Meta Tag

```html
<meta name="yaze-collab-server" content="ws://org.halext.org:8765">
```

### Method 3: Runtime Configuration

In your integration code:
```cpp
auto& collab = WasmCollaboration::GetInstance();
collab.SetWebSocketUrl("ws://org.halext.org:8765");
```

---

## Security Considerations

- Transport: terminate TLS in front of the server (`wss://`). The halext deployment currently runs plain `ws://org.halext.org:8765`; add nginx/Caddy to secure it.
- Built-in guardrails: 100 messages/min/IP, 10 join/host attempts/min/IP, 5 MB ROM diff limit, 10 MB snapshot limit, 30s heartbeat that drops dead sockets.
- Passwords: supported on both protocols (`password` for WASM, `session_password` for full sessions). Hashing is SHA-256 on the server side.
- AI: only enabled when `ENABLE_AI_AGENT=true` **and** `GEMINI_API_KEY` or `AI_AGENT_ENDPOINT` is set. Leave unset to disable AI endpoints.
- Persistence: halext uses in-memory SQLite (sessions reset on restart). For durability, run sqlite on disk (`SQLITE_DB_PATH=/var/lib/yaze-collab.db`) or swap to Postgres/MySQL with a lightweight adapter. Add backups/retention for audit.
- Authentication: front the service with an auth gateway if you need verified identities; yaze-server does not issue tokens itself.

---

## Troubleshooting

- **Handshake issues:** Match the scheme to the deployment. halext runs `ws://org.halext.org:8765`; use `wss://` only when you have a TLS proxy forwarding `Upgrade` headers.
- **Health checks:** `curl http://org.halext.org:8765/health` and `/metrics` to confirm the service is live.
- **TLS errors:** If you front with nginx/Caddy, ensure HTTP/1.1, `Upgrade`/`Connection` headers, and a valid certificate. Remove `wss://` if you have not enabled TLS.
- **Disconnects/rate limits:** Server sends heartbeats every 30s and enforces limits. Check `pm2 logs yaze-collab` on halext for details.
- **Performance:** Keep diffs under 5 MB, snapshots under 10 MB, and batch cursor updates on the client. Enable compression at the proxy if needed.

---

## Operations Playbook (halext-friendly)

- **Status:** `curl http://org.halext.org:8765/health` and `/metrics`; add `/metrics` scrape to Prometheus if available.
- **Logs:** `pm2 logs yaze-collab` (rotate externally if needed).
- **Restart/Redeploy:** `pm2 restart yaze-collab`; `pm2 list` to verify uptime.
- **Admin actions:** temporarily block abusive IPs at the proxy (nginx `deny`, Caddy `ip` matcher); rate limits already exist server-side.
- **Scaling path:** add Redis pub/sub for multi-instance broadcast; place proxy in front with sticky room affinity if you shard.

## Client UX hints

- Surface server status in the web UI by calling `/health` once on load and showing: server reachable, AI enabled/disabled (from health/metrics), and whether TLS is in use.
- Default `window.YAZE_CONFIG.collaboration.serverUrl` to `wss://collab.yourdomain.com/ws` when a TLS proxy is present; fall back to `ws://localhost:8765` for local dev.
- Show a small banner when AI is disabled or when the connection is downgraded to plain WS to set user expectations.

---

## Example: Complete Docker Deployment

**Dockerfile:**
```dockerfile
FROM node:18-alpine
WORKDIR /app
COPY package*.json ./
RUN npm ci --only=production
COPY server.js .
EXPOSE 8765
CMD ["node", "server.js"]
```

**docker-compose.yml:**
```yaml
version: '3.8'
services:
  collab:
    build: .
    ports:
      - "8765:8765"
    restart: unless-stopped
    environment:
      - NODE_ENV=production
      - ENABLE_AI_AGENT=true
      # Uncomment one of the following if AI responses are desired
      # - GEMINI_API_KEY=your_api_key
      # - AI_AGENT_ENDPOINT=http://ai-service:5000
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
wscat -c ws://org.halext.org:8765

# Send create message
{"type":"create","room":"TEST01","name":"Test","user":"TestUser","user_id":"test-123","color":"#FF0000"}

# Check response
# < {"type":"create_response","success":true,"session_name":"Test"}

# Test full session protocol (AI disabled)
{"type":"host_session","payload":{"session_name":"DocsCheck","username":"tester","ai_enabled":false}}
```

Health check:
```bash
curl http://org.halext.org:8765/health
curl http://org.halext.org:8765/metrics
```

Or use the browser console on your yaze deployment:
```javascript
window.YAZE_CONFIG = {
  collaboration: { serverUrl: 'ws://org.halext.org:8765' }
};
// Then use the collaboration UI in yaze
```
