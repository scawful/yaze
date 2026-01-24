# YAZE HTTP REST API

The YAZE HTTP REST API provides external access to YAZE functionality for automation, testing, and integration with external tools.

## Getting Started

### Building with HTTP API Support

The HTTP API is enabled automatically when building with AI-enabled presets:

```bash
# macOS
cmake --preset mac-ai
cmake --build --preset mac-ai --target z3ed

# Linux
cmake --preset lin-ai
cmake --build --preset lin-ai --target z3ed

# Windows
cmake --preset win-ai
cmake --build --preset win-ai --target z3ed
```

Or enable it explicitly with the CMake flag:

```bash
cmake -B build -DYAZE_ENABLE_HTTP_API=ON
cmake --build build --target z3ed
```

### Running the HTTP Server

Start z3ed with the `--http-port` flag:

```bash
# Start on default port 8080
./build/bin/z3ed --http-port=8080

# Start on custom port with display host for printed URLs
./build/bin/z3ed --http-port=9000 --http-host=example-host

# Combine with other z3ed commands
./build/bin/z3ed agent --rom=zelda3.sfc --http-port=8080
```

**Security Note**: The server currently binds `0.0.0.0` (all interfaces). Use firewall rules or SSH tunnels to keep it private. The `--http-host` flag only controls the URLs printed by `z3ed`.

## API Endpoints

All endpoints return JSON responses and include CORS headers. Send an `OPTIONS` request for a `204` preflight response with `Access-Control-Allow-*` headers. `/api/v1/symbols` returns plain text by default.

### GET /api/v1/health

Health check endpoint for monitoring server status.

**Request:**
```bash
curl http://localhost:8080/api/v1/health
```

**Response:**
```json
{
  "status": "ok",
  "version": "1.0",
  "service": "yaze-agent-api"
}
```

**Status Codes:**
- `200 OK` - Server is healthy

---

### GET /api/v1/models

List all available AI models from all registered providers (Ollama, Gemini, etc.).
Use `?refresh=1` (or `?refresh=true` / `?refresh`) to bypass the server-side model cache.

**Request:**
```bash
curl http://localhost:8080/api/v1/models
```

**Request (bypass cache):**
```bash
curl http://localhost:8080/api/v1/models?refresh=1
```

**Response:**
```json
{
  "models": [
    {
      "name": "qwen2.5-coder:7b",
      "display_name": "Qwen 2.5 Coder 7B",
      "provider": "ollama",
      "description": "Qwen 2.5 Coder 7B model",
      "family": "qwen2.5",
      "parameter_size": "7B",
      "quantization": "Q4_0",
      "size_bytes": 4661211616,
      "is_local": true
    },
    {
      "name": "gemini-1.5-pro",
      "display_name": "Gemini 1.5 Pro",
      "provider": "gemini",
      "description": "Gemini 1.5 Pro",
      "family": "gemini-1.5",
      "parameter_size": "",
      "quantization": "",
      "size_bytes": 0,
      "is_local": false
    }
  ],
  "count": 2
}
```

**Status Codes:**
- `200 OK` - Models retrieved successfully
- `500 Internal Server Error` - Failed to retrieve models (see `error` field)

**Response Fields:**
- `name` (string) - Model identifier
- `display_name` (string) - Human-friendly display name (falls back to `name`)
- `provider` (string) - Provider name ("ollama", "gemini", etc.)
- `description` (string) - Human-readable description
- `family` (string) - Model family/series
- `parameter_size` (string) - Model size (e.g., "7B", "13B")
- `quantization` (string) - Quantization method (e.g., "Q4_0", "Q8_0")
- `size_bytes` (number) - Model size in bytes
- `is_local` (boolean) - Whether model is hosted locally

---

### GET /api/v1/symbols

Export symbols from the active symbol provider. By default returns plain text; request JSON via `Accept: application/json`.
Use `format=mesen|asar|wla|bsnes` to choose the export format (unsupported values return `400`).

**Request (plain text):**
```bash
curl "http://localhost:8080/api/v1/symbols?format=mesen"
```

**Request (JSON):**
```bash
curl -H "Accept: application/json" \
  "http://localhost:8080/api/v1/symbols?format=asar"
```

**Response (plain text):**
```
; sample output truncated
some_symbol $008000
```

**Response (JSON):**
```json
{
  "symbols": "...",
  "format": "asar"
}
```

**Status Codes:**
- `200 OK` - Symbols retrieved successfully
- `400 Bad Request` - Unsupported `format` value
- `503 Service Unavailable` - Symbol provider not available
- `500 Internal Server Error` - Export failed

---

### POST /api/v1/navigate

Send a navigation request (for example, jump to a disassembly address).

**Request:**
```bash
curl -X POST http://localhost:8080/api/v1/navigate \
  -H "Content-Type: application/json" \
  -d '{"address":4660,"source":"tool"}'
```

**Response:**
```json
{
  "status": "ok",
  "address": 4660,
  "message": "Navigation request received"
}
```

**Status Codes:**
- `200 OK` - Navigation request accepted
- `400 Bad Request` - Invalid JSON payload

---

### POST /api/v1/breakpoint/hit

Notify YAZE that a breakpoint fired (optionally include CPU state).

**Request:**
```bash
curl -X POST http://localhost:8080/api/v1/breakpoint/hit \
  -H "Content-Type: application/json" \
  -d '{"address":4660,"source":"mesen","cpu_state":{"a":1}}'
```

