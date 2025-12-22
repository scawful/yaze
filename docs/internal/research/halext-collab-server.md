## Halext collaboration server hookup (yaze.halext.org)

Goal: keep the WASM bundle on GitHub Pages but let the page at `https://yaze.halext.org` talk to the collab server running on the box. The client now auto-points to `wss://<host>/ws` for any `*.halext.org` host when no server URL is configured.

### What changed in code
- `src/web/core/config.js` now falls back to `wss://<current-host>/ws` when the page is served from a `halext.org` domain and no explicit `window.YAZE_CONFIG.collaboration.serverUrl` or meta tag is set. This keeps GH Pages (front-end) and the collab server (backend) on the same origin.

### Nginx work (needs sudo)
Edit `/etc/nginx/sites/yaze.halext.org.conf` so GitHub Pages stays the default, but WebSocket/HTTP traffic under `/ws` proxies to the collab server on `127.0.0.1:8765`.

Add inside the `server { ... }` block that listens on 443:

```
    # Collab server (WS + HTTP)
    location /ws/ {
        proxy_pass http://127.0.0.1:8765/;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
        proxy_read_timeout 7d;
        proxy_send_timeout 7d;
        proxy_buffering off;
    }
```

Keep the existing `/` proxy to GitHub Pages so the WASM UI still serves from GH.

Then reload nginx (as root):

```
sudo nginx -t && sudo systemctl reload nginx
```

### Collab server config check (no sudo required)
- Service: user systemd unit `yaze-server.service` already runs `/home/halext/yaze-server/server.js` on port 8765.
- Allowed origins in `/home/halext/.config/yaze-server.env` include `https://yaze.halext.org`; leave secrets untouched.
- Health: `curl -H "Origin: https://yaze.halext.org" http://127.0.0.1:8765/health` (from the server) should return status 200.

### Usage once nginx is updated
- Load `https://yaze.halext.org` (served from GH Pages). The collab client should auto-connect to `wss://yaze.halext.org/ws`.
- Hosting/joining creates distinct room codes; multiple rooms can coexist. Admin API remains on the same origin under `/ws/admin/...` with `x-admin-key` header.

No secrets were changed; only client fallback logic was added locally.
