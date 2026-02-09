# YAZE API Reference (HTTP + gRPC)

This page documents YAZE automation APIs for tools and integrations. It covers the HTTP Agent API (REST) and the gRPC automation services.

## HTTP Agent API (REST)

The HTTP API is exposed by both `z3ed` (CLI) and the desktop app when enabled.

### Enable and Run

Build with HTTP API support:

```bash
cmake --preset mac-ai -DYAZE_ENABLE_HTTP_API=ON   # or lin-ai / win-ai
cmake --build build_ai --target z3ed yaze
```

Start the server:

```bash
# z3ed
./scripts/z3ed --http-port=8080

# yaze desktop app
./scripts/yaze --enable_api --api_port=8080
```

Notes:
- The HTTP server listens on all interfaces; use firewalls or SSH tunnels if you need it private.
- `--http-host` controls the host shown in z3ed output (useful for clients).
- All endpoints return JSON and include CORS headers (except `/api/v1/symbols`, which returns plain text by default). Use `OPTIONS` for a `204` preflight response with `Access-Control-Allow-*` headers.

### Smoke Testing

Use the helper script to validate core endpoints:

```bash
scripts/agents/test-http-api.sh 127.0.0.1 8080
```

The script checks health/models/symbols plus POST endpoints and basic CORS headers.

### Endpoints

`GET /api/v1/health`
- Returns service status.

`GET /api/v1/models`
- Lists available AI models from registered providers (may be empty).
- Optional query: `?refresh=1` (or `?refresh=true` / `?refresh`) to bypass the server-side model cache.
- Response fields include `name`, `display_name`, `provider`, `description`, `family`, `parameter_size`, `quantization`, `size_bytes`, and `is_local`.
- `display_name` falls back to `name` if the provider does not supply a friendly label.

`GET /api/v1/symbols?format=mesen|asar|wla|bsnes`
- Exports symbol data from the active symbol provider.
- Returns plain text by default; send `Accept: application/json` for JSON.
- Returns `400` for unsupported `format` values (JSON error with supported formats).
- Returns `503` if no symbol provider is available.

`POST /api/v1/navigate`
- Body: `{ "address": 4660, "source": "tool" }`
- Used for external tools to request navigation to a disassembly address.
- Returns `400` on invalid JSON payloads.

`POST /api/v1/breakpoint/hit`
- Body: `{ "address": 4660, "source": "mesen", "cpu_state": { ... } }`
- Optional `cpu_state` payload is stored for later analysis.
- Returns `400` on invalid JSON payloads.

`POST /api/v1/state/update`
- Body: `{ ... }` (schema is evolving; used for panel state updates).
- Returns `400` on invalid JSON payloads.

`POST /api/v1/window/show`
`POST /api/v1/window/hide`
- Controls the desktop window when available; returns `501` if unsupported.

### Errors

Errors return JSON with an `error` field and an appropriate HTTP status code:

```json
{ "error": "Detailed error message" }
```

## gRPC Automation API

The gRPC services are provided by the desktop app for automation, ROM access, and emulator debugging.
They are available only in builds with gRPC enabled (`YAZE_ENABLE_GRPC` / `YAZE_WITH_GRPC`).

### Enable and Run

```bash
./scripts/yaze --enable_test_harness --test_harness_port=50052
```

`--server` also enables automation in headless/service mode.

### Services (proto sources)

- `EmulatorService` - `src/protos/emulator_service.proto`
- `RomService` - `src/protos/rom_service.proto`
- `CanvasAutomation` - `src/protos/canvas_automation.proto`
- `ImGuiTestHarness` - `src/protos/imgui_test_harness.proto`
- `VisualService` - `src/protos/visual_service.proto`

Most services require a ROM to be loaded and will return `FAILED_PRECONDITION` when unavailable.

### Smoke Testing

The helper script uses grpcurl with the ImGui test harness Ping endpoint:

```bash
scripts/agents/test-grpc-api.sh 127.0.0.1 50052
```

You can also call grpcurl directly (no reflection enabled):

```bash
grpcurl -plaintext \
  -import-path src/protos \
  -proto imgui_test_harness.proto \
  -d '{"message":"ping"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Ping
```

## Security

These APIs do not require authentication. Keep them on `localhost` unless you intentionally expose them and understand the risks.