**Response:**
```json
{ "status": "ok", "address": 4660 }
```

**Status Codes:**
- `200 OK` - Breakpoint report accepted
- `400 Bad Request` - Invalid JSON payload

---

### POST /api/v1/state/update

Send a free-form JSON payload to update panel state (schema evolving).

**Request:**
```bash
curl -X POST http://localhost:8080/api/v1/state/update \
  -H "Content-Type: application/json" \
  -d '{"source":"tool","state":{"status":"ok"}}'
```

**Response:**
```json
{ "status": "ok" }
```

**Status Codes:**
- `200 OK` - State update accepted
- `400 Bad Request` - Invalid JSON payload

---

### POST /api/v1/window/show

Show the desktop window (if supported).

**Status Codes:**
- `200 OK` - Window shown
- `501 Not Implemented` - Window control unavailable
- `500 Internal Server Error` - Window action failed

---

### POST /api/v1/window/hide

Hide the desktop window (if supported).

**Status Codes:**
- `200 OK` - Window hidden
- `501 Not Implemented` - Window control unavailable
- `500 Internal Server Error` - Window action failed

---

## Error Handling

All endpoints return standard HTTP status codes and JSON error responses:

```json
{
  "error": "Detailed error message"
}
```

Common status codes:
- `200 OK` - Request succeeded
- `400 Bad Request` - Invalid request parameters
- `404 Not Found` - Endpoint not found
- `500 Internal Server Error` - Server-side error

## CORS Support

All endpoints include CORS headers to allow cross-origin requests:
```
Access-Control-Allow-Origin: *
```

## Implementation Details

### Architecture

The HTTP API is built using:
- **cpp-httplib** - Header-only HTTP server library (fetched via CPM)
- **nlohmann/json** - JSON serialization/deserialization
- **ModelRegistry** - Unified model management across providers

### Code Structure

```
src/cli/service/api/
├── http_server.h      # HttpServer class declaration
├── http_server.cc     # Server implementation and routing
├── api_handlers.h     # Endpoint handler declarations
├── api_handlers.cc    # Endpoint implementations
└── README.md          # This file
```

### Threading Model

The HTTP server runs in a background thread, allowing z3ed to continue normal operation. The server gracefully shuts down when z3ed exits.

### CMake Integration

Enable with:
```cmake
option(YAZE_ENABLE_HTTP_API "Enable HTTP REST API server" ${YAZE_ENABLE_AGENT_CLI})
```

When enabled, adds compile definition:
```cpp
#ifdef YAZE_HTTP_API_ENABLED
// HTTP API code
#endif
```

## Testing

### Manual Testing

1. Start the server:
```bash
./build/bin/z3ed --http-port=8080
```

2. Test health endpoint:
```bash
curl http://localhost:8080/api/v1/health | jq
```

3. Test models endpoint:
```bash
curl http://localhost:8080/api/v1/models | jq
```

### Automated Testing

Use the provided test script:
```bash
scripts/agents/test-http-api.sh 8080
```

### CI/CD Integration

The HTTP API can be tested in CI via workflow_dispatch:
```bash
gh workflow run ci.yml -f enable_http_api_tests=true
```

See `docs/internal/agents/archive/utility-tools/gh-actions-remote.md` for details.

## Future Endpoints (Planned)

The following endpoints are planned for future releases:

- `POST /api/v1/chat` - Send prompts to AI agent
- `POST /api/v1/tool/{tool_name}` - Execute specific tools
- `GET /api/v1/rom/status` - ROM loading status
- `GET /api/v1/rom/info` - ROM metadata

See `docs/internal/AI_API_ENHANCEMENT_HANDOFF.md` for the full roadmap.

## Security Considerations

- **Bind Address**: The server binds `0.0.0.0` (all interfaces) by default; use firewall rules or SSH tunnels to keep it private.
- **No Authentication**: Currently no authentication mechanism (planned for future).
- **CORS Enabled**: Cross-origin requests allowed (may be restricted in future).
- **Rate Limiting**: Not implemented (planned for future).

For production use, consider:
1. Running behind a reverse proxy (nginx, Apache)
2. Adding authentication middleware
3. Implementing rate limiting
4. Restricting CORS origins

## Troubleshooting

### "Port already in use"

If you see `Failed to listen on port`, another process is using that port:

```bash
# Find the process
lsof -i :8080

# Use a different port
./build/bin/z3ed --http-port=9000
```

### "Failed to start HTTP API server"

Check that:
1. The binary was built with `YAZE_ENABLE_HTTP_API=ON`
2. The port number is valid (1-65535)
3. You have permission to bind to the port (<1024 requires root)

### Server not responding

Verify the server is running and reachable:
```bash
# Check if server is listening
netstat -an | grep 8080

# Test with verbose curl
curl -v http://localhost:8080/api/v1/health
```

## Contributing

When adding new endpoints:

1. Declare handler in `api_handlers.h`
2. Implement handler in `api_handlers.cc`
3. Register route in `http_server.cc::RegisterRoutes()`
4. Document endpoint in this README
5. Add tests in `scripts/agents/test-http-api.sh`

Follow the existing handler pattern:
```cpp
void HandleYourEndpoint(const httplib::Request& req, httplib::Response& res) {
  // Set CORS header
  res.set_header("Access-Control-Allow-Origin", "*");

  // Build JSON response
  json response;
  response["field"] = "value";

  // Return JSON
  res.set_content(response.dump(), "application/json");
}
```

## License

Part of the YAZE project. See LICENSE for details.
