# ImGuiTestHarness Quick Start Guide

**Last Updated**: October 2, 2025  
**Status**: IT-01 Phase 3 Complete ✅

## Overview

The ImGuiTestHarness provides a gRPC service for automated GUI testing and AI-driven workflows. This guide shows you how to quickly get started with testing YAZE through remote procedure calls.

## Prerequisites

```bash
# Install grpcurl (for testing)
brew install grpcurl

# Build YAZE with gRPC support
cd /Users/scawful/Code/yaze
cmake -B build-grpc-test -DYAZE_WITH_GRPC=ON
cmake --build build-grpc-test --target yaze -j$(sysctl -n hw.ncpu)
```

## Quick Start

### 1. Start YAZE with Test Harness

```bash
./build-grpc-test/bin/yaze.app/Contents/MacOS/yaze \
  --enable_test_harness \
  --test_harness_port=50052 \
  --rom_file=assets/zelda3.sfc &
```

**Output**:
```
✓ ImGuiTestHarness gRPC server listening on 0.0.0.0:50052 (with TestManager integration)
  Use 'grpcurl -plaintext -d '{"message":"test"}' 0.0.0.0:50052 yaze.test.ImGuiTestHarness/Ping' to test
```

### 2. Run Automated Test Script

```bash
./scripts/test_harness_e2e.sh
```

This will test all RPC methods and report pass/fail status.

### 3. Manual Testing

Test individual RPCs with grpcurl:

```bash
# Health check
grpcurl -plaintext -import-path src/app/core/proto -proto imgui_test_harness.proto \
  -d '{"message":"Hello"}' 127.0.0.1:50052 yaze.test.ImGuiTestHarness/Ping

# Click button
grpcurl -plaintext -import-path src/app/core/proto -proto imgui_test_harness.proto \
  -d '{"target":"button:Overworld","type":"LEFT"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Click

# Type text
grpcurl -plaintext -import-path src/app/core/proto -proto imgui_test_harness.proto \
  -d '{"target":"input:Search","text":"tile16","clear_first":true}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Type

# Wait for window
grpcurl -plaintext -import-path src/app/core/proto -proto imgui_test_harness.proto \
  -d '{"condition":"window_visible:Overworld Editor","timeout_ms":5000}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Wait

# Assert state
grpcurl -plaintext -import-path src/app/core/proto -proto imgui_test_harness.proto \
  -d '{"condition":"visible:Main Window"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Assert
```

## RPC Reference

### Ping - Health Check

**Purpose**: Verify service is running and get version info

**Request**:
```json
{
  "message": "test"
}
```

**Response**:
```json
{
  "message": "Pong: test",
  "timestampMs": "1696262400000",
  "yazeVersion": "0.3.2"
}
```

### Click - GUI Interaction

**Purpose**: Click buttons, menu items, and other interactive elements

**Request**:
```json
{
  "target": "button:Open ROM",
  "type": "LEFT"
}
```

**Target Format**: `<widget_type>:<label>`  
**Click Types**: `LEFT`, `RIGHT`, `MIDDLE`, `DOUBLE`

**Response**:
```json
{
  "success": true,
  "message": "Clicked button 'Open ROM'",
  "executionTimeMs": "125"
}
```

### Type - Text Input

**Purpose**: Enter text into input fields

**Request**:
```json
{
  "target": "input:Filename",
  "text": "zelda3.sfc",
  "clear_first": true
}
```

**Parameters**:
- `target`: Input field identifier (format: `input:<label>`)
- `text`: Text to type
- `clear_first`: Clear existing text before typing (default: false)

**Response**:
```json
{
  "success": true,
  "message": "Typed 'zelda3.sfc' into input 'Filename' (cleared first)",
  "executionTimeMs": "250"
}
```

### Wait - Condition Polling

**Purpose**: Wait for UI conditions with timeout

**Request**:
```json
{
  "condition": "window_visible:Overworld Editor",
  "timeout_ms": 5000,
  "poll_interval_ms": 100
}
```

**Condition Types**:
- `window_visible:<WindowName>` - Window exists and not hidden
- `element_visible:<ElementLabel>` - Element exists and has visible rect
- `element_enabled:<ElementLabel>` - Element exists and not disabled

**Response**:
```json
{
  "success": true,
  "message": "Condition 'window_visible:Overworld Editor' met after 1250 ms",
  "elapsedMs": "1250"
}
```

### Assert - State Validation

**Purpose**: Validate GUI state and return actual vs expected values

**Request**:
```json
{
  "condition": "visible:Main Window"
}
```

**Assertion Types**:
- `visible:<WindowName>` - Check window visibility
- `enabled:<ElementLabel>` - Check if element is enabled
- `exists:<ElementLabel>` - Check if element exists
- `text_contains:<InputLabel>:<ExpectedText>` - Validate text content

**Response**:
```json
{
  "success": true,
  "message": "'Main Window' is visible",
  "actualValue": "visible",
  "expectedValue": "visible"
}
```

### Screenshot - Screen Capture

