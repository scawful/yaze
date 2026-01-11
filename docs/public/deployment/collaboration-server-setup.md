# Collaboration Server Setup Guide

This guide explains how to set up a WebSocket server for yaze's real-time collaboration feature, enabling multiple users to edit ROMs together.

## Quick Start with yaze-server

The official collaboration server is **[yaze-server](https://github.com/scawful/yaze-server)**, a Node.js WebSocket server with:
- Real-time session management
- AI agent integration (Gemini/Genkit or external agent endpoints)
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
- **halext-nj**: `ws://org.halext.org:8765` (pm2 process `yaze-collab`, no TLS on 8765 today; front with nginx/Caddy for `wss://` if desired)
- **Self-hosted**: Deploy to Railway, Render, Fly.io, or your own VPS

### Current halext deployment (ssh halext-nj)
- Process: pm2 `yaze-collab`
- Port: `8765` (plain WS/HTTP; add TLS proxy for WSS)
- Health: `http://org.halext.org:8765/health`, metrics at `/metrics`
- AI: enable with `GEMINI_API_KEY` or `AI_AGENT_ENDPOINT` + `ENABLE_AI_AGENT=true`

### Server v2.1 Features
- **Persistence**: Configurable SQLite storage (`SQLITE_DB_PATH` env var)
- **Admin API**: Protected endpoints for session/room management
- **Enhanced Health**: AI status, TLS detection, persistence info in `/health`
- **Configurable Limits**: Tunable rate limits via environment variables

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
- `ai_query`: `username`, `query` (requires `ENABLE_AI_AGENT` plus `GEMINI_API_KEY` or `AI_AGENT_ENDPOINT` for external providers)
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
3. Configure environment variables:

   **Core Settings:**
   ```bash
   PORT=8765                          # WebSocket/HTTP port (default: 8765)
   ENABLE_AI_AGENT=true               # Enable AI query handling (default: true)
   GEMINI_API_KEY=your_api_key        # Gemini API key for AI responses
   AI_AGENT_ENDPOINT=http://...       # Alternative: external AI endpoint (OpenAI/Anthropic/etc.)
   ```

   **Persistence (v2.1+):**
   ```bash
   SQLITE_DB_PATH=/var/lib/yaze-collab.db  # File-based persistence (default: :memory:)
   ```

   **Rate Limiting:**
   ```bash
   RATE_LIMIT_MAX_MESSAGES=100        # Messages per minute per IP (default: 100)
   JOIN_LIMIT_MAX_ATTEMPTS=10         # Join/host attempts per minute per IP (default: 10)
   ```

   **Admin API:**
   ```bash
   ADMIN_API_KEY=your_secret_key      # Protect admin endpoints (optional)
   ```

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
- **Admin actions:** Use Admin API (see below) or block abusive IPs at the proxy.
- **Scaling path:** add Redis pub/sub for multi-instance broadcast; place proxy in front with sticky room affinity if you shard.

---

## Admin API (v2.1+)

Protected endpoints for server administration. Set `ADMIN_API_KEY` to require authentication.

### Authentication
Include the key in requests:
```bash
curl -H "X-Admin-Key: your_secret_key" http://localhost:8765/admin/sessions
# Or as query param: http://localhost:8765/admin/sessions?admin_key=your_secret_key
```

### Endpoints

**List all sessions/rooms:**
```bash
GET /admin/sessions
# Response: { sessions: [...], wasm_rooms: [...], total_connections: N }
```

**List users in a session:**
```bash
GET /admin/sessions/:code/users
# Response: { code: "ABC123", type: "full"|"wasm", users: [...] }
```

**Close a session (kick all users):**
```bash
DELETE /admin/sessions/:code
# Body: { "reason": "Maintenance" }  (optional)
# Response: { success: true, code: "ABC123", reason: "..." }
```

**Kick a specific user:**
```bash
DELETE /admin/sessions/:code/users/:userId
# Body: { "reason": "Violation of rules" }  (optional)
# Response: { success: true, code: "ABC123", userId: "user-123", reason: "..." }
```

**Broadcast message to session:**
```bash
POST /admin/sessions/:code/broadcast
# Body: { "message": "Server maintenance in 5 minutes", "message_type": "admin" }
# Response: { success: true, code: "ABC123", recipients: N }
```

---

## Halext TLS Deployment Guide

Step-by-step guide to add WSS (TLS) to the halext deployment.

### Prerequisites
- SSH access to halext-nj
- Domain DNS pointing to server (e.g., `collab.halext.org` or use existing `org.halext.org`)
- Certbot or existing SSL certificates

### Option A: nginx reverse proxy

1. **Install nginx (if not present):**
   ```bash
   sudo apt update && sudo apt install nginx certbot python3-certbot-nginx
   ```

2. **Create nginx config:**
   ```bash
   sudo nano /etc/nginx/sites-available/yaze-collab
   ```
   ```nginx
   server {
       listen 80;
       server_name collab.halext.org;  # or org.halext.org

       # Redirect HTTP to HTTPS
       return 301 https://$server_name$request_uri;
   }

   server {
       listen 443 ssl http2;
       server_name collab.halext.org;

       ssl_certificate /etc/letsencrypt/live/collab.halext.org/fullchain.pem;
       ssl_certificate_key /etc/letsencrypt/live/collab.halext.org/privkey.pem;
       ssl_protocols TLSv1.2 TLSv1.3;
       ssl_ciphers HIGH:!aNULL:!MD5;

       # WebSocket proxy
       location / {
           proxy_pass http://127.0.0.1:8765;
           proxy_http_version 1.1;
           proxy_set_header Upgrade $http_upgrade;
           proxy_set_header Connection "upgrade";
           proxy_set_header Host $host;
           proxy_set_header X-Real-IP $remote_addr;
           proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
           proxy_set_header X-Forwarded-Proto $scheme;
           proxy_read_timeout 86400;
           proxy_send_timeout 86400;
       }
   }
   ```

3. **Enable site and get certificate:**
   ```bash
   sudo ln -s /etc/nginx/sites-available/yaze-collab /etc/nginx/sites-enabled/
   sudo certbot --nginx -d collab.halext.org
   sudo nginx -t && sudo systemctl reload nginx
   ```

4. **Update client config to use WSS:**
   ```javascript
   window.YAZE_CONFIG = {
     collaboration: { serverUrl: 'wss://collab.halext.org' }
   };
   ```

### Option B: Caddy (simpler, auto-TLS)

1. **Install Caddy:**
   ```bash
   sudo apt install -y debian-keyring debian-archive-keyring apt-transport-https
   curl -1sLf 'https://dl.cloudsmith.io/public/caddy/stable/gpg.key' | sudo gpg --dearmor -o /usr/share/keyrings/caddy-stable-archive-keyring.gpg
   curl -1sLf 'https://dl.cloudsmith.io/public/caddy/stable/debian.deb.txt' | sudo tee /etc/apt/sources.list.d/caddy-stable.list
   sudo apt update && sudo apt install caddy
   ```

2. **Create Caddyfile:**
   ```bash
   sudo nano /etc/caddy/Caddyfile
   ```
   ```caddy
   collab.halext.org {
       reverse_proxy localhost:8765
       # Caddy handles TLS automatically
   }
   ```

3. **Reload Caddy:**
   ```bash
   sudo systemctl reload caddy
   ```

### Verify TLS is working

```bash
# Check health endpoint shows TLS detected
curl -s https://collab.halext.org/health | jq '.tls'
# Expected: { "detected": true, "note": "Request via TLS proxy" }

# Test WebSocket connection
wscat -c wss://collab.halext.org
```

---

## PM2 Ecosystem File

For more control, use a PM2 ecosystem file:

**ecosystem.config.js:**
```javascript
module.exports = {
  apps: [{
    name: 'yaze-collab',
    script: 'server.js',
    instances: 1,
    autorestart: true,
    watch: false,
    max_memory_restart: '500M',
    env: {
      NODE_ENV: 'development',
      PORT: 8765
    },
    env_production: {
      NODE_ENV: 'production',
      PORT: 8765,
      SQLITE_DB_PATH: '/var/lib/yaze-collab/sessions.db',
      ENABLE_AI_AGENT: 'true',
      // GEMINI_API_KEY: 'your_key_here',
      // ADMIN_API_KEY: 'your_admin_key'
    }
  }]
};
```

**Usage:**
```bash
pm2 start ecosystem.config.js --env production
pm2 save
pm2 startup  # Enable startup on boot
```

---

## Persistence & Backup

### Enable file-based persistence
```bash
# Create data directory
sudo mkdir -p /var/lib/yaze-collab
sudo chown $(whoami) /var/lib/yaze-collab

# Set environment variable
export SQLITE_DB_PATH=/var/lib/yaze-collab/sessions.db
pm2 restart yaze-collab --update-env
```

### Backup strategy
```bash
# Daily backup cron job
echo "0 3 * * * sqlite3 /var/lib/yaze-collab/sessions.db '.backup /backups/yaze-collab-$(date +%Y%m%d).db'" | crontab -

# Retain last 7 days
echo "0 4 * * * find /backups -name 'yaze-collab-*.db' -mtime +7 -delete" | crontab -e
```

### Health endpoint (v2.1+)
The `/health` endpoint now reports persistence status:
```json
{
  "status": "healthy",
  "version": "2.1",
  "persistence": {
    "type": "file",
    "path": "/var/lib/yaze-collab/sessions.db"
  },
  "ai": {
    "enabled": true,
    "configured": true,
    "provider": "gemini"
  },
  "tls": {
    "detected": true,
    "note": "Request via TLS proxy"
  }
}
```

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