**Purpose**: Capture screenshot of YAZE window (NOT YET IMPLEMENTED)

**Request**:
```json
{
  "region": "full",
  "format": "PNG"
}
```

**Response**:
```json
{
  "success": false,
  "message": "Screenshot not yet implemented",
  "filePath": "",
  "fileSizeBytes": 0
}
```

## Common Workflows

### Workflow 1: Open Editor and Validate

```bash
# 1. Click Overworld button
grpcurl -plaintext -import-path src/app/core/proto -proto imgui_test_harness.proto \
  -d '{"target":"button:Overworld","type":"LEFT"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Click

# 2. Wait for Overworld Editor window
grpcurl -plaintext -import-path src/app/core/proto -proto imgui_test_harness.proto \
  -d '{"condition":"window_visible:Overworld Editor","timeout_ms":5000}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Wait

# 3. Assert window is visible
grpcurl -plaintext -import-path src/app/core/proto -proto imgui_test_harness.proto \
  -d '{"condition":"visible:Overworld Editor"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Assert
```

### Workflow 2: Search and Filter

```bash
# 1. Click search input
grpcurl -plaintext -import-path src/app/core/proto -proto imgui_test_harness.proto \
  -d '{"target":"input:Search","type":"LEFT"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Click

# 2. Type search query
grpcurl -plaintext -import-path src/app/core/proto -proto imgui_test_harness.proto \
  -d '{"target":"input:Search","text":"tile16","clear_first":true}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Type

# 3. Wait for results
grpcurl -plaintext -import-path src/app/core/proto -proto imgui_test_harness.proto \
  -d '{"condition":"element_visible:Results","timeout_ms":2000}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Wait
```

## Troubleshooting

### Server Not Starting

**Problem**: "Failed to start gRPC server"

**Solutions**:
1. Check if port is already in use: `lsof -i :50052`
2. Kill existing YAZE instances: `killall yaze`
3. Try a different port: `--test_harness_port=50053`

### Connection Refused

**Problem**: "Error connecting to server"

**Solutions**:
1. Verify server is running: `lsof -i :50052`
2. Check logs for startup errors
3. Ensure firewall allows connections

### Widget Not Found

**Problem**: "Input field 'XYZ' not found"

**Solutions**:
1. Verify widget label is correct (case-sensitive)
2. Check if widget is in a different window (use full path)
3. Wait for window to be visible first
4. Use Assert to check if widget exists before interacting

### Timeout Errors

**Problem**: "Condition not met after timeout"

**Solutions**:
1. Increase timeout value: `"timeout_ms": 10000`
2. Check if condition is realistic (e.g., window actually opens)
3. Verify window/element names are correct
4. Reduce poll interval for faster detection: `"poll_interval_ms": 50`

## Advanced Usage

### Chaining RPCs in Shell Scripts

```bash
#!/bin/bash
# Example: Automated Overworld Editor Test

set -e

PORT=50052
PROTO_PATH="src/app/core/proto"
PROTO_FILE="imgui_test_harness.proto"

rpc() {
  grpcurl -plaintext -import-path $PROTO_PATH -proto $PROTO_FILE \
    -d "$2" 127.0.0.1:$PORT yaze.test.ImGuiTestHarness/$1
}

# Health check
rpc Ping '{"message":"Starting test"}'

# Open Overworld Editor
rpc Click '{"target":"button:Overworld","type":"LEFT"}'

# Wait for window
rpc Wait '{"condition":"window_visible:Overworld Editor","timeout_ms":5000}'

# Validate
rpc Assert '{"condition":"visible:Overworld Editor"}'

echo "✓ All tests passed"
```

### Python Client (Future)

```python
import grpc
from proto import imgui_test_harness_pb2
from proto import imgui_test_harness_pb2_grpc

# Connect to test harness
channel = grpc.insecure_channel('localhost:50052')
stub = imgui_test_harness_pb2_grpc.ImGuiTestHarnessStub(channel)

# Ping
response = stub.Ping(imgui_test_harness_pb2.PingRequest(message="test"))
print(f"Version: {response.yaze_version}")

# Click
response = stub.Click(imgui_test_harness_pb2.ClickRequest(
    target="button:Overworld",
    type=imgui_test_harness_pb2.ClickRequest.LEFT
))
print(f"Click success: {response.success}")
```

## Next Steps

- **IT-02**: CLI agent integration (`z3ed agent test`)
- **IT-03**: Screenshot implementation
- **VP-02**: Integration tests with replay scripts
- **Windows Testing**: Cross-platform validation

## References

- **Implementation**: `src/app/core/imgui_test_harness_service.{h,cc}`
- **Proto Schema**: `src/app/core/proto/imgui_test_harness.proto`
- **Test Script**: `scripts/test_harness_e2e.sh`
- **Phase 3 Details**: `IT-01-PHASE3-COMPLETE.md`

---

**Last Updated**: October 2, 2025  
**Contributors**: @scawful, GitHub Copilot  
**License**: Same as YAZE (see ../../LICENSE)
